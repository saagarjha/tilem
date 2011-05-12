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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <gtk/gtk.h>

#include "gtk-compat.h"
#include "filedlg.h"

#ifdef GDK_WINDOWING_WIN32
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <commdlg.h>
# include <gdk/gdkwin32.h>
# include <wchar.h>

# ifndef OPENFILENAME_SIZE_VERSION_400
#  define OPENFILENAME_SIZE_VERSION_400 sizeof(OPENFILENAMEA)
# endif

struct fcinfo {
	const char *title;
	gboolean save;
	HWND parent_window;
	char *filename;
	char *dirname;
	char *extension;
	char *filters;
	unsigned int flags;
};

#define BUFFER_SIZE 32768

static char * file_chooser_main(const struct fcinfo *fci)
{
	if (G_WIN32_HAVE_WIDECHAR_API()) {
		OPENFILENAMEW ofnw;
		wchar_t *titlew, *filterw, *initdirw, *defextw;
		wchar_t filenamew[BUFFER_SIZE + 1];
		wchar_t *p;
		int result;
		int i;

		titlew = g_utf8_to_utf16(fci->title, -1, 0, 0, 0);

		filterw = g_utf8_to_utf16(fci->filters, -1, 0, 0, 0);
		for (i = 0; filterw[i]; i++)
			if (filterw[i] == '\n') filterw[i] = 0;

		memset(&ofnw, 0, sizeof(ofnw));
		ofnw.lStructSize = OPENFILENAME_SIZE_VERSION_400;

		ofnw.hwndOwner = fci->parent_window;
		ofnw.lpstrTitle = titlew;
		ofnw.lpstrFilter = filterw;
		ofnw.nFilterIndex = 1;
		ofnw.lpstrFile = filenamew;
		ofnw.nMaxFile = BUFFER_SIZE;

		memset(filenamew, 0, sizeof(filenamew));

		if (fci->filename) {
			p = g_utf8_to_utf16(fci->filename, -1, 0, 0, 0);
			if (p) {
				wcsncpy(filenamew, p, BUFFER_SIZE);
				g_free(p);
			}
		}

		if (fci->dirname)
			initdirw = g_utf8_to_utf16(fci->dirname, -1, 0, 0, 0);
		else
			initdirw = NULL;

		if (fci->extension)
			defextw = g_utf8_to_utf16(fci->extension, -1, 0, 0, 0);
		else
			defextw = NULL;

		ofnw.lpstrInitialDir = initdirw;
		ofnw.lpstrDefExt = defextw;

		ofnw.Flags = fci->flags;

		result = (fci->save
		          ? GetSaveFileNameW(&ofnw)
		          : GetOpenFileNameW(&ofnw));

		g_free(titlew);
		g_free(filterw);
		g_free(initdirw);
		g_free(defextw);

		if (!result)
			return NULL;

		if ((fci->flags & OFN_ALLOWMULTISELECT)) {
			for (i = 0; i < BUFFER_SIZE; i++) {
				if (filenamew[i] == 0 && filenamew[i + 1] == 0)
					break;
				else if (filenamew[i] == '/')
					filenamew[i] = '\\';
				else if (filenamew[i] == 0)
					filenamew[i] = '/';
			}
		}

		return g_utf16_to_utf8(filenamew, -1, 0, 0, 0);
	}
	else {
		OPENFILENAMEA ofna;
		char *titlel, *filterl, *initdirl, *defextl;
		char filenamel[BUFFER_SIZE + 1];
		char *p;
		int result;
		int i;

		titlel = g_locale_from_utf8(fci->title, -1, 0, 0, 0);

		filterl = g_locale_from_utf8(fci->filters, -1, 0, 0, 0);
		for (i = 0; filterl[i]; i++)
			if (filterl[i] == '\n') filterl[i] = 0;

		memset(&ofna, 0, sizeof(ofna));
		ofna.lStructSize = OPENFILENAME_SIZE_VERSION_400;

		ofna.hwndOwner = fci->parent_window;
		ofna.lpstrTitle = titlel;
		ofna.lpstrFilter = filterl;
		ofna.nFilterIndex = 1;
		ofna.lpstrFile = filenamel;
		ofna.nMaxFile = BUFFER_SIZE;

		memset(filenamel, 0, sizeof(filenamel));

		if (fci->filename) {
			p = g_locale_from_utf8(fci->filename, -1, 0, 0, 0);
			if (p) {
				strncpy(filenamel, p, BUFFER_SIZE);
				g_free(p);
			}
		}

		if (fci->dirname)
			initdirl = g_locale_from_utf8(fci->dirname, -1, 0, 0, 0);
		else
			initdirl = NULL;

		if (fci->extension)
			defextl = g_locale_from_utf8(fci->extension, -1, 0, 0, 0);
		else
			defextl = NULL;

		ofna.lpstrInitialDir = initdirl;
		ofna.lpstrDefExt = defextl;

		ofna.Flags = fci->flags;

		result = (fci->save
		          ? GetSaveFileNameA(&ofna)
		          : GetOpenFileNameA(&ofna));

		g_free(titlel);
		g_free(filterl);
		g_free(initdirl);
		g_free(defextl);

		if (!result)
			return NULL;

		if ((fci->flags & OFN_ALLOWMULTISELECT)) {
			for (i = 0; i < BUFFER_SIZE; i++) {
				if (filenamel[i] == 0 && filenamel[i + 1] == 0)
					break;
				else if (filenamel[i] == '/')
					filenamel[i] = '\\';
				else if (filenamel[i] == 0)
					filenamel[i] = '/';
			}
		}

		return g_locale_to_utf8(filenamel, -1, 0, 0, 0);
	}
}

static gboolean wakeup(G_GNUC_UNUSED gpointer data)
{
	gtk_main_quit();
	return FALSE;
}

static gpointer file_chooser_thread(gpointer data)
{
	struct fcinfo *fci = data;
	gpointer res = file_chooser_main(fci);
	g_idle_add(wakeup, NULL);
	return res;
}

static char * build_filter_string(const char *desc1,
                                  const char *pattern1,
                                  va_list ap)
{
	GString *str = g_string_new(NULL);

	while (desc1 && pattern1) {
		g_string_append(str, desc1);
		g_string_append_c(str, '\n');
		g_string_append(str, pattern1);
		g_string_append_c(str, '\n');

		desc1 = va_arg(ap, char *);
		if (!desc1) break;
		pattern1 = va_arg(ap, char *);
	}

	return g_string_free(str, FALSE);
}

static char ** run_file_chooser(const char *title,
                                GtkWindow *parent,
                                gboolean save,
                                gboolean multiple,
                                const char *suggest_name,
                                const char *suggest_dir,
                                const char *desc1,
                                const char *pattern1,
                                va_list ap)
{
	struct fcinfo fci;
	GThread *thread;
	GtkWidget *dummy;
	GdkWindow *pwin;
	char *fname, *p, *dir;
	char **result;
	int i;

	if (!g_thread_supported())
		g_thread_init(NULL);

	fci.title = title;
	fci.save = save;

	if (parent && (pwin = gtk_widget_get_window(GTK_WIDGET(parent))))
		fci.parent_window = GDK_WINDOW_HWND(pwin);
	else
		fci.parent_window = NULL;

	if (suggest_name && suggest_dir) {
		fci.filename = g_build_filename(suggest_dir,
		                                suggest_name, NULL);
		fci.dirname = NULL;
	}
	else if (suggest_name) {
		fci.filename = g_strdup(suggest_name);
		fci.dirname = NULL;
	}
	else if (suggest_dir) {
		fci.filename = NULL;
		fci.dirname = g_strdup(suggest_dir);
	}
	else {
		fci.filename = fci.dirname = NULL;
	}

	if (suggest_name && (p = strrchr(suggest_name, '.')))
		fci.extension = g_strdup(p + 1);
	else
		fci.extension = NULL;

	fci.filters = build_filter_string(desc1, pattern1, ap);

	fci.flags = (OFN_HIDEREADONLY | OFN_EXPLORER);

	if (save)
		fci.flags |= OFN_OVERWRITEPROMPT;
	else {
		fci.flags |= OFN_FILEMUSTEXIST;
		if (multiple)
			fci.flags |= OFN_ALLOWMULTISELECT;
	}

	if ((thread = g_thread_create(file_chooser_thread, &fci, TRUE, NULL))) {
		dummy = gtk_invisible_new();
		gtk_grab_add(dummy);
		gtk_main();
		fname = g_thread_join(thread);
		gtk_widget_destroy(dummy);
	}
	else {
		fname = file_chooser_main(&fci);
	}

	g_free(fci.filename);
	g_free(fci.dirname);
	g_free(fci.extension);
	g_free(fci.filters);

	if (!fname) {
		return NULL;
	}
	else if (multiple && (p = strchr(fname, '/'))) {
		dir = g_strndup(fname, p - fname);
		result = g_strsplit(p + 1, "/", -1);

		for (i = 0; result[i]; i++) {
			p = result[i];
			result[i] = g_build_filename(dir, p, NULL);
			g_free(p);
		}

		g_free(fname);
		return result;
	}
	else {
		result = g_new(char *, 2);
		result[0] = fname;
		result[1] = NULL;
		return result;
	}
}

#else  /* ! GDK_WINDOWING_WIN32 */

/* Case insensitive filter function */
static gboolean filter_lowercase(const GtkFileFilterInfo *info,
                                 gpointer data)
{
	GSList *list = data;
	const char *base;
	char *lowercase, *reversed;
	int length;
	gboolean matched = FALSE;

	if ((base = strrchr(info->filename, G_DIR_SEPARATOR)))
		base++;
	else
		base = info->filename;

	lowercase = g_ascii_strdown(base, -1);
	length = strlen(lowercase);
	reversed = g_memdup(lowercase, length + 1);
	g_strreverse(reversed);

	while (list) {
		if (g_pattern_match(list->data, length,
		                    lowercase, reversed)) {
			matched = TRUE;
			break;
		}
		list = list->next;
	}

	g_free(lowercase);
	g_free(reversed);
	return matched;
}

static void free_filter_info(gpointer data)
{
	GSList *list = data, *l;
	for (l = list; l; l = l->next)
		g_pattern_spec_free(l->data);
	g_slist_free(list);
}

static void setup_file_filters(GtkFileChooser *chooser,
                               const char *desc1,
                               const char *pattern1,
                               va_list ap)
{
	GtkFileFilter *ffilt;
	char **pats;
	GPatternSpec *pspec;
	GSList *pspeclist;
	int i;

	while (desc1 && pattern1) {
		ffilt = gtk_file_filter_new();
		gtk_file_filter_set_name(ffilt, desc1);

		pats = g_strsplit(pattern1, ";", -1);
		pspeclist = NULL;
		for (i = 0; pats && pats[i]; i++) {
			pspec = g_pattern_spec_new(pats[i]);
			pspeclist = g_slist_prepend(pspeclist, pspec);
		}
		g_strfreev(pats);

		gtk_file_filter_add_custom(ffilt, GTK_FILE_FILTER_FILENAME,
		                           &filter_lowercase,
		                           pspeclist,
		                           &free_filter_info);

		gtk_file_chooser_add_filter(chooser, ffilt);

		desc1 = va_arg(ap, char *);
		if (!desc1) break;
		pattern1 = va_arg(ap, char *);
	}
}

static gboolean prompt_overwrite(const char *fname,
                                 GtkWindow *parent)
{
	GtkWidget *dlg;
	GtkWidget *button;
	char *p, *q;

	if (!g_file_test(fname, G_FILE_TEST_EXISTS))
		return TRUE;

	p = g_filename_display_basename(fname);
	dlg = gtk_message_dialog_new(parent,
	                             GTK_DIALOG_MODAL,
	                             GTK_MESSAGE_QUESTION,
	                             GTK_BUTTONS_NONE,
	                             "A file named \"%s\" already exists.  "
	                             "Do you want to replace it?",
	                             p);
	g_free(p);

	p = g_path_get_dirname(fname);
	q = g_filename_display_basename(p);
	gtk_message_dialog_format_secondary_markup
		(GTK_MESSAGE_DIALOG(dlg),
		 "The file already exists in \"%s\".  Replacing it will "
		 "overwrite its contents.", q);
	g_free(p);
	g_free(q);

	gtk_dialog_add_button(GTK_DIALOG(dlg),
	                      GTK_STOCK_CANCEL,
	                      GTK_RESPONSE_CANCEL);

	button = gtk_button_new_with_mnemonic("_Replace");
	gtk_widget_set_can_default(button, TRUE);
	gtk_button_set_image(GTK_BUTTON(button),
	                     gtk_image_new_from_stock(GTK_STOCK_SAVE,
	                                              GTK_ICON_SIZE_BUTTON));
	gtk_widget_show(button);
	gtk_dialog_add_action_widget(GTK_DIALOG(dlg), button,
	                             GTK_RESPONSE_ACCEPT);

	gtk_dialog_set_alternative_button_order(GTK_DIALOG(dlg),
	                                        GTK_RESPONSE_ACCEPT,
	                                        GTK_RESPONSE_CANCEL,
	                                        -1);

	if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
		gtk_widget_destroy(dlg);
		return TRUE;
	}

	gtk_widget_destroy(dlg);
	return FALSE;
}

static char ** run_file_chooser(const char *title,
                                GtkWindow *parent,
                                gboolean save,
                                gboolean multiple,
                                const char *suggest_name,
                                const char *suggest_dir,
                                const char *desc1,
                                const char *pattern1,
                                va_list ap)
{
	GtkWidget *filesel;
	GSList *filelist, *l;
	char *fname;
	char **fnames;
	int i, n;

	filesel = gtk_file_chooser_dialog_new(title, parent,
	                                      (save
	                                       ? GTK_FILE_CHOOSER_ACTION_SAVE
	                                       : GTK_FILE_CHOOSER_ACTION_OPEN),
	                                      GTK_STOCK_CANCEL,
	                                      GTK_RESPONSE_CANCEL,
	                                      (save
	                                       ? GTK_STOCK_SAVE
	                                       : GTK_STOCK_OPEN),
	                                      GTK_RESPONSE_ACCEPT,
	                                      NULL);

	gtk_dialog_set_alternative_button_order(GTK_DIALOG(filesel),
	                                        GTK_RESPONSE_ACCEPT,
	                                        GTK_RESPONSE_CANCEL,
	                                        -1);

	gtk_dialog_set_default_response(GTK_DIALOG(filesel),
	                                GTK_RESPONSE_ACCEPT);

	if (suggest_dir)
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(filesel),
		                                    suggest_dir);

	if (suggest_name)
		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(filesel),
		                                  suggest_name);

	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(filesel),
	                                     multiple);

	setup_file_filters(GTK_FILE_CHOOSER(filesel), desc1, pattern1, ap);

	while (gtk_dialog_run(GTK_DIALOG(filesel)) == GTK_RESPONSE_ACCEPT) {
		if (save) {
			fname = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(filesel));
			if (!fname || !prompt_overwrite(fname, GTK_WINDOW(filesel))) {
				g_free(fname);
				continue;
			}

			fnames = g_new(char *, 2);
			fnames[0] = fname;
			fnames[1] = NULL;

			gtk_widget_destroy(filesel);
			return fnames;
		}
		else {
			filelist = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(filesel));
			if (!filelist)
				continue;

			n = g_slist_length(filelist);
			fnames = g_new(char *, n + 1);
			i = 0;
			for (l = filelist; l; l = l->next)
				fnames[i++] = l->data;
			g_slist_free(filelist);
			fnames[n] = NULL;

			for (i = 0; i < n; i++)
				if (!g_file_test(fnames[i], G_FILE_TEST_EXISTS))
					break;
			if (i < n) {
				g_strfreev(fnames);
				continue;
			}

			gtk_widget_destroy(filesel);
			return fnames;
		}
	}

	gtk_widget_destroy(filesel);
	return NULL;
}

#endif  /* ! G_OS_WIN32 */

char * prompt_open_file(const char *title,
                        GtkWindow *parent,
                        const char *suggest_dir,
                        const char *desc1,
                        const char *pattern1,
                        ...)
{
	char **result, *fname;
	va_list ap;

	va_start(ap, pattern1);
	result = run_file_chooser(title, parent, FALSE, FALSE,
	                          NULL, suggest_dir,
	                          desc1, pattern1, ap);
	va_end(ap);

	if (!result || !result[0] || result[1]) {
		g_strfreev(result);
		return NULL;
	}
	else {
		fname = result[0];
		g_free(result);
		return fname;
	}
}

char ** prompt_open_files(const char *title,
                          GtkWindow *parent,
                          const char *suggest_dir,
                          const char *desc1,
                          const char *pattern1,
                          ...)
{
	char **result;
	va_list ap;

	va_start(ap, pattern1);
	result = run_file_chooser(title, parent, FALSE, TRUE,
	                          NULL, suggest_dir,
	                          desc1, pattern1, ap);
	va_end(ap);
	return result;
}

char * prompt_save_file(const char *title,
                        GtkWindow *parent,
                        const char *suggest_name,
                        const char *suggest_dir,
                        const char *desc1,
                        const char *pattern1,
                        ...)
{
	char **result, *fname;
	va_list ap;

	va_start(ap, pattern1);
	result = run_file_chooser(title, parent, TRUE, FALSE,
	                          suggest_name, suggest_dir,
	                          desc1, pattern1, ap);
	va_end(ap);

	if (!result || !result[0] || result[1]) {
		g_strfreev(result);
		return NULL;
	}
	else {
		fname = result[0];
		g_free(result);
		return fname;
	}
}

