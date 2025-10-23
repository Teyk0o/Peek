/*
 * PEEK Daemon Auto-Update Module
 *
 * Checks for updates and notifies UI via IPC
 */

#ifndef PEEK_DAEMON_AUTO_UPDATE_H
#define PEEK_DAEMON_AUTO_UPDATE_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Initialize auto-update checker
 */
int auto_update_init(void);

/*
 * Cleanup
 */
void auto_update_cleanup(void);

/*
 * Check for updates (called periodically)
 * Returns: 1 if update available, 0 if no update, -1 on error
 */
int auto_update_check(void);

#ifdef __cplusplus
}
#endif

#endif /* PEEK_DAEMON_AUTO_UPDATE_H */
