/*
 * Main Window Implementation
 */

#include "window.h"

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

    RegisterClassW(&wc);

    /* Create window */
    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"Peek - Network Security Monitor",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1200, 800,
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
    switch (uMsg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_SIZE:
            /* Resize WebView to fill window */
            /* TODO: Implement WebView resize */
            return 0;

        case WM_CLOSE:
            PostMessageW(hwnd, WM_DESTROY, 0, 0);
            return 0;

        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
}
