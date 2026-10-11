// StarExtract Native - shared header (C++ Win32, no .NET)
#pragma once

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <wincrypt.h>
#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "msimg32.lib")

// Visual styles for standard controls (progress bar, listview headers)
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

// ---- version (overridable via /D from build.bat) ----
#ifndef SE_VERSION_MAJOR
#define SE_VERSION_MAJOR 1
#endif
#ifndef SE_VERSION_MINOR
#define SE_VERSION_MINOR 6
#endif
#ifndef SE_VERSION_PATCH
#define SE_VERSION_PATCH 0
#endif
#ifndef SE_VERSION_BUILD
#define SE_VERSION_BUILD 0
#endif
#define SE_VER_STR2(x) L#x
#define SE_VER_STR(x) SE_VER_STR2(x)
#define SE_VERSION_STR SE_VER_STR(SE_VERSION_MAJOR) L"." SE_VER_STR(SE_VERSION_MINOR) L"." SE_VER_STR(SE_VERSION_PATCH) L"." SE_VER_STR(SE_VERSION_BUILD)
#define SE_PRODUCT_NAME L"Star Extract"

// icon resource id (native\res\app.ico, embedded at build time)
#define IDR_APP_ICON 101

// streaming-job notifications (posted from the worker thread to the UI thread)
#define WM_APP_STREAM_LINE (WM_APP + 21)
#define WM_APP_STREAM_DONE (WM_APP + 22)
// component download / install job
#define WM_APP_COMP_PROGRESS (WM_APP + 23)
#define WM_APP_COMP_DONE (WM_APP + 24)

// ---- themes ----
enum class Theme { Dark = 0, Light = 1, HighContrast = 2, Midnight = 3, Ocean = 4, Rose = 5, Solarized = 6, Custom = 7, Count = 8 };

struct ThemeColors {
    COLORREF bgWindow, bgSurface, bgHover, bgPressed;
    COLORREF accent, accentHover, accentSoft;
    COLORREF textPrimary, textSecondary, textMuted;
    COLORREF border, success, warning, danger;
    COLORREF tipBg, tipBorder, logBg, logText, navBg, cardBg;
};

ThemeColors GetThemeColors(Theme t);
const wchar_t* GetThemeName(Theme t);
const wchar_t* GetThemeDesc(Theme t);
Theme ThemeFromName(const wchar_t* name);
COLORREF MixColor(COLORREF a, COLORREF b, double t);
void FillGradient(HDC hdc, const RECT& rc, COLORREF top, COLORREF bottom);
extern COLORREF g_customAccent;

// ---- UI scale ----
struct UIScale {
    double zoom = 1.0;
    int btnH, btnW, navW, statusH, tipH, margin, cardW, cardH, fontUI, fontBig, fontTip, fontHero;
    void Apply(double z);
};

// ---- pages ----
enum class Page { Home = 0, Browser, Extract, Compress, Tools, Settings, Count };

// ---- control ids ----
enum {
    ID_NAV_HOME = 100, ID_NAV_BROWSER, ID_NAV_EXTRACT, ID_NAV_COMPRESS, ID_NAV_TOOLS, ID_NAV_SETTINGS,
    ID_CARD_OPEN = 200, ID_CARD_EXTRACT, ID_CARD_COMPRESS,
    ID_TIP_NEXT, ID_TIP_TEXT, ID_CARD_TIP,
    ID_TREE_LIST = 299, ID_BREADCRUMB_BAR, ID_BTN_BACK, ID_BTN_FWD, ID_BTN_UP2,
    ID_BROWSE_PATH, ID_BTN_OPEN, ID_BTN_UP, ID_BTN_EXTRACT_ALL, ID_BTN_EXTRACT_SEL,
    ID_BTN_PREVIEW, ID_BTN_TEST2, ID_BTN_DELETE2, ID_FILE_LIST, ID_ENGINE_BADGE,
    ID_TXT_FILTER, ID_BTN_7ZFM, ID_BTN_EXTRACT_TO,
    ID_TXT_SRC = 400, ID_BTN_SRC_BROWSE, ID_TXT_DEST, ID_BTN_DEST_BROWSE, ID_BTN_GO,
    ID_CHK_SUBFOLDER, ID_CHK_OPEN_AFTER, ID_CHK_DELETE_AFTER,
    ID_TXT_PASSWORD, ID_CHK_REMEMBER_PWD, ID_ENGINE_LABEL, ID_PROGRESS, ID_TXT_PROGRESS,
    ID_TXT_PERCENT, ID_BTN_CANCEL,
    ID_COMP_LIST = 500, ID_BTN_COMP_ADD, ID_BTN_COMP_REMOVE, ID_BTN_COMP_CLEAR,
    ID_RAD_FMT_7Z, ID_RAD_FMT_ZIP, ID_RAD_MODE_COMBINE, ID_RAD_MODE_SEPARATE,
    ID_TXT_COMP_OUT, ID_BTN_COMP_OUT_BROWSE, ID_BTN_COMP_GO,
    ID_PLUGIN_GRID = 510, ID_BTN_PLUGIN_UPDATE_ALL, ID_BTN_PLUGIN_ADD, ID_TXT_PLUGIN_NAME, ID_TXT_PLUGIN_URL, ID_TXT_PLUGIN_VERIFY,
    ID_BTN_PLUGIN_DOWNLOAD = 520, ID_BTN_PLUGIN_SAVEURL, ID_BTN_PLUGIN_RESET,
    ID_BTN_PLUGIN_REMOVE, ID_LBL_PLUGIN_URL, ID_LBL_PLUGIN_NAME, ID_LBL_PLUGIN_VERIFY,
    ID_TXT_PLUGIN_STATUS, ID_BTN_PLUGIN_CANCEL,
    ID_TOOL_GRID = 600, ID_BTN_TOOL_REFRESH, ID_BTN_HASH, ID_BTN_IDENTIFY,
    ID_RAD_THEME_BEGIN = 700, // +0..+7 theme preview cards (see Theme enum)
    ID_BTN_CUSTOM_ACCENT = 708,
    ID_BTN_ZOOM_OUT = 710, ID_BTN_ZOOM_RESET, ID_BTN_ZOOM_IN, ID_TOG_TIPS,
    ID_VAULT_LIST, ID_TXT_NEW_PASSWORD, ID_BTN_ADD_PASSWORD, ID_BTN_REMOVE_PASSWORD, ID_BTN_CLEAR_PASSWORDS,
    ID_BTN_REGISTER_MENU, ID_BTN_UNREGISTER_MENU,
    ID_RAD_DBL_EXTRACT = 721, ID_RAD_DBL_OPEN, ID_BTN_UNREG_SHELL,
    ID_LOG_EDIT = 800, ID_BTN_CLEAR_LOG, ID_BTN_SAVE_LOG,
    ID_STATUS_BAR,
};

// ---- one row of `7z l -slt` ----
struct ArchiveEntry {
    std::wstring path;     // path inside the archive, backslash separated
    std::wstring name;     // last component of path
    std::wstring modified; // "YYYY-MM-DD HH:MM:SS" as printed by 7z, or empty
    std::wstring attrs;    // raw Attributes line (A, R, H, D, ...)
    unsigned long long size = 0;
    unsigned long long packed = 0;
    bool isDir = false;
    bool encrypted = false;
    bool isArchiveSelf = false; // the archive's own header block, not a row
};

// ---- app state ----
struct AppState {
    HWND hwndMain = nullptr, hwndStatus = nullptr, hwndLog = nullptr, hwndNav = nullptr;
    HWND hwndFileList = nullptr, hwndDestEdit = nullptr, hwndTxtProgress = nullptr;
    HWND hwndBtnStart = nullptr, hwndProgress = nullptr;
    HWND hwndChkSubfolder = nullptr, hwndChkOpenAfter = nullptr, hwndChkDeleteAfter = nullptr;
    HWND hwndTreeList = nullptr, hwndBreadcrumbBar = nullptr;
    HWND hwndPathLabel = nullptr, hwndEngineBadge = nullptr;
    HWND hwndBtnBack = nullptr, hwndBtnFwd = nullptr, hwndBtnUp2 = nullptr;
    HWND hwndFilter = nullptr, hwndTxtPercent = nullptr, hwndBtnCancel = nullptr;
    HWND hwndPluginGrid = nullptr;
    HWND hwndBtnPluginUpdateAll = nullptr, hwndBtnPluginAdd = nullptr;
    HWND hwndTxtPluginName = nullptr, hwndTxtPluginUrl = nullptr, hwndTxtPluginVerify = nullptr;
    HWND hwndVaultList = nullptr;
    HFONT hFontUI = nullptr, hFontBig = nullptr, hFontTip = nullptr, hFontBold = nullptr, hFontHero = nullptr;
    Theme currentTheme = Theme::Dark;
    ThemeColors colors;
    UIScale ui;
    Page page = Page::Home;
    std::wstring currentArchive;
    std::vector<ArchiveEntry> archiveEntries;
    std::wstring currentFolder; // browser subfolder, backslash separated, "" = root
    std::vector<std::wstring> folderHistory;
    size_t historyPos = 0;
    std::vector<std::wstring> pluginEntries; // name, url, verify file path, installed status
    bool browserTreeExpanded = true;
    bool isExtracting = false;
    bool archiveEncrypted = false;
    unsigned long long archiveTotalSize = 0;
    std::wstring filterText;   // live filter typed in the browser search box
    bool showTips = true;
    bool doubleExtract = true;
    std::wstring pendingOpen; // file to open in Browser when launched with a bare path
    std::vector<std::wstring> passwords;
    int tipIndex = 0;
    static constexpr double ZOOM_MIN = 0.5, ZOOM_MAX = 2.0, ZOOM_STEP = 0.1;
};

extern AppState g_state;

// ---- se_ui.cpp ----
void RegisterCustomControls(HINSTANCE hInst);
HWND CreateCard(HWND parent, int id, const wchar_t* title, const wchar_t* sub, int iconKind, bool* selectedFlag);
HWND CreateThemeCard(HWND parent, int id, Theme t, bool* selectedFlag);
HWND CreateToggle(HWND parent, int id, bool initial);
bool Toggle_Get(HWND h);
void Toggle_Set(HWND h, bool on);
HWND CreateTooltip(HWND hwndMain);
void AddTip(HWND hwndTips, HWND hwndTool, const wchar_t* text);
void DrawRoundRect(HDC hdc, const RECT& rc, int radius, HBRUSH fill, HPEN pen);
void DrawIconGlyph(HDC hdc, RECT rc, int kind, COLORREF color);
HFONT MakeFont(int px, bool bold);
std::wstring FitText(HDC hdc, const std::wstring& s, int maxPx);

// icon kinds for cards / nav
enum { ICON_FOLDER = 1, ICON_DOWN, ICON_BOX, ICON_GEAR, ICON_CHECK, ICON_MAG, ICON_TAG, ICON_BOLT };

// ---- se_tools.cpp ----
std::wstring GetExeDir();
std::wstring Get7zPath();
std::wstring GetLessMsiPath();
std::wstring GetInnounpPath();
std::wstring GetUnshieldPath();
std::wstring GetPeaPath();
std::wstring GetWisePath();
std::wstring GetTridPath();
bool FileExists(const std::wstring& path);
bool DirectoryExists(const std::wstring& path);
std::wstring GetWindowTextString(HWND hwnd);
void SetWindowTextString(HWND hwnd, const std::wstring& text);
void RunProcess(const std::wstring& exe, const std::wstring& args, const std::wstring& workDir = L"");
std::wstring RunProcessCapture(const std::wstring& exe, const std::wstring& args);
// Runs a tool on a worker thread and streams its output to the UI as
// WM_APP_STREAM_LINE (lParam = new std::wstring, caller deletes) followed by
// WM_APP_STREAM_DONE (wParam = exit code). Returns false if the process could
// not be started.
bool RunProcessStream(const std::wstring& exe, const std::wstring& args, HWND notify);
void CancelStreamedProcess();
bool IsStreamedProcessRunning();
std::wstring DetectEngine(const std::wstring& filePath);
std::vector<ArchiveEntry> ListArchiveEntries(const std::wstring& archive, const std::wstring& password = L"");
unsigned long long TotalPackedSize(const std::vector<ArchiveEntry>& entries);
bool EntriesEncrypted(const std::vector<ArchiveEntry>& entries);
std::wstring FormatSize(unsigned long long bytes);
void LoadPasswords();
void SavePasswords();
void AddPassword(const std::wstring& pwd);
void RemovePassword(int index);
void ClearPasswords();
bool TryVaultPasswords(const std::wstring& archive); // returns true if one works; out via param
bool TryVaultPasswords(const std::wstring& archive, std::wstring& working);
void RegisterContextMenu();
void UnregisterContextMenu();
bool RegisterShellAssociation(bool quickExtract); // double-click archive behavior
void UnregisterShellAssociation();
bool IsShellAssociationRegistered();
void LoadUiSettings();   // theme / accent / zoom / tips from HKCU
void SaveUiSettings();
std::wstring IdentifyFile(const std::wstring& path); // magic-byte sniff
std::wstring HashFile(const std::wstring& path, const std::wstring& algo); // via 7z h
struct EngineInfo { const wchar_t* name; const wchar_t* handles; std::wstring path; bool present; };
std::vector<EngineInfo> GetEngineStatus();
bool IsArchiveLike(const std::wstring& path);

void RefreshBrowserList();
void BrowserOpenSelected();
void DoPreviewSelected();
void UpdateBrowserStatus();        // item / selection / size / encryption readout
void SetProgress(int percent);     // progress bar + "NN%" label
void SetCurrentItemText(const std::wstring& text);
void ResetProgressUI();
// ---- main ----
void SetStatus(const std::wstring& msg);
void LogMessage(const std::wstring& msg);
void ShowPage(Page p);
void ApplyTheme(Theme t);
void ApplyZoom(double z);
void UpdateTitleBar();
HWND GetItem(HWND hwnd, int id);
std::wstring GetAppVersionString();
void RefreshBrowserTree();          // rebuild folder tree from archiveEntries
void RefreshBreadcrumbs();          // rebuild breadcrumb bar
void PushHistory(const std::wstring& folder);
void NavigateBrowser(const std::wstring& folder, bool pushHistory = true);
void RefreshPluginGrid();           // rebuild plugin list

// ---- se_components.cpp ----
// Component is opaque; the UI reads/writes it through the field accessors below.
struct CompResult { int code; std::wstring message; std::wstring sha256; };
std::wstring Sha256OfFile(const std::wstring& path);
void LoadComponents();
void SaveComponents();
void ResetComponentsToDefaults();
void RefreshComponentStatus();
bool IsComponentJobRunning();
void CancelComponentJob();
bool StartComponentInstall(int index, HWND notify);
int  AddCustomComponent(const std::wstring& name, const std::wstring& url, const std::wstring& verify);
bool RemoveComponentAt(int index);
void SetComponentUrl(int index, const std::wstring& url);
void PinComponentHash(int index, const std::wstring& sha);
std::wstring ComponentField(int index, int field);   // see FIELD_* below
void SetComponentField(int index, int field, const std::wstring& value);
int  ComponentCount();
enum {
    COMP_FIELD_NAME = 0, COMP_FIELD_STATUS, COMP_FIELD_INSTALLED, COMP_FIELD_LATEST,
    COMP_FIELD_URL, COMP_FIELD_VERIFY, COMP_FIELD_CATEGORY, COMP_FIELD_FORMATS,
    COMP_FIELD_SHA256, COMP_FIELD_CUSTOM, COMP_FIELD_COUNT
};