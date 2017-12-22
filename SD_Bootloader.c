/*
 * SD_Bootloader.c
 *
 * Created: 5/26/2017 12:35:14 AM
 *  Author: Islam Gamal 
 
 VERY IMPORTANT NOTES
 
 NOTE 1:
   to extract bin file from AVR studio(5) project from it's hex file you should do that :
 1- open project -> properties -> Build events 
 2- In pre-build event command line paste this line  : "C:\Program Files (x86)\Atmel\AVR Studio 5.0\AVR Toolchain\bin\avr-objcopy.exe" -I ihex  target.hex -O binary target.bin
 
 NOTE 2:
   to start burn boot loader hex file in boot section in flash memory you should do that :
   1- open project -> properties ->Toolchain->AVR/GNU c linker -> miscellaneous 
   2- In other linker flags paste this line: "-Wl,--section-start=.text=0xE000" without quotes "" .
   
 NOTE 3 : 
   If you build your boot loader project in optimization level that target also should be build in same level !! .   

 */ 

#define F_CPU 8000000UL
#include <stdint.h>
//#include <stdlib.h>
#include <string.h>

#include <avr/io.h>
#include <avr/boot.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/delay.h>

#include "sd.h"
#include "uart.h"

#define ENABLED   1
#define DISABLED  2

#define DEBUG_MODE ENABLED

#define MOUNT_ITERATION_MAX     128 
#define MOUNT_ERROR_NOT_FOUND   0xFF
#define APP_NO_SECTORS          10  // To-do : auto detect .
#define APP_OFFSET_SECTOR       647  // To-do : auto detect but now u can detect it from winhex program .
#define APPLICATION_FLASH_ADD   0x0000  // To-do : we can get it from hex file 

/*================================= Macros =============================*/
#define debug(ASSERTION,EN,... ) {\
	if(ASSERTION){ \
		if(EN) Uart_Transimit_String(__VA_ARGS__) ; \
		return 0 ; }}
/*================================= Function definitions =============================*/		
void boot_program_page (uint32_t page, uint8_t *buf) ;

/*================================= Main Function =============================*/		

int main(void)
{
	uint8_t mount_status = 0xFF ;
	int iterations = 0  , i = 0;
	volatile uint8_t app_bin_buff[512] ;
	
	#if ( DEBUG_MODE == ENABLED )
	Uart_init(9600) ;
	_delay_ms(100) ;
	Uart_Transimit_String("\nLoading SD Card ...") ;
	#endif
	
	int *ptr = malloc(sizeof(int)) ;
	// 1 - Wait while for mounting SD card 
    while( ((mount_status = SD_mount()) == 0xFF) && (iterations++ < MOUNT_ITERATION_MAX) ) ;
	
	//2- Check if SD card mounted successfully or not.
	
	 debug( mount_status == 0xFF , DEBUG_MODE , "\nMounting SD Card failed!!" ) ;
	 
	  #if ( DEBUG_MODE == ENABLED )
        	Uart_Transimit_String("\nSD Card mounted successfully!!") ;
	  #endif
		
		// 3 - Retrieve application program from sd card sector by sector and store it sector by sector .
		// (each sector contain 2 page in atmega644p) in flash memory .
		
		for( int i = 0 ; i<APP_NO_SECTORS ; i++)
		{
			uint8_t rd = SD_Read_Sector(APP_OFFSET_SECTOR+i , app_bin_buff) ;
			debug((rd == 0xFF) , DEBUG_MODE ,"\nRead sector failed!!");
			
			boot_program_page( APPLICATION_FLASH_ADD + (i*512)      , app_bin_buff     ) ;
			boot_program_page( APPLICATION_FLASH_ADD + (i*512) + 256 , app_bin_buff +256) ;
		}
		
		//4- Now jump to application program ... enjoy :) .
	    ( (void (*)(void)) APPLICATION_FLASH_ADD)() ;	
}



void boot_program_page (uint32_t page, uint8_t *buf)
{
	//I will use 24 page eache page contain 256 byte [128 word] .
	
	
    uint16_t i;
    uint8_t sreg;
    // Disable interrupts.
    sreg = SREG;
    cli();
    eeprom_busy_wait ();
    boot_page_erase (page);
    boot_spm_busy_wait ();      // Wait until the memory is erased.
    for (i=0; i<SPM_PAGESIZE; i+=2 , buf += 2)
    {
        // Set up little-endian word.
        uint16_t w = *( (uint16_t *) buf) ;
    
        boot_page_fill (page + i, w);
    }
    boot_page_write (page);     // Store buffer in flash page.
    boot_spm_busy_wait();       // Wait until the memory is written.
    // Reenable RWW-section again. We need this if we want to jump back
    // to the application after bootloading.
    boot_rww_enable ();
    // Re-enable interrupts (if they were ever enabled).
    SREG = sreg;
}