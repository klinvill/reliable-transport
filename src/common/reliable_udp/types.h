//
// Created by Kirby Linvill on 1/27/21.
//

#ifndef UDP_TYPES_H
#define UDP_TYPES_H

#include <sys/socket.h>


#define PAYLOAD_TOO_LARGE_ERROR -2
#define SENDER_TIMEOUT_ERROR -3


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

typedef struct {
    int last_ack;
    int message_timeout;
    int sender_timeout;
} RudpSender;

#endif //UDP_TYPES_H
