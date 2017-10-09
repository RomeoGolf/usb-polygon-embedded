/*
             LUFA Library
     Copyright (C) Dean Camera, 2015.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2015  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Main source file for the Mass Storage demo. This file contains the main tasks of the demo and
 *  is responsible for the initial application hardware configuration.
 */

#define  INCLUDE_FROM_MASSSTORAGE_C
#include "MassStorage.h"
#include "common.h"
#include "Lib/fake_fs.h"

#include <util/delay.h>

/** Structure to hold the latest Command Block Wrapper issued by the host, containing a SCSI command to execute. */
MS_CommandBlockWrapper_t  CommandBlock;

/** Structure to hold the latest Command Status Wrapper to return to the host, containing the status of the last issued command. */
MS_CommandStatusWrapper_t CommandStatus = { .Signature = MS_CSW_SIGNATURE };

/** Flag to asynchronously abort any in-progress data transfers upon the reception of a mass storage reset command. */
volatile bool IsMassStoreReset = false;

unsigned char cnt = 0;			// просто счетчик
uint8_t data_PC = 0;
uint8_t data_device = 0;
uint8_t canDo = 0;
uint8_t data[128] = { 0 };
uint8_t isSpiOn = 0;

void out8bit(uint8_t data8) {

	if (isSpiOn == 1) {
		PORTB &= ~(1 << 0);		// cs -> 0
		SPDR = data8;
		while (!(SPSR & (1 << SPIF))) ;	// wait for transmit
		PORTB |= (1 << 0);			// cs -> 1

	} else {
		/**
		PORTB &= ~(1 << 0);		// cs -> 0
		for (uint8_t i = 0; i < 8; i++){
			PORTB &= ~(1 << 1);		// sclk -> 0
			if (data8 & 0x80) {
				PORTB |= (1 << 2);		// data -> 1
			} else {
				PORTB &= ~(1 << 2);	// data -> 0
			}
			data8 <<= 1;
			PORTB |= (1 << 1);			// sclk -> 1
		}
		PORTB |= (1 << 0);			// cs -> 1
		**/
	}
}

void scrClear(void)
{
	PORTB &= ~(1 << 5);	// d/c -> 0
	out8bit(0x40); // Y = 0
	out8bit(0x80); // X = 0
	
	/* 96 * 65 ? 768 -> 864 */
	PORTB |= (1 << 5);		// d/c -> 1
	/*for(uint16_t i = 0; i < 864; i++) out8bit(0x00);*/
	for(uint16_t i = 0; i < (96 * 9); i++) out8bit(0x00);
	PORTB &= ~(1 << 5);	// d/c -> 0
}

/** Main program entry point. This routine configures the hardware required by the application, then
 *  enters a loop to run the application tasks in sequence.
 */
int main(void)
{

    PORTD = 0x00;    // начальное значение - все нули
    DDRD = 0xFF;     // все линии порта на вывод
    PORTC = 0x00;    // без "подтяжки" (есть внешняя)
	DDRC = 0x00;     // все линии порта на ввод
    DDRB = 0xFF;     // все линии порта на ввыод
    PORTB = 0x00;    // начальное значение - все нули
	/*PORTB |= (1 << 0)	// SC -> 1, not active*/

    unsigned char cnt_bt = 0;     // счетчик нажатий на кнопки
    unsigned char mode_out = 0;   // режим вывода
    unsigned char bt_now = 0;     // состояние кнопок
    unsigned char bt_old = 0;     // состояние кнопок в прошлый раз
								// разряды кнопок сверху вниз:
								// 4, 5, 2, 6, 7
								// маски:
								// 10, 20, 04, 40, 80

	/* Enable SPI, Master, set clock rate fck/16 */
	SPCR = (1 << SPE) | (1 << MSTR) | (0 << SPR1) | (0 << SPR0);
	isSpiOn = 1;

	// ********************************

	/*PORTB |= (1 << 4);*/
	/*_delay_ms(10);*/
	PORTB &= ~(1 << 4);
	_delay_ms(10);
	PORTB |= (1 << 4);


	//PORTB &= ~(1 << 0);		// cs -> 0

	// screen init
	PORTB &= ~(1 << 5);	// d/c -> 0
	out8bit(0x21);
	/*out8bit(0x80 + 56);*/
	/*out8bit(0x80 | 0x10);*/
	out8bit(0x80 | 29);
	/*out8bit(0x04);*/
	/*out8bit(0x13);*/
	out8bit(0x12);
	out8bit(0x20);
	out8bit(0x0c);

	scrClear();

	// line on the top
	PORTB &= ~(1 << 5);	// d/c -> 0
	out8bit(0x40);
	out8bit(0x80);
	PORTB |= (1 << 5);		// d/c -> 1
	for (uint8_t i = 0; i < 96; i++) {
		out8bit(0x02);
	}

	// line on the bottom
	PORTB &= ~(1 << 5);	// d/c -> 0
	out8bit(0x47);
	out8bit(0x80);
	PORTB |= (1 << 5);		// d/c -> 1
	for (uint8_t i = 0; i < 96; i++) {
		out8bit(0x80);
	}

	// position set
	PORTB &= ~(1 << 5);	// d/c -> 0
	out8bit(0x43);
	out8bit(0x80 | 0x28);

	// data out
	PORTB |= (1 << 5);		// d/c -> 1

	out8bit(0x00);
	out8bit(0x00);
	out8bit(0x00);
	out8bit(0x80);
	out8bit(0x40);
	out8bit(0x20);
	out8bit(0x10);
	out8bit(0x88);
	out8bit(0x44);
	out8bit(0x22);
	out8bit(0x31);
	out8bit(0xc3);
	out8bit(0x0c);
	out8bit(0x30);
	out8bit(0xc0);
	out8bit(0x00);
	out8bit(0x00);

	PORTB &= ~(1 << 5);		// d/c -> 0
	out8bit(0x44);
	out8bit(0x80 | 0x28);
	PORTB |= (1 << 5);		// d/c -> 1

	out8bit(0x04);
	out8bit(0x06);
	out8bit(0x05);
	out8bit(0x04);
	out8bit(0x04);
	out8bit(0x06);
	out8bit(0x05);
	out8bit(0x04);
	out8bit(0x05);
	out8bit(0x06);
	out8bit(0x05);
	out8bit(0x04);
	out8bit(0x07);
	out8bit(0x04);
	out8bit(0x04);
	out8bit(0x07);
	out8bit(0x04);

	PORTB &= ~(1 << 5);	// d/c -> 0

	// ********************************


	SetupHardware();

/*	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);*/
	GlobalInterruptEnable();

/*	for(int i = 0; i < 128; i++) {*/
/*		data[i] = (i >> 1);*/
/*	}*/

    /* запуск таймера 0 на период ~0.01 с */
    /* (защита от дребезга) */
    TCCR0B = 4;  /* 1 тик = 0.000032 с */
    TCNT0 = 0;   /* 256 раз ~ 0.008192 c  */

    /* запуск таймера 1 на период 0.5 с */
    /* (счетчик с полусекундной задержкой) */
    TCCR1B = 4;              /* 1 тик = 0.000032 с */
    TCNT1 = 65536 - 15625;
    /* разрешение прерываний таймера 1*/
    /*TIMSK1 = (1 << TOIE1);*/

	for (;;)
	{
		MassStorage_Task();
		USB_USBTask();
		
		/* проверка срабатывания таймера без прерываний по флагу */
        if ((TIFR1 & 1) == 1) {
            TCNT1 = 65536 - 15625;  /* перезапуск таймера 1 */
            TIFR1 = 1;              /* сброс флага таймера 1 */
            cnt++;                  /* инкремент контрольного счетчика по таймеру */
        }

        /* обработка действий по срабатыванию таймера 0 */
        if ((TIFR0 & 1) == 1) {
            TCNT0 = 0;  /* перезапуск таймера 0 */
            TIFR0 = 1;  /* сброс флага срабатывания таймера 0 */

            /* cnt++;*/     // инкремент счетчика - чтобы что-то изменялось
            bt_now = PINC;                  // считывание порта с кнопками
            if (bt_now != bt_old) {         // если состояние порта изменилось
                if ((bt_now & 0x30) == 0) { // если нажаты сразу две верхние кнопки на разрядах 3 и 4
                    mode_out++;             // циклически изменить режим отображения,
                    mode_out = mode_out & 3;// которых всего 4 - 0, 1, 2 и 3 (2 разряда по маске)
                } else {                    // в противном случае
                    if ((bt_now & 0x10) == 0) {cnt_bt++;}  // верхняя кнопка увеличивает счет нажатий
                    if ((bt_now & 0x20) == 0) {cnt_bt--;}  // а вторая сверху - уменьшает
                }
				if ((bt_now & 0x40) == 0) {
					canDo = 1;
					PORTB |= (1 << 6);
				}
				if ((bt_now & 0x80) == 0) {
					canDo = 0;
					PORTB &= ~(1 << 6);
				}

                bt_old = bt_now;            // и сохраняем состояние порта для следующей проверки
            }

			data_device = cnt_bt;

            switch (mode_out) {
                case 0 :
                    PORTD = cnt;    // просто счетчик
                    break;
                case 1 :
/*                     PORTD = bt_now;            // состояние кнопок*/
                    PORTD = canDo;
                    break;
                case 2 :
                    PORTD = cnt_bt;  // счетчик нажатий
                    break;
                default:
                    PORTD = data_PC;

            }
        }
	}
}

/* обработчик прерывания таймера, если разрешено, для проверки */
ISR (TIMER1_OVF_vect)
{
    TCNT1 = 65536-15625;    // перезапуск таймера
    cnt++;                  // инкремент контрольного счетчика
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void)
{
#if (ARCH == ARCH_AVR8)
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);
#elif (ARCH == ARCH_XMEGA)
	/* Start the PLL to multiply the 2MHz RC oscillator to 32MHz and switch the CPU core to run from it */
	XMEGACLK_StartPLL(CLOCK_SRC_INT_RC2MHZ, 2000000, F_CPU);
	XMEGACLK_SetCPUClockSource(CLOCK_SRC_PLL);

	/* Start the 32MHz internal RC oscillator and start the DFLL to increase it to 48MHz using the USB SOF as a reference */
	XMEGACLK_StartInternalOscillator(CLOCK_SRC_INT_RC32MHZ);
	XMEGACLK_StartDFLL(CLOCK_SRC_INT_RC32MHZ, DFLL_REF_INT_USBSOF, F_USB);

	PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
#endif

	/* Hardware Initialization */
/*	LEDs_Init();*/
/*	Dataflash_Init();*/
	USB_Init();
	fakeFsInit();

	/* Check if the Dataflash is working, abort if not */
/*	if (!(DataflashManager_CheckDataflashOperation()))*/
/*	{*/
/*		LEDs_SetAllLEDs(LEDMASK_USB_ERROR);*/
/*		for(;;);*/
/*	}*/

	/* Clear Dataflash sector protections, if enabled */
/*	DataflashManager_ResetDataflashProtections();*/
}

/** Event handler for the USB_Connect event. This indicates that the device is enumerating via the status LEDs. */
void EVENT_USB_Device_Connect(void)
{
	/* Indicate USB enumerating */
/*	LEDs_SetAllLEDs(LEDMASK_USB_ENUMERATING);*/

	/* Reset the MSReset flag upon connection */
	IsMassStoreReset = false;
}

/** Event handler for the USB_Disconnect event. This indicates that the device is no longer connected to a host via
 *  the status LEDs and stops the Mass Storage management task.
 */
void EVENT_USB_Device_Disconnect(void)
{
	/* Indicate USB not ready */
/*	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);*/
}

/** Event handler for the USB_ConfigurationChanged event. This is fired when the host set the current configuration
 *  of the USB device after enumeration - the device endpoints are configured and the Mass Storage management task started.
 */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	/* Setup Mass Storage Data Endpoints */
	ConfigSuccess &= Endpoint_ConfigureEndpoint(MASS_STORAGE_IN_EPADDR,  EP_TYPE_BULK, MASS_STORAGE_IO_EPSIZE, 1);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(MASS_STORAGE_OUT_EPADDR, EP_TYPE_BULK, MASS_STORAGE_IO_EPSIZE, 1);

	/* Indicate endpoint configuration success or failure */
/*	LEDs_SetAllLEDs(ConfigSuccess ? LEDMASK_USB_READY : LEDMASK_USB_ERROR);*/
}

/** Event handler for the USB_ControlRequest event. This is used to catch and process control requests sent to
 *  the device from the USB host before passing along unhandled control requests to the library for processing
 *  internally.
 */
void EVENT_USB_Device_ControlRequest(void)
{
	/* Process UFI specific control requests */
	switch (USB_ControlRequest.bRequest)
	{
		case MS_REQ_MassStorageReset:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();
				Endpoint_ClearStatusStage();

				/* Indicate that the current transfer should be aborted */
				IsMassStoreReset = true;
			}

			break;
		case MS_REQ_GetMaxLUN:
			if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();

				/* Indicate to the host the number of supported LUNs (virtual disks) on the device */
				Endpoint_Write_8(TOTAL_LUNS - 1);

				Endpoint_ClearIN();
				Endpoint_ClearStatusStage();
			}

			break;
	}
}

/** Task to manage the Mass Storage interface, reading in Command Block Wrappers from the host, processing the SCSI commands they
 *  contain, and returning Command Status Wrappers back to the host to indicate the success or failure of the last issued command.
 */
void MassStorage_Task(void)
{
	/* Device must be connected and configured for the task to run */
	if (USB_DeviceState != DEVICE_STATE_Configured)
	  return;

	/* Process sent command block from the host if one has been sent */
	if (ReadInCommandBlock())
	{
		/* Indicate busy */
/*		LEDs_SetAllLEDs(LEDMASK_USB_BUSY);*/

		/* Check direction of command, select Data IN endpoint if data is from the device */
		if (CommandBlock.Flags & MS_COMMAND_DIR_DATA_IN)
		  Endpoint_SelectEndpoint(MASS_STORAGE_IN_EPADDR);

		/* Decode the received SCSI command, set returned status code */
		CommandStatus.Status = SCSI_DecodeSCSICommand() ? MS_SCSI_COMMAND_Pass : MS_SCSI_COMMAND_Fail;

		/* Load in the CBW tag into the CSW to link them together */
		CommandStatus.Tag = CommandBlock.Tag;

		/* Load in the data residue counter into the CSW */
		CommandStatus.DataTransferResidue = CommandBlock.DataTransferLength;

		/* Stall the selected data pipe if command failed (if data is still to be transferred) */
		if ((CommandStatus.Status == MS_SCSI_COMMAND_Fail) && (CommandStatus.DataTransferResidue))
		  Endpoint_StallTransaction();

		/* Return command status block to the host */
		ReturnCommandStatus();

		/* Indicate ready */
/*		LEDs_SetAllLEDs(LEDMASK_USB_READY);*/
	}

	/* Check if a Mass Storage Reset occurred */
	if (IsMassStoreReset)
	{
		/* Reset the data endpoint banks */
		Endpoint_ResetEndpoint(MASS_STORAGE_OUT_EPADDR);
		Endpoint_ResetEndpoint(MASS_STORAGE_IN_EPADDR);

		Endpoint_SelectEndpoint(MASS_STORAGE_OUT_EPADDR);
		Endpoint_ClearStall();
		Endpoint_ResetDataToggle();
		Endpoint_SelectEndpoint(MASS_STORAGE_IN_EPADDR);
		Endpoint_ClearStall();
		Endpoint_ResetDataToggle();

		/* Clear the abort transfer flag */
		IsMassStoreReset = false;
	}
}

/** Function to read in a command block from the host, via the bulk data OUT endpoint. This function reads in the next command block
 *  if one has been issued, and performs validation to ensure that the block command is valid.
 *
 *  \return Boolean \c true if a valid command block has been read in from the endpoint, \c false otherwise
 */
static bool ReadInCommandBlock(void)
{
	uint16_t BytesTransferred;

	/* Select the Data Out endpoint */
	Endpoint_SelectEndpoint(MASS_STORAGE_OUT_EPADDR);

	/* Abort if no command has been sent from the host */
	if (!(Endpoint_IsOUTReceived()))
	  return false;

	/* Read in command block header */
	BytesTransferred = 0;
	while (Endpoint_Read_Stream_LE(&CommandBlock, (sizeof(CommandBlock) - sizeof(CommandBlock.SCSICommandData)),
	                               &BytesTransferred) == ENDPOINT_RWSTREAM_IncompleteTransfer)
	{
		/* Check if the current command is being aborted by the host */
		if (IsMassStoreReset)
		  return false;
	}

	/* Verify the command block - abort if invalid */
	if ((CommandBlock.Signature         != MS_CBW_SIGNATURE) ||
	    (CommandBlock.LUN               >= TOTAL_LUNS)       ||
		(CommandBlock.Flags              & 0x1F)             ||
		(CommandBlock.SCSICommandLength == 0)                ||
		(CommandBlock.SCSICommandLength >  sizeof(CommandBlock.SCSICommandData)))
	{
		/* Stall both data pipes until reset by host */
		Endpoint_StallTransaction();
		Endpoint_SelectEndpoint(MASS_STORAGE_IN_EPADDR);
		Endpoint_StallTransaction();

		return false;
	}

	/* Read in command block command data */
	BytesTransferred = 0;
	while (Endpoint_Read_Stream_LE(&CommandBlock.SCSICommandData, CommandBlock.SCSICommandLength,
	                               &BytesTransferred) == ENDPOINT_RWSTREAM_IncompleteTransfer)
	{
		/* Check if the current command is being aborted by the host */
		if (IsMassStoreReset)
		  return false;
	}

	/* Finalize the stream transfer to send the last packet */
	Endpoint_ClearOUT();

	return true;
}

/** Returns the filled Command Status Wrapper back to the host via the bulk data IN endpoint, waiting for the host to clear any
 *  stalled data endpoints as needed.
 */
static void ReturnCommandStatus(void)
{
	uint16_t BytesTransferred;

	/* Select the Data Out endpoint */
	Endpoint_SelectEndpoint(MASS_STORAGE_OUT_EPADDR);

	/* While data pipe is stalled, wait until the host issues a control request to clear the stall */
	while (Endpoint_IsStalled())
	{
		/* Check if the current command is being aborted by the host */
		if (IsMassStoreReset)
		  return;
	}

	/* Select the Data In endpoint */
	Endpoint_SelectEndpoint(MASS_STORAGE_IN_EPADDR);

	/* While data pipe is stalled, wait until the host issues a control request to clear the stall */
	while (Endpoint_IsStalled())
	{
		/* Check if the current command is being aborted by the host */
		if (IsMassStoreReset)
		  return;
	}

	/* Write the CSW to the endpoint */
	BytesTransferred = 0;
	while (Endpoint_Write_Stream_LE(&CommandStatus, sizeof(CommandStatus),
	                                &BytesTransferred) == ENDPOINT_RWSTREAM_IncompleteTransfer)
	{
		/* Check if the current command is being aborted by the host */
		if (IsMassStoreReset)
		  return;
	}

	/* Finalize the stream transfer to send the last packet */
	Endpoint_ClearIN();
}

