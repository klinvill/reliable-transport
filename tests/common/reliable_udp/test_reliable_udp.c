//
// Created by Kirby Linvill on 1/27/21.
//

#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

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

static void test_rudp_send_large_message(void** state) {
    char buffer[MAX_DATA_SIZE+1] = {0,};
    int buffer_len = MAX_DATA_SIZE+1;
    char chunk_1_value = 0x41;
    char chunk_2_value = 0x42;
    memset(buffer, chunk_1_value, buffer_len-1);
    buffer[buffer_len-1] = chunk_2_value;

    struct sockaddr_in addr = {.sin_port=8080, .sin_addr=0x7F000001, .sin_family=AF_INET};
    SocketInfo socket_info = {.addr=(struct sockaddr*) &addr, .addr_len=sizeof(addr), .sockfd=999};
    RudpSender sender = {.last_ack=0, .message_timeout=INITIAL_TIMEOUT, .sender_timeout=SENDER_TIMEOUT};

    // mocked responses
    will_return_count(poll, POLL_READY, 2);

    // mocked recvfrom response
    RudpHeader received_headers[2] = {
            {.ack_num=1},
            {.ack_num=2},
    };
    char* received_buffers[2] = {
            (char[MAX_PAYLOAD_SIZE]) {0,},
            (char[MAX_PAYLOAD_SIZE]) {0,},
    };
    for (int i = 0; i < 2; i++) {
        int serialized = serialize_header(&received_headers[i], received_buffers[i], MAX_PAYLOAD_SIZE);
        set_recvfrom_buffer(received_buffers[i], serialized, RECVFROM_SUCCESS);
    }

    RudpMessage expected_sent_messages[2] = {
            {.header= (RudpHeader) {.seq_num=1, .ack_num=0, .data_size=MAX_DATA_SIZE}, .data=buffer},
            {.header= (RudpHeader) {.seq_num=2, .ack_num=0, .data_size=buffer_len-MAX_DATA_SIZE}, .data=(&buffer[MAX_DATA_SIZE])},
    };
    // sanity check
    assert_memory_equal(&buffer[MAX_DATA_SIZE-1], &chunk_1_value, 1);
    assert_memory_equal(&buffer[MAX_DATA_SIZE], &chunk_2_value, 1);
    char* expected_sent_buffers[2] = {
            (char[MAX_PAYLOAD_SIZE]) {0,},
            (char[MAX_PAYLOAD_SIZE]) {0,},
    };
    for (int i = 0; i < 2; i++) {
        int serialized = serialize(&expected_sent_messages[i], expected_sent_buffers[i], MAX_PAYLOAD_SIZE);
        check_sendto(expected_sent_buffers[i], serialized, SENDTO_SUCCESS);
    }

    int result = rudp_send(buffer, buffer_len, &socket_info, &sender);
    assert_int_equal(result, 0);
    assert_int_equal(sender.last_ack, 2);
}

static void test_rudp_send_MAX_DATA_SIZE_message(void** state) {
    char buffer[MAX_DATA_SIZE] = {0,};
    int buffer_len = MAX_DATA_SIZE;
    memset(buffer, 0x41, buffer_len-1);

    struct sockaddr_in addr = {.sin_port=8080, .sin_addr=0x7F000001, .sin_family=AF_INET};
    SocketInfo socket_info = {.addr=(struct sockaddr*) &addr, .addr_len=sizeof(addr), .sockfd=999};
    RudpSender sender = {.last_ack=0, .message_timeout=INITIAL_TIMEOUT, .sender_timeout=SENDER_TIMEOUT};

    // mocked responses
    set_poll_rc(POLL_READY);

    // mocked recvfrom response
    RudpHeader recvfrom_header = {.ack_num=1};
    char recvfrom_buffer[MAX_PAYLOAD_SIZE] = {0,};
    int serialized = serialize_header(&recvfrom_header, recvfrom_buffer, MAX_PAYLOAD_SIZE);
    set_recvfrom_buffer(recvfrom_buffer, serialized, RECVFROM_SUCCESS);

    RudpMessage expected_sent_message = {.header= (RudpHeader) {.seq_num=1, .ack_num=0, .data_size=MAX_DATA_SIZE},
                                         .data=buffer};
    char expected_sent_buffer[MAX_PAYLOAD_SIZE] = {0,};
    serialized = serialize(&expected_sent_message, expected_sent_buffer, MAX_PAYLOAD_SIZE);
    check_sendto(expected_sent_buffer, serialized, SENDTO_SUCCESS);

    int result = rudp_send(buffer, buffer_len, &socket_info, &sender);
    assert_int_equal(result, 0);
    assert_int_equal(sender.last_ack, 1);
}

// TODO: do we really need a timeout for the sender? (for this assignment)
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

static void test_rudp_recv_acks_on_receipt(void** state) {
    char buffer[100] = {0,};
    int buffer_len = 100;
    struct sockaddr_in addr = {.sin_port=8080, .sin_addr=0x7F000001, .sin_family=AF_INET};
    SocketInfo socket_info = {.addr=(struct sockaddr*) &addr, .addr_len=sizeof(addr), .sockfd=999};
    RudpReceiver receiver = {.last_received=0};

    // mocked recvfrom message
    RudpHeader recvfrom_header = {.seq_num=1};
    char recvfrom_buffer[100] = {0,};
    serialize_header(&recvfrom_header, recvfrom_buffer, buffer_len);
    set_recvfrom_buffer(recvfrom_buffer, buffer_len, RECVFROM_SUCCESS);

    RudpHeader expected_sent_header = {.seq_num=0, .ack_num=1, .data_size=0};
    char expected_sent_buffer[100] = {0,};
    int serialized = serialize_header(&expected_sent_header, expected_sent_buffer, buffer_len);
    // mocks sendto, but also checks that the buffer sendto received is equal to expected_sent_buffer
    check_sendto(expected_sent_buffer, serialized, SENDTO_SUCCESS);

    int result = rudp_recv(buffer, buffer_len, &socket_info, &receiver);

    assert_int_equal(result, 0);
    assert_int_equal(receiver.last_received, 1);
}

static void test_rudp_recv_acks_previous_requests(void** state) {
    char buffer[100] = {0,};
    int buffer_len = 100;
    struct sockaddr_in addr = {.sin_port=8080, .sin_addr=0x7F000001, .sin_family=AF_INET};
    SocketInfo socket_info = {.addr=(struct sockaddr*) &addr, .addr_len=sizeof(addr), .sockfd=999};
    RudpReceiver receiver = {.last_received=1};

    // mocked recvfrom messages
    //
    // the receiver will keep listening until they receive the next packet they're looking for, so to avoid a timeout
    // we send the next sequence they're expecting
    RudpHeader received_headers[2] = {
            {.seq_num=1},
            {.seq_num=2},
    };
    char* received_buffers[2] = {
            (char[100]) {0,},
            (char[100]) {0,},
    };
    for (int i = 0; i < 2; i++) {
        int serialized = serialize_header(&received_headers[i], received_buffers[i], buffer_len);
        set_recvfrom_buffer(received_buffers[i], serialized, RECVFROM_SUCCESS);
    }

    RudpHeader expected_sent_headers[2] = {
            {.seq_num=0, .ack_num=1, .data_size=0},
            {.seq_num=0, .ack_num=2, .data_size=0},
    };
    char* expected_sent_buffers[2] = {
            (char[100]) {0,},
            (char[100]) {0,},
    };
    for (int i = 0; i < 2; i++) {
        int serialized = serialize_header(&expected_sent_headers[i], expected_sent_buffers[i], buffer_len);
        check_sendto(expected_sent_buffers[i], serialized, SENDTO_SUCCESS);
    }

    int result = rudp_recv(buffer, buffer_len, &socket_info, &receiver);

    assert_int_equal(result, 0);
    assert_int_equal(receiver.last_received, 2);
}

static void test_rudp_recv_does_not_ack_future_requests(void** state) {
    char buffer[100] = {0,};
    int buffer_len = 100;
    struct sockaddr_in addr = {.sin_port=8080, .sin_addr=0x7F000001, .sin_family=AF_INET};
    SocketInfo socket_info = {.addr=(struct sockaddr*) &addr, .addr_len=sizeof(addr), .sockfd=999};
    RudpReceiver receiver = {.last_received=0};

    // mocked recvfrom messages
    //
    // the receiver will keep listening until they receive the next packet they're looking for, so to avoid a timeout
    // we send the next sequence they're expecting
    RudpHeader received_headers[2] = {
            {.seq_num=2},
            {.seq_num=1},
    };
    char* received_buffers[2] = {
            (char[100]) {0,},
            (char[100]) {0,},
    };
    for (int i = 0; i < 2; i++) {
        int serialized = serialize_header(&received_headers[i], received_buffers[i], buffer_len);
        set_recvfrom_buffer(received_buffers[i], serialized, RECVFROM_SUCCESS);
    }

    RudpHeader expected_sent_header ={.seq_num=0, .ack_num=1, .data_size=0};
    char expected_sent_buffer[100] = {0,};
    int serialized = serialize_header(&expected_sent_header, expected_sent_buffer, buffer_len);
    check_sendto(expected_sent_buffer, serialized, SENDTO_SUCCESS);

    int result = rudp_recv(buffer, buffer_len, &socket_info, &receiver);

    assert_int_equal(result, 0);
    assert_int_equal(receiver.last_received, 1);
}


int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_rudp_send_succeeds_with_ack),
            cmocka_unit_test(test_rudp_send_succeeds_despite_message_loss),
            cmocka_unit_test(test_rudp_send_large_message),
            cmocka_unit_test(test_rudp_send_MAX_DATA_SIZE_message),
            cmocka_unit_test(test_rudp_send_eventually_times_out),
            cmocka_unit_test(test_rudp_recv_acks_on_receipt),
            cmocka_unit_test(test_rudp_recv_acks_previous_requests),
            cmocka_unit_test(test_rudp_recv_does_not_ack_future_requests),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}
