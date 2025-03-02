/**
 * @file platform.h
 * @brief Defines and marcros to help with identifying the build target/platform and hardware assignments
 * 
 * @copyright Copyright (c) 2021
 * 
 */
/* -*-*- COPYRIGHT-GOES-HERE -*-*-* */

#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#include <common/dll_export.h>

/* this forces out-of-order parameter expansion */
#define _EX(_hwa) _hwa
/**
 * @brief Hardware platform id, extracted from the compiler's command line -D parameters
 * 
 * @code
 * #if HW_PLATFORM == HW_PLATFORM_HOST
 *  ....
 * #endif
 * @endcode
 */
#define HW_PLATFORM _EX(TUBS_CONFIG_HW_TARGET)

#define HW_PLATFORM_CNA3000     1 /**< Platform ID for CNA3000 */
#define HW_PLATFORM_CPU1900     2 /**< Platform ID for CPU1900 */
#define HW_PLATFORM_HOST        3 /**< Platform ID for running on the host */
#define HW_PLATFORM_cmu         4 /**< Platform ID for CMU */
#define HW_PLATFORM_cmuv2       5 /**< Platform ID for CMUv2 */

#endif /* __PLATFORM_H__*/
