// ============================================================================
//  CloneX OS - Vibrant Desktop Simulator (C++20 / Win32)
//  Connected Modules: vim.cpp, browser.cpp, compiler.cpp, backendopt.cpp
// ============================================================================

#define UNICODE
#define _UNICODE
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwctype>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ============================================================================
//  EXTERNAL MODULES DECLARATIONS
// ============================================================================
LRESULT CALLBACK VimProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
void VimLoadFile(const fs::path& p);

LRESULT CALLBACK BrowserProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
void LaunchWebUrl(std::wstring input);

LRESULT CALLBACK CompilerProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

namespace BackendOpt {
    void OptimizeMemory();
    std::wstring GetSystemStatus(const fs::path& homePath);
}

// ============================================================================
//  THEME & GLOBALS
// ============================================================================
static const COLORREF C_ORANGE      = RGB(233, 84, 32);
static const COLORREF C_AUBERGINE   = RGB(48, 10, 36);
static const COLORREF C_PANEL       = RGB(22, 12, 20);
static const COLORREF C_DOCK        = RGB(36, 18, 32);
static const COLORREF C_WHITE       = RGB(255, 255, 255);
static const COLORREF C_WIN_BG      = RGB(250, 249, 248);
static const COLORREF C_SIDEBAR     = RGB(238, 235, 232);
static const COLORREF C_TEXT        = RGB(42, 42, 46);

HINSTANCE gInst = nullptr;
HWND hDesktop = nullptr, hTopBar = nullptr, hDock = nullptr, hLauncher = nullptr;
HWND hTerminal = nullptr, hFiles = nullptr, hEditor = nullptr;
HWND hVim = nullptr, hBrowser = nullptr, hCompiler = nullptr, hSettings = nullptr, hAbout = nullptr;

HFONT fUI = nullptr, fSmall = nullptr, fBold = nullptr, fTitle = nullptr, fMono = nullptr;
fs::path gHome;
static std::wstring gUser = L"clonex";
static const wchar_t* kHost = L"clonex-pc";
static const int DOCK_W = 74;

enum Cmd {
    IDM_FILES = 2001, IDM_TERMINAL, IDM_EDITOR, IDM_VIM, IDM_BROWSER,
    IDM_COMPILER, IDM_SETTINGS, IDM_ABOUT, IDM_POWEROFF, IDM_LAUNCHER
};

enum AppKind { APP_FILES = 0, APP_TERMINAL, APP_EDITOR, APP_VIM, APP_BROWSER, APP_COMPILER, APP_SETTINGS, APP_ABOUT };
static const int TOTAL_APPS = 8;

using Path = std::vector<std::wstring>;
void OpenFiles(const Path* start = nullptr);
void OpenTerminal();
void OpenEditor(const Path* file = nullptr);
void OpenVim(const fs::path* file = nullptr);
void OpenBrowser();
void OpenCompiler();
void OpenSettings();
void OpenAbout();
void ToggleLauncher(bool forceHide = false);

// ============================================================================
//  HELPERS & FILE I/O
// ============================================================================
static std::wstring Lower(std::wstring s) { for (auto& c : s) c = (wchar_t)std::towlower(c); return s; }
static std::wstring Pad2(int v) { return (v < 10 ? L"0" : L"") + std::to_wstring(v); }
static void SetFont(HWND h, HFONT f) { SendMessageW(h, WM_SETFONT, (WPARAM)f, TRUE); }

static std::wstring GetText(HWND h) {
    int len = GetWindowTextLengthW(h);
    std::wstring s((size_t)len + 1, L'\0');
    GetWindowTextW(h, s.data(), len + 1);
    s.resize((size_t)len);
    return s;
}

static HFONT MakeFont(int h, int w, const wchar_t* face) {
    return CreateFontW(-h, 0, 0, 0, w, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                       CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
}

static void RegClass(const wchar_t* name, WNDPROC proc, HBRUSH bg = nullptr) {
    WNDCLASSEXW wc{sizeof(wc), CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS, proc, 0, 0, gInst, nullptr,
                   LoadCursorW(nullptr, IDC_ARROW), bg, nullptr, name, nullptr};
    RegisterClassExW(&wc);
}

bool ReadTextFile(const fs::path& p, std::wstring& out) {
    FILE* f = _wfopen(p.wstring().c_str(), L"rb");
    if (!f) return false;
    std::string data; char buf[4096]; size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) data.append(buf, n);
    fclose(f);
    int wlen = MultiByteToWideChar(CP_UTF8, 0, data.data(), (int)data.size(), nullptr, 0);
    std::wstring w((size_t)wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, data.data(), (int)data.size(), w.data(), wlen);
    out.clear();
    for (wchar_t c : w) { if (c == L'\r') continue; out += (c == L'\n') ? L"\r\n" : std::wstring(1, c); }
    return true;
}

bool WriteTextFile(const fs::path& p, const std::wstring& text, bool append = false) {
    std::wstring lf;
    for (wchar_t c : text) if (c != L'\r') lf += c;
    FILE* f = _wfopen(p.wstring().c_str(), append ? L"ab" : L"wb");
    if (!f) return false;
    int len = WideCharToMultiByte(CP_UTF8, 0, lf.data(), (int)lf.size(), nullptr, 0, nullptr, nullptr);
    std::string u((size_t)len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, lf.data(), (int)lf.size(), u.data(), len, nullptr, nullptr);
    fwrite(u.data(), 1, u.size(), f);
    fclose(f);
    return true;
}

static fs::path ToReal(const Path& p) { fs::path r = gHome; for (auto& c : p) r /= c; return r; }
static std::wstring ToDisplay(const Path& p, bool tilde) {
    std::wstring s = tilde ? L"~" : L"/home/" + gUser;
    for (auto& c : p) s += L"/" + c;
    return s;
}

static bool Resolve(const Path& cwd, const std::wstring& arg, Path& out) {
    std::wstring a = arg;
    bool abs = (!a.empty() && (a[0] == L'/' || a[0] == L'\\' || a[0] == L'~'));
    if (!a.empty() && a[0] == L'~') a.erase(0, 1);
    out = abs ? Path{} : cwd;
    std::wstring cur;
    for (size_t i = 0; i <= a.size(); ++i) {
        if (i == a.size() || a[i] == L'/' || a[i] == L'\\') {
            if (cur == L"..") { if (!out.empty()) out.pop_back(); }
            else if (!cur.empty() && cur != L".") out.push_back(cur);
            cur.clear();
        } else cur += a[i];
    }
    return true;
}

static void InitSandbox() {
    wchar_t exe[MAX_PATH]{}; GetModuleFileNameW(nullptr, exe, MAX_PATH);
    gHome = fs::path(exe).parent_path() / L"CloneXHome";
    std::error_code ec;
    for (auto d : {L"Desktop", L"Documents", L"Downloads", L"Music", L"Pictures", L"Videos"})
        fs::create_directories(gHome / d, ec);
    fs::path readme = gHome / L"Documents" / L"Welcome.txt";
    if (!fs::exists(readme, ec))
        WriteTextFile(readme, L"Welcome to CloneX OS!\nApps: Files, Terminal, Editor, Vim, Browser, C++ IDE.\n");
}

// ============================================================================
//  CUSTOM GRAPHICS & VECTOR ICONS
// ============================================================================
struct DoubleBuf {
    HDC dc, mem; HBITMAP bmp, old; int w, h;
    DoubleBuf(HDC hdc, RECT r) : dc(hdc) {
        w = std::max<LONG>(1, r.right); h = std::max<LONG>(1, r.bottom);
        mem = CreateCompatibleDC(hdc); bmp = CreateCompatibleBitmap(hdc, w, h);
        old = (HBITMAP)SelectObject(mem, bmp);
    }
    ~DoubleBuf() { BitBlt(dc, 0, 0, w, h, mem, 0, 0, SRCCOPY); SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); }
};

static void FillRectC(HDC dc, RECT r, COLORREF c) { HBRUSH b = CreateSolidBrush(c); FillRect(dc, &r, b); DeleteObject(b); }

static void FillRound(HDC dc, RECT r, int rad, COLORREF fill, COLORREF border = CLR_INVALID) {
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = (border == CLR_INVALID) ? (HPEN)GetStockObject(NULL_PEN) : CreatePen(PS_SOLID, 1, border);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    RoundRect(dc, r.left, r.top, r.right, r.bottom, rad, rad);
    SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(b);
    if (border != CLR_INVALID) DeleteObject(p);
}

static void FillCircle(HDC dc, int cx, int cy, int rad, COLORREF fill, COLORREF border = CLR_INVALID) {
    HBRUSH b = CreateSolidBrush(fill);
    HPEN p = (border == CLR_INVALID) ? (HPEN)GetStockObject(NULL_PEN) : CreatePen(PS_SOLID, 2, border);
    HGDIOBJ ob = SelectObject(dc, b), op = SelectObject(dc, p);
    Ellipse(dc, cx - rad, cy - rad, cx + rad, cy + rad);
    SelectObject(dc, ob); SelectObject(dc, op); DeleteObject(b);
    if (border != CLR_INVALID) DeleteObject(p);
}

static void DrawLine(HDC dc, int x1, int y1, int x2, int y2, COLORREF c, int w = 1) {
    HPEN p = CreatePen(PS_SOLID, w, c); HGDIOBJ op = SelectObject(dc, p);
    MoveToEx(dc, x1, y1, nullptr); LineTo(dc, x2, y2);
    SelectObject(dc, op); DeleteObject(p);
}

static void TextIn(HDC dc, RECT r, const std::wstring& s, HFONT f, COLORREF c, UINT fl) {
    HGDIOBJ of = SelectObject(dc, f); SetBkMode(dc, TRANSPARENT); SetTextColor(dc, c);
    DrawTextW(dc, s.c_str(), -1, &r, fl); SelectObject(dc, of);
}

static void DrawAppIcon(HDC dc, int kind, RECT r) {
    int w = r.right - r.left, x = r.left, y = r.top;
    auto px = [&](double f) { return (int)std::lround(w * f); };

    switch (kind) {
    case APP_FILES:
        FillRound(dc, r, px(0.26), RGB(255, 245, 238));
        FillRound(dc, RECT{x + px(0.16), y + px(0.22), x + px(0.48), y + px(0.38)}, px(0.06), RGB(215, 95, 30));
        FillRound(dc, RECT{x + px(0.16), y + px(0.30), x + px(0.84), y + px(0.76)}, px(0.08), RGB(233, 84, 32));
        break;
    case APP_TERMINAL: {
        FillRound(dc, r, px(0.26), RGB(56, 16, 44), RGB(140, 65, 115));
        HFONT f = MakeFont(px(0.42), FW_BOLD, L"Consolas");
        TextIn(dc, r, L">_", f, RGB(120, 255, 130), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DeleteObject(f);
        break;
    }
    case APP_EDITOR:
        FillRound(dc, r, px(0.26), RGB(240, 244, 248));
        FillRound(dc, RECT{x + px(0.22), y + px(0.16), x + px(0.78), y + px(0.84)}, px(0.06), C_WHITE, RGB(180, 190, 205));
        break;
    case APP_VIM: {
        FillRound(dc, r, px(0.26), RGB(22, 115, 55), RGB(60, 210, 105));
        HFONT f = MakeFont(px(0.48), FW_BOLD, L"Consolas");
        TextIn(dc, r, L"V", f, RGB(235, 255, 235), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DeleteObject(f);
        break;
    }
    case APP_BROWSER:
        FillRound(dc, r, px(0.26), RGB(235, 245, 255));
        FillCircle(dc, x + w / 2, y + w / 2, px(0.34), RGB(33, 150, 243));
        break;
    case APP_COMPILER: {
        FillRound(dc, r, px(0.26), RGB(40, 53, 147), RGB(0, 188, 212));
        HFONT f = MakeFont(px(0.36), FW_BOLD, L"Consolas");
        TextIn(dc, r, L"C++", f, RGB(100, 255, 218), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        DeleteObject(f);
        break;
    }
    case APP_SETTINGS:
        FillRound(dc, r, px(0.26), RGB(75, 80, 95));
        FillCircle(dc, x + w / 2, y + w / 2, px(0.22), RGB(230, 235, 245));
        break;
    default:
        FillRound(dc, r, px(0.26), RGB(119, 33, 111));
        FillCircle(dc, x + w / 2, y + w / 2, px(0.30), C_WHITE);
        break;
    }
}

static const wchar_t* AppName(int k) {
    static const wchar_t* n[] = {L"Files", L"Terminal", L"Text Editor", L"Vim", L"Browser", L"C++ IDE", L"Settings", L"About"};
    return n[k];
}

// ============================================================================
//  MODAL INPUT BOX
// ============================================================================
struct InputState { std::wstring prompt, val; bool ok = false, done = false; HWND edit = nullptr; };
static LRESULT CALLBACK InputProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    auto* st = (InputState*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if (msg == WM_CREATE) {
        st = (InputState*)((CREATESTRUCTW*)lp)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)st);
        SetFont(CreateWindowW(L"STATIC", st->prompt.c_str(), WS_CHILD | WS_VISIBLE, 15, 12, 340, 20, hwnd, nullptr, gInst, nullptr), fUI);
        st->edit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", st->val.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 15, 38, 340, 26, hwnd, (HMENU)100, gInst, nullptr);
        SetFont(st->edit, fUI);
        CreateWindowW(L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, 180, 78, 80, 28, hwnd, (HMENU)IDCANCEL, gInst, nullptr);
        CreateWindowW(L"BUTTON", L"OK", WS_CHILD | WS_VISIBLE, 270, 78, 85, 28, hwnd, (HMENU)IDOK, gInst, nullptr);
    } else if (msg == WM_COMMAND) {
        if (LOWORD(wp) == IDOK && st) { st->val = GetText(st->edit); st->ok = true; DestroyWindow(hwnd); }
        else if (LOWORD(wp) == IDCANCEL) DestroyWindow(hwnd);
    } else if (msg == WM_DESTROY && st) st->done = true;
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static bool InputBox(HWND owner, const std::wstring& title, const std::wstring& prompt, std::wstring& val) {
    InputState st{prompt, val};
    HWND dlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"CloneXInput", title.c_str(),
                               WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, 400, 300, 380, 150, owner, nullptr, gInst, &st);
    if (!dlg) return false;
    EnableWindow(owner, FALSE); SetFocus(st.edit);
    MSG m;
    while (!st.done && GetMessageW(&m, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(dlg, &m)) { TranslateMessage(&m); DispatchMessageW(&m); }
    }
    EnableWindow(owner, TRUE); SetForegroundWindow(owner);
    if (st.ok) val = st.val;
    return st.ok;
}

// ============================================================================
//  TOP BAR & SYSTEM TRAY
// ============================================================================
static LRESULT CALLBACK TopBarProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_CREATE) SetTimer(hwnd, 1, 1000, nullptr);
    else if (msg == WM_TIMER) InvalidateRect(hwnd, nullptr, FALSE);
    else if (msg == WM_ERASEBKGND) return 1;
    else if (msg == WM_PAINT) {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps); RECT rc; GetClientRect(hwnd, &rc);
        {
            DoubleBuf b(hdc, rc);
            FillRectC(b.mem, rc, C_PANEL);

            FillRound(b.mem, RECT{8, 4, 104, 28}, 16, RGB(58, 36, 52));
            FillCircle(b.mem, 20, 16, 4, C_ORANGE);
            TextIn(b.mem, RECT{26, 4, 100, 28}, L"Activities", fBold, C_WHITE, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            static const wchar_t* mons[] = {L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun", L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec"};
            SYSTEMTIME t{}; GetLocalTime(&t);
            std::wstring clk = std::to_wstring(t.wDay) + L" " + mons[(t.wMonth - 1) % 12] + L"  " + Pad2(t.wHour) + L":" + Pad2(t.wMinute);
            TextIn(b.mem, rc, clk, fBold, C_WHITE, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            int rx = rc.right - 130;
            FillRound(b.mem, RECT{rx, 4, rc.right - 6, 28}, 16, RGB(52, 32, 46));
            FillCircle(b.mem, rx + 92, 16, 8, RGB(220, 60, 60));
            TextIn(b.mem, RECT{rx + 84, 8, rx + 100, 24}, L"⏻", fSmall, C_WHITE, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        EndPaint(hwnd, &ps);
    } else if (msg == WM_LBUTTONDOWN) {
        int x = GET_X_LPARAM(lp); RECT rc; GetClientRect(hwnd, &rc);
        if (x <= 108) ToggleLauncher();
        else if (x >= rc.right - 130) PostMessageW(hDesktop, WM_CLOSE, 0, 0);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ============================================================================
//  DOCK & LAUNCHER
// ============================================================================
static int gDockHover = -1;
static bool AppRunning(int i) {
    HWND hs[] = {hFiles, hTerminal, hEditor, hVim, hBrowser, hCompiler, hSettings, hAbout};
    return i >= 0 && i < TOTAL_APPS && hs[i] && IsWindow(hs[i]);
}

static LRESULT CALLBACK DockProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps); RECT rc; GetClientRect(hwnd, &rc);
        {
            DoubleBuf b(hdc, rc);
            FillRectC(b.mem, rc, C_DOCK);
            DrawLine(b.mem, rc.right - 1, 0, rc.right - 1, rc.bottom, RGB(85, 45, 75), 1);

            for (int i = 0; i < TOTAL_APPS; ++i) {
                RECT r{14, 12 + i * 56, 60, 12 + i * 56 + 46};
                if (i == gDockHover) { RECT hl = r; InflateRect(&hl, 6, 4); FillRound(b.mem, hl, 14, RGB(85, 50, 75)); }
                DrawAppIcon(b.mem, i, r);
                if (AppRunning(i)) FillCircle(b.mem, 6, (r.top + r.bottom) / 2, 3, C_ORANGE);
            }
        }
        EndPaint(hwnd, &ps);
    } else if (msg == WM_MOUSEMOVE) {
        int y = GET_Y_LPARAM(lp); RECT rc; GetClientRect(hwnd, &rc);
        int h = (y >= 12 && y < 12 + TOTAL_APPS * 56) ? (y - 12) / 56 : -1;
        if (h != gDockHover) { gDockHover = h; InvalidateRect(hwnd, nullptr, FALSE); }
        TRACKMOUSEEVENT t{sizeof(t), TME_LEAVE, hwnd, 0}; TrackMouseEvent(&t);
    } else if (msg == WM_MOUSELEAVE) { gDockHover = -1; InvalidateRect(hwnd, nullptr, FALSE); }
    else if (msg == WM_LBUTTONDOWN) {
        int y = GET_Y_LPARAM(lp);
        if (y >= 12 && y < 12 + TOTAL_APPS * 56) PostMessageW(hDesktop, WM_COMMAND, IDM_FILES + (y - 12) / 56, 0);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK LauncherProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_ERASEBKGND) return 1;
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hwnd, &ps); RECT rc; GetClientRect(hwnd, &rc);
        {
            DoubleBuf b(hdc, rc);
            FillRectC(b.mem, rc, RGB(35, 12, 30));
            TextIn(b.mem, RECT{0, 40, rc.right, 85}, L"CloneX Applications", fTitle, C_WHITE, DT_CENTER | DT_SINGLELINE);
        }
        EndPaint(hwnd, &ps);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void ToggleLauncher(bool forceHide) {
    if (!hLauncher) return;
    bool show = !forceHide && !IsWindowVisible(hLauncher);
    ShowWindow(hLauncher, show ? SW_SHOW : SW_HIDE);
    if (show) BringWindowToTop(hLauncher);
}

// ============================================================================
//  TERMINAL, FILES, EDITOR, SETTINGS, ABOUT
// ============================================================================
static HWND hTermOut = nullptr, hTermIn = nullptr, hTermPrompt = nullptr;
static HBRUSH gBrAub = nullptr;
static Path gTermCwd;
static WNDPROC gOldTermIn = nullptr;

static void TermAppend(const std::wstring& t) {
    if (!hTermOut) return;
    int len = GetWindowTextLengthW(hTermOut);
    SendMessageW(hTermOut, EM_SETSEL, len, len);
    SendMessageW(hTermOut, EM_REPLACESEL, FALSE, (LPARAM)t.c_str());
}

static std::wstring RunCommand(const std::wstring& line) {
    std::wstringstream ss(line); std::vector<std::wstring> tok; std::wstring w;
    while (ss >> w) tok.push_back(w);
    if (tok.empty()) return L"";
    std::wstring cmd = Lower(tok[0]); std::error_code ec; std::wstringstream out;

    if (cmd == L"help") out << L"Commands: ls, cd, pwd, mkdir, touch, cat, clear, exit\r\n";
    else if (cmd == L"clear") { SetWindowTextW(hTermOut, L""); return L""; }
    else if (cmd == L"pwd") out << ToDisplay(gTermCwd, false) << L"\r\n";
    else if (cmd == L"whoami") out << gUser << L"\r\n";
    else out << L"Command not found: " << tok[0] << L"\r\n";
    return out.str();
}

static LRESULT CALLBACK TermInProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_CHAR && w == VK_RETURN) return 0;
    if (m == WM_KEYDOWN && w == VK_RETURN) {
        std::wstring line = GetText(hTermIn);
        TermAppend(gUser + L"@" + kHost + L":" + ToDisplay(gTermCwd, true) + L"$ " + line + L"\r\n");
        SetWindowTextW(hTermIn, L"");
        TermAppend(RunCommand(line));
        SetWindowTextW(hTermPrompt, (ToDisplay(gTermCwd, true) + L"$").c_str());
        return 0;
    }
    return CallWindowProcW(gOldTermIn, h, m, w, l);
}

static LRESULT CALLBACK TerminalProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_CREATE) {
        hTermOut = CreateWindowExW(0, L"EDIT", L"CloneX Terminal - Type 'help'\r\n\r\n", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL, 0, 0, 10, 10, hwnd, nullptr, gInst, nullptr);
        hTermPrompt = CreateWindowW(L"STATIC", L"~$", WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE, 6, 0, 100, 24, hwnd, nullptr, gInst, nullptr);
        hTermIn = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 110, 0, 100, 24, hwnd, nullptr, gInst, nullptr);
        SetFont(hTermOut, fMono); SetFont(hTermPrompt, fMono); SetFont(hTermIn, fMono);
        gOldTermIn = (WNDPROC)SetWindowLongPtrW(hTermIn, GWLP_WNDPROC, (LONG_PTR)TermInProc);
        SetFocus(hTermIn);
    } else if (msg == WM_SIZE) {
        int w = LOWORD(lp), h = HIWORD(lp);
        MoveWindow(hTermOut, 0, 0, w, std::max(10, h - 28), TRUE);
        MoveWindow(hTermPrompt, 6, h - 26, 120, 24, TRUE);
        MoveWindow(hTermIn, 130, h - 26, std::max(10, w - 136), 24, TRUE);
    } else if (msg == WM_CTLCOLOREDIT || msg == WM_CTLCOLORSTATIC) {
        SetBkColor((HDC)wp, C_AUBERGINE); SetTextColor((HDC)wp, (HWND)lp == hTermPrompt ? RGB(120, 255, 130) : C_WHITE);
        return (LRESULT)gBrAub;
    } else if (msg == WM_DESTROY) { hTerminal = hTermOut = hTermIn = nullptr; InvalidateRect(hDock, nullptr, FALSE); }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static HWND hFileList = nullptr, hFilePath = nullptr;
static std::vector<std::pair<std::wstring, bool>> gFEntries;
static Path gFilesCwd;

static void RefreshFiles() {
    if (!hFileList) return;
    gFEntries.clear(); SendMessageW(hFileList, LB_RESETCONTENT, 0, 0);
    std::error_code ec;
    for (auto& e : fs::directory_iterator(ToReal(gFilesCwd), ec)) {
        bool d = e.is_directory(); std::wstring n = e.path().filename().wstring();
        gFEntries.push_back({n, d});
        SendMessageW(hFileList, LB_ADDSTRING, 0, (LPARAM)((d ? L"📁  " : L"📄  ") + n).c_str());
    }
    SetWindowTextW(hFilePath, ToDisplay(gFilesCwd, false).c_str());
}

static LRESULT CALLBACK FilesProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_CREATE) {
        hFilePath = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_READONLY, 78, 11, 360, 26, hwnd, nullptr, gInst, nullptr);
        SetFont(hFilePath, fUI);
        hFileList = CreateWindowExW(0, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY, 10, 48, 700, 420, hwnd, (HMENU)4002, gInst, nullptr);
        SetFont(hFileList, fUI); RefreshFiles();
    } else if (msg == WM_SIZE) {
        MoveWindow(hFileList, 10, 48, std::max(10, LOWORD(lp) - 20), std::max(10, HIWORD(lp) - 58), TRUE);
    } else if (msg == WM_DESTROY) { hFiles = hFileList = nullptr; InvalidateRect(hDock, nullptr, FALSE); }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static HWND hEdit = nullptr; static Path gEdPath;
static LRESULT CALLBACK EditorProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_CREATE) {
        hEdit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, 0, 48, 10, 10, hwnd, nullptr, gInst, nullptr);
        SetFont(hEdit, fMono);
    } else if (msg == WM_SIZE) MoveWindow(hEdit, 0, 48, LOWORD(lp), std::max(10, HIWORD(lp) - 48), TRUE);
    else if (msg == WM_DESTROY) { hEditor = hEdit = nullptr; InvalidateRect(hDock, nullptr, FALSE); }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK SettingsProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(hwnd, &ps); RECT rc; GetClientRect(hwnd, &rc);
        FillRectC(dc, rc, C_WIN_BG);
        TextIn(dc, RECT{30, 25, 500, 65}, L"CloneX System Settings", fTitle, C_TEXT, DT_LEFT | DT_SINGLELINE);
        EndPaint(hwnd, &ps);
    } else if (msg == WM_DESTROY) { hSettings = nullptr; InvalidateRect(hDock, nullptr, FALSE); }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static LRESULT CALLBACK AboutProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(hwnd, &ps); RECT rc; GetClientRect(hwnd, &rc);
        FillRectC(dc, rc, C_WIN_BG); FillRectC(dc, RECT{0, 0, rc.right, 95}, C_AUBERGINE);
        TextIn(dc, RECT{0, 26, rc.right, 80}, L"CloneX OS v1.0", fTitle, C_WHITE, DT_CENTER | DT_SINGLELINE);
        EndPaint(hwnd, &ps);
    } else if (msg == WM_DESTROY) { hAbout = nullptr; InvalidateRect(hDock, nullptr, FALSE); }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

// ============================================================================
//  WALLPAPER
// ============================================================================
static HBITMAP gWallpaper = nullptr;
static int gWallW = 0, gWallH = 0;

static void RenderWallpaper(HWND hwnd) {
    RECT rc; GetClientRect(hwnd, &rc);
    if (rc.right < 1 || rc.bottom < 1) return;
    HDC wnd = GetDC(hwnd), mem = CreateCompatibleDC(wnd);
    if (gWallpaper) DeleteObject(gWallpaper);
    gWallpaper = CreateCompatibleBitmap(wnd, rc.right, rc.bottom);
    gWallW = rc.right; gWallH = rc.bottom;
    HGDIOBJ old = SelectObject(mem, gWallpaper);

    for (int y = 0; y < rc.bottom; ++y) {
        double t = (double)y / std::max<LONG>(1, rc.bottom);
        FillRectC(mem, RECT{0, y, rc.right, y + 1}, RGB(46 + (int)(t * 58), 10 + (int)(t * 14), 38 + (int)(t * 48)));
    }
    SelectObject(mem, old); DeleteDC(mem); ReleaseDC(hwnd, wnd);
}

static LRESULT CALLBACK DesktopProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_CREATE) {
        hTopBar = CreateWindowW(L"CloneXTopBar", L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 10, 32, hwnd, nullptr, gInst, nullptr);
        hDock = CreateWindowW(L"CloneXDock", L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 32, DOCK_W, 10, hwnd, nullptr, gInst, nullptr);
        hLauncher = CreateWindowW(L"CloneXLauncher", L"", WS_CHILD | WS_CLIPSIBLINGS, DOCK_W, 32, 10, 10, hwnd, nullptr, gInst, nullptr);
    } else if (msg == WM_SIZE) {
        int w = LOWORD(lp), h = HIWORD(lp);
        MoveWindow(hTopBar, 0, 0, w, 32, TRUE);
        MoveWindow(hDock, 0, 32, DOCK_W, std::max(10, h - 32), TRUE);
        MoveWindow(hLauncher, DOCK_W, 32, std::max(10, w - DOCK_W), std::max(10, h - 32), TRUE);
        RenderWallpaper(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
    } else if (msg == WM_ERASEBKGND) return 1;
    else if (msg == WM_PAINT) {
        PAINTSTRUCT ps; HDC dc = BeginPaint(hwnd, &ps);
        if (gWallpaper) {
            HDC mem = CreateCompatibleDC(dc); HGDIOBJ old = SelectObject(mem, gWallpaper);
            BitBlt(dc, 0, 0, gWallW, gWallH, mem, 0, 0, SRCCOPY);
            SelectObject(mem, old); DeleteDC(mem);
        }
        EndPaint(hwnd, &ps);
    } else if (msg == WM_COMMAND) {
        switch (LOWORD(wp)) {
        case IDM_FILES: OpenFiles(); break; case IDM_TERMINAL: OpenTerminal(); break;
        case IDM_EDITOR: OpenEditor(); break; case IDM_VIM: OpenVim(); break;
        case IDM_BROWSER: OpenBrowser(); break; case IDM_COMPILER: OpenCompiler(); break;
        case IDM_SETTINGS: OpenSettings(); break; case IDM_ABOUT: OpenAbout(); break;
        case IDM_LAUNCHER: ToggleLauncher(); break;
        }
    } else if (msg == WM_CLOSE) {
        if (MessageBoxW(hwnd, L"Shut down CloneX OS?", L"Power Off", MB_YESNO | MB_ICONQUESTION) == IDYES) DestroyWindow(hwnd);
    } else if (msg == WM_DESTROY) { if (gWallpaper) DeleteObject(gWallpaper); PostQuitMessage(0); }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static int gCascade = 0;
static HWND MakeWin(const wchar_t* cls, const wchar_t* title, int w, int h) {
    int x = 115 + (gCascade % 5) * 28, y = 62 + (gCascade % 5) * 28; ++gCascade;
    return CreateWindowExW(WS_EX_APPWINDOW, cls, title, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_VISIBLE, x, y, w, h, hDesktop, nullptr, gInst, nullptr);
}
static void Raise(HWND h) { if (IsIconic(h)) ShowWindow(h, SW_RESTORE); SetForegroundWindow(h); }

void OpenTerminal() { if (hTerminal) { Raise(hTerminal); return; } hTerminal = MakeWin(L"CloneXTerminal", L"Terminal", 720, 440); InvalidateRect(hDock, nullptr, FALSE); }
void OpenFiles(const Path* s) { if (hFiles) { if (s) { gFilesCwd = *s; RefreshFiles(); } Raise(hFiles); return; } gFilesCwd = s ? *s : Path{}; hFiles = MakeWin(L"CloneXFiles", L"Files", 740, 480); InvalidateRect(hDock, nullptr, FALSE); }
void OpenEditor(const Path* f) { if (hEditor) { if (f) { gEdPath = *f; std::wstring t; if (ReadTextFile(ToReal(*f), t)) SetWindowTextW(hEdit, t.c_str()); } Raise(hEditor); return; } hEditor = MakeWin(L"CloneXEditor", L"Text Editor", 720, 480); if (f) { gEdPath = *f; std::wstring t; if (ReadTextFile(ToReal(*f), t)) SetWindowTextW(hEdit, t.c_str()); } InvalidateRect(hDock, nullptr, FALSE); }
void OpenVim(const fs::path* f) { if (hVim) { if (f) VimLoadFile(*f); Raise(hVim); return; } hVim = MakeWin(L"CloneXVim", L"Vim Editor", 740, 480); if (f) VimLoadFile(*f); InvalidateRect(hDock, nullptr, FALSE); }
void OpenBrowser() { if (hBrowser) { Raise(hBrowser); return; } hBrowser = MakeWin(L"CloneXBrowser", L"Web Browser", 820, 500); InvalidateRect(hDock, nullptr, FALSE); }
void OpenCompiler() { if (hCompiler) { Raise(hCompiler); return; } hCompiler = MakeWin(L"CloneXCompiler", L"C++ IDE & Runner", 780, 520); InvalidateRect(hDock, nullptr, FALSE); }
void OpenSettings() { if (hSettings) { Raise(hSettings); return; } hSettings = MakeWin(L"CloneXSettings", L"Settings", 680, 400); InvalidateRect(hDock, nullptr, FALSE); }
void OpenAbout() { if (hAbout) { Raise(hAbout); return; } hAbout = MakeWin(L"CloneXAbout", L"About CloneX", 480, 360); InvalidateRect(hDock, nullptr, FALSE); }

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    gInst = hInstance;
    InitSandbox();
    BackendOpt::OptimizeMemory();

    fUI = MakeFont(15, FW_NORMAL, L"Segoe UI"); fSmall = MakeFont(13, FW_NORMAL, L"Segoe UI");
    fBold = MakeFont(15, FW_SEMIBOLD, L"Segoe UI"); fTitle = MakeFont(24, FW_BOLD, L"Segoe UI");
    fMono = MakeFont(16, FW_NORMAL, L"Consolas"); gBrAub = CreateSolidBrush(C_AUBERGINE);

    RegClass(L"CloneXDesktop", DesktopProc); RegClass(L"CloneXTopBar", TopBarProc);
    RegClass(L"CloneXDock", DockProc);       RegClass(L"CloneXLauncher", LauncherProc);
    RegClass(L"CloneXTerminal", TerminalProc, gBrAub); RegClass(L"CloneXFiles", FilesProc);
    RegClass(L"CloneXEditor", EditorProc);   RegClass(L"CloneXVim", VimProc);
    RegClass(L"CloneXBrowser", BrowserProc); RegClass(L"CloneXCompiler", CompilerProc);
    RegClass(L"CloneXSettings", SettingsProc); RegClass(L"CloneXAbout", AboutProc);
    RegClass(L"CloneXInput", InputProc, (HBRUSH)(COLOR_BTNFACE + 1));

    RECT wa{}; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    hDesktop = CreateWindowExW(WS_EX_APPWINDOW, L"CloneXDesktop", L"CloneX OS", WS_POPUP | WS_CLIPCHILDREN | WS_VISIBLE,
                               wa.left, wa.top, wa.right - wa.left, wa.bottom - wa.top, nullptr, nullptr, hInstance, nullptr);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return (int)msg.wParam;
}