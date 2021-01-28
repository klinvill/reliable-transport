//
// Created by Kirby Linvill on 1/27/21.
//

#include "mocks.h"

ssize_t __attribute__ ((noinline)) sendto(int socket, const void* buffer, size_t length, int flags, const struct sockaddr* dest_addr, socklen_t dest_len) {
    return mocked_sendto();
}

int __attribute__ ((noinline)) poll(struct pollfd fds[], nfds_t ndfs, int timeout) {
    return mocked_poll();
}

ssize_t __attribute__ ((noinline)) recvfrom(int socket, void *restrict buffer, size_t length, int flags, struct sockaddr *restrict address, socklen_t *restrict address_len) {
    return mocked_recvfrom(buffer);
}

void set_sendto_fn(ssize_t (*f)(void)) {
    mocked_sendto = f;
};
void set_poll_fn(int (*f)(void)) {
    mocked_poll = f;
};
void set_recvfrom_fn(ssize_t (*f)(char*)) {
    mocked_recvfrom = f;
};
