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

/* StdLib includes */
#include <stdbool.h>
#include "fsl_common.h"
#include "fsl_dma.h"
#include "fsl_dmic.h"

#include "gfx_base.h"
#include "gfx_dma.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define DMAREQ_DMIC0 16U // Not used
#define DMAREQ_DMIC1 17

#define B0 (1 << 0) // For event groups
#define B1 (1 << 1)

#define BUFFER_LENGTH (1024*4)
#define DMIC_CHANNEL 1
#define DMIC_DMA_MAX_XFER 32 // Maximum FFT size is 8*1024
#define GRAPH_NPOINTS 400

extern EventGroupHandle_t event_group;

struct gfx_graph_desc {
	char ** xlabels;
	char ** ylabels;
	point16_t pos;
	point16_t size;
	point16_t pxlabels;
	point16_t pylabels;
	uint32_t xdiv;
	uint32_t ydiv;
	uint32_t npoints;
	point16_t * points;
	uint32_t * index;
	uint8_t color;
	uint8_t border_color;
	uint8_t div_color;
};

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
void DMIC_DMA_Callback(dma_handle_t *, void *, bool, uint32_t);
void Setup_DMIC_DMA(void);

float asm_log2(float x); // defined in asm_log2.s
void gfx_graph_init(struct gfx_graph_desc * gd);
void gfx_draw_graph(struct gfx_graph_desc * gd);

/*******************************************************************************
 * Variables
 ******************************************************************************/

// DMA xfer descriptor chains
__attribute__(( aligned(16) )) dma_descriptor_t dma_desc_a[DMIC_DMA_MAX_XFER];
__attribute__(( aligned(16) )) dma_descriptor_t dma_desc_b[DMIC_DMA_MAX_XFER];

// Samples from DMIC are placed on these buffers
int16_t rx_q15_buffer_a[BUFFER_LENGTH] = {0};
int16_t rx_q15_buffer_b[BUFFER_LENGTH] = {0};

static dma_handle_t dma_handle;

int16_t * curr_buffer = rx_q15_buffer_a;

bool rx_full_flag = 0;
bool dsp_run_flag = 0;

uint32_t rx_overrun = 0;

static char *xlabels[] = {"20Hz", "50", "100", "200", "500", "1K", "2K", "5K", "10K", "20K", 0U};
static char *ylabels[] = {"0 dB", "-10", "-20", "-30", "-40", "-50", "-60", "-70", "-80", "-90", 0U};

static point16_t graph_p[GRAPH_NPOINTS] = {0};
static uint32_t graph_i[GRAPH_NPOINTS] = {0};

static const struct gfx_graph_desc my_gd = {
		.xlabels = &xlabels,
		.pxlabels = {.x = 30, .y= 261},

		.ylabels = &ylabels,
		.pylabels = {.x = 3, .y = 8},

		.xdiv = 7,
		.ydiv = 3,

		.pos = {.x = 30, .y = 5},
		.size = {.x = 420, .y = 250},

		.points = &graph_p,
		.index = &graph_i,
		.npoints = GRAPH_NPOINTS,

		.color = 120,
		.border_color = 0xFF,
		.div_color = 10
};

/*******************************************************************************
 * Code
 ******************************************************************************/

extern uint32_t touch_x, touch_y;

void audio_task(void *pvParameters)
{
	vTaskDelay(100);

	// Clear all LCD framebuffer to black
	gfx_fill_rect(POINT16(0, 0), POINT16(480, 272), 0x00);

	gfx_init_dma();

	gfx_graph_init(&my_gd);

	/* DMIC DMA Channel 17 */
	DMA_EnableChannel(DMA0, DMAREQ_DMIC1);
	DMA_CreateHandle(&dma_handle, DMA0, DMAREQ_DMIC1);
	DMA_SetCallback(&dma_handle, DMIC_DMA_Callback, NULL);
	NVIC_SetPriority(DMA0_IRQn, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY);

	Setup_DMIC_DMA();
	volatile uint32_t sum = 0;

	DMA_StartTransfer(&dma_handle);

	EventBits_t event_bits;

	while (1)
	{
		event_bits = xEventGroupWaitBits(event_group,    /* The event group handle. */
		                                         B0 | B1,        /* The bit pattern the event group is waiting for. */
		                                         pdTRUE,         /* BIT_0 and BIT_4 will be cleared automatically. */
		                                         pdFALSE,        /* Don't wait for both bits, either bit unblock task. */
		                                         portMAX_DELAY); /* Block indefinitely to wait for the condition to be met. */

		rx_full_flag = 0;
		dsp_run_flag = 1;

		sum=0;
		uint32_t i=0;

		// Calculate squared value of each sample and add together
		while (i<BUFFER_LENGTH) {
			sum+=(curr_buffer[i] * curr_buffer[i++]) >> 4;
			sum+=(curr_buffer[i] * curr_buffer[i++]) >> 4;
			sum+=(curr_buffer[i] * curr_buffer[i++]) >> 4;
			sum+=(curr_buffer[i] * curr_buffer[i++]) >> 4;
		}

		uint32_t yy = (uint32_t) asm_log2((float) sum) * 6.0f;

		if (yy > 250) yy = 250;

		gfx_fill_rect((point16_t) {x:460,y:5}, (point16_t) {x:10,y:250-yy}, 91);
		gfx_fill_rect((point16_t) {x:460,y:5+250-yy}, (point16_t) {x:10,y:yy}, 255);

		int16_t y;


		// Clear graph area
		gfx_load_rect_dma(my_gd.pos, my_gd.size, scratch_buffer);
		//gfx_fill_rect_dma(my_gd.pos, my_gd.size, 0);

		for (uint32_t i=0; i<my_gd.npoints; i++) {
			y = (curr_buffer[my_gd.index[i]]>>4) + my_gd.size.y/2;
			if (y > my_gd.size.y - 1) my_gd.points[i].y = my_gd.size.y + my_gd.pos.y - 1;
			else if(y < 0) my_gd.points[i].y = my_gd.pos.y;
			else my_gd.points[i].y = y + my_gd.pos.y;
		}

		gfx_wait_dma();

		if (touch_x > my_gd.pos.x && touch_x < my_gd.pos.x + my_gd.size.x &&
				touch_y > my_gd.pos.y && touch_y < my_gd.pos.y + my_gd.size.y) {
			gfx_draw_hline(POINT16(my_gd.pos.x, touch_y), my_gd.size.x-1, 68);
			gfx_draw_vline(POINT16(touch_x, my_gd.pos.y), my_gd.size.y-1, 68);
		}

		gfx_draw_graph(&my_gd);

		dsp_run_flag = 0;
	}

	vTaskSuspend(NULL);
}

/* DMIC user callback */
void DMIC_DMA_Callback(dma_handle_t *handle, void *param, bool transferDone, uint32_t intAB)
{
	if (transferDone) {
		if (dsp_run_flag) {
			rx_overrun++;
		}

		if (intAB == kDMA_IntA) {
			curr_buffer = rx_q15_buffer_a;
		} else {
			curr_buffer = rx_q15_buffer_b;
		}
		rx_full_flag = 1;
		xEventGroupSetBitsFromISR(event_group, B0, NULL);
	}
}

/*
 * Setup DMA xfer descriptor chain to act as ping-pong buffer sampling from DMIC
 *
 *  .--------------------.
 *  | DMA xfer Head      |-----------------. - - - - - - - - - - - - - - - - - - - - - - -
 *  | Same dma_desc_a[0] |                 |                (if N==1)                     .
 *  '--------------------'                 |                                              .
 *                                         |                                              .
 *  DMA Transfer descriptors:              V                                              .
 *  .-----------------.            .-----------------.            .-----------------.     .
 *  |  dma_desc_a[0]  |----------->|  dma_desc_a[1]  |-- (...) -->| dma_desc_a[N-1] |     .
 *  |  0..1023        |            |  1023..2047     |            |   INTERRUPT A   |     .
 *  '-----------------'.           '-----------------'            '-----------------'     .
 *           ^          '-----------------------------------------.      |                .
 *           |                          (if N==1)                  v     v                .
 *  .-----------------.            .-----------------.            .-----------------.     .
 *  | dma_desc_b[N-1] |<- (...) ---|  dma_desc_b[1]  |<-----------|  dma_desc_b[0]  |< - -
 *  |   INTERRUPT B   |            |  1023..2047     |            |  0..1023        |
 *  '-----------------'            '-----------------'            '-----------------'
 *
 *  Where: N is handle->xfer_num
 *         1024 is handle->xfer_length (DMA_MAX_XFER_COUNT)
 *  */

void Setup_DMIC_DMA(void)
{
	uint32_t xfer_length;        /*!< Length of each transfer must be < 0x400 */
	uint32_t xfer_num;

	// Calculate no. of xfer needed (Only contempled for buffer length multiple of 1024)
	if (BUFFER_LENGTH > DMA_MAX_TRANSFER_COUNT)
	{
		xfer_num = BUFFER_LENGTH / DMA_MAX_TRANSFER_COUNT;
		xfer_length = DMA_MAX_TRANSFER_COUNT;
	} else {
		xfer_num = 1;
		xfer_length = BUFFER_LENGTH;
	}

	// Pointer to read DMIC samples
	uint8_t * pFifo = (uint8_t *) &(DMIC0->CHANNEL[DMIC_CHANNEL].FIFO_DATA);

	// Transfer Config (hi-level structure) for Buffer A End
	dma_xfercfg_t dma_xfercfg_a = {
			.byteWidth = 2, .reload = 1,
			.clrtrig   = 0, .swtrig = 0,
			.dstInc    = 1, .srcInc = 0,
			.intA      = 1,	.intB = 0,
			.valid     = 1,
			.transferCount = xfer_length
	};

	// Transfer Config (hi-level structure) for Buffer B End
	dma_xfercfg_t dma_xfercfg_b = {
			.byteWidth = 2, .reload = 1,
			.clrtrig   = 0, .swtrig = 0,
			.dstInc    = 1, .srcInc = 0,
			.intA      = 0,	.intB = 1,
			.valid     = 1,
			.transferCount = xfer_length
	};

	// Transfer Config (hi-level structure) for Buffer A,B middle xfers
	dma_xfercfg_t dma_xfercfg_mid = {
			.byteWidth = 2, .reload = 1,
			.clrtrig   = 0, .swtrig = 0,
			.dstInc    = 1, .srcInc = 0,
			.intA      = 0,	.intB = 0,
			.valid     = 1,
			.transferCount = xfer_length
	};

	// Descriptor (lo-level) for buffer A last transfer
	DMA_CreateDescriptor(
			&(dma_desc_a[xfer_num-1]),
			&dma_xfercfg_a, pFifo,
			&rx_q15_buffer_a[xfer_length*(xfer_num-1)],
			&(dma_desc_b[0]));

	// Descriptor (lo-level) for buffer B last transfer
	DMA_CreateDescriptor(
			&(dma_desc_b[xfer_num-1]),
			&dma_xfercfg_b, pFifo,
			&rx_q15_buffer_b[xfer_length*(xfer_num-1)],
			&(dma_desc_a[0]));

	// Descriptors (lo-level) for A,B first & middle transfers
	for (uint32_t i=0; i<(xfer_num-1); i++)
	{
		DMA_CreateDescriptor(
				&(dma_desc_a[i]),
				&dma_xfercfg_mid, pFifo,
				&rx_q15_buffer_a[xfer_length*i],
				&(dma_desc_a[i+1]));

		DMA_CreateDescriptor(
				&(dma_desc_b[i]),
				&dma_xfercfg_mid, pFifo,
				&rx_q15_buffer_b[xfer_length*i],
				&(dma_desc_b[i+1]));
	}

	// Initialize DMA transfer head here
	// No need to call DMA_PrepareTransfer()
	dma_transfer_config_t dma_xfer_config = {
			.isPeriph = true,
			.srcAddr = pFifo,
			.dstAddr = (uint8_t*) rx_q15_buffer_a,
			.xfercfg = dma_xfercfg_a,
			.nextDesc = (xfer_num == 1) ? (uint8_t*) &(dma_desc_b) : (uint8_t*) &(dma_desc_a[1])
	};

	// Put transfer header on their private array
	DMA_SubmitTransfer(&dma_handle, &dma_xfer_config);
}

void gfx_graph_init(struct gfx_graph_desc * gd) {

	// Draw graph labels
	gfx_text_color = 0xFF;

	uint32_t i;
	point16_t pa, pb;

	i=0;
	pa = gd->pxlabels;

	while (gd->xlabels[i]) {
		gfx_draw_string(pa, gd->xlabels[i], 0xFF);
		pa.x+=45;
		i++;
	}

	pa = gd->pylabels;
	i=0;

	while (gd->ylabels[i]) {
		gfx_draw_string(pa, gd->ylabels[i], 0xFF);
		pa.y+=25;
		i++;
	}

	// Draw graph box
	gfx_draw_rect(POINT16(gd->pos.x-1, gd->pos.y-1), POINT16(gd->size.x+1, gd->size.y+1), gd->border_color);

	pa = POINT16(gd->pos.x, gd->pos.y);
	pb = POINT16(gd->pos.x + gd->size.x, gd->pos.y);

	for (i=0; i<gd->ydiv; i++) {

		pa.y += gd->size.y / (gd->ydiv + 1);
		pb.y += gd->size.y / (gd->ydiv + 1);
		gfx_draw_line_pattern(pa, pb, gd->div_color, 0xF0F0F0F0);
	}

	pa = POINT16(gd->pos.x, gd->pos.y);
	pb = POINT16(gd->pos.x, gd->pos.y  + gd->size.y);

	for (i=0; i<gd->xdiv; i++) {

		pa.x += gd->size.x / (gd->xdiv + 1);
		pb.x += gd->size.x / (gd->xdiv + 1);
		gfx_draw_line_pattern(pa, pb, gd->div_color, 0xF0F0F0F0);
	}

	gfx_save_rect_dma(gd->pos, gd->size, scratch_buffer);
	gfx_wait_dma();

	// Icons on top of graph background
	// No icons for now.
	//vpm_draw_bmp(VPM_GRAPH_X+VPM_GRAPH_WIDTH-24, VPM_GRAPH_Y, 24, 24, icon_gear);
	//vpm_gfx_dma_wait();
	//vpm_draw_bmp(VPM_GRAPH_X+VPM_GRAPH_WIDTH-48, VPM_GRAPH_Y, 24, 24, icon_info);
	//vpm_gfx_dma_wait();


	// Initialize graph values to zero
	for (uint32_t i=0; i<gd->npoints; i++) {
		gd->points[i].x = gd->pos.x + 1 + gd->size.x*i/gd->npoints;
		gd->index[i] = i*(BUFFER_LENGTH/(gd->npoints*4));
		gd->points[i].y = gd->pos.y + gd->size.y - 1;
	}


	gfx_draw_graph(gd);
}

void gfx_draw_graph(struct gfx_graph_desc * gd)
{
	for (uint32_t i=1; i<gd->npoints; i++) {
		gfx_draw_line(gd->points[i-1], gd->points[i], gd->color);
	}
}

