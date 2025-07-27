#include <iostream>
#include <windows.h>

void getCompname(){
    char computerName[MAX_COMPUTERNAME_LENGTH +1];
    DWORD size = sizeof(computerName);
    if (GetComputerNameA(computerName, &size)) {
        std::cout << "Computer Name: " << computerName << "\n";
    }
    else{
        std::cout<< "name";
    }
}

void printMemoryInfo() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(memInfo);  // MUST set this before calling

    if (GlobalMemoryStatusEx(&memInfo)) {
        std::cout << "=== Memory Information ===\n";
        std::cout << "Total RAM: "
                  << (memInfo.ullTotalPhys / (1024 * 1024)) << " MB\n";
        std::cout << "Available RAM: "
                  << (memInfo.ullAvailPhys / (1024 * 1024)) << " MB\n";
    } else {
        std::cerr << "Failed to retrieve memory info. Error: " << GetLastError() << "\n";
    }
}

void printDiskSpace(const char* drive) {
    ULARGE_INTEGER freeBytesAvailable, totalBytes, freeBytes;

    if (GetDiskFreeSpaceExA(drive, &freeBytesAvailable, &totalBytes, &freeBytes)) {
        std::cout << "=== Disk Space for " << drive << " ===\n";
        std::cout << "Total Space: "
                  << (totalBytes.QuadPart / (1024 * 1024 * 1024)) << " GB\n";
        std::cout << "Free Space: "
                  << (freeBytes.QuadPart / (1024 * 1024 * 1024)) << " GB\n";
        std::cout << "Free Space (available to user): "
                  << (freeBytesAvailable.QuadPart / (1024 * 1024 * 1024)) << " GB\n";
    } else {
        std::cerr << "Failed to retrieve disk info for " << drive
                  << ". Error: " << GetLastError() << "\n";
    }
}

void printCPUInfo() {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    std::cout << "=== CPU Information ===\n";
    std::cout << "Number of Logical Processors: " << sysInfo.dwNumberOfProcessors << "\n";

    std::cout << "Processor Architecture: ";
    switch (sysInfo.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: std::cout << "x64 (AMD/Intel 64-bit)\n"; break;
        case PROCESSOR_ARCHITECTURE_INTEL: std::cout << "x86 (32-bit)\n"; break;
        case PROCESSOR_ARCHITECTURE_ARM: std::cout << "ARM\n"; break;
        default: std::cout << "Unknown\n"; break;
    }
}

int main (){
    printMemoryInfo();
    printDiskSpace("C:\\");
    printCPUInfo();
    return 0;
}