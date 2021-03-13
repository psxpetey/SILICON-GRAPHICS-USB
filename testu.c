#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#define TOKEN_SETUP  0x2D
#define TOKEN_IN     0x69
#define TOKEN_OUT    0xE1

typedef struct{
    uint32_t reply, info;
}td_t;

int main(){
    td_t td[8];
    uint32_t dev_address = 0;
    uint32_t packet_size = 8;
    uint32_t ls_device = 1; 
    uint32_t size = 8;
    
    int i = 1, t, sz = size, timeout;
    char setup_packet[8] = { 0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00 };
    char setup_packet2[8]= { 0x00, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    


    printf("GET 8\n"); 
    sz = 8;   
    setup_packet[6] = (char) size;
    td[0].reply = (ls_device ? (1<<26) : 0) | (3<<27) | (0x80 << 16);
    td[0].info = (7<<21) | ((dev_address & 0x7F)<<8) | TOKEN_SETUP;
 
    
    while ((sz > 0) && (i<9)) {
      td[i].reply = (ls_device ? (1<<26) : 0) | (3<<27) | (0x80 << 16);
      t = ((sz <= packet_size) ? sz : packet_size);
      td[i].info = ((t-1)<<21) | ((i & 1) ? (1<<19) : 0) | ((dev_address & 0x7F)<<8) | TOKEN_IN;
      sz -= t;
      i++;
    }

    td[i].reply = (ls_device ? (1<<26) : 0) | (3<<27) | (1<<24) | (0x80 << 16);
    td[i].info = (0x7FF<<21) | (1<<19) | ((dev_address & 0x7F)<<8) | TOKEN_OUT;
    i++; // for a total count


    for( t = 0; t < i; t++){
        printf("td[%d].st = 0x%8.8x\n", t, td[t].reply);
        printf("td[%d].tk = 0x%8.8x\n", t, td[t].info);
    } 
        
    for( t = 0; t < 4; t++){
        printf("0x%2.2x ", setup_packet[t]);
    }
    printf("\n");
    for( ; t < 8; t++){
        printf("0x%2.2x ", setup_packet[t]);
    }

    printf("\n\nSET ADDRESS\n");    
    dev_address = 1;
    i = 1;
    setup_packet2[2] = (char) dev_address;    
    td[0].reply = (ls_device ? (1<<26) : 0) | (3<<27) | (0x80 << 16);
    td[0].info = (7<<21) | (0<<8) | TOKEN_SETUP;
    td[1].reply = (ls_device ? (1<<26) : 0) | (3<<27) | (1<<24) | (0x80 << 16);
    td[1].info = (0x7FF<<21) | (1<<19) | (0<<8) | TOKEN_IN;
    
    for( t = 0; t < 2; t++){
        printf("td[%d].st = 0x%8.8x\n", t, td[t].reply);
        printf("td[%d].tk = 0x%8.8x\n", t, td[t].info);
    } 
    for( t = 0; t < 4; t++){
        printf("0x%2.2x ", setup_packet2[t]);
    }
    printf("\n");
    for( ; t < 8; t++){
        printf("0x%2.2x ", setup_packet2[t]);
    }

    
    
    
    
    printf("\n\nGET DESCRIPTOR\n");  
    dev_address = 1;
    sz = 18;
     i = 1; 
     setup_packet[6] = (char) size;
    td[0].reply = (ls_device ? (1<<26) : 0) | (3<<27) | (0x80 << 16);
    td[0].info = (7<<21) | ((dev_address & 0x7F)<<8) | TOKEN_SETUP;
 
    
    while ((sz > 0) && (i<9)) {
      td[i].reply = (ls_device ? (1<<26) : 0) | (3<<27) | (0x80 << 16);
      t = ((sz <= packet_size) ? sz : packet_size);
      td[i].info = ((t-1)<<21) | ((i & 1) ? (1<<19) : 0) | ((dev_address & 0x7F)<<8) | TOKEN_IN;
      sz -= t;
      i++;
    }

    td[i].reply = (ls_device ? (1<<26) : 0) | (3<<27) | (1<<24) | (0x80 << 16);
    td[i].info = (0x7FF<<21) | (1<<19) | ((dev_address & 0x7F)<<8) | TOKEN_OUT;
    i++; // for a total count


    for( t = 0; t < i; t++){
        printf("td[%d].st = 0x%8.8x\n", t, td[t].reply);
        printf("td[%d].tk = 0x%8.8x\n", t, td[t].info);
    } 
        
    for( t = 0; t < 4; t++){
        printf("0x%2.2x ", setup_packet[t]);
    }
    printf("\n");
    for( ; t < 8; t++){
        printf("0x%2.2x ", setup_packet[t]);
    }

    return 0;
}



