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
#include <wincrypt.h>
#include <wintrust.h>
#include <softpub.h>
#include <shlobj.h>  // For SHGetFolderPath
#include <aclapi.h>  // For ACL management

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "wintrust.lib")

#define MAX_CONNECTIONS 2000
#define MAX_PROCESS_NAME 260
#define SHA256_HASH_LENGTH 65  // 64 hex chars + null terminator

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

typedef enum {
    TRUST_UNKNOWN = 0,            // Not yet verified (gray)
    TRUST_MICROSOFT_SIGNED,       // Signed by Microsoft/Windows (dark green)
    TRUST_VERIFIED_SIGNED,        // Signed by verified publisher (green)
    TRUST_MANUAL_TRUSTED,         // Manually marked as trusted (yellow-green)
    TRUST_UNSIGNED,               // Not signed (orange)
    TRUST_INVALID,                // Invalid/expired signature (red)
    TRUST_MANUAL_THREAT,          // Manually marked as threat (dark red)
    TRUST_ERROR                   // Error during verification (gray-red)
} TrustStatus;

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
    char process_path[MAX_PATH];  // Full path to executable
    char sha256_hash[SHA256_HASH_LENGTH];  // SHA256 hash of binary
    TrustStatus trust_status;     // Digital signature verification status
    BOOL security_info_loaded;    // Whether security info has been computed (for lazy loading)
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

// Security & Integrity functions
BOOL network_calculate_sha256(const char* file_path, char* hash_output, size_t hash_size);

TrustStatus network_verify_signature(const char* file_path);

void network_get_process_security_info(DWORD pid, char* path_buffer, size_t path_size,
                                        char* hash_buffer, size_t hash_size,
                                        TrustStatus* trust_status);

// Lazy version: only gets path, defers hash/trust computation
void network_get_process_path_only(DWORD pid, char* path_buffer, size_t path_size);

// Compute security info for an existing connection (can be called later)
void network_compute_security_info_deferred(NetworkConnection* conn);

// Batch compute security info for multiple connections in parallel (thread-safe)
void network_compute_security_batch_parallel(NetworkConnection* connections, int count);

// Compute security info for all seen connections in parallel, updating in place
void network_compute_security_for_all_seen(void);

// Find a connection in seen_connections by PID, remote addr and ports
// Returns direct pointer to the connection in seen_connections (not a copy)
NetworkConnection* network_find_connection(DWORD pid, DWORD remote_addr, DWORD remote_port, DWORD local_port);

// Manual trust override management
void network_load_trust_overrides(void);
void network_save_trust_override(const char* process_path, TrustStatus status);
TrustStatus network_get_trust_override(const char* process_path);
void network_apply_trust_override(const char* process_path, TrustStatus status);

#endif