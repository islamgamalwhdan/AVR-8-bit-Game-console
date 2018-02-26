/*
 * sd.c
 *
 * Created: 5/15/2017 5:46:06 PM
 *  Author: Islam Gamal
 */ 

#include "spi.h"
#include "diskio.h"

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
	
	//1- Initialize SPI mode for MCU .
	
	spi_init(SPI_FOSC_32) ; 

	uint8_t response = 0xFF ;
   
   while( response != 0x01  && attempts++ < MAX_ITERATION_RESPONSE )
   {
	   
	   //2-  powering-up SD card
	  // Note: AT LEAST 74 clock cycles (aprox 10byte send = 80clock) are required prior to starting bus communication at CS high but here i use 128byte (1024 clock)
	   de_assert_CS() ;
	   
	   for( int i= 0 ; i<POWERUP_MAX ; i++)
	    spi_write(0xFF) ;
	
	 //3- Set SD card in SPI mode  
	  assert_CS() ;
	  response = SD_Send_Command(GO_IDLE_STATE , 0x00) ;
   }	
	if( response != 0x01 )
	{	
		return 0xFF ; // Failed operation .
	}
	
	//4- Activates the card’s initialization process.
	
	attempts = 0 ;
	while( response != 0x00 && attempts++ < MAX_ITERATION_RESPONSE )
	{
		response = SD_Send_Command(SEND_OP_COND , 0x00) ;
		
	}		
	if( response != 0x00 )
	{
		
		return STA_NOINIT ; // Failed operation .
	}
	
	//5- Check if this card is SD or MMC .
	
	response = SD_Send_Command(SD_APP_CMD , 0x00) ;
	if( response != 0x00 )
	{
		de_assert_CS() ;
		spi_set_speed(SPI_FOSC_2);
		return 0 ; // 0x02 indicate that it's MMc card not SD card .
	} 
	
	response = SD_Send_Command(SD_SEND_OP_CMD , 0x00) ;
	if( response != 0x00 )
	{
		de_assert_CS() ;
		spi_set_speed(SPI_FOSC_2);
		return 0 ; // 0x02 indicate that it's MMc card not SD card . 
	}
	
	de_assert_CS() ;
	spi_set_speed(SPI_FOSC_2);
	return 0 ; // No errors 
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
