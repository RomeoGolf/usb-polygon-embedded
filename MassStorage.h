
/*
  Copyright 2016 - 2018 RomeoGolf
  Copyright 2015  Dean Camera (dean [at] fourwalledcubicle [dot] com)

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/** \file
 *
 *  Header file for MassStorage.c.
 */

#ifndef _MASS_STORAGE_H_
#define _MASS_STORAGE_H_

    /* Includes: */
        #include <avr/io.h>
        #include <avr/wdt.h>
        #include <avr/power.h>
        #include <avr/interrupt.h>

        #include "Descriptors.h"

        #include "Lib/SCSI.h"
        #include "Config/AppConfig.h"

        #include <LUFA/Drivers/USB/USB.h>

    /* Macros: */
        /** LED mask for the library LED driver, to indicate that
         * the USB interface is not ready. */
        #define LEDMASK_USB_NOTREADY       LEDS_LED1

        /** LED mask for the library LED driver, to indicate that
         * the USB interface is enumerating. */
        #define LEDMASK_USB_ENUMERATING   (LEDS_LED2 | LEDS_LED3)

        /** LED mask for the library LED driver, to indicate that
         * the USB interface is ready. */
        #define LEDMASK_USB_READY         (LEDS_LED2 | LEDS_LED4)

        /** LED mask for the library LED driver, to indicate that
         * an error has occurred in the USB interface. */
        #define LEDMASK_USB_ERROR         (LEDS_LED1 | LEDS_LED3)

        /** LED mask for the library LED driver, to indicate that
         * the USB interface is busy. */
        #define LEDMASK_USB_BUSY           LEDS_LED2

    /* Global Variables: */
        extern MS_CommandBlockWrapper_t  CommandBlock;
        extern MS_CommandStatusWrapper_t CommandStatus;
        extern volatile bool             IsMassStoreReset;

    /* Function Prototypes: */
        void SetupHardware(void);
        void MassStorage_Task(void);

        void EVENT_USB_Device_Connect(void);
        void EVENT_USB_Device_Disconnect(void);
        void EVENT_USB_Device_ConfigurationChanged(void);
        void EVENT_USB_Device_ControlRequest(void);

        #if defined(INCLUDE_FROM_MASSSTORAGE_C)
            static bool ReadInCommandBlock(void);
            static void ReturnCommandStatus(void);
        #endif

#endif

