/*
 * WebView2 Implementation
 */

#include "webview.h"
#include <webview2.h>
#include <wrl.h>
#include <string>

using namespace Microsoft::WRL;

/* Global WebView controller and environment */
static ComPtr<ICoreWebView2Controller> g_webviewController;
static ComPtr<ICoreWebView2> g_webview;

/*
 * Initialize WebView2
 */
int webview_init(HWND hwnd) {
    /* TODO: Implement full WebView2 initialization
     *
     * Steps:
     * 1. Create WebView2 environment
     * 2. Create WebView2 controller
     * 3. Load frontend from resources (frontend/dist/index.html)
     * 4. Setup IPC message handler
     * 5. Resize to parent window
     */

    return 0; /* Stub implementation */
}

/*
 * Cleanup
 */
void webview_cleanup(void) {
    if (g_webviewController) {
        g_webviewController->Close();
        g_webviewController = nullptr;
    }

    g_webview = nullptr;
}

/*
 * Execute JavaScript
 */
int webview_execute_script(const wchar_t *script) {
    if (!g_webview) {
        return -1;
    }

    /* TODO: Execute script and wait for result */

    return 0;
}

/*
 * Post message to WebView (from daemon)
 */
int webview_post_message(const char *message) {
    if (!g_webview) {
        return -1;
    }

    /* TODO: Convert to wide string and post to WebView
     * This will trigger window.chrome.webview.postMessage in the frontend
     */

    return 0;
}
