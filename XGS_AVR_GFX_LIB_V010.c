///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Copyright Nurve Networks LLC 2008
// 
// Filename: XGS_AVR_GFX_LIB_V010.c
// 
// Last Modified: 9.1.08
//
// Description: 
//
// Overview:
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// define chip type here
#define __AVR_ATmega644__

// define CPU frequency in Mhz here if not defined in Makefile, this is used for conditional compilation, so needs
// to be first, before other XGS includes, also note there is a copy of this constant in XGS_AVR_SYSTEM_V010, so if you 
// change it here, also change it in the header to make certain everthing is sync'ed
#define F_CPU 28636360UL

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <avr/eeprom.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <inttypes.h>
#include <compat/twi.h>
#include <avr/pgmspace.h>

// include local XGS API header files
#include "XGS_AVR_SYSTEM_V010.h"
#include "XGS_AVR_GFX_LIB_V010.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINES AND MACROS /////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TYPES //////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GLOBALS/////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

volatile UCHAR *tile_map_ptr       = NULL;
volatile UCHAR *tile_bitmaps_ptr   = NULL;
volatile UCHAR *vram_ptr           = NULL;

// screen screen variable for bitmap and tile modes
int g_screen_width    = 0;
int g_screen_height   = 0;

int g_tile_map_width  = 0;
int g_tile_map_height = 0;

// clipping variables
int g_clip_min_x      = 0;
int g_clip_max_x      = 0;
int g_clip_min_y      = 0;
int g_clip_max_y      = 0;

// these globals keep track of the tile map console printing cursor position
int g_tmap_cursor_x = 0;
int g_tmap_cursor_y = 0;

// storage for our lookup tables, for now declare as size 1 until FLASH is working
float cos_look[1]; // 361, 1 extra so we can store 0-360 inclusive
float sin_look[1]; // 361, 1 extra so we can store 0-360 inclusive

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// EXTERNALS //////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// all of these are exported from the ASM drivers always
volatile extern unsigned int  curr_line;
volatile extern unsigned char curr_raster_line;
volatile extern unsigned char tile_map_logical_width;
volatile extern char          tile_map_vscroll;
volatile extern unsigned char palette_table[];

// single missile for VGA tile mode 1 (exported from ASM driver)
volatile extern unsigned char missile_x;
volatile extern unsigned char missile_y; 
volatile extern unsigned char missile_color;
volatile extern unsigned char missile_state;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PROTOTYPES /////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FUNCTIONS  /////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GRAPHICS FUNCTIONS /////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void GFX_Init(int video_interrupt_rate)
{
// this function initializes the I/O ports for NTSC/PAL/VGA graphics as well as initilizes variables and turns on the 
// driver interrupt which will immediately start feeding the data sources to the currently installed ASM driver, thus
// make sure any pointers to data structures are initialized before calling this function
// when done with graphics and you want to stop calling the graphics interrupt, call GFX_Shutdown(..)
//
// Inputs:
//     video_interrupt_rate - the clock divider, so that the number of clocks per interrupt is equal to the HSYNC rate
//                            since the interrupt handler runs every HSYNC in both NTSC/PAL and VGA modes

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// VIDEO INTERRUPT EXPERIMENT /////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// main graphics port that luma/chroma and VGA signals come from, port A
#define GFX_MAIN_PORT       PORTA
#define GFX_MAIN_DDR        DDRA

// the "saturation" control is connected to port B
#define GFX_SATURATION_PORT PORTB
#define GFX_SATURATION_DDR  DDRB

// set video ports to output
GFX_MAIN_DDR  = 0xFF; // NTSC/PAL LUMA7:4 | CHROMA3:0, VGA7:0 [vsync, hsync, B1,B0, G1,G0, G1,G0] 
              //                                        b7      b6    b5:4   b3:2   b1:0
GFX_MAIN_PORT = 0x00;

// set sat control bits PB2, PB3 as outputs
GFX_SATURATION_DDR  =  SETPORTBITS(0,0,0,0,1,1,0,0);
PORTB               =  SETPORTBITS(0,0,0,0,1,1,0,0);

// set up timer                                     
TCCR1A = (0 << COM1A1) | (0 << COM1A0) | (0 << COM1B1) | (0 << COM1B0) | (0 << WGM11) | (0 << WGM10);

// set clock pre-scaler and counter mode
TCCR1B = (0 << WGM13) | (1 << WGM12) | (0 << CS12) | (0 << CS11)  | (1 << CS10);    

// 168. 644 use TIMSK1,TIFR1
// 162 use TIMSK,TIFR

// enable timer interrupts for timer 
TIMSK1 = (1 << OCIE1A);                  //(1<<TOIE1);                 

// clear and pendings timer x CTC interrupts (1 clears it)
TIFR1  = (1 << OCF1A); // (1<<TOV1);                  

// set output compare register value
OCR1A  = video_interrupt_rate; // 1817+-1 for NTSC at 28.636 Mhz = 15.748Hkz hsync rate, 910 for VGA @ 28.636Mhz = 31.468Khz, hsync rate

// enable interrupt
sei();

} // end GFX_Init

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void GFX_Fill_Screen(int c, UCHAR *vbuffer)
{
// this functions fills the entire screen with the sent 2-bit color

memset( vbuffer, (UCHAR)( c | (c << 2) | (c << 4) | (c << 6) ), (g_screen_width >> 2) * g_screen_height );

} // end GFX_Fill_Screen

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void GFX_Fill_Region(int c, int y1, int y2, UCHAR *vbuffer)
{
// this functions fills a vertical region from y1 to y2 with the sent 2-bit color

int ytemp;

// clip
if ( y1 < 0 )
  y1 = 0;
else 
if ( y1 >= g_screen_height )
  y1 = g_screen_height-1;

if ( y2 < 0 )
  y2 = 0;
else
if ( y2 >= g_screen_height )
  y2 = g_screen_height-1;

// swap?
if (y2 < y1)
    {
    ytemp = y1;
    y1 = y2;
    y2 = ytemp;
    } // end if

// and fill memory
memset( vbuffer + (y1*(g_screen_width >> 2)), (UCHAR)( c | (c << 2) | (c << 4) | (c << 6) ), (g_screen_width >> 2) * (y2 - y1 + 1) );

} // end GFX_Fill_Region


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline void GFX_Plot(int x, int y, int c, UCHAR *vbuffer)
{
// plots a pixel on the screen at (x,y) in color c
// there are 2 bits per pixel, thus to locate the pixel, we need to divide the x by 4 and add that
// to the y * SCREEN_WIDTH/4, then that byte contains the pixel, next we need to shift the color over 0..3 pixels over
// where each pixel is 2-bit, and AND/OR mask it into the video byte
// Note: later convert to pure ASM, also try optimizing common terms (compiler should do this?)
// shifts are slower than multiples, so on AVR the mutliplications should be faster?

vbuffer [ (x >> 2) + (y * (g_screen_width >> 2) ) ] = 
                 (vbuffer [ (x >> 2) + (y * (g_screen_width >> 2)) ] & ~(0x03 << 2*(x & 0x03)) ) | (c << 2*(x & 0x03));

} // end GFX_Plot

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline void GFX_Plot_Clipped(int x, int y, int c, UCHAR *vbuffer)
{
// plots a pixel on the screen at (x,y) in color c
// there are 2 bits per pixel, thus to locate the pixel, we need to divide the x by 4 and add that
// to the y * SCREEN_WIDTH/4, then that byte contains the pixel, next we need to shift the color over 0..3 pixels over
// where each pixel is 2-bit, and AND/OR mask it into the video byte
// Note: later convert to pure ASM, also try optimizing common terms (compiler should do this?)
// shifts are slower than multiples, so on AVR the mutliplications should be faster?

// this version clips to the clipping rectangle which is global
if ( (x > g_clip_max_x) || ( x < g_clip_min_x ) || ( y > g_clip_max_y ) || ( y < g_clip_min_y ) )
    return;

// write the pixel to video RAM
vbuffer [ (x >> 2) + (y * (g_screen_width >> 2)) ] = 
                (vbuffer [ (x >> 2) + (y * (g_screen_width >> 2)) ] & ~(0x03 << 2*(x & 0x03)) ) | (c << 2*(x & 0x03));

} // end GFX_Plot_Clipped

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

unsigned char GFX_Get_Pixel(int x, int y, UCHAR *vbuffer)
{
// returns the 2-bit pixel code at screen location (x,y)
// there are 2 bits per pixel, thus to locate the pixel, we need to divide the x by 4 and add that
// to the y * SCREEN_WIDTH/4, then that byte contains the pixel, once retrieved, we need to shift the color over 0..3 pixels over
// where each pixel is 2-bit, and AND mask it to retrieve the pixel and then return it to caller
// Note: later convert to pure ASM, also try optimizing common terms (compiler should do this?)
// shifts are slower than multiples, so on AVR the mutliplications should be faster?

return ( (vbuffer [ (x >> 2) + (y * (g_screen_width >> 2)) ] >> (2*(x & 0x03))) & (0x03) );

} // end GFX_Get_Pixel

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void GFX_Set_Clipping_Rect(int clip_min_x, int clip_min_y, int clip_max_x, int clip_max_y)
{
// this function sets the clipping rectangle for ALL graphics functions that clip to a 2D viewplane; NTSC, VGA, etc.

g_clip_min_x = clip_min_x;
g_clip_max_x = clip_max_x;
g_clip_min_y = clip_min_y;
g_clip_max_y = clip_max_y;

} // end GFX_Set_Clipping_Rect

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void GFX_Draw_Clip_Line(int x0,int y0, int x1, int y1, int color, UCHAR *dest_buffer)
{
// this function draws a wireframe triangle

int cxs, cys,
	cxe, cye;

// clip and draw each line
cxs = x0;
cys = y0;
cxe = x1;
cye = y1;

// clip the line
if (GFX_Clip_Line(&cxs,&cys,&cxe,&cye))
	GFX_Draw_Line(cxs, cys, cxe,cye,color,dest_buffer);

} // end GFX_Draw_Clip_Line

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int GFX_Clip_Line(int *_x1,int *_y1,int *_x2, int *_y2)
{
// this function clips the sent line using the globally defined clipping
// region

// internal clipping codes
#define CLIP_CODE_C  0x0000
#define CLIP_CODE_N  0x0008
#define CLIP_CODE_S  0x0004
#define CLIP_CODE_E  0x0002
#define CLIP_CODE_W  0x0001

#define CLIP_CODE_NE 0x000a
#define CLIP_CODE_SE 0x0006
#define CLIP_CODE_NW 0x0009
#define CLIP_CODE_SW 0x0005

int x1=*_x1,
    y1=*_y1,
	x2=*_x2,
	y2=*_y2;

int xc1=x1,
    yc1=y1,
	xc2=x2,
	yc2=y2;

int p1_code=0,
    p2_code=0;

// determine codes for p1 and p2
if (y1 < g_clip_min_y)
	p1_code|=CLIP_CODE_N;
else
if (y1 > g_clip_max_y)
	p1_code|=CLIP_CODE_S;

if (x1 < g_clip_min_x)
	p1_code|=CLIP_CODE_W;
else
if (x1 > g_clip_max_x)
	p1_code|=CLIP_CODE_E;

if (y2 < g_clip_min_y)
	p2_code|=CLIP_CODE_N;
else
if (y2 > g_clip_max_y)
	p2_code|=CLIP_CODE_S;

if (x2 < g_clip_min_x)
	p2_code|=CLIP_CODE_W;
else
if (x2 > g_clip_max_x)
	p2_code|=CLIP_CODE_E;

// try and trivially reject
if ((p1_code & p2_code))
	return(0);

// test for totally visible, if so leave points untouched
if (p1_code==0 && p2_code==0)
	return(1);

// determine end clip point for p1
switch(p1_code)
	  {
	  case CLIP_CODE_C: break;

	  case CLIP_CODE_N:
		   {
		   yc1 = g_clip_min_y;
		   xc1 = x1 + 0.5+(g_clip_min_y-y1)*(x2-x1)/(y2-y1);
		   } break;
	  case CLIP_CODE_S:
		   {
		   yc1 = g_clip_max_y;
		   xc1 = x1 + 0.5+(g_clip_max_y-y1)*(x2-x1)/(y2-y1);
		   } break;

	  case CLIP_CODE_W:
		   {
		   xc1 = g_clip_min_x;
		   yc1 = y1 + 0.5+(g_clip_min_x-x1)*(y2-y1)/(x2-x1);
		   } break;

	  case CLIP_CODE_E:
		   {
		   xc1 = g_clip_max_x;
		   yc1 = y1 + 0.5+(g_clip_max_x-x1)*(y2-y1)/(x2-x1);
		   } break;

	// these cases are more complex, must compute 2 intersections
	  case CLIP_CODE_NE:
		   {
		   // north hline intersection
		   yc1 = g_clip_min_y;
		   xc1 = x1 + 0.5+(g_clip_min_y-y1)*(x2-x1)/(y2-y1);

		   // test if intersection is valid, of so then done, else compute next
			if (xc1 < g_clip_min_x || xc1 > g_clip_max_x)
				{
				// east vline intersection
				xc1 = g_clip_max_x;
				yc1 = y1 + 0.5+(g_clip_max_x-x1)*(y2-y1)/(x2-x1);
				} // end if

		   } break;

	  case CLIP_CODE_SE:
      	   {
		   // south hline intersection
		   yc1 = g_clip_max_y;
		   xc1 = x1 + 0.5+(g_clip_max_y-y1)*(x2-x1)/(y2-y1);

		   // test if intersection is valid, of so then done, else compute next
		   if (xc1 < g_clip_min_x || xc1 > g_clip_max_x)
		      {
			  // east vline intersection
			  xc1 = g_clip_max_x;
			  yc1 = y1 + 0.5+(g_clip_max_x-x1)*(y2-y1)/(x2-x1);
			  } // end if

		   } break;

	  case CLIP_CODE_NW:
      	   {
		   // north hline intersection
		   yc1 = g_clip_min_y;
		   xc1 = x1 + 0.5+(g_clip_min_y-y1)*(x2-x1)/(y2-y1);

		   // test if intersection is valid, of so then done, else compute next
		   if (xc1 < g_clip_min_x || xc1 > g_clip_max_x)
		      {
			  xc1 = g_clip_min_x;
		      yc1 = y1 + 0.5+(g_clip_min_x-x1)*(y2-y1)/(x2-x1);
			  } // end if

		   } break;

	  case CLIP_CODE_SW:
		   {
		   // south hline intersection
		   yc1 = g_clip_max_y;
		   xc1 = x1 + 0.5+(g_clip_max_y-y1)*(x2-x1)/(y2-y1);

		   // test if intersection is valid, of so then done, else compute next
		   if (xc1 < g_clip_min_x || xc1 > g_clip_max_x)
		      {
			  xc1 = g_clip_min_x;
		      yc1 = y1 + 0.5+(g_clip_min_x-x1)*(y2-y1)/(x2-x1);
			  } // end if

		   } break;

	  default:break;

	  } // end switch

// determine clip point for p2
switch(p2_code)
	  {
	  case CLIP_CODE_C: break;

	  case CLIP_CODE_N:
		   {
		   yc2 = g_clip_min_y;
		   xc2 = x2 + (g_clip_min_y-y2)*(x1-x2)/(y1-y2);
		   } break;

	  case CLIP_CODE_S:
		   {
		   yc2 = g_clip_max_y;
		   xc2 = x2 + (g_clip_max_y-y2)*(x1-x2)/(y1-y2);
		   } break;

	  case CLIP_CODE_W:
		   {
		   xc2 = g_clip_min_x;
		   yc2 = y2 + (g_clip_min_x-x2)*(y1-y2)/(x1-x2);
		   } break;

	  case CLIP_CODE_E:
		   {
		   xc2 = g_clip_max_x;
		   yc2 = y2 + (g_clip_max_x-x2)*(y1-y2)/(x1-x2);
		   } break;

		// these cases are more complex, must compute 2 intersections
	  case CLIP_CODE_NE:
		   {
		   // north hline intersection
		   yc2 = g_clip_min_y;
		   xc2 = x2 + 0.5+(g_clip_min_y-y2)*(x1-x2)/(y1-y2);

		   // test if intersection is valid, of so then done, else compute next
			if (xc2 < g_clip_min_x || xc2 > g_clip_max_x)
				{
				// east vline intersection
				xc2 = g_clip_max_x;
				yc2 = y2 + 0.5+(g_clip_max_x-x2)*(y1-y2)/(x1-x2);
				} // end if

		   } break;

	  case CLIP_CODE_SE:
      	   {
		   // south hline intersection
		   yc2 = g_clip_max_y;
		   xc2 = x2 + 0.5+(g_clip_max_y-y2)*(x1-x2)/(y1-y2);

		   // test if intersection is valid, of so then done, else compute next
		   if (xc2 < g_clip_min_x || xc2 > g_clip_max_x)
		      {
			  // east vline intersection
			  xc2 = g_clip_max_x;
			  yc2 = y2 + 0.5+(g_clip_max_x-x2)*(y1-y2)/(x1-x2);
			  } // end if

		   } break;

	  case CLIP_CODE_NW:
      	   {
		   // north hline intersection
		   yc2 = g_clip_min_y;
		   xc2 = x2 + 0.5+(g_clip_min_y-y2)*(x1-x2)/(y1-y2);

		   // test if intersection is valid, of so then done, else compute next
		   if (xc2 < g_clip_min_x || xc2 > g_clip_max_x)
		      {
			  xc2 = g_clip_min_x;
		      yc2 = y2 + 0.5+(g_clip_min_x-x2)*(y1-y2)/(x1-x2);
			  } // end if

		   } break;

	  case CLIP_CODE_SW:
		   {
		   // south hline intersection
		   yc2 = g_clip_max_y;
		   xc2 = x2 + 0.5+(g_clip_max_y-y2)*(x1-x2)/(y1-y2);

		   // test if intersection is valid, of so then done, else compute next
		   if (xc2 < g_clip_min_x || xc2 > g_clip_max_x)
		      {
			  xc2 = g_clip_min_x;
		      yc2 = y2 + 0.5+(g_clip_min_x-x2)*(y1-y2)/(x1-x2);
			  } // end if

		   } break;

	  default:break;

	  } // end switch

// do bounds check
if ((xc1 < g_clip_min_x) || (xc1 > g_clip_max_x) ||
	(yc1 < g_clip_min_y) || (yc1 > g_clip_max_y) ||
	(xc2 < g_clip_min_x) || (xc2 > g_clip_max_x) ||
	(yc2 < g_clip_min_y) || (yc2 > g_clip_max_y) )
	{
	return(0);
	} // end if

// store vars back
*_x1 = xc1;
*_y1 = yc1;
*_x2 = xc2;
*_y2 = yc2;

return(1);

} // end GFX_Clip_Line

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void GFX_Draw_Line(int x0, int y0,  // starting position
                   int x1, int y1,  // ending position
                   int color,       // color index
                   UCHAR *vb_start) // video buffer
{
// this function draws a line from x0,y0 to x1,y1 using differential error analysis
// terms (based on Bresenahams work)

int dx,             // difference in x's
    dy,             // difference in y's
    dx2,            // dx,dy * 2
    dy2,
    x_inc,          // amount in pixel space to move during drawing
    y_inc,          // amount in pixel space to move during drawing
    error,          // the discriminant i.e. error i.e. decision variable
    index;          // used for looping

// pre-compute first pixel address in video buffer
// vb_start = vb_start + x0 + y0*lpitch;


// compute horizontal and vertical deltas
dx = x1-x0;
dy = y1-y0;

// test which direction the line is going in i.e. slope angle
if (dx>=0)
   {
   x_inc = 1;

   } // end if line is moving right
else
   {
   x_inc = -1;
   dx    = -dx;  // need absolute value

   } // end else moving left

// test y component of slope

if (dy>=0)
   {
   y_inc = 1;
   } // end if line is moving down
else
   {
   y_inc = -1;
   dy    = -dy;  // need absolute value

   } // end else moving up

// compute (dx,dy) * 2
dx2 = dx << 1;
dy2 = dy << 1;

// now based on which delta is greater we can draw the line
if (dx > dy)
   {
   // initialize error term
   error = dy2 - dx;

   // draw the line
   for (index=0; index <= dx; index++)
       {
       // set the pixel
       // *vb_start = color;

       // plot the pixel, this can definitely be optimized with pre-computation etc. however, due to the pixel packing, it slows
       // down the direct addressing optimizations available with 1 byte per pixel
       vb_start [ (x0 >> 2) + (y0 * (g_screen_width >> 2) ) ] = 
                 (vb_start [ (x0 >> 2) + (y0 * (g_screen_width >> 2)) ] & ~(0x03 << 2*(x0 & 0x03)) ) | (color << 2*(x0 & 0x03));

       // test if error has overflowed
       if (error >= 0)
          {
          error-=dx2;

          // move to next line
          y0 += y_inc;

	   } // end if error overflowed

       // adjust the error term
       error+=dy2;

       // move to the next pixel
       x0 += x_inc;

       } // end for

   } // end if |slope| <= 1
else
   {
   // initialize error term
   error = dx2 - dy;

   // draw the line
   for (index=0; index <= dy; index++)
       {
       // set the pixel
       //*vb_start = color;

       // plot the pixel, this can definitely be optimized with pre-computation etc. however, due to the pixel packing, it slows
       // down the direct addressing optimizations available with 1 byte per pixel
       vb_start [ (x0 >> 2) + (y0 * (g_screen_width >> 2) ) ] = 
                 (vb_start [ (x0 >> 2) + (y0 * (g_screen_width >> 2)) ] & ~(0x03 << 2*(x0 & 0x03)) ) | (color << 2*(x0 & 0x03));

       // test if error overflowed
       if (error >= 0)
          {
          error-=dy2;

          // move to next line
          x0 += x_inc;

          } // end if error overflowed

       // adjust the error term
       error+=dx2;

       // move to the next pixel
       y0 += y_inc;

       } // end for

   } // end else |slope| > 1

} // end GFX_Draw_Line

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void GFX_Draw_HLine(int x1,int x2,int y, UCHAR c, UCHAR *vbuffer)
{
// draw a horizontal line using the memset function, end points are computed as special case, then the longest run
// of 4-pixels at a time is rendered ...|XXXXXXXXXXXXXXXXXXXXXXXXX|...
// clips to clipping rectangle, code is long due to special cases, but is fast

int temp;                    // used for temporary storage during endpoint swap
int xs, xe, dx;              // holds start/end byte to render into
UCHAR xp1, xp2;              // holds sub-pixel accurate pixels in each 4-pixel packed block
UCHAR mask, pixels;          // holds bit mask and pixels being drawn
unsigned int mask16, pixels16; // holds 16 bit version, bit mask and pixels being drawn

// these tables are for the special case of < 4 pixels, since it can straddle a byte boundary
// these lookups will cut down on computation
UCHAR colors[3*4] = { 0b00000000, 0b00000001, 0b00000010,  0b00000011,
                      0b00000000, 0b00000101, 0b00001010,  0b00001111,
                      0b00000000, 0b00010101, 0b00101010,  0b00111111 };

UCHAR bitmasks[1*3] = { 0b00000011, 0b00001111, 0b00111111 }; 

// perform trivial rejections
if (y > g_clip_max_y || y < g_clip_min_y)
   return;

// sort x1 and x2, so that x2 > x1
if (x1 > x2)
   {
   temp = x1;
   x1   = x2;
   x2   = temp;
   } // end swap

// perform trivial rejections
if (x1 > g_clip_max_x || x2 < g_clip_min_x)
   return;

// now clip
x1 = ((x1 < g_clip_min_x) ? g_clip_min_x : x1);
x2 = ((x2 > g_clip_max_x) ? g_clip_max_x : x2);

// advance vbuffer to the proper line
vbuffer += (y * (g_screen_width >> 2));

// draw the row of pixels, special case needed for endpoints due to pixel packing per byte [p3 | p2 | p1 | p0] 
// which are drawn [p0 | p1 | p2 | p3]

// first test if there are less than 4 pixels to draw? This is a special icky case since it can straddle a 
if ( (dx = 1 + x2 - x1) < 4 )
    {
    // compute initial byte position and pixel within byte
    xs  = x1 >> 2;            // xs  = x1 / 4
    xp1 = (x1 & 0x03) << 1;   // xp1 = (x1 mod 4)*2

    // set up to draw 1-3 pixels of color 0,1,2,3
    pixels16 = colors[ ( (dx-1) << 2) + c ] << xp1;
    mask16   = bitmasks[ (dx-1) ] << xp1;

    // advance screen pointer to start x postion
    vbuffer += xs;

    // write the pixels at one time with 16-bit write
    *(unsigned int *)vbuffer = ( (*(unsigned int *)vbuffer & ~mask16) | pixels16);

    // return
    return;
    } // end if

xs  = x1 >> 2;   // xs  = x1 / 4
xp1 = x1 & 0x03; // xp1 = x1 mod 4;

// which case are we dealing with?
switch (xp1)
    {
    case 1:
        {
        // draw pixel 1,2,3, _XXX|
        mask    = ~0b11111100;
        pixels  = ( c << 2 ) | ( c << 4) | (c << 6);
        vbuffer[ xs ] = (vbuffer[ xs ] & mask) | pixels;

        // advance xs 
        xs++;
        } break;
    
    case 2:
        {
        // draw pixel 2,3, __XX|
        mask    = ~0b11110000;
        pixels  = ( c << 4) | (c << 6);
        vbuffer[ xs ] = (vbuffer[ xs ] & mask) | pixels;

        // advance xs 
        xs++;

        } break;

    case 3:
        {
        // draw pixel 3, ___X|
        mask    = ~0b11000000;
        pixels  = (c << 6);
        vbuffer[ xs ] = (vbuffer[ xs ] & mask) | pixels;

        // advance xs 
        xs++;

        } break;

    } // end switch

// compute end point metrics
xe  = (x2+1) >> 2;  // xe  = (x2+1) / 4
xp2 = x2 & 0x03;    // xp2 = x2 mod 4;

// draw solid bytes in between endpoints, compute how many solid blocks of 4 pixels to draw?
dx = xe - xs;

pixels = ( (c) | ( c << 2 ) | ( c << 4) | (c << 6) );
memset ( vbuffer + xs, pixels, dx );

// and finally, the trailing pixels
xs += dx;

// which case are we dealing with?
switch (xp2)
    {
    case 0:
        {
        // draw pixel 0, X___|
        mask    = ~0b00000011;
        pixels  = (c);
        vbuffer[ xs ] = (vbuffer[ xs ] & mask) | pixels;

        } break;
    
    case 1:
        {
        // draw pixel 0,1, XX__|
        mask    = ~0b00001111;
        pixels  = ( c ) | (c << 2);
        vbuffer[ xs ] = (vbuffer[ xs ] & mask) | pixels;

        } break;

    case 2:
        {
        // draw pixel 0,1,2, XXX_|
        mask    = ~0b00111111;
        pixels  = ( c ) | ( c << 2 ) | ( c << 4);
        vbuffer[ xs ] = (vbuffer[ xs ] & mask) | pixels;

        } break;

    } // end switch

} // end GFX_Draw_HLine

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void GFX_Draw_VLine(int y1,int y2,int x,int c,UCHAR *vbuffer)
{
// draw a vertical line, note that a memset function can no longer be
// used since the pixel addresses are no longer contiguous in memory
// note that the end points of the line must be on the screen

UCHAR *start_offset; // starting memory offset of line

int index, // loop index
    temp;  // used for temporary storage during swap

UCHAR xp1;          // holds sub-pixel accurate pixels in each 4-pixel packed block
UCHAR mask, pixels; // holds bit mask and pixels being drawn

// perform trivial rejections
if (x > g_clip_max_x || x < g_clip_min_x)
   return;

// make sure y2 > y1
if (y1>y2)
   {
   temp = y1;
   y1   = y2;
   y2   = temp;
   } // end swap

// perform trivial rejections
if (y1 > g_clip_max_y || y2 < g_clip_min_y)
   return;

// now clip
y1 = ((y1 < g_clip_min_y) ? g_clip_min_y : y1);
y2 = ((y2 > g_clip_max_y) ? g_clip_max_y : y2);

// compute starting position byte position at x, y1
start_offset = vbuffer + y1*(g_screen_width >> 2) + (x >> 2);

// build mask and pixels
// compute initial byte position and pixel within byte
xp1 = ((x & 0x03) << 1); // xp1 = (x1 mod 4)*2

// get pixel and mask ready to iterate pixels into screen
pixels = (c << xp1);
mask   = (0x03 << xp1);

// draw line one pixel at a time
for (index=0; index<=y2-y1; index++)
    {
    // set the pixel, mask first then OR pixels
    *start_offset = ( (*start_offset & ~mask) | pixels);

    // move downward to next line
    start_offset+=(g_screen_width >> 2);

    } // end for index

} // end GFX_Draw_VLine

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

GFX_Draw_Circle(int x0, int y0, int r, int c, UCHAR *vbuffer)
{
// draws a circle at (x0, y0) with radius r and color c, based on DDA, midpoint algorithm, aka bresenham's circle
// basically uses differential analysis, that is the change from one iteration to another to compute the terms
// of the curve
int f = 1 - r;
int ddF_x = 1;
int ddF_y = -2 * r;
int x = 0;
int y = r;

// draw the 4 endpoints
GFX_Plot(x0, y0 + r, c, vbuffer);
GFX_Plot(x0, y0 - r, c, vbuffer);
GFX_Plot(x0 + r, y0, c, vbuffer);
GFX_Plot(x0 - r, y0, c, vbuffer);

// enter loop and rasterize curve
while (x < y) 
    {
    
    if(f >= 0) 
      {
      y--;
      ddF_y += 2;
      f += ddF_y;
      } // end if

  x++;
  ddF_x += 2;
  f += ddF_x;    

  // draw all octants
  GFX_Plot(x0 + x, y0 + y, c, vbuffer);
  GFX_Plot(x0 - x, y0 + y, c, vbuffer);
  GFX_Plot(x0 + x, y0 - y, c, vbuffer);
  GFX_Plot(x0 - x, y0 - y, c, vbuffer);
  GFX_Plot(x0 + y, y0 + x, c, vbuffer);
  GFX_Plot(x0 - y, y0 + x, c, vbuffer);
  GFX_Plot(x0 + y, y0 - x, c, vbuffer);
  GFX_Plot(x0 - y, y0 - x, c, vbuffer);

  } // end while

} // GFX_Draw_Circle

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

GFX_Draw_Circle_Clipped(int x0, int y0, int r, int c, UCHAR *vbuffer)
{
// draws a circle at (x0, y0) with radius r and color c, based on DDA, midpoint algorithm, aka bresenham's circle
// basically uses differential analysis, that is the change from one iteration to another to compute the terms
// of the curve
int f = 1 - r;
int ddF_x = 1;
int ddF_y = -2 * r;
int x = 0;
int y = r;

// first test for trivial rejection of circle
if ( (x0+r < 0) || (x0-r >= g_screen_width) || (y0+r < 0) || (y0-r >= g_screen_height) )
    return;

// draw the 4 endpoints
GFX_Plot_Clipped(x0, y0 + r, c, vbuffer);
GFX_Plot_Clipped(x0, y0 - r, c, vbuffer);
GFX_Plot_Clipped(x0 + r, y0, c, vbuffer);
GFX_Plot_Clipped(x0 - r, y0, c, vbuffer);

// enter loop and rasterize curve
while (x < y) 
    {
    
    if(f >= 0) 
      {
      y--;
      ddF_y += 2;
      f += ddF_y;
      } // end if

  x++;
  ddF_x += 2;
  f += ddF_x;    

  // draw all octants
  GFX_Plot_Clipped(x0 + x, y0 + y, c, vbuffer);
  GFX_Plot_Clipped(x0 - x, y0 + y, c, vbuffer);
  GFX_Plot_Clipped(x0 + x, y0 - y, c, vbuffer);
  GFX_Plot_Clipped(x0 - x, y0 - y, c, vbuffer);
  GFX_Plot_Clipped(x0 + y, y0 + x, c, vbuffer);
  GFX_Plot_Clipped(x0 - y, y0 + x, c, vbuffer);
  GFX_Plot_Clipped(x0 + y, y0 - x, c, vbuffer);
  GFX_Plot_Clipped(x0 - y, y0 - x, c, vbuffer);
  } // end while

} // GFX_Draw_Circle_Clipped

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void GFX_Draw_Top_Tri(int x1,int y1,
                      int x2,int y2,
                      int x3,int y3,
                      int color,
                      UCHAR *dest_buffer)
{
// this function draws a triangle that has a flat top

float dx_right,    // the dx/dy ratio of the right edge of line
      dx_left,     // the dx/dy ratio of the left edge of line
      xs,xe,       // the starting and ending points of the edges
      height;      // the height of the triangle

int temp_x,        // used during sorting as temps
    temp_y,
    right,         // used by clipping
    left;
    
// destination address of next scanline
//UCHAR  *dest_addr = NULL;

// test order of x1 and x2
if (x2 < x1)
   {
   temp_x = x2;
   x2     = x1;
   x1     = temp_x;
   } // end if swap

// compute delta's
height = y3-y1;

dx_left  = (x3-x1)/height;
dx_right = (x3-x2)/height;

// set starting points
xs = (float)x1;
xe = (float)x2+(float)0.5;

// perform y clipping
if (y1 < g_clip_min_y)
   {
   // compute new xs and ys
   xs = xs+dx_left*(float)(-y1+g_clip_min_y);
   xe = xe+dx_right*(float)(-y1+g_clip_min_y);

   // reset y1
   y1=g_clip_min_y;

   } // end if top is off screen

if (y3>g_clip_max_y)
   y3=g_clip_max_y;

// compute starting address in video memory
//dest_addr = dest_buffer+y1*mempitch;

// test if x clipping is needed
if (x1>=g_clip_min_x && x1<=g_clip_max_x &&
    x2>=g_clip_min_x && x2<=g_clip_max_x &&
    x3>=g_clip_min_x && x3<=g_clip_max_x)
    {
    // draw the triangle
    for (temp_y=y1; temp_y<=y3; temp_y++) //, dest_addr+=mempitch)
        {
        // draw segment
        // memset((UCHAR *)dest_addr+(unsigned int)xs, color,(unsigned int)((int)xe-(int)xs+1));
        GFX_Draw_HLine( (int)xs, (int)xe, temp_y, color, dest_buffer);
               
        // adjust starting point and ending point
        xs+=dx_left;
        xe+=dx_right;

        } // end for

    } // end if no x clipping needed
else
   {
   // clip x axis with slower version

   // draw the triangle
   for (temp_y=y1; temp_y<=y3; temp_y++) // ,dest_addr+=mempitch)
       {
       // do x clip
       left  = (int)xs;
       right = (int)xe;

       // adjust starting point and ending point
       xs+=dx_left;
       xe+=dx_right;

       // clip line
       if (left < g_clip_min_x)
          {
          left = g_clip_min_x;

          if (right < g_clip_min_x)
             continue;
          }

       if (right > g_clip_max_x)
          {
          right = g_clip_max_x;

          if (left > g_clip_max_x)
             continue;
          }
       // draw segment
       //memset((UCHAR  *)dest_addr+(unsigned int)left, color,(unsigned int)(right-left+1));
       GFX_Draw_HLine( (int)left, (int)right, temp_y, color, dest_buffer);

       } // end for

   } // end else x clipping needed

} // end GFX_Draw_Top_Tri

/////////////////////////////////////////////////////////////////////////

void GFX_Draw_Bottom_Tri(int x1,int y1,
                         int x2,int y2,
                         int x3,int y3,
                         int color,
                         UCHAR *dest_buffer)
{
// this function draws a triangle that has a flat bottom

float dx_right,    // the dx/dy ratio of the right edge of line
      dx_left,     // the dx/dy ratio of the left edge of line
      xs,xe,       // the starting and ending points of the edges
      height;      // the height of the triangle

int temp_x,        // used during sorting as temps
    temp_y,
    right,         // used by clipping
    left;

// destination address of next scanline
//UCHAR  *dest_addr;

// test order of x1 and x2
if (x3 < x2)
   {
   temp_x = x2;
   x2     = x3;
   x3     = temp_x;
   } // end if swap

// compute delta's
height = y3-y1;

dx_left  = (x2-x1)/height;
dx_right = (x3-x1)/height;

// set starting points
xs = (float)x1;
xe = (float)x1; // +(float)0.5;

// perform y clipping
if (y1<g_clip_min_y)
   {
   // compute new xs and ys
   xs = xs+dx_left*(float)(-y1+g_clip_min_y);
   xe = xe+dx_right*(float)(-y1+g_clip_min_y);

   // reset y1
   y1=g_clip_min_y;

   } // end if top is off screen

if (y3>g_clip_max_y)
   y3=g_clip_max_y;

// compute starting address in video memory
//dest_addr = dest_buffer+y1*mempitch;

// test if x clipping is needed
if (x1>=g_clip_min_x && x1<=g_clip_max_x &&
    x2>=g_clip_min_x && x2<=g_clip_max_x &&
    x3>=g_clip_min_x && x3<=g_clip_max_x)
    {
    // draw the triangle
    for (temp_y=y1; temp_y<=y3; temp_y++) //,dest_addr+=mempitch)
        {
        // draw segment
        //memset((UCHAR  *)dest_addr+(unsigned int)xs, color,(unsigned int)((int)xe-(int)xs+1));
        GFX_Draw_HLine( (int)xs, (int)xe, temp_y, color, dest_buffer);

        // adjust starting point and ending point
        xs+=dx_left;
        xe+=dx_right;

        } // end for

    } // end if no x clipping needed
else
   {
   // clip x axis with slower version

   // draw the triangle

   for (temp_y=y1; temp_y<=y3; temp_y++) // ,dest_addr+=mempitch)
       {
       // do x clip
       left  = (int)xs;
       right = (int)xe;

       // adjust starting point and ending point
       xs+=dx_left;
       xe+=dx_right;

       // clip line
       if (left < g_clip_min_x)
          {
          left = g_clip_min_x;

          if (right < g_clip_min_x)
             continue;
          }

       if (right > g_clip_max_x)
          {
          right = g_clip_max_x;

          if (left > g_clip_max_x)
             continue;
          }

       // draw segment
       //memset((UCHAR  *)dest_addr+(unsigned int)left, color,(unsigned int)(right-left+1));
       GFX_Draw_HLine( (int)left, (int)right, temp_y, color, dest_buffer);

       } // end for

   } // end else x clipping needed

} // end GFX_Draw_Bottom_Tri

/////////////////////////////////////////////////////////////////////////

void GFX_Draw_Triangle_2D(int x1,int y1,
                          int x2,int y2,
                          int x3,int y3,
                          int color,
					      UCHAR *dest_buffer)
{
// this function draws a triangle on the destination buffer
// it decomposes all triangles into a pair of flat top, flat bottom

int temp_x, // used for sorting
    temp_y,
    new_x;


// test for h lines and v lines
if ((x1==x2 && x2==x3)  ||  (y1==y2 && y2==y3))
   return;

// sort p1,p2,p3 in ascending y order
if (y2<y1)
   {
   temp_x = x2;
   temp_y = y2;
   x2     = x1;
   y2     = y1;
   x1     = temp_x;
   y1     = temp_y;
   } // end if

// now we know that p1 and p2 are in order
if (y3<y1)
   {
   temp_x = x3;
   temp_y = y3;
   x3     = x1;
   y3     = y1;
   x1     = temp_x;
   y1     = temp_y;
   } // end if

// finally test y3 against y2
if (y3<y2)
   {
   temp_x = x3;
   temp_y = y3;
   x3     = x2;
   y3     = y2;
   x2     = temp_x;
   y2     = temp_y;

   } // end if

// do trivial rejection tests for clipping
if ( y3<g_clip_min_y || y1>g_clip_max_y ||
    (x1<g_clip_min_x && x2<g_clip_min_x && x3<g_clip_min_x) ||
    (x1>g_clip_max_x && x2>g_clip_max_x && x3>g_clip_max_x) )
   return;

// test if top of triangle is flat
if (y1==y2)
   {
   GFX_Draw_Top_Tri(x1,y1,x2,y2,x3,y3,color, dest_buffer);
   } // end if
else
if (y2==y3)
   {
   GFX_Draw_Bottom_Tri(x1,y1,x2,y2,x3,y3,color, dest_buffer);
   } // end if bottom is flat
else
   {
   // general triangle that's needs to be broken up along long edge
   new_x = x1 + (int)(0.5+(float)(y2-y1)*(float)(x3-x1)/(float)(y3-y1));

   // draw each sub-triangle
   GFX_Draw_Bottom_Tri(x1,y1,new_x,y2,x2,y2,color, dest_buffer);
   GFX_Draw_Top_Tri(x2,y2,new_x,y2,x3,y3,color, dest_buffer);

   } // end else

} // end GFX_Draw_Triangle_2D

/////////////////////////////////////////////////////////////////////////

void Draw_TriangleFP_2D(int x1,int y1,
                        int x2,int y2,
                        int x3,int y3,
                        int color,
    	   			    UCHAR *dest_buffer, int mempitch)
{
// this function draws a triangle on the destination buffer using fixed point
// it decomposes all triangles into a pair of flat top, flat bottom

int temp_x, // used for sorting
    temp_y,
    new_x;

#ifdef DEBUG_ON
	// track rendering stats
    debug_polys_rendered_per_frame++;
#endif


// test for h lines and v lines
if ((x1==x2 && x2==x3)  ||  (y1==y2 && y2==y3))
   return;

// sort p1,p2,p3 in ascending y order
if (y2<y1)
   {
   temp_x = x2;
   temp_y = y2;
   x2     = x1;
   y2     = y1;
   x1     = temp_x;
   y1     = temp_y;
   } // end if

// now we know that p1 and p2 are in order
if (y3<y1)
   {
   temp_x = x3;
   temp_y = y3;
   x3     = x1;
   y3     = y1;
   x1     = temp_x;
   y1     = temp_y;
   } // end if

// finally test y3 against y2
if (y3<y2)
   {
   temp_x = x3;
   temp_y = y3;
   x3     = x2;
   y3     = y2;
   x2     = temp_x;
   y2     = temp_y;

   } // end if

// do trivial rejection tests for clipping
if ( y3<g_clip_min_y || y1>g_clip_max_y ||
    (x1<g_clip_min_x && x2<g_clip_min_x && x3<g_clip_min_x) ||
    (x1>g_clip_max_x && x2>g_clip_max_x && x3>g_clip_max_x) )
   return;

// test if top of triangle is flat
if (y1==y2)
   {
   GFX_Draw_Top_TriFP(x1,y1,x2,y2,x3,y3,color, dest_buffer);
   } // end if
else
if (y2==y3)
   {
   GFX_Draw_Bottom_TriFP(x1,y1,x2,y2,x3,y3,color, dest_buffer);
   } // end if bottom is flat
else
   {
   // general triangle that's needs to be broken up along long edge
   new_x = x1 + (int)(0.5+(float)(y2-y1)*(float)(x3-x1)/(float)(y3-y1));

   // draw each sub-triangle
   GFX_Draw_Bottom_TriFP(x1,y1,new_x,y2,x2,y2,color, dest_buffer);
   GFX_Draw_Top_TriFP(x2,y2,new_x,y2,x3,y3,color, dest_buffer);

   } // end else

} // end Draw_TriangleFP_2D

/////////////////////////////////////////////////////////////////////////

void GFX_Draw_Top_TriFP(int x1,int y1,
                        int x2,int y2,
                        int x3,int y3,
                        int color,
                        UCHAR *dest_buffer)
{
// this function draws a triangle that has a flat top using fixed point math

long dx_right,    // the dx/dy ratio of the right edge of line
     dx_left,     // the dx/dy ratio of the left edge of line
     xs,xe,       // the starting and ending points of the edges
     height;      // the height of the triangle

long temp_x,        // used during sorting as temps
     temp_y,
     right,         // used by clipping
     left;

//UCHAR  *dest_addr;

// test for degenerate
if (y1==y3 || y2==y3)
	return;

// test order of x1 and x2
if (x2 < x1)
   {
   temp_x = x2;
   x2     = x1;
   x1     = temp_x;
   } // end if swap

// compute delta's
height = y3-y1;

dx_left  = ((x3-x1)<<FIXP16_SHIFT)/height;
dx_right = ((x3-x2)<<FIXP16_SHIFT)/height;

// set starting points
xs = (x1<<FIXP16_SHIFT);
xe = (x2<<FIXP16_SHIFT);

// perform y clipping
if (y1<g_clip_min_y)
   {
   // compute new xs and ys
   xs = xs+dx_left*(-y1+g_clip_min_y);
   xe = xe+dx_right*(-y1+g_clip_min_y);

   // reset y1
   y1=g_clip_min_y;

   } // end if top is off screen

if (y3>g_clip_max_y)
   y3=g_clip_max_y;

// compute starting address in video memory
//dest_addr = dest_buffer+y1*mempitch;

// test if x clipping is needed
if (x1>=g_clip_min_x && x1<=g_clip_max_x &&
    x2>=g_clip_min_x && x2<=g_clip_max_x &&
    x3>=g_clip_min_x && x3<=g_clip_max_x)
    {
    // draw the triangle
    for (temp_y=y1; temp_y<=y3; temp_y++ )//,dest_addr+=mempitch)
        {
      // memset((UCHAR *)dest_addr+((xs+FIXP16_ROUND_UP)>>FIXP16_SHIFT),  color, (((xe-xs+FIXP16_ROUND_UP)>>FIXP16_SHIFT)+1));

        // adjust starting point and ending point
        xs+=dx_left;
        xe+=dx_right;

        } // end for

    } // end if no x clipping needed
else
   {
   // clip x axis with slower version

   // draw the triangle
   for (temp_y=y1; temp_y<=y3; temp_y++) //,dest_addr+=mempitch)
       {
       // do x clip
       left  = ((xs+FIXP16_ROUND_UP)>>16);
       right = ((xe+FIXP16_ROUND_UP)>>16);

       // adjust starting point and ending point
       xs+=dx_left;
       xe+=dx_right;

       // clip line
       if (left < g_clip_min_x)
          {
          left = g_clip_min_x;

          if (right < g_clip_min_x)
             continue;
          }

       if (right > g_clip_max_x)
          {
          right = g_clip_max_x;

          if (left > g_clip_max_x)
             continue;
          }

       //memset((UCHAR  *)dest_addr+(unsigned int)left, color,(unsigned int)(right-left+1));

       } // end for

   } // end else x clipping needed

} // end GFX_Draw_Top_TriFP

/////////////////////////////////////////////////////////////////////////

void GFX_Draw_Bottom_TriFP(int x1,int y1,
                           int x2,int y2,
                           int x3,int y3,
                           int color,
                           UCHAR *dest_buffer)
{

// this function draws a triangle that has a flat bottom using fixed point math

long dx_right,    // the dx/dy ratio of the right edge of line
     dx_left,     // the dx/dy ratio of the left edge of line
     xs,xe,       // the starting and ending points of the edges
     height;      // the height of the triangle

long temp_x,        // used during sorting as temps
     temp_y,
     right,         // used by clipping
     left;

//UCHAR  *dest_addr;

if (y1==y2 || y1==y3)
	return;

// test order of x1 and x2
if (x3 < x2)
   {
   temp_x = x2;
   x2     = x3;
   x3     = temp_x;

   } // end if swap

// compute delta's
height = y3-y1;

dx_left  = ((x2-x1)<<FIXP16_SHIFT)/height;
dx_right = ((x3-x1)<<FIXP16_SHIFT)/height;

// set starting points
xs = (x1<<FIXP16_SHIFT);
xe = (x1<<FIXP16_SHIFT);

// perform y clipping
if (y1<g_clip_min_y)
   {
   // compute new xs and ys
   xs = xs+dx_left*(-y1+g_clip_min_y);
   xe = xe+dx_right*(-y1+g_clip_min_y);

   // reset y1
   y1=g_clip_min_y;

   } // end if top is off screen

if (y3>g_clip_max_y)
   y3=g_clip_max_y;

// compute starting address in video memory
//dest_addr = dest_buffer+y1*mempitch;

// test if x clipping is needed
if (x1>=g_clip_min_x && x1<=g_clip_max_x &&
    x2>=g_clip_min_x && x2<=g_clip_max_x &&
    x3>=g_clip_min_x && x3<=g_clip_max_x)
    {
    // draw the triangle
    for (temp_y=y1; temp_y<=y3; temp_y++)//,dest_addr+=mempitch)
        {
        //memset((UCHAR *)dest_addr+((xs+FIXP16_ROUND_UP)>>FIXP16_SHIFT), color, (((xe-xs+FIXP16_ROUND_UP)>>FIXP16_SHIFT)+1));

        // adjust starting point and ending point
        xs+=dx_left;
        xe+=dx_right;

        } // end for

    } // end if no x clipping needed
else
   {
   // clip x axis with slower version

   // draw the triangle
   for (temp_y=y1; temp_y<=y3; temp_y++)//,dest_addr+=mempitch)
       {
       // do x clip
       left  = ((xs+FIXP16_ROUND_UP)>>FIXP16_SHIFT);
       right = ((xe+FIXP16_ROUND_UP)>>FIXP16_SHIFT);

       // adjust starting point and ending point
       xs+=dx_left;
       xe+=dx_right;

       // clip line
       if (left < g_clip_min_x)
          {
          left = g_clip_min_x;

          if (right < g_clip_min_x)
             continue;
          }

       if (right > g_clip_max_x)
          {
          right = g_clip_max_x;

          if (left > g_clip_max_x)
             continue;
          }

        //memset((UCHAR *)dest_addr+left, color, (right-left+1));

       } // end for

   } // end else x clipping needed

} // end GFX_Draw_Bottom_TriFP

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int Fast_Distance_2D(int x, int y)
{
// this function computes the distance from 0,0 to x,y with 3.5% error

// first compute the absolute value of x,y
x = abs(x);
y = abs(y);

// compute the minimum of x,y
int mn = MIN(x,y);

// return the distance
return(x+y-(mn>>1)-(mn>>2)+(mn>>4));

} // end Fast_Distance_2D

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float Fast_Distance_3D(float fx, float fy, float fz)
{
// this function computes the distance from the origin to x,y,z

int temp;  // used for swaping
int x,y,z; // used for algorithm

// make sure values are all positive
x = fabs(fx) * 1024;
y = fabs(fy) * 1024;
z = fabs(fz) * 1024;

// sort values
if (y < x) SWAP(x,y,temp)

if (z < y) SWAP(y,z,temp)

if (y < x) SWAP(x,y,temp)

int dist = (z + 11 * (y >> 5) + (x >> 2) );

// compute distance with 8% error
return((float)(dist >> 10));

} // end Fast_Distance_3D

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int Find_Bounding_Box_Poly2D(POLYGON2D_PTR poly,
                             float *min_x, float *max_x,
                             float *min_y, float *max_y)
{
// this function finds the bounding box of a 2D polygon
// and returns the values in the sent vars

int index; // looping var

// is this poly valid?
if (poly->num_verts == 0)
    return(0);

// initialize output vars (note they are pointers)
// also note that the algorithm assumes local coordinates
// that is, the poly verts are relative to 0,0
*max_x = *max_y = *min_x = *min_y = 0;

// process each vertex
for (index=0; index < poly->num_verts; index++)
    {
    // update vars - run min/max seek
    if (poly->vlist[index].x > *max_x)
       *max_x = poly->vlist[index].x;

    if (poly->vlist[index].x < *min_x)
       *min_x = poly->vlist[index].x;

    if (poly->vlist[index].y > *max_y)
       *max_y = poly->vlist[index].y;

    if (poly->vlist[index].y < *min_y)
       *min_y = poly->vlist[index].y;

} // end for index

// return success
return(1);

} // end Find_Bounding_Box_Poly2D

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Build_Sin_Cos_Tables(void)
{
// create sin/cos lookup table
// note the creation of one extra element; 360
// this helps with logic in using the tables

int ang; 

// generate the tables 0 - 360 inclusive
for (ang = 0; ang <= 360; ang++)
    {
    // convert ang to radians
    float theta = (float)ang*PI/(float)180;

    // insert next entry into table
    cos_look[ang] = cos(theta);
    sin_look[ang] = sin(theta);

    } // end for ang

} // end Build_Sin_Cos_Tables

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int Translate_Polygon2D(POLYGON2D_PTR poly, int dx, int dy)
{
// this function translates the center of a polygon

// test for valid pointer
if (!poly)
   return(0);

// translate
poly->x0+=dx;
poly->y0+=dy;

// return success
return(1);

} // end Translate_Polygon2D

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int Rotate_Polygon2D(POLYGON2D_PTR poly, int theta)
{
// this function rotates the local coordinates of the polygon

int curr_vert;
float ftheta;

// test for valid pointer
if (!poly)
   return(0);

// test for negative rotation angle
if (theta < 0)
   theta+=360;

// conver to radians
ftheta = DEG_TO_RAD( theta );

// loop and rotate each point, very crude, no lookup!!!
for (curr_vert = 0; curr_vert < poly->num_verts; curr_vert++)
    {

    // perform rotation
//    float xr = (float)poly->vlist[curr_vert].x*cos_look[theta] -
//                    (float)poly->vlist[curr_vert].y*sin_look[theta];

//    float yr = (float)poly->vlist[curr_vert].x*sin_look[theta] +
//                    (float)poly->vlist[curr_vert].y*cos_look[theta];

    // later convert back to sin/cos look up table when memory is freed
    float xr = (float)poly->vlist[curr_vert].x*cos( ftheta ) -
                    (float)poly->vlist[curr_vert].y*sin( ftheta );

    float yr = (float)poly->vlist[curr_vert].x*sin( ftheta ) +
                    (float)poly->vlist[curr_vert].y*cos( ftheta );

    // store result back
    poly->vlist[curr_vert].x = xr;
    poly->vlist[curr_vert].y = yr;

    } // end for curr_vert

// return success
return(1);

} // end Rotate_Polygon2D

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int Scale_Polygon2D(POLYGON2D_PTR poly, float sx, float sy)
{
// this function scalesthe local coordinates of the polygon

int curr_vert;

// test for valid pointer
if (!poly)
   return(0);

// loop and scale each point
for (curr_vert = 0; curr_vert < poly->num_verts; curr_vert++)
    {
    // scale and store result back
    poly->vlist[curr_vert].x *= sx;
    poly->vlist[curr_vert].y *= sy;

    } // end for curr_vert

// return success
return(1);

} // end Scale_Polygon2D

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int GFX_Draw_Polygon2D(POLYGON2D_PTR poly, UCHAR *vbuffer)
{
// this function draws a POLYGON2D based on

int index; // looping var

// test if the polygon is visible
if (poly->state)
   {
   // loop thru and draw a line from vertices 1 to n
   for (index=0; index < poly->num_verts-1; index++)
        {
        // draw line from ith to ith+1 vertex
        GFX_Draw_Clip_Line(poly->vlist[index].x+poly->x0,
                           poly->vlist[index].y+poly->y0,
                           poly->vlist[index+1].x+poly->x0,
                           poly->vlist[index+1].y+poly->y0,
                           poly->color, vbuffer);

        } // end for

       // now close up polygon
       // draw line from last vertex to 0th
       GFX_Draw_Clip_Line(poly->vlist[0].x+poly->x0,
                          poly->vlist[0].y+poly->y0,
                          poly->vlist[index].x+poly->x0,
                          poly->vlist[index].y+poly->y0,
                          poly->color, vbuffer);

   // return success
   return(1);
   } // end if
else
   return(0);

} // end GFX_Draw_Polygon2D

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// these are the matrix versions, note they are more inefficient for
// single transforms, but their power comes into play when you concatenate
// multiple transformations, not to mention that all transforms are accomplished
// with the same code, just the matrix differs

int Translate_Polygon2D_Mat(POLYGON2D_PTR poly, int dx, int dy)
{
// this function translates the center of a polygon by using a matrix multiply
// on the the center point, this is incredibly inefficient, but for educational purposes
// if we had an object that wasn't in local coordinates then it would make more sense to
// use a matrix, but since the origin of the object is at x0,y0 then 2 lines of code can
// translate, but lets do it the hard way just to see :)

// test for valid pointer
if (!poly)
   return(0);

MATRIX3X2 mt; // used to hold translation transform matrix

// initialize the matrix with translation values dx dy
Mat_Init_3X2(&mt,1,0, 0,1, dx, dy);

// create a 1x2 matrix to do the transform
MATRIX1X2 p0;
MATRIX1X2 p1; // this will hold result

p0.M00 = poly->x0;
p0.M01 = poly->y0;

p1.M00 = 0;
p1.M01 = 0;

// now translate via a matrix multiply
Mat_Mul_1X2_3X2(&p0, &mt, &p1);

// now copy the result back into polygon
poly->x0 = p1.M[0];
poly->y0 = p1.M[1];

// return success
return(1);

} // end Translate_Polygon2D_Mat

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int Rotate_Polygon2D_Mat(POLYGON2D_PTR poly, int theta)
{
// this function rotates the local coordinates of the polygon

int curr_vert;
float ftheta;

// test for valid pointer
if (!poly)
   return(0);

// test for negative rotation angle
if (theta < 0)
   theta+=360;

MATRIX3X2 mr; // used to hold rotation transform matrix

// convert angle to radians for use by math sin/cos, later go back to degrees and lookup tables
ftheta = DEG_TO_RAD(theta);

// initialize the matrix with translation values dx dy
//Mat_Init_3X2(&mr,cos_look[theta],sin_look[theta],
//                 -sin_look[theta],cos_look[theta],
//                  0, 0);

Mat_Init_3X2(&mr,cos(ftheta),sin(ftheta),
                 -sin(ftheta),cos(ftheta),
                  0, 0);

// loop and rotate each point, very crude, no lookup!!!
for (curr_vert = 0; curr_vert < poly->num_verts; curr_vert++)
    {
    // create a 1x2 matrix to do the transform
    MATRIX1X2 p0; 
    MATRIX1X2 p1; // this will hold result

    p0.M00 = poly->vlist[curr_vert].x;
    p0.M01 = poly->vlist[curr_vert].y;

    p1.M00 = 0;
    p1.M01 = 0;

    // now rotate via a matrix multiply
    Mat_Mul_1X2_3X2(&p0, &mr, &p1);

    // now copy the result back into vertex
    poly->vlist[curr_vert].x = p1.M[0];
    poly->vlist[curr_vert].y = p1.M[1];

    } // end for curr_vert

// return success
return(1);

} // end Rotate_Polygon2D_Mat

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int Scale_Polygon2D_Mat(POLYGON2D_PTR poly, float sx, float sy)
{
// this function scalesthe local coordinates of the polygon
int curr_vert;

// test for valid pointer
if (!poly)
   return(0);


MATRIX3X2 ms; // used to hold scaling transform matrix

// initialize the matrix with translation values dx dy
Mat_Init_3X2(&ms,sx,0,
                 0,sy,
                 0, 0);

// loop and scale each point
for (curr_vert = 0; curr_vert < poly->num_verts; curr_vert++)
    {
    // scale and store result back

    // create a 1x2 matrix to do the transform
    MATRIX1X2 p0; 
    MATRIX1X2 p1; // this will hold result

    p0.M00 = poly->vlist[curr_vert].x;
    p0.M01 = poly->vlist[curr_vert].y;

    p1.M00 = 0;
    p1.M01 = 0;

    // now scale via a matrix multiply
    Mat_Mul_1X2_3X2(&p0, &ms, &p1);

    // now copy the result back into vertex
    poly->vlist[curr_vert].x = p1.M[0];
    poly->vlist[curr_vert].y = p1.M[1];

    } // end for curr_vert

// return success
return(1);

} // end Scale_Polygon2D_Mat


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int Mat_Mul_3X3(MATRIX3X3_PTR ma,
               MATRIX3X3_PTR mb,
               MATRIX3X3_PTR mprod)
{
// this function multiplies two matrices together and
// and stores the result

int row, col, index;

for (row=0; row<3; row++)
    {
    for (col=0; col<3; col++)
        {
        // compute dot product from row of ma
        // and column of mb

        float sum = 0; // used to hold result

        for (index=0; index<3; index++)
             {
             // add in next product pair
             sum+=(ma->M[row][index]*mb->M[index][col]);
             } // end for index

        // insert resulting row,col element
        mprod->M[row][col] = sum;

        } // end for col

    } // end for row

return(1);

} // end Mat_Mul_3X3

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int Mat_Mul_1X3_3X3(MATRIX1X3_PTR ma,
                   MATRIX3X3_PTR mb,
                   MATRIX1X3_PTR mprod)
{
// this function multiplies a 1x3 matrix against a
// 3x3 matrix - ma*mb and stores the result
int col, index;

    for (col=0; col<3; col++)
        {
        // compute dot product from row of ma
        // and column of mb

        float sum = 0; // used to hold result

        for (index=0; index<3; index++)
             {
             // add in next product pair
             sum+=(ma->M[index]*mb->M[index][col]);
             } // end for index

        // insert resulting col element
        mprod->M[col] = sum;

        } // end for col

return(1);

} // end Mat_Mul_1X3_3X3

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int Mat_Mul_1X2_3X2(MATRIX1X2_PTR ma,
                   MATRIX3X2_PTR mb,
                   MATRIX1X2_PTR mprod)
{
// this function multiplies a 1x2 matrix against a
// 3x2 matrix - ma*mb and stores the result
// using a dummy element for the 3rd element of the 1x2
// to make the matrix multiply valid i.e. 1x3 X 3x2

int index, col; // looping var

    for (col=0; col<2; col++)
        {
        // compute dot product from row of ma
        // and column of mb

        float sum = 0; // used to hold result

        for (index=0; index<2; index++)
             {
             // add in next product pair
             sum+=(ma->M[index]*mb->M[index][col]);
             } // end for index

        // add in last element * 1
        sum+= mb->M[index][col];

        // insert resulting col element
        mprod->M[col] = sum;

        } // end for col

return(1);

} // end Mat_Mul_1X2_3X2

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int Mat_Init_3X2(MATRIX3X2_PTR ma,
                 float m00, float m01,
                 float m10, float m11,
                 float m20, float m21)
{
// this function fills a 3x2 matrix with the sent data in row major form
ma->M[0][0] = m00; ma->M[0][1] = m01;
ma->M[1][0] = m10; ma->M[1][1] = m11;
ma->M[2][0] = m20; ma->M[2][1] = m21;

// return success
return(1);

} // end Mat_Init_3X2

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FIXP16 FIXP16_DIV(FIXP16 fp1, FIXP16 fp2)
{
// this function computes the quotient fp1/fp2 using
// 32 bit math, try to keep as much precision as possible

// compute fp1/fp2, for result not to underflow the divisor needs to be shifted to the left 16 digits, however
// this would overflow the datatype, thus a comprimise must be struck where we shift the divisor legt 6 digits
// and the dividend right 10 digits, thus the resulting division will be back in 16.16 format, but the range
// of the dividend was cut to 10 digits of integer accuracy and the divisor to 6 digits of fractional accuracy

return ( (fp1 << 6) / (fp2 >> 10) );

} // end FIXP16_DIV

/////////////////////////////////////////////////////////////////////////

FIXP16 FIXP16_MUL(FIXP16 fp1, FIXP16 fp2)
{
// this function computes the product fp_prod = fp1*fp2
// using 32 bit math, try to keep as much precission as possible

// compute fp1*fp2, for result not to overflow, we must shift to the right 8 times each operand, so final product
// is stil in 16.16 format, this however, causes the loss of 8 bits of precision, the correct way to do this would
// be to use a full 64-bit multiply then shift the result back, but not worth it probably due to overhead
return ( (fp1 >> 8) * (fp2 >> 8) );

} // end FIXP16_MUL

///////////////////////////////////////////////////////////

float Fast_Sin(float theta)
{
// this function uses the sin_look[] lookup table, but
// has logic to handle negative angles as well as fractional
// angles via interpolation, use this for a more robust
// sin computation that the blind lookup, but with with
// a slight hit in speed

// convert angle to 0-359, used to use fmodf()
theta = fmod(theta,360);

// make angle positive
if (theta < 0) theta+=360.0;

// compute floor of theta and fractional part to interpolate
int theta_int    = (int)theta;
float theta_frac = theta - theta_int;

// now compute the value of sin(angle) using the lookup tables
// and interpolating the fractional part, note that if theta_int
// is equal to 359 then theta_int+1=360, but this is fine since the
// table was made with the entries 0-360 inclusive
return(sin_look[theta_int] +
       theta_frac*(sin_look[theta_int+1] - sin_look[theta_int]));

} // end Fast_Sin

/////////////////////////////////////////////////////////////////////////

float Fast_Cos(float theta)
{
// this function uses the cos_look[] lookup table, but
// has logic to handle negative angles as well as fractional
// angles via interpolation, use this for a more robust
// cos computation that the blind lookup, but with with
// a slight hit in speed

// convert angle to 0-359
theta = fmod(theta,360);

// make angle positive
if (theta < 0) theta+=360.0;

// compute floor of theta and fractional part to interpolate
int theta_int    = (int)theta;
float theta_frac = theta - theta_int;

// now compute the value of sin(angle) using the lookup tables
// and interpolating the fractional part, note that if theta_int
// is equal to 359 then theta_int+1=360, but this is fine since the
// table was made with the entries 0-360 inclusive
return(cos_look[theta_int] +
       theta_frac*(cos_look[theta_int+1] - cos_look[theta_int]));

} // end Fast_Cos

/////////////////////////////////////////////////////////////////////////

// Tile map functions, added 11/10/08

void GFX_TMap_Print_Char(char ch)
{
// this function prints a character to the next terminal position and updates the cursor position
// if the character is not  " ", "0...9", ".,", "A...Z" then the function prints a space character

// test for control code first
if (ch == 0x0A || ch == 0x0D)
    {
    // reset cursor to left side of screen
    g_tmap_cursor_x = 0;

    if (++g_tmap_cursor_y >= g_tile_map_height)
       {
       g_tmap_cursor_y = g_tile_map_height-1; 

       // scroll tile map data one line up vertically
       memcpy( tile_map_ptr, tile_map_ptr + g_tile_map_width, g_tile_map_width*(g_tile_map_height-1) );

       // clear last line out 
       memset( tile_map_ptr + g_tile_map_width*(g_tile_map_height-1), TMAP_BASE_BLANK, g_tile_map_width );
    
       } // end if

    // return to caller
    return;

    } // end if
else // normal printable character?
    {
    // convert to upper case first
    ch = toupper(ch);

    // now test if character is in range
    if (ch >= 'A' && ch <= 'Z') ch = ch - 'A' + TMAP_BASE_LETTERS;
    else
    if (ch >= '0' && ch <= '9') ch = ch - '0' + TMAP_BASE_NUMBERS;
    else
    if (ch == '.') ch = ch - '.' + TMAP_BASE_PUNC;
    else
    if (ch == ',') ch = ch - ',' + TMAP_BASE_PUNC+1;
    else 
       ch = TMAP_BASE_BLANK;

    // write character to tile_map
    tile_map_ptr[g_tmap_cursor_x + (g_tmap_cursor_y * g_tile_map_width) ] = ch;

    // update cursor position
    if (++g_tmap_cursor_x >= g_tile_map_width)
       {
       g_tmap_cursor_x = 0;

       if (++g_tmap_cursor_y >= g_tile_map_height)
          {
          g_tmap_cursor_y = g_tile_map_height-1; 

          // scroll tile map data one line up vertically
          memcpy( tile_map_ptr, tile_map_ptr + g_tile_map_width, g_tile_map_width*(g_tile_map_height-1) );

          // clear last line out 
          memset( tile_map_ptr + g_tile_map_width*(g_tile_map_height-1), TMAP_BASE_BLANK, g_tile_map_width );
          } // end if

       } // end if

    // return to caller
    return;

    } // end else

} // end GFX_TMap_Print_Char

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void GFX_TMap_Print_String(char *string)
{
// this function prints the sent string by making repeated calls to the character print function
while( *string != NULL)
   GFX_TMap_Print_Char( *string++ );

} // end GFX_TMap_Print_String

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void GFX_TMap_CLS(char tile_index)
{
// this function clears the tile map memory out with the sent absolute tile index

// clear the memory
memset( tile_map_ptr, tile_index, g_tile_map_width*g_tile_map_height );

// home cursor
g_tmap_cursor_x = 0;
g_tmap_cursor_y = 0;

} // end GFX_TMap_CLS

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void GFX_TMap_Setcursor(int x, int y)
{
// this function sets the cursor position for the tile mapped console

// test if cursor is on screen
if (x < 0) x = 0;
else
if (x >= g_tile_map_width ) x = g_tile_map_width - 1;

if (y < 0) y = 0;
else
if (y >= g_tile_map_height ) y = g_tile_map_height - 1;

// make assignment
g_tmap_cursor_x = x;
g_tmap_cursor_y = y;

} // end GFX_TMap_Setcursor

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
