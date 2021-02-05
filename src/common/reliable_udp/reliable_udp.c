//
// Created by Kirby Linvill on 1/26/21.
//

#include "reliable_udp.h"

#include <stdbool.h>
#include <sys/time.h>
#include <poll.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "serde.h"
#include "types.h"
#include "../utils.h"


bool in_old_ack_window(RudpMessage* received_message, RudpReceiver* receiver) {
    // TODO: we treat 0 as a special value here to indicate the type of message. Should instead modify the header
    //  struct to include an indication of if the message is a seq or ack
    return received_message->header.seq_num != 0
        && 0 <= (receiver->last_received - received_message->header.seq_num)
        && (receiver->last_received - received_message->header.seq_num) < ACK_WINDOW;
}

int ack(RudpMessage* received_message, SocketInfo* from) {
    RudpMessage ack_message = {.header = (RudpHeader) {.ack_num=received_message->header.seq_num, .data_size=0}};

    char wire_data[MAX_PAYLOAD_SIZE] = {0,};
    int wire_data_len = serialize(&ack_message, wire_data, MAX_PAYLOAD_SIZE);
    if (wire_data_len < 0) {
        fprintf(stderr, "Error serializing ack message\n");
        return wire_data_len;
    }

    return sendto(from->sockfd, wire_data, wire_data_len, 0, from->addr, from->addr_len);
}

int rudp_send_chunk(char* data, int data_size, SocketInfo* to, RudpSender* sender, RudpReceiver* receiver) {
    if(data_size > MAX_DATA_SIZE || data_size < 0)
        return PAYLOAD_TOO_LARGE_ERROR;

    RudpHeader header = {.seq_num = sender->last_ack + 1, .ack_num = EMPTY_ACK_NUM, data_size = data_size};
    RudpMessage message = {.header = header, .data = data};

    int estimated_serialized_size = data_size + sizeof(header);
    if (estimated_serialized_size > MAX_PAYLOAD_SIZE || estimated_serialized_size < 0)
        return PAYLOAD_TOO_LARGE_ERROR;

    char wire_data[MAX_PAYLOAD_SIZE] = {0,};
    int wire_data_len = serialize(&message, wire_data, MAX_PAYLOAD_SIZE);

    if (wire_data_len < 0)
        return wire_data_len;

    if(wire_data_len > MAX_PAYLOAD_SIZE || wire_data_len < 0)
        return PAYLOAD_TOO_LARGE_ERROR;

    bool acked = false;

    struct timeval sender_start;
    struct timeval current_time;
    int status = gettimeofday(&sender_start, NULL);
    // TODO: error handling
    if (status < 0)
        return status;

    struct pollfd poll_fds[1];
    poll_fds[0] = (struct pollfd) {.fd=to->sockfd, .events=POLLRDNORM};

    do {
        status = gettimeofday(&current_time, NULL);
        // TODO: error handling
        if (status < 0)
            return status;

        if(elapsed_time(&sender_start, &current_time) > sender->sender_timeout)
            return SENDER_TIMEOUT_ERROR;

        status = sendto(to->sockfd, wire_data, wire_data_len, 0, to->addr, to->addr_len);
        // TODO: error handling
        if (status < 0)
            return status;

        // TODO: replace with adaptive timeout based on average RTTs
        status = poll(poll_fds, 1, sender->message_timeout);
        // TODO: error handling
        if (status < 0)
            return status;
        else if (status == 0)
            // timed out, retry
            continue;
        else {
            char buffer[MAX_PAYLOAD_SIZE] = {0,};
            int n = recvfrom(to->sockfd, buffer, MAX_PAYLOAD_SIZE, 0, to->addr, &to->addr_len);
            // TODO: error handling
            if (n < 0)
                return n;

            RudpMessage received_message = {};
            // TODO: ensure any memory allocated by deserialization is freed
            int deserialized = deserialize(buffer, MAX_PAYLOAD_SIZE, &received_message);

            if (deserialized < 0) {
                fprintf(stderr, "Deserialization error %d in rudp_send_chunk, ignoring message\n", deserialized);
                continue;
            }

            if (received_message.header.ack_num == sender->last_ack + 1) {
                sender->last_ack++;
                acked = true;
            }
            // if a previously sent ack is lost, the receiver could be stuck re-sending their message and never process
            // the one we just sent. To handle this situation, we also need to be able to respond with acks to previous
            // incoming messages
            else if (in_old_ack_window(&received_message, receiver)) {
                status = ack(&received_message, to);
                // TODO: error handling
                if (status < 0)
                    continue;
            }
        }
    } while(!acked);

    return 0;
}

int rudp_send(char* data, int data_size, SocketInfo* to, RudpSender* sender, RudpReceiver* receiver) {
    int num_chunks = (data_size / (MAX_DATA_SIZE+1)) + 1;
    // TODO: error handling
    if (num_chunks < 1)
        return -1;

    int bytes_sent = 0;
    for (int i = 0; i < num_chunks; i++) {
        int chunk_size = min(data_size - bytes_sent, MAX_DATA_SIZE);
        int status = rudp_send_chunk(&data[bytes_sent], chunk_size, to, sender, receiver);
        // TODO: error handling
        if (status < 0)
            return status;
        bytes_sent += chunk_size;
    }

    return 0;
}

// deserialized messages have a dynamically allocated data buffer that needs to be freed
int rudp_handle_received_message(RudpMessage* received_message, char* buffer, int buffer_size, SocketInfo* from, RudpReceiver* receiver) {
    int ret_code = 0;

    // TODO: error handling
    if (received_message->header.data_size > buffer_size) {
        fprintf(stderr, "Received message's payload too large for buffer\n");
        ret_code = -1;
        goto dealloc;
    }

    // To cover the case where an ack for a previous message has been sent that the receiver hasn't received, we
    // simply reply with an ACK for any message that has a sequence number within the ACK_WINDOW preceding our last
    // received sequence number
    if (received_message->header.seq_num == receiver->last_received + 1
        || in_old_ack_window(received_message, receiver)) {
        int status = ack(received_message, from);
        // TODO: error handling
        if (status < 0) {
            ret_code = status;
            goto dealloc;
        }

        if (received_message->header.seq_num == receiver->last_received + 1) {
            receiver->last_received++;
            assert(buffer_size >= received_message->header.data_size);
            memcpy(buffer, received_message->data, received_message->header.data_size);
            ret_code = received_message->header.data_size;
            goto dealloc;
        }
    }

dealloc:
    free(received_message->data);

    return ret_code;
}

int rudp_recv(char* buffer, int buffer_size, SocketInfo* from, RudpReceiver* receiver) {
    while (1) {
        int n = recvfrom(from->sockfd, buffer, buffer_size, 0, from->addr, &from->addr_len);
        // TODO: error handling
        if (n < 0)
            return n;

        RudpMessage received_message = {};
        int deserialized = deserialize(buffer, buffer_size, &received_message);
        // TODO: error handling
        if (deserialized < 0) {
            fprintf(stderr, "Deserialization error %d in rudp_recv, ignoring message\n", deserialized);
            continue;
        }

        int last_received = receiver->last_received;
        // frees received_message's dynamically allocated data buffer before exiting
        int status = rudp_handle_received_message(&received_message, buffer, buffer_size, from, receiver);
        if (status < 0 || receiver->last_received == last_received + 1)
            return status;
    }
}

// deserialized messages have a dynamically allocated data buffer that needs to be freed
int rudp_handle_received_ack(RudpMessage* received_message, SocketInfo* from, RudpReceiver* receiver) {
    int ret_code = 0;

    // we only care about acks, will drop any other messages
    if (!in_old_ack_window(received_message, receiver)) {
        fprintf(stderr, "Received message not in ack window, dropping\n");
        goto dealloc;
    }

    int status = ack(received_message, from);
    ret_code = status;
    // TODO: error handling
    if (status < 0)
        goto dealloc;

    dealloc:
    free(received_message->data);

    // 0 if not acked, <0 if error, >0 if acked
    return ret_code;
}

int rudp_check_acks(char* buffer, int buffer_size, SocketInfo* from, RudpReceiver* receiver) {
    bool handled_ack = true;
    int handled_acks = 0;

    struct pollfd poll_fds[1];
    poll_fds[0] = (struct pollfd) {.fd=from->sockfd, .events=POLLRDNORM};

    while (handled_ack) {
        // TODO: replace with adaptive timeout
        int status = poll(poll_fds, 1, INITIAL_TIMEOUT);
        // TODO: error handling
        if (status < 0)
            return status;
        else if (status == 0)
            // timed out, no acks
            break;

        int n = recvfrom(from->sockfd, buffer, buffer_size, 0, from->addr, &from->addr_len);
        // TODO: error handling
        if (n < 0)
            return n;

        RudpMessage received_message = {};
        int deserialized = deserialize(buffer, buffer_size, &received_message);
        // TODO: error handling
        if (deserialized < 0) {
            fprintf(stderr, "Deserialization error %d in rudp_check_acks, ignoring message\n", deserialized);
            continue;
        }

        status = rudp_handle_received_ack(&received_message, from, receiver);
        handled_ack = status > 0;
        if (handled_ack)
            handled_acks++;
    }
    return handled_acks;
}
