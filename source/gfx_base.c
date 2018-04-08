#include "gfx_base.h"

extern uint8_t gfx_buffer[];

struct gfx_font * gfx_current_font = &Font12; // Selected font
uint32_t gfx_fill_color = 0x01;
uint32_t gfx_text_color = 0x07;
uint32_t gfx_back_color = 0x00;

void gfx_draw_pixel(point16_t p, uint8_t color)
{
	gfx_buffer[p.x + p.y * GFX_WIDTH] = color;
}

#define ABS(X)  ((X) > 0 ? (X) : -(X))
void gfx_draw_line(point16_t p0, point16_t p1, uint8_t color)
{
	point16_t pinc;
	int16_t deltax = 0, deltay = 0, num = 0, numpixels = 0;

	deltax = ABS(p1.x - p0.x);
	deltay = ABS(p1.y - p0.y);

	pinc.x = (p1.x >= p0.x) ? 1 : -1;
	pinc.y = (p1.y >= p0.y) ? 1 : -1;

	if (deltax >= deltay)
	{
		num = deltax / 2;
		numpixels = deltax;

		do {
			gfx_draw_pixel(p0, color);
			num += deltay;
			if (num >= deltax) {
				num -= deltax;
				p0.y += pinc.y;
			}
			p0.x += pinc.x;
		} while (numpixels--);

	} else {
		num = deltay / 2;
		numpixels = deltay;

		do {
			gfx_draw_pixel(p0, color);
			num += deltax;
			if (num >= deltay) {
				num -= deltay;
				p0.x += pinc.x;
			}
			p0.y += pinc.y;
		} while (numpixels--);
	}
}

void gfx_draw_line_pattern(point16_t p0, point16_t p1, uint8_t color, uint32_t pattern)
{
	point16_t pinc;
	int16_t deltax = 0, deltay = 0, num = 0, numpixels = 0;

	deltax = ABS(p1.x - p0.x);
	deltay = ABS(p1.y - p0.y);

	pinc.x = (p1.x >= p0.x) ? 1 : -1;
	pinc.y = (p1.y >= p0.y) ? 1 : -1;

	if (deltax >= deltay)
	{
		num = deltax / 2;
		numpixels = deltax;

		do {
			if ((pattern >> (numpixels % 32) ) & 0x01)
				gfx_draw_pixel(p0, color);
			num += deltay;
			if (num >= deltax) {
				num -= deltax;
				p0.y += pinc.y;
			}
			p0.x += pinc.x;
		} while (numpixels--);

	} else {
		num = deltay / 2;
		numpixels = deltay;

		do {
			if ((pattern >> (numpixels % 32) ) & 0x01)
				gfx_draw_pixel(p0, color);
			num += deltax;
			if (num >= deltay) {
				num -= deltay;
				p0.x += pinc.x;
			}
			p0.y += pinc.y;
		} while (numpixels--);
	}
}

void gfx_fill_rect(point16_t p, point16_t size, uint8_t color) {
	point16_t pm;
	for (pm.x=p.x;pm.x<p.x+size.x; pm.x++)
		for (pm.y=p.y;pm.y<p.y+size.y; pm.y++)
			gfx_draw_pixel(pm, color);
}

/* TEXT SUBROUTINES
 * Adapted from STM32 example code
 * See font12.c for license
 * */
void gfx_draw_char(point16_t p, char ch, uint8_t color)
{
	uint32_t i = 0, j = 0;
	uint8_t  offset;
	uint8_t  *pchar;
	uint32_t line;

	if (ch == '\r' || ch == '\n') return;

	uint16_t height = gfx_current_font->Height;
	uint16_t width  = gfx_current_font->Width;

	uint32_t t_pos = (ch-' ') * gfx_current_font->Height * ((gfx_current_font->Width + 7) / 8);

	const uint8_t * c = &gfx_current_font->table[t_pos];

	offset =  8 *((width + 7)/8) -  width ;

	for(i = 0; i < height; i++)
	{
		pchar = ((uint8_t *)c + (width + 7)/8 * i);

		switch(((width + 7)/8))
		{

		case 1:
			line =  pchar[0];
			break;

		case 2:
			line =  (pchar[0]<< 8) | pchar[1];
			break;

		case 3:
		default:
			line =  (pchar[0]<< 16) | (pchar[1]<< 8) | pchar[2];
			break;
		}

		for (j = 0; j < width; j++) {
			if(line & (1 << (width- j + offset- 1))) {
				gfx_draw_pixel((point16_t) {x: p.x+j, y: p.y}, color);
			}
		}
		p.y++;
	}
}

void gfx_draw_string(point16_t p, char * str, enum gfx_text_align Mode)
{
	int32_t refcolumn = 0;
	uint32_t i = 0;
	uint32_t size = 0;
	uint8_t  *ptr = (uint8_t *)str;

	/* Get the text size */
	while (*ptr++) size ++ ;

	switch (Mode) {
	case CENTER_ALIGN:
		p.x = p.x - size * gfx_current_font->Width / 2;
		break;
	case RIGHT_ALIGN:
		p.x = - p.x - size * gfx_current_font->Width;
		break;
	default:
		break;
	}

	refcolumn = refcolumn < 0 ? 0 : refcolumn;

	point16_t p2 = p;

	/* Send the string character by character on LCD */
	while ((*str != 0) & (((GFX_WIDTH - (i*gfx_current_font->Width)) & 0xFFFF) >= gfx_current_font->Width))
	{
		if(*str == '\n') {
			p2.y += gfx_current_font->Height;
		} else if (*str == '\r') {
			p2.x=p.x;
			i=0;
		} else if (*str == '\t') {
			p2.x += gfx_current_font->Width;
			i++;
		} else {
			gfx_draw_char(p2, *(uint8_t *)str, gfx_text_color);
			p2.x += gfx_current_font->Width;
			i++;
		}
		str++;
	}
}

void gfx_draw_string_at(uint16_t line, uint16_t col, char *ptr)
{
  gfx_draw_string(
		  (point16_t) {x: col * gfx_current_font->Width,
					   y: line * gfx_current_font->Height},
	  ptr, LEFT_ALIGN);
}

void gfx_draw_hline(point16_t p, uint16_t len, uint8_t color)
{
	do {
		gfx_draw_pixel(p, color);
		p.x++;
	} while (len--);
}

void gfx_draw_vline(point16_t p, uint16_t len, uint8_t color)
{
	do {
		gfx_draw_pixel(p, color);
		p.y++;
	} while (len--);
}

void gfx_draw_rect(point16_t p, point16_t size, uint8_t color)
{
	gfx_draw_hline(p, size.x, color);
	gfx_draw_vline(p, size.y, color);
	gfx_draw_hline(POINT16(p.x, p.y + size.y), size.x, color);
	gfx_draw_vline(POINT16(p.x + size.x, p.y), size.y, color);
}

