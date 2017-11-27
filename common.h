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

	/* variables */
		extern uint8_t data_PC;
		extern uint8_t data_device;
		extern uint8_t canDo;
		extern uint8_t data[128];
		extern void out8bit(uint8_t data8);
		extern void scrClear(void);
		extern bool SdReadDataBlock(uint32_t address, uint32_t size, uint8_t * buffer);
		extern bool SdWriteDataBlock(uint32_t address, uint32_t size, uint8_t * buffer);
#endif // _COMMON_H_
