#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <msp430.h>
#include <ctl_api.h>
#include <terminal.h>
#include <ARCbus.h>
#include <UCA1_uart.h>
#include <Error.h>
#include <commandLib.h>
#include "Proxy_errors.h"


//change the stored I2C address. this does not change the address for the I2C peripheral
int addrCmd(char **argv,unsigned short argc){
  //unsigned long addr;
  //unsigned char tmp;
  //char *end;
  int en;
  unsigned char addr;
  if(argc==0){
    if(*(unsigned char*)0x01000!=((~UCGCEN)&UCB0I2COA)){
        printf("Flash I2C address = 0x%02X\r\nPeripheral I2C address = 0x%02X\r\n",*(unsigned char*)0x01000,((~UCGCEN)&UCB0I2COA));
    }else{
      printf("I2C address = 0x%02X\r\n",*(unsigned char*)0x01000);
    }
    return 0;
  }
  if(argc>1){
    printf("Error : too many arguments\r\n");
    return 1;
  }
  addr=getI2C_addr(argv[1],1,busAddrSym);
  if(addr==0xFF){
    return 1;
  }
  //erase address section
  en=ctl_global_interrupts_set(0);
  //first disable watchdog
  WDT_STOP();
  //unlock flash memory
  FCTL3=FWKEY;
  //setup flash for erase
  FCTL1=FWKEY|ERASE;
  //dummy write to indicate which segment to erase
  *((char*)0x01000)=0;
  //enable writing
  FCTL1=FWKEY|WRT;
  //write address
  *((char*)0x01000)=addr;
  //disable writing
  FCTL1=FWKEY;
  //lock flash
  FCTL3=FWKEY|LOCK;
  ctl_global_interrupts_set(en);
  //Kick WDT to restart it
  //WDT_KICK();
  //print out message
  printf("I2C Address Changed. Changes will not take effect until after reset.\r\n");
  return 0;
}

int printCmd(char **argv,unsigned short argc){
  unsigned char buff[40],*ptr,id;
  unsigned char addr;
  unsigned short len;
  int i,j,k;
  //check number of arguments
  if(argc<2){
    printf("Error : too few arguments.\r\n");
    return 1;
  }
  //get address
  addr=getI2C_addr(argv[1],0,busAddrSym);
  if(addr==0xFF){
    return 1;
  }
  //setup packet 
  ptr=BUS_cmd_init(buff,6);
  //coppy strings into buffer for sending
  for(i=2,k=0;i<=argc && k<sizeof(buff);i++){
    j=0;
    while(argv[i][j]!=0){
      ptr[k++]=argv[i][j++];
    }
    ptr[k++]=' ';
  }
  //get length
  len=k;
  //TESTING: set pin high
  P8OUT|=BIT0;
  //send command
  BUS_cmd_tx(addr,buff,len,0,BUS_I2C_SEND_FOREGROUND);
  //TESTING: set pin low
  P8OUT&=~BIT0;
  return 0;
}

int tstCmd(char **argv,unsigned short argc){
  unsigned char buff[40],*ptr,*end;
  unsigned char addr;
  unsigned short len;
  int i,j,k;
  //check number of arguments
  if(argc<2){
    printf("Error : too few arguments.\r\n");
    return 1;
  }
  if(argc>2){
    printf("Error : too many arguments.\r\n");
    return 1;
  }
  //get address
  addr=getI2C_addr(argv[1],0,busAddrSym);
  len = atoi(argv[2]);
  /*if(len<0){
    printf("Error : bad length");
    return 2;
  }*/
  //setup packet 
  ptr=BUS_cmd_init(buff,7);
  //fill packet with dummy data
  for(i=0;i<len;i++){
    ptr[i]=i;
  }
  //TESTING: set pin high
  P8OUT|=BIT0;
  //send command
  BUS_cmd_tx(addr,buff,len,0,BUS_I2C_SEND_FOREGROUND);
  //TESTING: wait for transaction to fully complete
  while(UCB0STAT&UCBBUSY);
  //TESTING: set pin low
  P8OUT&=~BIT0;
  return 0;
}

int asyncCmd(char **argv,unsigned short argc){
   char c;
   int err;
   CTL_EVENT_SET_t e=0,evt;
   unsigned char addr;
   if(argc>0){
    printf("Error : %s takes no arguments\r\n",argv[0]);
    return -1;
  }
  if(err=async_close()){
    printf("\r\nError : async_close() failed : %s\r\n",BUS_error_str(err));
  }
}

const char spamdat[]="This is a spam test\r\n";

int spamCmd(char **argv,unsigned short argc){
  unsigned int n,i,j;
  if(argc<1 || argc==0){
    printf("Error : %s takes only one argument but %i given\r\n",argv[0],argc);
    return -1;
  }
  n=atoi(argv[1]);
  for(i=0,j=0;i<n;i++,j++){
    if(j>=sizeof(spamdat)){
      j=0;
    }
    async_TxChar(spamdat[j]);
  }
  //pause to let chars clear
  ctl_timeout_wait(ctl_get_current_time()+100);
  //print message
  printf("\r\nSpaming complete %u chars sent\r\n",n);
  return 0;
}

int incCmd(char **argv,unsigned short argc){
  unsigned int n,i,c;
  if(argc<1 || argc==0){
    printf("Error : %s takes only one argument but %i given\r\n",argv[0],argc);
    return -1;
  }
  n=atoi(argv[1]);
  for(i=0;i<n;i++){
    c+=printf("%u \r\n",i);
  }
  //pause to let chars clear
  ctl_timeout_wait(ctl_get_current_time()+100);
  //print message
  printf("\r\nSpaming complete\r\n%u chars printed\r\n",c);
  return 0;
}

int replayCmd(char **argv,unsigned short argc){
  error_log_replay();
  return 0;
}

//table of commands with help
const CMD_SPEC cmd_tbl[]={{"help"," [command]\r\n\t""get a list of commands or help on a spesific command.",helpCmd},
                         CTL_COMMANDS,ARC_COMMANDS,ERROR_COMMANDS,
                         {"addr"," [addr]\r\n\t""Get/Set I2C address.",addrCmd},
                         {"print"," addr str1 [[str2] ... ]\r\n\t""Send a string to addr.",printCmd},
                         {"tst"," addr len\r\n\t""Send test data to addr.",tstCmd},
                         {"async","\r\n\t""Close async connection.",asyncCmd},
                         {"exit","\r\n\t""Close async connection.",asyncCmd},
                         {"spam","n\r\n\t""Spam the terminal with n chars",spamCmd},
                         {"inc","n\r\n\t""Spam the by printing numbers from 0 to n-1",incCmd},
                         //end of list
                         {NULL,NULL,NULL}};
