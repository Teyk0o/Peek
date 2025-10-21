/*
* PEEK - Network Monitor
*/

#ifndef PEEK_GUI_H
#define PEEK_GUI_H

#include "network.h"
#include <windows.h>
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

#define COLOR_BG_DARK       RGB(255, 255, 255)
#define COLOR_BG_LIGHT      RGB(245, 245, 245)
#define COLOR_TEXT_PRIMARY  RGB(0, 0, 0)
#define COLOR_TEXT_SECONDARY RGB(100, 100, 100)
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

#endif