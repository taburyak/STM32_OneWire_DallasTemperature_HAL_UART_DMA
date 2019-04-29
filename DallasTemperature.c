/*
 * DallasTemperature.c
 *
 *  Created on: 21 êâ³ò. 2019 ð.
 *      Author: Andriy
 */

#include "DallasTemperature.h"

// OneWire commands
#define STARTCONVO      0x44  // Tells device to take a temperature reading and put it on the scratchpad
#define COPYSCRATCH     0x48  // Copy EEPROM
#define READSCRATCH     0xBE  // Read EEPROM
#define WRITESCRATCH    0x4E  // Write to EEPROM
#define RECALLSCRATCH   0xB8  // Reload from last known
#define READPOWERSUPPLY 0xB4  // Determine if device needs parasite power
#define ALARMSEARCH     0xEC  // Query bus for devices with an alarm condition

// Scratchpad locations
#define TEMP_LSB        0
#define TEMP_MSB        1
#define HIGH_ALARM_TEMP 2
#define LOW_ALARM_TEMP  3
#define CONFIGURATION   4
#define INTERNAL_BYTE   5
#define COUNT_REMAIN    6
#define COUNT_PER_C     7
#define SCRATCHPAD_CRC  8

// Device resolution
#define TEMP_9_BIT  0x1F //  9 bit
#define TEMP_10_BIT 0x3F // 10 bit
#define TEMP_11_BIT 0x5F // 11 bit
#define TEMP_12_BIT 0x7F // 12 bit

#define NO_ALARM_HANDLER ((AlarmHandler *)0)

typedef uint8_t ScratchPad[9];

// parasite power on or off
bool parasite;

// used to determine the delay amount needed to allow for the
// temperature conversion to take place
uint8_t bitResolution;

// used to requestTemperature with or without delay
bool waitForConversion;

// used to requestTemperature to dynamically check if a conversion is complete
bool checkForConversion;

// count of devices on the bus
uint8_t devices;

// count of DS18xxx Family devices on bus
uint8_t ds18Count;

static void blockTillConversionComplete(uint8_t bitResolution);

// convert from raw to Celsius
static float DT_RawToCelsius(int16_t);

// convert from raw to Fahrenheit
static float DT_RawToFahrenheit(int16_t);

bool DT_ValidFamily(const uint8_t* deviceAddress)
{
	switch (deviceAddress[0])
	{
	case DS18S20MODEL:
	case DS18B20MODEL:
	case DS1822MODEL:
	case DS1825MODEL:
	case DS28EA00MODEL:
		return true;
	default:
		return false;
	}
}

// initialise the bus
void DT_Begin(void)
{

	AllDeviceAddress deviceAddress;

	devices = 0; 	// Reset the number of devices when we enumerate wire devices
	ds18Count = 0; 	// Reset number of DS18xxx Family devices

	devices = OW_Search(deviceAddress, ONEWIRE_MAX_DEVICES);

	for(uint8_t i = 0; i < devices; i++)
	{
		if (DT_ValidAddress(&deviceAddress[i * 8]))
		{

			if (!parasite && DT_ReadPowerSupply(&deviceAddress[i * 8]))
				parasite = true;

			bitResolution = max(bitResolution, DT_GetResolution(&deviceAddress[i * 8]));

			if (DT_ValidFamily(&deviceAddress[i * 8]))
			{
				ds18Count++;
			}
		}
	}
}

// returns the number of devices found on the bus
uint8_t DT_GetDeviceCount(void)
{
	return devices;
}

uint8_t DT_GetDS18Count(void)
{
	return ds18Count;
}

// returns true if address is valid
bool DT_ValidAddress(const uint8_t* deviceAddress)
{
	return (OW_crc8(deviceAddress, 7) == deviceAddress[7]);
}

// finds an address at a given index on the bus
// returns true if the device was found
bool DT_GetAddress(uint8_t* currentDeviceAddress, uint8_t index)
{
	AllDeviceAddress deviceAddress;

	uint8_t depth = 0;

	depth = OW_Search(deviceAddress, ONEWIRE_MAX_DEVICES);

	if(index < depth && DT_ValidAddress(&deviceAddress[index * 8]))
	{
		memcpy(currentDeviceAddress, &deviceAddress[index * 8], 8);
		return true;
	}

	return false;

}

// attempt to determine if the device at the given address is connected to the bus
bool DT_IsConnected(const uint8_t* deviceAddress)
{
	ScratchPad scratchPad;
	return DT_IsConnected_ScratchPad(deviceAddress, scratchPad);
}

// attempt to determine if the device at the given address is connected to the bus
// also allows for updating the read scratchpad
bool DT_IsConnected_ScratchPad(const uint8_t* deviceAddress, uint8_t* scratchPad)
{
	bool b = DT_ReadScratchPad(deviceAddress, scratchPad);
	return b /*&& (OW_crc8(scratchPad, 8) == scratchPad[SCRATCHPAD_CRC])*/;
}

bool DT_ReadScratchPad(const uint8_t* deviceAddress, uint8_t* scratchPad)
{
	// send the reset command and fail fast
	int b = OW_Reset();
	if (b == 0)
		return false;

	uint8_t query[18]={0x55, 0, 0, 0, 0, 0, 0, 0, 0, READSCRATCH, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	memcpy(&query[1], deviceAddress, 8);

	// Read all registers in a simple loop
	// byte 0: temperature LSB
	// byte 1: temperature MSB
	// byte 2: high alarm temp
	// byte 3: low alarm temp
	// byte 4: DS18S20: store for crc
	//         DS18B20 & DS1822: configuration register
	// byte 5: internal use & crc
	// byte 6: DS18S20: COUNT_REMAIN
	//         DS18B20 & DS1822: store for crc
	// byte 7: DS18S20: COUNT_PER_C
	//         DS18B20 & DS1822: store for crc
	// byte 8: SCRATCHPAD_CRC

	b = OW_Send(OW_SEND_RESET, query, 18, scratchPad, 8, 10);

	return (b == OW_OK);
}

void DT_WriteScratchPad(const uint8_t* deviceAddress, const uint8_t* scratchPad)
{


	uint8_t query[19]={0x55, 0, 0, 0, 0, 0, 0, 0, 0, WRITESCRATCH, scratchPad[HIGH_ALARM_TEMP], scratchPad[LOW_ALARM_TEMP], scratchPad[CONFIGURATION]};
	memcpy(&query[1], deviceAddress, 8);

	// DS1820 and DS18S20 have no configuration register
	if (deviceAddress[0] != DS18S20MODEL)
	{
		OW_Send(OW_SEND_RESET, query, 19, NULL, 0, OW_NO_READ);
	}
	else
	{
		OW_Send(OW_SEND_RESET, query, 18, NULL, 0, OW_NO_READ);
	}

	OW_Reset();

	// save the newly written values to eeprom
	query[9] = COPYSCRATCH;

	OW_Send(OW_SEND_RESET, query, 10, NULL, 0, OW_NO_READ);

	HAL_Delay(20); // <--- added 20ms delay to allow 10ms long EEPROM write operation (as specified by datasheet)

	OW_Reset();

}

bool DT_ReadPowerSupply(const uint8_t* deviceAddress)
{
	uint8_t ret = 0;

	uint8_t query[11]={0x55, 0, 0, 0, 0, 0, 0, 0, 0, READPOWERSUPPLY, 0xFF};
	memcpy(&query[1], deviceAddress, 8);
	OW_Send(OW_SEND_RESET, query, 10, &ret, 1, 10);
	OW_Reset();

	if (ret == 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

// set resolution of all devices to 9, 10, 11, or 12 bits
// if new resolution is out of range, it is constrained.
void DT_SetAllResolution(uint8_t newResolution)
{
	bitResolution = constrain(newResolution, 9, 12);
	AllDeviceAddress deviceAddress;

	for (int i = 0; i < devices; i++)
	{
		DT_GetAddress(deviceAddress, i);
		DT_SetResolution(deviceAddress, bitResolution, true);
	}
}

// set resolution of a device to 9, 10, 11, or 12 bits
// if new resolution is out of range, 9 bits is used.
bool DT_SetResolution(const uint8_t* deviceAddress, uint8_t newResolution, bool skipGlobalBitResolutionCalculation)
{

	// ensure same behavior as setResolution(uint8_t newResolution)
	newResolution = constrain(newResolution, 9, 12);

	// return when stored value == new value
	if (DT_GetResolution(deviceAddress) == newResolution)
		return true;

	ScratchPad scratchPad;
	if (DT_IsConnected_ScratchPad(deviceAddress, scratchPad))
	{
		// DS1820 and DS18S20 have no resolution configuration register
		if (deviceAddress[0] != DS18S20MODEL)
		{
			switch (newResolution)
			{
			case 12:
				scratchPad[CONFIGURATION] = TEMP_12_BIT;
				break;
			case 11:
				scratchPad[CONFIGURATION] = TEMP_11_BIT;
				break;
			case 10:
				scratchPad[CONFIGURATION] = TEMP_10_BIT;
				break;
			case 9:
			default:
				scratchPad[CONFIGURATION] = TEMP_9_BIT;
				break;
			}

			DT_WriteScratchPad(deviceAddress, scratchPad);

			// without calculation we can always set it to max
			bitResolution = max(bitResolution, newResolution);

			if (!skipGlobalBitResolutionCalculation && (bitResolution > newResolution))
			{
				bitResolution = newResolution;
				AllDeviceAddress deviceAddr;
				for (int i = 0; i < devices; i++)
				{
					DT_GetAddress(deviceAddr, i);
					bitResolution = max(bitResolution, DT_GetResolution(deviceAddr));
				}
			}
		}
		return true;  // new value set
	}
	return false;
}

// returns the global resolution
uint8_t DT_GetAllResolution()
{
	return bitResolution;
}

// returns the current resolution of the device, 9-12
// returns 0 if device not found
uint8_t DT_GetResolution(const uint8_t* deviceAddress)
{

	// DS1820 and DS18S20 have no resolution configuration register
	if (deviceAddress[0] == DS18S20MODEL)
		return 12;

	ScratchPad scratchPad;
	if (DT_IsConnected_ScratchPad(deviceAddress, scratchPad))
	{
		switch (scratchPad[CONFIGURATION])
		{
		case TEMP_12_BIT:
			return 12;

		case TEMP_11_BIT:
			return 11;

		case TEMP_10_BIT:
			return 10;

		case TEMP_9_BIT:
			return 9;
		}
	}
	return 0;
}

// sets the value of the waitForConversion flag
// TRUE : function requestTemperature() etc returns when conversion is ready
// FALSE: function requestTemperature() etc returns immediately (USE WITH CARE!!)
//        (1) programmer has to check if the needed delay has passed
//        (2) but the application can do meaningful things in that time
void DT_SetWaitForConversion(bool flag)
{
	waitForConversion = flag;
}

// gets the value of the waitForConversion flag
bool DT_GetWaitForConversion()
{
	return waitForConversion;
}

// sets the value of the checkForConversion flag
// TRUE : function requestTemperature() etc will 'listen' to an IC to determine whether a conversion is complete
// FALSE: function requestTemperature() etc will wait a set time (worst case scenario) for a conversion to complete
void DT_SetCheckForConversion(bool flag)
{
	checkForConversion = flag;
}

// gets the value of the waitForConversion flag
bool DT_GetCheckForConversion()
{
	return checkForConversion;
}

bool DT_IsConversionComplete()
{
	uint8_t b;
	OW_Send(OW_NO_RESET, (uint8_t *) OW_READ_SLOT, 0, &b, 1, 0);

	return (b == 1);
}

// sends command for all devices on the bus to perform a temperature conversion
void DT_RequestTemperatures()
{
	OW_Send(OW_SEND_RESET, (uint8_t *) "\xcc\x44", 2, (uint8_t *) NULL, 0, OW_NO_READ);

	// ASYNC mode?
	if (!waitForConversion)
		return;
	blockTillConversionComplete(bitResolution);

}

// sends command for one device to perform a temperature by address
// returns FALSE if device is disconnected
// returns TRUE  otherwise
bool DT_RequestTemperaturesByAddress(const uint8_t* deviceAddress)
{
	uint8_t bitResolution = DT_GetResolution(deviceAddress);

	if (bitResolution == 0)
	{
		return false; //Device disconnected
	}

	uint8_t query[10]={0x55, 0, 0, 0, 0, 0, 0, 0, 0, STARTCONVO};
	memcpy(&query[1], deviceAddress, 8);
	OW_Send(OW_SEND_RESET, query, 10, NULL, 0, OW_NO_READ);

	// ASYNC mode?
	if (!waitForConversion)
		return true;

	blockTillConversionComplete(bitResolution);

	return true;

}

// Continue to check if the IC has responded with a temperature
static void blockTillConversionComplete(uint8_t bitResolution)
{

	int delms = DT_MillisToWaitForConversion(bitResolution);
	if (checkForConversion && !parasite)
	{
		unsigned long now = HAL_GetTick();
		while (!DT_IsConversionComplete() && (HAL_GetTick() - delms < now))
		{
			__NOP();
		}
	}
	else
	{
		HAL_Delay(delms);
	}
}

// returns number of milliseconds to wait till conversion is complete (based on IC datasheet)
int16_t DT_MillisToWaitForConversion(uint8_t bitResolution)
{
	switch (bitResolution)
	{
	case 9:
		return 94;
	case 10:
		return 188;
	case 11:
		return 375;
	default:
		return 750;
	}
}

// sends command for one device to perform a temp conversion by index
bool DT_RequestTemperaturesByIndex(uint8_t deviceIndex)
{
	AllDeviceAddress deviceAddress;
	DT_GetAddress(deviceAddress, deviceIndex);

	return DT_RequestTemperaturesByAddress(deviceAddress);
}

// Fetch temperature for device index
float DT_GetTempCByIndex(uint8_t deviceIndex)
{
	AllDeviceAddress deviceAddress;
	if (!DT_GetAddress(deviceAddress, deviceIndex))
	{
		return DEVICE_DISCONNECTED_C;
	}

	return DT_GetTempC((uint8_t*) deviceAddress);
}

// Fetch temperature for device index
float DT_GetTempFByIndex(uint8_t deviceIndex)
{
	AllDeviceAddress deviceAddress;

	if (!DT_GetAddress(deviceAddress, deviceIndex))
	{
		return DEVICE_DISCONNECTED_F;
	}

	return DT_GetTempF((uint8_t*) deviceAddress);
}

// reads scratchpad and returns fixed-point temperature, scaling factor 2^-7
int16_t DT_CalculateTemperature(const uint8_t* deviceAddress, uint8_t* scratchPad)
{
	int16_t fpTemperature = (((int16_t) scratchPad[TEMP_MSB]) << 11) | (((int16_t) scratchPad[TEMP_LSB]) << 3);

	/*
	 DS1820 and DS18S20 have a 9-bit temperature register.

	 Resolutions greater than 9-bit can be calculated using the data from
	 the temperature, and COUNT REMAIN and COUNT PER °C registers in the
	 scratchpad.  The resolution of the calculation depends on the model.

	 While the COUNT PER °C register is hard-wired to 16 (10h) in a
	 DS18S20, it changes with temperature in DS1820.

	 After reading the scratchpad, the TEMP_READ value is obtained by
	 truncating the 0.5°C bit (bit 0) from the temperature data. The
	 extended resolution temperature can then be calculated using the
	 following equation:

	 COUNT_PER_C - COUNT_REMAIN
	 TEMPERATURE = TEMP_READ - 0.25 + --------------------------
	 COUNT_PER_C

	 Hagai Shatz simplified this to integer arithmetic for a 12 bits
	 value for a DS18S20, and James Cameron added legacy DS1820 support.

	 See - http://myarduinotoy.blogspot.co.uk/2013/02/12bit-result-from-ds18s20.html
	 */

	if (deviceAddress[0] == DS18S20MODEL)
	{
		fpTemperature = ((fpTemperature & 0xfff0) << 3) - 16 + (((scratchPad[COUNT_PER_C] - scratchPad[COUNT_REMAIN]) << 7) / scratchPad[COUNT_PER_C]);
	}

	return fpTemperature;
}

// returns temperature in 1/128 degrees C or DEVICE_DISCONNECTED_RAW if the
// device's scratch pad cannot be read successfully.
// the numeric value of DEVICE_DISCONNECTED_RAW is defined in
// DallasTemperature.h. It is a large negative number outside the
// operating range of the device
int16_t DT_GetTemp(const uint8_t* deviceAddress)
{
	ScratchPad scratchPad;
	if (DT_IsConnected_ScratchPad(deviceAddress, scratchPad))
		return DT_CalculateTemperature(deviceAddress, scratchPad);
	return DEVICE_DISCONNECTED_RAW;
}

// returns temperature in degrees C or DEVICE_DISCONNECTED_C if the
// device's scratch pad cannot be read successfully.
// the numeric value of DEVICE_DISCONNECTED_C is defined in
// DallasTemperature.h. It is a large negative number outside the
// operating range of the device
float DT_GetTempC(const uint8_t* deviceAddress)
{
	return DT_RawToCelsius(DT_GetTemp(deviceAddress));
}

// returns temperature in degrees F or DEVICE_DISCONNECTED_F if the
// device's scratch pad cannot be read successfully.
// the numeric value of DEVICE_DISCONNECTED_F is defined in
// DallasTemperature.h. It is a large negative number outside the
// operating range of the device
float DT_GetTempF(const uint8_t* deviceAddress)
{
	return DT_RawToFahrenheit(DT_GetTemp(deviceAddress));
}

// returns true if the bus requires parasite power
bool DT_IsParasitePowerMode(void)
{
	return parasite;
}

// IF alarm is not used one can store a 16 bit int of userdata in the alarm
// registers. E.g. an ID of the sensor.
// See github issue #29

// note if device is not connected it will fail writing the data.
void DT_SetUserData(const uint8_t* deviceAddress, int16_t data)
{
	// return when stored value == new value
	if (DT_GetUserData(deviceAddress) == data)
		return;

	ScratchPad scratchPad;
	if (DT_IsConnected_ScratchPad(deviceAddress, scratchPad))
	{
		scratchPad[HIGH_ALARM_TEMP] = data >> 8;
		scratchPad[LOW_ALARM_TEMP] = data & 255;
		DT_WriteScratchPad(deviceAddress, scratchPad);
	}
}

int16_t DT_GetUserData(const uint8_t* deviceAddress)
{
	int16_t data = 0;
	ScratchPad scratchPad;
	if (DT_IsConnected_ScratchPad(deviceAddress, scratchPad))
	{
		data = scratchPad[HIGH_ALARM_TEMP] << 8;
		data += scratchPad[LOW_ALARM_TEMP];
	}
	return data;
}

// note If address cannot be found no error will be reported.
int16_t DT_GetUserDataByIndex(uint8_t deviceIndex)
{
	AllDeviceAddress deviceAddress;
	DT_GetAddress(deviceAddress, deviceIndex);
	return DT_GetUserData((uint8_t*) deviceAddress);
}

void DT_SetUserDataByIndex(uint8_t deviceIndex, int16_t data)
{
	AllDeviceAddress deviceAddress;
	DT_GetAddress(deviceAddress, deviceIndex);
	DT_SetUserData((uint8_t*) deviceAddress, data);
}

// Convert float Celsius to Fahrenheit
float DT_ToFahrenheit(float celsius)
{
	return (celsius * 1.8) + 32;
}

// Convert float Fahrenheit to Celsius
float DT_ToCelsius(float fahrenheit)
{
	return (fahrenheit - 32) * 0.555555556;
}

// convert from raw to Celsius
float DT_RawToCelsius(int16_t raw)
{
	if (raw <= DEVICE_DISCONNECTED_RAW)
		return DEVICE_DISCONNECTED_C;
	// C = RAW/128
	return (float) raw * 0.0078125;
}

// convert from raw to Fahrenheit
float DT_RawToFahrenheit(int16_t raw)
{
	if (raw <= DEVICE_DISCONNECTED_RAW)
		return DEVICE_DISCONNECTED_F;
	// C = RAW/128
	// F = (C*1.8)+32 = (RAW/128*1.8)+32 = (RAW*0.0140625)+32
	return ((float) raw * 0.0140625) + 32;
}
