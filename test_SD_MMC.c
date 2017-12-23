/*
 * test_SD_MMC.c
 *
 * Created: 5/15/2017 5:10:00 AM
 *  Author: ahmed
 */ 

#define F_CPU 20000000UL
#include <stdint.h>
#include <string.h>

#include <avr/io.h>


#include "sd.h"
#include "uart.h"

uint8_t buffer[50] ;
int main(void)
{
	
    int response ;
	uint8_t mount = 0;
	
	DDRA = 0 ;
	PORTA = 0xFF ;
	DDRC = 0xFF ;
   Uart_init(9600) ;
   mount = SD_mount() ;
  
  uint8_t Tbuffer[512] ;
  uint8_t Rbuffer[512] ;
  
  for(int i = 0 ; i<256 ; i++)
  {
	  Tbuffer[i] = i ; 
  }

  while(1)
  {
	 if( !(PINA & (1<<PA0) ) && (mount == SD_CARD || mount == MMC_CARD ) )
	 {
		 SD_Write_Sector(0 , Tbuffer) ;
		 
		 Uart_Transimit_String("\n512 byte sent to SD/MMC card") ;
		 
		 
		  while( !(PINA & (1<<PA0) ) ) ;
	 }//if
	 
	 if( !(PINA & (1<<PA1) )  && (mount == SD_CARD || mount == MMC_CARD ) ) 
	 {
		 SD_Read_Sector(0 ,Rbuffer ) ;
		 
		 sprintf(buffer , "\nData in file is 0x%X", Rbuffer[0x05]) ;
		 Uart_Transimit_String(buffer) ;
		 
		  while( !(PINA & (1<<PA1) ) ) ;
	 }//if
	  
  }	//while   
}