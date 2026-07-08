#include "protocol.h"
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <time.h>

/**
 * Send all bytes over TLS or raw socket
 * Blocks until complete or error
 */
static int send_all(int sockfd, SSL *ssl, const void *buffer, size_t length)
{
    const char *data = (const char *)buffer;
    size_t total = 0;
    
    while (total < length) {
        int bytes;
        
        if (ssl != NULL) {
            /* Send via TLS */
            bytes = SSL_write(ssl, data + total, length - total);
            
            if (bytes <= 0) {
                int err = SSL_get_error(ssl, bytes);
                
                if (err == SSL_ERROR_WANT_WRITE) 
                {
        
                    
                 struct timespec ts = 
                 {
                  .tv_sec = 0,
                  .tv_nsec = 10000000   // 10 ms
                 };

                 nanosleep(&ts, NULL);
                    continue;
                }
                
                fprintf(stderr, "[PROTOCOL] SSL_write error: %d\n", err);
                return -1;
            }
        } else {
            /* Send via raw socket */
            bytes = send(sockfd, data + total, length - total, 0);
            
            if (bytes <= 0) {
                perror("[PROTOCOL] send");
                return -1;
            }
        }
        
        total += bytes;
    }
    
    return 0;
}

/**
 * Receive all bytes over TLS or raw socket
 * Blocks until complete or error
 */
static int recv_all(int sockfd, SSL *ssl, void *buffer, size_t length)
{
    char *data = (char *)buffer;
    size_t total = 0;
    
    while (total < length) {
        int bytes;
        
        if (ssl != NULL) {
            /* Receive via TLS */
            bytes = SSL_read(ssl, data + total, length - total);
            
            if (bytes <= 0) {
                int err = SSL_get_error(ssl, bytes);
                
                if (err == SSL_ERROR_ZERO_RETURN) {
                    /* Peer closed connection */
                    return -2;
                }
                
                if (err == SSL_ERROR_WANT_READ) {
                    /* Retry — this shouldn't happen in blocking mode but handle it */
                    sleep(10000);
                    continue;
                }
                
                fprintf(stderr, "[PROTOCOL] SSL_read error: %d\n", err);
                return -1;
            }
        } else {
            /* Receive via raw socket */
            bytes = recv(sockfd, data + total, length - total, 0);
            
            if (bytes <= 0) {
                if (bytes == 0) {
                    /* Connection closed */
                    return -2;
                }
                perror("[PROTOCOL] recv");
                return -1;
            }
        }
        
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

/**
 * Send Message over TLS or raw socket
 * Sends header (24 bytes) then payload
 */
int send_msg(int sockfd, SSL *ssl, Message *msg)
{
    if (msg == NULL || sockfd < 0) {
        fprintf(stderr, "[PROTOCOL] Invalid socket or message\n");
        return -1;
    }
    
    uint8_t header[24];
    
    /* Build header */
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
    
    /* Calculate CRC32 */
    msg->crc32 = calculate_crc32(msg->payload, msg->length);
    uint32_t crc = htonl(msg->crc32);
    memcpy(header + 16, &crc, 4);
    memcpy(header + 20, &(uint32_t){0}, 4);  /* Reserved */
    
    /* Send header */
    if (send_all(sockfd, ssl, header, 24) < 0)
        return -1;
    
    /* Send payload */
    if (send_all(sockfd, ssl, msg->payload, msg->length) < 0)
        return -1;
    
    return 0;
}

/**
 * Receive Message over TLS or raw socket
 * Receives header (24 bytes) then payload, validates CRC32
 */
int recv_msg(int sockfd, SSL *ssl, Message *msg)
{
    if (msg == NULL || sockfd < 0) {
        fprintf(stderr, "[PROTOCOL] Invalid socket or message\n");
        return -1;
    }
    
    uint8_t header[24];
    
    /* Receive header */
    int ret = recv_all(sockfd, ssl, header, 24);
    if (ret < 0)
        return ret;  /* -1 = error, -2 = connection closed */
    
    /* Parse header */
    msg->version = header[0];
    msg->type = header[1];
    msg->flags = header[2];
    
    memcpy(&msg->length, header + 4, 2);
    msg->length = ntohs(msg->length);
    
    /* Validate payload size */
    if (msg->length > MAX_PAYLOAD_SIZE) {
        fprintf(stderr, "[PROTOCOL] Payload size %u exceeds maximum %d\n",
                msg->length, MAX_PAYLOAD_SIZE);
        return -1;
    }
    
    uint32_t msg_id;
    memcpy(&msg_id, header + 8, 4);
    msg->message_id = ntohl(msg_id);
    
    uint32_t ts;
    memcpy(&ts, header + 12, 4);
    msg->timestamp = ntohl(ts);
    
    uint32_t crc;
    memcpy(&crc, header + 16, 4);
    msg->crc32 = ntohl(crc);
    
    /* Receive payload */
    if (msg->length > 0) {
        ret = recv_all(sockfd, ssl, msg->payload, msg->length);
        if (ret < 0)
            return ret;
    }
    
    msg->payload[msg->length] = '\0';
    
    /* Verify CRC32 */
    uint32_t calculated = calculate_crc32(msg->payload, msg->length);
    if (calculated != msg->crc32) {
        fprintf(stderr, "[PROTOCOL] CRC32 mismatch (expected %u, got %u)\n",
                msg->crc32, calculated);
        return -1;
    }
    
    return 0;
}

/**
 * Non-blocking message receive for use with poll()
 * Works with both TLS and raw sockets
 * 
 * Returns:  1 = complete message received
 *           0 = incomplete message, try again later
 *          -1 = protocol error
 *          -2 = peer closed connection
 */
int receive_msg_nonblocking(int sockfd, SSL *ssl, MessageBuffer *buf, Message *msg)
{
    int n;
    
    /* Step 1: Receive header (24 bytes) */
    if (buf->header_bytes < 24) {
        if (ssl != NULL) {
            /* Non-blocking TLS receive */
            SSL_set_connect_state(ssl);  /* Ensure non-blocking mode */
            n = SSL_read(ssl, buf->header + buf->header_bytes, 24 - buf->header_bytes);
            
            if (n <= 0) {
                int err = SSL_get_error(ssl, n);
                
                if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                    return 0;  /* No data yet, try again later */
                }
                
                if (err == SSL_ERROR_ZERO_RETURN) {
                    return -2;  /* Peer closed connection */
                }
                
                fprintf(stderr, "[PROTOCOL] SSL_read error during header: %d\n", err);
                return -1;
            }
        } else {
            /* Non-blocking raw socket receive */
            n = recv(sockfd, buf->header + buf->header_bytes, 24 - buf->header_bytes, MSG_DONTWAIT);
            
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return 0;  /* No data available yet */
                }
                return -1;  /* Error */
            }
            
            if (n == 0) {
                return -2;  /* Peer closed connection */
            }
        }
        
        buf->header_bytes += n;
        if (buf->header_bytes < 24) {
            return 0;  /* Header still incomplete */
        }
    }
    
    /* Step 2: Parse header to get payload size */
    if (buf->payload_expected == 0) {
        uint16_t len;
        memcpy(&len, buf->header + 4, 2);
        buf->payload_expected = ntohs(len);
        
        if (buf->payload_expected > MAX_PAYLOAD_SIZE) {
            fprintf(stderr, "[PROTOCOL] Payload size %u exceeds maximum\n",
                    buf->payload_expected);
            return -1;
        }
    }
    
    /* Step 3: Receive payload */
    if (buf->payload_bytes < buf->payload_expected) {
        if (ssl != NULL) {
            /* Non-blocking TLS receive */
            n = SSL_read(ssl, buf->payload + buf->payload_bytes,
                        buf->payload_expected - buf->payload_bytes);
            
            if (n <= 0) {
                int err = SSL_get_error(ssl, n);
                
                if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
                    return 0;  /* Try again later */
                }
                
                if (err == SSL_ERROR_ZERO_RETURN) {
                    return -2;
                }
                
                fprintf(stderr, "[PROTOCOL] SSL_read error during payload: %d\n", err);
                return -1;
            }
        } else {
            /* Non-blocking raw socket receive */
            n = recv(sockfd, buf->payload + buf->payload_bytes,
                    buf->payload_expected - buf->payload_bytes, MSG_DONTWAIT);
            
            if (n < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    return 0;
                }
                return -1;
            }
            
            if (n == 0) {
                return -2;
            }
        }
        
        buf->payload_bytes += n;
        if (buf->payload_bytes < buf->payload_expected) {
            return 0;  /* Payload still incomplete */
        }
    }
    
    /* Step 4: Parse complete message */
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
    if (calculated != msg->crc32) {
        fprintf(stderr, "[PROTOCOL] CRC32 mismatch\n");
        return -1;
    }
    
    /* Reset buffer for next message */
    buf->header_bytes = 0;
    buf->payload_bytes = 0;
    buf->payload_expected = 0;
    
    return 1;  /* Complete message received */
}