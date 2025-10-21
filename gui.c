/*
* PEEK - Network Monitor
*/

#include "gui.h"
#include "logger.h"
#include <stdio.h>
#include <dwmapi.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#define CLASS_NAME L"PeekWindowClass"
#define WINDOW_TITLE L"PEEK - Network Monitor"

#define ID_LISTVIEW 1001
#define ID_BTN_START 1002
#define ID_BTN_STOP 1003
#define ID_BTN_CLEAR 1004
#define ID_STATUSBAR 1005
#define ID_TIMER 1006

static HWND g_hwndMain = NULL;
static HWND g_hwndListView = NULL;
static HWND g_hwndStatusBar = NULL;
static HWND g_hwndBtnStart = NULL;
static HWND g_hwndBtnStop = NULL;
static HWND g_hwndBtnClear = NULL;
static HINSTANCE g_hInstance = NULL;
static BOOL g_monitoring = FALSE;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateControls(HWND hwnd);
void InitializeListView(void);
void UpdateMonitoringState(BOOL monitoring);

int gui_init(HINSTANCE hInstance) {
    g_hInstance = hInstance;

    LOG_INFO("Initializing GUI...");

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(COLOR_BG_DARK);
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)) {
        LOG_ERROR("Failed to register window class");
        return -1;
    }

    LOG_SUCCESS("GUI initialized");
    return 0;
}

HWND gui_create_window(HINSTANCE hInstance) {
    g_hwndMain = CreateWindowEx(
        0,
        CLASS_NAME,
        WINDOW_TITLE,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        1200, 700,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!g_hwndMain) {
        LOG_ERROR("Failed to create main window");
        return NULL;
    }

    gui_enable_dark_mode(g_hwndMain);

    CreateControls(g_hwndMain);

    ShowWindow(g_hwndMain, SW_SHOW);
    UpdateWindow(g_hwndMain);

    LOG_SUCCESS("Main window created");
    return g_hwndMain;
}

void CreateControls(HWND hwnd) {
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);

    int btnWidth = 100;
    int btnHeight = 35;
    int btnMargin = 10;
    int btnY = 10;

    g_hwndBtnStart = CreateWindowW(
        L"BUTTON",
        L"Start",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnMargin, btnY, btnWidth, btnHeight,
        hwnd,
        (HMENU)ID_BTN_START,
        g_hInstance,
        NULL
    );

    g_hwndBtnStop = CreateWindowW(
        L"BUTTON",
        L"Stop",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_DISABLED,
        btnMargin * 2 + btnWidth, btnY, btnWidth, btnHeight,
        hwnd,
        (HMENU)ID_BTN_STOP,
        g_hInstance,
        NULL
    );

    g_hwndBtnClear = CreateWindowW(
        L"BUTTON",
        L"Clear",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        btnMargin * 3 + btnWidth * 2, btnY, btnWidth, btnHeight,
        hwnd,
        (HMENU)ID_BTN_CLEAR,
        g_hInstance,
        NULL
    );

    int listY = btnY + btnHeight + btnMargin;
    g_hwndListView = CreateWindowEx(
        0,
        WC_LISTVIEW,
        NULL,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_BORDER,
        btnMargin, listY,
        rcClient.right - btnMargin * 2,
        rcClient.bottom - listY - 40,
        hwnd,
        (HMENU)ID_LISTVIEW,
        g_hInstance,
        NULL
    );

    InitializeListView();

    g_hwndStatusBar = CreateWindowEx(
        0,
        STATUSCLASSNAME,
        NULL,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        hwnd,
        (HMENU)ID_STATUSBAR,
        g_hInstance,
        NULL
    );

    int statusParts[] = {200, 400, 600, -1};
    SendMessage(g_hwndStatusBar, SB_SETPARTS, 4, (LPARAM)statusParts);
    SendMessage(g_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"Ready");
}

void InitializeListView(void) {
    ListView_SetExtendedListViewStyle(g_hwndListView,
        LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

    LVCOLUMN lvc = {0};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
    lvc.fmt = LVCFMT_LEFT;

    // Time column
    lvc.pszText = L"Time";
    lvc.cx = 80;
    ListView_InsertColumn(g_hwndListView, 0, &lvc);

    // Remote Address column
    lvc.pszText = L"Remote Address";
    lvc.cx = 180;
    ListView_InsertColumn(g_hwndListView, 1, &lvc);

    // Remote Port column
    lvc.pszText = L"Port";
    lvc.cx = 80;
    ListView_InsertColumn(g_hwndListView, 2, &lvc);

    // Local Address column
    lvc.pszText = L"Local Address";
    lvc.cx = 180;
    ListView_InsertColumn(g_hwndListView, 3, &lvc);

    // Local Port column
    lvc.pszText = L"Local Port";
    lvc.cx = 80;
    ListView_InsertColumn(g_hwndListView, 4, &lvc);

    // Process column
    lvc.pszText = L"Process";
    lvc.cx = 200;
    ListView_InsertColumn(g_hwndListView, 5, &lvc);

    // PID column
    lvc.pszText = L"PID";
    lvc.cx = 80;
    ListView_InsertColumn(g_hwndListView, 6, &lvc);
}

void gui_add_connection(const NetworkConnection* conn) {
    if (!g_hwndListView || !conn) return;

    wchar_t time_str[32];
    swprintf(time_str, 32, L"%02d:%02d:%02d",
             conn->timestamp.wHour,
             conn->timestamp.wMinute,
             conn->timestamp.wSecond);

    char remote_ip[32];
    network_format_ip(conn->remote_addr, remote_ip, sizeof(remote_ip));
    wchar_t w_remote_ip[32];
    MultiByteToWideChar(CP_ACP, 0, remote_ip, -1, w_remote_ip, 32);

    wchar_t remote_port[16];
    swprintf(remote_port, 16, L"%lu", conn->remote_port);

    char local_ip[32];
    network_format_ip(conn->local_addr, local_ip, sizeof(local_ip));
    wchar_t w_local_ip[32];
    MultiByteToWideChar(CP_ACP, 0, local_ip, -1, w_local_ip, 32);

    wchar_t local_port[16];
    swprintf(local_port, 16, L"%lu", conn->local_port);

    wchar_t w_process[MAX_PROCESS_NAME];
    MultiByteToWideChar(CP_ACP, 0, conn->process_name, -1, w_process, MAX_PROCESS_NAME);

    wchar_t pid_str[16];
    swprintf(pid_str, 16, L"%lu", conn->pid);

    LVITEM lvi = {0};
    lvi.mask = LVIF_TEXT;
    lvi.iItem = 0;
    lvi.iSubItem = 0;
    lvi.pszText = time_str;
    int index = ListView_InsertItem(g_hwndListView, &lvi);

    ListView_SetItemText(g_hwndListView, index, 1, w_remote_ip);
    ListView_SetItemText(g_hwndListView, index, 2, remote_port);
    ListView_SetItemText(g_hwndListView, index, 3, w_local_ip);
    ListView_SetItemText(g_hwndListView, index, 4, local_port);
    ListView_SetItemText(g_hwndListView, index, 5, w_process);
    ListView_SetItemText(g_hwndListView, index, 6, pid_str);

    ListView_EnsureVisible(g_hwndListView, 0, FALSE);
}

void gui_update_stats(const NetworkStats* stats) {
    if (!g_hwndStatusBar || !stats) return;

    wchar_t status_text[256];

    swprintf(status_text, 256, L"Monitoring");
    SendMessage(g_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)status_text);

    swprintf(status_text, 256, L"New: %d", stats->new_connections);
    SendMessage(g_hwndStatusBar, SB_SETTEXT, 1, (LPARAM)status_text);

    swprintf(status_text, 256, L"Active: %d", stats->active_connections);
    SendMessage(g_hwndStatusBar, SB_SETTEXT, 2, (LPARAM)status_text);

    swprintf(status_text, 256, L"Total: %d", stats->total_connections);
    SendMessage(g_hwndStatusBar, SB_SETTEXT, 3, (LPARAM)status_text);
}

void gui_clear_list(void) {
    if (g_hwndListView) {
        ListView_DeleteAllItems(g_hwndListView);
        LOG_INFO("ListView cleared");
    }
}

void gui_enable_dark_mode(HWND hwnd) {
    BOOL value = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
}

void UpdateMonitoringState(BOOL monitoring) {
    g_monitoring = monitoring;

    if (monitoring) {
        EnableWindow(g_hwndBtnStart, FALSE);
        EnableWindow(g_hwndBtnStop, TRUE);

        // Display all existing connections when starting
        NetworkConnection* existing_conns = NULL;
        int existing_count = 0;

        if (network_get_all_seen_connections(&existing_conns, &existing_count) == 0) {
            for (int i = 0; i < existing_count; i++) {
                gui_add_connection(&existing_conns[i]);
            }
            free(existing_conns);
            LOG_INFO("Displayed %d existing connection(s)", existing_count);
        }

        // Update stats
        NetworkStats stats;
        network_get_stats(&stats);
        gui_update_stats(&stats);

        SetTimer(g_hwndMain, ID_TIMER, 500, NULL);  // Check every 500ms
        LOG_INFO("Monitoring started");
    } else {
        EnableWindow(g_hwndBtnStart, TRUE);
        EnableWindow(g_hwndBtnStop, FALSE);
        KillTimer(g_hwndMain, ID_TIMER);
        LOG_INFO("Monitoring stopped");
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            return 0;

        case WM_SIZE: {
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);

            if (g_hwndStatusBar) {
                SendMessage(g_hwndStatusBar, WM_SIZE, 0, 0);
            }

            if (g_hwndListView) {
                RECT rcStatus;
                GetWindowRect(g_hwndStatusBar, &rcStatus);
                int statusHeight = rcStatus.bottom - rcStatus.top;

                int btnMargin = 10;
                int btnHeight = 35;
                int listY = btnMargin + btnHeight + btnMargin;

                SetWindowPos(g_hwndListView, NULL,
                    btnMargin, listY,
                    rcClient.right - btnMargin * 2,
                    rcClient.bottom - listY - statusHeight - btnMargin,
                    SWP_NOZORDER);
            }
            return 0;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);

            switch (wmId) {
                case ID_BTN_START:
                    UpdateMonitoringState(TRUE);
                    break;

                case ID_BTN_STOP:
                    UpdateMonitoringState(FALSE);
                    break;

                case ID_BTN_CLEAR:
                    gui_clear_list();
                    break;
            }
            return 0;
        }

        case WM_TIMER:
            if (wParam == ID_TIMER && g_monitoring) {
                NetworkConnection* new_conns = NULL;
                int count = 0;

                if (network_check_new_connections(&new_conns, &count) == 0) {
                    for (int i = 0; i < count; i++) {
                        gui_add_connection(&new_conns[i]);

                        char remote_ip[32], local_ip[32];
                        network_format_ip(new_conns[i].remote_addr, remote_ip, sizeof(remote_ip));
                        network_format_ip(new_conns[i].local_addr, local_ip, sizeof(local_ip));

                        LOG_SUCCESS("NEW CONNECTION: %s:%lu -> %s:%lu | %s (PID: %lu)",
                            remote_ip, new_conns[i].remote_port,
                            local_ip, new_conns[i].local_port,
                            new_conns[i].process_name, new_conns[i].pid);
                    }

                    if (count > 0) {
                        NetworkStats stats;
                        network_get_stats(&stats);
                        gui_update_stats(&stats);
                    }

                    free(new_conns);
                }
            }
            return 0;

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, COLOR_TEXT_PRIMARY);
            SetBkColor(hdc, COLOR_BG_DARK);
            return (LRESULT)CreateSolidBrush(COLOR_BG_DARK);
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int gui_run(void) {
    MSG msg = {0};

    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}