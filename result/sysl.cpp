// Enable Unicode support
#define _UNICODE
#define UNICODE

// Windows-specific includes
#include <windows.h>
#include <psapi.h>       // For process info functions
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <commctrl.h>    // For common controls like ListView
#include <strsafe.h>     // Safer string handling
#include <chrono>
#include <fstream>
#include <sstream>

// Link against required Windows libraries
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "psapi.lib")

// Define control IDs
#define ID_LISTVIEW 1001
#define ID_REFRESH 1002
#define ID_ALERT_THRESHOLD 1003
#define ID_ALERT_EDIT 1004
#define MAX_HISTORY 60 // Number of history points to store (e.g. last 60 seconds)

// A structure to store details about each process
struct ProcessInfo {
    DWORD pid;                        // Process ID
    std::wstring name;               // Process name
    double cpuUsage;                 // Current CPU usage
    SIZE_T memoryUsage;              // Memory usage in bytes
    ULONGLONG lastCpuTime;           // Last recorded CPU time
    std::vector<double> cpuHistory;  // History of CPU usage
    std::vector<SIZE_T> memHistory;  // History of memory usage
};

// Main class to monitor and display processes
class ProcessMonitor {
private:
    // GUI elements
    HWND hWnd;
    HWND hListView;
    HWND hRefreshButton;
    HWND hAlertEdit;

    // Data storage
    std::vector<ProcessInfo> processes;
    std::map<DWORD, ULONGLONG> lastSystemTimes;
    double cpuAlertThreshold = 80.0; // Default CPU alert threshold
    ULONGLONG lastUpdateTime;

    // Get number of CPU cores
    DWORD GetNumberOfProcessors() {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        return sysInfo.dwNumberOfProcessors;
    }

    // Initialize GUI controls
    void InitGUI(HWND hwnd) {
        // Initialize common controls (like ListView)
        INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES };
        InitCommonControlsEx(&icex);

        // Create ListView for process display
        hListView = CreateWindowW(WC_LISTVIEWW, L"", 
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
            10, 10, 580, 300, hwnd, (HMENU)ID_LISTVIEW, GetModuleHandleW(NULL), NULL);

        // Define and add ListView columns
        LVCOLUMNW lvCol = { 0 };
        lvCol.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        lvCol.cx = 150;
        lvCol.pszText = const_cast<LPWSTR>(L"Process Name");
        ListView_InsertColumn(hListView, 0, &lvCol);

        lvCol.cx = 100;
        lvCol.pszText = const_cast<LPWSTR>(L"PID");
        ListView_InsertColumn(hListView, 1, &lvCol);

        lvCol.pszText = const_cast<LPWSTR>(L"CPU Usage (%)");
        ListView_InsertColumn(hListView, 2, &lvCol);

        lvCol.pszText = const_cast<LPWSTR>(L"Memory Usage (MB)");
        ListView_InsertColumn(hListView, 3, &lvCol);

        // Refresh button
        hRefreshButton = CreateWindowW(L"BUTTON", L"Refresh", 
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 320, 100, 30, hwnd, (HMENU)ID_REFRESH, GetModuleHandleW(NULL), NULL);

        // Static label
        CreateWindowW(L"STATIC", L"CPU Alert Threshold (%):", 
            WS_CHILD | WS_VISIBLE,
            120, 320, 150, 20, hwnd, NULL, GetModuleHandleW(NULL), NULL);

        // Edit box to enter alert threshold
        hAlertEdit = CreateWindowW(L"EDIT", L"80.0", 
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            270, 320, 60, 20, hwnd, (HMENU)ID_ALERT_EDIT, GetModuleHandleW(NULL), NULL);
    }

    // Main function to update process data
    void UpdateProcessList() {
        processes.clear();                  // Clear previous data
        ListView_DeleteAllItems(hListView); // Clear GUI list

        DWORD processesIds[1024], cbNeeded;
        if (!EnumProcesses(processesIds, sizeof(processesIds), &cbNeeded)) return;

        // Get current system time
        FILETIME ftSystem;
        GetSystemTimeAsFileTime(&ftSystem);
        ULONGLONG currentTime = ((ULONGLONG)ftSystem.dwHighDateTime << 32) | ftSystem.dwLowDateTime;

        DWORD processCount = cbNeeded / sizeof(DWORD);
        for (DWORD i = 0; i < processCount; i++) {
            if (processesIds[i] == 0) continue;

            // Open process for reading info
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processesIds[i]);
            if (hProcess == NULL) continue;

            WCHAR szProcessName[MAX_PATH] = L"<unknown>";
            GetModuleBaseNameW(hProcess, NULL, szProcessName, MAX_PATH);

            FILETIME ftCreate, ftExit, ftKernelTime, ftUserTime;
            if (GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernelTime, &ftUserTime)) {
                // Calculate total CPU time (kernel + user)
                ULONGLONG kernel = ((ULONGLONG)ftKernelTime.dwHighDateTime << 32) | ftKernelTime.dwLowDateTime;
                ULONGLONG user = ((ULONGLONG)ftUserTime.dwHighDateTime << 32) | ftUserTime.dwLowDateTime;
                ULONGLONG totalTime = kernel + user;

                // Estimate CPU usage since last check
                double cpuUsage = 0.0;
                auto it = lastSystemTimes.find(processesIds[i]);
                if (it != lastSystemTimes.end()) {
                    ULONGLONG timeDiff = currentTime - lastUpdateTime;
                    ULONGLONG cpuDiff = totalTime - it->second;
                    cpuUsage = (cpuDiff * 100.0) / (timeDiff * GetNumberOfProcessors());
                }

                // Get memory usage
                PROCESS_MEMORY_COUNTERS pmc;
                SIZE_T memoryUsage = 0;
                if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                    memoryUsage = pmc.WorkingSetSize;
                }

                // Save the info to struct
                ProcessInfo info;
                info.pid = processesIds[i];
                info.name = szProcessName;
                info.cpuUsage = cpuUsage;
                info.memoryUsage = memoryUsage;
                info.lastCpuTime = totalTime;
                
                // Store history
                info.cpuHistory.push_back(cpuUsage);
                info.memHistory.push_back(memoryUsage);
                if (info.cpuHistory.size() > MAX_HISTORY) {
                    info.cpuHistory.erase(info.cpuHistory.begin());
                    info.memHistory.erase(info.memHistory.begin());
                }

                processes.push_back(info);
                lastSystemTimes[processesIds[i]] = totalTime;

                // Show alert if CPU usage too high
                if (cpuUsage > cpuAlertThreshold) {
                    WCHAR alertMsg[256];
                    StringCchPrintfW(alertMsg, 256, L"High CPU Usage Alert: %s (PID: %d) - %.2f%%", 
                        szProcessName, processesIds[i], cpuUsage);
                    MessageBoxW(hWnd, alertMsg, L"Alert", MB_OK | MB_ICONWARNING);
                }
            }

            CloseHandle(hProcess);
        }

        lastUpdateTime = currentTime;
        UpdateListView(); // Display data on ListView
    }

    // Update the GUI ListView with current process data
    void UpdateListView() {
        ListView_DeleteAllItems(hListView);
        
        for (size_t i = 0; i < processes.size(); i++) {
            LVITEMW lvItem = { 0 };
            lvItem.mask = LVIF_TEXT;
            lvItem.iItem = (int)i;

            WCHAR buffer[256];
            lvItem.pszText = (LPWSTR)processes[i].name.c_str();
            ListView_InsertItem(hListView, &lvItem);

            StringCchPrintfW(buffer, 256, L"%d", processes[i].pid);
            ListView_SetItemText(hListView, i, 1, buffer);

            StringCchPrintfW(buffer, 256, L"%.2f", processes[i].cpuUsage);
            ListView_SetItemText(hListView, i, 2, buffer);

            StringCchPrintfW(buffer, 256, L"%.2f", processes[i].memoryUsage / (1024.0 * 1024.0));
            ListView_SetItemText(hListView, i, 3, buffer);
        }
    }

    // Save process history to file
    void SaveHistoricalData() {
        std::wofstream file(L"process_history.txt", std::ios::app);
        if (!file.is_open()) return;

        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        file << L"Timestamp: " << std::ctime(&now_c) << L"\n";
        
        for (const auto& proc : processes) {
            file << L"Process: " << proc.name << L" (PID: " << proc.pid << L")\n";
            file << L"CPU History: ";
            for (double cpu : proc.cpuHistory)
                file << cpu << L", ";
            file << L"\nMemory History (MB): ";
            for (SIZE_T mem : proc.memHistory)
                file << (mem / (1024.0 * 1024.0)) << L", ";
            file << L"\n\n";
        }
        file << L"------------------------\n";
        file.close();
    }

public:
    // Constructor
    ProcessMonitor(HWND hwnd) : hWnd(hwnd), lastUpdateTime(0) {
        InitGUI(hwnd);
        UpdateProcessList();
    }

    // Handle user commands like button click or text change
    void HandleCommand(WPARAM wParam) {
        if (LOWORD(wParam) == ID_REFRESH) {
            UpdateProcessList();
            SaveHistoricalData();
        }
        else if (LOWORD(wParam) == ID_ALERT_EDIT && HIWORD(wParam) == EN_CHANGE) {
            WCHAR buffer[32];
            GetWindowTextW(hAlertEdit, buffer, 32);
            cpuAlertThreshold = _wtof(buffer); // Convert string to float
        }
    }

    // Timer-based refresh
    void Refresh() {
        UpdateProcessList();
        SaveHistoricalData();
    }
};

// Main window procedure (event handler)
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static ProcessMonitor* monitor = nullptr;

    switch (msg) {
    case WM_CREATE:
        monitor = new ProcessMonitor(hwnd);
        SetTimer(hwnd, 1, 5000, NULL); // Auto-refresh every 5 seconds
        break;

    case WM_COMMAND:
        if (monitor) monitor->HandleCommand(wParam);
        break;

    case WM_TIMER:
        if (monitor) monitor->Refresh();
        break;

    case WM_DESTROY:
        if (monitor) delete monitor;
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

// Application entry point
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {
    // Register window class
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ProcessMonitor";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassExW(&wc);

    // Create main window
    HWND hwnd = CreateWindowW(wc.lpszClassName, L"Process Monitor",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 620, 400,
        NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Message loop
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
