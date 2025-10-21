<div align="center">

![Banner Placeholder](assets/banner.png)

**A real-time network connection monitor for Windows, built in pure C with the Win32 API.**
Peek lets you instantly see which processes are talking to which destinations — inbound or outbound — in a clean, animated, and filterable interface.

</div>

---

## 🎯 Overview

**PEEK** is a lightweight, open-source network monitoring tool written in C. It provides real-time visibility into TCP connections established on your Windows machine, with intelligent filtering and process association.

It aims to fill the gap between heavy tools like Wireshark and limited CLI utilities like `netstat`, offering a **modern, GUI-driven experience** without sacrificing performance.

---

## 🛠️ Project Architecture

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

## 🔍 Core Modules

### **1. Network Module (`network.c/h`)**

Responsible for querying Windows APIs to fetch active TCP connections and determine their direction (INBOUND / OUTBOUND).

Key APIs:

* `GetExtendedTcpTable()` for active and listening sockets
* `OpenProcess()` / `QueryFullProcessImageNameA()` for process names

PEEK introduces a precise **direction detection algorithm**:

1. Retrieve all listening ports with `TCP_TABLE_OWNER_PID_LISTENER`.
2. Compare each connection's local port and PID:

    * If it matches a listening port → **INBOUND**
    * Otherwise → **OUTBOUND**

It also tracks connection stats and maintains a cached table of seen connections for efficient updates.

---

### **2. GUI Module (`gui.c/h`)**

Handles the entire Win32 user interface: ListView, filters, buttons, and animations.

**Features:**

* Real-time connection table with direction indicators
* Filtering by direction (Inbound / Outbound / All)
* Toggle for localhost traffic visibility
* Smart grouping by process and endpoint
* Flash animation for new or updated connections

Visual layout:

```
┌─────────────────────────────────────────────────────────────────┐
│ [Start] [Stop] [Clear]  (○)Out (○)In (○)All  [✓]Show Localhost │
├─────────────────────────────────────────────────────────────────┤
│ Time   │Dir│Remote Addr     │Port │Local Addr│LPort│Process│Cnt│
├────────┼───┼────────────────┼─────┼──────────┼─────┼───────┼───┤
│16:54:41│OUT│142.250.200.46  │443  │192.168.1 │54123│chrome │ 1 │
│16:54:42│IN │203.45.67.89    │52341│192.168.1 │80   │nginx  │ 3 │
└────────┴───┴────────────────┴─────┴──────────┴─────┴───────┴───┘
│ Monitoring │ New: 12 │ Active: 45 │ Total: 143                  │
└─────────────────────────────────────────────────────────────────┘
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
    if (network_init() != 0) return 1;
    gui_init(hInstance);
    gui_create_window(hInstance);
    int result = gui_run();
    network_cleanup();
    return result;
}
```

---

## ⚙️ Technologies & APIs

| API                         | Purpose                                   |
| --------------------------- | ----------------------------------------- |
| GetExtendedTcpTable         | Fetch TCP connections and listening ports |
| OpenProcess                 | Open handle to process                    |
| QueryFullProcessImageNameA  | Retrieve process executable name          |
| CreateWindowEx / ListView_* | GUI creation and updates                  |
| SetTimer / GetTickCount     | Polling & animation timing                |
| LoadIcon / LoadImage        | Load custom icons                         |

Linked Libraries:

* `ws2_32` – Winsock API
* `iphlpapi` – IP Helper API
* `comctl32` – Common Controls

---

## 🎨 Design & UX

* Modern light theme inspired by Visual Studio Code
* Blue flash effect for new connections
* Smooth fade animations (20 FPS)
* Alternating row colors (white / light gray)
* Compact and intuitive controls

---

## 🚀 Build & Distribution

**Using CMake:**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

**GitHub Actions (CI/CD):**

* Auto-builds on tagged releases
* Produces artifacts: `Peek.exe`, `Peek-vX.Y.Z-Windows-x64.zip`

---

## 📊 Performance

| Resource | Typical usage         |
| -------- | --------------------- |
| CPU      | ~1–3% (500ms polling) |
| RAM      | ~15–25 MB             |
| Disk     | 0 – no file logs      |
| Network  | 0 – system APIs only  |

Optimized for low overhead via static arrays, caching, and minimal redraw logic.

---

## 🔐 Permissions

| Mode          | Access level                      |
| ------------- | --------------------------------- |
| Standard User | Own processes only                |
| Administrator | System-wide visibility (all PIDs) |

PEEK runs fine without admin rights, but with limited visibility.

---

## 🧭 Typical Use Cases

| Role      | Problem                              | Solution                   |
| --------- | ------------------------------------ | -------------------------- |
| Developer | "Where does my app connect?"         | Outbound filter by process |
| Sysadmin  | "Who connects to my web server?"     | Inbound + port filter      |
| Security  | "Is this process exfiltrating data?" | Outbound + process monitor |

---

## 🆚 Comparison

| Feature            | **PEEK** | TCPView   | Netstat | Wireshark  |
| ------------------ | -------- | --------- | ------- | ---------- |
| Accurate direction | ✅        | ❌         | ❌       | ⚠️ Complex |
| Smart grouping     | ✅        | ❌         | ❌       | ❌          |
| Live filters       | ✅        | ⚠️        | ❌       | ✅          |
| UI                 | ✅ Modern | ⚠️ Legacy | ❌ CLI   | ⚠️ Heavy   |
| Open Source        | ✅        | ❌         | ✅       | ✅          |

---

## 🧩 Known Limitations

* IPv4 only (AF_INET)
* TCP only (no UDP/ICMP)
* Windows-only (Win32 APIs)
* No historical graphing or persistence

---

## 📈 Future Roadmap

**Short term:**

* IPv6 & UDP support
* IP/domain filter
* Export to CSV
* Dark mode toggle

**Mid term:**

* Real-time bandwidth graphs
* Anomaly alerts
* Persistent stats (SQLite)

**Long term:**

* Remote monitoring
* VirusTotal IP lookup integration
* Plugin system for extensions

---

## 🏆 Highlights

* Precise connection direction detection (via TCP_TABLE_OWNER_PID_LISTENER)
* Smart connection grouping (reduce noise by up to 90%)
* Smooth Win32 UI animations (20 FPS)
* Fully automated CI/CD via GitHub Actions
* 100% pure C, zero dependencies
* Portable single `.exe`

---

## 🤝 Contributing

PEEK is open-source and welcomes contributions!
Feel free to submit pull requests, report issues, or suggest enhancements.

```bash
git clone https://github.com/Teyk0o/Peek.git
```

---

## 📜 License

MIT License © 2025 Théo V. / Teyko

---

<div align="center">
Made with ❤️ in C for Windows Developer
</div>
