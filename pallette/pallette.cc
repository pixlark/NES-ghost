#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

struct ARGB {
	uint8_t a;
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

inline ARGB parse_hex_code(uint32_t color)
{
	ARGB argb;
	argb.a = (color & 0xFF000000) >> 3 * 8;
	argb.r = (color & 0x00FF0000) >> 2 * 8;
	argb.g = (color & 0x0000FF00) >> 1 * 8;
	argb.b = (color & 0x000000FF);
	return argb;
}

inline uint32_t make_hex_code(ARGB argb)
{
	uint32_t code = 0;
	code |= argb.a << 3 * 8;
	code |= argb.r << 2 * 8;
	code |= argb.g << 1 * 8;
	code |= argb.b;
	return code;
}

struct ColorLink {
	ARGB color;
	ColorLink * next;
};

void parse_line(ColorLink * l, FILE * pal) {
	char hex [9];
	for (int i = 0; i < 8; i++) {
		hex[i] = fgetc(pal);
	}
	hex[8] = '\0';
	printf("#%s - ", hex);
	uint32_t code;
	sscanf(hex, "%8x", &code);
	printf("#%08x - ", code);
	ARGB color = parse_hex_code(code);
	printf("%02x %02x %02x %02x\n", color.a, color.r, color.g, color.b);
	l->color = color;
	if (fgetc(pal) == '\n') {
		l->next = NULL;
		return;
	}
	l->next = (ColorLink*) malloc(sizeof(ColorLink));
	parse_line(l->next, pal);
}

ARGB get_pixel(uint8_t * data, int x, int y, int w)
{
	ARGB color;
	color.r = data[(x * 4) + (y * 4 * w) + 0];
	color.g = data[(x * 4) + (y * 4 * w) + 1];
	color.b = data[(x * 4) + (y * 4 * w) + 2];
	color.a = data[(x * 4) + (y * 4 * w) + 3];
	return color;
}

void set_pixel(uint8_t * data, int x, int y, int w, ARGB color)
{
	data[(x * 4) + (y * 4 * w) + 0] = color.r;
	data[(x * 4) + (y * 4 * w) + 1] = color.g;
	data[(x * 4) + (y * 4 * w) + 2] = color.b;
	data[(x * 4) + (y * 4 * w) + 3] = color.a;
}

inline int find_color(ColorLink * links, int link_count, ARGB color)
{
	for (int i = 0; i < link_count; i++) {
		ARGB a = links[i].color;
		if (a.a == color.a &&
			a.r == color.r &&
			a.g == color.g &&
			a.b == color.b) {
			return i;
		}
	}
	return -1;
}

int main(int argc, char ** argv)
{
	/* TODO(pixlark): Should probably make this program, you know,
	 * SAFE, at some point. Right now almost all undefined input
	 * results in undefined behaviour.
	 *     -Paul T. Fri Feb 23 04:12:45 2018 */
	if (argc < 3) {
		printf("Specify pallette file and expansion locations.");
		return 1;
	}
	// Load image
	int image_w, image_h, comp;
	uint8_t * atlas_data = stbi_load("atlas.png", &image_w, &image_h, &comp, 4);
	printf("%d wide, %d tall, %d bytes per pixel\n", image_w, image_h, comp);
	// Load pallette file
	FILE * pal = fopen(argv[1], "r");
	if (pal == NULL) {
		printf("Specify a valid pallette file please.");
		return 1;
	}
	int lines = 0;
	{
		char c;
		while ((c = fgetc(pal)) != EOF)
			if (c == '\n') lines++;
		fseek(pal, 0, SEEK_SET);
	}
	printf("%d lines in pallette file.\n", lines);
	ColorLink * links = (ColorLink*) malloc(sizeof(ColorLink) * lines);
	for (int i = 0; i < lines; i++) {
		ColorLink * l = links + i;
		parse_line(l, pal);
		ColorLink * t = l;
		while (t != NULL) {
			printf("%02x %02x %02x -> ", t->color.r, t->color.g, t->color.b);
			t = t->next;
		}
		printf("\n");
	}
	// Expand from locations
	for (int i = 2; i < argc; i++) {
		int ix, iy, w, h, num;
		if (sscanf(argv[i], "%d,%d,%d,%d,%d", &ix, &iy, &w, &h, &num) != 5) {
			printf("Specify each expansion with x,y,w,h,count\n");
			return 1;
		}
		printf("Expanding %d out from %d, %d (%dx%d)\n", num, ix, iy, w, h);
		// For each row
		for (int y = iy; y < iy + h; y++) {
			// For each pixel
			for (int x = ix; x < ix + w; x++) {
				ARGB color = get_pixel(atlas_data, x, y, image_w);
				int pos = find_color(links, lines, color);
				ColorLink * t = pos == -1 ? NULL : links + pos;
				// For each expansion
				for (int n = 1; n <= num; n++) {
					ARGB mod_color;
					if (pos == -1) {
						mod_color = color;
					} else {
						mod_color = t->next->color;
						t = t->next;
					}
					set_pixel(atlas_data, x + (n * w), y, image_w, mod_color);
				}
			}
		}
	}
	// Write image back
	stbi_write_png("expanded.png", image_w, image_h, comp, atlas_data, image_w * comp);
	return 0;
}
