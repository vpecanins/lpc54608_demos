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
#include "board.h"
#include "pin_mux.h"
#include "hw_self_test.h"

/* Graphics library */
#include "lvgl.h"

/* StdLib includes */
#include <stdbool.h>

/*******************************************************************************
 * Definitions
 ******************************************************************************/
__attribute__(( section(".noinit.$RAM4"), aligned(8) ))
lv_color_t gfx_buffer[LV_VER_RES][LV_HOR_RES] = {0};

__attribute__(( section(".rodata.$BOARD_FLASH"), aligned(4) ))
uint8_t spifi_test[] = {'h', 'e', 'l', 'l', 'o', 'L', 'P', 'C'};

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
static void hello_task(void *pvParameters);
static void cursor_task(void *pvParameters);
static void lv_task(void *pvParameters);

int main(void)
{
    /* Init board hardware. */
    BOARD_InitPins();
    BOARD_BootClockPLL180M();

	CLOCK_EnableClock(kCLOCK_Gpio0);
	CLOCK_EnableClock(kCLOCK_Gpio1);
	CLOCK_EnableClock(kCLOCK_Gpio2);
	CLOCK_EnableClock(kCLOCK_Gpio3);

    BOARD_InitSDRAM();
    BOARD_InitSPIFI();
    BOARD_InitLCD();
    BOARD_InitTouchPanel();
    BOARD_InitButtons();

    TEST_SDRAM();
    TEST_SPIFI();

    if (xTaskCreate(hello_task, "Hello_task", 150, NULL, (configMAX_PRIORITIES - 1), NULL) != pdPASS)
    {
        printf("Task creation failed!.\r\n");
        while (1) {

        }
    }

   if (xTaskCreate(cursor_task, "Cursor_task", 150, NULL, (configMAX_PRIORITIES - 1), NULL) != pdPASS)
   {
	   printf("Task creation failed!.\r\n");
	   while (1) {

	   }
   }

   if (xTaskCreate(lv_task, "LVGL_task", 1000, NULL, (configMAX_PRIORITIES - 1), NULL) != pdPASS)
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

lv_res_t btn_action(lv_obj_t * btn)
{
    printf("Clicked\n");
    return LV_RES_OK;
}

static void lv_task(void *pvParameters)
{
	lv_init();

	lv_port_disp_init();
	lv_port_indev_init();

	lv_obj_t * btn = lv_btn_create(lv_scr_act(), NULL);     /*Add a button the current screen*/
	lv_obj_set_pos(btn, 10, 10);                            /*Set its position*/
	lv_obj_set_size(btn, 100, 50);                          /*Set its size*/

	lv_btn_set_action(btn, LV_BTN_ACTION_CLICK, btn_action);/*Assign a callback to the button*/
	lv_obj_t * label = lv_label_create(btn, NULL);          /*Add a label to the button*/
	lv_label_set_text(label, "Button");                     /*Set the labels text*/

	/*Create styles for the switch*/
	static lv_style_t bg_style;
	static lv_style_t indic_style;
	static lv_style_t knob_on_style;
	static lv_style_t knob_off_style;
	lv_style_copy(&bg_style, &lv_style_pretty);
	bg_style.body.radius = LV_RADIUS_CIRCLE;

	lv_style_copy(&indic_style, &lv_style_pretty_color);
	indic_style.body.radius = LV_RADIUS_CIRCLE;
	indic_style.body.main_color = LV_COLOR_HEX(0x9fc8ef);
	indic_style.body.grad_color = LV_COLOR_HEX(0x9fc8ef);
	indic_style.body.padding.hor = 0;
	indic_style.body.padding.ver = 0;

	lv_style_copy(&knob_off_style, &lv_style_pretty);
	knob_off_style.body.radius = LV_RADIUS_CIRCLE;
	knob_off_style.body.shadow.width = 4;
	knob_off_style.body.shadow.type = LV_SHADOW_BOTTOM;

	lv_style_copy(&knob_on_style, &lv_style_pretty_color);
	knob_on_style.body.radius = LV_RADIUS_CIRCLE;
	knob_on_style.body.shadow.width = 4;
	knob_on_style.body.shadow.type = LV_SHADOW_BOTTOM;

	/*Create a switch and apply the styles*/
	lv_obj_t *sw1 = lv_sw_create(lv_scr_act(), NULL);
	lv_sw_set_style(sw1, LV_SW_STYLE_BG, &bg_style);
	lv_sw_set_style(sw1, LV_SW_STYLE_INDIC, &indic_style);
	lv_sw_set_style(sw1, LV_SW_STYLE_KNOB_ON, &knob_on_style);
	lv_sw_set_style(sw1, LV_SW_STYLE_KNOB_OFF, &knob_off_style);
	lv_obj_align(sw1, NULL, LV_ALIGN_CENTER, 0, -50);

	/*Copy the first switch and turn it ON*/
	lv_obj_t *sw2 = lv_sw_create(lv_scr_act(), sw1);

	lv_obj_align(sw2, NULL, LV_ALIGN_CENTER, 0, 50);

	while (1) {
		lv_tick_inc(1);

		lv_task_handler();

		vTaskDelay(1);
	}

    vTaskSuspend(NULL);
}

void pint_intr_callback(pint_pin_int_t pintr, uint32_t pmatch_status)
{
    printf("\f\r\nPINT Pin Interrupt %d event detected.", pintr);
}
