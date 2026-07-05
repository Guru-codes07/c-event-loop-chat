#include "protocol.h"
#include<stdio.h>
#include<string.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<errno.h>

// initialising the CRC32 func() 
// CRC = cyclic redundancy check algorithm 
uint32_t calculate_crc32(const char *data,uint16_t length)
{
  // declaring the crc variable
  uint32_t crc = 0xFFFFFFFF;
  for(uint16_t i = 0;i<length;i++)
  {
     uint8_t byte = data[i];
      crc ^= byte;
      for (int  j = 0;j<8;j++)
      {
        if(crc & 1)
        {
            crc = (crc>>1) ^ 0xEDB88320;
        }
        else
        {
            crc >>= 1;
        }
    }
  }
      return crc ^ 0xFFFFFFFF;
}

// send msg func()
int send_msg(int socketfd,Message *msg)
{
   uint8_t header[24];
   header[0] = PROTOCOL_VERSION;
   header[1] = msg->type;
   header[2] = msg->flags;
   header[3] = 0;          // this is reserved 

   uint16_t len = htons(msg->length);
   memcpy(header+4,&len,2);
   memcpy(header+6,&(uint16_t){0},2); // this is reserved
   
   uint32_t msg_id = hton1(msg->message_id);
   memcpy(header+8,&msg_id,4);

   uint32_t ts = hton1(msg->timestamp);
   memcpy(header+12,&ts,4);

   msg->crc32 = calculate_crc32(msg->payload,msg->length);
   uint32_t crc = hton1(msg->crc32);
   memcpy(header+16,&crc,4);
   memcpy(header+20,&(uint32_t){0},4); // this is reserved
   
   if(send_all(socketfd,header,24)<0)
   {
    return -1;
   }
   if(send_all(socketfd,msg->payload,msg->length)<0)
   {
    return -1;
   }
  
   return 0;
}

 int receive_msg_nonblocking(int socketfd,MessageBuffer *buff,Message *msg)
 {
    // filling the header
    if(buff->header_bytes<24)
    {
        ssize_t n = recv(socketfd,buff->header + buff->header_bytes, 24-buff->header_bytes,MSG_DONTWAIT);
        if(n<0)
        {
            if(errno == EAGAIN || errno ==EWOULDBLOCK)
            return 0; // if no data avaialable
            return -1; // if error 
        }
        if(n==0)
        return -2; // error again
    
        buff->header_bytes += n;
        if(buff->header_bytes<24)
        return 0;
    }
   
    // to get payload size 
    if(buff->payload_expected == 0)
    {
        uint16_t len;
        memcpy(&len,buff->payload_expected+4,2);
        buff->payload_expected = ntohs(len);

        if(buff->payload_expected>MAX_PAYLOAD_SIZE)
        return -1;
    }
    
    // now to fill the payload
    if(buff->payload_bytes<buff->payload_expected)
    {
        ssize_t n = recv(socketfd,buff->payload + buff->payload_bytes,buff->payload_expected - buff->payload_bytes,MSG_DONTWAIT);
        if(n<0)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            return 0; // if no data available
            return-1; // if error
        }
        if(n==0)
        return -2;

        if(buff->payload_bytes<buff->payload_expected)
        return 0;
    }

    // message received and parsing 
    msg->version = buff->header[0];
    msg->type = buff->header[1];
    msg->flags = buff->header[2];

    memcpy(&msg->length,buff->header+4,2);
    msg->length = ntohs(msg->length);
    
    uint32_t msg_id;
    memcpy(&msg_id,buff->header+8,4);
    msg->message_id = ntoh1(msg_id);

    uint32_t ts;
    memcpy(&ts,buff->header+12,4);
    msg->timestamp = ntoh1(ts);

    uint32_t crc;
    memcpy(&crc,buff->header+16,4);
    msg->crc32 = ntoh1(crc);

    memcpy(msg->payload,buff->payload,msg->length);
    msg->payload[msg->length] = '\0';

    // verify the crc again
    uint32_t calculate = calculate_crc32(msg->payload,msg->length);
    if(calculate!=msg->crc32)
    return -1; // error 

    // reset the buffer for the next msg
    buff->header_bytes = 0;
    buff->payload_bytes = 0;
    buff->payload_expected = 0;

    return 1;
}