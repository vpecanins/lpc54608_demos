#include "gfx_base.h"

extern color_t gfx_buffer[];

struct gfx_font * gfx_current_font = &Font12; // Selected font
color_t gfx_fill_color = 0x01;
color_t gfx_text_color = 0x07;
color_t gfx_back_color = 0x00;

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

void gfx_draw_string(point16_t p, char * str, color_t color)
{
	uint32_t i = 0;
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
			gfx_draw_char(p2, *(uint8_t *)str, color);
			p2.x += gfx_current_font->Width;
			i++;
		}
		str++;
	}
}

void gfx_draw_string_center(point16_t p, char * str, color_t color)
{
	uint32_t size = 0;
	uint8_t  *ptr = (uint8_t *)str;

	/* Get the text size */
	while (*ptr++) size ++ ;

	p.x = p.x - size * gfx_current_font->Width / 2;

	gfx_draw_string(p, str, color);

	//case RIGHT_ALIGN:
	//	p.x = - p.x - size * gfx_current_font->Width;

}

void gfx_draw_string_at(uint16_t line, uint16_t col, char *ptr, color_t color)
{
  gfx_draw_string(
	  POINT16( col * gfx_current_font->Width, line * gfx_current_font->Height), ptr, color);
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

//
// DMA Routines
//

#include "fsl_dma.h"

__attribute__(( aligned(16) ))
dma_descriptor_t gfx_dma_xfers[GFX_HEIGHT];

dma_handle_t gfx_dma_handle;
/*
void gfx_copy_rect_dma(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint32_t * bmp)
{
	dma_transfer_config_t transferConfig0;

	assert(height >= 0 && height <= GFX_HEIGHT);
	assert(width >= 0 && width <= GFX_WIDTH);

	uint32_t is_multi = (height>1);

	// check max
	assert((width / 4 <= DMA_MAX_TRANSFER_COUNT));

	// DMA Transfer configuration (Header)
	// Will clrtrig, intA and not reload by default,
	// (Unless is_multi)
	dma_xfercfg_t xfer_cfg = {
			.srcInc = 1,
			.dstInc = 1,
			.transferCount = width,
			.byteWidth = 4,
			.intA = true,
			.intB = false,
			.clrtrig = true,
			.swtrig = true,
			.reload = false,
			.valid = true
	};

	transferConfig0.xfercfg = xfer_cfg;

	// Continue with other segments
	// The rest (height-1) lines are done using a linked list of DMA transfer descriptors
	if (is_multi) {
		xfer_cfg.intA = false;
		xfer_cfg.clrtrig = false;
		xfer_cfg.reload = true;

		// Update the header
		transferConfig0.xfercfg = xfer_cfg;

		// In the middle, (height-2) transfers are done in a for loop
		uint32_t i;
		for (i=0; i<height-1; i++) {
			DMA_CreateDescriptor(
					&gfx_dma_xfers[i],
					&xfer_cfg,
					(uint8_t*)&(bmp[i*width]),
					(uint8_t*)&(gfx_buffer[y+i][x]),
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
				(uint8_t*)&(bmp[i*width]),
				(uint8_t*)&(gfx_buffer[y+i][x]),
				NULL
		);
	}

	// Use this custom function to directly submit the first descriptor
	DMA_SubmitDescriptor(&gfx_dma_handle, &gfx_dma_xfers[0], false);

	// Trigger first DMA transfer
	DMA_StartTransfer(&gfx_dma_handle);
}
*/
