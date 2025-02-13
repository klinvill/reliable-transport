//
// Created by Kirby Linvill on 1/29/21.
//

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../../mocks/mocks.h"
#include "../../mocks/reliable_udp_mocks.h"
#include "../../../src/common/utils.h"
#include "../../../src/common/kftp/kftp.h"
#include "../../../src/common/kftp/kftp_serde.h"
#include "../../../src/common/reliable_udp/reliable_udp.h"


#define FSEEK_SUCCESS 0
#define RUDP_SEND_SUCCESS 0
#define FEOF_EOF 0
#define FEOF_NOT_EOF 1


char* create_random_buffer(size_t size) {
    char* buffer = malloc(size);

    for (int i = 0; i < size; i++) {
        buffer[i] = (char) rand();
    }

    return buffer;
}

void destroy_random_buffer(char* buffer) {
    free(buffer);
}


static void test_kftp_send_small_file(void** state) {
    SocketInfo socket_info = {};
    RudpSender sender = {};
    RudpReceiver receiver = {};

    int dummy_filesize = 10;
    char* dummy_file_contents = create_random_buffer(dummy_filesize);

    // mocks
    will_return_always(fseek, FSEEK_SUCCESS);
    will_return(ftell, dummy_filesize);
    // should hit EOF on first read
    will_return(feof, FEOF_EOF);
    set_fread_buffer(dummy_file_contents, dummy_filesize, dummy_filesize);

    char expected_data[100] = {};
    int buffer_len = 100;
    KftpHeader header = {.data_size=dummy_filesize};
    int serialized = serialize_kftp_header(&header , expected_data, buffer_len);
    int expected_data_size = dummy_filesize + serialized;
    assert(expected_data_size < buffer_len);
    memcpy(&expected_data[serialized], dummy_file_contents, dummy_filesize);
    check_rudp_send(expected_data, expected_data_size, RUDP_SEND_SUCCESS);

    int result = kftp_send_file(NULL, &socket_info, &sender, &receiver);

    assert_int_equal(result, 0);

    destroy_random_buffer(dummy_file_contents);
}

static void test_kftp_send_file_over_two_rudp_messages(void** state) {
    SocketInfo socket_info = {};
    RudpSender sender = {};
    RudpReceiver receiver = {};

    int dummy_filesize = MAX_DATA_SIZE;
    char* dummy_file_contents = create_random_buffer(dummy_filesize);

    int first_msg_data_size = MAX_DATA_SIZE - KFTP_HEADER_SIZE;
    int second_msg_data_size = dummy_filesize - first_msg_data_size;

    // mocks
    will_return_always(fseek, FSEEK_SUCCESS);
    will_return(ftell, dummy_filesize);
    set_fread_buffer(dummy_file_contents, first_msg_data_size, first_msg_data_size);
    set_fread_buffer(&dummy_file_contents[first_msg_data_size], second_msg_data_size, second_msg_data_size);

    char expected_data[MAX_DATA_SIZE] = {};

    // Check first sent rudp message
    KftpHeader header = {.data_size=dummy_filesize};
    int serialized = serialize_kftp_header(&header , expected_data, MAX_DATA_SIZE);
    assert(first_msg_data_size + serialized <= MAX_DATA_SIZE);
    memcpy(&expected_data[serialized], dummy_file_contents, first_msg_data_size);
    check_rudp_send(expected_data, MAX_DATA_SIZE, RUDP_SEND_SUCCESS);

    // Check second sent rudp message
    assert(second_msg_data_size <= MAX_DATA_SIZE);
    memcpy(expected_data, &dummy_file_contents[first_msg_data_size], second_msg_data_size);
    check_rudp_send(expected_data, second_msg_data_size, RUDP_SEND_SUCCESS);

    int result = kftp_send_file(NULL, &socket_info, &sender, &receiver);

    assert_int_equal(result, 0);

    destroy_random_buffer(dummy_file_contents);
}

static void test_kftp_send_file_over_several_rudp_messages(void** state) {
    SocketInfo socket_info = {};
    RudpSender sender = {};
    RudpReceiver receiver = {};

    int dummy_filesize = MAX_DATA_SIZE * 5;
    char* dummy_file_contents = create_random_buffer(dummy_filesize);

    int first_msg_data_size = MAX_DATA_SIZE - KFTP_HEADER_SIZE;
    int remaining_data_size = dummy_filesize - first_msg_data_size;

    // mocks
    will_return_always(fseek, FSEEK_SUCCESS);
    will_return(ftell, dummy_filesize);
    set_fread_buffer(dummy_file_contents, first_msg_data_size, first_msg_data_size);
    while(remaining_data_size > 0) {
        int i = dummy_filesize - remaining_data_size;
        int next_read_size = min(remaining_data_size, MAX_DATA_SIZE);
        set_fread_buffer(&dummy_file_contents[i], next_read_size, next_read_size);
        remaining_data_size -= next_read_size;
    }

    char expected_data[MAX_DATA_SIZE] = {};

    // Check first sent rudp message
    KftpHeader header = {.data_size=dummy_filesize};
    int serialized = serialize_kftp_header(&header , expected_data, MAX_DATA_SIZE);
    assert(first_msg_data_size + serialized <= MAX_DATA_SIZE);
    memcpy(&expected_data[serialized], dummy_file_contents, first_msg_data_size);
    check_rudp_send(expected_data, MAX_DATA_SIZE, RUDP_SEND_SUCCESS);

    // Check successive sent rudp messages
    remaining_data_size = dummy_filesize - first_msg_data_size;
    while(remaining_data_size > 0) {
        int i = dummy_filesize - remaining_data_size;
        int next_read_size = min(remaining_data_size, MAX_DATA_SIZE);
        memcpy(expected_data, &dummy_file_contents[i], next_read_size);
        check_rudp_send(expected_data, next_read_size, RUDP_SEND_SUCCESS);
        remaining_data_size -= next_read_size;
    }

    int result = kftp_send_file(NULL, &socket_info, &sender, &receiver);

    assert_int_equal(result, 0);

    destroy_random_buffer(dummy_file_contents);
}

static void test_kftp_recv_small_file(void** state) {
    SocketInfo socket_info = {};
    RudpReceiver receiver = {};

    int dummy_filesize = 10;
    char* dummy_file_contents = create_random_buffer(dummy_filesize);

    // mocks
    char received_data[100] = {};
    int buffer_len = 100;
    KftpHeader header = {.data_size=dummy_filesize};
    int serialized = serialize_kftp_header(&header, received_data, buffer_len);
    int received_data_size = dummy_filesize + serialized;
    assert(received_data_size <= buffer_len);
    memcpy(&received_data[serialized], dummy_file_contents, dummy_filesize);
    set_rudp_recv_buffer(received_data, received_data_size, received_data_size);

    check_fwrite(dummy_file_contents, dummy_filesize, dummy_filesize);

    int result = kftp_recv_file(NULL, &socket_info, &receiver);

    assert_int_equal(result, 0);

    destroy_random_buffer(dummy_file_contents);
}

static void test_kftp_recv_file_over_two_messages(void** state) {
    SocketInfo socket_info = {};
    RudpReceiver receiver = {};

    int dummy_filesize = MAX_DATA_SIZE;
    char* dummy_file_contents = create_random_buffer(dummy_filesize);

    int first_msg_data_size = MAX_DATA_SIZE - KFTP_HEADER_SIZE;
    int second_msg_data_size = dummy_filesize - first_msg_data_size;

    char* received_buffers[2] = {
            (char[MAX_DATA_SIZE]) {},
            (char[MAX_DATA_SIZE]) {},
    };

    // mocks
    // receive and write first chunk
    KftpHeader header = {.data_size=dummy_filesize};
    int serialized = serialize_kftp_header(&header, received_buffers[0], MAX_DATA_SIZE);
    int received_data_size = serialized + first_msg_data_size;
    assert(received_data_size <= MAX_DATA_SIZE);
    memcpy(&received_buffers[0][serialized], dummy_file_contents, first_msg_data_size);
    set_rudp_recv_buffer(received_buffers[0], received_data_size, received_data_size);

    check_fwrite(dummy_file_contents, first_msg_data_size, first_msg_data_size);

    // receive and write second chunk
    assert(second_msg_data_size < MAX_DATA_SIZE);
    memcpy(received_buffers[1], &dummy_file_contents[first_msg_data_size], second_msg_data_size);
    set_rudp_recv_buffer(received_buffers[1], second_msg_data_size, second_msg_data_size);

    check_fwrite(&dummy_file_contents[first_msg_data_size], second_msg_data_size, second_msg_data_size);

    int result = kftp_recv_file(NULL, &socket_info, &receiver);

    assert_int_equal(result, 0);

    destroy_random_buffer(dummy_file_contents);
}

static void test_kftp_recv_file_over_several_rudp_messages(void** state) {
    SocketInfo socket_info = {};
    RudpReceiver receiver = {};

    int dummy_filesize = MAX_DATA_SIZE * 5;
    char* dummy_file_contents = create_random_buffer(dummy_filesize);
    int num_messages = 1 + (dummy_filesize + KFTP_HEADER_SIZE - 1) / MAX_DATA_SIZE;

    int first_msg_data_size = MAX_DATA_SIZE - KFTP_HEADER_SIZE;
    int remaining_data_size = dummy_filesize - first_msg_data_size;

    // mocks
    check_fwrite(dummy_file_contents, first_msg_data_size, first_msg_data_size);
    while(remaining_data_size > 0) {
        int i = dummy_filesize - remaining_data_size;
        int next_recv_size = min(remaining_data_size, MAX_DATA_SIZE);
        check_fwrite(&dummy_file_contents[i], next_recv_size, next_recv_size);
        remaining_data_size -= next_recv_size;
    }

    // should have a buffer for each message
    assert(num_messages == 6);
    char* received_buffers[6] = {
            (char[MAX_DATA_SIZE]) {},
            (char[MAX_DATA_SIZE]) {},
            (char[MAX_DATA_SIZE]) {},
            (char[MAX_DATA_SIZE]) {},
            (char[MAX_DATA_SIZE]) {},
            (char[MAX_DATA_SIZE]) {},
    };

    // Set first received rudp message
    KftpHeader header = {.data_size=dummy_filesize};
    int serialized = serialize_kftp_header(&header, received_buffers[0], MAX_DATA_SIZE);
    int received_data_size = serialized + first_msg_data_size;
    assert(received_data_size <= MAX_DATA_SIZE);
    memcpy(&received_buffers[0][serialized], dummy_file_contents, first_msg_data_size);
    set_rudp_recv_buffer(received_buffers[0], received_data_size, received_data_size);

    // Set successive received rudp messages
    remaining_data_size = dummy_filesize - first_msg_data_size;
    int i = 1;
    while(remaining_data_size > 0) {
        int offset = dummy_filesize - remaining_data_size;
        int next_recv_size = min(remaining_data_size, MAX_DATA_SIZE);
        memcpy(received_buffers[i], &dummy_file_contents[offset], next_recv_size);
        set_rudp_recv_buffer(received_buffers[i], next_recv_size, next_recv_size);
        remaining_data_size -= next_recv_size;
        i++;
    }

    int result = kftp_recv_file(NULL, &socket_info, &receiver);

    assert_int_equal(result, 0);

    destroy_random_buffer(dummy_file_contents);
}

int main(void) {
    const struct CMUnitTest tests[] = {
            cmocka_unit_test(test_kftp_send_small_file),
            cmocka_unit_test(test_kftp_send_file_over_two_rudp_messages),
            cmocka_unit_test(test_kftp_send_file_over_several_rudp_messages),
            cmocka_unit_test(test_kftp_recv_small_file),
            cmocka_unit_test(test_kftp_recv_file_over_two_messages),
            cmocka_unit_test(test_kftp_recv_file_over_several_rudp_messages),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

