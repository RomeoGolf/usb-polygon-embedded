#include "fake_fs.h"

#define ATTR_READ 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOL_LABEL 0x08
#define ATTR_DIR 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LONG_FNAME 0x0F

#define BYTES_PER_SECTOR    512
#define BYTES_PER_SECT_SHIFT    9
#define SECTORS_PER_CLUSTER 8
#define SECTORS_PER_CLUST_SHIFT 3
#define BYTES_PER_CLUSTER   (BYTES_PER_SECTOR * SECTORS_PER_CLUSTER)
#define BYTES_PER_CLUST_SHIFT   (BYTES_PER_SECT_SHIFT + SECTORS_PER_CLUST_SHIFT)

/* должно быть кратно секторам на кластер */
#define SECTORS_PER_FAT	    8
#define MBR_SECTOR	    0
#define BOOT_SECTOR	    62
#define FAT1_SECTOR	    (BOOT_SECTOR + 1)
#define FAT2_SECTOR	    (FAT1_SECTOR + SECTORS_PER_FAT)
#define ROOT_SECTOR	    (FAT2_SECTOR + SECTORS_PER_FAT)
#define ROOT_CLUSTER	    (SECTORS_PER_FAT * 2 / SECTORS_PER_CLUSTER)
/* предполагается, что root-каталог занимает 1 сектор */

/*
 *
 * область данных и соответствующие ей 32-р. блоки фат:
 *
 * 0 кластер - фат1
 * 1 кластер - фат2
 * 2 кластер - корневой каталог
 * 3 кластер - первый файл
 *
 */

static bool first = true;

/* data "device -> PC" */
uint8_t * prepare_data(uint8_t * data_buf, uint32_t BlockAddress, uint8_t BytesInBlockDiv16);
/* data "PC -> device" */
void process_data(uint8_t * data_buf, uint32_t BlockAddress, uint8_t BytesInBlockDiv16);


uint8_t * read_mbr(uint8_t * data_buf, uint8_t BytesInBlockDiv16);
uint8_t * read_boot_sect(uint8_t * data_buf, uint8_t BytesInBlockDiv16);
uint8_t * read_fat(uint8_t * data_buf, uint32_t BlockAddress, uint8_t BytesInBlockDiv16);
uint8_t * read_data(uint8_t * data_buf, uint32_t BlockAddress, uint8_t BytesInBlockDiv16);

/** Writes blocks (OS blocks, not Dataflash pages) to the storage medium, the board Dataflash IC(s), from
 *  the pre-selected data OUT endpoint. This routine reads in OS sized blocks from the endpoint and writes
 *  them to the Dataflash in Dataflash page sized blocks.
 *
 *  \param[in] BlockAddress  Data block starting address for the write sequence
 *  \param[in] TotalBlocks   Number of blocks of data to write
 */
void WriteBlocks(uint32_t BlockAddress,
                                  uint16_t TotalBlocks)
{
	uint16_t CurrDFPage          = ((BlockAddress * VIRTUAL_MEMORY_BLOCK_SIZE) / DATAFLASH_PAGE_SIZE);
	uint16_t CurrDFPageByte      = ((BlockAddress * VIRTUAL_MEMORY_BLOCK_SIZE) % DATAFLASH_PAGE_SIZE);
	uint8_t  CurrDFPageByteDiv16 = (CurrDFPageByte >> 4);
	uint8_t  data_8[16];

	/* Wait until endpoint is ready before continuing */
	if (Endpoint_WaitUntilReady())
	  return;

	while (TotalBlocks)
	{
		uint8_t BytesInBlockDiv16 = 0;

		/* Write an endpoint packet sized data block to the Dataflash */
		while (BytesInBlockDiv16 < (VIRTUAL_MEMORY_BLOCK_SIZE >> 4))
		{
			/* Check if the endpoint is currently empty */
			if (!(Endpoint_IsReadWriteAllowed()))
			{
				/* Clear the current endpoint bank */
				Endpoint_ClearOUT();
				/* Wait until the host has sent another packet */
				if (Endpoint_WaitUntilReady())
				  return;
			}
			/* Check if end of Dataflash page reached */
			if (CurrDFPageByteDiv16 == (DATAFLASH_PAGE_SIZE >> 4))
			{
				/* Reset the Dataflash buffer counter, increment the page counter */
				CurrDFPageByteDiv16 = 0;
				CurrDFPage++;
			}

			/* Write one 16-byte chunk of data to the Dataflash */
			data_8[0] = Endpoint_Read_8();
			data_8[1] = Endpoint_Read_8();
			data_8[2] = Endpoint_Read_8();
			data_8[3] = Endpoint_Read_8();
			data_8[4] = Endpoint_Read_8();
			data_8[5] = Endpoint_Read_8();
			data_8[6] = Endpoint_Read_8();
			data_8[7] = Endpoint_Read_8();
			data_8[8] = Endpoint_Read_8();
			data_8[9] = Endpoint_Read_8();
			data_8[10] = Endpoint_Read_8();
			data_8[11] = Endpoint_Read_8();
			data_8[12] = Endpoint_Read_8();
			data_8[13] = Endpoint_Read_8();
			data_8[14] = Endpoint_Read_8();
			data_8[15] = Endpoint_Read_8();

			process_data(data_8, BlockAddress, BytesInBlockDiv16);

			/* Increment the Dataflash page 16 byte block counter */
			CurrDFPageByteDiv16++;

			/* Increment the block 16 byte block counter */
			BytesInBlockDiv16++;

			/* Check if the current command is being aborted by the host */
			if (IsMassStoreReset)
			  return;
		}

		/* Decrement the blocks remaining counter */
		TotalBlocks--;
		BlockAddress++;
	}

	/* If the endpoint is empty, clear it ready for the next packet from the host */
	if (!(Endpoint_IsReadWriteAllowed()))
	  Endpoint_ClearOUT();
}

/** Reads blocks (OS blocks, not Dataflash pages) from the storage medium, the board Dataflash IC(s), into
 *  the pre-selected data IN endpoint. This routine reads in Dataflash page sized blocks from the Dataflash
 *  and writes them in OS sized blocks to the endpoint.
 *
 *  \param[in] BlockAddress  Data block starting address for the read sequence
 *  \param[in] TotalBlocks   Number of blocks of data to read
 */
void ReadBlocks(uint32_t BlockAddress,
                                 uint16_t TotalBlocks)
{
	uint16_t CurrDFPage          = ((BlockAddress * VIRTUAL_MEMORY_BLOCK_SIZE) / DATAFLASH_PAGE_SIZE);
	uint16_t CurrDFPageByte      = ((BlockAddress * VIRTUAL_MEMORY_BLOCK_SIZE) % DATAFLASH_PAGE_SIZE);
	uint8_t  CurrDFPageByteDiv16 = (CurrDFPageByte >> 4);
	uint8_t  data_8[16];
	uint8_t  *pdata = data_8;

	/* Wait until endpoint is ready before continuing */
	if (Endpoint_WaitUntilReady())
	  return;

	while (TotalBlocks)
	{
		uint8_t BytesInBlockDiv16 = 0;

		/* Read an endpoint packet sized data block from the Dataflash */
		while (BytesInBlockDiv16 < (VIRTUAL_MEMORY_BLOCK_SIZE >> 4))
		{
			/* Check if the endpoint is currently full */
			if (!(Endpoint_IsReadWriteAllowed()))
			{
				/* Clear the endpoint bank to send its contents to the host */
				Endpoint_ClearIN();

				/* Wait until the endpoint is ready for more data */
				if (Endpoint_WaitUntilReady())
				  return;
			}

			/* Check if end of Dataflash page reached */
			if (CurrDFPageByteDiv16 == (DATAFLASH_PAGE_SIZE >> 4))
			{
				/* Reset the Dataflash buffer counter, increment the page counter */
				CurrDFPageByteDiv16 = 0;
				CurrDFPage++;
			}

			pdata = prepare_data(data_8, BlockAddress, BytesInBlockDiv16);

			/* Read one 16-byte chunk of data from the Dataflash */
			Endpoint_Write_8(pdata[0]);
			Endpoint_Write_8(pdata[1]);
			Endpoint_Write_8(pdata[2]);
			Endpoint_Write_8(pdata[3]);
			Endpoint_Write_8(pdata[4]);
			Endpoint_Write_8(pdata[5]);
			Endpoint_Write_8(pdata[6]);
			Endpoint_Write_8(pdata[7]);
			Endpoint_Write_8(pdata[8]);
			Endpoint_Write_8(pdata[9]);
			Endpoint_Write_8(pdata[10]);
			Endpoint_Write_8(pdata[11]);
			Endpoint_Write_8(pdata[12]);
			Endpoint_Write_8(pdata[13]);
			Endpoint_Write_8(pdata[14]);
			Endpoint_Write_8(pdata[15]);

			/* Increment the Dataflash page 16 byte block counter */
			CurrDFPageByteDiv16++;

			/* Increment the block 16 byte block counter */
			BytesInBlockDiv16++;

			/* Check if the current command is being aborted by the host */
			if (IsMassStoreReset)
			  return;
		}

		/* Decrement the blocks remaining counter */
		TotalBlocks--;
		BlockAddress++;
	}

	/* If the endpoint is full, send its contents to the host */
	if (!(Endpoint_IsReadWriteAllowed()))
	  Endpoint_ClearIN();
}

uint8_t * prepare_data(uint8_t * data_buf, uint32_t BlockAddress, uint8_t BytesInBlockDiv16){
    uint8_t *pdata;
    if (BlockAddress == MBR_SECTOR) {
		pdata = read_mbr(data_buf, BytesInBlockDiv16);
		return pdata;
    } else
    if (BlockAddress == BOOT_SECTOR) {
		pdata = read_boot_sect(data_buf, BytesInBlockDiv16);
		return pdata;
    } else
    if ((BlockAddress >= FAT1_SECTOR) && (BlockAddress < (FAT2_SECTOR))) {
		pdata = read_fat(data_buf, BlockAddress - FAT1_SECTOR, BytesInBlockDiv16);
		return pdata;
    } else
    if ((BlockAddress >= (FAT2_SECTOR)) && (BlockAddress < (ROOT_SECTOR))) {
		pdata = read_fat(data_buf, BlockAddress - FAT2_SECTOR, BytesInBlockDiv16);
		return pdata;
	} else
	if (BlockAddress >= (ROOT_SECTOR)) {
		pdata = read_data(data_buf, BlockAddress, BytesInBlockDiv16);
		return pdata;
    } else {
        memset(data_buf, 0, 16);
		return data_buf;
    };
}

void process_data(uint8_t * data_buf, uint32_t BlockAddress, uint8_t BytesInBlockDiv16){
	if (first) {
		first = false;
		data_PC = data_buf[0];
	}
}

uint8_t * read_mbr(uint8_t * data_buf, uint8_t BytesInBlockDiv16){
    memset(data_buf, 0, 16);	/* по умолчанию нули */
    if (BytesInBlockDiv16 == 28) {
	data_buf[2] = 0x0C; /* type FAT32 with LBA */
	data_buf[6] = BOOT_SECTOR; /* start LBA */
	data_buf[11] = 0x10; /* num of sect */
    }
    if (BytesInBlockDiv16 == 31) {     /* part 4, signature */
	data_buf[14] = 0x55;
	data_buf[15] = 0xAA;
    }
    return data_buf;
}

uint8_t * read_boot_sect(uint8_t * data_buf, uint8_t BytesInBlockDiv16){
    memset(data_buf, 0, 16);
    if (BytesInBlockDiv16 == 0) {
	data_buf[0] = 0xEB;
	data_buf[1] = 0x58;
	data_buf[2] = 0x90;
	data_buf[3] = 'M';
	data_buf[4] = 'S';
	data_buf[5] = 'D';
	data_buf[6] = 'O';
	data_buf[7] = 'S';
	data_buf[8] = '5';
	data_buf[9] = '.';
	data_buf[10] = '0';
	data_buf[12] = 0x02;
	data_buf[13] = 0x08;
	data_buf[14] = 0x01;
    }
    if (BytesInBlockDiv16 == 1) {
	data_buf[0] = 0x02;
	data_buf[5] = 0xF8;
	data_buf[8] = 63;
	data_buf[10] = 0xFF;
	data_buf[12] = 62;
    }
    if (BytesInBlockDiv16 == 2) {
	data_buf[1] = 0x10;
	data_buf[4] = 0x8;
	data_buf[12] = 2;
    }
    if (BytesInBlockDiv16 == 3) {
	data_buf[0] = 0x01;
	data_buf[2] = 0x06;
    }
    if (BytesInBlockDiv16 == 4) {
	data_buf[0] = 0x80;
	data_buf[2] = 0x29;
	/* datetime (vol id) ? */
	data_buf[3] = 148;
	data_buf[4] = 14;
	data_buf[5] = 13;
	data_buf[6] = 8;
	/* vol label */
	data_buf[7] = 'N';
	data_buf[8] = 'O';
	data_buf[9] = 0x20;
	data_buf[10] = 'N';
	data_buf[11] = 'A';
	data_buf[12] = 'M';
	data_buf[13] = 'E';
	data_buf[14] = ' ';
	data_buf[15] = ' ';
    }
    if (BytesInBlockDiv16 == 5) {
	/* vol label */
	data_buf[0] = ' ';
	data_buf[1] = ' ';
	/* fs type */
	data_buf[2] = 'F';
	data_buf[3] = 'A';
	data_buf[4] = 'T';
	data_buf[5] = '3';
	data_buf[6] = '2';
	data_buf[7] = ' ';
	data_buf[8] = ' ';
	data_buf[9] = ' ';
    }
    if (BytesInBlockDiv16 == 31) {     /* code, signature */
	data_buf[14] = 0x55;
	data_buf[15] = 0xAA;
    }
    return data_buf;
}

uint8_t * read_fat(uint8_t * data_buf, uint32_t BlockAddress, uint8_t BytesInBlockDiv16) {
    memset(data_buf, 0, 16);
    if ((BlockAddress == 0) && (BytesInBlockDiv16 == 0)) {
	/* first fat */
	data_buf[0] = 0xF8;
	data_buf[1] = 0xFF;
	data_buf[2] = 0xFF;
	data_buf[3] = 0x0F;
	/* second fat */
	data_buf[4] = 0xF8;
	data_buf[5] = 0xFF;
	data_buf[6] = 0xFF;
	data_buf[7] = 0x0F;
	/* root dir */
	data_buf[8] = 0xFF;
	data_buf[9] = 0xFF;
	data_buf[10] = 0xFF;
	data_buf[11] = 0xFF;
	/* file1 */
	data_buf[12] = 0x00;
	data_buf[13] = 0x00;
	data_buf[14] = 0x00;
	data_buf[15] = 0x00;
	}
    return data_buf;
}


uint8_t * read_data(uint8_t * data_buf, uint32_t BlockAddress, uint8_t BytesInBlockDiv16) {
    memset(data_buf, 0, 16);
    uint32_t size;
    if ((BlockAddress == ROOT_SECTOR) && (BytesInBlockDiv16 == 0)) {
	/* root, chunck 1 */
	data_buf[0] = 'L';
	data_buf[1] = 'U';
	data_buf[2] = 'F';
	data_buf[3] = 'A';

	data_buf[4] = '_';
	data_buf[5] = '1';
	data_buf[6] = ' ';
	data_buf[7] = ' ';

	data_buf[8] = ' ';
	data_buf[9] = ' ';
	data_buf[10] = ' ';
	data_buf[11] = 0x08;
    } else
    if ((BlockAddress == ROOT_SECTOR) && (BytesInBlockDiv16 == 1)) {
	/* root, chunck 2 */
	/* все нули */
    }
    return data_buf;
}

