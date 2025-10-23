/*
 * PEEK Daemon Configuration Module
 *
 * Manages daemon settings and persistent configuration
 */

#ifndef PEEK_DAEMON_CONFIG_H
#define PEEK_DAEMON_CONFIG_H

#include <windows.h>
#include "../../shared/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Load daemon configuration from registry or config file
 * Returns: 0 on success
 */
int config_load(Settings *settings);

/*
 * Save daemon configuration
 * Returns: 0 on success
 */
int config_save(const Settings *settings);

/*
 * Get current settings
 */
void config_get(Settings *settings);

/*
 * Update a setting
 */
void config_set(const Settings *settings);

#ifdef __cplusplus
}
#endif

#endif /* PEEK_DAEMON_CONFIG_H */
