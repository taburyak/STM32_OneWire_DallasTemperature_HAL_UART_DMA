/*
 * OneWire.h
 *
 *  Created on: Dec 16, 2020
 *      Author: Andriy Honcharenko
 */

#ifndef INC_ONEWIRE_H_
#define INC_ONEWIRE_H_

#include "main.h"
#include <stdbool.h>
#include <string.h>

// You can exclude certain features from OneWire.  In theory, this
// might save some space.  In practice, the compiler automatically
// removes unused code (technically, the linker, using -fdata-sections
// and -ffunction-sections when compiling, and Wl,--gc-sections
// when linking), so most of these will not result in any code size
// reduction.  Well, unless you try to use the missing features
// and redesign your program to not need them!  ONEWIRE_CRC8_TABLE
// is the exception, because it selects a fast but large algorithm
// or a small but slow algorithm.

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

#define OW_OK				1
#define OW_ERROR			2
#define OW_NO_DEVICE		3

#define OW_0				0x00
#define OW_1				0xff
#define OW_R_1				0xff

#define OW_SEND_RESET		1
#define OW_NO_RESET			2

#define OW_NO_READ			0xff
#define OW_READ_SLOT		0xff

typedef struct{
	UART_HandleTypeDef* huart;
	unsigned char ROM_NO[8];
	#if ONEWIRE_SEARCH
	// global search state
	uint8_t LastDiscrepancy;
	uint8_t LastFamilyDiscrepancy;
	bool LastDeviceFlag;
	#endif
}OneWire_HandleTypeDef;

HAL_StatusTypeDef OneWire(OneWire_HandleTypeDef* ow, UART_HandleTypeDef* huart);
HAL_StatusTypeDef OW_Begin(OneWire_HandleTypeDef* ow, UART_HandleTypeDef* huart);

// Perform a 1-Wire reset cycle. Returns 1 if a device responds
// with a presence pulse.  Returns 0 if there is no device or the
// bus is shorted or otherwise held low for more than 250uS
uint8_t OW_Reset(OneWire_HandleTypeDef* ow);
uint8_t OW_Send(OneWire_HandleTypeDef* ow, uint8_t *command, uint8_t cLen, uint8_t *data, uint8_t dLen, uint8_t readStart);

#if ONEWIRE_SEARCH
// Clear the search state so that if will start from the beginning again.
void OW_ResetSearch(OneWire_HandleTypeDef* ow);

// Setup the search to find the device type 'family_code' on the next call
// to search(*newAddr) if it is present.
void OW_TargetSearch(OneWire_HandleTypeDef* ow, uint8_t family_code);

// Look for the next device. Returns 1 if a new address has been
// returned. A zero might mean that the bus is shorted, there are
// no devices, or you have already retrieved all of them.  It
// might be a good idea to check the CRC to make sure you didn't
// get garbage.  The order is deterministic. You will always get
// the same devices in the same order.
uint8_t OW_Search(OneWire_HandleTypeDef* ow, uint8_t *buf, uint8_t num);
#endif

#if ONEWIRE_CRC
// Compute a Dallas Semiconductor 8 bit CRC, these are used in the
// ROM and scratchpad registers.
uint8_t OW_Crc8(const uint8_t *addr, uint8_t len);

#if ONEWIRE_CRC16
// Compute the 1-Wire CRC16 and compare it against the received CRC.
// Example usage (reading a DS2408):
//    // Put everything in a buffer so we can compute the CRC easily.
//    uint8_t buf[13];
//    buf[0] = 0xF0;    // Read PIO Registers
//    buf[1] = 0x88;    // LSB address
//    buf[2] = 0x00;    // MSB address
//    WriteBytes(net, buf, 3);    // Write 3 cmd bytes
//    ReadBytes(net, buf+3, 10);  // Read 6 data bytes, 2 0xFF, 2 CRC16
//    if (!CheckCRC16(buf, 11, &buf[11])) {
//        // Handle error.
//    }
//
// @param input - Array of bytes to checksum.
// @param len - How many bytes to use.
// @param inverted_crc - The two CRC16 bytes in the received data.
//                       This should just point into the received data,
//                       *not* at a 16-bit integer.
// @param crc - The crc starting value (optional)
// @return True, iff the CRC matches.
bool OW_CheckCrc16(const uint8_t* input, uint16_t len, const uint8_t* inverted_crc, uint16_t crc);

// Compute a Dallas Semiconductor 16 bit CRC.  This is required to check
// the integrity of data received from many 1-Wire devices.  Note that the
// CRC computed here is *not* what you'll get from the 1-Wire network,
// for two reasons:
//   1) The CRC is transmitted bitwise inverted.
//   2) Depending on the endian-ness of your processor, the binary
//      representation of the two-byte return value may have a different
//      byte order than the two bytes you get from 1-Wire.
// @param input - Array of bytes to checksum.
// @param len - How many bytes to use.
// @param crc - The crc starting value (optional)
// @return The CRC16, as defined by Dallas Semiconductor.
uint16_t OW_Crc16(const uint8_t* input, uint16_t len, uint16_t crc);
#endif
#endif

#endif /* INC_ONEWIRE_H_ */
