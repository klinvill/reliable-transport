//
// Created by Kirby Linvill on 1/27/21.
//

#include <check.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "../../mocks/mocks.h"
#include "../../../src/common/reliable_udp/reliable_udp.h"
#include "../../../src/common/reliable_udp/serde.h"

ssize_t sendto_succeed() {
    return 1;
}

int poll_ready() {
    return 1;
}

int poll_not_ready() {
    return 0;
}

int poll_counter = 0;
int poll_delayed_ready() {
    if (poll_counter > 3)
        return 1;

    poll_counter++;
    return 0;
}

ssize_t recvfrom_succeed(char* buffer) {
    RudpHeader header = {.ack_num=1, .data_size=0, .seq_num=0};
    return serialize_header(&header, buffer, sizeof(header));
}


START_TEST(test_rudp_send_succeeds_with_ack) {
    char buffer[100] = {0,};
    struct sockaddr_in addr = {.sin_port=8080, .sin_addr=0x7F000001, .sin_family=AF_INET};
    SocketInfo socket_info = {.addr=(struct sockaddr*) &addr, .addr_len=sizeof(addr), .sockfd=999};
    RudpSender sender = {.last_ack=0, .message_timeout=INITIAL_TIMEOUT, .sender_timeout=SENDER_TIMEOUT};

    set_sendto_fn(sendto_succeed);
    set_poll_fn(poll_ready);
    set_recvfrom_fn(recvfrom_succeed);

    int result = rudp_send(buffer, 100, &socket_info, &sender);
    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(sender.last_ack, 1);
}
END_TEST

START_TEST(test_rudp_send_succeeds_despite_message_loss) {
    char buffer[100] = {0,};
    struct sockaddr_in addr = {.sin_port=8080, .sin_addr=0x7F000001, .sin_family=AF_INET};
    SocketInfo socket_info = {.addr=(struct sockaddr*) &addr, .addr_len=sizeof(addr), .sockfd=999};
    RudpSender sender = {.last_ack=0, .message_timeout=INITIAL_TIMEOUT, .sender_timeout=SENDER_TIMEOUT};

    set_sendto_fn(sendto_succeed);
    poll_counter = 0;
    set_poll_fn(poll_delayed_ready);
    set_recvfrom_fn(recvfrom_succeed);

    int result = rudp_send(buffer, 100, &socket_info, &sender);
    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(sender.last_ack, 1);
}
END_TEST

START_TEST(test_rudp_send_eventually_times_out) {
    char buffer[100] = {0,};
    struct sockaddr_in addr = {.sin_port=8080, .sin_addr=0x7F000001, .sin_family=AF_INET};
    SocketInfo socket_info = {.addr=(struct sockaddr*) &addr, .addr_len=sizeof(addr), .sockfd=999};
    RudpSender sender = {.last_ack=0, .message_timeout=INITIAL_TIMEOUT, .sender_timeout=SENDER_TIMEOUT};

    set_sendto_fn(sendto_succeed);
    set_poll_fn(poll_not_ready);
    set_recvfrom_fn(recvfrom_succeed);

    int result = rudp_send(buffer, 100, &socket_info, &sender);
    ck_assert_int_eq(result, SENDER_TIMEOUT_ERROR);
    ck_assert_int_eq(sender.last_ack, 0);
}
END_TEST


Suite* reliable_udp_suite(void) {
    Suite *s;
    TCase *tc_core;
    TCase *tc_delayed;
    s = suite_create("Reliable UDP");
    tc_core = tcase_create("Core");
    tc_delayed = tcase_create("Delayed Timeout");
    tcase_set_timeout(tc_delayed, 15);

    tcase_add_test(tc_core, test_rudp_send_succeeds_with_ack);
    tcase_add_test(tc_core, test_rudp_send_succeeds_despite_message_loss);

    tcase_add_test(tc_delayed, test_rudp_send_eventually_times_out);

    suite_add_tcase(s, tc_core);
    suite_add_tcase(s, tc_delayed);

    return s;
}

int main(void) {
    int num_failed = 0;
    Suite *s;
    SRunner *sr;

    s = reliable_udp_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    num_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return num_failed;
}
