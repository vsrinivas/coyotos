#ifndef __COYOTOS_MACHINE_ENDIAN_H__
#define __COYOTOS_MACHINE_ENDIAN_H__
/** @file
 * @brief Automatically generated architecture wrapper header.
 */

#include "target.h"

#if (COYOTOS_TARGET == COYOTOS_TARGET_i386) || defined(COYOTOS_UNIVERSAL_CROSS)
#include "../i386/endian.h"
#endif

#if (COYOTOS_TARGET == COYOTOS_TARGET_coldfire) || defined(COYOTOS_UNIVERSAL_CROSS)
#include "../coldfire/endian.h"
#endif

#if (COYOTOS_TARGET == COYOTOS_TARGET_arm) || defined(COYOTOS_UNIVERSAL_CROSS)
#include "../arm/endian.h"
#endif

#endif /* __COYOTOS_MACHINE_ENDIAN_H__ */
