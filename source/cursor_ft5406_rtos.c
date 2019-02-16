#include "cursor_ft5406_rtos.h"
#include "fsl_i2c_freertos.h"
#include "fsl_lcdc.h"
#include "fsl_gpio.h"

#define FT5406_I2C_BASE I2C2_BASE

#define FT5406_I2C_MASTER ((I2C_Type *)FT5406_I2C_BASE)

#define FT5406_IRQ FLEXCOMM2_IRQn

/*! @brief FT5406 I2C address. */
#define FT5406_I2C_ADDRESS (0x38)

/*! @brief FT5406 maximum number of simultaneously detected touches. */
#define FT5406_MAX_TOUCHES (10U)

/*! @brief FT5406 register address where touch data begin. */
#define FT5406_TOUCH_DATA_SUBADDR (1)

/*! @brief FT5406 raw touch data length. */
#define FT5406_TOUCH_DATA_LEN (0x20)

i2c_master_handle_t *g_m_handle;

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

typedef enum _touch_event
{
    kTouch_Down = 0,    /*!< The state changed to touched. */
    kTouch_Up = 1,      /*!< The state changed to not touched. */
    kTouch_Contact = 2, /*!< There is a continuous touch being detected. */
    kTouch_Reserved = 3 /*!< No touch information available. */
} touch_event_t;

typedef struct _touch_point
{
    touch_event_t event; /*!< Indicates the state or event of the touch point. */
    uint8_t id; /*!< Id of the touch point. This numeric value stays constant between down and up event. */
    uint16_t x; /*!< X coordinate of the touch point */
    uint16_t y; /*!< Y coordinate of the touch point */
} touch_point_t;

#define TOUCH_POINT_GET_EVENT(T) ((touch_event_t)((T).XH >> 6))
#define TOUCH_POINT_GET_ID(T) ((T).YH >> 4)
#define TOUCH_POINT_GET_X(T) ((((T).XH & 0x0f) << 8) | (T).XL)
#define TOUCH_POINT_GET_Y(T) ((((T).YH & 0x0f) << 8) | (T).YL)

uint8_t touch_buf[FT5406_TOUCH_DATA_LEN];

static void ft5406_reset(void) {
	// Toggle Touch panel RSTn pin
	gpio_pin_config_t pin_config = {kGPIO_DigitalOutput, 0};

	GPIO_PinInit(GPIO, 2, 27, &pin_config);

	uint32_t i=0;
	GPIO_WritePinOutput(GPIO, 2, 27, 1);
	while (i < 1000U) i++;
	i=0;
	GPIO_WritePinOutput(GPIO, 2, 27, 0);
	while (i < 1000U) i++;
	i=0;
	GPIO_WritePinOutput(GPIO, 2, 27, 1);
	while (i < 1000U) i++;
}

static i2c_master_transfer_t xfer;
static i2c_rtos_handle_t master_rtos_handle;

void cursor_ft5406_rtos_init(void) {
    i2c_master_config_t masterConfig;

    status_t status;
    uint8_t mode;

	/* attach 12 MHz clock to FLEXCOMM2 (I2C touch ctl) */
	CLOCK_AttachClk(kFRO12M_to_FLEXCOMM2);

	NVIC_SetPriority(FT5406_IRQ, 3);
	EnableIRQ(FT5406_IRQ);

	/* Initialize the I2C master peripheral */
	I2C_MasterGetDefaultConfig(&masterConfig);
	masterConfig.baudRate_Bps = 100000U;

	status = I2C_RTOS_Init(&master_rtos_handle, FT5406_I2C_MASTER, &masterConfig, 12000000);
	if (status != kStatus_Success)
	{
		printf("FT5406: error during I2C init, %d", status);
	}

	ft5406_reset();

	 /* clear transfer structure and buffer */
	memset(&xfer, 0, sizeof(xfer));
	memset(touch_buf, 0, FT5406_TOUCH_DATA_LEN);

	/* set device mode to normal operation */
	mode = 0;
	xfer.slaveAddress = FT5406_I2C_ADDRESS;
	xfer.direction = kI2C_Write;
	xfer.subaddress = 0;
	xfer.subaddressSize = 1;
	xfer.data = &mode;
	xfer.dataSize = 1;
	xfer.flags = kI2C_TransferDefaultFlag;

	status = I2C_RTOS_Transfer(&master_rtos_handle, &xfer);
	if (status != kStatus_Success)
	{
		printf("FT5406: error during I2C write transaction, %d", status);
	}

	/* prepare transfer structure for reading touch data */
	xfer.slaveAddress = FT5406_I2C_ADDRESS;
	xfer.direction = kI2C_Read;
	xfer.subaddress = 1;
	xfer.subaddressSize = 1;
	xfer.data = touch_buf;
	xfer.dataSize = FT5406_TOUCH_DATA_LEN;
	xfer.flags = kI2C_TransferDefaultFlag;

}

uint32_t touch_x, touch_y;

void cursor_ft5406_rtos_task(void) {
	status_t status;
	touch_event_t touch_event;

	status = I2C_RTOS_Transfer(&master_rtos_handle, &xfer);
	if (status != kStatus_Success)
	{
		printf("FT5406: error during I2C read transaction, %d", status);
	}
	else
	{
		// Process only single touch for now
		ft5406_touch_data_t *touch_data = (ft5406_touch_data_t *)(void *)(touch_buf);
		touch_event = TOUCH_POINT_GET_EVENT(touch_data->TOUCH[0]);

		/* Update coordinates only if there is touch detected */
		if ((touch_event == kTouch_Down) || (touch_event == kTouch_Contact))
		{
			// Y and X seem to be swapped
			touch_y = TOUCH_POINT_GET_X(touch_data->TOUCH[0]);
			touch_x = TOUCH_POINT_GET_Y(touch_data->TOUCH[0]);

			LCDC_SetCursorPosition(LCD, touch_x, touch_y);
		}
	}

}
