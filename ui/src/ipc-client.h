/*
 * IPC Client - Communication with peekd.exe
 */

#ifndef PEEK_UI_IPC_CLIENT_H
#define PEEK_UI_IPC_CLIENT_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialize IPC client and connect to daemon
 * Returns: 0 on success, non-zero if daemon not available
 */
int ipc_client_init(void);

/*
 * Cleanup IPC client connection
 */
void ipc_client_cleanup(void);

/*
 * Check if connected to daemon
 * Returns: TRUE if connected, FALSE otherwise
 */
BOOL ipc_client_is_connected(void);

/*
 * Send command to daemon and receive response
 * params: cmd_id - Command ID (from ipc-protocol.h)
 *         request_payload - Request data (usually JSON)
 *         response_buffer - Buffer for response data
 *         response_size - Size of response buffer
 * Returns: Number of bytes in response, -1 on error
 */
int ipc_client_send_command(uint32_t cmd_id,
                            const uint8_t *request_payload, uint32_t request_size,
                            uint8_t *response_buffer, uint32_t response_size);

/*
 * Register callback for daemon events
 * Events will be delivered when received from daemon
 */
void ipc_client_set_event_callback(void (*callback)(uint32_t event, const char *payload));

#ifdef __cplusplus
}
#endif

#endif /* PEEK_UI_IPC_CLIENT_H */
