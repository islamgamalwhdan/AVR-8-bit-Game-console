///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// Copyright Nurve Networks LLC 2008
// 
// Filename: XGS_AVR_GFX_LIB_V010.h
//
// Original Author: Andre' LaMothe
// 
// Last Modified: 9.1.08
//
// Description: Graphics library header file
//
// Overview: 
//  
// 
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// INCLUDES ///////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// watch for multiple inclusions
#ifndef XGS_AVR_GFX_LIB
#define XGS_AVR_GFX_LIB

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MACROS /////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// used to compute the min and max of two expresions
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#define SIGN(a) ( ((a) > 0) ? (1) : (-1) )

// a more useful random function
#define RAND_RANGE(x,y) ( (x) + (rand()%((y)-(x)+1)))

// used for swapping algorithm
#define SWAP(a,b,t) {t=a; a=b; b=t;}

// some math macros
#define DEG_TO_RAD(ang) ((ang)*PI/180.0)
#define RAD_TO_DEG(rads) ((rads)*180.0/PI)


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// DEFINES/////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// pi defines
#define PI         ((float)3.141592654f)
#define PI2        ((float)6.283185307f)
#define PI_DIV_2   ((float)1.570796327f)
#define PI_DIV_4   ((float)0.785398163f)
#define PI_INV     ((float)0.318309886f)

// fixed point mathematics constants for 16.16 fixed point math library
#define FIXP16_SHIFT     16
#define FIXP16_MAG       65536
#define FIXP16_DP_MASK   0x0000ffff
#define FIXP16_WP_MASK   0xffff0000
#define FIXP16_ROUND_UP  0x00008000

// general interrupt rates for 28.636360Mhz clock
#define VGA_INTERRUPT_RATE   910
#define NTSC_INTERRUPT_RATE  1817

// with the current D/A converter range 0 .. 15, 1 = 62mV
#define NTSC_SYNC           (0x0F)     // 0V  
#define NTSC_BLACK          (0x4F)     // 0.3V (about 30-40 IRE)

#define NTSC_CBURST         (NTSC_CBURST0) 
#define NTSC_CBURST0        (0x40) 
#define NTSC_CBURST1        (0x41) 
#define NTSC_CBURST2        (0x42) 

#define COLOR_LUMA    (NTSC_CBURST0 + 0x40)

#define NTSC_GREEN    (COLOR_LUMA + 0)        
#define NTSC_YELLOW   (COLOR_LUMA + 2)        
#define NTSC_ORANGE   (COLOR_LUMA + 4)        
#define NTSC_RED      (COLOR_LUMA + 5)        
#define NTSC_VIOLET   (COLOR_LUMA + 9)        
#define NTSC_PURPLE   (COLOR_LUMA + 11)        
#define NTSC_BLUE     (COLOR_LUMA + 14)        

#define NTSC_GRAY0    (NTSC_BLACK + 0x10)        
#define NTSC_GRAY1    (NTSC_BLACK + 0x20)        
#define NTSC_GRAY2    (NTSC_BLACK + 0x30)        
#define NTSC_GRAY3    (NTSC_BLACK + 0x40)        
#define NTSC_GRAY4    (NTSC_BLACK + 0x50)        
#define NTSC_GRAY5    (NTSC_BLACK + 0x60)        
#define NTSC_GRAY6    (NTSC_BLACK + 0x70)        
#define NTSC_GRAY7    (NTSC_BLACK + 0x80)        
#define NTSC_GRAY8    (NTSC_BLACK + 0x90)        
#define NTSC_GRAY9    (NTSC_BLACK + 0xA0)        
#define NTSC_WHITE    (NTSC_BLACK + 0xB0)        

// VGA color channel masks
#define VGA_RMASK           0b00000011
#define VGA_GMASK           0b00001100
#define VGA_BMASK           0b00110000

// VGA basic colors
#define VGA_RGB_BLACK       0b00000000
#define VGA_RGB_RED         0b00000011
#define VGA_RGB_GREEN       0b00001100
#define VGA_RGB_BLUE        0b00110000
#define VGA_RGB_WHITE       0b00111111
#define VGA_RGB_GRAY        0b00101010

// VGA sync signals
#define VGA_HSYNC           0b01000000
#define VGA_VSYNC           0b10000000

// used to access data structures in ASM easily
#define SPRITE_INDEX_STATE   0
#define SPRITE_INDEX_ATTR    1
#define SPRITE_INDEX_COLOR   2
#define SPRITE_INDEX_HEIGHT  3
#define SPRITE_INDEX_X       4
#define SPRITE_INDEX_Y       5
#define SPRITE_INDEX_BITMAP  6

// sprite attributes
#define SPRITE_ATTR_2X       1  // enable 2X scaling mode

// size of SPRITE type in bytes
#define SPRITE_SIZE          8  

// tile map console defines
// added 11/10/08

// these refer to the starting bitmap indices in the tile bitmaps for the printable character set
// due to memory constraints only a sub-set of characters are defined and used for printing to keep the memory 
// footprint low, so all character sets that you define must adher to these constraints unless you want to make changes
// to the tile map console and printing functions
#define TMAP_BASE_BLANK     0  // character index for space
#define TMAP_BASE_NUMBERS   1  // character index for 0...9 
#define TMAP_BASE_PUNC      11 // character index for . ,
#define TMAP_BASE_LETTERS   13 // character index for A...Z
#define TMAP_NUM_CHARACTERS 39 // total number of characters in character set


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// TYPES/CLASSES //////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// SPRITE type, 8 bytes each
typedef struct SPRITE_TYP
    {
    // SPRITE members, note depending on driver some are used, some are not
    UCHAR state;    // state of sprite    
    UCHAR attr;     // bit encoded attributes
    UCHAR color;    // general color information
    UCHAR height;   // height of sprite
    UCHAR x;        // x position of sprite
    UCHAR y;        // y position of sprite
    UCHAR *bitmap;  // pointer to bitmap (2 bytes)

    } SPRITE, *SPRITE_PTR;

// a 2D vertex
typedef struct VERTEX2DI_TYP
        {
        int x,y; // the vertex
        } VERTEX2DI, *VERTEX2DI_PTR;

// a 2D vertex
typedef struct VERTEX2DF_TYP
        {
        float x,y; // the vertex
        } VERTEX2DF, *VERTEX2DF_PTR;

// a 2D polygon
typedef struct POLYGON2D_TYP
        {
        int state;        // state of polygon
        int num_verts;    // number of vertices
        int x0,y0;        // position of center of polygon
        int xv,yv;        // initial velocity
        int color;        // color index, or actual RGB, NTSC encoding
        VERTEX2DF *vlist; // pointer to vertex list

        } POLYGON2D, *POLYGON2D_PTR;

// matrix defines

// 3x3 matrix ///////////////////////////////////////////////////////////
typedef struct MATRIX3X3_TYP
        {
        union
        {
        float M[3][3]; // array indexed data storage

        // storage in row major form with explicit names
        struct
             {
             float M00, M01, M02;
             float M10, M11, M12;
             float M20, M21, M22;
             }; // end explicit names

        }; // end union
        } MATRIX3X3, *MATRIX3X3_PTR;

// 1x3 matrix ///////////////////////////////////////////////////////////
typedef struct MATRIX1X3_TYP
        {
        union
        {
        float M[3]; // array indexed data storage

        // storage in row major form with explicit names
        struct
             {
             float M00, M01, M02;

             }; // end explicit names
        }; // end union
        } MATRIX1X3, *MATRIX1X3_PTR;

// 3x2 matrix ///////////////////////////////////////////////////////////
typedef struct MATRIX3X2_TYP
        {
        union
        {
        float M[3][2]; // array indexed data storage

        // storage in row major form with explicit names
        struct
             {
             float M00, M01;
             float M10, M11;
             float M20, M21;
             }; // end explicit names

        }; // end union
        } MATRIX3X2, *MATRIX3X2_PTR;

// 1x2 matrix ///////////////////////////////////////////////////////////
typedef struct MATRIX1X2_TYP
        {
        union
        {
        float M[2]; // array indexed data storage

        // storage in row major form with explicit names
        struct
             {
             float M00, M01;

             }; // end explicit names
        }; // end union
        } MATRIX1X2, *MATRIX1X2_PTR;


// fixed point types ////////////////////////////////////////////////////
typedef long FIXP16;
typedef long *FIXP16_PTR;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// GLOBALS ////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// storage for our lookup tables, for now declare as size 1 until FLASH is working
extern float cos_look[]; // 361, 1 extra so we can store 0-360 inclusive
extern float sin_look[]; // 361, 1 extra so we can store 0-360 inclusive

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// EXTERNALS //////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// tile and vram pointers
volatile extern UCHAR *tile_map_ptr;
volatile extern UCHAR *tile_bitmaps_ptr;
volatile extern UCHAR *vram_ptr;

// sprite table
volatile extern SPRITE sprite_table[];

// screen size variables for bitmap and tile modes
extern int g_screen_width;
extern int g_screen_height;

extern int g_tile_map_width;
extern int g_tile_map_height;

// clipping variables
extern int g_clip_min_x;
extern int g_clip_max_x;
extern int g_clip_min_y;
extern int g_clip_max_y;

// these globals keep track of the tile map console printing cursor position
extern int g_tmap_cursor_x;
extern int g_tmap_cursor_y;

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

// these globals keep track of the tile map console printing cursor position
extern int g_tmap_cursor_x;
extern int g_tmap_cursor_y;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PROTOTYPES /////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// initializes the graphics driver and start up the HSYNC interrupt 
void GFX_Init(int video_interrupt_rate);

// plots a pixel in 2-bit bitmap modes
void GFX_Plot(int x, int y, int c, UCHAR *vbuffer);

// plots a pixel in 2-bit bitmap modes with clipping rectangle
void GFX_Plot_Clipped(int x, int y, int c, UCHAR *vbuffer);

// returns the 2-bit pixel code at (x,y)
unsigned char GFX_Get_Pixel(int x, int y, UCHAR *vbuffer);

// fills the screen with sent color
void GFX_Fill_Screen(int c, UCHAR *vbuffer);

// fills a vertical region on the screen with sent color
void GFX_Fill_Region(int c, int y1, int y2, UCHAR *vbuffer);

// set the bitmap clipping rectangle for all 2D operations
void GFX_Set_Clipping_Rect(int clip_min_x, int clip_min_y, int clip_max_x, int clip_max_y);

// draws a horizontal line efficiently, special case
void GFX_Draw_HLine(int x1,int x2,int y, UCHAR c, UCHAR *vbuffer);

// draws a vertical line efficiently, special case
void GFX_Draw_VLine(int y1,int y2,int x,int c,UCHAR *vbuffer);

void GFX_Draw_Clip_Line(int x0,int y0, int x1, int y1, int color, UCHAR *dest_buffer);

int GFX_Clip_Line(int *_x1,int *_y1,int *_x2, int *_y2);

void GFX_Draw_Line(int x0, int y0,   // starting position
                   int x1, int y1,   // ending position
                   int color,        // color index
                   UCHAR *vb_start); // video buffer

// draw circles
void GFX_Draw_Circle(int x0, int y0, int r, int c, UCHAR *vbuffer);
void GFX_Draw_Circle_Clipped(int x0, int y0, int r, int c, UCHAR *vbuffer);

// 2d 8-bit, 16-bit triangle rendering
void GFX_Draw_Top_Tri(int x1,int y1,int x2,int y2, int x3,int y3,int color,UCHAR *dest_buffer);
void GFX_Draw_Bottom_Tri(int x1,int y1, int x2,int y2, int x3,int y3,int color,UCHAR *dest_buffer);
void GFX_Draw_Triangle_2D(int x1,int y1,int x2,int y2,int x3,int y3, int color,UCHAR *dest_buffer);

void GFX_Draw_Top_TriFP(int x1,int y1,int x2,int y2, int x3,int y3,int color,UCHAR *dest_buffer);
void GFX_Draw_Bottom_TriFP(int x1,int y1, int x2,int y2, int x3,int y3,int color,UCHAR *dest_buffer);
void GFX_Draw_TriangleFP_2D(int x1,int y1,int x2,int y2,int x3,int y3, int color,UCHAR *dest_buffer);

// tile mapped console support for NTSC tile mode 5 with 6x8 characters
void GFX_TMap_Print_Char(char ch);
void GFX_TMap_Print_String(char *string);
void GFX_TMap_CLS(char tile_index);
void GFX_TMap_Setcursor(int x, int y);

// general 2D 8-bit, 16-bit polygon rendering and transforming functions
int Translate_Polygon2D(POLYGON2D_PTR poly, int dx, int dy);
int Rotate_Polygon2D(POLYGON2D_PTR poly, int theta);
int Scale_Polygon2D(POLYGON2D_PTR poly, float sx, float sy);
void Build_Sin_Cos_Tables(void);
int Translate_Polygon2D_Mat(POLYGON2D_PTR poly, int dx, int dy);
int Rotate_Polygon2D_Mat(POLYGON2D_PTR poly, int theta);
int Scale_Polygon2D_Mat(POLYGON2D_PTR poly, float sx, float sy);
int GFX_Draw_Polygon2D(POLYGON2D_PTR poly, UCHAR *vbuffer);

// math functions
int Fast_Distance_2D(int x, int y);
float Fast_Distance_3D(float x, float y, float z);

// uses look up table to compute fractional sin/cos
float Fast_Sin(float theta);
float Fast_Cos(float theta);

// some fixed point 
FIXP16 FIXP16_DIV(FIXP16 fp1, FIXP16 fp2);
FIXP16 FIXP16_MUL(FIXP16 fp1, FIXP16 fp2);

// collision detection functions
int Find_Bounding_Box_Poly2D(POLYGON2D_PTR poly,
                             float *min_x, float *max_x,
                             float *min_y, float *max_y);

int Mat_Mul_1X2_3X2(MATRIX1X2_PTR ma,
                   MATRIX3X2_PTR mb,
                   MATRIX1X2_PTR mprod);

int Mat_Mul_1X3_3X3(MATRIX1X3_PTR ma,
                   MATRIX3X3_PTR mb,
                   MATRIX1X3_PTR mprod);

int Mat_Mul_3X3(MATRIX3X3_PTR ma,
               MATRIX3X3_PTR mb,
               MATRIX3X3_PTR mprod);

int Mat_Init_3X2(MATRIX3X2_PTR ma,
                 float m00, float m01,
                 float m10, float m11,
                float m20, float m21);

// INLINE FUNCTIONS ///////////////////////////////////////////////////////////////////////////////////////////////////


// end multiple inclusions 
#endif
