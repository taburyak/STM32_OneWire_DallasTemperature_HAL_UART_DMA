/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include "OneWire.h"
#include "DallasTemperature.h"
/* USER CODE END Includes */

/* USER CODE BEGIN PV */
OneWire_HandleTypeDef ow1;
DallasTemperature_HandleTypeDef dt1;
OneWire_HandleTypeDef ow2;
DallasTemperature_HandleTypeDef dt2;
/* USER CODE END PV */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int _write(int file, char *ptr, int len)
{
	HAL_UART_Transmit(&huart2, (uint8_t *) ptr, len, HAL_MAX_DELAY);
	return len;
}

// function to print a device address
void printAddress(CurrentDeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
	  printf("0x%02X ", deviceAddress[i]);
  }
}
/* USER CODE END 0 */

/* USER CODE BEGIN 2 */
printf("[%8lu] Debug UART2 is OK!\r\n", HAL_GetTick());

OW_Begin(&ow1, &huart1);
OW_Begin(&ow2, &huart3);

if(OW_Reset(&ow1) == OW_OK)
{
  printf("[%8lu] OneWire 1 line devices are present :)\r\n", HAL_GetTick());
}
else
{
  printf("[%8lu] OneWire 1 line no devices :(\r\n", HAL_GetTick());
}

if(OW_Reset(&ow2) == OW_OK)
{
  printf("[%8lu] OneWire 2 line devices are present :)\r\n", HAL_GetTick());
}
else
{
  printf("[%8lu] OneWire 2 line no devices :(\r\n", HAL_GetTick());
}

DT_SetOneWire(&dt1, &ow1);
DT_SetOneWire(&dt2, &ow2);

// arrays to hold device address
CurrentDeviceAddress insideThermometer;

// locate devices on the bus
printf("[%8lu] 1-line Locating devices...\r\n", HAL_GetTick());
DT_Begin(&dt1);
uint8_t numDevOneLine = DT_GetDeviceCount(&dt1);
printf("[%8lu] 1-line Found %d devices.\r\n", HAL_GetTick(), numDevOneLine);

printf("[%8lu] 2-line Locating devices...\r\n", HAL_GetTick());
DT_Begin(&dt2);
uint8_t numDevTwoLine = DT_GetDeviceCount(&dt2);
printf("[%8lu] 2-line Found %d devices.\r\n", HAL_GetTick(), numDevTwoLine);

for (int i = 0; i < numDevOneLine; ++i)
{
	if (!DT_GetAddress(&dt1, insideThermometer, i))
		  printf("[%8lu] 1-line: Unable to find address for Device %d\r\n", HAL_GetTick(), i);
	printf("[%8lu] 1-line: Device %d Address: ", HAL_GetTick(), i);
	printAddress(insideThermometer);
	printf("\r\n");
	// set the resolution to 12 bit (Each Dallas/Maxim device is capable of several different resolutions)
	DT_SetResolution(&dt1, insideThermometer, 12, true);
	printf("[%8lu] 1-line: Device %d Resolution: %d\r\n", HAL_GetTick(), i, DT_GetResolution(&dt1, insideThermometer));
}

for (int i = 0; i < numDevTwoLine; ++i)
{
	if (!DT_GetAddress(&dt2, insideThermometer, i))
		printf("[%8lu] 2-line: Unable to find address for Device %d\r\n", HAL_GetTick(), i);
	printf("[%8lu] 2-line: Device %d Address: ", HAL_GetTick(), i);
	printAddress(insideThermometer);
	printf("\r\n");
	// set the resolution to 12 bit (Each Dallas/Maxim device is capable of several different resolutions)
	DT_SetResolution(&dt2, insideThermometer, 12, true);
	printf("[%8lu] 2-line: Device %d Resolution: %d\r\n", HAL_GetTick(), i, DT_GetResolution(&dt2, insideThermometer));
}
/* USER CODE END 2 */

/* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	// call sensors.requestTemperatures() to issue a global temperature
	// request to all devices on the bus
	printf("[%8lu] 1-line: Requesting temperatures...", HAL_GetTick());
	DT_RequestTemperatures(&dt1); // Send the command to get temperatures
	printf("\r\n[%8lu] 1-line: DONE\r\n", HAL_GetTick());

	printf("[%8lu] 2-line: Requesting temperatures...", HAL_GetTick());
	DT_RequestTemperatures(&dt2); // Send the command to get temperatures
	printf("\r\n[%8lu] 2-line: DONE\r\n", HAL_GetTick());

	for (int i = 0; i < numDevOneLine; ++i)
	{
		// After we got the temperatures, we can print them here.
		// We use the function ByIndex, and as an example get the temperature from the first sensor only.
		printf("[%8lu] 1-line: Temperature for the device %d (index %d) is: %.2f\r\n", HAL_GetTick(), i, i, DT_GetTempCByIndex(&dt1, i));
	}

	for (int i = 0; i < numDevTwoLine; ++i)
	{
		// After we got the temperatures, we can print them here.
		// We use the function ByIndex, and as an example get the temperature from the first sensor only.
		printf("[%8lu] 2-line: Temperature for the device %d (index %d) is: %.2f\r\n", HAL_GetTick(), i, i, DT_GetTempCByIndex(&dt2, i));
	}

	HAL_Delay(DT_MillisToWaitForConversion(DT_GetAllResolution(&dt1)));
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */