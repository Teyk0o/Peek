/*
* PEEK - Network Monitor
*/

#include "network.h"
#include "gui.h"
#include "logger.h"
#include <windows.h>

// Check if the application is running with administrator privileges
BOOL IsRunningAsAdmin(void) {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }

    return isAdmin;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR pCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)pCmdLine;
    (void)nCmdShow;

    logger_init();
    LOG_INFO("Starting PEEK - Network Monitor");

    // Check for administrator privileges
    if (!IsRunningAsAdmin()) {
        LOG_WARNING("Application not running with administrator privileges");
        int result = MessageBoxW(NULL,
            L"PEEK is not running with administrator privileges.\n\n"
            L"Some process names may not be available (showing as [System] PID:xxx).\n\n"
            L"For full functionality, please run PEEK as administrator.\n\n"
            L"Do you want to continue anyway?",
            L"PEEK - Administrator Privileges Required",
            MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON1);

        if (result == IDNO) {
            LOG_INFO("User chose to exit due to missing admin privileges");
            return 0;
        }
    } else {
        LOG_SUCCESS("Running with administrator privileges");
    }

    if (network_init() != 0) {
        LOG_ERROR("Failed to initialize network module");
        MessageBox(NULL, L"Failed to initialize network module", L"Error", MB_ICONERROR);
        return 1;
    }

    if (gui_init(hInstance) != 0) {
        LOG_ERROR("Failed to initialize GUI");
        network_cleanup();
        return 1;
    }

    if (gui_create_window(hInstance) == NULL) {
        LOG_ERROR("Failed to create main window");
        network_cleanup();
        return 1;
    }

    LOG_SUCCESS("Application started successfully");
    LOG_INFO("Click 'Start' to begin monitoring network connections");

    int result = gui_run();

    LOG_INFO("Shutting down...");
    network_cleanup();
    LOG_SUCCESS("Application closed");

    return result;
}