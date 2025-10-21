/*
* PEEK - Network Monitor
*/

#ifndef PEEK_GUI_H
#define PEEK_GUI_H

#include "network.h"
#include <windows.h>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")

#define COLOR_BG_DARK       RGB(30, 30, 30)
#define COLOR_BG_LIGHT      RGB(45, 45, 45)
#define COLOR_TEXT_PRIMARY  RGB(240, 240, 240)
#define COLOR_TEXT_SECONDARY RGB(180, 180, 180)
#define COLOR_ACCENT        RGB(0, 120, 212)
#define COLOR_SUCCESS       RGB(16, 185, 129)
#define COLOR_WARNING       RGB(245, 158, 11)
#define COLOR_ERROR         RGB(239, 68, 68)

int gui_init(HINSTANCE hInstance);

HWND gui_create_window(HINSTANCE hInstance);

int gui_run(void);

void gui_add_connection(const NetworkConnection* conn);

void gui_update_stats(const NetworkStats* stats);

void gui_clear_list(void);

void gui_enable_dark_mode(HWND hwnd);

#endif