
#ifndef _FAKE_FS_H_
#define _FAKE_FS_H_

	#include "../MassStorage.h"

/* ----- instead DataflashManager ----- */
/* from ..\..\LUFA\CodeTemplates\DriverStubs\Dataflash.h */
	/* Public Interface - May be used in end-application: */
		/* Macros: */
			/** Constant indicating the total number of dataflash ICs mounted on the selected board. */
			#define DATAFLASH_TOTALCHIPS     1 // TODO: Replace with the number of Dataflashes on the board, max 2
			/** Mask for no dataflash chip selected. */
			#define DATAFLASH_NO_CHIP        0
			/** Mask for the first dataflash chip selected. */
			#define DATAFLASH_CHIP1          1 // TODO: Replace with mask with the pin attached to the first Dataflash /CS set
			/** Mask for the second dataflash chip selected. */
			#define DATAFLASH_CHIP2          2 // TODO: Replace with mask with the pin attached to the second Dataflash /CS set
			/** Internal main memory page size for the board's dataflash ICs. */
			#define DATAFLASH_PAGE_SIZE      1024 // TODO: Replace with the page size for the Dataflash ICs
			/** Total number of pages inside each of the board's dataflash ICs. */
			#define DATAFLASH_PAGES          8192 // TODO: Replace with the total number of pages inside one of the Dataflash ICs

/* from DataflashManager.h */
	/* Preprocessor Checks: */
		#if (DATAFLASH_PAGE_SIZE % 16)
			#error Dataflash page size must be a multiple of 16 bytes.
		#endif

	/* Defines: */
		/** Total number of bytes of the storage medium, comprised of one or more Dataflash ICs. */
		#define VIRTUAL_MEMORY_BYTES         ((uint32_t)DATAFLASH_PAGES * DATAFLASH_PAGE_SIZE * DATAFLASH_TOTALCHIPS)
		/** Block size of the device. This is kept at 512 to remain compatible with the OS despite the underlying
		 *  storage media (Dataflash) using a different native block size. Do not change this value.
		 */
		#define VIRTUAL_MEMORY_BLOCK_SIZE    512
		/** Total number of blocks of the virtual memory for reporting to the host as the device's total capacity. Do not
		 *  change this value; change VIRTUAL_MEMORY_BYTES instead to alter the media size.
		 */
		#define VIRTUAL_MEMORY_BLOCKS        (VIRTUAL_MEMORY_BYTES / VIRTUAL_MEMORY_BLOCK_SIZE)
		/** Blocks in each LUN, calculated from the total capacity divided by the total number of Logical Units in the device. */
		#define LUN_MEDIA_BLOCKS             (VIRTUAL_MEMORY_BLOCKS / TOTAL_LUNS)

	/* Function Prototypes: */
		void WriteBlocks(const uint32_t BlockAddress,
		                                  uint16_t TotalBlocks);
		void ReadBlocks(const uint32_t BlockAddress,
		                                 uint16_t TotalBlocks);

/* ------------------------------------ */

	void fakeFsInit(void);

#endif
