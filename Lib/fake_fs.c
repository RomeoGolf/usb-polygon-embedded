#include "fake_fs.h"

/** Writes blocks (OS blocks, not Dataflash pages) to the storage medium, the board Dataflash IC(s), from
 *  the pre-selected data OUT endpoint. This routine reads in OS sized blocks from the endpoint and writes
 *  them to the Dataflash in Dataflash page sized blocks.
 *
 *  \param[in] BlockAddress  Data block starting address for the write sequence
 *  \param[in] TotalBlocks   Number of blocks of data to write
 */
void WriteBlocks(const uint32_t BlockAddress,
                                  uint16_t TotalBlocks)
{
	uint16_t CurrDFPage          = ((BlockAddress * VIRTUAL_MEMORY_BLOCK_SIZE) / DATAFLASH_PAGE_SIZE);
	uint16_t CurrDFPageByte      = ((BlockAddress * VIRTUAL_MEMORY_BLOCK_SIZE) % DATAFLASH_PAGE_SIZE);
	uint8_t  CurrDFPageByteDiv16 = (CurrDFPageByte >> 4);
	uint8_t  data_8[16];

	bool first = true;

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

			if (first) {
				first = false;
				data_PC = data_8[0];
			}

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
void ReadBlocks(const uint32_t BlockAddress,
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

			data_8[0] = data_device;

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
	}

	/* If the endpoint is full, send its contents to the host */
	if (!(Endpoint_IsReadWriteAllowed()))
	  Endpoint_ClearIN();
}
