/*
 * sd.c
 *
 * Created: 5/15/2017 5:46:06 PM
 *  Author: Islam Gamal
 */ 

#include <string.h>  // sprintf usage .

#include <avr/delay.h> 

#include "spi.h"
#include "sd.h"
#include "uart.h"

#if (SD_DEBUG == ENABLE)
uint8_t buffer[101] ;  // buffer for debug
#endif

int cmd_iterations = 0 ; 

uint8_t SD_Send_Command(uint8_t command , uint32_t address) 
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

uint8_t SD_mount(void) 
{
	uint16_t attempts = 0 ;
	
	//1- Initialize SPI mode for MCU .
	
	spi_init(SPI_FOSC_16) ;
	_delay_ms(100) ;  
	//spi_set_speed(SPI_FOSC_2) ; // Set to max speed .
	
	#if (SD_DEBUG == ENABLE)	
		Uart_init(9600);
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
		
		return 0xFF ; // Failed operation .
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
		
		return 0xFF ; // Failed operation .
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
		return 0x02 ; // 0x02 indicate that it's MMc card not SD card .
	} 
	
	response = SD_Send_Command(SD_SEND_OP_CMD , 0x00) ;
	if( response != 0x00 )
	{
		#if (SD_DEBUG == ENABLE)
		    Uart_Transimit_String("\nIts MMC NOT SD CARD !!\nMMC card mounted successfully") ;
		#endif
		de_assert_CS() ;
		return 0x02 ; // 0x02 indicate that it's MMc card not SD card . 
	}
	
	
	#if (SD_DEBUG == ENABLE)	
		Uart_Transimit_String("\nSD card mounted successfully") ;
	#endif
	de_assert_CS() ;
	
	return 0x01 ; // No errors  & 0x01 idicate that it's SD card not MMC Card .
}

uint8_t SD_unmount(void)
{
	#if (SD_DEBUG == ENABLE)	
		Uart_Transimit_String("\nSD card unmounted.") ;
	#endif
	
	de_assert_CS() ;
	
	return 0 ;
}

uint8_t SD_Read_Sector( uint32_t sector_offset , uint8_t *recv_buffer ) 
{
	// 1- Send command to SD/MMC card 
	
	assert_CS() ;
	uint8_t response = SD_Send_Command(SD_READ_SECTOR_CMD , ((uint32_t)sector_offset) << 9U) ;
	
	if(response != READ_RESPONSE_OK )
	  return response ;  // Read Failed
	
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
		 	
		  return 0xFF ;  // Means failed operation .
	   }
	   
	   //3- Start receive 512 byte data from SD card .
	   
	   for( i = 0 ; i< SECTOR_SIZE ; i++ )
	   {
		   recv_buffer[i] = spi_read(0xFF) ;
	   }
	   
	   // 4-Receive 16-bit CRC and we can discard it 
	   spi_read(0xFF) ;
	   spi_read(0xFF) ;
	   
	   //5- Send 8bit 0xFF to finish Read operation .
	   
	   spi_write(0xFF) ;
	   
	   //6- De-assert chip
	   de_assert_CS() ;
	   
	   return 0 ; // means no errors
}


uint8_t SD_Write_Sector( uint32_t sector_offset , uint8_t *trans_buffer )
{
	// 1- Send write command to SD/MMC card 
	
	assert_CS() ;
	uint8_t response = SD_Send_Command(SD_WRITE_SECOTR_CMD , sector_offset << 9) ;
	
	if(response  != WRITE_RESPONSE_OK )
	  return response ;  // Read Failed
	  
	  //2-Send 8bit dummy 0xFF before data token
	  spi_write(0xFF) ;
	  //3-Send data token 0xFE
	  spi_write(0xFE) ;
	  
	  //4- Send sector data to be written in SD card sector address
	  
	  for( uint16_t i = 0 ; i< SECTOR_SIZE ; i++ )
	  {
		  spi_write(trans_buffer[i]) ;
	  }
	    
	  //5- Wait for response after data block sent .
	  uint16_t iterations = 0 ;
	  while(iterations++ < MAX_ITERATION_RESPONSE )
	  {
		  response = spi_read(0xFF) ;
		  response &= 0x0F ;
		  if(response  == WRITE_RESPONSE_ACCEPTED)
		     break ;
		  else if( response  == WRITE_CRC_REJECTED || response == WRITE_ERROR_REJECTED)
		      return 0xFF ; // Operation Failed 
	  }
	  
	  if( response  != WRITE_RESPONSE_ACCEPTED )
	  {
	     return 0xFF;  // Failed due to receive not acceptable response Or it take too long .
	  }		 
	// 6- Wait for Card to finish busy mode after write data block
	
	iterations = 0 ;
	while(iterations++ < MAX_ITERATION_RESPONSE && !( response = spi_read(0xFF)) ) ;
	
	//7 -finally send 8bit clock dummy to finish 0xFF after busy mode finish.
	if(response)  // response != 0
	{ 
	   spi_write(0xFF) ;	
	   de_assert_CS() ;
	   return 0x00 ;  //No error .
	}	   
	else
	  return 0xFF ;   // Failed due to too long time wait .	
}
  