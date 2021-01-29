//
// Created by Kirby Linvill on 1/27/21.
//

#include <sys/socket.h>
#include <netinet/in.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "../../mocks/mocks.h"
#include "../../../src/common/reliable_udp/reliable_udp.h"
#include "../../../src/common/reliable_udp/serde.h"


#define SENDTO_SUCCESS 1
#define POLL_READY 1
#define POLL_NOT_READY 0
#define RECVFROM_SUCCESS 1


static void test_rudp_send_succeeds_with_ack(void** state) {
    char buffer[100] = {0,};
    int buffer_len = 100;
    struct sockaddr_in addr = {.sin_port=8080, .sin_addr=0x7F000001, .sin_family=AF_INET};
    SocketInfo socket_info = {.addr=(struct sockaddr*) &addr, .addr_len=sizeof(addr), .sockfd=999};
    RudpSender sender = {.last_ack=0, .message_timeout=INITIAL_TIMEOUT, .sender_timeout=SENDER_TIMEOUT};

    // mocked responses
    set_sendto_rc(SENDTO_SUCCESS);
    set_poll_rc(POLL_READY);

    // mocked recvfrom response
    RudpHeader recvfrom_header = {.ack_num=1};
    char recvfrom_buffer[100] = {0,};
    serialize_header(&recvfrom_header, recvfrom_buffer, buffer_len);
    set_recvfrom_buffer(recvfrom_buffer, buffer_len, RECVFROM_SUCCESS);

    int result = rudp_send(buffer, buffer_len, &socket_info, &sender);
    assert_int_equal(result, 0);
    assert_int_equal(sender.last_ack, 1);
}

static void test_rudp_send_succeeds_despite_message_loss(void** state) {
    char buffer[100] = {0,};
    int buffer_len = 100;
    struct sockaddr_in addr = {.sin_port=8080, .sin_addr=0x7F000001, .sin_family=AF_INET};
    SocketInfo socket_info = {.addr=(struct sockaddr*) &addr, .addr_len=sizeof(addr), .sockfd=999};
    RudpSender sender = {.last_ack=0, .message_timeout=INITIAL_TIMEOUT, .sender_timeout=SENDER_TIMEOUT};

    set_sendto_rc_count(SENDTO_SUCCESS, 4);
    will_return_count(poll, POLL_NOT_READY, 3);
    will_return(poll, POLL_READY);

    // mocked recvfrom response
    RudpHeader recvfrom_header = {.ack_num=1};
    char recvfrom_buffer[100] = {0,};
    serialize_header(&recvfrom_header, recvfrom_buffer, buffer_len);
    set_recvfrom_buffer(recvfrom_buffer, buffer_len, RECVFROM_SUCCESS);

    int result = rudp_send(buffer, 100, &socket_info, &sender);
    assert_int_equal(result, 0);
    assert_int_equal(sender.last_ack, 1);
}

static void test_rudp_send_eventually_times_out(void** state) {
    char buffer[100] = {0,};
    struct sockaddr_in addr = {.sin_port=8080, .sin_addr=0x7F000001, .sin_family=AF_INET};
    SocketInfo socket_info = {.addr=(struct sockaddr*) &addr, .addr_len=sizeof(addr), .sockfd=999};
    RudpSender sender = {.last_ack=0, .message_timeout=INITIAL_TIMEOUT, .sender_timeout=SENDER_TIMEOUT};

    will_return_always(sendto, 0);
    will_return_always(poll, POLL_NOT_READY);

    int result = rudp_send(buffer, 100, &socket_info, &sender);
    assert_int_equal(result, SENDER_TIMEOUT_ERROR);
    assert_int_equal(sender.last_ack, 0);
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_rudp_send_succeeds_with_ack),
            cmocka_unit_test(test_rudp_send_succeeds_despite_message_loss),
            cmocka_unit_test(test_rudp_send_eventually_times_out),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
