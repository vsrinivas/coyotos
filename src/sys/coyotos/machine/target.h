#ifndef __COYOTOS_MACHINE_TARGET_H__
#define __COYOTOS_MACHINE_TARGET_H__
/** @file
 * @brief Automatically generated architecture identification header.
 */

#define COYOTOS_TARGET_i386       1
#define COYOTOS_TARGET_coldfire   2
#define COYOTOS_TARGET_arm        3

#ifndef CROSS_COMPILING

#if defined(__i386__)
#define COYOTOS_TARGET COYOTOS_TARGET_i386
#endif
#if defined(__coldfire__)
#define COYOTOS_TARGET COYOTOS_TARGET_coldfire
#endif
#if defined(__arm__)
#define COYOTOS_TARGET COYOTOS_TARGET_arm
#endif

#endif /* CROSS_COMPILING */

#endif /* __COYOTOS_MACHINE_TARGET_H__ */
