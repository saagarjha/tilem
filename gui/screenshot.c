/*
 * TilEm II
 *
 * Copyright (c) 2010-2017 Thibault Duponchelle
 * Copyright (c) 2011-2013 Benjamin Moody
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
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <ticalcs.h>
#include <tilem.h>

#include "gui.h"
#include "files.h"
#include "filedlg.h"
#include "msgbox.h"

#define DEFAULT_WIDTH_96    192
#define DEFAULT_HEIGHT_96   128
#define DEFAULT_WIDTH_128   256
#define DEFAULT_HEIGHT_128  128
#define DEFAULT_WIDTH_320   320
#define DEFAULT_HEIGHT_320  240
#define DEFAULT_FORMAT      "png"
#define DEFAULT_DITHER_MODE 3

struct imgsize {
	int width;
	int height;
};

static const struct imgsize sizes_96[] =
	{ { 96, 64 }, { 192, 128 }, { 288, 192 } };

static const struct imgsize sizes_128[] =
	/* actual aspect ratio is 92:55 or 1.673:1 */
	{ { 128, 64 }, { 128, 77 },
	  { 214, 128 }, { 256, 128 }, { 256, 153 },
	  { 321, 192 }, { 384, 192 } };

static const struct imgsize sizes_320[] =
	{ { 160, 120 }, { 320, 240 }, { 640, 480 } };

static const struct {
	const char *id;
	const char *desc;
	gboolean grayscale;
	gboolean rgb_fixed;
	int color_cube_size;
} dither_modes[] =
	{ { "gray",  N_("Grayscale"),                     TRUE,  FALSE, 0 },
	  { "none",  N_("No dithering"),                  FALSE, FALSE, 0 },
	  { "auto5", N_("Automatic dithering (5-level)"), FALSE, FALSE, 5 },
	  { "auto6", N_("Automatic dithering (6-level)"), FALSE, FALSE, 6 },
	  { "full",  N_("Full dithering"),                FALSE, TRUE,  6 } };

static void grab_screen(GtkButton *btn, TilemScreenshotDialog *ssdlg);
static void begin_animation(GtkButton *btn, TilemScreenshotDialog *ssdlg);
static void end_animation(GtkButton *btn, TilemScreenshotDialog *ssdlg);
static gboolean save_output(TilemScreenshotDialog *ssdlg);

static char* find_free_filename(const char* directory,
                                const char* filename,
                                const char* extension);

static int get_lcd_width(TilemCalcEmulator *emu)
{
	g_return_val_if_fail(emu != NULL, 0);
	g_return_val_if_fail(emu->calc != NULL, 0);
	return emu->calc->hw.lcdwidth;
}

static int dither_string_to_mode(const char *str)
{
	guint i;

	for (i = 0; i < G_N_ELEMENTS(dither_modes); i++)
		if (str && !strcmp(str, dither_modes[i].id))
			return i;

	return DEFAULT_DITHER_MODE;
}

static void set_dither_mode(TilemAnimation *anim, int mode)
{
	if (mode < 0)
		mode = DEFAULT_DITHER_MODE;

	tilem_animation_set_quantization(anim, dither_modes[mode].grayscale,
	                                 dither_modes[mode].rgb_fixed,
	                                 dither_modes[mode].color_cube_size);
}

/* Quick screenshot: save a screenshot with predefined settings,
   without prompting the user */
void quick_screenshot(TilemEmulatorWindow *ewin)
{
	char *folder, *filename, *format, *dithermode;
	int grayscale, w96, h96, w128, h128, w320, h320, width, height;
	TilemAnimation *anim;
	GError *err = NULL;
	GdkRGBA fg, bg;

	tilem_config_get("screenshot",
	                 "directory/f", &folder,
	                 "format/s", &format,
	                 "grayscale/b=1", &grayscale,
	                 "width_96x64/i", &w96,
	                 "height_96x64/i", &h96,
	                 "width_128x64/i", &w128,
	                 "height_128x64/i", &h128,
	                 "width_320x240/i", &w320,
	                 "height_320x240/i", &h320,
	                 "foreground/c=#000", &fg,
	                 "background/c=#fff", &bg,
	                 "dither_mode/s", &dithermode,
	                 NULL);

	anim = tilem_calc_emulator_get_screenshot(ewin->emu, grayscale);
	if (!anim) {
		g_free(folder);
		g_free(format);
		return;
	}

	if (get_lcd_width(ewin->emu) == 320) {
		width = (w320 > 0 ? w320 : DEFAULT_WIDTH_320);
		height = (h320 > 0 ? h320 : DEFAULT_HEIGHT_320);
	}
	else if (get_lcd_width(ewin->emu) == 128) {
		width = (w128 > 0 ? w128 : DEFAULT_WIDTH_128);
		height = (h128 > 0 ? h128 : DEFAULT_HEIGHT_128);
	}
	else {
		width = (w96 > 0 ? w96 : DEFAULT_WIDTH_96);
		height = (h96 > 0 ? h96 : DEFAULT_HEIGHT_96);
	}

	tilem_animation_set_size(anim, width, height);
	tilem_animation_set_colors(anim, &fg, &bg);
	set_dither_mode(anim, dither_string_to_mode(dithermode));

	if (!folder)
		folder = get_config_file_path("screenshots", NULL);

	if (!format)
		format = g_strdup(DEFAULT_FORMAT);

	g_mkdir_with_parents(folder, 0755);

	filename = find_free_filename(folder, "screenshot", format);
	if (!filename) {
		g_free(folder);
		g_free(format);
		g_object_unref(anim);
		return;
	}

	printf(_("screenshot saved : %s\n"), filename);

	if (!tilem_animation_save(anim, filename, format, NULL, NULL, &err)) {
		messagebox01(ewin->window, GTK_MESSAGE_ERROR,
		             _("Unable to save screenshot"),
		             "%s", err->message);
		g_error_free(err);
	}

	g_object_unref(anim);
	g_free(filename);
	g_free(folder);
	g_free(format);
	g_free(dithermode);
}

/* Look for a free filename by testing [folder]/[basename]000.[extension] to [folder]/[basename]999.[extension]
   Return a newly allocated string if success
   Return null if no filename found */
static char* find_free_filename(const char* folder,
                                const char* basename,
                                const char* extension)
{
	int i;
	char *filename, *prefix;

	if(folder)
		prefix = g_build_filename(folder, basename, NULL);
	else
		prefix = g_build_filename(basename, NULL);

	/* I do not use a while and limit number to 1000 because for any reason, if there's a problem in this scope
	   I don't want to freeze tilem (if tilem don't find a free filename and never return anything)
	   Limit to 1000 prevent this problem but if you prefer we could use a while wich wait a valid filename... */
	for(i=0; i<999; i++) {
		filename = g_strdup_printf("%s%03d.%s", prefix, i, extension);
		if(!g_file_test(filename, G_FILE_TEST_IS_REGULAR)) {
			g_free(prefix);
			return filename;
		}
		g_free(filename);
	}

	g_free(prefix);
	return NULL;
}

/* Change the review image to set the current animation */
static void set_current_animation(TilemScreenshotDialog *ssdlg,
                                  TilemAnimation *anim)
{
	GtkImage *img = GTK_IMAGE(ssdlg->screenshot_preview_image);
	int width, height, mode;
	GdkRGBA fg, bg;
	gdouble speed;

	if (anim)
		g_object_ref(anim);
	if (ssdlg->current_anim)
		g_object_unref(ssdlg->current_anim);
	ssdlg->current_anim = anim;

	if (!anim) {
		gtk_image_set_from_animation(img, NULL);
		gtk_dialog_set_response_sensitive(GTK_DIALOG(ssdlg->window),
		                                  GTK_RESPONSE_ACCEPT, FALSE);
	}
	else {
		width = gtk_spin_button_get_value_as_int
			(GTK_SPIN_BUTTON(ssdlg->width_spin));
		height = gtk_spin_button_get_value_as_int
			(GTK_SPIN_BUTTON(ssdlg->height_spin));
		tilem_animation_set_size(anim, width, height);

		gtk_color_chooser_get_rgba
			(GTK_COLOR_CHOOSER(ssdlg->foreground_color), &fg);
		gtk_color_chooser_get_rgba
			(GTK_COLOR_CHOOSER(ssdlg->background_color), &bg);
		tilem_animation_set_colors(anim, &fg, &bg);

		mode = gtk_combo_box_get_active
			(GTK_COMBO_BOX(ssdlg->dither_mode_combo));
		set_dither_mode(anim, mode);

		speed = gtk_spin_button_get_value
			(GTK_SPIN_BUTTON(ssdlg->animation_speed));
		tilem_animation_set_speed(anim, speed);

		gtk_image_set_from_animation(img, GDK_PIXBUF_ANIMATION(anim));

		/* Need to call gtk_widget_show because we hide it
		   while recording */
		gtk_widget_show(ssdlg->screenshot_preview_image);

		gtk_dialog_set_response_sensitive(GTK_DIALOG(ssdlg->window),
		                                  GTK_RESPONSE_ACCEPT, TRUE);
	}
}

static void dialog_response(G_GNUC_UNUSED GtkDialog *dialog, gint response, gpointer data)
{
	TilemScreenshotDialog *ssdlg = data;

	if (response == GTK_RESPONSE_ACCEPT) {
		if (!save_output(ssdlg))
			return;
	}

	gtk_widget_hide(GTK_WIDGET(dialog));
	end_animation(NULL, ssdlg);
	set_current_animation(ssdlg, NULL);
}

static void set_size_spin_buttons(TilemScreenshotDialog *ssdlg,
                                  int width, int height)
{
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ssdlg->width_spin), width);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ssdlg->height_spin), height);
}

enum {
	COL_TEXT,
	COL_WIDTH,
	COL_HEIGHT
};

static void animation_speed_changed(GtkSpinButton *animation_speed,
                                    gpointer data)
{
	TilemScreenshotDialog *ssdlg = data;
	TilemAnimation * anim = ssdlg->current_anim;
	gdouble value = gtk_spin_button_get_value(animation_speed);
	tilem_animation_set_speed(anim, value);
}

/* Combo box changed.  Update spin buttons accordingly. */
static void size_combo_changed(GtkComboBox *combo,
                               TilemScreenshotDialog *ssdlg)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	int width, height;

	if (gtk_combo_box_get_active_iter(combo, &iter)) {
		model = gtk_combo_box_get_model(combo);
		gtk_tree_model_get(model, &iter,
		                   COL_WIDTH, &width,
		                   COL_HEIGHT, &height,
		                   -1);
		if (width && height)
			set_size_spin_buttons(ssdlg, width, height);
	}
}

static void size_spin_changed(G_GNUC_UNUSED GtkSpinButton *sb,
                              TilemScreenshotDialog *ssdlg)
{
	GtkComboBox *combo = GTK_COMBO_BOX(ssdlg->ss_size_combo);
	GtkTreeModel *model;
	GtkTreeIter iter;
	int width, height, w, h;

	model = gtk_combo_box_get_model(combo);
	if (!model || !gtk_tree_model_get_iter_first(model, &iter))
		return;

	width = gtk_spin_button_get_value_as_int
		(GTK_SPIN_BUTTON(ssdlg->width_spin));
	height = gtk_spin_button_get_value_as_int
		(GTK_SPIN_BUTTON(ssdlg->height_spin));

	do {
		gtk_tree_model_get(model, &iter,
		                   COL_WIDTH, &w,
		                   COL_HEIGHT, &h,
		                   -1);

		if ((w == 0 && h == 0) || (w == width && h == height)) {
			gtk_combo_box_set_active_iter(combo, &iter);
			break;
		}
	} while (gtk_tree_model_iter_next(model, &iter));

	set_current_animation(ssdlg, ssdlg->current_anim);
}

static void fill_size_combobox(GtkComboBox *combo,
                               const struct imgsize *sizes,
                               int nsizes)
{
	GtkListStore *store;
	GtkTreeIter iter;
	int i;
	char *s;

	store = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT);

	for (i = 0; i < nsizes; i++) {
		s = g_strdup_printf(_("%d \303\227 %d"),
		                    sizes[i].width,
		                    sizes[i].height);

		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
		                   COL_TEXT, s,
		                   COL_WIDTH, sizes[i].width,
		                   COL_HEIGHT, sizes[i].height,
		                   -1);
		g_free(s);
	}

	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter,
	                   COL_TEXT, _("Custom"),
	                   COL_WIDTH, 0,
	                   COL_HEIGHT, 0,
	                   -1);

	gtk_combo_box_set_model(GTK_COMBO_BOX(combo), GTK_TREE_MODEL(store));
}

/* This method is called when a color is set (foreground or background)
 * It set a new palette based on new custom colors
 * It refresh the screen to print new colors 
 */
static void color_changed(G_GNUC_UNUSED GtkSpinButton *sb,
                              TilemScreenshotDialog *ssdlg)
{
	set_current_animation(ssdlg, ssdlg->current_anim);
}

static void dither_mode_changed(G_GNUC_UNUSED GtkComboBox *combo,
                                TilemScreenshotDialog *ssdlg)
{
	if (ssdlg->current_anim)
		tilem_animation_set_quantize_preview(ssdlg->current_anim, TRUE);

	set_current_animation(ssdlg, ssdlg->current_anim);
}

/* Create the screenshot menu */
static TilemScreenshotDialog * create_screenshot_window(TilemCalcEmulator *emu)
{
	TilemScreenshotDialog *ssdlg = g_slice_new0(TilemScreenshotDialog);
	GtkWidget *main_grid, *vbox, *frame, *config_expander,
		*grd, *lbl, *align;
	GtkCellRenderer *cell;
	guint i;

	ssdlg->emu = emu;

	ssdlg->window = gtk_dialog_new_with_buttons
		(_("Screenshot"),
		 (emu->ewin ? GTK_WINDOW(emu->ewin->window) : NULL),
		 GTK_DIALOG_DESTROY_WITH_PARENT,
		 _("Cancel"), GTK_RESPONSE_CANCEL,
		 _("Save"), GTK_RESPONSE_ACCEPT,
		 NULL);
	
	gtk_window_set_resizable(GTK_WINDOW(ssdlg->window), FALSE);

	gtk_dialog_set_alternative_button_order(GTK_DIALOG(ssdlg->window),
	                                        GTK_RESPONSE_ACCEPT,
	                                        GTK_RESPONSE_CANCEL,
	                                        -1);

	gtk_dialog_set_default_response(GTK_DIALOG(ssdlg->window),
	                                GTK_RESPONSE_ACCEPT);

	g_signal_connect(ssdlg->window, "response",
	                 G_CALLBACK(dialog_response), ssdlg);

	g_signal_connect(ssdlg->window, "delete-event",
	                 G_CALLBACK(gtk_widget_hide_on_delete), NULL);

	main_grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(main_grid), 6);
	gtk_grid_set_column_spacing(GTK_GRID(main_grid), 12);
	gtk_container_set_border_width(GTK_CONTAINER(main_grid), 6);

	/* Preview */

	frame = gtk_frame_new(_("Preview"));
	gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);

	ssdlg->screenshot_preview_image = gtk_image_new();
	align = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
	gtk_alignment_set_padding(GTK_ALIGNMENT(align), 0, 0, 12, 0);
	gtk_container_add(GTK_CONTAINER(align), ssdlg->screenshot_preview_image);

	gtk_container_add(GTK_CONTAINER(frame), align);
	gtk_grid_attach(GTK_GRID(main_grid), frame, 0, 0, 1, 1);

	/* Buttons */

	vbox = gtk_button_box_new(GTK_ORIENTATION_VERTICAL);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(vbox), GTK_BUTTONBOX_START);
	gtk_box_set_spacing(GTK_BOX(vbox), 6);

	ssdlg->screenshot = gtk_button_new_with_mnemonic(_("_Grab"));
	gtk_box_pack_start(GTK_BOX(vbox), ssdlg->screenshot, FALSE, FALSE, 0);

	ssdlg->record = gtk_button_new_with_mnemonic(_("_Record"));
	gtk_box_pack_start(GTK_BOX(vbox), ssdlg->record, FALSE, FALSE, 0);

	ssdlg->stop = gtk_button_new_with_mnemonic(_("_Stop"));
	gtk_box_pack_start(GTK_BOX(vbox), ssdlg->stop, FALSE, FALSE, 0);
	gtk_widget_set_sensitive(GTK_WIDGET(ssdlg->stop), FALSE);

	gtk_grid_attach(GTK_GRID(main_grid), vbox, 1, 0, 1, 2);

	/* Options */

	config_expander = gtk_expander_new(_("Options"));

	grd = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grd), 6);
	gtk_grid_set_column_spacing(GTK_GRID(grd), 6);

	ssdlg->grayscale_tb = gtk_check_button_new_with_mnemonic(_("Gra_yscale"));
	gtk_grid_attach(GTK_GRID(grd), ssdlg->grayscale_tb, 0, 0, 2, 1);

	lbl = gtk_label_new_with_mnemonic(_("Image si_ze:"));
	gtk_misc_set_alignment(GTK_MISC(lbl), LABEL_X_ALIGN, 0.5);
	gtk_grid_attach(GTK_GRID(grd), lbl, 0, 1, 1, 1);

	ssdlg->ss_size_combo = gtk_combo_box_new();
	cell = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(ssdlg->ss_size_combo),
	                           cell, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(ssdlg->ss_size_combo),
	                               cell, "text", COL_TEXT, NULL);
	gtk_label_set_mnemonic_widget(GTK_LABEL(lbl), ssdlg->ss_size_combo);
	gtk_grid_attach(GTK_GRID(grd), ssdlg->ss_size_combo, 1, 1, 1, 1);

	lbl = gtk_label_new_with_mnemonic(_("_Width:"));
	gtk_misc_set_alignment(GTK_MISC(lbl), LABEL_X_ALIGN, 0.5);
	gtk_grid_attach(GTK_GRID(grd), lbl, 0, 2, 1, 1);

	ssdlg->width_spin = gtk_spin_button_new_with_range(1, 750, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(lbl), ssdlg->width_spin);
	align = gtk_alignment_new(0.0, 0.5, 0.0, 1.0);
	gtk_container_add(GTK_CONTAINER(align), ssdlg->width_spin);
	gtk_grid_attach(GTK_GRID(grd), align, 1, 2, 1, 1);

	lbl = gtk_label_new_with_mnemonic(_("_Height:"));
	gtk_misc_set_alignment(GTK_MISC(lbl), LABEL_X_ALIGN, 0.5);
	gtk_grid_attach(GTK_GRID(grd), lbl, 0, 3, 1, 1);

	ssdlg->height_spin = gtk_spin_button_new_with_range(1, 500, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(lbl), ssdlg->height_spin);
	align = gtk_alignment_new(0.0, 0.5, 0.0, 1.0);
	gtk_container_add(GTK_CONTAINER(align), ssdlg->height_spin);
	gtk_grid_attach(GTK_GRID(grd), align, 1, 3, 1, 1);


	lbl = gtk_label_new_with_mnemonic(_("Animation s_peed:"));
	gtk_misc_set_alignment(GTK_MISC(lbl), LABEL_X_ALIGN, 0.5);
	gtk_grid_attach(GTK_GRID(grd), lbl, 0, 4, 1, 1);

	ssdlg->animation_speed = gtk_spin_button_new_with_range(0.1, 100.0, 0.1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(lbl), ssdlg->animation_speed);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(ssdlg->animation_speed), 1.0);
	align = gtk_alignment_new(0.0, 0.5, 0.0, 1.0);
	gtk_container_add(GTK_CONTAINER(align), ssdlg->animation_speed);
	gtk_grid_attach(GTK_GRID(grd), align, 1, 4, 1, 1);

	/* Foreground color and background color */
	lbl = gtk_label_new_with_mnemonic(_("_Foreground:"));
	ssdlg->foreground_label = lbl;
	gtk_misc_set_alignment(GTK_MISC(lbl), LABEL_X_ALIGN, 0.5);
	gtk_grid_attach(GTK_GRID(grd), lbl, 0, 5, 1, 1);

	ssdlg->foreground_color = gtk_color_button_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(lbl), ssdlg->foreground_color);
	align = gtk_alignment_new(0.0, 0.5, 0.0, 1.0);
	gtk_container_add(GTK_CONTAINER(align), ssdlg->foreground_color);
	gtk_grid_attach(GTK_GRID(grd), align, 1, 5, 1, 1);
	
	lbl = gtk_label_new_with_mnemonic(_("_Background:"));
	ssdlg->background_label = lbl;
	gtk_misc_set_alignment(GTK_MISC(lbl), LABEL_X_ALIGN, 0.5);
	gtk_grid_attach(GTK_GRID(grd), lbl, 0, 6, 1, 1);

	ssdlg->background_color = gtk_color_button_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(lbl), ssdlg->background_color);
	align = gtk_alignment_new(0.0, 0.5, 0.0, 1.0);
	gtk_container_add(GTK_CONTAINER(align), ssdlg->background_color);
	gtk_grid_attach(GTK_GRID(grd), align, 1, 6, 1, 1);

	/* Dithering mode */
	lbl = gtk_label_new_with_mnemonic(_("GI_F color mode:"));
	ssdlg->dither_mode_label = lbl;
	gtk_misc_set_alignment(GTK_MISC(lbl), LABEL_X_ALIGN, 0.5);
	gtk_grid_attach(GTK_GRID(grd), lbl, 0, 5, 1, 2);

	ssdlg->dither_mode_combo = gtk_combo_box_text_new();
	for (i = 0; i < G_N_ELEMENTS(dither_modes); i++)
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(ssdlg->dither_mode_combo),
		                          _(dither_modes[i].desc));
	gtk_label_set_mnemonic_widget(GTK_LABEL(lbl), ssdlg->dither_mode_combo);
	align = gtk_alignment_new(0.0, 0.5, 0.0, 1.0);
	gtk_container_add(GTK_CONTAINER(align), ssdlg->dither_mode_combo);
	gtk_grid_attach(GTK_GRID(grd), align, 1, 5, 1, 2);

	align = gtk_alignment_new(0.5, 0.5, 1.0, 1.0);
	gtk_alignment_set_padding(GTK_ALIGNMENT(align), 0, 0, 12, 0);
	gtk_container_add(GTK_CONTAINER(align), grd);

	gtk_container_add(GTK_CONTAINER(config_expander), align);

	gtk_grid_attach(GTK_GRID(main_grid), config_expander, 0, 1, 1, 1);

	g_signal_connect(ssdlg->screenshot, "clicked",
	                 G_CALLBACK(grab_screen), ssdlg);
	g_signal_connect(ssdlg->record, "clicked",
	                 G_CALLBACK(begin_animation), ssdlg);
	g_signal_connect(ssdlg->stop, "clicked",
	                 G_CALLBACK(end_animation), ssdlg);

	g_signal_connect(ssdlg->ss_size_combo, "changed",
	                 G_CALLBACK(size_combo_changed), ssdlg);
	g_signal_connect(ssdlg->width_spin, "value-changed",
	                 G_CALLBACK(size_spin_changed), ssdlg);
	g_signal_connect(ssdlg->height_spin, "value-changed",
	                 G_CALLBACK(size_spin_changed), ssdlg);
	g_signal_connect(ssdlg->animation_speed, "value-changed",
	                 G_CALLBACK(animation_speed_changed), ssdlg);
	
	g_signal_connect(ssdlg->foreground_color, "color-set",
	                 G_CALLBACK(color_changed), ssdlg);
	g_signal_connect(ssdlg->background_color, "color-set",
	                 G_CALLBACK(color_changed), ssdlg);
	g_signal_connect(ssdlg->dither_mode_combo, "changed",
	                 G_CALLBACK(dither_mode_changed), ssdlg);

	/*g_signal_connect(config_expander, "activate",
	                 G_CALLBACK(on_config_expander_activate), ssdlg);
	*/
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(ssdlg->window));
	gtk_container_add(GTK_CONTAINER(vbox), main_grid);
	gtk_widget_show_all(main_grid);

	return ssdlg;
}

/* Popup the screenshot window */
void popup_screenshot_window(TilemEmulatorWindow *ewin)
{
	TilemScreenshotDialog *ssdlg;
	int w96, h96, w128, h128, w320, h320, width, height, grayscale;
	GdkRGBA fg, bg;
	char *dithermode;

	g_return_if_fail(ewin != NULL);
	g_return_if_fail(ewin->emu != NULL);

	if (!ewin->emu->ssdlg)
		ewin->emu->ssdlg = create_screenshot_window(ewin->emu);
	ssdlg = ewin->emu->ssdlg;

	tilem_config_get("screenshot",
	                 "grayscale/b=1", &grayscale,
	                 "width_96x64/i", &w96,
	                 "height_96x64/i", &h96,
	                 "width_128x64/i", &w128,
	                 "height_128x64/i", &h128,
	                 "width_320x240/i", &w320,
	                 "height_320x240/i", &h320,
	                 "foreground/c=#000", &fg,
	                 "background/c=#fff", &bg,
	                 "dither_mode/s=normal", &dithermode,
	                 NULL);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ssdlg->grayscale_tb),
	                             grayscale);

	if (get_lcd_width(ewin->emu) == 320) {
		fill_size_combobox(GTK_COMBO_BOX(ssdlg->ss_size_combo),
		                   sizes_320, G_N_ELEMENTS(sizes_320));
		width = (w320 > 0 ? w320 : DEFAULT_WIDTH_320);
		height = (h320 > 0 ? h320 : DEFAULT_HEIGHT_320);
	}
	else if (get_lcd_width(ewin->emu) == 128) {
		fill_size_combobox(GTK_COMBO_BOX(ssdlg->ss_size_combo),
		                   sizes_128, G_N_ELEMENTS(sizes_128));
		width = (w128 > 0 ? w128 : DEFAULT_WIDTH_128);
		height = (h128 > 0 ? h128 : DEFAULT_HEIGHT_128);
	}
	else {
		fill_size_combobox(GTK_COMBO_BOX(ssdlg->ss_size_combo),
		                   sizes_96, G_N_ELEMENTS(sizes_96));
		width = (w96 > 0 ? w96 : DEFAULT_WIDTH_96);
		height = (h96 > 0 ? h96 : DEFAULT_HEIGHT_96);
	}

	if (ewin->emu->calc
	    && (ewin->emu->calc->hw.flags & TILEM_CALC_HAS_COLOR)) {
		gtk_widget_show(ssdlg->dither_mode_combo);
		gtk_widget_show(ssdlg->dither_mode_label);

		gtk_widget_hide(ssdlg->grayscale_tb);
		gtk_widget_hide(ssdlg->foreground_color);
		gtk_widget_hide(ssdlg->foreground_label);
		gtk_widget_hide(ssdlg->background_color);
		gtk_widget_hide(ssdlg->background_label);
	}
	else {
		gtk_widget_hide(ssdlg->dither_mode_combo);
		gtk_widget_hide(ssdlg->dither_mode_label);

		gtk_widget_show(ssdlg->grayscale_tb);
		gtk_widget_show(ssdlg->foreground_color);
		gtk_widget_show(ssdlg->foreground_label);
		gtk_widget_show(ssdlg->background_color);
		gtk_widget_show(ssdlg->background_label);
	} 

	set_size_spin_buttons(ssdlg, width, height);
	size_spin_changed(NULL, ssdlg);

	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(ssdlg->foreground_color), &fg);
	gtk_color_chooser_set_rgba(GTK_COLOR_CHOOSER(ssdlg->background_color), &bg);
	gtk_combo_box_set_active(GTK_COMBO_BOX(ssdlg->dither_mode_combo),
	                         dither_string_to_mode(dithermode));

	g_free(dithermode);

	grab_screen(NULL, ssdlg);
	gtk_window_present(GTK_WINDOW(ssdlg->window));
}

/* Save the current (static) output */
static gboolean save_output(TilemScreenshotDialog *ssdlg)
{
	char *dir, *format, *filename, *basename;
	TilemAnimation *anim = ssdlg->current_anim;
	GdkPixbufAnimation *ganim = GDK_PIXBUF_ANIMATION(anim);
	const char *format_opt, *width_opt, *height_opt;
	gboolean is_static;
	int width, height, mode;
	GdkRGBA fg, bg;
	GError *err = NULL;

	g_return_val_if_fail(anim != NULL, FALSE);

	is_static = gdk_pixbuf_animation_is_static_image(ganim);
	width = gdk_pixbuf_animation_get_width(ganim);
	height = gdk_pixbuf_animation_get_height(ganim);

	gtk_color_chooser_get_rgba
		(GTK_COLOR_CHOOSER(ssdlg->foreground_color), &fg);
	gtk_color_chooser_get_rgba
		(GTK_COLOR_CHOOSER(ssdlg->background_color), &bg);

	mode = gtk_combo_box_get_active(GTK_COMBO_BOX(ssdlg->dither_mode_combo));
	if (mode < 0)
		mode = DEFAULT_DITHER_MODE;

	tilem_config_get("screenshot",
	                 "directory/f", &dir,
	                 "static_format/s", &format,
	                 NULL);

	if (!dir)
		dir = g_get_current_dir();

	if (!is_static) {
		g_free(format);
		format = g_strdup("gif");
	}
	else if (!format) {
		format = g_strdup(DEFAULT_FORMAT);
	}

	filename = find_free_filename(dir, "screenshot", format);
	basename = (filename ? g_filename_display_basename(filename) : NULL);
	g_free(filename);
	g_free(format);

	if (!is_static) {
		filename = prompt_save_file(_("Save Screenshot"),
		                            GTK_WINDOW(ssdlg->window),
		                            basename, dir,
		                            _("GIF images"), "*.gif",
		                            _("All files"), "*",
		                            NULL);
	}
	else {
		/* FIXME: perhaps check the list of supported output
		   formats (gdk_pixbuf_get_formats()) - e.g., tiff is
		   usually supported, although it requires libtiff
		   installed (png and jpeg also require external
		   libraries, but we need those libraries anyway for
		   other reasons) */
		filename = prompt_save_file(_("Save Screenshot"),
		                            GTK_WINDOW(ssdlg->window),
		                            basename, dir,
		                            _("PNG images"), "*.png",
		                            _("GIF images"), "*.gif",
		                            _("BMP images"), "*.bmp",
		                            _("JPEG images"), "*.jpg;*.jpe;*.jpeg",
		                            _("All files"), "*",
		                            NULL);
	}

	g_free(basename);
	g_free(dir);

	if (!filename)
		return FALSE;

	if (!is_static) {
		format = g_strdup("gif");
	}
	else {
		basename = g_path_get_basename(filename);
		format = strrchr(basename, '.');
		if (!format) {
			messagebox00(ssdlg->window, GTK_MESSAGE_ERROR,
			             _("Unable to save screenshot"),
			             _("File name does not have a"
			               " recognized suffix"));
			g_free(filename);
			g_free(basename);
			return FALSE;
		}
		else {
			format = g_strdup(format + 1);
		}
	}

	tilem_animation_save(anim, filename, format, NULL, NULL, &err);

	dir = g_path_get_dirname(filename);

	if (err) {
		messagebox01(ssdlg->window, GTK_MESSAGE_ERROR,
		             _("Unable to save screenshot"),
		             "%s", err->message);
		g_error_free(err);
		g_free(dir);
		g_free(filename);
		g_free(format);
		return FALSE;
	}

	if (is_static)
		format_opt = "static_format/s";
	else
		format_opt = NULL;

	if (get_lcd_width(ssdlg->emu) == 320) {
		width_opt = "width_320x240/i";
		height_opt = "height_320x240/i";
	}
	else if (get_lcd_width(ssdlg->emu) == 128) {
		width_opt = "width_128x64/i";
		height_opt = "height_128x64/i";
	}
	else {
		width_opt = "width_96x64/i";
		height_opt = "height_96x64/i";
	}

	tilem_config_set("screenshot",
	                 "directory/f", dir,
	                 "grayscale/b", ssdlg->current_anim_grayscale,
	                 "foreground/c", &fg,
	                 "background/c", &bg,
	                 "dither_mode/s", dither_modes[mode].id,
	                 width_opt, width,
	                 height_opt, height,
	                 format_opt, format,
	                 NULL);

	g_free(dir);
	g_free(filename);
	g_free(format);
	return TRUE;
}

/* Callback for record button */
static void begin_animation(G_GNUC_UNUSED GtkButton *btn,
                            TilemScreenshotDialog *ssdlg)
{
	gboolean grayscale = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(ssdlg->grayscale_tb));

	gtk_widget_set_sensitive(GTK_WIDGET(ssdlg->animation_speed), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(ssdlg->screenshot), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(ssdlg->record), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(ssdlg->stop), TRUE);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(ssdlg->window),
	                                  GTK_RESPONSE_ACCEPT, FALSE);

	tilem_calc_emulator_begin_animation(ssdlg->emu, grayscale);
	ssdlg->current_anim_grayscale = grayscale;

	/* You can choose to hide current animation while recording or not
	   It's as you prefer... For the moment I hide it */
	/*gtk_widget_hide(GTK_WIDGET(ssdlg->screenshot_preview_image)); */

	//set_current_animation(ssdlg, NULL);
}

/* Callback for stop button (stop the recording) */
static void end_animation(G_GNUC_UNUSED GtkButton *btn,
                          TilemScreenshotDialog *ssdlg)
{
	TilemAnimation *anim;

	if (ssdlg->emu->anim) {
		anim = tilem_calc_emulator_end_animation(ssdlg->emu);
		set_current_animation(ssdlg, anim);
		tilem_animation_set_quantize_preview(anim, TRUE);
		g_object_unref(anim);
	}
	else {
		set_current_animation(ssdlg, NULL);
	}

	gtk_widget_set_sensitive(GTK_WIDGET(ssdlg->animation_speed), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(ssdlg->screenshot), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(ssdlg->record), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(ssdlg->stop), FALSE);
}

/* Callback for screenshot button (take a screenshot) */
static void grab_screen(G_GNUC_UNUSED GtkButton *btn,
                        TilemScreenshotDialog *ssdlg)
{
	TilemAnimation *anim;
		
	gboolean grayscale = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(ssdlg->grayscale_tb));

	anim = tilem_calc_emulator_get_screenshot(ssdlg->emu, grayscale);
	ssdlg->current_anim_grayscale = grayscale;
	set_current_animation(ssdlg, anim);
	g_object_unref(anim);
}

