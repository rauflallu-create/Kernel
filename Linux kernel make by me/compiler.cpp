// compiler.cpp
#define UNICODE
#define _UNICODE
#define NOMINMAX

#include <windows.h>
#include <windowsx.h>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

extern HINSTANCE gInst;
extern HWND hDock, hCompiler;
extern HFONT fUI, fBold, fMono;
extern fs::path gHome;

bool WriteTextFile(const fs::path& p, const std::wstring& text, bool append = false);
bool ReadTextFile(const fs::path& p, std::wstring& out);

enum {
    ID_IDE_NEW = 8001,
    ID_IDE_OPEN,
    ID_IDE_SAVE,
    ID_IDE_RUN,
    ID_IDE_FILENAME,
    ID_IDE_CODE,
    ID_IDE_STDIN,
    ID_IDE_OUTPUT
};

static HWND hFileNameEdit = nullptr;
static HWND hCodeEdit = nullptr;
static HWND hStdinEdit = nullptr;
static HWND hOutputEdit = nullptr;

static DWORD RunSilentProcess(const std::wstring& cmdLine) {
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    std::wstring mutableCmd = L"cmd.exe /c " + cmdLine;

    if (!CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        return 1;
    }

    WaitForSingleObject(pi.hProcess, 10000);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exitCode;
}

static std::wstring GetCtrlText(HWND h) {
    int len = GetWindowTextLengthW(h);
    std::wstring s((size_t)len + 1, L'\0');
    GetWindowTextW(h, s.data(), len + 1);
    s.resize((size_t)len);
    return s;
}

static fs::path GetTargetFilePath() {
    std::wstring fname = GetCtrlText(hFileNameEdit);
    if (fname.empty()) fname = L"program.cpp";
    if (fname.find(L'.') == std::wstring::npos) fname += L".cpp";
    return gHome / L"Documents" / fname;
}

static void IdeSaveFile(bool notify = true) {
    if (!hCodeEdit) return;
    fs::path target = GetTargetFilePath();
    std::error_code ec;
    fs::create_directories(target.parent_path(), ec);

    if (WriteTextFile(target, GetCtrlText(hCodeEdit))) {
        if (notify)
            SetWindowTextW(hOutputEdit, (L"[Clolix] File saved: ~/Documents/" + target.filename().wstring()).c_str());
    } else {
        SetWindowTextW(hOutputEdit, L"[Error] Failed to save C++ file.");
    }
}

static void IdeOpenFile() {
    fs::path target = GetTargetFilePath();
    std::wstring content;
    if (ReadTextFile(target, content)) {
        SetWindowTextW(hCodeEdit, content.c_str());
        SetWindowTextW(hOutputEdit, (L"[Clolix] Opened file: ~/Documents/" + target.filename().wstring()).c_str());
    } else {
        SetWindowTextW(hOutputEdit, (L"[Error] File not found: " + target.filename().wstring()).c_str());
    }
}

static void IdeCompileAndRun() {
    if (!hCodeEdit || !hOutputEdit) return;

    IdeSaveFile(false);

    fs::path srcFile = GetTargetFilePath();
    fs::path exeFile = srcFile;
    exeFile.replace_extension(L".exe");

    fs::path stdinFile = gHome / L"Documents" / L".ide_stdin.txt";
    fs::path logFile   = gHome / L"Documents" / L".ide_build.log";
    fs::path outFile   = gHome / L"Documents" / L".ide_output.txt";

    std::wstring rawInput = GetCtrlText(hStdinEdit);
    WriteTextFile(stdinFile, rawInput);

    SetWindowTextW(hOutputEdit, L"Compiling C++ code (-std=c++20)...");
    UpdateWindow(hOutputEdit);

    std::wstring compileCmd = L"g++ -std=c++20 \"" + srcFile.wstring() + L"\" -o \"" + exeFile.wstring() +
                              L"\" > \"" + logFile.wstring() + L"\" 2>&1";
    DWORD compRes = RunSilentProcess(compileCmd);

    std::wstring resultText;
    if (compRes != 0) {
        ReadTextFile(logFile, resultText);
        resultText = L"=== COMPILATION ERROR ===\r\n\r\n" + resultText;
    } else {
        std::wstring runCmd = L"\"" + exeFile.wstring() + L"\" < \"" + stdinFile.wstring() +
                              L"\" > \"" + outFile.wstring() + L"\" 2>&1";
        DWORD runRes = RunSilentProcess(runCmd);

        ReadTextFile(outFile, resultText);
        if (resultText.empty()) resultText = L"(Program executed successfully with no output)";
        resultText = L"=== EXECUTION SUCCESS (Exit Code: " + std::to_wstring(runRes) + L") ===\r\n\r\n" + resultText;
    }

    SetWindowTextW(hOutputEdit, resultText.c_str());
}

LRESULT CALLBACK CompilerProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    static HBRUSH brDark = CreateSolidBrush(RGB(28, 28, 36));
    switch (msg) {
    case WM_CREATE: {
        CreateWindowW(L"STATIC", L"File Name:", WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE,
                      12, 12, 80, 28, hwnd, nullptr, gInst, nullptr);

        hFileNameEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"solution.cpp",
                                        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                        98, 12, 140, 28, hwnd, (HMENU)ID_IDE_FILENAME, gInst, nullptr);

        CreateWindowW(L"BUTTON", L"New", WS_CHILD | WS_VISIBLE, 246, 12, 58, 28, hwnd, (HMENU)ID_IDE_NEW, gInst, nullptr);
        CreateWindowW(L"BUTTON", L"Open", WS_CHILD | WS_VISIBLE, 310, 12, 58, 28, hwnd, (HMENU)ID_IDE_OPEN, gInst, nullptr);
        CreateWindowW(L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE, 374, 12, 58, 28, hwnd, (HMENU)ID_IDE_SAVE, gInst, nullptr);

        HWND runBtn = CreateWindowW(L"BUTTON", L"▶ Run C++ Code", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                    442, 12, 140, 28, hwnd, (HMENU)ID_IDE_RUN, gInst, nullptr);

        hCodeEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
            L"#include <iostream>\r\n"
            L"using namespace std;\r\n\r\n"
            L"int main() {\r\n"
            L"    int t;\r\n"
            L"    if (cin >> t) {\r\n"
            L"        while(t--) {\r\n"
            L"            int n;\r\n"
            L"            cin >> n;\r\n"
            L"            if (n % 2 == 0) cout << n << \" : YES\\n\";\r\n"
            L"            else cout << n << \" : NO\\n\";\r\n"
            L"        }\r\n"
            L"    }\r\n"
            L"    return 0;\r\n"
            L"}\r\n",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
            12, 50, 760, 250, hwnd, (HMENU)ID_IDE_CODE, gInst, nullptr);

        CreateWindowW(L"STATIC", L"Input (cin):", WS_CHILD | WS_VISIBLE,
                      12, 308, 120, 20, hwnd, nullptr, gInst, nullptr);

        hStdinEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"5\r\n1\r\n2\r\n3\r\n4\r\n5",
                                     WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                                     12, 330, 220, 170, hwnd, (HMENU)ID_IDE_STDIN, gInst, nullptr);

        CreateWindowW(L"STATIC", L"Virtual Console Output:", WS_CHILD | WS_VISIBLE,
                      244, 308, 200, 20, hwnd, nullptr, gInst, nullptr);
        hOutputEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT",
                                      L"Ready. Click 'Run C++ Code' to execute your program.",
                                      WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY,
                                      244, 330, 528, 170, hwnd, (HMENU)ID_IDE_OUTPUT, gInst, nullptr);

        SendMessageW(hFileNameEdit, WM_SETFONT, (WPARAM)fUI, TRUE);
        SendMessageW(runBtn, WM_SETFONT, (WPARAM)fBold, TRUE);
        SendMessageW(hCodeEdit, WM_SETFONT, (WPARAM)fMono, TRUE);
        SendMessageW(hStdinEdit, WM_SETFONT, (WPARAM)fMono, TRUE);
        SendMessageW(hOutputEdit, WM_SETFONT, (WPARAM)fMono, TRUE);
        return 0;
    }
    case WM_SIZE: {
        int w = LOWORD(lp), h = HIWORD(lp);
        int codeH = std::max(120, (int)(h * 0.52));
        int bottomY = 50 + codeH + 26;
        int bottomH = std::max(80, h - bottomY - 12);
        int inW = std::min(230, w / 3);

        MoveWindow(hCodeEdit, 12, 50, std::max(10, w - 24), codeH, TRUE);
        MoveWindow(hStdinEdit, 12, bottomY, inW, bottomH, TRUE);
        MoveWindow(hOutputEdit, 24 + inW, bottomY, std::max(10, w - inW - 36), bottomH, TRUE);
        return 0;
    }
    case WM_COMMAND: {
        switch (LOWORD(wp)) {
        case ID_IDE_NEW:
            SetWindowTextW(hFileNameEdit, L"program.cpp");
            SetWindowTextW(hCodeEdit, L"#include <iostream>\r\nusing namespace std;\r\n\r\nint main() {\r\n    \r\n    return 0;\r\n}\r\n");
            SetWindowTextW(hOutputEdit, L"New file created.");
            break;
        case ID_IDE_OPEN:
            IdeOpenFile();
            break;
        case ID_IDE_SAVE:
            IdeSaveFile(true);
            break;
        case ID_IDE_RUN:
            IdeCompileAndRun();
            break;
        }
        return 0;
    }
    case WM_CTLCOLOREDIT: {
        HWND target = (HWND)lp;
        if (target == hCodeEdit || target == hOutputEdit || target == hStdinEdit) {
            HDC dc = (HDC)wp;
            SetBkColor(dc, RGB(28, 28, 36));
            SetTextColor(dc, target == hOutputEdit ? RGB(100, 255, 140) : RGB(235, 235, 245));
            return (LRESULT)brDark;
        }
        break;
    }
    case WM_DESTROY:
        hCompiler = hCodeEdit = hOutputEdit = hStdinEdit = hFileNameEdit = nullptr;
        InvalidateRect(hDock, nullptr, FALSE);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}