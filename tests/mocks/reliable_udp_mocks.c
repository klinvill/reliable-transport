//
// Created by Kirby Linvill on 1/29/21.
//

#include "reliable_udp_mocks.h"

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <string.h>


void check_rudp_send(char* expected_data, size_t expected_data_size, ssize_t ret_code) {
    expect_value(rudp_send, data_size, expected_data_size);
    expect_memory(rudp_send, data, expected_data, expected_data_size);

    will_return(rudp_send, ret_code);
}

void set_rudp_recv_buffer(char* buffer, size_t buff_size, size_t ret_val) {
    will_return(rudp_recv, buff_size);
    will_return(rudp_recv, buffer);

    will_return(rudp_recv, ret_val);
}

int rudp_send(char* data, int data_size, SocketInfo* to, RudpSender* sender, RudpReceiver* receiver) {
    check_expected(data_size);
    check_expected(data);

    return mock_type(int);
}

int rudp_recv(char* buffer, int buffer_size, SocketInfo* from, RudpReceiver* receiver) {
    size_t in_buffer_len = mock_type(int);
    char *in_buffer = mock_type(char*);
    if (in_buffer_len > buffer_size)
        return -1;

    memcpy(buffer, in_buffer, in_buffer_len);

    return mock_type(size_t);
}
