# Process Monitor

## Overview

Process Monitor is a Windows-based application built in C++ to monitor CPU and memory usage of running processes in real-time. It provides a graphical interface to view process details, historical usage data, and system-wide resource metrics, with configurable alerts and data logging capabilities.

## Key Features

- Displays real-time process information (Name, PID, CPU Usage, Memory Usage).
- Tracks 60-second history of CPU and memory usage with average calculations.
- Shows total system CPU and memory usage.
- Configurable CPU usage alerts (default: 80%) and automatic memory alerts (80% of system memory).
- Saves historical data to `process_history.txt`.
- Responsive UI that adjusts to window resizing.

## Getting Started

### Prerequisites

- Windows OS (tested on Windows 10/11)
- C++ compiler (e.g., MSVC) with Windows API support
- Libraries: `user32.lib`, `comctl32.lib`, `psapi.lib`

### Installation

1. Clone or download the repository containing `ProcessMonitor.cpp`.
2. Compile the source code using a C++ compiler (e.g., MSVC) with the required libraries.
3. Run the generated `.exe` file to launch the application.

### Usage

1. Launch the application to view the process list and historical data.
2. Click the "Refresh" button to update process information and save data to `process_history.txt`.
3. Adjust the CPU alert threshold in the text box (default: 80%) to receive alerts for high usage.
4. Check total CPU and memory usage at the bottom of the window.
5. Review historical data in `process_history.txt` in the application directory.

## Documentation

For detailed instructions, including system requirements, full usage guide, and troubleshooting, refer to the Process Monitor Documentation.

## Notes

- The application updates data only on "Refresh" to minimize CPU usage.

Ensure write permissions in the application directory for saving historical data.
