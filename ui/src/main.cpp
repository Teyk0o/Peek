/*
 * PEEK UI Host (peek.exe)
 *
 * Win32 application that hosts a WebView2 control
 * Loads embedded React frontend from resources
 * Communicates with peekd.exe daemon via Named Pipes IPC
 */

#include <windows.h>
#include <wrl.h>
#include <webview2.h>

#include "webview.h"
#include "window.h"
#include "ipc-client.h"

using namespace Microsoft::WRL;

/* Forward declarations */
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR pCmdLine, int nCmdShow);

/*
 * Main entry point
 */
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR pCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)pCmdLine;
    (void)nCmdShow;

    /* Initialize COM */
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    /* Create main window */
    HWND hwndMainWindow = window_create(hInstance);
    if (!hwndMainWindow) {
        MessageBoxW(nullptr, L"Failed to create main window", L"Error", MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    /* Initialize WebView2 */
    if (webview_init(hwndMainWindow) != 0) {
        MessageBoxW(nullptr, L"Failed to initialize WebView2", L"Error", MB_ICONERROR);
        DestroyWindow(hwndMainWindow);
        CoUninitialize();
        return 1;
    }

    /* Initialize IPC client (connect to daemon) */
    if (ipc_client_init() != 0) {
        /* Non-fatal: UI can work without daemon initially */
        MessageBoxW(nullptr,
            L"Warning: Could not connect to Peek daemon.\n\n"
            L"Some features may be unavailable.\n\n"
            L"Make sure 'peekd.exe --install-service' was run as Administrator.",
            L"Warning", MB_ICONWARNING);
    }

    /* Show window */
    ShowWindow(hwndMainWindow, SW_SHOW);
    UpdateWindow(hwndMainWindow);

    /* Main message loop */
    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    /* Cleanup */
    ipc_client_cleanup();
    webview_cleanup();
    CoUninitialize();

    return (int)msg.wParam;
}
