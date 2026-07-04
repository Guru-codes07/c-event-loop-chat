#include "protocol.h"
#include<stdio.h>
#include<string.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<err.h>

// initialising the CRC32 func() 
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

}