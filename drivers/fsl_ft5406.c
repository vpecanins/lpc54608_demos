/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2017 NXP
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
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

#include "fsl_common.h"º
#include "fsl_i2c.h"
#include "fsl_ft5406.h"

typedef struct _ft5406_touch_point
{
    uint8_t XH;
    uint8_t XL;
    uint8_t YH;
    uint8_t YL;
    uint8_t RESERVED[2];
} ft5406_touch_point_t;

typedef struct _ft5406_touch_data
{
    uint8_t GEST_ID;
    uint8_t TD_STATUS;
    ft5406_touch_point_t TOUCH[FT5406_MAX_TOUCHES];
} ft5406_touch_data_t;

#define TOUCH_POINT_GET_EVENT(T) ((touch_event_t)((T).XH >> 6))
#define TOUCH_POINT_GET_ID(T) ((T).YH >> 4)
#define TOUCH_POINT_GET_X(T) ((((T).XH & 0x0f) << 8) | (T).XL)
#define TOUCH_POINT_GET_Y(T) ((((T).YH & 0x0f) << 8) | (T).YL)

status_t FT5406_Init(ft5406_handle_t *handle, I2C_Type *base)
{
    i2c_master_transfer_t *xfer = &(handle->xfer);
    status_t status;
    uint8_t mode;

    assert(handle);
    assert(base);

    if (!handle || !base)
    {
        return kStatus_InvalidArgument;
    }

    handle->base = base;

    /* clear transfer structure and buffer */
    memset(xfer, 0, sizeof(*xfer));
    memset(handle->touch_buf, 0, FT5406_TOUCH_DATA_LEN);

    /* set device mode to normal operation */
    mode = 0;
    xfer->slaveAddress = 0x38;
    xfer->direction = kI2C_Write;
    xfer->subaddress = 0;
    xfer->subaddressSize = 1;
    xfer->data = &mode;
    xfer->dataSize = 1;
    xfer->flags = kI2C_TransferDefaultFlag;

    status = I2C_MasterTransferBlocking(handle->base, &handle->xfer);

    /* prepare transfer structure for reading touch data */
    xfer->slaveAddress = 0x38;
    xfer->direction = kI2C_Read;
    xfer->subaddress = 1;
    xfer->subaddressSize = 1;
    xfer->data = handle->touch_buf;
    xfer->dataSize = FT5406_TOUCH_DATA_LEN;
    xfer->flags = kI2C_TransferDefaultFlag;

    return status;
}

status_t FT5406_Denit(ft5406_handle_t *handle)
{
    assert(handle);

    if (!handle)
    {
        return kStatus_InvalidArgument;
    }

    handle->base = NULL;
    return kStatus_Success;
}

status_t FT5406_ReadTouchData(ft5406_handle_t *handle)
{
    assert(handle);

    if (!handle)
    {
        return kStatus_InvalidArgument;
    }

    return I2C_MasterTransferBlocking(handle->base, &handle->xfer);
}

status_t FT5406_GetSingleTouch(ft5406_handle_t *handle, touch_event_t *touch_event, uint32_t *touch_x, uint32_t *touch_y)
{
    status_t status;
    touch_event_t touch_event_local;

    status = FT5406_ReadTouchData(handle);

    if (status == kStatus_Success)
    {
        ft5406_touch_data_t *touch_data = (ft5406_touch_data_t *)(void *)(handle->touch_buf);

        if (touch_event == NULL)
        {
            touch_event = &touch_event_local;
        }
        *touch_event = TOUCH_POINT_GET_EVENT(touch_data->TOUCH[0]);

        /* Update coordinates only if there is touch detected */
        if ((*touch_event == kTouch_Down) || (*touch_event == kTouch_Contact))
        {
            if (touch_x)
            {
                *touch_x = TOUCH_POINT_GET_X(touch_data->TOUCH[0]);
            }
            if (touch_y)
            {
                *touch_y = TOUCH_POINT_GET_Y(touch_data->TOUCH[0]);
            }
        }
    }

    return status;
}

status_t FT5406_GetMultiTouch(ft5406_handle_t *handle, uint32_t *touch_count,
		touch_point_t touch_array[FT5406_MAX_TOUCHES], uint32_t swap_xy)
{
    status_t status;

    status = FT5406_ReadTouchData(handle);

    if (status == kStatus_Success)
    {
        ft5406_touch_data_t *touch_data = (ft5406_touch_data_t *)(void *)(handle->touch_buf);
        uint32_t i;
        uint32_t touch_n;

        /* Bug Fix: It is observed that sometimes TD_STATUS could be 255.
         * Consider this an error and return always at most FT5406_MAX_TOUCHES
         */
        if (touch_data->TD_STATUS < FT5406_MAX_TOUCHES)
        	touch_n = touch_data->TD_STATUS;
        else
        	touch_n = 0;

        /* Decode number of touches */
        if (touch_count) *touch_count = touch_n;

        /* Decode valid touch points */
        for (i = 0; i < touch_n; i++)
        {
            touch_array[i].id = TOUCH_POINT_GET_ID(touch_data->TOUCH[i]);
            touch_array[i].event = TOUCH_POINT_GET_EVENT(touch_data->TOUCH[i]);
            touch_array[i].x = swap_xy ?
            		TOUCH_POINT_GET_Y(touch_data->TOUCH[i]) :
					TOUCH_POINT_GET_X(touch_data->TOUCH[i]);
            touch_array[i].y =swap_xy ?
            		TOUCH_POINT_GET_X(touch_data->TOUCH[i]) :
					TOUCH_POINT_GET_Y(touch_data->TOUCH[i]);
        }

        /* Clear vacant elements of touch_array */
        for (; i < FT5406_MAX_TOUCHES; i++)
        {
            touch_array[i].id = 0;
            touch_array[i].event = kTouch_Reserved;
            touch_array[i].x = 0;
            touch_array[i].y = 0;
        }
    }

    return status;
}
