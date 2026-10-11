// StarExtract Native - custom controls: CardButton, Toggle, tooltips, drawing
#include "se.h"

AppState g_state;
COLORREF g_customAccent = RGB(245, 158, 11);

COLORREF MixColor(COLORREF a, COLORREF b, double t) {
    if (t < 0) t = 0; if (t > 1) t = 1;
    int r = (int)(GetRValue(a) * (1 - t) + GetRValue(b) * t + 0.5);
    int g = (int)(GetGValue(a) * (1 - t) + GetGValue(b) * t + 0.5);
    int bl = (int)(GetBValue(a) * (1 - t) + GetBValue(b) * t + 0.5);
    return RGB(r, g, bl);
}

void FillGradient(HDC hdc, const RECT& rc, COLORREF top, COLORREF bottom) {
    TRIVERTEX v[2];
    v[0].x = rc.left; v[0].y = rc.top; v[0].Red = (COLOR16)(GetRValue(top) << 8);
    v[0].Green = (COLOR16)(GetGValue(top) << 8);
    v[0].Blue = (COLOR16)(GetBValue(top) << 8);
    v[0].Alpha = 0;
    v[1].x = rc.right; v[1].y = rc.bottom; v[1].Red = (COLOR16)(GetRValue(bottom) << 8);
    v[1].Green = (COLOR16)(GetGValue(bottom) << 8);
    v[1].Blue = (COLOR16)(GetBValue(bottom) << 8);
    v[1].Alpha = 0;
    GRADIENT_RECT gr = { 0, 1 };
    GradientFill(hdc, v, 2, &gr, 1, GRADIENT_FILL_RECT_V);
}

ThemeColors GetThemeColors(Theme t) {
    switch (t) {
    case Theme::Midnight:
        return { RGB(24,24,38),RGB(34,34,54),RGB(48,48,74),RGB(20,20,32),
            RGB(129,140,248),RGB(152,162,255),RGB(56,58,112),
            RGB(237,237,246),RGB(178,178,204),RGB(116,116,148),
            RGB(58,58,90),RGB(76,175,80),RGB(255,193,7),RGB(244,96,106),
            RGB(32,32,60),RGB(99,102,241),RGB(20,20,34),RGB(167,243,208),
            RGB(30,30,48),RGB(42,42,66) };
    case Theme::Ocean:
        return { RGB(16,32,38),RGB(24,46,54),RGB(34,64,74),RGB(12,26,32),
            RGB(45,212,191),RGB(64,224,205),RGB(20,80,76),
            RGB(236,246,246),RGB(172,200,200),RGB(112,146,148),
            RGB(42,76,86),RGB(52,211,153),RGB(251,191,36),RGB(248,113,113),
            RGB(20,44,52),RGB(13,148,136),RGB(12,26,32),RGB(94,234,212),
            RGB(22,42,50),RGB(30,56,66) };
    case Theme::Rose:
        return { RGB(36,24,32),RGB(50,32,44),RGB(68,44,60),RGB(28,18,26),
            RGB(232,121,249),RGB(242,146,255),RGB(90,46,100),
            RGB(247,239,247),RGB(204,180,202),RGB(142,116,140),
            RGB(80,52,72),RGB(118,206,132),RGB(244,180,80),RGB(244,96,120),
            RGB(46,30,50),RGB(192,80,210),RGB(30,20,28),RGB(167,243,208),
            RGB(44,30,40),RGB(64,42,58) };
    case Theme::Solarized:
        return { RGB(0,43,54),RGB(7,54,66),RGB(17,68,80),RGB(0,36,45),
            RGB(38,139,210),RGB(60,158,230),RGB(20,74,100),
            RGB(238,232,213),RGB(147,161,161),RGB(101,123,131),
            RGB(22,72,86),RGB(133,153,0),RGB(181,137,0),RGB(220,50,47),
            RGB(7,54,66),RGB(42,161,152),RGB(0,36,45),RGB(133,153,0),
            RGB(4,50,62),RGB(13,64,77) };
    case Theme::Custom:
        return { RGB(30,30,35),RGB(40,40,48),RGB(55,55,65),RGB(25,25,30),
            g_customAccent, MixColor(g_customAccent, RGB(255,255,255), 0.22),
            MixColor(g_customAccent, RGB(40,40,48), 0.62),
            RGB(240,240,240),RGB(180,180,190),RGB(120,120,130),
            RGB(60,60,70),RGB(76,175,80),RGB(255,193,7),RGB(244,67,54),
            RGB(25,35,50),MixColor(g_customAccent, RGB(25,35,50), 0.55),
            RGB(25,25,30),RGB(120,220,130),
            RGB(36,36,44),MixColor(RGB(46,46,56), g_customAccent, 0.10) };
    case Theme::Light:
        return { RGB(245,245,248),RGB(255,255,255),RGB(228,228,236),RGB(214,214,224),
            RGB(0,100,200),RGB(0,120,220),RGB(220,236,252),
            RGB(30,30,35),RGB(80,80,90),RGB(140,140,150),
            RGB(200,200,210),RGB(46,125,50),RGB(180,120,0),RGB(200,40,40),
            RGB(235,244,255),RGB(0,100,200),RGB(255,255,255),RGB(20,60,20),
            RGB(232,232,238),RGB(255,255,255) };
    case Theme::HighContrast:
        return { RGB(0,0,0),RGB(0,0,0),RGB(48,48,48),RGB(24,24,24),
            RGB(255,255,0),RGB(255,255,0),RGB(64,64,0),
            RGB(255,255,255),RGB(255,255,255),RGB(210,210,210),
            RGB(255,255,0),RGB(0,255,0),RGB(255,255,0),RGB(255,60,60),
            RGB(0,0,48),RGB(255,255,0),RGB(0,0,0),RGB(255,255,255),
            RGB(0,0,0),RGB(16,16,16) };
    case Theme::Dark:
    default:
        return { RGB(30,30,35),RGB(40,40,48),RGB(55,55,65),RGB(25,25,30),
            RGB(0,120,215),RGB(0,140,235),RGB(24,52,80),
            RGB(240,240,240),RGB(180,180,190),RGB(120,120,130),
            RGB(60,60,70),RGB(76,175,80),RGB(255,193,7),RGB(244,67,54),
            RGB(25,35,50),RGB(0,100,180),RGB(25,25,30),RGB(120,220,130),
            RGB(36,36,44),RGB(46,46,56) };
    }
}
const wchar_t* GetThemeName(Theme t) {
    switch (t) {
    case Theme::Light: return L"Light";
    case Theme::HighContrast: return L"High Contrast";
    case Theme::Midnight: return L"Midnight";
    case Theme::Ocean: return L"Ocean";
    case Theme::Rose: return L"Rose";
    case Theme::Solarized: return L"Solarized";
    case Theme::Custom: return L"Custom";
    case Theme::Dark: default: return L"Dark";
    }
}

const wchar_t* GetThemeDesc(Theme t) {
    switch (t) {
    case Theme::Dark: return L"Classic graphite";
    case Theme::Light: return L"Clean daylight";
    case Theme::HighContrast: return L"Maximum visibility";
    case Theme::Midnight: return L"Indigo after hours";
    case Theme::Ocean: return L"Calm teal depths";
    case Theme::Rose: return L"Soft plum bloom";
    case Theme::Solarized: return L"Low glare solarized";
    case Theme::Custom: default: return L"Your accent color";
    }
}

Theme ThemeFromName(const wchar_t* name) {
    if (!name) return Theme::Dark;
    if (lstrcmpiW(name, L"light") == 0) return Theme::Light;
    if (lstrcmpiW(name, L"highcontrast") == 0 || lstrcmpiW(name, L"hc") == 0) return Theme::HighContrast;
    if (lstrcmpiW(name, L"midnight") == 0) return Theme::Midnight;
    if (lstrcmpiW(name, L"ocean") == 0) return Theme::Ocean;
    if (lstrcmpiW(name, L"rose") == 0) return Theme::Rose;
    if (lstrcmpiW(name, L"solarized") == 0) return Theme::Solarized;
    if (lstrcmpiW(name, L"custom") == 0) return Theme::Custom;
    return Theme::Dark;
}

void UIScale::Apply(double z) {
    zoom = z;
    btnH = (int)(56 * z); btnW = (int)(180 * z);
    navW = (int)(196 * z); statusH = (int)(40 * z); tipH = (int)(190 * z);
    margin = (int)(16 * z);
    cardW = (int)(252 * z); cardH = (int)(176 * z);
    fontUI = (int)(16 * z); fontBig = (int)(20 * z);
    fontTip = (int)(14 * z); fontHero = (int)(30 * z);
}

HFONT MakeFont(int px, bool bold) {
    return CreateFont(px, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

std::wstring FitText(HDC hdc, const std::wstring& s, int maxPx) {
    if (s.empty()) return s;
    SIZE sz;
    GetTextExtentPoint32(hdc, s.c_str(), (int)s.size(), &sz);
    if (sz.cx <= maxPx) return s;
    std::wstring out;
    for (size_t i = 0; i < s.size(); i++) {
        std::wstring t = out + s[i] + L"...";
        GetTextExtentPoint32(hdc, t.c_str(), (int)t.size(), &sz);
        if (sz.cx > maxPx) break;
        out += s[i];
    }
    return out + L"...";
}

void DrawRoundRect(HDC hdc, const RECT& rc, int radius, HBRUSH fill, HPEN pen) {
    HBRUSH oldB = (HBRUSH)SelectObject(hdc, fill ? fill : (HBRUSH)GetStockObject(NULL_BRUSH));
    HPEN oldP = (HPEN)SelectObject(hdc, pen ? pen : (HPEN)GetStockObject(NULL_PEN));
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(hdc, oldB); SelectObject(hdc, oldP);
}

// Simple geometric outline icons drawn with GDI (always render, no font needed)
void DrawIconGlyph(HDC hdc, RECT rc, int kind, COLORREF color) {
    HPEN pen = CreatePen(PS_SOLID, 3, color);
    HPEN oldP = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldB = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    int cx = (rc.left + rc.right) / 2, cy = (rc.top + rc.bottom) / 2;
    int s = min(rc.right - rc.left, rc.bottom - rc.top) / 2 - 4;
    switch (kind) {
    case ICON_FOLDER: // folder: tab + body
        MoveToEx(hdc, cx - s, cy - s / 2, nullptr); LineTo(hdc, cx - s / 3, cy - s / 2);
        LineTo(hdc, cx - s / 6, cy - s / 6); LineTo(hdc, cx + s, cy - s / 6);
        LineTo(hdc, cx + s, cy + s); LineTo(hdc, cx - s, cy + s); LineTo(hdc, cx - s, cy - s / 2);
        break;
    case ICON_DOWN: // download into tray
        MoveToEx(hdc, cx, cy - s, nullptr); LineTo(hdc, cx, cy + s / 3);
        MoveToEx(hdc, cx - s / 2, cy - s / 6, nullptr); LineTo(hdc, cx, cy + s / 3); LineTo(hdc, cx + s / 2, cy - s / 6);
        MoveToEx(hdc, cx - s, cy + s / 2, nullptr); LineTo(hdc, cx - s, cy + s);
        LineTo(hdc, cx + s, cy + s); LineTo(hdc, cx + s, cy + s / 2);
        break;
    case ICON_BOX: // box with plus (compress)
        Rectangle(hdc, cx - s, cy - s * 2 / 3, cx + s, cy + s * 2 / 3);
        MoveToEx(hdc, cx - s, cy - s / 6, nullptr); LineTo(hdc, cx + s, cy - s / 6);
        MoveToEx(hdc, cx, cy - s * 2 / 3, nullptr); LineTo(hdc, cx, cy - s / 6);
        break;
    case ICON_GEAR: {
        Ellipse(hdc, cx - s / 2, cy - s / 2, cx + s / 2, cy + s / 2);
        for (int i = 0; i < 8; i++) {
            double a = i * 3.14159 / 4.0;
            MoveToEx(hdc, (int)(cx + cos(a) * s * 0.62), (int)(cy + sin(a) * s * 0.62), nullptr);
            LineTo(hdc, (int)(cx + cos(a) * s), (int)(cy + sin(a) * s));
        }
        break;
    }
    case ICON_CHECK:
        MoveToEx(hdc, cx - s, cy, nullptr); LineTo(hdc, cx - s / 4, cy + s / 2); LineTo(hdc, cx + s, cy - s / 2);
        break;
    case ICON_MAG: // magnifier
        Ellipse(hdc, cx - s, cy - s, cx + s / 3, cy + s / 3);
        MoveToEx(hdc, cx + s / 6, cy + s / 6, nullptr); LineTo(hdc, cx + s, cy + s);
        break;
    case ICON_TAG:
        MoveToEx(hdc, cx - s / 2, cy - s, nullptr); LineTo(hdc, cx + s / 2, cy - s);
        LineTo(hdc, cx + s, cy - s / 2); LineTo(hdc, cx + s / 3, cy + s);
        LineTo(hdc, cx - s / 3, cy + s); LineTo(hdc, cx - s, cy - s / 2); LineTo(hdc, cx - s / 2, cy - s);
        break;
    case ICON_BOLT:
        MoveToEx(hdc, cx + s / 3, cy - s, nullptr); LineTo(hdc, cx - s / 3, cy + s / 6);
        LineTo(hdc, cx + s / 6, cy + s / 6); LineTo(hdc, cx - s / 3, cy + s);
        break;
    }
    SelectObject(hdc, oldB); SelectObject(hdc, oldP);
    DeleteObject(pen);
}

// ---------------- CardButton: big rounded action card ----------------
struct CardData { std::wstring title, sub; int icon; bool* selected; Theme preview; };
static LRESULT CALLBACK CardProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    CardData* d = (CardData*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_NCCREATE: {
        CREATESTRUCT* cs = (CREATESTRUCT*)lp;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return TRUE;
    }
    case WM_ERASEBKGND: return 1;
    case WM_SETCURSOR:
        SetCursor(LoadCursor(nullptr, IDC_HAND));
        return TRUE;
    case WM_PAINT: {
        if (!d) break;
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        bool hot = false, down = false;
        POINT pt; GetCursorPos(&pt); ScreenToClient(hwnd, &pt);
        hot = PtInRect(&rc, pt);
        down = hot && (GetKeyState(VK_LBUTTON) & 0x8000) && GetCapture() == hwnd;
        bool sel = d->selected && *d->selected;
        bool previewing = d->preview < Theme::Count;
        ThemeColors col = GetThemeColors(previewing ? d->preview : g_state.currentTheme);
        SetBkMode(hdc, TRANSPARENT);
        SetGraphicsMode(hdc, GM_ADVANCED);

        // elevated body: shrink to leave room for a soft drop shadow
        RECT card = { rc.left, rc.top, rc.right - 4, rc.bottom - 5 };
        RECT shadow = { card.left + 2, card.top + 4, card.right + 4, card.bottom + 5 };
        HBRUSH sb = CreateSolidBrush(MixColor(col.bgWindow, RGB(0, 0, 0), previewing ? 0.35 : 0.28));
        DrawRoundRect(hdc, shadow, 18, sb, nullptr);
        DeleteObject(sb);

        // vertical gradient body, clipped to the rounded rect
        COLORREF top, bot;
        if (down) { top = col.bgPressed; bot = MixColor(col.bgPressed, RGB(0, 0, 0), 0.16); }
        else if (hot) { top = col.bgHover; bot = MixColor(col.bgHover, col.bgSurface, 0.55); }
        else { top = col.cardBg; bot = MixColor(col.cardBg, col.bgWindow, 0.55); }
        HRGN body = CreateRoundRectRgn(card.left, card.top, card.right, card.bottom, 18, 18);
        SelectClipRgn(hdc, body);
        FillGradient(hdc, card, top, bot);
        DeleteObject(body);
        SelectClipRgn(hdc, nullptr);

        // border + subtle top highlight
        HPEN pen = CreatePen(PS_SOLID, sel ? 2 : 1, sel ? col.accent : col.border);
        DrawRoundRect(hdc, card, 18, nullptr, pen);
        DeleteObject(pen);
        RECT hi = { card.left + 6, card.top + 1, card.right - 6, card.top + 2 };
        HBRUSH hb = CreateSolidBrush(MixColor(top, RGB(255, 255, 255), 0.14));
        FillRect(hdc, &hi, hb);
        DeleteObject(hb);

        // icon tile
        int tile = 64;
        int m = g_state.ui.margin;
        RECT tileRc = { card.left + m, card.top + (card.bottom - card.top - tile) / 2, card.left + m + tile, 0 };
        tileRc.bottom = tileRc.top + tile;
        HRGN trgn = CreateRoundRectRgn(tileRc.left, tileRc.top, tileRc.right, tileRc.bottom, 14, 14);
        SelectClipRgn(hdc, trgn);
        FillGradient(hdc, tileRc, MixColor(col.accentSoft, col.accent, 0.30), col.accentSoft);
        DeleteObject(trgn);
        SelectClipRgn(hdc, nullptr);
        HPEN tp = CreatePen(PS_SOLID, 1, MixColor(col.accent, col.bgWindow, 0.45));
        DrawRoundRect(hdc, tileRc, 14, nullptr, tp);
        DeleteObject(tp);
        RECT glyph = { tileRc.left + 12, tileRc.top + 12, tileRc.right - 12, tileRc.bottom - 12 };
        DrawIconGlyph(hdc, glyph, d->icon, col.accent);

        // texts
        HFONT oldF = (HFONT)SelectObject(hdc, g_state.hFontBig);
        SetTextColor(hdc, col.textPrimary);
        RECT tRc = { tileRc.right + m, card.top + m, card.right - m, card.top + m + 34 };
        DrawText(hdc, FitText(hdc, d->title, tRc.right - tRc.left).c_str(), -1, &tRc, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(hdc, g_state.hFontUI);
        SetTextColor(hdc, col.textSecondary);
        RECT sRc = { tileRc.right + m, tRc.bottom + 2, card.right - m, card.bottom - m };
        DrawText(hdc, d->sub.c_str(), -1, &sRc, DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);
        SelectObject(hdc, oldF);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE:
        InvalidateRect(hwnd, nullptr, FALSE);
        { TRACKMOUSEEVENT t = { sizeof(t), TME_LEAVE, hwnd, 0 }; TrackMouseEvent(&t); }
        break;
    case WM_MOUSELEAVE: InvalidateRect(hwnd, nullptr, FALSE); break;
    case WM_LBUTTONDOWN: SetCapture(hwnd); InvalidateRect(hwnd, nullptr, FALSE); break;
    case WM_LBUTTONUP: {
        bool was = GetCapture() == hwnd;
        ReleaseCapture(); InvalidateRect(hwnd, nullptr, FALSE);
        if (was) {
            RECT rc; GetClientRect(hwnd, &rc);
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            if (PtInRect(&rc, pt))
                SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(GetWindowLongPtr(hwnd, GWLP_ID), BN_CLICKED), (LPARAM)hwnd);
        }
        break;
    }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// ---------------- Toggle switch ----------------
static LRESULT CALLBACK ToggleProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    bool* on = (bool*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_NCCREATE: {
        CREATESTRUCT* cs = (CREATESTRUCT*)lp;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        return TRUE;
    }
    case WM_ERASEBKGND: return 1;
    case WM_SETCURSOR:
        SetCursor(LoadCursor(nullptr, IDC_HAND));
        return TRUE;
    case WM_PAINT: {
        if (!on) break;
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        int w = rc.right - rc.left, h = rc.bottom - rc.top;
        int knob = h - 8;
        RECT track = { 0, (h - knob + 4) / 2, w, (h + knob - 4) / 2 + 4 };
        SetGraphicsMode(hdc, GM_ADVANCED);
        COLORREF trackTop = *on ? MixColor(g_state.colors.accent, RGB(255, 255, 255), 0.14) : g_state.colors.bgHover;
        COLORREF trackBot = *on ? MixColor(g_state.colors.accent, RGB(0, 0, 0), 0.20) : MixColor(g_state.colors.bgHover, g_state.colors.bgWindow, 0.45);
        HRGN trgn = CreateRoundRectRgn(track.left, track.top, track.right, track.bottom, knob, knob);
        SelectClipRgn(hdc, trgn);
        FillGradient(hdc, track, trackTop, trackBot);
        DeleteObject(trgn);
        SelectClipRgn(hdc, nullptr);
        HPEN tpen = CreatePen(PS_SOLID, 1, *on ? MixColor(g_state.colors.accent, RGB(0, 0, 0), 0.30) : g_state.colors.border);
        DrawRoundRect(hdc, track, knob, nullptr, tpen);
        DeleteObject(tpen);
        int kx = *on ? (w - knob - 2) : 2;
        RECT kr = { kx, 2, kx + knob, 2 + knob };
        HRGN krgn = CreateRoundRectRgn(kr.left, kr.top, kr.right, kr.bottom, knob, knob);
        SelectClipRgn(hdc, krgn);
        FillGradient(hdc, kr, RGB(255, 255, 255), RGB(226, 226, 232));
        DeleteObject(krgn);
        SelectClipRgn(hdc, nullptr);
        HPEN kp = CreatePen(PS_SOLID, 1, MixColor(g_state.colors.border, RGB(0, 0, 0), 0.15));
        DrawRoundRect(hdc, kr, knob, nullptr, kp);
        DeleteObject(kp);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONUP: {
        if (on) {
            *on = !*on;
            InvalidateRect(hwnd, nullptr, FALSE);
            SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(GetWindowLongPtr(hwnd, GWLP_ID), BN_CLICKED), (LPARAM)hwnd);
        }
        break;
    }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

void RegisterCustomControls(HINSTANCE hInst) {
    WNDCLASS wc = {};
    wc.hInstance = hInst; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"SECard"; wc.lpfnWndProc = CardProc;
    RegisterClass(&wc);
    wc.lpszClassName = L"SEToggle"; wc.lpfnWndProc = ToggleProc;
    RegisterClass(&wc);
}

HWND CreateCard(HWND parent, int id, const wchar_t* title, const wchar_t* sub, int iconKind, bool* selectedFlag) {
    CardData* d = new CardData{};
    d->title = title; d->sub = sub; d->icon = iconKind;
    d->selected = selectedFlag; d->preview = Theme::Count;
    return CreateWindowEx(0, L"SECard", L"", WS_CHILD | WS_VISIBLE,
        0, 0, 100, 100, parent, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), d);
}
HWND CreateThemeCard(HWND parent, int id, Theme t, bool* selectedFlag) {
    CardData* d = new CardData{};
    d->title = GetThemeName(t); d->sub = GetThemeDesc(t); d->icon = ICON_TAG;
    d->selected = selectedFlag; d->preview = t;
    return CreateWindowEx(0, L"SECard", L"", WS_CHILD | WS_VISIBLE,
        0, 0, 100, 100, parent, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), d);
}
HWND CreateToggle(HWND parent, int id, bool initial) {
    bool* b = new bool(initial);
    return CreateWindowEx(0, L"SEToggle", L"", WS_CHILD | WS_VISIBLE,
        0, 0, 64, 32, parent, (HMENU)(INT_PTR)id, GetModuleHandle(nullptr), b);
}
bool Toggle_Get(HWND h) { bool* b = (bool*)GetWindowLongPtr(h, GWLP_USERDATA); return b ? *b : false; }
void Toggle_Set(HWND h, bool on) {
    bool* b = (bool*)GetWindowLongPtr(h, GWLP_USERDATA);
    if (b) { *b = on; InvalidateRect(h, nullptr, FALSE); }
}

// ---------------- tooltips ----------------
HWND CreateTooltip(HWND hwndMain) {
    HWND h = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, nullptr,
        WS_POPUP | TTS_NOPREFIX | TTS_BALLOON, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, hwndMain, nullptr, GetModuleHandle(nullptr), nullptr);
    SendMessage(h, TTM_SETMAXTIPWIDTH, 0, 360);
    SendMessage(h, TTM_SETDELAYTIME, TTDT_AUTOPOP, 15000);
    return h;
}
void AddTip(HWND hwndTips, HWND hwndTool, const wchar_t* text) {
    if (!hwndTips || !hwndTool || !text) return;
    TOOLINFO ti = { sizeof(ti) };
    ti.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    ti.hwnd = GetParent(hwndTool);
    ti.uId = (UINT_PTR)hwndTool;
    ti.lpszText = (LPWSTR)text;
    SendMessage(hwndTips, TTM_ADDTOOL, 0, (LPARAM)&ti);
}
