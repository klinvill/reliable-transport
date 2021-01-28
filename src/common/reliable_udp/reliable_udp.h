//
// Created by Kirby Linvill on 1/26/21.
//

#ifndef UDP_RELIABLE_UDP_H
#define UDP_RELIABLE_UDP_H

#include "types.h"


#define MAX_PAYLOAD_SIZE 1024
#define EMPTY_ACK_NUM 0
#define INITIAL_TIMEOUT 200     // in milliseconds, timeout until a message will be resent
#define SENDER_TIMEOUT 5000     // in milliseconds, timeout until a message is considered impossible to deliver


// sends data as a single UDP message
int rudp_send(char* data, int data_size, SocketInfo* to, RudpSender* sender);
void rudp_recv(char* buffer, SocketInfo* from);

// Special cases that will send or receive the contents of a file in a streaming manner
// Reads the contents of the file specified by read_fd, and then sends it to the socket specified by to
void rudp_send_file(int* read_fd, SocketInfo* to);
// Receives the contents of a file from the socket specified by from, and writes it to the file specified by write_fd
void rudp_recv_file(int* write_fd, SocketInfo* from);

#endif //UDP_RELIABLE_UDP_H
