//
// Created by Kirby Linvill on 1/26/21.
//

#ifndef UDP_RELIABLE_UDP_H
#define UDP_RELIABLE_UDP_H

#include "types.h"


#define MAX_PAYLOAD_SIZE 1024
#define MAX_DATA_SIZE (MAX_PAYLOAD_SIZE - HEADER_SIZE)

#define EMPTY_ACK_NUM 0
#define INITIAL_TIMEOUT 200     // in milliseconds, timeout until a message will be resent
#define SENDER_TIMEOUT 5000     // in milliseconds, timeout until a message is considered impossible to deliver

// if a receiver sees a message with a sequence number <= it's last received sequence number, it will still send an
// ack if the difference is within the ack window
#define ACK_WINDOW 100


// sends data as a single UDP message
int rudp_send(char* data, int data_size, SocketInfo* to, RudpSender* sender, RudpReceiver* receiver);
int rudp_recv(char* buffer, int buffer_size, SocketInfo* from, RudpReceiver* receiver);

// listens a little longer for messages and sends acks if applicable, will discard other messages
int rudp_check_acks(char* buffer, int buffer_size, SocketInfo* from, RudpReceiver* receiver);

#endif //UDP_RELIABLE_UDP_H
