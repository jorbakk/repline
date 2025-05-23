/* ----------------------------------------------------------------------------
  Example that shows the color palette of the terminal
-----------------------------------------------------------------------------*/
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "repline.h"

static const char *patch = "■";

typedef enum color_order_e {
	RGB,
	BGR,
	GRB
} color_order_t;

#include "bbcode_colors.c"

static int
color_weight(rpl_color_t c)
{
	return (int)(c);
}

static int
html_color_compare(const void *p1, const void *p2)
{
	const style_color_t *s1 = (const style_color_t *)p1;
	const style_color_t *s2 = (const style_color_t *)p2;
	int w1 = color_weight(s1->color);
	int w2 = color_weight(s2->color);
	return (w1 - w2);
}

static void
write_html_colors(void)
{
	qsort(html_colors, RPL_HTML_COLOR_COUNT, sizeof(html_colors[0]),
	      &html_color_compare);
	rpl_print("print html colors\n");
	for (int i = 0; i < RPL_HTML_COLOR_COUNT - 1; i++) {
		rpl_printf("[width=10][bgcolor=%s]%s[/][/] ", html_colors[i].name,
		           html_colors[i].name);
		if ((i + 1) % 8 == 0)
			rpl_print("\n\n");
	}
	rpl_println("");
}

static void
write_palette(int order)
{
	rpl_print("\n  // ");
	rpl_print(order == RGB ? "17x9x9" : (order == BGR ? "9x9x17" : "9x17x9"));
	rpl_print("colors");
	for (int x = 0; x <= 256; x += 16) {
		rpl_print("\n  ");
		for (int y = 0; y <= 256; y += 32) {
			for (int z = 0; z <= 256; z += 32) {
				int r, g, b;
				if (order == RGB) {
					r = x;
					g = y;
					b = z;
				}
				if (order == BGR) {
					r = z;
					g = y;
					b = x;
				} else if (order == GRB) {
					r = y;
					g = x;
					b = z;
				}
				if (r == 256)
					r = 255;
				if (g == 256)
					g = 255;
				if (b == 256)
					b = 255;
				rpl_printf("[#%02x%02x%02x]%s[/]", r, g, b, patch);
			}
			rpl_print(" ");
		}
	}
}

static void
show_ansi_color(const char *name, const char *brightname)
{
	rpl_printf("[ansi-%s]%16s[/] | [ansi-%s bold]bold[/] | [ansi-%s]%s[/]\n",
	           name, name, name, brightname, brightname);
}

// main example
int
main()
{
	// how many bits has our palette? (24 bits is good :-)
	rpl_printf("terminal color bits: %d\n", rpl_term_get_color_bits());

	// Write out a palette
	rpl_println("colors rgb:");
	write_palette(RGB);
	write_palette(BGR);
	write_palette(GRB);

	rpl_println
	    ("\n\nansi reds:\n  [ansi-maroon]ansi8-red[/], [ansi-red]ansi16-bright-red[/], [#D70000]ansi256-red160[/], [#fa1754]ansirgb-cherry[/]");

	// Shades
	rpl_println("\nshades:");
	for (int i = 0; i <= 64; i++) {
		rpl_printf("[#%02x0000]%s[/]", (i == 64 ? 255 : i * 4), patch);
	}
	rpl_println("");
	for (int i = 0; i <= 64; i++) {
		rpl_printf("[#00%02x00]%s[/]", (i == 64 ? 255 : i * 4), patch);
	}
	rpl_println("");
	for (int i = 0; i <= 64; i++) {
		rpl_printf("[#0000%02x]%s[/]", (i == 64 ? 255 : i * 4), patch);
	}
	rpl_println("");
	for (int i = 0; i <= 64; i++) {
		int g = (i == 64 ? 255 : i * 4);
		rpl_printf("[#%02x%02x%02x]%s[/]", g, g, g, patch);
	}
	rpl_println("\n");

	// html colors
	write_html_colors();

	// bbcodes
	rpl_println
	    ("\n[b]bold [i]bold and italic[/i] [yellow on blue]yellow on blue in bold[/][/b] default");

	rpl_style_def("em", "underline ansi-olive");
	rpl_style_open("i");
	rpl_print("[em]emphasis[/em]\n");
	rpl_style_close();

	// direct ANSI escapes
	rpl_println("\ndirect ansi escape sequence colors:\n");
	show_ansi_color("black", "gray");
	show_ansi_color("maroon", "red");
	show_ansi_color("green", "lime");
	show_ansi_color("olive", "yellow");
	show_ansi_color("navy", "blue");
	show_ansi_color("purple", "fuchsia");
	show_ansi_color("teal", "aqua");
	show_ansi_color("silver", "white");
	show_ansi_color("default", "default");

	rpl_println("");
	return 0;
}
