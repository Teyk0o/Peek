#!/usr/bin/env node

/**
 * Peek Build Orchestration Script
 * Builds all components in the correct order:
 * 1. Frontend (React) -> frontend/dist
 * 2. Daemon (peekd.exe)
 * 3. UI Host (peek.exe) with embedded frontend
 * 4. Updater (updater.exe)
 */

const { execSync } = require('child_process');
const path = require('path');
const fs = require('fs');

const ROOT_DIR = path.resolve(__dirname, '..');
const FRONTEND_DIR = path.join(ROOT_DIR, 'frontend');
const BUILD_DIR = path.join(ROOT_DIR, 'build');

const colors = {
    reset: '\x1b[0m',
    bright: '\x1b[1m',
    green: '\x1b[32m',
    yellow: '\x1b[33m',
    red: '\x1b[31m',
    blue: '\x1b[34m',
};

function log(message, color = 'reset') {
    console.log(`${colors[color]}${message}${colors.reset}`);
}

function logStep(step, message) {
    log(`\n[${step}] ${message}`, 'bright');
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

function execCmd(cmd, options = {}) {
    const cwd = options.cwd || process.cwd();
    const env = { ...process.env, ...options.env };

    try {
        return execSync(cmd, {
            cwd,
            env,
            stdio: 'inherit',
            shell: process.platform === 'win32' ? 'cmd.exe' : '/bin/bash',
        });
    } catch (error) {
        throw error;
    }
}

async function main() {
    try {
        log('\n========================================', 'bright');
        log('Peek Build System v2.0', 'bright');
        log('========================================\n', 'bright');

        // Step 1: Frontend build
        logStep('1/4', 'Building React frontend...');

        if (!fs.existsSync(path.join(FRONTEND_DIR, 'node_modules'))) {
            logWarning('node_modules not found, running npm install...');
            execCmd('npm install', { cwd: FRONTEND_DIR });
        }

        execCmd('npm run build', { cwd: FRONTEND_DIR });

        if (!fs.existsSync(path.join(FRONTEND_DIR, 'dist'))) {
            logError('Frontend build failed - dist directory not created');
        }
        logSuccess('Frontend built to frontend/dist');

        // Step 2: CMake configuration
        logStep('2/4', 'Configuring CMake...');

        const buildType = process.env.BUILD_TYPE || 'Release';
        const cmakeCmd = `cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=${buildType} "${ROOT_DIR}"`;

        execCmd(cmakeCmd);
        logSuccess('CMake configuration complete');

        // Step 3: Build all targets
        logStep('3/4', 'Building all C/C++ targets (daemon, UI host, updater)...');

        execCmd(`cmake --build "${BUILD_DIR}" --config ${buildType}`);
        logSuccess('All C/C++ targets built');

        // Step 4: Verify outputs
        logStep('4/4', 'Verifying build outputs...');

        const expectedFiles = [
            path.join(BUILD_DIR, 'ui', 'peek.exe'),
            path.join(BUILD_DIR, 'daemon', 'peekd.exe'),
            path.join(BUILD_DIR, 'updater', 'updater.exe'),
        ];

        const missingFiles = expectedFiles.filter(file => !fs.existsSync(file));

        if (missingFiles.length > 0) {
            logWarning('Some executables were not created:');
            missingFiles.forEach(f => log(`  - ${f}`, 'yellow'));
        } else {
            logSuccess('All executables built successfully:');
            expectedFiles.forEach(f => log(`  ✓ ${path.basename(f)}`, 'green'));
        }

        log('\n========================================', 'bright');
        log('Build Complete!', 'green');
        log('========================================\n', 'bright');

        log('Next steps:', 'blue');
        log('  1. Sign binaries (if certificate available):');
        log('     npm run sign', 'yellow');
        log('  2. Package for distribution:');
        log('     npm run package', 'yellow');

    } catch (error) {
        logError(`Build failed: ${error.message}`);
    }
}

main();
