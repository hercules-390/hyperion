/* HQADEFS.H    (C) "Fish" (David B. Trout), 2013                    */
/*              QA Build Configuration Testing Scenarios             */
/*                                                                   */
/*   Released under "The Q Public License Version 1"                 */
/*   (http://www.hercules-390.org/herclic.html) as modifications to  */
/*   Hercules.                                                       */

#ifndef _HQADEFS_H_
#define _HQADEFS_H_

/*-------------------------------------------------------------------*/
/* QA Scenario 0:              Reserved for normal builds            */
/*-------------------------------------------------------------------*/

#if !defined(HQA_SCENARIO) || HQA_SCENARIO == 0

    /* Do nothing. This effectively does a normal build */

#else

/*-------------------------------------------------------------------*/
/* QA Scenario 1:          System/370 support only                   */
/*-------------------------------------------------------------------*/

#if HQA_SCENARIO == 1

  #undef  CUSTOM_BUILD_STRING
  #define CUSTOM_BUILD_STRING "\n\n          QA Scenario 1\n"

  #define OPTION_370_MODE
  #define NO_390_MODE
  #define NO_900_MODE

#endif

/*-------------------------------------------------------------------*/
/* QA Scenario 2:           ESA/390 support only                     */
/*-------------------------------------------------------------------*/

#if HQA_SCENARIO == 2

  #undef  CUSTOM_BUILD_STRING
  #define CUSTOM_BUILD_STRING "\n\n          QA Scenario 2\n"

  #define NO_370_MODE
  #define OPTION_390_MODE
  #define NO_900_MODE

#endif

/*-------------------------------------------------------------------*/
/* QA Scenario 3:           System/370 and ESA/390 support only      */
/*-------------------------------------------------------------------*/

#if HQA_SCENARIO == 3

  #undef  CUSTOM_BUILD_STRING
  #define CUSTOM_BUILD_STRING "\n\n          QA Scenario 3\n"

  #define OPTION_370_MODE
  #define OPTION_390_MODE
  #define NO_900_MODE

#endif

/*-------------------------------------------------------------------*/
/* QA Scenario 4:        zArchitecure and ESA/390 support only       */
/*-------------------------------------------------------------------*/

#if HQA_SCENARIO == 4

  #undef  CUSTOM_BUILD_STRING
  #define CUSTOM_BUILD_STRING "\n\n          QA Scenario 4\n"

  #define NO_370_MODE
  #define OPTION_390_MODE
  #define OPTION_900_MODE

#endif

/*-------------------------------------------------------------------*/
/* QA Scenario 5:              fthreads, Herc I/O                    */
/*-------------------------------------------------------------------*/

#if HQA_SCENARIO == 5

  #if !defined(WIN32) && !defined(_WIN32)
    #error Selected HQA scenario is for Windows only
  #endif

  #undef  CUSTOM_BUILD_STRING
  #define CUSTOM_BUILD_STRING "\n\n          QA Scenario 5\n"

  #define OPTION_FTHREADS
  #undef  OPTION_WTHREADS

  #define OPTION_HERCIO
  #undef  OPTION_FISHIO

#endif

/*-------------------------------------------------------------------*/
/* QA Scenario 6:                Vista, Win-threads, FishIO          */
/*-------------------------------------------------------------------*/

#if HQA_SCENARIO == 6

  #if !defined(WIN32) && !defined(_WIN32)
    #error Selected HQA scenario is for Windows only
  #endif

  #undef  CUSTOM_BUILD_STRING
  #define CUSTOM_BUILD_STRING "\n\n          QA Scenario 6\n"

  // Vista or later required for WTHREADS...

  #undef  _WIN32_WINNT
  #undef  WINVER
  #undef  NTDDI_VERSION
  #undef  _WIN32_IE

  #define _WIN32_WINNT      0x0600          // Vista
  #define WINVER            0x0600          // Vista
  #define NTDDI_VERSION     0x06000100      // Vista SP1
  #define _WIN32_IE         0x0700          // IE 7.0

  #define OPTION_WTHREADS
  #undef  OPTION_FTHREADS

  #define OPTION_FISHIO
  #undef  OPTION_HERCIO

#endif

/*-------------------------------------------------------------------*/
/* QA Scenario 7:                Vista, Win-threads, Herc I/O        */
/*-------------------------------------------------------------------*/

#if HQA_SCENARIO == 7

  #if !defined(WIN32) && !defined(_WIN32)
    #error Selected HQA scenario is for Windows only
  #endif

  #undef  CUSTOM_BUILD_STRING
  #define CUSTOM_BUILD_STRING "\n\n          QA Scenario 7\n"

  // Vista or later required for WTHREADS...

  #undef  _WIN32_WINNT
  #undef  WINVER
  #undef  NTDDI_VERSION
  #undef  _WIN32_IE

  #define _WIN32_WINNT      0x0600          // Vista
  #define WINVER            0x0600          // Vista
  #define NTDDI_VERSION     0x06000100      // Vista SP1
  #define _WIN32_IE         0x0700          // IE 7.0

  #define OPTION_WTHREADS
  #undef  OPTION_FTHREADS

  #define OPTION_HERCIO
  #undef  OPTION_FISHIO

#endif

/*-------------------------------------------------------------------*/

#endif // !defined(HQA_SCENARIO) || HQA_SCENARIO == 0

#endif /*_HQADEFS_H_*/
