/*
 * The Clear BSD License
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted (subject to the limitations in the disclaimer below) provided
 * that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY THIS LICENSE.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* FreeRTOS kernel includes. */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "timers.h"

/* Freescale includes. */
#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "board.h"
#include "pin_mux.h"

/* StdLib includes */
#include <stdbool.h>

/*******************************************************************************
 * Definitions
 ******************************************************************************/
__attribute__(( section(".noinit.$RAM4"), aligned(8) ))
uint8_t gfx_buffer[480*272/2] = {0};

/* Task priorities. */
#define hello_task_PRIORITY (configMAX_PRIORITIES - 1)
/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void hello_task(void *pvParameters);

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

int main(void)
{
    /* Init board hardware. */
    /* attach 12 MHz clock to FLEXCOMM0 (debug console) */
    CLOCK_AttachClk(BOARD_DEBUG_UART_CLK_ATTACH);

    BOARD_InitPins();
    BOARD_BootClockPLL180M();
    BOARD_InitDebugConsole();
    BOARD_InitSDRAM();
    BOARD_InitLCD();

    if (SDRAM_DataBusCheck(0xa0000000) != kStatus_Success)
	{
		PRINTF("SDRAM data bus check is failure.\r\n");
	} else {
		PRINTF("SDRAM data bus OK.\r\n");
	}

	if (SDRAM_AddressBusCheck(0xa0000000, (8 * 1024 * 1024)) != kStatus_Success)
	{
		PRINTF("SDRAM address bus check is failure.\r\n");
	} else {
		PRINTF("SDRAM address bus OK.\r\n");
	}

	lcd_test_pattern();

    if (xTaskCreate(hello_task, "Hello_task", configMINIMAL_STACK_SIZE + 10, NULL, hello_task_PRIORITY, NULL) != pdPASS)
    {
        PRINTF("Task creation failed!.\r\n");
        while (1) {

        }
    }
    vTaskStartScheduler();

    while (1) {

    }
}

static void hello_task(void *pvParameters)
{
    for (;;)
    {
        PRINTF("Hello world.\r\n");
        vTaskSuspend(NULL);
    }
}
