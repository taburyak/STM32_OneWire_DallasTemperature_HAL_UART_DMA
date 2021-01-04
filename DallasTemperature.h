/*
 * DallasTemperature.h
 *
 *  Created on: Dec 24, 2020
 *      Author: tabur
 */

#ifndef INC_DALLASTEMPERATURE_H_
#define INC_DALLASTEMPERATURE_H_

// set to true to include code implementing alarm search functions
#ifndef REQUIRESALARMS
#define REQUIRESALARMS	false
#endif

#include "OneWire.h"

// Model IDs
#define DS18S20MODEL 	0x10  // also DS1820
#define DS18B20MODEL 	0x28  // also MAX31820
#define DS1822MODEL  	0x22
#define DS1825MODEL  	0x3B
#define DS28EA00MODEL 	0x42

// Error Codes
#define DEVICE_DISCONNECTED_C 	-127
#define DEVICE_DISCONNECTED_F 	-196.6
#define DEVICE_DISCONNECTED_RAW -7040

#define max(a,b) (((a)>(b))?(a):(b))
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))

#define ONEWIRE_MAX_DEVICES	5

#if REQUIRESALARMS
typedef void AlarmHandler(const uint8_t*);
// Alarm handler
#define NO_ALARM_HANDLER ((AlarmHandler *)0)
#endif

typedef struct{
	OneWire_HandleTypeDef* ow;
	// count of devices on the bus
	uint8_t devices;
	// count of DS18xxx Family devices on bus
	uint8_t ds18Count;
	// parasite power on or off
	bool parasite;
	// external pullup
	bool useExternalPullup;
	// used to determine the delay amount needed to allow for the
	// temperature conversion to take place
	uint8_t bitResolution;
	// used to requestTemperature with or without delay
	bool waitForConversion;
	// used to requestTemperature to dynamically check if a conversion is complete
	bool checkForConversion;
	// used to determine if values will be saved from scratchpad to EEPROM on every scratchpad write
	bool autoSaveScratchPad;
#if REQUIRESALARMS
	// required for alarmSearch
	uint8_t alarmSearchAddress[8];
	int8_t alarmSearchJunction;
	uint8_t alarmSearchExhausted;
	// the alarm handler function pointer
	AlarmHandler *_AlarmHandler;
#endif
}DallasTemperature_HandleTypeDef;

typedef uint8_t ScratchPad[9];
typedef uint8_t AllDeviceAddress[8 * ONEWIRE_MAX_DEVICES];
typedef uint8_t CurrentDeviceAddress[8];

// initialise bus
void DT_SetOneWire(DallasTemperature_HandleTypeDef* dt, OneWire_HandleTypeDef* ow);
void DT_Begin(DallasTemperature_HandleTypeDef* dt);
uint8_t DT_GetDeviceCount(DallasTemperature_HandleTypeDef* dt);
uint8_t DT_GetDS18Count(DallasTemperature_HandleTypeDef* dt);
bool DT_ValidAddress(const uint8_t* deviceAddress);
bool DT_ValidFamily(const uint8_t* deviceAddress);
bool DT_GetAddress(DallasTemperature_HandleTypeDef* dt, uint8_t* currentDeviceAddress, uint8_t index);
bool DT_IsConnected(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress);
bool DT_IsConnected_ScratchPad(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress, uint8_t* scratchPad);
bool DT_ReadScratchPad(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress, uint8_t* scratchPad);
void DT_WriteScratchPad(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress, const uint8_t* scratchPad);
bool DT_ReadPowerSupply(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress);
void DT_SetAllResolution(DallasTemperature_HandleTypeDef* dt, uint8_t newResolution);
bool DT_SetResolution(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress, uint8_t newResolution, bool skipGlobalBitResolutionCalculation);
uint8_t DT_GetAllResolution(DallasTemperature_HandleTypeDef* dt);
uint8_t DT_GetResolution(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress);
void DT_SetWaitForConversion(DallasTemperature_HandleTypeDef* dt, bool flag);
bool DT_GetWaitForConversion(DallasTemperature_HandleTypeDef* dt);
void DT_SetCheckForConversion(DallasTemperature_HandleTypeDef* dt, bool flag);
bool DT_GetCheckForConversion(DallasTemperature_HandleTypeDef* dt);
bool DT_IsConversionComplete(DallasTemperature_HandleTypeDef* dt);
void DT_RequestTemperatures(DallasTemperature_HandleTypeDef* dt);
bool DT_RequestTemperaturesByAddress(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress);
bool DT_RequestTemperaturesByIndex(DallasTemperature_HandleTypeDef* dt, uint8_t deviceIndex);
int16_t DT_MillisToWaitForConversion(uint8_t bitResolution);
bool DT_SaveScratchPadByIndex(DallasTemperature_HandleTypeDef* dt, uint8_t deviceIndex);
bool DT_SaveScratchPad(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress);
bool DT_RecallScratchPadByIndex(DallasTemperature_HandleTypeDef* dt, uint8_t deviceIndex);
bool DT_RecallScratchPad(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress);
void DT_SetAutoSaveScratchPad(DallasTemperature_HandleTypeDef* dt, bool flag);
bool DT_GetAutoSaveScratchPad(DallasTemperature_HandleTypeDef* dt);
uint8_t DT_GetAllResolution(DallasTemperature_HandleTypeDef* dt);
uint8_t DT_GetResolution(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress);
float DT_GetTempCByIndex(DallasTemperature_HandleTypeDef* dt, uint8_t deviceIndex);
float DT_GetTempFByIndex(DallasTemperature_HandleTypeDef* dt, uint8_t deviceIndex);
int16_t DT_GetTemp(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress);
float DT_GetTempC(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress);
float DT_GetTempF(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress);
int16_t DT_GetUserData(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress);
int16_t DT_GetUserDataByIndex(DallasTemperature_HandleTypeDef* dt, uint8_t deviceIndex);
void DT_SetUserDataByIndex(DallasTemperature_HandleTypeDef* dt, uint8_t deviceIndex, int16_t data);
float DT_ToFahrenheit(float celsius);
float DT_ToCelsius(float fahrenheit);
float DT_RawToCelsius(int16_t raw);
float DT_RawToFahrenheit(int16_t raw);
bool DT_IsParasitePowerMode(DallasTemperature_HandleTypeDef* dt);
void DT_SetPullupPin(DallasTemperature_HandleTypeDef* dt, GPIO_TypeDef* port, uint32_t pin);
int16_t DT_CalculateTemperature(const uint8_t* deviceAddress, uint8_t* scratchPad);



#if REQUIRESALARMS
	// sets the high alarm temperature for a device
	// accepts a int8_t.  valid range is -55C - 125C
	void DT_SetHighAlarmTemp(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress, int8_t celsius);

	// sets the low alarm temperature for a device
	// accepts a int8_t.  valid range is -55C - 125C
	void DT_SetLowAlarmTemp(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress, int8_t celsius);

	// returns a int8_t with the current high alarm temperature for a device
	// in the range -55C - 125C
	int8_t DT_GetHighAlarmTemp(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress);

	// returns a int8_t with the current low alarm temperature for a device
	// in the range -55C - 125C
	int8_t DT_GetLowAlarmTemp(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress);

	// resets internal variables used for the alarm search
	void DT_ResetAlarmSearch(DallasTemperature_HandleTypeDef* dt);

	// search the wire for devices with active alarms
	bool DT_AlarmSearch(DallasTemperature_HandleTypeDef* dt, uint8_t* newAddr);

	// returns true if ia specific device has an alarm
	bool DT_HasAlarm(DallasTemperature_HandleTypeDef* dt);
	bool DT_HasAlarmByAddress(DallasTemperature_HandleTypeDef* dt, const uint8_t* deviceAddress);

	// runs the alarm handler for all devices returned by alarmSearch()
	void DT_ProcessAlarms(DallasTemperature_HandleTypeDef* dt);

	// sets the alarm handler
	void DT_SetAlarmHandler(DallasTemperature_HandleTypeDef* dt, const AlarmHandler *);

	// returns true if an AlarmHandler has been set
	bool DT_HasAlarmHandler(DallasTemperature_HandleTypeDef* dt);

#endif

#endif /* INC_DALLASTEMPERATURE_H_ */
