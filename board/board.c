/*
 * The Clear BSD License
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
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

#include "board.h"
#include <stdint.h>
#include "clock_config.h"
#include "fsl_common.h"
#include "fsl_emc.h"
#include "fsl_lcdc.h"
#include "fsl_spifi.h"
#include "fsl_ft5406.h"
#include "fsl_dmic.h"
#include "fsl_ctimer.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
/* The SDRAM timing. */
#define SDRAM_REFRESHPERIOD_NS (64 * 1000000 / 4096) /* 4096 rows/ 64ms */
#define SDRAM_TRP_NS (18u)
#define SDRAM_TRAS_NS (42u)
#define SDRAM_TSREX_NS (67u)
#define SDRAM_TAPR_NS (18u)
#define SDRAM_TWRDELT_NS (6u)
#define SDRAM_TRC_NS (60u)
#define SDRAM_RFC_NS (60u)
#define SDRAM_XSR_NS (67u)
#define SDRAM_RRD_NS (12u)
#define SDRAM_MRD_NCLK (2u)
#define SDRAM_RAS_NCLK (2u)
#define SDRAM_MODEREG_VALUE (0x23u)
#define SDRAM_DEV_MEMORYMAP (0x09u) /* 128Mbits (8M*16, 4banks, 12 rows, 9 columns)*/

/* SPIFI flash parameters */
#define SPIFI_PAGE_SIZE (256)
#define SPIFI_SECTOR_SIZE (4096)
#define SPIFI_BAUDRATE (12000000)
#define SPIFI_CHANNEL 18
#define SPIFI_CMD_NUM (6)
#define SPIFI_CMD_READ (0)
#define SPIFI_CMD_PROGRAM_PAGE (1)
#define SPIFI_CMD_GET_STATUS (2)
#define SPIFI_CMD_ERASE_SECTOR (3)
#define SPIFI_CMD_WRITE_ENABLE (4)
#define SPIFI_CMD_WRITE_REGISTER (5)

/* LCDC parameters */
extern uint8_t gfx_buffer[]; // Frame buffer must be allocated in another file

static const lcdc_config_t lcd_config = {
		.panelClock_Hz = 9000000,
		.ppl = 480,
		.hsw = 2,
		.hfp = 8,
		.hbp = 43,
		.lpp = 272,
		.vsw = 10,
		.vfp = 4,
		.vbp = 12,
		.acBiasFreq = 1U,
		.polarityFlags = kLCDC_InvertVsyncPolarity | kLCDC_InvertHsyncPolarity,
		.enableLineEnd = false,
		.lineEndDelay = 0U,
		.upperPanelAddr = (const uint32_t) &gfx_buffer[0],
		.lowerPanelAddr = 0U,
		.bpp = kLCDC_8BPP,
		.display = kLCDC_DisplayTFT,
		.swapRedBlue = false,
		.dataFormat = kLCDC_LittleEndian
};

extern const uint8_t lcd_cursor[];

static const lcdc_cursor_config_t lcd_cursor_config = {
		.size = kLCDC_CursorSize32,
		.syncMode = kLCDC_CursorAsync,
		.palette0.red = 0U,
		.palette0.green = 0U,
		.palette0.blue = 0U,
		.palette1.red = 255U,
		.palette1.green = 255U,
		.palette1.blue = 255U,

		.image[0] = &lcd_cursor,
		.image[1] = NULL,
		.image[2] = NULL,
		.image[3] = NULL
};

/*******************************************************************************
 * Variables
 ******************************************************************************/

/* SPIFI commands */
spifi_command_t spifi_command[6] = {
		{SPIFI_PAGE_SIZE, false, kSPIFI_DataInput, 1, kSPIFI_CommandDataQuad, kSPIFI_CommandOpcodeAddrThreeBytes, 0x6B},
		{SPIFI_PAGE_SIZE, false, kSPIFI_DataOutput, 0, kSPIFI_CommandOpcodeSerial, kSPIFI_CommandOpcodeAddrThreeBytes, 0x38},
		{4, false, kSPIFI_DataInput, 0, kSPIFI_CommandAllSerial, kSPIFI_CommandOpcodeOnly, 0x05},
		{0, false, kSPIFI_DataOutput, 0, kSPIFI_CommandAllSerial, kSPIFI_CommandOpcodeAddrThreeBytes, 0x20},
		{0, false, kSPIFI_DataOutput, 0, kSPIFI_CommandAllSerial, kSPIFI_CommandOpcodeOnly, 0x06},
		{4, false, kSPIFI_DataOutput, 0, kSPIFI_CommandAllSerial, kSPIFI_CommandOpcodeOnly, 0x01}};

/*******************************************************************************
 * Code
 ******************************************************************************/

/* Initialize the external memory. */
void BOARD_InitSDRAM(void)
{
    emc_basic_config_t basicConfig;
    emc_dynamic_timing_config_t dynTiming;
    emc_dynamic_chip_config_t dynChipConfig;

    /* Basic configuration. */
    basicConfig.endian = kEMC_LittleEndian;
    basicConfig.fbClkSrc = kEMC_IntloopbackEmcclk;
    /* EMC Clock = CPU FREQ/2 here can fit CPU freq from 12M ~ 180M.
     * If you change the divide to 0 and EMC clock is larger than 100M
     * please take refer to emc.dox to adjust EMC clock delay.
     */
    basicConfig.emcClkDiv = 1;
    /* Dynamic memory timing configuration. */
    dynTiming.readConfig = kEMC_Cmddelay;
    dynTiming.refreshPeriod_Nanosec = SDRAM_REFRESHPERIOD_NS;
    dynTiming.tRp_Ns = SDRAM_TRP_NS;
    dynTiming.tRas_Ns = SDRAM_TRAS_NS;
    dynTiming.tSrex_Ns = SDRAM_TSREX_NS;
    dynTiming.tApr_Ns = SDRAM_TAPR_NS;
    dynTiming.tWr_Ns = (1000000000 / CLOCK_GetFreq(kCLOCK_EMC) + SDRAM_TWRDELT_NS); /* one clk + 6ns */
    dynTiming.tDal_Ns = dynTiming.tWr_Ns + dynTiming.tRp_Ns;
    dynTiming.tRc_Ns = SDRAM_TRC_NS;
    dynTiming.tRfc_Ns = SDRAM_RFC_NS;
    dynTiming.tXsr_Ns = SDRAM_XSR_NS;
    dynTiming.tRrd_Ns = SDRAM_RRD_NS;
    dynTiming.tMrd_Nclk = SDRAM_MRD_NCLK;
    /* Dynamic memory chip specific configuration: Chip 0 - MTL48LC8M16A2B4-6A */
    dynChipConfig.chipIndex = 0;
    dynChipConfig.dynamicDevice = kEMC_Sdram;
    dynChipConfig.rAS_Nclk = SDRAM_RAS_NCLK;
    dynChipConfig.sdramModeReg = SDRAM_MODEREG_VALUE;
    dynChipConfig.sdramExtModeReg = 0; /* it has no use for normal sdram */
    dynChipConfig.devAddrMap = SDRAM_DEV_MEMORYMAP;
    /* EMC Basic configuration. */
    EMC_Init(EMC, &basicConfig);
    /* EMC Dynamc memory configuration. */
    EMC_DynamicMemInit(EMC, &dynTiming, &dynChipConfig, 1);
}

void BOARD_InitLCD(void)
{
	/* Route Main clock to LCD. */
    CLOCK_AttachClk(kMCLK_to_LCD_CLK);
    CLOCK_SetClkDiv(kCLOCK_DivLcdClk, 1, true);   /* LCD clock divider */

	LCDC_Init(LCD, &lcd_config, CLOCK_GetFreq(kCLOCK_LCD));

	for (uint32_t i=0; i<128; i++) {
		LCD->PAL[i] = 0xFFFFFFFF;
	}

	/* Setup the Cursor. */
	LCDC_SetCursorConfig(LCD, &lcd_cursor_config);
	LCDC_ChooseCursor(LCD, 0);
	LCDC_EnableCursor(LCD, true);
	LCDC_SetCursorPosition(LCD, lcd_config.ppl/2, lcd_config.lpp/2);

	/* Trigger interrupt at start of every vertical back porch. */
	/*LCDC_SetVerticalInterruptMode(LCD, kLCDC_StartOfBackPorch);
	LCDC_EnableInterrupts(LCD, kLCDC_VerticalCompareInterrupt);
	NVIC_EnableIRQ(LCD_IRQn);*/



	LCDC_Start(LCD);
	LCDC_PowerUp(LCD);
}

static void check_if_finish()
{
    uint32_t val = 0;
    /* Check WIP bit */
    do
    {
        SPIFI_SetCommand(SPIFI0, &spifi_command[SPIFI_CMD_GET_STATUS]);
        while ((SPIFI0->STAT & SPIFI_STAT_INTRQ_MASK) == 0U)
        {
        }
        val = SPIFI_ReadData(SPIFI0);
    } while (val & 0x1);
}

static void enable_quad_mode()
{
    /* Write enable */
    SPIFI_SetCommand(SPIFI0, &spifi_command[SPIFI_CMD_WRITE_ENABLE]);

    /* Set write register command */
    SPIFI_SetCommand(SPIFI0, &spifi_command[SPIFI_CMD_WRITE_REGISTER]);

    SPIFI_WriteData(SPIFI0, 0x40);

    check_if_finish();
}

void BOARD_InitSPIFI(void)
{
	spifi_config_t config = {0};

	CLOCK_AttachClk(kMAIN_CLK_to_SPIFI_CLK);

    uint32_t sourceClockFreq = CLOCK_GetSpifiClkFreq();
	CLOCK_SetClkDiv(kCLOCK_DivSpifiClk, sourceClockFreq / SPIFI_BAUDRATE - 1U, false);

	/* Initialize SPIFI */
	SPIFI_GetDefaultConfig(&config);
	SPIFI_Init(SPIFI0, &config);

	/* Enable Quad mode */
	enable_quad_mode();

	/* Reset to memory command mode */
	SPIFI_ResetCommand(SPIFI0);

	/* Setup memory command */
	SPIFI_SetMemoryCommand(SPIFI0, &spifi_command[SPIFI_CMD_READ]);
}

void BOARD_InitTouchPanel(void)
{
	/* attach 12 MHz clock to FLEXCOMM2 (I2C touch ctl) */
	CLOCK_AttachClk(kFRO12M_to_FLEXCOMM2);

	// Initialize Touch panel I2C interface
	i2c_master_config_t masterConfig;

	I2C_MasterGetDefaultConfig(&masterConfig);

	/* Change the default baudrate configuration */
	masterConfig.baudRate_Bps = 100000U;

	/* Initialize the I2C master peripheral */
	I2C_MasterInit(((I2C_Type *) (I2C2_BASE)), &masterConfig, 12000000);

	// Touch panel RSTn pin
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

void BOARD_InitDMIC(void)
{
	dmic_channel_config_t dmic_channel_cfg = {
			.divhfclk = kDMIC_PdmDiv1,
			.osr = 25U,
			.gainshft = 2U,
			.preac2coef = kDMIC_CompValueZero,
			.preac4coef = kDMIC_CompValueZero,
			.dc_cut_level = kDMIC_DcCut39,
			.post_dc_gain_reduce = 1,
			.saturate16bit = 1U,
			.sample_rate = kDMIC_PhyFullSpeed
	};

	/* DMIC clock */
	CLOCK_AttachClk(kFRO12M_to_DMIC);

	/* Do we need this? */
	CLOCK_AttachClk(kFRO12M_to_FLEXCOMM9);

	/* Divider 12MHz/(4+1) = 48 KHz sample rate*/
	CLOCK_SetClkDiv(kCLOCK_DivDmicClk, 4, false);

	DMIC_Init(DMIC0);
	DMIC_ConfigIO(DMIC0, kDMIC_PdmDual);
	DMIC_Use2fs(DMIC0, true);
	DMIC_SetOperationMode(DMIC0, kDMIC_OperationModeDma);
	DMIC_ConfigChannel(DMIC0, kDMIC_Channel1, kDMIC_Left, &dmic_channel_cfg);
	DMIC_FifoChannel(DMIC0, kDMIC_Channel1, 15, true, true);
	DMIC_EnableChannnel(DMIC0, DMIC_CHANEN_EN_CH1(1));
}

void BOARD_InitCTIMER3(void) {
	/* Setup auxiliar CTIMER (for measuring CPU usage) */
	ctimer_config_t config = {
		.input = kCTIMER_Capture_0,
		.mode = kCTIMER_TimerMode,
		.prescale = 180
	};

	CLOCK_AttachClk(kMAIN_CLK_to_ASYNC_APB);      /* Use Main clock for some of the Ctimers */
	/* Enable the asynchronous bridge */
	SYSCON->ASYNCAPBCTRL = 1;

	CTIMER_Init(CTIMER3, &config);
	CTIMER_StartTimer(CTIMER3);
}
