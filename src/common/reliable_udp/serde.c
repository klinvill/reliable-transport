//
// Provides serialize and deserialize functions for RUDP headers and messages
//

#include "serde.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>


int serialize(RudpMessage* message, char* buffer, int buffer_len) {
    // We expect the `data_size` field in the header to accurately represent the size of `buffer`
    unsigned int space_needed = sizeof(message->header) + sizeof(*message->data) * message->header.data_size;
    if (space_needed > buffer_len)
        return PAYLOAD_TOO_LARGE_ERROR;

    int i = 0;
    int serialized;

    serialized = serialize_header(&message->header, buffer, buffer_len);
    // TODO: error handling
    if (serialized < 0)
        return serialized;

    i += serialized;

    // To serialize the data, we can simply copy it directly to the buffer since they're both char arrays
    memcpy(&buffer[i], message->data, message->header.data_size);

    return i + message->header.data_size;
}


int deserialize(char* buffer, int buffer_len, RudpMessage* message) {
    int deserialized = deserialize_header(buffer, buffer_len, &message->header);
    // TODO: error handling
    if (deserialized < 0)
        return -1;

    // We rely on the `data_size` field to determine the size of the `data`
    int expected_data_size = message->header.data_size * sizeof(*message->data);
    int expected_size = deserialized + expected_data_size;

    // TODO: error handling
    if (buffer_len < expected_size || expected_data_size < 0 || expected_size < 0)
        return -1;

    // TODO: error handling
    // expects message to not have pre-allocated the data buffer
    if (message->data != NULL)
        return -1;

    // TODO: make sure message->data is freed
    // Memory is dynamically allocated here and must be freed once it is no longer needed
    message->data = malloc(expected_data_size);
    memcpy(message->data, &buffer[deserialized], expected_data_size);

    return expected_size;
}


// Helper function that serializes a RudpHeader
int serialize_header(RudpHeader* header, char* buffer, int buffer_len) {
    // TODO: error handling
    if (buffer_len < sizeof(*header))
        return -1;

    int i = 0;
    int serialized;

    serialized = serialize_int(header->seq_num, &buffer[i], buffer_len - i);
    if (serialized < 0)
        return serialized;
    else
        i += serialized;

    serialized = serialize_int(header->ack_num, &buffer[i], buffer_len - i);
    if (serialized < 0)
        return serialized;
    else
        i += serialized;

    serialized = serialize_int(header->data_size, &buffer[i], buffer_len - i);
    if (serialized < 0)
        return serialized;
    else
        i += serialized;

    return i;
}


// Helper function to serialize ints.
//
// This function stores ints in a big-endian format.
int serialize_int(int value, char* buffer, int buffer_len) {
    // TODO: error handling
    if (buffer_len < sizeof(value))
        return -1;

    assert(sizeof(value) == 4);

    buffer[0] = (value >> 24) & 0xFF;
    buffer[1] = (value >> 16) & 0xFF;
    buffer[2] = (value >> 8) & 0xFF;
    buffer[3] = value & 0xFF;
    return 4;
}


// Helper function to deserialize a RudpHeader
//
// Since the RudpHeader has a fixed size, this function does not need to dynamically allocate any memory
int deserialize_header(char* buffer, int buffer_len, RudpHeader* header) {
    // TODO: error handling
    if (buffer_len < sizeof(*header))
        return -1;

    int i = 0;
    int deserialized = 0;

    deserialized = deserialize_int(&buffer[i], buffer_len, &header->seq_num);
    // TODO: error handling
    if (deserialized < 0)
        return -1;
    i += deserialized;

    deserialized = deserialize_int(&buffer[i], buffer_len, &header->ack_num);
    // TODO: error handling
    if (deserialized < 0)
        return -1;
    i += deserialized;

    deserialized = deserialize_int(&buffer[i], buffer_len, &header->data_size);
    // TODO: error handling
    if (deserialized < 0)
        return -1;
    i += deserialized;

    return i;
}


// Helper function to deserialize an int, stored in big-endian format
int deserialize_int(char* buffer, int buffer_len, int* value) {
    int expected_int_size = 4;
    // TODO: error handling
    if (buffer_len < expected_int_size)
        return -1;

    assert(sizeof(int) == expected_int_size);

    *value =  ((unsigned char) buffer[0] << 24)
            | ((unsigned char) buffer[1] << 16)
            | ((unsigned char) buffer[2] << 8)
            |  (unsigned char) buffer[3];

    return expected_int_size;
}
