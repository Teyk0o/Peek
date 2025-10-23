#!/usr/bin/env node

/**
 * Peek Packaging Script
 * Creates distribution package with all components
 */

const path = require('path');
const fs = require('fs');
const { execSync } = require('child_process');

const ROOT_DIR = path.resolve(__dirname, '..');
const BUILD_DIR = path.join(ROOT_DIR, 'build');
const DIST_DIR = path.join(ROOT_DIR, 'dist');
const PACKAGE_DIR = path.join(DIST_DIR, 'Peek-v2.0.0-windows-x64');

const colors = {
    reset: '\x1b[0m',
    green: '\x1b[32m',
    yellow: '\x1b[33m',
    blue: '\x1b[34m',
};

function log(message, color = 'reset') {
    console.log(`${colors[color]}${message}${colors.reset}`);
}

function logSuccess(message) {
    log(`✓ ${message}`, 'green');
}

function logWarning(message) {
    log(`⚠ ${message}`, 'yellow');
}

async function main() {
    log('\n========================================');
    log('Peek Packaging Script', 'blue');
    log('========================================\n');

    // Create output directory
    if (fs.existsSync(DIST_DIR)) {
        execSync(`rm -rf "${DIST_DIR}"`, { shell: true });
    }
    fs.mkdirSync(DIST_DIR, { recursive: true });
    fs.mkdirSync(PACKAGE_DIR, { recursive: true });

    log('Created distribution directory: ' + DIST_DIR);

    // Copy binaries
    const binaries = [
        { src: 'ui/Release/peek.exe', name: 'peek.exe' },
        { src: 'ui/peek.exe', name: 'peek.exe', alt: true },
        { src: 'daemon/Release/peekd.exe', name: 'peekd.exe' },
        { src: 'daemon/peekd.exe', name: 'peekd.exe', alt: true },
        { src: 'updater/Release/updater.exe', name: 'updater.exe' },
        { src: 'updater/updater.exe', name: 'updater.exe', alt: true },
    ];

    const copiedBinaries = new Set();

    for (const binary of binaries) {
        const srcPath = path.join(BUILD_DIR, binary.src);
        if (fs.existsSync(srcPath) && !copiedBinaries.has(binary.name)) {
            const destPath = path.join(PACKAGE_DIR, binary.name);
            fs.copyFileSync(srcPath, destPath);
            logSuccess(`Copied: ${binary.name}`);
            copiedBinaries.add(binary.name);
        }
    }

    if (copiedBinaries.size === 0) {
        logWarning('No binaries found to package. Did you build first?');
        process.exit(1);
    }

    // Copy installer
    const installerSrc = path.join(ROOT_DIR, 'installer');
    if (fs.existsSync(installerSrc)) {
        const installerDest = path.join(PACKAGE_DIR, 'installer');
        execSync(`cp -r "${installerSrc}" "${installerDest}"`, { shell: true });
        logSuccess('Copied installer scripts');
    }

    // Copy build config and version info
    const buildConfigSrc = path.join(ROOT_DIR, 'build-config.json');
    if (fs.existsSync(buildConfigSrc)) {
        const buildConfigDest = path.join(PACKAGE_DIR, 'build-config.json');
        fs.copyFileSync(buildConfigSrc, buildConfigDest);
        logSuccess('Copied build configuration');
    }

    // Create README for distribution
    const readmePath = path.join(PACKAGE_DIR, 'README-DISTRIBUTION.md');
    fs.writeFileSync(readmePath, `# Peek v2.0.0 Distribution

## Contents

- \`peek.exe\` - UI Host with embedded React frontend
- \`peekd.exe\` - Background daemon service
- \`updater.exe\` - Secure auto-updater
- \`installer/\` - NSIS installer scripts

## Installation

### Quick Start (Recommended)
Run the installer:
\`\`\`
installer/Peek-Setup.exe
\`\`\`

### Manual Installation
1. Place \`peek.exe\`, \`peekd.exe\`, and \`updater.exe\` in \`C:\\Program Files\\Peek\\\`
2. Run \`peekd.exe --install-service\` as Administrator
3. Create shortcut to \`peek.exe\` on desktop

## Requirements

- Windows 10 or later (1909+)
- WebView2 Runtime (or Chromium Edge installed)
- Administrator privileges (recommended for full system visibility)

## First Run

1. Launch \`peek.exe\`
2. Grant Windows Firewall access if prompted
3. Allow daemon service to start

## Uninstall

1. Run installer again and select "Uninstall"
2. Or: Run \`peekd.exe --remove-service\` as Administrator

## Support

For issues and updates, visit: https://github.com/Teyk0o/Peek
`);
    logSuccess('Created distribution README');

    // Create checksum file
    const checksumPath = path.join(DIST_DIR, 'SHA256SUMS');
    let checksumContent = '';

    for (const binary of Array.from(copiedBinaries)) {
        const filePath = path.join(PACKAGE_DIR, binary);
        if (fs.existsSync(filePath)) {
            const hash = execSync(`sha256sum "${filePath}"`, {
                encoding: 'utf-8',
                shell: '/bin/bash'
            }).split(' ')[0];
            checksumContent += `${hash}  ${binary}\n`;
        }
    }

    fs.writeFileSync(checksumPath, checksumContent);
    logSuccess('Created checksums');

    // Create archive
    log('\nCreating archive...');
    const archivePath = path.join(DIST_DIR, 'Peek-v2.0.0-windows-x64.zip');
    execSync(`cd "${DIST_DIR}" && zip -r "Peek-v2.0.0-windows-x64.zip" "Peek-v2.0.0-windows-x64"`, {
        shell: true
    });
    logSuccess('Created archive: Peek-v2.0.0-windows-x64.zip');

    log('\n========================================');
    log('Packaging Complete!', 'green');
    log('========================================');
    log(`Distribution ready: ${DIST_DIR}`);
    log(`Archive size: ${(fs.statSync(archivePath).size / 1024 / 1024).toFixed(2)} MB`);
    log('');
}

main().catch(error => {
    log(`✗ Error: ${error.message}`, 'red');
    process.exit(1);
});
