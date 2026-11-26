#define UNICODE
#define _UNICODE
#define NOMINMAX

#include <windows.h>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

extern HINSTANCE gInst;
extern HWND hDock;
extern HWND hVim;
extern HFONT fMono;
extern fs::path gHome;

bool ReadTextFile(const fs::path& p, std::wstring& out);
bool WriteTextFile(const fs::path& p, const std::wstring& text, bool append = false);

static HWND hVimEdit = nullptr;
static HWND hVimStatus = nullptr;
static WNDPROC gOldVimEdit = nullptr;

enum VimMode { VIM_NORMAL = 0, VIM_INSERT, VIM_COMMAND };
static VimMode gVimMode = VIM_NORMAL;
static std::wstring gVimCmdBuffer;
static std::wstring gVimFileName = L"workspace_code.cpp";
static fs::path gVimFilePath;

static void UpdateVimStatus(const std::wstring& customMsg = L"") {
    if (!hVimStatus) return;
    if (!customMsg.empty()) {
        SetWindowTextW(hVimStatus, customMsg.c_str());
        return;
    }
    if (gVimMode == VIM_NORMAL)
        SetWindowTextW(hVimStatus, (L" NORMAL  |  file: " + gVimFileName + L"  |  [i] Insert, [:] Command").c_str());
    else if (gVimMode == VIM_INSERT)
        SetWindowTextW(hVimStatus, (L" INSERT  |  file: " + gVimFileName + L"  |  [ESC] Normal Mode").c_str());
    else if (gVimMode == VIM_COMMAND)
        SetWindowTextW(hVimStatus, (L":" + gVimCmdBuffer).c_str());
}

static void ExecuteVimCommand(HWND hwnd) {
    std::wstring cmd = gVimCmdBuffer;
    gVimCmdBuffer.clear();
    gVimMode = VIM_NORMAL;

    if (cmd == L"q" || cmd == L"q!") {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        return;
    }
    if (cmd == L"w" || cmd == L"wq") {
        int len = GetWindowTextLengthW(hVimEdit);
        std::wstring text(len + 1, L'\0');
        GetWindowTextW(hVimEdit, text.data(), len + 1);
        text.resize(len);

        if (gVimFilePath.empty()) gVimFilePath = gHome / L"Documents" / gVimFileName;
        if (WriteTextFile(gVimFilePath, text)) {
            UpdateVimStatus(L"\"" + gVimFileName + L"\" written successfully.");
            if (cmd == L"wq") PostMessageW(hwnd, WM_CLOSE, 0, 0);
        } else {
            UpdateVimStatus(L"E212: Can't open file for writing");
        }
        return;
    }
    if (cmd.rfind(L"w ", 0) == 0 && cmd.size() > 2) {
        gVimFileName = cmd.substr(2);
        gVimFilePath = gHome / L"Documents" / gVimFileName;
        gVimCmdBuffer = L"w";
        ExecuteVimCommand(hwnd);
        return;
    }
    UpdateVimStatus(L"E492: Not an editor command: " + cmd);
}

static LRESULT CALLBACK VimEditSubProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    HWND parent = GetParent(h);
    if (m == WM_KEYDOWN) {
        if (w == VK_ESCAPE) {
            gVimMode = VIM_NORMAL;
            gVimCmdBuffer.clear();
            UpdateVimStatus();
            return 0;
        }
        if (gVimMode == VIM_COMMAND) {
            if (w == VK_RETURN) {
                ExecuteVimCommand(parent);
                return 0;
            }
            if (w == VK_BACK && !gVimCmdBuffer.empty()) {
                gVimCmdBuffer.pop_back();
                UpdateVimStatus();
                return 0;
            }
            return 0;
        }
        if (gVimMode == VIM_NORMAL) {
            if (w == 'H' || w == VK_LEFT) { SendMessageW(h, WM_KEYDOWN, VK_LEFT, 0); return 0; }
            if (w == 'L' || w == VK_RIGHT) { SendMessageW(h, WM_KEYDOWN, VK_RIGHT, 0); return 0; }
            if (w == 'J' || w == VK_DOWN) { SendMessageW(h, WM_KEYDOWN, VK_DOWN, 0); return 0; }
            if (w == 'K' || w == VK_UP) { SendMessageW(h, WM_KEYDOWN, VK_UP, 0); return 0; }
            return 0;
        }
    }
    if (m == WM_CHAR) {
        if (gVimMode == VIM_COMMAND) {
            if (w >= 32) {
                gVimCmdBuffer += (wchar_t)w;
                UpdateVimStatus();
            }
            return 0;
        }
        if (gVimMode == VIM_NORMAL) {
            if (w == L'i' || w == L'I' || w == L'a' || w == L'A') {
                gVimMode = VIM_INSERT;
                UpdateVimStatus();
            } else if (w == L':') {
                gVimMode = VIM_COMMAND;
                gVimCmdBuffer.clear();
                UpdateVimStatus();
            }
            return 0;
        }
    }
    return CallWindowProcW(gOldVimEdit, h, m, w, l);
}

void VimLoadFile(const fs::path& p) {
    gVimFilePath = p;
    gVimFileName = p.filename().wstring();
    std::wstring content;
    if (fs::exists(p) && ReadTextFile(p, content)) {
        if (hVimEdit) SetWindowTextW(hVimEdit, content.c_str());
    } else {
        if (hVimEdit) SetWindowTextW(hVimEdit, L"");
    }
    gVimMode = VIM_NORMAL;
    UpdateVimStatus();
}

LRESULT CALLBACK VimProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HBRUSH brDark = CreateSolidBrush(RGB(30, 30, 38));
    switch (msg) {
    case WM_CREATE:
        hVimEdit = CreateWindowExW(0, L"EDIT",
            L"#include <iostream>\r\n\r\nint main() {\r\n    std::cout << \"Vim in MyVirtualOS is working!\\n\";\r\n    return 0;\r\n}\r\n",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
            0, 0, 100, 100, hwnd, nullptr, gInst, nullptr);

        hVimStatus = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE,
            0, 0, 100, 28, hwnd, nullptr, gInst, nullptr);

        SendMessageW(hVimEdit, WM_SETFONT, (WPARAM)fMono, TRUE);
        SendMessageW(hVimStatus, WM_SETFONT, (WPARAM)fMono, TRUE);
        gOldVimEdit = (WNDPROC)SetWindowLongPtrW(hVimEdit, GWLP_WNDPROC, (LONG_PTR)VimEditSubProc);

        gVimMode = VIM_NORMAL;
        UpdateVimStatus();
        SetFocus(hVimEdit);
        return 0;

    case WM_SIZE: {
        int w = LOWORD(lp), h = HIWORD(lp);
        MoveWindow(hVimEdit, 0, 0, w, std::max(10, h - 30), TRUE);
        MoveWindow(hVimStatus, 0, h - 28, w, 28, TRUE);
        return 0;
    }
    case WM_SETFOCUS:
        if (hVimEdit) SetFocus(hVimEdit);
        return 0;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC: {
        HDC dc = (HDC)wp;
        SetBkColor(dc, RGB(30, 30, 38));
        SetTextColor(dc, (HWND)lp == hVimStatus ? RGB(255, 204, 102) : RGB(225, 225, 235));
        return (LRESULT)brDark;
    }
    case WM_DESTROY:
        hVim = hVimEdit = hVimStatus = nullptr;
        InvalidateRect(hDock, nullptr, FALSE);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}