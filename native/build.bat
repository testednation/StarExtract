@echo off
setlocal enabledelayedexpansion

set "MSVC=C:\buildtools\VC\Tools\MSVC\14.44.35207"
set "WINSDK=C:\Program Files (x86)\Windows Kits\10"
set "SDKVER=10.0.22621.0"

:: Version: optional %1 = full version (1.6.0.1), else read repo files.
:: Optional %2 = "nobump" to skip the BUILD_NUMBER increment (release pipeline
:: owns the bump in that case so installer + native versions stay identical).
:: Optional %3 = target arch: x64 (default) or x86; x86 output goes to x86\.
set "BASEVER=1.6.0"
set "BUILDNUM=0"
if not "%~1"=="" (
    for /f "tokens=1-4 delims=." %%a in ("%~1") do (
        set "BASEVER=%%a.%%b.%%c"
        set "BUILDNUM=%%d"
    )
) else (
    if exist "..\VERSION" (
        for /f "usebackq delims=" %%V in ("..\VERSION") do set "BASEVER=%%V"
    )
    if exist "..\BUILD_NUMBER" (
        for /f "usebackq delims=" %%B in ("..\BUILD_NUMBER") do set "BUILDNUM=%%B"
    )
)
for /f "tokens=* delims= " %%V in ("%BASEVER%") do set "BASEVER=%%V"
for /f "tokens=* delims= " %%B in ("%BUILDNUM%") do set "BUILDNUM=%%B"
set "VERSION=%BASEVER%.%BUILDNUM%"

set "ARCH=x64"
if /I "%~3"=="x86" set "ARCH=x86"
set "OUTEXE=StarExtractNative.exe"
set "LIB_MSVC=%MSVC%\lib\x64"
set "LIB_SDK=%WINSDK%\Lib\%SDKVER%\um\x64"
if "%ARCH%"=="x86" (
    set "OUTEXE=x86\StarExtractNative.exe"
    set "LIB_MSVC=%MSVC%\lib\x86"
    set "LIB_SDK=%WINSDK%\Lib\%SDKVER%\um\x86"
    if not exist x86 mkdir x86
)
echo Building Star Extract v%VERSION% (%ARCH%)

if "%ARCH%"=="x86" (
    call "C:\buildtools\VC\Auxiliary\Build\vcvars32.bat" >nul 2>&1
) else (
    call "C:\buildtools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
)

if errorlevel 1 (
    echo [ERROR] Failed to initialize MSVC environment
    exit /b 1
)

echo [1/3] Compiling StarExtractNative.cpp + se_ui.cpp + se_tools.cpp + se_components.cpp...
cl.exe /nologo /EHsc /O2 /W4 /DUNICODE /D_UNICODE ^
    /DSE_VERSION_MAJOR=%BASEVER:~0,1% ^
    /DSE_VERSION_MINOR=%BASEVER:~2,1% ^
    /DSE_VERSION_PATCH=%BASEVER:~4,1% ^
    /DSE_VERSION_BUILD=%BUILDNUM% ^
    /I"%MSVC%\include" /I"%WINSDK%\Include\%SDKVER%\ucrt" /I"%WINSDK%\Include\%SDKVER%\um" /I"%WINSDK%\Include\%SDKVER%\shared" ^
    /Fe:%OUTEXE% ^
    StarExtractNative.cpp se_ui.cpp se_tools.cpp se_components.cpp ^
    /link /SUBSYSTEM:WINDOWS ^
    /LIBPATH:"%LIB_MSVC%" /LIBPATH:"%LIB_SDK%" ^
    kernel32.lib user32.lib gdi32.lib comctl32.lib comdlg32.lib shell32.lib ole32.lib uuid.lib msimg32.lib crypt32.lib advapi32.lib shlwapi.lib wininet.lib

if errorlevel 1 (
    echo [ERROR] Compilation failed!
    exit /b 1
)

echo [2/3] Creating version resource...
(
echo #include ^<windows.h^>
echo VS_VERSION_INFO VERSIONINFO
echo FILEVERSION %BASEVER:~0,1%,%BASEVER:~2,1%,%BASEVER:~4,1%,%BUILDNUM%
echo PRODUCTVERSION %BASEVER:~0,1%,%BASEVER:~2,1%,%BASEVER:~4,1%,%BUILDNUM%
echo FILEFLAGSMASK 0x3fL
echo FILEFLAGS 0x0L
echo FILEOS 0x40004L
echo FILETYPE 0x1L
echo FILESUBTYPE 0x0L
echo BEGIN
echo     BLOCK "StringFileInfo"
echo     BEGIN
echo         BLOCK "040904b0"
echo         BEGIN
echo             VALUE "CompanyName", "Star Extract Project"
echo             VALUE "FileDescription", "Star Extract - Native Edition"
echo             VALUE "FileVersion", "%VERSION%"
echo             VALUE "InternalName", "StarExtractNative"
echo             VALUE "LegalCopyright", "MIT-style license"
echo             VALUE "OriginalFilename", "StarExtractNative.exe"
echo             VALUE "ProductName", "Star Extract"
echo             VALUE "ProductVersion", "%VERSION%"
echo         END
echo     END
echo     BLOCK "VarFileInfo"
echo     BEGIN
echo         VALUE "Translation", 0x409, 1200
echo     END
echo END
echo.
echo 101 ICON "app.ico"
) > res\version.rc

rc.exe /fo res\version.res res\version.rc
if errorlevel 1 (
    echo [WARNING] Resource compilation failed, continuing without version resource...
) else (
    echo [3/3] Linking with version resource...
    link.exe /SUBSYSTEM:WINDOWS /OUT:%OUTEXE% ^
        StarExtractNative.obj se_ui.obj se_tools.obj se_components.obj res\version.res ^
        /LIBPATH:"%LIB_MSVC%" /LIBPATH:"%LIB_SDK%" ^
        kernel32.lib user32.lib gdi32.lib comctl32.lib comdlg32.lib shell32.lib ole32.lib uuid.lib msimg32.lib crypt32.lib advapi32.lib shlwapi.lib wininet.lib
)

if errorlevel 1 (
    echo [ERROR] Linking failed!
    exit /b 1
)

echo.
echo [SUCCESS] Star Extract v%VERSION% (%ARCH%) built!
echo Output: %CD%\%OUTEXE%

:: Bump build number (skipped when the release pipeline passes "nobump" -
:: build-installers.ps1 performs the single authoritative increment)
if /I "%~2"=="nobump" goto :nobump
powershell -NoProfile -ExecutionPolicy Bypass -Command "$f='..\BUILD_NUMBER'; $c=0; if (Test-Path $f) { [int]::TryParse(((Get-Content $f -Raw).Trim()), [ref]$c) | Out-Null }; Set-Content -Path $f -Value ($c+1) -NoNewline; Write-Host ('Build number bumped from ' + $c + ' to ' + ($c+1))"
:nobump

endlocal
