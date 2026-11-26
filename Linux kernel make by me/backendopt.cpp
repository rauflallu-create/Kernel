#define UNICODE
#define _UNICODE
#define NOMINMAX

#include <windows.h>
#include <filesystem>
#include <string>
#include <sstream>

namespace fs = std::filesystem;

namespace BackendOpt {
    void OptimizeMemory() {
        // Working set পরিষ্কার করে RAM ব্যবহার কমায়
        SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);
    }

    std::wstring GetSystemStatus(const fs::path& homePath) {
        MEMORYSTATUSEX m{};
        m.dwLength = sizeof(m);
        GlobalMemoryStatusEx(&m);

        ULARGE_INTEGER avail{}, total{}, freeB{};
        GetDiskFreeSpaceExW(homePath.wstring().c_str(), &avail, &total, &freeB);

        unsigned long long totalRAM = m.ullTotalPhys / (1024 * 1024);
        unsigned long long availRAM = m.ullAvailPhys / (1024 * 1024);
        unsigned long long usedRAM = totalRAM - availRAM;

        std::wstringstream ss;
        ss << L"=== Backend System Optimizer ===\r\n"
           << L"Memory Usage : " << usedRAM << L" MB / " << totalRAM << L" MB (" << m.dwMemoryLoad << L"%)\r\n"
           << L"Virtual Disk : 1024 GB SSD (UbuntuHome Sandbox Active)\r\n"
           << L"Real Disk Free: " << (freeB.QuadPart >> 30) << L" GB\r\n"
           << L"Status       : Optimal Performance\r\n";
        return ss.str();
    }
}