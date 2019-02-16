#include "gfx_base.h"
#include "fsl_dma.h"
#include "gfx_dma.h"

__attribute__(( aligned(16) ))
dma_descriptor_t gfx_dma_xfers[GFX_HEIGHT];

dma_handle_t gfx_dma_handle;

void GFX_DMA_Callback(dma_handle_t *handle, void *param, bool transferDone, uint32_t tcds)
{
	if (transferDone)
	{
		// Unused
	}
}

void gfx_init_dma(void)
{
	/* GFX mem2mem DMA Channel 29 */
	DMA_EnableChannel(DMA0, 29);
	DMA_CreateHandle(&gfx_dma_handle, DMA0, 29);
	DMA_SetCallback(&gfx_dma_handle, GFX_DMA_Callback, NULL);
}

static void gfx_xfer_dma(point16_t p, point16_t size, uint8_t* p_src, uint8_t* p_dst,  uint8_t src_inc)
{
	assert(size.y >= 0 && size.y <= GFX_HEIGHT);
	assert(size.x >= 0 && size.x <= GFX_WIDTH);

	uint32_t is_multi = (size.y>1);

	// check max
	assert((size.x / 4 <= DMA_MAX_TRANSFER_COUNT));

	// DMA Transfer configuration (Header)
	// Will clrtrig, intA and not reload by default,
	// (Unless is_multi)
	dma_xfercfg_t xfer_cfg = {
			.srcInc = src_inc,
			.dstInc = 1,
			.transferCount = size.x,
			.byteWidth = 1,
			.intA = true,
			.intB = false,
			.clrtrig = true,
			.swtrig = true,
			.reload = false,
			.valid = true
	};

	// Continue with other segments
	// The rest (height-1) lines are done using a linked list of DMA transfer descriptors
	if (is_multi) {
		xfer_cfg.intA = false;
		xfer_cfg.clrtrig = false;
		xfer_cfg.reload = true;

		// In the middle, (height-2) transfers are done in a for loop
		uint32_t i;
		for (i=0; i<size.y-1; i++) {
			DMA_CreateDescriptor(
					&gfx_dma_xfers[i],
					&xfer_cfg,
					src_inc ? &p_src[p.x + (p.y + i) * GFX_WIDTH] : p_src,
					&p_dst[p.x + (p.y + i) * GFX_WIDTH],
					&gfx_dma_xfers[i+1]
			);
		}

		// The last transfer is done outside the for loop
		xfer_cfg.clrtrig = true;
		xfer_cfg.reload = false;
		xfer_cfg.intA = true;

		DMA_CreateDescriptor(
				&gfx_dma_xfers[i],
				&xfer_cfg,
				src_inc ? &p_src[p.x + (p.y + i) * GFX_WIDTH] : p_src,
				&p_dst[p.x + (p.y + i) * GFX_WIDTH],
				NULL
		);
	}

	// Use this custom function to directly submit the first descriptor
	DMA_SubmitDescriptor(&gfx_dma_handle, &gfx_dma_xfers[0], false);

	// Trigger first DMA transfer
	DMA_StartTransfer(&gfx_dma_handle);
}

void gfx_fill_rect_dma(point16_t p, point16_t size, uint8_t color)
{
	static uint8_t fill_color;

	fill_color = color; // To survive after function has finished

	gfx_xfer_dma(p, size, &fill_color, gfx_buffer, 0);
}

void gfx_save_rect_dma(point16_t p, point16_t size, uint8_t * ptr)
{
	gfx_xfer_dma(p, size, gfx_buffer, ptr, 1);
}

void gfx_load_rect_dma(point16_t p, point16_t size, uint8_t * ptr)
{
	gfx_xfer_dma(p, size, ptr, gfx_buffer, 1);
}
