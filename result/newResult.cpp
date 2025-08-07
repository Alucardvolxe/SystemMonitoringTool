#define _UNICODE
#define UNICODE
#include <windows.h>
#include <psapi.h>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <commctrl.h>
#include <strsafe.h>
#include <chrono>
#include <fstream>
#include <sstream>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "psapi.lib")

#define ID_LISTVIEW 1001
#define ID_HISTORY_LISTVIEW 1007
#define ID_REFRESH 1002
#define ID_ALERT_THRESHOLD 1003
#define ID_ALERT_EDIT 1004
#define ID_TOTAL_CPU 1005
#define ID_TOTAL_MEM 1006
#define MAX_HISTORY 60 // Store 60 seconds of history

struct ProcessInfo {
    DWORD pid;
    std::wstring name;
    double cpuUsage;
    SIZE_T memoryUsage;
    ULONGLONG lastCpuTime;
    std::vector<double> cpuHistory;
    std::vector<SIZE_T> memHistory;
};

class ProcessMonitor {
private:
    HWND hWnd;
    HWND hListView;
    HWND hHistoryListView;
    HWND hRefreshButton;
    HWND hAlertEdit;
    HWND hTotalCpuLabel;
    HWND hTotalMemLabel;
    std::vector<ProcessInfo> processes;
    std::map<DWORD, ULONGLONG> lastSystemTimes;
    double cpuAlertThreshold = 80.0;
    SIZE_T memoryAlertThreshold = 0; // Will be set based on system memory
    ULONGLONG lastUpdateTime;
    double totalCpuUsage = 0.0;
    SIZE_T totalMemoryUsage = 0;

    DWORD GetNumberOfProcessors() {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        return sysInfo.dwNumberOfProcessors;
    }

    SIZE_T GetTotalSystemMemory() {
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(memInfo);
        GlobalMemoryStatusEx(&memInfo);
        return memInfo.ullTotalPhys;
    }

    void InitGUI(HWND hwnd) {
        INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES };
        InitCommonControlsEx(&icex);

        hListView = CreateWindowW(WC_LISTVIEWW, L"", 
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
            10, 10, 580, 200, hwnd, (HMENU)ID_LISTVIEW, GetModuleHandleW(NULL), NULL);

        LVCOLUMNW lvCol = { 0 };
        lvCol.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
        lvCol.cx = 150;
        const wchar_t* processName = L"Process Name";
        lvCol.pszText = const_cast<LPWSTR>(processName);
        ListView_InsertColumn(hListView, 0, &lvCol);
        
        lvCol.cx = 100;
        const wchar_t* pid = L"PID";
        lvCol.pszText = const_cast<LPWSTR>(pid);
        ListView_InsertColumn(hListView, 1, &lvCol);
        
        const wchar_t* cpuUsage = L"CPU Usage (%)";
        lvCol.pszText = const_cast<LPWSTR>(cpuUsage);
        ListView_InsertColumn(hListView, 2, &lvCol);
        
        const wchar_t* memUsage = L"Memory Usage (MB)";
        lvCol.pszText = const_cast<LPWSTR>(memUsage);
        ListView_InsertColumn(hListView, 3, &lvCol);

        hHistoryListView = CreateWindowW(WC_LISTVIEWW, L"", 
            WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SHOWSELALWAYS,
            10, 220, 580, 100, hwnd, (HMENU)ID_HISTORY_LISTVIEW, GetModuleHandleW(NULL), NULL);

        lvCol.cx = 150;
        const wchar_t* avgName = L"Process Name";
        lvCol.pszText = const_cast<LPWSTR>(avgName);
        ListView_InsertColumn(hHistoryListView, 0, &lvCol);
        
        const wchar_t* avgCpu = L"Avg CPU (%)";
        lvCol.pszText = const_cast<LPWSTR>(avgCpu);
        ListView_InsertColumn(hHistoryListView, 1, &lvCol);
        
        const wchar_t* avgMem = L"Avg Memory (MB)";
        lvCol.pszText = const_cast<LPWSTR>(avgMem);
        ListView_InsertColumn(hHistoryListView, 2, &lvCol);

        hRefreshButton = CreateWindowW(L"BUTTON", L"Refresh", 
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            10, 330, 100, 30, hwnd, (HMENU)ID_REFRESH, GetModuleHandleW(NULL), NULL);

        CreateWindowW(L"STATIC", L"CPU Alert Threshold (%):", 
            WS_CHILD | WS_VISIBLE,
            120, 330, 150, 20, hwnd, NULL, GetModuleHandleW(NULL), NULL);
        
        hAlertEdit = CreateWindowW(L"EDIT", L"80.0", 
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            270, 330, 60, 20, hwnd, (HMENU)ID_ALERT_EDIT, GetModuleHandleW(NULL), NULL);

        hTotalCpuLabel = CreateWindowW(L"STATIC", L"Total CPU Usage: 0.00%", 
            WS_CHILD | WS_VISIBLE,
            10, 370, 150, 20, hwnd, (HMENU)ID_TOTAL_CPU, GetModuleHandleW(NULL), NULL);

        hTotalMemLabel = CreateWindowW(L"STATIC", L"Total Memory Usage: 0.00 MB", 
            WS_CHILD | WS_VISIBLE,
            170, 370, 200, 20, hwnd, (HMENU)ID_TOTAL_MEM, GetModuleHandleW(NULL), NULL);

        memoryAlertThreshold = GetTotalSystemMemory() * 0.8; // 80% of total system memory
    }

    void UpdateProcessList() {
        processes.clear();
        ListView_DeleteAllItems(hListView);
        ListView_DeleteAllItems(hHistoryListView);
        totalCpuUsage = 0.0;
        totalMemoryUsage = 0;

        DWORD processesIds[1024], cbNeeded;
        if (!EnumProcesses(processesIds, sizeof(processesIds), &cbNeeded)) return;

        ULONGLONG currentTime;
        FILETIME ftSystem, ftUser, ftKernel;
        GetSystemTimeAsFileTime(&ftSystem);
        currentTime = ((ULONGLONG)ftSystem.dwHighDateTime << 32) | ftSystem.dwLowDateTime;

        DWORD processCount = cbNeeded / sizeof(DWORD);
        for (DWORD i = 0; i < processCount; i++) {
            if (processesIds[i] == 0) continue;

            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processesIds[i]);
            if (hProcess == NULL) continue;

            WCHAR szProcessName[MAX_PATH] = L"<unknown>";
            GetModuleBaseNameW(hProcess, NULL, szProcessName, MAX_PATH);

            FILETIME ftCreate, ftExit, ftKernelTime, ftUserTime;
            if (GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernelTime, &ftUserTime)) {
                ULONGLONG kernel = ((ULONGLONG)ftKernelTime.dwHighDateTime << 32) | ftKernelTime.dwLowDateTime;
                ULONGLONG user = ((ULONGLONG)ftUserTime.dwHighDateTime << 32) | ftUserTime.dwLowDateTime;
                ULONGLONG totalTime = kernel + user;

                double cpuUsage = 0.0;
                auto it = lastSystemTimes.find(processesIds[i]);
                if (it != lastSystemTimes.end()) {
                    ULONGLONG timeDiff = currentTime - lastUpdateTime;
                    ULONGLONG cpuDiff = totalTime - it->second;
                    cpuUsage = (cpuDiff * 100.0) / (timeDiff * GetNumberOfProcessors());
                }

                PROCESS_MEMORY_COUNTERS pmc;
                SIZE_T memoryUsage = 0;
                if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                    memoryUsage = pmc.WorkingSetSize;
                }

                ProcessInfo info;
                info.pid = processesIds[i];
                info.name = szProcessName;
                info.cpuUsage = cpuUsage;
                info.memoryUsage = memoryUsage;
                info.lastCpuTime = totalTime;
                
                info.cpuHistory.push_back(cpuUsage);
                info.memHistory.push_back(memoryUsage);
                if (info.cpuHistory.size() > MAX_HISTORY) {
                    info.cpuHistory.erase(info.cpuHistory.begin());
                    info.memHistory.erase(info.memHistory.begin());
                }

                processes.push_back(info);
                lastSystemTimes[processesIds[i]] = totalTime;

                totalCpuUsage += cpuUsage;
                totalMemoryUsage += memoryUsage;

                if (cpuUsage > cpuAlertThreshold || totalMemoryUsage > memoryAlertThreshold) {
                    WCHAR alertMsg[256];
                    StringCchPrintfW(alertMsg, 256, L"Alert: %s (PID: %d) - CPU: %.2f%%, Total CPU: %.2f%%, Total Mem: %.2f MB (Threshold Exceeded)",
                        szProcessName, processesIds[i], cpuUsage, totalCpuUsage, totalMemoryUsage / (1024.0 * 1024.0));
                    MessageBoxW(hWnd, alertMsg, L"Usage Alert", MB_OK | MB_ICONWARNING);
                }
            }

            CloseHandle(hProcess);
        }

        lastUpdateTime = currentTime;
        UpdateListView();
        UpdateHistoryListView();
        UpdateTotalUsage();
    }

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

    void UpdateHistoryListView() {
        ListView_DeleteAllItems(hHistoryListView);
        
        for (size_t i = 0; i < processes.size(); i++) {
            LVITEMW lvItem = { 0 };
            lvItem.mask = LVIF_TEXT;
            lvItem.iItem = (int)i;

            WCHAR buffer[256];
            lvItem.pszText = (LPWSTR)processes[i].name.c_str();
            ListView_InsertItem(hHistoryListView, &lvItem);

            double avgCpu = 0.0;
            for (double cpu : processes[i].cpuHistory) avgCpu += cpu;
            avgCpu /= processes[i].cpuHistory.size();
            StringCchPrintfW(buffer, 256, L"%.2f", avgCpu);
            ListView_SetItemText(hHistoryListView, i, 1, buffer);

            SIZE_T avgMem = 0;
            for (SIZE_T mem : processes[i].memHistory) avgMem += mem;
            avgMem /= processes[i].memHistory.size();
            StringCchPrintfW(buffer, 256, L"%.2f", avgMem / (1024.0 * 1024.0));
            ListView_SetItemText(hHistoryListView, i, 2, buffer);
        }
    }

    void UpdateTotalUsage() {
        WCHAR buffer[256];
        StringCchPrintfW(buffer, 256, L"Total CPU Usage: %.2f%%", totalCpuUsage);
        SetWindowTextW(hTotalCpuLabel, buffer);

        StringCchPrintfW(buffer, 256, L"Total Memory Usage: %.2f MB", totalMemoryUsage / (1024.0 * 1024.0));
        SetWindowTextW(hTotalMemLabel, buffer);
    }

    void ResizeControls(int width, int height) {
        MoveWindow(hListView, 10, 10, width - 20, 200, TRUE);
        MoveWindow(hHistoryListView, 10, 220, width - 20, 100, TRUE);
        MoveWindow(hRefreshButton, 10, height - 70, 100, 30, TRUE);
        MoveWindow(GetDlgItem(hWnd, 0), 120, height - 70, 150, 20, TRUE); // CPU Alert Threshold label
        MoveWindow(hAlertEdit, 270, height - 70, 60, 20, TRUE);
        MoveWindow(hTotalCpuLabel, 10, height - 40, 150, 20, TRUE);
        MoveWindow(hTotalMemLabel, 170, height - 40, 200, 20, TRUE);
    }

    void SaveHistoricalData() {
        std::wofstream file(L"process_history.txt", std::ios::app);
        if (!file.is_open()) return;

        auto now = std::chrono::system_clock::now();
        auto now_c = std::chrono::system_clock::to_time_t(now);
        file << L"Timestamp: " << std::ctime(&now_c) << L"\n";
        file << L"Total CPU Usage: " << totalCpuUsage << L"%\n";
        file << L"Total Memory Usage: " << (totalMemoryUsage / (1024.0 * 1024.0)) << L" MB\n";
        
        for (const auto& proc : processes) {
            file << L"Process: " << proc.name << L" (PID: " << proc.pid << L")\n";
            file << L"CPU History: ";
            for (double cpu : proc.cpuHistory) {
                file << cpu << L", ";
            }
            file << L"\nMemory History (MB): ";
            for (SIZE_T mem : proc.memHistory) {
                file << (mem / (1024.0 * 1024.0)) << L", ";
            }
            file << L"\n\n";
        }
        file << L"------------------------\n";
        file.close();
    }

public:
    ProcessMonitor(HWND hwnd) : hWnd(hwnd), lastUpdateTime(0) {
        InitGUI(hwnd);
        UpdateProcessList();
    }

    void HandleCommand(WPARAM wParam) {
        if (LOWORD(wParam) == ID_REFRESH) {
            UpdateProcessList();
            SaveHistoricalData();
        }
        else if (LOWORD(wParam) == ID_ALERT_EDIT && HIWORD(wParam) == EN_CHANGE) {
            WCHAR buffer[32];
            GetWindowTextW(hAlertEdit, buffer, 32);
            cpuAlertThreshold = _wtof(buffer);
        }
    }

    void HandleResize(WPARAM wParam, LPARAM lParam) {
        int width = LOWORD(lParam);
        int height = HIWORD(lParam);
        ResizeControls(width, height);
    }

    void Refresh() {
        UpdateProcessList();
        SaveHistoricalData();
    }
};

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static ProcessMonitor* monitor = nullptr;

    switch (msg) {
    case WM_CREATE:
        monitor = new ProcessMonitor(hwnd);
        break;

    case WM_COMMAND:
        if (monitor) monitor->HandleCommand(wParam);
        break;

    case WM_SIZE:
        if (monitor) monitor->HandleResize(wParam, lParam);
        break;

    case WM_DESTROY:
        if (monitor) delete monitor;
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ProcessMonitor";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowW(wc.lpszClassName, L"Process Monitor",
        WS_OVERLAPPEDWINDOW | WS_THICKFRAME, CW_USEDEFAULT, CW_USEDEFAULT, 620, 450,
        NULL, NULL, hInstance, NULL);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}