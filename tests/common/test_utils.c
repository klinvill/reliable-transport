//
// Created by Kirby Linvill on 1/27/21.
//

#include <check.h>

#include "../../src/common/utils.h"


START_TEST(test_elapsed_time_no_diff_is_zero) {
    struct timeval start = {.tv_sec = 0, .tv_usec = 0};
    struct timeval end = {.tv_sec = 0, .tv_usec = 0};

    int result = elapsed_time(&start, &end);
    ck_assert_int_eq(result, 0);

    start = (struct timeval) {.tv_sec = 5, .tv_usec = 32};
    end = (struct timeval) {.tv_sec = 5, .tv_usec = 32};

    result = elapsed_time(&start, &end);
    ck_assert_int_eq(result, 0);
}
END_TEST


START_TEST(test_elapsed_time_is_in_milliseconds) {
    struct timeval start = {.tv_sec = 0, .tv_usec = 0};
    struct timeval end = {.tv_sec = 5, .tv_usec = 32};

    int expected_result = 5000;
    int result = elapsed_time(&start, &end);
    ck_assert_int_eq(result, expected_result);

    start = (struct timeval) {.tv_sec = 0, .tv_usec = 0};
    end = (struct timeval) {.tv_sec = 5, .tv_usec = 3200};

    expected_result = 5003;
    result = elapsed_time(&start, &end);
    ck_assert_int_eq(result, expected_result);
}
END_TEST


START_TEST(test_elapsed_time_can_be_negative) {
    struct timeval start = {.tv_sec = 5, .tv_usec = 0};
    struct timeval end = {.tv_sec = 0, .tv_usec = 0};

    int expected_result = -5000;
    int result = elapsed_time(&start, &end);
    ck_assert_int_eq(result, expected_result);

    start = (struct timeval) {.tv_sec = 5, .tv_usec = 0};
    end = (struct timeval) {.tv_sec = 0, .tv_usec = 3200};

    expected_result = -5000 + 3;
    result = elapsed_time(&start, &end);
    ck_assert_int_eq(result, expected_result);
}
END_TEST


Suite* utils_suite(void) {
    Suite *s;
    TCase *tc_core;
    s = suite_create("Utils");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_elapsed_time_no_diff_is_zero);
    tcase_add_test(tc_core, test_elapsed_time_is_in_milliseconds);
    tcase_add_test(tc_core, test_elapsed_time_can_be_negative);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void) {
    int num_failed = 0;
    Suite *s;
    SRunner *sr;

    s = utils_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    num_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return num_failed;
}
