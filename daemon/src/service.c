/*
 * PEEK Daemon Service Implementation
 *
 * Core daemon functionality for network monitoring and IPC
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

#include "service.h"
#include "network.h"
#include "ipc.h"
#include "logger.h"
#include <stdlib.h>
#include <time.h>

/* Forward declarations */
static void service_update_network_stats(void);
static void service_handle_ipc_messages(void);

/* Service state */
static struct {
    BOOL initialized;
    DWORD last_network_update;
    DWORD polling_interval_ms;
} g_service_state = {0};

/*
 * Initialize daemon components
 */
int service_init(void) {
    logger_info("Initializing daemon service...");

    /* Initialize network monitoring */
    if (network_init() != 0) {
        logger_error("Failed to initialize network module");
        return 1;
    }
    logger_info("Network module initialized");

    /* Initialize IPC server */
    if (ipc_server_init() != 0) {
        logger_error("Failed to initialize IPC server");
        network_cleanup();
        return 1;
    }
    logger_info("IPC server initialized");

    /* Initialize service state */
    g_service_state.initialized = TRUE;
    g_service_state.last_network_update = 0;
    g_service_state.polling_interval_ms = 500; /* Default polling interval */

    logger_info("Daemon service initialization complete");
    return 0;
}

/*
 * Update network statistics and connection list
 */
static void service_update_network_stats(void) {
    DWORD now = GetTickCount();

    /* Poll network at configured interval */
    if (now - g_service_state.last_network_update < g_service_state.polling_interval_ms) {
        return;
    }

    g_service_state.last_network_update = now;

    /* Trigger network module to refresh connections */
    /* TODO: Implement network polling/update mechanism */
    /* network_update(); */

    /* Get current stats */
    NetworkStats stats;
    network_get_stats(&stats);

    /* Notify connected UI clients about updated stats */
    /* This will be handled by IPC event system */
}

/*
 * Handle incoming IPC messages
 */
static void service_handle_ipc_messages(void) {
    /* Poll for new IPC connections and messages */
    ipc_server_tick();
}

/*
 * Main service tick
 */
void service_tick(void) {
    if (!g_service_state.initialized) {
        return;
    }

    /* Update network monitoring */
    service_update_network_stats();

    /* Process IPC messages */
    service_handle_ipc_messages();
}

/*
 * Cleanup and shutdown
 */
void service_cleanup(void) {
    logger_info("Cleaning up daemon service...");

    /* Close IPC server */
    ipc_server_cleanup();

    /* Cleanup network module */
    network_cleanup();

    g_service_state.initialized = FALSE;

    logger_info("Daemon service cleanup complete");
}
