/*-------------------------------------------------------------------------*/
/* PFF - Low level disk control module for AVR            (C)ChaN, 2010    */
/*-------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>

#include <avr/io.h>			/* Device include file */
#include <avr/delay.h> 

#include "spi.h"
#include "pff.h"
#include "diskio.h"
#include "uart.h"

/*========== Constants ==========================*/

#define READ_DATATOKEN_ITERATION_RESPONSE 256
#define SD_WRITE_SECTOR_LIMIT             128
#define SECTOR_SIZE                       512

#define MAX_ITERATION_RESPONSE            256
#define POWERUP_MAX                       256
#define GO_IDLE_STATE          ( 0x00 + 0x40 )   // To make SD card go to SPI mode . 
#define SEND_OP_COND           ( 0x01 + 0x40 )   // Activates the card’s initialization process
#define SD_READ_SECTOR_CMD     ( 0x11 + 0x40 ) 
#define SD_WRITE_SECOTR_CMD    ( 0x18 + 0x40 )
#define SD_APP_CMD             ( 0x37 + 0x40 )
#define SD_SEND_OP_CMD         ( 0x29 + 0x40 )

//Responses values from SD Data sheet

#define READ_RESPONSE_OK        0x00
#define WRITE_RESPONSE_OK       0x00
#define WRITE_RESPONSE_ACCEPTED 0x05
#define WRITE_CRC_REJECTED      0x0B
#define WRITE_ERROR_REJECTED    0x0D

#define SD_CARD   0x01
#define MMC_CARD  0x02

#define ENABLE  1
#define DISABLE 0
#define SD_DEBUG DISABLE

/*-----------------------------------------------------------------------*/
/* Send a command packet to MMC                                          */
/*-----------------------------------------------------------------------*/
#if (SD_DEBUG == ENABLE)
uint8_t buffer[101] ;  // buffer for debug
#endif

int cmd_iterations = 0 ; 

static uint8_t SD_Send_Command(uint8_t command , uint32_t address) 
{
	
	// Send 6 byte command format [ 1byte command - 4 bytes address - 1byte CRC ] . 
	
	spi_write(command) ;
	
	spi_write((uint32_t)address >> 24)  ;
	spi_write((uint32_t)address >> 16) ; 
	spi_write( address >> 8) ; 
	spi_write(address) ;
	
	spi_write(0x95) ;
	
	// Wait for response
	
	uint8_t response = 0xFF ;
	
	for( int i = 0 ; i< MAX_ITERATION_RESPONSE ; i++)
	{
		response = spi_read(0xFF) ;
		cmd_iterations = i ;
		if(response !=0xFF)
		{
			spi_write(0xFF) ;
			return response ;
		}
	}
	
	   return 0xFF ;        // If function fails then return 0xFF  
}

/*--------------------------------------------------------------------------

   Public Functions

---------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------*/
/* Initialize Disk Drive                                                 */
/*-----------------------------------------------------------------------*/


DSTATUS disk_initialize(void) 
{
	uint16_t attempts = 0 ;
	DSTATUS ret = 0 ;
	//1- Initialize SPI mode for MCU .
	
	spi_init(SPI_FOSC_16 , MASTER) ;
	_delay_ms(100) ;  
	//spi_set_speed(SPI_FOSC_2) ; // Set to max speed .
	
	#if (SD_DEBUG == ENABLE)	
		Uart_init(9600,0);
		_delay_ms(100) ;
	#endif	
	
	#if (SD_DEBUG == ENABLE)
	    Uart_Transimit_String("\nmounting SD card...") ;
	#endif   
	
	uint8_t response = 0xFF ;
   
   while( response != 0x01  && attempts++ < MAX_ITERATION_RESPONSE )
   {
	   
	   //2-  powering-up SD card
	  // Note: AT LEAST 74 clock cycles (aprox 10byte send = 80clock) are required prior to starting bus communication at CS high but here i use 128byte (1024 clock)
	   de_assert_CS() ;
	   
	   for( int i= 0 ; i<POWERUP_MAX ; i++)
	    spi_write(0xFF) ;
	
	 //3- Set SD card in SPI mode 
	 
	 #if (SD_DEBUG == ENABLE)
		Uart_Transimit_String("\nTrying to put SD card in SPI mode...") ;
	#endif	
	 
	  assert_CS() ;
	  response = SD_Send_Command(GO_IDLE_STATE , 0x00) ;
		
		#if (SD_DEBUG == ENABLE)
		sprintf(buffer , "\nNo of iterations = %d" , cmd_iterations ) ;
		Uart_Transimit_String(buffer) ;
		
		sprintf(buffer , "\nNo of attempts = %d" , attempts);
		Uart_Transimit_String(buffer) ;
		
		sprintf(buffer , "\nResponse = 0x%X" , response) ;
		Uart_Transimit_String(buffer) ;
		#endif
   }	
	if( response != 0x01 )
	{
		#if (SD_DEBUG == ENABLE)
		    Uart_Transimit_String("\nFailed to respond from SD card!!") ;
		#endif
		ret = 0xFF ;
		//return 0xFF ; // Failed operation .
	}
	
	//4- Activates the card’s initialization process.
	
	#if (SD_DEBUG == ENABLE)	
		Uart_Transimit_String("\ninitialization process..") ;
	#endif
	
	attempts = 0 ;
	while( response != 0x00 && attempts++ < MAX_ITERATION_RESPONSE )
	{
		response = SD_Send_Command(SEND_OP_COND , 0x00) ;
		
		#if (SD_DEBUG == ENABLE)
		sprintf(buffer , "\nNo of iterations = %d" , cmd_iterations ) ;
		Uart_Transimit_String(buffer) ;
		
		sprintf(buffer , "\nNo of attempts = %d" , attempts);
		Uart_Transimit_String(buffer) ;
		
		sprintf(buffer , "\nResponse = 0x%X" , response) ;
		Uart_Transimit_String(buffer) ;
		#endif
	}		
	if( response != 0x00 )
	{
		#if (SD_DEBUG == ENABLE)
		    Uart_Transimit_String("\nInitialization process Failed.!!") ;
		#endif
		ret = 0xFF ;
		//return 0xFF ; // Failed operation .
	}
	
	//5- Check if this card is SD or MMC .
	
	#if (SD_DEBUG == ENABLE)	
		Uart_Transimit_String("\nChecking if it's SD (Not MMC)..") ;
	#endif

	response = SD_Send_Command(SD_APP_CMD , 0x00) ;
	if( response != 0x00 )
	{
		#if (SD_DEBUG == ENABLE)
		     Uart_Transimit_String("\nIts MMC NOT SD CARD !!\nMMC card mounted successfully") ;
		#endif
		de_assert_CS() ;
		//return 0x02 ; // 0x02 indicate that it's MMc card not SD card .
	} 
	
	response = SD_Send_Command(SD_SEND_OP_CMD , 0x00) ;
	if( response != 0x00 )
	{
		#if (SD_DEBUG == ENABLE)
		    Uart_Transimit_String("\nIts MMC NOT SD CARD !!\nMMC card mounted successfully") ;
		#endif
		de_assert_CS() ;
		//return 0x02 ; // 0x02 indicate that it's MMc card not SD card . 
	}
	
	
	#if (SD_DEBUG == ENABLE)	
		Uart_Transimit_String("\nSD card mounted successfully") ;
	#endif
	de_assert_CS() ;
	
	return ret ; // No errors  & 0x01 idicate that it's SD card not MMC Card .
}





/*-----------------------------------------------------------------------*/
/* Read partial sector                                                   */
/*-----------------------------------------------------------------------*/

DRESULT disk_readp (
	BYTE *buff,		/* Pointer to the read buffer (NULL:Read bytes are forwarded to the stream) */
	DWORD sector,	/* Sector number (LBA) */
	UINT offset,	/* Byte offset to read from (0..511) */
	UINT count		/* Number of bytes to read (ofs + cnt mus be <= 512) */
)
{
	DRESULT res = RES_OK ;
	
	assert_CS() ;
	uint8_t response = SD_Send_Command(SD_READ_SECTOR_CMD , ((uint32_t)sector) << 9U) ;
	
	if(response != READ_RESPONSE_OK )
	  res = RES_ERROR;  // Read Failed
	
	// 2-Wait for data token response from SD card
      uint16_t i  ;
	for(  i = 0 ; i <READ_DATATOKEN_ITERATION_RESPONSE ; i++ )
	{
		response = spi_read(0xFF) ;
		
		if (response == 0xFE) 
		   break ;  
	}
	
	if( response != 0xFE )
	   {
		 	
		  res  =RES_ERROR  ;  // Means failed operation .
	   }
       uint16_t final_discard = 512 - (offset+count) ;
	   // Discard bytes from sector_offset to offset 

       while(offset--) spi_read(0xFF) ;
	   
	   //3- Start receive 512 byte data from SD card .
	   
	   for( i = 0 ; i< count ; i++ )
	   {
		   buff[i] = spi_read(0xFF) ;
	   }
	   
	    //Discard bytes from (sector_offset + offset) to 512

	   while(final_discard --) spi_read(0xFF) ;

	   // 4-Receive 16-bit CRC and we can discard it 
	   spi_read(0xFF) ;
	   spi_read(0xFF) ;
	   
	   //5- Send 8bit 0xFF to finish Read operation .
	   
	   spi_write(0xFF) ;
	   
	   //6- De-assert chip
	   de_assert_CS() ;
	   
	   return res ; // means no errors
}