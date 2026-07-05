#include "protocol.h"
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

/* Helper: Send all bytes (blocks until complete) */
static int send_all(int sockfd, const void *buffer, size_t length)
{
    size_t total = 0;
    while (total < length) {
        ssize_t bytes = send(sockfd,
                            (const char *)buffer + total,
                            length - total,
                            0);
        if (bytes <= 0)
            return -1;
        total += bytes;
    }
    return 0;
}

/* Helper: Receive all bytes (blocks until complete) */
static int recv_all(int sockfd, void *buffer, size_t length)
{
    size_t total = 0;
    while (total < length) {
        ssize_t bytes = recv(sockfd,
                            (char *)buffer + total,
                            length - total,
                            0);
        if (bytes <= 0)
            return -1;
        total += bytes;
    }
    return 0;
}

/* CRC32 Checksum Calculation */
uint32_t calculate_crc32(const char *data, uint16_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    for (uint16_t i = 0; i < length; i++) {
        uint8_t byte = data[i];
        crc ^= byte;
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc ^ 0xFFFFFFFF;
}

/* Send Message */
int send_msg(int sockfd, Message *msg)
{
    uint8_t header[24];
    
    header[0] = PROTOCOL_VERSION;
    header[1] = msg->type;
    header[2] = msg->flags;
    header[3] = 0;  /* Reserved */
    
    uint16_t len = htons(msg->length);
    memcpy(header + 4, &len, 2);
    memcpy(header + 6, &(uint16_t){0}, 2);  /* Reserved */
    
    uint32_t msg_id = htonl(msg->message_id);
    memcpy(header + 8, &msg_id, 4);
    
    uint32_t ts = htonl(msg->timestamp);
    memcpy(header + 12, &ts, 4);
    
    msg->crc32 = calculate_crc32(msg->payload, msg->length);
    uint32_t crc = htonl(msg->crc32);
    memcpy(header + 16, &crc, 4);
    memcpy(header + 20, &(uint32_t){0}, 4);  /* Reserved */
    
    if (send_all(sockfd, header, 24) < 0)
        return -1;
    if (send_all(sockfd, msg->payload, msg->length) < 0)
        return -1;
    
    return 0;
}

/* Receive Message (Blocking) */
int recv_msg(int sockfd, Message *msg)
{
    uint8_t header[24];
    
    if (recv_all(sockfd, header, 24) < 0)
        return -1;
    
    msg->version = header[0];
    msg->type = header[1];
    msg->flags = header[2];
    
    memcpy(&msg->length, header + 4, 2);
    msg->length = ntohs(msg->length);
    
    if (msg->length > MAX_PAYLOAD_SIZE)
        return -1;
    
    uint32_t msg_id;
    memcpy(&msg_id, header + 8, 4);
    msg->message_id = ntohl(msg_id);
    
    uint32_t ts;
    memcpy(&ts, header + 12, 4);
    msg->timestamp = ntohl(ts);
    
    uint32_t crc;
    memcpy(&crc, header + 16, 4);
    msg->crc32 = ntohl(crc);
    
    if (recv_all(sockfd, msg->payload, msg->length) < 0)
        return -1;
    
    msg->payload[msg->length] = '\0';
    
    uint32_t calculated = calculate_crc32(msg->payload, msg->length);
    if (calculated != msg->crc32)
        return -1;
    
    return 0;
}

/* Receive Message for poll() */
int receive_msg_nonblocking(int sockfd, MessageBuffer *buf, Message *msg)
{
    /* Step 1: Fill header */
    if (buf->header_bytes < 24) {
        ssize_t n = recv(sockfd,
                        buf->header + buf->header_bytes,
                        24 - buf->header_bytes,
                        MSG_DONTWAIT);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return 0;  /* No data available, try again later */
            return -1;  /* Error */
        }
        if (n == 0)
            return -2;  /* Peer closed connection */
        
        buf->header_bytes += n;
        if (buf->header_bytes < 24)
            return 0;  /* Header incomplete */
    }
    
    /* Parse header to get payload size */
    if (buf->payload_expected == 0) {
        uint16_t len;
        memcpy(&len, buf->header + 4, 2);
        buf->payload_expected = ntohs(len);
        
        if (buf->payload_expected > MAX_PAYLOAD_SIZE)
            return -1;
    }
    
    /*  Fill payload */
    if (buf->payload_bytes < buf->payload_expected) {
        ssize_t n = recv(sockfd,
                        buf->payload + buf->payload_bytes,
                        buf->payload_expected - buf->payload_bytes,
                        MSG_DONTWAIT);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return 0;
            return -1;
        }
        if (n == 0)
            return -2;
        
        buf->payload_bytes += n;
        if (buf->payload_bytes < buf->payload_expected)
            return 0;  /* Payload incomplete */
    }
    
    /* Parse complete message */
    msg->version = buf->header[0];
    msg->type = buf->header[1];
    msg->flags = buf->header[2];
    
    memcpy(&msg->length, buf->header + 4, 2);
    msg->length = ntohs(msg->length);
    
    uint32_t msg_id;
    memcpy(&msg_id, buf->header + 8, 4);
    msg->message_id = ntohl(msg_id);
    
    uint32_t ts;
    memcpy(&ts, buf->header + 12, 4);
    msg->timestamp = ntohl(ts);
    
    uint32_t crc;
    memcpy(&crc, buf->header + 16, 4);
    msg->crc32 = ntohl(crc);
    
    memcpy(msg->payload, buf->payload, msg->length);
    msg->payload[msg->length] = '\0';
    
    /* Verify CRC32 */
    uint32_t calculated = calculate_crc32(msg->payload, msg->length);
    if (calculated != msg->crc32)
        return -1;
    
    /* Reset buffer for next message */
    buf->header_bytes = 0;
    buf->payload_bytes = 0;
    buf->payload_expected = 0;
    
    return 1;  /* Complete message received */
}