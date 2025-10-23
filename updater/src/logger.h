/*
 * Simple logging for updater
 */

#ifndef PEEK_UPDATER_LOGGER_H
#define PEEK_UPDATER_LOGGER_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

void logger_init(void);

#define logger_info(fmt, ...) printf("[INFO] " fmt "\n", ##__VA_ARGS__)
#define logger_error(fmt, ...) printf("[ERROR] " fmt "\n", ##__VA_ARGS__)
#define logger_warning(fmt, ...) printf("[WARNING] " fmt "\n", ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* PEEK_UPDATER_LOGGER_H */
