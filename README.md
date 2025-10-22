<div align="center">

![Banner Placeholder](assets/banner.png)

[![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey.svg)](https://creativecommons.org/licenses/by-nc-sa/4.0/)
[![Platform](https://img.shields.io/badge/platform-Windows-blue.svg)](https://www.microsoft.com/windows)
[![Build Status](https://img.shields.io/github/actions/workflow/status/Teyk0o/Peek/release.yml?branch=main)](https://github.com/Teyk0o/Peek/actions)
[![VirusTotal](https://img.shields.io/badge/VirusTotal-0%2F71-success)](https://www.virustotal.com/gui/file/9298c1aab0d267c9126d32834e81663c30b3eae5d9cd95672711631449a58fe6)
[![Language](https://img.shields.io/badge/language-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Release](https://img.shields.io/github/v/release/Teyk0o/Peek)](https://github.com/Teyk0o/Peek/releases)

**A real-time network connection monitor for Windows, built in pure C with the Win32 API.**
Peek lets you instantly see which processes are talking to which destinations — inbound or outbound — in a clean, animated, and filterable interface.

</div>

---

## Overview

**PEEK** is a lightweight, open-source network monitoring tool written in C. It provides real-time visibility into TCP and UDP connections (both IPv4 and IPv6) established on your Windows machine, with intelligent filtering and process association.

It aims to fill the gap between heavy tools like Wireshark and limited CLI utilities like `netstat`, offering a **modern, GUI-driven experience** without sacrificing performance.

---

## Project Architecture

```
Peek/
├── main.c              # Application entry point (wWinMain)
├── gui.c / gui.h       # Win32 GUI module
├── network.c / network.h  # Network logic (Windows API)
├── logger.c / logger.h    # Colored log system
├── app.rc              # Windows resource file (icon)
├── resource.h          # Resource definitions
├── assets/             # Application icons
│   ├── icon_black.ico
│   ├── icon_white.ico
│   ├── icon_black.png
│   └── icon_white.png
├── CMakeLists.txt      # Build configuration
└── .github/workflows/
    └── release.yml     # CI/CD workflow
```

---

## Core Modules

### **1. Network Module (`network.c/h`)**

Responsible for querying Windows APIs to fetch active TCP/UDP connections (IPv4 & IPv6) and determine their direction (INBOUND / OUTBOUND).

Key APIs:

* `GetExtendedTcpTable()` / `GetExtendedUdpTable()` for active and listening sockets (IPv4 & IPv6)
* `OpenProcess()` / `QueryFullProcessImageNameA()` for process names
* `EnumProcessModules()` / `GetModuleBaseNameA()` for enhanced process name resolution

PEEK introduces a precise **direction detection algorithm**:

1. Retrieve all listening ports with `TCP_TABLE_OWNER_PID_LISTENER`.
2. Compare each connection's local port and PID:

    * If it matches a listening port → **INBOUND**
    * Otherwise → **OUTBOUND**

**Enhanced process name resolution:**
* Multiple access level attempts (from limited to full access)
* Fallback to PSAPI for system processes
* Special handling for System (PID 4) and System Idle (PID 0)
* Administrator privilege detection with user warning

It also tracks connection stats and maintains a cached table of seen connections for efficient updates.

---

### **2. GUI Module (`gui.c/h`)**

Handles the entire Win32 user interface: ListView, filters, buttons, and animations.

**Features:**

* Real-time connection table with direction and protocol indicators
* Filtering by direction (Inbound / Outbound / All)
* Filtering by protocol (TCP / UDP / All) with dropdown selector
* Toggle for localhost traffic visibility
* Smart grouping by process, protocol, and endpoint
* Flash animation for new or updated connections
* IPv6 address display support

Visual layout:

```
┌──────────────────────────────────────────────────────────────────────────┐
│ [Start] [Stop] [Clear]  (○)Out (○)In (○)All  [✓]Show Localhost Protocol:[All▼]│
├──────────────────────────────────────────────────────────────────────────┤
│ Time   │Dir│Proto│Remote Addr          │Port │Local Addr│LPort│Process│Cnt│
├────────┼───┼─────┼─────────────────────┼─────┼──────────┼─────┼───────┼───┤
│16:54:41│OUT│TCP  │142.250.200.46       │443  │192.168.1 │54123│chrome │ 1 │
│16:54:42│IN │TCP  │203.45.67.89         │52341│192.168.1 │80   │nginx  │ 3 │
│16:54:43│?  │UDP  │0.0.0.0              │-    │0.0.0.0   │53   │dns    │ 1 │
│16:54:44│OUT│TCP  │2001:4860:4860::8888 │443  │fe80::1   │54200│firefox│ 1 │
└────────┴───┴─────┴─────────────────────┴─────┴──────────┴─────┴───────┴───┘
│ Monitoring │ New: 12 │ Active: 45 │ Total: 143                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

---

### **3. Logger Module (`logger.c/h`)**

Provides colorful console logging with timestamps and levels:

```
[16:54:41] [INFO ] Starting PEEK - Network Monitor
[16:54:41] [OK   ] Network module initialized (33 connections)
[16:54:43] [ERROR] Network initialization failed
```

Supports ANSI escape codes and auto-enables VT sequences in Windows Terminal.

---

### **4. Main Module (`main.c`)**

Handles initialization and orchestration of all subsystems.

```c
int WINAPI wWinMain(HINSTANCE hInstance, ...) {
    logger_init();

    // Check administrator privileges and warn user if not elevated
    if (!IsRunningAsAdmin()) {
        // Show warning popup with option to continue or exit
    }

    if (network_init() != 0) return 1;
    gui_init(hInstance);
    gui_create_window(hInstance);
    int result = gui_run();
    network_cleanup();
    return result;
}
```

---

## Technologies & APIs

| API                         | Purpose                                   |
| --------------------------- | ----------------------------------------- |
| GetExtendedTcpTable         | Fetch TCP connections (IPv4 & IPv6)       |
| GetExtendedUdpTable         | Fetch UDP sockets (IPv4 & IPv6)           |
| OpenProcess                 | Open handle to process                    |
| QueryFullProcessImageNameA  | Retrieve process executable name          |
| EnumProcessModules          | Enumerate process modules                 |
| GetModuleBaseNameA          | Get module base name (fallback)           |
| CheckTokenMembership        | Verify administrator privileges           |
| CreateWindowEx / ListView_* | GUI creation and updates                  |
| SetTimer / GetTickCount     | Polling & animation timing                |
| LoadIcon / LoadImage        | Load custom icons                         |

Linked Libraries:

* `ws2_32` – Winsock API
* `iphlpapi` – IP Helper API (TCP/UDP tables)
* `psapi` – Process Status API (module enumeration)
* `comctl32` – Common Controls (ListView, ComboBox)

---

## Design & UX

* Modern light theme inspired by Visual Studio Code
* Blue flash effect for new connections
* Smooth fade animations (20 FPS)
* Alternating row colors (white / light gray)
* Compact and intuitive controls

---

## Build & Distribution

**Using CMake:**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

**GitHub Actions (CI/CD):**

* Auto-builds on tagged releases
* Produces artifacts: `Peek.exe`, `Peek-vX.Y.Z-Windows-x64.zip`

---

## Performance

| Resource | Typical usage         |
| -------- | --------------------- |
| CPU      | ~1–3% (500ms polling) |
| RAM      | ~15–25 MB             |
| Disk     | 0 – no file logs      |
| Network  | 0 – system APIs only  |

Optimized for low overhead via static arrays, caching, and minimal redraw logic.

---

## Permissions

| Mode          | Access level                      |
| ------------- | --------------------------------- |
| Standard User | Own processes + limited system visibility |
| Administrator | Full system-wide visibility (all PIDs)    |

PEEK runs without admin rights, but some system process names may appear as `[System] PID:xxx`. For full functionality, run as administrator. PEEK will show a warning popup if not elevated, with the option to continue or exit.

---

## Typical Use Cases

| Role      | Problem                              | Solution                   |
| --------- | ------------------------------------ | -------------------------- |
| Developer | "Where does my app connect?"         | Outbound filter by process |
| Sysadmin  | "Who connects to my web server?"     | Inbound + port filter      |
| Security  | "Is this process exfiltrating data?" | Outbound + process monitor |

---

## Comparison

| Feature            | **PEEK** | TCPView   | Netstat | Wireshark  |
| ------------------ | -------- | --------- | ------- | ---------- |
| IPv6 support       | ✅        | ✅         | ✅       | ✅          |
| UDP support        | ✅        | ✅         | ✅       | ✅          |
| Accurate direction | ✅        | ❌         | ❌       | ⚠️ Complex |
| Protocol filtering | ✅        | ❌         | ❌       | ✅          |
| Smart grouping     | ✅        | ❌         | ❌       | ❌          |
| Live filters       | ✅        | ⚠️        | ❌       | ✅          |
| UI                 | ✅ Modern | ⚠️ Legacy | ❌ CLI   | ⚠️ Heavy   |
| Open Source        | ✅        | ❌         | ✅       | ✅          |

---

## Known Limitations

* Windows-only (Win32 APIs)
* UDP remote endpoints not tracked (connectionless protocol)
* No ICMP/RAW socket support
* No historical graphing or persistence
* Requires admin rights for full process visibility

---

## Future Roadmap

**✅ Completed (v1.2.0):**

* ~~IPv6 support~~ ✅
* ~~UDP support~~ ✅
* ~~Protocol filtering (TCP/UDP/All)~~ ✅
* ~~Enhanced process name resolution~~ ✅
* ~~Administrator privilege detection~~ ✅

**Short term (v1.3.x):**

* IP/domain filter with search
* Export to CSV/JSON
* Dark mode toggle
* Connection statistics dashboard

**Mid term (v1.4.x - v1.5.x):**

* Real-time bandwidth graphs per connection
* Country/ASN lookup (GeoIP)
* Anomaly detection alerts
* Persistent stats (SQLite)

**Long term (v2.x):**

* Remote monitoring capability
* VirusTotal IP lookup integration
* Plugin system for extensions
* Packet capture integration

---

## Highlights

* **IPv4 & IPv6 support** for TCP and UDP protocols
* **Precise connection direction detection** (via TCP_TABLE_OWNER_PID_LISTENER)
* **Protocol filtering** (TCP/UDP/All) with real-time switching
* **Enhanced process resolution** with multi-level access attempts
* **Admin privilege detection** with user-friendly warnings
* **Smart connection grouping** (reduce noise by up to 90%)
* **Smooth Win32 UI animations** (20 FPS flash effects)
* **Fully automated CI/CD** via GitHub Actions
* **100% pure C**, zero external dependencies
* **Portable single `.exe`** (~100KB)

---

## Contributing

PEEK is open-source and welcomes contributions!
Feel free to submit pull requests, report issues, or suggest enhancements.

```bash
git clone https://github.com/Teyk0o/Peek.git
```

---

## License

CC BY-NC-SA 4.0 © 2025 Théo V. / Teyko

---

<div align="center">
Made with ❤️ in C
</div>
