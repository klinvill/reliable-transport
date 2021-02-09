//
// Common types and definitions for RUDP (Reliable UDP)
//

#ifndef UDP_TYPES_H
#define UDP_TYPES_H

#include <sys/socket.h>


// Errors
// TODO: should not clash with other potential return values
#define PAYLOAD_TOO_LARGE_ERROR (-2)
#define SENDER_TIMEOUT_ERROR (-3)

// size of RudpHeader in bytes
#define HEADER_SIZE 12


// Holds information about the socket to send/receive data to/from
typedef struct {
    int sockfd;
    struct sockaddr* addr;
    socklen_t addr_len;
} SocketInfo;

typedef struct {
    int seq_num;
    int ack_num;
    int data_size; // size of data in bytes
} RudpHeader;

typedef struct {
    RudpHeader header;
    char* data;
} RudpMessage;

// Information needed when sending a RUDP message
typedef struct {
    int last_ack;           // last received ack
    int message_timeout;    // in milliseconds, timeout until a message should be resent
    int sender_timeout;     // in milliseconds, timeout until a sender should abort trying to send a message
} RudpSender;

// Information needed when receiving a RUDP message
typedef struct {
    int last_received;  // last ack'd seq number
} RudpReceiver;

#endif //UDP_TYPES_H
