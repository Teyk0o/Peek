/*
 * PEEK Daemon IPC Server Implementation
 *
 * Manages Named Pipes communication with UI clients
 */

#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable:4996)
#pragma warning(disable:4100)
#pragma warning(disable:4005)
#pragma warning(disable:4011)

/* Must include winsock2.h BEFORE windows.h to avoid conflicts */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>

#include "ipc.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>

#define IPC_MAX_CLIENTS 10
#define IPC_PIPE_INSTANCES 4

/* Client connection state */
typedef struct {
    HANDLE hPipe;
    BOOL connected;
    OVERLAPPED overlap;
    IPC_Message message;
} IPCClient;

/* IPC Server state */
static struct {
    BOOL initialized;
    HANDLE hEvents[IPC_MAX_CLIENTS + 1];
    IPCClient clients[IPC_MAX_CLIENTS];
    DWORD client_count;
} g_ipc_server = {0};

/*
 * Create a new Named Pipe instance for client connection
 */
static HANDLE ipc_create_pipe(void) {
    HANDLE hPipe = CreateNamedPipeW(
        PEEK_IPC_PIPE_NAME,
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        PEEK_IPC_BUFFER_SIZE,
        PEEK_IPC_BUFFER_SIZE,
        0,
        NULL
    );

    if (hPipe == INVALID_HANDLE_VALUE) {
        logger_error("Failed to create named pipe: %ld", GetLastError());
        return INVALID_HANDLE_VALUE;
    }

    return hPipe;
}

/*
 * Initialize IPC server
 */
int ipc_server_init(void) {
    logger_info("Initializing IPC server on %S", PEEK_IPC_PIPE_NAME);

    memset(&g_ipc_server, 0, sizeof(g_ipc_server));

    /* Create initial pipe instances */
    for (int i = 0; i < IPC_PIPE_INSTANCES; i++) {
        HANDLE hPipe = ipc_create_pipe();
        if (hPipe == INVALID_HANDLE_VALUE) {
            logger_error("Failed to create initial pipe instance %d", i);
            ipc_server_cleanup();
            return 1;
        }

        g_ipc_server.clients[i].hPipe = hPipe;
        g_ipc_server.clients[i].connected = FALSE;
        g_ipc_server.hEvents[i] = CreateEventW(NULL, TRUE, FALSE, NULL);

        if (g_ipc_server.hEvents[i] == NULL) {
            logger_error("Failed to create event handle");
            CloseHandle(hPipe);
            ipc_server_cleanup();
            return 1;
        }

        g_ipc_server.clients[i].overlap.hEvent = g_ipc_server.hEvents[i];
    }

    g_ipc_server.client_count = IPC_PIPE_INSTANCES;
    g_ipc_server.initialized = TRUE;

    logger_info("IPC server initialized with %d pipe instances", IPC_PIPE_INSTANCES);
    return 0;
}

/*
 * Poll for IPC connections and messages
 */
void ipc_server_tick(void) {
    if (!g_ipc_server.initialized) {
        return;
    }

    /* For now, this is a stub implementation
     * Full implementation will:
     * 1. Wait for client connections
     * 2. Read messages from clients
     * 3. Process commands (get connections, set settings, etc.)
     * 4. Send responses back to clients
     * 5. Send events to all connected clients
     */

    /* TODO: Implement actual IPC handling */
}

/*
 * Send event to all connected clients
 */
int ipc_send_event(uint32_t event, const uint8_t *payload, uint32_t payload_size) {
    if (!g_ipc_server.initialized) {
        return -1;
    }

    if (payload_size > (PEEK_IPC_BUFFER_SIZE - sizeof(IPC_MessageHeader))) {
        return -1;
    }

    /* TODO: Send event to all connected clients */

    return 0;
}

/*
 * Cleanup IPC server
 */
void ipc_server_cleanup(void) {
    logger_info("Cleaning up IPC server...");

    if (!g_ipc_server.initialized) {
        return;
    }

    for (DWORD i = 0; i < g_ipc_server.client_count; i++) {
        if (g_ipc_server.clients[i].hPipe != INVALID_HANDLE_VALUE) {
            DisconnectNamedPipe(g_ipc_server.clients[i].hPipe);
            CloseHandle(g_ipc_server.clients[i].hPipe);
        }

        if (g_ipc_server.hEvents[i] != NULL) {
            CloseHandle(g_ipc_server.hEvents[i]);
        }
    }

    g_ipc_server.initialized = FALSE;

    logger_info("IPC server cleanup complete");
}
