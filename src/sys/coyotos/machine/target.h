#ifndef __COYOTOS_MACHINE_TARGET_H__
#define __COYOTOS_MACHINE_TARGET_H__
/** @file
 * @brief Automatically generated architecture identification header.
 */

#define COYOTOS_ARCH_i386       1
#define COYOTOS_ARCH_coldfire   2
#define COYOTOS_ARCH_arm        3

#ifndef CROSS_COMPILING

#if defined(__i386__)
#define COYOTOS_ARCH COYOTOS_ARCH_i386
#endif
#if defined(__mcoldfire__)
#define COYOTOS_ARCH COYOTOS_ARCH_coldfire
#endif
#if defined(__arm__)
#define COYOTOS_ARCH COYOTOS_ARCH_arm
#endif

#endif /* CROSS_COMPILING */

#endif /* __COYOTOS_MACHINE_TARGET_H__ */
