@echo off
setlocal enabledelayedexpansion

:: Unity build - compile all .cpp in one invocation
:: This is ~3x faster than separate compilations

set "MSVC=C:\buildtools\VC\Tools\MSVC\14.44.35207"
set "WINSDK=C:\Program Files (x86)\Windows Kits\10"
set "SDKVER=10.0.22621.0"

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
    if exist "..\BUILD_NUMBER" (
        for /f "usebackq delims=" %%B in ("..\BUILD_NUMBER") do set "BUILDNUM=%%B"
)
for /f "tokens=* delims= " %%V in ("%BASEVER%") do set "BASEVER=%%V"
for /f "tokens=* delims= " %%B in ("%BUILDNUM%") do set "BUILDNUM=%%B"
set "VERSION=%BASEVER%.%BUILDNUM%"

set "ARCH=x64"
if /I "%~3"=="x86" set "ARCH=x86"
set "OUTEXE=StarExtractNative.exe"

echo Building Star Extract v%VERSION% (%ARCH%) [Unity Build]...
echo [1/3] Compiling all sources...

"C:\buildtools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe" /nologo /EHsc /W3 /DUNICODE /D_UNICODE ^
    /DSE_VERSION_MAJOR=%BASEVER:~0,1% ^
    /DSE_VERSION_MINOR=%BASEVER:~2,1% ^
    /DSE_VERSION_PATCH=%BASEVER:~4,1% ^
    /DSE_VERSION_BUILD=%BUILDNUM% ^
    /I"%MSVC%\include" ^
    /I"%WINSDK%\Include\%SDKVER%\ucrt" ^
    /I"%WINSDK%\Include\%SDKVER%\um" ^
    /I"%WINSDK%\Include\%SDKVER%\shared" ^
    /Fe:%OUTEXE% ^
    StarExtractNative.cpp se_ui.cpp se_tools.cpp ^
    /link /SUBSYSTEM:WINDOWS ^
    /LIBPATH:"%MSVC%\lib\x64" ^
    /LIBPATH:"%WINSDK%\Lib\%SDKVER%\um\x64" ^
    kernel32.lib user32.lib gdi32.lib comctl32.lib comdlg32.lib shell32.lib ole32.lib uuid.lib msimg32.lib crypt32.lib advapi32.lib shlwapi.lib

if errorlevel 1 (
    echo [ERROR] Build failed!
    exit /b 1
)

echo [2/3] Creating version resource...
(
echo #include ^<windows.h^>
echo VS_VERSION_INFO VERSIONINFO
echo FILEVERSION %BASEVER:~0,1%,%BASEVER:~2,1%,%BASEVER:~4,1%,%BUILDNUM%
echo FILEFLAGSMASK 0x3fL
echo FILEFLAGS 0x0L
echo FILEOS 0x40004L
echo FILETYPE 0x1L
echo FILESUBTYPE 0x0L
echo BEGIN
echo     BLOCK "StringFileInfo"
echo         BLOCK "040904b0"
echo             VALUE "CompanyName", "Star Extract Project"
echo             VALUE "FileDescription", "Star Extract - Native Edition"
echo             VALUE "FileVersion", "%VERSION%"
echo             VALUE "InternalName", "StarExtractNative"
echo             VALUE "LegalCopyright", "MIT-style license"
echo             VALUE "OriginalFilename", "StarExtractNative.exe"
echo             VALUE "ProductName", "Star Extract"
echo             VALUE "ProductVersion", "%VERSION%"
echo         END
echo     BLOCK "VarFileInfo"
echo         VALUE "Translation", 0x409, 1200
echo.
echo 101 ICON "app.ico"
) > res\version.rc

"C:\buildtools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\rc.exe" /fo res\version.res res\version.rc
if errorlevel 1 (
    echo [WARNING] Resource compilation failed, continuing without version resource...
) else (
    echo [3/3] Linking with version resource...
    "C:\buildtools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe" /SUBSYSTEM:WINDOWS /OUT:%OUTEXE% ^
        StarExtractNative.obj se_ui.obj se_tools.obj res\version.res ^
        /LIBPATH:"%MSVC%\lib\x64" ^
        /LIBPATH:"%WINSDK%\Lib\%SDKVER%\um\x64" ^
        kernel32.lib user32.lib gdi32.lib comctl32.lib comdlg32.lib shell32.lib ole32.lib uuid.lib msimg32.lib crypt32.lib advapi32.lib shlwapi.lib

    if errorlevel 1 (
        echo [ERROR] Linking failed!
        exit /b 1
    )
)

echo [SUCCESS] Star Extract v%VERSION% (%ARCH%) built!
echo Output: %CD%\%OUTEXE%

:: Bump build number
if /I "%~2"=="nobump" goto :nobump
powershell -NoProfile -ExecutionPolicy Bypass -Command "$f='..\BUILD_NUMBER'; $c=0; if (Test-Path $f) { [int]::TryParse(((Get-Content $f -Raw).Trim()), [ref]$c) | Out-Null }; Set-Content -Path $f -Value ($c+1) -NoNewline; Write-Host ('Build number bumped from ' + $c + ' to ' + ($c+1))"
:nobump

endlocal
