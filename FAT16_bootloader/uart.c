/*
 * uart.c
 *
 * Created: 5/3/2017 12:51:01 AM
 *  Author: Islam Gamal  
 */ 

// Uart interrupt driven driver for ATmega32 .
// also there is non-interrupt function to receive and transmit data .

//Last modified : 5/5/2017 .

/* =================== Includes ================================ */
#ifndef F_CPU
       #define F_CPU 8000000UL
#endif

#include <stdio.h>  // sprintf usage .
 
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#include "uart.h"

/* ==================== Data structures ================================= */

//Queue data structure 

  typedef char QueueEntry ; // Queue element abstract data type entered by users .

  typedef struct
  {
       uint8_t head ;
       int8_t tail ;  // signed as it will take -1 at initialization .
       uint8_t size_ ;
       QueueEntry entry[MAXQUEUE] ;
  }Queue_t ;

// Queue data structure accessing mechanism ( function prototypes )

static uint8_t InitializeQueue(volatile Queue_t *) ;
static inline uint8_t Append(QueueEntry , volatile Queue_t *) ;
static inline uint8_t Serve(QueueEntry *, volatile Queue_t *) ;
static inline uint8_t QueueEmpty(volatile Queue_t *) ;
static inline uint8_t QueueFull( volatile Queue_t *) ;
static inline uint8_t all_served( volatile Queue_t *pq);
static inline uint8_t ClearQueue(volatile Queue_t *) ;

/* =================== Global variables ================================ */

 volatile Queue_t recv_buffer   ;  // receive buffer
 volatile Queue_t trans_buffer  ; // transmit buffer 
 volatile uint16_t b_r ;
/* =================== Function Definitions ================================ */

uint8_t Uart_init(uint16_t b_rate , uint8_t inter_en) 
{
	// Step1 : Set baud rate reg value .
	                           
	volatile uint16_t baud_rate_reg ;
	
	if(F_CPU == 8000000UL)
	{
		switch(b_rate)
		{
			case 9600 :
			           {
						  baud_rate_reg = 51 ;  
					   }break;
			
			case 19200 :
			            {
							baud_rate_reg = 25 ;
						}break ;
			
			case 38400 :
			            {
							baud_rate_reg = 12 ;
						}break ;
			default:
			{
				baud_rate_reg = F_CPU/(16UL*b_rate) -1UL ;
			}								   
		}// End switch
	}// End if
	
	else
	{
	    baud_rate_reg = (F_CPU)/(16UL*b_rate)-1UL ;
	}
	
		
	   UBRRH = baud_rate_reg >> 8;
	   UBRRL = baud_rate_reg     ;
	
	   // Step 2 : Enable Transmit , Receive And Receive Interrupt . 
	
	   UCSRB |= ( 1<< RXEN ) | ( 1<< TXEN ) | ( 1<< RXCIE ) ;
	
	   // Step 3 : Set Frame specifications to N81 [ No parity , 8bit character size and  1 stop bit ] & Asynchronous mode .
	   #ifdef __AVR_ATmega644P__
	      UCSRC =  ( 0 << UMSEL00 ) | ( 0 << UMSEL01 ) | ( 0 << UPM00 ) | ( 0 << UPM01 ) | ( 0 << USBS0 ) | ( 1 << UCSZ00 ) | ( 1 << UCSZ01  ) ;
		  
	   #else   //  __AVR_ATmega32__  
	   
	      UCSRC  = ( 1<<URSEL ) | ( 0 << UMSEL ) | ( 0 << UPM0 ) | ( 0 << UPM1 ) | ( 0 << USBS ) | ( 1 << UCSZ0 ) | ( 1 << UCSZ1  ) ;
	   #endif //  __AVR_ATmega644__ definied 	  
	
	
	
	//Step 4 : Initialize queues
	 
	InitializeQueue(&recv_buffer)  ;
	InitializeQueue(&trans_buffer) ;
	 
	// Step 5 : Enable Global interrupt
	
	if(inter_en)
	    sei() ;  // VERY IMPORTANT BUG here Uart_receve_chr function bug you should disable it to work .
	 
	return 1 ;
}


uint8_t Uart_Disable(void)
{
	UCSRB = ( 0 << RXEN ) | ( 0 << TXEN ) | ( 0 << RXCIE ) ;
	
	return 1 ;
}


char get_RecvBuffer_data(void)
{
	/*Note we disable interrupts and re-enable it again to save shared data to not to be interrupted during operation on it at task  (atomic block) */ 
	QueueEntry data ;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		
	 if( !QueueEmpty(&recv_buffer) )
	 {
	    Serve(&data , &recv_buffer);
	}	 	
		return ((uint8_t)data) ;
	  }

	  return 0 ;
}

uint8_t put_TransBuffer_data(char data) 
{
	//Keep in mind that if the data to be transmit has length > MAX_TRANS_CH it will cause overflow in circular buffer and some data will lost 
	// take care of that !!! .
	
	if( !QueueFull(&trans_buffer) )  // this check prevent put in the buffer more than MAX_TRANS_CH and without serve loaded data in the buffer .
	{
		Append((QueueEntry)data , &trans_buffer) ;
		SETBIT(UCSRB , UDRIE) ;  // Enable UDRE interrupt .
		return 1 ;
		
	}
	
	
	return 0 ;  //failed to insert new data to the transmit buffer .
	
}

uint8_t put_TransBuffer_String(char *str)
{
	while(*str)
	{
		if(*str == 0x0A)  //  line feed
		{
			put_TransBuffer_data(0x0A) ;
			put_TransBuffer_data(0x0D) ;
			str++ ;
		}
		else
		   put_TransBuffer_data(*str++) ;
	}
	put_TransBuffer_data('\0') ;
	
	return 1 ;
}

uint8_t Uart_Transimit_chr(char ch) 
{
	while ( !( UCSRA & (1<<UDRE)) ) ;

  /* Put data into buffer, sends the data */
       if(! (UCSRA & (1<<FE)) )
	   {
	    UDR = ch;
		return 1 ;
	   }			

	return 0;
}

uint8_t Uart_Transimit_String(char *str)
{
	while(*str)
	{
		if(*str == 0x0A) // line feed .
		{
			Uart_Transimit_chr(0x0A) ;
			Uart_Transimit_chr(0x0D) ;
			str++ ;
		}
		else 
		    Uart_Transimit_chr(*str++) ;
		
	}
	//Uart_Transimit_chr('\0') ;
	
	return 1 ;
}

char Uart_Receive_chr(uint8_t wait)
{
	// if wait = 1 then program will wait until data in the receive buffer come 
	// or if wait = 0 then program will check if any data in the receive buffer and read it if not it send failed error .
	
	if(wait)
	{
		while( ! (UCSRA &(1<<RXC) )) ;
	    return UDR ;
	}
	
	else
	{
		if( UCSRA &(1<<RXC) )  
		  return UDR ;
		else
		  return 0 ;  // No received data .
	}
	
}

uint8_t Uart_Receive_String(char *str) 
{
	uint8_t rec_ch ;
	while( (rec_ch = Uart_Receive_chr(1)) ) 
	{
		*str++ = rec_ch ;
	}
	*str = '\0' ; // Terminate string ;
	
	return 1 ;
}

void Uart_Newline(void)
{
	Uart_Transimit_chr(0x0D) ;
	Uart_Transimit_chr(0x0A) ;
}

void Uart_Print_Int(int num) 
{
	char int_str[6] ;
	sprintf(int_str , "%d" , num) ;
	Uart_Transimit_String(int_str) ;
}

/* =================== Interrupt Service Routines ================================ */

// If there is new data received completely .

ISR(USART_RXC_vect)
{
	//If there is any new received data??
	 
	//Keep in mind that if the received data length > MAX_RECV_CH it will cause overflow in circular buffer and some data will lost 
	// take care of that !!! .
	
  	if( !QueueFull(&recv_buffer) )  // this check prevent put in the buffer more than MAX_RECV_CH and without serve loaded data in the buffer .
	  {
		  Append( UDR , &recv_buffer) ;
	  }
	  
	  else
	  {
		  uint8_t flush_temp = UDR ;  // Read receive buffer to flush it and make RCX bit zero .
	  }
	  
}


ISR(USART_UDRE_vect)
{
   // Now Transmit buffer ready to go out >>>> UDR .
   
   if( !QueueEmpty(&trans_buffer) )
   {
	    Serve( (QueueEntry *)&UDR , &trans_buffer) ;
   }
   
   else
   {
	   CLEARBIT(UCSRB , UDRIE) ;   // Disable UDRE interrupt enable .
   }

}
/* =================== Queue data structure accessing mechanism  ================================ */

static uint8_t InitializeQueue(volatile Queue_t *pq)
{
	pq->head = 0 ;
	pq->tail = -1 ; // As when we add new element we will increase rear by one (tail = 0) and add element .
	pq->size_ = 0 ;
	
	return 1 ;
}


static inline uint8_t Append(QueueEntry e , volatile Queue_t *pq)
{
	pq->tail = (pq->tail +1) % MAXQUEUE ;  // if tail == MAXQUEUE then tail will be 0 .
	pq->entry[pq->tail] = e ;
	pq->size_++ ;
	
	return 1 ;
}

static inline uint8_t Serve(QueueEntry *pe ,volatile  Queue_t *pq)
{
	*pe = pq->entry[pq->head] ;
	pq->head = (pq->head + 1) % MAXQUEUE ;
	pq->size_--;
	
	return 1 ;
}

static inline uint8_t QueueEmpty(volatile Queue_t *pq)
{
   return (!pq->size_) ;	
} 

static inline uint8_t QueueFull( volatile Queue_t *pq) 
{
	return (pq->size_ == MAXQUEUE ) ;
}

static inline uint8_t all_served(volatile Queue_t *pq)
{
	if(pq->head == 0 && (pq->size_ == MAXQUEUE) ) return 1 ;  
	return (pq->head  == pq->size_ ) ;
} 


static inline uint8_t ClearQueue(volatile Queue_t *pq)
{
	pq->head = 0 ;
	pq->tail = -1 ;
	pq->size_ = 0 ;
	
	return 1 ;
} 
