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

// Security info cache to avoid redundant expensive operations
#define MAX_SECURITY_CACHE 500
typedef struct {
    char process_path[MAX_PATH];
    char sha256_hash[SHA256_HASH_LENGTH];
    TrustStatus trust_status;
    BOOL valid;
} SecurityCacheEntry;

static SecurityCacheEntry security_cache[MAX_SECURITY_CACHE];
static int security_cache_count = 0;

// Critical section for thread-safe cache access
static CRITICAL_SECTION security_cache_cs;
static BOOL cs_initialized = FALSE;

// Critical section for thread-safe seen_connections access
static CRITICAL_SECTION seen_connections_cs;
static BOOL seen_cs_initialized = FALSE;

// Manual trust overrides - persisted to disk
#define MAX_TRUST_OVERRIDES 500
typedef struct {
    char process_path[MAX_PATH];
    TrustStatus override_status;
    BOOL valid;
} TrustOverride;

static TrustOverride trust_overrides[MAX_TRUST_OVERRIDES];
static int trust_overrides_count = 0;

// Secure file path in %APPDATA%
static char TRUST_OVERRIDES_FILE[MAX_PATH] = {0};
static BOOL file_path_initialized = FALSE;

// Cache of listening ports - used to determine connection direction
typedef struct {
    DWORD port;
    DWORD pid;
} ListeningPort;

static ListeningPort listening_ports[1000];
static int listening_ports_count = 0;

int network_init(void) {
    LOG_INFO("Network module initialization...");

    // Initialize critical section for thread-safe cache
    if (!cs_initialized) {
        InitializeCriticalSection(&security_cache_cs);
        cs_initialized = TRUE;
    }

    // Initialize critical section for thread-safe seen_connections access
    if (!seen_cs_initialized) {
        InitializeCriticalSection(&seen_connections_cs);
        seen_cs_initialized = TRUE;
    }

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

    // Load manual trust overrides from file
    network_load_trust_overrides();

    initialized = TRUE;
    return 0;
}

void network_cleanup(void) {
    LOG_INFO("Network module clean-up");
    WSACleanup();

    if (cs_initialized) {
        DeleteCriticalSection(&security_cache_cs);
        cs_initialized = FALSE;
    }

    if (seen_cs_initialized) {
        DeleteCriticalSection(&seen_connections_cs);
        seen_cs_initialized = FALSE;
    }

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
            network_get_process_path_only(conn->pid, conn->process_path, MAX_PATH);
            strcpy(conn->sha256_hash, "N/A");
            conn->trust_status = TRUST_UNKNOWN;
            conn->security_info_loaded = FALSE;
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
            network_get_process_path_only(conn->pid, conn->process_path, MAX_PATH);
            strcpy(conn->sha256_hash, "N/A");
            conn->trust_status = TRUST_UNKNOWN;
            conn->security_info_loaded = FALSE;
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
        network_get_process_security_info(conn->pid, conn->process_path, MAX_PATH,
                                           conn->sha256_hash, SHA256_HASH_LENGTH,
                                           &conn->trust_status);
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
        network_get_process_security_info(conn->pid, conn->process_path, MAX_PATH,
                                           conn->sha256_hash, SHA256_HASH_LENGTH,
                                           &conn->trust_status);
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

// ============================================================================
// Security Cache Functions
// ============================================================================

static SecurityCacheEntry* find_in_security_cache(const char* process_path) {
    SecurityCacheEntry* result = NULL;

    EnterCriticalSection(&security_cache_cs);
    for (int i = 0; i < security_cache_count; i++) {
        if (security_cache[i].valid && strcmp(security_cache[i].process_path, process_path) == 0) {
            result = &security_cache[i];
            break;
        }
    }
    LeaveCriticalSection(&security_cache_cs);

    return result;
}

static void add_to_security_cache(const char* process_path, const char* hash, TrustStatus status) {
    EnterCriticalSection(&security_cache_cs);

    if (security_cache_count >= MAX_SECURITY_CACHE) {
        LeaveCriticalSection(&security_cache_cs);
        return;
    }

    SecurityCacheEntry* entry = &security_cache[security_cache_count];
    strncpy(entry->process_path, process_path, MAX_PATH - 1);
    entry->process_path[MAX_PATH - 1] = '\0';
    strncpy(entry->sha256_hash, hash, SHA256_HASH_LENGTH - 1);
    entry->sha256_hash[SHA256_HASH_LENGTH - 1] = '\0';
    entry->trust_status = status;
    entry->valid = TRUE;

    security_cache_count++;

    LeaveCriticalSection(&security_cache_cs);
}

// ============================================================================
// Security & Integrity Functions
// ============================================================================

BOOL network_calculate_sha256(const char* file_path, char* hash_output, size_t hash_size) {
    if (!file_path || !hash_output || hash_size < SHA256_HASH_LENGTH) {
        return FALSE;
    }

    // Open file
    HANDLE hFile = CreateFileA(file_path, GENERIC_READ, FILE_SHARE_READ,
                                NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return FALSE;
    }

    BOOL success = FALSE;
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE hash[32];  // SHA256 = 32 bytes
    DWORD hashLen = 32;
    BYTE buffer[8192];
    DWORD bytesRead = 0;

    // Acquire crypto context
    if (!CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        goto cleanup;
    }

    // Create hash object
    if (!CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash)) {
        goto cleanup;
    }

    // Read file and hash
    while (ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
        if (!CryptHashData(hHash, buffer, bytesRead, 0)) {
            goto cleanup;
        }
    }

    // Get hash value
    if (!CryptGetHashParam(hHash, HP_HASHVAL, hash, &hashLen, 0)) {
        goto cleanup;
    }

    // Convert to hex string
    for (DWORD i = 0; i < hashLen; i++) {
        sprintf(&hash_output[i * 2], "%02x", hash[i]);
    }
    hash_output[hashLen * 2] = '\0';

    success = TRUE;

cleanup:
    if (hHash) CryptDestroyHash(hHash);
    if (hProv) CryptReleaseContext(hProv, 0);
    CloseHandle(hFile);

    return success;
}

TrustStatus network_verify_signature(const char* file_path) {
    if (!file_path || strlen(file_path) == 0) {
        return TRUST_ERROR;
    }

    // Convert to wide string
    WCHAR wFilePath[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, file_path, -1, wFilePath, MAX_PATH);

    // Setup WINTRUST_FILE_INFO
    WINTRUST_FILE_INFO fileInfo;
    memset(&fileInfo, 0, sizeof(fileInfo));
    fileInfo.cbStruct = sizeof(WINTRUST_FILE_INFO);
    fileInfo.pcwszFilePath = wFilePath;
    fileInfo.hFile = NULL;
    fileInfo.pgKnownSubject = NULL;

    // Setup WINTRUST_DATA
    WINTRUST_DATA winTrustData;
    memset(&winTrustData, 0, sizeof(winTrustData));
    winTrustData.cbStruct = sizeof(WINTRUST_DATA);
    winTrustData.pPolicyCallbackData = NULL;
    winTrustData.pSIPClientData = NULL;
    winTrustData.dwUIChoice = WTD_UI_NONE;
    winTrustData.fdwRevocationChecks = WTD_REVOKE_NONE;
    winTrustData.dwUnionChoice = WTD_CHOICE_FILE;
    winTrustData.dwStateAction = WTD_STATEACTION_VERIFY;
    winTrustData.hWVTStateData = NULL;
    winTrustData.pwszURLReference = NULL;
    winTrustData.dwProvFlags = WTD_SAFER_FLAG;
    winTrustData.dwUIContext = 0;
    winTrustData.pFile = &fileInfo;

    // WinVerifyTrust GUID
    GUID policyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;

    // Verify signature
    LONG status = WinVerifyTrust(NULL, &policyGUID, &winTrustData);

    // Close state data
    winTrustData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(NULL, &policyGUID, &winTrustData);

    // Interpret result - basic check first
    TrustStatus basic_status;
    switch (status) {
        case ERROR_SUCCESS:
            basic_status = TRUST_VERIFIED_SIGNED;  // Will check if Microsoft below
            break;

        case TRUST_E_NOSIGNATURE:
            return TRUST_UNSIGNED;

        case TRUST_E_EXPLICIT_DISTRUST:
        case TRUST_E_SUBJECT_NOT_TRUSTED:
        case CRYPT_E_SECURITY_SETTINGS:
            return TRUST_INVALID;

        default:
            return TRUST_ERROR;
    }

    // If signed, check if it's Microsoft/Windows
    if (basic_status == TRUST_VERIFIED_SIGNED) {
        // Check signer name
        HCERTSTORE hStore = NULL;
        HCRYPTMSG hMsg = NULL;
        DWORD dwEncoding = X509_ASN_ENCODING | PKCS_7_ASN_ENCODING;
        DWORD dwContentType = 0;
        DWORD dwFormatType = 0;

        if (CryptQueryObject(CERT_QUERY_OBJECT_FILE, wFilePath,
                             CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                             CERT_QUERY_FORMAT_FLAG_BINARY,
                             0, &dwEncoding, &dwContentType, &dwFormatType,
                             &hStore, &hMsg, NULL)) {

            // Get signer info
            DWORD dwSignerInfo = 0;
            CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, NULL, &dwSignerInfo);

            if (dwSignerInfo > 0) {
                PCMSG_SIGNER_INFO pSignerInfo = (PCMSG_SIGNER_INFO)malloc(dwSignerInfo);
                if (CryptMsgGetParam(hMsg, CMSG_SIGNER_INFO_PARAM, 0, pSignerInfo, &dwSignerInfo)) {
                    // Find certificate
                    CERT_INFO certInfo;
                    certInfo.Issuer = pSignerInfo->Issuer;
                    certInfo.SerialNumber = pSignerInfo->SerialNumber;

                    PCCERT_CONTEXT pCertContext = CertFindCertificateInStore(
                        hStore, dwEncoding, 0, CERT_FIND_SUBJECT_CERT, &certInfo, NULL);

                    if (pCertContext) {
                        // Get subject name
                        DWORD dwNameSize = CertGetNameStringW(pCertContext,
                                                              CERT_NAME_SIMPLE_DISPLAY_TYPE,
                                                              0, NULL, NULL, 0);
                        if (dwNameSize > 1) {
                            WCHAR* szName = (WCHAR*)malloc(dwNameSize * sizeof(WCHAR));
                            CertGetNameStringW(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE,
                                             0, NULL, szName, dwNameSize);

                            // Check if Microsoft
                            if (wcsstr(szName, L"Microsoft") != NULL ||
                                wcsstr(szName, L"Windows") != NULL) {
                                basic_status = TRUST_MICROSOFT_SIGNED;
                            }

                            free(szName);
                        }
                        CertFreeCertificateContext(pCertContext);
                    }
                }
                free(pSignerInfo);
            }

            if (hMsg) CryptMsgClose(hMsg);
            if (hStore) CertCloseStore(hStore, 0);
        }
    }

    return basic_status;
}

void network_get_process_security_info(DWORD pid, char* path_buffer, size_t path_size,
                                        char* hash_buffer, size_t hash_size,
                                        TrustStatus* trust_status) {
    // Initialize outputs
    if (path_buffer && path_size > 0) {
        path_buffer[0] = '\0';
    }
    if (hash_buffer && hash_size > 0) {
        strcpy(hash_buffer, "N/A");
    }
    if (trust_status) {
        *trust_status = TRUST_UNKNOWN;
    }

    // Special cases
    if (pid == 0 || pid == 4) {
        if (path_buffer) strncpy(path_buffer, "[System Process]", path_size - 1);
        if (hash_buffer) strcpy(hash_buffer, "N/A");
        if (trust_status) *trust_status = TRUST_MICROSOFT_SIGNED;  // System processes are Microsoft signed
        return;
    }

    // Get process path
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) {
        return;
    }

    char path[MAX_PATH];
    DWORD pathSize = MAX_PATH;
    if (QueryFullProcessImageNameA(hProcess, 0, path, &pathSize)) {
        if (path_buffer) {
            strncpy(path_buffer, path, path_size - 1);
            path_buffer[path_size - 1] = '\0';
        }

        // Check cache first
        SecurityCacheEntry* cached = find_in_security_cache(path);
        if (cached) {
            // Use cached results
            if (hash_buffer && hash_size >= SHA256_HASH_LENGTH) {
                strncpy(hash_buffer, cached->sha256_hash, hash_size - 1);
                hash_buffer[hash_size - 1] = '\0';
            }
            if (trust_status) {
                *trust_status = cached->trust_status;
            }
        } else {
            // Not in cache - compute and cache
            char computed_hash[SHA256_HASH_LENGTH];
            TrustStatus computed_trust = TRUST_UNKNOWN;

            // Calculate SHA256 hash
            if (network_calculate_sha256(path, computed_hash, SHA256_HASH_LENGTH)) {
                if (hash_buffer && hash_size >= SHA256_HASH_LENGTH) {
                    strncpy(hash_buffer, computed_hash, hash_size - 1);
                    hash_buffer[hash_size - 1] = '\0';
                }
            } else {
                strcpy(computed_hash, "Error");
                if (hash_buffer) strcpy(hash_buffer, "Error");
            }

            // Verify signature (expensive operation)
            computed_trust = network_verify_signature(path);
            if (trust_status) {
                *trust_status = computed_trust;
            }

            // Add to cache
            add_to_security_cache(path, computed_hash, computed_trust);
        }
    }

    CloseHandle(hProcess);
}

void network_get_process_path_only(DWORD pid, char* path_buffer, size_t path_size) {
    // Initialize output
    if (path_buffer && path_size > 0) {
        path_buffer[0] = '\0';
    }

    // Special cases
    if (pid == 0 || pid == 4) {
        if (path_buffer) strncpy(path_buffer, "[System Process]", path_size - 1);
        return;
    }

    // Get process path only (fast operation)
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) {
        return;
    }

    char path[MAX_PATH];
    DWORD pathSize = MAX_PATH;
    if (QueryFullProcessImageNameA(hProcess, 0, path, &pathSize)) {
        if (path_buffer) {
            strncpy(path_buffer, path, path_size - 1);
            path_buffer[path_size - 1] = '\0';
        }
    }

    CloseHandle(hProcess);
}

void network_compute_security_info_deferred(NetworkConnection* conn) {
    if (!conn || conn->security_info_loaded) {
        return; // Already loaded
    }

    // If no path, try to get it
    if (strlen(conn->process_path) == 0) {
        network_get_process_path_only(conn->pid, conn->process_path, MAX_PATH);
    }

    // If still no path, mark as loaded and return
    if (strlen(conn->process_path) == 0) {
        strcpy(conn->sha256_hash, "N/A");
        conn->trust_status = TRUST_UNKNOWN;
        conn->security_info_loaded = TRUE;
        return;
    }

    // Special case for system processes
    if (conn->pid == 0 || conn->pid == 4) {
        strcpy(conn->sha256_hash, "N/A");
        conn->trust_status = TRUST_MICROSOFT_SIGNED;
        conn->security_info_loaded = TRUE;
        return;
    }

    // Check for manual trust override first
    TrustStatus override = network_get_trust_override(conn->process_path);
    if (override != TRUST_UNKNOWN) {
        // Manual override exists - use it
        conn->trust_status = override;
        // Still compute hash for info
        if (network_calculate_sha256(conn->process_path, conn->sha256_hash, SHA256_HASH_LENGTH)) {
            // Hash computed
        } else {
            strcpy(conn->sha256_hash, "N/A");
        }
        conn->security_info_loaded = TRUE;
        return;
    }

    // Check cache
    SecurityCacheEntry* cached = find_in_security_cache(conn->process_path);
    if (cached) {
        strncpy(conn->sha256_hash, cached->sha256_hash, SHA256_HASH_LENGTH - 1);
        conn->sha256_hash[SHA256_HASH_LENGTH - 1] = '\0';
        conn->trust_status = cached->trust_status;
    } else {
        // Compute and cache
        char computed_hash[SHA256_HASH_LENGTH];

        if (network_calculate_sha256(conn->process_path, computed_hash, SHA256_HASH_LENGTH)) {
            strncpy(conn->sha256_hash, computed_hash, SHA256_HASH_LENGTH - 1);
            conn->sha256_hash[SHA256_HASH_LENGTH - 1] = '\0';
        } else {
            strcpy(conn->sha256_hash, "Error");
            strcpy(computed_hash, "Error");
        }

        conn->trust_status = network_verify_signature(conn->process_path);
        add_to_security_cache(conn->process_path, computed_hash, conn->trust_status);
    }

    conn->security_info_loaded = TRUE;
}

// Thread worker function for computing security info
static DWORD WINAPI SecurityWorkerThread(LPVOID lpParam) {
    NetworkConnection* conn = (NetworkConnection*)lpParam;
    if (conn) {
        network_compute_security_info_deferred(conn);
    }
    return 0;
}

void network_compute_security_batch_parallel(NetworkConnection* connections, int count) {
    if (!connections || count <= 0) {
        return;
    }

    #define MAX_CONCURRENT_THREADS 8
    HANDLE threads[MAX_CONCURRENT_THREADS];
    int active_threads = 0;
    int processed = 0;

    while (processed < count) {
        // Launch threads up to MAX_CONCURRENT_THREADS
        while (active_threads < MAX_CONCURRENT_THREADS && processed < count) {
            if (!connections[processed].security_info_loaded) {
                threads[active_threads] = CreateThread(
                    NULL,
                    0,
                    SecurityWorkerThread,
                    &connections[processed],
                    0,
                    NULL
                );

                if (threads[active_threads] != NULL) {
                    active_threads++;
                }
            }
            processed++;
        }

        // Wait for at least one thread to complete
        if (active_threads > 0) {
            WaitForMultipleObjects(active_threads, threads, TRUE, INFINITE);

            // Close thread handles
            for (int i = 0; i < active_threads; i++) {
                CloseHandle(threads[i]);
            }
            active_threads = 0;
        }
    }
}

// Find a connection in seen_connections by matching key fields
NetworkConnection* network_find_connection(DWORD pid, DWORD remote_addr, DWORD remote_port, DWORD local_port) {
    if (!initialized) {
        return NULL;
    }

    EnterCriticalSection(&seen_connections_cs);

    for (int i = 0; i < seen_count; i++) {
        if (seen_connections[i].pid == pid &&
            seen_connections[i].remote_addr == remote_addr &&
            seen_connections[i].remote_port == remote_port &&
            seen_connections[i].local_port == local_port) {

            LeaveCriticalSection(&seen_connections_cs);
            return &seen_connections[i];
        }
    }

    LeaveCriticalSection(&seen_connections_cs);
    return NULL;
}

// Compute security info for all seen connections in parallel, updating in place
void network_compute_security_for_all_seen(void) {
    if (!initialized) {
        return;
    }

    #define MAX_CONCURRENT_THREADS 8
    HANDLE threads[MAX_CONCURRENT_THREADS];
    int active_threads = 0;
    int processed = 0;

    EnterCriticalSection(&seen_connections_cs);
    int total_count = seen_count;
    LeaveCriticalSection(&seen_connections_cs);

    while (processed < total_count) {
        // Launch threads up to MAX_CONCURRENT_THREADS
        while (active_threads < MAX_CONCURRENT_THREADS && processed < total_count) {
            EnterCriticalSection(&seen_connections_cs);
            BOOL should_process = (processed < seen_count && !seen_connections[processed].security_info_loaded);
            NetworkConnection* conn_ptr = &seen_connections[processed];
            LeaveCriticalSection(&seen_connections_cs);

            if (should_process) {
                threads[active_threads] = CreateThread(
                    NULL,
                    0,
                    SecurityWorkerThread,
                    conn_ptr,
                    0,
                    NULL
                );

                if (threads[active_threads] != NULL) {
                    active_threads++;
                }
            }
            processed++;
        }

        // Wait for at least one thread to complete
        if (active_threads > 0) {
            WaitForMultipleObjects(active_threads, threads, TRUE, INFINITE);

            // Close thread handles
            for (int i = 0; i < active_threads; i++) {
                CloseHandle(threads[i]);
            }
            active_threads = 0;
        }
    }
}

// ============================================================================
// Manual Trust Override Functions - Secured with DPAPI + Atomic Writes
// ============================================================================

// Initialize secure file path in %APPDATA%\Peek\

static void init_secure_file_path(void) {
    if (file_path_initialized) {
        return;
    }

    char appdata[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) == S_OK) {
        // Create Peek subdirectory
        snprintf(TRUST_OVERRIDES_FILE, MAX_PATH, "%s\\Peek", appdata);
        CreateDirectoryA(TRUST_OVERRIDES_FILE, NULL);

        // Full path to trust file
        snprintf(TRUST_OVERRIDES_FILE, MAX_PATH, "%s\\Peek\\trust_overrides.dat", appdata);
        file_path_initialized = TRUE;
        LOG_INFO("Secure file path: %s", TRUST_OVERRIDES_FILE);
    } else {
        // Fallback to current directory
        strcpy(TRUST_OVERRIDES_FILE, "trust_overrides.dat");
        file_path_initialized = TRUE;
        LOG_WARNING("Using fallback file path");
    }
}

// Encrypt data using DPAPI (user-scoped, tied to Windows account)
static BOOL dpapi_encrypt(const BYTE* plaintext, DWORD plaintext_len, BYTE** ciphertext, DWORD* ciphertext_len) {
    DATA_BLOB input;
    DATA_BLOB output;

    input.pbData = (BYTE*)plaintext;
    input.cbData = plaintext_len;

    // Encrypt with DPAPI (CRYPTPROTECT_LOCAL_MACHINE for machine scope, 0 for user scope)
    // Using user scope for better security isolation
    if (!CryptProtectData(&input, L"PEEK Trust Overrides", NULL, NULL, NULL,
                          CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        LOG_ERROR("DPAPI encryption failed: %lu", GetLastError());
        return FALSE;
    }

    *ciphertext = output.pbData;
    *ciphertext_len = output.cbData;
    return TRUE;
}

// Decrypt data using DPAPI
static BOOL dpapi_decrypt(const BYTE* ciphertext, DWORD ciphertext_len, BYTE** plaintext, DWORD* plaintext_len) {
    DATA_BLOB input;
    DATA_BLOB output;
    LPWSTR description = NULL;

    input.pbData = (BYTE*)ciphertext;
    input.cbData = ciphertext_len;

    if (!CryptUnprotectData(&input, &description, NULL, NULL, NULL,
                            CRYPTPROTECT_UI_FORBIDDEN, &output)) {
        LOG_ERROR("DPAPI decryption failed: %lu", GetLastError());
        return FALSE;
    }

    if (description) {
        LocalFree(description);
    }

    *plaintext = output.pbData;
    *plaintext_len = output.cbData;
    return TRUE;
}


// Set restrictive ACLs on trust overrides file (user only)
static void set_file_acl_user_only(const char* file_path) {
    // Get current user SID
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        LOG_WARNING("Failed to open process token for ACL: %lu", GetLastError());
        return;
    }

    DWORD dwSize = 0;
    GetTokenInformation(hToken, TokenUser, NULL, 0, &dwSize);
    PTOKEN_USER pTokenUser = (PTOKEN_USER)malloc(dwSize);
    
    if (!GetTokenInformation(hToken, TokenUser, pTokenUser, dwSize, &dwSize)) {
        LOG_WARNING("Failed to get token information: %lu", GetLastError());
        free(pTokenUser);
        CloseHandle(hToken);
        return;
    }

    PSID pSID = pTokenUser->User.Sid;
    CloseHandle(hToken);

    // Create ACL with only user access
    EXPLICIT_ACCESSA ea[1];
    ZeroMemory(&ea, sizeof(EXPLICIT_ACCESSA));
    ea[0].grfAccessPermissions = GENERIC_READ | GENERIC_WRITE;
    ea[0].grfAccessMode = SET_ACCESS;
    ea[0].grfInheritance = NO_INHERITANCE;
    ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea[0].Trustee.TrusteeType = TRUSTEE_IS_USER;
    ea[0].Trustee.ptstrName = (LPSTR)pSID;

    PACL pACL = NULL;
    DWORD dwRes = SetEntriesInAclA(1, ea, NULL, &pACL);
    
    if (dwRes != ERROR_SUCCESS) {
        LOG_WARNING("SetEntriesInAcl failed: %lu", dwRes);
        free(pTokenUser);
        return;
    }

    // Apply ACL to file
    dwRes = SetNamedSecurityInfoA(
        (LPSTR)file_path,
        SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
        NULL,
        NULL,
        pACL,
        NULL
    );

    if (dwRes != ERROR_SUCCESS) {
        LOG_WARNING("SetNamedSecurityInfo failed: %lu", dwRes);
    } else {
        LOG_SUCCESS("Applied restrictive ACLs to trust file (user only)");
    }

    LocalFree(pACL);
    free(pTokenUser);
}

void network_load_trust_overrides(void) {
    init_secure_file_path();

    FILE* f = fopen(TRUST_OVERRIDES_FILE, "rb");
    if (!f) {
        LOG_INFO("No trust overrides file found (first run)");
        return;
    }

    // Read entire file
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size < 12) {
        LOG_ERROR("Trust overrides file too small (corrupted)");
        fclose(f);
        return;
    }

    BYTE* file_data = (BYTE*)malloc(file_size);
    if (fread(file_data, 1, file_size, f) != file_size) {
        LOG_ERROR("Failed to read trust overrides file");
        free(file_data);
        fclose(f);
        return;
    }
    fclose(f);

    // Check magic + version
    DWORD magic = *(DWORD*)&file_data[0];
    DWORD version = *(DWORD*)&file_data[4];
    DWORD count = *(DWORD*)&file_data[8];

    if (magic != 0x4B454550) {
        LOG_ERROR("Invalid trust overrides file (bad magic) - renaming to .corrupt");
        char corrupt_path[MAX_PATH];
        snprintf(corrupt_path, MAX_PATH, "%s.corrupt.%lu", TRUST_OVERRIDES_FILE, (unsigned long)GetTickCount());
        MoveFileA(TRUST_OVERRIDES_FILE, corrupt_path);
        free(file_data);
        return;
    }

    if (count > MAX_TRUST_OVERRIDES) {
        LOG_ERROR("Invalid count in trust overrides file");
        free(file_data);
        return;
    }

    // DPAPI encrypted data starts at offset 12
    BYTE* encrypted_blob = &file_data[12];
    DWORD encrypted_size = file_size - 12;

    // Decrypt with DPAPI
    BYTE* decrypted_data = NULL;
    DWORD decrypted_size = 0;

    if (!dpapi_decrypt(encrypted_blob, encrypted_size, &decrypted_data, &decrypted_size)) {
        LOG_ERROR("DPAPI decryption failed - file may be corrupted or tampered");
        char corrupt_path[MAX_PATH];
        snprintf(corrupt_path, MAX_PATH, "%s.corrupt.%lu", TRUST_OVERRIDES_FILE, (unsigned long)GetTickCount());
        MoveFileA(TRUST_OVERRIDES_FILE, corrupt_path);
        free(file_data);
        return;
    }

    // Verify size matches
    size_t expected_size = count * sizeof(TrustOverride);
    if (decrypted_size != expected_size) {
        LOG_ERROR("Decrypted data size mismatch (expected %zu, got %lu)", expected_size, (unsigned long)decrypted_size);
        LocalFree(decrypted_data);
        free(file_data);
        return;
    }

    // Load into memory
    memcpy(trust_overrides, decrypted_data, decrypted_size);
    trust_overrides_count = count;

    LocalFree(decrypted_data);
    free(file_data);

    LOG_SUCCESS("Loaded %d trust override(s) (DPAPI verified)", trust_overrides_count);
}

void network_save_trust_override(const char* process_path, TrustStatus status) {
    if (!process_path || strlen(process_path) == 0) {
        return;
    }

    init_secure_file_path();

    // Update or add to in-memory array
    BOOL found = FALSE;
    for (int i = 0; i < trust_overrides_count; i++) {
        if (trust_overrides[i].valid && strcmp(trust_overrides[i].process_path, process_path) == 0) {
            if (status == TRUST_UNKNOWN) {
                trust_overrides[i].valid = FALSE;
            } else {
                trust_overrides[i].override_status = status;
            }
            found = TRUE;
            break;
        }
    }

    if (!found && status != TRUST_UNKNOWN && trust_overrides_count < MAX_TRUST_OVERRIDES) {
        TrustOverride* override = &trust_overrides[trust_overrides_count];
        strncpy(override->process_path, process_path, MAX_PATH - 1);
        override->process_path[MAX_PATH - 1] = '\0';
        override->override_status = status;
        override->valid = TRUE;
        trust_overrides_count++;
    }

    // Compact array (remove invalid)
    int write_pos = 0;
    for (int i = 0; i < trust_overrides_count; i++) {
        if (trust_overrides[i].valid) {
            if (write_pos != i) {
                trust_overrides[write_pos] = trust_overrides[i];
            }
            write_pos++;
        }
    }
    trust_overrides_count = write_pos;

    // Prepare data for encryption
    size_t data_size = trust_overrides_count * sizeof(TrustOverride);

    // Encrypt with DPAPI
    BYTE* encrypted_blob = NULL;
    DWORD encrypted_size = 0;

    if (!dpapi_encrypt((BYTE*)trust_overrides, data_size, &encrypted_blob, &encrypted_size)) {
        LOG_ERROR("DPAPI encryption failed");
        return;
    }

    // Atomic write: write to temp file first
    char temp_path[MAX_PATH];
    snprintf(temp_path, MAX_PATH, "%s.tmp", TRUST_OVERRIDES_FILE);

    FILE* f = fopen(temp_path, "wb");
    if (!f) {
        LOG_ERROR("Failed to create temp file for trust overrides");
        LocalFree(encrypted_blob);
        return;
    }

    // Write: [MAGIC:4][VERSION:4][COUNT:4][DPAPI_ENCRYPTED_DATA]
    DWORD magic = 0x4B454550;
    DWORD version = 2; // Version 2 = DPAPI
    DWORD count = trust_overrides_count;

    fwrite(&magic, sizeof(DWORD), 1, f);
    fwrite(&version, sizeof(DWORD), 1, f);
    fwrite(&count, sizeof(DWORD), 1, f);
    fwrite(encrypted_blob, 1, encrypted_size, f);

    fflush(f);
    fclose(f);

    LocalFree(encrypted_blob);

    // Atomic replace using Windows ReplaceFile
    if (!ReplaceFileA(TRUST_OVERRIDES_FILE, temp_path, NULL, 
                      REPLACEFILE_WRITE_THROUGH, NULL, NULL)) {
        // If target doesn't exist, just rename
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            if (!MoveFileA(temp_path, TRUST_OVERRIDES_FILE)) {
                LOG_ERROR("Failed to move temp file: %lu", GetLastError());
                DeleteFileA(temp_path);
                return;
            }
        } else {
            LOG_ERROR("Failed to replace trust overrides file: %lu", GetLastError());
            DeleteFileA(temp_path);
            return;
        }
    }

    // Apply restrictive ACLs
    set_file_acl_user_only(TRUST_OVERRIDES_FILE);

    LOG_SUCCESS("Saved trust override for: %s (DPAPI encrypted, atomic write, ACLs applied)", process_path);
}

TrustStatus network_get_trust_override(const char* process_path) {
    if (!process_path || strlen(process_path) == 0) {
        return TRUST_UNKNOWN;
    }

    for (int i = 0; i < trust_overrides_count; i++) {
        if (trust_overrides[i].valid && strcmp(trust_overrides[i].process_path, process_path) == 0) {
            return trust_overrides[i].override_status;
        }
    }

    return TRUST_UNKNOWN;
}

void network_apply_trust_override(const char* process_path, TrustStatus status) {
    if (!process_path || strlen(process_path) == 0) {
        return;
    }

    // Save to file
    network_save_trust_override(process_path, status);

    // Apply to all seen connections with this process path
    EnterCriticalSection(&seen_connections_cs);
    for (int i = 0; i < seen_count; i++) {
        if (strcmp(seen_connections[i].process_path, process_path) == 0) {
            if (status == TRUST_UNKNOWN) {
                // Reset to auto - recompute
                seen_connections[i].security_info_loaded = FALSE;
                network_compute_security_info_deferred(&seen_connections[i]);
            } else {
                seen_connections[i].trust_status = status;
            }
        }
    }
    LeaveCriticalSection(&seen_connections_cs);

    // Update cache
    EnterCriticalSection(&security_cache_cs);
    for (int i = 0; i < security_cache_count; i++) {
        if (security_cache[i].valid && strcmp(security_cache[i].process_path, process_path) == 0) {
            if (status == TRUST_UNKNOWN) {
                // Remove from cache to force recomputation
                security_cache[i].valid = FALSE;
            } else {
                security_cache[i].trust_status = status;
            }
        }
    }
    LeaveCriticalSection(&security_cache_cs);

    LOG_SUCCESS("Applied trust override to all connections: %s", process_path);
}
