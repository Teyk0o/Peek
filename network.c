/*
* PEEK - Network Monitor
*/

#include "network.h"
#include "logger.h"
#include <stdio.h>
#include <string.h>

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

void network_get_process_name(const DWORD pid, char* buffer, const size_t size) {
    const HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProcess) {
        char path[MAX_PATH];
        DWORD pathSize = MAX_PATH;
        if (QueryFullProcessImageNameA(hProcess, 0, path, &pathSize)) {
            const char* filename = strrchr(path, '\\');
            if (filename) {
                strncpy(buffer, filename + 1, size - 1);
            } else {
                strncpy(buffer, path, size - 1);
            }
        } else {
            snprintf(buffer, size, "PID:%lu", pid);
        }
        CloseHandle(hProcess);
    } else {
        snprintf(buffer, size, "PID:%lu", pid);
    }
    buffer[size - 1] = '\0';
}

int network_get_connections(NetworkConnection** connections, int* count) {
    PMIB_TCPTABLE_OWNER_PID pTcpTable = NULL;
    DWORD dwSize = 0;
    DWORD dwRetVal = 0;

    // Update listening ports cache for accurate direction detection
    update_listening_ports();

    dwRetVal = GetExtendedTcpTable(NULL, &dwSize, FALSE, AF_INET,
                                    TCP_TABLE_OWNER_PID_ALL, 0);

    if (dwRetVal != ERROR_INSUFFICIENT_BUFFER) {
        LOG_ERROR("GetExtendedTcpTable failed: %d", dwRetVal);
        return -1;
    }

    pTcpTable = (PMIB_TCPTABLE_OWNER_PID)malloc(dwSize);
    if (pTcpTable == NULL) {
        LOG_ERROR("Memory allocation error");
        return -1;
    }

    dwRetVal = GetExtendedTcpTable(pTcpTable, &dwSize, FALSE, AF_INET,
                                    TCP_TABLE_OWNER_PID_ALL, 0);

    if (dwRetVal != NO_ERROR) {
        LOG_ERROR("GetExtendedTcpTable failed: %d", dwRetVal);
        free(pTcpTable);
        return -1;
    }

    *connections = (NetworkConnection*)malloc(pTcpTable->dwNumEntries * sizeof(NetworkConnection));
    if (*connections == NULL) {
        free(pTcpTable);
        return -1;
    }

    *count = 0;
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

            // Check if this is a localhost connection (127.0.0.1)
            const unsigned char* local_bytes = (unsigned char*)&conn->local_addr;
            const unsigned char* remote_bytes = (unsigned char*)&conn->remote_addr;
            conn->is_localhost = (local_bytes[0] == 127 && remote_bytes[0] == 127);

            // Determine connection direction accurately using listening ports
            // If we're listening on the local port with this PID, it's INBOUND
            // Otherwise, we initiated the connection (OUTBOUND)
            if (is_listening_on_port(conn->local_port, conn->pid)) {
                conn->direction = CONN_INBOUND;  // We're the server accepting connections
            } else {
                conn->direction = CONN_OUTBOUND; // We initiated this connection
            }

            network_get_process_name(conn->pid, conn->process_name, MAX_PROCESS_NAME);

            GetLocalTime(&conn->timestamp);

            (*count)++;
        }
    }

    free(pTcpTable);
    return 0;
}

static BOOL connection_exists(const NetworkConnection* conn) {
    for (int i = 0; i < seen_count; i++) {
        if (seen_connections[i].local_addr == conn->local_addr &&
            seen_connections[i].local_port == conn->local_port &&
            seen_connections[i].remote_addr == conn->remote_addr &&
            seen_connections[i].remote_port == conn->remote_port &&
            seen_connections[i].pid == conn->pid) {
            return TRUE;
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