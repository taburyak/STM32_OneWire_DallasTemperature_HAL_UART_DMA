/*
 * OneWire.c
 *
 *  Created on: Dec 16, 2020
 *      Author: Andriy Honcharenko
 */
#include "OneWire.h"

static HAL_StatusTypeDef OW_UART_Init(OneWire_HandleTypeDef* ow, uint32_t baudRate);
static void OW_ToBits(uint8_t owByte, uint8_t *owBits);
static uint8_t OW_ToByte(uint8_t *owBits);

#if ONEWIRE_SEARCH
static void OW_SendBits(OneWire_HandleTypeDef* ow, uint8_t num_bits);
#endif

static HAL_StatusTypeDef OW_UART_Init(OneWire_HandleTypeDef* ow, uint32_t baudRate)
{
	UART_HandleTypeDef* HUARTx = ow->huart;

	/*Configure GPIO pin : ONE_WIRE_Pin */
//	GPIO_InitTypeDef GPIO_InitStruct = {0};
//	GPIO_InitStruct.Pin = GPIO_PIN_10;
//	GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
//	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
//	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    HUARTx->Init.BaudRate = baudRate;
    HUARTx->Init.WordLength = UART_WORDLENGTH_8B;
    HUARTx->Init.StopBits = UART_STOPBITS_1;
    HUARTx->Init.Parity = UART_PARITY_NONE;
    HUARTx->Init.Mode = UART_MODE_TX_RX;
    HUARTx->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    HUARTx->Init.OverSampling = UART_OVERSAMPLING_16;
	return HAL_HalfDuplex_Init(HUARTx);
}

static void OW_ToBits(uint8_t owByte, uint8_t *owBits)
{
	uint8_t i;
	for (i = 0; i < 8; i++)
	{
		if (owByte & 0x01)
		{
			*owBits = OW_1;
		}
		else
		{
			*owBits = OW_0;
		}
		owBits++;
		owByte = owByte >> 1;
	}
}

static uint8_t OW_ToByte(uint8_t *owBits)
{
	uint8_t owByte, i;
	owByte = 0;
	for (i = 0; i < 8; i++)
	{
		owByte = owByte >> 1;
		if (*owBits == OW_R_1)
		{
			owByte |= 0x80;
		}
		owBits++;
	}

	return owByte;
}

HAL_StatusTypeDef OneWire(OneWire_HandleTypeDef* ow, UART_HandleTypeDef* huart)
{
	return OW_Begin(ow, huart);
}

HAL_StatusTypeDef OW_Begin(OneWire_HandleTypeDef* ow, UART_HandleTypeDef* huart)
{
	ow->huart = huart;
	HAL_StatusTypeDef status = OW_UART_Init(ow, 9600);
#if ONEWIRE_SEARCH
	OW_ResetSearch(ow);
#endif
	return status;
}

// Perform the onewire reset function.  We will wait up to 250uS for
// the bus to come high, if it doesn't then it is broken or shorted
// and we return a 0;
//
// Returns 1 if a device asserted a presence pulse, 0 otherwise.
//
uint8_t OW_Reset(OneWire_HandleTypeDef* ow)
{
	uint8_t owPresence = 0xf0;

	OW_UART_Init(ow, 9600);

	HAL_UART_Receive_DMA(ow->huart, &owPresence, 1);
	//HAL_UART_Transmit(ow->huart, &owPresence, 1, HAL_MAX_DELAY);
	HAL_UART_Transmit_DMA(ow->huart, &owPresence, 1);

	/*## Wait for the end of the transfer ###################################*/
	while (HAL_UART_GetState(ow->huart) != HAL_UART_STATE_READY)
	{
		__NOP();
	}

	OW_UART_Init(ow, 115200);

	if (owPresence != 0xf0)
	{
		return OW_OK;
	}

	return OW_NO_DEVICE;
}

//-----------------------------------------------------------------------------
// procedure bus communication 1-wire
// command - an array of bytes sent to the bus. If you need reading, we send OW_READ_SLOT
// cLen - the length of the command buffer, as many bytes will be sent to the bus
// data - if reading is required, then a reference to the buffer for reading
// dLen - the length of the read buffer. Read no more than this length
// readStart - which transmission character to start reading from (numbered from 0)
// you can specify OW_NO_READ, then you don't need to specify data and dLen
//-----------------------------------------------------------------------------
uint8_t OW_Send(OneWire_HandleTypeDef* ow, uint8_t *command, uint8_t cLen, uint8_t *data, uint8_t dLen, uint8_t readStart)
{
	if (OW_Reset(ow) == OW_NO_DEVICE)
	{
		return OW_NO_DEVICE;
	}

	while (cLen > 0)
	{

		OW_ToBits(*command, ow->ROM_NO);
		command++;
		cLen--;

		HAL_UART_Receive_DMA(ow->huart, ow->ROM_NO, sizeof(ow->ROM_NO) / sizeof(ow->ROM_NO[0]));
		HAL_UART_Transmit_DMA(ow->huart, ow->ROM_NO, sizeof(ow->ROM_NO) / sizeof(ow->ROM_NO[0]));

		while (HAL_UART_GetState(ow->huart) != HAL_UART_STATE_READY)
		{
			__NOP();
		}

		if (readStart == 0 && dLen > 0)
		{
			*data = OW_ToByte(ow->ROM_NO);
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
static void OW_SendBits(OneWire_HandleTypeDef* ow, uint8_t numBits)
{
	HAL_UART_Receive_DMA(ow->huart, ow->ROM_NO, numBits);
	HAL_UART_Transmit_DMA(ow->huart, ow->ROM_NO, numBits);

	while (HAL_UART_GetState(ow->huart) != HAL_UART_STATE_READY)
	{
		__NOP();
	}
}

//
// You need to use this function to start a search again from the beginning.
// You do not need to do it for the first search, though you could.
//
void OW_ResetSearch(OneWire_HandleTypeDef* ow)
{
  // reset the search state
  ow->LastDiscrepancy = 0;
  ow->LastDeviceFlag = false;
  ow->LastFamilyDiscrepancy = 0;
  for(int i = 7; ; i--)
  {
    ow->ROM_NO[i] = 0;
    if ( i == 0) break;
  }
}

// Setup the search to find the device type 'family_code' on the next call
// to search(*newAddr) if it is present.
//
void OW_TargetSearch(OneWire_HandleTypeDef* ow, uint8_t family_code)
{
   // set the search state to find SearchFamily type devices
   ow->ROM_NO[0] = family_code;
   for (uint8_t i = 1; i < 8; i++)
      ow->ROM_NO[i] = 0;
   ow->LastDiscrepancy = 64;
   ow->LastFamilyDiscrepancy = 0;
   ow->LastDeviceFlag = false;
}

uint8_t OW_Search(OneWire_HandleTypeDef* ow, uint8_t *buf, uint8_t num)
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

		OW_Send(ow, (uint8_t*)"\xf0", 1, NULL, 0, OW_NO_READ);

		for (numBit = 1; numBit <= 64; numBit++)
		{
			OW_ToBits(OW_READ_SLOT, ow->ROM_NO);
			OW_SendBits(ow, 2);

			if (ow->ROM_NO[0] == OW_R_1)
			{
				if (ow->ROM_NO[1] == OW_R_1)
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
				if (ow->ROM_NO[1] == OW_R_1)
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
				OW_ToBits(0x01, ow->ROM_NO);
			}
			else
			{
				curDevice[(numBit - 1) >> 3] &= ~(1 << ((numBit - 1) & 0x07));
				OW_ToBits(0x00, ow->ROM_NO);
			}

			OW_SendBits(ow, 1);
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
uint8_t OW_Crc8(const uint8_t *addr, uint8_t len)
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
uint8_t OW_Crc8(const uint8_t *addr, uint8_t len)
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
bool OW_CheckCrc16(const uint8_t* input, uint16_t len, const uint8_t* inverted_crc, uint16_t crc)
{
    crc = ~OW_Crc16(input, len, crc);
    return (crc & 0xFF) == inverted_crc[0] && (crc >> 8) == inverted_crc[1];
}

uint16_t OW_Crc16(const uint8_t* input, uint16_t len, uint16_t crc)
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
