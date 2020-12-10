/*
 * OneWire.c
 *
 *  Created on: 18 ���. 2019 �.
 *      Author: Andriy Honcharenko
 */

#include <OneWire.h>

#define OW_0	0x00
#define OW_1	0xff
#define OW_R_1	0xff

uint8_t ow_buf[8];

static void OW_toBits(uint8_t ow_byte, uint8_t *ow_bits);
static uint8_t OW_toByte(uint8_t *ow_bits);
static HAL_StatusTypeDef OW_UART_Init(uint32_t baudRate);

/*----------------------------------------------------------*/
static void OW_toBits(uint8_t ow_byte, uint8_t *ow_bits)
{
	uint8_t i;
	for (i = 0; i < 8; i++)
	{
		if (ow_byte & 0x01)
		{
			*ow_bits = OW_1;
		}
		else
		{
			*ow_bits = OW_0;
		}
		ow_bits++;
		ow_byte = ow_byte >> 1;
	}
}

static uint8_t OW_toByte(uint8_t *ow_bits)
{
	uint8_t ow_byte, i;
	ow_byte = 0;
	for (i = 0; i < 8; i++)
	{
		ow_byte = ow_byte >> 1;
		if (*ow_bits == OW_R_1)
		{
			ow_byte |= 0x80;
		}
		ow_bits++;
	}

	return ow_byte;
}

static void OW_SendBits(uint8_t num_bits)
{
	HAL_UART_Receive_DMA(&HUARTx, ow_buf, num_bits);
	HAL_UART_Transmit_DMA(&HUARTx, ow_buf, num_bits);

	while (HAL_UART_GetState(&HUARTx) != HAL_UART_STATE_READY)
	{
		__NOP();
	}
}

static HAL_StatusTypeDef OW_UART_Init(uint32_t baudRate)
{
    HUARTx.Instance = USARTx;
    HUARTx.Init.BaudRate = baudRate;
    HUARTx.Init.WordLength = UART_WORDLENGTH_8B;
    HUARTx.Init.StopBits = UART_STOPBITS_1;
    HUARTx.Init.Parity = UART_PARITY_NONE;
    HUARTx.Init.Mode = UART_MODE_TX_RX;
    HUARTx.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    HUARTx.Init.OverSampling = UART_OVERSAMPLING_16;

	return HAL_HalfDuplex_Init(&HUARTx);
}

uint8_t OW_Reset(void)
{
	uint8_t ow_presence = 0xf0;

	OW_UART_Init(9600);

	HAL_UART_Transmit(&HUARTx, &ow_presence, 1, HAL_MAX_DELAY);
	HAL_UART_Receive_DMA(&HUARTx, &ow_presence, 1);

	/*## Wait for the end of the transfer ###################################*/
	while (HAL_UART_GetState(&HUARTx) != HAL_UART_STATE_READY)
	{
		__NOP();
	}

	OW_UART_Init(115200);

	if (ow_presence != 0xf0)
	{
		return OW_OK;
	}

	return OW_NO_DEVICE;
}

HAL_StatusTypeDef OW_Init(void)
{
	return OW_UART_Init(9600);
}

uint8_t OW_Send(uint8_t sendReset, uint8_t *command, uint8_t cLen, uint8_t *data, uint8_t dLen, uint8_t readStart)
{
	if (sendReset == OW_SEND_RESET)
	{
		if (OW_Reset() == OW_NO_DEVICE)
		{
			return OW_NO_DEVICE;
		}
	}

	while (cLen > 0)
	{

		OW_toBits(*command, ow_buf);
		command++;
		cLen--;

		HAL_UART_Transmit_DMA(&HUARTx, ow_buf, sizeof(ow_buf));
		HAL_UART_Receive_DMA(&HUARTx, ow_buf, sizeof(ow_buf));

		while (HAL_UART_GetState(&HUARTx) != HAL_UART_STATE_READY)
		{
			__NOP();
		}

		if (readStart == 0 && dLen > 0)
		{
			*data = OW_toByte(ow_buf);
			data++;
			dLen--;
		}
		else
		{
			if (readStart != OW_NO_READ)
			{
				readStart--;
			}
		}
	}

	return OW_OK;
}

#if ONEWIRE_SEARCH

uint8_t OW_Search(uint8_t *buf, uint8_t num)
{

	uint8_t found = 0;
	uint8_t *lastDevice = NULL;
	uint8_t *curDevice = buf;
	uint8_t numBit, lastCollision, currentCollision, currentSelection;

	lastCollision = 0;

	while (found < num)
	{
		numBit = 1;
		currentCollision = 0;

		OW_Send(OW_SEND_RESET, (uint8_t*)"\xf0", 1, NULL, 0, OW_NO_READ);

		for (numBit = 1; numBit <= 64; numBit++)
		{
			OW_toBits(OW_READ_SLOT, ow_buf);
			OW_SendBits(2);

			if (ow_buf[0] == OW_R_1)
			{
				if (ow_buf[1] == OW_R_1)
				{
					return found;
				}
				else
				{
					currentSelection = 1;
				}
			}
			else
			{
				if (ow_buf[1] == OW_R_1)
				{
					currentSelection = 0;
				}
				else
				{
					if (numBit < lastCollision)
					{
							if (lastDevice[(numBit - 1) >> 3] & 1 << ((numBit - 1) & 0x07))
							{
							currentSelection = 1;

								if (currentCollision < numBit)
								{
										currentCollision = numBit;
								}
							}
							else
							{
								currentSelection = 0;
							}
					}
					else
					{
						if (numBit == lastCollision)
						{
								currentSelection = 0;
						}
						else
						{
							currentSelection = 1;

							if (currentCollision < numBit)
							{
									currentCollision = numBit;
							}
						}
					}
				}
			}

			if (currentSelection == 1)
			{
				curDevice[(numBit - 1) >> 3] |= 1 << ((numBit - 1) & 0x07);
				OW_toBits(0x01, ow_buf);
			}
			else
			{
				curDevice[(numBit - 1) >> 3] &= ~(1 << ((numBit - 1) & 0x07));
				OW_toBits(0x00, ow_buf);
			}

			OW_SendBits(1);
		}

		found++;
		lastDevice = curDevice;
		curDevice += 8;
		if (currentCollision == 0)
		{
			return found;
		}

		lastCollision = currentCollision;
	}

        return found;
}

#endif

#if ONEWIRE_CRC
// The 1-Wire CRC scheme is described in Maxim Application Note 27:
// "Understanding and Using Cyclic Redundancy Checks with Maxim iButton Products"
//

#if ONEWIRE_CRC8_TABLE
// Dow-CRC using polynomial X^8 + X^5 + X^4 + X^0
// Tiny 2x16 entry CRC table created by Arjen Lentz
// See http://lentz.com.au/blog/calculating-crc-with-a-tiny-32-entry-lookup-table
static const uint8_t dscrc2x16_table[] = {
	0x00, 0x5E, 0xBC, 0xE2, 0x61, 0x3F, 0xDD, 0x83,
	0xC2, 0x9C, 0x7E, 0x20, 0xA3, 0xFD, 0x1F, 0x41,
	0x00, 0x9D, 0x23, 0xBE, 0x46, 0xDB, 0x65, 0xF8,
	0x8C, 0x11, 0xAF, 0x32, 0xCA, 0x57, 0xE9, 0x74
};

// Compute a Dallas Semiconductor 8 bit CRC. These show up in the ROM
// and the registers.  (Use tiny 2x16 entry CRC table)
uint8_t OW_crc8(const uint8_t *addr, uint8_t len)
{
	uint8_t crc = 0;

	while (len--)
	{
		crc = *addr++ ^ crc;  // just re-using crc as intermediate
		crc = dscrc2x16_table[crc & 0x0f] ^ dscrc2x16_table[16 + ((crc >> 4) & 0x0f)];
	}

	return crc;
}
#else
//
// Compute a Dallas Semiconductor 8 bit CRC directly.
// this is much slower, but a little smaller, than the lookup table.
//
uint8_t OW_crc8(const uint8_t *addr, uint8_t len)
{
	uint8_t crc = 0;

	while (len--)
	{
		uint8_t inbyte = *addr++;
		for (uint8_t i = 8; i; i--)
		{
			uint8_t mix = (crc ^ inbyte) & 0x01;
			crc >>= 1;
			if (mix) crc ^= 0x8C;
			inbyte >>= 1;
		}
	}
	return crc;
}
#endif

#if ONEWIRE_CRC16
bool OW_Check_crc16(const uint8_t* input, uint16_t len, const uint8_t* inverted_crc, uint16_t crc)
{
    crc = ~OW_crc16(input, len, crc);
    return (crc & 0xFF) == inverted_crc[0] && (crc >> 8) == inverted_crc[1];
}

uint16_t OW_crc16(const uint8_t* input, uint16_t len, uint16_t crc)
{
    static const uint8_t oddparity[16] = { 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 };

    for (uint16_t i = 0 ; i < len ; i++)
    {
      // Even though we're just copying a byte from the input,
      // we'll be doing 16-bit computation with it.
      uint16_t cdata = input[i];
      cdata = (cdata ^ crc) & 0xff;
      crc >>= 8;

      if (oddparity[cdata & 0x0F] ^ oddparity[cdata >> 4])
          crc ^= 0xC001;

      cdata <<= 6;
      crc ^= cdata;
      cdata <<= 1;
      crc ^= cdata;
    }

    return crc;
}
#endif

#endif
