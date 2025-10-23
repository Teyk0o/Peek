/*
* PEEK - Network Monitor
*/

#include "logger.h"
#include <stdarg.h>

#define ANSI_RESET   "\033[0m"
#define ANSI_BOLD    "\033[1m"
#define ANSI_GRAY    "\033[90m"
#define ANSI_BLUE    "\033[94m"
#define ANSI_GREEN   "\033[92m"
#define ANSI_YELLOW  "\033[93m"
#define ANSI_RED     "\033[91m"
#define ANSI_CYAN    "\033[96m"

static HANDLE console_handle = NULL;

void logger_init(void) {
    AllocConsole();

    console_handle = GetStdHandle(STD_OUTPUT_HANDLE);

    DWORD mode = 0;
    GetConsoleMode(console_handle, &mode);
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(console_handle, mode);

    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
}

void logger_log(const LogLevel level, const char* format, ...) {

    const time_t now = time(NULL);
    const struct tm* t = localtime(&now);
    char timestamp[32];
    sprintf(timestamp, "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);

    const char* prefix;
    const char* color;

    switch (level) {
        case LOG_DEBUG:
            prefix = "DEBUG";
            color = ANSI_GRAY;
            break;
        case LOG_INFO:
            prefix = "INFO ";
            color = ANSI_BLUE;
            break;
        case LOG_SUCCESS:
            prefix = "OK   ";
            color = ANSI_GREEN;
            break;
        case LOG_WARNING:
            prefix = "WARN ";
            color = ANSI_YELLOW;
            break;
        case LOG_ERROR:
            prefix = "ERROR";
            color = ANSI_RED;
            break;
        default:
            prefix = "LOG  ";
            color = ANSI_RESET;
    }

    printf("%s[%s]%s %s[%s]%s ",
           ANSI_GRAY, timestamp, ANSI_RESET,
           color, prefix, ANSI_RESET);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
    fflush(stdout);
}