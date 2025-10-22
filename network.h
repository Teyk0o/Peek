/*
* PEEK - Network Monitor
*/

#ifndef PEEK_NETWORK_H
#define PEEK_NETWORK_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#include <psapi.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "psapi.lib")

#define MAX_CONNECTIONS 2000
#define MAX_PROCESS_NAME 260

typedef enum {
    CONN_OUTBOUND,  // Local initiated connection
    CONN_INBOUND,   // Remote initiated connection
    CONN_UNKNOWN
} ConnectionDirection;

typedef enum {
    PROTO_TCP,
    PROTO_UDP
} Protocol;

typedef enum {
    IP_V4,
    IP_V6
} IPVersion;

typedef struct {
    DWORD local_addr;
    DWORD local_port;
    DWORD remote_addr;
    DWORD remote_port;
    BYTE local_addr_v6[16];   // IPv6 address
    BYTE remote_addr_v6[16];  // IPv6 address
    DWORD pid;
    DWORD state;
    ConnectionDirection direction;
    BOOL is_localhost;
    Protocol protocol;
    IPVersion ip_version;
    char process_name[MAX_PROCESS_NAME];
    SYSTEMTIME timestamp;
} NetworkConnection;

typedef struct {
    int total_connections;
    int new_connections;
    int active_connections;
    int initial_connections;
} NetworkStats;

int network_init(void);

void network_cleanup(void);

int network_get_connections(NetworkConnection** connections, int* count);

int network_check_new_connections(NetworkConnection** new_connections, int* count);

void network_get_stats(NetworkStats* stats);

void network_format_ip(DWORD addr, char* buffer, size_t size);

void network_format_ipv6(const BYTE* addr, char* buffer, size_t size);

void network_get_process_name(DWORD pid, char* buffer, size_t size);

int network_get_all_seen_connections(NetworkConnection** connections, int* count);

#endif