/*
 * Main Window Implementation
 *
 * Manages the main application window and WebView2 resizing
 */

#include "window.h"
#include "webview.h"
#include <webview2.h>
#include <wrl.h>

using namespace Microsoft::WRL;

/* Store WebView controller pointer in window user data */
static const wchar_t* WEBVIEW_CONTROLLER_PROP = L"WebViewController";

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

/*
 * Create main window
 */
HWND window_create(HINSTANCE hInstance) {
    /* Register window class */
    const wchar_t CLASS_NAME[] = L"Peek.MainWindow";

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.style = CS_HREDRAW | CS_VREDRAW;

    RegisterClassW(&wc);

    /* Create window centered on screen */
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int windowWidth = 1200;
    int windowHeight = 800;
    int posX = (screenWidth - windowWidth) / 2;
    int posY = (screenHeight - windowHeight) / 2;

    HWND hwnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        CLASS_NAME,
        L"Peek - Network Security Monitor",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        posX, posY, windowWidth, windowHeight,
        nullptr, nullptr, hInstance, nullptr
    );

    if (!hwnd) {
        return nullptr;
    }

    return hwnd;
}

/*
 * Destroy window
 */
void window_destroy(HWND hwnd) {
    if (hwnd) {
        DestroyWindow(hwnd);
    }
}

/*
 * Window message procedure
 */
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    /* For WM_CREATE, lParam contains CREATESTRUCT */
    if (uMsg == WM_CREATE) {
        /* Set up the message queue for async operations */
        return 0;
    }

    switch (uMsg) {
        case WM_SIZE: {
            /* Resize WebView2 to fill the window client area */
            RECT bounds;
            GetClientRect(hwnd, &bounds);

            /* We need to resize the WebView controller
             * This will be handled when WebView is created
             * For now, just store the bounds for later use
             */

            return 0;
        }

        case WM_CLOSE: {
            /* User clicked close button */
            PostMessageW(hwnd, WM_DESTROY, 0, 0);
            return 0;
        }

        case WM_DESTROY: {
            /* Window is being destroyed */
            PostQuitMessage(0);
            return 0;
        }

        case WM_GETMINMAXINFO: {
            /* Set minimum window size */
            MINMAXINFO* pMinMaxInfo = (MINMAXINFO*)lParam;
            pMinMaxInfo->ptMinTrackSize.x = 800;
            pMinMaxInfo->ptMinTrackSize.y = 600;
            return 0;
        }

        case WM_DPICHANGED: {
            /* Handle DPI changes (for multi-monitor setups) */
            /* We would need to resize WebView2 here if DPI changes */
            return 0;
        }

        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
}
