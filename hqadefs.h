/* HQADEFS.H    (C) "Fish" (David B. Trout), 2013                    */
/*              QA Build Configuration Testing Scenarios             */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

#ifndef _HQADEFS_H_
#define _HQADEFS_H_

/*-------------------------------------------------------------------*/
/* QA Scenario 0:          Reserved for normal builds                */
/*-------------------------------------------------------------------*/

#if !defined(HQA_SCENARIO) || HQA_SCENARIO == 0

    /* Do nothing. This effectively does a normal build */

#else

/*-------------------------------------------------------------------*/
/* QA Scenario 1:          System/370 support only                   */
/*-------------------------------------------------------------------*/

#if HQA_SCENARIO == 1   // System/370 support only

  #undef  CUSTOM_BUILD_STRING
  #define CUSTOM_BUILD_STRING "\n\n          QA Scenario 1\n"

  #define OPTION_370_MODE
  #define NO_390_MODE
  #define NO_900_MODE

#endif

/*-------------------------------------------------------------------*/
/* QA Scenario 2:          ESA/390 support only                      */
/*-------------------------------------------------------------------*/

#if HQA_SCENARIO == 2   // ESA/390 support only

  #undef  CUSTOM_BUILD_STRING
  #define CUSTOM_BUILD_STRING "\n\n          QA Scenario 2\n"

  #define NO_370_MODE
  #define OPTION_390_MODE
  #define NO_900_MODE

#endif

/*-------------------------------------------------------------------*/
/* QA Scenario 3:          System/370 and ESA/390 support only       */
/*-------------------------------------------------------------------*/

#if HQA_SCENARIO == 3   // System/370 and ESA/390 support only

  #undef  CUSTOM_BUILD_STRING
  #define CUSTOM_BUILD_STRING "\n\n          QA Scenario 3\n"

  #define OPTION_370_MODE
  #define OPTION_390_MODE
  #define NO_900_MODE

#endif

/*-------------------------------------------------------------------*/
/* QA Scenario 4:          zArchitecure and ESA/390 support only     */
/*-------------------------------------------------------------------*/

#if HQA_SCENARIO == 4   // zArchitecure and ESA/390 support only

  #undef  CUSTOM_BUILD_STRING
  #define CUSTOM_BUILD_STRING "\n\n          QA Scenario 4\n"

  #define NO_370_MODE
  #define OPTION_390_MODE
  #define OPTION_900_MODE

#endif

/*-------------------------------------------------------------------*/
/* QA Scenario 5:          Windows, fthreads                        */
/*-------------------------------------------------------------------*/

#if HQA_SCENARIO == 5   // Windows, fthreads

  #undef  CUSTOM_BUILD_STRING
  #define CUSTOM_BUILD_STRING "\n\n          QA Scenario 5\n"

  #if !defined(_MSVC_)
    #error Selected HQA scenario is for MSVC only
  #endif

  #define OPTION_FTHREADS

#endif

/*-------------------------------------------------------------------*/
/* QA Scenario 6:          Windows, Posix threads                    */
/*-------------------------------------------------------------------*/

#if HQA_SCENARIO == 6   // Windows, Posix threads

  #undef  CUSTOM_BUILD_STRING
  #define CUSTOM_BUILD_STRING "\n\n          QA Scenario 6\n"

  #if defined(_MSVC_)
    #error Selected HQA scenario is not supported for MSVC builds
  #endif
  #if !defined(WIN32) && !defined(_WIN32)
    #error Selected HQA scenario is for Windows only
  #endif

  #undef  OPTION_FTHREADS

#endif

/*-------------------------------------------------------------------*/
/* QA Scenario 7:          Vista, fthreads                           */
/*-------------------------------------------------------------------*/

#if HQA_SCENARIO == 7   // Vista, fthreads

  #undef  CUSTOM_BUILD_STRING
  #define CUSTOM_BUILD_STRING "\n\n          QA Scenario 7\n"

  #if !defined(_MSVC_)
    #error Selected HQA scenario is for MSVC only
  #endif

  // Vista or later...

  #undef  _WIN32_WINNT
  #undef  WINVER
  #undef  NTDDI_VERSION
  #undef  _WIN32_IE

  #define _WIN32_WINNT      0x0600          // Vista
  #define WINVER            0x0600          // Vista
  #define NTDDI_VERSION     0x06000100      // Vista SP1
  #define _WIN32_IE         0x0700          // IE 7.0

  #define OPTION_FTHREADS

#endif

/*-------------------------------------------------------------------*/
/* QA Scenario 8:          Vista, Posix threads                      */
/*-------------------------------------------------------------------*/

#if HQA_SCENARIO == 8   // Vista, Posix threads

  #undef  CUSTOM_BUILD_STRING
  #define CUSTOM_BUILD_STRING "\n\n          QA Scenario 8\n"

  #if defined(_MSVC_)
    #error Selected HQA scenario is not supported for MSVC builds
  #endif
  #if !defined(WIN32) && !defined(_WIN32)
    #error Selected HQA scenario is for Windows only
  #endif

  // Vista or later...

  #undef  _WIN32_WINNT
  #undef  WINVER
  #undef  NTDDI_VERSION
  #undef  _WIN32_IE

  #define _WIN32_WINNT      0x0600          // Vista
  #define WINVER            0x0600          // Vista
  #define NTDDI_VERSION     0x06000100      // Vista SP1
  #define _WIN32_IE         0x0700          // IE 7.0

  #undef  OPTION_FTHREADS

#endif

/*-------------------------------------------------------------------*/
/* QA Scenario 9:          INLINE == forced inline                   */
/*-------------------------------------------------------------------*/

#if HQA_SCENARIO == 9   // INLINE == forced inline

  #undef  CUSTOM_BUILD_STRING
  #define CUSTOM_BUILD_STRING "\n\n          QA Scenario 9\n"

  #undef    INLINE
  #if defined(__GNUC__)
    #define INLINE          __inline__ __attribute__((always_inline))
  #elif defined(_MSVC_)
    #define INLINE          __forceinline
  #else
    #define INLINE          __inline
  #endif

#endif

/*-------------------------------------------------------------------*/
/* QA Scenario 10:         INLINE == inline (just a suggestion)      */
/*-------------------------------------------------------------------*/

#if HQA_SCENARIO == 10  // INLINE == inline (just a suggestion)

  #undef  CUSTOM_BUILD_STRING
  #define CUSTOM_BUILD_STRING "\n\n          QA Scenario 10\n"

  #undef    INLINE
  #if defined(__GNUC__)
    #define INLINE          __inline__
  #elif defined(_MSVC_)
    #define INLINE          __inline
  #else
    #define INLINE          __inline
  #endif

#endif

/*-------------------------------------------------------------------*/
/* QA Scenario 11:         INLINE == null (compiler decides on own)  */
/*-------------------------------------------------------------------*/

#if HQA_SCENARIO == 11  // INLINE == null (compiler decides on own)

  #undef  CUSTOM_BUILD_STRING
  #define CUSTOM_BUILD_STRING "\n\n          QA Scenario 11\n"

  #undef  INLINE
  #define INLINE            /* nothing */

#endif

/*-------------------------------------------------------------------*/
/* QA Scenario 12:         With Shared Devices, With Syncio          */
/*-------------------------------------------------------------------*/

#if HQA_SCENARIO == 12  // With Shared Devices, With Syncio

  #undef  CUSTOM_BUILD_STRING
  #define CUSTOM_BUILD_STRING "\n\n          QA Scenario 12\n"

  #define OPTION_SHARED_DEVICES
  #undef  OPTION_NO_SHARED_DEVICES
  #define OPTION_SYNCIO

#endif

/*-------------------------------------------------------------------*/
/* QA Scenario 13:         Without Shared Devices, With Syncio       */
/*-------------------------------------------------------------------*/

#if HQA_SCENARIO == 13  // Without Shared Devices, With Syncio

  #undef  CUSTOM_BUILD_STRING
  #define CUSTOM_BUILD_STRING "\n\n          QA Scenario 13\n"

  #undef  OPTION_SHARED_DEVICES
  #define OPTION_NO_SHARED_DEVICES
  #define OPTION_SYNCIO

#endif

/*-------------------------------------------------------------------*/
/* QA Scenario 14:         With Shared Devices, Without Syncio       */
/*-------------------------------------------------------------------*/

#if HQA_SCENARIO == 14  // With Shared Devices, Without Syncio

  #undef  CUSTOM_BUILD_STRING
  #define CUSTOM_BUILD_STRING "\n\n          QA Scenario 14\n"

  #define OPTION_SHARED_DEVICES
  #undef  OPTION_NO_SHARED_DEVICES
  #define OPTION_NOSYNCIO

#endif

/*-------------------------------------------------------------------*/
/* QA Scenario 15:         Without Shared Devices, Without Syncio    */
/*-------------------------------------------------------------------*/

#if HQA_SCENARIO == 15  // Without Shared Devices, Without Syncio

  #undef  CUSTOM_BUILD_STRING
  #define CUSTOM_BUILD_STRING "\n\n          QA Scenario 15\n"

  #undef  OPTION_SHARED_DEVICES
  #define OPTION_NO_SHARED_DEVICES
  #define OPTION_NOSYNCIO

#endif

/*-------------------------------------------------------------------*/

#endif // !defined(HQA_SCENARIO) || HQA_SCENARIO == 0

#endif /*_HQADEFS_H_*/
