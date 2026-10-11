// StarExtract Native - main window: beginner-first page UX (C++ Win32, no .NET)
#include "se.h"

// ---------- module globals ----------
static HWND g_hTips = nullptr;
static HBRUSH g_hbrWindow = nullptr, g_hbrSurface = nullptr;
static bool g_themeSel[(int)Theme::Count] = { true, false, false, false, false, false, false, false };
struct CtlTag { HWND h; Page p; };
static std::vector<CtlTag> g_ctls;
static std::vector<std::wstring> g_compFiles;
static HWND g_heroTitle = nullptr, g_heroSub = nullptr, g_stepsLabel = nullptr, g_tipText = nullptr;
static HWND g_pathLabel = nullptr, g_engineBadge = nullptr, g_browserHint = nullptr;
static HWND g_extractHint = nullptr, g_compHint = nullptr, g_compNote = nullptr, g_compOutLabel = nullptr;
static HWND g_toolResult = nullptr;
static HWND g_exLbl1 = nullptr, g_exLbl2 = nullptr, g_exPwdLbl = nullptr, g_exEngCap = nullptr;
static HWND g_lblTheme = nullptr, g_lblAccent = nullptr, g_lblZoom = nullptr, g_lblTips = nullptr;
static HWND g_lblVault = nullptr, g_lblCtx = nullptr, g_lblSetNote = nullptr, g_lblDbl = nullptr;
static HWND g_lblPluginUrl = nullptr, g_lblPluginName = nullptr;
static std::wstring g_themeTips[(int)Theme::Count];

static const wchar_t* kTips[] = {
    L"Tip 1 of 6: Drag && drop ANY file onto this window. Archives open for extraction, other files go to Compress.",
    L"Tip 2 of 6: Office files, Android apps (.apk) and e-books (.epub) are ZIPs inside. Open them like any archive.",
    L"Tip 3 of 6: MSI installers always extract with LessMSI automatically. Watch for Engine: LessMSI.",
    L"Tip 4 of 6: Saved passwords are encrypted with Windows DPAPI and tried automatically on locked archives.",
    L"Tip 5 of 6: Press F2 to switch themes. Press Ctrl + Plus / Minus to zoom the whole interface.",
    L"Tip 6 of 6: Register the right-click menu, then compress right from Explorer with Separate or Combine.",
};

static void Tag(HWND h, Page p) { if (h) g_ctls.push_back({ h, p }); }
static void AddTipCtl(HWND h, const wchar_t* t) { AddTip(g_hTips, h, t); }
static int Z(int v) { return (int)(v * g_state.ui.zoom); }

std::wstring GetAppVersionString() {
    return std::wstring(SE_PRODUCT_NAME) + L" v" + SE_VERSION_STR;
}
void UpdateTitleBar() {
    std::wstring t = GetAppVersionString() + L"  |  " + GetThemeName(g_state.currentTheme) +
        L"  |  Zoom " + std::to_wstring((int)(g_state.ui.zoom * 100)) + L"%";
    if (g_state.hwndMain) SetWindowText(g_state.hwndMain, t.c_str());
}
HWND GetItem(HWND hwnd, int id) { return GetDlgItem(hwnd, id); }

static void SetStatusPart(int part, const std::wstring& msg) {
    if (!g_state.hwndStatus) return;
    SendMessage(g_state.hwndStatus, SB_SETTEXT, (WPARAM)part, (LPARAM)msg.c_str());
}
void SetStatus(const std::wstring& msg) { SetStatusPart(0, msg); }

static std::wstring TimeStamp() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t b[16];
    swprintf(b, 16, L"%02d:%02d:%02d", st.wHour, st.wMinute, st.wSecond);
    return b;
}
void LogMessage(const std::wstring& msg) {
    if (!g_state.hwndLog) return;
    std::wstring e = GetWindowTextString(g_state.hwndLog);
    if (!e.empty()) e += L"\r\n";
    e += L"[" + TimeStamp() + L"] " + msg;
    SetWindowTextString(g_state.hwndLog, e);
    SendMessage(g_state.hwndLog, EM_LINESCROLL, 0, SendMessage(g_state.hwndLog, EM_GETLINECOUNT, 0, 0));
}

void SetProgress(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    if (g_state.hwndProgress) SendMessage(g_state.hwndProgress, PBM_SETPOS, percent, 0);
    if (g_state.hwndTxtPercent) {
        wchar_t b[16];
        swprintf(b, 16, L"%d%%", percent);
        SetWindowTextString(g_state.hwndTxtPercent, b);
    }
}
void SetCurrentItemText(const std::wstring& text) {
    if (g_state.hwndTxtProgress) SetWindowTextString(g_state.hwndTxtProgress, text);
}
void ResetProgressUI() {
    SetProgress(0);
    SetCurrentItemText(L"Idle.");
    if (g_state.hwndBtnCancel) EnableWindow(g_state.hwndBtnCancel, FALSE);
}

// Status bar readout: "<n> items - <k> selected", total size, encryption state.
void UpdateBrowserStatus() {
    if (!g_state.hwndStatus) return;
    HWND list = g_state.hwndFileList;
    int shown = list ? ListView_GetItemCount(list) : 0;
    int selected = 0;
    if (list) {
        for (int i = 0; ; i++) {
            if (ListView_GetNextItem(list, i - 1, LVNI_SELECTED) == -1) break;
            selected++;
        }
    }
    unsigned long long total = TotalPackedSize(g_state.archiveEntries);
    std::wstring items = std::to_wstring(shown) + L" items";
    if (selected > 0) items += L" - " + std::to_wstring(selected) + L" selected";
    SetStatusPart(1, items);
    SetStatusPart(2, L"Size: " + FormatSize(total));
    SetStatusPart(3, g_state.archiveEncrypted ? L"Encrypted" : L"Not encrypted");
}

static void LayoutMain(); // forward
static void TintListView(HWND h);

// ---------------- component manager ----------------
static std::vector<int> g_queue;   // components still to install

static int SelectedPluginIndex() {
    HWND h = g_state.hwndPluginGrid;
    if (!h) return -1;
    int sel = ListView_GetNextItem(h, -1, LVNI_SELECTED);
    if (sel < 0) return -1;
    LVITEM it = { 0 };
    it.mask = LVIF_PARAM;
    it.iItem = sel;
    if (!ListView_GetItem(h, &it)) return -1;
    return (int)it.lParam;
}

// Shows the URL of whatever row is highlighted so it can be edited.
static void ShowSelectedComponentUrl() {
    int sel = SelectedPluginIndex();
    if (sel < 0) return;
    std::wstring url = ComponentField(sel, COMP_FIELD_URL);
    if (url.empty()) return;
    if (GetWindowTextString(g_state.hwndTxtPluginUrl) == url) return;
    SetWindowTextString(g_state.hwndTxtPluginUrl, url);
}

// Asks for permission before anything is downloaded. A component that pins a
// SHA-256 is verified automatically; one without a pin gets an explicit yes.
static bool ConfirmComponent(int index) {
    std::wstring name = ComponentField(index, COMP_FIELD_NAME);
    std::wstring url = ComponentField(index, COMP_FIELD_URL);
    std::wstring verify = ComponentField(index, COMP_FIELD_VERIFY);
    std::wstring sha = ComponentField(index, COMP_FIELD_SHA256);
    std::wstring target = GetExeDir() + L"\\" + verify;

    std::wstring body = L"Component: " + name + L"\n\nDownload from:\n" + url;
    body += L"\n\nIt will be unpacked to:\n" + target;
    if (sha.empty()) {
        body += L"\n\nNo SHA-256 is pinned for this component, so the download cannot be "
                L"verified against a known hash. Only continue if you trust this address.";
    } else {
        body += L"\n\nPinned SHA-256:\n" + sha +
                L"\n\nIf the download does not match, it is deleted and nothing is installed.";
    }
    if (MessageBox(g_state.hwndMain, body.c_str(), L"Install component?", MB_YESNO | MB_ICONQUESTION) != IDYES)
        return false;
    return true;
}

static void StartNextComponent() {
    if (g_queue.empty()) return;
    if (IsComponentJobRunning()) return;
    int index = g_queue.front();
    if (index < 0 || index >= ComponentCount()) { g_queue.erase(g_queue.begin()); return StartNextComponent(); }
    if (ComponentField(index, COMP_FIELD_URL).empty()) {
        g_queue.erase(g_queue.begin());
        return StartNextComponent();
    }
    if (!ConfirmComponent(index)) {
        g_queue.clear();
        SetStatus(L"Install cancelled.");
        SetWindowTextString(GetItem(g_state.hwndMain, ID_TXT_PLUGIN_STATUS), L"");
        return;
    }
    if (!StartComponentInstall(index, g_state.hwndMain)) {
        g_queue.clear();
        SetStatus(L"Could not start the download.");
        return;
    }
    SetStatus(L"Downloading " + ComponentField(index, COMP_FIELD_NAME) + L"...");
    SetWindowTextString(GetItem(g_state.hwndMain, ID_TXT_PLUGIN_STATUS),
        L"Downloading " + ComponentField(index, COMP_FIELD_NAME) + L"...");
}

void ShowPage(Page p) {
    g_state.page = p;
    LayoutMain();
}

void ApplyTheme(Theme t) {
    g_state.currentTheme = t;
    g_state.colors = GetThemeColors(t);
    if (g_hbrWindow) DeleteObject(g_hbrWindow);
    if (g_hbrSurface) DeleteObject(g_hbrSurface);
    g_hbrWindow = CreateSolidBrush(g_state.colors.bgWindow);
    g_hbrSurface = CreateSolidBrush(g_state.colors.bgSurface);
    for (int i = 0; i < (int)Theme::Count; i++) g_themeSel[i] = ((int)t == i);
    if (g_state.hwndStatus) {
        SendMessage(g_state.hwndStatus, SB_SETBKCOLOR, 0, (LPARAM)g_state.colors.bgSurface);
        SendMessage(g_state.hwndStatus, WM_SETFONT, (WPARAM)g_state.hFontUI, TRUE);
    }
    if (g_state.hwndProgress) {
        SendMessage(g_state.hwndProgress, PBM_SETBKCOLOR, 0, (LPARAM)g_state.colors.bgSurface);
        SendMessage(g_state.hwndProgress, PBM_SETBARCOLOR, 0, (LPARAM)g_state.colors.accent);
    }
    TintListView(g_state.hwndFileList);
    TintListView(g_state.hwndVaultList);
    TintListView(GetItem(g_state.hwndMain, ID_TOOL_GRID));
    TintListView(GetItem(g_state.hwndMain, ID_COMP_LIST));
    UpdateTitleBar();
    if (g_state.hwndMain) InvalidateRect(g_state.hwndMain, nullptr, TRUE);
}

static void RebuildFonts() {
    if (g_state.hFontUI) DeleteObject(g_state.hFontUI);
    if (g_state.hFontBig) DeleteObject(g_state.hFontBig);
    if (g_state.hFontTip) DeleteObject(g_state.hFontTip);
    if (g_state.hFontBold) DeleteObject(g_state.hFontBold);
    if (g_state.hFontHero) DeleteObject(g_state.hFontHero);
    g_state.hFontUI = MakeFont(g_state.ui.fontUI, false);
    g_state.hFontBig = MakeFont(g_state.ui.fontBig, true);
    g_state.hFontTip = MakeFont(g_state.ui.fontTip, false);
    g_state.hFontBold = MakeFont(g_state.ui.fontUI, true);
    g_state.hFontHero = MakeFont(g_state.ui.fontHero, true);
    if (g_state.hwndMain) {
        EnumChildWindows(g_state.hwndMain, [](HWND h, LPARAM) -> BOOL {
            SendMessage(h, WM_SETFONT, (WPARAM)g_state.hFontUI, TRUE);
            return TRUE;
        }, 0);
    }
    if (g_heroTitle) SendMessage(g_heroTitle, WM_SETFONT, (WPARAM)g_state.hFontHero, TRUE);
    if (g_state.hwndLog) SendMessage(g_state.hwndLog, WM_SETFONT, (WPARAM)g_state.hFontTip, TRUE);
    if (g_toolResult) SendMessage(g_toolResult, WM_SETFONT, (WPARAM)g_state.hFontTip, TRUE);
    HWND sectionLabels[] = { g_lblTheme, g_lblAccent, g_lblZoom, g_lblTips, g_lblVault, g_lblCtx, g_lblDbl };
    for (HWND l : sectionLabels)
        if (l) SendMessage(l, WM_SETFONT, (WPARAM)g_state.hFontBold, TRUE);
}
void ApplyZoom(double z) {
    if (z < AppState::ZOOM_MIN) z = AppState::ZOOM_MIN;
    if (z > AppState::ZOOM_MAX) z = AppState::ZOOM_MAX;
    g_state.ui.Apply(z);
    RebuildFonts();
    UpdateTitleBar();
    LayoutMain();
    SaveUiSettings();
}

// ---------------- drawing: nav + generic owner buttons ----------------
static const int kNavIcons[6] = { ICON_BOLT, ICON_FOLDER, ICON_DOWN, ICON_BOX, ICON_MAG, ICON_GEAR };

static void DrawNavButton(LPDRAWITEMSTRUCT dis, bool active) {
    HDC hdc = dis->hDC; RECT rc = dis->rcItem;
    bool hot = (dis->itemState & ODS_HOTLIGHT) != 0;
    bool down = (dis->itemState & ODS_SELECTED) != 0;
    SetBkMode(hdc, TRANSPARENT);
    SetGraphicsMode(hdc, GM_ADVANCED);

    COLORREF top = active ? MixColor(g_state.colors.accentSoft, g_state.colors.accent, 0.22)
        : (down ? g_state.colors.bgPressed : (hot ? g_state.colors.bgHover : g_state.colors.navBg));
    COLORREF bot = active ? g_state.colors.accentSoft
        : (down ? MixColor(g_state.colors.bgPressed, g_state.colors.bgWindow, 0.4)
            : (hot ? MixColor(g_state.colors.bgHover, g_state.colors.navBg, 0.6) : MixColor(g_state.colors.navBg, g_state.colors.bgWindow, 0.30)));
    int rad = Z(12);
    HRGN rgn = CreateRoundRectRgn(rc.left, rc.top, rc.right, rc.bottom, rad, rad);
    SelectClipRgn(hdc, rgn);
    FillGradient(hdc, rc, top, bot);
    DeleteObject(rgn);
    SelectClipRgn(hdc, nullptr);
    if (active) {
        RECT bar = { rc.left, rc.top + Z(6), rc.left + Z(4), rc.bottom - Z(6) };
        HBRUSH ab = CreateSolidBrush(g_state.colors.accent);
        DrawRoundRect(hdc, bar, Z(3), ab, nullptr);
        DeleteObject(ab);
    }
    HPEN pen = CreatePen(PS_SOLID, 1, active ? MixColor(g_state.colors.accent, g_state.colors.bgWindow, 0.35) : g_state.colors.border);
    DrawRoundRect(hdc, rc, rad, nullptr, pen);
    DeleteObject(pen);

    // icon
    int idx = dis->CtlID - ID_NAV_HOME;
    int gy = (rc.top + rc.bottom) / 2;
    int gh = Z(16);
    RECT glyph = { rc.left + Z(12), gy - gh, rc.left + Z(12) + gh * 2, gy + gh };
    if (idx >= 0 && idx < 6)
        DrawIconGlyph(hdc, glyph, kNavIcons[idx], active ? g_state.colors.accent : g_state.colors.textSecondary);

    SetTextColor(hdc, active ? g_state.colors.accent : g_state.colors.textPrimary);
    HFONT oldF = (HFONT)SelectObject(hdc, active ? g_state.hFontBold : g_state.hFontUI);
    wchar_t txt[64]; GetWindowText(dis->hwndItem, txt, 64);
    RECT tRc = rc; tRc.left += Z(54);
    DrawText(hdc, txt, -1, &tRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldF);
}
static void DrawBigButton(LPDRAWITEMSTRUCT dis) {
    HDC hdc = dis->hDC; RECT rc = dis->rcItem;
    bool down = (dis->itemState & ODS_SELECTED) != 0;
    bool hot = (dis->itemState & ODS_HOTLIGHT) != 0;
    bool disab = (dis->itemState & ODS_DISABLED) != 0;
    bool primary = (dis->CtlID == ID_BTN_GO || dis->CtlID == ID_BTN_COMP_GO);
    SetBkMode(hdc, TRANSPARENT);
    SetGraphicsMode(hdc, GM_ADVANCED);

    COLORREF top, bot, border;
    if (disab) {
        top = g_state.colors.bgSurface; bot = MixColor(g_state.colors.bgSurface, g_state.colors.bgWindow, 0.45);
        border = MixColor(g_state.colors.border, g_state.colors.bgWindow, 0.4);
    } else if (primary) {
        COLORREF base = (down || hot) ? g_state.colors.accentHover : g_state.colors.accent;
        if (down) base = MixColor(g_state.colors.accent, RGB(0, 0, 0), 0.22);
        top = MixColor(base, RGB(255, 255, 255), 0.10);
        bot = MixColor(base, RGB(0, 0, 0), 0.22);
        border = MixColor(base, RGB(0, 0, 0), 0.35);
    } else {
        if (down) { top = g_state.colors.bgPressed; bot = MixColor(g_state.colors.bgPressed, g_state.colors.bgWindow, 0.45); }
        else if (hot) { top = g_state.colors.bgHover; bot = MixColor(g_state.colors.bgHover, g_state.colors.bgSurface, 0.55); }
        else { top = g_state.colors.bgSurface; bot = MixColor(g_state.colors.bgSurface, g_state.colors.bgWindow, 0.50); }
        border = hot && !down ? MixColor(g_state.colors.border, g_state.colors.accent, 0.45) : g_state.colors.border;
    }
    int rad = Z(12);
    HRGN rgn = CreateRoundRectRgn(rc.left, rc.top, rc.right, rc.bottom, rad, rad);
    SelectClipRgn(hdc, rgn);
    FillGradient(hdc, rc, top, bot);
    DeleteObject(rgn);
    SelectClipRgn(hdc, nullptr);
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    DrawRoundRect(hdc, rc, rad, nullptr, pen);
    DeleteObject(pen);
    RECT hi = { rc.left + Z(6), rc.top + 1, rc.right - Z(6), rc.top + 2 };
    HBRUSH hb = CreateSolidBrush(MixColor(top, RGB(255, 255, 255), primary ? 0.30 : 0.16));
    FillRect(hdc, &hi, hb);
    DeleteObject(hb);

    SetTextColor(hdc, (primary && !disab) ? RGB(255, 255, 255) : (disab ? g_state.colors.textMuted : g_state.colors.textPrimary));
    HFONT oldF = (HFONT)SelectObject(hdc, primary ? g_state.hFontBig : g_state.hFontUI);
    wchar_t txt[128]; GetWindowText(dis->hwndItem, txt, 128);
    DrawText(hdc, txt, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    SelectObject(hdc, oldF);
    if (dis->itemState & ODS_FOCUS) {
        RECT fr = { rc.left + Z(4), rc.top + Z(4), rc.right - Z(4), rc.bottom - Z(4) };
        DrawFocusRect(hdc, &fr);
    }
}

// ---------------- control factories ----------------
static HWND MkOwnerBtn(HWND parent, int id, const wchar_t* label, Page p, const wchar_t* tip) {
    HWND h = CreateWindow(L"BUTTON", label, WS_CHILD | BS_OWNERDRAW | BS_PUSHBUTTON,
        0, 0, 100, 40, parent, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
    Tag(h, p); if (tip) AddTipCtl(h, tip); return h;
}
static HWND MkPushBtn(HWND parent, int id, const wchar_t* label, Page p, const wchar_t* tip) {
    HWND h = CreateWindow(L"BUTTON", label, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | BS_PUSHBUTTON,
        0, 0, 100, 40, parent, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
    Tag(h, p); if (tip) AddTipCtl(h, tip); return h;
}
static HWND MkStaticId(HWND parent, int id, const wchar_t* text, Page p) {
    HWND h = CreateWindow(L"STATIC", text, WS_CHILD | WS_VISIBLE,
        0, 0, 100, 30, parent, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
    Tag(h, p); return h;
}
static HWND MkStatic(HWND parent, const wchar_t* text, Page p) {
    return MkStaticId(parent, 0, text, p);
}
static HWND MkCheck(HWND parent, int id, const wchar_t* label, Page p, const wchar_t* tip, bool checked) {
    HWND h = CreateWindow(L"BUTTON", label, WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        0, 0, 200, 30, parent, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
    SendMessage(h, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    Tag(h, p); if (tip) AddTipCtl(h, tip); return h;
}
static HWND MkEdit(HWND parent, int id, Page p, const wchar_t* tip, DWORD extra = 0) {
    HWND h = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | extra,
        0, 0, 200, 30, parent, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
    Tag(h, p); if (tip) AddTipCtl(h, tip); return h;
}
static HWND MkRadio(HWND parent, int id, const wchar_t* label, Page p, const wchar_t* tip, bool first, bool checked) {
    HWND h = CreateWindow(L"BUTTON", label, WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | (first ? WS_GROUP : 0),
        0, 0, 200, 30, parent, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
    SendMessage(h, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    Tag(h, p); if (tip) AddTipCtl(h, tip); return h;
}
static HWND MkListView(HWND parent, int id, Page p, const wchar_t* tip) {
    HWND h = CreateWindow(WC_LISTVIEW, L"", WS_CHILD | LVS_REPORT | LVS_SHOWSELALWAYS | WS_BORDER,
        0, 0, 0, 0, parent, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), nullptr);
    Tag(h, p); if (tip) AddTipCtl(h, tip);
    ListView_SetExtendedListViewStyle(h, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    return h;
}
static void AddColumn(HWND h, int i, const wchar_t* name, int w) {
    LVCOLUMN c = { 0 }; c.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    c.pszText = (LPWSTR)name; c.cx = w; ListView_InsertColumn(h, i, &c);
}
static void TintListView(HWND h) {
    if (!h) return;
    ListView_SetBkColor(h, g_state.colors.bgSurface);
    ListView_SetTextBkColor(h, g_state.colors.bgSurface);
    ListView_SetTextColor(h, g_state.colors.textPrimary);
    InvalidateRect(h, nullptr, TRUE);
}

// ---------------- list refreshers ----------------
void RefreshVaultList() {
    HWND h = g_state.hwndMain ? GetItem(g_state.hwndMain, ID_VAULT_LIST) : nullptr;
    if (!h) return;
    ListView_DeleteAllItems(h);
    for (size_t i = 0; i < g_state.passwords.size(); i++) {
        std::wstring masked = L"password " + std::to_wstring(i + 1) + L"  (" +
            std::to_wstring(g_state.passwords[i].size()) + L" chars)";
        LVITEM it = { 0 }; it.mask = LVIF_TEXT; it.iItem = (int)i;
        it.pszText = (LPWSTR)masked.c_str();
        ListView_InsertItem(h, &it);
    }
}

void RefreshToolGrid() {
    HWND h = g_state.hwndMain ? GetItem(g_state.hwndMain, ID_TOOL_GRID) : nullptr;
    if (!h) return;
    ListView_DeleteAllItems(h);
    ListView_SetBkColor(h, g_state.colors.bgSurface);
    ListView_SetTextBkColor(h, g_state.colors.bgSurface);
    ListView_SetTextColor(h, g_state.colors.textPrimary);
    auto engines = GetEngineStatus();
    for (size_t i = 0; i < engines.size(); i++) {
        LVITEM it = { 0 }; it.mask = LVIF_TEXT; it.iItem = (int)i;
        it.pszText = (LPWSTR)engines[i].name;
        ListView_InsertItem(h, &it);
        ListView_SetItemText(h, (int)i, 1, (LPWSTR)engines[i].handles);
        ListView_SetItemText(h, (int)i, 2, (LPWSTR)(engines[i].present ? L"Ready" : L"Missing"));
    }
}

// ---------------- actions ----------------
static void DoOpenArchive(const std::wstring& path) {
    if (!FileExists(path)) {
        MessageBox(g_state.hwndMain, L"File not found.", L"Star Extract", MB_OK | MB_ICONWARNING);
        return;
    }
    g_state.currentArchive = path;
    g_state.currentFolder.clear();
    SetStatus(L"Reading archive...");
    HWND srcEdit = GetItem(g_state.hwndMain, ID_TXT_SRC);
    if (srcEdit) SetWindowTextString(srcEdit, path);
    std::wstring dir = path, name = path;
    size_t s = path.find_last_of(L"\\/");
    if (s != std::wstring::npos) { dir = path.substr(0, s); name = path.substr(s + 1); }
    size_t d = name.find_last_of(L'.');
    if (d != std::wstring::npos) name = name.substr(0, d);
    HWND dstEdit = GetItem(g_state.hwndMain, ID_TXT_DEST);
    if (dstEdit) SetWindowTextString(dstEdit, dir + L"\\" + name);
    std::wstring eng = DetectEngine(path);
    SetWindowTextString(g_engineBadge, (L"Engine: " + eng).c_str());
    HWND engLbl = GetItem(g_state.hwndMain, ID_ENGINE_LABEL);
    if (engLbl) SetWindowTextString(engLbl, eng.c_str());
    LogMessage(L"Opened: " + path + L"  [" + eng + L"]");
    g_state.archiveEntries = ListArchiveEntries(path);
    g_state.currentFolder.clear();
    g_state.folderHistory.clear();
    g_state.historyPos = 0;
    // An encrypted header lists nothing without the right password: try the
    // vault before telling the user the archive is empty.
    bool onlyHeader = true;
    for (const auto& e : g_state.archiveEntries) if (!e.isArchiveSelf) { onlyHeader = false; break; }
    if (onlyHeader) {
        std::wstring found;
        if (TryVaultPasswords(path, found)) {
            g_state.archiveEntries = ListArchiveEntries(path, found);
            LogMessage(L"Unlocked with a saved password.");
        }
    }
    g_state.archiveEncrypted = EntriesEncrypted(g_state.archiveEntries);
    if (!g_state.archiveEncrypted) {
        // headers can hide the flag, so fall back to the tool's own wording
        std::wstring probe = RunProcessCapture(Get7zPath(), L"l -slt \"" + path + L"\"");
        g_state.archiveEncrypted = probe.find(L"Encrypted = +") != std::wstring::npos ||
            probe.find(L"Can not open encrypted") != std::wstring::npos;
    }
    g_state.archiveTotalSize = TotalPackedSize(g_state.archiveEntries);
    RefreshBrowserList();
    RefreshBreadcrumbs();
    SetStatus(L"Loaded " + std::to_wstring(g_state.archiveEntries.size()) + L" entries with " + eng);
    ShowPage(Page::Browser);
}

static void BrowseOpen() {
    OPENFILENAME ofn = { 0 };
    wchar_t file[MAX_PATH] = L"";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_state.hwndMain;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"All supported\0*.7z;*.zip;*.rar;*.tar;*.gz;*.tgz;*.bz2;*.xz;*.zst;*.cab;*.iso;*.msi;*.msp;*.exe;*.pea\0All files\0*.*\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Open archive - pick any archive or installer";
    if (GetOpenFileName(&ofn)) DoOpenArchive(file);
}

static std::wstring BrowseFolder(HWND owner, const wchar_t* title) {
    BROWSEINFO bi = { 0 };
    bi.hwndOwner = owner; bi.lpszTitle = title;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
    std::wstring out;
    if (pidl) {
        wchar_t p[MAX_PATH];
        if (SHGetPathFromIDList(pidl, p)) out = p;
        CoTaskMemFree(pidl);
    }
    return out;
}

// One in-flight tool job. The tool runs on a worker thread; WM_APP_STREAM_*
// messages drive the UI and finalise the job.
enum class JobKind { None, Extract, Compress };
struct PendingJob {
    JobKind kind = JobKind::None;
    std::wstring src, dest;
    bool openAfter = false, deleteAfter = false;
    int filesDone = 0;
};
static PendingJob g_job;

// Works out the password: typed value first, then the vault. Returns false when
// the archive needs one and nothing worked. Uses the encryption flag already
// known from the listing, so the common case costs no extra 7z run.
static bool NegotiatePassword(const std::wstring& archive, const std::wstring& typedPwd,
    std::wstring& pwd) {
    pwd = typedPwd;
    if (!pwd.empty()) return true;

    bool locked;
    if (_wcsicmp(archive.c_str(), g_state.currentArchive.c_str()) == 0) {
        locked = g_state.archiveEncrypted;
    } else {
        std::wstring probe = RunProcessCapture(Get7zPath(), L"t -p\"\"" + archive + L"\"");
        locked = probe.find(L"password") != std::wstring::npos ||
            probe.find(L"Password") != std::wstring::npos ||
            probe.find(L"Encrypted") != std::wstring::npos ||
            probe.find(L"Can not open encrypted") != std::wstring::npos;
    }
    if (!locked) return true;

    std::wstring found;
    if (TryVaultPasswords(archive, found)) {
        pwd = found;
        LogMessage(L"A saved password worked. Continuing...");
        return true;
    }
    return false;
}

// Starts the tool for the detected engine. `headless` is used by the CLI path
// (no window yet), which has to run it synchronously.
static bool StartExtraction(const std::wstring& archive, const std::wstring& dest,
    const std::wstring& pwd, bool headless) {
    std::wstring eng = DetectEngine(archive);
    auto run = [&](const std::wstring& exe, const std::wstring& args) -> bool {
        if (!headless && g_state.hwndMain) return RunProcessStream(exe, args, g_state.hwndMain);
        RunProcess(exe, args);
        return true;
    };
    if (eng == L"LessMSI") {
        if (!FileExists(GetLessMsiPath())) {
            LogMessage(L"LessMSI is missing (bin\\lessmsi\\lessmsi.exe). See Tools page.");
            if (g_state.hwndMain)
                MessageBox(g_state.hwndMain, L"LessMSI was not found next to the app.", L"Star Extract", MB_OK | MB_ICONWARNING);
            return false;
        }
        return run(GetLessMsiPath(), L"x \"" + archive + L"\" \"" + dest + L"\"");
    }
    if (eng == L"InnoUnp") {
        if (!FileExists(GetInnounpPath())) { LogMessage(L"InnoUnp is missing (bin\\innounp.exe)."); return false; }
        return run(GetInnounpPath(), L"-x -y -d\"" + dest + L"\" \"" + archive + L"\"");
    }
    if (eng == L"Unshield") {
        if (!FileExists(GetUnshieldPath())) { LogMessage(L"Unshield is missing (bin\\unshield.exe)."); return false; }
        return run(GetUnshieldPath(), L"x -d \"" + dest + L"\" \"" + archive + L"\"");
    }
    std::wstring args = L"x -y -bsp1 -o\"" + dest + L"\" \"" + archive + L"\"";
    if (!pwd.empty()) args += L" -p\"" + pwd + L"\"";
    return run(Get7zPath(), args);
}

static bool ExtractWithPasswordFlow(const std::wstring& archive, const std::wstring& dest,
    const std::wstring& typedPwd, bool remember) {
    std::wstring pwd;
    if (!NegotiatePassword(archive, typedPwd, pwd)) {
        LogMessage(L"This archive needs a password. Type it on the Extract page and press the button again.");
        if (!g_state.hwndMain) {
            // Explorer /extract verb: there is no window, so say it out loud
            MessageBox(nullptr,
                (L"This archive is encrypted:\n\n" + archive +
                 L"\n\nNo saved password worked. Open it in Star Extract and type the password on the Extract page.").c_str(),
                L"Password required", MB_OK | MB_ICONWARNING);
        }
        return false;
    }
    g_job.kind = JobKind::Extract;
    g_job.src = archive;
    g_job.dest = dest;
    if (!StartExtraction(archive, dest, pwd, !g_state.hwndMain)) return false;
    if (!g_state.hwndMain) {
        while (IsStreamedProcessRunning()) Sleep(30);   // CLI: wait it out
    } else {
        g_state.isExtracting = true;
        if (g_state.hwndBtnStart) EnableWindow(g_state.hwndBtnStart, FALSE);
        if (g_state.hwndBtnCancel) EnableWindow(g_state.hwndBtnCancel, TRUE);
    }
    if (!pwd.empty() && remember) {
        AddPassword(pwd);
        RefreshVaultList();
        LogMessage(L"Password remembered (encrypted).");
    }
    return true;
}

static void FinishExtraction(bool ok, bool cancelled) {
    g_state.isExtracting = false;
    if (g_state.hwndBtnStart) EnableWindow(g_state.hwndBtnStart, TRUE);
    if (g_state.hwndBtnCancel) EnableWindow(g_state.hwndBtnCancel, FALSE);
    SetProgress(ok ? 100 : 0);
    SetCurrentItemText(cancelled ? L"Cancelled." : (ok ? L"Done." : L"Needs attention - see log."));
    if (cancelled) {
        LogMessage(L"Cancelled by the user.");
        SetStatus(L"Cancelled.");
        g_job.kind = JobKind::None;
        return;
    }
    if (ok) {
        LogMessage(L"Finished: " + g_job.dest + L"  (" + std::to_wstring(g_job.filesDone) + L" items)");
        SetStatus(L"Done.");
        if (g_job.openAfter) ShellExecute(nullptr, L"open", g_job.dest.c_str(), nullptr, nullptr, SW_SHOW);
        if (g_job.deleteAfter) DeleteFile(g_job.src.c_str());
    } else {
        LogMessage(L"Extraction failed - see the log above.");
        SetStatus(L"Extraction failed.");
    }
    g_job.kind = JobKind::None;
}

// One line of tool output. `-bsp1` prints " 45% 3 - path\to\file"; anything
// else is plain log text.
static void HandleStreamLine(const std::wstring& line) {
    size_t i = 0;
    while (i < line.size() && line[i] == L' ') i++;
    size_t digits = i;
    while (digits < line.size() && iswdigit(line[digits])) digits++;
    if (digits > i && digits < line.size() && line[digits] == L'%') {
        SetProgress(_wtoi(line.substr(i, digits - i).c_str()));
        std::wstring rest = line.substr(digits + 1);
        size_t dash = rest.find(L" - ");
        std::wstring name = (dash == std::wstring::npos) ? rest : rest.substr(dash + 3);
        while (!name.empty() && name.front() == L' ') name.erase(name.begin());
        if (!name.empty()) {
            SetCurrentItemText(name);
            g_job.filesDone++;
        }
        return;
    }
    if (!line.empty()) LogMessage(line);
}

static void DoExtractFlow() {
    HWND main = g_state.hwndMain;
    std::wstring src = GetWindowTextString(GetItem(main, ID_TXT_SRC));
    std::wstring dest = GetWindowTextString(GetItem(main, ID_TXT_DEST));
    if (src.empty() || !FileExists(src)) {
        MessageBox(main, L"Step 1: pick an archive first (Browse, or drop a file here).", L"Star Extract", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (dest.empty()) {
        size_t d = src.find_last_of(L'.');
        dest = (d == std::wstring::npos) ? src + L"_out" : src.substr(0, d);
        SetWindowTextString(GetItem(main, ID_TXT_DEST), dest);
    }
    if (SendMessage(GetItem(main, ID_CHK_SUBFOLDER), BM_GETCHECK, 0, 0) == BST_CHECKED) {
        size_t s = src.find_last_of(L"\\/");
        std::wstring nm = (s == std::wstring::npos) ? src : src.substr(s + 1);
        size_t d = nm.find_last_of(L'.');
        if (d != std::wstring::npos) nm = nm.substr(0, d);
        size_t bs = dest.find_last_of(L"\\/");
        std::wstring dir = (bs == std::wstring::npos) ? dest : dest.substr(0, bs);
        dest = dir + L"\\" + nm;
    }
    CreateDirectory(dest.c_str(), nullptr);
    std::wstring typed = GetWindowTextString(GetItem(main, ID_TXT_PASSWORD));
    bool rem = SendMessage(GetItem(main, ID_CHK_REMEMBER_PWD), BM_GETCHECK, 0, 0) == BST_CHECKED;
    ShowPage(Page::Extract);
    ResetProgressUI();
    SetCurrentItemText(L"Starting...");
    SetStatus(L"Extracting...");
    LogMessage(L"Extracting: " + src + L"  ->  " + dest);

    g_job.kind = JobKind::Extract;
    g_job.src = src;
    g_job.dest = dest;
    g_job.openAfter = SendMessage(GetItem(main, ID_CHK_OPEN_AFTER), BM_GETCHECK, 0, 0) == BST_CHECKED;
    g_job.deleteAfter = SendMessage(GetItem(main, ID_CHK_DELETE_AFTER), BM_GETCHECK, 0, 0) == BST_CHECKED;
    g_job.filesDone = 0;

    std::wstring pwd;
    if (!NegotiatePassword(src, typed, pwd)) {
        LogMessage(L"This archive needs a password. Type it above and press Extract again.");
        SetCurrentItemText(L"Password required.");
        SetStatus(L"Password required - see log.");
        MessageBox(main, L"This archive is encrypted.\nType the password in the box above and press Extract again.",
            L"Password required", MB_OK | MB_ICONWARNING);
        g_job.kind = JobKind::None;
        return;
    }
    if (g_state.hwndBtnStart) EnableWindow(g_state.hwndBtnStart, FALSE);
    if (g_state.hwndBtnCancel) EnableWindow(g_state.hwndBtnCancel, TRUE);
    g_state.isExtracting = true;
    if (!StartExtraction(src, dest, pwd, false)) {
        g_state.isExtracting = false;
        g_job.kind = JobKind::None;
        if (g_state.hwndBtnStart) EnableWindow(g_state.hwndBtnStart, TRUE);
        if (g_state.hwndBtnCancel) EnableWindow(g_state.hwndBtnCancel, FALSE);
        SetCurrentItemText(L"Could not start - see log.");
        SetStatus(L"Could not start the extraction tool.");
        return;
    }
    if (!pwd.empty() && rem) {
        AddPassword(pwd);
        RefreshVaultList();
        LogMessage(L"Password remembered (encrypted).");
    }
}

// Real path inside the archive for a list row (".." for the parent row), plus
// its display name. Returns false when the row has no payload (header row).
static bool GetRowPath(HWND list, int sel, std::wstring& path, std::wstring& name) {
    LVITEM it = { 0 };
    it.mask = LVIF_PARAM | LVIF_TEXT;
    it.iItem = sel;
    wchar_t text[MAX_PATH] = L"";
    it.pszText = text;
    it.cchTextMax = MAX_PATH;
    if (!ListView_GetItem(list, &it)) return false;
    name = text;
    if (!it.lParam) return false;
    path = *((std::wstring*)it.lParam);
    return true;
}

void BrowserOpenSelected() {
    HWND h = GetItem(g_state.hwndMain, ID_FILE_LIST);
    int sel = ListView_GetNextItem(h, -1, LVNI_SELECTED);
    if (sel < 0 || g_state.currentArchive.empty()) return;
    std::wstring path, name;
    if (!GetRowPath(h, sel, path, name)) return;
    if (path == L"..") {
        size_t p = g_state.currentFolder.find_last_of(L'\\');
        g_state.currentFolder = (p == std::wstring::npos) ? L"" : g_state.currentFolder.substr(0, p);
        NavigateBrowser(g_state.currentFolder);
        return;
    }
    bool isFolder = (sel >= 0 && name == L"..") ? false : (name.find(L'.') == std::wstring::npos);
    wchar_t type[32]; ListView_GetItemText(h, sel, 1, type, 32);
    if (!wcscmp(type, L"Folder")) isFolder = true;
    if (isFolder) {
        g_state.currentFolder = path;
        NavigateBrowser(g_state.currentFolder);
    } else {
        DoPreviewSelected();
    }
}

void DoPreviewSelected() {
    HWND h = GetItem(g_state.hwndMain, ID_FILE_LIST);
    int sel = ListView_GetNextItem(h, -1, LVNI_SELECTED);
    if (sel < 0 || g_state.currentArchive.empty()) return;
    std::wstring full, name;
    if (!GetRowPath(h, sel, full, name)) return;
    if (full == L"..") return;
    wchar_t tmp[MAX_PATH]; GetTempPath(MAX_PATH, tmp);
    // `e` extracts without paths, so the file lands flat in the temp folder
    std::wstring leaf = full.substr(full.find_last_of(L"\\/") + 1);
    wchar_t dirBuf[MAX_PATH];
    swprintf(dirBuf, MAX_PATH, L"%sStarExtractPreview-%lu", tmp, (unsigned long)GetCurrentProcessId());
    std::wstring out = dirBuf;
    CreateDirectory(out.c_str(), nullptr);
    RunProcess(Get7zPath(), L"e -y -o\"" + out + L"\" \"" + g_state.currentArchive + L"\" \"" + full + L"\"");
    std::wstring fn = out + L"\\" + leaf;
    if (FileExists(fn)) ShellExecute(nullptr, L"open", fn.c_str(), nullptr, nullptr, SW_SHOW);
    else MessageBox(g_state.hwndMain, L"Could not pull that file out for preview.", L"Star Extract", MB_OK | MB_ICONWARNING);
}

static void AddCompFiles(const std::vector<std::wstring>& files) {
    HWND h = GetItem(g_state.hwndMain, ID_COMP_LIST);
    for (auto& f : files) {
        if (std::find(g_compFiles.begin(), g_compFiles.end(), f) != g_compFiles.end()) continue;
        g_compFiles.push_back(f);
        LVITEM it = { 0 }; it.mask = LVIF_TEXT; it.iItem = ListView_GetItemCount(h);
        it.pszText = (LPWSTR)g_compFiles.back().c_str();
        ListView_InsertItem(h, &it);
    }
    SetWindowTextString(g_compNote, std::to_wstring(g_compFiles.size()) + L" file(s) ready. Pick a format, then Compress Now.");
}

static void BrowseCompAdd() {
    wchar_t buf[32768] = L"";
    OPENFILENAME ofn = { 0 };
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_state.hwndMain;
    ofn.lpstrFile = buf;
    ofn.nMaxFile = 32768;
    ofn.lpstrFilter = L"All files\0*.*\0";
    ofn.Flags = OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    ofn.lpstrTitle = L"Pick files to pack - you can select many at once";
    if (!GetOpenFileName(&ofn)) return;
    std::vector<std::wstring> files;
    if (ofn.nFileOffset == 0 || buf[ofn.nFileOffset - 1] == 0) {
        // single select: buffer holds full path... actually with EXPLORER style check double-null
        std::wstring first(buf);
        size_t second = first.size() + 1;
        if (second < 32768 && buf[second] != 0) {
            std::wstring dir = first;
            size_t p = second;
            while (p < 32768 && buf[p] != 0) {
                std::wstring nm(&buf[p]);
                files.push_back(dir + L"\\" + nm);
                p += nm.size() + 1;
            }
        } else {
            files.push_back(first);
        }
    } else {
        files.push_back(std::wstring(buf));
    }
    AddCompFiles(files);
}

static void DoCompressGo() {
    if (g_compFiles.empty()) {
        MessageBox(g_state.hwndMain, L"Step 1: add some files first (Add files, or drop them here).", L"Star Extract", MB_OK | MB_ICONINFORMATION);
        return;
    }
    bool useZip = SendMessage(GetItem(g_state.hwndMain, ID_RAD_FMT_ZIP), BM_GETCHECK, 0, 0) == BST_CHECKED;
    bool combine = SendMessage(GetItem(g_state.hwndMain, ID_RAD_MODE_COMBINE), BM_GETCHECK, 0, 0) == BST_CHECKED;
    std::wstring fmt = useZip ? L"-tzip" : L"-t7z";
    std::wstring ext = useZip ? L".zip" : L".7z";
    ShowPage(Page::Extract);
    if (combine) {
        std::wstring out = GetWindowTextString(GetItem(g_state.hwndMain, ID_TXT_COMP_OUT));
        if (out.empty()) {
            std::wstring first = g_compFiles[0];
            size_t s = first.find_last_of(L"\\/");
            std::wstring dir = (s == std::wstring::npos) ? L"." : first.substr(0, s);
            out = dir + L"\\Combined" + ext;
            int n = 2;
            while (FileExists(out)) { out = dir + L"\\Combined (" + std::to_wstring(n++) + L")" + ext; }
        }
        std::wstring args = L"a " + fmt + L" \"" + out + L"\"";
        for (auto& f : g_compFiles) args += L" \"" + f + L"\"";
        LogMessage(L"Packing " + std::to_wstring(g_compFiles.size()) + L" file(s) into " + out);
        RunProcess(Get7zPath(), args);
        LogMessage(L"Finished: " + out);
        SetWindowTextString(g_compNote, L"Done: " + out);
        MessageBox(g_state.hwndMain, (L"Combined archive created:\n" + out).c_str(), L"Star Extract", MB_OK | MB_ICONINFORMATION);
    } else {
        int done = 0;
        for (auto& f : g_compFiles) {
            std::wstring base = f;
            size_t s = base.find_last_of(L"\\/");
            std::wstring dir = (s == std::wstring::npos) ? L"." : base.substr(0, s);
            std::wstring nm = (s == std::wstring::npos) ? base : base.substr(s + 1);
            size_t d = nm.find_last_of(L'.');
            if (d != std::wstring::npos) nm = nm.substr(0, d);
            std::wstring out = dir + L"\\" + nm + ext;
            LogMessage(L"Packing " + f + L"  ->  " + out);
            RunProcess(Get7zPath(), L"a " + fmt + L" \"" + out + L"\" \"" + f + L"\"");
            done++;
        }
        SetWindowTextString(g_compNote, L"Done: " + std::to_wstring(done) + L" separate archive(s) created.");
        MessageBox(g_state.hwndMain, (L"Created " + std::to_wstring(done) + L" separate archive(s).").c_str(), L"Star Extract", MB_OK | MB_ICONINFORMATION);
    }
    SetStatus(L"Compress finished.");
}

// ---------------- create controls ----------------
static void CreateControls(HWND hwnd) {
    g_state.ui.Apply(g_state.ui.zoom);
    g_state.colors = GetThemeColors(g_state.currentTheme);
    g_hbrWindow = CreateSolidBrush(g_state.colors.bgWindow);
    g_hbrSurface = CreateSolidBrush(g_state.colors.bgSurface);
    RebuildFonts();
    g_hTips = CreateTooltip(hwnd);

    const wchar_t* navLabels[] = { L"Home", L"Browse", L"Extract", L"Compress", L"Tools", L"Settings" };
    const wchar_t* navTips[] = {
        L"Start here. Big actions and beginner guidance.",
        L"Look inside an archive before unpacking it.",
        L"Unpack an archive step by step.",
        L"Pack files into a new archive.",
        L"Check tools, test archives, hash and identify files.",
        L"Theme, text size, tips, passwords, right-click menu.",
    };
    for (int i = 0; i < 6; i++) {
        HWND h = CreateWindow(L"BUTTON", navLabels[i], WS_CHILD | WS_VISIBLE | BS_OWNERDRAW | BS_PUSHBUTTON,
            0, 0, 100, 40, hwnd, (HMENU)(INT_PTR)(ID_NAV_HOME + i), GetModuleHandle(nullptr), nullptr);
        Tag(h, Page::Count);
        AddTipCtl(h, navTips[i]);
    }

    // HOME
    g_heroTitle = MkStatic(hwnd, L"What would you like to do?", Page::Home);
    SendMessage(g_heroTitle, WM_SETFONT, (WPARAM)g_state.hFontHero, TRUE);
    g_heroSub = MkStatic(hwnd, L"Pick an action below. Hover anything for help. You can drag && drop files anywhere.", Page::Home);
    HWND c1 = CreateCard(hwnd, ID_CARD_OPEN, L"Open archive", L"Look inside a zip, 7z, rar or installer first.", ICON_FOLDER, nullptr);
    Tag(c1, Page::Home); AddTipCtl(c1, L"Opens any archive in the Browser so you can see what is inside before unpacking.");
    HWND c2 = CreateCard(hwnd, ID_CARD_EXTRACT, L"Extract files", L"Unpack an archive into a folder you choose.", ICON_DOWN, nullptr);
    Tag(c2, Page::Home); AddTipCtl(c2, L"Unpacks the archive. The right engine (7-Zip, LessMSI and friends) is picked for you.");
    HWND c3 = CreateCard(hwnd, ID_CARD_COMPRESS, L"Compress files", L"Pack files into a .7z or .zip archive.", ICON_BOX, nullptr);
    Tag(c3, Page::Home); AddTipCtl(c3, L"Choose files, pick 7z (smaller) or zip (works everywhere), then Combine or keep Separate.");
    g_stepsLabel = MkStatic(hwnd, L"How it works:  1. Drop a file anywhere   >   2. Pick Extract or Compress   >   3. Done.", Page::Home);
    HWND tipCard = CreateCard(hwnd, ID_CARD_TIP, L"Did you know?", L"Click Next tip for guidance.", ICON_TAG, nullptr);
    Tag(tipCard, Page::Home); EnableWindow(tipCard, FALSE);
    g_tipText = MkStatic(hwnd, g_state.showTips ? kTips[0] : L"Tips are off. Turn them back on in Settings anytime.", Page::Home);
    MkPushBtn(hwnd, ID_TIP_NEXT, L"Next tip", Page::Home, L"Show another beginner tip.");
    // BROWSER
    g_pathLabel = g_state.hwndPathLabel = MkStatic(hwnd, L"No archive open yet.", Page::Browser);
    g_state.hwndBtnBack = MkOwnerBtn(hwnd, ID_BTN_BACK, L"Back", Page::Browser, L"Go back in folder history.");
    g_state.hwndBtnFwd = MkOwnerBtn(hwnd, ID_BTN_FWD, L"Forward", Page::Browser, L"Go forward in folder history.");
    g_state.hwndBtnUp2 = MkOwnerBtn(hwnd, ID_BTN_UP2, L"Up", Page::Browser, L"Go up one folder.");
    g_state.hwndBreadcrumbBar = MkStatic(hwnd, L"", Page::Browser);
    MkOwnerBtn(hwnd, ID_BTN_OPEN, L"Open...", Page::Browser, L"Choose an archive file to browse.");
    MkOwnerBtn(hwnd, ID_BTN_UP, L"Up", Page::Browser, L"Go up one folder inside the archive.");
    MkOwnerBtn(hwnd, ID_BTN_EXTRACT_ALL, L"Extract all", Page::Browser, L"Unpack everything to a folder you choose.");
    MkOwnerBtn(hwnd, ID_BTN_EXTRACT_SEL, L"Extract selected", Page::Browser, L"Unpack only the highlighted files.");
    MkOwnerBtn(hwnd, ID_BTN_PREVIEW, L"Preview", Page::Browser, L"Open the highlighted file with its normal app.");
    MkOwnerBtn(hwnd, ID_BTN_TEST2, L"Test", Page::Browser, L"Check the archive for errors.");
    MkOwnerBtn(hwnd, ID_BTN_DELETE2, L"Delete", Page::Browser, L"Remove the highlighted entry from the archive.");
    MkOwnerBtn(hwnd, ID_BTN_EXTRACT_TO, L"Extract to...", Page::Browser, L"Jump to the Extract page with this archive and a destination filled in.");
    MkOwnerBtn(hwnd, ID_BTN_7ZFM, L"7-Zip", Page::Browser, L"Open this archive in the classic 7-Zip File Manager (needs 7zFM.exe next to the app).");
    g_engineBadge = g_state.hwndEngineBadge = MkStatic(hwnd, L"Engine: -", Page::Browser);
    {
        HWND h = MkListView(hwnd, ID_TREE_LIST, Page::Browser,
            L"Folder tree. Click a folder to navigate there.");
        AddColumn(h, 0, L"Folders", 200);
        g_state.hwndTreeList = h;
    }
    {
        HWND h = MkListView(hwnd, ID_FILE_LIST, Page::Browser,
            L"Archive contents. Double-click a folder to enter it, double-click a file to preview it.");
        AddColumn(h, 0, L"Name", 330); AddColumn(h, 1, L"Type", 110);
        AddColumn(h, 2, L"Size", 100); AddColumn(h, 3, L"Packed", 100);
        AddColumn(h, 4, L"Modified", 140);
        g_state.hwndFileList = h;
    }
    g_state.hwndFilter = MkEdit(hwnd, ID_TXT_FILTER, Page::Browser,
        L"Type to search every file in the archive. Clear it to go back to the folder view.");
    g_browserHint = MkStatic(hwnd, L"Double-click a folder to open it. Double-click a file to preview it.", Page::Browser);


    // EXTRACT
    g_extractHint = MkStatic(hwnd, L"New here? 1. Pick the archive.  2. Pick where it goes.  3. Press the big button. Passwords are tried automatically.", Page::Extract);
    g_exLbl1 = MkStatic(hwnd, L"Archive:", Page::Extract);
    MkEdit(hwnd, ID_TXT_SRC, Page::Extract, L"The archive file to unpack.");
    MkOwnerBtn(hwnd, ID_BTN_SRC_BROWSE, L"Browse...", Page::Extract, L"Choose the archive file.");
    g_exLbl2 = MkStatic(hwnd, L"Destination folder:", Page::Extract);
    g_state.hwndDestEdit = MkEdit(hwnd, ID_TXT_DEST, Page::Extract, L"Where the unpacked files will go.");
    MkOwnerBtn(hwnd, ID_BTN_DEST_BROWSE, L"Browse...", Page::Extract, L"Choose the destination folder.");
    g_state.hwndChkSubfolder = MkCheck(hwnd, ID_CHK_SUBFOLDER, L"Use a tidy subfolder", Page::Extract, L"Puts everything in a new folder named after the archive. Recommended.", true);
    g_state.hwndChkOpenAfter = MkCheck(hwnd, ID_CHK_OPEN_AFTER, L"Open folder when done", Page::Extract, L"Opens the destination in Explorer after extraction.", true);
    g_state.hwndChkDeleteAfter = MkCheck(hwnd, ID_CHK_DELETE_AFTER, L"Delete archive after", Page::Extract, L"Removes the archive after success. Off is safer.", false);
    g_exPwdLbl = MkStatic(hwnd, L"Password (only if the archive is locked):", Page::Extract);
    MkEdit(hwnd, ID_TXT_PASSWORD, Page::Extract, L"Leave empty unless locked. Saved passwords are tried first, automatically.", ES_PASSWORD);
    MkCheck(hwnd, ID_CHK_REMEMBER_PWD, L"Remember password", Page::Extract, L"Stores it encrypted (Windows DPAPI) for next time.", false);
    g_exEngCap = MkStatic(hwnd, L"Detected engine:", Page::Extract);
    MkStaticId(hwnd, ID_ENGINE_LABEL, L"-", Page::Extract);
    g_state.hwndBtnStart = MkOwnerBtn(hwnd, ID_BTN_GO, L"Extract Archive Now", Page::Extract, L"Starts unpacking with the detected engine.");
    g_state.hwndProgress = CreateWindow(PROGRESS_CLASS, L"", WS_CHILD | PBS_SMOOTH,
        0, 0, 100, 20, hwnd, (HMENU)(INT_PTR)ID_PROGRESS, GetModuleHandle(nullptr), nullptr);
    Tag(g_state.hwndProgress, Page::Extract);
    MkStaticId(hwnd, ID_TXT_PROGRESS, L"Ready", Page::Extract);
    g_state.hwndTxtProgress = GetItem(hwnd, ID_TXT_PROGRESS);
    g_state.hwndTxtPercent = MkStaticId(hwnd, ID_TXT_PERCENT, L"0%", Page::Extract);
    SendMessage(g_state.hwndTxtPercent, WM_SETFONT, (WPARAM)g_state.hFontBold, TRUE);
    g_state.hwndBtnCancel = MkOwnerBtn(hwnd, ID_BTN_CANCEL, L"Cancel", Page::Extract,
        L"Stop the running extraction.");
    g_state.hwndLog = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
        0, 0, 100, 60, hwnd, (HMENU)(INT_PTR)ID_LOG_EDIT, GetModuleHandle(nullptr), nullptr);
    Tag(g_state.hwndLog, Page::Extract);
    AddTipCtl(g_state.hwndLog, L"Step-by-step log of what the extractor is doing.");
    SendMessage(g_state.hwndLog, WM_SETFONT, (WPARAM)g_state.hFontTip, TRUE);
    MkPushBtn(hwnd, ID_BTN_CLEAR_LOG, L"Clear", Page::Extract, L"Clears the log.");
    MkPushBtn(hwnd, ID_BTN_SAVE_LOG, L"Save log", Page::Extract, L"Saves the log to a text file.");

    // COMPRESS
    g_compHint = MkStatic(hwnd, L"Add files, pick 7z (smaller) or zip (works everywhere), then Combine into one archive or keep Separate.", Page::Compress);
    {
        HWND h = MkListView(hwnd, ID_COMP_LIST, Page::Compress, L"Files waiting to be packed. Use Add files, or just drop files here.");
        AddColumn(h, 0, L"Files to pack", 420);
        ListView_SetExtendedListViewStyle(h, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_CHECKBOXES);
    }
    MkOwnerBtn(hwnd, ID_BTN_COMP_ADD, L"Add files...", Page::Compress, L"Choose files and folders to pack.");
    MkOwnerBtn(hwnd, ID_BTN_COMP_REMOVE, L"Remove", Page::Compress, L"Removes highlighted entries from the list (not from disk).");
    MkPushBtn(hwnd, ID_BTN_COMP_CLEAR, L"Clear", Page::Compress, L"Empties the whole list.");
    MkRadio(hwnd, ID_RAD_FMT_7Z, L".7z  (smaller, needs 7-Zip to open)", Page::Compress, L"Best compression. Opens with 7-Zip or Star Extract.", true, true);
    MkRadio(hwnd, ID_RAD_FMT_ZIP, L".zip  (works everywhere)", Page::Compress, L"Maximum compatibility: phones, Macs, Windows.", false, false);
    MkRadio(hwnd, ID_RAD_MODE_COMBINE, L"Combine into one archive", Page::Compress, L"All files packed into a single Combined archive.", true, true);
    MkRadio(hwnd, ID_RAD_MODE_SEPARATE, L"Separate archive per file", Page::Compress, L"Each file gets its own archive next to the original.", false, false);
    g_compOutLabel = MkStatic(hwnd, L"Output (empty = automatic):", Page::Compress);
    MkEdit(hwnd, ID_TXT_COMP_OUT, Page::Compress, L"Optional fixed output path for Combine mode. Leave empty for automatic naming.");
    MkOwnerBtn(hwnd, ID_BTN_COMP_OUT_BROWSE, L"Browse...", Page::Compress, L"Pick the output archive path (Combine mode).");
    MkOwnerBtn(hwnd, ID_BTN_COMP_GO, L"Compress Now", Page::Compress, L"Packs everything in the list.");
    g_compNote = MkStatic(hwnd, L"", Page::Compress);

    // TOOLS
    {
        HWND h = MkListView(hwnd, ID_TOOL_GRID, Page::Tools, L"Every helper tool StarExtract can use, and whether it was found next to the app.");
        AddColumn(h, 0, L"Tool", 220); AddColumn(h, 1, L"Handles", 240); AddColumn(h, 2, L"Status", 110);

    }
    // Plugin / component grid
    {
        HWND h = MkListView(hwnd, ID_PLUGIN_GRID, Page::Tools, L"Components StarExtract can download and install. * marks plugins you added yourself.");
        AddColumn(h, 0, L"Component", 200); AddColumn(h, 1, L"Status", 100);
        AddColumn(h, 2, L"Latest", 70); AddColumn(h, 3, L"Verified", 80);
        AddColumn(h, 4, L"Source URL", 360); AddColumn(h, 5, L"Verify file", 190);
        g_state.hwndPluginGrid = h;
    }
    g_state.hwndBtnPluginUpdateAll = MkOwnerBtn(hwnd, ID_BTN_PLUGIN_UPDATE_ALL, L"Install All Missing", Page::Tools,
        L"Download and install every component that is not there yet.");
    MkOwnerBtn(hwnd, ID_BTN_PLUGIN_DOWNLOAD, L"Install Selected", Page::Tools,
        L"Download and install the highlighted component.");
    MkOwnerBtn(hwnd, ID_BTN_PLUGIN_CANCEL, L"Stop", Page::Tools, L"Cancels the running download.");
    MkOwnerBtn(hwnd, ID_BTN_PLUGIN_REMOVE, L"Remove", Page::Tools,
        L"Removes a plugin you added. Built-in components cannot be removed.");
    g_lblPluginUrl = MkStatic(hwnd, L"Source URL of the highlighted component:", Page::Tools);
    g_state.hwndTxtPluginUrl = MkEdit(hwnd, ID_TXT_PLUGIN_URL, Page::Tools,
        L"Point this at a mirror or a different release if the default one is blocked.");
    MkOwnerBtn(hwnd, ID_BTN_PLUGIN_SAVEURL, L"Save URL", Page::Tools,
        L"Keeps the edited URL in components.ini.");
    MkOwnerBtn(hwnd, ID_BTN_PLUGIN_RESET, L"Reset All", Page::Tools,
        L"Restores the default URLs and drops plugins you added.");
    g_lblPluginName = MkStatic(hwnd, L"Add your own plugin:", Page::Tools);
    g_state.hwndTxtPluginName = MkEdit(hwnd, ID_TXT_PLUGIN_NAME, Page::Tools, L"name");
    g_state.hwndTxtPluginVerify = MkEdit(hwnd, ID_TXT_PLUGIN_VERIFY, Page::Tools, L"verify file, e.g. bin\\mytool\\mytool.exe");
    g_state.hwndTxtPluginUrl = GetItem(hwnd, ID_TXT_PLUGIN_URL);
    g_state.hwndTxtPluginName = GetItem(hwnd, ID_TXT_PLUGIN_NAME);
    g_state.hwndTxtPluginVerify = GetItem(hwnd, ID_TXT_PLUGIN_VERIFY);
    g_state.hwndBtnPluginAdd = MkOwnerBtn(hwnd, ID_BTN_PLUGIN_ADD, L"Add Plugin", Page::Tools,
        L"Adds a custom plugin that points at any .zip / .7z download URL.");

    MkOwnerBtn(hwnd, ID_BTN_TOOL_REFRESH, L"Refresh", Page::Tools, L"Re-check which tools are present.");
    MkOwnerBtn(hwnd, ID_BTN_HASH, L"Hash a file...", Page::Tools, L"Computes SHA-256 and CRC32 so you can verify downloads.");
    MkOwnerBtn(hwnd, ID_BTN_IDENTIFY, L"Identify a file...", Page::Tools, L"Sniffs the real file type from its binary signature.");
    MkOwnerBtn(hwnd, ID_BTN_TEST2 + 1000, L"Test current", Page::Tools, L"Integrity test on the open archive.");
    g_toolResult = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"Results appear here.",
        WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
        0, 0, 100, 60, hwnd, (HMENU)(INT_PTR)(ID_BTN_TEST2 + 1001), GetModuleHandle(nullptr), nullptr);
    Tag(g_toolResult, Page::Tools);
    SendMessage(g_toolResult, WM_SETFONT, (WPARAM)g_state.hFontTip, TRUE);

    // SETTINGS
    g_lblTheme = MkStatic(hwnd, L"Theme  (press F2 to cycle)  -  click a style to use it:", Page::Settings);
    SendMessage(g_lblTheme, WM_SETFONT, (WPARAM)g_state.hFontBold, TRUE);
    for (int i = 0; i < (int)Theme::Count; i++) {
        Theme th = (Theme)i;
        HWND h = CreateThemeCard(hwnd, ID_RAD_THEME_BEGIN + i, th, &g_themeSel[i]);
        Tag(h, Page::Settings);
        g_themeTips[i] = std::wstring(L"Switch to the ") + GetThemeName(th) + L" theme - " + GetThemeDesc(th) + L".";
        AddTipCtl(h, g_themeTips[i].c_str());
    }
    g_lblAccent = MkStatic(hwnd, L"Custom accent color:", Page::Settings);
    SendMessage(g_lblAccent, WM_SETFONT, (WPARAM)g_state.hFontBold, TRUE);
    MkOwnerBtn(hwnd, ID_BTN_CUSTOM_ACCENT, L"Pick accent...", Page::Settings,
        L"Choose the highlight color used by the Custom theme.");

    g_lblZoom = MkStatic(hwnd, L"Interface size  (Ctrl + / - / 0):", Page::Settings);
    SendMessage(g_lblZoom, WM_SETFONT, (WPARAM)g_state.hFontBold, TRUE);
    MkOwnerBtn(hwnd, ID_BTN_ZOOM_OUT, L"- Smaller", Page::Settings, L"Shrink the interface.");
    MkOwnerBtn(hwnd, ID_BTN_ZOOM_RESET, L"100%", Page::Settings, L"Reset interface size.");
    MkOwnerBtn(hwnd, ID_BTN_ZOOM_IN, L"+ Bigger", Page::Settings, L"Enlarge the interface.");

    g_lblTips = MkStatic(hwnd, L"Beginner tips panel:", Page::Settings);
    SendMessage(g_lblTips, WM_SETFONT, (WPARAM)g_state.hFontBold, TRUE);
    HWND tog = CreateToggle(hwnd, ID_TOG_TIPS, g_state.showTips);
    Tag(tog, Page::Settings); AddTipCtl(tog, L"Shows the blue tip panel with guidance.");

    g_lblVault = MkStatic(hwnd, L"Saved passwords (encrypted with Windows DPAPI):", Page::Settings);
    SendMessage(g_lblVault, WM_SETFONT, (WPARAM)g_state.hFontBold, TRUE);
    {
        HWND h = MkListView(hwnd, ID_VAULT_LIST, Page::Settings, L"Passwords are hidden and tried automatically on locked archives.");
        AddColumn(h, 0, L"Saved passwords", 260);
        g_state.hwndVaultList = h;
    }
    MkEdit(hwnd, ID_TXT_NEW_PASSWORD, Page::Settings, L"Type a password to remember.", ES_PASSWORD);
    MkOwnerBtn(hwnd, ID_BTN_ADD_PASSWORD, L"Add", Page::Settings, L"Save the typed password (encrypted).");
    MkOwnerBtn(hwnd, ID_BTN_REMOVE_PASSWORD, L"Remove", Page::Settings, L"Forget the highlighted password.");
    MkPushBtn(hwnd, ID_BTN_CLEAR_PASSWORDS, L"Clear all", Page::Settings, L"Forget every saved password.");

    g_lblCtx = MkStatic(hwnd, L"Right-click menu in Explorer:", Page::Settings);
    SendMessage(g_lblCtx, WM_SETFONT, (WPARAM)g_state.hFontBold, TRUE);
    MkOwnerBtn(hwnd, ID_BTN_REGISTER_MENU, L"Register menu", Page::Settings, L"Adds Star Extract actions to every file's right-click menu.");
    MkPushBtn(hwnd, ID_BTN_UNREGISTER_MENU, L"Remove menu", Page::Settings, L"Removes Star Extract from the right-click menu.");
    g_lblSetNote = MkStatic(hwnd, L"Includes Separate/Combine for 7z+zip and a dedicated LessMSI entry for .msi files.", Page::Settings);

    g_lblDbl = MkStatic(hwnd, L"Double-click an archive in Explorer:", Page::Settings);
    SendMessage(g_lblDbl, WM_SETFONT, (WPARAM)g_state.hFontBold, TRUE);
    MkRadio(hwnd, ID_RAD_DBL_EXTRACT, L"Quick extract here (unzip without opening the app)", Page::Settings,
        L"Double-clicking an archive unpacks it into a folder next to itself.", true, g_state.doubleExtract);
    MkRadio(hwnd, ID_RAD_DBL_OPEN, L"Open in Star Extract", Page::Settings,
        L"Double-clicking an archive opens it in the Star Extract browser first.", false, !g_state.doubleExtract);
    MkPushBtn(hwnd, ID_BTN_UNREG_SHELL, L"Remove double-click behavior", Page::Settings,
        L"Stops Star Extract from handling archive double-clicks; other apps take over again.");

    g_state.hwndStatus = CreateWindow(STATUSCLASSNAME, L"", WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)ID_STATUS_BAR, GetModuleHandle(nullptr), nullptr);
    SendMessage(g_state.hwndStatus, WM_SETFONT, (WPARAM)g_state.hFontUI, TRUE);

    LoadPasswords();
    LoadComponents();
    RefreshVaultList();
    RefreshToolGrid();
    RefreshPluginGrid();
    SetStatus(L"Ready. Start on Home: everything begins there.");
    LogMessage(L"Star Extract " + std::wstring(SE_VERSION_STR) + L" ready.");
}

// ---------------- layout ----------------
static void Place(HWND h, int x, int y, int w, int hh) { if (h) MoveWindow(h, x, y, w, hh, TRUE); }

static void LayoutExtract(int x, int y, int fw) {
    HWND main = g_state.hwndMain;
    int eh = Z(40), bz = Z(130);
    Place(g_extractHint, x, y, fw, Z(30)); y += Z(36);
    Place(g_exLbl1, x, y + Z(6), Z(120), Z(28));
    Place(GetItem(main, ID_TXT_SRC), x + Z(130), y, fw - Z(130) - bz - Z(8), eh);
    Place(GetItem(main, ID_BTN_SRC_BROWSE), x + fw - bz, y, bz, eh);
    y += eh + Z(8);
    Place(g_exLbl2, x, y + Z(6), Z(200), Z(28));
    Place(GetItem(main, ID_TXT_DEST), x + Z(210), y, fw - Z(210) - bz - Z(8), eh);
    Place(GetItem(main, ID_BTN_DEST_BROWSE), x + fw - bz, y, bz, eh);
    y += eh + Z(10);
    Place(g_state.hwndChkSubfolder, x, y, Z(260), Z(32));
    Place(g_state.hwndChkOpenAfter, x + Z(270), y, Z(260), Z(32));
    Place(g_state.hwndChkDeleteAfter, x + Z(540), y, Z(260), Z(32));
    y += Z(40);
    Place(g_exPwdLbl, x, y + Z(4), Z(420), Z(28));
    y += Z(32);
    Place(GetItem(main, ID_TXT_PASSWORD), x, y, Z(320), eh);
    Place(GetItem(main, ID_CHK_REMEMBER_PWD), x + Z(330), y + Z(4), Z(260), Z(32));
    Place(g_exEngCap, x + Z(600), y + Z(4), Z(170), Z(28));
    Place(GetItem(main, ID_ENGINE_LABEL), x + Z(770), y + Z(4), fw - Z(770), Z(28));
    y += eh + Z(10);
    Place(g_state.hwndBtnStart, x, y, Z(320), Z(60));
    Place(g_state.hwndBtnCancel, x + Z(330), y, Z(120), Z(60));
    Place(g_state.hwndProgress, x + Z(462), y + Z(10), fw - Z(462) - Z(300), Z(24));
    Place(GetItem(main, ID_TXT_PROGRESS), x + Z(462), y + Z(38), fw - Z(462) - Z(80), Z(26));
    Place(g_state.hwndTxtPercent, x + fw - Z(70), y + Z(10), Z(70), Z(28));
    y += Z(72);
    Place(g_state.hwndLog, x, y, fw, Z(130));
    y += Z(130) + Z(8);
    Place(GetItem(main, ID_BTN_CLEAR_LOG), x, y, Z(130), Z(42));
    Place(GetItem(main, ID_BTN_SAVE_LOG), x + Z(140), y, Z(130), Z(42));
}

static void LayoutCompress(int x, int y, int fw, int ch) {
    HWND main = g_state.hwndMain;
    Place(g_compHint, x, y, fw, Z(52)); y += Z(58);
    int listH = Z(190);
    Place(GetItem(main, ID_COMP_LIST), x, y, fw, listH); y += listH + Z(8);
    int bw = Z(150), bh = Z(50);
    int bx = x;
    Place(GetItem(main, ID_BTN_COMP_ADD), bx, y, bw, bh); bx += bw + Z(8);
    Place(GetItem(main, ID_BTN_COMP_REMOVE), bx, y, bw, bh); bx += bw + Z(8);
    Place(GetItem(main, ID_BTN_COMP_CLEAR), bx, y, Z(110), bh);
    y += bh + Z(10);
    Place(GetItem(main, ID_RAD_FMT_7Z), x, y, Z(380), Z(32));
    Place(GetItem(main, ID_RAD_FMT_ZIP), x + Z(390), y, Z(340), Z(32));
    y += Z(38);
    Place(GetItem(main, ID_RAD_MODE_COMBINE), x, y, Z(380), Z(32));
    Place(GetItem(main, ID_RAD_MODE_SEPARATE), x + Z(390), y, Z(340), Z(32));
    y += Z(38);
    Place(g_compOutLabel, x, y, Z(260), Z(26)); y += Z(30);
    int eh = Z(40), bz = Z(130);
    Place(GetItem(main, ID_TXT_COMP_OUT), x, y, fw - bz - Z(220), eh);
    Place(GetItem(main, ID_BTN_COMP_OUT_BROWSE), x + fw - bz - Z(212), y, bz, eh);
    Place(GetItem(main, ID_BTN_COMP_GO), x + fw - Z(200), y - Z(6), Z(200), Z(56));
    y += eh + Z(10);
    Place(g_compNote, x, y, fw, Z(30));
    (void)ch;
}

// Tools page: helper-tool table on top, component manager underneath, results
// area at the bottom.
static void LayoutTools(int x, int y, int fw, int ch) {
    HWND main = g_state.hwndMain;
    (void)ch;
    const int bw = Z(150), bh = Z(44), gap = Z(8);

    int gridH = Z(140);
    Place(GetItem(main, ID_TOOL_GRID), x, y, fw, gridH); y += gridH + Z(10);

    int pluginGridH = Z(190);
    Place(GetItem(main, ID_PLUGIN_GRID), x, y, fw, pluginGridH); y += pluginGridH + Z(10);

    int bx = x;
    auto buttonRow = [&](const int* ids, int count) {
        int cx = x;
        for (int i = 0; i < count; i++) {
            Place(GetItem(main, ids[i]), cx, y, bw, bh);
            cx += bw + gap;
        }
        y += bh + Z(8);
        (void)bx;
    };
    const int actionIds[] = { ID_BTN_PLUGIN_DOWNLOAD, ID_BTN_PLUGIN_UPDATE_ALL,
                              ID_BTN_PLUGIN_CANCEL, ID_BTN_PLUGIN_REMOVE,
                              ID_BTN_PLUGIN_SAVEURL, ID_BTN_PLUGIN_RESET };
    buttonRow(actionIds, (int)(sizeof(actionIds) / sizeof(actionIds[0])));

    Place(g_lblPluginUrl, x, y, fw, Z(24)); y += Z(28);
    Place(g_state.hwndTxtPluginUrl, x, y, fw, Z(32)); y += Z(38);

    Place(g_lblPluginName, x, y, fw, Z(24)); y += Z(28);
    int third = (fw - bw - gap * 3) / 3;   // leave room for the Add button
    Place(g_state.hwndTxtPluginName, x, y, third, Z(32));
    Place(g_state.hwndTxtPluginVerify, x + third + gap, y, third, Z(32));
    Place(g_state.hwndBtnPluginAdd, x + fw - bw, y, bw, Z(32));
    y += Z(42);

    const int toolIds[] = { ID_BTN_TOOL_REFRESH, ID_BTN_HASH, ID_BTN_IDENTIFY, ID_BTN_TEST2 + 1000 };
    buttonRow(toolIds, (int)(sizeof(toolIds) / sizeof(toolIds[0])));

    RECT rc; GetClientRect(main, &rc);
    int bottom = rc.bottom - Z(40) - Z(16);
    if (bottom < y + Z(60)) bottom = y + Z(60);
    Place(g_toolResult, x, y, fw, bottom - y);
}


static void LayoutSettings(int x, int y, int fw, int ch) {
    HWND main = g_state.hwndMain;
    (void)ch;
    int yy = y;

    // theme: label + 4x2 preview grid
    Place(g_lblTheme, x, yy, fw, Z(28)); yy += Z(34);
    int gap = Z(12);
    int cw = (fw - gap * 3) / 4;
    int chh = Z(96);
    for (int i = 0; i < (int)Theme::Count; i++) {
        Place(GetItem(main, ID_RAD_THEME_BEGIN + i),
            x + (i % 4) * (cw + gap), yy + (i / 4) * (chh + gap), cw, chh);
    }
    yy += chh * 2 + gap + Z(14);

    // custom accent
    Place(g_lblAccent, x, yy, Z(200), Z(34));
    Place(GetItem(main, ID_BTN_CUSTOM_ACCENT), x + Z(210), yy - Z(3), Z(180), Z(40));
    yy += Z(48);

    // two columns below: left = size/tips/menu, right = passwords
    int colGap = Z(24);
    int colW = (fw - colGap) / 2;
    int lx = x, rx = x + colW + colGap;

    Place(g_lblZoom, lx, yy, colW, Z(28));
    int zy = yy + Z(32);
    Place(GetItem(main, ID_BTN_ZOOM_OUT), lx, zy, Z(150), Z(44));
    Place(GetItem(main, ID_BTN_ZOOM_RESET), lx + Z(160), zy, Z(110), Z(44));
    Place(GetItem(main, ID_BTN_ZOOM_IN), lx + Z(280), zy, Z(150), Z(44));
    zy += Z(54);
    Place(g_lblTips, lx, zy + Z(4), Z(240), Z(28));
    Place(GetItem(main, ID_TOG_TIPS), lx + Z(250), zy, Z(70), Z(36));
    zy += Z(50);
    Place(g_lblCtx, lx, zy, colW, Z(28)); zy += Z(34);
    Place(GetItem(main, ID_BTN_REGISTER_MENU), lx, zy, Z(200), Z(46));
    Place(GetItem(main, ID_BTN_UNREGISTER_MENU), lx + Z(210), zy, Z(170), Z(46));
    zy += Z(56);
    Place(g_lblSetNote, lx, zy, colW, Z(44));
    zy += Z(50);
    Place(g_lblDbl, lx, zy, colW, Z(28)); zy += Z(32);
    Place(GetItem(main, ID_RAD_DBL_EXTRACT), lx, zy, colW - Z(12), Z(28)); zy += Z(30);
    Place(GetItem(main, ID_RAD_DBL_OPEN), lx, zy, colW - Z(12), Z(28)); zy += Z(30);
    Place(GetItem(main, ID_BTN_UNREG_SHELL), lx, zy, Z(220), Z(34));

    Place(g_lblVault, rx, yy, colW, Z(28));
    int vy = yy + Z(32);
    Place(GetItem(main, ID_VAULT_LIST), rx, vy, colW, Z(150)); vy += Z(158);
    Place(GetItem(main, ID_TXT_NEW_PASSWORD), rx, vy, colW - Z(120), Z(38));
    Place(GetItem(main, ID_BTN_ADD_PASSWORD), rx + colW - Z(112), vy - Z(3), Z(112), Z(44));
    vy += Z(52);
    Place(GetItem(main, ID_BTN_REMOVE_PASSWORD), rx, vy, Z(150), Z(44));
    Place(GetItem(main, ID_BTN_CLEAR_PASSWORDS), rx + Z(160), vy, Z(150), Z(44));
}

void LayoutMain() {
    HWND hwnd = g_state.hwndMain;
    if (!hwnd) return;
    RECT rc; GetClientRect(hwnd, &rc);
    int W = rc.right, H = rc.bottom;
    int m = Z(16), navW = Z(196), stH = Z(40);
    int brandH = Z(58);

    int ny = m + brandH;
    for (int i = 0; i < 6; i++) {
        HWND h = GetItem(hwnd, ID_NAV_HOME + i);
        Place(h, m / 2, ny, navW - m, Z(52));
        ny += Z(52) + Z(6);
    }
    // version footer on rail
    // (drawn via status bar instead)

    int cx = navW + m, cw = W - cx - m;
    int cy = m;

    for (auto& c : g_ctls) ShowWindow(c.h, (c.p == g_state.page || c.p == Page::Count) ? SW_SHOW : SW_HIDE);

    if (g_state.page == Page::Home) {
        int y = cy + m;
        Place(g_heroTitle, cx + m, y, cw - m * 2, Z(48)); y += Z(54);
        Place(g_heroSub, cx + m, y, cw - m * 2, Z(26)); y += Z(36);
        int cwid = (cw - m * 4) / 3;
        Place(GetItem(hwnd, ID_CARD_OPEN), cx + m, y, cwid, Z(176));
        Place(GetItem(hwnd, ID_CARD_EXTRACT), cx + m * 2 + cwid, y, cwid, Z(176));
        Place(GetItem(hwnd, ID_CARD_COMPRESS), cx + m * 3 + cwid * 2, y, cwid, Z(176));
        y += Z(188);
        Place(g_stepsLabel, cx + m, y, cw - m * 2, Z(30)); y += Z(38);
        HWND tipCard = GetItem(hwnd, ID_CARD_TIP);
        HWND nextTip = GetItem(hwnd, ID_TIP_NEXT);
        int vis = g_state.showTips ? SW_SHOW : SW_HIDE;
        if (tipCard) ShowWindow(tipCard, vis);
        if (g_tipText) ShowWindow(g_tipText, vis);
        if (nextTip) ShowWindow(nextTip, vis);
        if (g_state.showTips) {
            Place(tipCard, cx + m, y, Z(220), Z(120));
            Place(g_tipText, cx + m + Z(232), y, cw - m * 3 - Z(232) - Z(150), Z(120));
            Place(nextTip, cx + cw - m - Z(140), y + Z(38), Z(140), Z(44));
        }
    } else if (g_state.page == Page::Browser) {
        int y = cy + m / 2;
        int bw = Z(120), bh = Z(40);
        int x = cx + m;
        // Path label
        Place(g_pathLabel, x, y, cw - m * 2, Z(28)); y += Z(32);
        // Breadcrumb bar
        Place(g_state.hwndBreadcrumbBar, x, y, cw - m * 2 - Z(100), Z(26));
        // Navigation buttons: Back, Forward, Up (right aligned, one row)
        x = cx + cw - m - Z(100);
        Place(g_state.hwndBtnBack, x, y, Z(30), Z(30)); x += Z(32);
        Place(g_state.hwndBtnFwd, x, y, Z(30), Z(30)); x += Z(32);
        Place(g_state.hwndBtnUp2, x, y, Z(30), Z(30));
        y += Z(36);
        // Toolbar buttons
        x = cx + m;
        const int ids[] = { ID_BTN_OPEN, ID_BTN_UP, ID_BTN_EXTRACT_ALL, ID_BTN_EXTRACT_SEL,
                            ID_BTN_PREVIEW, ID_BTN_TEST2, ID_BTN_DELETE2, ID_BTN_EXTRACT_TO, ID_BTN_7ZFM };
        for (int id : ids) { Place(GetItem(hwnd, id), x, y, bw, bh); x += bw + Z(8); }
        y += bh + Z(8);
        Place(g_state.hwndFilter, cx + m, y, cw - m * 2, Z(30)); y += Z(36);
        Place(g_engineBadge, cx + m, y, cw - m * 2, Z(26)); y += Z(30);
        // Tree list (left) and File list (right)
        int treeW = Z(220);
        int listH = H - stH - y - Z(44);
        Place(g_state.hwndTreeList, cx + m, y, treeW, listH);
        Place(g_state.hwndFileList, cx + m + treeW + Z(8), y, cw - m * 2 - treeW - Z(8), listH);
        Place(g_browserHint, cx + m, H - stH - Z(36), cw - m * 2, Z(28));
    } else if (g_state.page == Page::Extract) {
        LayoutExtract(cx + m, cy + m / 2, cw - m * 2);
    } else if (g_state.page == Page::Compress) {
        LayoutCompress(cx + m, cy + m / 2, cw - m * 2, H - stH);
    } else if (g_state.page == Page::Tools) {
        LayoutTools(cx + m, cy + m / 2, cw - m * 2, H - stH);
    } else if (g_state.page == Page::Settings) {
        LayoutSettings(cx + m, cy + m / 2, cw - m * 2, H - stH);
    }

    Place(g_state.hwndStatus, 0, H - stH, W, stH);
    int parts[6] = { W - Z(560), W - Z(440), W - Z(330), W - Z(230), W - Z(130), -1 };
    SendMessage(g_state.hwndStatus, SB_SETPARTS, 6, (LPARAM)parts);
    wchar_t ver[64]; wcscpy(ver, SE_VERSION_STR);
    SetStatusPart(5, std::wstring(L"v") + ver);
    std::wstring eng = g_state.currentArchive.empty() ? L"Engine: -" : L"Engine: " + DetectEngine(g_state.currentArchive);
    SetStatusPart(4, eng);
    UpdateBrowserStatus();
    InvalidateRect(hwnd, nullptr, TRUE);
}

// ---------------- drop files ----------------
static void HandleDrop(HDROP hDrop) {
    UINT n = DragQueryFile(hDrop, 0xFFFFFFFF, nullptr, 0);
    std::vector<std::wstring> archives, others, dirs;
    for (UINT i = 0; i < n; i++) {
        wchar_t p[MAX_PATH];
        DragQueryFile(hDrop, i, p, MAX_PATH);
        std::wstring s = p;
        if (DirectoryExists(s)) dirs.push_back(s);
        else if (IsArchiveLike(s)) archives.push_back(s);
        else others.push_back(s);
    }
    DragFinish(hDrop);
    if (archives.size() == 1 && others.empty() && dirs.empty()) {
        DoOpenArchive(archives[0]);   // lands on the Browser
        return;
    }
    std::vector<std::wstring> all = archives;
    all.insert(all.end(), others.begin(), others.end());
    all.insert(all.end(), dirs.begin(), dirs.end());
    if (!all.empty()) {
        AddCompFiles(all);
        ShowPage(Page::Compress);
        SetStatus(L"Dropped files added to Compress.");
    }
}

// ---------------- CLI (headless, before GUI) ----------------
// returns true if it handled the command and the app should exit
static bool HandleCLI() {
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv || argc < 2) { if (argv) LocalFree(argv); return false; }
    std::vector<std::wstring> files;
    bool fExtract = false, fHere = false, fAdd7z = false, fAddZip = false;
    bool fC7z = false, fCZip = false, fReg = false, fUnreg = false, fTest = false, fHash = false, fSha = false;
    for (int i = 1; i < argc; i++) {
        std::wstring a = argv[i];
        std::wstring l = a;
        for (auto& ch : l) ch = towlower(ch);
        if (l == L"/extract" || l == L"-extract") fExtract = true;
        else if (l == L"/extracthere" || l == L"-extracthere") fHere = true;
        else if (l == L"/add7z" || l == L"-add7z") fAdd7z = true;
        else if (l == L"/addzip" || l == L"-addzip") fAddZip = true;
        else if (l == L"/combine7z" || l == L"-combine7z") fC7z = true;
        else if (l == L"/combinezip" || l == L"-combinezip") fCZip = true;
        else if (l == L"/register" || l == L"-register") fReg = true;
        else if (l == L"/unregister" || l == L"-unregister") fUnreg = true;
        else if (l == L"/test" || l == L"-test") fTest = true;
        else if (l == L"/hash" || l == L"-hash") fHash = true;
    else if (l == L"/sha256" || l == L"-sha256") fSha = true;
        else if (!a.empty() && a[0] != L'/' && a[0] != L'-') files.push_back(a);
    }
    LocalFree(argv);
    auto need7z = [&]() {
        if (!FileExists(Get7zPath())) {
            MessageBox(nullptr, L"7z.exe was not found next to StarExtractNative.exe.", L"Star Extract", MB_OK | MB_ICONERROR);
            return false;
        }
        return true;
    };
    if (fReg) { RegisterContextMenu(); return true; }
    if (fUnreg) { UnregisterContextMenu(); return true; }
    if ((fExtract || fHere) && !files.empty()) {
        if (!need7z()) return true;
        LoadPasswords();
        for (auto& f : files) {
            std::wstring dir = f.substr(0, f.find_last_of(L"\\/"));
            std::wstring nm = f.substr(f.find_last_of(L"\\/") + 1);
            size_t d = nm.find_last_of(L'.');
            if (d != std::wstring::npos) nm = nm.substr(0, d);
            std::wstring dest = fHere ? dir : dir + L"\\" + nm;
            CreateDirectory(dest.c_str(), nullptr);
            ExtractWithPasswordFlow(f, dest, L"", false);
        }
        return true;
    }
    auto doSeparate = [&](const std::wstring& fmt, const std::wstring& ext) {
        if (!need7z()) return;
        for (auto& f : files) {
            std::wstring dir = f.substr(0, f.find_last_of(L"\\/"));
            std::wstring nm = f.substr(f.find_last_of(L"\\/") + 1);
            size_t d = nm.find_last_of(L'.');
            if (d != std::wstring::npos) nm = nm.substr(0, d);
            RunProcess(Get7zPath(), L"a " + fmt + L" \"" + dir + L"\\" + nm + ext + L"\" \"" + f + L"\"");
        }
    };
    auto doCombine = [&](const std::wstring& fmt, const std::wstring& ext) {
        if (!need7z() || files.empty()) return;
        std::wstring dir = files[0].substr(0, files[0].find_last_of(L"\\/"));
        std::wstring out = dir + L"\\Combined" + ext;
        int k = 2;
        while (FileExists(out)) out = dir + L"\\Combined (" + std::to_wstring(k++) + L")" + ext;
        std::wstring args = L"a " + fmt + L" \"" + out + L"\"";
        for (auto& f : files) args += L" \"" + f + L"\"";
        RunProcess(Get7zPath(), args);
    };
    if (fAdd7z && !files.empty()) { doSeparate(L"-t7z", L".7z"); return true; }
    if (fAddZip && !files.empty()) { doSeparate(L"-tzip", L".zip"); return true; }
    if (fC7z && !files.empty()) { doCombine(L"-t7z", L".7z"); return true; }
    if (fCZip && !files.empty()) { doCombine(L"-tzip", L".zip"); return true; }
    if (fTest && !files.empty()) {
        if (!need7z()) return true;
        for (auto& f : files) RunProcess(Get7zPath(), L"t \"" + f + L"\"");
        return true;
    }
    if (fHash && !files.empty()) {
        if (!need7z()) return true;
        for (auto& f : files) RunProcess(Get7zPath(), L"h -scrcSHA256 \"" + f + L"\"");
        return true;
    }
    if (fSha && !files.empty()) {
        // same hash the component installer pins, printed so a downloaded
        // component archive can be checked by hand
        for (auto& f : files) {
            std::wstring sha = Sha256OfFile(f);
            if (sha.empty()) sha = L"(could not read the file)";
            wprintf(L"%s  %s\n", sha.c_str(), f.c_str());
        }
        return true;
    }
    // bare file path (no flags): open it in the browser - this is what the
    // Explorer "Open in Star Extract" double-click association does
    if (!files.empty()) {
        g_state.pendingOpen = files[0];
        return false;
    }
    return false;
}

// ---------------- tools page actions ----------------
static void DoHashFlow() {
    OPENFILENAME ofn = { 0 };
    wchar_t file[MAX_PATH] = L"";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_state.hwndMain;
    ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"All files\0*.*\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Hash a file - pick anything to fingerprint";
    if (!GetOpenFileName(&ofn)) return;
    std::wstring out = HashFile(file, L"SHA256");
    size_t p = out.find(L"\n");
    std::wstring first = (p == std::wstring::npos) ? out : out.substr(0, p);
    std::wstring crc = HashFile(file, L"CRC32");
    SetWindowTextString(g_toolResult, (L"SHA-256:\n" + first + L"\n\nCRC32:\n" + crc).c_str());
    LogMessage(L"Hashed: " + std::wstring(file));
}
static void DoIdentifyFlow() {
    OPENFILENAME ofn = { 0 };
    wchar_t file[MAX_PATH] = L"";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_state.hwndMain;
    ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"All files\0*.*\0";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Identify a file - what is it really?";
    if (!GetOpenFileName(&ofn)) return;
    SetWindowTextString(g_toolResult, IdentifyFile(file).c_str());
    LogMessage(L"Identified: " + std::wstring(file));
}
static void DoTestCurrent() {
    if (g_state.currentArchive.empty()) {
        MessageBox(g_state.hwndMain, L"Open an archive first.", L"Star Extract", MB_OK | MB_ICONINFORMATION);
        return;
    }
    std::wstring out = RunProcessCapture(Get7zPath(), L"t \"" + g_state.currentArchive + L"\"");
    bool ok = out.find(L"Everything is Ok") != std::wstring::npos;
    SetWindowTextString(g_toolResult, (ok ? L"Integrity test PASSED.\n\n" : L"Integrity test found problems (or a password is needed).\n\n") + out.substr(0, 1500));
}

// ---------------- window proc ----------------
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        CreateControls(hwnd);
        DragAcceptFiles(hwnd, TRUE);
        ApplyTheme(g_state.currentTheme);
        LayoutMain();
        SetStatus(L"Ready. Start on Home: everything begins there.");
        {
            HWND re = GetItem(hwnd, ID_RAD_DBL_EXTRACT);
            HWND ro = GetItem(hwnd, ID_RAD_DBL_OPEN);
            bool registered = IsShellAssociationRegistered();
            if (re) SendMessage(re, BM_SETCHECK, (registered && g_state.doubleExtract) ? BST_CHECKED : BST_UNCHECKED, 0);
            if (ro) SendMessage(ro, BM_SETCHECK, (registered && !g_state.doubleExtract) ? BST_CHECKED : BST_UNCHECKED, 0);
        }
        if (!g_state.pendingOpen.empty()) {
            std::wstring p = g_state.pendingOpen;
            g_state.pendingOpen.clear();
            DoOpenArchive(p);
        }
        return 0;
    case WM_SIZE:
        LayoutMain();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        int W = rc.right, H = rc.bottom;
        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, W, H);
        HGDIOBJ oldBmp = SelectObject(mem, bmp);
        SetBkMode(mem, TRANSPARENT);
        SetGraphicsMode(mem, GM_ADVANCED);

        int navW = Z(196), stH = Z(40), brandH = Z(58);
        ThemeColors c = g_state.colors;

        // page background: soft vignette
        FillGradient(mem, rc, c.bgWindow, MixColor(c.bgWindow, RGB(0, 0, 0), 0.10));

        // content panel
        RECT panel = { navW + Z(4), Z(4), W - Z(4), H - stH - Z(4) };
        if (panel.right - panel.left > Z(40) && panel.bottom - panel.top > Z(40)) {
            int rad = Z(16);
            HRGN pr = CreateRoundRectRgn(panel.left, panel.top, panel.right, panel.bottom, rad, rad);
            SelectClipRgn(mem, pr);
            FillGradient(mem, panel, MixColor(c.bgSurface, c.bgWindow, 0.55), c.bgWindow);
            DeleteObject(pr);
            SelectClipRgn(mem, nullptr);
            HPEN pp = CreatePen(PS_SOLID, 1, MixColor(c.border, c.bgWindow, 0.45));
            DrawRoundRect(mem, panel, rad, nullptr, pp);
            DeleteObject(pp);
            RECT phi = { panel.left + Z(8), panel.top + 1, panel.right - Z(8), panel.top + 2 };
            HBRUSH phb = CreateSolidBrush(MixColor(MixColor(c.bgSurface, c.bgWindow, 0.55), RGB(255, 255, 255), 0.12));
            FillRect(mem, &phi, phb);
            DeleteObject(phb);
        }

        // nav rail
        RECT rail = { Z(4), Z(4), navW - Z(4), H - stH - Z(4) };
        if (rail.right - rail.left > Z(40) && rail.bottom - rail.top > Z(40)) {
            int rad = Z(16);
            HRGN rr = CreateRoundRectRgn(rail.left, rail.top, rail.right, rail.bottom, rad, rad);
            SelectClipRgn(mem, rr);
            FillGradient(mem, rail, MixColor(c.navBg, c.bgWindow, 0.45), MixColor(c.navBg, RGB(0, 0, 0), 0.10));
            DeleteObject(rr);
            SelectClipRgn(mem, nullptr);
            HPEN rp = CreatePen(PS_SOLID, 1, MixColor(c.border, c.bgWindow, 0.35));
            DrawRoundRect(mem, rail, rad, nullptr, rp);
            DeleteObject(rp);

            // brand: accent tile + name + version
            RECT tile = { Z(14), Z(14), Z(52), Z(52) };
            int tr = Z(10);
            HRGN tg = CreateRoundRectRgn(tile.left, tile.top, tile.right, tile.bottom, tr, tr);
            SelectClipRgn(mem, tg);
            FillGradient(mem, tile, MixColor(c.accent, RGB(255, 255, 255), 0.22), MixColor(c.accent, RGB(0, 0, 0), 0.16));
            DeleteObject(tg);
            SelectClipRgn(mem, nullptr);
            HPEN tp = CreatePen(PS_SOLID, 1, MixColor(c.accent, RGB(0, 0, 0), 0.35));
            DrawRoundRect(mem, tile, tr, nullptr, tp);
            DeleteObject(tp);
            RECT glyph = { tile.left + Z(8), tile.top + Z(8), tile.right - Z(8), tile.bottom - Z(8) };
            DrawIconGlyph(mem, glyph, ICON_BOLT, RGB(255, 255, 255));

            RECT nameRc = { Z(60), Z(14), navW - Z(8), Z(38) };
            HFONT oldF = (HFONT)SelectObject(mem, g_state.hFontBold);
            SetTextColor(mem, c.textPrimary);
            DrawText(mem, L"Star Extract", -1, &nameRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            RECT verRc = { Z(60), Z(36), navW - Z(8), Z(56) };
            SelectObject(mem, g_state.hFontTip);
            SetTextColor(mem, c.textMuted);
            std::wstring ver = L"v" + std::wstring(SE_VERSION_STR);
            DrawText(mem, ver.c_str(), -1, &verRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectObject(mem, oldF);

            // separator under the brand
            int sepY = Z(16) + brandH - Z(8);
            RECT sep = { Z(14), sepY, navW - Z(14), sepY + 1 };
            HBRUSH sb = CreateSolidBrush(MixColor(c.border, c.bgWindow, 0.30));
            FillRect(mem, &sep, sb);
            DeleteObject(sb);
        }

        BitBlt(hdc, 0, 0, W, H, mem, 0, 0, SRCCOPY);
        SelectObject(mem, oldBmp);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DROPFILES:
        HandleDrop((HDROP)wp);
        return 0;
    case WM_APP_STREAM_LINE: {
        std::wstring* line = (std::wstring*)lp;
        if (line) {
            HandleStreamLine(*line);
            delete line;
        }
        return 0;
    }
    case WM_APP_STREAM_DONE: {
        int code = (int)wp;
        bool cancelled = (code == -2);
        if (g_job.kind == JobKind::Compress) {
            g_job.kind = JobKind::None;
            ResetProgressUI();
            SetCurrentItemText(cancelled ? L"Cancelled." : (code == 0 ? L"Done." : L"Failed - see log."));
            SetStatus(cancelled ? L"Cancelled." : (code == 0 ? L"Archive created." : L"Compression failed."));
            LogMessage(cancelled ? L"Compression cancelled." :
                (code == 0 ? L"Compressed: " + g_job.src : L"7z reported an error while compressing."));
        } else {
            FinishExtraction(code == 0 && !cancelled, cancelled);
        }
        return 0;
    }
    case WM_APP_COMP_PROGRESS: {
        std::wstring* text = (std::wstring*)lp;
        if (text) {
            int pct = (int)wp;
            std::wstring stage = *text;
            SetWindowTextString(GetItem(hwnd, ID_TXT_PLUGIN_STATUS),
                pct >= 0 ? (stage + L"  " + std::to_wstring(pct) + L"%").c_str() : stage.c_str());
            if (pct >= 0) SetProgress(pct);
            delete text;
        }
        return 0;
    }
    case WM_APP_COMP_DONE: {
        CompResult* r = (CompResult*)lp;
        if (!r) return 0;
        int finished = r->code;
        std::wstring report = r->message;
        std::wstring sha = r->sha256;
        delete r;

        // pin what we actually fetched so the next update can be checked
        if (!sha.empty() && !g_queue.empty()) {
            int idx = g_queue.front();
            if (ComponentField(idx, COMP_FIELD_SHA256).empty()) PinComponentHash(idx, sha);
        }
        if (!g_queue.empty()) g_queue.erase(g_queue.begin());

        RefreshPluginGrid();
        ResetProgressUI();
        SetWindowTextString(GetItem(hwnd, ID_TXT_PLUGIN_STATUS), report.c_str());
        LogMessage(report);
        if (finished == 0) {
            SetStatus(L"Component installed.");
            MessageBox(hwnd, report.c_str(), L"Component installed", MB_OK | MB_ICONINFORMATION);
        } else if (finished == 2) {
            SetStatus(L"Download cancelled.");
        } else {
            SetStatus(L"Install failed - see the log.");
            MessageBox(hwnd, report.c_str(), L"Install failed", MB_OK | MB_ICONWARNING);
        }
        StartNextComponent();   // keep going through the queue
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        // live filter box in the browser
        if (id == ID_TXT_FILTER && HIWORD(wp) == EN_CHANGE) {
            g_state.filterText = GetWindowTextString(g_state.hwndFilter);
            if (!g_state.filterText.empty()) g_state.currentFolder.clear();
            RefreshBrowserList();
            return 0;
        }
        // nav
        if (id >= ID_NAV_HOME && id <= ID_NAV_SETTINGS) { ShowPage((Page)(id - ID_NAV_HOME)); return 0; }
        if (id >= ID_RAD_THEME_BEGIN && id < ID_RAD_THEME_BEGIN + (int)Theme::Count) {
            ApplyTheme((Theme)(id - ID_RAD_THEME_BEGIN));
            SaveUiSettings();
            return 0;
        }
        switch (id) {
        case ID_CARD_OPEN: BrowseOpen(); break;
        case ID_CARD_EXTRACT:
            if (g_state.currentArchive.empty()) BrowseOpen();
            else ShowPage(Page::Extract);
            break;
        case ID_CARD_COMPRESS: ShowPage(Page::Compress); break;
        case ID_TIP_NEXT:
            g_state.tipIndex = (g_state.tipIndex + 1) % 6;
            SetWindowTextString(g_tipText, kTips[g_state.tipIndex]);
            break;
        case ID_BTN_CUSTOM_ACCENT: {
            CHOOSECOLOR cc = { sizeof(cc) };
            static COLORREF custom[16] = { 0 };
            cc.hwndOwner = hwnd;
            cc.lpCustColors = custom;
            cc.rgbResult = g_customAccent;
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;
            if (ChooseColor(&cc)) {
                g_customAccent = cc.rgbResult;
                ApplyTheme(Theme::Custom);
                SaveUiSettings();
                SetStatus(L"Custom accent color updated.");
            }
            break;
        }
        case ID_BTN_ZOOM_OUT:
            ApplyZoom(g_state.ui.zoom - AppState::ZOOM_STEP); break;
        case ID_BTN_ZOOM_RESET: ApplyZoom(1.0); break;
        case ID_BTN_ZOOM_IN:
            ApplyZoom(g_state.ui.zoom + AppState::ZOOM_STEP); break;
        case ID_TOG_TIPS:
            g_state.showTips = Toggle_Get((HWND)lp);
            if (!g_state.showTips) SetWindowTextString(g_tipText, L"Tips are off. Turn them back on in Settings anytime.");
            else SetWindowTextString(g_tipText, kTips[g_state.tipIndex]);
            LayoutMain();
            SaveUiSettings();
            break;
        case ID_BTN_OPEN: BrowseOpen(); break;
        case ID_BTN_BACK:
            if (g_state.historyPos > 0) {
                g_state.historyPos--;
                NavigateBrowser(g_state.folderHistory[g_state.historyPos], false);
            }
            break;
        case ID_BTN_FWD:
            if (g_state.historyPos + 1 < g_state.folderHistory.size()) {
                g_state.historyPos++;
                NavigateBrowser(g_state.folderHistory[g_state.historyPos], false);
            }
            break;
        case ID_BTN_UP2:
        case ID_BTN_UP:
            if (!g_state.filterText.empty()) {
                SetWindowTextString(g_state.hwndFilter, L"");
                g_state.filterText.clear();
            }
            if (!g_state.currentFolder.empty()) {
                size_t p = g_state.currentFolder.find_last_of(L'\\');
                g_state.currentFolder = (p == std::wstring::npos) ? L"" : g_state.currentFolder.substr(0, p);
                RefreshBrowserList();
                RefreshBreadcrumbs();
            }
            break;
        case ID_BTN_EXTRACT_TO:
            if (g_state.currentArchive.empty()) {
                MessageBox(hwnd, L"Please open an archive first.", L"Extract", MB_OK | MB_ICONINFORMATION);
                break;
            }
            SetWindowTextString(GetItem(hwnd, ID_TXT_SRC), g_state.currentArchive);
            SetWindowTextString(g_state.hwndDestEdit, L"");
            DoExtractFlow();
            break;
        case ID_BTN_7ZFM: {
            std::wstring fm = GetExeDir() + L"\\7zFM.exe";
            if (!g_state.currentArchive.empty()) {
                if (FileExists(fm)) RunProcess(fm, L"\"" + g_state.currentArchive + L"\"");
                else MessageBox(hwnd, L"7zFM.exe was not found next to the app.", L"7-Zip File Manager", MB_OK | MB_ICONWARNING);
            } else {
                MessageBox(hwnd, L"Open an archive first.", L"7-Zip File Manager", MB_OK | MB_ICONINFORMATION);
            }
            break;
        }
        case ID_BTN_CANCEL:
            if (IsStreamedProcessRunning()) {
                CancelStreamedProcess();
                SetStatus(L"Cancelling...");
                SetCurrentItemText(L"Cancelling...");
            }
            break;
        case ID_BTN_PLUGIN_UPDATE_ALL: {
            // queue every component that is not installed yet, one at a time
            int first = -1;
            for (int i = 0; i < ComponentCount(); i++) {
                if (ComponentField(i, COMP_FIELD_STATUS) != L"Installed") { first = i; break; }
            }
            if (first < 0) {
                SetStatus(L"Everything in the catalog is already installed.");
                break;
            }
            g_queue.clear();
            for (int i = 0; i < ComponentCount(); i++)
                if (ComponentField(i, COMP_FIELD_STATUS) != L"Installed") g_queue.push_back(i);
            StartNextComponent();
            break;
        }
        case ID_BTN_PLUGIN_DOWNLOAD: {
            int sel = SelectedPluginIndex();
            if (sel < 0) { SetStatus(L"Highlight a component first."); break; }
            g_queue.clear();
            g_queue.push_back(sel);
            StartNextComponent();
            break;
        }
        case ID_BTN_PLUGIN_CANCEL:
            if (IsComponentJobRunning()) {
                CancelComponentJob();
                g_queue.clear();
                SetStatus(L"Cancelling the download...");
                SetWindowTextString(GetItem(hwnd, ID_TXT_PLUGIN_STATUS), L"Cancelling...");
            }
            break;
        case ID_BTN_PLUGIN_SAVEURL: {
            int sel = SelectedPluginIndex();
            if (sel < 0) { SetStatus(L"Highlight a component first."); break; }
            std::wstring url = GetWindowTextString(g_state.hwndTxtPluginUrl);
            if (url.empty()) { SetStatus(L"The URL is empty - nothing to save."); break; }
            SetComponentUrl(sel, url);
            RefreshPluginGrid();
            LogMessage(L"Plugin URL saved: " + url);
            SetStatus(L"Saved. New downloads will use this address.");
            break;
        }
        case ID_BTN_PLUGIN_RESET:
            if (MessageBox(hwnd,
                L"Restore every default URL and drop the plugins you added?\n\nFiles already downloaded are left alone.",
                L"Reset components", MB_YESNO | MB_ICONQUESTION) != IDYES) break;
            ResetComponentsToDefaults();
            RefreshPluginGrid();
            LogMessage(L"Component list reset to defaults.");
            SetStatus(L"Defaults restored.");
            break;
        case ID_BTN_PLUGIN_REMOVE: {
            int sel = SelectedPluginIndex();
            if (sel < 0) { SetStatus(L"Highlight a component first."); break; }
            if (ComponentField(sel, COMP_FIELD_CUSTOM) != L"1") {
                MessageBox(hwnd, L"Built-in components cannot be removed.\nYou can change their source URL instead.",
                    L"Remove plugin", MB_OK | MB_ICONINFORMATION);
                break;
            }
            std::wstring name = ComponentField(sel, COMP_FIELD_NAME);
            if (MessageBox(hwnd, (L"Remove '" + name + L"' from the list?\n\nAlready-downloaded files stay on disk.").c_str(),
                L"Remove plugin", MB_YESNO | MB_ICONQUESTION) != IDYES) break;
            RemoveComponentAt(sel);
            RefreshPluginGrid();
            SetWindowTextString(g_state.hwndTxtPluginUrl, L"");
            SetStatus(L"Plugin removed from the list.");
            break;
        }
        case ID_BTN_PLUGIN_ADD: {
            std::wstring name = GetWindowTextString(g_state.hwndTxtPluginName);
            std::wstring url = GetWindowTextString(g_state.hwndTxtPluginUrl);
            std::wstring verify = GetWindowTextString(g_state.hwndTxtPluginVerify);
            while (!name.empty() && name.front() == L' ') name.erase(name.begin());
            while (!verify.empty() && verify.front() == L' ') verify.erase(verify.begin());
            if (name.empty()) {
                MessageBox(hwnd, L"Give the plugin a name first.", L"Add plugin", MB_OK | MB_ICONWARNING);
                break;
            }
            if (url.empty() || url.find(L"http://") != 0 && url.find(L"https://") != 0) {
                MessageBox(hwnd, L"The download URL must be an http:// or https:// address.", L"Add plugin", MB_OK | MB_ICONWARNING);
                break;
            }
            if (verify.empty()) verify = L"bin\\" + name + L"\\" + name + L".exe";
            for (auto& ch : verify) if (ch == L'/') ch = L'\\';
            AddCustomComponent(name, url, verify);
            RefreshPluginGrid();
            SetWindowTextString(g_state.hwndTxtPluginName, L"");
            SetWindowTextString(g_state.hwndTxtPluginUrl, L"");
            SetWindowTextString(g_state.hwndTxtPluginVerify, L"");
            SetStatus(L"Plugin added. Highlight it and press Install Selected.");
            break;
        }
        case ID_BTN_EXTRACT_ALL: DoExtractFlow(); break;
        case ID_BTN_EXTRACT_SEL: {
            HWND h = GetItem(hwnd, ID_FILE_LIST);
            int sel = ListView_GetNextItem(h, -1, LVNI_SELECTED);
            if (sel < 0 || g_state.currentArchive.empty()) {
                MessageBox(hwnd, L"Highlight a file in the list first.", L"Star Extract", MB_OK | MB_ICONINFORMATION);
                break;
            }
            DoExtractFlow();
            break;
        }
        case ID_BTN_PREVIEW: DoPreviewSelected(); break;
        case ID_BTN_TEST2: DoTestCurrent(); ShowPage(Page::Tools); break;
        case ID_BTN_DELETE2: {
            HWND h = GetItem(hwnd, ID_FILE_LIST);
            if (g_state.currentArchive.empty()) break;
            // collect every selected row so multi-select actually deletes them all
            std::vector<std::wstring> victims;
            for (int i = 0; ; i++) {
                int sel = ListView_GetNextItem(h, i - 1, LVNI_SELECTED);
                if (sel == -1) break;
                std::wstring path, name;
                if (GetRowPath(h, sel, path, name) && path != L"..") victims.push_back(path);
            }
            if (victims.empty()) break;
            std::wstring q = victims.size() == 1
                ? (L"Remove '" + victims[0] + L"' from the archive?")
                : (L"Remove " + std::to_wstring(victims.size()) + L" entries from the archive?");
            if (MessageBox(hwnd, q.c_str(), L"Confirm", MB_YESNO | MB_ICONQUESTION) != IDYES) break;
            std::wstring args = L"d \"" + g_state.currentArchive + L"\"";
            for (auto& v : victims) args += L" \"" + v + L"\"";
            RunProcess(Get7zPath(), args);
            g_state.archiveEntries = ListArchiveEntries(g_state.currentArchive);
            g_state.archiveTotalSize = TotalPackedSize(g_state.archiveEntries);
            RefreshBrowserList();
            SetStatus(victims.size() == 1 ? L"Entry removed." : L"Entries removed.");
            break;
        }
        case ID_BTN_SRC_BROWSE: {
            OPENFILENAME ofn = { 0 };
            wchar_t file[MAX_PATH] = L"";
            ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hwnd;
            ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH;
            ofn.lpstrFilter = L"All supported\0*.7z;*.zip;*.rar;*.tar;*.gz;*.cab;*.iso;*.msi;*.exe\0All files\0*.*\0";
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            if (GetOpenFileName(&ofn)) {
                SetWindowTextString(GetItem(hwnd, ID_TXT_SRC), file);
                // prefill dest + engine
                std::wstring p = file;
                size_t s = p.find_last_of(L"\\/");
                std::wstring dir = (s == std::wstring::npos) ? L"." : p.substr(0, s);
                std::wstring nm = (s == std::wstring::npos) ? p : p.substr(s + 1);
                size_t d = nm.find_last_of(L'.');
                if (d != std::wstring::npos) nm = nm.substr(0, d);
                SetWindowTextString(GetItem(hwnd, ID_TXT_DEST), dir + L"\\" + nm);
                SetWindowTextString(GetItem(hwnd, ID_ENGINE_LABEL), DetectEngine(p).c_str());
                g_state.currentArchive = p;
                g_state.currentFolder.clear();
                g_state.archiveEntries = ListArchiveEntries(p);
                RefreshBrowserList();
                SetWindowTextString(g_engineBadge, (L"Engine: " + DetectEngine(p)).c_str());
            }
            break;
        }
        case ID_BTN_DEST_BROWSE: {
            std::wstring d = BrowseFolder(hwnd, L"Where should the files go?");
            if (!d.empty()) SetWindowTextString(GetItem(hwnd, ID_TXT_DEST), d.c_str());
            break;
        }
        case ID_BTN_GO: DoExtractFlow(); break;
        case ID_BTN_CLEAR_LOG: SetWindowTextString(g_state.hwndLog, L""); break;
        case ID_BTN_SAVE_LOG: {
            OPENFILENAME ofn = { 0 };
            wchar_t file[MAX_PATH] = L"star-extract-log.txt";
            ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hwnd;
            ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH;
            ofn.lpstrFilter = L"Text files\0*.txt\0";
            ofn.Flags = OFN_OVERWRITEPROMPT;
            if (GetSaveFileName(&ofn)) {
                std::wstring log = GetWindowTextString(g_state.hwndLog);
                HANDLE h = CreateFile(file, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (h != INVALID_HANDLE_VALUE) {
                    DWORD w = 0;
                    WriteFile(h, log.c_str(), (DWORD)(log.size() * sizeof(wchar_t)), &w, nullptr);
                    CloseHandle(h);
                }
            }
            break;
        }
        case ID_BTN_COMP_ADD: BrowseCompAdd(); break;
        case ID_BTN_COMP_REMOVE: {
            HWND h = GetItem(hwnd, ID_COMP_LIST);
            int sel = ListView_GetNextItem(h, -1, LVNI_SELECTED);
            if (sel >= 0 && sel < (int)g_compFiles.size()) {
                g_compFiles.erase(g_compFiles.begin() + sel);
                ListView_DeleteItem(h, sel);
            }
            break;
        }
        case ID_BTN_COMP_CLEAR:
            g_compFiles.clear();
            ListView_DeleteAllItems(GetItem(hwnd, ID_COMP_LIST));
            SetWindowTextString(g_compNote, L"");
            break;
        case ID_BTN_COMP_OUT_BROWSE: {
            bool useZip = SendMessage(GetItem(hwnd, ID_RAD_FMT_ZIP), BM_GETCHECK, 0, 0) == BST_CHECKED;
            OPENFILENAME ofn = { 0 };
            wchar_t file[MAX_PATH] = L"Combined";
            wcscat(file, useZip ? L".zip" : L".7z");
            ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hwnd;
            ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH;
            ofn.lpstrFilter = useZip ? L"ZIP archive\0*.zip\0" : L"7z archive\0*.7z\0";
            ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
            if (GetSaveFileName(&ofn)) SetWindowTextString(GetItem(hwnd, ID_TXT_COMP_OUT), file);
            break;
        }
        case ID_BTN_COMP_GO: DoCompressGo(); break;
        case ID_BTN_TOOL_REFRESH: RefreshToolGrid(); RefreshPluginGrid(); SetStatus(L"Tool list refreshed."); break;
        case ID_BTN_HASH: DoHashFlow(); break;
        case ID_BTN_IDENTIFY: DoIdentifyFlow(); break;
        case ID_BTN_TEST2 + 1000: DoTestCurrent(); break;
        case ID_BTN_ADD_PASSWORD: {
            std::wstring p = GetWindowTextString(GetItem(hwnd, ID_TXT_NEW_PASSWORD));
            if (!p.empty()) {
                AddPassword(p);
                RefreshVaultList();
                SetWindowTextString(GetItem(hwnd, ID_TXT_NEW_PASSWORD), L"");
                SetStatus(L"Password saved (encrypted).");
            }
            break;
        }
        case ID_BTN_REMOVE_PASSWORD: {
            HWND h = GetItem(hwnd, ID_VAULT_LIST);
            int sel = ListView_GetNextItem(h, -1, LVNI_SELECTED);
            if (sel >= 0) { RemovePassword(sel); RefreshVaultList(); }
            break;
        }
        case ID_BTN_CLEAR_PASSWORDS:
            if (MessageBox(hwnd, L"Forget every saved password?", L"Star Extract", MB_YESNO | MB_ICONQUESTION) == IDYES) {
                ClearPasswords(); RefreshVaultList();
            }
            break;
        case ID_BTN_REGISTER_MENU:
            RegisterContextMenu();
            MessageBox(hwnd, L"Right-click menu registered for all files and folders.", L"Star Extract", MB_OK | MB_ICONINFORMATION);
            break;
        case ID_BTN_UNREGISTER_MENU:
            UnregisterContextMenu();
            MessageBox(hwnd, L"Right-click menu removed.", L"Star Extract", MB_OK | MB_ICONINFORMATION);
            break;
        case ID_RAD_DBL_EXTRACT:
        case ID_RAD_DBL_OPEN: {
            g_state.doubleExtract = (id == ID_RAD_DBL_EXTRACT);
            SaveUiSettings();
            if (RegisterShellAssociation(g_state.doubleExtract)) {
                SetStatus(g_state.doubleExtract
                    ? L"Explorer: double-click now quick-extracts archives."
                    : L"Explorer: double-click now opens archives in Star Extract.");
                LogMessage(std::wstring(L"Double-click association: ") +
                    (g_state.doubleExtract ? L"quick extract" : L"open in Star Extract"));
            } else {
                SetStatus(L"Could not register the double-click association.");
            }
            break;
        }
        case ID_BTN_UNREG_SHELL:
            UnregisterShellAssociation();
            SendMessage(GetItem(hwnd, ID_RAD_DBL_EXTRACT), BM_SETCHECK, BST_UNCHECKED, 0);
            SendMessage(GetItem(hwnd, ID_RAD_DBL_OPEN), BM_SETCHECK, BST_UNCHECKED, 0);
            SetStatus(L"Double-click behavior removed - archive files use their previous default app.");
            break;
        }
        return 0;
    }
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lp;
        int id = (int)dis->CtlID;
        if (id >= ID_NAV_HOME && id <= ID_NAV_SETTINGS) {
            DrawNavButton(dis, (id - ID_NAV_HOME) == (int)g_state.page);
            return TRUE;
        }
        DrawBigButton(dis);
        return TRUE;
    }
    case WM_NOTIFY: {
        LPNMHDR nm = (LPNMHDR)lp;
        if (nm->code == NM_DBLCLK && nm->idFrom == ID_FILE_LIST) { BrowserOpenSelected(); return 0; }
        if (nm->code == LVN_ITEMCHANGED && nm->idFrom == ID_FILE_LIST) {
            LPNMLISTVIEW lv = (LPNMLISTVIEW)lp;
            if ((lv->uChanged & LVIF_STATE) &&
                (((lv->uOldState & LVIS_SELECTED) != (lv->uNewState & LVIS_SELECTED))))
                UpdateBrowserStatus();
            return 0;
        }
        if (nm->code == NM_DBLCLK && nm->idFrom == ID_TREE_LIST) {
            LVITEM it = { 0 }; it.mask = LVIF_PARAM | LVIF_TEXT; it.iItem = ListView_GetNextItem(nm->hwndFrom, -1, LVNI_SELECTED);
            if (it.iItem >= 0) {
                ListView_GetItem(nm->hwndFrom, &it);
                std::wstring* path = (std::wstring*)it.lParam;
                if (path) {
                    NavigateBrowser(*path, true);
                    delete path;
                }
            }
            return 0;
        }
        if (nm->code == LVN_ITEMCHANGED && nm->idFrom == ID_PLUGIN_GRID) {
            LPNMLISTVIEW lv = (LPNMLISTVIEW)lp;
            if ((lv->uChanged & LVIF_STATE) &&
                (((lv->uOldState & LVIS_SELECTED) != (lv->uNewState & LVIS_SELECTED))))
                ShowSelectedComponentUrl();
            return 0;
        }
        if (nm->code == NM_DBLCLK && nm->idFrom == ID_PLUGIN_GRID) {
            int sel = SelectedPluginIndex();
            if (sel >= 0) {
                g_queue.clear();
                g_queue.push_back(sel);
                StartNextComponent();
            }
            return 0;
        }
        if (nm->code == NM_CUSTOMDRAW &&
            (nm->idFrom == ID_FILE_LIST || nm->idFrom == ID_TOOL_GRID || nm->idFrom == ID_PLUGIN_GRID || nm->idFrom == ID_TREE_LIST ||
             nm->idFrom == ID_VAULT_LIST || nm->idFrom == ID_COMP_LIST)) {
            LPNMLVCUSTOMDRAW cd = (LPNMLVCUSTOMDRAW)lp;
            if (cd->nmcd.dwDrawStage == CDDS_PREPAINT) return CDRF_NOTIFYITEMDRAW;
            if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT) {
                bool alt = (cd->nmcd.dwItemSpec % 2) == 1;
                cd->clrText = g_state.colors.textPrimary;
                cd->clrTextBk = alt ? MixColor(g_state.colors.bgSurface, g_state.colors.bgWindow, 0.55)
                                    : g_state.colors.bgSurface;
                return CDRF_NEWFONT;
            }
        }
        break;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        HWND ctl = (HWND)lp;
        SetBkMode(hdc, TRANSPARENT);
        COLORREF col = g_state.colors.textPrimary;
        if (ctl == g_engineBadge || ctl == GetItem(hwnd, ID_ENGINE_LABEL)) col = g_state.colors.accent;
        else if (ctl == g_heroSub || ctl == g_stepsLabel || ctl == g_browserHint ||
                 ctl == g_extractHint || ctl == g_compHint || ctl == g_compNote ||
                 ctl == g_lblSetNote) col = g_state.colors.textSecondary;
        else if (ctl == g_heroTitle) col = g_state.colors.textPrimary;
        SetTextColor(hdc, col);
        return (LRESULT)GetStockObject(NULL_BRUSH);
    }
    case WM_CTLCOLORBTN: {
        HDC hdc = (HDC)wp;
        HWND ctl = (HWND)lp;
        wchar_t cls[32] = L"";
        if (ctl) GetClassName(ctl, cls, 32);
        if (!wcscmp(cls, L"BUTTON")) {
            DWORD st = (DWORD)GetWindowLongPtr(ctl, GWL_STYLE);
            DWORD type = st & BS_TYPEMASK;
            if (type != BS_PUSHBUTTON && type != BS_DEFPUSHBUTTON) {
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, g_state.colors.textPrimary);
                return (LRESULT)GetStockObject(NULL_BRUSH);
            }
        }
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, g_state.colors.textPrimary);
        return (LRESULT)GetStockObject(NULL_BRUSH);
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = (HDC)wp;
        SetBkColor(hdc, g_state.colors.bgSurface);
        SetTextColor(hdc, ((HWND)lp == g_state.hwndLog) ? g_state.colors.logText : g_state.colors.textPrimary);
        return (LRESULT)g_hbrSurface;
    }
    case WM_KEYDOWN:
        if (wp == VK_F1) {
            MessageBox(hwnd,
                L"Star Extract - quick help\n\nHome: pick what to do.\nBrowse: look inside first.\nExtract: unpack step by step.\nCompress: pack files up.\nTools: test, hash, identify.\n\nF1 help  |  F2 theme  |  Ctrl + / - / 0 zoom",
                L"Help", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        if (wp == VK_F2) { ApplyTheme((Theme)(((int)g_state.currentTheme + 1) % (int)Theme::Count)); return 0; }
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            if (wp == VK_OEM_PLUS || wp == VK_ADD) { ApplyZoom(g_state.ui.zoom + AppState::ZOOM_STEP); return 0; }
            if (wp == VK_OEM_MINUS || wp == VK_SUBTRACT) { ApplyZoom(g_state.ui.zoom - AppState::ZOOM_STEP); return 0; }
            if (wp == '0' || wp == VK_NUMPAD0) { ApplyZoom(1.0); return 0; }
        }
        break;
    case WM_MOUSEWHEEL:
        if (GetKeyState(VK_CONTROL) & 0x8000) {
            if (GET_WHEEL_DELTA_WPARAM(wp) > 0) ApplyZoom(g_state.ui.zoom + AppState::ZOOM_STEP);
            else ApplyZoom(g_state.ui.zoom - AppState::ZOOM_STEP);
            return 0;
        }
        break;
    case WM_DESTROY:
        SaveUiSettings();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// ---------------- entry ----------------
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int nCmdShow) {
    (void)nCmdShow;
    if (HandleCLI()) return 0;

    LoadUiSettings();
    {
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv) {
            for (int i = 1; i < argc; i++) {
                if (wcsncmp(argv[i], L"--theme=", 8) == 0)
                    g_state.currentTheme = ThemeFromName(argv[i] + 8);
            }
            LocalFree(argv);
        }
    }

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS | ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);
    RegisterCustomControls(hInst);

    WNDCLASSEX wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 35));
    wc.lpszClassName = L"StarExtractNative";
    wc.hIcon = (HICON)LoadImageW(hInst, MAKEINTRESOURCEW(IDR_APP_ICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
    if (!wc.hIcon) wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    RegisterClassEx(&wc);

    RECT wa = { 0, 0, 0, 0 };
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    int waW = wa.right - wa.left, waH = wa.bottom - wa.top;
    int wwin = (1240 < waW) ? 1240 : waW;
    int hwin = (820 < waH) ? 820 : waH;
    int wx = wa.left + (waW - wwin) / 2;
    int wy = wa.top + (waH - hwin) / 2;
    HWND hwnd = CreateWindowEx(WS_EX_ACCEPTFILES, L"StarExtractNative", GetAppVersionString().c_str(),
        WS_OVERLAPPEDWINDOW, wx, wy, wwin, hwin,
        nullptr, nullptr, hInst, nullptr);
    if (!hwnd) return 1;
    g_state.hwndMain = hwnd;
    UpdateTitleBar();
    DragAcceptFiles(hwnd, TRUE);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    DeleteObject(g_hbrWindow);
    DeleteObject(g_hbrSurface);
    return (int)msg.wParam;
}

