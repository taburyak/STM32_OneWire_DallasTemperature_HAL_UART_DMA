/*
 * DallasTemperature.c
 *
 *  Created on: Dec 24, 2020
 *      Author: tabur
 */
#include "DallasTemperature.h"

// OneWire commands
#define STARTCONVO      0x44  // Tells device to take a temperature reading and put it on the scratchpad
#define COPYSCRATCH     0x48  // Copy scratchpad to EEPROM
#define READSCRATCH     0xBE  // Read from scratchpad
#define WRITESCRATCH    0x4E  // Write to scratchpad
#define RECALLSCRATCH   0xB8  // Recall from EEPROM to scratchpad
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

// DSROM FIELDS
#define DSROM_FAMILY    0
#define DSROM_CRC       7

// Device resolution
#define TEMP_9_BIT  0x1F //  9 bit
#define TEMP_10_BIT 0x3F // 10 bit
#define TEMP_11_BIT 0x5F // 11 bit
#define TEMP_12_BIT 0x7F // 12 bit

#define MAX_CONVERSION_TIMEOUT		750

static void BlockTillConversionComplete(DallasTemperature_HandleTypeDef* dt, uint8_t bitResolution);
static void ActivateExternalPullup(DallasTemperature_HandleTypeDef* dt);
static void DeactivateExternalPullup(DallasTemperature_HandleTypeDef* dt);
//static bool IsAllZeros(const uint8_t * const scratchPad, const size_t length);

// Continue to check if the IC has responded with a temperature
static void BlockTillConversionComplete(DallasTemperature_HandleTypeDef* dt, uint8_t bitResolution)
{
	int delms = DT_MillisToWaitForConversion(bitResolution);

	if (dt->checkForConversion && !dt->parasite)
	{
		unsigned long now = HAL_GetTick();
		while (!DT_IsConversionComplete(dt) && (HAL_GetTick() - delms < now))
		{
			__NOP();
		}
	}
	else
	{
		ActivateExternalPullup(dt);
		HAL_Delay(delms);
		DeactivateExternalPullup(dt);
	}
}

static void ActivateExternalPullup(DallasTemperature_HandleTypeDef* dt)
{
	if(dt->useExternalPullup)
	{
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, GPIO_PIN_RESET);
	}
}

static void DeactivateExternalPullup(DallasTemperature_HandleTypeDef* dt)
{
	if(dt->useExternalPullup)
	{
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_10, GPIO_PIN_SET);
	}
}

// Returns true if all bytes of scratchPad are '\0'
//static bool IsAllZeros(const uint8_t * const scratchPad, const size_t length)
//{
//	for (size_t i = 0; i < length; i++)
//	{
//		if (scratchPad[i] != 0)
//		{
//			return false;
//		}
//	}
//
//	return true;
//}

void DT_SetPullupPin(DallasTemperature_HandleTypeDef* dt, GPIO_TypeDef* port, uint32_t pin)
{
	dt->useExternalPullup = true;

	/*Configure GPIO pin : ONE_WIRE_Pin */
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
	HAL_GPIO_Init(port, &GPIO_InitStruct);

	DeactivateExternalPullup(dt);
}

void DT_SetOneWire(DallasTemperature_HandleTypeDef* dt, OneWire_HandleTypeDef* ow)
{
	dt->ow 					= ow;
	dt->devices 			= 0;
	dt->ds18Count 			= 0;
	dt->parasite 			= false;
	dt->bitResolution 		= 9;
	dt->waitForConversion 	= true;
	dt->checkForConversion 	= true;
	dt->autoSaveScratchPad 	= true;
	dt->useExternalPullup 	= false;
}

void DT_Begin(DallasTemperature_HandleTypeDef* dt)
{
	AllDeviceAddress deviceAddress;

	OW_ResetSearch(dt->ow);
	dt->devices = 0; 	// Reset the number of devices when we enumerate wire devices
	dt->ds18Count = 0; 	// Reset number of DS18xxx Family devices

	dt->devices = OW_Search(dt->ow, deviceAddress, ONEWIRE_MAX_DEVICES);

	for(uint8_t i = 0; i < dt->devices; i++)
	{
		if (DT_ValidAddress(&deviceAddress[i * 8]))
		{

			if (!dt->parasite && DT_ReadPowerSupply(dt, &deviceAddress[i * 8]))
				dt->parasite = true;

			dt->bitResolution = max(dt->bitResolution, DT_GetResolution(dt, &deviceAddress[i * 8]));

			if (DT_ValidFamily(&deviceAddress[i * 8]))
			{
				dt->ds18Count++;
			}
		}
	}
}

// returns the number of devices found on the bus
uint8_t DT_GetDeviceCount(DallasTemperature_HandleTypeDef* dt)
{
	return dt->devices;
}

uint8_t DT_GetDS18Count(DallasTemperature_HandleTypeDef* dt)
{
	return dt->ds18Count;
}

// returns true if address is valid
bool DT_ValidAddress(const uint8_t* deviceAddress)
{
	return (OW_Crc8(deviceAddress, 7) == deviceAddress[7]);
}

bool DT_ValidFamily(const uint8_t* deviceAddress)
{
	switch (deviceAddress[DSROM_FAMILY])
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

// finds an address at a given index on the bus
// returns true if the device was found
bool DT_GetAddress(DallasTemperature_HandleTypeDef* dt, uint8_t* currentDeviceAddress, uint8_t index)
{
	AllDeviceAddress deviceAddress;

	uint8_t depth = 0;

	depth = OW_Search(dt->ow, deviceAddress, ONEWIRE_MAX_DEVICES);

	if(index < depth && DT_ValidAddress(&deviceAddress[index * 8]))
	{
		memcpy(currentDeviceAddress, &deviceAddress[index * 8], 8);
		return true;
	}

	return false;
}

// attempt to determine if the device at the given address is connected to the bus
bool DT_IsConnected(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress)
{
	ScratchPad scratchPad;
	return DT_IsConnected_ScratchPad(dt, deviceAddress, scratchPad);
}

// attempt to determine if the device at the given address is connected to the bus
// also allows for updating the read scratchpad
bool DT_IsConnected_ScratchPad(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress, uint8_t* scratchPad)
{
	bool b = DT_ReadScratchPad(dt, deviceAddress, scratchPad);
	return (b /*&& IsAllZeros(scratchPad, 8)*/ && (OW_Crc8(scratchPad, 8) == scratchPad[SCRATCHPAD_CRC]));
}

bool DT_ReadScratchPad(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress, uint8_t* scratchPad)
{
	// send the reset command and fail fast
	int b = OW_Reset(dt->ow);

	if (b == 0)
		return false;

	uint8_t query[19]={0x55, 0, 0, 0, 0, 0, 0, 0, 0, READSCRATCH, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
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

	b = OW_Send(dt->ow, query, 19, scratchPad, 9, 10);

	return (b == OW_OK);
}

void DT_WriteScratchPad(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress, const uint8_t* scratchPad)
{
	uint8_t query[13]={0x55, 0, 0, 0, 0, 0, 0, 0, 0, WRITESCRATCH, scratchPad[HIGH_ALARM_TEMP], scratchPad[LOW_ALARM_TEMP], scratchPad[CONFIGURATION]};
	memcpy(&query[1], deviceAddress, 8);

	// DS1820 and DS18S20 have no configuration register
	if (deviceAddress[DSROM_FAMILY] != DS18S20MODEL)
	{
		OW_Send(dt->ow, query, 13, NULL, 0, OW_NO_READ);
	}
	else
	{
		OW_Send(dt->ow, query, 12, NULL, 0, OW_NO_READ);
	}

	if (dt->autoSaveScratchPad)
	{
		DT_SaveScratchPad(dt, deviceAddress);
	}
	else
	{
		OW_Reset(dt->ow);
	}
}

// returns true if parasite mode is used (2 wire)
// returns false if normal mode is used (3 wire)
// if no address is given (or nullptr) it checks if any device on the bus
// uses parasite mode.
bool DT_ReadPowerSupply(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress)
{
	uint8_t parasiteMode = 0;

	OW_Reset(dt->ow);

	uint8_t query[11]={0x55, 0, 0, 0, 0, 0, 0, 0, 0, READPOWERSUPPLY, 0xFF};

	if (deviceAddress == NULL)
	{
	  query[0] = 0xCC;
	  query[1] = READPOWERSUPPLY;
	  OW_Send(dt->ow, query, 3, &parasiteMode, 1, 2);
	}
	else
	{
	  query[0] = 0x55;
	  memcpy(&query[1], deviceAddress, 8);
	  OW_Send(dt->ow, query, 10, &parasiteMode, 1, 10);
	}

	OW_Reset(dt->ow);

	if (parasiteMode == 0)
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
void DT_SetAllResolution(DallasTemperature_HandleTypeDef* dt, uint8_t newResolution)
{
	dt->bitResolution = constrain(newResolution, 9, 12);
	CurrentDeviceAddress deviceAddress;

	for (int i = 0; i < dt->devices; i++)
	{
		DT_GetAddress(dt, deviceAddress, i);
		DT_SetResolution(dt, deviceAddress, dt->bitResolution, true);
	}
}

// set resolution of a device to 9, 10, 11, or 12 bits
// if new resolution is out of range, 9 bits is used.
bool DT_SetResolution(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress, uint8_t newResolution, bool skipGlobalBitResolutionCalculation)
{
	// ensure same behavior as setResolution(uint8_t newResolution)
	newResolution = constrain(newResolution, 9, 12);

	// return when stored value == new value
	if (DT_GetResolution(dt, deviceAddress) == newResolution)
		return true;

	ScratchPad scratchPad;

	if (DT_IsConnected_ScratchPad(dt, deviceAddress, scratchPad))
	{
		// DS1820 and DS18S20 have no resolution configuration register
		if (deviceAddress[DSROM_FAMILY] != DS18S20MODEL)
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

			DT_WriteScratchPad(dt, deviceAddress, scratchPad);

			// without calculation we can always set it to max
			dt->bitResolution = max(dt->bitResolution, newResolution);

			if (!skipGlobalBitResolutionCalculation && (dt->bitResolution > newResolution))
			{
				dt->bitResolution = newResolution;

				CurrentDeviceAddress deviceAddr;

				for (int i = 0; i < dt->devices; i++)
				{
					DT_GetAddress(dt, deviceAddr, i);
					dt->bitResolution = max(dt->bitResolution, DT_GetResolution(dt, deviceAddr));
				}
			}
		}
		return true;  // new value set
	}
	return false;
}

// returns the global resolution
uint8_t DT_GetAllResolution(DallasTemperature_HandleTypeDef* dt)
{
	return dt->bitResolution;
}

// returns the current resolution of the device, 9-12
// returns 0 if device not found
uint8_t DT_GetResolution(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress)
{
	// DS1820 and DS18S20 have no resolution configuration register
	if (deviceAddress[0] == DS18S20MODEL)
		return 12;

	ScratchPad scratchPad;
	if (DT_IsConnected_ScratchPad(dt, deviceAddress, scratchPad))
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
void DT_SetWaitForConversion(DallasTemperature_HandleTypeDef* dt, bool flag)
{
	dt->waitForConversion = flag;
}

// gets the value of the waitForConversion flag
bool DT_GetWaitForConversion(DallasTemperature_HandleTypeDef* dt)
{
	return dt->waitForConversion;
}

// sets the value of the checkForConversion flag
// TRUE : function requestTemperature() etc will 'listen' to an IC to determine whether a conversion is complete
// FALSE: function requestTemperature() etc will wait a set time (worst case scenario) for a conversion to complete
void DT_SetCheckForConversion(DallasTemperature_HandleTypeDef* dt, bool flag)
{
	dt->checkForConversion = flag;
}

// gets the value of the waitForConversion flag
bool DT_GetCheckForConversion(DallasTemperature_HandleTypeDef* dt)
{
	return dt->checkForConversion;
}

bool DT_IsConversionComplete(DallasTemperature_HandleTypeDef* dt)
{
	uint8_t b;
	OW_Send(dt->ow, (uint8_t *) OW_READ_SLOT, 0, &b, 1, 0);

	return (b == 1);
}

// sends command for all devices on the bus to perform a temperature conversion
void DT_RequestTemperatures(DallasTemperature_HandleTypeDef* dt)
{
	OW_Send(dt->ow, (uint8_t *) "\xcc\x44", 2, (uint8_t *) NULL, 0, OW_NO_READ);

	// ASYNC mode?
	if (!dt->waitForConversion)
		return;

	BlockTillConversionComplete(dt, dt->bitResolution);
}

// sends command for one device to perform a temperature by address
// returns FALSE if device is disconnected
// returns TRUE  otherwise
bool DT_RequestTemperaturesByAddress(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress)
{
	uint8_t bitResolution = DT_GetResolution(dt, deviceAddress);

	if (bitResolution == 0)
	{
		return false; //Device disconnected
	}

	uint8_t query[10]={0x55, 0, 0, 0, 0, 0, 0, 0, 0, STARTCONVO};
	memcpy(&query[1], deviceAddress, 8);
	OW_Send(dt->ow, query, 10, NULL, 0, OW_NO_READ);

	// ASYNC mode?
	if (!dt->waitForConversion)
		return true;

	BlockTillConversionComplete(dt, dt->bitResolution);

	return true;
}

// sends command for one device to perform a temp conversion by index
bool DT_RequestTemperaturesByIndex(DallasTemperature_HandleTypeDef* dt, uint8_t deviceIndex)
{
	CurrentDeviceAddress deviceAddress;
	DT_GetAddress(dt, deviceAddress, deviceIndex);

	return DT_RequestTemperaturesByAddress(dt, deviceAddress);
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

// Sends command to one device to save values from scratchpad to EEPROM by index
// Returns true if no errors were encountered, false indicates failure
bool DT_SaveScratchPadByIndex(DallasTemperature_HandleTypeDef* dt, uint8_t deviceIndex)
{
	CurrentDeviceAddress deviceAddress;
  if (!DT_GetAddress(dt, deviceAddress, deviceIndex)) return false;

  return DT_SaveScratchPad(dt, deviceAddress);
}

// Sends command to one or more devices to save values from scratchpad to EEPROM
// If optional argument deviceAddress is omitted the command is send to all devices
// Returns true if no errors were encountered, false indicates failure
bool DT_SaveScratchPad(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress)
{
	uint8_t query[10]={0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	if (OW_Reset(dt->ow) == 0)
		return false;

  if (deviceAddress == NULL)
  {
	  query[0] = 0xCC;
	  query[1] = COPYSCRATCH;
	  OW_Send(dt->ow, query, 2, NULL, 0, OW_NO_READ);
  }
  else
  {
	  query[0] = 0x55;
	  memcpy(&query[1], deviceAddress, 8);
	  query[9] = COPYSCRATCH;
	  OW_Send(dt->ow, query, 10, NULL, 0, OW_NO_READ);
  }

  // Specification: NV Write Cycle Time is typically 2ms, max 10ms
  // Waiting 20ms to allow for sensors that take longer in practice
  if (!dt->parasite)
  {
    HAL_Delay(20);
  }
  else
  {

	ActivateExternalPullup(dt);
    HAL_Delay(20);
    DeactivateExternalPullup(dt);
  }

  return OW_Reset(dt->ow) == 1;
}

// Sends command to one device to recall values from EEPROM to scratchpad by index
// Returns true if no errors were encountered, false indicates failure
bool DT_RecallScratchPadByIndex(DallasTemperature_HandleTypeDef* dt, uint8_t deviceIndex)
{
  CurrentDeviceAddress deviceAddress;
  if (!DT_GetAddress(dt, deviceAddress, deviceIndex)) return false;

  return DT_RecallScratchPad(dt, deviceAddress);
}

// Sends command to one or more devices to recall values from EEPROM to scratchpad
// If optional argument deviceAddress is omitted the command is send to all devices
// Returns true if no errors were encountered, false indicates failure
bool DT_RecallScratchPad(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress)
{
	uint8_t query[10]={0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	if (OW_Reset(dt->ow) == 0)
		return false;

	if (deviceAddress == NULL)
	{
	  query[0] = 0xCC;
	  query[1] = RECALLSCRATCH;
	  OW_Send(dt->ow, query, 2, NULL, 0, OW_NO_READ);
	}
	else
	{
	  query[0] = 0x55;
	  memcpy(&query[1], deviceAddress, 8);
	  query[9] = RECALLSCRATCH;
	  OW_Send(dt->ow, query, 10, NULL, 0, OW_NO_READ);
	}

	// Specification: Strong pullup only needed when writing to EEPROM (and temp conversion)
	uint32_t start = HAL_GetTick();

	while (dt->ow->huart->gState == HAL_UART_STATE_TIMEOUT)
	{
		// Datasheet doesn't specify typical/max duration, testing reveals typically within 1ms
		if (HAL_GetTick() - start > 20) return false;
		__NOP();
	}

	return OW_Reset(dt->ow) == 1;
}

// Sets the autoSaveScratchPad flag
void DT_SetAutoSaveScratchPad(DallasTemperature_HandleTypeDef* dt, bool flag)
{
  dt->autoSaveScratchPad = flag;
}

// Gets the autoSaveScratchPad flag
bool DT_GetAutoSaveScratchPad(DallasTemperature_HandleTypeDef* dt)
{
  return dt->autoSaveScratchPad;
}

// Fetch temperature for device index
float DT_GetTempCByIndex(DallasTemperature_HandleTypeDef* dt, uint8_t deviceIndex)
{
	CurrentDeviceAddress deviceAddress;

	if (!DT_GetAddress(dt, deviceAddress, deviceIndex))
	{
		return DEVICE_DISCONNECTED_C;
	}

	return DT_GetTempC(dt, (uint8_t*) deviceAddress);
}

// Fetch temperature for device index
float DT_GetTempFByIndex(DallasTemperature_HandleTypeDef* dt, uint8_t deviceIndex)
{
	CurrentDeviceAddress deviceAddress;

	if (!DT_GetAddress(dt, deviceAddress, deviceIndex))
	{
		return DEVICE_DISCONNECTED_F;
	}

	return DT_GetTempF(dt, (uint8_t*) deviceAddress);
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
int16_t DT_GetTemp(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress)
{
	ScratchPad scratchPad;
	if (DT_IsConnected_ScratchPad(dt, deviceAddress, scratchPad))
		return DT_CalculateTemperature(deviceAddress, scratchPad);
	return DEVICE_DISCONNECTED_RAW;
}

// returns temperature in degrees C or DEVICE_DISCONNECTED_C if the
// device's scratch pad cannot be read successfully.
// the numeric value of DEVICE_DISCONNECTED_C is defined in
// DallasTemperature.h. It is a large negative number outside the
// operating range of the device
float DT_GetTempC(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress)
{
	return DT_RawToCelsius(DT_GetTemp(dt, deviceAddress));
}

// returns temperature in degrees F or DEVICE_DISCONNECTED_F if the
// device's scratch pad cannot be read successfully.
// the numeric value of DEVICE_DISCONNECTED_F is defined in
// DallasTemperature.h. It is a large negative number outside the
// operating range of the device
float DT_GetTempF(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress)
{
	return DT_RawToFahrenheit(DT_GetTemp(dt, deviceAddress));
}

// returns true if the bus requires parasite power
bool DT_IsParasitePowerMode(DallasTemperature_HandleTypeDef* dt)
{
	return dt->parasite;
}

// IF alarm is not used one can store a 16 bit int of userdata in the alarm
// registers. E.g. an ID of the sensor.
// See github issue #29

// note if device is not connected it will fail writing the data.
void DT_SetUserData(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress, int16_t data)
{
	// return when stored value == new value
	if (DT_GetUserData(dt, deviceAddress) == data)
		return;

	ScratchPad scratchPad;
	if (DT_IsConnected_ScratchPad(dt, deviceAddress, scratchPad))
	{
		scratchPad[HIGH_ALARM_TEMP] = data >> 8;
		scratchPad[LOW_ALARM_TEMP] = data & 255;
		DT_WriteScratchPad(dt, deviceAddress, scratchPad);
	}
}

int16_t DT_GetUserData(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress)
{
	int16_t data = 0;
	ScratchPad scratchPad;
	if (DT_IsConnected_ScratchPad(dt, deviceAddress, scratchPad))
	{
		data = scratchPad[HIGH_ALARM_TEMP] << 8;
		data += scratchPad[LOW_ALARM_TEMP];
	}
	return data;
}

// note If address cannot be found no error will be reported.
int16_t DT_GetUserDataByIndex(DallasTemperature_HandleTypeDef* dt, uint8_t deviceIndex)
{
	CurrentDeviceAddress deviceAddress;
	DT_GetAddress(dt, deviceAddress, deviceIndex);
	return DT_GetUserData(dt, (uint8_t*) deviceAddress);
}

void DT_SetUserDataByIndex(DallasTemperature_HandleTypeDef* dt, uint8_t deviceIndex, int16_t data)
{
	CurrentDeviceAddress deviceAddress;
	DT_GetAddress(dt, deviceAddress, deviceIndex);
	DT_SetUserData(dt, (uint8_t*) deviceAddress, data);
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

#if REQUIRESALARMS
/*

 ALARMS:

 TH and TL Register Format

 BIT 7 BIT 6 BIT 5 BIT 4 BIT 3 BIT 2 BIT 1 BIT 0
 S    2^6   2^5   2^4   2^3   2^2   2^1   2^0

 Only bits 11 through 4 of the temperature register are used
 in the TH and TL comparison since TH and TL are 8-bit
 registers. If the measured temperature is lower than or equal
 to TL or higher than or equal to TH, an alarm condition exists
 and an alarm flag is set inside the DS18B20. This flag is
 updated after every temperature measurement; therefore, if the
 alarm condition goes away, the flag will be turned off after
 the next temperature conversion.

 */

// sets the high alarm temperature for a device in degrees Celsius
// accepts a float, but the alarm resolution will ignore anything
// after a decimal point.  valid range is -55C - 125C
void DT_SetHighAlarmTemp(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress, int8_t celsius)
{
	// return when stored value == new value
	if (DT_GetHighAlarmTemp(dt, deviceAddress) == celsius)
		return;

	// make sure the alarm temperature is within the device's range
	if (celsius > 125)
		celsius = 125;
	else if (celsius < -55)
		celsius = -55;

	ScratchPad scratchPad;
	if (DT_IsConnected_ScratchPad(dt, deviceAddress, scratchPad))
	{
		scratchPad[HIGH_ALARM_TEMP] = (uint8_t) celsius;
		DT_WriteScratchPad(dt, deviceAddress, scratchPad);
	}
}

// sets the low alarm temperature for a device in degrees Celsius
// accepts a float, but the alarm resolution will ignore anything
// after a decimal point.  valid range is -55C - 125C
void DT_SetLowAlarmTemp(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress, int8_t celsius)
{
	// return when stored value == new value
	if (DT_GetLowAlarmTemp(dt, deviceAddress) == celsius)
		return;

	// make sure the alarm temperature is within the device's range
	if (celsius > 125)
		celsius = 125;
	else if (celsius < -55)
		celsius = -55;

	ScratchPad scratchPad;
	if (DT_IsConnected_ScratchPad(dt, deviceAddress, scratchPad))
	{
		scratchPad[LOW_ALARM_TEMP] = (uint8_t) celsius;
		DT_WriteScratchPad(dt, deviceAddress, scratchPad);
	}
}

// returns a int8_t with the current high alarm temperature or
// DEVICE_DISCONNECTED for an address
int8_t DT_GetHighAlarmTemp(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress)
{
	ScratchPad scratchPad;
	if (DT_IsConnected_ScratchPad(dt, deviceAddress, scratchPad))
		return (int8_t) scratchPad[HIGH_ALARM_TEMP];
	return DEVICE_DISCONNECTED_C;
}

// returns a int8_t with the current low alarm temperature or
// DEVICE_DISCONNECTED for an address
int8_t DT_GetLowAlarmTemp(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress)
{
	ScratchPad scratchPad;
	if (DT_IsConnected_ScratchPad(dt, deviceAddress, scratchPad))
		return (int8_t) scratchPad[LOW_ALARM_TEMP];
	return DEVICE_DISCONNECTED_C;
}

// resets internal variables used for the alarm search
void DT_ResetAlarmSearch(DallasTemperature_HandleTypeDef* dt)
{
	dt->alarmSearchJunction = -1;
	dt->alarmSearchExhausted = 0;
	for (uint8_t i = 0; i < 7; i++)
	{
		dt->alarmSearchAddress[i] = 0;
	}
}

// This is a modified version of the OneWire::search method.
//
// Also added the OneWire search fix documented here:
// http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1238032295
//
// Perform an alarm search. If this function returns a '1' then it has
// enumerated the next device and you may retrieve the ROM from the
// OneWire::address variable. If there are no devices, no further
// devices, or something horrible happens in the middle of the
// enumeration then a 0 is returned.  If a new device is found then
// its address is copied to newAddr.  Use
// DallasTemperature::resetAlarmSearch() to start over.
bool DT_AlarmSearch(DallasTemperature_HandleTypeDef* dt, uint8_t* newAddr)
{
//	uint8_t i;
//	int8_t lastJunction = -1;
//	uint8_t done = 1;

	if (dt->alarmSearchExhausted)
		return false;

	if (!OW_Reset(dt->ow))
		return false;
/* TODO:
	// send the alarm search command
	_wire->write(0xEC, 0);

	for (i = 0; i < 64; i++) {

		uint8_t a = _wire->read_bit();
		uint8_t nota = _wire->read_bit();
		uint8_t ibyte = i / 8;
		uint8_t ibit = 1 << (i & 7);

		// I don't think this should happen, this means nothing responded, but maybe if
		// something vanishes during the search it will come up.
		if (a && nota)
			return false;

		if (!a && !nota) {
			if (i == alarmSearchJunction) {
				// this is our time to decide differently, we went zero last time, go one.
				a = 1;
				alarmSearchJunction = lastJunction;
			} else if (i < alarmSearchJunction) {

				// take whatever we took last time, look in address
				if (alarmSearchAddress[ibyte] & ibit) {
					a = 1;
				} else {
					// Only 0s count as pending junctions, we've already exhausted the 0 side of 1s
					a = 0;
					done = 0;
					lastJunction = i;
				}
			} else {
				// we are blazing new tree, take the 0
				a = 0;
				alarmSearchJunction = i;
				done = 0;
			}
			// OneWire search fix
			// See: http://www.arduino.cc/cgi-bin/yabb2/YaBB.pl?num=1238032295
		}

		if (a)
			alarmSearchAddress[ibyte] |= ibit;
		else
			alarmSearchAddress[ibyte] &= ~ibit;

		_wire->write_bit(a);
	}

	if (done)
		alarmSearchExhausted = 1;
	for (i = 0; i < 8; i++)
		newAddr[i] = alarmSearchAddress[i];*/
	return true;

}

// returns true if device address might have an alarm condition
// (only an alarm search can verify this)
bool DT_HasAlarmByAddress(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress)
{
	ScratchPad scratchPad;

	if (DT_IsConnected_ScratchPad(dt, deviceAddress, scratchPad))
	{
		int8_t temp = DT_CalculateTemperature(deviceAddress, scratchPad) >> 7;

		// check low alarm
		if (temp <= (int8_t) scratchPad[LOW_ALARM_TEMP])
			return true;

		// check high alarm
		if (temp >= (int8_t) scratchPad[HIGH_ALARM_TEMP])
			return true;
	}

	// no alarm
	return false;

}

// returns true if any device is reporting an alarm condition on the bus
bool DT_HasAlarm(DallasTemperature_HandleTypeDef* dt)
{
	CurrentDeviceAddress deviceAddress;
	DT_ResetAlarmSearch(dt);
	return DT_AlarmSearch(dt, deviceAddress);
}

// runs the alarm handler for all devices returned by alarmSearch()
// unless there no _AlarmHandler exist.
void DT_ProcessAlarms(DallasTemperature_HandleTypeDef* dt)
{
	if (!DT_HasAlarmHandler(dt))
	{
		return;
	}

	DT_ResetAlarmSearch(dt);
	CurrentDeviceAddress alarmAddr;

	while (DT_AlarmSearch(dt, alarmAddr))
	{
		if (DT_ValidAddress(alarmAddr))
		{
			dt->_AlarmHandler(alarmAddr);
		}
	}
}

// sets the alarm handler
void DT_SetAlarmHandler(DallasTemperature_HandleTypeDef* dt, const AlarmHandler *handler)
{
	dt->_AlarmHandler = handler;
}

// checks if AlarmHandler has been set.
bool DT_HasAlarmHandler(DallasTemperature_HandleTypeDef* dt)
{
  return dt->_AlarmHandler != NO_ALARM_HANDLER;
}

#endif
