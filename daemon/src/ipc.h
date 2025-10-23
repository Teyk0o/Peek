/*
 * PEEK Daemon IPC Server
 *
 * Implements Named Pipes server for communication with UI host (peek.exe)
 * Protocol defined in shared/ipc-protocol.h
 */

#ifndef PEEK_DAEMON_IPC_H
#define PEEK_DAEMON_IPC_H

#include <windows.h>
#include "../../shared/ipc-protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialize IPC server
 * Sets up Named Pipes and starts listening for connections
 * Returns: 0 on success, non-zero on error
 */
int ipc_server_init(void);

/*
 * Poll for IPC connections and messages
 * Should be called regularly from service_tick()
 * Handles client connections and processes incoming messages
 */
void ipc_server_tick(void);

/*
 * Cleanup and close IPC server
 */
void ipc_server_cleanup(void);

/*
 * Send event to all connected clients
 * params: event - IPC_EventType (e.g., EVT_CONNECTION_ADDED)
 *         payload - Event payload (JSON string)
 *         payload_size - Size of payload
 * Returns: 0 on success
 */
int ipc_send_event(uint32_t event, const uint8_t *payload, uint32_t payload_size);

#ifdef __cplusplus
}
#endif

#endif /* PEEK_DAEMON_IPC_H */
