#ifndef _COMMON_H_
#define _COMMON_H_

	// разряды порта B - SPI и управление экраном
	#define BIT_DC   (1 << 5)
	#define BIT_RES  (1 << 4)
	#define BIT_SCLK (1 << 1)
	#define BIT_MOSI (1 << 2)
	#define BIT_MISO (1 << 3)
	#define BIT_SS   (1 << 0)
	#define BIT_CS_SD (1 << 6)

	// разряды порта С - кнопки
	#define BT_1   (1 << 4)
	#define BT_2   (1 << 5)
	#define BT_3   (1 << 2)
	#define BT_4   (1 << 6)
	#define BT_5   (1 << 7)

/* =============================================================== */
/* MMC card commands                                               */
/* =============================================================== */
/* Standart card commands CMDx */
#define MMC_COMMANDS_BASE       (unsigned char) 0x40
#define MMC_GO_IDLE_STATE       MMC_COMMANDS_BASE + 0         // CMD0
#define MMC_SEND_OP_COND        MMC_COMMANDS_BASE + 1         // CMD1
#define MMC_SEND_IF_COND        MMC_COMMANDS_BASE + 8         // CMD8
#define MMC_SEND_CSD            MMC_COMMANDS_BASE + 9         // CMD9
#define MMC_SEND_CID            MMC_COMMANDS_BASE + 10        // CMD10
#define MMC_SEND_STATUS         MMC_COMMANDS_BASE + 13        // CMD13
#define MMC_SET_BLOCK_LEN       MMC_COMMANDS_BASE + 16        // CMD13
#define MMC_READ_SINGLE_BLOCK   MMC_COMMANDS_BASE + 17        // CMD17
#define MMC_WRITE_SINGLE_BLOCK  MMC_COMMANDS_BASE + 24        // CMD24
#define MMC_WRITE_MULTI_BLOCK   MMC_COMMANDS_BASE + 25        // CMD24

#define MMC_ERASE_WR_BLK_START  MMC_COMMANDS_BASE + 32        // CMD24
#define MMC_ERASE_WR_BLK_END 	MMC_COMMANDS_BASE + 33        // CMD24
#define MMC_ERASE				MMC_COMMANDS_BASE + 38        // CMD24

#define MMC_APP_CMD             MMC_COMMANDS_BASE + 55        // CMD55
#define MMC_READ_OCR            MMC_COMMANDS_BASE + 58        // CMD58
// Application specific command ACMDx
#define MMC_CMD_SD_STATUS       MMC_COMMANDS_BASE + 13        // ACMD13
#define MMC_CMD_SD_SEND_OP_COND MMC_COMMANDS_BASE + 41        // ACMD41
#define MMC_CMD_SD_SEND_SCR     MMC_COMMANDS_BASE + 51        // ACMD51

//=====================================================================================//
// MMC data tokens                                                                     //
//=====================================================================================//
#define MMC_START_TOKEN_SINGLE (unsigned char) 0xFE
#define MMC_START_TOKEN_MULTI  (unsigned char) 0xFC
#define MMC_STOP_TRAN_TOKEN    (unsigned char) 0xFD
//=====================================================================================//

enum ResponceType {R1, R2, R3, R7};

	/* variables */
		extern uint8_t data_PC;
		extern uint8_t data_device;
		extern uint8_t canDo;
		extern uint8_t data[128];
		extern uint8_t sdResponce[5];

	/* functions */
		extern void out8bit(uint8_t data8);
		extern void scrClear(void);
		extern bool SdReadDataBlock(uint32_t address, uint32_t size, uint8_t * buffer);
		extern bool SdWriteDataBlock(uint32_t address, uint32_t size, uint8_t * buffer);
		extern void SdOutByte(uint8_t data8);
		extern uint8_t SdInByte(void);
		extern void SdSendCommand(uint8_t Index, uint32_t Argument, uint8_t Crc, enum ResponceType responceType, uint8_t *Responce);
#endif // _COMMON_H_
