# Star Extract: Build and Generate Installers (+ portable ZIPs)
# Version comes from VERSION + BUILD_NUMBER files at the repo root:
#   full = "<VERSION>.<BUILD_NUMBER>"  (e.g. 1.6.0.1)
# The full version is stamped into StarExtract.exe (Version/AssemblyVersion/
# FileVersion) and into every artifact name (setup exes + portable/setup ZIPs).
# BUILD_NUMBER is incremented after a successful build so the next build is new.
#
# -NativeOnly builds installers that ship only the C++ Win32 edition
# (StarExtractNative.exe, no .NET build, no .NET runtime requirement).
# Artifacts get a "-native" marker in their names.
param(
    [switch]$NoIncrement,
    [switch]$NoPortable,
    [switch]$NoSetupZip,
    [switch]$NativeOnly
)

$ErrorActionPreference = "Stop"

$rootDir = $PSScriptRoot
if (-not $rootDir) { $rootDir = "C:\Users\Administrator\Documents\StarExtract" }

function Get-RepoVersion {
    param([string]$Root)
    $base = "1.6.0"
    $build = "0"
    try {
        $vFile = Join-Path $Root "VERSION"
        if (Test-Path $vFile) { $base = (Get-Content $vFile -Raw).Trim() }
    } catch { }
    try {
        $bFile = Join-Path $Root "BUILD_NUMBER"
        if (Test-Path $bFile) { $build = (Get-Content $bFile -Raw).Trim() }
    } catch { }
    if ($base -match '^\d+\.\d+\.\d+\.\d+$') { return $base }
    return "$base.$build"
}

$version = Get-RepoVersion -Root $rootDir
$variant = if ($NativeOnly) { "-native" } else { "" }

$env:PATH = "C:\Program Files\dotnet;C:\temp\dotnet-sdk;C:\Program Files\Inno Setup 7;C:\temp\installer\tools\innoportable\{app};" + $env:PATH

Write-Host "======================================================================" -ForegroundColor Cyan
Write-Host " Star Extract v$version - Automated Build & Installer Generator" -ForegroundColor Cyan
Write-Host "======================================================================" -ForegroundColor Cyan

# 1. Locate dotnet (must have an SDK, not just the runtime) - not needed for
# the native-only installer, which never builds the .NET project
$dotnetExe = $null
if (-not $NativeOnly) {
    $dotnetCandidates = @(
        "C:\Program Files\dotnet\dotnet.exe",
        "C:\temp\dotnet-sdk\dotnet.exe"
    )
    foreach ($c in $dotnetCandidates) {
        if (Test-Path $c) {
            try {
                $sdkList = & $c --list-sdks 2>$null
                if ($sdkList) { $dotnetExe = $c; break }
            } catch { }
        }
    }
    if (-not $dotnetExe) {
        $cmd = Get-Command dotnet.exe -ErrorAction SilentlyContinue
        if ($cmd) { $dotnetExe = $cmd.Source }
    }
    if (-not $dotnetExe) {
        throw "[ERROR] .NET SDK (dotnet.exe) was not found!"
    }
    Write-Host "[OK] Using .NET SDK: $dotnetExe" -ForegroundColor Gray
}

# 2. Locate ISCC
$isccCandidates = @(
    "C:\Program Files\Inno Setup 7\ISCC.exe",
    "C:\temp\installer\tools\innoportable\{app}\ISCC.exe",
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
)
$isccExe = $null
foreach ($c in $isccCandidates) {
    if (Test-Path $c) { $isccExe = $c; break }
}
if (-not $isccExe) {
    $cmd = Get-Command ISCC.exe -ErrorAction SilentlyContinue
    if ($cmd) { $isccExe = $cmd.Source }
}
if (-not $isccExe) {
    throw "[ERROR] Inno Setup compiler (ISCC.exe) was not found!"
}
Write-Host "[OK] Using Inno Setup: $isccExe" -ForegroundColor Gray

# 3. Build Binaries (stamped with the full version incl. build number)
$nativeDir = Join-Path $rootDir "native"
$vcvars = "C:\buildtools\VC\Auxiliary\Build\vcvars64.bat"

if ($NativeOnly) {
    # Native-only: build the C++ edition for both arches; no .NET involved.
    Write-Host "`n[1/5] Building native edition x64 + x86 (v$version)..." -ForegroundColor Yellow
    if (-not (Test-Path $vcvars)) { throw "[ERROR] MSVC toolchain not found: $vcvars" }
    foreach ($a in @("x64", "x86")) {
        Write-Host "Building native $a (Release)..." -ForegroundColor Gray
        Push-Location $nativeDir
        try {
            & cmd.exe /c "build.bat $version nobump $a"
            if ($LASTEXITCODE -ne 0) { throw "native build $a failed with exit code $LASTEXITCODE" }
        } finally {
            Pop-Location
        }
    }
} else {
    Write-Host "`n[1/5] Building Release x64 and x86 binaries (v$version)..." -ForegroundColor Yellow
    $proj = "$rootDir\src\StarExtract.csproj"

    Write-Host "Building x64 (Release)..." -ForegroundColor Gray
    & $dotnetExe build $proj -c Release -p:Platform=x64 -p:Version=$version -p:AssemblyVersion=$version -p:FileVersion=$version -p:InformationalVersion=$version
    if ($LASTEXITCODE -ne 0) { throw "dotnet build x64 failed with exit code $LASTEXITCODE" }

    Write-Host "Building x86 (Release)..." -ForegroundColor Gray
    & $dotnetExe build $proj -c Release -p:Platform=x86 -p:Version=$version -p:AssemblyVersion=$version -p:FileVersion=$version -p:InformationalVersion=$version
    if ($LASTEXITCODE -ne 0) { throw "dotnet build x86 failed with exit code $LASTEXITCODE" }

    # 3b. Build the native C++ edition with the same version stamp so the exe
    # bundled in the installers/ZIPs matches the artifact version exactly.
    # The toolchain path is machine-specific (works locally + on this host); on
    # machines without it we fall back to a pre-built exe or skip it.
    if (Test-Path $vcvars) {
        Write-Host "Building native edition (v$version, no bump)..." -ForegroundColor Gray
        Push-Location $nativeDir
        try {
            & cmd.exe /c "build.bat $version nobump"
            if ($LASTEXITCODE -ne 0) { throw "native build failed with exit code $LASTEXITCODE" }
        } finally {
            Pop-Location
        }
        Write-Host "Native edition built: $nativeDir\StarExtractNative.exe" -ForegroundColor Gray
    } else {
        Write-Warning "MSVC toolchain not found ($vcvars) - bundling pre-built native exe if present."
    }
}

# 4. Stage Payloads
Write-Host "`n[2/5] Staging Payloads..." -ForegroundColor Yellow
$outDir = "C:\temp\installer\output"

# Minimal PE check: verifies a staged binary matches the target architecture.
function Test-PEArch {
    param([string]$Path, [ValidateSet("x64", "x86")][string]$Want)
    $fs = [System.IO.File]::OpenRead($Path)
    try {
        $br = New-Object System.IO.BinaryReader($fs)
        $fs.Position = 0x3C
        $peOff = $br.ReadInt32()
        $fs.Position = $peOff + 4
        $machine = $br.ReadUInt16()
    } finally { $fs.Dispose() }
    $isX64 = ($machine -eq 0x8664)
    $isX86 = ($machine -eq 0x014C)
    if ($Want -eq "x64" -and -not $isX64) { throw ("PE check failed (machine=0x{0:X4}, want x64): $Path" -f $machine) }
    if ($Want -eq "x86" -and -not $isX86) { throw ("PE check failed (machine=0x{0:X4}, want x86): $Path" -f $machine) }
}

if ($NativeOnly) {
    # Native-only payloads live in their own dirs (payload-nat-*) and are
    # rebuilt from scratch each run: native exe + 7-Zip CLI core only.
    $p64 = "C:\temp\installer\payload-nat-x64"
    $p86 = "C:\temp\installer\payload-nat-x86"
    foreach ($d in @($p64, $p86, $outDir)) {
        if (Test-Path $d) { Remove-Item $d -Recurse -Force }
        New-Item -ItemType Directory -Path $d -Force | Out-Null
    }

    # x64: native x64 exe + repo x64 7-Zip core + codecs
    Copy-Item "$nativeDir\StarExtractNative.exe" "$p64\StarExtractNative.exe" -Force
    foreach ($f in @("7z.exe", "7z.dll")) { Copy-Item "$rootDir\$f" "$p64\$f" -Force }
    Copy-Item "$rootDir\License.txt" "$p64\7zip-license.txt" -Force
    Copy-Item "$rootDir\Codecs" "$p64\Codecs" -Recurse -Force
    Test-PEArch -Path "$p64\StarExtractNative.exe" -Want x64
    Test-PEArch -Path "$p64\7z.exe" -Want x64

    # x86: native x86 exe + 32-bit 7-Zip core from bin\x86
    Copy-Item "$nativeDir\x86\StarExtractNative.exe" "$p86\StarExtractNative.exe" -Force
    foreach ($f in @("7z.exe", "7z.dll")) { Copy-Item "$rootDir\bin\x86\$f" "$p86\$f" -Force }
    Copy-Item "$rootDir\License.txt" "$p86\7zip-license.txt" -Force
    Test-PEArch -Path "$p86\StarExtractNative.exe" -Want x86
    Test-PEArch -Path "$p86\7z.exe" -Want x86

    # 32-bit codecs (pinned release, cached after first download; best-effort -
    # without them 7z.exe still handles every core format)
    try {
        $dlDir = "C:\temp\installer\downloads"
        $codecX86 = Join-Path $dlDir "Codecs-x86"
        if (-not (Test-Path (Join-Path $codecX86 "zstd.dll"))) {
            New-Item -ItemType Directory -Path $dlDir -Force | Out-Null
            $codecZip = Join-Path $dlDir "Codecs-x86.7z"
            if (-not (Test-Path $codecZip)) {
                [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12
                Write-Host "[dl] x86 codecs (7-Zip-zstd v26.02-v1.5.7-R1)"
                (New-Object System.Net.WebClient).DownloadFile(
                    "https://github.com/mcmilk/7-Zip-zstd/releases/download/v26.02-v1.5.7-R1/Codecs-x86.7z",
                    $codecZip)
            }
            if (Test-Path $codecX86) { Remove-Item $codecX86 -Recurse -Force }
            New-Item -ItemType Directory -Path $codecX86 -Force | Out-Null
            & "$rootDir\7z.exe" x $codecZip ("-o" + $codecX86) -y | Out-Null
            if ($LASTEXITCODE -ne 0) { throw "7z extraction failed" }
        }
        New-Item -ItemType Directory -Path "$p86\Codecs" -Force | Out-Null
        Copy-Item "$codecX86\*" "$p86\Codecs" -Force
        Get-ChildItem "$p86\Codecs" -Filter *.dll | ForEach-Object { Test-PEArch -Path $_.FullName -Want x86 }
        Write-Host "[stage] x86 codecs bundled" -ForegroundColor Gray
    } catch {
        Write-Warning "x86 codecs unavailable ($($_.Exception.Message)) - shipping without them."
    }

    Write-Host ("[stage] payload-nat-x64: {0:N1} MB / payload-nat-x86: {1:N1} MB verified" -f `
        ((Get-ChildItem $p64 -Recurse | Measure-Object Length -Sum).Sum / 1MB),
        ((Get-ChildItem $p86 -Recurse | Measure-Object Length -Sum).Sum / 1MB))
} else {
$p64 = "C:\temp\installer\payload-x64"
$p86 = "C:\temp\installer\payload-x86"

@($p64, $p86, $outDir) | ForEach-Object {
    if (-not (Test-Path $_)) { New-Item -ItemType Directory -Path $_ -Force | Out-Null }
}

Copy-Item "$rootDir\src\bin\x64\Release\net48\StarExtract.exe" "$p64\StarExtract.exe" -Force
Copy-Item "$rootDir\src\bin\x64\Release\net48\StarExtract.pdb" "$p64\StarExtract.pdb" -Force
Copy-Item "$rootDir\src\bin\x64\Release\net48\StarExtract.exe" "$rootDir\StarExtract.exe" -Force

Copy-Item "$rootDir\src\bin\x86\Release\net48\StarExtract.exe" "$p86\StarExtract.exe" -Force
Copy-Item "$rootDir\src\bin\x86\Release\net48\StarExtract.pdb" "$p86\StarExtract.pdb" -Force

# Bundle the native C++ edition beside the .NET app when it has been built
$nativeExe = "$rootDir\native\StarExtractNative.exe"
if (Test-Path $nativeExe) {
    Copy-Item $nativeExe "$p64\StarExtractNative.exe" -Force
    Copy-Item $nativeExe "$p86\StarExtractNative.exe" -Force
    Write-Host "Bundled native edition: $nativeExe" -ForegroundColor Gray
} else {
    Write-Warning "native\StarExtractNative.exe not found - run native\build.bat first to include it."
}
}

# 5. Compile Installers
Write-Host "`n[3/5] Compiling Installers with Inno Setup (v$version$variant)..." -ForegroundColor Yellow
$issPath = "$rootDir\installer\StarExtract-installer.iss"
$extraDefines = @()
if ($NativeOnly) { $extraDefines = @("/DNativeOnly") }

Write-Host "Compiling x64 installer..." -ForegroundColor Gray
& $isccExe $issPath /DArch=x64 "/DRepoRoot=$rootDir" "/DPayloadRoot=C:\temp\installer" "/DAppVersion=$version" @extraDefines
if ($LASTEXITCODE -ne 0) { throw "ISCC x64 compilation failed with exit code $LASTEXITCODE" }

Write-Host "Compiling x86 installer..." -ForegroundColor Gray
& $isccExe $issPath /DArch=x86 "/DRepoRoot=$rootDir" "/DPayloadRoot=C:\temp\installer" "/DAppVersion=$version" @extraDefines
if ($LASTEXITCODE -ne 0) { throw "ISCC x86 compilation failed with exit code $LASTEXITCODE" }

# 6. Portable ZIPs + setup ZIPs (all names carry the full version incl. build number)
# NOTE: zips are built with 7z.exe, not Compress-Archive — the bundled plugin
# tree contains pre-1980 timestamps that break Compress-Archive on PS 5.1.
Write-Host "`n[4/5] Creating portable and setup ZIPs..." -ForegroundColor Yellow
$sevenZip = Join-Path $rootDir "7z.exe"
if (-not (Test-Path $sevenZip)) { throw "[ERROR] 7z.exe not found at repo root: $sevenZip" }
$portableStaging = "C:\temp\installer\portable-staging"
if (Test-Path $portableStaging) { Remove-Item $portableStaging -Recurse -Force }
New-Item -ItemType Directory -Path $portableStaging -Force | Out-Null

foreach ($arch in @("x64", "x86")) {
    $payload = if ($arch -eq "x64") { $p64 } else { $p86 }
    $stage = Join-Path $portableStaging $arch
    New-Item -ItemType Directory -Path $stage -Force | Out-Null
    Copy-Item "$payload\*" $stage -Recurse -Force
    # Mirror the installer layout: bin\ plugin suite + README at the root
    if (Test-Path "$rootDir\bin") {
        Copy-Item "$rootDir\bin" (Join-Path $stage "bin") -Recurse -Force
    }
    if (Test-Path "$rootDir\README.md") {
        Copy-Item "$rootDir\README.md" (Join-Path $stage "README.md") -Force
    }
}

if (-not $NoPortable) {
    $portX64 = "$outDir\StarExtract-$version$variant-portable-x64.zip"
    $portX86 = "$outDir\StarExtract-$version$variant-portable-x86.zip"
    if (Test-Path $portX64) { Remove-Item $portX64 -Force }
    if (Test-Path $portX86) { Remove-Item $portX86 -Force }
    & $sevenZip a -tzip $portX64 (Join-Path $portableStaging "x64\*") -mx=9 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "7z portable x64 zip failed with exit code $LASTEXITCODE" }
    & $sevenZip a -tzip $portX86 (Join-Path $portableStaging "x86\*") -mx=9 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "7z portable x86 zip failed with exit code $LASTEXITCODE" }
    Write-Host "  portable x64: $portX64" -ForegroundColor Gray
    Write-Host "  portable x86: $portX86" -ForegroundColor Gray
}

if (-not $NoSetupZip) {
    foreach ($arch in @("x64", "x86")) {
        $setupExe = "$outDir\StarExtract-$version$variant-setup-$arch.exe"
        $setupZip = "$outDir\StarExtract-$version$variant-setup-$arch.zip"
        if (Test-Path $setupExe) {
            if (Test-Path $setupZip) { Remove-Item $setupZip -Force }
            & $sevenZip a -tzip $setupZip $setupExe -mx=9 | Out-Null
            if ($LASTEXITCODE -ne 0) { throw "7z setup $arch zip failed with exit code $LASTEXITCODE" }
            Write-Host "  setup zip ${arch}: $setupZip" -ForegroundColor Gray
        }
    }
}

# 7. Copy output
Write-Host "`n[5/5] Syncing installers to repository..." -ForegroundColor Yellow
$repoInstallerDir = "$rootDir\installer"
if (-not (Test-Path $repoInstallerDir)) {
    New-Item -ItemType Directory -Path $repoInstallerDir -Force | Out-Null
}

Copy-Item "$outDir\StarExtract-$version$variant-setup-x64.exe" "$repoInstallerDir\StarExtract-$version$variant-setup-x64.exe" -Force
Copy-Item "$outDir\StarExtract-$version$variant-setup-x86.exe" "$repoInstallerDir\StarExtract-$version$variant-setup-x86.exe" -Force
if (-not $NoPortable) {
    Copy-Item "$outDir\StarExtract-$version$variant-portable-x64.zip" "$repoInstallerDir\StarExtract-$version$variant-portable-x64.zip" -Force
    Copy-Item "$outDir\StarExtract-$version$variant-portable-x86.zip" "$repoInstallerDir\StarExtract-$version$variant-portable-x86.zip" -Force
}
if (-not $NoSetupZip) {
    Copy-Item "$outDir\StarExtract-$version$variant-setup-x64.zip" "$repoInstallerDir\StarExtract-$version$variant-setup-x64.zip" -Force
    Copy-Item "$outDir\StarExtract-$version$variant-setup-x86.zip" "$repoInstallerDir\StarExtract-$version$variant-setup-x86.zip" -Force
}

# 8. Bump BUILD_NUMBER for the next build
if (-not $NoIncrement) {
    try {
        $bFile = Join-Path $rootDir "BUILD_NUMBER"
        $cur = 0
        if (Test-Path $bFile) { [int]::TryParse((Get-Content $bFile -Raw).Trim(), [ref]$cur) | Out-Null }
        $next = $cur + 1
        Set-Content -Path $bFile -Value "$next" -NoNewline
        Write-Host "Build number bumped: $cur -> $next" -ForegroundColor Gray
    } catch {
        Write-Warning "Could not bump BUILD_NUMBER: $($_.Exception.Message)"
    }
}

Write-Host "`n======================================================================" -ForegroundColor Green
Write-Host " [SUCCESS] Star Extract v$version$variant Installers + ZIPs Generated!" -ForegroundColor Green
Write-Host "======================================================================" -ForegroundColor Green
Write-Host "  x64 Installer: $repoInstallerDir\StarExtract-$version$variant-setup-x64.exe" -ForegroundColor White
Write-Host "  x86 Installer: $repoInstallerDir\StarExtract-$version$variant-setup-x86.exe" -ForegroundColor White
if (-not $NoPortable) {
    Write-Host "  x64 Portable:  $repoInstallerDir\StarExtract-$version$variant-portable-x64.zip" -ForegroundColor White
    Write-Host "  x86 Portable:  $repoInstallerDir\StarExtract-$version$variant-portable-x86.zip" -ForegroundColor White
}
Write-Host "======================================================================" -ForegroundColor Green
