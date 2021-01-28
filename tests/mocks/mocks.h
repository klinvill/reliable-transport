//
// Created by Kirby Linvill on 1/27/21.
//

#ifndef UDP_MOCKS_H
#define UDP_MOCKS_H

#include <poll.h>
#include <sys/socket.h>

ssize_t default_mock_ssize_t(void) {
    return 0;
}

int default_mock_int(void) {
    return 0;
}

ssize_t default_mock_ssize_t_buffer(char* buffer) {
    return 0;
}

ssize_t (*mocked_sendto)(void) = default_mock_ssize_t;
int (*mocked_poll)(void) = default_mock_int;
ssize_t (*mocked_recvfrom)(char*)= default_mock_ssize_t_buffer;

ssize_t sendto(int socket, const void* buffer, size_t length, int flags, const struct sockaddr* dest_addr, socklen_t dest_len);
int poll(struct pollfd fds[], nfds_t ndfs, int timeout);
ssize_t recvfrom(int socket, void *restrict buffer, size_t length, int flags, struct sockaddr *restrict address, socklen_t *restrict address_len);

void set_sendto_fn(ssize_t (*f)(void));
void set_poll_fn(int (*f)(void));
void set_recvfrom_fn(ssize_t (*f)(char*));

#endif //UDP_MOCKS_H
