/*
 * sd.h
 *
 * Created: 5/15/2017 5:46:46 PM
 *  Author: Islam Gamal
 */ 


#ifndef SD_H_
#define SD_H_

/*========== Includes ==========================*/

#include <stdint.h>


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

/*========== Functions prototypes ==========================*/

uint8_t SD_Send_Command(uint8_t command , uint32_t address) ;
uint8_t SD_mount(void) ;
uint8_t SD_unmount(void) ;
uint8_t SD_Read_Sector( uint32_t sector_offset , uint8_t *recv_buffer ) ;
uint8_t SD_Write_Sector( uint32_t sector_offset , uint8_t *trans_buffer ) ;




#endif /* SD_H_ */