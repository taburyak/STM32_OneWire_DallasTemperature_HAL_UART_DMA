/*
 * DallasTemperature.h
 *
 *  Created on: 21 квіт. 2019 р.
 *      Author: Andriy
 */

#ifndef DALLASTEMPERATURE_H_
#define DALLASTEMPERATURE_H_

#include <OneWire.h>

// Model IDs
#define DS18S20MODEL 	0x10  // also DS1820
#define DS18B20MODEL 	0x28
#define DS1822MODEL  	0x22
#define DS1825MODEL  	0x3B
#define DS28EA00MODEL 	0x42

// Error Codes
#define DEVICE_DISCONNECTED_C 	-127
#define DEVICE_DISCONNECTED_F 	-196.6
#define DEVICE_DISCONNECTED_RAW -7040

#define ONEWIRE_MAX_DEVICES	5
typedef uint8_t AllDeviceAddress[8 * ONEWIRE_MAX_DEVICES];
typedef uint8_t CurrentDeviceAddress[8];

#define max(a,b) (((a)>(b))?(a):(b))
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))

// initialise bus
void DT_Begin(void);

// returns the number of devices found on the bus
uint8_t DT_GetDeviceCount(void);

// returns the number of DS18xxx Family devices on bus
uint8_t DT_GetDS18Count(void);

// returns true if address is valid
bool DT_ValidAddress(const uint8_t*);

// returns true if address is of the family of sensors the lib supports.
bool DT_ValidFamily(const uint8_t* deviceAddress);

// finds an address at a given index on the bus
bool DT_GetAddress(uint8_t*, uint8_t);

// attempt to determine if the device at the given address is connected to the bus
bool DT_IsConnected(const uint8_t*);

// attempt to determine if the device at the given address is connected to the bus
// also allows for updating the read scratchpad
bool DT_IsConnected_ScratchPad(const uint8_t*, uint8_t*);

// read device's scratchpad
bool DT_ReadScratchPad(const uint8_t*, uint8_t*);

// write device's scratchpad
void DT_WriteScratchPad(const uint8_t*, const uint8_t*);

// read device's power requirements
bool DT_ReadPowerSupply(const uint8_t*);

// get global resolution
uint8_t DR_GetResolution();

// set global resolution to 9, 10, 11, or 12 bits
void DT_SetAllResolution(uint8_t newResolution);

// returns the device resolution: 9, 10, 11, or 12 bits
uint8_t DT_GetAllResolution(void);
uint8_t DT_GetResolution(const uint8_t* deviceAddress);

// set resolution of a device to 9, 10, 11, or 12 bits
bool DT_SetResolution(const uint8_t* deviceAddress, uint8_t newResolution, bool skipGlobalBitResolutionCalculation);

// sets/gets the waitForConversion flag
void DT_SetWaitForConversion(bool);
bool DT_GetWaitForConversion(void);

// sets/gets the checkForConversion flag
void DT_SetCheckForConversion(bool);
bool DT_GetCheckForConversion(void);

// sends command for all devices on the bus to perform a temperature conversion
void DT_RequestTemperatures(void);

// sends command for one device to perform a temperature conversion by address
bool DT_RequestTemperaturesByAddress(const uint8_t* deviceAddress);

// sends command for one device to perform a temperature conversion by index
bool DT_RequestTemperaturesByIndex(uint8_t);

// reads scratchpad and returns the raw temperature
int16_t DT_CalculateTemperature(const uint8_t*, uint8_t*);

// returns temperature raw value (12 bit integer of 1/128 degrees C)
int16_t DT_GetTemp(const uint8_t*);

// returns temperature in degrees C
float DT_GetTempC(const uint8_t*);

// returns temperature in degrees F
float DT_GetTempF(const uint8_t*);

// Get temperature for device index (slow)
float DT_GetTempCByIndex(uint8_t);

// Get temperature for device index (slow)
float DT_GetTempFByIndex(uint8_t);

// returns true if the bus requires parasite power
bool DT_IsParasitePowerMode(void);

// Is a conversion complete on the wire? Only applies to the first sensor on the wire.
bool DT_IsConversionComplete(void);

int16_t DT_MillisToWaitForConversion(uint8_t);

void DT_SetUserData(const uint8_t*, int16_t);
void DT_SetUserDataByIndex(uint8_t, int16_t);
int16_t DT_GetUserData(const uint8_t*);
int16_t DT_GetUserDataByIndex(uint8_t);

// convert from Celsius to Fahrenheit
float DT_ToFahrenheit(float celsius);

// convert from Fahrenheit to Celsius
float DT_ToCelsius(float fahrenheit);
#endif /* DALLASTEMPERATURE_H_ */
