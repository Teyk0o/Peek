/*
 * WebView2 Initialization and Management
 */

#ifndef PEEK_UI_WEBVIEW_H
#define PEEK_UI_WEBVIEW_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialize WebView2 and load frontend
 * params: hwnd - Parent window handle
 * Returns: 0 on success, non-zero on error
 */
int webview_init(HWND hwnd);

/*
 * Cleanup and release WebView2 resources
 */
void webview_cleanup(void);

/*
 * Execute JavaScript in the WebView
 */
int webview_execute_script(const wchar_t *script);

/*
 * Post message to WebView (for IPC from daemon)
 */
int webview_post_message(const char *message);

#ifdef __cplusplus
}
#endif

#endif /* PEEK_UI_WEBVIEW_H */
