#include "hw_self_test.h"

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"

/* Freescale includes. */
#include "fsl_device_registers.h"
#include "board.h"
#include "pin_mux.h"
#include "fsl_ft5406.h"

status_t SDRAM_DataBusCheck(volatile uint32_t *address)
{
	uint32_t data = 0;

	/* Write the walking 1's data test. */
	for (data = 1; data != 0; data <<= 1)
	{
		*address = data;

		/* Read the data out of the address and check. */
		if (*address != data)
		{
			return kStatus_Fail;
		}
	}
	return kStatus_Success;
}

status_t SDRAM_AddressBusCheck(volatile uint32_t *address, uint32_t bytes)
{
	uint32_t pattern = 0x55555555;
	uint32_t size = bytes / 4;
	uint32_t offset;
	uint32_t checkOffset;

	/* write the pattern to the power-of-two address. */
	for (offset = 1; offset < size; offset <<= 1)
	{
		address[offset] = pattern;
	}
	address[0] = ~pattern;

	/* Read and check. */
	for (offset = 1; offset < size; offset <<= 1)
	{
		if (address[offset] != pattern)
		{
			return kStatus_Fail;
		}
	}

	if (address[0] != ~pattern)
	{
		return kStatus_Fail;
	}

	/* Change the data to the revert one address each time
	 * and check there is no effect to other address. */
	for (offset = 1; offset < size; offset <<= 1)
	{
		address[offset] = ~pattern;
		for (checkOffset = 1; checkOffset < size; checkOffset <<= 1)
		{
			if ((checkOffset != offset) && (address[checkOffset] != pattern))
			{
				return kStatus_Fail;
			}
		}
		address[offset] = pattern;
	}
	return kStatus_Success;
}

void TEST_LEDS(void) {
	static const gpio_pin_config_t pin_config = {
			kGPIO_DigitalOutput, 0,
	};

	GPIO_PinInit(GPIO, 2, 2, &pin_config);
	GPIO_PinInit(GPIO, 3, 3, &pin_config);
	GPIO_PinInit(GPIO, 3, 14, &pin_config);

	GPIO_PinWrite(GPIO, 2, 2, 1);
	GPIO_PinWrite(GPIO, 3, 3, 1);
	GPIO_PinWrite(GPIO, 3, 14, 1);

	while (1)
	{
		GPIO_PinWrite(GPIO, 3, 3, 1);
		GPIO_PinWrite(GPIO, 2, 2, 0);
		vTaskDelay(30);
		GPIO_PinWrite(GPIO, 2, 2, 1);
		GPIO_PinWrite(GPIO, 3, 3, 0);
		vTaskDelay(30);
		GPIO_PinWrite(GPIO, 3, 3, 1);
		GPIO_PinWrite(GPIO, 3, 14, 0);
		vTaskDelay(30);
		GPIO_PinWrite(GPIO, 3, 14, 1);
		GPIO_PinWrite(GPIO, 3, 3, 0);
		vTaskDelay(30);
	}
}

void TEST_SDRAM(void)
{
	if (SDRAM_DataBusCheck((uint32_t*)0xa0000000) != kStatus_Success)
	{
		printf("SDRAM data bus check is failure.\r\n");
	} else {
		printf("SDRAM data bus OK.\r\n");
	}

	if (SDRAM_AddressBusCheck((uint32_t*)0xa0000000, (8 * 1024 * 1024)) != kStatus_Success)
	{
		printf("SDRAM address bus check is failure.\r\n");
	} else {
		printf("SDRAM address bus OK.\r\n");
	}
}

void TEST_SPIFI(void)
{

}

uint32_t touch_pressed=0;
uint32_t touch_x=0;
uint32_t touch_y=0;

void TEST_TouchCursor(void)
{
	static ft5406_handle_t touch_handle;
	static status_t status;
	static uint32_t touch_count;
	static touch_point_t touch_array[FT5406_MAX_TOUCHES];
	static uint32_t i=0;

	FT5406_Init(&touch_handle, ((I2C_Type *) (I2C2_BASE)));
	while (1) {

		status = FT5406_GetMultiTouch(&touch_handle, &touch_count, touch_array, 1);

		if (status == kStatus_Success) {
			if (touch_count >= 1) {
				i = touch_count-1;
				LCDC_SetCursorPosition(LCD, touch_array[i].x, touch_array[i].y);
				touch_pressed = 1;
				touch_x=touch_array[i].x;
				touch_y=touch_array[i].y;
			} else {
				touch_pressed=0;
			}
		} else {
			FT5406_Init(&touch_handle, ((I2C_Type *) (I2C2_BASE)));
			vTaskDelay(1000U);
		}

		vTaskDelay(3);
	}
}
