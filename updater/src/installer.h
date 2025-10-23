/*
 * Atomic Installation with Rollback
 */

#ifndef PEEK_UPDATER_INSTALLER_H
#define PEEK_UPDATER_INSTALLER_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Atomically install binary with rollback on failure
 * Old version is backed up to .bak file
 * Returns: 0 on success, non-zero on error
 */
int installer_install_binary(const char *new_binary_path, const char *install_location);

#ifdef __cplusplus
}
#endif

#endif /* PEEK_UPDATER_INSTALLER_H */
