/*
 * Binary and Manifest Downloader
 */

#ifndef PEEK_UPDATER_DOWNLOADER_H
#define PEEK_UPDATER_DOWNLOADER_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Download manifest from server
 * Returns: 0 on success, saves to manifest_path
 */
int downloader_fetch_manifest(const char *server_url, const char *manifest_path);

/*
 * Download binary file
 */
int downloader_fetch_binary(const char *url, const char *dest_path);

#ifdef __cplusplus
}
#endif

#endif /* PEEK_UPDATER_DOWNLOADER_H */
