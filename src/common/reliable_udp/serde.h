//
// Provides serialize and deserialize functions for RUDP headers and messages
//

#ifndef UDP_RELIABLE_UDP_SERDE_H
#define UDP_RELIABLE_UDP_SERDE_H

#include "types.h"

// Serializes (converts into bytes) an RudpMessage
//
// Returns the number of bytes serialized on success, returns a negative int on failure
int serialize(RudpMessage* message, char* buffer, int buffer_len);

// Deserializes (converts from bytes) an RudpMessage
//
// Dynamically allocates a data buffer that must be freed once it is no longer needed
//
// Returns the number of bytes deserialized on success, returns a negative int on failure
int deserialize(char* buffer, int buffer_len, RudpMessage* message);


// Remaining methods intended primarily for internal use
int serialize_header(RudpHeader* header, char* buffer, int buffer_len);
int serialize_int(int number, char* buffer, int buffer_len);

int deserialize_header(char* buffer, int buffer_len, RudpHeader* header);
int deserialize_int(char* buffer, int buffer_len, int* value);

#endif //UDP_RELIABLE_UDP_SERDE_H
