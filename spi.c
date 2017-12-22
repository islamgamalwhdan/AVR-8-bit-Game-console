/*
 * spi.c
 *
 * Created: 5/6/2017 1:25:52 AM
 *  Author: Islam Gamal 
 */ 

#include "spi.h"

void spi_init( uint8_t spi_speed  )
{
	SET_BIT(SPI_PORT , SS) ; // De-assert CS line pin .
	SPI_DDR |= ( 1<<MOSI ) | ( 1<<SCK ) | ( 1<<SS )  ;	
	CLEAR_BIT(SPI_DDR , MISO) ; // MISO input .
	SET_BIT(SPI_PORT , MISO); // pull-up MISO pin .
	SPCR = ( 1<<SPE ) | ( 1 <<MSTR ) | ( (spi_speed & 0x03)<< SPR0  ) | ( 0<<CPOL ) | ( 0<<CPHA ) | ( 0 << DORD )  ;    /* Enable SPI module , controller is master or slave  SPI clock speed = Fosc/ 16 . */     
	SPSR |= ((spi_speed>>2) << SPI2X) ;  
}

uint8_t spi_set_speed( uint8_t spi_speed ) 
{
	SPCR  =  ( 1<<SPE ) | ( 1 <<MSTR ) | ( (spi_speed & 0x03)<< SPR0  ) | ( 0<<CPOL ) | ( 0<<CPHA ) | ( 0<< DORD )  ;
	SPSR |= ((spi_speed>>2) << SPI2X) ;
	
	return 1 ;
}

uint8_t spi_write(uint8_t data_s)
{
	//preconditions  : Assert chip select line    ( make SS 0) .
	//Post-conditions : De-assert chip select line ( make SS 1) .
	
	SPI_DATA_REG = data_s ; 
	while(!(SPSR & (1<<SPIF))) ;
	return SPI_DATA_REG;      // As in SPI buffer are circular means that when you send complete 8bit from master to slave
	                         // you also receive complete 8bit from slave device  . So you can use return value or just discard it .
}

uint8_t spi_read(uint8_t dummy)
{
	// Dummy can be 0x00 or oxFF .
	SPI_DATA_REG = dummy ; // Send any dummy byte to make only clocks on SCK pin to get the value from slave shift register .
	while(!(SPSR & (1<<SPIF))) ;
	return SPI_DATA_REG ;
}