//
// Created by Kirby Linvill on 1/29/21.
//

#include "kftp_serde.h"

#include "../reliable_udp/serde.h"


int serialize_kftp_header(KftpHeader* header, char* buffer, int buffer_len) {
    // TODO: error handling
    if (buffer_len < sizeof(*header))
        return -1;

    int i = 0;
    int serialized;

    serialized = serialize_int(header->data_size, &buffer[i], buffer_len - i);
    if (serialized < 0)
        return serialized;
    else
        i += serialized;

    return i;
}

int deserialize_kftp_header(char* buffer, int buffer_len, KftpHeader * header) {
    // TODO: error handling
    if (buffer_len < sizeof(*header))
        return -1;

    int i = 0;
    int deserialized = 0;

    deserialized = deserialize_int(&buffer[i], buffer_len, &header->data_size);
    // TODO: error handling
    if (deserialized < 0)
        return -1;
    i += deserialized;

    return i;
}
