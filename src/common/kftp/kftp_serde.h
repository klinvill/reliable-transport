//
// Provides serialize and deserialize functions for KFTP headers
//
// We currently don't need to serialize or deserialize KFTP messages since the message only adds the data which is sent
// incrementally over multiple RUDP messages.
//

#ifndef UDP_KFTP_SERDE_H
#define UDP_KFTP_SERDE_H

#include "kftp.h"

// Serializes (converts into bytes) a KftpHeader
//
// Returns the number of bytes serialized on success, returns a negative int on failure
int serialize_kftp_header(KftpHeader* header, char* buffer, int buffer_len);

// Deserializes (converts from bytes) a KftpHeader
//
// Returns the number of bytes deserialized on success, returns a negative int on failure
int deserialize_kftp_header(char* buffer, int buffer_len, KftpHeader* header);

#endif //UDP_KFTP_SERDE_H
