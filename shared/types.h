/*
 * Shared Type Definitions
 * Used across daemon, UI host, and frontend
 */

#ifndef PEEK_SHARED_TYPES_H
#define PEEK_SHARED_TYPES_H

#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Connection Direction */
typedef enum {
    DIR_INBOUND = 0,
    DIR_OUTBOUND = 1,
    DIR_UNKNOWN = 2,
} ConnectionDirection;

/* Protocol Type */
typedef enum {
    PROTO_TCP = 0,
    PROTO_UDP = 1,
    PROTO_UNKNOWN = 2,
} ProtocolType;

/* Trust Status Level (8 levels) */
typedef enum {
    TRUST_MICROSOFT_SIGNED = 0,      /* Microsoft/Windows signed */
    TRUST_VERIFIED = 1,               /* Verified publisher */
    TRUST_MANUALLY_TRUSTED = 2,       /* User manually trusted */
    TRUST_UNSIGNED = 3,               /* No signature */
    TRUST_INVALID_SIGNATURE = 4,      /* Invalid/expired signature */
    TRUST_MANUALLY_THREAT = 5,        /* User marked as threat */
    TRUST_VERIFICATION_ERROR = 6,     /* Verification error */
    TRUST_UNKNOWN = 7,                /* Unknown/verifying */
} TrustStatus;

/* IP Address Family */
typedef enum {
    ADDR_FAMILY_UNKNOWN = 0,
    ADDR_FAMILY_INET = 4,      /* IPv4 */
    ADDR_FAMILY_INET6 = 6,     /* IPv6 */
} AddressFamily;

/* Connection State */
typedef enum {
    CONN_STATE_ESTABLISHED = 1,
    CONN_STATE_LISTEN = 2,
    CONN_STATE_SYN_SENT = 3,
    CONN_STATE_SYN_RECEIVED = 4,
    CONN_STATE_FIN_WAIT_1 = 5,
    CONN_STATE_FIN_WAIT_2 = 6,
    CONN_STATE_CLOSE_WAIT = 7,
    CONN_STATE_CLOSING = 8,
    CONN_STATE_TIME_WAIT = 9,
    CONN_STATE_CLOSED = 10,
} ConnectionState;

/* IPv4/IPv6 Address */
typedef union {
    uint32_t ipv4;              /* IPv4 address (network byte order) */
    uint8_t ipv6[16];           /* IPv6 address (128-bit) */
} IPAddress;

/* Network Connection Information */
typedef struct {
    uint32_t connection_id;     /* Unique ID for this session */
    uint32_t process_id;        /* Windows PID */
    char process_name[260];     /* Executable name */
    char process_path[520];     /* Full path to executable */

    ProtocolType protocol;      /* TCP or UDP */
    ConnectionDirection direction; /* Inbound, Outbound, Unknown */
    AddressFamily address_family; /* IPv4 or IPv6 */

    IPAddress local_address;    /* Local IP address */
    uint16_t local_port;        /* Local port */
    IPAddress remote_address;   /* Remote IP address */
    uint16_t remote_port;       /* Remote port */

    ConnectionState state;      /* TCP connection state (if TCP) */

    TrustStatus trust_status;   /* Binary trust verification status */
    uint8_t trust_override;     /* 1 if user manually overridden */
    char binary_hash[65];       /* SHA256 hex (64 chars + null) */

    uint64_t bytes_sent;        /* Total bytes sent */
    uint64_t bytes_received;    /* Total bytes received */

    time_t created_at;          /* Connection creation timestamp */
    time_t last_seen;           /* Last activity timestamp */
    uint32_t packet_count;      /* Total packets */

    uint32_t flash_counter;     /* Animation counter */
} Connection;

/* Settings Structure */
typedef struct {
    uint32_t polling_interval_ms;   /* Milliseconds between network polls */
    uint8_t show_localhost;         /* 1 = show 127.0.0.1 and [::1] */
    uint8_t enable_auto_update;     /* 1 = check for updates */
    uint8_t start_with_windows;     /* 1 = add to startup */
    uint8_t run_as_service;         /* 1 = run daemon as Windows service */
    uint32_t reserved[4];           /* Reserved for future settings */
} Settings;

/* Daemon Status */
typedef struct {
    uint8_t is_running;             /* 1 = daemon is active */
    uint32_t uptime_seconds;        /* Daemon uptime */
    uint32_t connection_count;      /* Current monitored connections */
    uint64_t total_bytes_sent;      /* Total bytes sent (all connections) */
    uint64_t total_bytes_received;  /* Total bytes received (all connections) */
    uint32_t memory_usage_mb;       /* Approximate memory usage */
    char version[32];               /* Daemon version string */
} DaemonStatus;

#ifdef __cplusplus
}
#endif

#endif /* PEEK_SHARED_TYPES_H */
