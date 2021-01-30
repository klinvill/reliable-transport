//
// Created by Kirby Linvill on 1/27/21.
//

#ifndef UDP_MOCKS_H
#define UDP_MOCKS_H

#include <poll.h>
#include <sys/socket.h>
#include <stdio.h>

// Socket mocks
ssize_t sendto(int socket, const void* buffer, size_t length, int flags, const struct sockaddr* dest_addr, socklen_t dest_len);
int poll(struct pollfd fds[], nfds_t ndfs, int timeout);
ssize_t recvfrom(int socket, void *restrict buffer, size_t length, int flags, struct sockaddr *restrict address, socklen_t *restrict address_len);

// File mocks
int fseek(FILE *stream, long offset, int whence);
long ftell(FILE *stream);
int feof(FILE *stream);
size_t fread(void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream);
size_t fwrite(const void *restrict ptr, size_t size, size_t nitems, FILE *restrict stream);

// helper functions to wrap expected cmocka arguments
void set_sendto_rc(ssize_t ret_code);
void set_sendto_rc_count(ssize_t ret_code, int count);
void set_sendto_buffer(char* buffer, size_t buff_size, ssize_t ret_code);
void set_sendto_buffer_count(char* buffer, size_t buff_size, ssize_t ret_code, int count);
void check_sendto(char* expected_buffer, size_t buff_size, ssize_t ret_code);

void set_recvfrom_rc(ssize_t ret_code);
void set_recvfrom_rc_count(ssize_t ret_code, int count);
void set_recvfrom_buffer(char* buffer, size_t buff_size, ssize_t ret_code);
void set_recvfrom_buffer_count(char* buffer, size_t buff_size, ssize_t ret_code, int count);
void check_recvfrom(char* expected_buffer, size_t buff_size, ssize_t ret_code);

void set_poll_rc(ssize_t ret_code);

void set_fread_buffer(char* buffer, size_t buff_size, size_t ret_val);

void check_fwrite(char* expected_buffer, size_t buff_size, ssize_t ret_code);

#endif //UDP_MOCKS_H
