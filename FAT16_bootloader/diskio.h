/*-----------------------------------------------------------------------
/  PFF - Low level disk interface modlue include file    (C)ChaN, 2009
/-----------------------------------------------------------------------*/

#ifndef _DISKIO

#include "integer.h"


/* Status of Disk Functions */
typedef BYTE DSTATUS;


/* Results of Disk Functions */
typedef enum {
	RES_OK = 0,		/* 0: Function succeeded */
	RES_ERROR,		/* 1: Disk error */
	RES_NOTRDY,		/* 2: Not ready */
	RES_PARERR		/* 3: Invalid parameter */
} DRESULT;

#include <stdint.h>


/*========== Constants ==========================*/

#define READ_DATATOKEN_ITERATION_RESPONSE 4096
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

#define STA_NOINIT		0x01	/* Drive not initialized */
#define STA_NODISK		0x02	/* No medium in the drive */

/* Card type flags (CardType) */
#define CT_MMC				0x01	/* MMC ver 3 */
#define CT_SD1				0x02	/* SD ver 1 */
#define CT_SD2				0x04	/* SD ver 2 */
#define CT_SDC				(CT_SD1|CT_SD2)	/* SD */
#define CT_BLOCK			0x08	/* Block addressing */

/*---------------------------------------*/
/* Prototypes for disk control functions */

DSTATUS disk_initialize (void);
DRESULT disk_readp (BYTE*, DWORD, UINT, UINT);

#define _DISKIO
#endif
