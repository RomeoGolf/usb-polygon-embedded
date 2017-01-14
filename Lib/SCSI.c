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
 *  SCSI command processing routines, for SCSI commands issued by the host. Mass Storage
 *  devices use a thin "Bulk-Only Transport" protocol for issuing commands and status information,
 *  which wrap around standard SCSI device commands for controlling the actual storage medium.
 */

#define  INCLUDE_FROM_SCSI_C
#include "SCSI.h"

/** Structure to hold the SCSI response data to a SCSI INQUIRY command. This gives information about the device's
 *  features and capabilities.
 */
static const SCSI_Inquiry_Response_t InquiryData =
	{
		.DeviceType          = DEVICE_TYPE_BLOCK,
		.PeripheralQualifier = 0,

		.Removable           = true,

		.Version             = 0,

		.ResponseDataFormat  = 2,
		.NormACA             = false,
		.TrmTsk              = false,
		.AERC                = false,

		.AdditionalLength    = 0x1F,

		.SoftReset           = false,
		.CmdQue              = false,
		.Linked              = false,
		.Sync                = false,
		.WideBus16Bit        = false,
		.WideBus32Bit        = false,
		.RelAddr             = false,

		.VendorID            = "LUFA",
		.ProductID           = "Dataflash Disk",
		.RevisionID          = {'0','.','0','0'},
	};

/** Structure to hold the sense data for the last issued SCSI command, which is returned to the host after a SCSI REQUEST SENSE
 *  command is issued. This gives information on exactly why the last command failed to complete.
 */
static SCSI_Request_Sense_Response_t SenseData =
	{
		.ResponseCode        = 0x70,
		.AdditionalLength    = 0x0A,
	};


/** Main routine to process the SCSI command located in the Command Block Wrapper read from the host. This dispatches
 *  to the appropriate SCSI command handling routine if the issued command is supported by the device, else it returns
 *  a command failure due to a ILLEGAL REQUEST.
 *
 *  \return Boolean \c true if the command completed successfully, \c false otherwise
 */
bool SCSI_DecodeSCSICommand(void)
{
	bool CommandSuccess = false;

	/* Run the appropriate SCSI command hander function based on the passed command */
	switch (CommandBlock.SCSICommandData[0])
	{
		case SCSI_CMD_INQUIRY:
			CommandSuccess = SCSI_Command_Inquiry();
			break;
		case SCSI_CMD_REQUEST_SENSE:
			CommandSuccess = SCSI_Command_Request_Sense();
			break;
		case SCSI_CMD_READ_CAPACITY_10:
			CommandSuccess = SCSI_Command_Read_Capacity_10();
			break;
		case SCSI_CMD_SEND_DIAGNOSTIC:
			CommandSuccess = SCSI_Command_Send_Diagnostic();
			break;
		case SCSI_CMD_WRITE_10:
		case 0xC1:
			CommandSuccess = SCSI_Command_ReadWrite_10(DATA_WRITE);
			break;
		case SCSI_CMD_READ_10:
			CommandSuccess = SCSI_Command_ReadWrite_10(DATA_READ);
			break;
		case SCSI_CMD_MODE_SENSE_6:
			CommandSuccess = SCSI_Command_ModeSense_6();
			break;
		case SCSI_CMD_START_STOP_UNIT:
		case SCSI_CMD_TEST_UNIT_READY:
		case SCSI_CMD_PREVENT_ALLOW_MEDIUM_REMOVAL:
		case SCSI_CMD_VERIFY_10:
			/* These commands should just succeed, no handling required */
			CommandSuccess = true;
			CommandBlock.DataTransferLength = 0;
			break;
		default:
			/* Update the SENSE key to reflect the invalid command */
			SCSI_SET_SENSE(SCSI_SENSE_KEY_ILLEGAL_REQUEST,
		                   SCSI_ASENSE_INVALID_COMMAND,
		                   SCSI_ASENSEQ_NO_QUALIFIER);
			break;
	}

	/* Check if command was successfully processed */
	if (CommandSuccess)
	{
		SCSI_SET_SENSE(SCSI_SENSE_KEY_GOOD,
		               SCSI_ASENSE_NO_ADDITIONAL_INFORMATION,
		               SCSI_ASENSEQ_NO_QUALIFIER);

		return true;
	}

	return false;
}

/** Command processing for an issued SCSI INQUIRY command. This command returns information about the device's features
 *  and capabilities to the host.
 *
 *  \return Boolean \c true if the command completed successfully, \c false otherwise.
 */
static bool SCSI_Command_Inquiry(void)
{
	uint16_t AllocationLength  = SwapEndian_16(*(uint16_t*)&CommandBlock.SCSICommandData[3]);
	uint16_t BytesTransferred  = MIN(AllocationLength, sizeof(InquiryData));

	/* Only the standard INQUIRY data is supported, check if any optional INQUIRY bits set */
	if ((CommandBlock.SCSICommandData[1] & ((1 << 0) | (1 << 1))) ||
	     CommandBlock.SCSICommandData[2])
	{
		/* Optional but unsupported bits set - update the SENSE key and fail the request */
		SCSI_SET_SENSE(SCSI_SENSE_KEY_ILLEGAL_REQUEST,
		               SCSI_ASENSE_INVALID_FIELD_IN_CDB,
		               SCSI_ASENSEQ_NO_QUALIFIER);

		return false;
	}

	/* Write the INQUIRY data to the endpoint */
	Endpoint_Write_Stream_LE(&InquiryData, BytesTransferred, NULL);

	/* Pad out remaining bytes with 0x00 */
	Endpoint_Null_Stream((AllocationLength - BytesTransferred), NULL);

	/* Finalize the stream transfer to send the last packet */
	Endpoint_ClearIN();

	/* Succeed the command and update the bytes transferred counter */
	CommandBlock.DataTransferLength -= BytesTransferred;

	return true;
}

/** Command processing for an issued SCSI REQUEST SENSE command. This command returns information about the last issued command,
 *  including the error code and additional error information so that the host can determine why a command failed to complete.
 *
 *  \return Boolean \c true if the command completed successfully, \c false otherwise.
 */
static bool SCSI_Command_Request_Sense(void)
{
	uint8_t  AllocationLength = CommandBlock.SCSICommandData[4];
	uint8_t  BytesTransferred = MIN(AllocationLength, sizeof(SenseData));

	/* Send the SENSE data - this indicates to the host the status of the last command */
	Endpoint_Write_Stream_LE(&SenseData, BytesTransferred, NULL);

	/* Pad out remaining bytes with 0x00 */
	Endpoint_Null_Stream((AllocationLength - BytesTransferred), NULL);

	/* Finalize the stream transfer to send the last packet */
	Endpoint_ClearIN();

	/* Succeed the command and update the bytes transferred counter */
	CommandBlock.DataTransferLength -= BytesTransferred;

	return true;
}

/** Command processing for an issued SCSI READ CAPACITY (10) command. This command returns information about the device's capacity
 *  on the selected Logical Unit (drive), as a number of OS-sized blocks.
 *
 *  \return Boolean \c true if the command completed successfully, \c false otherwise.
 */
static bool SCSI_Command_Read_Capacity_10(void)
{
	/* Send the total number of logical blocks in the current LUN */
	Endpoint_Write_32_BE(LUN_MEDIA_BLOCKS - 1);

	/* Send the logical block size of the device (must be 512 bytes) */
	Endpoint_Write_32_BE(VIRTUAL_MEMORY_BLOCK_SIZE);

	/* Check if the current command is being aborted by the host */
	if (IsMassStoreReset)
	  return false;

	/* Send the endpoint data packet to the host */
	Endpoint_ClearIN();

	/* Succeed the command and update the bytes transferred counter */
	CommandBlock.DataTransferLength -= 8;

	return true;
}

/** Command processing for an issued SCSI SEND DIAGNOSTIC command. This command performs a quick check of the Dataflash ICs on the
 *  board, and indicates if they are present and functioning correctly. Only the Self-Test portion of the diagnostic command is
 *  supported.
 *
 *  \return Boolean \c true if the command completed successfully, \c false otherwise.
 */
static bool SCSI_Command_Send_Diagnostic(void)
{
	/* Check to see if the SELF TEST bit is not set */
	if (!(CommandBlock.SCSICommandData[1] & (1 << 2)))
	{
		/* Only self-test supported - update SENSE key and fail the command */
		SCSI_SET_SENSE(SCSI_SENSE_KEY_ILLEGAL_REQUEST,
		               SCSI_ASENSE_INVALID_FIELD_IN_CDB,
		               SCSI_ASENSEQ_NO_QUALIFIER);

		return false;
	}

	/* Check to see if all attached Dataflash ICs are functional */
/*	if (!(DataflashManager_CheckDataflashOperation()))*/
/*	{*/
		/* Update SENSE key with a hardware error condition and return command fail */
/*		SCSI_SET_SENSE(SCSI_SENSE_KEY_HARDWARE_ERROR,*/
/*		               SCSI_ASENSE_NO_ADDITIONAL_INFORMATION,*/
/*		               SCSI_ASENSEQ_NO_QUALIFIER);*/
/**/
/*		return false;*/
/*	}*/

	/* Succeed the command and update the bytes transferred counter */
	CommandBlock.DataTransferLength = 0;

	return true;
}

/** Command processing for an issued SCSI READ (10) or WRITE (10) command. This command reads in the block start address
 *  and total number of blocks to process, then calls the appropriate low-level Dataflash routine to handle the actual
 *  reading and writing of the data.
 *
 *  \param[in] IsDataRead  Indicates if the command is a READ (10) command or WRITE (10) command (DATA_READ or DATA_WRITE)
 *
 *  \return Boolean \c true if the command completed successfully, \c false otherwise.
 */
static bool SCSI_Command_ReadWrite_10(const bool IsDataRead)
{
	uint32_t BlockAddress;
	uint16_t TotalBlocks;

	/* Check if the disk is write protected or not */
	if ((IsDataRead == DATA_WRITE) && DISK_READ_ONLY)
	{
		/* Block address is invalid, update SENSE key and return command fail */
		SCSI_SET_SENSE(SCSI_SENSE_KEY_DATA_PROTECT,
		               SCSI_ASENSE_WRITE_PROTECTED,
		               SCSI_ASENSEQ_NO_QUALIFIER);

		return false;
	}

	BlockAddress = SwapEndian_32(*(uint32_t*)&CommandBlock.SCSICommandData[2]);
	TotalBlocks  = SwapEndian_16(*(uint16_t*)&CommandBlock.SCSICommandData[7]);

	/* Check if the block address is outside the maximum allowable value for the LUN */
	if (BlockAddress >= LUN_MEDIA_BLOCKS)
	{
		/* Block address is invalid, update SENSE key and return command fail */
		SCSI_SET_SENSE(SCSI_SENSE_KEY_ILLEGAL_REQUEST,
		               SCSI_ASENSE_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE,
		               SCSI_ASENSEQ_NO_QUALIFIER);

		return false;
	}

	#if (TOTAL_LUNS > 1)
	/* Adjust the given block address to the real media address based on the selected LUN */
	BlockAddress += ((uint32_t)CommandBlock.LUN * LUN_MEDIA_BLOCKS);
	#endif

	/* Determine if the packet is a READ (10) or WRITE (10) command, call appropriate function */
	if (IsDataRead == DATA_READ)
	  ReadBlocks(BlockAddress, TotalBlocks);
	else
	  WriteBlocks(BlockAddress, TotalBlocks);

	/* Update the bytes transferred counter and succeed the command */
	CommandBlock.DataTransferLength -= ((uint32_t)TotalBlocks * VIRTUAL_MEMORY_BLOCK_SIZE);

	return true;
}

/** Command processing for an issued SCSI MODE SENSE (6) command. This command returns various informational pages about
 *  the SCSI device, as well as the device's Write Protect status.
 *
 *  \return Boolean \c true if the command completed successfully, \c false otherwise.
 */
static bool SCSI_Command_ModeSense_6(void)
{
	/* Send an empty header response with the Write Protect flag status */
	Endpoint_Write_8(0x00);
	Endpoint_Write_8(0x00);
	Endpoint_Write_8(DISK_READ_ONLY ? 0x80 : 0x00);
	Endpoint_Write_8(0x00);
	Endpoint_ClearIN();

	/* Update the bytes transferred counter and succeed the command */
	CommandBlock.DataTransferLength -= 4;

	return true;
}

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
