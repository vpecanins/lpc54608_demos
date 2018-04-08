#ifndef __GFX_BASE_H
#define __GFX_BASE_H

#ifdef __cplusplus
 extern "C" {
#endif

#include <stdint.h>

#define GFX_HEIGHT 272
#define GFX_WIDTH 480



 struct gfx_font {
	 const uint8_t *table;
	 uint16_t Width;
	 uint16_t Height;
 };

 enum gfx_text_align
 {
	 CENTER_ALIGN             = 0x01,    /* Center mode */
	 RIGHT_ALIGN              = 0x02,    /* Right mode  */
	 LEFT_ALIGN               = 0x03     /* Left mode   */
 };

 typedef union {
	 struct {
		 uint16_t x;
		 uint16_t y;
	 };
	 uint32_t uval;
 } point16_t;

#define POINT16(xa,ya) (point16_t){x: (xa), y: (ya)}

void gfx_draw_pixel(point16_t p, uint8_t color);
void gfx_draw_line(point16_t p0, point16_t p1, uint8_t color);
void gfx_draw_line_pattern(point16_t p0, point16_t p1, uint8_t color, uint32_t pattern);

void gfx_draw_hline(point16_t p, uint16_t len, uint8_t color);
void gfx_draw_vline(point16_t p, uint16_t len, uint8_t color);
void gfx_draw_rect(point16_t p, point16_t size, uint8_t color);

void gfx_fill_rect(point16_t p, point16_t size, uint8_t color);

void gfx_draw_char(point16_t p, char ch, uint8_t color);
void gfx_draw_string(point16_t p, char * str, enum gfx_text_align Mode);
void gfx_draw_string_at(uint16_t line, uint16_t col, char *ptr);

extern struct gfx_font Font24;
extern struct gfx_font Font20;
extern struct gfx_font Font16;
extern struct gfx_font Font12;
extern struct gfx_font Font8;

extern struct gfx_font * gfx_current_font;
extern uint32_t gfx_fill_color;
extern uint32_t gfx_text_color;
extern uint32_t gfx_back_color;

#ifdef __cplusplus
}
#endif

#endif
