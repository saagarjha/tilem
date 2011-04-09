/*
 * TilEm II
 *
 * Copyright (c) 2010-2011 Thibault Duponchelle
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <gtk/gtk.h>
#include <ticalcs.h>
#include <tilem.h>

#include "gui.h"
#include "animatedgif.h"

/* Create an empty gif and add the first frame */ 
void tilem_animation_start(GLOBAL_SKIN_INFOS* gsi) {
	
	/* The 11th byte is a set of flags  : 
	bit 0:    Global Color Table Flag (GCTF)
        bit 1..3: Color Resolution
        bit 4:    Sort Flag to Global Color Table
        bit 5..7: Size of Global Color Table: 2^(1+n)
	It means "use the GCT wich is given after (from the size bit 5..7) and a resolution bit 1..3 
	The Background color is an index in the Global Color Table
	*/
	
	/* Magic number (GIF89a), width pixel 2 bytes, height pixel 2 bytes, GCT follows 256 values * 3 bytes (RGB) , bg color, default aspect ration */ 	
	char gif_header[13] = {'G', 'I', 'F', '8', '9', 'a', 96, 0, 64, 0, 0xf7, 0x00, 0};
    	/* Introduce the block 3bytes, netscape (type of gif : animation), a new block comment */
	char gif_infos[31] = {
        0x21, 0xff, 0x0b, 'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E', '2', '.', '0', 3, 1, 0, 0, 0,
	0x21, 0xfe, 8, 'T', 'i', 'l', 'E', 'm', '2', 0, 0, 0
	};
	
	int width, height;
	guchar* lcddata;
	int k;
	int x, y;
	width = gsi->emu->calc->hw.lcdwidth;
	height = gsi->emu->calc->hw.lcdheight;
	
	/* Alloc mmem */
	lcddata = g_new(guchar, (width / 8) * height);
		
	/* Get the lcd content using the function 's pointer from Benjamin's core */
	(*gsi->emu->calc->hw.get_lcd)(gsi->emu->calc, lcddata);





	printf("GIF ENCODER\n");
	FILE* fp;
	fp = fopen("gifencod.gif", "w");
  	if(fp) { 
			
		/* TODO : convert this padding stuff to a real palette */ 
		char palette_start[] = { 0x00, 0x00, 0x00};
		char padding[] = { 0xff, 0xff, 0xff};
		char palette_end[] = { 0xff, 0xff, 0xff};

	    
		/* Extension block introduced by 0x21 ('!'), and an img introduced by 0x2c (',') followed by coordinate corner(0,0), canvas 4 bytes, no local color table */
		static char gif_img[18] = {0x21, 0xf9, 4, 5, 11, 0, 0x0f, 0, 0x2c, 0, 0, 0, 0, 96, 0, 64, 0, 0};
	    
		char end[1] = { 0x00};
		
		fwrite(gif_header, 13, 1, fp);
		fwrite(palette_start, 3, 1, fp);
		for(k = 0; k < 254; k++) {
			fwrite(padding, 3, 1, fp);
		}
		fwrite(palette_end, 3, 1, fp);
		fwrite(gif_infos, 31, 1, fp);
		fwrite(gif_img, 18, 1, fp);
		
		long i= 0;
		
		unsigned char q[(96*64)];


		/* Reduce screen buffer to one byte per pixel */
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				if (lcddata[(y * width + x) / 8] & (0x80 >> (x % 8))) {
					//printf("i = %d", i);
					q[i] = 0x00;
					i++;
				} else {
					q[i] = 0x02;
					i++;
				}
			}
		}	
		
		

		GifEncode(fp, q , 1, (width*height));
		fwrite(end, 1, 1,fp);	/* Write end of the frame */
		fclose(fp);
		gsi->isAnimScreenshotRecording = TRUE;
	}
}
    

/* Add a frame to an existing animated gif */
void tilem_animation_add_frame(GLOBAL_SKIN_INFOS* gsi) {
	
	printf("GIFENCODER addframe\n");
	int width, height;
	guchar* lcddata;
	int x, y;
	width = gsi->emu->calc->hw.lcdwidth;
	height = gsi->emu->calc->hw.lcdheight;
	/* Alloc mmem */
	lcddata = g_new(guchar, (width / 8) * height);
		
	/* Get the lcd content using the function 's pointer from Benjamin's core */
	(*gsi->emu->calc->hw.get_lcd)(gsi->emu->calc, lcddata);


	FILE* fp;
	fp = fopen("gifencod.gif", "a");
	if(fp) {
    	
    		/* Extension block introduced by 0x21 ('!'), and an img introduced by 0x2c (',') followed by coordinate corner(0,0), canvas 4 bytes, no local color table */
		static char gif_img[18] = {0x21, 0xf9, 4, 8, 11, 0, 0x0f, 0, 0x2c, 0, 0, 0, 0, 96, 0, 64, 0, 0};
		char end[1] = { 0x00};
		
		fwrite(gif_img, 18, 1, fp);
		long i= 0;
		
		unsigned char q[(width * height)];

		/* Reduce screen buffer to one byte per pixel */
		for (y = 0; y < height; y++) {
			for (x = 0; x < width; x++) {
				if (lcddata[(y * width + x) / 8] & (0x80 >> (x % 8))) {
					q[i] = 0x00;
					i++;
				} else {
					q[i] = 0x02;
					i++;
				}
			}
		}	
		
		

		GifEncode(fp, q , 1, (width*height));
		fwrite(end, 1, 1,fp);
		fclose(fp);
	}
}

void tilem_animation_stop(GLOBAL_SKIN_INFOS* gsi) {
	
	
    	char trailer[1] = { 0x3b};
	if(gsi->isAnimScreenshotRecording) {
		gsi->isAnimScreenshotRecording = FALSE;
		FILE* fp;
		fp = fopen("gifencod.gif", "a");
		fwrite(trailer, 1, 1,fp);
		fclose(fp);
	}

}

gboolean tilem_animation_record(gpointer data) {
	GLOBAL_SKIN_INFOS * gsi = (GLOBAL_SKIN_INFOS*) data;
	
	if(gsi->isAnimScreenshotRecording) 
		tilem_animation_add_frame(gsi);
	return TRUE;

}

