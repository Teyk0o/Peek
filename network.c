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

            // Determine connection direction
            // Heuristic: high port (>= 49152) typically means outbound from that end
            // Common server ports (< 1024) typically mean inbound to client
            if (conn->local_port >= 49152 && conn->remote_port < 49152) {
                conn->direction = CONN_OUTBOUND;  // We initiated
            } else if (conn->remote_port >= 49152 && conn->local_port < 49152) {
                conn->direction = CONN_INBOUND;   // Remote initiated
            } else if (conn->remote_port == 80 || conn->remote_port == 443 ||
                       conn->remote_port == 21 || conn->remote_port == 22) {
                conn->direction = CONN_OUTBOUND;  // Common protocols we're accessing
            } else {
                conn->direction = CONN_OUTBOUND;  // Default to outbound
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