//
// Created by Kirby Linvill on 1/27/21.
//

#include "mocks.h"

#include <stdbool.h>
#include <string.h>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>


void set_sendto_rc(ssize_t ret_code) {
    will_return(sendto, false);     // set_buffer
    will_return(sendto, false);     // check_buffer

    will_return(sendto, ret_code);
}

void set_sendto_rc_count(ssize_t ret_code, int count) {
    // Note: we can't use will_return_always or will_return_count here. If we wanted to repeatedly send the arguments
    // a and b to a function f, the different functions would give us the following calls:
    //      will_return_always(f, _) => f(a,a), f(a,a), f(a,a), f(a,a), ...
    //      will_return_count(f, _, 2) => f(a,a), f(b,b)
    //      will_return() x 2 => f(a,b), f(a,b)
    for (int i = 0; i < count; i++) {
        set_sendto_rc(ret_code);
    }
}

void set_sendto_buffer(char* buffer, size_t buff_size, ssize_t ret_code) {
    will_return(sendto, true);      // set_buffer
    will_return(sendto, false);     // check_buffer
    will_return(sendto, buff_size);
    will_return(sendto, buffer);

    will_return(sendto, ret_code);
}

void set_sendto_buffer_count(char* buffer, size_t buff_size, ssize_t ret_code, int count) {
    for (int i = 0; i < count; i++) {
        set_sendto_buffer(buffer, buff_size, ret_code);
    }
}

void check_sendto(char* expected_buffer, size_t buff_size, ssize_t ret_code) {
    will_return(sendto, false);     // set_buffer
    will_return(sendto, true);      // check_buffer
    expect_memory(sendto, buffer, expected_buffer, buff_size);

    will_return(sendto, ret_code);
}

void set_recvfrom_rc(ssize_t ret_code) {
    will_return(recvfrom, false);     // set_buffer
    will_return(recvfrom, false);     // check_buffer

    will_return(recvfrom, ret_code);
}

void set_recvfrom_rc_count(ssize_t ret_code, int count) {
    for (int i = 0; i < count; i++) {
        set_recvfrom_rc(ret_code);
    }
}

void set_recvfrom_buffer(char* buffer, size_t buff_size, ssize_t ret_code) {
    will_return(recvfrom, true);      // set_buffer
    will_return(recvfrom, false);     // check_buffer
    will_return(recvfrom, buff_size);
    will_return(recvfrom, buffer);

    will_return(recvfrom, ret_code);
}

void set_recvfrom_buffer_count(char* buffer, size_t buff_size, ssize_t ret_code, int count) {
    for (int i = 0; i < count; i++) {
        set_recvfrom_buffer(buffer, buff_size, ret_code);
    }
}

void check_recvfrom(char* expected_buffer, size_t buff_size, ssize_t ret_code) {
    will_return(recvfrom, false);     // set_buffer
    will_return(recvfrom, true);      // check_buffer
    expect_memory(recvfrom, buffer, expected_buffer, buff_size);

    will_return(recvfrom, ret_code);
}

void set_poll_rc(ssize_t ret_code) {
    will_return(poll, ret_code);
}

ssize_t sendto(int socket, const void* buffer, size_t length, int flags, const struct sockaddr* dest_addr, socklen_t dest_len)
{
    bool set_buffer = mock_type(bool);
    bool check_buffer = mock_type(bool);

    if (set_buffer) {
        size_t in_buffer_len = mock_type(int);
        char *in_buffer = mock_type(char*);
        if (in_buffer_len > length)
            return -1;
        memcpy((void *) buffer, in_buffer, in_buffer_len);
    }

    if (check_buffer) {
        check_expected(buffer);
    }

    return mock_type(ssize_t);
}

int poll(struct pollfd fds[], nfds_t ndfs, int timeout) {
    return mock_type(int);
}

ssize_t recvfrom(int socket, void *restrict buffer, size_t length, int flags, struct sockaddr *restrict address, socklen_t *restrict address_len)
{
    bool set_buffer = mock_type(bool);
    bool check_buffer = mock_type(bool);

    if (set_buffer) {
        size_t in_buffer_len = mock_type(int);
        char *in_buffer = mock_type(char*);
        if (in_buffer_len > length)
            return -1;
        memcpy(buffer, in_buffer, in_buffer_len);
    }

    if (check_buffer) {
        check_expected(buffer);
    }

    return mock_type(ssize_t);
}


void set_fread_buffer(char* buffer, size_t buff_size, size_t ret_val) {
    will_return(fread, buff_size);
    will_return(fread, buffer);

    will_return(fread, ret_val);
}

void check_fwrite(char* expected_buffer, size_t buff_size, ssize_t ret_code) {
    expect_value(fwrite, size, sizeof(char));
    expect_value(fwrite, nitems, buff_size);
    expect_memory(fwrite, ptr, expected_buffer, buff_size);

    will_return(fwrite, ret_code);
}

int fseek(FILE *stream, long offset, int whence) {
    return mock_type(int);
}

long ftell(FILE *stream) {
    return mock_type(long);
};

int feof(FILE *stream) {
    return mock_type(int);
}

size_t fread(void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream) {
    size_t in_buffer_len = mock_type(int);
    char *in_buffer = mock_type(char*);
    if (in_buffer_len > (size * nitems))
        return 0;

    memcpy(ptr, in_buffer, in_buffer_len);

    return mock_type(size_t);
}

size_t fwrite(const void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream) {
    check_expected(size);
    check_expected(nitems);
    check_expected(ptr);

    return mock_type(size_t);
}
