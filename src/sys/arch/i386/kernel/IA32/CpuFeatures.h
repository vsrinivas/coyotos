#ifndef __IA32_CPUFEATURES_H__
#define __IA32_CPUFEATURES_H__
/*
 * Copyright (C) 2005, Jonathan S. Shapiro.
 *
 * This file is part of the Coyotos Operating System.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/** @file
 * @brief CPU features bitmap definitions.
 */

/* Local Variables: */
/* comment-column:36 */
/* End: */

#define CPUFEAT_FPU   0x01u         /**< @brief processor has FPU */
#define CPUFEAT_VME   0x02u	    /**< @brief v86 mode */
#define CPUFEAT_DE    0x04u	    /**< @brief debugging extensions */
#define CPUFEAT_PSE   0x08u	    /**< @brief page size extensions */
#define CPUFEAT_TSC   0x10u	    /**< @brief time stamp counter */
#define CPUFEAT_MSR   0x20u	    /**< @brief model specific registers */
#define CPUFEAT_PAE   0x40u	    /**< @brief physical address extensions */
#define CPUFEAT_MCE   0x80u	    /**< @brief machine check exception */
#define CPUFEAT_CXB   0x100u	    /**< @brief compare and exchange byte */
#define CPUFEAT_APIC  0x200u	    /**< @brief APIC on chip */
#define CPUFEAT_SEP   0x800u	    /**< @brief Fast system call */
#define CPUFEAT_MTRR  0x1000u	    /**< @brief Memory type range registers */
#define CPUFEAT_PGE   0x2000u	    /**< @brief global pages */
#define CPUFEAT_MCA   0x4000u	    /**< @brief machine check architecture */
#define CPUFEAT_CMOV  0x8000u	    /**< @brief conditional move
				       instructions */
#define CPUFEAT_FGPAT 0x10000u	    /**< @brief CMOVcc/FMOVCC */
#define CPUFEAT_PSE36 0x20000u	    /**< @brief 4MB pages w/ 36-bit
				       phys addr */
#define CPUFEAT_PN    0x40000u	    /**< @brief processor number */
#define CPUFEAT_MMX   0x800000u	    /**< @brief processor has MMX extensions */
#define CPUFEAT_FXSR  0x1000000u    /**< @brief streaming float save */
#define CPUFEAT_XMM   0x2000000u    /**< @brief streaming SIMD extensions */

#endif /* __CPUFEATURES_H__ */
