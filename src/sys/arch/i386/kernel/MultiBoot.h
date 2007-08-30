#ifndef I686_MULTIBOOT_H
#define I686_MULTIBOOT_H
/*
 * Copyright (C) 2005, The EROS Group, LLC.
 *
 * This file is part of the EROS Operating System runtime library.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330 Boston, MA 02111-1307, USA.
 */

/** @file
 * @brief Description of multiboot interface.
 *
 * This file was created directly from the specification
 * document. This header is NOT derived from the multiboot header file
 * distributed with the GRUB distribution, or the similar file
 * distributed with the Linux distribution.
 */

/** @brief Multiboot magic number */
#define MULTIBOOT_HEADER_MAGIC 0x1BADB002

#ifdef __ELF__
#define MULTIBOOT_HEADER_FLAGS 0x00000007
#else
#define MULTIBOOT_HEADER_FLAGS 0x00010007
#endif

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/* Bits of the flags field: */
#define MBH_PageAlign 0x1 /**< @brief Require modules aligned to 4k 
			     boundary */
#define MBH_MemInfo   0x2     /**< @brief Require mem_* fields
				 populated, and best effort on 
				 mmap_* fields */
#define MBH_VidModes  0x4     /**< @brief Require video mode table */
#define MBF_LoadFrom  0x10000 /**< @brief Use headerAddr and friends */

#ifndef __ASSEMBLER__

#include <stdint.h>

/** @brief Multiboot header structure. */
struct MultibootHeader {
  uint32_t magic;
  uint32_t flags;
  uint32_t checksum;		/**< @brief magic+flags+checksum == 0 */

  /* The following fields are significant only if flags[16] is set.
     They can otherwise be ommitted. */
  uint32_t headerAddr;
  uint32_t loadAddr;
  uint32_t loadEndAddr;
  uint32_t bssEndAddr;
  uint32_t entryAddr;

  /* The following fields are significant only if flags[2] is
     set. They can otherwise be ommitted. */
  uint32_t modeType;
  uint32_t width;
  uint32_t height;
  uint32_t depth;
};

#define MBI_MEMVALID  0x001  /**< @brief mem_* fields valid */
#define MBI_BOOTDEV   0x002  /**< @brief boot_device fields valid */
#define MBI_CMDLINE   0x004  /**< @brief cmdline valid */
#define MBI_MODS      0x008  /**< @brief mods valid */
#define MBI_SYMTAB    0x010  /**< @brief symtab valid */
#define MBI_SECHDR    0x020  /**< @brief section header valid */
#define MBI_MMAP      0x040  /**< @brief mmap_* valid */
#define MBI_DRIVES    0x080  /**< @brief drives_* valid */
#define MBI_CFGTBL    0x100  /**< @brief config table valid */
#define MBI_BOOTLDR   0x200  /**< @brief boot loader name valid */
#define MBI_APMTBL    0x400  /**< @brief APM table valid */
#define MBI_GFXTBL    0x800  /**< @brief graphcs table valid */
 
/** @brief Generic multiboot information */
struct MultibootInfo {
  uint32_t flags;

  /* valid if flags [0] is set: */
  uint32_t mem_lower;		/**< @brief in kilobytes, start at 0, <= 640 */
  uint32_t mem_upper;		/**< @brief in kilobytes, start at 1Mbyte */
  /* valid if flags [1] is set: */
  struct {
    uint8_t  drive;
    uint8_t  part1;
    uint8_t  part2;
    uint8_t  part3;
  } boot_device;
  /* valid if flags [2] is set: */
  uint32_t    cmdline;		/**< @brief phys addr of null-terminated cmdline */
  /* valid if flags [3] is set: */
  uint32_t mods_count;
  uint32_t mods_addr;
  /* valid if flags [4] or flags[5] is set (mutually exclusive): */
  union {
    struct {
      uint32_t tabsize;		/**< @brief number of nlist structs */
      uint32_t strsize;		/*  */
      uint32_t addr;		/**< @brief of table of nlist structs */
      uint32_t : 32;		/* reserved */
    } aout;			/* bit 4 set */
    struct {
      uint32_t num;		/**< @brief number of elf section headers */
      uint32_t size;    	/**< @brief size of each entry */
      uint32_t addr;		/**< @brief pointer to them */
      uint32_t shndx;		/**< @brief string table */
    } elf;
  } u;
  /* valid if flags [6] is set */
  uint32_t mmap_length;
  uint32_t mmap_addr;
  /* valid if flags [7] is set */
  uint32_t drives_length;
  uint32_t drives_addr;
  /* valid if flags [8] is set */
  uint32_t config_table;	/**< @brief per GET CONFIGURATION */
  /* valid if flags [9] is set */
  uint32_t boot_loader_name;	/**< @brief null terminated */
  /* valid if flags [10] is set */
  uint32_t apm_table;
  /* valid if flags [11] is set */
  uint32_t vbe_control_info;	/**< @brief as returned by VBE FN 00h */
  uint32_t vbe_mode_info;     /**< @brief as returned by VBE FN 01h */
  uint32_t vbe_mode;		/**< @brief current mode */
  uint32_t vbe_interface_seg;	/**< @brief protected mode interface */
  uint32_t vbe_interface_off;
  uint32_t vbe_interface_len;
};

/** @brief Multiboot loaded module information */
struct MultibootModuleInfo {
  uint32_t mod_start;
  uint32_t mod_end;
  uint32_t string;
  uint32_t :32;			/* reserved */
};

/** @brief Multiboot memory map.
 *
 * Size field is not included in reported size.
 */
struct MultibootMMap {
  uint32_t size;
  uint32_t base_addr_low;
  uint32_t base_addr_high;
  uint32_t length_low;
  uint32_t length_high;
  uint32_t type;
};

/** @brief Multiboot drive information */
struct MultibootDrive {
  uint32_t size;		/**< @brief distance to next */
  uint8_t  drive_number;	/**< @brief BIOS drive number */
  uint8_t  drive_mode;		/**< @brief 0 == CHS, 1 == LBA */
  uint16_t drive_cylinders;	/**< @brief BIOS-detected geometry */
  uint8_t  drive_heads;
  uint8_t  drive_sectors;
  uint16_t drive_ports[0];	/**< @brief I/O ports used to talk to this
				   drive. Last entry is zero *//*  */
};

/** @brief Multiboot APM information */
struct MultibootAPM {
  uint16_t version;
  uint16_t cseg;
  uint32_t offset;
  uint16_t cseg_16;
  uint16_t dseg;
  uint16_t flags;
  uint16_t cseg_len;
  uint16_t cseg_16_len;
  uint16_t dseg_len;
};

#endif /* __ASSEMBLER__ */

#endif /* I686_MULTIBOOT_H */
