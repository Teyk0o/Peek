/*
 * PEEK Updater (updater.exe)
 *
 * Secure auto-update utility for Peek components
 *
 * Features:
 * - Download manifest and binaries from release server
 * - Verify RSA-SHA256 manifest signature
 * - Verify SHA256 binary hashes
 * - Atomic installation with rollback
 * - Silent or interactive modes
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "downloader.h"
#include "verifier.h"
#include "installer.h"
#include "logger.h"

/*
 * Print usage
 */
static void print_usage(const char *program_name) {
    printf("Peek Updater v2.0\n\n");
    printf("Usage: %s [options]\n\n", program_name);
    printf("Options:\n");
    printf("  --check                Check for updates (exit with code 0 if available)\n");
    printf("  --install               Download and install available update\n");
    printf("  --force-install <url>   Force install from specific URL\n");
    printf("  --silent                Run silently without prompts\n");
    printf("  --help                  Show this help message\n");
    printf("\n");
}

/*
 * Main entry point
 */
int main(int argc, char *argv[]) {
    logger_init();

    if (argc < 2) {
        print_usage(argv[0]);
        return 0;
    }

    int result = 0;
    BOOL silent_mode = FALSE;

    /* Parse arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--silent") == 0) {
            silent_mode = TRUE;
        } else if (strcmp(argv[i], "--check") == 0) {
            logger_info("Checking for updates...");
            /* TODO: Check for updates */
            printf("No update available\n");
            return 1;
        } else if (strcmp(argv[i], "--install") == 0) {
            logger_info("Checking for and installing updates...");
            /* TODO: Download and install */
            return 0;
        } else if (strcmp(argv[i], "--force-install") == 0) {
            if (i + 1 < argc) {
                logger_info("Force installing from: %s", argv[i + 1]);
                /* TODO: Force install from URL */
                i++;
            }
        }
    }

    return result;
}
