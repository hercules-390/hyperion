#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#elif defined(HAVE_U_INT)
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#define uint8_t u_int8_t
#define uint16_t u_int16_t
#define uint32_t u_int32_t
#define uint64_t u_int64_t
#else
#error Unable to find fixed-size data types
#endif
