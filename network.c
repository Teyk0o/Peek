/*
* PEEK - Network Monitor
*/

#include "network.h"
#include "logger.h"
#include <stdio.h>
#include <string.h>
#include <psapi.h>

static NetworkConnection seen_connections[MAX_CONNECTIONS];
static int seen_count = 0;
static NetworkStats stats = {0};
static BOOL initialized = FALSE;

// Cache of listening ports - used to determine connection direction
typedef struct {
    DWORD port;
    DWORD pid;
} ListeningPort;

static ListeningPort listening_ports[1000];
static int listening_ports_count = 0;

int network_init(void) {
    LOG_INFO("Network module initialization...");

    WSADATA wsaData;
    const int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        LOG_ERROR("WSAStartup startup error: %d", result);
        return -1;
    }

    NetworkConnection* initial_conns = NULL;
    int initial_count = 0;

    if (network_get_connections(&initial_conns, &initial_count) == 0) {
        stats.initial_connections = initial_count;
        stats.active_connections = initial_count;

        for (int i = 0; i < initial_count && i < MAX_CONNECTIONS; i++) {
            memcpy(&seen_connections[i], &initial_conns[i], sizeof(NetworkConnection));
        }
        seen_count = (initial_count < MAX_CONNECTIONS) ? initial_count : MAX_CONNECTIONS;

        free(initial_conns);
        LOG_SUCCESS("Network module initialized (%d existing(s) connection(s))", initial_count);
    } else {
        LOG_WARNING("Unable to get initials connections");
    }

    initialized = TRUE;
    return 0;
}

void network_cleanup(void) {
    LOG_INFO("Network module clean-up");
    WSACleanup();
    initialized = FALSE;
}

static void update_listening_ports(void) {
    PMIB_TCPTABLE_OWNER_PID pTcpTable = NULL;
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;

    // Get table size
    dwRetVal = GetExtendedTcpTable(NULL, &dwSize, FALSE, AF_INET,
                                    TCP_TABLE_OWNER_PID_LISTENER, 0);

    if (dwRetVal != ERROR_INSUFFICIENT_BUFFER) {
        listening_ports_count = 0;
        return;
    }

    pTcpTable = (PMIB_TCPTABLE_OWNER_PID)malloc(dwSize);
    if (pTcpTable == NULL) {
        listening_ports_count = 0;
        return;
    }

    dwRetVal = GetExtendedTcpTable(pTcpTable, &dwSize, FALSE, AF_INET,
                                    TCP_TABLE_OWNER_PID_LISTENER, 0);

    if (dwRetVal != NO_ERROR) {
        free(pTcpTable);
        listening_ports_count = 0;
        return;
    }

    // Store listening ports
    listening_ports_count = 0;
    for (DWORD i = 0; i < pTcpTable->dwNumEntries && listening_ports_count < 1000; i++) {
        MIB_TCPROW_OWNER_PID row = pTcpTable->table[i];
        listening_ports[listening_ports_count].port = ntohs((u_short)row.dwLocalPort);
        listening_ports[listening_ports_count].pid = row.dwOwningPid;
        listening_ports_count++;
    }

    free(pTcpTable);
}

static BOOL is_listening_on_port(DWORD port, DWORD pid) {
    for (int i = 0; i < listening_ports_count; i++) {
        if (listening_ports[i].port == port && listening_ports[i].pid == pid) {
            return TRUE;
        }
    }
    return FALSE;
}

void network_format_ip(DWORD addr, char* buffer, const size_t size) {
    const unsigned char* bytes = (unsigned char*)&addr;
    snprintf(buffer, size, "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
}

void network_format_ipv6(const BYTE* addr, char* buffer, const size_t size) {
    // Convert IPv6 address to standard notation
    snprintf(buffer, size, "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x",
        addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], addr[6], addr[7],
        addr[8], addr[9], addr[10], addr[11], addr[12], addr[13], addr[14], addr[15]);
}

void network_get_process_name(const DWORD pid, char* buffer, const size_t size) {
    // Special case for System Idle Process
    if (pid == 0) {
        strncpy(buffer, "System Idle Process", size - 1);
        buffer[size - 1] = '\0';
        return;
    }

    // Special case for System process
    if (pid == 4) {
        strncpy(buffer, "System", size - 1);
        buffer[size - 1] = '\0';
        return;
    }

    char path[MAX_PATH];
    DWORD pathSize = MAX_PATH;
    HANDLE hProcess = NULL;

    // Try multiple access levels in order of preference
    DWORD access_levels[] = {
        PROCESS_QUERY_LIMITED_INFORMATION,
        PROCESS_QUERY_INFORMATION,
        PROCESS_VM_READ | PROCESS_QUERY_INFORMATION,
        PROCESS_ALL_ACCESS
    };

    for (int i = 0; i < 4; i++) {
        hProcess = OpenProcess(access_levels[i], FALSE, pid);
        if (hProcess) {
            if (QueryFullProcessImageNameA(hProcess, 0, path, &pathSize)) {
                const char* filename = strrchr(path, '\\');
                if (filename) {
                    strncpy(buffer, filename + 1, size - 1);
                } else {
                    strncpy(buffer, path, size - 1);
                }
                buffer[size - 1] = '\0';
                CloseHandle(hProcess);
                return;
            }
            CloseHandle(hProcess);
        }
    }

    // If all methods failed, try using Process Status API
    HMODULE hMods[1024];
    DWORD cbNeeded;
    hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);

    if (hProcess && EnumProcessModules(hProcess, hMods, sizeof(hMods), &cbNeeded)) {
        if (GetModuleBaseNameA(hProcess, hMods[0], buffer, size)) {
            CloseHandle(hProcess);
            return;
        }
    }

    if (hProcess) {
        CloseHandle(hProcess);
    }

    // Last resort: format as PID with system indicator
    snprintf(buffer, size, "[System] PID:%lu", pid);
    buffer[size - 1] = '\0';
}

static int get_tcp_connections_v4(NetworkConnection** connections, int* count) {
    PMIB_TCPTABLE_OWNER_PID pTcpTable = NULL;
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;

    dwRetVal = GetExtendedTcpTable(NULL, &dwSize, FALSE, AF_INET,
                                    TCP_TABLE_OWNER_PID_ALL, 0);

    if (dwRetVal != ERROR_INSUFFICIENT_BUFFER) {
        return -1;
    }

    pTcpTable = (PMIB_TCPTABLE_OWNER_PID)malloc(dwSize);
    if (pTcpTable == NULL) {
        return -1;
    }

    dwRetVal = GetExtendedTcpTable(pTcpTable, &dwSize, FALSE, AF_INET,
                                    TCP_TABLE_OWNER_PID_ALL, 0);

    if (dwRetVal != NO_ERROR) {
        free(pTcpTable);
        return -1;
    }

    for (DWORD i = 0; i < pTcpTable->dwNumEntries; i++) {
        MIB_TCPROW_OWNER_PID row = pTcpTable->table[i];

        if (row.dwState == MIB_TCP_STATE_ESTAB && row.dwRemoteAddr != 0) {
            NetworkConnection* conn = &(*connections)[*count];

            conn->local_addr = row.dwLocalAddr;
            conn->local_port = ntohs((u_short)row.dwLocalPort);
            conn->remote_addr = row.dwRemoteAddr;
            conn->remote_port = ntohs((u_short)row.dwRemotePort);
            conn->pid = row.dwOwningPid;
            conn->state = row.dwState;
            conn->protocol = PROTO_TCP;
            conn->ip_version = IP_V4;

            // Check if this is a localhost connection (127.0.0.1)
            const unsigned char* local_bytes = (unsigned char*)&conn->local_addr;
            const unsigned char* remote_bytes = (unsigned char*)&conn->remote_addr;
            conn->is_localhost = (local_bytes[0] == 127 && remote_bytes[0] == 127);

            // Determine connection direction
            if (is_listening_on_port(conn->local_port, conn->pid)) {
                conn->direction = CONN_INBOUND;
            } else {
                conn->direction = CONN_OUTBOUND;
            }

            network_get_process_name(conn->pid, conn->process_name, MAX_PROCESS_NAME);
            GetLocalTime(&conn->timestamp);

            (*count)++;
        }
    }

    free(pTcpTable);
    return 0;
}

static int get_tcp_connections_v6(NetworkConnection** connections, int* count) {
    PMIB_TCP6TABLE_OWNER_PID pTcp6Table = NULL;
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;

    dwRetVal = GetExtendedTcpTable(NULL, &dwSize, FALSE, AF_INET6,
                                    TCP_TABLE_OWNER_PID_ALL, 0);

    if (dwRetVal != ERROR_INSUFFICIENT_BUFFER) {
        return -1;
    }

    pTcp6Table = (PMIB_TCP6TABLE_OWNER_PID)malloc(dwSize);
    if (pTcp6Table == NULL) {
        return -1;
    }

    dwRetVal = GetExtendedTcpTable(pTcp6Table, &dwSize, FALSE, AF_INET6,
                                    TCP_TABLE_OWNER_PID_ALL, 0);

    if (dwRetVal != NO_ERROR) {
        free(pTcp6Table);
        return -1;
    }

    for (DWORD i = 0; i < pTcp6Table->dwNumEntries; i++) {
        MIB_TCP6ROW_OWNER_PID row = pTcp6Table->table[i];

        if (row.dwState == MIB_TCP_STATE_ESTAB) {
            NetworkConnection* conn = &(*connections)[*count];

            memcpy(conn->local_addr_v6, row.ucLocalAddr, 16);
            memcpy(conn->remote_addr_v6, row.ucRemoteAddr, 16);
            conn->local_port = ntohs((u_short)row.dwLocalPort);
            conn->remote_port = ntohs((u_short)row.dwRemotePort);
            conn->pid = row.dwOwningPid;
            conn->state = row.dwState;
            conn->protocol = PROTO_TCP;
            conn->ip_version = IP_V6;

            // Check for localhost (::1)
            conn->is_localhost = (memcmp(conn->local_addr_v6, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1", 16) == 0 &&
                                  memcmp(conn->remote_addr_v6, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1", 16) == 0);

            conn->direction = CONN_OUTBOUND; // Simplified for IPv6

            network_get_process_name(conn->pid, conn->process_name, MAX_PROCESS_NAME);
            GetLocalTime(&conn->timestamp);

            (*count)++;
        }
    }

    free(pTcp6Table);
    return 0;
}

static int get_udp_connections_v4(NetworkConnection** connections, int* count) {
    PMIB_UDPTABLE_OWNER_PID pUdpTable = NULL;
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;

    dwRetVal = GetExtendedUdpTable(NULL, &dwSize, FALSE, AF_INET,
                                    UDP_TABLE_OWNER_PID, 0);

    if (dwRetVal != ERROR_INSUFFICIENT_BUFFER) {
        return -1;
    }

    pUdpTable = (PMIB_UDPTABLE_OWNER_PID)malloc(dwSize);
    if (pUdpTable == NULL) {
        return -1;
    }

    dwRetVal = GetExtendedUdpTable(pUdpTable, &dwSize, FALSE, AF_INET,
                                    UDP_TABLE_OWNER_PID, 0);

    if (dwRetVal != NO_ERROR) {
        free(pUdpTable);
        return -1;
    }

    for (DWORD i = 0; i < pUdpTable->dwNumEntries; i++) {
        MIB_UDPROW_OWNER_PID row = pUdpTable->table[i];

        NetworkConnection* conn = &(*connections)[*count];

        conn->local_addr = row.dwLocalAddr;
        conn->local_port = ntohs((u_short)row.dwLocalPort);
        conn->remote_addr = 0; // UDP doesn't have remote connection info
        conn->remote_port = 0;
        conn->pid = row.dwOwningPid;
        conn->state = 0; // UDP is connectionless
        conn->protocol = PROTO_UDP;
        conn->ip_version = IP_V4;

        // Check if this is localhost
        const unsigned char* local_bytes = (unsigned char*)&conn->local_addr;
        conn->is_localhost = (local_bytes[0] == 127);

        conn->direction = CONN_UNKNOWN; // UDP doesn't have clear direction

        network_get_process_name(conn->pid, conn->process_name, MAX_PROCESS_NAME);
        GetLocalTime(&conn->timestamp);

        (*count)++;
    }

    free(pUdpTable);
    return 0;
}

static int get_udp_connections_v6(NetworkConnection** connections, int* count) {
    PMIB_UDP6TABLE_OWNER_PID pUdp6Table = NULL;
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;

    dwRetVal = GetExtendedUdpTable(NULL, &dwSize, FALSE, AF_INET6,
                                    UDP_TABLE_OWNER_PID, 0);

    if (dwRetVal != ERROR_INSUFFICIENT_BUFFER) {
        return -1;
    }

    pUdp6Table = (PMIB_UDP6TABLE_OWNER_PID)malloc(dwSize);
    if (pUdp6Table == NULL) {
        return -1;
    }

    dwRetVal = GetExtendedUdpTable(pUdp6Table, &dwSize, FALSE, AF_INET6,
                                    UDP_TABLE_OWNER_PID, 0);

    if (dwRetVal != NO_ERROR) {
        free(pUdp6Table);
        return -1;
    }

    for (DWORD i = 0; i < pUdp6Table->dwNumEntries; i++) {
        MIB_UDP6ROW_OWNER_PID row = pUdp6Table->table[i];

        NetworkConnection* conn = &(*connections)[*count];

        memcpy(conn->local_addr_v6, row.ucLocalAddr, 16);
        memset(conn->remote_addr_v6, 0, 16); // UDP doesn't have remote connection
        conn->local_port = ntohs((u_short)row.dwLocalPort);
        conn->remote_port = 0;
        conn->pid = row.dwOwningPid;
        conn->state = 0; // UDP is connectionless
        conn->protocol = PROTO_UDP;
        conn->ip_version = IP_V6;

        // Check for localhost (::1)
        conn->is_localhost = (memcmp(conn->local_addr_v6, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\1", 16) == 0);

        conn->direction = CONN_UNKNOWN; // UDP doesn't have clear direction

        network_get_process_name(conn->pid, conn->process_name, MAX_PROCESS_NAME);
        GetLocalTime(&conn->timestamp);

        (*count)++;
    }

    free(pUdp6Table);
    return 0;
}

int network_get_connections(NetworkConnection** connections, int* count) {
    // Update listening ports cache for accurate direction detection
    update_listening_ports();

    // Allocate space for both IPv4 and IPv6 connections
    *connections = (NetworkConnection*)malloc(MAX_CONNECTIONS * sizeof(NetworkConnection));
    if (*connections == NULL) {
        LOG_ERROR("Memory allocation error");
        return -1;
    }

    *count = 0;

    // Get IPv4 TCP connections
    if (get_tcp_connections_v4(connections, count) != 0) {
        LOG_WARNING("Failed to get IPv4 TCP connections");
    }

    // Get IPv6 TCP connections
    if (get_tcp_connections_v6(connections, count) != 0) {
        LOG_WARNING("Failed to get IPv6 TCP connections");
    }

    // Get IPv4 UDP connections
    if (get_udp_connections_v4(connections, count) != 0) {
        LOG_WARNING("Failed to get IPv4 UDP connections");
    }

    // Get IPv6 UDP connections
    if (get_udp_connections_v6(connections, count) != 0) {
        LOG_WARNING("Failed to get IPv6 UDP connections");
    }

    return 0;
}

static BOOL connection_exists(const NetworkConnection* conn) {
    for (int i = 0; i < seen_count; i++) {
        // Check protocol and IP version first
        if (seen_connections[i].protocol != conn->protocol ||
            seen_connections[i].ip_version != conn->ip_version) {
            continue;
        }

        // Check IPv4 addresses
        if (conn->ip_version == IP_V4) {
            if (seen_connections[i].local_addr == conn->local_addr &&
                seen_connections[i].local_port == conn->local_port &&
                seen_connections[i].remote_addr == conn->remote_addr &&
                seen_connections[i].remote_port == conn->remote_port &&
                seen_connections[i].pid == conn->pid) {
                return TRUE;
            }
        }
        // Check IPv6 addresses
        else if (conn->ip_version == IP_V6) {
            if (memcmp(seen_connections[i].local_addr_v6, conn->local_addr_v6, 16) == 0 &&
                seen_connections[i].local_port == conn->local_port &&
                memcmp(seen_connections[i].remote_addr_v6, conn->remote_addr_v6, 16) == 0 &&
                seen_connections[i].remote_port == conn->remote_port &&
                seen_connections[i].pid == conn->pid) {
                return TRUE;
            }
        }
    }
    return FALSE;
}

static void add_seen_connection(const NetworkConnection* conn) {
    if (seen_count < MAX_CONNECTIONS) {
        memcpy(&seen_connections[seen_count], conn, sizeof(NetworkConnection));
        seen_count++;
    }
}

int network_check_new_connections(NetworkConnection** new_connections, int* count) {
    if (!initialized) {
        return -1;
    }

    NetworkConnection* current_conns = NULL;
    int current_count = 0;

    if (network_get_connections(&current_conns, &current_count) != 0) {
        return -1;
    }

    *new_connections = (NetworkConnection*)malloc(current_count * sizeof(NetworkConnection));
    if (*new_connections == NULL) {
        free(current_conns);
        return -1;
    }

    *count = 0;

    for (int i = 0; i < current_count; i++) {
        if (!connection_exists(&current_conns[i])) {
            memcpy(&(*new_connections)[*count], &current_conns[i], sizeof(NetworkConnection));
            add_seen_connection(&current_conns[i]);
            (*count)++;
            stats.new_connections++;
        }
    }

    stats.active_connections = current_count;
    stats.total_connections = seen_count;

    free(current_conns);
    return 0;
}

void network_get_stats(NetworkStats* out_stats) {
    if (out_stats) {
        memcpy(out_stats, &stats, sizeof(NetworkStats));
    }
}

int network_get_all_seen_connections(NetworkConnection** connections, int* count) {
    if (!initialized) {
        return -1;
    }

    *connections = (NetworkConnection*)malloc(seen_count * sizeof(NetworkConnection));
    if (*connections == NULL) {
        return -1;
    }

    for (int i = 0; i < seen_count; i++) {
        memcpy(&(*connections)[i], &seen_connections[i], sizeof(NetworkConnection));
    }

    *count = seen_count;
    return 0;
}