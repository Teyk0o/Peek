# Peek v2.0 Development Guide

## Architecture Overview

Peek v2.0 is a **modular security monitoring suite** with three main components:

### 1. **peek.exe** — UI Host (Win32 + WebView2)
- **Purpose**: Embedded browser application that displays the React frontend
- **Tech Stack**: C++, WebView2 SDK, Win32 API
- **Build**: `ui/CMakeLists.txt`
- **Output**: `build/ui/peek.exe`
- **Dependencies**: Frontend must be built to `frontend/dist` first

### 2. **peekd.exe** — Background Daemon (Windows Service)
- **Purpose**: Network monitoring service that runs in the background
- **Tech Stack**: C, Windows Service API, Named Pipes IPC
- **Build**: `daemon/CMakeLists.txt`
- **Output**: `build/daemon/peekd.exe`
- **Features**:
  - Continuous network monitoring
  - IPC server (Named Pipes: `\\.\pipe\Peek-IPC-v1`)
  - Auto-update notification
  - Trust verification

### 3. **updater.exe** — Secure Auto-Updater
- **Purpose**: Downloads, verifies, and atomically installs updates
- **Tech Stack**: C, WinInet, CAPI (cryptography)
- **Build**: `updater/CMakeLists.txt`
- **Output**: `build/updater/updater.exe`
- **Features**:
  - RSA-SHA256 manifest verification
  - SHA256 binary hash verification
  - Atomic file replacement with rollback
  - Silent installation

## Build Process

### Prerequisites

- **Windows 10/11** (build machine)
- **CMake 3.27+**
- **Visual Studio 2022** or **MinGW**
- **Node.js 18+** (for frontend build)
- **WebView2 SDK** (for UI host)

### Step 1: Install Dependencies

```bash
# Frontend dependencies
cd frontend
npm install

# Return to root
cd ..
```

### Step 2: Configure CMake

```bash
# Create build directory and configure
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

This will:
1. Read version from `build-config.json`
2. Setup frontend build target
3. Configure daemon, UI host, and updater

### Step 3: Build Everything

```bash
# Option A: Manual build
npm run build

# Option B: Direct CMake
cmake --build build --config Release
```

This will:
1. Build React frontend → `frontend/dist`
2. Build peekd.exe (daemon)
3. Build peek.exe (UI host) with embedded frontend
4. Build updater.exe

### Step 4: Sign Binaries (Optional)

Requires `PeekCertificate.pfx` and `PEEK_CERT_PASSWORD` environment variable:

```bash
npm run sign
# or manually
node scripts/sign-binaries.js
```

### Step 5: Package Distribution

```bash
npm run package
```

Creates `dist/Peek-v2.0.0-windows-x64/` with:
- All three executables
- Checksums
- Build metadata
- Ready for installer

## Build Configuration

### Version Management

The **single source of truth** is `build-config.json`:

```json
{
  "version": "2.0.0-alpha.1",
  "app_name": "Peek",
  "app_description": "Professional Network Security Monitor",
  ...
}
```

**All components** read this version:
- C/C++ macros: `PEEK_VERSION`, `PEEK_APP_NAME`
- Frontend env: `NEXT_PUBLIC_APP_VERSION`
- Installer: Version strings

### Updating Version

1. Edit `build-config.json`
2. Rebuild with CMake — version automatically propagates

## Frontend Development

### Development Mode

```bash
cd frontend
npm run dev
# Starts Next.js dev server at http://localhost:3000
```

### Building Static Export

```bash
cd frontend
npm run build
# Output: frontend/dist/ (static HTML/CSS/JS)
```

### Key Configuration

- **Output format**: Static export (`output: 'export'` in `next.config.js`)
- **Output directory**: `dist/` (not `out/`)
- **Relative paths**: `assetPrefix: './'` for WebView2 embedding
- **Styling**: Tailwind CSS with Peek-specific color palette

### Project Structure

```
frontend/
├── src/
│   ├── pages/           # Next.js pages (route: /pages/foo.tsx → /foo)
│   ├── components/      # Reusable React components
│   ├── hooks/          # Custom hooks (useIPC, useConnections, etc.)
│   ├── lib/            # Utilities (IPC client, API, types)
│   └── styles/         # Global styles
├── public/             # Static assets (favicon, etc.)
├── next.config.js      # Next.js configuration
├── tailwind.config.js  # Tailwind CSS theme
├── package.json        # Dependencies
└── dist/              # Build output (static)
```

## IPC Communication

### Protocol

**Named Pipes**: `\\.\pipe\Peek-IPC-v1`

**Message format** (C header in `shared/ipc-protocol.h`):

```c
typedef struct {
    uint32_t magic;         // 0x50454B49 ('PEKI')
    uint32_t version;       // Protocol version: 1
    uint32_t msg_id;        // Unique request ID
    uint32_t msg_type;      // REQUEST / RESPONSE / EVENT
    uint32_t cmd_id;        // Command or event type
    uint32_t payload_size;  // 0-65508 bytes
    uint32_t flags;
    uint32_t reserved;
} IPC_MessageHeader;
```

### Command Types

**Requests** (peek.exe → peekd.exe):

- `CMD_GET_CONNECTIONS` — Fetch current connections
- `CMD_FILTER_UPDATE` — Update filtering options
- `CMD_GET_SETTINGS` — Fetch daemon settings
- `CMD_SET_SETTINGS` — Change daemon settings
- `CMD_START_MONITOR` — Start monitoring
- `CMD_STOP_MONITOR` — Stop monitoring

**Events** (peekd.exe → peek.exe):

- `EVT_CONNECTION_ADDED` — New connection detected
- `EVT_CONNECTION_UPDATED` — Connection stats changed
- `EVT_TRUST_CHANGED` — Trust status updated
- `EVT_UPDATE_AVAILABLE` — New version available

## Code Signing

### Prerequisites

1. Obtain or generate a code-signing certificate (Authenticode)
2. Export as `.pfx` file with password
3. Place `PeekCertificate.pfx` in project root
4. Set environment variable: `PEEK_CERT_PASSWORD=your_password`

### Signing

```bash
npm run sign
# or set BUILD_TYPE=Release && npm run build (auto-signs)
```

### Verification

Users can verify signature in Windows:
```powershell
# PowerShell
Get-AuthenticodeSignature C:\Program Files\Peek\peek.exe
```

## Testing

### Manual Testing

1. **Start daemon service**:
   ```cmd
   peekd.exe --install-service
   net start PeekDaemon
   ```

2. **Launch UI**:
   ```cmd
   peek.exe
   ```

3. **View logs** (if console enabled):
   - Look for network connections in the table
   - Test filters (protocol, direction, trust level)
   - Check trust override functionality

### Automated Tests (Future)

- Unit tests for C modules (network parsing, IPC)
- Integration tests (daemon ↔ UI communication)
- E2E tests (React components with mock data)

## Continuous Integration

### GitHub Actions Workflows

1. **`build.yml`** — Builds on every commit
   - Install dependencies
   - Run CMake build
   - Upload artifacts

2. **`release.yml`** — Creates release on tag
   - Build with Release config
   - Sign binaries (requires secrets)
   - Create NSIS installer
   - Generate checksums
   - Draft GitHub release

3. **`sign.yml`** — Offline signing workflow (optional)
   - Requires manually adding signed executables

### Secrets Required

For CI/CD to work fully:

```
PEEK_CERT_PASSWORD    # Certificate password for code signing
```

## Troubleshooting

### "WebView2 SDK not found"

```bash
# Install WebView2 NuGet package
vcpkg install webview2:x64-windows

# Or download SDK manually:
# https://developer.microsoft.com/en-us/microsoft-edge/webview2/
```

### "frontend/dist not found" during build

```bash
# Ensure frontend is built first
cd frontend
npm install
npm run build
cd ..

# Then build UI host
cmake --build build --config Release
```

### "peekd.exe --install-service" fails

- Run as **Administrator**
- Check Windows Event Viewer for errors
- Ensure no previous instance is running: `net stop PeekDaemon`

### IPC connection timeout

- Verify daemon is running: `net start PeekDaemon`
- Check Named Pipe name: `\\.\pipe\Peek-IPC-v1`
- Review daemon logs for errors

## Release Checklist

- [ ] Update version in `build-config.json`
- [ ] Update `README.md` with new features
- [ ] Run `npm run build` — verify no errors
- [ ] Sign binaries (if certificate available)
- [ ] Test installer on clean VM
- [ ] Create git tag: `git tag v2.0.0`
- [ ] Push tag: `git push origin v2.0.0`
- [ ] GitHub Actions will create release automatically

## Resources

- **WebView2 Docs**: https://docs.microsoft.com/en-us/microsoft-edge/webview2/
- **Windows Service**: https://docs.microsoft.com/en-us/windows/win32/services/
- **Named Pipes**: https://docs.microsoft.com/en-us/windows/win32/ipc/named-pipes
- **Authenticode**: https://docs.microsoft.com/en-us/windows/win32/seccrypto/
- **NSIS Installer**: https://nsis.sourceforge.io/
