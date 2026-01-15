/*
 * Tiny Math Library
 *
 * Copyright (c) 2024 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
/*
 * ====================================================
 * Copyright (C) 2004 by Sun Microsystems, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */
#include <inttypes.h>
#include <math.h>
#define NDEBUG
#include <assert.h>

#include "cutils.h"
#include "libm.h"

/* define to enable softfloat support */
//#define USE_SOFTFLOAT
/* use less code for tan() but currently less precise */
#define USE_TAN_SHORTCUT 

/* 
  TODO:
  - smaller scalbn implementation ?
  - add all ES6 math functions
*/
/*
   tc32: 
   - base: size libm+libgcc: 21368
   - size libm+libgcc: 11832

   x86:
   - size libm softfp: 18510
   - size libm hardfp: 10051

   TODO:
   - unify i32 bit and i64 bit conversions
   - unify comparisons operations
*/

typedef enum {
    RM_RNE, /* Round to Nearest, ties to Even */
    RM_RTZ, /* Round towards Zero */
    RM_RDN, /* Round Down (must be even) */
    RM_RUP, /* Round Up (must be odd) */
    RM_RMM, /* Round to Nearest, ties to Max Magnitude */
    RM_RMMUP, /* only for rint_sf64(): round to nearest, ties to +inf (must be odd) */
} RoundingModeEnum;

#define FFLAG_INVALID_OP  (1 << 4)
#define FFLAG_DIVIDE_ZERO (1 << 3)
#define FFLAG_OVERFLOW    (1 << 2)
#define FFLAG_UNDERFLOW   (1 << 1)
#define FFLAG_INEXACT     (1 << 0)

typedef enum {
    FMINMAX_PROP, /* min(1, qNaN/sNaN) -> qNaN */
    FMINMAX_IEEE754_2008, /* min(1, qNaN) -> 1, min(1, sNaN) -> qNaN */
    FMINMAX_IEEE754_201X, /* min(1, qNaN/sNaN) -> 1 */
} SoftFPMinMaxTypeEnum;

typedef uint32_t sfloat32;
typedef uint64_t sfloat64;

#define F_STATIC static __maybe_unused
#define F_USE_FFLAGS 0

#define F_SIZE 32
#define F_NORMALIZE_ONLY
#include "softfp_template.h"

#define F_SIZE 64
#include "softfp_template.h"

#ifdef USE_SOFTFLOAT

/* wrappers */
double __adddf3(double a, double b)
{
    return uint64_as_float64(add_sf64(float64_as_uint64(a),
                                     float64_as_uint64(b), RM_RNE));
}

double __subdf3(double a, double b)
{
    return uint64_as_float64(sub_sf64(float64_as_uint64(a),
                                      float64_as_uint64(b), RM_RNE));
}

double __muldf3(double a, double b)
{
    return uint64_as_float64(mul_sf64(float64_as_uint64(a),
                                      float64_as_uint64(b), RM_RNE));
}

double __divdf3(double a, double b)
{
    return uint64_as_float64(div_sf64(float64_as_uint64(a),
                                      float64_as_uint64(b), RM_RNE));
}

/* comparisons */

int __eqdf2(double a, double b)
{
    int ret = cmp_sf64(float64_as_uint64(a),
                       float64_as_uint64(b));
    return ret;
}

/* NaN: return 0 */
int __nedf2(double a, double b)
{
    int ret = cmp_sf64(float64_as_uint64(a),
                       float64_as_uint64(b));
    if (unlikely(ret == 2))
        return 0;
    else
        return ret;
}

int __ledf2(double a, double b)
{
    int ret = cmp_sf64(float64_as_uint64(a),
                       float64_as_uint64(b));
    return ret;
}

int __ltdf2(double a, double b)
{
    int ret = cmp_sf64(float64_as_uint64(a),
                       float64_as_uint64(b));
    return ret;
}

int __gedf2(double a, double b)
{
    int ret = cmp_sf64(float64_as_uint64(a),
                       float64_as_uint64(b));
    if (unlikely(ret == 2))
        return -1;
    else
        return ret;
}

int __gtdf2(double a, double b)
{
    int ret = cmp_sf64(float64_as_uint64(a),
                       float64_as_uint64(b));
    if (unlikely(ret == 2))
        return -1;
    else
        return ret;
}

int __unorddf2(double a, double b)
{
    return isnan_sf64(float64_as_uint64(a)) ||
        isnan_sf64(float64_as_uint64(b));
}

/* conversions */
double __floatsidf(int32_t a)
{
    return uint64_as_float64(cvt_i32_sf64(a, RM_RNE));
}

double __floatdidf(int64_t a)
{
    return uint64_as_float64(cvt_i64_sf64(a, RM_RNE));
}

double __floatunsidf(unsigned int a)
{
    return uint64_as_float64(cvt_u32_sf64(a, RM_RNE));
}

int32_t __fixdfsi(double a)
{
    return cvt_sf64_i32(float64_as_uint64(a), RM_RTZ);
}

double __extendsfdf2(float a)
{
    return uint64_as_float64(cvt_sf32_sf64(float_as_uint(a)));
}

float __truncdfsf2(double a)
{
    return uint_as_float(cvt_sf64_sf32(float64_as_uint64(a), RM_RNE));
}

double js_sqrt(double a)
{
    return uint64_as_float64(sqrt_sf64(float64_as_uint64(a), RM_RNE));
}

#if defined(__tc32__)
/* XXX: check */
int __fpclassifyd(double a)
{
    uint64_t u = float64_as_uint64(a);
    uint32_t h = u >> 32;
    uint32_t l = u;

    h &= 0x7fffffff;
    if (h >= 0x7ff00000) {
        if (h == 0x7ff00000 && l == 0)
            return FP_INFINITE;
        else
            return FP_NAN;
    } else if (h < 0x00100000) {
        if (h == 0 && l == 0)
            return FP_ZERO;
        else
            return FP_SUBNORMAL;
    } else {
        return FP_NORMAL;
    }
}
#endif

#endif /* USE_SOFTFLOAT */

int32_t js_lrint(double a)
{
    return cvt_sf64_i32(float64_as_uint64(a), RM_RNE);
}

double js_fmod(double a, double b)
{
    return uint64_as_float64(fmod_sf64(float64_as_uint64(a), float64_as_uint64(b)));
}

/* supported rounding modes: RM_UP, RM_DN, RM_RTZ, RM_RMMUP, RM_RMM */
static double rint_sf64(double a, RoundingModeEnum rm)
{
    uint64_t u = float64_as_uint64(a);
    uint64_t frac_mask, one, m, addend;
    int e;
    unsigned int s;

    e = ((u >> 52) & 0x7ff) - 0x3ff;
    s = u >> 63;
    if (e < 0) {
        m = u & (((uint64_t)1 << 52) - 1);
        if (e == -0x3ff && m == 0) {
            /* zero: nothing to do */
        } else {
            /* abs(a) < 1 */
            s = u >> 63;
            one = (uint64_t)0x3ff << 52;
            u = 0;
            switch(rm) {
            case RM_RUP:
            case RM_RDN:
                if (s ^ (rm & 1))
                    u = one;
                break;
            default:
            case RM_RMM:
            case RM_RMMUP:
                if (e == -1 && (m != 0 || (m == 0 && (!s || rm == RM_RMM))))
                    u = one;
                break;
            case RM_RTZ:
                break;
            }
            u |= (uint64_t)s << 63;
        }
    } else if (e < 52) {
        one = (uint64_t)1 << (52 - e);
        frac_mask = one - 1;
        addend = 0;
        switch(rm) {
        case RM_RMMUP:
            addend = (one >> 1) - s;
            break;
        default:
        case RM_RMM:
            addend = (one >> 1);
            break;
        case RM_RTZ:
            break;
        case RM_RUP:
        case RM_RDN:
            if (s ^ (rm & 1))
                addend = one - 1;
            break;
        }
        u += addend;
        u &= ~frac_mask; /* truncate to an integer */
    }
    /* otherwise: abs(a) >= 2^52, or NaN, +/-Infinity: no change */
    return uint64_as_float64(u);
}

double js_floor(double x)
{
    return rint_sf64(x, RM_RDN);
}

double js_ceil(double x)
{
    return rint_sf64(x, RM_RUP);
}

double js_trunc(double x)
{
    return rint_sf64(x, RM_RTZ);
}

double js_round_inf(double x)
{
    return rint_sf64(x, RM_RMMUP);
}

double js_fabs(double x)
{
    uint64_t a = float64_as_uint64(x);
    return uint64_as_float64(a & 0x7fffffffffffffff);
}

/************************************************************/
/* libm */

#define EXTRACT_WORDS(ix0,ix1,d)				\
    do {                                                        \
        uint64_t __u = float64_as_uint64(d);                    \
        (ix0) = (uint32_t)(__u >> 32);                          \
        (ix1) = (uint32_t)__u;                                  \
    } while (0)

static uint32_t get_high_word(double d)
{
    return float64_as_uint64(d) >> 32;
}

static double set_high_word(double d, uint32_t h)
{
    uint64_t u = float64_as_uint64(d);
    u = (u & 0xffffffff) | ((uint64_t)h << 32);
    return uint64_as_float64(u);
}

static uint32_t get_low_word(double d)
{
    return float64_as_uint64(d);
}

/* set the low 32 bits to zero */
static double zero_low(double x)
{
    uint64_t u = float64_as_uint64(x);
    u &= 0xffffffff00000000;
    return uint64_as_float64(u);
}

static double float64_from_u32(uint32_t h, uint32_t l)
{
    return uint64_as_float64(((uint64_t)h << 32) | l);
}

static const double zero   = 0.0;
static const double one    = 1.0;
static const double half =  5.00000000000000000000e-01;
static const double tiny = 1.0e-300;
static const double huge = 1.0e300;

/* @(#)s_scalbn.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

/* 
 * scalbn (double x, int n)
 * scalbn(x,n) returns x* 2**n  computed by  exponent  
 * manipulation rather than by actually performing an 
 * exponentiation or a multiplication.
 */

static const double
    two54   =  1.80143985094819840000e+16, /* 0x43500000, 0x00000000 */
    twom54  =  5.55111512312578270212e-17; /* 0x3C900000, 0x00000000 */

double js_scalbn(double x, int n)
{
	int  k,hx,lx;
	EXTRACT_WORDS(hx, lx, x);
        k = (hx&0x7ff00000)>>20;		/* extract exponent */
        if (k==0) {				/* 0 or subnormal x */
            if ((lx|(hx&0x7fffffff))==0) return x; /* +-0 */
	    x *= two54; 
	    hx = get_high_word(x);
	    k = ((hx&0x7ff00000)>>20) - 54; 
            if (n< -50000) return tiny*x; 	/*underflow*/
	    }
        if (k==0x7ff) return x+x;		/* NaN or Inf */
        k = k+n; 
        if (k >  0x7fe) return huge*copysign(huge,x); /* overflow  */
        if (k > 0) 				/* normal result */
	    {x = set_high_word(x, (hx&0x800fffff)|(k<<20)); return x;}
        if (k <= -54) {
          if (n > 50000) /* in case integer overflow in n+k */
            return huge * copysign(huge, x); /*overflow*/
          else
            return tiny * copysign(tiny, x); /*underflow*/
        }
        k += 54;				/* subnormal result */
        x = set_high_word(x, (hx&0x800fffff)|(k<<20));
        return x*twom54;
}

#ifndef USE_SOFTFLOAT
/* @(#)e_sqrt.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

/* __ieee754_sqrt(x)
 * Return correctly rounded sqrt.
 *           ------------------------------------------
 *	     |  Use the hardware sqrt if you have one |
 *           ------------------------------------------
 * Method: 
 *   Bit by bit method using integer arithmetic. (Slow, but portable) 
 *   1. Normalization
 *	Scale x to y in [1,4) with even powers of 2: 
 *	find an integer k such that  1 <= (y=x*2^(2k)) < 4, then
 *		sqrt(x) = 2^k * sqrt(y)
 *   2. Bit by bit computation
 *	Let q  = sqrt(y) truncated to i bit after binary point (q = 1),
 *	     i							 0
 *                                     i+1         2
 *	    s  = 2*q , and	y  =  2   * ( y - q  ).		(1)
 *	     i      i            i                 i
 *                                                        
 *	To compute q    from q , one checks whether 
 *		    i+1       i                       
 *
 *			      -(i+1) 2
 *			(q + 2      ) <= y.			(2)
 *     			  i
 *							      -(i+1)
 *	If (2) is false, then q   = q ; otherwise q   = q  + 2      .
 *		 	       i+1   i             i+1   i
 *
 *	With some algebraic manipulation, it is not difficult to see
 *	that (2) is equivalent to 
 *                             -(i+1)
 *			s  +  2       <= y			(3)
 *			 i                i
 *
 *	The advantage of (3) is that s  and y  can be computed by 
 *				      i      i
 *	the following recurrence formula:
 *	    if (3) is false
 *
 *	    s     =  s  ,	y    = y   ;			(4)
 *	     i+1      i		 i+1    i
 *
 *	    otherwise,
 *                         -i                     -(i+1)
 *	    s	  =  s  + 2  ,  y    = y  -  s  - 2  		(5)
 *           i+1      i          i+1    i     i
 *				
 *	One may easily use induction to prove (4) and (5). 
 *	Note. Since the left hand side of (3) contain only i+2 bits,
 *	      it does not necessary to do a full (53-bit) comparison 
 *	      in (3).
 *   3. Final rounding
 *	After generating the 53 bits result, we compute one more bit.
 *	Together with the remainder, we can decide whether the
 *	result is exact, bigger than 1/2ulp, or less than 1/2ulp
 *	(it will never equal to 1/2ulp).
 *	The rounding mode can be detected by checking whether
 *	huge + tiny is equal to huge, and whether huge - tiny is
 *	equal to huge for some floating point number "huge" and "tiny".
 *		
 * Special cases:
 *	sqrt(+-0) = +-0 	... exact
 *	sqrt(inf) = inf
 *	sqrt(-ve) = NaN		... with invalid signal
 *	sqrt(NaN) = NaN		... with invalid signal for signaling NaN
 *
 * Other methods : see the appended file at the end of the program below.
 *---------------
 */

#if defined(__aarch64__) || defined(__x86_64__) || defined(__i386__)
/* hardware sqrt is available */
double js_sqrt(double x)
{
    return sqrt(x);
}
#else
double js_sqrt(double x)
{
	double z;
	int 	sign = (int)0x80000000; 
	unsigned r,t1,s1,ix1,q1;
	int ix0,s0,q,m,t,i;

	EXTRACT_WORDS(ix0, ix1, x);

    /* take care of Inf and NaN */
	if((ix0&0x7ff00000)==0x7ff00000) {			
	    return x*x+x;		/* sqrt(NaN)=NaN, sqrt(+inf)=+inf
					   sqrt(-inf)=sNaN */
	} 
    /* take care of zero */
	if(ix0<=0) {
	    if(((ix0&(~sign))|ix1)==0) return x;/* sqrt(+-0) = +-0 */
	    else if(ix0<0)
		return (x-x)/(x-x);		/* sqrt(-ve) = sNaN */
	}
    /* normalize x */
	m = (ix0>>20);
	if(m==0) {				/* subnormal x */
	    while(ix0==0) {
		m -= 21;
		ix0 |= (ix1>>11); ix1 <<= 21;
	    }
	    for(i=0;(ix0&0x00100000)==0;i++) ix0<<=1;
	    m -= i-1;
	    ix0 |= (ix1>>(32-i));
	    ix1 <<= i;
	}
	m -= 1023;	/* unbias exponent */
	ix0 = (ix0&0x000fffff)|0x00100000;
	if(m&1){	/* odd m, double x to make it even */
	    ix0 += ix0 + ((ix1&sign)>>31);
	    ix1 += ix1;
	}
	m >>= 1;	/* m = [m/2] */

    /* generate sqrt(x) bit by bit */
	ix0 += ix0 + ((ix1&sign)>>31);
	ix1 += ix1;
	q = q1 = s0 = s1 = 0;	/* [q,q1] = sqrt(x) */
	r = 0x00200000;		/* r = moving bit from right to left */

	while(r!=0) {
	    t = s0+r; 
	    if(t<=ix0) { 
		s0   = t+r; 
		ix0 -= t; 
		q   += r; 
	    } 
	    ix0 += ix0 + ((ix1&sign)>>31);
	    ix1 += ix1;
	    r>>=1;
	}

	r = sign;
	while(r!=0) {
	    t1 = s1+r; 
	    t  = s0;
	    if((t<ix0)||((t==ix0)&&(t1<=ix1))) { 
		s1  = t1+r;
		if(((t1&sign)==sign)&&(s1&sign)==0) s0 += 1;
		ix0 -= t;
		if (ix1 < t1) ix0 -= 1;
		ix1 -= t1;
		q1  += r;
	    }
	    ix0 += ix0 + ((ix1&sign)>>31);
	    ix1 += ix1;
	    r>>=1;
	}

    /* use floating add to find out rounding direction */
	if((ix0|ix1)!=0) {
	    z = one-tiny; /* trigger inexact flag */
	    if (z>=one) {
	        z = one+tiny;
	        if (q1==(unsigned)0xffffffff) { q1=0; q += 1;}
		else if (z>one) {
		    if (q1==(unsigned)0xfffffffe) q+=1;
		    q1+=2; 
		} else
	            q1 += (q1&1);
	    }
	}
	ix0 = (q>>1)+0x3fe00000;
	ix1 =  q1>>1;
	if ((q&1)==1) ix1 |= sign;
	ix0 += (m <<20);
	return float64_from_u32(ix0, ix1);
}
#endif /* !hardware sqrt */
#endif /* USE_SOFTFLOAT */

/* to have smaller code */
/* n >= 1 */
/* return sum(x^i*coefs[i] with i = 0 ... n - 1 and n >= 1 using
   Horner algorithm. */
static double eval_poly(double x, const double *coefs, int n)
{
    double r;
    int i;
    r = coefs[n - 1];
    for(i = n - 2; i >= 0; i--) {
        r = r * x + coefs[i];
    }
    return r;
}

/* @(#)k_sin.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

/* __kernel_sin( x, y, iy)
 * kernel sin function on [-pi/4, pi/4], pi/4 ~ 0.7854
 * Input x is assumed to be bounded by ~pi/4 in magnitude.
 * Input y is the tail of x.
 * Input iy indicates whether y is 0. (if iy=0, y assume to be 0). 
 *
 * Algorithm
 *	1. Since sin(-x) = -sin(x), we need only to consider positive x. 
 *	2. if x < 2^-27 (hx<0x3e400000 0), return x with inexact if x!=0.
 *	3. sin(x) is approximated by a polynomial of degree 13 on
 *	   [0,pi/4]
 *		  	         3            13
 *	   	sin(x) ~ x + S1*x + ... + S6*x
 *	   where
 *	
 * 	|sin(x)         2     4     6     8     10     12  |     -58
 * 	|----- - (1+S1*x +S2*x +S3*x +S4*x +S5*x  +S6*x   )| <= 2
 * 	|  x 					           | 
 * 
 *	4. sin(x+y) = sin(x) + sin'(x')*y
 *		    ~ sin(x) + (1-x*x/2)*y
 *	   For better accuracy, let 
 *		     3      2      2      2      2
 *		r = x *(S2+x *(S3+x *(S4+x *(S5+x *S6))))
 *	   then                   3    2
 *		sin(x) = x + (S1*x + (x *(r-y/2)+y))
 */

static const double 
S1  = -1.66666666666666324348e-01; /* 0xBFC55555, 0x55555549 */
static const double S_tab[] = {
    /* S2 */  8.33333333332248946124e-03, /* 0x3F811111, 0x1110F8A6 */
    /* S3 */ -1.98412698298579493134e-04, /* 0xBF2A01A0, 0x19C161D5 */
    /* S4 */  2.75573137070700676789e-06, /* 0x3EC71DE3, 0x57B1FE7D */
    /* S5 */ -2.50507602534068634195e-08, /* 0xBE5AE5E6, 0x8A2B9CEB */
    /* S6 */  1.58969099521155010221e-10, /* 0x3DE5D93A, 0x5ACFD57C */
};

/* iy=0 if y is zero */
static double __kernel_sin(double x, double y, int iy)
{
	double z,r,v;
	int ix;
	ix = get_high_word(x)&0x7fffffff;	/* high word of x */
	if(ix<0x3e400000)			/* |x| < 2**-27 */
	   {if((int)x==0) return x;}		/* generate inexact */
	z	=  x*x;
	v	=  z*x;
	r	=  eval_poly(z, S_tab, 5);
	if(iy==0) return x+v*(S1+z*r);
	else      return x-((z*(half*y-v*r)-y)-v*S1);
}


/* @(#)k_cos.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

/*
 * __kernel_cos( x,  y )
 * kernel cos function on [-pi/4, pi/4], pi/4 ~ 0.785398164
 * Input x is assumed to be bounded by ~pi/4 in magnitude.
 * Input y is the tail of x. 
 *
 * Algorithm
 *	1. Since cos(-x) = cos(x), we need only to consider positive x.
 *	2. if x < 2^-27 (hx<0x3e400000 0), return 1 with inexact if x!=0.
 *	3. cos(x) is approximated by a polynomial of degree 14 on
 *	   [0,pi/4]
 *		  	                 4            14
 *	   	cos(x) ~ 1 - x*x/2 + C1*x + ... + C6*x
 *	   where the remez error is
 *	
 * 	|              2     4     6     8     10    12     14 |     -58
 * 	|cos(x)-(1-.5*x +C1*x +C2*x +C3*x +C4*x +C5*x  +C6*x  )| <= 2
 * 	|    					               | 
 * 
 * 	               4     6     8     10    12     14 
 *	4. let r = C1*x +C2*x +C3*x +C4*x +C5*x  +C6*x  , then
 *	       cos(x) = 1 - x*x/2 + r
 *	   since cos(x+y) ~ cos(x) - sin(x)*y 
 *			  ~ cos(x) - x*y,
 *	   a correction term is necessary in cos(x) and hence
 *		cos(x+y) = 1 - (x*x/2 - (r - x*y))
 *	   For better accuracy when x > 0.3, let qx = |x|/4 with
 *	   the last 32 bits mask off, and if x > 0.78125, let qx = 0.28125.
 *	   Then
 *		cos(x+y) = (1-qx) - ((x*x/2-qx) - (r-x*y)).
 *	   Note that 1-qx and (x*x/2-qx) is EXACT here, and the
 *	   magnitude of the latter is at least a quarter of x*x/2,
 *	   thus, reducing the rounding error in the subtraction.
 */

static const double C_tab[] = {
    /* C1 */  4.16666666666666019037e-02, /* 0x3FA55555, 0x5555554C */
    /* C2 */ -1.38888888888741095749e-03, /* 0xBF56C16C, 0x16C15177 */
    /* C3 */  2.48015872894767294178e-05, /* 0x3EFA01A0, 0x19CB1590 */
    /* C4 */ -2.75573143513906633035e-07, /* 0xBE927E4F, 0x809C52AD */
    /* C5 */  2.08757232129817482790e-09, /* 0x3E21EE9E, 0xBDB4B1C4 */
    /* C6 */ -1.13596475577881948265e-11, /* 0xBDA8FAE9, 0xBE8838D4 */
};

static double __kernel_cos(double x, double y)
{
	double a,hz,z,r,qx;
	int ix;
	ix = get_high_word(x)&0x7fffffff;	/* ix = |x|'s high word*/
	if(ix<0x3e400000) {			/* if x < 2**27 */
	    if(((int)x)==0) return one;		/* generate inexact */
	}
	z  = x*x;
	r  = z * eval_poly(z, C_tab, 6);
	if(ix < 0x3FD33333) 			/* if |x| < 0.3 */ 
	    return one - (0.5*z - (z*r - x*y));
	else {
	    if(ix > 0x3fe90000) {		/* x > 0.78125 */
		qx = 0.28125;
	    } else {
	        qx = float64_from_u32(ix-0x00200000, 0);	/* x/4 */
	    }
	    hz = 0.5*z-qx;
	    a  = one-qx;
	    return a - (hz - (z*r-x*y));
	}
}

/* rem_pio2 */

#define T_LEN 19

/* T[i] = floor(2^(64*(T_LEN - i))/2pi) mod 2^64 */
static const uint64_t T[T_LEN] = {
    0x1580cc11bf1edaea,
    0x9afed7ec47e35742,
    0xcf41ce7de294a4ba,
    0x5d49eeb1faf97c5e,
    0xd3d18fd9a797fa8b,
    0xdb4d9fb3c9f2c26d,
    0xfbcbc462d6829b47,
    0xc7fe25fff7816603,
    0x272117e2ef7e4a0e,
    0x4e64758e60d4ce7d,
    0x3a671c09ad17df90,
    0xba208d7d4baed121,
    0x3f877ac72c4a69cf,
    0x01924bba82746487,
    0x6dc91b8e909374b8,
    0x7f9458eaf7aef158,
    0x36d8a5664f10e410,
    0x7f09d5f47d4d3770,
    0x28be60db9391054a, /* high part */
};

/* PIO2[i] = floor(2^(64*(2 - i))*PI/4) mod 2^64 */
static const uint64_t PIO4[2] = {
    0xc4c6628b80dc1cd1,
    0xc90fdaa22168c234,
};

static uint64_t get_u64_at_bit(const uint64_t *tab, uint32_t tab_len,
                               uint32_t pos)
{
    uint64_t v;
    uint32_t p = pos / 64;
    int shift = pos % 64;
    v = tab[p] >> shift;
    if (shift != 0 && (p + 1) < tab_len)
        v |= tab[p + 1] << (64 - shift);
    return v;
}

/* return n = round(x/(pi/2)) (only low 2 bits are valid) and
   (y[0], y[1]) = x - (pi/2) * n. 
   'x' must be finite and such as abs(x) >= PI/4.
   The initial algorithm comes from the CORE-MATH project.
*/
static int rem_pio2_large(double x, double *y)
{
    uint64_t m;
    int e, sgn, n, rnd, j, i, y_sgn;
    uint64_t c[2], d[3];
    uint64_t r0, r1;
    uint32_t carry, carry1;

    m = float64_as_uint64(x);
    sgn = m >> 63;
    e = (m >> 52) & 0x7ff;
    /* 1022 <= e <= 2047 */
    m = (m & (((uint64_t)1 << 52) - 1)) | ((uint64_t)1 << 52);

    /* multiply m by T[j:j+192] */
    j = T_LEN * 64 - (e - 1075) - 192;
    /* 53 <= j <= 1077 */
    //    printf("m=0x%016" PRIx64 " e=%d j=%d\n", m, e, j);
    for(i = 0; i < 3; i++) {
        d[i] = get_u64_at_bit(T, T_LEN, j + i * 64);
    }
    r1 = mul_u64(&r0, m, d[0]);
    c[0] = r1;
    r1 = mul_u64(&r0, m, d[1]);
    c[0] += r0;
    carry = c[0] < r0;
    c[1] = r1 + carry;
    mul_u64(&r0, m, d[2]);
    c[1] += r0;

    //    printf("c0=%016" PRIx64 " %016" PRIx64 "\n", c[1], c[0]);

    /* n = round(c[1]/2^62) */
    n = c[1] >> 62;
    rnd = (c[1] >> 61) & 1;
    n += rnd;
    /* c = c * 4 - n */
    c[1] = (c[1] << 2) | (c[0] >> 62);
    c[0] = (c[0] << 2);
    y_sgn = sgn;
    if (rnd) {
        /* 'y' sign change */
        y_sgn ^= 1;
        c[0] = ~c[0];
        c[1] = ~c[1];
        if (++c[0] == 0)
            c[1]++;
    }
    //    printf("c1=%016" PRIx64 " %016" PRIx64 " n=%d sgn=%d\n", c[1], c[0], n, sgn);

    /* c = c * (PI/2) (high 128 bits of the product) */
    r1 = mul_u64(&r0, c[0], PIO4[1]);
    d[0] = r0;
    d[1] = r1;

    r1 = mul_u64(&r0, c[1], PIO4[0]);
    d[0] += r0;
    carry = d[0] < r0;
    d[1] += r1;
    carry1 = d[1] < r1;
    d[1] += carry;
    carry1 |= (d[1] < carry);
    d[2] = carry1;

    r1 = mul_u64(&r0, c[1], PIO4[1]);
    d[1] += r0;
    carry = d[1] < r0;
    d[2] += r1 + carry;
    
    /* convert d to two float64 */
    //    printf("d=%016" PRIx64 " %016" PRIx64 "\n", d[2], d[1]);
    if (d[2] == 0) {
        /* should never happen (see ARGUMENT REDUCTION FOR HUGE
           ARGUMENTS: Good to the Last Bit, K. C. Ng and the members
           of the FP group of SunPro */
        y[0] = y[1] = 0;
    } else {
        uint64_t m0, m1;
        int e1;
        
        e = clz64(d[2]);
        d[2] = (d[2] << e) | (d[1] >> (64 - e));
        d[1] = (d[1] << e);
        //        printf("d=%016" PRIx64 " %016" PRIx64 " e=%d\n", d[2], d[1], e);
        m0 = (d[2] >> 11) & (((uint64_t)1 << 52) - 1);
        m1 = ((d[2] & 0x7ff) << 42) | (d[1] >> (64 - 42));
        y[0] = uint64_as_float64(((uint64_t)y_sgn << 63) |
                                 ((uint64_t)(1023 - e) << 52) |
                                 m0);
        if (m1 == 0) {
            y[1] = 0;
        } else {
            e1 = clz64(m1) - 11;
            m1 = (m1 << e1) & (((uint64_t)1 << 52) - 1);
            y[1] = uint64_as_float64(((uint64_t)y_sgn << 63) |
                                     ((uint64_t)(1023 - e - 53 - e1) << 52) |
                                     m1);
        }
    }
    if (sgn)
        n = -n;
    return n;
}

#ifdef USE_SOFTFLOAT
/* when using softfloat, the FP reduction should be not much faster
   than the generic one */
int js_rem_pio2(double x, double *y)
{
    int ix,hx;

    hx = get_high_word(x);		/* high word of x */
    ix = hx&0x7fffffff;
    if(ix<=0x3fe921fb) {
        /* |x| ~<= pi/4 , no need for reduction */
        y[0] = x;
        y[1] = 0;
        return 0;
    }
    /* 
     * all other (large) arguments
     */
    if(ix>=0x7ff00000) {		/* x is inf or NaN */
        y[0]=y[1]=x-x;
        return 0;
    }

    return rem_pio2_large(x, y);
}
#else
/*
 * invpio2:  53 bits of 2/pi
 * pio2_1:   first  33 bit of pi/2
 * pio2_1t:  pi/2 - pio2_1
 * pio2_2:   second 33 bit of pi/2
 * pio2_2t:  pi/2 - (pio2_1+pio2_2)
 * pio2_3:   third  33 bit of pi/2
 * pio2_3t:  pi/2 - (pio2_1+pio2_2+pio2_3)
 */

static const double 
invpio2 =  6.36619772367581382433e-01; /* 0x3FE45F30, 0x6DC9C883 */
static const double pio2_tab[3] = {
    /* pio2_1 */  1.57079632673412561417e+00, /* 0x3FF921FB, 0x54400000 */
    /* pio2_2 */ 6.07710050630396597660e-11, /* 0x3DD0B461, 0x1A600000 */
    /* pio2_3 */  2.02226624871116645580e-21, /* 0x3BA3198A, 0x2E000000 */
};
static const double pio2_t_tab[3] = {
    /* pio2_1t */  6.07710050650619224932e-11, /* 0x3DD0B461, 0x1A626331 */
    /* pio2_2t */  2.02226624879595063154e-21, /* 0x3BA3198A, 0x2E037073 */
    /* pio2_3t */  8.47842766036889956997e-32, /* 0x397B839A, 0x252049C1 */
};
static uint8_t rem_pio2_emax[2] = { 16, 49 };

int js_rem_pio2(double x, double *y)
{
    double w,t,r,fn;
    int i,j,n,ix,hx,it;

    hx = get_high_word(x);		/* high word of x */
    ix = hx&0x7fffffff;
    if(ix<=0x3fe921fb) {
        /* |x| ~<= pi/4 , no need for reduction */
        y[0] = x;
        y[1] = 0;
        return 0;
    }
    if(ix<=0x413921fb) { /* |x| ~<= 2^19*(pi/2), medium size */
        t  = fabs(x);
        if (ix<0x4002d97c) {
            /* |x| < 3pi/4, special case with n=+-1 */
            n = 1;
            fn = 1;
        } else {
            n  = (int) (t*invpio2+half);
            fn = (double)n;
        }

        it = 0;
        for(;;) {
            /* 1st round good to 85 bit */
            /* 2nd iteration needed, good to 118 */
            /* 3rd iteration need, 151 bits acc */
            r  = t-fn*pio2_tab[it];
            w  = fn*pio2_t_tab[it];	
            y[0] = r-w;
            j  = ix>>20;
            i = j-(((get_high_word(y[0]))>>20)&0x7ff);
            if (it == 2 || i <= rem_pio2_emax[it])
                break;
            t = r;
            it++;
        }
        y[1] = (r-y[0])-w;
        if (hx<0) {
            y[0] = -y[0];
            y[1] = -y[1];
            return -n;
        } else {
            return n;
        }
    }
    /* 
     * all other (large) arguments
     */
    if(ix>=0x7ff00000) {		/* x is inf or NaN */
        y[0]=y[1]=x-x;
        return 0;
    }

    return rem_pio2_large(x, y);
}
#endif /* !USE_SOFTFLOAT */

/* @(#)s_sin.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

/* sin(x)
 * Return sine function of x.
 *
 * kernel function:
 *	__kernel_sin		... sine function on [-pi/4,pi/4]
 *	__kernel_cos		... cose function on [-pi/4,pi/4]
 *	__ieee754_rem_pio2	... argument reduction routine
 *
 * Method.
 *      Let S,C and T denote the sin, cos and tan respectively on 
 *	[-PI/4, +PI/4]. Reduce the argument x to y1+y2 = x-k*pi/2 
 *	in [-pi/4 , +pi/4], and let n = k mod 4.
 *	We have
 *
 *          n        sin(x)      cos(x)        tan(x)
 *     ----------------------------------------------------------
 *	    0	       S	   C		 T
 *	    1	       C	  -S		-1/T
 *	    2	      -S	  -C		 T
 *	    3	      -C	   S		-1/T
 *     ----------------------------------------------------------
 *
 * Special cases:
 *      Let trig be any of sin, cos, or tan.
 *      trig(+-INF)  is NaN, with signals;
 *      trig(NaN)    is that NaN;
 *
 * Accuracy:
 *	TRIG(x) returns trig(x) nearly rounded 
 */

/* flag = 0: sin()
   flag = 1: cos()
   flag = 3: tan()
*/
static double js_sin_cos(double x, int flag)
{
    double y[2], z, s, c;
    int ix;
    uint32_t n;
    
    /* High word of x. */
    ix = get_high_word(x);

    /* sin(Inf or NaN) is NaN */
    if (ix>=0x7ff00000)
        return x-x;
    
    n = js_rem_pio2(x,y);
    s = c = 0; /* avoid warning */
    if (flag == 3 || (n & 1) == flag) {
        s = __kernel_sin(y[0],y[1],1);
        if (flag != 3)
            goto done;
    }
    if (flag == 3 || (n & 1) != flag) {
        c = __kernel_cos(y[0],y[1]);
        if (flag != 3) {
            s = c;
            goto done;
        }
    }
    if (n & 1)
        z = -c / s;
    else
        z = s / c;
    return z;
done:
    if ((n + flag) & 2)
        s = -s;
    return s;
}

double js_sin(double x)
{
    return js_sin_cos(x, 0);
}

double js_cos(double x)
{
    return js_sin_cos(x, 1);
}

#ifdef USE_TAN_SHORTCUT
double js_tan(double x)
{
    return js_sin_cos(x, 3);
}
#endif

#ifndef USE_TAN_SHORTCUT
/*
 * ====================================================
 * Copyright 2004 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

/* INDENT OFF */
/* __kernel_tan( x, y, k )
 * kernel tan function on [-pi/4, pi/4], pi/4 ~ 0.7854
 * Input x is assumed to be bounded by ~pi/4 in magnitude.
 * Input y is the tail of x.
 * Input k indicates whether tan (if k = 1) or -1/tan (if k = -1) is returned.
 *
 * Algorithm
 *	1. Since tan(-x) = -tan(x), we need only to consider positive x.
 *	2. if x < 2^-28 (hx<0x3e300000 0), return x with inexact if x!=0.
 *	3. tan(x) is approximated by a odd polynomial of degree 27 on
 *	   [0,0.67434]
 *		  	         3             27
 *	   	tan(x) ~ x + T1*x + ... + T13*x
 *	   where
 *
 * 	        |tan(x)         2     4            26   |     -59.2
 * 	        |----- - (1+T1*x +T2*x +.... +T13*x    )| <= 2
 * 	        |  x 					|
 *
 *	   Note: tan(x+y) = tan(x) + tan'(x)*y
 *		          ~ tan(x) + (1+x*x)*y
 *	   Therefore, for better accuracy in computing tan(x+y), let
 *		     3      2      2       2       2
 *		r = x *(T2+x *(T3+x *(...+x *(T12+x *T13))))
 *	   then
 *		 		    3    2
 *		tan(x+y) = x + (T1*x + (x *(r+y)+y))
 *
 *      4. For x in [0.67434,pi/4],  let y = pi/4 - x, then
 *		tan(x) = tan(pi/4-y) = (1-tan(y))/(1+tan(y))
 *		       = 1 - 2*(tan(y) - (tan(y)^2)/(1+tan(y)))
 */

static const double T0 = 3.33333333333334091986e-01;	/* 3FD55555, 55555563 */
static const double T_even[] = {
    5.39682539762260521377e-02,	/* 3FABA1BA, 1BB341FE */
    8.86323982359930005737e-03,	/* 3F8226E3, E96E8493 */
    1.45620945432529025516e-03,	/* 3F57DBC8, FEE08315 */
    2.46463134818469906812e-04,	/* 3F3026F7, 1A8D1068 */
    7.14072491382608190305e-05,	/* 3F12B80F, 32F0A7E9 */
    2.59073051863633712884e-05,	/* 3EFB2A70, 74BF7AD4 */
};
static const double T_odd[] = {
    1.33333333333201242699e-01,	/* 3FC11111, 1110FE7A */
    2.18694882948595424599e-02,	/* 3F9664F4, 8406D637 */
    3.59207910759131235356e-03,	/* 3F6D6D22, C9560328 */
    5.88041240820264096874e-04,	/* 3F4344D8, F2F26501 */
    7.81794442939557092300e-05,	/* 3F147E88, A03792A6 */
    -1.85586374855275456654e-05,	/* BEF375CB, DB605373 */
};
static const double pio4 = 7.85398163397448278999e-01,	/* 3FE921FB, 54442D18 */
    pio4lo =  3.06161699786838301793e-17;	/* 3C81A626, 33145C07 */

/* compute -1 / (x+y) carefully */
static double minus_inv(double x, double y)
{
    double a, t, z, v, s, w;
    
    w = x + y;
    z = zero_low(w);
    v = y - (z - x);
    a = -one / w;
    t = zero_low(a);
    s = one + t * z;
    return t + a * (s + t * v);
}

static double
__kernel_tan(double x, double y, int iy) {
	double z, r, v, w, s;
	int ix, hx;

	hx = get_high_word(x);		/* high word of x */
	ix = hx & 0x7fffffff;			/* high word of |x| */
	if (ix < 0x3e300000) {			/* x < 2**-28 */
		if ((int) x == 0) {		/* generate inexact */
			if (((ix | get_low_word(x)) | (iy + 1)) == 0)
				return one / fabs(x);
			else {
				if (iy == 1)
                                    return x;
				else
                                    return minus_inv(x, y);
			}
		}
	}
	if (ix >= 0x3FE59428) {	/* |x| >= 0.6744 */
		if (hx < 0) {
			x = -x;
			y = -y;
		}
		z = pio4 - x;
		w = pio4lo - y;
		x = z + w;
		y = 0.0;
	}
	z = x * x;
	w = z * z;
	/*
	 * Break x^5*(T[1]+x^2*T[2]+...) into
	 * x^5(T[1]+x^4*T[3]+...+x^20*T[11]) +
	 * x^5(x^2*(T[2]+x^4*T[4]+...+x^22*[T12]))
	 */
	r = eval_poly(w, T_odd, 6);
	v = z * eval_poly(w, T_even, 6);
	s = z * x;
	r = y + z * (s * (r + v) + y);
	r += T0 * s;
	w = x + r;
	if (ix >= 0x3FE59428) {
		v = (double) iy;
		return (double) (1 - ((hx >> 30) & 2)) *
			(v - 2.0 * (x - (w * w / (w + v) - r)));
	}
	if (iy == 1) {
		return w;
        } else {
		/*
		 * if allow error up to 2 ulp, simply return
		 * -1.0 / (x+r) here
		 */
            return minus_inv(x, r);
	}
}

/* @(#)s_tan.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

/* tan(x)
 * Return tangent function of x.
 *
 * kernel function:
 *	__kernel_tan		... tangent function on [-pi/4,pi/4]
 *	__ieee754_rem_pio2	... argument reduction routine
 *
 * Method.
 *      Let S,C and T denote the sin, cos and tan respectively on 
 *	[-PI/4, +PI/4]. Reduce the argument x to y1+y2 = x-k*pi/2 
 *	in [-pi/4 , +pi/4], and let n = k mod 4.
 *	We have
 *
 *          n        sin(x)      cos(x)        tan(x)
 *     ----------------------------------------------------------
 *	    0	       S	   C		 T
 *	    1	       C	  -S		-1/T
 *	    2	      -S	  -C		 T
 *	    3	      -C	   S		-1/T
 *     ----------------------------------------------------------
 *
 * Special cases:
 *      Let trig be any of sin, cos, or tan.
 *      trig(+-INF)  is NaN, with signals;
 *      trig(NaN)    is that NaN;
 *
 * Accuracy:
 *	TRIG(x) returns trig(x) nearly rounded 
 */

double js_tan(double x) 
{
	double y[2],z=0.0;
	int n, ix;

    /* High word of x. */
	ix = get_high_word(x);

    /* |x| ~< pi/4 */
	ix &= 0x7fffffff;
	if(ix <= 0x3fe921fb) return __kernel_tan(x,z,1);

    /* tan(Inf or NaN) is NaN */
	else if (ix>=0x7ff00000) return x-x;		/* NaN */

    /* argument reduction needed */
	else {
	    n = js_rem_pio2(x,y);
	    return __kernel_tan(y[0],y[1],1-((n&1)<<1)); /*   1 -- n even
							-1 -- n odd */
	}
}
#endif

/* @(#)e_asin.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

/* __ieee754_asin(x)
 * Method :                  
 *	Since  asin(x) = x + x^3/6 + x^5*3/40 + x^7*15/336 + ...
 *	we approximate asin(x) on [0,0.5] by
 *		asin(x) = x + x*x^2*R(x^2)
 *	where
 *		R(x^2) is a rational approximation of (asin(x)-x)/x^3 
 *	and its remez error is bounded by
 *		|(asin(x)-x)/x^3 - R(x^2)| < 2^(-58.75)
 *
 *	For x in [0.5,1]
 *		asin(x) = pi/2-2*asin(sqrt((1-x)/2))
 *	Let y = (1-x), z = y/2, s := sqrt(z), and pio2_hi+pio2_lo=pi/2;
 *	then for x>0.98
 *		asin(x) = pi/2 - 2*(s+s*z*R(z))
 *			= pio2_hi - (2*(s+s*z*R(z)) - pio2_lo)
 *	For x<=0.98, let pio4_hi = pio2_hi/2, then
 *		f = hi part of s;
 *		c = sqrt(z) - f = (z-f*f)/(s+f) 	...f+c=sqrt(z)
 *	and
 *		asin(x) = pi/2 - 2*(s+s*z*R(z))
 *			= pio4_hi+(pio4-2s)-(2s*z*R(z)-pio2_lo)
 *			= pio4_hi+(pio4-2f)-(2s*z*R(z)-(pio2_lo+2c))
 *
 * Special cases:
 *	if x is NaN, return x itself;
 *	if |x|>1, return NaN with invalid signal.
 *
 */


static const double 
pio2_hi =  1.57079632679489655800e+00, /* 0x3FF921FB, 0x54442D18 */
    pio2_lo =  6.12323399573676603587e-17, /* 0x3C91A626, 0x33145C07 */
    pio4_hi =  7.85398163397448278999e-01; /* 0x3FE921FB, 0x54442D18 */
/* coefficient for R(x^2) */
static const double pS[] = {    
    /* pS0 */  1.66666666666666657415e-01, /* 0x3FC55555, 0x55555555 */
    /* pS1 */-3.25565818622400915405e-01, /* 0xBFD4D612, 0x03EB6F7D */
    /* pS2 */ 2.01212532134862925881e-01, /* 0x3FC9C155, 0x0E884455 */
    /* pS3 */ -4.00555345006794114027e-02, /* 0xBFA48228, 0xB5688F3B */
    /* pS4 */  7.91534994289814532176e-04, /* 0x3F49EFE0, 0x7501B288 */
    /* pS5 */  3.47933107596021167570e-05, /* 0x3F023DE1, 0x0DFDF709 */
};

static const double qS[] = {    
    /* qS1 */ -2.40339491173441421878e+00, /* 0xC0033A27, 0x1C8A2D4B */
    /* qS2 */  2.02094576023350569471e+00, /* 0x40002AE5, 0x9C598AC8 */
    /* qS3 */ -6.88283971605453293030e-01, /* 0xBFE6066C, 0x1B8D0159 */
    /* qS4 */  7.70381505559019352791e-02, /* 0x3FB3B8C5, 0xB12E9282 */
};

static double R(double t)
{
    double p, q, w;
    p = t * eval_poly(t, pS, 6);
    q = one + t * eval_poly(t, qS, 4);
    w = p/q;
    return w;
}

double js_asin(double x)
{
	double t,w,p,q,c,r,s;
	int hx,ix;
	hx = get_high_word(x);
	ix = hx&0x7fffffff;
	if(ix>= 0x3ff00000) {		/* |x|>= 1 */
	    if(((ix-0x3ff00000)|get_low_word(x))==0)
		    /* asin(1)=+-pi/2 with inexact */
		return x*pio2_hi+x*pio2_lo;	
	    return (x-x)/(x-x);		/* asin(|x|>1) is NaN */   
	} else if (ix<0x3fe00000) {	/* |x|<0.5 */
	    if(ix<0x3e400000) {		/* if |x| < 2**-27 */
		if(huge+x>one) return x;/* return x with inexact if x!=0*/
	    } else {
		t = x*x;
                w = R(t);
		return x+x*w;
            }
	}
	/* 1> |x|>= 0.5 */
	w = one-fabs(x);
	t = w*0.5;
	r = R(t);
	s = js_sqrt(t);
	if(ix>=0x3FEF3333) { 	/* if |x| > 0.975 */
	    w = r;
	    t = pio2_hi-(2.0*(s+s*w)-pio2_lo);
	} else {
	    w = zero_low(s);
	    c  = (t-w*w)/(s+w);
	    p  = 2.0*s*r-(pio2_lo-2.0*c);
	    q  = pio4_hi-2.0*w;
	    t  = pio4_hi-(p-q);
	}    
	if(hx>0) return t; else return -t;    
}


/* @(#)e_acos.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

/* __ieee754_acos(x)
 * Method :                  
 *	acos(x)  = pi/2 - asin(x)
 *	acos(-x) = pi/2 + asin(x)
 * For |x|<=0.5
 *	acos(x) = pi/2 - (x + x*x^2*R(x^2))	(see asin.c)
 * For x>0.5
 * 	acos(x) = pi/2 - (pi/2 - 2asin(sqrt((1-x)/2)))
 *		= 2asin(sqrt((1-x)/2))  
 *		= 2s + 2s*z*R(z) 	...z=(1-x)/2, s=sqrt(z)
 *		= 2f + (2c + 2s*z*R(z))
 *     where f=hi part of s, and c = (z-f*f)/(s+f) is the correction term
 *     for f so that f+c ~ sqrt(z).
 * For x<-0.5
 *	acos(x) = pi - 2asin(sqrt((1-|x|)/2))
 *		= pi - 0.5*(s+s*z*R(z)), where z=(1-|x|)/2,s=sqrt(z)
 *
 * Special cases:
 *	if x is NaN, return x itself;
 *	if |x|>1, return NaN with invalid signal.
 *
 * Function needed: sqrt
 */

static const double 
pi =  3.14159265358979311600e+00; /* 0x400921FB, 0x54442D18 */

double js_acos(double x)
{
	double z,r,w,s,c,df;
	int hx,ix;
	hx = get_high_word(x);
	ix = hx&0x7fffffff;
	if(ix>=0x3ff00000) {	/* |x| >= 1 */
	    if(((ix-0x3ff00000)|get_low_word(x))==0) {	/* |x|==1 */
		if(hx>0) return 0.0;		/* acos(1) = 0  */
		else return pi+2.0*pio2_lo;	/* acos(-1)= pi */
	    }
	    return (x-x)/(x-x);		/* acos(|x|>1) is NaN */
	}
	if(ix<0x3fe00000) {	/* |x| < 0.5 */
	    if(ix<=0x3c600000) return pio2_hi+pio2_lo;/*if|x|<2**-57*/
	    z = x*x;
            r = R(z);
	    return pio2_hi - (x - (pio2_lo-x*r));
	} else  {
	    z = (one-fabs(x))*0.5;
            r = R(z);
	    s = js_sqrt(z);
            if (hx<0) {		/* x < -0.5 */
                w = r*s-pio2_lo;
                return pi - 2.0*(s+w);
            } else {			/* x > 0.5 */
                df = zero_low(s);
                c  = (z-df*df)/(s+df);
                w = r*s+c;
                return 2.0*(df+w);
            }
        }
}


/* @(#)s_atan.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 *
 */

/* atan(x)
 * Method
 *   1. Reduce x to positive by atan(x) = -atan(-x).
 *   2. According to the integer k=4t+0.25 chopped, t=x, the argument
 *      is further reduced to one of the following intervals and the
 *      arctangent of t is evaluated by the corresponding formula:
 *
 *      [0,7/16]      atan(x) = t-t^3*(a1+t^2*(a2+...(a10+t^2*a11)...)
 *      [7/16,11/16]  atan(x) = atan(1/2) + atan( (t-0.5)/(1+t/2) )
 *      [11/16.19/16] atan(x) = atan( 1 ) + atan( (t-1)/(1+t) )
 *      [19/16,39/16] atan(x) = atan(3/2) + atan( (t-1.5)/(1+1.5t) )
 *      [39/16,INF]   atan(x) = atan(INF) + atan( -1/t )
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following 
 * constants. The decimal values may be used, provided that the 
 * compiler will convert from decimal to binary accurately enough 
 * to produce the hexadecimal values shown.
 */

static const double atanhi[] = {
  4.63647609000806093515e-01, /* atan(0.5)hi 0x3FDDAC67, 0x0561BB4F */
  7.85398163397448278999e-01, /* atan(1.0)hi 0x3FE921FB, 0x54442D18 */
  9.82793723247329054082e-01, /* atan(1.5)hi 0x3FEF730B, 0xD281F69B */
  1.57079632679489655800e+00, /* atan(inf)hi 0x3FF921FB, 0x54442D18 */
};

static const double atanlo[] = {
  2.26987774529616870924e-17, /* atan(0.5)lo 0x3C7A2B7F, 0x222F65E2 */
  3.06161699786838301793e-17, /* atan(1.0)lo 0x3C81A626, 0x33145C07 */
  1.39033110312309984516e-17, /* atan(1.5)lo 0x3C700788, 0x7AF0CBBD */
  6.12323399573676603587e-17, /* atan(inf)lo 0x3C91A626, 0x33145C07 */
};

static const double aT_even[] = {
  3.33333333333329318027e-01, /* 0x3FD55555, 0x5555550D */
  1.42857142725034663711e-01, /* 0x3FC24924, 0x920083FF */
  9.09088713343650656196e-02, /* 0x3FB745CD, 0xC54C206E */
  6.66107313738753120669e-02, /* 0x3FB10D66, 0xA0D03D51 */
  4.97687799461593236017e-02, /* 0x3FA97B4B, 0x24760DEB */
  1.62858201153657823623e-02, /* 0x3F90AD3A, 0xE322DA11 */
};
static const double aT_odd[] = {
  -1.99999999998764832476e-01, /* 0xBFC99999, 0x9998EBC4 */
  -1.11111104054623557880e-01, /* 0xBFBC71C6, 0xFE231671 */
  -7.69187620504482999495e-02, /* 0xBFB3B0F2, 0xAF749A6D */
  -5.83357013379057348645e-02, /* 0xBFADDE2D, 0x52DEFD9A */
  -3.65315727442169155270e-02, /* 0xBFA2B444, 0x2C6A6C2F */
};

double js_atan(double x)
{
	double w,s1,s2,z;
	int ix,hx,id;

	hx = get_high_word(x);
	ix = hx&0x7fffffff;
	if(ix>=0x44100000) {	/* if |x| >= 2^66 */
	    if(ix>0x7ff00000||
		(ix==0x7ff00000&&(get_low_word(x)!=0)))
		return x+x;		/* NaN */
	    if(hx>0) return  atanhi[3]+atanlo[3];
	    else     return -atanhi[3]-atanlo[3];
	} if (ix < 0x3fdc0000) {	/* |x| < 0.4375 */
	    if (ix < 0x3e200000) {	/* |x| < 2^-29 */
		if(huge+x>one) return x;	/* raise inexact */
	    }
	    id = -1;
	} else {
	x = fabs(x);
	if (ix < 0x3ff30000) {		/* |x| < 1.1875 */
	    if (ix < 0x3fe60000) {	/* 7/16 <=|x|<11/16 */
		id = 0; x = (2.0*x-one)/(2.0+x); 
	    } else {			/* 11/16<=|x|< 19/16 */
		id = 1; x  = (x-one)/(x+one); 
	    }
	} else {
	    if (ix < 0x40038000) {	/* |x| < 2.4375 */
		id = 2; x  = (x-1.5)/(one+1.5*x);
	    } else {			/* 2.4375 <= |x| < 2^66 */
		id = 3; x  = -1.0/x;
	    }
	}}
    /* end of argument reduction */
	z = x*x;
	w = z*z;
    /* break sum from i=0 to 10 aT[i]z**(i+1) into odd and even poly */
	s1 = z*eval_poly(w, aT_even, 6);
	s2 = w*eval_poly(w, aT_odd, 5);
	if (id<0) return x - x*(s1+s2);
	else {
	    z = atanhi[id] - ((x*(s1+s2) - atanlo[id]) - x);
	    return (hx<0)? -z:z;
	}
}

/* @(#)e_atan2.c 1.3 95/01/18 */
/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunSoft, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 *
 */

/* __ieee754_atan2(y,x)
 * Method :
 *	1. Reduce y to positive by atan2(y,x)=-atan2(-y,x).
 *	2. Reduce x to positive by (if x and y are unexceptional): 
 *		ARG (x+iy) = arctan(y/x)   	   ... if x > 0,
 *		ARG (x+iy) = pi - arctan[y/(-x)]   ... if x < 0,
 *
 * Special cases:
 *
 *	ATAN2((anything), NaN ) is NaN;
 *	ATAN2(NAN , (anything) ) is NaN;
 *	ATAN2(+-0, +(anything but NaN)) is +-0  ;
 *	ATAN2(+-0, -(anything but NaN)) is +-pi ;
 *	ATAN2(+-(anything but 0 and NaN), 0) is +-pi/2;
 *	ATAN2(+-(anything but INF and NaN), +INF) is +-0 ;
 *	ATAN2(+-(anything but INF and NaN), -INF) is +-pi;
 *	ATAN2(+-INF,+INF ) is +-pi/4 ;
 *	ATAN2(+-INF,-INF ) is +-3pi/4;
 *	ATAN2(+-INF, (anything but,0,NaN, and INF)) is +-pi/2;
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following 
 * constants. The decimal values may be used, provided that the 
 * compiler will convert from decimal to binary accurately enough 
 * to produce the hexadecimal values shown.
 */

static const double 
pi_o_4  = 7.8539816339744827900E-01, /* 0x3FE921FB, 0x54442D18 */
pi_o_2  = 1.5707963267948965580E+00, /* 0x3FF921FB, 0x54442D18 */
pi_lo   = 1.2246467991473531772E-16; /* 0x3CA1A626, 0x33145C07 */

double js_atan2(double y, double x)
{  
	double z;
	int k,m,hx,hy,ix,iy;
	unsigned lx,ly;

        EXTRACT_WORDS(hx, lx, x);
        EXTRACT_WORDS(hy, ly, y);
	ix = hx&0x7fffffff;
	iy = hy&0x7fffffff;
	if(((ix|((lx|-lx)>>31))>0x7ff00000)||
	   ((iy|((ly|-ly)>>31))>0x7ff00000))	/* x or y is NaN */
	   return x+y;
	if(((hx-0x3ff00000)|lx)==0)
            return js_atan(y);   /* x=1.0 */
	m = ((hy>>31)&1)|((hx>>30)&2);	/* 2*sign(x)+sign(y) */

    /* when y = 0 */
	if((iy|ly)==0) {
            z = 0;
            goto done;
	}
    /* when x = 0 */
	if((ix|lx)==0) return (hy<0)?  -pi_o_2-tiny: pi_o_2+tiny;
	    
    /* when x is INF */
	if(ix==0x7ff00000) {
	    if(iy==0x7ff00000) {
                z = pi_o_4;
	    } else {
                z = 0;
	    }
            goto done;
	}
    /* when y is INF */
	if(iy==0x7ff00000) return (hy<0)? -pi_o_2-tiny: pi_o_2+tiny;

    /* compute y/x */
	k = (iy-ix)>>20;
	if(k > 60) {
            z=pi_o_2+0.5*pi_lo; 	/* |y/x| >  2**60 */
        } else if(hx<0&&k<-60) {
            z=0.0; 	/* |y|/x < -2**60 */
        } else {
            z=js_atan(fabs(y/x));		/* safe to do y/x */
        }
 done:
	switch (m) {
	    case 0:
                return       z  ;	/* atan(+,+) */
	    case 1:
                z = set_high_word(z, get_high_word(z) ^ 0x80000000);
                return       z  ;	/* atan(-,+) */
	    case 2:
                return  pi-(z-pi_lo);/* atan(+,-) */
	    default: /* case 3 */
                return  (z-pi_lo)-pi;/* atan(-,-) */
	}
}

/* @(#)e_exp.c 1.6 04/04/22 */
/*
 * ====================================================
 * Copyright (C) 2004 by Sun Microsystems, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

/* __ieee754_exp(x)
 * Returns the exponential of x.
 *
 * Method
 *   1. Argument reduction:
 *      Reduce x to an r so that |r| <= 0.5*ln2 ~ 0.34658.
 *	Given x, find r and integer k such that
 *
 *               x = k*ln2 + r,  |r| <= 0.5*ln2.  
 *
 *      Here r will be represented as r = hi-lo for better 
 *	accuracy.
 *
 *   2. Approximation of exp(r) by a special rational function on
 *	the interval [0,0.34658]:
 *	Write
 *	    R(r**2) = r*(exp(r)+1)/(exp(r)-1) = 2 + r*r/6 - r**4/360 + ...
 *      We use a special Remes algorithm on [0,0.34658] to generate 
 * 	a polynomial of degree 5 to approximate R. The maximum error 
 *	of this polynomial approximation is bounded by 2**-59. In
 *	other words,
 *	    R(z) ~ 2.0 + P1*z + P2*z**2 + P3*z**3 + P4*z**4 + P5*z**5
 *  	(where z=r*r, and the values of P1 to P5 are listed below)
 *	and
 *	    |                  5          |     -59
 *	    | 2.0+P1*z+...+P5*z   -  R(z) | <= 2 
 *	    |                             |
 *	The computation of exp(r) thus becomes
 *                             2*r
 *		exp(r) = 1 + -------
 *		              R - r
 *                                 r*R1(r)	
 *		       = 1 + r + ----------- (for better accuracy)
 *		                  2 - R1(r)
 *	where
 *			         2       4             10
 *		R1(r) = r - (P1*r  + P2*r  + ... + P5*r   ).
 *	
 *   3. Scale back to obtain exp(x):
 *	From step 1, we have
 *	   exp(x) = 2^k * exp(r)
 *
 * Special cases:
 *	exp(INF) is INF, exp(NaN) is NaN;
 *	exp(-INF) is 0, and
 *	for finite argument, only exp(0)=1 is exact.
 *
 * Accuracy:
 *	according to an error analysis, the error is always less than
 *	1 ulp (unit in the last place).
 *
 * Misc. info.
 *	For IEEE double 
 *	    if x >  7.09782712893383973096e+02 then exp(x) overflow
 *	    if x < -7.45133219101941108420e+02 then exp(x) underflow
 *
 * Constants:
 * The hexadecimal values are the intended ones for the following 
 * constants. The decimal values may be used, provided that the 
 * compiler will convert from decimal to binary accurately enough
 * to produce the hexadecimal values shown.
 */

static const double
two	=  2.0,
halF[2]	= {0.5,-0.5,},
twom1000= 9.33263618503218878990e-302,     /* 2**-1000=0x01700000,0*/
o_threshold=  7.09782712893383973096e+02,  /* 0x40862E42, 0xFEFA39EF */
u_threshold= -7.45133219101941108420e+02,  /* 0xc0874910, 0xD52D3051 */
ln2HI[2]   ={ 6.93147180369123816490e-01,  /* 0x3fe62e42, 0xfee00000 */
	     -6.93147180369123816490e-01,},/* 0xbfe62e42, 0xfee00000 */
ln2LO[2]   ={ 1.90821492927058770002e-10,  /* 0x3dea39ef, 0x35793c76 */
	     -1.90821492927058770002e-10,},/* 0xbdea39ef, 0x35793c76 */
    invln2 =  1.44269504088896338700e+00; /* 0x3ff71547, 0x652b82fe */
static const double P[] = {
    /* P1 */  1.66666666666666019037e-01, /* 0x3FC55555, 0x5555553E */
    /* P2 */ -2.77777777770155933842e-03, /* 0xBF66C16C, 0x16BEBD93 */
    /* P3 */  6.61375632143793436117e-05, /* 0x3F11566A, 0xAF25DE2C */
    /* P4 */ -1.65339022054652515390e-06, /* 0xBEBBBD41, 0xC5D26BF1 */
    /* P5 */  4.13813679705723846039e-08, /* 0x3E663769, 0x72BEA4D0 */
};

/* compute exp(z+w)*2^n */
static double kernel_exp(double z, double w, double lo, double hi, int n)
{
    int j;
    double t, t1, r;
    t  = z*z;
    t1  = z - t*eval_poly(t, P, 5);
    r  = (z*t1)/(t1-two) - (w+z*w);
    z  = one-((lo + r)-hi);
    j  = get_high_word(z);
    j += (n<<20);
    if((j>>20)<=0)
        z = js_scalbn(z,n);	/* subnormal output */
    else
        z = set_high_word(z, get_high_word(z) + (n<<20));
    return z;
}

double js_exp(double x)
{
	double hi,lo,t;
	int k,xsb;
	unsigned hx;

	hx  = get_high_word(x);	/* high word of x */
	xsb = (hx>>31)&1;		/* sign bit of x */
	hx &= 0x7fffffff;		/* high word of |x| */

    /* filter out non-finite argument */
	if(hx >= 0x40862E42) {			/* if |x|>=709.78... */
            if(hx>=0x7ff00000) {
		if(((hx&0xfffff)|get_low_word(x))!=0) 
		     return x+x; 		/* NaN */
		else return (xsb==0)? x:0.0;	/* exp(+-inf)={inf,0} */
	    }
	    if(x > o_threshold) return huge*huge; /* overflow */
	    if(x < u_threshold) return twom1000*twom1000; /* underflow */
	}

    /* argument reduction */
	if(hx > 0x3fd62e42) {		/* if  |x| > 0.5 ln2 */ 
	    if(hx < 0x3FF0A2B2) {	/* and |x| < 1.5 ln2 */
		hi = x-ln2HI[xsb]; lo=ln2LO[xsb]; k = 1-xsb-xsb;
	    } else {
		k  = (int)(invln2*x+halF[xsb]);
		t  = k;
		hi = x - t*ln2HI[0];	/* t*ln2HI is exact here */
		lo = t*ln2LO[0];
	    }
	    x  = hi - lo;
	} 
	else if(hx < 0x3e300000)  {	/* when |x|<2**-28 */
	    if(huge+x>one) return one+x;/* trigger inexact */
            k = 0; /* avoid warning */
	}
	else k = 0;

        /* x is now in primary range */
        if (k == 0) {
            lo = 0;
            hi = x;
        }
        return kernel_exp(x, 0, lo, hi, k);
}


/* @(#)e_pow.c 1.5 04/04/22 SMI */
/*
 * ====================================================
 * Copyright (C) 2004 by Sun Microsystems, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice 
 * is preserved.
 * ====================================================
 */

/* __ieee754_pow(x,y) return x**y
 *
 *		      n
 * Method:  Let x =  2   * (1+f)
 *	1. Compute and return log2(x) in two pieces:
 *		log2(x) = w1 + w2,
 *	   where w1 has 53-24 = 29 bit trailing zeros.
 *	2. Perform y*log2(x) = n+y' by simulating multi-precision 
 *	   arithmetic, where |y'|<=0.5.
 *	3. Return x**y = 2**n*exp(y'*log2)
 *
 * Special cases:
 *	1.  (anything) ** 0  is 1
 *	2.  (anything) ** 1  is itself
 *	3.  (anything) ** NAN is NAN
 *	4.  NAN ** (anything except 0) is NAN
 *	5.  +-(|x| > 1) **  +INF is +INF
 *	6.  +-(|x| > 1) **  -INF is +0
 *	7.  +-(|x| < 1) **  +INF is +0
 *	8.  +-(|x| < 1) **  -INF is +INF
 *	9.  +-1         ** +-INF is NAN
 *	10. +0 ** (+anything except 0, NAN)               is +0
 *	11. -0 ** (+anything except 0, NAN, odd integer)  is +0
 *	12. +0 ** (-anything except 0, NAN)               is +INF
 *	13. -0 ** (-anything except 0, NAN, odd integer)  is +INF
 *	14. -0 ** (odd integer) = -( +0 ** (odd integer) )
 *	15. +INF ** (+anything except 0,NAN) is +INF
 *	16. +INF ** (-anything except 0,NAN) is +0
 *	17. -INF ** (anything)  = -0 ** (-anything)
 *	18. (-anything) ** (integer) is (-1)**(integer)*(+anything**integer)
 *	19. (-anything except 0 and inf) ** (non-integer) is NAN
 *
 * Accuracy:
 *	pow(x,y) returns x**y nearly rounded. In particular
 *			pow(integer,integer)
 *	always returns the correct integer provided it is 
 *	representable.
 *
 * Constants :
 * The hexadecimal values are the intended ones for the following 
 * constants. The decimal values may be used, provided that the 
 * compiler will convert from decimal to binary accurately enough 
 * to produce the hexadecimal values shown.
 */

static const double 
bp[] = {1.0, 1.5,},
dp_h[] = { 0.0, 5.84962487220764160156e-01,}, /* 0x3FE2B803, 0x40000000 */
dp_l[] = { 0.0, 1.35003920212974897128e-08,}, /* 0x3E4CFDEB, 0x43CFD006 */
two53	=  9007199254740992.0,	/* 0x43400000, 0x00000000 */
	/* poly coefs for (3/2)*(log(x)-2s-2/3*s**3 */
lg2  =  6.93147180559945286227e-01, /* 0x3FE62E42, 0xFEFA39EF */
lg2_h  =  6.93147182464599609375e-01, /* 0x3FE62E43, 0x00000000 */
lg2_l  = -1.90465429995776804525e-09, /* 0xBE205C61, 0x0CA86C39 */
ovt =  8.0085662595372944372e-0017, /* -(1024-log2(ovfl+.5ulp)) */
cp    =  9.61796693925975554329e-01, /* 0x3FEEC709, 0xDC3A03FD =2/(3ln2) */
cp_h  =  9.61796700954437255859e-01, /* 0x3FEEC709, 0xE0000000 =(float)cp */
cp_l  = -7.02846165095275826516e-09, /* 0xBE3E2FE0, 0x145B01F5 =tail of cp_h*/
ivln2    =  1.44269504088896338700e+00, /* 0x3FF71547, 0x652B82FE =1/ln2 */
ivln2_h  =  1.44269502162933349609e+00, /* 0x3FF71547, 0x60000000 =24b 1/ln2*/
ivln2_l  =  1.92596299112661746887e-08, /* 0x3E54AE0B, 0xF85DDF44 =1/ln2 tail*/
ivlg10b2 = 0.3010299956639812, /*    0x3fd34413509f79ff 1/log2(10) */
ivlg10b2_h = 0.30102992057800293, /* 0x3fd3441300000000 1/log2(10) high */
ivlg10b2_l = 7.508597826552624e-8; /* 0x3e7427de7fbcc47c 1/log2(10) low */

static const double L_tab[] = {
    /* L1 */ 5.99999999999994648725e-01, /* 0x3FE33333, 0x33333303 */
    /* L2 */ 4.28571428578550184252e-01, /* 0x3FDB6DB6, 0xDB6FABFF */
    /* L3 */ 3.33333329818377432918e-01, /* 0x3FD55555, 0x518F264D */
    /* L4 */ 2.72728123808534006489e-01, /* 0x3FD17460, 0xA91D4101 */
    /* L5 */ 2.30660745775561754067e-01, /* 0x3FCD864A, 0x93C9DB65 */
    /* L6 */ 2.06975017800338417784e-01, /* 0x3FCA7E28, 0x4A454EEF */
};

/* compute (t1, t2) = log2(ax). is_small_ax is true if abs(ax)<= 2**-20 */
static void kernel_log2(double *pt1, double *pt2, double ax)
{
    double t, u, v, t1, t2, r;
    int n, j, ix, k;
    double ss, s2, s_h, s_l, t_h, t_l, p_l, p_h, z_h, z_l;

    n = 0;
    ix = get_high_word(ax);
    /* take care subnormal number */
    if(ix<0x00100000)
        {ax *= two53; n -= 53; ix = get_high_word(ax); }
    n  += ((ix)>>20)-0x3ff;
    j  = ix&0x000fffff;
    /* determine interval */
    ix = j|0x3ff00000;		/* normalize ix */
    if(j<=0x3988E) k=0;		/* |x|<sqrt(3/2) */
    else if(j<0xBB67A) k=1;	/* |x|<sqrt(3)   */
    else {k=0;n+=1;ix -= 0x00100000;}
    ax = set_high_word(ax, ix);
        
    /* compute ss = s_h+s_l = (x-1)/(x+1) or (x-1.5)/(x+1.5) */
    u = ax-bp[k];		/* bp[0]=1.0, bp[1]=1.5 */
    v = one/(ax+bp[k]);
    ss = u*v;
    s_h = zero_low(ss);
    /* t_h=ax+bp[k] High */
    t_h = zero;
    t_h = set_high_word(t_h, ((ix>>1)|0x20000000)+0x00080000+(k<<18));
    t_l = ax - (t_h-bp[k]);
    s_l = v*((u-s_h*t_h)-s_h*t_l);
    /* compute log(ax) */
    s2 = ss*ss;
    r = s2*s2*eval_poly(s2, L_tab, 6);
    r += s_l*(s_h+ss);
    s2  = s_h*s_h;
    t_h = zero_low(3.0+s2+r);
    t_l = r-((t_h-3.0)-s2);
    /* u+v = ss*(1+...) */
    u = s_h*t_h;
    v = s_l*t_h+t_l*ss;
    /* 2/(3log2)*(ss+...) */
    p_h = zero_low(u+v);
    p_l = v-(p_h-u);
    z_h = cp_h*p_h;		/* cp_h+cp_l = 2/(3*log2) */
    z_l = cp_l*p_h+p_l*cp+dp_l[k];
    /* log2(ax) = (ss+..)*2/(3*log2) = n + dp_h + z_h + z_l */
    t = (double)n;
    t1 = zero_low(((z_h+z_l)+dp_h[k])+t);
    t2 = z_l-(((t1-t)-dp_h[k])-z_h);

    *pt1 = t1;
    *pt2 = t2;
}

/* flag = 0: log2()
   flag = 1: log()
   flag = 2: log10()
*/
static double js_log_internal(double x, int flag)
{
    double p_h, p_l, t, u, v;
    int hx, lx;

    EXTRACT_WORDS(hx, lx, x);
    if (hx <= 0) {
        if (((hx&0x7fffffff)|lx)==0) 
            return -INFINITY;		/* log(+-0)=-inf */
        if (hx<0)
            return NAN;	/* log(-#) = NaN */
    } else if (hx >= 0x7ff00000) {
        /* log(inf) = inf, log(nan) = nan */
        return x+x;
    }
    kernel_log2(&p_h, &p_l, x);

    t = p_h + p_l;
    if (flag == 0) {
        return t;
    } else {
        t = zero_low(t);
        if (flag == 1) {
            /* multiply (p_l+p_h) by lg2 */
            u = t*lg2_h;
            v = (p_l-(t-p_h))*lg2+t*lg2_l;
        } else {
            /* mutiply (p_l+p_h) by 1/log2(10) */
            u = t*ivlg10b2_h;
            v = (p_l-(t-p_h))*ivlg10b2+t*ivlg10b2_l;
        }
        return u+v;
    }
}

double js_log2(double x)
{
    return js_log_internal(x, 0);
}

double js_log(double x)
{
    return js_log_internal(x, 1);
}

double js_log10(double x)
{
    return js_log_internal(x, 2);
}

double js_pow(double x, double y)
{
	double z,ax,p_h,p_l;
	double y1,t1,t2,s,t,u,v,w;
	int i,j,k,yisint,n;
	int hx,hy,ix,iy;
	unsigned lx,ly;

        EXTRACT_WORDS(hx, lx, x);
        EXTRACT_WORDS(hy, ly, y);
	ix = hx&0x7fffffff;  iy = hy&0x7fffffff;

    /* y==zero: x**0 = 1 */
	if((iy|ly)==0) return one; 	

    /* +-NaN return x+y */
	if(ix > 0x7ff00000 || ((ix==0x7ff00000)&&(lx!=0)) ||
	   iy > 0x7ff00000 || ((iy==0x7ff00000)&&(ly!=0))) 
		return x+y;	

    /* determine if y is an odd int when x < 0
     * yisint = 0	... y is not an integer
     * yisint = 1	... y is an odd int
     * yisint = 2	... y is an even int
     */
	yisint  = 0;
	if(hx<0) {	
	    if(iy>=0x43400000) yisint = 2; /* even integer y */
	    else if(iy>=0x3ff00000) {
		k = (iy>>20)-0x3ff;	   /* exponent */
		if(k>20) {
		    j = ly>>(52-k);
		    if((j<<(52-k))==ly) yisint = 2-(j&1);
		} else if(ly==0) {
		    j = iy>>(20-k);
		    if((j<<(20-k))==iy) yisint = 2-(j&1);
		}
	    }		
	} 

    /* special value of y */
	if(ly==0) { 	
	    if (iy==0x7ff00000) {	/* y is +-inf */
	        if(((ix-0x3ff00000)|lx)==0)
		    return  y - y;	/* inf**+-1 is NaN */
	        else if (ix >= 0x3ff00000)/* (|x|>1)**+-inf = inf,0 */
		    return (hy>=0)? y: zero;
	        else			/* (|x|<1)**-,+inf = inf,0 */
		    return (hy<0)?-y: zero;
	    } 
	    if(iy==0x3ff00000) {	/* y is  +-1 */
		if(hy<0) return one/x; else return x;
	    }
	    if(hy==0x40000000) return x*x; /* y is  2 */
	    if(hy==0x3fe00000) {	/* y is  0.5 */
		if(hx>=0)	/* x >= +0 */
		return js_sqrt(x);	
	    }
	}

	ax   = fabs(x);
    /* special value of x */
	if(lx==0) {
	    if(ix==0x7ff00000||ix==0||ix==0x3ff00000){
		z = ax;			/*x is +-0,+-inf,+-1*/
		if(hy<0) z = one/z;	/* z = (1/|x|) */
		if(hx<0) {
		    if(((ix-0x3ff00000)|yisint)==0) {
			z = (z-z)/(z-z); /* (-1)**non-int is NaN */
		    } else if(yisint==1) 
			z = -z;		/* (x<0)**odd = -(|x|**odd) */
		}
		return z;
	    }
	}
    
	n = (hx>>31)+1;

    /* (x<0)**(non-int) is NaN */
	if((n|yisint)==0) return (x-x)/(x-x);

	s = one; /* s (sign of result -ve**odd) = -1 else = 1 */
	if((n|(yisint-1))==0) s = -one;/* (-ve)**(odd int) */

    /* |y| is huge */
	if(iy>0x41e00000) { /* if |y| > 2**31 */
	    if(iy>0x43f00000){	/* if |y| > 2**64, must o/uflow */
		if(ix<=0x3fefffff) return (hy<0)? huge*huge:tiny*tiny;
		if(ix>=0x3ff00000) return (hy>0)? huge*huge:tiny*tiny;
	    }
            /* over/underflow if x is not close to one */
	    if(ix<0x3fefffff) return (hy<0)? s*huge*huge:s*tiny*tiny;
	    if(ix>0x3ff00000) return (hy>0)? s*huge*huge:s*tiny*tiny;
            t = ax-one;		/* t has 20 trailing zeros */
            w = (t*t)*(0.5-t*(0.3333333333333333333333-t*0.25));
            u = ivln2_h*t;	/* ivln2_h has 21 sig. bits */
            v = t*ivln2_l-w*ivln2;
            t1 = zero_low(u+v);
            t2 = v-(t1-u);
        } else {
            kernel_log2(&t1, &t2, ax);
        }
    /* split up y into y1+y2 and compute (y1+y2)*(t1+t2) */
	y1  = zero_low(y);
	p_l = (y-y1)*t1+y*t2;
	p_h = y1*t1;
	z = p_l+p_h;
        EXTRACT_WORDS(j, i, z);
	if (j>=0x40900000) {				/* z >= 1024 */
	    if(((j-0x40900000)|i)!=0)			/* if z > 1024 */
		return s*huge*huge;			/* overflow */
	    else {
		if(p_l+ovt>z-p_h) return s*huge*huge;	/* overflow */
	    }
	} else if((j&0x7fffffff)>=0x4090cc00 ) {	/* z <= -1075 */
	    if(((j-0xc090cc00)|i)!=0) 		/* z < -1075 */
		return s*tiny*tiny;		/* underflow */
	    else {
		if(p_l<=z-p_h) return s*tiny*tiny;	/* underflow */
	    }
	}
    /*
     * compute 2**(p_h+p_l)
     */
	i = j&0x7fffffff;
	k = (i>>20)-0x3ff;
	n = 0;
	if(i>0x3fe00000) {		/* if |z| > 0.5, set n = [z+0.5] */
	    n = j+(0x00100000>>(k+1));
	    k = ((n&0x7fffffff)>>20)-0x3ff;	/* new k for n */
	    t = zero;
	    t = set_high_word(t, n&~(0x000fffff>>k));
	    n = ((n&0x000fffff)|0x00100000)>>(20-k);
	    if(j<0) n = -n;
	    p_h -= t;
	}
        /* multiply (p_l+p_h) by lg2 */
	t = zero_low(p_l+p_h);
	u = t*lg2_h;
	v = (p_l-(t-p_h))*lg2+t*lg2_l;
	z = u+v;
	w = v-(z-u);
        return s * kernel_exp(z, w, 0, z, n);
}

