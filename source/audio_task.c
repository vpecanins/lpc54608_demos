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

/*******************************************************************************
 * Definitions
 ******************************************************************************/

#define DMAREQ_DMIC0 16U // Not used
#define DMAREQ_DMIC1 17U

#define BUFFER_LENGTH (2048*4)
#define DMIC_CHANNEL 1
#define DMIC_DMA_MAX_XFER 8 // Maximum FFT size is 8*1024

/*******************************************************************************
 * Prototypes
 ******************************************************************************/
void DMIC_DMA_Callback(dma_handle_t *, void *, bool, uint32_t);
void Setup_DMIC_DMA(void);

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

EventGroupHandle_t rx_full_event = NULL;
bool rx_full_flag = 0;

/*******************************************************************************
 * Code
 ******************************************************************************/

void audio_task(void *pvParameters)
{


	vTaskDelay(100);

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
	        event_bits = xEventGroupWaitBits(rx_full_event,  // The event group handle.
	                                         (1<<0)|(1<<1),  // The bit pattern the event group is waiting for.
	                                         pdTRUE,         // bits will be cleared automatically.
	                                         pdFALSE,        // Don't wait for both bits, either bit unblock task
	                                         portMAX_DELAY); // Block indefinitely to wait for the condition to be met.

	    	/*while (!rx_full_flag) {
	    		vTaskDelay(50);
	    	}

	    	rx_full_flag = 0;*/

			sum=0;
			for (uint32_t i=0; i<BUFFER_LENGTH; i++) {
				sum+=(curr_buffer[i] * curr_buffer[i++]) >> 8;
				sum+=(curr_buffer[i] * curr_buffer[i++]) >> 8;
				sum+=(curr_buffer[i] * curr_buffer[i++]) >> 8;
				sum+=(curr_buffer[i] * curr_buffer[i++]) >> 8;
			}
			printf("RX %d\r\n", sum);

	}

    vTaskSuspend(NULL);
}

/* DMIC user callback */
void DMIC_DMA_Callback(dma_handle_t *handle, void *param, bool transferDone, uint32_t intAB)
{
	BaseType_t xHigherPriorityTaskWoken=0, xResult;

	if (transferDone) {
		if (intAB == kDMA_IntA) {
			curr_buffer = rx_q15_buffer_a;
		} else {
			curr_buffer = rx_q15_buffer_b;
		}
		rx_full_flag = 1;

		xResult = xEventGroupSetBitsFromISR(
				rx_full_event,
				1<<intAB,
				&xHigherPriorityTaskWoken );

		if( xResult != pdFAIL ) portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
	}
}

/*
 * Setup DMA xfer descriptor chain to act as ping-pong buffer sampling from DMIC
 *
 *  .--------------------.
 *  | DMA xfer Head      |-----------------. - - - - - - - - - - - - - - - - - - - - - - -
 *  | Like dma_desc_a[0] |                 |                (if N==1)                     .
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

    // Calculate no. of xfer needed (Only contempled for length=2^N)
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
			.dstAddr = rx_q15_buffer_a,
			.xfercfg = dma_xfercfg_a,
			.nextDesc = (xfer_num == 1) ? &(dma_desc_b) : &(dma_desc_a[1])
    };

    // Put transfer header on their private array
    DMA_SubmitTransfer(&dma_handle, &dma_xfer_config);
}
