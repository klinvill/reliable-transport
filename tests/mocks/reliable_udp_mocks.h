//
// Created by Kirby Linvill on 1/29/21.
//

#ifndef UDP_RELIABLE_UDP_MOCKS_H
#define UDP_RELIABLE_UDP_MOCKS_H

#include "../../src/common/reliable_udp/types.h"


int rudp_send(char* data, int data_size, SocketInfo* to, RudpSender* sender);
int rudp_recv(char* buffer, int buffer_size, SocketInfo* from, RudpReceiver* receiver);

// helper functions to wrap expected cmocka arguments
void check_rudp_send(char* expected_data, size_t expected_data_size, ssize_t ret_code);

#endif //UDP_RELIABLE_UDP_MOCKS_H
