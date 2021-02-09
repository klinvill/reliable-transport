//
// RUDP (Reliable UDP) interface
//
// RUDP provides reliable, in-order delivery on top of UDP
//

#ifndef UDP_RELIABLE_UDP_H
#define UDP_RELIABLE_UDP_H

#include "types.h"


// max size of an RUDP message
#define MAX_PAYLOAD_SIZE 1024

// max size of the data part of an RUDP message
#define MAX_DATA_SIZE (MAX_PAYLOAD_SIZE - HEADER_SIZE)

// TODO: instead of treating 0 as an empty ACK or SEQ value, should include a flag to specify if a message is an ACK
//  or SEQ
#define EMPTY_ACK_NUM 0
#define INITIAL_TIMEOUT 200     // in milliseconds, timeout until a message will be resent
#define SENDER_TIMEOUT 5000     // in milliseconds, timeout until a message is considered impossible to deliver

// if a receiver sees a message with a sequence number <= its last received sequence number, it will still send an
// ack if the difference is within the ack window
#define ACK_WINDOW 100


// Sends data as a (reliable) UDP message
//
// Returns a 0 on success, and a negative int on failure
int rudp_send(char* data, int data_size, SocketInfo* to, RudpSender* sender, RudpReceiver* receiver);

// Receives a single (reliable) UDP message
//
// Returns the number of received bytes (of data) on success, and a negative int on failure
int rudp_recv(char* buffer, int buffer_size, SocketInfo* from, RudpReceiver* receiver);

// Listens a little longer for messages and sends acks if applicable, will discard other messages
//
// Returns the number of messages that were ack'd on success, or a negative int on failure
int rudp_check_acks(char* buffer, int buffer_size, SocketInfo* from, RudpReceiver* receiver);

#endif //UDP_RELIABLE_UDP_H
