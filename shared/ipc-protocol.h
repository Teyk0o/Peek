/*
 * IPC Protocol Definition
 * Communication between peek.exe (UI Host) and peekd.exe (Daemon)
 * Uses Windows Named Pipes for local IPC
 */

#ifndef PEEK_IPC_PROTOCOL_H
#define PEEK_IPC_PROTOCOL_H

#include <stdint.h>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Named Pipe name */
#define PEEK_IPC_PIPE_NAME L"\\\\.\\pipe\\Peek-IPC-v1"
#define PEEK_IPC_TIMEOUT_MS 5000
#define PEEK_IPC_BUFFER_SIZE 65536

/* Message Types */
typedef enum {
    MSG_REQUEST = 0x01,
    MSG_RESPONSE = 0x02,
    MSG_EVENT = 0x03,
} IPC_MessageType;

/* Request/Response Command IDs */
typedef enum {
    CMD_GET_CONNECTIONS = 0x0001,
    CMD_FILTER_UPDATE = 0x0002,
    CMD_TRUST_OVERRIDE = 0x0003,
    CMD_GET_SETTINGS = 0x0004,
    CMD_SET_SETTINGS = 0x0005,
    CMD_START_MONITOR = 0x0006,
    CMD_STOP_MONITOR = 0x0007,
    CMD_GET_DAEMON_STATUS = 0x0008,
    CMD_CHECK_UPDATE = 0x0009,
    CMD_APPLY_UPDATE = 0x000A,
    CMD_CLEAR_CONNECTIONS = 0x000B,
} IPC_CommandID;

/* Event Types (async notifications from daemon) */
typedef enum {
    EVT_CONNECTION_ADDED = 0x0100,
    EVT_CONNECTION_UPDATED = 0x0101,
    EVT_CONNECTION_REMOVED = 0x0102,
    EVT_TRUST_CHANGED = 0x0103,
    EVT_UPDATE_AVAILABLE = 0x0104,
    EVT_DAEMON_STATUS_CHANGED = 0x0105,
} IPC_EventType;

/* IPC Message Header (fixed-size) */
typedef struct {
    uint32_t magic;         /* Magic number: 0x50454B49 ('PEKI') */
    uint32_t version;       /* Protocol version: 1 */
    uint32_t msg_id;        /* Unique request ID (for correlation) */
    uint32_t msg_type;      /* IPC_MessageType */
    uint32_t cmd_id;        /* IPC_CommandID or IPC_EventType */
    uint32_t payload_size;  /* Size of payload (0-65508) */
    uint32_t flags;         /* Reserved for future use */
    uint32_t reserved;      /* Padding */
} IPC_MessageHeader;

/* IPC Message (header + payload) */
typedef struct {
    IPC_MessageHeader header;
    uint8_t payload[PEEK_IPC_BUFFER_SIZE - sizeof(IPC_MessageHeader)];
} IPC_Message;

#define IPC_MAGIC 0x50454B49

/* Response Status Codes */
typedef enum {
    STATUS_OK = 0x0000,
    STATUS_ERROR = 0x0001,
    STATUS_INVALID_CMD = 0x0002,
    STATUS_INVALID_PARAMS = 0x0003,
    STATUS_PERMISSION_DENIED = 0x0004,
    STATUS_DAEMON_NOT_READY = 0x0005
} IPC_StatusCode;

/* Payload Structures (JSON serialized in most cases) */
/* CMD_GET_CONNECTIONS response: JSON array of connections */
/* CMD_GET_SETTINGS response: JSON object with settings */
/* CMD_SET_SETTINGS request: JSON object with new settings */
/* CMD_TRUST_OVERRIDE request/response: JSON with process path + trust level */

#ifdef __cplusplus
}
#endif

#endif /* PEEK_IPC_PROTOCOL_H */
