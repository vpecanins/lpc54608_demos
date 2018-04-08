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
#include "event_groups.h"

/* Freescale includes. */
#include "fsl_device_registers.h"
#include "board.h"
#include "pin_mux.h"
#include "hw_self_test.h"
#include "gfx_base.h"

/* StdLib includes */
#include <stdbool.h>

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void hello_task(void *pvParameters);
static void cursor_task(void *pvParameters);
extern void audio_task(void *pvParameters);
static void monitor_task(void *pvParameters);

/*******************************************************************************
 * Variables
 ******************************************************************************/
__attribute__(( section(".noinit.$RAM4"), aligned(8) ))
uint8_t gfx_buffer[480*272] = {0};

__attribute__(( section(".rodata.$BOARD_FLASH"), aligned(4) ))
uint8_t spifi_test[] = {'h', 'e', 'l', 'l', 'o', 'L', 'P', 'C'};

/*******************************************************************************
 * Code
 ******************************************************************************/
int main(void)
{
    /* Init board hardware. */

    BOARD_InitPins();
    BOARD_BootClockPLL180M();
    BOARD_InitSDRAM();
    BOARD_InitSPIFI();
    BOARD_InitLCD();
    BOARD_InitTouchPanel();
    BOARD_InitDMIC();
    BOARD_InitCTIMER3();

	CLOCK_EnableClock(kCLOCK_Gpio0);
	CLOCK_EnableClock(kCLOCK_Gpio1);
	CLOCK_EnableClock(kCLOCK_Gpio2);
	CLOCK_EnableClock(kCLOCK_Gpio3);

	DMA_Init(DMA0);

    TEST_SDRAM();
    TEST_SPIFI();
    TEST_LCD();

    if (xTaskCreate(hello_task, "Hello_task", 200, NULL, (configMAX_PRIORITIES - 3), NULL) != pdPASS)
    {
    	printf("Task creation failed!.\r\n");
    	while (1) {

    	}
    }

    if (xTaskCreate(cursor_task, "Cursor_task", 150, NULL, (configMAX_PRIORITIES - 3), NULL) != pdPASS)
    {
    	printf("Task creation failed!.\r\n");
    	while (1) {

    	}
    }

    if (xTaskCreate(audio_task, "Audio_task", 200, NULL, (configMAX_PRIORITIES - 1), NULL) != pdPASS)
    {
    	printf("Task creation failed!.\r\n");
    	while (1) {

    	}
    }

    if (xTaskCreate(monitor_task, "Monitor_task", 200, NULL, (configMAX_PRIORITIES - 3), NULL) != pdPASS)
    {
    	printf("Task creation failed!.\r\n");
    	while (1) {

    	}
    }

    vTaskStartScheduler();

    while (1) {

    }
}

static void hello_task(void *pvParameters)
{
	printf("Hello world.\r\n");
	for (uint32_t i=0; i<8; i++) {
		printf("%c", spifi_test[i]);
	}
	printf("\r\n");

	TEST_LEDS();

    vTaskSuspend(NULL);
}

static void cursor_task(void *pvParameters)
{
	TEST_TouchCursor();

    vTaskSuspend(NULL);
}

static void monitor_task(void *pvParameters)
{
	static char stats_buffer[512];

	vTaskDelay(250);

	while (1) {
		vTaskGetRunTimeStats( stats_buffer );

		gfx_fill_rect((point16_t) {x: 50, y: 20}, (point16_t) {x: 250, y: 70}, 0x08);
		gfx_draw_string((point16_t) {x: 50, y: 20}, stats_buffer, LEFT_ALIGN);

		vTaskDelay(250);
	}
}
