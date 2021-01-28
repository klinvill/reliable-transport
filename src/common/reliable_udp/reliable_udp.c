//
// Created by Kirby Linvill on 1/26/21.
//

#include "reliable_udp.h"

#include <stdbool.h>
#include <sys/time.h>
#include <poll.h>

#include "serde.h"
#include "types.h"
#include "../utils.h"


int rudp_send(char* data, int data_size, SocketInfo* to, RudpSender* sender) {
    if(data_size >= MAX_PAYLOAD_SIZE || data_size < 0)
        return PAYLOAD_TOO_LARGE_ERROR;

    RudpHeader header = {.seq_num = sender->last_ack + 1, .ack_num = EMPTY_ACK_NUM, data_size = data_size};
    RudpMessage message = {.header = header, .data = data};

    int estimated_serialized_size = data_size + sizeof(header);
    if (estimated_serialized_size > MAX_PAYLOAD_SIZE)
        return PAYLOAD_TOO_LARGE_ERROR;

    char wire_data[MAX_PAYLOAD_SIZE] = {0,};
    int wire_data_len = serialize(&message, wire_data, MAX_PAYLOAD_SIZE);

    if (wire_data_len < 0)
        return wire_data_len;

    if(wire_data_len > MAX_PAYLOAD_SIZE)
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
        status = poll(poll_fds, 1, sender->sender_timeout);
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
            int deserialized = deserialize(buffer, MAX_PAYLOAD_SIZE, &received_message);

            if (deserialized < 0)
                return deserialized;

            if (received_message.header.ack_num == sender->last_ack + 1) {
                sender->last_ack++;
                acked = true;
            }
        }
    } while(!acked);

    return 0;
}
