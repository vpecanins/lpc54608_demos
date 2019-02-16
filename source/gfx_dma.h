/*
 * gfx_dma.h
 *
 *  Created on: Feb 16, 2019
 *      Author: peca
 */

#include "gfx_base.h"
#include "fsl_dma.h"

#ifndef GFX_DMA_H_
#define GFX_DMA_H_

/* DMA Handle */
extern dma_handle_t gfx_dma_handle;

void gfx_init_dma(void);

static inline void gfx_wait_dma() {
	while (DMA_ChannelIsActive(gfx_dma_handle.base, gfx_dma_handle.channel)) {;}
}

void gfx_fill_rect_dma(point16_t p, point16_t size, uint8_t color);
void gfx_save_rect_dma(point16_t p, point16_t size, uint8_t * ptr);
void gfx_load_rect_dma(point16_t p, point16_t size, uint8_t * ptr);

#endif /* GFX_DMA_H_ */
