@echo off
setlocal enabledelayedexpansion

:: Star Extract: Build + Installers + portable/setup ZIPs
:: Full version = VERSION file + BUILD_NUMBER file (e.g. 1.6.0.1).
:: Stamped into StarExtract.exe and every artifact name. BUILD_NUMBER is
:: bumped after a successful build unless /noincrement is passed.

set "ROOT_DIR=%~dp0"
if "%ROOT_DIR:~-1%"=="\" set "ROOT_DIR=%ROOT_DIR:~0,-1%"

set NOINCREMENT=0
set NOPORTABLE=0
set NOSETUPZIP=0
for %%A in (%*) do (
    if /i "%%A"=="/noincrement" set NOINCREMENT=1
    if /i "%%A"=="-noincrement" set NOINCREMENT=1
    if /i "%%A"=="/noportable" set NOPORTABLE=1
    if /i "%%A"=="-noportable" set NOPORTABLE=1
    if /i "%%A"=="/nosetupzip" set NOSETUPZIP=1
    if /i "%%A"=="-nosetupzip" set NOSETUPZIP=1
)

:: ---- resolve full version from VERSION + BUILD_NUMBER ----
set "BASEVER=1.6.0"
set "BUILDNUM=0"
if exist "%ROOT_DIR%\VERSION" (
    for /f "usebackq delims=" %%V in ("%ROOT_DIR%\VERSION") do set "BASEVER=%%V"
)
if exist "%ROOT_DIR%\BUILD_NUMBER" (
    for /f "usebackq delims=" %%B in ("%ROOT_DIR%\BUILD_NUMBER") do set "BUILDNUM=%%B"
)
:: trim spaces
for /f "tokens=* delims= " %%V in ("%BASEVER%") do set "BASEVER=%%V"
for /f "tokens=* delims= " %%B in ("%BUILDNUM%") do set "BUILDNUM=%%B"
echo %BASEVER% | findstr /r "^[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*$" >nul
if %errorlevel% equ 0 (
    set "VERSION=%BASEVER%"
) else (
    set "VERSION=%BASEVER%.%BUILDNUM%"
)

:: Prepend all tool paths to PATH
set "PATH=C:\Program Files\dotnet;C:\temp\dotnet-sdk;C:\Program Files\Inno Setup 7;C:\temp\installer\tools\innoportable\{app};%PATH%"

echo ======================================================================
echo  Star Extract v%VERSION% - Automated Build ^& Installer Generator
echo ======================================================================
echo.

:: 1. Locate dotnet.exe (must have an SDK, not just the runtime)
set "DOTNET_EXE="
for %%D in ("C:\Program Files\dotnet\dotnet.exe" "C:\temp\dotnet-sdk\dotnet.exe") do (
    if not defined DOTNET_EXE (
        if exist %%D (
            %%~D --list-sdks 2>nul | findstr /r "[0-9]\.[0-9]" >nul
            if !errorlevel! equ 0 set "DOTNET_EXE=%%~D"
        )
    )
)
if "%DOTNET_EXE%"=="" (
    where dotnet.exe >nul 2>nul
    if %errorlevel% equ 0 (
        dotnet --list-sdks 2>nul | findstr /r "[0-9]\.[0-9]" >nul
        if !errorlevel! equ 0 set "DOTNET_EXE=dotnet"
    )
)

if "%DOTNET_EXE%"=="" (
    echo [ERROR] .NET SDK (dotnet.exe) was not found!
    echo Checked:
    echo   - C:\Program Files\dotnet\dotnet.exe
    echo   - C:\temp\dotnet-sdk\dotnet.exe
    echo   - System PATH
    echo Please install .NET SDK or place it in C:\temp\dotnet-sdk\
    pause
    exit /b 1
)
echo [OK] Found .NET SDK: "%DOTNET_EXE%"

:: 2. Locate ISCC.exe
set "ISCC_EXE="
if exist "C:\Program Files\Inno Setup 7\ISCC.exe" (
    set "ISCC_EXE=C:\Program Files\Inno Setup 7\ISCC.exe"
) else if exist "C:\temp\installer\tools\innoportable\{app}\ISCC.exe" (
    set "ISCC_EXE=C:\temp\installer\tools\innoportable\{app}\ISCC.exe"
) else if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" (
    set "ISCC_EXE=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
) else (
    where ISCC.exe >nul 2>nul
    if %errorlevel% equ 0 set "ISCC_EXE=ISCC.exe"
)

if "%ISCC_EXE%"=="" (
    echo [ERROR] Inno Setup Compiler (ISCC.exe) was not found!
    echo Checked:
    echo   - C:\Program Files\Inno Setup 7\ISCC.exe
    echo   - C:\temp\installer\tools\innoportable\{app}\ISCC.exe
    echo   - System PATH
    pause
    exit /b 1
)
echo [OK] Found Inno Setup Compiler: "%ISCC_EXE%"

echo.
echo [1/5] Compiling Star Extract x64 (Release) v%VERSION%...
"%DOTNET_EXE%" build "%ROOT_DIR%\src\StarExtract.csproj" -c Release -p:Platform=x64 -p:Version=%VERSION% -p:AssemblyVersion=%VERSION% -p:FileVersion=%VERSION% -p:InformationalVersion=%VERSION%
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] dotnet build x64 failed with exit code %errorlevel%!
    pause
    exit /b %errorlevel%
)

echo.
echo [2/5] Compiling Star Extract x86 (Release) v%VERSION%...
"%DOTNET_EXE%" build "%ROOT_DIR%\src\StarExtract.csproj" -c Release -p:Platform=x86 -p:Version=%VERSION% -p:AssemblyVersion=%VERSION% -p:FileVersion=%VERSION% -p:InformationalVersion=%VERSION%
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] dotnet build x86 failed with exit code %errorlevel%!
    pause
    exit /b %errorlevel%
)

echo.
echo [3/5] Staging Payloads...
if not exist "C:\temp\installer\payload-x64" mkdir "C:\temp\installer\payload-x64"
if not exist "C:\temp\installer\payload-x86" mkdir "C:\temp\installer\payload-x86"
if not exist "C:\temp\installer\output" mkdir "C:\temp\installer\output"

copy /y "%ROOT_DIR%\src\bin\x64\Release\net48\StarExtract.exe" "C:\temp\installer\payload-x64\StarExtract.exe" >nul
copy /y "%ROOT_DIR%\src\bin\x64\Release\net48\StarExtract.pdb" "C:\temp\installer\payload-x64\StarExtract.pdb" >nul
copy /y "%ROOT_DIR%\src\bin\x64\Release\net48\StarExtract.exe" "%ROOT_DIR%\StarExtract.exe" >nul

copy /y "%ROOT_DIR%\src\bin\x86\Release\net48\StarExtract.exe" "C:\temp\installer\payload-x86\StarExtract.exe" >nul
copy /y "%ROOT_DIR%\src\bin\x86\Release\net48\StarExtract.pdb" "C:\temp\installer\payload-x86\StarExtract.pdb" >nul
if exist "%ROOT_DIR%\native\StarExtractNative.exe" (
    copy /y "%ROOT_DIR%\native\StarExtractNative.exe" "C:\temp\installer\payload-x64\StarExtractNative.exe" >nul
    copy /y "%ROOT_DIR%\native\StarExtractNative.exe" "C:\temp\installer\payload-x86\StarExtractNative.exe" >nul
    echo Native edition bundled.
) else (
    echo [WARN] native\StarExtractNative.exe not found - run native\build.bat first to include it.
)
echo Payloads staged successfully.

echo.
echo [4/5] Building Inno Setup Installers v%VERSION%...
echo --- Compiling x64 installer ---
"%ISCC_EXE%" "%ROOT_DIR%\installer\StarExtract-installer.iss" /DArch=x64 "/DRepoRoot=%ROOT_DIR%" "/DPayloadRoot=C:\temp\installer" "/DAppVersion=%VERSION%"
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Inno Setup x64 installer compilation failed!
    pause
    exit /b %errorlevel%
)

echo.
echo --- Compiling x86 installer ---
"%ISCC_EXE%" "%ROOT_DIR%\installer\StarExtract-installer.iss" /DArch=x86 "/DRepoRoot=%ROOT_DIR%" "/DPayloadRoot=C:\temp\installer" "/DAppVersion=%VERSION%"
if %errorlevel% neq 0 (
    echo.
    echo [ERROR] Inno Setup x86 installer compilation failed!
    pause
    exit /b %errorlevel%
)

echo.
echo [5/5] Creating portable and setup ZIPs...
:: NOTE: zips are built with 7z.exe, not Compress-Archive - the bundled plugin
:: tree contains pre-1980 timestamps that break Compress-Archive on PS 5.1.
if "%NOPORTABLE%"=="1" goto :skipportable
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$p64='C:\temp\installer\payload-x64'; $p86='C:\temp\installer\payload-x86';" ^
  "$stage='C:\temp\installer\portable-staging'; if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }; New-Item -ItemType Directory -Path \"$stage\x64\" -Force | Out-Null; New-Item -ItemType Directory -Path \"$stage\x86\" -Force | Out-Null;" ^
  "Copy-Item \"$p64\*\" \"$stage\x64\" -Recurse -Force; Copy-Item \"$p86\*\" \"$stage\x86\" -Recurse -Force;" ^
  "if (Test-Path '%ROOT_DIR%\bin') { Copy-Item '%ROOT_DIR%\bin' \"$stage\x64\bin\" -Recurse -Force; Copy-Item '%ROOT_DIR%\bin' \"$stage\x86\bin\" -Recurse -Force }" ^
  "if (Test-Path '%ROOT_DIR%\README.md') { Copy-Item '%ROOT_DIR%\README.md' \"$stage\x64\README.md\" -Force; Copy-Item '%ROOT_DIR%\README.md' \"$stage\x86\README.md\" -Force }"
if %errorlevel% neq 0 (
    echo [ERROR] Portable staging failed!
    pause
    exit /b %errorlevel%
)
if exist "C:\temp\installer\output\StarExtract-%VERSION%-portable-x64.zip" del "C:\temp\installer\output\StarExtract-%VERSION%-portable-x64.zip"
if exist "C:\temp\installer\output\StarExtract-%VERSION%-portable-x86.zip" del "C:\temp\installer\output\StarExtract-%VERSION%-portable-x86.zip"
"%ROOT_DIR%\7z.exe" a -tzip "C:\temp\installer\output\StarExtract-%VERSION%-portable-x64.zip" "C:\temp\installer\portable-staging\x64\*" -mx=9
if %errorlevel% neq 0 (
    echo [ERROR] Portable x64 ZIP creation failed!
    pause
    exit /b %errorlevel%
)
"%ROOT_DIR%\7z.exe" a -tzip "C:\temp\installer\output\StarExtract-%VERSION%-portable-x86.zip" "C:\temp\installer\portable-staging\x86\*" -mx=9
if %errorlevel% neq 0 (
    echo [ERROR] Portable x86 ZIP creation failed!
    pause
    exit /b %errorlevel%
)
:skipportable

if "%NOSETUPZIP%"=="1" goto :skipsetupzip
if exist "C:\temp\installer\output\StarExtract-%VERSION%-setup-x64.zip" del "C:\temp\installer\output\StarExtract-%VERSION%-setup-x64.zip"
if exist "C:\temp\installer\output\StarExtract-%VERSION%-setup-x86.zip" del "C:\temp\installer\output\StarExtract-%VERSION%-setup-x86.zip"
"%ROOT_DIR%\7z.exe" a -tzip "C:\temp\installer\output\StarExtract-%VERSION%-setup-x64.zip" "C:\temp\installer\output\StarExtract-%VERSION%-setup-x64.exe" -mx=9
"%ROOT_DIR%\7z.exe" a -tzip "C:\temp\installer\output\StarExtract-%VERSION%-setup-x86.zip" "C:\temp\installer\output\StarExtract-%VERSION%-setup-x86.exe" -mx=9
:skipsetupzip

echo.
echo Syncing output installers to repository...
if not exist "%ROOT_DIR%\installer" mkdir "%ROOT_DIR%\installer"
copy /y "C:\temp\installer\output\StarExtract-%VERSION%-setup-x64.exe" "%ROOT_DIR%\installer\StarExtract-%VERSION%-setup-x64.exe" >nul
copy /y "C:\temp\installer\output\StarExtract-%VERSION%-setup-x86.exe" "%ROOT_DIR%\installer\StarExtract-%VERSION%-setup-x86.exe" >nul
if "%NOPORTABLE%"=="0" (
    copy /y "C:\temp\installer\output\StarExtract-%VERSION%-portable-x64.zip" "%ROOT_DIR%\installer\StarExtract-%VERSION%-portable-x64.zip" >nul
    copy /y "C:\temp\installer\output\StarExtract-%VERSION%-portable-x86.zip" "%ROOT_DIR%\installer\StarExtract-%VERSION%-portable-x86.zip" >nul
)
if "%NOSETUPZIP%"=="0" (
    copy /y "C:\temp\installer\output\StarExtract-%VERSION%-setup-x64.zip" "%ROOT_DIR%\installer\StarExtract-%VERSION%-setup-x64.zip" >nul
    copy /y "C:\temp\installer\output\StarExtract-%VERSION%-setup-x86.zip" "%ROOT_DIR%\installer\StarExtract-%VERSION%-setup-x86.zip" >nul
)

if "%NOINCREMENT%"=="1" goto :skipincrement
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$f='%ROOT_DIR%\BUILD_NUMBER'; $c=0; if (Test-Path $f) { [int]::TryParse(((Get-Content $f -Raw).Trim()), [ref]$c) | Out-Null }; Set-Content -Path $f -Value ($c+1) -NoNewline; Write-Host \"Build number bumped: $c -> $($c+1)\""
:skipincrement

echo.
echo ======================================================================
echo  [SUCCESS] Star Extract v%VERSION% Installers + ZIPs Generated!
echo ======================================================================
echo.
echo  x64 Installer:
echo    - %ROOT_DIR%\installer\StarExtract-%VERSION%-setup-x64.exe
echo    - C:\temp\installer\output\StarExtract-%VERSION%-setup-x64.exe
echo.
echo  x86 Installer:
echo    - %ROOT_DIR%\installer\StarExtract-%VERSION%-setup-x86.exe
echo    - C:\temp\installer\output\StarExtract-%VERSION%-setup-x86.exe
if "%NOPORTABLE%"=="0" (
    echo.
    echo  Portable ZIPs:
    echo    - %ROOT_DIR%\installer\StarExtract-%VERSION%-portable-x64.zip
    echo    - %ROOT_DIR%\installer\StarExtract-%VERSION%-portable-x86.zip
)
echo ======================================================================
echo.
pause
