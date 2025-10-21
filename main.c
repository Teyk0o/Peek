/*
* PEEK - Network Monitor
*/

#include "network.h"
#include "gui.h"
#include "logger.h"
#include <windows.h>

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR pCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)pCmdLine;
    (void)nCmdShow;

    logger_init();
    LOG_INFO("Starting PEEK - Network Monitor");

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