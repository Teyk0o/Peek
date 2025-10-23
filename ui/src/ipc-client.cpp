/*
 * IPC Client Implementation
 *
 * Communicates with peekd.exe daemon via Named Pipes
 */

#pragma warning(disable:4100)

#include "ipc-client.h"
#include "../../shared/ipc-protocol.h"
#include <string.h>
#include <stdlib.h>

/* IPC client state */
static struct {
    BOOL connected;
    HANDLE hPipe;
    HANDLE hReadThread;
    BOOL stopThread;
    void (*event_callback)(uint32_t event, const char *payload);
} g_ipc_client;

/* Forward declarations */
static DWORD WINAPI IpcReadThread(LPVOID param);

/*
 * Initialize IPC client
 */
int ipc_client_init(void) {
    /* Initialize global state */
    g_ipc_client.connected = FALSE;
    g_ipc_client.hPipe = INVALID_HANDLE_VALUE;
    g_ipc_client.hReadThread = nullptr;
    g_ipc_client.stopThread = FALSE;
    g_ipc_client.event_callback = nullptr;

    /* Attempt to connect to daemon's Named Pipe
     * Retry up to 5 times with 200ms delay
     */
    int retries = 5;
    HANDLE hPipe = INVALID_HANDLE_VALUE;

    while (retries > 0 && hPipe == INVALID_HANDLE_VALUE) {
        hPipe = CreateFileW(
            PEEK_IPC_PIPE_NAME,
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,  /* Use overlapped I/O for async reads */
            nullptr
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            if (GetLastError() == ERROR_PIPE_BUSY) {
                /* Daemon exists but all instances are busy */
                if (!WaitNamedPipeW(PEEK_IPC_PIPE_NAME, 1000)) {
                    retries--;
                    Sleep(200);
                    continue;
                }
                retries--;
                continue;
            } else {
                /* Daemon not running */
                return -1;
            }
        }
    }

    if (hPipe == INVALID_HANDLE_VALUE) {
        return -1; /* Failed to connect */
    }

    /* Set to message mode */
    DWORD dwMode = PIPE_READMODE_MESSAGE;
    if (!SetNamedPipeHandleState(hPipe, &dwMode, nullptr, nullptr)) {
        CloseHandle(hPipe);
        return -1;
    }

    g_ipc_client.hPipe = hPipe;
    g_ipc_client.connected = TRUE;
    g_ipc_client.stopThread = FALSE;

    /* Start read thread to listen for async messages */
    g_ipc_client.hReadThread = CreateThread(
        nullptr,
        0,
        IpcReadThread,
        nullptr,
        0,
        nullptr
    );

    if (!g_ipc_client.hReadThread) {
        CloseHandle(hPipe);
        g_ipc_client.connected = FALSE;
        return -1;
    }

    return 0;
}

/*
 * Background thread for reading async messages from daemon
 */
static DWORD WINAPI IpcReadThread(LPVOID param) {
    (void)param;

    uint8_t buffer[PEEK_IPC_BUFFER_SIZE];
    DWORD bytesRead = 0;

    while (!g_ipc_client.stopThread && g_ipc_client.connected) {
        ZeroMemory(buffer, sizeof(buffer));

        /* Read message from daemon */
        if (!ReadFile(g_ipc_client.hPipe, buffer, sizeof(buffer), &bytesRead, nullptr)) {
            DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED) {
                /* Daemon disconnected */
                g_ipc_client.connected = FALSE;
                break;
            }
            Sleep(100);
            continue;
        }

        if (bytesRead == 0) {
            continue;
        }

        /* Parse message header */
        IPC_Message* msg = (IPC_Message*)buffer;

        if (msg->header.magic != IPC_MAGIC) {
            /* Invalid message */
            continue;
        }

        /* Handle based on message type */
        if (msg->header.msg_type == MSG_EVENT && g_ipc_client.event_callback) {
            /* Convert payload to string and call callback */
            const char* payload = (const char*)msg->payload;
            g_ipc_client.event_callback(msg->header.cmd_id, payload);
        }
    }

    return 0;
}

/*
 * Cleanup
 */
void ipc_client_cleanup(void) {
    /* Stop read thread */
    g_ipc_client.stopThread = TRUE;

    if (g_ipc_client.hReadThread) {
        WaitForSingleObject(g_ipc_client.hReadThread, 5000);
        CloseHandle(g_ipc_client.hReadThread);
        g_ipc_client.hReadThread = nullptr;
    }

    /* Close pipe */
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
 * Send command to daemon
 */
int ipc_client_send_command(uint32_t cmd_id,
                            const uint8_t *request_payload, uint32_t request_size,
                            uint8_t *response_buffer, uint32_t response_size) {
    if (!g_ipc_client.connected) {
        return -1;
    }

    if (request_size > (PEEK_IPC_BUFFER_SIZE - sizeof(IPC_MessageHeader))) {
        return -1; /* Payload too large */
    }

    /* Build request message */
    IPC_Message request;
    ZeroMemory(&request, sizeof(request));

    request.header.magic = IPC_MAGIC;
    request.header.version = 1;
    request.header.msg_id = GetTickCount();  /* Use timestamp as unique ID */
    request.header.msg_type = MSG_REQUEST;
    request.header.cmd_id = cmd_id;
    request.header.payload_size = request_size;

    if (request_payload && request_size > 0) {
        memcpy(request.payload, request_payload, request_size);
    }

    /* Send request */
    DWORD bytesWritten = 0;
    if (!WriteFile(g_ipc_client.hPipe, &request,
                   sizeof(IPC_MessageHeader) + request_size,
                   &bytesWritten, nullptr)) {
        return -1;
    }

    /* Read response */
    IPC_Message response;
    DWORD bytesRead = 0;

    if (!ReadFile(g_ipc_client.hPipe, &response, sizeof(response), &bytesRead, nullptr)) {
        return -1;
    }

    if (bytesRead < sizeof(IPC_MessageHeader)) {
        return -1; /* Invalid response */
    }

    if (response.header.magic != IPC_MAGIC) {
        return -1; /* Invalid response */
    }

    /* Copy response payload */
    uint32_t copySize = (response.header.payload_size < response_size) ?
                        response.header.payload_size : response_size;

    if (copySize > 0 && response_buffer) {
        memcpy(response_buffer, response.payload, copySize);
    }

    return copySize;
}

/*
 * Register event callback
 */
void ipc_client_set_event_callback(void (*callback)(uint32_t event, const char *payload)) {
    g_ipc_client.event_callback = callback;
}
