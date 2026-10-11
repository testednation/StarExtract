; Star Extract — Inno Setup script
; Compile: ISCC.exe StarExtract-installer.iss /DArch=x64  (or /DArch=x86)
;
; Optional overrides (all have local defaults):
;   /DRepoRoot="C:\path\to\StarExtract"    repo checkout (src, bin, README.md)
;   /DPayloadRoot="C:\temp\installer"    staging + output folder
;   /DAppVersion="1.6.0"                 version embedded in the setup exe
; tools\stage-payload.ps1 prepares the payload folders; CI passes /D overrides
; so it never depends on absolute local paths.
;
; Install modes:
;   Interactive   : user picks "all users (admin)" vs "me only" at start
;   Per-machine   : /ALLUSERS    -> Program Files, HKLM uninstall entry
;   Per-user      : /CURRENTUSER -> %LOCALAPPDATA%\Programs, HKCU uninstall entry
;
; Upgrades: the AppId GUID below is the stable upgrade code. Installing a new
; version with the same AppId replaces the previous install in place. Setup
; also detects installs in *any* registry hive (HKLM x64/x86, HKCU), prefills
; the previous directory, and reports the version it is upgrading from.

#define MyAppName "Star Extract"
#define MyAppPublisher "Star Extract Project"
#ifdef NativeOnly
  ; Native-only installer ships the Win32 C++ edition (no .NET dependency)
  #define MyAppExeName "StarExtractNative.exe"
#else
  #define MyAppExeName "StarExtract.exe"
#endif
; Stable upgrade code — never change this between releases.
#define MyAppId "{7E1A2C64-9B3D-4E7F-9C2A-5F0D8B1A4E6C}"

; ---- overridable paths / version (defaults = this developer machine) ----
#ifndef RepoRoot
  #define RepoRoot "C:\Users\Administrator\Documents\StarExtract"
#endif
#define RepoRootN RemoveBackslashUnlessRoot(RepoRoot)
#ifndef PayloadRoot
  #define PayloadRoot "C:\temp\installer"
#endif
#define PayloadRootN RemoveBackslashUnlessRoot(PayloadRoot)
#ifndef AppVersion
  #define AppVersion "1.6.0.1"
#endif
#define MyAppVersion AppVersion
; AppVersion is always the full 4-part version incl. build number
; (build scripts pass /DAppVersion=<VERSION>.<BUILD_NUMBER>).

#ifndef Arch
  #define Arch "x64"
#endif

#if Arch == "x64"
  #ifdef NativeOnly
    #define PayloadDir PayloadRootN + "\payload-nat-x64"
  #else
    #define PayloadDir PayloadRootN + "\payload-x64"
  #endif
  #define OutSuffix "x64"
#else
  #ifdef NativeOnly
    #define PayloadDir PayloadRootN + "\payload-nat-x86"
  #else
    #define PayloadDir PayloadRootN + "\payload-x86"
  #endif
  #define OutSuffix "x86"
#endif

[Setup]
AppId={{#MyAppId}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion} ({#Arch})
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
UninstallDisplayName={#MyAppName} {#MyAppVersion}
UninstallDisplayIcon={app}\{#MyAppExeName}
#ifdef NativeOnly
  SetupIconFile={#RepoRootN}\native\res\app.ico
#else
  SetupIconFile={#RepoRootN}\src\app.ico
#endif
WizardStyle=modern
; Default to per-machine, but let the user (or /CURRENTUSER // /ALLUSERS) override.
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog commandline
#if Arch == "x64"
ArchitecturesInstallIn64BitMode=x64
#endif
ArchitecturesAllowed=x86 x64
; Close a running instance before replacing files (Restart Manager).
CloseApplications=yes
RestartApplications=no
; File-version metadata on the setup binary itself.
VersionInfoVersion={#MyAppVersion}
VersionInfoTextVersion={#MyAppVersion}
VersionInfoDescription={#MyAppName} {#MyAppVersion} ({#Arch}) Setup
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}
VersionInfoCompany={#MyAppPublisher}
VersionInfoCopyright=MIT-style license; see bundled licenses
OutputDir={#PayloadRootN}\output
#ifdef NativeOnly
  OutputBaseFilename=StarExtract-{#MyAppVersion}-native-setup-{#OutSuffix}
#else
  OutputBaseFilename=StarExtract-{#MyAppVersion}-setup-{#OutSuffix}
#endif
Compression=lzma2/normal
SolidCompression=yes
LZMANumBlockThreads=4
MinVersion=6.1sp1

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "contextmenu"; Description: "Add Star Extract context menu options for all files and folders"; GroupDescription: "Windows Explorer Integration"; Flags: checkedonce

[Files]
; Application + arch-matched 7-Zip core + codecs + help
Source: "{#PayloadDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Excludes: "*.pdb"
; UniExtract plugin suite (arch-neutral, shared by both installers)
Source: "{#RepoRootN}\bin\*"; DestDir: "{app}\bin"; Flags: ignoreversion recursesubdirs createallsubdirs
; Project readme
Source: "{#RepoRootN}\README.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Parameters: "/register"; Tasks: contextmenu; Flags: runhidden
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{app}\{#MyAppExeName}"; Parameters: "/unregister"; RunOnceId: "UnregisterContextMenu"; Flags: runhidden

[Code]
const
  UninstKey = 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{#MyAppId}_is1';

var
  PrevInstallDir: string;
  PrevVersion: string;

// Looks for a previous install in HKLM (64/32-bit views) and HKCU.
// Handles upgrades across per-machine <-> per-user mode changes.
function FindPrevInstall(): Boolean;
var
  loc, ver: string;
begin
  Result := False;
  if RegQueryStringValue(HKLM64, UninstKey, 'InstallLocation', loc) then
    Result := True
  else if RegQueryStringValue(HKLM32, UninstKey, 'InstallLocation', loc) then
    Result := True
  else if RegQueryStringValue(HKCU, UninstKey, 'InstallLocation', loc) then
    Result := True;
  if Result then
  begin
    PrevInstallDir := loc;
    PrevVersion := '';
    if RegQueryStringValue(HKLM64, UninstKey, 'DisplayVersion', ver) or
       RegQueryStringValue(HKLM32, UninstKey, 'DisplayVersion', ver) or
       RegQueryStringValue(HKCU, UninstKey, 'DisplayVersion', ver) then
      PrevVersion := ver;
  end;
end;

function InitializeSetup(): Boolean;
#ifndef NativeOnly
var
  netRelease: Cardinal;
#endif
begin
  Result := True;

#ifndef NativeOnly
  // .NET Framework 4.8 = Release 528040 (Win10 1903+ ships it built in)
  if not RegQueryDWordValue(HKLM, 'SOFTWARE\Microsoft\NET Framework Setup\NDP\v4\Full', 'Release', netRelease) then
    netRelease := 0;
  if netRelease < 528040 then
  begin
    if MsgBox('Star Extract requires Microsoft .NET Framework 4.8, which was not detected.' #13#10 #13#10 'Install it from:' #13#10 'https://dotnet.microsoft.com/download/dotnet-framework/net48' #13#10 #13#10 'Continue setup anyway?', mbCriticalError, MB_YESNO) = IDNO then
    begin
      Result := False;
      Exit;
    end;
  end;
#endif

  // Upgrade detection: remember the previous install (any hive/mode) and
  // tell the user what they are upgrading — never touch WizardForm here
  // (it does not exist yet, and not at all in silent mode).
  if FindPrevInstall() and (not WizardSilent()) then
  begin
    if PrevVersion = '{#MyAppVersion}' then
      MsgBox('{#MyAppName} {#MyAppVersion} is already installed at:' #13#10 + PrevInstallDir + #13#10 #13#10 'Setup will repair/reinstall it.', mbInformation, MB_OK)
    else
      MsgBox('Found {#MyAppName} ' + PrevVersion + ' at:' #13#10 + PrevInstallDir + #13#10 #13#10 'Setup will upgrade it to {#MyAppVersion}.', mbInformation, MB_OK);
  end;
end;

// Keep the directory page in sync with a detected previous install.
procedure CurPageChanged(CurPageID: Integer);
begin
  if (CurPageID = wpSelectDir) and (PrevInstallDir <> '') and DirExists(PrevInstallDir) then
    WizardForm.DirEdit.Text := PrevInstallDir;
end;

// Block a downgrade unless the user insists.
function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = wpSelectDir) and (PrevVersion <> '') and (PrevVersion <> '{#MyAppVersion}') then
  begin
    // Simple numeric-ish compare on "major.minor.patch"
    if CompareStr('{#MyAppVersion}', PrevVersion) < 0 then
      Result := MsgBox('The installed version (' + PrevVersion + ') is NEWER than the one you are installing ({#MyAppVersion}).' #13#10 'Downgrade anyway?', mbConfirmation, MB_YESNO) = IDYES;
  end;
end;
