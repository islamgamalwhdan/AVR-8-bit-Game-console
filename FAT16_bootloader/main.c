/*
 * main.c
 *
 * Created: 2/6/2018 5:43:16 AM
 *  Author: Islam Gamal
 *  Target : AVR 8-bit - ATmega644P
 
 VERY IMPORTANT NOTES
 
 NOTE 1:
   to extract bin file from AVR studio(5) project from it's hex file you should do that :
 1- open project -> properties -> Build events 
 2- In pre-build event command line paste this line  : "C:\Program Files (x86)\Atmel\AVR Studio 5.0\AVR Toolchain\bin\avr-objcopy.exe" -I ihex  target.hex -O binary target.bin
 
 NOTE 2:
   to start burn boot loader hex file in boot section in flash memory you should do that :
   1- open project -> properties ->Toolchain->AVR/GNU c linker -> miscellaneous 
   2- In other linker flags paste this line: "-Wl,--section-start=.text=0xE000" without quotes "" .
   
 */ 

#define F_CPU 8000000UL

#include <stdint.h>
#include <string.h>

#include <avr/io.h>
#include <avr/boot.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include <avr/delay.h>

#include "pff.h"
#include "diskio.h"
#include "uart.h"

#define ENABLED   1
#define DISABLED  2
#define DEBUG_MODE ENABLED
#define APPLICATION_FLASH_ADD   0x0000  // To-do : we can get it from hex file 
#define SPM_PAGESIZE 128
#define  MAX_FILES 10
#define  MAX_FILE_NAME 13
#define  DIR_NAME "files"
#define  DIR_PATH "files/"

char buffer_out[200] ;

typedef struct
{
	char file_name[MAX_FILES][MAX_FILE_NAME] ;
	uint16_t file_size[MAX_FILES] ;
}FILES_INFO;

/*================================= Macros =============================*/
#define debug(ASSERTION,EN,... ) {\
	if(ASSERTION){ \
		if(EN) Uart_Transimit_String(__VA_ARGS__) ; \
		return 0 ; }}
/*================================= Function definitions =============================*/	
	
void boot_program_page (uint32_t page, uint8_t *buf) ;
int choose_file_num() ;

/*================================= Main Function =============================*/		



int main(void)
{
    FATFS fs;
	DIR dir_files ;
	FRESULT rc ;
	FILINFO fno;
	
    FILES_INFO content ;
	int i = 0 ;	
	Uart_init(9600,0) ;  // for debug .
	// mount sd card .
	pf_mount(&fs) ; 
	// open directory it's name is "files" .
   if(pf_opendir(&dir_files , DIR_NAME) == FR_OK) 
 {			
		   
	for(;;) 
	{//loop to read files or directory inside "files" directory .
		
		rc = pf_readdir(&dir_files, &fno);	/* Read a directory item */
		if (rc || !fno.fname[0]) break;	/* Error or end of dir */
		
		if (! (fno.fattrib & AM_DIR) )  // if it is not directory So it should be files .
		{
		    strncpy(content.file_name[i] , fno.fname , strlen(fno.fname)) ;
			content.file_size[i++] = fno.fsize ;
		 }//iner_if
		 
	} //for
	
 }//if
 
 // Display bin directory to choose bin file from it .
 
 Uart_Transimit_String("FILES :\n\n") ;
	   for(int j = 0 ; j<i ; j++)
	    {
		  //sprintf(buffer_out,"%d- %d %s\n " , j+1 , content.file_size[j] , content.file_name[j] ) ;	
          Uart_Transimit_String(content.file_name[j]) ;
		  Uart_Transimit_String("\n") ; 
		}	

UINT rb = SPM_PAGESIZE ; 
uint8_t app_bin_buff[128] ;
char file_path[26] = DIR_PATH  ;
	
int file_num  = choose_file_num() ; 
strcat(file_path , content.file_name[file_num]) ;  // to be like that for example "files/app.bin" .
//open target bin file .
if(pf_open(file_path) == FR_OK) 

 {
	 
  while(rb== SPM_PAGESIZE) // read while until reach rb != SPM_PAGESIZE (end of file) .
  {
	 memset(app_bin_buff , 0xFF , SPM_PAGESIZE) ;  // This to prevent if it read less than 128 byte at the end of file ex . rb = 100 so it reads only 100 and the remaining 28 byte will be 0xFF to setat the end .
	 pf_read(app_bin_buff , SPM_PAGESIZE , &rb) ;
	 if(rb) boot_program_page(APPLICATION_FLASH_ADD + (i*SPM_PAGESIZE) , app_bin_buff) ; // write on flash page by page .
  } // while
  
 }//if  
 
 	
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

int choose_file_num()
{
	return 0 ;
}
