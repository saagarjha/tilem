/*
 * TilEm II
 *
 * Copyright (c) 2011 Benjamin Moody
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

typedef struct _TilemCalcEmulator {
	GThread *thread;

	GMutex *calc_mutex;
	GCond *calc_wakeup_cond;
	TilemCalc *calc;
	gboolean paused;
	gboolean exiting;

	GMutex *lcd_mutex;
	TilemGrayLCD *glcd;

	char *rom_file_name;

	/* FIXME: following stuff belongs elsewhere */

	byte* lcd_image_buf;
	int lcd_image_width;
	int lcd_image_height;
	GdkRgbCmap* lcd_cmap;

	GtkWidget* lcdwin;
	GtkWidget* background;

	GdkGeometry geomhints;
	GdkWindowHints geomhintmask;
} TilemCalcEmulator;

/* Create a new TilemCalcEmulator. */
TilemCalcEmulator *tilem_calc_emulator_new(void);

/* Free a TilemCalcEmulator. */
void tilem_calc_emulator_free(TilemCalcEmulator *emu);

/* Load the calculator state from the given ROM file (and accompanying
   sav file, if any.) */
gboolean tilem_calc_emulator_load_state(TilemCalcEmulator *emu,
                                        const char *filename);

/* Save the calculator state. */
gboolean tilem_calc_emulator_save_state(TilemCalcEmulator *emu);

/* Reset the calculator. */
void tilem_calc_emulator_reset(TilemCalcEmulator *emu);

/* Pause emulation (if currently running.) */
void tilem_calc_emulator_pause(TilemCalcEmulator *emu);

/* Resume emulation (if currently paused.) */
void tilem_calc_emulator_run(TilemCalcEmulator *emu);