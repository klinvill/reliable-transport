//
// RUDP (Reliable UDP) implementation
//
// RUDP provides reliable, in-order delivery on top of UDP
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


// Helper function that determines if a received message is within the old ack window and therefore should be sent an
// ack. This helps ensure that acks can be resent at a future time if they are lost on the way to the receiver.
bool in_old_ack_window(RudpMessage* received_message, RudpReceiver* receiver) {
    // TODO: we treat 0 as a special value here to indicate the type of message. Should instead modify the header
    //  struct to include an indication of if the message is a seq or ack
    return received_message->header.seq_num != 0
        && 0 <= (receiver->last_received - received_message->header.seq_num)
        && (receiver->last_received - received_message->header.seq_num) < ACK_WINDOW;
}


// Helper function to send an ack for the `received_message` to the `from` socket.
//
// Acks are not reliably delivered, so we can simply fire and forget the ack.
int ack(RudpMessage* received_message, SocketInfo* from) {
    RudpMessage ack_message = {.header = (RudpHeader) {.ack_num=received_message->header.seq_num, .data_size=0}};

    char wire_data[MAX_PAYLOAD_SIZE] = {0,};
    int wire_data_len = serialize(&ack_message, wire_data, MAX_PAYLOAD_SIZE);
    if (wire_data_len < 0) {
        fprintf(stderr, "ERROR in ack: Error serializing ack message\n");
        return wire_data_len;
    }

    return sendto(from->sockfd, wire_data, wire_data_len, 0, from->addr, from->addr_len);
}


// TODO: we only use RUDP to send a single message at a time, should we really support sending multiple chunks?
// Helper function to periodically send a single RUDP message until an ack is received
int rudp_send_chunk(char* data, int data_size, SocketInfo* to, RudpSender* sender, RudpReceiver* receiver) {
    if(data_size > MAX_DATA_SIZE || data_size < 0)
        return PAYLOAD_TOO_LARGE_ERROR;

    RudpHeader header = {.seq_num = sender->last_ack + 1, .ack_num = EMPTY_ACK_NUM, data_size = data_size};
    RudpMessage message = {.header = header, .data = data};

    // we estimate the size of the serialized data to proactively avoid potential buffer overflows from serialization
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

    // we keep track of the start and current times so we can eventually timeout the sender if a single RUDP message
    // isn't ever ack'd
    struct timeval sender_start;
    struct timeval current_time;
    int status = gettimeofday(&sender_start, NULL);
    if (status < 0) {
        fprintf(stderr, "ERROR in rudp_send_chunk: error getting sender start time\n");
        return status;
    }

    struct pollfd poll_fds[1];
    poll_fds[0] = (struct pollfd) {.fd=to->sockfd, .events=POLLIN};

    // Keep retrying to send the message until either an ack is received or the sender times out
    while(!acked) {
        status = gettimeofday(&current_time, NULL);
        if (status < 0) {
            fprintf(stderr, "ERROR in rudp_send_chunk: error getting current time\n");
            return status;
        }

        if(elapsed_time(&sender_start, &current_time) > sender->sender_timeout)
            return SENDER_TIMEOUT_ERROR;

        status = sendto(to->sockfd, wire_data, wire_data_len, 0, to->addr, to->addr_len);
        if (status < 0) {
            fprintf(stderr, "ERROR in rudp_send_chunk: error in sendto\n");
            continue;
        }

        // TODO: replace with adaptive timeout based on average RTTs
        // If a response isn't received within the expected RTT, we try sending the message again
        status = poll(poll_fds, 1, sender->message_timeout);
        if (status < 0) {
            fprintf(stderr, "ERROR in rudp_send_chunk: error polling socket\n");
            continue;
        }
        else if (status == 0)
            // timed out, retry
            continue;
        else {
            char buffer[MAX_PAYLOAD_SIZE] = {0,};
            int n = recvfrom(to->sockfd, buffer, MAX_PAYLOAD_SIZE, 0, to->addr, &to->addr_len);
            if (n < 0) {
                fprintf(stderr, "ERROR in rudp_send_chunk: error in recvfrom\n");
                continue;
            }

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
                if (status < 0) {
                    fprintf(stderr, "ERROR in rudp_send_chunk: error in ack\n");
                    continue;
                }
            }
        }
    };

    return 0;
}


// TODO: we only use RUDP to send a single message at a time, should we really support sending multiple chunks?
// Sends data in chunks through several RUDP messages
int rudp_send(char* data, int data_size, SocketInfo* to, RudpSender* sender, RudpReceiver* receiver) {
    int num_chunks = (data_size / (MAX_DATA_SIZE+1)) + 1;
    if (num_chunks < 1) {
        fprintf(stderr, "ERROR in rudp_send: invalid number of chunks to send\n");
        return -1;
    }

    int bytes_sent = 0;
    for (int i = 0; i < num_chunks; i++) {
        int chunk_size = min(data_size - bytes_sent, MAX_DATA_SIZE);
        int status = rudp_send_chunk(&data[bytes_sent], chunk_size, to, sender, receiver);
        if (status < 0) {
            fprintf(stderr, "ERROR in rudp_send: error sending chunk\n");
            return status;
        }
        bytes_sent += chunk_size;
    }

    assert(bytes_sent == data_size);
    return 0;
}


// Helper function to handle received message and free allocated memory. In particular, this function frees up the
// dynamically allocated data buffer in deserialized messages
int rudp_handle_received_message(RudpMessage* received_message, char* buffer, int buffer_size, SocketInfo* from, RudpReceiver* receiver) {
    int ret_code = 0;

    if (received_message->header.data_size > buffer_size) {
        fprintf(stderr, "ERROR in rudp_handle_received_message: Received message's payload too large for buffer\n");
        ret_code = -1;
        goto dealloc;
    }

    // To cover the case where an ack for a previous message has been sent that the receiver hasn't received, we
    // simply reply with an ACK for any message that has a sequence number within the ACK_WINDOW preceding our last
    // received sequence number
    if (received_message->header.seq_num == receiver->last_received + 1
        || in_old_ack_window(received_message, receiver)) {
        int status = ack(received_message, from);
        if (status < 0) {
            fprintf(stderr, "ERROR in rudp_handle_received_message: error in ack\n");
            ret_code = status;
            goto dealloc;
        }

        // Found the next message we are looking for
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
    // TODO: should include a receiver timeout like the sender timeout
    while (1) {
        int n = recvfrom(from->sockfd, buffer, buffer_size, 0, from->addr, &from->addr_len);
        if (n < 0) {
            fprintf(stderr, "ERROR in rudp_recv: error in recvfrom\n");
            continue;
        }

        RudpMessage received_message = {};
        int deserialized = deserialize(buffer, buffer_size, &received_message);
        if (deserialized < 0) {
            fprintf(stderr, "Deserialization error %d in rudp_recv, ignoring message\n", deserialized);
            continue;
        }

        // Beyond this point, memory should have been allocated by the deserialize() function. It needs to be freed.
        // This is currently taken care of in rudp_handle_received_message().

        int last_received = receiver->last_received;
        int status = rudp_handle_received_message(&received_message, buffer, buffer_size, from, receiver);
        if (status < 0) {
            fprintf(stderr, "ERROR in rudp_recv: error in rudp_handle_received_message, ignoring message\n");
            continue;
        }

        // If the message received was the message we're looking for, it should increment the receiver->last_received
        // field
        if (receiver->last_received == last_received + 1) {
            return status;
        }
    }
}

// Helper function to handle received message and free allocated memory. In particular, this function frees up the
// dynamically allocated data buffer in deserialized messages
int rudp_handle_received_ack(RudpMessage* received_message, SocketInfo* from, RudpReceiver* receiver) {
    int ret_code = 0;

    // we only care about when we need to send acks, will drop any other messages
    if (!in_old_ack_window(received_message, receiver)) {
        fprintf(stderr, "Received message not in ack window, dropping\n");
        goto dealloc;
    }

    int status = ack(received_message, from);
    ret_code = status;
    if (status < 0) {
        fprintf(stderr, "ERROR in rudp_handle_received_ack: error in ack\n");
        goto dealloc;
    }

dealloc:
    free(received_message->data);

    // 0 if not acked, <0 if error, >0 if acked
    return ret_code;
}

int rudp_check_acks(char* buffer, int buffer_size, SocketInfo* from, RudpReceiver* receiver) {
    bool handled_ack = true;
    int handled_acks = 0;

    struct pollfd poll_fds[1];
    poll_fds[0] = (struct pollfd) {.fd=from->sockfd, .events=POLLIN};

    while (handled_ack) {
        // TODO: replace with adaptive timeout
        int status = poll(poll_fds, 1, INITIAL_TIMEOUT);
        if (status < 0) {
            fprintf(stderr, "ERROR in rudp_check_acks: error in poll\n");
            return status;
        }
        else if (status == 0)
            // timed out, no acks
            break;

        int n = recvfrom(from->sockfd, buffer, buffer_size, 0, from->addr, &from->addr_len);
        if (n < 0) {
            fprintf(stderr, "ERROR in rudp_check_acks: error in recvfrom\n");
            continue;
        }

        RudpMessage received_message = {};
        int deserialized = deserialize(buffer, buffer_size, &received_message);
        if (deserialized < 0) {
            fprintf(stderr, "Deserialization error %d in rudp_check_acks, ignoring message\n", deserialized);
            continue;
        }

        // Beyond this point, memory should have been allocated by the deserialize() function. It needs to be freed.
        // This is currently taken care of in rudp_handle_received_ack().

        status = rudp_handle_received_ack(&received_message, from, receiver);
        handled_ack = status > 0;
        if (handled_ack)
            handled_acks++;
    }
    return handled_acks;
}
