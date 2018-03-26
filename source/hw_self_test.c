#include "hw_self_test.h"

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"

/* Freescale includes. */
#include "fsl_device_registers.h"
#include "board.h"
#include "pin_mux.h"
#include "fsl_ft5406.h"

extern uint8_t gfx_buffer[];

/* 16-bit palette format: RGB555
 *
 * 15 14 13 12 11 10  9 8 7 6 5  4 3 2 1 0
 *     B  B  B  B  B  G G G G G  R R R R R
 */
static const uint16_t rgb555(const uint8_t r,const uint8_t g,const uint8_t b) {
	return (b << 10) | (g << 5) | (r & 0x1F);
}

void lcd_test_pattern()
{
	LCD->PAL[0] = rgb555(0,0,0) | (rgb555(31,0,0) << 16);
	LCD->PAL[1] = rgb555(0,31,0) | (rgb555(0,0,31) << 16);
	LCD->PAL[2] = rgb555(31,31,0) | (rgb555(31,0,31) << 16);
	LCD->PAL[3] = rgb555(0,31,31) | (rgb555(31,31,31) << 16);

	LCD->PAL[4] = rgb555(15,15,15) | (rgb555(15,0,0) << 16);
	LCD->PAL[5] = rgb555(0,15,0) | (rgb555(0,0,15) << 16);
	LCD->PAL[6] = rgb555(15,15,0) | (rgb555(15,0,15) << 16);
	LCD->PAL[7] = rgb555(0,15,15) | (rgb555(23,23,23) << 16);

	for (uint32_t j = 0; j<272; j++)
		for (uint32_t i = 0; i<240; i++)
			gfx_buffer[i+240*j] = (i/(240/16)) * 0x11; // two pixels
}

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

void TEST_LCD(void)
{
	lcd_test_pattern();
}

void TEST_SPIFI(void)
{

}

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
			}
		} else {
			FT5406_Init(&touch_handle, ((I2C_Type *) (I2C2_BASE)));
			vTaskDelay(1000U);
		}

		vTaskDelay(3);
	}
}
