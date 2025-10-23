/*
 * PEEK Daemon (peekd.exe)
 *
 * Background Windows Service for network monitoring
 *
 * Features:
 * - Windows Service integration (install/remove/start/stop)
 * - Network monitoring via network.c
 * - IPC server (Named Pipes) for UI communication
 * - Auto-update notification
 */

#include <windows.h>
#include <winsvc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "service.h"
#include "logger.h"

#define SERVICE_NAME L"PeekDaemon"
#define SERVICE_DISPLAY_NAME L"Peek Network Monitor Daemon"

/* Global flag for graceful shutdown */
volatile BOOL g_service_running = FALSE;

/*
 * Service entry point (called by SCM)
 */
void WINAPI ServiceMain(DWORD argc, LPTSTR *argv);

/*
 * Service control handler
 */
DWORD WINAPI ServiceControlHandler(DWORD dwControl, DWORD dwEventType,
                                   LPVOID lpEventData, LPVOID lpContext);

/*
 * Service dispatch table (required by SCM)
 */
SERVICE_TABLE_ENTRY ServiceTable[] = {
    {SERVICE_NAME, ServiceMain},
    {NULL, NULL}
};

/*
 * Check if running as a Windows Service
 */
BOOL IsServiceMode(void) {
    BOOL bResult = FALSE;
    DWORD dwProcessId = GetCurrentProcessId();
    DWORD dwParentProcessId = 0;

    /* Simplified check: if stdin is not a console, we're likely in service mode */
    return GetFileType(GetStdHandle(STD_INPUT_HANDLE)) != FILE_TYPE_CHAR;
}

/*
 * Service Main Function
 */
void WINAPI ServiceMain(DWORD argc, LPTSTR *argv) {
    (void)argc;
    (void)argv;

    SERVICE_STATUS_HANDLE hServiceStatusHandle;
    SERVICE_STATUS serviceStatus;

    /* Register control handler */
    hServiceStatusHandle = RegisterServiceCtrlHandlerEx(
        SERVICE_NAME,
        ServiceControlHandler,
        NULL
    );

    if (!hServiceStatusHandle) {
        logger_error("RegisterServiceCtrlHandlerEx failed");
        return;
    }

    /* Initialize service status */
    ZeroMemory(&serviceStatus, sizeof(serviceStatus));
    serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    serviceStatus.dwCurrentState = SERVICE_START_PENDING;
    serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    serviceStatus.dwWin32ExitCode = 0;
    serviceStatus.dwServiceSpecificExitCode = 0;
    serviceStatus.dwCheckPoint = 0;
    serviceStatus.dwWaitHint = 3000;

    if (!SetServiceStatus(hServiceStatusHandle, &serviceStatus)) {
        logger_error("SetServiceStatus failed");
        return;
    }

    /* Initialize daemon */
    logger_init();
    logger_info("Peek Daemon starting...");

    if (service_init() != 0) {
        logger_error("Service initialization failed");
        serviceStatus.dwCurrentState = SERVICE_STOPPED;
        serviceStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
        serviceStatus.dwServiceSpecificExitCode = 1;
        SetServiceStatus(hServiceStatusHandle, &serviceStatus);
        return;
    }

    /* Service is running */
    g_service_running = TRUE;
    serviceStatus.dwCurrentState = SERVICE_RUNNING;
    serviceStatus.dwCheckPoint = 0;
    serviceStatus.dwWaitHint = 0;

    if (!SetServiceStatus(hServiceStatusHandle, &serviceStatus)) {
        logger_error("SetServiceStatus failed");
        return;
    }

    logger_info("Peek Daemon running");

    /* Main service loop */
    while (g_service_running) {
        service_tick();
        Sleep(100);
    }

    /* Cleanup */
    service_cleanup();

    /* Report stopped status */
    serviceStatus.dwCurrentState = SERVICE_STOPPED;
    serviceStatus.dwCheckPoint = 0;
    serviceStatus.dwWaitHint = 0;
    SetServiceStatus(hServiceStatusHandle, &serviceStatus);

    logger_info("Peek Daemon stopped");
}

/*
 * Service Control Handler
 */
DWORD WINAPI ServiceControlHandler(DWORD dwControl, DWORD dwEventType,
                                   LPVOID lpEventData, LPVOID lpContext) {
    (void)dwEventType;
    (void)lpEventData;
    (void)lpContext;

    switch (dwControl) {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            logger_info("Service stop requested");
            g_service_running = FALSE;
            return NO_ERROR;

        case SERVICE_CONTROL_INTERROGATE:
            return NO_ERROR;

        default:
            return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

/*
 * Install service
 */
static int service_install(void) {
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    TCHAR szPath[MAX_PATH];

    if (!GetModuleFileName(NULL, szPath, MAX_PATH)) {
        fprintf(stderr, "GetModuleFileName failed\n");
        return 1;
    }

    schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!schSCManager) {
        fprintf(stderr, "OpenSCManager failed: %ld\n", GetLastError());
        return 1;
    }

    schService = CreateService(
        schSCManager,
        SERVICE_NAME,
        SERVICE_DISPLAY_NAME,
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        szPath,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    );

    if (!schService) {
        fprintf(stderr, "CreateService failed: %ld\n", GetLastError());
        CloseServiceHandle(schSCManager);
        return 1;
    }

    printf("Service installed successfully\n");
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    return 0;
}

/*
 * Remove service
 */
static int service_remove(void) {
    SC_HANDLE schSCManager;
    SC_HANDLE schService;

    schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!schSCManager) {
        fprintf(stderr, "OpenSCManager failed: %ld\n", GetLastError());
        return 1;
    }

    schService = OpenService(schSCManager, SERVICE_NAME, SERVICE_ALL_ACCESS);
    if (!schService) {
        fprintf(stderr, "OpenService failed: %ld\n", GetLastError());
        CloseServiceHandle(schSCManager);
        return 1;
    }

    if (!DeleteService(schService)) {
        fprintf(stderr, "DeleteService failed: %ld\n", GetLastError());
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return 1;
    }

    printf("Service removed successfully\n");
    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    return 0;
}

/*
 * Main entry point
 */
int main(int argc, char *argv[]) {
    /* Parse command line arguments */
    if (argc > 1) {
        if (strcmp(argv[1], "--install-service") == 0) {
            return service_install();
        } else if (strcmp(argv[1], "--remove-service") == 0) {
            return service_remove();
        } else if (strcmp(argv[1], "--help") == 0) {
            printf("Peek Daemon v2.0\n\n");
            printf("Usage: peekd.exe [option]\n\n");
            printf("Options:\n");
            printf("  --install-service   Install as Windows service\n");
            printf("  --remove-service    Remove Windows service\n");
            printf("  --help               Show this help message\n");
            printf("\n");
            printf("If no option is specified, runs as Windows Service (when started by SCM)\n");
            return 0;
        }
    }

    /* Try to start as service (if called by SCM) */
    if (!StartServiceCtrlDispatcher(ServiceTable)) {
        DWORD dwError = GetLastError();
        if (dwError == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
            /* Not called by SCM, run as console for debugging */
            logger_init();
            logger_info("Running in console mode (not as service)");

            if (service_init() != 0) {
                logger_error("Daemon initialization failed");
                return 1;
            }

            g_service_running = TRUE;
            printf("Press Ctrl+C to stop...\n");

            while (g_service_running) {
                service_tick();
                Sleep(100);
            }

            service_cleanup();
            return 0;
        } else {
            logger_error("StartServiceCtrlDispatcher failed");
            return 1;
        }
    }

    return 0;
}
