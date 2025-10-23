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
#define ID_TIMER_SECURITY 1013
#define ID_RADIO_ALL 1008
#define ID_RADIO_OUTBOUND 1009
#define ID_RADIO_INBOUND 1010
#define ID_CHECK_LOCALHOST 1011
#define ID_COMBO_PROTOCOL 1012
#define ID_COMBO_TRUST 1015
#define ID_LEGEND_GROUP 1014

#define FLASH_DURATION_MS 1500
#define LEGEND_HEIGHT 50
#define MAX_HIGHLIGHTED_ITEMS 100

typedef struct {
    int item_index;
    DWORD start_time;
} HighlightedItem;

// Store connection keys to find them in seen_connections
typedef struct {
    DWORD pid;
    DWORD remote_addr;
    DWORD remote_port;
    DWORD local_port;
} ConnectionKey;

#define MAX_LISTVIEW_ITEMS 2000
static ConnectionKey g_connection_keys[MAX_LISTVIEW_ITEMS];
static int g_connection_keys_count = 0;

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
static HWND g_hwndComboTrust = NULL;
static HINSTANCE g_hInstance = NULL;
static BOOL g_monitoring = FALSE;
static ConnectionDirection g_filter = CONN_OUTBOUND; // Default to outbound only
static BOOL g_show_localhost = TRUE; // Default to show localhost connections

// Protocol filter: -1 = All, PROTO_TCP = TCP only, PROTO_UDP = UDP only
static int g_protocol_filter = -1; // Default to show all protocols

// Trust level filter: -1 = All, or a specific TrustStatus value to filter by
// When -1, show all trust levels; otherwise only show connections with the specified trust status
static int g_trust_filter = -1; // Default to show all trust levels

static HighlightedItem g_highlighted_items[MAX_HIGHLIGHTED_ITEMS];
static int g_highlighted_count = 0;

// Security info loading progress
static HANDLE g_security_thread = NULL;
static BOOL g_security_loading = FALSE;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CreateControls(HWND hwnd);
void InitializeListView(void);
void UpdateMonitoringState(BOOL monitoring);
void AddHighlightedItem(int item_index);
void UpdateHighlights(void);
COLORREF GetHighlightColor(int item_index, BOOL* is_highlighted);
void RefreshListViewWithFilter(void);
void UpdateTrustColumnForConnection(DWORD pid, DWORD remote_addr, DWORD remote_port, DWORD local_port);

// Security loading thread
static DWORD WINAPI LoadSecurityInfoThread(LPVOID lpParam) {
    (void)lpParam;

    // Compute security info for all seen connections in parallel
    // This updates the seen_connections array directly (thread-safe)
    network_compute_security_for_all_seen();

    g_security_loading = FALSE;

    // Trigger full UI refresh after all security info is loaded
    if (g_hwndListView) {
        PostMessage(g_hwndMain, WM_USER + 1, 0, 0); // Custom message to refresh
    }

    return 0;
}

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

    // Trust Level filter ComboBox
    int trustComboX = comboX + 65 + 80 + 20;
    CreateWindowW(L"STATIC", L"Trust:",
        WS_CHILD | WS_VISIBLE,
        trustComboX, btnY + 8, 45, 20,
        hwnd, NULL, g_hInstance, NULL);

    g_hwndComboTrust = CreateWindowW(
        L"COMBOBOX",
        NULL,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        trustComboX + 50, btnY + 5, 120, 150,
        hwnd,
        (HMENU)ID_COMBO_TRUST,
        g_hInstance,
        NULL
    );

    // Add items to Trust ComboBox
    SendMessageW(g_hwndComboTrust, CB_ADDSTRING, 0, (LPARAM)L"All");
    SendMessageW(g_hwndComboTrust, CB_ADDSTRING, 0, (LPARAM)L"Microsoft Signed");
    SendMessageW(g_hwndComboTrust, CB_ADDSTRING, 0, (LPARAM)L"Verified Signed");
    SendMessageW(g_hwndComboTrust, CB_ADDSTRING, 0, (LPARAM)L"Manually Trusted");
    SendMessageW(g_hwndComboTrust, CB_ADDSTRING, 0, (LPARAM)L"Unsigned");
    SendMessageW(g_hwndComboTrust, CB_ADDSTRING, 0, (LPARAM)L"Invalid");
    SendMessageW(g_hwndComboTrust, CB_ADDSTRING, 0, (LPARAM)L"Manually Threat");
    SendMessageW(g_hwndComboTrust, CB_ADDSTRING, 0, (LPARAM)L"Error");
    SendMessageW(g_hwndComboTrust, CB_ADDSTRING, 0, (LPARAM)L"Unknown");

    // Set default to "All"
    SendMessageW(g_hwndComboTrust, CB_SETCURSEL, 0, 0);

    // Legend/Trust Status Key
    int legendY = btnY + btnHeight + btnMargin;
    CreateWindowW(L"STATIC", L"Trust Status Legend:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        btnMargin, legendY, 150, 20,
        hwnd, NULL, g_hInstance, NULL);

    // Create legend items with colors (2 rows, 3 columns each)
    int legendItemX = btnMargin + 150;
    int legendItemY = legendY;
    int colorBoxSize = 15;
    int spacingX = 200;
    int spacingY = 25;

    // Helper function to draw a colored legend box
    // We'll use WM_CTLCOLORSTATIC to color the static controls instead

    // Row 1: Dark Green, Green, Yellow
    // Dark Green - Microsoft Signed
    CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
        legendItemX, legendItemY + 2, colorBoxSize, colorBoxSize,
        hwnd, (HMENU)1401, g_hInstance, NULL);
    SetWindowLongPtrW(GetDlgItem(hwnd, 1401), GWLP_USERDATA, (LONG_PTR)RGB(144, 238, 144));
    CreateWindowW(L"STATIC", L"Microsoft Signed", WS_CHILD | WS_VISIBLE,
        legendItemX + colorBoxSize + 5, legendItemY, 120, 20,
        hwnd, NULL, g_hInstance, NULL);

    // Green - Verified Publisher Signed
    CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
        legendItemX + spacingX, legendItemY + 2, colorBoxSize, colorBoxSize,
        hwnd, (HMENU)1402, g_hInstance, NULL);
    SetWindowLongPtrW(GetDlgItem(hwnd, 1402), GWLP_USERDATA, (LONG_PTR)RGB(200, 255, 200));
    CreateWindowW(L"STATIC", L"Verified Signed", WS_CHILD | WS_VISIBLE,
        legendItemX + spacingX + colorBoxSize + 5, legendItemY, 120, 20,
        hwnd, NULL, g_hInstance, NULL);

    // Yellow - Manually Trusted
    CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
        legendItemX + spacingX * 2, legendItemY + 2, colorBoxSize, colorBoxSize,
        hwnd, (HMENU)1403, g_hInstance, NULL);
    SetWindowLongPtrW(GetDlgItem(hwnd, 1403), GWLP_USERDATA, (LONG_PTR)RGB(255, 255, 153));
    CreateWindowW(L"STATIC", L"Manually Trusted", WS_CHILD | WS_VISIBLE,
        legendItemX + spacingX * 2 + colorBoxSize + 5, legendItemY, 120, 20,
        hwnd, NULL, g_hInstance, NULL);

    // Row 2: Orange, Red, Dark Red
    // Orange - Unsigned
    CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
        legendItemX, legendItemY + spacingY + 2, colorBoxSize, colorBoxSize,
        hwnd, (HMENU)1404, g_hInstance, NULL);
    SetWindowLongPtrW(GetDlgItem(hwnd, 1404), GWLP_USERDATA, (LONG_PTR)RGB(255, 220, 180));
    CreateWindowW(L"STATIC", L"Not Signed", WS_CHILD | WS_VISIBLE,
        legendItemX + colorBoxSize + 5, legendItemY + spacingY, 120, 20,
        hwnd, NULL, g_hInstance, NULL);

    // Red - Invalid Signature
    CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
        legendItemX + spacingX, legendItemY + spacingY + 2, colorBoxSize, colorBoxSize,
        hwnd, (HMENU)1405, g_hInstance, NULL);
    SetWindowLongPtrW(GetDlgItem(hwnd, 1405), GWLP_USERDATA, (LONG_PTR)RGB(255, 200, 200));
    CreateWindowW(L"STATIC", L"Invalid Signature", WS_CHILD | WS_VISIBLE,
        legendItemX + spacingX + colorBoxSize + 5, legendItemY + spacingY, 120, 20,
        hwnd, NULL, g_hInstance, NULL);

    // Dark Red - Manually Threat
    CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
        legendItemX + spacingX * 2, legendItemY + spacingY + 2, colorBoxSize, colorBoxSize,
        hwnd, (HMENU)1406, g_hInstance, NULL);
    SetWindowLongPtrW(GetDlgItem(hwnd, 1406), GWLP_USERDATA, (LONG_PTR)RGB(255, 153, 153));
    CreateWindowW(L"STATIC", L"Manually Threat", WS_CHILD | WS_VISIBLE,
        legendItemX + spacingX * 2 + colorBoxSize + 5, legendItemY + spacingY, 120, 20,
        hwnd, NULL, g_hInstance, NULL);

    // listY accounts for: btnMargin + btnHeight (toolbar) + legendY offset + legend height (70) + spacing
    int listY = btnMargin + btnHeight + btnMargin + 70;
    g_hwndListView = CreateWindowEx(
        0,
        WC_LISTVIEW,
        NULL,
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_BORDER,
        btnMargin, listY,
        rcClient.right - btnMargin * 2,
        rcClient.bottom - listY - 40,  // Leave space for status bar
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
    lvc.fmt = LVCFMT_LEFT;
    ListView_InsertColumn(g_hwndListView, 7, &lvc);

    // Trust column
    lvc.pszText = L"Trust";
    lvc.cx = 50;
    lvc.fmt = LVCFMT_CENTER;
    ListView_InsertColumn(g_hwndListView, 8, &lvc);

    // PID column
    lvc.pszText = L"PID";
    lvc.cx = 70;
    lvc.fmt = LVCFMT_CENTER;
    ListView_InsertColumn(g_hwndListView, 9, &lvc);

    // Count column
    lvc.pszText = L"Count";
    lvc.cx = 60;
    lvc.fmt = LVCFMT_CENTER;
    ListView_InsertColumn(g_hwndListView, 10, &lvc);
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

    // Apply trust level filter
    if (g_trust_filter != -1 && conn->trust_status != g_trust_filter) {
        return; // Skip this connection based on trust level filter
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

    // Trust status icon - use symbols that match the color scheme
    // Manual overrides get a lock icon prefix
    wchar_t trust_str[8];
    switch (conn->trust_status) {
        case TRUST_MICROSOFT_SIGNED:
            wcscpy(trust_str, L"✓✓");  // Double checkmark for Microsoft
            break;
        case TRUST_VERIFIED_SIGNED:
            wcscpy(trust_str, L"✓");   // Checkmark for verified publisher
            break;
        case TRUST_MANUAL_TRUSTED:
            wcscpy(trust_str, L"🔒👍");  // Lock + Thumbs up for manually trusted
            break;
        case TRUST_UNSIGNED:
            wcscpy(trust_str, L"○");   // Circle for unsigned
            break;
        case TRUST_INVALID:
            wcscpy(trust_str, L"✗");   // X for invalid
            break;
        case TRUST_MANUAL_THREAT:
            wcscpy(trust_str, L"🔒⚠");   // Lock + Warning for manually marked threat
            break;
        case TRUST_ERROR:
            wcscpy(trust_str, L"!");   // Exclamation for error
            break;
        case TRUST_UNKNOWN:
        default:
            wcscpy(trust_str, L"?");   // Question for unknown
            break;
    }

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
        ListView_GetItemText(g_hwndListView, i, 10, existing_count, 16);

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
            ListView_SetItemText(g_hwndListView, i, 10, new_count);

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

    // Store connection key for this item
    int key_index = g_connection_keys_count;
    if (key_index < MAX_LISTVIEW_ITEMS) {
        g_connection_keys[key_index].pid = conn->pid;
        g_connection_keys[key_index].remote_addr = conn->remote_addr;
        g_connection_keys[key_index].remote_port = conn->remote_port;
        g_connection_keys[key_index].local_port = conn->local_port;
        g_connection_keys_count++;
    }

    LVITEM lvi = {0};
    lvi.mask = LVIF_TEXT | LVIF_PARAM;
    lvi.iItem = item_count; // Insert at end
    lvi.iSubItem = 0;
    lvi.pszText = time_str;
    lvi.lParam = (LPARAM)key_index; // Store index in g_connection_keys
    int index = ListView_InsertItem(g_hwndListView, &lvi);

    ListView_SetItemText(g_hwndListView, index, 1, direction_str);
    ListView_SetItemText(g_hwndListView, index, 2, protocol_str);
    ListView_SetItemText(g_hwndListView, index, 3, w_remote_ip);
    ListView_SetItemText(g_hwndListView, index, 4, remote_port);
    ListView_SetItemText(g_hwndListView, index, 5, w_local_ip);
    ListView_SetItemText(g_hwndListView, index, 6, local_port);
    ListView_SetItemText(g_hwndListView, index, 7, w_process);
    ListView_SetItemText(g_hwndListView, index, 8, trust_str);
    ListView_SetItemText(g_hwndListView, index, 9, pid_str);
    ListView_SetItemText(g_hwndListView, index, 10, L"1");

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
        g_connection_keys_count = 0; // Reset connection keys
        LOG_INFO("ListView cleared");
    }
}

void RefreshListViewWithFilter(void) {
    if (!g_hwndListView) return;

    // Clear current list and highlights
    ListView_DeleteAllItems(g_hwndListView);
    g_highlighted_count = 0;
    g_connection_keys_count = 0; // Reset connection keys

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

// Update the trust column text for a specific connection in the ListView
void UpdateTrustColumnForConnection(DWORD pid, DWORD remote_addr, DWORD remote_port, DWORD local_port) {
    if (!g_hwndListView) return;

    int item_count = ListView_GetItemCount(g_hwndListView);

    // Search for the item with matching connection key
    for (int i = 0; i < item_count; i++) {
        LVITEM lvi = {0};
        lvi.mask = LVIF_PARAM;
        lvi.iItem = i;

        if (ListView_GetItem(g_hwndListView, &lvi)) {
            int key_index = (int)lvi.lParam;
            if (key_index >= 0 && key_index < g_connection_keys_count) {
                ConnectionKey* key = &g_connection_keys[key_index];

                // Check if this is the connection we're looking for
                if (key->pid == pid &&
                    key->remote_addr == remote_addr &&
                    key->remote_port == remote_port &&
                    key->local_port == local_port) {

                    // Get the real connection to read its trust status
                    NetworkConnection* conn = network_find_connection(pid, remote_addr, remote_port, local_port);
                    if (conn) {
                        wchar_t trust_str[8];
                        switch (conn->trust_status) {
                            case TRUST_MICROSOFT_SIGNED:
                                wcscpy(trust_str, L"✓✓");  // Double checkmark for Microsoft
                                break;
                            case TRUST_VERIFIED_SIGNED:
                                wcscpy(trust_str, L"✓");   // Checkmark for verified publisher
                                break;
                            case TRUST_MANUAL_TRUSTED:
                                wcscpy(trust_str, L"🔒👍");  // Lock + Thumbs up for manually trusted
                                break;
                            case TRUST_UNSIGNED:
                                wcscpy(trust_str, L"○");   // Circle for unsigned
                                break;
                            case TRUST_INVALID:
                                wcscpy(trust_str, L"✗");   // X for invalid
                                break;
                            case TRUST_MANUAL_THREAT:
                                wcscpy(trust_str, L"🔒⚠");   // Lock + Warning for manually marked threat
                                break;
                            case TRUST_ERROR:
                                wcscpy(trust_str, L"!");   // Exclamation for error
                                break;
                            case TRUST_UNKNOWN:
                            default:
                                wcscpy(trust_str, L"?");   // Question for unknown
                                break;
                        }

                        // Update the Trust column text
                        ListView_SetItemText(g_hwndListView, i, 8, trust_str);
                    }
                    return; // Found and updated
                }
            }
        }
    }
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

        // Launch security info loading in background thread
        if (!g_security_loading) {
            g_security_loading = TRUE;
            g_security_thread = CreateThread(NULL, 0, LoadSecurityInfoThread, NULL, 0, NULL);
        }

        LOG_INFO("Monitoring started");
    } else {
        EnableWindow(g_hwndBtnStart, TRUE);
        EnableWindow(g_hwndBtnStop, FALSE);
        KillTimer(g_hwndMain, ID_TIMER);
        KillTimer(g_hwndMain, ID_TIMER_FLASH);

        // Wait for security thread to complete if still running
        if (g_security_thread) {
            WaitForSingleObject(g_security_thread, 2000);  // Wait max 2s
            CloseHandle(g_security_thread);
            g_security_thread = NULL;
        }

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
                // Legend takes: 20 (title) + 25 (first row) + 25 (second row) + spacing
                int legendHeight = 70;
                int listY = btnMargin + btnHeight + btnMargin + legendHeight;

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

                case ID_COMBO_TRUST:
                    if (wmEvent == CBN_SELCHANGE) {
                        int sel = SendMessageW(g_hwndComboTrust, CB_GETCURSEL, 0, 0);
                        switch (sel) {
                            case 0: // All
                                g_trust_filter = -1;
                                LOG_INFO("Trust filter: All");
                                break;
                            case 1: // Microsoft Signed
                                g_trust_filter = TRUST_MICROSOFT_SIGNED;
                                LOG_INFO("Trust filter: Microsoft Signed");
                                break;
                            case 2: // Verified Signed
                                g_trust_filter = TRUST_VERIFIED_SIGNED;
                                LOG_INFO("Trust filter: Verified Signed");
                                break;
                            case 3: // Manually Trusted
                                g_trust_filter = TRUST_MANUAL_TRUSTED;
                                LOG_INFO("Trust filter: Manually Trusted");
                                break;
                            case 4: // Unsigned
                                g_trust_filter = TRUST_UNSIGNED;
                                LOG_INFO("Trust filter: Unsigned");
                                break;
                            case 5: // Invalid
                                g_trust_filter = TRUST_INVALID;
                                LOG_INFO("Trust filter: Invalid");
                                break;
                            case 6: // Manually Threat
                                g_trust_filter = TRUST_MANUAL_THREAT;
                                LOG_INFO("Trust filter: Manually Threat");
                                break;
                            case 7: // Error
                                g_trust_filter = TRUST_ERROR;
                                LOG_INFO("Trust filter: Error");
                                break;
                            case 8: // Unknown
                                g_trust_filter = TRUST_UNKNOWN;
                                LOG_INFO("Trust filter: Unknown");
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
                    BOOL need_refresh = FALSE;
                    for (int i = 0; i < count; i++) {
                        // Add connection to GUI first
                        gui_add_connection(&new_conns[i]);

                        // Then find the real connection in seen_connections and compute security info
                        NetworkConnection* real_conn = network_find_connection(
                            new_conns[i].pid,
                            new_conns[i].remote_addr,
                            new_conns[i].remote_port,
                            new_conns[i].local_port
                        );

                        if (real_conn && !real_conn->security_info_loaded) {
                            network_compute_security_info_deferred(real_conn);
                            // Update the Trust column text and color for this connection
                            UpdateTrustColumnForConnection(
                                new_conns[i].pid,
                                new_conns[i].remote_addr,
                                new_conns[i].remote_port,
                                new_conns[i].local_port
                            );
                            need_refresh = TRUE;
                        }

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

                    // Refresh ListView if security info was computed for new connections
                    if (need_refresh) {
                        InvalidateRect(g_hwndListView, NULL, FALSE);
                    }

                    free(new_conns);
                }
            } else if (wParam == ID_TIMER_FLASH) {
                UpdateHighlights();
            }
            return 0;

        case WM_NOTIFY: {
            LPNMHDR nmhdr = (LPNMHDR)lParam;

            // Handle right-click context menu for ListView
            if (nmhdr->hwndFrom == g_hwndListView && nmhdr->code == NM_RCLICK) {
                LPNMITEMACTIVATE pnmia = (LPNMITEMACTIVATE)lParam;
                if (pnmia->iItem >= 0) {
                    // Show context menu for the clicked item
                    POINT pt;
                    GetCursorPos(&pt);

                    HMENU hMenu = CreatePopupMenu();
                    AppendMenuW(hMenu, MF_STRING, 1, L"Mark as Trusted (Green)");
                    AppendMenuW(hMenu, MF_STRING, 2, L"Mark as Threat (Red)");
                    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
                    AppendMenuW(hMenu, MF_STRING, 3, L"Reset to Auto (Default)");

                    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
                    DestroyMenu(hMenu);

                    if (cmd > 0) {
                        // Get the connection for this item
                        LVITEM lvi = {0};
                        lvi.mask = LVIF_PARAM;
                        lvi.iItem = pnmia->iItem;
                        if (ListView_GetItem(g_hwndListView, &lvi)) {
                            int key_index = (int)lvi.lParam;
                            if (key_index >= 0 && key_index < g_connection_keys_count) {
                                ConnectionKey* key = &g_connection_keys[key_index];
                                NetworkConnection* conn = network_find_connection(
                                    key->pid, key->remote_addr, key->remote_port, key->local_port);

                                if (conn && strlen(conn->process_path) > 0) {
                                    TrustStatus new_status = TRUST_UNKNOWN;
                                    switch (cmd) {
                                        case 1: // Mark as Trusted
                                            new_status = TRUST_MANUAL_TRUSTED;
                                            break;
                                        case 2: // Mark as Threat
                                            new_status = TRUST_MANUAL_THREAT;
                                            break;
                                        case 3: // Reset to Auto
                                            new_status = TRUST_UNKNOWN;
                                            break;
                                    }

                                    // Apply trust override (saves to file and updates all connections)
                                    network_apply_trust_override(conn->process_path, new_status);

                                    // Refresh the entire ListView to show changes
                                    RefreshListViewWithFilter();
                                }
                            }
                        }
                    }
                }
                return 0;
            }

            if (nmhdr->hwndFrom == g_hwndListView && nmhdr->code == NM_CUSTOMDRAW) {
                LPNMLVCUSTOMDRAW lplvcd = (LPNMLVCUSTOMDRAW)lParam;

                switch (lplvcd->nmcd.dwDrawStage) {
                    case CDDS_PREPAINT:
                        return CDRF_NOTIFYITEMDRAW;

                    case CDDS_ITEMPREPAINT:
                        return CDRF_NOTIFYSUBITEMDRAW;

                    case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
                        int item = (int)lplvcd->nmcd.dwItemSpec;
                        int subitem = lplvcd->iSubItem;

                        // Special coloring for Trust column (column 8)
                        if (subitem == 8) {
                            // Get the connection key index stored in lParam
                            LVITEM lvi = {0};
                            lvi.mask = LVIF_PARAM;
                            lvi.iItem = item;
                            if (ListView_GetItem(g_hwndListView, &lvi)) {
                                int key_index = (int)lvi.lParam;
                                if (key_index >= 0 && key_index < g_connection_keys_count) {
                                    ConnectionKey* key = &g_connection_keys[key_index];
                                    NetworkConnection* conn = network_find_connection(
                                        key->pid, key->remote_addr, key->remote_port, key->local_port);

                                    if (conn) {
                                    // Set background color based on trust status (6-level system)
                                    switch (conn->trust_status) {
                                        case TRUST_MICROSOFT_SIGNED:
                                            // Dark Green - Microsoft/Windows signed (highest trust)
                                            lplvcd->clrTextBk = RGB(144, 238, 144);  // Light green
                                            lplvcd->clrText = RGB(0, 80, 0);          // Very dark green
                                            break;

                                        case TRUST_VERIFIED_SIGNED:
                                            // Green - Verified publisher signature
                                            lplvcd->clrTextBk = RGB(200, 255, 200);  // Lighter green
                                            lplvcd->clrText = RGB(0, 120, 0);        // Dark green
                                            break;

                                        case TRUST_MANUAL_TRUSTED:
                                            // Yellow-Green - Manually marked as trusted
                                            lplvcd->clrTextBk = RGB(255, 255, 153);  // Light yellow
                                            lplvcd->clrText = RGB(100, 100, 0);      // Dark yellow-green
                                            break;

                                        case TRUST_UNSIGNED:
                                            // Orange - Not signed (caution)
                                            lplvcd->clrTextBk = RGB(255, 220, 180);  // Light orange
                                            lplvcd->clrText = RGB(180, 100, 0);      // Dark orange
                                            break;

                                        case TRUST_INVALID:
                                            // Red - Invalid/expired signature (danger)
                                            lplvcd->clrTextBk = RGB(255, 200, 200);  // Light red
                                            lplvcd->clrText = RGB(180, 0, 0);        // Dark red
                                            break;

                                        case TRUST_MANUAL_THREAT:
                                            // Dark Red - Manually marked as threat
                                            lplvcd->clrTextBk = RGB(255, 153, 153);  // Medium red
                                            lplvcd->clrText = RGB(139, 0, 0);        // Very dark red
                                            break;

                                        case TRUST_ERROR:
                                            // Gray-Red - Error during verification
                                            lplvcd->clrTextBk = RGB(240, 200, 200);  // Grayish red
                                            lplvcd->clrText = RGB(120, 60, 60);      // Dark gray-red
                                            break;

                                        default: // TRUST_UNKNOWN
                                            // Gray - Not yet verified
                                            lplvcd->clrTextBk = RGB(220, 220, 220);  // Light gray
                                            lplvcd->clrText = RGB(80, 80, 80);       // Dark gray
                                            break;
                                    }
                                    }
                                }
                            }
                            return CDRF_NEWFONT;
                        }

                        // Default highlighting for other columns
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

        case WM_CTLCOLORSTATIC: {
            // Handle coloring of legend color boxes (IDs 1401-1406)
            HWND hwndControl = (HWND)lParam;
            HDC hdcControl = (HDC)wParam;

            int ctrlId = GetDlgCtrlID(hwndControl);
            // Check if this is one of the legend color boxes
            if (ctrlId >= 1401 && ctrlId <= 1406) {
                // Get the color stored in user data
                DWORD_PTR userData = GetWindowLongPtrW(hwndControl, GWLP_USERDATA);
                if (userData != 0) {
                    COLORREF color = (COLORREF)userData;
                    // Create brush with the stored color
                    static HBRUSH hBrush[6] = {NULL};
                    static COLORREF lastColor[6] = {0};

                    int idx = ctrlId - 1401;
                    if (idx >= 0 && idx < 6) {
                        // Delete old brush if color changed
                        if (lastColor[idx] != color && hBrush[idx] != NULL) {
                            DeleteObject(hBrush[idx]);
                            hBrush[idx] = NULL;
                        }

                        // Create new brush if needed
                        if (hBrush[idx] == NULL) {
                            hBrush[idx] = CreateSolidBrush(color);
                            lastColor[idx] = color;
                        }

                        SetBkColor(hdcControl, color);
                        SetTextColor(hdcControl, color);  // Match text to background
                        return (LRESULT)hBrush[idx];
                    }
                }
            }
            // Default: return white background for other static controls
            return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
        }

        case WM_CTLCOLORBTN: {
            return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
        }

        case WM_USER + 1: {
            // Security info loading completed - refresh the list view
            if (g_monitoring && g_hwndListView) {
                RefreshListViewWithFilter();
            }
            return 0;
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