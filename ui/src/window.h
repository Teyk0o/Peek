/*
 * Main Window Management
 */

#ifndef PEEK_UI_WINDOW_H
#define PEEK_UI_WINDOW_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Create main application window
 * Returns: HWND on success, nullptr on error
 */
HWND window_create(HINSTANCE hInstance);

/*
 * Destroy window and cleanup
 */
void window_destroy(HWND hwnd);

#ifdef __cplusplus
}
#endif

#endif /* PEEK_UI_WINDOW_H */
