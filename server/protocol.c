#include "protocol.h"
#include<stdio.h>
#include<string.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<err.h>

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