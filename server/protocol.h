// Implementing a "Binary Protocol" 
/* its better to send messages in raw bytes instead of human
 readable form, so we implement a binary protocol.
 its efficient , fast and provides type safety.  */

#ifndef PROTOCOL_H
#define PROTOCOL_H
#include<stdint.h>
#include<time.h>
#define MAX_PAYLOAD_SIZE 1024
#define PROTOCOL_VERSION 1

// message types
// replacing numbers using meaningful names using enum.
typedef enum
{
    MSG_CONNECTION = 1,
    MSG_CHAT = 2,
    MSG_PRIVATE_MSG = 3,
    MSG_WHO_COMMAND = 4,
    MSG_DISCONNECT = 5,
    MSG_SERVER = 6,
    MSG_ERROR = 7
}MessageType;

// flags:
#define MSG_FLAG_ACK_REQUIRED 0x01 // to send ACK back and forth
#define MSG_FLAG_COMPRESSED 0x02   // compressing/decompressing before sending/receiving

// binary messages
typedef struct
{
    uint8_t version;
    uint8_t type;
    uint8_t flags;
    uint8_t _reserved;
    uint16_t length;
    uint16_t _reserved2;
    uint32_t message_id;
    uint32_t timestamp;
    uint32_t crc32;
    char payload[MAX_PAYLOAD_SIZE];
}Message;

// serialization , declaring the functions that i am gonna use 
 int send_msg(int socketfd,Message *msg); // to send messages.
 int recv_msg(int socketfd,Message *msg); // to recieve messages.

// custom header struct
typedef struct
{
   uint8_t header[24];         // fixed header size;
   uint8_t header_bytes;       // bytes recieved till now
   char payload[MAX_PAYLOAD_SIZE];
   uint16_t payload_bytes;     // bytes recieved till now
   uint16_t payload_expected;  // expected payload size
}MessageBuffer;

// func() for event loop
int receive_msg_nonblocking(int socketfd,MessageBuffer *buf,Message *msg);

// CRC32 algorithm function
uint32_t calculate_crc32(const char *data,uint16_t length);

#endif