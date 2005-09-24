#ifndef _IEEE_W32_H
#define _IEEE_W32_H

#if defined(_MSVC_) && (LDBL_MANT_DIG == DBL_MANT_DIG)
 #define LONG_DOUBLE_IS_SAME_AS_DOUBLE
#endif

#undef FP_NAN
#undef FP_INFINITE
#undef FP_ZERO
#undef FP_SUBNORMAL
#undef FP_NORMAL
#undef fpclassify
#undef signbit

/* All floating-point numbers can be put in one of these categories.  */
enum
  {
    FP_NAN,
# define FP_NAN FP_NAN
    FP_INFINITE,
# define FP_INFINITE FP_INFINITE
    FP_ZERO,
# define FP_ZERO FP_ZERO
    FP_SUBNORMAL,
# define FP_SUBNORMAL FP_SUBNORMAL
    FP_NORMAL
# define FP_NORMAL FP_NORMAL
  };

/* Type representing floating-point environment.  This function corresponds
   to the layout of the block written by the `fstenv'.  */
typedef struct
  {
    unsigned short int __control_word;
    unsigned short int __unused1;
    unsigned short int __status_word;
    unsigned short int __unused2;
    unsigned short int __tags;
    unsigned short int __unused3;
    unsigned int __eip;
    unsigned short int __cs_selector;
    unsigned int __opcode:11;
    unsigned int __unused4:5;
    unsigned int __data_offset;
    unsigned short int __data_selector;
    unsigned short int __unused5;
  }
fenv_t;

/* Define bits representing the exception.  We use the bit positions
   of the appropriate bits in the FPU control word.  */
enum
  {
    FE_INVALID = 0x01,
#define FE_INVALID  FE_INVALID
    __FE_DENORM = 0x02,
    FE_DIVBYZERO = 0x04,
#define FE_DIVBYZERO    FE_DIVBYZERO
    FE_OVERFLOW = 0x08,
#define FE_OVERFLOW FE_OVERFLOW
    FE_UNDERFLOW = 0x10,
#define FE_UNDERFLOW    FE_UNDERFLOW
    FE_INEXACT = 0x20
#define FE_INEXACT  FE_INEXACT
  };

#define FE_ALL_EXCEPT \
    (FE_INEXACT | FE_DIVBYZERO | FE_UNDERFLOW | FE_OVERFLOW | FE_INVALID)

#if defined(LONG_DOUBLE_IS_SAME_AS_DOUBLE)
 #define fpclassify(x) \
     (sizeof (x) == sizeof (float)  ? __fpclassifyf (x) \
    : __fpclassify (x))
#else
 #define fpclassify(x) \
     (sizeof (x) == sizeof (float)  ? __fpclassifyf (x) \
    : sizeof (x) == sizeof (double) ? __fpclassify (x) \
    : __fpclassifyl (x))
#endif

typedef union
{
  float value;
  uint32_t word;
} ieee_float_shape_type;

#define GET_FLOAT_WORD(i,d)      \
do {                             \
  ieee_float_shape_type gf_u;    \
  gf_u.value = (d);              \
  (i) = gf_u.word;               \
} while (0)


typedef union
{
  double value;
  struct
  {
    uint32_t lsw;
    uint32_t msw;
  } parts;
} ieee_double_shape_type;

#define EXTRACT_WORDS(ix0,ix1,d)  \
do {                              \
  ieee_double_shape_type ew_u;    \
  ew_u.value = (d);               \
  (ix0) = ew_u.parts.msw;         \
  (ix1) = ew_u.parts.lsw;         \
} while (0)

#if !defined(LONG_DOUBLE_IS_SAME_AS_DOUBLE)
typedef union
{
  long double value;
  struct
  {
    uint32_t lsw;
    uint32_t msw;
    int sign_exponent:16;
    unsigned int empty:16;
  } parts;
} ieee_long_double_shape_type;

/* Get three 32 bit ints from a double.  */

#define GET_LDOUBLE_WORDS(exp,ix0,ix1,d) \
do {                                     \
  ieee_long_double_shape_type ew_u;      \
  ew_u.value = (d);                      \
  (exp) = ew_u.parts.sign_exponent;      \
  (ix0) = ew_u.parts.msw;                \
  (ix1) = ew_u.parts.lsw;                \
} while (0)
#endif /*!defined(LONG_DOUBLE_IS_SAME_AS_DOUBLE)*/

int
fegetenv(fenv_t *f)
{
    f=f;
    return(0);
}

int
feholdexcept(fenv_t *f)
{
    f=f;
    return(0);
}

int
__fpclassifyf (float x)
{
  uint32_t wx;
  int retval = FP_NORMAL;

  GET_FLOAT_WORD (wx, x);
  wx &= 0x7fffffff;
  if (wx == 0)
    retval = FP_ZERO;
  else if (wx < 0x800000)
    retval = FP_SUBNORMAL;
  else if (wx >= 0x7f800000)
    retval = wx > 0x7f800000 ? FP_NAN : FP_INFINITE;

  return retval;
}

int
__fpclassify (double x)
{
  uint32_t hx, lx;
  int retval = FP_NORMAL;

  EXTRACT_WORDS (hx, lx, x);
  lx |= hx & 0xfffff;
  hx &= 0x7ff00000;
  if ((hx | lx) == 0)
    retval = FP_ZERO;
  else if (hx == 0)
    retval = FP_SUBNORMAL;
  else if (hx == 0x7ff00000)
    retval = lx != 0 ? FP_NAN : FP_INFINITE;

  return retval;
}

#if !defined(LONG_DOUBLE_IS_SAME_AS_DOUBLE)
int
__fpclassifyl (long double x)
{
  uint32_t ex, hx, lx;
  int retval = FP_NORMAL;

  GET_LDOUBLE_WORDS (ex, hx, lx, x);
  ex &= 0x7fff;
  if ((ex | lx | hx) == 0)
    retval = FP_ZERO;
  else if (ex == 0 && (hx & 0x80000000) == 0)
    retval = FP_SUBNORMAL;
  else if (ex == 0x7fff)
    retval = ((hx & 0x7fffffff) | lx) != 0 ? FP_NAN : FP_INFINITE;

  return retval;
}
#endif // !defined(LONG_DOUBLE_IS_SAME_AS_DOUBLE)

int
fetestexcept (int excepts)
{
  int temp;

  /* Get current exceptions.  */
  #if defined(_MSVC_)
  __asm fnstsw temp
  #else
  __asm__ ("fnstsw %0" : "=a" (temp));
  #endif

  return temp & excepts & FE_ALL_EXCEPT;
}

int
feclearexcept (int excepts)
{
  fenv_t temp;

  /* Mask out unsupported bits/exceptions.  */
  excepts &= FE_ALL_EXCEPT;

  /* Bah, we have to clear selected exceptions.  Since there is no
     `fldsw' instruction we have to do it the hard way.  */
  #if defined(_MSVC_)
  __asm fnstenv temp
  #else
  __asm__ ("fnstenv %0" : "=m" (*&temp));
  #endif

  /* Clear the relevant bits.  */
  temp.__status_word &= excepts ^ FE_ALL_EXCEPT;

  /* Put the new data in effect.  */
  #if defined(_MSVC_)
  __asm fldenv temp
  #else
  __asm__ ("fldenv %0" : : "m" (*&temp));
  #endif

  /* Success.  */
  return 0;
}

#define GET_HIGH_WORD(i,d)       \
do {                             \
  ieee_double_shape_type gh_u;   \
  gh_u.value = (d);              \
  (i) = gh_u.parts.msw;          \
} while (0)

int
signbit (double x)
{
  int32_t hx;

  GET_HIGH_WORD (hx, x);
  return hx & 0x80000000;
}

#if !defined(HAVE_FREXPL)

#define GET_LDOUBLE_EXP(exp,d)       \
do {                                 \
  ieee_long_double_shape_type ge_u;  \
  ge_u.value = (d);                  \
  (exp) = ge_u.parts.sign_exponent;  \
} while (0)

#define SET_LDOUBLE_EXP(d,exp)       \
do {                                 \
  ieee_long_double_shape_type se_u;  \
  se_u.value = (d);                  \
  se_u.parts.sign_exponent = (exp);  \
  (d) = se_u.value;                  \
} while (0)

long double frexpl(long double x, int *eptr)
{
 uint32_t se, hx, ix, lx;
 uint64_t two64 = 0x43f0000000000000ULL;
 GET_LDOUBLE_WORDS(se,hx,lx,x);
 ix = 0x7fff&se;
 *eptr = 0;
 if(ix==0x7fff||((ix|hx|lx)==0)) return x;  /* 0,inf,nan */
 if (ix==0x0000) { /* subnormal */
   x *= two64;
   GET_LDOUBLE_EXP(se,x);
   ix = se&0x7fff;
   *eptr = -64;
 }
 *eptr += ix-16382;
 se = (se & 0x8000) | 0x3ffe;
 SET_LDOUBLE_EXP(x,se);
 return x;
}

#endif /*!defined(HAVE_FREXPL)*/

#endif // _IEEE_W32_H
