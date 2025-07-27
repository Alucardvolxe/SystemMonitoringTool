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

int main (){
    printMemoryInfo();
    printDiskSpace("C:\\");
    return 0;
}