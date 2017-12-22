/*
 * spi.h
 *
 * Created: 5/6/2017 1:26:05 AM
 *  Author: Islam Gamal
 */ 


#ifndef SPI_H_
#define SPI_H_


#include <avr/io.h>

/*================Types=====================================*/
//typedef unsigned char uint8_t ;
//typedef unsigned int uint16_t ;

/*================Constants================================ */
#define SPI_DDR  DDRB
#define SPI_PORT PORTB
#define SPI_DATA_REG SPDR
#define MOSI      PB5
#define MISO      PB6
#define SCK       PB7
#define SS        PB4
#define SPI_FOSC_2    0b100
#define SPI_FOSC_4    0b000
#define SPI_FOSC_8    0b101
#define SPI_FOSC_16   0b001
#define SPI_FOSC_32   0b110
#define SPI_FOSC_64   0b111
#define SPI_FOSC_128  0b011

#define MSB_FIRST 0
#define LSB_FIRST 1
/*=============Macros=========================*/

#define SET_BIT(REG,BIT) ( (REG) |=( 1<<(BIT)) )
#define CLEAR_BIT(REG,BIT) ( (REG) &=~(1<<(BIT)) ) 
#define assert_CS()  ( CLEAR_BIT(SPI_PORT , SS) )
#define de_assert_CS() ( SET_BIT(SPI_PORT , SS) )

/*============================================*/

/*==========Functions prototypes==========================*/
void spi_init( uint8_t spi_speed ) ;
uint8_t spi_set_speed( uint8_t spi_speed ) ;
uint8_t spi_write(uint8_t data_s) ;
uint8_t spi_read(uint8_t dummy) ;
/*=======================================================*/



#endif /* SPI_H_ */