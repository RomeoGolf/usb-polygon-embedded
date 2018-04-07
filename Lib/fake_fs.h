
/*
  Copyright 2016 - 2018 RomeoGolf

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef _FAKE_FS_H_
#define _FAKE_FS_H_

#include "../MassStorage.h"

/* ----- instead DataflashManager ----- */
/* from ..\..\LUFA\CodeTemplates\DriverStubs\Dataflash.h */
    /* Public Interface - May be used in end-application: */
        /* Macros: */
            /** Constant indicating the total number of dataflash ICs
             * mounted on the selected board. */
            #define DATAFLASH_TOTALCHIPS     1
            /** Mask for no dataflash chip selected. */
            #define DATAFLASH_NO_CHIP        0
            /** Mask for the first dataflash chip selected. */
            #define DATAFLASH_CHIP1          1
            /** Mask for the second dataflash chip selected. */
            #define DATAFLASH_CHIP2          2
            /** Internal main memory page size for the board's
             * dataflash ICs. */
            #define DATAFLASH_PAGE_SIZE      1024
            /** Total number of pages inside each of the board's
             * dataflash ICs. */
            #define DATAFLASH_PAGES          8192

/* from DataflashManager.h */
    /* Preprocessor Checks: */
        #if (DATAFLASH_PAGE_SIZE % 16)
            #error Dataflash page size must be a multiple of 16 bytes.
        #endif

    /* Defines: */
        /** Total number of bytes of the storage medium, comprised
         * of one or more Dataflash ICs. */
        #define VIRTUAL_MEMORY_BYTES         ((uint32_t)DATAFLASH_PAGES * DATAFLASH_PAGE_SIZE * DATAFLASH_TOTALCHIPS)
        /** Block size of the device. This is kept at 512 to remain
         * compatible with the OS despite the underlying
         * storage media (Dataflash) using a different native block
         * size. Do not change this value.
         */
        #define VIRTUAL_MEMORY_BLOCK_SIZE    512
        /** Total number of blocks of the virtual memory for reporting
         * to the host as the device's total capacity. Do not change
         * this value; change VIRTUAL_MEMORY_BYTES instead to alter
         * the media size.
         */
        #define VIRTUAL_MEMORY_BLOCKS        (VIRTUAL_MEMORY_BYTES / VIRTUAL_MEMORY_BLOCK_SIZE)
        /** Blocks in each LUN, calculated from the total capacity
         * divided by the total number of Logical Units in the device. */
        #define LUN_MEDIA_BLOCKS             (VIRTUAL_MEMORY_BLOCKS / TOTAL_LUNS)

    /* Function Prototypes: */
        void WriteBlocks(const uint32_t BlockAddress,
                                          uint16_t TotalBlocks);
        void ReadBlocks(const uint32_t BlockAddress,
                                         uint16_t TotalBlocks);

/* ------------------------------------ */

    void fakeFsInit(void);

#endif
