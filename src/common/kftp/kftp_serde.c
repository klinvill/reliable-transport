//
// Provides serialize and deserialize functions for KFTP headers
//
// We currently don't need to serialize or deserialize KFTP messages since the message only adds the data which is sent
// incrementally over multiple RUDP messages.
//

#include "kftp_serde.h"

#include "../reliable_udp/serde.h"


int serialize_kftp_header(KftpHeader* header, char* buffer, int buffer_len) {
    if (buffer_len < sizeof(*header)) {
        fprintf(stderr, "ERROR in serialize_kftp_header: buffer too small to hold header\n");
        return -1;
    }

    int i = 0;
    int serialized;

    serialized = serialize_int(header->data_size, &buffer[i], buffer_len - i);
    if (serialized < 0) {
        fprintf(stderr, "ERROR in serialize_kftp_header: error serializing data_size field");
        return serialized;
    }
    else
        i += serialized;

    return i;
}

int deserialize_kftp_header(char* buffer, int buffer_len, KftpHeader * header) {
    if (buffer_len < sizeof(*header)) {
        fprintf(stderr, "ERROR in deserialize_kftp_header: buffer too small to hold header\n");
        return -1;
    }

    int i = 0;
    int deserialized;

    deserialized = deserialize_int(&buffer[i], buffer_len, &header->data_size);
    if (deserialized < 0) {
        fprintf(stderr, "ERROR in serialize_kftp_header: error deserializing data_size field");
        return -1;
    }
    i += deserialized;

    return i;
}
