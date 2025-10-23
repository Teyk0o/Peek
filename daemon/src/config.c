/*
 * PEEK Daemon Configuration Implementation
 */

#include "config.h"
#include "logger.h"
#include <stdlib.h>
#include <string.h>

/* Default settings */
static Settings g_current_settings = {
    .polling_interval_ms = 500,
    .show_localhost = 1,
    .enable_auto_update = 1,
    .start_with_windows = 0,
    .run_as_service = 1,
};

/*
 * Load configuration from registry
 */
int config_load(Settings *settings) {
    logger_info("Loading daemon configuration...");

    /* TODO: Load from HKLM\Software\Peek\Daemon */
    /* For now, use defaults */

    if (settings) {
        memcpy(settings, &g_current_settings, sizeof(Settings));
    }

    return 0;
}

/*
 * Save configuration to registry
 */
int config_save(const Settings *settings) {
    logger_info("Saving daemon configuration...");

    /* TODO: Save to HKLM\Software\Peek\Daemon */

    if (settings) {
        memcpy(&g_current_settings, settings, sizeof(Settings));
    }

    return 0;
}

/*
 * Get current settings
 */
void config_get(Settings *settings) {
    if (settings) {
        memcpy(settings, &g_current_settings, sizeof(Settings));
    }
}

/*
 * Update settings
 */
void config_set(const Settings *settings) {
    if (settings) {
        memcpy(&g_current_settings, settings, sizeof(Settings));
        config_save(settings);
    }
}
