/* fixedpt_16_16.h  
FIXED point manpulation unsing 32-bit representation 16.16 */

#ifndef FIXEDPT_16_16_H
#define FIXEDPT_16_16_H

/*Data structures*/

typedef  int FIXPONT ;

/*Constants*/

 #define FP_SHIFT 16
 #define FP_SCALE 65536

/*Macros*/

 #define INT_TO_FXPONT(n)    ((FIXPONT)(n<<FP_SHIFT))
 #define FLT_TO_FIXPONT(n)   ((FIXPONT) ((float)n*FP_SCALE))
 #define INTEGRL_FX_PART(f)  (f>>FP_SHIFT)
 #define FRACTION_FX_PART(f) (f&0x0000FFFF)
 #define FRACT_DISPlAY(f)    (100*(unsigned int)FRACTION_FX_PART(f)/FP_SCALE) // have to be refine .
 #define MUL_FXPONT(f1,f2)   ((FIXPONT)(((f1)*(f2))>>FP_SHIFT))
 #define DIV_FXPONT(f1,f2)   ((FIXPONT)(((f1)<<FP_SCALE)/(f2)))

#endif //FIXEDPT_16_16_H
