//
// Created by Kirby Linvill on 1/27/21.
//

#include <check.h>

#include "../../../src/common/reliable_udp/serde.h"

#include <stdlib.h>


START_TEST(test_serialize_int) {
    int val = 0;
    char expected[4] = {0,};
    char result[4] = {0,};

    int serialized = serialize_int(val, result, 4);
    ck_assert_int_eq(serialized, 4);
    ck_assert_mem_eq(result, expected, 4);

    val = 236;
    memcpy(expected, (char[4]) {0, 0, 0, val}, sizeof(*expected) * 4);

    serialized = serialize_int(val, result, 4);
    ck_assert_int_eq(serialized, 4);
    ck_assert_mem_eq(result, expected, 4);
}
END_TEST

START_TEST(test_serialize_header) {
    int buffer_length = 12;
    RudpHeader header = {.seq_num=0, .ack_num=0, .data_size=0};
    char expected[12] = {0,};
    char result[12] = {0,};

    int serialized = serialize_header(&header, result, buffer_length);
    ck_assert_int_eq(serialized, buffer_length);
    ck_assert_mem_eq(result, expected, buffer_length);

    header = (RudpHeader) {.seq_num=123, .ack_num=456, .data_size=789};
    memcpy(expected, (char[]) {0, 0, 0, 123, 0, 0, 1, 200, 0, 0, 3, 21}, sizeof(*expected) * buffer_length);

    serialized = serialize_header(&header, result, buffer_length);
    ck_assert_int_eq(serialized, buffer_length);
    ck_assert_mem_eq(result, expected, buffer_length);
}
END_TEST

START_TEST(test_serialize_message) {
    int buffer_length = 1024;
    int header_size = 12;
    char data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    int data_size = 9;
    RudpHeader header = {.seq_num=0, .ack_num=0, .data_size=data_size};
    RudpMessage message = {.header=header, .data=data};
    // the data comes after the header, and the header should be 12 bytes long
    char expected[1024] = {[11]=9, [12]=1, [13]=2, [14]=3, [15]=4, [16]=5, [17]=6, [18]=7, [19]=8, [20]=9, 0,};
    char result[1024] = {0,};

    int serialized = serialize(&message, result, buffer_length);
    ck_assert_int_eq(serialized, header_size + data_size);
    ck_assert_mem_eq(result, expected, buffer_length);
}
END_TEST

START_TEST(test_serialize_message_with_empty_data) {
    int buffer_length = 1024;
    int header_size = 12;
    RudpHeader header = {.seq_num=0, .ack_num=0, .data_size=0};
    char* data = NULL;
    int data_size = 0;
    RudpMessage message = {.header=header, .data=data};
    char expected[1024] = {0,};
    char result[1024] = {0,};

    int serialized = serialize(&message, result, buffer_length);
    ck_assert_int_eq(serialized, header_size + data_size);
    ck_assert_mem_eq(result, expected, buffer_length);
}
END_TEST

START_TEST(test_deserialize_int) {
    int expected_int_len = 4;
    char buffer[4] = {0x0a, 0xbc, 0xde, 0xf0};
    int expected_val = 0x0abcdef0;

    int result = 0;
    int deserialized = deserialize_int(buffer, expected_int_len, &result);

    ck_assert_int_eq(deserialized, expected_int_len);
    ck_assert_int_eq(result, expected_val);


    memcpy(buffer, (char[4]) {0xf0, 0, 0, 0}, expected_int_len);
    expected_val = 0xf0000000;

    deserialized =  deserialize_int(buffer, expected_int_len, &result);
    ck_assert_int_eq(deserialized, expected_int_len);
    ck_assert_int_eq(result, expected_val);
}
END_TEST

START_TEST(test_deserialize_header) {
    int expected_deserialized_bytes = 12;
    char buffer[12] = {0, 0, 0, 123, 0, 0, 1, 200, 0, 0, 3, 21};
    RudpHeader expected_header = {.seq_num=123, .ack_num=456, .data_size=789};

    RudpHeader result = {};
    int deserialized = deserialize_header(buffer, expected_deserialized_bytes, &result);

    ck_assert_int_eq(deserialized, expected_deserialized_bytes);
    ck_assert(result.seq_num == expected_header.seq_num
                && result.ack_num == expected_header.ack_num
                && result.data_size == expected_header.data_size
    );

}
END_TEST

START_TEST(test_deserialize_message) {
    int expected_deserialized_bytes = 21;
    // deserialization relies on the length field to accurately represent the size of data
    char buffer[21] = {[11]=9, [12]=1, [13]=2, [14]=3, [15]=4, [16]=5, [17]=6, [18]=7, [19]=8, [20]=9};
    int expected_data_size = 9;
    RudpHeader  expected_header = {.seq_num=0, .ack_num=0, .data_size=expected_data_size};

    RudpMessage result = {};
    int deserialized = deserialize(buffer, expected_deserialized_bytes, &result);

    ck_assert_int_eq(deserialized, expected_deserialized_bytes);
    ck_assert(result.header.seq_num == expected_header.seq_num
              && result.header.ack_num == expected_header.ack_num
              && result.header.data_size == expected_header.data_size
    );
    ck_assert_int_eq(result.header.data_size, expected_data_size);
    ck_assert_mem_eq(&buffer[12], result.data, result.header.data_size);

    // TODO: should avoid needing to manually free allocated data buffers
    free(result.data);
}
END_TEST

START_TEST(test_serialize_then_deserialize_message) {
    int buffer_length = 1024;
    char data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    int data_size = 9;
    RudpHeader header = {.seq_num=0, .ack_num=0, .data_size=data_size};
    RudpMessage message = {.header=header, .data=data};

    char result_buffer[1024] = {0,};
    RudpMessage result_message = {};

    int serialized = serialize(&message, result_buffer, buffer_length);
    int deserialized = deserialize(result_buffer, buffer_length, &result_message);

    ck_assert_int_eq(deserialized, serialized);
    ck_assert(result_message.header.seq_num == message.header.seq_num
              && result_message.header.ack_num == message.header.ack_num
              && result_message.header.data_size == message.header.data_size
    );
    ck_assert_mem_eq(result_message.data, message.data, result_message.header.data_size);

    // TODO: should avoid needing to manually free allocated data buffers
    free(result_message.data);
}
END_TEST

START_TEST(test_deserialize_then_serialize_message) {
    int buffer_length = 1024;
    char buffer[1024] = {[11]=9, [12]=1, [13]=2, [14]=3, [15]=4, [16]=5, [17]=6, [18]=7, [19]=8, [20]=9};

    RudpMessage result_message = {};
    char result_buffer[1024] = {0,};

    int deserialized = deserialize(buffer, buffer_length, &result_message);
    int serialized = serialize(&result_message, result_buffer, buffer_length);

    ck_assert_int_eq(serialized, deserialized);
    ck_assert_mem_eq(result_buffer, buffer, buffer_length);

    // TODO: should avoid needing to manually free allocated data buffers
    free(result_message.data);
}
END_TEST

Suite* serde_suite(void) {
    Suite *s;
    TCase *tc_core;
    s = suite_create("SerDe");

    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_serialize_int);
    tcase_add_test(tc_core, test_serialize_header);
    tcase_add_test(tc_core, test_serialize_message);
    tcase_add_test(tc_core, test_serialize_message_with_empty_data);

    tcase_add_test(tc_core, test_deserialize_int);
    tcase_add_test(tc_core, test_deserialize_header);
    tcase_add_test(tc_core, test_deserialize_message);

    tcase_add_test(tc_core, test_serialize_then_deserialize_message);
    tcase_add_test(tc_core, test_deserialize_then_serialize_message);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void) {
    int num_failed = 0;
    Suite *s;
    SRunner *sr;

    s = serde_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    num_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return num_failed;
}
