#define UNICODE
#define _UNICODE
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

extern HINSTANCE gInst;
extern HWND hDock, hBrowser;
extern HFONT fUI, fBold, fMono;

static HWND hUrlEdit = nullptr;
static HWND hInfoStatic = nullptr;
static std::wstring gCurrentUrl = L"https://www.google.com";

// ব্রাউজার হিস্ট্রি এবং ন্যাভিগেশন স্ট্যাক
static std::vector<std::wstring> gHistory;
static int gHistoryIdx = -1;

static std::wstring GetCtrlText(HWND h) {
    int len = GetWindowTextLengthW(h);
    std::wstring s((size_t)len + 1, L'\0');
    GetWindowTextW(h, s.data(), len + 1);
    s.resize((size_t)len);
    return s;
}

// ভার্চুয়াল ক্রোম স্টাইলের ওয়েব পেজ রেন্ডারার
static void RenderWebPage(HWND hwnd, const std::wstring& url, bool pushHistory = true) {
    gCurrentUrl = url;
    if (hUrlEdit) SetWindowTextW(hUrlEdit, url.c_str());

    if (pushHistory) {
        if (gHistoryIdx < (int)gHistory.size() - 1) {
            gHistory.erase(gHistory.begin() + gHistoryIdx + 1, gHistory.end());
        }
        gHistory.push_back(url);
        gHistoryIdx = (int)gHistory.size() - 1;
    }

    std::wstring content;
    if (url.find(L"google.com/search") != std::wstring::npos || url.find(L"google.com") != std::wstring::npos) {
        content = L"============================================================\r\n"
                  L"  G O O G L E   V I R TUAL   S E A R C H   E N G I N E\r\n"
                  L"============================================================\r\n\r\n"
                  L"Query / URL: " + url + L"\r\n\r\n"
                  L"Web Results:\r\n"
                  L"------------------------------------------------------------\r\n"
                  L" [1] MyVirtualOS - Ubuntu 24.04 LTS Desktop in C++20\r\n"
                  L"     https://www.myvirtualos.ubuntu.dev\r\n"
                  L"     Build high-fidelity modular operating systems inside Windows\r\n"
                  L"     using Win32 API, C++20, and custom vector graphics.\r\n\r\n"
                  L" [2] GitHub - Ubuntu Developer / MyVirtualOS\r\n"
                  L"     https://github.com/ubuntu/myvirtualos\r\n"
                  L"     Source code, compiler integration, vim editor, and backend tools.\r\n\r\n"
                  L" [3] C++20 Win32 GUI Programming Guide\r\n"
                  L"     https://docs.microsoft.com/windows/win32\r\n"
                  L"     Creating modern UI elements, custom controls, and memory management.\r\n";
    } else if (url.find(L"github.com") != std::wstring::npos) {
        content = L"============================================================\r\n"
                  L"  G I T H U B  -  [ubuntu / MyVirtualOS]\r\n"
                  L"============================================================\r\n\r\n"
                  L"  [+] Star  1.4k    [&] Fork  320    [@] Watching  85\r\n\r\n"
                  L"Files  |  Pull Requests  |  Actions  |  Security  |  Insights\r\n"
                  L"------------------------------------------------------------\r\n"
                  L" Latest commit by ubuntu-dev on main (2 mins ago):\r\n"
                  L" Update compiler.cpp: Fixed multi-line input support for C++ IDE\r\n\r\n"
                  L" Directory structure:\r\n"
                  L"   📁 .github/workflows\r\n"
                  L"   📁 UbuntuHome/Documents\r\n"
                  L"   📄 main.cpp\r\n"
                  L"   📄 vim.cpp\r\n"
                  L"   📄 browser.cpp\r\n"
                  L"   📄 compiler.cpp\r\n"
                  L"   📄 backendopt.cpp\r\n"
                  L"   📄 CMakeLists.txt\r\n";
    } else if (url.find(L"ubuntu.com") != std::wstring::npos) {
        content = L"============================================================\r\n"
                  L"  U B U N T U   2 4 . 0 4   L T S   (Noble Numbat)\r\n"
                  L"============================================================\r\n\r\n"
                  L"Secure, enterprise-grade open source operating system for cloud,\r\n"
                  L"desktop, IoT, and virtual environments.\r\n\r\n"
                  L"Highlights of Ubuntu 24.04 LTS:\r\n"
                  L" • Powered by Linux Kernel 6.8\r\n"
                  L" • Refreshed Aubergine & Orange Yaru Theme\r\n"
                  L" • Enhanced C++20 Toolchain and Dev Security\r\n\r\n"
                  L" [ Download Ubuntu Desktop ]     [ Explore Developer Hub ]\r\n";
    } else {
        content = L"============================================================\r\n"
                  L"  C H R O M E   V I R T U A L   B R O W S E R\r\n"
                  L"============================================================\r\n\r\n"
                  L"Address: " + url + L"\r\n\r\n"
                  L"This page is running securely inside MyVirtualOS Web Engine.\r\n"
                  L"You can type any website URL or search term in the address bar above.\r\n";
    }

    if (hInfoStatic) SetWindowTextW(hInfoStatic, content.c_str());
}

void LaunchWebUrl(std::wstring input) {
    if (input.find(L"http://") != 0 && input.find(L"https://") != 0) {
        if (input.find(L'.') == std::wstring::npos || input.find(L' ') != std::wstring::npos) {
            input = L"https://www.google.com/search?q=" + input;
        } else {
            input = L"https://" + input;
        }
    }
    if (hBrowser) {
        ShowWindow(hBrowser, SW_RESTORE);
        SetForegroundWindow(hBrowser);
        RenderWebPage(hBrowser, input);
    }
}

LRESULT CALLBACK BrowserProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HBRUSH brBg = CreateSolidBrush(RGB(255, 255, 255));
    switch (msg) {
    case WM_CREATE: {
        // ক্রোম স্টাইল ট্যাব বার
        CreateWindowW(L"BUTTON", L" ≡ MyVirtualOS Tab x ", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 12, 6, 170, 26, hwnd, (HMENU)9006, gInst, nullptr);
        CreateWindowW(L"BUTTON", L" + ", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 186, 6, 30, 26, hwnd, (HMENU)9007, gInst, nullptr);

        // নেভিগেশন কন্ট্রোল (Back, Forward, Refresh, Address Bar, Go)
        CreateWindowW(L"BUTTON", L"◄", WS_CHILD | WS_VISIBLE, 12, 36, 36, 30, hwnd, (HMENU)9001, gInst, nullptr);
        CreateWindowW(L"BUTTON", L"►", WS_CHILD | WS_VISIBLE, 52, 36, 36, 30, hwnd, (HMENU)9002, gInst, nullptr);
        CreateWindowW(L"BUTTON", L"↻", WS_CHILD | WS_VISIBLE, 92, 36, 36, 30, hwnd, (HMENU)9003, gInst, nullptr);

        hUrlEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", gCurrentUrl.c_str(),
                                   WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                   136, 38, 530, 28, hwnd, (HMENU)9004, gInst, nullptr);

        HWND goBtn = CreateWindowW(L"BUTTON", L"Search / Go", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                   674, 36, 100, 30, hwnd, (HMENU)9005, gInst, nullptr);

        // ওয়েব কনটেন্ট ভিউয়ার
        hInfoStatic = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                      WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
                                      12, 74, 762, 430, hwnd, nullptr, gInst, nullptr);

        SendMessageW(hUrlEdit, WM_SETFONT, (WPARAM)fUI, TRUE);
        SendMessageW(goBtn, WM_SETFONT, (WPARAM)fBold, TRUE);
        SendMessageW(hInfoStatic, WM_SETFONT, (WPARAM)fMono, TRUE);

        RenderWebPage(hwnd, gCurrentUrl);
        return 0;
    }
    case WM_SIZE: {
        int w = LOWORD(lp), h = HIWORD(lp);
        MoveWindow(hUrlEdit, 136, 38, std::max(100, w - 246), 28, TRUE);
        MoveWindow(GetDlgItem(hwnd, 9005), w - 104, 36, 92, 30, TRUE);
        MoveWindow(hInfoStatic, 12, 74, std::max(50, w - 24), std::max(50, h - 86), TRUE);
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        if (id == 9001) { // Back button
            if (gHistoryIdx > 0) {
                gHistoryIdx--;
                RenderWebPage(hwnd, gHistory[gHistoryIdx], false);
            }
        } else if (id == 9002) { // Forward button
            if (gHistoryIdx < (int)gHistory.size() - 1) {
                gHistoryIdx++;
                RenderWebPage(hwnd, gHistory[gHistoryIdx], false);
            }
        } else if (id == 9003) { // Refresh button
            if (gHistoryIdx >= 0 && gHistoryIdx < (int)gHistory.size()) {
                RenderWebPage(hwnd, gHistory[gHistoryIdx], false);
            }
        } else if (id == 9005 || id == 9004) { // Go or Enter in URL bar
            if (id == 9005 || (id == 9004 && HIWORD(wp) == EN_UPDATE)) {
                // If Go button clicked
                if (id == 9005) {
                    std::wstring url = GetCtrlText(hUrlEdit);
                    LaunchWebUrl(url);
                }
            }
        }
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
        HDC dc = (HDC)wp;
        SetBkColor(dc, RGB(255, 255, 255));
        SetTextColor(dc, RGB(30, 30, 35));
        return (LRESULT)brBg;
    }
    case WM_DESTROY:
        hBrowser = hUrlEdit = hInfoStatic = nullptr;
        InvalidateRect(hDock, nullptr, FALSE);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}