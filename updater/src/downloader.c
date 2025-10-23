/*
 * Downloader Implementation
 */

#define _CRT_SECURE_NO_WARNINGS
#pragma warning(disable:4996)
#pragma warning(disable:4100)

#include "downloader.h"
#include "logger.h"

int downloader_fetch_manifest(const char *server_url, const char *manifest_path) {
    logger_info("Downloading manifest from %s", server_url);
    /* TODO: Download manifest.json from server */
    return 0;
}

int downloader_fetch_binary(const char *url, const char *dest_path) {
    logger_info("Downloading binary from %s", url);
    /* TODO: Download binary file */
    return 0;
}
