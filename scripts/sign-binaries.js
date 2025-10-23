#!/usr/bin/env node

/**
 * Code Signing Script for Peek Binaries
 * Uses Windows signtool to sign:
 * - peek.exe (UI Host)
 * - peekd.exe (Daemon)
 * - updater.exe (Updater)
 *
 * Requires:
 * - PeekCertificate.pfx in project root
 * - PEEK_CERT_PASSWORD environment variable
 */

const { execSync, spawnSync } = require('child_process');
const path = require('path');
const fs = require('fs');

const ROOT_DIR = path.resolve(__dirname, '..');
const CERT_FILE = path.join(ROOT_DIR, 'PeekCertificate.pfx');
const CERT_PASSWORD = process.env.PEEK_CERT_PASSWORD;
const BUILD_DIR = path.join(ROOT_DIR, 'build');

const colors = {
    reset: '\x1b[0m',
    bright: '\x1b[1m',
    green: '\x1b[32m',
    yellow: '\x1b[33m',
    red: '\x1b[31m',
};

function log(message, color = 'reset') {
    console.log(`${colors[color]}${message}${colors.reset}`);
}

function logSuccess(message) {
    log(`✓ ${message}`, 'green');
}

function logError(message) {
    log(`✗ ${message}`, 'red');
    process.exit(1);
}

function logWarning(message) {
    log(`⚠ ${message}`, 'yellow');
}

function findSigntool() {
    const possiblePaths = [
        'C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.22621.0\\x64\\signtool.exe',
        'C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.22000.0\\x64\\signtool.exe',
        'C:\\Program Files (x86)\\Windows Kits\\10\\bin\\x64\\signtool.exe',
    ];

    for (const p of possiblePaths) {
        if (fs.existsSync(p)) {
            return p;
        }
    }

    // Try using 'where' command
    try {
        const result = execSync('where signtool', { encoding: 'utf-8' });
        return result.trim();
    } catch (e) {
        return null;
    }
}

function signBinary(binaryPath, signtool) {
    if (!fs.existsSync(binaryPath)) {
        logWarning(`Binary not found: ${binaryPath}`);
        return false;
    }

    const cmd = `"${signtool}" sign /f "${CERT_FILE}" /p "${CERT_PASSWORD}" /tr http://timestamp.digicert.com /td sha256 /fd sha256 /v "${binaryPath}"`;

    log(`Signing: ${path.basename(binaryPath)}`);

    try {
        execSync(cmd, { stdio: 'inherit' });
        logSuccess(`Signed: ${path.basename(binaryPath)}`);
        return true;
    } catch (error) {
        logError(`Failed to sign ${path.basename(binaryPath)}: ${error.message}`);
        return false;
    }
}

async function main() {
    log('\n========================================');
    log('Peek Binary Code Signing', 'bright');
    log('========================================\n');

    // Check for certificate
    if (!fs.existsSync(CERT_FILE)) {
        logWarning('Certificate file not found: ' + CERT_FILE);
        log('\nTo enable code signing:');
        log('  1. Place your PeekCertificate.pfx in the project root');
        log('  2. Set PEEK_CERT_PASSWORD environment variable');
        log('  3. Re-run this script');
        process.exit(0);
    }

    // Check for password
    if (!CERT_PASSWORD) {
        logWarning('PEEK_CERT_PASSWORD environment variable not set');
        log('\nTo sign binaries:');
        log('  set PEEK_CERT_PASSWORD=your_password');
        log('  npm run sign');
        process.exit(0);
    }

    // Find signtool
    const signtool = findSigntool();
    if (!signtool) {
        logError('signtool.exe not found. Please install Windows 10 SDK or Windows 11 SDK.');
    }

    logSuccess('Found signtool: ' + signtool);

    // Sign binaries
    const binaries = [
        path.join(BUILD_DIR, 'ui', 'Release', 'peek.exe'),
        path.join(BUILD_DIR, 'daemon', 'Release', 'peekd.exe'),
        path.join(BUILD_DIR, 'updater', 'Release', 'updater.exe'),
    ];

    // Alternative paths for different CMake generators
    const altBinaries = [
        path.join(BUILD_DIR, 'ui', 'peek.exe'),
        path.join(BUILD_DIR, 'daemon', 'peekd.exe'),
        path.join(BUILD_DIR, 'updater', 'updater.exe'),
    ];

    const filesToSign = [...binaries, ...altBinaries].filter(f => fs.existsSync(f));

    if (filesToSign.length === 0) {
        logError('No binaries found to sign. Please build first: npm run build');
    }

    log(`\nSigning ${filesToSign.length} binary(ies)...\n`);

    let successCount = 0;
    filesToSign.forEach(binary => {
        if (signBinary(binary, signtool)) {
            successCount++;
        }
    });

    log('\n========================================');
    log(`Signing complete: ${successCount}/${filesToSign.length} successful`, 'green');
    log('========================================\n');

    if (successCount < filesToSign.length) {
        process.exit(1);
    }
}

main().catch(error => {
    logError(`Unexpected error: ${error.message}`);
});
