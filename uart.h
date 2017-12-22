/*
 * uart.h
 *
 * Created: 5/3/2017 12:51:20 AM
 *  Author: Islam Gamal
 */ 

// Uart interrupt driven driver for ATmega32 .
// also there is non-interrupt function to receive and transmit data .

//Last modified : 5/5/2017 .


#ifndef UART_H_
#define UART_H_

/* =================== Includes ================================ */

#include <stdint.h>
#include <avr/io.h>

/* ==================== Constant ================================= */
#ifdef  __AVR_ATmega644P__
   #define UBRRL            UBRR0L
   #define UBRRH            UBRR0H
   #define UDR              UDR0
   #define UCSRA            UCSR0A
   #define UCSRB            UCSR0B
   #define UCSRC            UCSR0C
   #define RXC              RXC0
   #define UDRE             UDRE0
   #define FE               FE0
   #define RXEN             RXEN0
   #define TXEN             TXEN0
   #define RXCIE            RXCIE0
   #define UDRIE            UDRIE0
   #define USART_RXC_vect   USART0_RX_vect
   #define USART_UDRE_vect  USART0_UDRE_vect
#endif   

#define DATA_REG          UDR
#define MAXQUEUE          32
#define MAX_RECV_CH       (MAXQUEUE)
#define MAX_TRANS_CH      (MAXQUEUE)


/* ==================== Macros ================================= */
#define SETBIT(MEM , BIT)   ( (MEM) |= (1<< (BIT)) )
#define CLEARBIT(MEM , BIT) ( (MEM) &=~(1<< (BIT)) )


/* ==================== External Variables =========================*/

//extern volatile Queue_t recv_buffer   ;
//extern volatile Queue_t trans_buffer  ;
/* ==================== Functions Prototypes =========================*/


uint8_t Uart_init(uint16_t b_rate) ;
uint8_t Uart_Disable(void) ;
char get_RecvBuffer_data(void) ;
uint8_t put_TransBuffer_data(char data) ;
uint8_t put_TransBuffer_String(char *str) ;

// These functions used to receive and transmit without interrupt  
uint8_t Uart_Transimit_chr(char ch) ;
uint8_t Uart_Transimit_String(char *str) ;
char Uart_Receive_chr(uint8_t wait) ;
uint8_t Uart_Receive_String(char *str) ;
void Uart_Newline(void);
void Uart_Print_Int(int num) ;
#endif /* UART_H_ */