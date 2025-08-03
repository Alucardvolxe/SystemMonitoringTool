// system_monitor.h
#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include <deque>
#include <unordered_map>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
    #include <windows.h>
    #include <psapi.h>
    #include <tlhelp32.h>
    #include <pdh.h>
    #include <pdhmsg.h>
    #pragma comment(lib, "pdh.lib")
    #pragma comment(lib, "psapi.lib")
#elif __linux__
    #include <unistd.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <sys/sysinfo.h>
    #include <dirent.h>
    #include <fstream>
    #include <signal.h>
#elif __APPLE__
    #include <sys/types.h>
    #include <sys/sysctl.h>
    #include <sys/mount.h>
    #include <mach/mach.h>
    #include <mach/processor_info.h>
    #include <mach/mach_host.h>
    #include <signal.h>
#endif

namespace SystemMonitor {

// Forward declarations
class ProcessInfo;
class SystemStats;
class DataCollector;
class AlertSystem;

// Process information structure
class ProcessInfo {
public:
    uint32_t pid;
    std::string name;
    double cpu_percent;
    double memory_percent;
    uint64_t memory_bytes;
    uint64_t io_read_bytes;
    uint64_t io_write_bytes;
    std::string status;
    std::chrono::system_clock::time_point create_time;
    
    ProcessInfo() : pid(0), cpu_percent(0.0), memory_percent(0.0), 
                   memory_bytes(0), io_read_bytes(0), io_write_bytes(0) {}
    
    ProcessInfo(uint32_t p, const std::string& n) 
        : pid(p), name(n), cpu_percent(0.0), memory_percent(0.0),
          memory_bytes(0), io_read_bytes(0), io_write_bytes(0) {}
};

// System-wide statistics
class SystemStats {
public:
    std::chrono::system_clock::time_point timestamp;
    double cpu_percent;
    double memory_percent;
    uint64_t memory_used_bytes;
    uint64_t memory_total_bytes;
    double disk_io_read_rate;  // bytes per second
    double disk_io_write_rate; // bytes per second
    uint32_t process_count;
    
    SystemStats() : cpu_percent(0.0), memory_percent(0.0), 
                   memory_used_bytes(0), memory_total_bytes(0),
                   disk_io_read_rate(0.0), disk_io_write_rate(0.0),
                   process_count(0) {
        timestamp = std::chrono::system_clock::now();
    }
};

// Cross-platform system information collector
class SystemInfoCollector {
private:
    struct IOCounters {
        uint64_t read_bytes = 0;
        uint64_t write_bytes = 0;
        std::chrono::system_clock::time_point timestamp;
    };
    
    IOCounters prev_io_counters_;
    bool first_io_read_ = true;
    
#ifdef _WIN32
    PDH_HQUERY cpu_query_;
    PDH_HCOUNTER cpu_counter_;
    bool pdh_initialized_ = false;
#elif __linux__
    struct CPUTimes {
        uint64_t user, nice, system, idle, iowait, irq, softirq, steal;
    };
    CPUTimes prev_cpu_times_;
    bool first_cpu_read_ = true;
#endif

public:
    SystemInfoCollector();
    ~SystemInfoCollector();
    
    bool initialize();
    SystemStats getSystemStats();
    std::vector<ProcessInfo> getProcessList();
    bool killProcess(uint32_t pid);
    
private:
    double getCPUUsage();
    void getMemoryInfo(uint64_t& used, uint64_t& total);
    void getIOStats(double& read_rate, double& write_rate);
    
#ifdef _WIN32
    std::vector<ProcessInfo> getProcessListWindows();
    double getCPUUsageWindows();
#elif __linux__
    std::vector<ProcessInfo> getProcessListLinux();
    double getCPUUsageLinux();
    bool readCPUTimes(CPUTimes& times);
#elif __APPLE__
    std::vector<ProcessInfo> getProcessListMacOS();
    double getCPUUsageMacOS();
#endif
};

// Alert system for monitoring thresholds
class AlertSystem {
public:
    struct AlertThresholds {
        double cpu_threshold = 80.0;
        double memory_threshold = 85.0;
        double io_threshold = 100.0; // MB/s
        std::chrono::seconds cooldown_period{60};
    };
    
    struct Alert {
        std::string message;
        std::chrono::system_clock::time_point timestamp;
        enum Type { CPU, MEMORY, IO, PROCESS } type;
    };

private:
    AlertThresholds thresholds_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> last_alerts_;
    std::function<void(const Alert&)> alert_callback_;
    mutable std::mutex mutex_;

public:
    AlertSystem() = default;
    
    void setThresholds(const AlertThresholds& thresholds);
    AlertThresholds getThresholds() const;
    void setAlertCallback(std::function<void(const Alert&)> callback);
    
    std::vector<Alert> checkAlerts(const SystemStats& stats, 
                                  const std::vector<ProcessInfo>& processes);

private:
    bool shouldAlert(const std::string& alert_key);
};

// Historical data collector and storage
class DataCollector {
private:
    std::deque<SystemStats> system_history_;
    std::unordered_map<uint32_t, std::deque<ProcessInfo>> process_history_;
    size_t max_history_size_;
    
    std::unique_ptr<SystemInfoCollector> info_collector_;
    std::unique_ptr<AlertSystem> alert_system_;
    
    std::thread collection_thread_;
    std::atomic<bool> collecting_;
    std::chrono::milliseconds collection_interval_;
    
    mutable std::mutex data_mutex_;
    std::function<void(const SystemStats&, const std::vector<ProcessInfo>&)> data_callback_;

public:
    DataCollector(size_t max_history = 1000, 
                  std::chrono::milliseconds interval = std::chrono::milliseconds(1000));
    ~DataCollector();
    
    bool initialize();
    void startCollection();
    void stopCollection();
    
    // Data access
    SystemStats getLatestSystemStats() const;
    std::vector<ProcessInfo> getLatestProcessList() const;
    std::vector<SystemStats> getSystemHistory(size_t count = 0) const;
    
    // Configuration
    void setDataCallback(std::function<void(const SystemStats&, const std::vector<ProcessInfo>&)> callback);
    void setAlertCallback(std::function<void(const AlertSystem::Alert&)> callback);
    void setAlertThresholds(const AlertSystem::AlertThresholds& thresholds);
    
    // Process management
    bool killProcess(uint32_t pid);
    
    // Data persistence
    bool saveHistoryToFile(const std::string& filename) const;
    bool loadHistoryFromFile(const std::string& filename);
    void clearHistory();

private:
    void collectionLoop();
    std::string formatTimestamp(const std::chrono::system_clock::time_point& tp) const;
};

} // namespace SystemMonitor

// system_monitor.cpp
#include "system_monitor.h"
#include <algorithm>
#include <json/json.h> // You'll need jsoncpp library

namespace SystemMonitor {

// SystemInfoCollector Implementation
SystemInfoCollector::SystemInfoCollector() {
#ifdef _WIN32
    cpu_query_ = nullptr;
    cpu_counter_ = nullptr;
#endif
}

SystemInfoCollector::~SystemInfoCollector() {
#ifdef _WIN32
    if (pdh_initialized_) {
        if (cpu_counter_) PdhRemoveCounter(cpu_counter_);
        if (cpu_query_) PdhCloseQuery(cpu_query_);
    }
#endif
}

bool SystemInfoCollector::initialize() {
#ifdef _WIN32
    PDH_STATUS status = PdhOpenQuery(nullptr, 0, &cpu_query_);
    if (status != ERROR_SUCCESS) return false;
    
    status = PdhAddEnglishCounter(cpu_query_, L"\\Processor(_Total)\\% Processor Time", 0, &cpu_counter_);
    if (status != ERROR_SUCCESS) {
        PdhCloseQuery(cpu_query_);
        return false;
    }
    
    // Initial query to establish baseline
    PdhCollectQueryData(cpu_query_);
    pdh_initialized_ = true;
#endif
    return true;
}

SystemStats SystemInfoCollector::getSystemStats() {
    SystemStats stats;
    stats.timestamp = std::chrono::system_clock::now();
    
    stats.cpu_percent = getCPUUsage();
    getMemoryInfo(stats.memory_used_bytes, stats.memory_total_bytes);
    
    if (stats.memory_total_bytes > 0) {
        stats.memory_percent = (static_cast<double>(stats.memory_used_bytes) / 
                               stats.memory_total_bytes) * 100.0;
    }
    
    getIOStats(stats.disk_io_read_rate, stats.disk_io_write_rate);
    
    // Get process count
    auto processes = getProcessList();
    stats.process_count = static_cast<uint32_t>(processes.size());
    
    return stats;
}

std::vector<ProcessInfo> SystemInfoCollector::getProcessList() {
#ifdef _WIN32
    return getProcessListWindows();
#elif __linux__
    return getProcessListLinux();
#elif __APPLE__
    return getProcessListMacOS();
#else
    return std::vector<ProcessInfo>(); // Unsupported platform
#endif
}

bool SystemInfoCollector::killProcess(uint32_t pid) {
#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (hProcess == nullptr) return false;
    
    BOOL result = TerminateProcess(hProcess, 1);
    CloseHandle(hProcess);
    return result != 0;
#else
    return kill(pid, SIGTERM) == 0;
#endif
}

#ifdef _WIN32
std::vector<ProcessInfo> SystemInfoCollector::getProcessListWindows() {
    std::vector<ProcessInfo> processes;
    
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return processes;
    
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    
    if (Process32First(hSnapshot, &pe32)) {
        do {
            ProcessInfo proc(pe32.th32ProcessID, pe32.szExeName);
            
            // Get detailed process information
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 
                                        FALSE, pe32.th32ProcessID);
            if (hProcess) {
                PROCESS_MEMORY_COUNTERS_EX pmc;
                if (GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc))) {
                    proc.memory_bytes = pmc.WorkingSetSize;
                }
                
                FILETIME createTime, exitTime, kernelTime, userTime;
                if (GetProcessTimes(hProcess, &createTime, &exitTime, &kernelTime, &userTime)) {
                    // Convert FILETIME to time_point (simplified)
                    ULARGE_INTEGER uli;
                    uli.LowPart = createTime.dwLowDateTime;
                    uli.HighPart = createTime.dwHighDateTime;
                    
                    // Convert from 100-nanosecond intervals since 1601 to system_clock
                    auto duration = std::chrono::duration_cast<std::chrono::system_clock::duration>(
                        std::chrono::nanoseconds((uli.QuadPart - 116444736000000000ULL) * 100));
                    proc.create_time = std::chrono::system_clock::time_point(duration);
                }
                
                CloseHandle(hProcess);
            }
            
            processes.push_back(proc);
            
        } while (Process32Next(hSnapshot, &pe32));
    }
    
    CloseHandle(hSnapshot);
    return processes;
}

double SystemInfoCollector::getCPUUsageWindows() {
    if (!pdh_initialized_) return 0.0;
    
    PDH_STATUS status = PdhCollectQueryData(cpu_query_);
    if (status != ERROR_SUCCESS) return 0.0;
    
    PDH_FMT_COUNTERVALUE counterVal;
    status = PdhGetFormattedCounterValue(cpu_counter_, PDH_FMT_DOUBLE, nullptr, &counterVal);
    if (status != ERROR_SUCCESS) return 0.0;
    
    return counterVal.doubleValue;
}

#elif __linux__

std::vector<ProcessInfo> SystemInfoCollector::getProcessListLinux() {
    std::vector<ProcessInfo> processes;
    
    DIR* proc_dir = opendir("/proc");
    if (!proc_dir) return processes;
    
    struct dirent* entry;
    while ((entry = readdir(proc_dir)) != nullptr) {
        // Check if directory name is a number (PID)
        char* endptr;
        uint32_t pid = strtoul(entry->d_name, &endptr, 10);
        if (*endptr != '\0') continue; // Not a number
        
        ProcessInfo proc(pid, "");
        
        // Read process name from /proc/PID/comm
        std::string comm_path = "/proc/" + std::string(entry->d_name) + "/comm";
        std::ifstream comm_file(comm_path);
        if (comm_file.is_open()) {
            std::getline(comm_file, proc.name);
            comm_file.close();
        }
        
        // Read memory information from /proc/PID/status
        std::string status_path = "/proc/" + std::string(entry->d_name) + "/status";
        std::ifstream status_file(status_path);
        if (status_file.is_open()) {
            std::string line;
            while (std::getline(status_file, line)) {
                if (line.substr(0, 6) == "VmRSS:") {
                    std::istringstream iss(line);
                    std::string label, value, unit;
                    iss >> label >> value >> unit;
                    proc.memory_bytes = std::stoull(value) * 1024; // Convert kB to bytes
                    break;
                }
            }
            status_file.close();
        }
        
        // Read I/O statistics from /proc/PID/io
        std::string io_path = "/proc/" + std::string(entry->d_name) + "/io";
        std::ifstream io_file(io_path);
        if (io_file.is_open()) {
            std::string line;
            while (std::getline(io_file, line)) {
                if (line.substr(0, 10) == "read_bytes") {
                    proc.io_read_bytes = std::stoull(line.substr(12));
                } else if (line.substr(0, 11) == "write_bytes") {
                    proc.io_write_bytes = std::stoull(line.substr(13));
                }
            }
            io_file.close();
        }
        
        proc.status = "running"; // Simplified status
        processes.push_back(proc);
    }
    
    closedir(proc_dir);
    return processes;
}

double SystemInfoCollector::getCPUUsageLinux() {
    CPUTimes current_times;
    if (!readCPUTimes(current_times)) return 0.0;
    
    if (first_cpu_read_) {
        prev_cpu_times_ = current_times;
        first_cpu_read_ = false;
        return 0.0;
    }
    
    uint64_t prev_idle = prev_cpu_times_.idle + prev_cpu_times_.iowait;
    uint64_t idle = current_times.idle + current_times.iowait;
    
    uint64_t prev_non_idle = prev_cpu_times_.user + prev_cpu_times_.nice + 
                            prev_cpu_times_.system + prev_cpu_times_.irq + 
                            prev_cpu_times_.softirq + prev_cpu_times_.steal;
    uint64_t non_idle = current_times.user + current_times.nice + 
                       current_times.system + current_times.irq + 
                       current_times.softirq + current_times.steal;
    
    uint64_t prev_total = prev_idle + prev_non_idle;
    uint64_t total = idle + non_idle;
    
    uint64_t total_diff = total - prev_total;
    uint64_t idle_diff = idle - prev_idle;
    
    prev_cpu_times_ = current_times;
    
    if (total_diff == 0) return 0.0;
    
    return ((double)(total_diff - idle_diff) / total_diff) * 100.0;
}

bool SystemInfoCollector::readCPUTimes(CPUTimes& times) {
    std::ifstream stat_file("/proc/stat");
    if (!stat_file.is_open()) return false;
    
    std::string line;
    std::getline(stat_file, line);
    stat_file.close();
    
    std::istringstream iss(line);
    std::string cpu_label;
    iss >> cpu_label >> times.user >> times.nice >> times.system >> times.idle
        >> times.iowait >> times.irq >> times.softirq >> times.steal;
    
    return true;
}

#endif

double SystemInfoCollector::getCPUUsage() {
#ifdef _WIN32
    return getCPUUsageWindows();
#elif __linux__
    return getCPUUsageLinux();
#elif __APPLE__
    return getCPUUsageMacOS();
#else
    return 0.0;
#endif
}

void SystemInfoCollector::getMemoryInfo(uint64_t& used, uint64_t& total) {
#ifdef _WIN32
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    
    total = memInfo.ullTotalPhys;
    used = total - memInfo.ullAvailPhys;
    
#elif __linux__
    struct sysinfo si;
    if (sysinfo(&si) == 0) {
        total = si.totalram * si.mem_unit;
        used = (si.totalram - si.freeram) * si.mem_unit;
    } else {
        total = used = 0;
    }
    
#elif __APPLE__
    int mib[2];
    size_t length;
    
    // Get total memory
    mib[0] = CTL_HW;
    mib[1] = HW_MEMSIZE;
    length = sizeof(uint64_t);
    sysctl(mib, 2, &total, &length, nullptr, 0);
    
    // Get used memory (simplified - using vm_stat would be more accurate)
    vm_size_t page_size;
    vm_statistics64_data_t vm_stat;
    mach_msg_type_number_t host_size = sizeof(vm_statistics64_data_t) / sizeof(natural_t);
    
    host_page_size(mach_host_self(), &page_size);
    host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm_stat, &host_size);
    
    used = (vm_stat.active_count + vm_stat.inactive_count + vm_stat.wire_count) * page_size;
#else
    total = used = 0;
#endif
}

void SystemInfoCollector::getIOStats(double& read_rate, double& write_rate) {
    auto now = std::chrono::system_clock::now();
    IOCounters current_io;
    current_io.timestamp = now;
    
#ifdef _WIN32
    // Windows implementation would use performance counters
    // Simplified for this example
    current_io.read_bytes = 0;
    current_io.write_bytes = 0;
    
#elif __linux__
    std::ifstream diskstats("/proc/diskstats");
    if (diskstats.is_open()) {
        std::string line;
        uint64_t total_read = 0, total_write = 0;
        
        while (std::getline(diskstats, line)) {
            std::istringstream iss(line);
            std::vector<std::string> tokens;
            std::string token;
            while (iss >> token) tokens.push_back(token);
            
            if (tokens.size() >= 14) {
                // Sectors read/written are in positions 5 and 9 (0-indexed)
                total_read += std::stoull(tokens[5]) * 512; // Convert sectors to bytes
                total_write += std::stoull(tokens[9]) * 512;
            }
        }
        diskstats.close();
        
        current_io.read_bytes = total_read;
        current_io.write_bytes = total_write;
    }
#endif

    if (!first_io_read_) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - prev_io_counters_.timestamp).count();
        
        if (duration > 0) {
            read_rate = (double)(current_io.read_bytes - prev_io_counters_.read_bytes) * 1000.0 / duration;
            write_rate = (double)(current_io.write_bytes - prev_io_counters_.write_bytes) * 1000.0 / duration;
        } else {
            read_rate = write_rate = 0.0;
        }
    } else {
        read_rate = write_rate = 0.0;
        first_io_read_ = false;
    }
    
    prev_io_counters_ = current_io;
}

// AlertSystem Implementation
void AlertSystem::setThresholds(const AlertThresholds& thresholds) {
    std::lock_guard<std::mutex> lock(mutex_);
    thresholds_ = thresholds;
}

AlertSystem::AlertThresholds AlertSystem::getThresholds() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return thresholds_;
}

void AlertSystem::setAlertCallback(std::function<void(const Alert&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    alert_callback_ = callback;
}

std::vector<AlertSystem::Alert> AlertSystem::checkAlerts(
    const SystemStats& stats, const std::vector<ProcessInfo>& processes) {
    
    std::vector<Alert> alerts;
    
    // Check CPU usage
    if (stats.cpu_percent > thresholds_.cpu_threshold) {
        if (shouldAlert("cpu")) {
            Alert alert;
            alert.type = Alert::CPU;
            alert.timestamp = std::chrono::system_clock::now();
            alert.message = "High CPU usage: " + std::to_string(stats.cpu_percent) + "%";
            alerts.push_back(alert);
            
            if (alert_callback_) alert_callback_(alert);
        }
    }
    
    // Check memory usage
    if (stats.memory_percent > thresholds_.memory_threshold) {
        if (shouldAlert("memory")) {
            Alert alert;
            alert.type = Alert::MEMORY;
            alert.timestamp = std::chrono::system_clock::now();
            alert.message = "High memory usage: " + std::to_string(stats.memory_percent) + "%";
            alerts.push_back(alert);
            
            if (alert_callback_) alert_callback_(alert);
        }
    }
    
    // Check I/O rates
    double total_io = (stats.disk_io_read_rate + stats.disk_io_write_rate) / (1024 * 1024); // Convert to MB/s
    if (total_io > thresholds_.io_threshold) {
        if (shouldAlert("io")) {
            Alert alert;
            alert.type = Alert::IO;
            alert.timestamp = std::chrono::system_clock::now();
            alert.message = "High I/O activity: " + std::to_string(total_io) + " MB/s";
            alerts.push_back(alert);
            
            if (alert_callback_) alert_callback_(alert);
        }
    }
    
    // Check individual processes
    for (const auto& proc : processes) {
        if (proc.cpu_percent > 50.0) { // High CPU usage by single process
            std::string alert_key = "process_" + std::to_string(proc.pid);
            if (shouldAlert(alert_key)) {
                Alert alert;
                alert.type = Alert::PROCESS;
                alert.timestamp = std::chrono::system_clock::now();
                alert.message = "Process " + proc.name + " (PID " + std::to_string(proc.pid) + 
                               ") using " + std::to_string(proc.cpu_percent) + "% CPU";
                alerts.push_back(alert);
                
                if (alert_callback_) alert_callback_(alert);
            }
        }
    }
    
    return alerts;
}

bool AlertSystem::shouldAlert(const std::string& alert_key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::system_clock::now();
    
    auto it = last_alerts_.find(alert_key);
    if (it == last_alerts_.end() || 
        (now - it->second) >= thresholds_.cooldown_period) {
        last_alerts_[alert_key] = now;
        return true;
    }
    
    return false;
}

// DataCollector Implementation
DataCollector::DataCollector(size_t max_history, std::chrono::milliseconds interval)
    : max_history_size_(max_history), collection_interval_(interval), collecting_(false) {
    
    info_collector_ = std::make_unique<SystemInfoCollector>();
    alert_system_ = std::make_unique<AlertSystem>();
}

DataCollector::~DataCollector() {
    stopCollection();
}

bool DataCollector::initialize() {
    return info_collector_->initialize();
}

void DataCollector::startCollection() {
    if (collecting_.load()) return;
    
    collecting_ = true;
    collection_thread_ = std::thread(&DataCollector::collectionLoop, this);
}

void DataCollector::stopCollection() {
    collecting_ = false;
    if (collection_thread_.joinable()) {
        collection_thread_.join();
    }
}

SystemStats DataCollector::getLatestSystemStats() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return system_history_.empty() ? SystemStats() : system_history_.back();
}

std::vector<ProcessInfo> DataCollector::getLatestProcessList() const {
    // This would need to be stored separately for thread safety
    // For now, get fresh data
    return info_collector_->getProcessList();
}

std::vector<SystemStats> DataCollector::getSystemHistory(size_t count) const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    
    if (count == 0 || count >= system_history_.size()) {
        return std::vector<SystemStats>(system_history_.begin(), system_history_.end());
    }
    
    auto start = system_history_.end() - count;
    return std::vector<SystemStats>(start, system_history_.end());
}

void DataCollector::setDataCallback(
    std::function<void(const SystemStats&, const std::vector<ProcessInfo>&)> callback) {
    data_callback_ = callback;
}

void DataCollector::setAlertCallback(std::function<void(const AlertSystem::Alert&)> callback) {
    alert_system_->setAlertCallback(callback);
}

void DataCollector::setAlertThresholds(const AlertSystem::AlertThresholds& thresholds) {
    alert_system_->setThresholds(thresholds);
}

bool DataCollector::killProcess(uint32_t pid) {
    return info_collector_->killProcess(pid);
}

void DataCollector::collectionLoop() {
    while (collecting_.load()) {
        try {
            // Collect system statistics
            SystemStats stats = info_collector_->getSystemStats();
            std::vector<ProcessInfo> processes = info_collector_->getProcessList();
            
            // Calculate CPU percentages for processes (simplified)
            uint64_t total_memory = stats.memory_total_bytes;
            for (auto& proc : processes) {
                if (total_memory > 0) {
                    proc.memory_percent = (static_cast<double>(proc.memory_bytes) / total_memory) * 100.0;
                }
            }
            
            // Sort processes by CPU usage
            std::sort(processes.begin(), processes.end(), 
                     [](const ProcessInfo& a, const ProcessInfo& b) {
                         return a.cpu_percent > b.cpu_percent;
                     });
            
            // Store data
            {
                std::lock_guard<std::mutex> lock(data_mutex_);
                system_history_.push_back(stats);
                if (system_history_.size() > max_history_size_) {
                    system_history_.pop_front();
                }
            }
            
            // Check for alerts
            alert_system_->checkAlerts(stats, processes);
            
            // Notify callback
            if (data_callback_) {
                data_callback_(stats, processes);
            }
            
        } catch (const std::exception& e) {
            std::cerr << "Error in collection loop: " << e.what() << std::endl;
        }
        
        std::this_thread::sleep_for(collection_interval_);
    }
}

bool DataCollector::saveHistoryToFile(const std::string& filename) const {
    try {
        std::lock_guard<std::mutex> lock(data_mutex_);
        
        Json::Value root;
        Json::Value history_array(Json::arrayValue);
        
        for (const auto& stats : system_history_) {
            Json::Value entry;
            entry["timestamp"] = formatTimestamp(stats.timestamp);
            entry["cpu_percent"] = stats.cpu_percent;
            entry["memory_percent"] = stats.memory_percent;
            entry["memory_used_bytes"] = static_cast<Json::UInt64>(stats.memory_used_bytes);
            entry["memory_total_bytes"] = static_cast<Json::UInt64>(stats.memory_total_bytes);
            entry["disk_io_read_rate"] = stats.disk_io_read_rate;
            entry["disk_io_write_rate"] = stats.disk_io_write_rate;
            entry["process_count"] = stats.process_count;
            
            history_array.append(entry);
        }
        
        root["system_history"] = history_array;
        root["export_timestamp"] = formatTimestamp(std::chrono::system_clock::now());
        
        std::ofstream file(filename);
        if (!file.is_open()) return false;
        
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "  ";
        std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
        writer->write(root, &file);
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error saving history: " << e.what() << std::endl;
        return false;
    }
}

bool DataCollector::loadHistoryFromFile(const std::string& filename) {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) return false;
        
        Json::Value root;
        Json::CharReaderBuilder builder;
        std::string errors;
        
        if (!Json::parseFromStream(builder, file, &root, &errors)) {
            std::cerr << "JSON parse error: " << errors << std::endl;
            return false;
        }
        
        std::lock_guard<std::mutex> lock(data_mutex_);
        system_history_.clear();
        
        const Json::Value& history_array = root["system_history"];
        for (const auto& entry : history_array) {
            SystemStats stats;
            // Note: timestamp parsing would need proper implementation
            stats.cpu_percent = entry["cpu_percent"].asDouble();
            stats.memory_percent = entry["memory_percent"].asDouble();
            stats.memory_used_bytes = entry["memory_used_bytes"].asUInt64();
            stats.memory_total_bytes = entry["memory_total_bytes"].asUInt64();
            stats.disk_io_read_rate = entry["disk_io_read_rate"].asDouble();
            stats.disk_io_write_rate = entry["disk_io_write_rate"].asDouble();
            stats.process_count = entry["process_count"].asUInt();
            
            system_history_.push_back(stats);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading history: " << e.what() << std::endl;
        return false;
    }
}

void DataCollector::clearHistory() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    system_history_.clear();
}

std::string DataCollector::formatTimestamp(const std::chrono::system_clock::time_point& tp) const {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

} // namespace SystemMonitor

// Example usage and demo
int main() {
    using namespace SystemMonitor;
    
    std::cout << "=== System Monitor C++ Demo ===" << std::endl;
    
    DataCollector collector;
    
    if (!collector.initialize()) {
        std::cerr << "Failed to initialize system monitor" << std::endl;
        return 1;
    }
    
    // Set up alert callback
    collector.setAlertCallback([](const AlertSystem::Alert& alert) {
        std::cout << "ALERT: " << alert.message << std::endl;
    });
    
    // Set up data callback for real-time display
    collector.setDataCallback([](const SystemStats& stats, const std::vector<ProcessInfo>& processes) {
        static int counter = 0;
        if (++counter % 5 == 0) { // Display every 5 seconds
            std::cout << "\n=== System Statistics ===" << std::endl;
            std::cout << "CPU Usage: " << std::fixed << std::setprecision(1) 
                      << stats.cpu_percent << "%" << std::endl;
            std::cout << "Memory Usage: " << stats.memory_percent << "% ("
                      << stats.memory_used_bytes / (1024*1024) << " MB / "
                      << stats.memory_total_bytes / (1024*1024) << " MB)" << std::endl;
            std::cout << "Disk I/O: Read " << stats.disk_io_read_rate / (1024*1024) 
                      << " MB/s, Write " << stats.disk_io_write_rate / (1024*1024) << " MB/s" << std::endl;
            std::cout << "Active Processes: " << stats.process_count << std::endl;
            
            std::cout << "\n--- Top Processes by CPU ---" << std::endl;
            std::cout << std::left << std::setw(8) << "PID" 
                      << std::setw(20) << "Name" 
                      << std::setw(8) << "CPU%" 
                      << std::setw(10) << "Memory%" 
                      << std::setw(12) << "Memory(MB)" << std::endl;
            std::cout << std::string(58, '-') << std::endl;
            
            int count = 0;
            for (const auto& proc : processes) {
                if (count++ >= 10) break; // Show top 10
                std::cout << std::left << std::setw(8) << proc.pid
                          << std::setw(20) << (proc.name.length() > 18 ? proc.name.substr(0, 15) + "..." : proc.name)
                          << std::setw(8) << std::fixed << std::setprecision(1) << proc.cpu_percent
                          << std::setw(10) << std::fixed << std::setprecision(1) << proc.memory_percent
                          << std::setw(12) << proc.memory_bytes / (1024*1024) << std::endl;
            }
        }
    });
    
    // Set alert thresholds
    AlertSystem::AlertThresholds thresholds;
    thresholds.cpu_threshold = 80.0;
    thresholds.memory_threshold = 85.0;
    thresholds.io_threshold = 100.0; // MB/s
    collector.setAlertThresholds(thresholds);
    
    // Start collection
    std::cout << "Starting system monitoring... (Press Enter to stop)" << std::endl;
    collector.startCollection();
    
    // Wait for user input
    std::cin.get();
    
    // Stop collection
    std::cout << "Stopping system monitoring..." << std::endl;
    collector.stopCollection();
    
    // Save history
    std::cout << "Saving history to file..." << std::endl;
    if (collector.saveHistoryToFile("system_monitor_history.json")) {
        std::cout << "History saved successfully." << std::endl;
    } else {
        std::cout << "Failed to save history." << std::endl;
    }
    
    // Display final statistics
    auto final_stats = collector.getLatestSystemStats();
    auto history = collector.getSystemHistory(10); // Last 10 entries
    
    std::cout << "\n=== Final Statistics ===" << std::endl;
    std::cout << "Last CPU Usage: " << final_stats.cpu_percent << "%" << std::endl;
    std::cout << "Last Memory Usage: " << final_stats.memory_percent << "%" << std::endl;
    std::cout << "History entries collected: " << history.size() << std::endl;
    
    return 0;
}