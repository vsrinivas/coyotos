#ifndef __COYOTOS_MACHINE_REGISTERS_H__
#define __COYOTOS_MACHINE_REGISTERS_H__
/** @file
 * @brief Automatically generated architecture wrapper header.
 */

#include "target.h"

#if (COYOTOS_ARCH == COYOTOS_ARCH_i386) || defined(COYOTOS_UNIVERSAL_CROSS)
#include "../i386/registers.h"
#endif

#if (COYOTOS_ARCH == COYOTOS_ARCH_coldfire) || defined(COYOTOS_UNIVERSAL_CROSS)
#include "../coldfire/registers.h"
#endif

#if (COYOTOS_ARCH == COYOTOS_ARCH_arm) || defined(COYOTOS_UNIVERSAL_CROSS)
#include "../arm/registers.h"
#endif

#endif /* __COYOTOS_MACHINE_REGISTERS_H__ */
