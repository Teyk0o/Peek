/*
* PEEK - Network Monitor
*/

#include "gui.h"
#include "logger.h"
#include "resource.h"
#include <stdio.h>

#define CLASS_NAME L"PeekWindowClass"
#define WINDOW_TITLE L"PEEK - Network Monitor"

#define ID_LISTVIEW 1001
#define ID_BTN_START 1002
#define ID_BTN_STOP 1003
#define ID_BTN_CLEAR 1004
#define ID_STATUSBAR 1005
#define ID_TIMER 1006
#define ID_TIMER_FLASH 1007
#define ID_RADIO_ALL 1008
#define ID_RADIO_OUTBOUND 1009
#define ID_RADIO_INBOUND 1010
#define ID_CHECK_LOCALHOST 1011
#define ID_COMBO_PROTOCOL 1012

#define FLASH_DURATION_MS 1500
#define MAX_HIGHLIGHTED_ITEMS 100

typedef struct {
    int item_index;
    DWORD start_time;
} HighlightedItem;

static HWND g_hwndMain = NULL;
static HWND g_hwndListView = NULL;
static HWND g_hwndStatusBar = NULL;
static HWND g_hwndBtnStart = NULL;
static HWND g_hwndBtnStop = NULL;
static HWND g_hwndBtnClear = NULL;
static HWND g_hwndRadioAll = NULL;
static HWND g_hwndRadioOutbound = NULL;
static HWND g_hwndRadioInbound = NULL;
static HWND g_hwndCheckLocalhost = NULL;
static HWND g_hwndComboProtocol = NULL;
static HINSTANCE g_hInstance = NULL;
static BOOL g_monitoring = FALSE;
static ConnectionDirection g_filter = CONN_OUTBOUND; // Default to outbound only
static BOOL g_show_localhost = TRUE; // Default to show localhost connections

// Protocol filter: -1 = All, PROTO_TCP = TCP only, PROTO_UDP = UDP only
static int g_protocol_filter = -1; // Default to show all protocols

static HighlightedItem g_highlighted_items[MAX_HIGHLIGHTED_ITEMS];
static int g_highlighted_count = 0;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateControls(HWND hwnd);
void InitializeListView(void);
void UpdateMonitoringState(BOOL monitoring);
void AddHighlightedItem(int item_index);
void UpdateHighlights(void);
COLORREF GetHighlightColor(int item_index, BOOL* is_highlighted);
void RefreshListViewWithFilter(void);

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
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;

    // Load custom icon from resources
    wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
    wc.hIconSm = (HICON)LoadImage(hInstance, MAKEINTRESOURCE(IDI_APP_ICON),
                                   IMAGE_ICON, 16, 16, 0);

    // Fallback to default if custom icon fails to load
    if (!wc.hIcon) {
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        LOG_WARNING("Failed to load custom icon, using default");
    }
    if (!wc.hIconSm) {
        wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    }

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

    // Radio buttons for filtering
    int radioX = btnMargin * 4 + btnWidth * 3 + 20;
    int radioWidth = 90;

    g_hwndRadioOutbound = CreateWindowW(
        L"BUTTON",
        L"Outbound",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
        radioX, btnY + 5, radioWidth, 25,
        hwnd,
        (HMENU)ID_RADIO_OUTBOUND,
        g_hInstance,
        NULL
    );

    g_hwndRadioInbound = CreateWindowW(
        L"BUTTON",
        L"Inbound",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        radioX + radioWidth, btnY + 5, radioWidth, 25,
        hwnd,
        (HMENU)ID_RADIO_INBOUND,
        g_hInstance,
        NULL
    );

    g_hwndRadioAll = CreateWindowW(
        L"BUTTON",
        L"All",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        radioX + radioWidth * 2, btnY + 5, 60, 25,
        hwnd,
        (HMENU)ID_RADIO_ALL,
        g_hInstance,
        NULL
    );

    // Set default to Outbound
    SendMessage(g_hwndRadioOutbound, BM_SETCHECK, BST_CHECKED, 0);

    // Checkbox for localhost filtering
    int checkX = radioX + radioWidth * 2 + 70;
    g_hwndCheckLocalhost = CreateWindowW(
        L"BUTTON",
        L"Show Localhost",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        checkX, btnY + 5, 130, 25,
        hwnd,
        (HMENU)ID_CHECK_LOCALHOST,
        g_hInstance,
        NULL
    );

    // Set default to checked (show localhost)
    SendMessage(g_hwndCheckLocalhost, BM_SETCHECK, BST_CHECKED, 0);

    // Protocol filter ComboBox
    int comboX = checkX + 140;
    CreateWindowW(L"STATIC", L"Protocol:",
        WS_CHILD | WS_VISIBLE,
        comboX, btnY + 8, 60, 20,
        hwnd, NULL, g_hInstance, NULL);

    g_hwndComboProtocol = CreateWindowW(
        L"COMBOBOX",
        NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        comboX + 65, btnY + 5, 80, 100,
        hwnd,
        (HMENU)ID_COMBO_PROTOCOL,
        g_hInstance,
        NULL
    );

    // Add items to ComboBox
    SendMessageW(g_hwndComboProtocol, CB_ADDSTRING, 0, (LPARAM)L"All");
    SendMessageW(g_hwndComboProtocol, CB_ADDSTRING, 0, (LPARAM)L"TCP");
    SendMessageW(g_hwndComboProtocol, CB_ADDSTRING, 0, (LPARAM)L"UDP");

    // Set default to "All"
    SendMessageW(g_hwndComboProtocol, CB_SETCURSEL, 0, 0);

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

    // Direction column
    lvc.pszText = L"Dir";
    lvc.cx = 50;
    lvc.fmt = LVCFMT_CENTER;
    ListView_InsertColumn(g_hwndListView, 1, &lvc);

    // Protocol column
    lvc.pszText = L"Proto";
    lvc.cx = 55;
    lvc.fmt = LVCFMT_CENTER;
    ListView_InsertColumn(g_hwndListView, 2, &lvc);

    // Remote Address column
    lvc.pszText = L"Remote Address";
    lvc.cx = 200;
    lvc.fmt = LVCFMT_LEFT;
    ListView_InsertColumn(g_hwndListView, 3, &lvc);

    // Remote Port column
    lvc.pszText = L"Port";
    lvc.cx = 60;
    ListView_InsertColumn(g_hwndListView, 4, &lvc);

    // Local Address column
    lvc.pszText = L"Local Address";
    lvc.cx = 200;
    ListView_InsertColumn(g_hwndListView, 5, &lvc);

    // Local Port column
    lvc.pszText = L"Local Port";
    lvc.cx = 80;
    ListView_InsertColumn(g_hwndListView, 6, &lvc);

    // Process column
    lvc.pszText = L"Process";
    lvc.cx = 150;
    ListView_InsertColumn(g_hwndListView, 7, &lvc);

    // PID column
    lvc.pszText = L"PID";
    lvc.cx = 70;
    ListView_InsertColumn(g_hwndListView, 8, &lvc);

    // Count column
    lvc.pszText = L"Count";
    lvc.cx = 60;
    lvc.fmt = LVCFMT_CENTER;
    ListView_InsertColumn(g_hwndListView, 9, &lvc);
}

void gui_add_connection(const NetworkConnection* conn) {
    if (!g_hwndListView || !conn) return;

    // Apply direction filter
    if (g_filter != CONN_UNKNOWN && conn->direction != g_filter) {
        return; // Skip this connection based on direction filter
    }

    // Apply localhost filter
    if (!g_show_localhost && conn->is_localhost) {
        return; // Skip localhost connections if filter is disabled
    }

    // Apply protocol filter
    if (g_protocol_filter != -1 && conn->protocol != g_protocol_filter) {
        return; // Skip this connection based on protocol filter
    }

    // Prepare connection data
    char remote_ip[64];
    wchar_t w_remote_ip[64];
    char local_ip[64];
    wchar_t w_local_ip[64];

    // Format IP addresses based on version
    if (conn->ip_version == IP_V4) {
        network_format_ip(conn->remote_addr, remote_ip, sizeof(remote_ip));
        network_format_ip(conn->local_addr, local_ip, sizeof(local_ip));
    } else { // IPv6
        network_format_ipv6(conn->remote_addr_v6, remote_ip, sizeof(remote_ip));
        network_format_ipv6(conn->local_addr_v6, local_ip, sizeof(local_ip));
    }

    MultiByteToWideChar(CP_ACP, 0, remote_ip, -1, w_remote_ip, 64);
    MultiByteToWideChar(CP_ACP, 0, local_ip, -1, w_local_ip, 64);

    wchar_t remote_port[16];
    if (conn->remote_port != 0) {
        swprintf(remote_port, 16, L"%lu", conn->remote_port);
    } else {
        wcscpy(remote_port, L"-");
    }

    wchar_t local_port[16];
    swprintf(local_port, 16, L"%lu", conn->local_port);

    wchar_t w_process[MAX_PROCESS_NAME];
    MultiByteToWideChar(CP_ACP, 0, conn->process_name, -1, w_process, MAX_PROCESS_NAME);

    wchar_t pid_str[16];
    swprintf(pid_str, 16, L"%lu", conn->pid);

    wchar_t direction_str[4];
    if (conn->direction == CONN_OUTBOUND) {
        wcscpy(direction_str, L"OUT");
    } else if (conn->direction == CONN_INBOUND) {
        wcscpy(direction_str, L"IN");
    } else {
        wcscpy(direction_str, L"?");
    }

    wchar_t protocol_str[8];
    wcscpy(protocol_str, (conn->protocol == PROTO_TCP) ? L"TCP" : L"UDP");

    // Check if this connection already exists (same process, protocol, remote_addr, remote_port)
    int item_count = ListView_GetItemCount(g_hwndListView);
    for (int i = 0; i < item_count; i++) {
        wchar_t existing_process[MAX_PROCESS_NAME];
        wchar_t existing_protocol[16];
        wchar_t existing_remote_ip[64];
        wchar_t existing_remote_port[16];
        wchar_t existing_count[16];

        ListView_GetItemText(g_hwndListView, i, 7, existing_process, MAX_PROCESS_NAME);
        ListView_GetItemText(g_hwndListView, i, 2, existing_protocol, 16);
        ListView_GetItemText(g_hwndListView, i, 3, existing_remote_ip, 64);
        ListView_GetItemText(g_hwndListView, i, 4, existing_remote_port, 16);
        ListView_GetItemText(g_hwndListView, i, 9, existing_count, 16);

        // If same process, protocol and same remote endpoint, increment count
        if (wcscmp(existing_process, w_process) == 0 &&
            wcscmp(existing_protocol, protocol_str) == 0 &&
            wcscmp(existing_remote_ip, w_remote_ip) == 0 &&
            wcscmp(existing_remote_port, remote_port) == 0) {

            // Parse existing count
            int count = 1;
            if (wcslen(existing_count) > 0) {
                count = _wtoi(existing_count);
            }
            count++;

            // Update count
            wchar_t new_count[16];
            swprintf(new_count, 16, L"%d", count);
            ListView_SetItemText(g_hwndListView, i, 9, new_count);

            // Update timestamp to the latest
            wchar_t time_str[32];
            swprintf(time_str, 32, L"%02d:%02d:%02d",
                     conn->timestamp.wHour,
                     conn->timestamp.wMinute,
                     conn->timestamp.wSecond);
            ListView_SetItemText(g_hwndListView, i, 0, time_str);

            // Trigger highlight effect
            AddHighlightedItem(i);

            return; // Connection grouped, exit
        }
    }

    // New unique connection - add at the end (newest at bottom)
    wchar_t time_str[32];
    swprintf(time_str, 32, L"%02d:%02d:%02d",
             conn->timestamp.wHour,
             conn->timestamp.wMinute,
             conn->timestamp.wSecond);

    LVITEM lvi = {0};
    lvi.mask = LVIF_TEXT;
    lvi.iItem = item_count; // Insert at end
    lvi.iSubItem = 0;
    lvi.pszText = time_str;
    int index = ListView_InsertItem(g_hwndListView, &lvi);

    ListView_SetItemText(g_hwndListView, index, 1, direction_str);
    ListView_SetItemText(g_hwndListView, index, 2, protocol_str);
    ListView_SetItemText(g_hwndListView, index, 3, w_remote_ip);
    ListView_SetItemText(g_hwndListView, index, 4, remote_port);
    ListView_SetItemText(g_hwndListView, index, 5, w_local_ip);
    ListView_SetItemText(g_hwndListView, index, 6, local_port);
    ListView_SetItemText(g_hwndListView, index, 7, w_process);
    ListView_SetItemText(g_hwndListView, index, 8, pid_str);
    ListView_SetItemText(g_hwndListView, index, 9, L"1");

    // Trigger highlight effect for new connection
    AddHighlightedItem(index);

    // Scroll to bottom to show newest connection
    ListView_EnsureVisible(g_hwndListView, index, FALSE);
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
        g_highlighted_count = 0; // Clear all highlights
        LOG_INFO("ListView cleared");
    }
}

void RefreshListViewWithFilter(void) {
    if (!g_hwndListView) return;

    // Clear current list and highlights
    ListView_DeleteAllItems(g_hwndListView);
    g_highlighted_count = 0;

    // Get all seen connections and re-add them with the current filter
    NetworkConnection* all_conns = NULL;
    int count = 0;

    if (network_get_all_seen_connections(&all_conns, &count) == 0) {
        for (int i = 0; i < count; i++) {
            gui_add_connection(&all_conns[i]);
        }
        free(all_conns);
        LOG_INFO("ListView refreshed with filter (%d connections)", count);
    }

    // Update stats
    NetworkStats stats;
    network_get_stats(&stats);
    gui_update_stats(&stats);
}

void AddHighlightedItem(int item_index) {
    // Check if already highlighted
    for (int i = 0; i < g_highlighted_count; i++) {
        if (g_highlighted_items[i].item_index == item_index) {
            // Reset the timer for this item
            g_highlighted_items[i].start_time = GetTickCount();
            return;
        }
    }

    // Add new highlighted item
    if (g_highlighted_count < MAX_HIGHLIGHTED_ITEMS) {
        g_highlighted_items[g_highlighted_count].item_index = item_index;
        g_highlighted_items[g_highlighted_count].start_time = GetTickCount();
        g_highlighted_count++;
    }
}

void UpdateHighlights(void) {
    DWORD current_time = GetTickCount();
    int write_index = 0;

    // Remove expired highlights
    for (int i = 0; i < g_highlighted_count; i++) {
        DWORD elapsed = current_time - g_highlighted_items[i].start_time;
        if (elapsed < FLASH_DURATION_MS) {
            if (write_index != i) {
                g_highlighted_items[write_index] = g_highlighted_items[i];
            }
            write_index++;
        }
    }

    g_highlighted_count = write_index;

    // Redraw ListView if there are active highlights
    if (g_highlighted_count > 0 || write_index > 0) {
        InvalidateRect(g_hwndListView, NULL, FALSE);
    }
}

COLORREF GetHighlightColor(int item_index, BOOL* is_highlighted) {
    DWORD current_time = GetTickCount();

    // Determine base background color (alternating rows)
    COLORREF base_color = (item_index % 2 == 0) ? COLOR_BG_DARK : COLOR_BG_LIGHT;

    for (int i = 0; i < g_highlighted_count; i++) {
        if (g_highlighted_items[i].item_index == item_index) {
            DWORD elapsed = current_time - g_highlighted_items[i].start_time;
            if (elapsed < FLASH_DURATION_MS) {
                // Calculate fade factor (1.0 at start, 0.0 at end)
                float fade = 1.0f - ((float)elapsed / FLASH_DURATION_MS);

                // Extract RGB components from base color
                int bg_r = GetRValue(base_color);
                int bg_g = GetGValue(base_color);
                int bg_b = GetBValue(base_color);

                // Highlight color (blue accent)
                int hl_r = 0, hl_g = 120, hl_b = 212;

                // Interpolate between highlight and background
                int r = (int)(hl_r * fade + bg_r * (1.0f - fade) + 0.5f);
                int g = (int)(hl_g * fade + bg_g * (1.0f - fade) + 0.5f);
                int b = (int)(hl_b * fade + bg_b * (1.0f - fade) + 0.5f);

                *is_highlighted = TRUE;
                return RGB(r, g, b);
            }
        }
    }

    *is_highlighted = FALSE;
    return base_color;
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
        SetTimer(g_hwndMain, ID_TIMER_FLASH, 50, NULL);  // Update highlights every 50ms for smooth fade
        LOG_INFO("Monitoring started");
    } else {
        EnableWindow(g_hwndBtnStart, TRUE);
        EnableWindow(g_hwndBtnStop, FALSE);
        KillTimer(g_hwndMain, ID_TIMER);
        KillTimer(g_hwndMain, ID_TIMER_FLASH);
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
            int wmEvent = HIWORD(wParam);

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

                case ID_RADIO_OUTBOUND:
                    g_filter = CONN_OUTBOUND;
                    LOG_INFO("Filter set to: Outbound");
                    RefreshListViewWithFilter();
                    break;

                case ID_RADIO_INBOUND:
                    g_filter = CONN_INBOUND;
                    LOG_INFO("Filter set to: Inbound");
                    RefreshListViewWithFilter();
                    break;

                case ID_RADIO_ALL:
                    g_filter = CONN_UNKNOWN; // Use UNKNOWN as "all"
                    LOG_INFO("Filter set to: All");
                    RefreshListViewWithFilter();
                    break;

                case ID_CHECK_LOCALHOST:
                    g_show_localhost = (SendMessage(g_hwndCheckLocalhost, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    LOG_INFO("Localhost filter: %s", g_show_localhost ? "Enabled" : "Disabled");
                    RefreshListViewWithFilter();
                    break;

                case ID_COMBO_PROTOCOL:
                    if (wmEvent == CBN_SELCHANGE) {
                        int sel = SendMessageW(g_hwndComboProtocol, CB_GETCURSEL, 0, 0);
                        switch (sel) {
                            case 0: // All
                                g_protocol_filter = -1;
                                LOG_INFO("Protocol filter: All");
                                break;
                            case 1: // TCP
                                g_protocol_filter = PROTO_TCP;
                                LOG_INFO("Protocol filter: TCP");
                                break;
                            case 2: // UDP
                                g_protocol_filter = PROTO_UDP;
                                LOG_INFO("Protocol filter: UDP");
                                break;
                        }
                        RefreshListViewWithFilter();
                    }
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

                        char remote_ip[64], local_ip[64];
                        if (new_conns[i].ip_version == IP_V4) {
                            network_format_ip(new_conns[i].remote_addr, remote_ip, sizeof(remote_ip));
                            network_format_ip(new_conns[i].local_addr, local_ip, sizeof(local_ip));
                        } else {
                            network_format_ipv6(new_conns[i].remote_addr_v6, remote_ip, sizeof(remote_ip));
                            network_format_ipv6(new_conns[i].local_addr_v6, local_ip, sizeof(local_ip));
                        }

                        const char* proto_str = (new_conns[i].protocol == PROTO_TCP) ? "TCP" : "UDP";

                        LOG_SUCCESS("NEW CONNECTION [%s]: %s:%lu -> %s:%lu | %s (PID: %lu)",
                            proto_str,
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
            } else if (wParam == ID_TIMER_FLASH) {
                UpdateHighlights();
            }
            return 0;

        case WM_NOTIFY: {
            LPNMHDR nmhdr = (LPNMHDR)lParam;
            if (nmhdr->hwndFrom == g_hwndListView && nmhdr->code == NM_CUSTOMDRAW) {
                LPNMLVCUSTOMDRAW lplvcd = (LPNMLVCUSTOMDRAW)lParam;

                switch (lplvcd->nmcd.dwDrawStage) {
                    case CDDS_PREPAINT:
                        return CDRF_NOTIFYITEMDRAW;

                    case CDDS_ITEMPREPAINT:
                        return CDRF_NOTIFYSUBITEMDRAW;

                    case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
                        int item = (int)lplvcd->nmcd.dwItemSpec;
                        BOOL is_highlighted = FALSE;
                        COLORREF bg_color = GetHighlightColor(item, &is_highlighted);

                        lplvcd->clrTextBk = bg_color;
                        lplvcd->clrText = COLOR_TEXT_PRIMARY;

                        return CDRF_NEWFONT;
                    }
                }
            }
            return 0;
        }

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
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