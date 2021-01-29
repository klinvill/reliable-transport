//
// Created by Kirby Linvill on 1/29/21.
//

#include "reliable_udp_mocks.h"

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>


void check_rudp_send(char* expected_data, size_t expected_data_size, ssize_t ret_code) {
    expect_value(rudp_send, data_size, expected_data_size);
    expect_memory(rudp_send, data, expected_data, expected_data_size);

    will_return(rudp_send, ret_code);
}

int rudp_send(char* data, int data_size, SocketInfo* to, RudpSender* sender) {
    check_expected(data_size);
    check_expected(data);

    return mock_type(int);
}
