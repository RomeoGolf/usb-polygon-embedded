
/*
  Copyright 2016 - 2018 RomeoGolf
  Copyright 2015  Dean Camera (dean [at] fourwalledcubicle [dot] com)

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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

uint8_t cnt = 0;            // просто счетчик
uint8_t data_PC = 0;
uint8_t data_device = 0;
uint8_t canDo = 0;
uint8_t data[128] = { 0 };
uint8_t isSpiOn = 0;

/* ---------------------------- */
/* for work with the SD card */

enum RegType {CID, CSD};
uint8_t sdResponce[5];

void SdOutByte(uint8_t data8)
{
    PORTB &= ~BIT_CS_SD;            // cs -> 0
    SPDR = data8;
    while (!(SPSR & (1 << SPIF))) ; // wait for transmit
    PORTB |= BIT_CS_SD;             // cs -> 1
}

uint8_t SdInByte(void)
{
    PORTB &= ~BIT_CS_SD;            // cs -> 0
    SPDR = 0xFF;
    while (!(SPSR & (1 << SPIF))) ; // wait for transmit
    PORTB |= BIT_CS_SD;             // cs -> 1
    return SPDR;
}

void SdSendCommand(uint8_t Index, uint32_t Argument, uint8_t Crc,
        enum ResponceType responceType, uint8_t *Responce)
{
    SdOutByte(0xFF);
    SdOutByte(Index);
    SdOutByte((uint8_t)(Argument >> 24));
    SdOutByte((uint8_t)(Argument >> 16));
    SdOutByte((uint8_t)(Argument >> 8));
    SdOutByte((uint8_t)(Argument >> 0));
    SdOutByte(Crc);

    uint8_t cntErr = 0xFF;
    do {
        Responce[0] = SdInByte();
    } while ((cntErr-- != 0) && ((Responce[0] & 0x80) != 0));

    switch(responceType) {
        case R1:
            return;
        case R2:
            Responce[1] = SdInByte();
            return;
        case R3:
        case R7:
            Responce[4] = SdInByte();
            Responce[3] = SdInByte();
            Responce[2] = SdInByte();
            Responce[1] = SdInByte();
            return;
    }
}

bool SdWaitForDataToken(void)
{
    uint8_t answer;
    uint8_t maxErrors = 0xFF;

    do {
        answer = SdInByte();
    } while ((maxErrors--)&&(answer != MMC_START_TOKEN_SINGLE));

    if (answer != MMC_START_TOKEN_SINGLE)
    {
        return false;
    } else {
        return true;
    }
}

bool SdReadReg(enum RegType regType, uint8_t *buffer)
{
    switch (regType) {
        case CID:
            SdSendCommand(MMC_SEND_CID, 0, 1, R1, sdResponce);
            break;
        case CSD:
            SdSendCommand(MMC_SEND_CSD, 0, 1, R1, sdResponce);
            break;
        default:
            return false;
    }
    if (sdResponce[0] == 0x00) {
        if (SdWaitForDataToken()) {
            for (uint8_t i = 0; i < 16; i++) {
                buffer[i] = SdInByte();
            }
            SdOutByte(0xFF);
            SdOutByte(0xFF);
            return true;
        } else {
            return false;
        }
  } else {
      return false;
  }
}

bool SdReadDataBlock(uint32_t address, uint32_t size, uint8_t *buffer)
{
    SdSendCommand(MMC_SET_BLOCK_LEN, size, 1, R1, sdResponce);
    SdSendCommand(MMC_READ_SINGLE_BLOCK, address, 1, R1, sdResponce);
    if (sdResponce[0] == 0x00) {
        if (SdWaitForDataToken()) {
            for (uint8_t i = 0; i < size; i++) {
                buffer[i] = SdInByte();
            }
            data_device = 1;
            return true;
        } else {
            data_device = 2;
            return false;
        }
  } else {
            data_device = 3;
      return false;
  }
}

bool SdWriteDataBlock(uint32_t address, uint32_t size, uint8_t *buffer)
{
    SdSendCommand(MMC_SET_BLOCK_LEN, size, 1, R1, sdResponce);
    SdSendCommand(MMC_WRITE_SINGLE_BLOCK, address, 1, R1, sdResponce);
    if (sdResponce[0] == 0x00) {
        SdOutByte(0xFF);
        SdOutByte(0xFF);
        SdOutByte(MMC_START_TOKEN_SINGLE);
        for (uint8_t i = 0; i < size; i++) {
            SdOutByte(buffer[i]);
        }
        SdOutByte(0xFF);    /* intstead CRC */
        SdOutByte(0xFF);
        SdInByte();         /* Data Responce */
        while(SdInByte() == 0x00);
        data_device = 1;
        return true;
  } else {
      data_device = 2;
      return false;
  }
}

/* ---------------------------- */


/* ---------------------------- */
/* for work with the LCD screen */
void out8bit(uint8_t data8)
{
    if (isSpiOn == 1) {
        PORTB &= ~BIT_SS;       // cs -> 0
        SPDR = data8;
        while (!(SPSR & (1 << SPIF))) ; // wait for transmit
        PORTB |= BIT_SS;            // cs -> 1
    } else {
        /** закрыто в отладочных целях, так и оставлено за ненадобностью.
        PORTB &= ~BIT_SS;       // cs -> 0
        for (uint8_t i = 0; i < 8; i++){
            PORTB &= ~BIT_SCLK;     // sclk -> 0
            if (data8 & 0x80) {
                PORTB |= BIT_MOSI;      // data -> 1
            } else {
                PORTB &= ~BIT_MOSI; // data -> 0
            }
            data8 <<= 1;
            PORTB |= BIT_SCLK;          // sclk -> 1
        }
        PORTB |= BIT_SS;            // cs -> 1
        **/
    }
}

void scrClear(void)
{
    PORTB &= ~BIT_DC;   // d/c -> 0
    out8bit(0x40); // Y = 0
    out8bit(0x80); // X = 0

    /* 96 * 65 ? 768 -> 864 */
    PORTB |= BIT_DC;        // d/c -> 1
    for(uint16_t i = 0; i < (96 * 9); i++) out8bit(0x00);
    PORTB &= ~BIT_DC;   // d/c -> 0
}
/* ---------------------------- */

/*****************************************************/
/*****************************************************/
int main(void)
{
    SetupHardware();

    PORTD = 0x00;    // начальное значение - все нули
    DDRD = 0xFF;     // все линии порта на вывод
    /* Port C : buttons */
    PORTC = 0x00;    // без "подтяжки" (есть внешняя)
    DDRC = 0x00;     // все линии порта на ввод

    /* SPI : LCD screen & SD card */
    DDRB = 0xFF & ~BIT_MISO;     // конфигурация порта на ввод/вывод
    PORTB = 0x00 | BIT_CS_SD;    // начальное значение

    unsigned char cnt_bt = 0;     // счетчик нажатий на кнопки
    unsigned char mode_out = 0;   // режим вывода
    unsigned char mode_out_old = 0;   // режим вывода
    unsigned char bt_now = 0;     // состояние кнопок
    unsigned char bt_old = 0;     // состояние кнопок в прошлый раз
                                // разряды кнопок сверху вниз:
                                // 4, 5, 2, 6, 7
                                // маски:
                                // 10, 20, 04, 40, 80
    int8_t cnt18 = 0;  // счетчик для циферблата
    int8_t cnt18old = -1;
    uint8_t canDoOld = 0;  // для определения момента включения

    /* Enable SPI, Master, set clock rate fck/2 (4 MHz) */
    SPCR = (1 << SPE) | (1 << MSTR) | (0 << SPR1) | (0 << SPR0);
    SPSR = (1 << SPI2X);
    isSpiOn = 1;

    // ********************************
    /* screen */

    /*PORTB |= BIT_RES;*/
    PORTB &= ~BIT_RES;
    _delay_ms(10);
    PORTB |= BIT_RES;

    // screen init
    PORTB &= ~BIT_DC;       // d/c -> 0
    out8bit(0x21);
    out8bit(0x80 | 29);
    out8bit(0x12);
    out8bit(0x20);
    out8bit(0x0c);

    scrClear();

    // line on the top
    PORTB &= ~BIT_DC;       // d/c -> 0
    out8bit(0x40);
    out8bit(0x80);
    PORTB |= BIT_DC;        // d/c -> 1
    for (uint8_t i = 0; i < 96; i++) {
        out8bit(0x02);
    }

    // line on the bottom
    PORTB &= ~BIT_DC;       // d/c -> 0
    out8bit(0x47);
    out8bit(0x80);
    PORTB |= BIT_DC;        // d/c -> 1
    for (uint8_t i = 0; i < 96; i++) {
        out8bit(0x80);
    }

    // position set
    PORTB &= ~BIT_DC;       // d/c -> 0
    out8bit(0x43);
    out8bit(0x80 | 0x28);

    // data out
    PORTB |= BIT_DC;        // d/c -> 1

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

    PORTB &= ~BIT_DC;       // d/c -> 0
    out8bit(0x44);
    out8bit(0x80 | 0x28);
    PORTB |= BIT_DC;        // d/c -> 1

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

    PORTB &= ~BIT_DC;   // d/c -> 0

    // ********************************
    /* SD card */

    /* > 74 clk to init */
    for (uint8_t i = 0; i < 10; i++) {
        SdOutByte(0xFF);
    }

    SdSendCommand(MMC_GO_IDLE_STATE, 0, 0x95, R1, sdResponce);
    PORTD = sdResponce[0];

    uint16_t cntErr = 0x7FF;

    if(sdResponce[0] == 0x01) {
        do {
            SdSendCommand(MMC_SEND_OP_COND, 0, 1, R1, sdResponce);
        } while ((sdResponce[0] != 0) && (cntErr-- != 0));

        PORTD = 0x1C;
        /*PORTD = sdResponce[0];*/

        if (sdResponce[0] != 0) {
            PORTD = 0x55;
        }
    } else {
        /* error! */
        PORTD = 0xAA;
    }

    // ********************************
    GlobalInterruptEnable();

    for(int i = 0; i < 128; i++) {
        data[i] = (i + 5);
    }

    /*SdSendCommand(MMC_SET_BLOCK_LEN, 128, 1, R1, sdResponce);*/

    //SdReadReg([>CID*/ CSD/*<], data);
    /*SdReadDataBlock(0, 128, data);*/

    /*SdSendCommand(MMC_ERASE_WR_BLK_START, 0x0, 1, R1, sdResponce);*/
    /*SdSendCommand(MMC_ERASE_WR_BLK_END, 0x800, 1, R1, sdResponce);*/
    /*SdSendCommand(MMC_ERASE, 0, 1, R1, sdResponce);*/

    /*SdSendCommand(MMC_SET_BLOCK_LEN, 128, 1, R1, sdResponce);*/
    /*SdWriteDataBlock(0x51b00, 128, data);*/
    /*SdWriteDataBlock(0x000, 122, data);*/

    /* запуск таймера 0 на период ~0.01 с */
    /* (защита от дребезга) */
    /* 1 тик = 0.000032 с ; 3 -> 0,000008; 2 -> 0,000001; 1 -> 0,000000125  */
    TCCR0B = 4;
    TCNT0 = 0;   /* 256 раз ~ 0.008192 c  */

    /* запуск таймера 1 на период 0.5 с */
    /* (счетчик с полусекундной задержкой) */
    TCCR1B = 4;              /* 1 тик = 0.000032 с */
    TCNT1 = 65536 - 15625;
    /* разрешение прерываний таймера 1*/
    /*TIMSK1 = (1 << TOIE1);*/

    uint8_t encoderState = 0;
    int8_t encoderCounter = 0;

    for (;;)
    {
        MassStorage_Task();
        USB_USBTask();

        /* проверка срабатывания таймера без прерываний по флагу */
        if ((TIFR1 & 1) == 1) {
            TCNT1 = 65536 - 15625;  /* перезапуск таймера 1 */
            TIFR1 = 1;              /* сброс флага таймера 1 */
            cnt++;      /* инкремент контрольного счетчика по таймеру */
        }

        /* обработка действий по срабатыванию таймера 0 */
        if ((TIFR0 & 1) == 1) {
            TCNT0 = 0;  /* перезапуск таймера 0 */
            TIFR0 = 1;  /* сброс флага срабатывания таймера 0 */

            /* cnt++;*/     // инкремент счетчика - чтобы что-то изменялось
            bt_now = PINC;                  // считывание порта с кнопками
            if (bt_now != bt_old) {         // если состояние порта изменилось
                // если нажаты сразу две верхние кнопки на разрядах 3 и 4
                if ((bt_now & (BT_1 | BT_2)) == 0) {
                    // циклически изменить режим отображения,
                    mode_out++;
                    // которых всего 4 - 0, 1, 2 и 3 (2 разряда по маске)
                    mode_out = mode_out & 3;
                } else {                    // в противном случае
                    // верхняя кнопка увеличивает счет нажатий
                    if ((bt_now & BT_1) == 0) {
                        cnt_bt++;
                        cnt18++;
                        if (cnt18 >= 18) {cnt18 = 0;}
                    }
                    if ((bt_now & BT_2) == 0) {  // а вторая сверху - уменьшает
                        cnt_bt--;
                        cnt18--;
                        if (cnt18 < 0) {cnt18 = 17;}
                    }
                }
                if ((bt_now & BT_3) == 0) {
                    canDo = canDo ^ 1;
                    PORTB |= (1 << 6);
                }

                /* for encoder */
                uint8_t nowEnc = (bt_now >> 6) & 0x03;
                encoderState <<= 2;
                encoderState |= nowEnc;
                encoderState &= 0x0F;

                switch (encoderState) {
                    case 0x00: /* - */
                        break;
                    case 0x01:
                        encoderCounter--;
                        break;
                    case 0x03: /* err */
                        break;
                    case 0x02:
                        encoderCounter++;
                        break;
                    case 0x06: /* err */
                        break;
                    case 0x07:
                        encoderCounter--;
                        break;
                    case 0x05: /* - */
                        break;
                    case 0x04:
                        encoderCounter++;
                        break;
                    case 0x0c: /* err */
                        break;
                    case 0x0d:
                        encoderCounter++;
                        break;
                    case 0x0f: /* - */
                        break;
                    case 0x0e:
                        encoderCounter--;
                        break;
                    case 0x0a: /* - */
                        break;
                    case 0x0b:
                        encoderCounter++;
                        break;
                    case 0x09: /* err */
                        break;
                    case 0x08:
                        encoderCounter--;
                        break;
                }
                if (encoderCounter >= (18 * 2)) {encoderCounter = 0;}
                if (encoderCounter < 0) {encoderCounter = (18 * 2 - 1);}
                data_device = encoderCounter >> 1;
                cnt18 = encoderCounter >> 1;

                // и сохраняем состояние порта для следующей проверки
                bt_old = bt_now;
            }

            /* TODO: нет защиты от многократного срабатывания */
            if (canDo == 1) {
                if (canDoOld == 0) {
                    canDoOld = 1;
                    mode_out_old = mode_out;
                    mode_out = 2;
                    scrClear();
                }
                if (cnt18 != cnt18old) {
                    cnt18old = cnt18;
                    uint32_t addr = cnt18 << 7;
                    uint32_t size = 2;
                    uint8_t forScreen[2];
                    bool canOut = false;
                    for (uint8_t i = 0; i < (128 / 2); i++) {
                        SdReadDataBlock(addr++, size, forScreen);
                        addr++;
                        switch (forScreen[0]) {
                            case 1:
                                PORTB &= ~BIT_DC;       // d/c -> 0
                                canOut = true;
                                break;
                            case 2:
                                PORTB |= BIT_DC;        // d/c -> 1
                                canOut = true;
                                break;
                            case 3:
                                canOut = false;
                                break;
                            case 4:
                                scrClear();
                                canOut = false;
                                break;
                            default:
                                ;
                        }
                        if (forScreen[0] == 3) {break;}
                        if (canOut) {out8bit(forScreen[1]);}
                    }
                }

            } else {
                canDoOld = 0;
                mode_out = mode_out_old;
            }

            switch (mode_out) {
                case 0 :
                    PORTD = cnt;    // просто счетчик
                    break;
                case 1 :
                    PORTD = data_device;
                    break;
                case 2 :
                    PORTD = cnt18;
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
    /* Start the PLL to multiply the 2MHz RC oscillator to 32MHz and
     * switch the CPU core to run from it */
    XMEGACLK_StartPLL(CLOCK_SRC_INT_RC2MHZ, 2000000, F_CPU);
    XMEGACLK_SetCPUClockSource(CLOCK_SRC_PLL);

    /* Start the 32MHz internal RC oscillator and start the DFLL to
     * increase it to 48MHz using the USB SOF as a reference */
    XMEGACLK_StartInternalOscillator(CLOCK_SRC_INT_RC32MHZ);
    XMEGACLK_StartDFLL(CLOCK_SRC_INT_RC32MHZ, DFLL_REF_INT_USBSOF, F_USB);

    PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
#endif

    /* Hardware Initialization */
    USB_Init();
    fakeFsInit();
}

/** Event handler for the USB_Connect event. This indicates that the
 * device is enumerating via the status LEDs. */
void EVENT_USB_Device_Connect(void)
{
    /* Reset the MSReset flag upon connection */
    IsMassStoreReset = false;
}

/** Event handler for the USB_Disconnect event. This indicates that
 * the device is no longer connected to a host via
 *  the status LEDs and stops the Mass Storage management task.
 */
void EVENT_USB_Device_Disconnect(void)
{
}

/** Event handler for the USB_ConfigurationChanged event. This is fired
 * when the host set the current configuration
 *  of the USB device after enumeration - the device endpoints are configured
 *  and the Mass Storage management task started.
 */
void EVENT_USB_Device_ConfigurationChanged(void)
{
    bool ConfigSuccess = true;

    /* Setup Mass Storage Data Endpoints */
    ConfigSuccess &= Endpoint_ConfigureEndpoint(MASS_STORAGE_IN_EPADDR,
            EP_TYPE_BULK, MASS_STORAGE_IO_EPSIZE, 1);
    ConfigSuccess &= Endpoint_ConfigureEndpoint(MASS_STORAGE_OUT_EPADDR,
            EP_TYPE_BULK, MASS_STORAGE_IO_EPSIZE, 1);
}

/** Event handler for the USB_ControlRequest event. This is used to catch
 * and process control requests sent to the device from the USB host before
 * passing along unhandled control requests to the library for processing
 *  internally.
 */
void EVENT_USB_Device_ControlRequest(void)
{
    /* Process UFI specific control requests */
    switch (USB_ControlRequest.bRequest)
    {
        case MS_REQ_MassStorageReset:
            if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE
                        | REQTYPE_CLASS
                        | REQREC_INTERFACE))
            {
                Endpoint_ClearSETUP();
                Endpoint_ClearStatusStage();

                /* Indicate that the current transfer should be aborted */
                IsMassStoreReset = true;
            }

            break;
        case MS_REQ_GetMaxLUN:
            if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST
                        | REQTYPE_CLASS
                        | REQREC_INTERFACE))
            {
                Endpoint_ClearSETUP();

                /* Indicate to the host the number of supported LUNs (virtual
                 * disks) on the device */
                Endpoint_Write_8(TOTAL_LUNS - 1);

                Endpoint_ClearIN();
                Endpoint_ClearStatusStage();
            }

            break;
    }
}

/** Task to manage the Mass Storage interface, reading in Command Block
 * Wrappers from the host, processing the SCSI commands they
 * contain, and returning Command Status Wrappers back to the host
 * to indicate the success or failure of the last issued command.
 */
void MassStorage_Task(void)
{
    /* Device must be connected and configured for the task to run */
    if (USB_DeviceState != DEVICE_STATE_Configured)
      return;

    /* Process sent command block from the host if one has been sent */
    if (ReadInCommandBlock())
    {
        /* Check direction of command, select Data IN endpoint if data
         * is from the device */
        if (CommandBlock.Flags & MS_COMMAND_DIR_DATA_IN)
          Endpoint_SelectEndpoint(MASS_STORAGE_IN_EPADDR);

        /* Decode the received SCSI command, set returned status code */
        CommandStatus.Status = SCSI_DecodeSCSICommand() ? MS_SCSI_COMMAND_Pass
            : MS_SCSI_COMMAND_Fail;

        /* Load in the CBW tag into the CSW to link them together */
        CommandStatus.Tag = CommandBlock.Tag;

        /* Load in the data residue counter into the CSW */
        CommandStatus.DataTransferResidue = CommandBlock.DataTransferLength;

        /* Stall the selected data pipe if command failed (if data is still
         * to be transferred) */
        if ((CommandStatus.Status == MS_SCSI_COMMAND_Fail)
         && (CommandStatus.DataTransferResidue))
          Endpoint_StallTransaction();

        /* Return command status block to the host */
        ReturnCommandStatus();

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

/** Function to read in a command block from the host, via the bulk data
 * OUT endpoint. This function reads in the next command block
 * if one has been issued, and performs validation to ensure that
 * the block command is valid.
 *
 *  \return Boolean \c true if a valid command block has been read in
 *  from the endpoint, \c false otherwise
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
    while (Endpoint_Read_Stream_LE(&CommandBlock, (sizeof(CommandBlock)
                    - sizeof(CommandBlock.SCSICommandData)), &BytesTransferred)
            == ENDPOINT_RWSTREAM_IncompleteTransfer)
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
    while (Endpoint_Read_Stream_LE(&CommandBlock.SCSICommandData,
                CommandBlock.SCSICommandLength, &BytesTransferred)
            == ENDPOINT_RWSTREAM_IncompleteTransfer)
    {
        /* Check if the current command is being aborted by the host */
        if (IsMassStoreReset)
          return false;
    }

    /* Finalize the stream transfer to send the last packet */
    Endpoint_ClearOUT();

    return true;
}

/** Returns the filled Command Status Wrapper back to the host via
 * the bulk data IN endpoint, waiting for the host to clear any
 * stalled data endpoints as needed.
 */
static void ReturnCommandStatus(void)
{
    uint16_t BytesTransferred;

    /* Select the Data Out endpoint */
    Endpoint_SelectEndpoint(MASS_STORAGE_OUT_EPADDR);

    /* While data pipe is stalled, wait until the host issues a control
     * request to clear the stall */
    while (Endpoint_IsStalled())
    {
        /* Check if the current command is being aborted by the host */
        if (IsMassStoreReset)
          return;
    }

    /* Select the Data In endpoint */
    Endpoint_SelectEndpoint(MASS_STORAGE_IN_EPADDR);

    /* While data pipe is stalled, wait until the host issues a control
     * request to clear the stall */
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

