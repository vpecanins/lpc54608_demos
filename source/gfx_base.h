#ifndef __GFX_BASE_H
#define __GFX_BASE_H

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdint.h>
#include "fsl_dma.h"

#define GFX_HEIGHT 272
#define GFX_WIDTH 480

 struct gfx_font {
	 const uint8_t *table;
	 uint16_t Width;
	 uint16_t Height;
 };

 // Point: X & Y are 16-bit values packed on a single 32-bit word
 typedef union {
	 struct {
		 uint16_t x;
		 uint16_t y;
	 };
	 uint32_t uval;
 } point16_t;

// To avoid having to write (uint16_t) {x: 14, y: 6} every time
#define POINT16(xa,ya) (point16_t){x: (xa), y: (ya)}

// To migrate between RGB323, RGB565, RGB888
typedef uint8_t color_t;

void gfx_draw_pixel(point16_t p, color_t color);

// Line subroutines
void gfx_draw_line(point16_t p0, point16_t p1, color_t color);
void gfx_draw_line_pattern(point16_t p0, point16_t p1, color_t color, uint32_t pattern);
void gfx_draw_hline(point16_t p, uint16_t len, color_t color);
void gfx_draw_vline(point16_t p, uint16_t len, color_t color);

// Rect subroutines
void gfx_draw_rect(point16_t p, point16_t size, color_t color);
void gfx_fill_rect(point16_t p, point16_t size, color_t color);

// Text subroutines
void gfx_draw_char(point16_t p, char ch, color_t color);
void gfx_draw_string(point16_t p, char * str, color_t color);
void gfx_draw_string_center(point16_t p, char * str, color_t color);
void gfx_draw_string_at(uint16_t line, uint16_t col, char *ptr, color_t color);

// DMA subroutines
void gfx_init_dma(void);

void gfx_fill_rect_dma(point16_t p, point16_t size, uint8_t color);
void gfx_save_rect_dma(point16_t p, point16_t size, uint8_t * ptr);
void gfx_load_rect_dma(point16_t p, point16_t size, uint8_t * ptr);

/* DMA Handle */
extern dma_handle_t gfx_dma_handle;

static inline void gfx_wait_dma() {
	while (DMA_ChannelIsActive(gfx_dma_handle.base, gfx_dma_handle.channel)) {;}
}

extern struct gfx_font Font24;
extern struct gfx_font Font20;
extern struct gfx_font Font16;
extern struct gfx_font Font12;
extern struct gfx_font Font8;

extern struct gfx_font * gfx_current_font;
extern color_t gfx_fill_color;
extern color_t gfx_text_color;
extern color_t gfx_back_color;

#ifdef __cplusplus
}
#endif

#endif
