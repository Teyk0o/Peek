/*
 * IPC Client Implementation
 */

#include "ipc-client.h"
#include "../../shared/ipc-protocol.h"
#include <string.h>

/* IPC client state */
static struct {
    BOOL connected;
    HANDLE hPipe;
    void (*event_callback)(uint32_t event, const char *payload);
} g_ipc_client = {FALSE, INVALID_HANDLE_VALUE, nullptr};

/*
 * Initialize IPC client
 */
int ipc_client_init(void) {
    /* Attempt to connect to daemon's Named Pipe */
    HANDLE hPipe = CreateFileW(
        PEEK_IPC_PIPE_NAME,
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr
    );

    if (hPipe == INVALID_HANDLE_VALUE) {
        return -1; /* Daemon not running */
    }

    /* Set to message mode */
    DWORD dwMode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(hPipe, &dwMode, nullptr, nullptr)) {
        CloseHandle(hPipe);
        return -1;
    }

    g_ipc_client.hPipe = hPipe;
    g_ipc_client.connected = TRUE;

    return 0;
}

/*
 * Cleanup
 */
void ipc_client_cleanup(void) {
    if (g_ipc_client.hPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(g_ipc_client.hPipe);
        g_ipc_client.hPipe = INVALID_HANDLE_VALUE;
    }

    g_ipc_client.connected = FALSE;
}

/*
 * Check if connected
 */
BOOL ipc_client_is_connected(void) {
    return g_ipc_client.connected;
}

/*
 * Send command
 */
int ipc_client_send_command(uint32_t cmd_id,
                            const uint8_t *request_payload, uint32_t request_size,
                            uint8_t *response_buffer, uint32_t response_size) {
    if (!g_ipc_client.connected) {
        return -1;
    }

    /* TODO: Build IPC message, send via Named Pipe, receive response */

    return 0; /* Stub implementation */
}

/*
 * Register event callback
 */
void ipc_client_set_event_callback(void (*callback)(uint32_t event, const char *payload)) {
    g_ipc_client.event_callback = callback;
}
