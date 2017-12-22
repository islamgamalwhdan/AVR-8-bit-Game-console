///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Copyright Nurve Networks LLC 2008
// 
// Filename: XGS_AVR_VGA_BMP_120_96_2_V010.S
//  
// Original Author: Andre' LaMothe
// 
// Last Modified: 9.1.08
// 
//  
// Description: 120x96 resolution VGA driver with 2-bits per pixel and a new color palette every 8 scanlines, thus
// every 2-bit pixel is used as an index into the color palette for that particular pixel's palette as shown 
// below:
//                                               Color Palette for every 8 scanline
//                                                    
// Pixel Value     Palette Index                Index      Color Encoding
//   00       =         0    ---------------->   0   - [0 0 |B1 B0| G1 G0| R1 R0 ] - 6-bit color, upper 2-bits 0
//   01       =         1    ---------------->   1   - [0 0 |B1 B0| G1 G0| R1 R0 ] - " "
//   10       =         2    ---------------->   2   - [0 0 |B1 B0| G1 G0| R1 R0 ] - " "
//   11       =         3    ---------------->   3   - [0 0 |B1 B0| G1 G0| R1 R0 ] - " "
//
// Memory for bitmap modes is laid out from top left corner of the screen from left to right, top to bottom.
// Each byte represents 4 pixels where pixel 0,1,2,3 on screen are encoded frow low bit to high bit as shown below:
// 
// Pixel Encoding
//
//     P3  |   P2  |   P1  |   P0     <------ pixel #
// [ b7 b6 | b5 b4 | b3 b2 | b1 b0 ]  <------ bit encodings
// msb                            lsb        
// 
//                Video RAM Layout
// (0,0).....................................(119,0)
// .P0 P1 P2 P3...                           P119
// .
// .
// .
// .
// .
// .
// (0,95)...................................(119, 95)                          
// 
// For example, if you wanted to change the color of the pixel (0,0), then you would write the lower 2-bits
// of the very first byte of Video RAM.
// 
// The color palette changes every 8 scanlines, thus a new set of 4 colors can be used every 8 scanlines. 
// However, the SAME palette must be used for the entire "width" of the scaline, so vertical color changes
// are possible, but not horiztonal. There are 96 lines of video, thus 96/8 = 12 palettes, and each palette
// consumes 4 bytes, thus a total of 48 bytes for the color palettes.
//
//
// Below shows how the palettes work:
//
// Scanlines  Palette#       4-byte palettes Color 0 - Color 3
//
//   0-7         0           | C3, C2, C1, C0 |         
//   7-15        1           | C3, C2, C1, C0 |
//   16-23       2           | C3, C2, C1, C0 |
//     .
//     .
//     .
//   88 - 95    11          | C3, C2, C1, C0 |
//
//
// However, there is a constraint on the palette memory base address.... it must be on a 256 
// byte page boundary, in other words, the palette can NOT cross a page boundary. Therefore, care must be
// taken when final compilation is performed and linking, the situation is shown below graphically.
// 
//
//  Page Boundaries, palette lies between two page boundaries - GOOD.
//   |                       |                         |    
//   n*256...................(n+1)*256.................(n+2)*256
//   |...Palette Data........|
//
//
//  Page Boundaries, palette does NOT lie between two page boundaries - BAD.
//   |                       |                         |    
//   n*256...................(n+1)*256.................(n+2)*256
//               |...Palette Data........|
//
// Although, its possible to enforce a page boundary on data structure, the compiler will abuse memory so
// much and waste RAM, thus, its better to do the alignment MANUALLY. All you have to do if there is a 
// problem is simply edit the palette declaration below and move it up or down around the other variables
// this will slide it around in memory and you can get it to a safe spot. Another approach, is dynamic 
// memory allocation and you write a memory mananger that can retrieve page boundary memory blocks from
// a heap, and not waste other memory, way too much trouble. So bottom line is when you use this engine
// if the palette malfunctions, simply move it above or below the scanline buffers and that will shift 
// it enough either way to get it within a page boundary again.
//
// The interface between the C caller and this driver is accomplished thru a pointer to VRAM from C,
// the palette base address exported back to C from this driver, along with a few status variables
// from the driver to interogate the current line, etc. See below:
//
// There is a single pointer to VRAM, the driver expects the C caller to allocate memory and then 
// initialize a pointer with the following name to point to the VRAM:
// 
// .extern vram_ptr // 16-pit pointer passed in from caller that points to video RAM
//
// Next, the driver passses back the current absolute video line 0..524 via curr_line and the current
// virtual raster line via raster_line:
//
// .global curr_line     // read only, current physical line
// .global raster_line   // read only, current logical raster line
//
// Finally,the driver allocates its own memory for the palette which can be accessed here:
//
// .global palette_table // read/write, 16-bit base address to palette used for driver
//
// The caller is recommended to initialize the palette since it only has simple test colors in it.
//
//  
// Original Author: Andre' LaMothe
// 
// Last Modified: 8.20.08
//
// Overview:
//
//
//
// Modifications|Bugs|Additions:
//
// Author Name: 
//
//
//
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SET PROCESSOR TYPE (must come first)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef __AVR_ATmega644__
#define __AVR_ATmega644__
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// INCLUDES 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
 
#include <avr/interrupt.h> 
#include <avr/io.h>

// include header file for driver
#include "XGS_AVR_VGA_BMP_120_96_2_V010.h"


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINES 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// needed for macro, also fixes so you can use register names directly and NOT need macro _SFR_IO_ADDR( x )
#define __SFR_OFFSET 0 

// with the current D/A converter range 0 .. 15, 1 = 62mV


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MACROS
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// this macro delays any number of clocks, uses chunks of 4
.macro  DELAYX reg = r16, clocks
    ldi     \reg, (\clocks)/4        // (1)
1:
    dec     \reg                     // (1)
    nop                              // (1)
    brne    1b                       // (1/2)

    // now delay fractional component
    .rept   (\clocks % 4)
    nop                              // (1)
    .endr

.endm

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// this macro delays any number of clocks that are a multiple of 4, anything else is truncated
.macro  DELAY4X reg = r16, clocks
    ldi     \reg, (\clocks)/4        // (1)
1:
    dec     \reg                     // (1)
    nop                              // (1)
    brne    1b                       // (1/2)
.endm

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// this macro delays any number of clocks that are a multiple of 3, anything else is truncated
.macro  DELAY3X reg = r16, clocks
    ldi     \reg, (\clocks)/3        // (1)
1:
    dec     \reg                     // (1)
    brne    1b                       // (1/2)
.endm

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

.macro SPLIT_PIXEL4 rt = r0
    // pre-computational pixel block, simply splits each 4-pixels packed into a byte and spreads them across 
    // 4-bytes in the scanline buffer

    // assumes X is pointing to the next pixel in VRAM to split, 
    // Y is pointing to the back scanline buffer to store the split pixels
    // rt is the working register

    // 16 clocks per macro

    // split 0: retrieve next 4 pixels from VRAM, 1 byte
    // store lowest 2-bits, pixel 0
    ld      \rt, X+             // (2)
    st      Y+, \rt             // (2)

    // split 1: shift data 2x right to get next pixel value then store it
    lsr     \rt                 // (1)
    lsr     \rt                 // (1)
    st      Y+, \rt             // (2)

    // split 2: shift data 2x right to get next pixel value then store it
    lsr     \rt                 // (1)
    lsr     \rt                 // (1)
    st      Y+, \rt             // (2)

    // split 3: shift data 2x right to get next pixel value then store it
    lsr     \rt                 // (1)
    lsr     \rt                 // (1)
    st      Y+, \rt             // (2)
.endm

    
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

.macro DRAW_PIXELS_W_COMPV1 rpix = r16, rraw = r17, rrgb = r18
    // draw the next pixel block (5 pixels) from current front scanline buffer while computing next pixel and placing 
    // in back scanline buffer each super block of 5 pixels is used to perform 9 clocks of real calculation and nop that 
    // process another pixel into the back scanline buffer thus, each pixel has 2 clocks for computation which is broken 
    // into elements named: A2,B1,C1,D2,E2,F1, where the trailing # indicates clocks needed

    // assumes X is pointing to front scanline buffer of 120 pixels for rendering
    // Y is pointing to back scanline buffer for 2nd processing pass on the 24 pixels previously split in the hsync region

    // 5 pixels per block, total time 5 * 5 clocks = 25 clocks per 5 pixels
    // 24 sets of 5 pixels = 120 pixels, or 24 sets * 25 clocks per set = total clocks = 600
    // 25 clocks per macro

    // this version assumes that color palette is on 256 byte boundary thus simplified address calc can be used, 
    // freeing up time for bit mask operation

    // ---------------------------------------------------------------------------------------------------------------
                                                                                                                                                                                                                                                                                                                            
    // draw pixel 0 + computation 
    ld      \rpix, X+             // (2), output the next pixel to screen
    out     PORTA, \rpix          // (1)

    // computation (A2)
    ld      \rraw, Y              // (2), retrive next raw pixel from back scanline buffer, lower 2-bits contains pixel code

    // ---------------------------------------------------------------------------------------------------------------

    // draw pixel 1 + computation            
    ld      \rpix, X+             // (2), output the next pixel to screen
    out     PORTA, \rpix          // (1)

    // computation (B1)
    andi    \rraw, 0x03           // (1), mask lower 2-bits representing pixel color
    // computation (C1)
    add     ZL, \rraw             // (1), ZL += rraw, index into color palette 0...3 

    // ---------------------------------------------------------------------------------------------------------------

    // draw pixel 2 + computation            
    ld      \rpix, X+             // (2), output the next pixel to screen
    out     PORTA, \rpix          // (1)

    // computation (D2)
    ld      \rrgb, Z              // (2), finally retrieve 8-bit RGB value from palette based on 2-bit code and 4-byte palette look up

    // ---------------------------------------------------------------------------------------------------------------

    // draw pixel 3 + computation            
    ld      \rpix, X+             // (2), output the next pixel to screen
    out     PORTA, \rpix          // (1)

    // computation (E2)
    st      Y+, \rrgb             // (2), store final pixel value back to back scanline buffer, increment pointer to next value
      

    // ---------------------------------------------------------------------------------------------------------------

    // draw pixel 4 + computation            
    ld      \rpix, X+             // (2), output the next pixel to screen
    out     PORTA, \rpix          // (1)

    // computation (F1)        
    sub     ZL, \rraw             // (1), adjust ZL, ZL -= rraw

    // free time
    nop                           // (1)

    .endm

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

.macro DRAW_PIXELS_W_COMPV2 rpix = r16, rraw = r17, rzlsave = r18
    // draw the next pixel block (5 pixels) from current front scanline buffer while computing next pixel and placing 
    // in back scanline buffer each super block of 5 pixels is used to perform 9 clocks of real calculation and nop that 
    // process another pixel into the back scanline buffer thus, each pixel has 2 clocks for computation which is broken 
    // into elements named: A2,B1,C1,D2,E2,F1, where the trailing # indicates clocks needed

    // assumes X is pointing to front scanline buffer of 120 pixels for rendering
    // Y is pointing to back scanline buffer for 2nd processing pass on the 24 pixels previously split in the hsync region

    // 5 pixels per block, total time 5 * 5 clocks = 25 clocks per 5 pixels
    // 24 sets of 5 pixels = 120 pixels, or 24 sets * 25 clocks per set = total clocks = 600
    // 25 clocks per macro

    // ---------------------------------------------------------------------------------------------------------------

    // draw pixel 0 + computation 
    ld      \rpix, X+             // (2), output the next pixel to screen
    out     PORTA, \rpix          // (1)

    // computation (A2)
    ld      \rraw, Y              // (2), retrive next raw pixel from back scanline buffer, lower 2-bits contains pixel code

    // ---------------------------------------------------------------------------------------------------------------

    // draw pixel 1 + computation            
    ld      \rpix, X+              // (2), output the next pixel to screen
    out     PORTA, \rpix           // (1)

    // computation (B1)
    mov     \rzlsave, ZL           // (1), save ZL in rzlsave
    // computation (C1)
    add     ZL, \rraw              // (1), ZL = ZL + rraw

    // ---------------------------------------------------------------------------------------------------------------

    // draw pixel 2 + computation            
    ld      \rpix, X+             // (2), output the next pixel to screen
    out     PORTA, \rpix          // (1)

    // computation (D2)
    ld      \rraw, Z              // (2), finally retrieve 8-bit RGB value from palette based on 2-bit code and 
                                  //      4-byte palette look up with 64 shadow copies

    // ---------------------------------------------------------------------------------------------------------------

    // draw pixel 3 + computation            
    ld      \rpix, X+             // (2), output the next pixel to screen
    out     PORTA, \rpix          // (1)

    // computation (E2)
    st      Y+, \rraw             // (2), store final pixel value back to back scanline buffer, increment pointer to next value

    // ---------------------------------------------------------------------------------------------------------------

    // draw pixel 4 + computation            
    ld      \rpix, X+             // (2), output the next pixel to screen
    out     PORTA, \rpix          // (1)

    // computation (F1)
    mov     ZL, \rzlsave          // (1), restore ZL from the previously saved value
    nop                           // (1), free instruction for later use        

    .endm

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


.macro VGA_PIXEL128_2COLOR

    // 5 clocks per pixel at 128 pixels = 640 clocks
    ld      r17, X+             // (2), r17  = vram[ X++ ] 
    out     PORTA, r17          // (1)
    nop                         // (2)
    nop
  
.endm

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

.macro  SAVE_SREG reg = r16

    in      \reg, SREG                       // save status register
    push    \reg

.endm

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

.macro  RESTORE_SREG reg = r16

    pop     \reg
    out     _SFR_IO_ADDR( SREG ), \reg      // restore status register

.endm

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// LOCAL INITIALIZED VARIABLES (SRAM)
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

.section .data

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// NOTE: VERY IMPORTANT
// palette lookup table must have a low byte address from 0..(255-48) for addressing to work properly since only low byte 
// is modified in address lookup in pixel loop, and there are 96/8 = 12 palettes of 4 bytes spanning the scanlines each 
// 8 lines for a total of 48 bytes, if addressing problems occur simply move this data structure above/below the 
// scanline buffers, and that should shift the palette enough to get it back within a page boundary
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

palette_table:  .byte 0b00000000, 0b00000011, 0b00001100, 0b00110000    // palette 0
                .byte 0b00000000, 0b00000010, 0b00001000, 0b00100000    // palette 1
                .byte 0b00000000, 0b00000001, 0b00000100, 0b00010000    // palette 2
                .byte 0b00000000, 0b00001111, 0b00111100, 0b00110011    // palette 3
                .byte 0b00000000, 0b00000011, 0b00001100, 0b00110000    // palette 4
                .byte 0b00000000, 0b00010111, 0b00011101, 0b00110101    // palette 5
                .byte 0b00000000, 0b00000011, 0b00001100, 0b00110000    // palette 6
                .byte 0b00000000, 0b00000010, 0b00001000, 0b00100000    // palette 7
                .byte 0b00000000, 0b00000001, 0b00000100, 0b00010000    // palette 8
                .byte 0b00000000, 0b00001111, 0b00111100, 0b00110011    // palette 9
                .byte 0b00000000, 0b00000011, 0b00001100, 0b00110000    // palette 10
                .byte 0b00000000, 0b00010111, 0b00011101, 0b00110101    // palette 11

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


// scanline buffers, 120 bytes each, they are used interchangeably and swapped each 5 active scanlines
slinebuff0:  .byte 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 
             .byte 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 
             .byte 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0

slinebuff1:  .byte 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 
             .byte 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 
             .byte 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0

// scanline buffer pointers
slinebuff_front_ptr:      .byte 0,0 
slinebuff_back_ptr:       .byte 0,0

slinebuff_back_save_ptr:  .byte 0,0
slinebuff_front_save_ptr: .byte 0,0

// pointer to vram's current position
vram_curr_ptr:            .byte 0,0

// current video line
curr_line:                .byte 0,0    

// current pixel
curr_pixel:               .byte 0     

// current sub line of 5 video line computational path to generate one raster line
curr_sub_line:            .byte 0

// current raster line 0..95
raster_line:               .byte 0

video_state:              .byte 0     // state of video kernal 0=init, 1=run normally
vga_state:                .byte VGA_STATE_TOP_OVERSCAN     // current state of VGA driver

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DATA STRUCTURES IN FLASH/ROM - NOTE: MUST ON WORD BOUNDARIES
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

.section .text

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// EXTERNALS
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

.extern vram_ptr // 16-pit pointer passed in from caller that points to video RAM

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GLOBALS EXPORTS
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// export these to caller, so he can interogate and change
.global curr_line     // read only, current physical line
.global raster_line   // read only, current logical raster line
.global palette_table // read/write, 16-bit base address to palette used for driver

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ASM FUNCTIONS 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// VGA Interface Reference
//DDRA  = 0xFF; // NTSC/PAL LUMA7:4 | CHROMA3:0, VGA7:0 [vsync, hsync, B1,B0, G1,G0, G1,G0] 
//                                                        b7      b6    b5:4   b3:2   b1:0

.section .text

// asm entry point

.global	TIMER1_COMPA_vect  
TIMER1_COMPA_vect:

Save_Regs:

    // save working registers on stack
    push    r0              // (2)        
    
    push    r16             // (2)
    push    r17             // (2)

    push    r18             // (2)
    push    r19             // (2)

    push    r20             // (2)

    push    r24             // (2)
    push    r25             // (2)

    push    r26             // (2)   X
    push    r27             // (2)

    push    r28             // (2)   Y
    push    r29             // (2)

    push    r30             // (2)   Z
    push    r31             // (2)

    in      r0, SREG        // (2)
    push    r0              // (2) save the status register

                            // (32) clocks for stack push

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  
#if 0

Notes for computational distribution for 120x96x2 mode

Overview: the amount of computation required per pixel is much greater than the number of clocks per pixel at 80 pixels 
per line at 28.636MHZ thus, a division of work must be performed where the computations for each 120 pixel line are 
divided into both the hsync itself and the active scan. This necessitates the use of very complex timing, and seperation 
of mutually exclusion work. The main idea is that at 120 lines of active video there are 5 actual VGA video lines per 
raster line at 480 lines, this there are 5 video lines per virtual raster line to perform the computations for the NEXT
video line of 84 pixels. However, this is still not enough time to complete to operation since while the computations 
are being performed video still must be output to the VGA, therefore, a dual scanline buffer is needed; one to hold the 
current line data, one to hold the new line being built. Furthermore, there isn't enough time to do all the computation 
while rendering, thus, some of the work must be done during the HSYNC. 

The final result is that there are a number of computational block boundaries where various computations take place, 
these are then used to arrive at the final result (get the next video line ready), while all staying under 910 clocks 
per scanline (VGA HSYNC rate at 31.77uS per line and the 28.636MHZ clock).

Work will be broken into 5 scanlines at a time, during which time the next video line will be computed and the current 
video line will be rendered over and over 5 times.

The computational load is broken into "splitting" the pixels out from the bytes during the hsync/blanking periods 
then the pixels are finally processed and rendered via the active scan time, very tricky in general.
#endif
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  

VGA_Start:

    // load r25:24 with curr_line
    lds     r24, curr_line          // (2)
    lds     r25, curr_line+1        // (2)      


VGA_Video_State_Test1:

    // if (curr_line < 32) then VGA_Draw_Blank_Line
    ldi     r16, hi8(VGA_VERTICAL_OFFSET)   // (1)
    cpi     r24, lo8(VGA_VERTICAL_OFFSET)   // (1)
    cpc     r25, r16                // (1)
    brlo    VGA_Draw_Blank_Line_Top // (1/2)

// elseif
VGA_Video_State_Test2:    

    // if (curr_line < (512+5) ) then VGA_Draw_Active_Line
    ldi     r16, hi8(VGA_SCREEN_HEIGHT*5+5+VGA_VERTICAL_OFFSET)         // (1) 
    cpi     r24, lo8(VGA_SCREEN_HEIGHT*5+5+VGA_VERTICAL_OFFSET)         // (1)
    cpc     r25, r16                // (1)

    //brlo    VGA_Draw_Active_Line  // (1/2) 

    brsh    VGA_Video_State_Test3   // (1/2)
    jmp     VGA_Draw_Active_Line    // (3)   VGA_Start: (15) clocks -> jump, jump need due to code out of range for branch


// elseif
VGA_Video_State_Test3:    

    // if (curr_line < 523) then VGA_Draw_Blank_Line
    ldi     r16, hi8(523)           // (1) 
    cpi     r24, lo8(523)           // (1)
    cpc     r25, r16                // (1)
    brlo    VGA_Draw_Blank_Line_Bot // (1/2)

// elseif
VGA_Video_State_Test4:    

    // if (curr_line < 525) then VGA_Draw_Vsync_Line
    ldi     r16, hi8(525)           // (1) 
    cpi     r24, lo8(525)           // (1)
    cpc     r25, r16                // (1)
    brlo    VGA_Draw_Vsync_Line     // (1/2)
    // else should never get here...

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  

VGA_Draw_Blank_Line_Top: 
VGA_Draw_Blank_Line_Bot: 

    nop                             // (1+3 below) delay to even out conditional paths to active, non-active, sync lines

    // test if this is the top of frame here, we need to initialize the rendering kernel
    // if (curr_line == 0) then VGA_Reset_Kernel
    ldi     r16, hi8(0)             // (1)
    cpi     r24, lo8(0)             // (1)
    cpc     r25, r16                // (1)

    // flags are set now, but we need to enable the hsync then finish conditional

    // enable hsync, set RGB to black
    ldi     r16, VGA_HSYNC          // (1)
    out     PORTA, r16              // (1)

    // now based on flags set above, jump to normal blank line or reset 
    breq    VGA_Reset_Kernel        // (1/2)

    // else delay hsync time (this will be filled with processing in later versions)

    // delay for hsync pulse length 108 clocks is total hsync
    // 3 clocks used for setup above,3 needed for clean up below, so need 102 more, n = 102/3
        
    // t = 3*n, n = t/3
    DELAYX r17, (129)
    
    // disble hsync, maintain black
    ldi     r16, VGA_RGB_BLACK      // (1)
    out     PORTA, r16              // (1)

    jmp     VGA_Next_Line           // (3)    

VGA_Reset_Kernel:
    // reset all data structures each frame at line 0

    // step 1: initialize any variables for algorithm    

    // step 2: assign VRAM pointer to begining of VRAM

    // vram_curr_ptr = vram_ptr
    lds     XL, vram_ptr                    // (2)
    lds     XH, vram_ptr+1                  // (2)
    sts     vram_curr_ptr, XL               // (2) 
    sts     vram_curr_ptr+1, XH             // (2)
    
    // step 3: assign front and back scanline ptrs to actually scanline buffers

    // slinebuff_front_ptr = &slinebuff0
    ldi     XL, lo8( slinebuff0 )           // (1)  
    ldi     XH, hi8( slinebuff0 )           // (1)  
    sts     slinebuff_front_ptr,   XL       // (2)
    sts     slinebuff_front_ptr+1, XH       // (2)

    // slinebuff_back_ptr = &slinebuff0
    ldi     XL, lo8( slinebuff1 )           // (1)  
    ldi     XH, hi8( slinebuff1 )           // (1)  
    sts     slinebuff_back_ptr,   XL        // (2)
    sts     slinebuff_back_ptr+1, XH        // (2)
 
    // step 4: clear out scanline buffer that is about to be rendered with garbage in it at the top of scan
    // assume X is pointing to scanline buffer

    // this delay needs to be same number of clocks as non-reset path, but hsync needs to be disabled midway, 
    // so break 120 bytes into (m + n)
    // fill bytes 0..21
    ldi     r16, 0                          // (1), r16=0, fill value

    ldi     r17, 22                         // (1) fill 120 bytes with 0's, lets do first m then the rest, since we have to interleave the hsync pulse
1:
    st      X+, r16                         // (2)
    dec     r17                             // (1)
    brne    1b                              // (1/2) while (--r17 != 0) loop

    // disble hsync, maintain black
    ldi     r16, VGA_RGB_BLACK              // (1)
    out     PORTA, r16                      // (1)

    // now finish off clear
    // fill bytes 22...119
    ldi     r17, 98                         // (1) fill 120 bytes with 0's, finish last n bytes, since we have to interleave the hsync pulse
1:
    st      X+, r16                         // (2)
    dec     r17                             // (1)
    brne    1b                              // (1/2) while (--r17 != 0) loop

    // initialize raster_line for next frame, 
    ldi     r16, (255)                      // (1), first set of lines are to build first visible line, so raster_line is 5 lines behind
    sts     raster_line, r16                // (2)

    jmp     VGA_Next_Line                   // (3)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  
VGA_Draw_Vsync_Line:

    // enable vsync and hsync, set RGB to black
    ldi     r16, VGA_HSYNC  // (1)
    ori     r16, VGA_VSYNC  // (1)
    out     PORTA, r16      // (1)

    // delay for hsync pulse length 108 clocks is total hsync
    // 3 clocks used for setup above,3 needed for clean up below, so need 102 more, n = 102/3
        
    // t = 3*n, n = t/3
    DELAYX r17, (129)    

    // disble hsync, but maintain vsync and black
    ldi     r16, VGA_VSYNC  // (1)
    out     PORTA, r16      // (1)

    jmp     VGA_Next_Line   // (3)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  

// enter (32 stack push) + (13 scanline conditional) = (46 clocks)
VGA_Draw_Active_Line:

    // enable hsync, set RGB to black
    ldi     r16, VGA_HSYNC                  // (1)
    out     PORTA, r16                      // (1)

    // test which sub-line is being drawn from 0, 1...4
    lds     r20, curr_sub_line              // (2), r20 holds subline thru line
    cpi     r20, 0                          // (1)
    breq    VGA_Subline_0                   // (1/2)        VGA_Draw_Active_Line: (7) clocks -> jump
    jmp     VGA_Subline_1_4                 // (3)

// (56 clocks to here from entry at stack push)
VGA_Subline_0:    
    // this section handles the first line of each set of 5 scanlines
    //nop                                     // (2), delay to equalize conditional branching into line handlers
    //nop

    // step 8: update raster line
    lds     r0, raster_line                 // (2)
    inc     r0                              // (1), add 1 to the current raster line this will be used to compute the active palette address
    sts     raster_line, r0                 // (2)

    // step 1: swap scanline buffer pointers ( slinebuff_front_ptr <--> slinebuff_back_ptr )
    lds     r16, slinebuff_front_ptr        // (2)  (16) clocks for swap
    lds     r17, slinebuff_front_ptr+1      // (2)

    lds     r18, slinebuff_back_ptr         // (2)
    lds     r19, slinebuff_back_ptr+1       // (2)
    
    sts     slinebuff_front_ptr,   r18      // (2)
    sts     slinebuff_front_ptr+1, r19      // (2)

    sts     slinebuff_back_ptr,    r16      // (2)
    sts     slinebuff_back_ptr+1,  r17      // (2)

    // step 2: restore X from VRAM saved current ptr   
    lds     XL, vram_curr_ptr               // (2)  (4) for restore
    lds     XH, vram_curr_ptr+1             // (2)
    
    // step 3: assign Y from head of scanline back buffer ptr 
    lds     YL, slinebuff_back_ptr          // (2)  (4) for assignment
    lds     YH, slinebuff_back_ptr+1        // (2)

    // step 4: save head of back sline buffer to save sline pointer, these next operations omitted in next line handler, (8) clocks
    sts     slinebuff_back_save_ptr, YL     // (2)  (4) for save of pointer
    sts     slinebuff_back_save_ptr+1, YH   // (2)

    // step 5: split 6 blocks of 4 pixels = 24 pixels, macro assumes that on entry X -> VRAM, Y -> scanline buffer
    // 16 clocks per split, 6 splits * 16 clocks per split = 96 clocks 
    .rept 6
    SPLIT_PIXEL4 // rt = r0
    .endr

    // turn HSYNC off
    ldi     r16, VGA_RGB_BLACK              // (1)
    out     PORTA, r16                      // (1)


    // step 6: save VRAM ptr from X
    sts     vram_curr_ptr, XL               // (2) (4) clocks for save
    sts     vram_curr_ptr+1, XH             // (2)

    // step 7: increment current sub-line, compare omitted in this case, but in 1..4 case, consider in timing
    inc     r20                             // (1)  (3) clocks for increment and store
    sts     curr_sub_line, r20              // (2)


    // step 9: draw pixels on line now
    jmp     VGA_Draw_Pixels                 // (3)     

VGA_Subline_1_4:
    // this section handles the remaining scanlines 1..4 of the set of 5 scanlines, slightly different setup

    // swap operation omitted, also raster_line update, but this sections add the line compare, consider both. 
    // This time can be used for other computation
    .rept (16-3+3)                          // (16-3+3)
    nop
    .endr

    // step 1: restore X from VRAM saved current ptr   
    lds     XL, vram_curr_ptr               // (2)
    lds     XH, vram_curr_ptr+1             // (2)
    
    // step 2: assign Y from saved sline pointer
    lds     YL, slinebuff_back_save_ptr     // (2)
    lds     YH, slinebuff_back_save_ptr+1   // (2)

    // save operations ommitted, this time can be used for other computations
    .rept 4                                 // (4)
    nop
    .endr

    // step 3: draw 6 blocks of 4 pixels = 24 pixels, macro assumes that on entry X -> VRAM, Y -> scanline buffer
    .rept 6
    SPLIT_PIXEL4 // rt = r0
    .endr

    // turn HSYNC off
    ldi     r16, VGA_RGB_BLACK              // (1)
    out     PORTA, r16                      // (1)

    // step 4: save VRAM ptr from X
    sts     vram_curr_ptr, XL               // (2)
    sts     vram_curr_ptr+1, XH             // (2)

    // step 5: increment current sub-line (stored in r20)
    inc     r20                             // (1)

    // this section added in this line case, omitted in above case, but above has store (2), 
    // total time thru code = (5), net difference (5-2)=(3)
    // if (r20 == 5) r20 = 0
    cpi     r20, 5                          // (1)
    brne    2f                              // (1/2)

1:  // this resets the line

    ldi     r20, 0                          // (1)
    sts     curr_sub_line, r20              // (2)

    // step 6: draw pixels on line now
    jmp     VGA_Draw_Pixels                 // (3)     

2:  // this doesn't reset the line

    sts     curr_sub_line, r20              // (2)

    // step 6: draw pixels on line now
    jmp     VGA_Draw_Pixels                 // (3)     

VGA_Draw_Pixels:

    // step 1: point X at current front buffer, this contains processed pixels ready for rendering
    // the rendering is always of the same 120 pixels for all 5 lines, then the system has the new line 
    // ready in the back scanline buffer which is swaped in
    lds     XL, slinebuff_front_ptr              // (2)     (4) clocks for setup of X
    lds     XH, slinebuff_front_ptr+1            // (2)    

    // step 2: point Y at current back buffer segment currently being re-processed and finished
    lds     YL, slinebuff_back_save_ptr          // (2)     (4) clocks for setup of Y
    lds     YH, slinebuff_back_save_ptr+1        // (2)

    // step 3: point Z at color palette table which has 4 entries for each palette, 0..5, which cover each 16 line span of scanlines

    lds     r16, raster_line                      // (2), total of 8 clocks for setup to support palettes
    andi    r16, 0xF8                             // (1), mask upper 4-bits
    lsr     r16                                   // (1), ZL = (raster_line / 8) * 4
    //lsr     r16                                 // (1) 
    

    ldi     ZL, lo8( palette_table )              // (1)  
    add     ZL, r16                               // (1), ZL = palette_low + (raster_line / 8) * 4
    ldi     ZH, hi8( palette_table )              // (1)

    // step 4: render the pixels and perform computation on next 24 pixel segment to build up next scanline of pixels 
    // 24 sets of 5 pixels = 120 pixels, each pixel take 5 clocks, 25 clocks per set, 24 sets * 25 clocks per set = 600
    .rept 24
    DRAW_PIXELS_W_COMPV1  // rpix = r16, rraw = r17, rrgb = r18
    .endr

    // and back to black
    ldi     r16, 0x00           // (1)
    out     PORTA, r16          // (1)
   
    // step 5: save the current back buffer working position
    sts     slinebuff_back_save_ptr,   YL   // (2)  (4) for save of pointer
    sts     slinebuff_back_save_ptr+1, YH   // (2)


    jmp     VGA_Next_Line                   // (3)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  

VGA_Next_Line:

    // curr_line++
    adiw    r24, 1                 // (2)

    // if (curr_line >= 525) curr_line = 0
    ldi     r16, hi8(525)          // (1) 
    cpi     r24, lo8(525)          // (1)
    cpc     r25, r16               // (1)
    brne    VGA_End_Line           // (1/2)

    // curr_line = 0
    ldi     r24, 0                 // (1)
    ldi     r25, 0                 // (1)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  
VGA_End_Line:

    // save registers back to curr_line in SRAM for exit back to C/C++ caller
    sts     curr_line,   r24       // (2)
    sts     curr_line+1, r25       // (2)

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////  

Restore_Regs:

    // restore the status register
    pop     r0          // (2)
    out     SREG, r0    // (1)        

    // restore working registers
    pop     r31         // (2), Z
    pop     r30         // (2)

    pop     r29         // (2), Y
    pop     r28         // (2)

    pop     r27         // (2), X
    pop     r26         // (2)

    pop     r25         // (2)
    pop     r24         // (2)

    pop     r20         // (2)

    pop     r19         // (2)
    pop     r18         // (2)

    pop     r17         // (2)
    pop     r16         // (2)

    pop     r0          // (2)

#if 0
    .rept 50
    nop
    .endr
#endif
    
    reti                // (5) return from interrupt and reset interupt flag, total time of restore 36 clocks for restore

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// END ASM FUNCTIONS 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

.end

