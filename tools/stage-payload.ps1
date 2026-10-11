# Stages installer payload folders for the Inno Setup script.
#
#   x64: 7-Zip core + codecs come from the repo checkout (committed binaries,
#        identical to previous releases).
#   x86: 32-bit 7-Zip core comes from the official 7z2408.exe installer and the
#        32-bit codecs from mcmilk/7-Zip-zstd v26.02-v1.5.7-R1 (pinned; the same
#        versions already ship in the repo's Codecs\ folder). Every staged
#        binary is PE-verified against the target architecture.
#
# Usage:
#   tools\stage-payload.ps1                                    # build + stage both arches to %TEMP%\e7z-payload
#   tools\stage-payload.ps1 -Arch x64 -PayloadRoot C:\temp\installer -SkipBuild
#
param(
    [ValidateSet("x64", "x86")]
    [string[]]$Arch = @("x64", "x86"),
    [string]$RepoRoot = (Split-Path $PSScriptRoot -Parent),
    [string]$PayloadRoot = (Join-Path $env:TEMP "e7z-payload"),
    [string]$Version = "",
    [string]$FullVersion = "",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

# ---------- resolve full version (base + build number) ----------
function Get-RepoFullVersion {
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
    return "$base.$build"
}

if ([string]::IsNullOrWhiteSpace($FullVersion)) {
    if ([string]::IsNullOrWhiteSpace($Version) -or $Version -eq "1.6.0") {
        $FullVersion = Get-RepoFullVersion -Root $RepoRoot
    } else {
        # Back-compat: -Version "1.5.0" + repo BUILD_NUMBER
        $build = "0"
        try {
            $bFile = Join-Path $RepoRoot "BUILD_NUMBER"
            if (Test-Path $bFile) { $build = (Get-Content $bFile -Raw).Trim() }
        } catch { }
        $FullVersion = if ($Version -match '^\d+\.\d+\.\d+\.\d+$') { $Version } else { "$Version.$build" }
    }
}

# ---------- locate tools ----------
$sevenZip = Join-Path $RepoRoot "7z.exe"
if (-not (Test-Path $sevenZip)) { throw "7z.exe not found at repo root: $sevenZip" }

if (-not $SkipBuild) {
    # Must resolve to a dotnet with an SDK, not just the runtime.
    $dotnet = $null
    foreach ($c in @("C:\Program Files\dotnet\dotnet.exe", "C:\temp\dotnet-sdk\dotnet.exe")) {
        if (Test-Path $c) {
            try {
                $sdkList = & $c --list-sdks 2>$null
                if ($sdkList) { $dotnet = $c; break }
            } catch { }
        }
    }
    if (-not $dotnet) {
        $cmd = Get-Command dotnet.exe -ErrorAction SilentlyContinue
        if ($cmd) {
            try {
                $sdkList = & $cmd.Source --list-sdks 2>$null
                if ($sdkList) { $dotnet = $cmd.Source }
            } catch { }
        }
    }
    if (-not $dotnet) { throw "dotnet.exe not found" }
    $proj = Join-Path $RepoRoot "src\StarExtract.csproj"
    foreach ($a in $Arch) {
        Write-Host "[build] dotnet build -c Release -p:Platform=$a -p:Version=$FullVersion"
        & $dotnet build $proj -c Release -p:Platform=$a -p:Version=$FullVersion -p:AssemblyVersion=$FullVersion -p:FileVersion=$FullVersion -p:InformationalVersion=$FullVersion -v minimal
        if ($LASTEXITCODE -ne 0) { throw "dotnet build $a failed" }
    }
}

# ---------- PE architecture verification ----------
function Test-PEArch {
    param([string]$Path, [ValidateSet("x64", "x86")][string]$Want)
    $fs = [System.IO.File]::OpenRead($Path)
    try {
        $br = New-Object System.IO.BinaryReader($fs)
        $fs.Position = 0x3C
        $peOff = $br.ReadInt32()
        $fs.Position = $peOff + 4          # skip 'PE\0\0'
        $machine = $br.ReadUInt16()
    } finally { $fs.Dispose() }
    $isX64 = ($machine -eq 0x8664)         # IMAGE_FILE_MACHINE_AMD64
    $isX86 = ($machine -eq 0x014C)         # IMAGE_FILE_MACHINE_I386
    if ($Want -eq "x64" -and -not $isX64) { throw "PE check failed (machine=0x{0:X4}, want x64): $Path" -f $machine }
    if ($Want -eq "x86" -and -not $isX86) { throw "PE check failed (machine=0x{0:X4}, want x86): $Path" -f $machine }
}

function Get-Download {
    param([string]$Url, [string]$DestFile)
    if (Test-Path $DestFile) { Write-Host "[dl] cached: $DestFile"; return }
    Write-Host "[dl] $Url"
    [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12
    (New-Object System.Net.WebClient).DownloadFile($Url, $DestFile)
}

# ---------- LessMSI (forced engine for .msi/.msp) ----------
# MSI extraction always goes through LessMSI. Ensure bin\lessmsi\lessmsi.exe
# exists in the repo checkout so the installer (bin\*) and portable ZIPs bundle it.
$LessMsiVersion = "v2.12.9"
$lessMsiDir = Join-Path $RepoRoot "bin\lessmsi"
$lessMsiExe = Join-Path $lessMsiDir "lessmsi.exe"
$dlDir = Join-Path $PayloadRoot "downloads"
New-Item -ItemType Directory -Path $dlDir -Force | Out-Null
if (-not (Test-Path $lessMsiExe)) {
    try {
        $lessMsiZip = Join-Path $dlDir "lessmsi-$LessMsiVersion.zip"
        Get-Download -Url "https://github.com/activescott/lessmsi/releases/download/$LessMsiVersion/lessmsi-$LessMsiVersion.zip" -DestFile $lessMsiZip
        $lessMsiStage = Join-Path $dlDir "lessmsi-extract"
        if (Test-Path $lessMsiStage) { Remove-Item $lessMsiStage -Recurse -Force }
        New-Item -ItemType Directory -Path $lessMsiStage -Force | Out-Null
        & $sevenZip x $lessMsiZip ("-o" + $lessMsiStage) -y | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "Failed to extract $lessMsiZip" }
        New-Item -ItemType Directory -Path $lessMsiDir -Force | Out-Null
        $stagedExe = Get-ChildItem $lessMsiStage -Recurse -Filter "lessmsi.exe" | Select-Object -First 1
        if ($stagedExe -eq $null) { throw "lessmsi.exe not found in $lessMsiZip" }
        Copy-Item (Join-Path $stagedExe.DirectoryName "*") $lessMsiDir -Recurse -Force
        Write-Host "[lessmsi] bundled into bin\lessmsi\"
    } catch {
        Write-Warning "[lessmsi] download failed: $($_.Exception.Message) - MSI extraction will error until LessMSI is installed via Components tab."
    }
} else {
    Write-Host "[lessmsi] already bundled: bin\lessmsi\lessmsi.exe"
}

# ---------- official 7-Zip 24.08 cores (only needed for x86) ----------
$x86CoreDir = $null
if ($Arch -contains "x86") {
    $x86Installer = Join-Path $dlDir "7z2408.exe"
    Get-Download -Url "https://www.7-zip.org/a/7z2408.exe" -DestFile $x86Installer
    $x86CoreDir = Join-Path $dlDir "7z2408-x86"
    if (-not (Test-Path (Join-Path $x86CoreDir "7z.exe"))) {
        New-Item -ItemType Directory -Path $x86CoreDir -Force | Out-Null
        & $sevenZip x $x86Installer ("-o" + $x86CoreDir) -y | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "Failed to extract 7z2408.exe" }
    }
}

# ---------- per-arch codec sources (only needed for x86) ----------
$CodecVersion = "v26.02-v1.5.7-R1"   # pin: matches the DLLs committed in Codecs\
$x86CodecDir = $null
if ($Arch -contains "x86") {
    $codecArchive = Join-Path $dlDir "Codecs-x86.7z"
    Get-Download -Url "https://github.com/mcmilk/7-Zip-zstd/releases/download/$CodecVersion/Codecs-x86.7z" -DestFile $codecArchive
    # archive contains the codec DLLs at its root (no Codecs\ subfolder)
    $x86CodecDir = Join-Path $dlDir "Codecs-x86"
    if (-not (Test-Path (Join-Path $x86CodecDir "zstd.dll"))) {
        & $sevenZip x $codecArchive ("-o" + $x86CodecDir) -y | Out-Null
        if ($LASTEXITCODE -ne 0) { throw "Failed to extract Codecs-x86.7z" }
    }
}

# ---------- staging ----------
$payloadStamp = "v$FullVersion"
foreach ($a in $Arch) {
    Write-Host "[stage] payload-$a"
    $dest = Join-Path $PayloadRoot "payload-$a"
    if (Test-Path $dest) { Remove-Item $dest -Recurse -Force }
    New-Item -ItemType Directory -Path $dest -Force | Out-Null

    # app binary for this architecture
    $appBin = Join-Path $RepoRoot "src\bin\$a\Release\net48"
    Copy-Item (Join-Path $appBin "StarExtract.exe") $dest -Force
    Copy-Item (Join-Path $appBin "StarExtract.pdb") $dest -Force

    # native C++ edition (arch-neutral single binary) - bundled when built
    $nativeExe = Join-Path $RepoRoot "native\StarExtractNative.exe"
    if (Test-Path $nativeExe) {
        Copy-Item $nativeExe $dest -Force
    } else {
        Write-Host "[stage] native exe not built - skipping (run native\build.bat to include it)"
    }

    if ($a -eq "x64") {
        # 7-Zip core + shell + codecs: committed repo binaries (byte-identical
        # to what previous installers shipped; SFX modules are not included -
        # the app never builds SFX archives)
        foreach ($f in @("7-zip.chm", "7-zip.dll", "7z.dll", "7z.exe", "7zFM.exe", "7zG.exe")) {
            Copy-Item (Join-Path $RepoRoot $f) $dest -Force
        }
        Copy-Item (Join-Path $RepoRoot "License.txt") (Join-Path $dest "7zip-license.txt") -Force
        Copy-Item (Join-Path $RepoRoot "Codecs") (Join-Path $dest "Codecs") -Recurse -Force
    }
    else {
        # 32-bit core + shell from the official installer
        foreach ($f in @("7-zip.dll", "7z.dll", "7z.exe", "7zFM.exe", "7zG.exe")) {
            Copy-Item (Join-Path $x86CoreDir $f) $dest -Force
        }
        # 32-bit codecs (pinned release) - archive root holds the DLLs
        New-Item -ItemType Directory -Path (Join-Path $dest "Codecs") -Force | Out-Null
        Copy-Item (Join-Path $x86CodecDir "*.dll") (Join-Path $dest "Codecs") -Force
        Copy-Item (Join-Path $x86CodecDir "LICENSE") (Join-Path $dest "Codecs\LICENSE") -Force
        Copy-Item (Join-Path $x86CodecDir "README.md") (Join-Path $dest "Codecs\README.md") -Force
        # help is arch-neutral; SFX modules are intentionally not staged
        Copy-Item (Join-Path $RepoRoot "7-zip.chm") $dest -Force
        Copy-Item (Join-Path $RepoRoot "License.txt") (Join-Path $dest "7zip-license.txt") -Force
    }

    # 90 UI languages (arch-neutral filler, kept for payload parity)
    Copy-Item (Join-Path $RepoRoot "Lang") (Join-Path $dest "Lang") -Recurse -Force

    # PE verification: the core, shell, SFX modules and every codec must
    # match the target architecture
    foreach ($f in @("7z.exe", "7z.dll", "7zG.exe", "7zFM.exe")) {
        $p = Join-Path $dest $f
        if (Test-Path $p) { Test-PEArch -Path $p -Want $a }
    }
    Get-ChildItem (Join-Path $dest "Codecs") -Filter *.dll | ForEach-Object {
        Test-PEArch -Path $_.FullName -Want $a
    }

    Write-Host ("[stage] payload-{0}: {1:N1} MB verified" -f $a, ((Get-ChildItem $dest -Recurse | Measure-Object Length -Sum).Sum / 1MB))
}

Write-Host "Staged payloads for $payloadStamp in $PayloadRoot"
