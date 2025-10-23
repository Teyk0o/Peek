/*
 * PEEK Daemon Service Module
 *
 * Provides the core daemon functionality:
 * - Network monitoring loop
 * - IPC server setup
 * - Connection tracking
 * - Event notification
 */

#ifndef PEEK_DAEMON_SERVICE_H
#define PEEK_DAEMON_SERVICE_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialize daemon components
 * Returns: 0 on success, non-zero on error
 */
int service_init(void);

/*
 * Main service tick (called in polling loop)
 * Performs one iteration of:
 * - Network monitoring
 * - IPC message handling
 * - Event notification
 */
void service_tick(void);

/*
 * Cleanup and shutdown
 */
void service_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* PEEK_DAEMON_SERVICE_H */
