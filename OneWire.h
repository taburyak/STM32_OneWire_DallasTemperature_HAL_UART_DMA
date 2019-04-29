/*
 * OneWire.h
 *
 *  Created on: 18 квіт. 2019 р.
 *      Author: Andriy Honcharenko
 */

#ifndef ONEWIRE_H_
#define ONEWIRE_H_

#include "main.h"
#include <string.h>
#include <stdbool.h>

#define HUARTx				huart1
#define USARTx				USART1

extern UART_HandleTypeDef 	HUARTx;

// you can exclude onewire_search by defining that to 0
#ifndef ONEWIRE_SEARCH
#define ONEWIRE_SEARCH 1
#endif

// You can exclude CRC checks altogether by defining this to 0
#ifndef ONEWIRE_CRC
#define ONEWIRE_CRC 1
#endif

// Select the table-lookup method of computing the 8-bit CRC
// by setting this to 1.  The lookup table enlarges code size by
// about 250 bytes.  It does NOT consume RAM (but did in very
// old versions of OneWire).  If you disable this, a slower
// but very compact algorithm is used.
#ifndef ONEWIRE_CRC8_TABLE
#define ONEWIRE_CRC8_TABLE 1
#endif

// You can allow 16-bit CRC checks by defining this to 1
// (Note that ONEWIRE_CRC must also be 1.)
#ifndef ONEWIRE_CRC16
#define ONEWIRE_CRC16 1
#endif

#define OW_SEND_RESET		1
#define OW_NO_RESET			2

#define OW_OK				1
#define OW_ERROR			2
#define OW_NO_DEVICE		3

#define OW_NO_READ			0xff
#define OW_READ_SLOT		0xff

uint8_t OW_Init();
uint8_t OW_Reset(void);
uint8_t OW_Send(uint8_t sendReset, uint8_t *command, uint8_t cLen, uint8_t *data, uint8_t dLen, uint8_t readStart);
uint8_t OW_Search(uint8_t *buf, uint8_t num);
uint8_t OW_crc8(const uint8_t *addr, uint8_t len);
uint16_t OW_crc16(const uint8_t* input, uint16_t len, uint16_t crc);

#endif /* ONEWIRE_H_ */
