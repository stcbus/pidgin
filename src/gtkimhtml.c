/*
 * GtkIMHtml
 *
 * Gaim is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "debug.h"
#include "util.h"
#include "gtkimhtml.h"
#include "gtksourceiter.h"
#include <gtk/gtk.h>
#include <glib/gerror.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#ifdef HAVE_LANGINFO_CODESET
#include <langinfo.h>
#include <locale.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif

#ifdef ENABLE_NLS
#  include <libintl.h>
#  define _(x) gettext(x)
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define N_(String) (String)
#  define _(x) (x)
#endif

#include <pango/pango-font.h>

/* GTK+ < 2.2.2 hack, see ui.h for details. */
#ifndef GTK_WRAP_WORD_CHAR
#define GTK_WRAP_WORD_CHAR GTK_WRAP_WORD
#endif

#define TOOLTIP_TIMEOUT 500

/* GTK+ 2.0 hack */
#if (!GTK_CHECK_VERSION(2,2,0))
#define gtk_widget_get_clipboard(x, y) gtk_clipboard_get(y)
#endif

static void preinsert_cb(GtkTextBuffer *buffer, GtkTextIter *iter, gchar *text, gint len, GtkIMHtml *imhtml);
static void insert_cb(GtkTextBuffer *buffer, GtkTextIter *iter, gchar *text, gint len, GtkIMHtml *imhtml);
static gboolean gtk_imhtml_is_amp_escape (const gchar *string, gchar **replace, gint *length);
void gtk_imhtml_close_tags(GtkIMHtml *imhtml, GtkTextIter *iter);
static void gtk_imhtml_link_drag_rcv_cb(GtkWidget *widget, GdkDragContext *dc, guint x, guint y, GtkSelectionData *sd, guint info, guint t, GtkIMHtml *imhtml);
static void mark_set_cb(GtkTextBuffer *buffer, GtkTextIter *arg1, GtkTextMark *mark, GtkIMHtml *imhtml);
static void hijack_menu_cb(GtkIMHtml *imhtml, GtkMenu *menu, gpointer data);
static void paste_received_cb (GtkClipboard *clipboard, GtkSelectionData *selection_data, gpointer data);
static void paste_plaintext_received_cb (GtkClipboard *clipboard, const gchar *text, gpointer data);

/* POINT_SIZE converts from AIM font sizes to point sizes.  It probably should be redone in such a
 * way that it base the sizes off the default font size rather than using arbitrary font sizes. */
#define MAX_FONT_SIZE 7
#define POINT_SIZE(x) (options & GTK_IMHTML_USE_POINTSIZE ? x : _point_sizes [MIN ((x), MAX_FONT_SIZE) - 1])
static gdouble _point_sizes [] = { .69444444, .8333333, 1, 1.2, 1.44, 1.728, 2.0736};

enum { 
	TARGET_HTML,
	TARGET_UTF8_STRING,
	TARGET_COMPOUND_TEXT,
	TARGET_STRING,
	TARGET_TEXT
};

enum {
	DRAG_URL
};

enum {
	URL_CLICKED,
	BUTTONS_UPDATE,
	TOGGLE_FORMAT,
	CLEAR_FORMAT,
	UPDATE_FORMAT,
	LAST_SIGNAL
};
static guint signals [LAST_SIGNAL] = { 0 };

GtkTargetEntry selection_targets[] = {
	{ "text/html", 0, TARGET_HTML },
	{ "UTF8_STRING", 0, TARGET_UTF8_STRING },
	{ "COMPOUND_TEXT", 0, TARGET_COMPOUND_TEXT },
	{ "STRING", 0, TARGET_STRING },
	{ "TEXT", 0, TARGET_TEXT}};

GtkTargetEntry link_drag_drop_targets[] = {
	{"x-url/ftp", 0, DRAG_URL},
	{"x-url/http", 0, DRAG_URL},
	{"text/uri-list", 0, DRAG_URL},
	{"_NETSCAPE_URL", 0, DRAG_URL}};

#ifdef _WIN32
/* Win32 clipboard format value, and functions to convert back and
 * forth between HTML and the clipboard format.
 */
static UINT win_html_fmt;

static gchar *
clipboard_win32_to_html(char *clipboard) {
	const char *begin, *end;
	gchar *html;

	begin = strstr(clipboard, "<!--StartFragment");
	while(*begin++ != '>');
	end = strstr(clipboard, "<!--EndFragment");
	html = g_strstrip(g_strndup(begin, (end ? (end - begin) : strlen(begin))));
	return html;
}

static gchar *
clipboard_html_to_win32(char *html) {
	int length;
	gchar *ret;
	GString *clipboard;

	if (html == NULL)
		return NULL;

	length = strlen(html);
	clipboard = g_string_new ("Version:0.9\r\n");
	g_string_append(clipboard, "StartHTML:0000000105\r\n");
	g_string_append(clipboard, g_strdup_printf("EndHTML:%010d\r\n", 143 + length));
	g_string_append(clipboard, "StartFragment:0000000105\r\n");
	g_string_append(clipboard, g_strdup_printf("EndFragment:%010d\r\n", 143 + length));
	g_string_append(clipboard, "<!--StartFragment-->");
	g_string_append(clipboard, html);
	g_string_append(clipboard, "<!--EndFragment-->");
	ret = clipboard->str;
	g_string_free(clipboard, FALSE);
	return ret;
}
#endif

static GtkSmileyTree*
gtk_smiley_tree_new ()
{
	return g_new0 (GtkSmileyTree, 1);
}

static void
gtk_smiley_tree_insert (GtkSmileyTree *tree,
			GtkIMHtmlSmiley *smiley)
{
	GtkSmileyTree *t = tree;
	const gchar *x = smiley->smile;

	if (!strlen (x))
		return;

	while (*x) {
		gchar *pos;
		gint index;

		if (!t->values)
			t->values = g_string_new ("");

		pos = strchr (t->values->str, *x);
		if (!pos) {
			t->values = g_string_append_c (t->values, *x);
			index = t->values->len - 1;
			t->children = g_realloc (t->children, t->values->len * sizeof (GtkSmileyTree *));
			t->children [index] = g_new0 (GtkSmileyTree, 1);
		} else
			index = GPOINTER_TO_INT(pos) - GPOINTER_TO_INT(t->values->str);

		t = t->children [index];

		x++;
	}

	t->image = smiley;
}


void gtk_smiley_tree_destroy (GtkSmileyTree *tree)
{
	GSList *list = g_slist_append (NULL, tree);

	while (list) {
		GtkSmileyTree *t = list->data;
		gint i;
		list = g_slist_remove(list, t);
		if (t && t->values) {
			for (i = 0; i < t->values->len; i++)
				list = g_slist_append (list, t->children [i]);
			g_string_free (t->values, TRUE);
			g_free (t->children);
		}
		g_free (t);
	}
}

static gboolean gtk_size_allocate_cb(GtkIMHtml *widget, GtkAllocation *alloc, gpointer user_data)
{
	GdkRectangle rect;
	int xminus;

	gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(widget), &rect);
	if(widget->old_rect.width != rect.width || widget->old_rect.height != rect.height){
		GList *iter = GTK_IMHTML(widget)->scalables;

		xminus = gtk_text_view_get_left_margin(GTK_TEXT_VIEW(widget)) +
		         gtk_text_view_get_right_margin(GTK_TEXT_VIEW(widget));

		while(iter){
			GtkIMHtmlScalable *scale = GTK_IMHTML_SCALABLE(iter->data);
			scale->scale(scale, rect.width - xminus, rect.height);

			iter = iter->next;
		}
	}

	widget->old_rect = rect;
	return FALSE;
}

static gint
gtk_imhtml_tip_paint (GtkIMHtml *imhtml)
{
	PangoLayout *layout;

	g_return_val_if_fail(GTK_IS_IMHTML(imhtml), FALSE);

	layout = gtk_widget_create_pango_layout(imhtml->tip_window, imhtml->tip);

	gtk_paint_flat_box (imhtml->tip_window->style, imhtml->tip_window->window,
						GTK_STATE_NORMAL, GTK_SHADOW_OUT, NULL, imhtml->tip_window,
						"tooltip", 0, 0, -1, -1);

	gtk_paint_layout (imhtml->tip_window->style, imhtml->tip_window->window, GTK_STATE_NORMAL,
					  FALSE, NULL, imhtml->tip_window, NULL, 4, 4, layout);

	g_object_unref(layout);
	return FALSE;
}

static gint
gtk_imhtml_tip (gpointer data)
{
	GtkIMHtml *imhtml = data;
	PangoFontMetrics *font_metrics;
	PangoLayout *layout;
	PangoFont *font;

	gint gap, x, y, h, w, scr_w, baseline_skip;

	g_return_val_if_fail(GTK_IS_IMHTML(imhtml), FALSE);

	if (!imhtml->tip || !GTK_WIDGET_DRAWABLE (GTK_WIDGET(imhtml))) {
		imhtml->tip_timer = 0;
		return FALSE;
	}

	if (imhtml->tip_window){
		gtk_widget_destroy (imhtml->tip_window);
		imhtml->tip_window = NULL;
	}

	imhtml->tip_timer = 0;
	imhtml->tip_window = gtk_window_new (GTK_WINDOW_POPUP);
	gtk_widget_set_app_paintable (imhtml->tip_window, TRUE);
	gtk_window_set_resizable (GTK_WINDOW (imhtml->tip_window), FALSE);
	gtk_widget_set_name (imhtml->tip_window, "gtk-tooltips");
	g_signal_connect_swapped (G_OBJECT (imhtml->tip_window), "expose_event",
							  G_CALLBACK (gtk_imhtml_tip_paint), imhtml);

	gtk_widget_ensure_style (imhtml->tip_window);
	layout = gtk_widget_create_pango_layout(imhtml->tip_window, imhtml->tip);
	font = pango_context_load_font(pango_layout_get_context(layout),
			      imhtml->tip_window->style->font_desc);

	if (font == NULL) {
		char *tmp = pango_font_description_to_string(
					imhtml->tip_window->style->font_desc);

		gaim_debug(GAIM_DEBUG_ERROR, "gtk_imhtml_tip",
			"pango_context_load_font() couldn't load font: '%s'\n",
			tmp);
		g_free(tmp);

		return FALSE;
		
	}

	font_metrics = pango_font_get_metrics(font, NULL);
	

	pango_layout_get_pixel_size(layout, &scr_w, NULL);
	gap = PANGO_PIXELS((pango_font_metrics_get_ascent(font_metrics) +
					   pango_font_metrics_get_descent(font_metrics))/ 4);

	if (gap < 2)
		gap = 2;
	baseline_skip = PANGO_PIXELS(pango_font_metrics_get_ascent(font_metrics) +
								pango_font_metrics_get_descent(font_metrics));
	w = 8 + scr_w;
	h = 8 + baseline_skip;

	gdk_window_get_pointer (NULL, &x, &y, NULL);
	if (GTK_WIDGET_NO_WINDOW (GTK_WIDGET(imhtml)))
		y += GTK_WIDGET(imhtml)->allocation.y;

	scr_w = gdk_screen_width();

	x -= ((w >> 1) + 4);

	if ((x + w) > scr_w)
		x -= (x + w) - scr_w;
	else if (x < 0)
		x = 0;

	y = y + PANGO_PIXELS(pango_font_metrics_get_ascent(font_metrics) +
						pango_font_metrics_get_descent(font_metrics));

	gtk_widget_set_size_request (imhtml->tip_window, w, h);
	gtk_widget_show (imhtml->tip_window);
	gtk_window_move (GTK_WINDOW(imhtml->tip_window), x, y);

	pango_font_metrics_unref(font_metrics);
	g_object_unref(layout);

	return FALSE;
}

gboolean gtk_motion_event_notify(GtkWidget *imhtml, GdkEventMotion *event, gpointer data)
{
	GtkTextIter iter;
	GdkWindow *win = event->window;
	int x, y;
	char *tip = NULL;
	GSList *tags = NULL, *templist = NULL;
	gdk_window_get_pointer(GTK_WIDGET(imhtml)->window, NULL, NULL, NULL);
	gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(imhtml), GTK_TEXT_WINDOW_WIDGET,
										  event->x, event->y, &x, &y);
	gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(imhtml), &iter, x, y);
	tags = gtk_text_iter_get_tags(&iter);

	templist = tags;
	while (templist) {
		GtkTextTag *tag = templist->data;
		tip = g_object_get_data(G_OBJECT(tag), "link_url");
		if (tip)
			break;
		templist = templist->next;
	}

	if (GTK_IMHTML(imhtml)->tip) {
		if ((tip == GTK_IMHTML(imhtml)->tip)) {
			return FALSE;
		}
		/* We've left the cell.  Remove the timeout and create a new one below */
		if (GTK_IMHTML(imhtml)->tip_window) {
			gtk_widget_destroy(GTK_IMHTML(imhtml)->tip_window);
			GTK_IMHTML(imhtml)->tip_window = NULL;
		}
		if (GTK_IMHTML(imhtml)->editable)
			gdk_window_set_cursor(win, GTK_IMHTML(imhtml)->text_cursor);
		else
			gdk_window_set_cursor(win, GTK_IMHTML(imhtml)->arrow_cursor);
		if (GTK_IMHTML(imhtml)->tip_timer)
			g_source_remove(GTK_IMHTML(imhtml)->tip_timer);
		GTK_IMHTML(imhtml)->tip_timer = 0;
	}

	if(tip){
		if (!GTK_IMHTML(imhtml)->editable)
			gdk_window_set_cursor(win, GTK_IMHTML(imhtml)->hand_cursor);
		GTK_IMHTML(imhtml)->tip_timer = g_timeout_add (TOOLTIP_TIMEOUT,
							       gtk_imhtml_tip, imhtml);
	}

	GTK_IMHTML(imhtml)->tip = tip;
	g_slist_free(tags);
	return FALSE;
}

gboolean gtk_leave_event_notify(GtkWidget *imhtml, GdkEventCrossing *event, gpointer data)
{
	/* when leaving the widget, clear any current & pending tooltips and restore the cursor */
	if (GTK_IMHTML(imhtml)->tip_window) {
		gtk_widget_destroy(GTK_IMHTML(imhtml)->tip_window);
		GTK_IMHTML(imhtml)->tip_window = NULL;
	}
	if (GTK_IMHTML(imhtml)->tip_timer) {
		g_source_remove(GTK_IMHTML(imhtml)->tip_timer);
		GTK_IMHTML(imhtml)->tip_timer = 0;
	}
	if (GTK_IMHTML(imhtml)->editable)
		gdk_window_set_cursor(event->window, GTK_IMHTML(imhtml)->text_cursor);
	else
		gdk_window_set_cursor(event->window, GTK_IMHTML(imhtml)->arrow_cursor);

	/* propagate the event normally */
	return FALSE;
}

/*
 * XXX - This should be removed eventually.
 *
 * This function exists to work around a gross bug in GtkTextView.
 * Basically, we short circuit ctrl+a and ctrl+end because they make
 * el program go boom.
 *
 * It's supposed to be fixed in gtk2.2.  You can view the bug report at
 * http://bugzilla.gnome.org/show_bug.cgi?id=107939
 */

/*
 * I'm adding some keyboard shortcuts too.
 */

gboolean gtk_key_pressed_cb(GtkIMHtml *imhtml, GdkEventKey *event, gpointer data)
{
	char buf[7];
	buf[0] = '\0';

	if (event->state & GDK_CONTROL_MASK)
		switch (event->keyval) {
#if (!GTK_CHECK_VERSION(2,2,0))
		case 'a':
			return TRUE;
			break;

		case GDK_Home:
			return TRUE;
			break;

		case GDK_End:
			return TRUE;
			break;
#endif /* !(Gtk+ >= 2.2.0) */
		
		case 'b':  /* ctrl-b is GDK_Left, which moves backwards. */
		case 'B':
			if (imhtml->format_functions & GTK_IMHTML_BOLD) {
				if(imhtml->html_shortcuts) {
					gtk_imhtml_toggle_bold(imhtml);
					return TRUE;
				}
			}
			return FALSE;
			break;

#ifdef LAY_OFF_MY_FRIGGIN_KEYBINDINGS
		case 'f':
		case 'F':
			/*set_toggle(gtkconv->toolbar.font,
			  !gtk_toggle_button_get_active(
			  GTK_TOGGLE_BUTTON(gtkconv->toolbar.font)));*/
			
			return TRUE;
			break;
#endif /* this doesn't even DO anything */
			
		case 'i':
		case 'I':
			if (imhtml->format_functions & GTK_IMHTML_ITALIC) {
				if(imhtml->html_shortcuts) {
					gtk_imhtml_toggle_italic(imhtml);
					return TRUE;
				}
			}
			return FALSE;
			break;
			
		case 'u':  /* ctrl-u is GDK_Clear, which clears the line. */
		case 'U':
			if (imhtml->format_functions & GTK_IMHTML_UNDERLINE) {
				if(imhtml->html_shortcuts) {
					gtk_imhtml_toggle_underline(imhtml);
					return TRUE;
				}
			}
			return FALSE;
			break;
			
		case '-':
			if (imhtml->format_functions & GTK_IMHTML_SHRINK)
				gtk_imhtml_font_shrink(imhtml);
			return TRUE;
			break;
			
		case '=':
		case '+':
			if (imhtml->format_functions & GTK_IMHTML_GROW)
				gtk_imhtml_font_grow(imhtml);
			return TRUE;
			break;

		case '1': strcpy(buf, ":-)");  break;
		case '2': strcpy(buf, ":-(");  break;
		case '3': strcpy(buf, ";-)");  break;
		case '4': strcpy(buf, ":-P");  break;
		case '5': strcpy(buf, "=-O");  break;
		case '6': strcpy(buf, ":-*");  break;
		case '7': strcpy(buf, ">:o");  break;
		case '8': strcpy(buf, "8-)");  break;
		case '!': strcpy(buf, ":-$");  break;
		case '@': strcpy(buf, ":-!");  break;
		case '#': strcpy(buf, ":-[");  break;
		case '$': strcpy(buf, "O:-)"); break;
		case '%': strcpy(buf, ":-/");  break;
		case '^': strcpy(buf, ":'(");  break;
		case '&': strcpy(buf, ":-X");  break;
		case '*': strcpy(buf, ":-D");  break;
		}
	if (*buf && imhtml->smiley_shortcuts) {
		gtk_imhtml_insert_smiley(imhtml, imhtml->protocol_name, buf);
		return TRUE;
	}
	return FALSE;
}

static void paste_unformatted_cb(GtkMenuItem *menu, GtkIMHtml *imhtml)
{
	GtkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(imhtml), GDK_SELECTION_CLIPBOARD);

	gtk_clipboard_request_text(clipboard, paste_plaintext_received_cb, imhtml);

}

static void hijack_menu_cb(GtkIMHtml *imhtml, GtkMenu *menu, gpointer data)
{
	GtkWidget *menuitem;

	menuitem = gtk_menu_item_new_with_mnemonic(_("Pa_ste As Text"));
	gtk_widget_show(menuitem);
	gtk_widget_set_sensitive(menuitem,
	                        (imhtml->editable &&
	                        gtk_clipboard_wait_is_text_available(
	                        gtk_widget_get_clipboard(GTK_WIDGET(imhtml), GDK_SELECTION_CLIPBOARD))));
	/* put it after "Paste" */
	gtk_menu_shell_insert(GTK_MENU_SHELL(menu), menuitem, 3);

	g_signal_connect(G_OBJECT(menuitem), "activate",
					 G_CALLBACK(paste_unformatted_cb), imhtml);
}

static void gtk_imhtml_clipboard_get(GtkClipboard *clipboard, GtkSelectionData *selection_data, guint info, GtkIMHtml *imhtml) {
	char *text;
	gboolean primary;
	GtkTextIter start, end;
	GtkTextMark *sel = gtk_text_buffer_get_selection_bound(imhtml->text_buffer);
	GtkTextMark *ins = gtk_text_buffer_get_insert(imhtml->text_buffer);
	
	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &start, sel);
	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &end, ins);
	primary = gtk_widget_get_clipboard(GTK_WIDGET(imhtml), GDK_SELECTION_PRIMARY) == clipboard;

	if (info == TARGET_HTML) {
		gsize len;
		char *selection;
		GString *str = g_string_new(NULL);
		if (primary) {
			text = gtk_imhtml_get_markup_range(imhtml, &start, &end);
		} else 
			text = imhtml->clipboard_html_string;

		/* Mozilla asks that we start our text/html with the Unicode byte order mark */
		str = g_string_append_unichar(str, 0xfeff);
		str = g_string_append(str, text);
		str = g_string_append_unichar(str, 0x0000);
		selection = g_convert(str->str, str->len, "UCS-2", "UTF-8", NULL, &len, NULL);
		gtk_selection_data_set(selection_data, gdk_atom_intern("text/html", FALSE), 16, selection, len);
		g_string_free(str, TRUE);
		g_free(selection);
	} else {
		if (primary) {
			text = gtk_imhtml_get_text(imhtml, &start, &end);
		} else
			text = imhtml->clipboard_text_string;
		gtk_selection_data_set_text(selection_data, text, strlen(text));
	}
	if (primary) /* This was allocated here */
		g_free(text);
 }

static void gtk_imhtml_primary_clipboard_clear(GtkClipboard *clipboard, GtkIMHtml *imhtml)
{
	GtkTextIter insert;
	GtkTextIter selection_bound;

	gtk_text_buffer_get_iter_at_mark (imhtml->text_buffer, &insert,
					  gtk_text_buffer_get_mark (imhtml->text_buffer, "insert"));
	gtk_text_buffer_get_iter_at_mark (imhtml->text_buffer, &selection_bound,
					  gtk_text_buffer_get_mark (imhtml->text_buffer, "selection_bound"));

	if (!gtk_text_iter_equal (&insert, &selection_bound))
		gtk_text_buffer_move_mark (imhtml->text_buffer,
					   gtk_text_buffer_get_mark (imhtml->text_buffer, "selection_bound"),
					   &insert);
}

static void copy_clipboard_cb(GtkIMHtml *imhtml, gpointer unused)
{
	GtkTextIter start, end;
	GtkTextMark *sel = gtk_text_buffer_get_selection_bound(imhtml->text_buffer);
	GtkTextMark *ins = gtk_text_buffer_get_insert(imhtml->text_buffer);

	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &start, sel);
	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &end, ins);

	gtk_clipboard_set_with_owner(gtk_widget_get_clipboard(GTK_WIDGET(imhtml), GDK_SELECTION_CLIPBOARD),
				     selection_targets, sizeof(selection_targets) / sizeof(GtkTargetEntry),
				     (GtkClipboardGetFunc)gtk_imhtml_clipboard_get,
				     (GtkClipboardClearFunc)NULL, G_OBJECT(imhtml));

	if (imhtml->clipboard_html_string) {
		g_free(imhtml->clipboard_html_string);
		g_free(imhtml->clipboard_text_string);
	}

	imhtml->clipboard_html_string = gtk_imhtml_get_markup_range(imhtml, &start, &end);
	imhtml->clipboard_text_string = gtk_imhtml_get_text(imhtml, &start, &end);

#ifdef _WIN32
	/* We're going to still copy plain text, but let's toss the "HTML Format"
	   we need into the windows clipboard now as well.	*/
	HGLOBAL hdata;
	gchar *clipboard = clipboard_html_to_win32(imhtml->clipboard_html_string);
	gchar *buffer;
	gint length = strlen(clipboard);
	if(clipboard != NULL) {
		OpenClipboard(NULL);
		hdata = GlobalAlloc(GMEM_MOVEABLE, length);
		buffer = GlobalLock(hdata);
        memcpy(buffer, clipboard, length);
		GlobalUnlock(hdata);
        SetClipboardData(win_html_fmt, hdata);
		CloseClipboard();
		g_free(clipboard);
	}
#endif

	g_signal_stop_emission_by_name(imhtml, "copy-clipboard");
}

static void cut_clipboard_cb(GtkIMHtml *imhtml, gpointer unused)
{
	GtkTextIter start, end;
	GtkTextMark *sel = gtk_text_buffer_get_selection_bound(imhtml->text_buffer);
	GtkTextMark *ins = gtk_text_buffer_get_insert(imhtml->text_buffer);

	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &start, sel);
	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &end, ins);

	gtk_clipboard_set_with_owner(gtk_widget_get_clipboard(GTK_WIDGET(imhtml), GDK_SELECTION_CLIPBOARD),
				     selection_targets, sizeof(selection_targets) / sizeof(GtkTargetEntry),
				     (GtkClipboardGetFunc)gtk_imhtml_clipboard_get,
				     (GtkClipboardClearFunc)NULL, G_OBJECT(imhtml));

	if (imhtml->clipboard_html_string) {
		g_free(imhtml->clipboard_html_string);
		g_free(imhtml->clipboard_text_string);
	}

	imhtml->clipboard_html_string = gtk_imhtml_get_markup_range(imhtml, &start, &end);
	imhtml->clipboard_text_string = gtk_imhtml_get_text(imhtml, &start, &end);

#ifdef _WIN32
	/* We're going to still copy plain text, but let's toss the "HTML Format"
	   we need into the windows clipboard now as well.	*/
	HGLOBAL hdata;
	gchar *clipboard = clipboard_html_to_win32(imhtml->clipboard_html_string);
	gchar *buffer;
	gint length = strlen(clipboard);
	if(clipboard != NULL) {
		OpenClipboard(NULL);
		hdata = GlobalAlloc(GMEM_MOVEABLE, length);
		buffer = GlobalLock(hdata);
        memcpy(buffer, clipboard, length);
		GlobalUnlock(hdata);
        SetClipboardData(win_html_fmt, hdata);
		CloseClipboard();
		g_free(clipboard);
	}
#endif

	if (imhtml->editable)
		gtk_text_buffer_delete_selection(imhtml->text_buffer, FALSE, FALSE);
	g_signal_stop_emission_by_name(imhtml, "cut-clipboard");
}

static void imhtml_paste_insert(GtkIMHtml *imhtml, const char *text, gboolean plaintext)
{
	GtkTextIter iter;
	GtkIMHtmlOptions flags = plaintext ? 0 : GTK_IMHTML_NO_NEWLINE;

	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &iter, gtk_text_buffer_get_insert(imhtml->text_buffer));
	if (!imhtml->wbfo && !plaintext)
		gtk_imhtml_close_tags(imhtml, &iter);

	gtk_imhtml_insert_html_at_iter(imhtml, text, flags, &iter);
	gtk_text_buffer_move_mark_by_name(imhtml->text_buffer, "insert", &iter);
	gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(imhtml), gtk_text_buffer_get_insert(imhtml->text_buffer),
	                             0, FALSE, 0.0, 0.0);
}

static void paste_plaintext_received_cb (GtkClipboard *clipboard, const gchar *text, gpointer data)
{
	char *tmp;

	if (text == NULL)
		return;

	tmp = gaim_escape_html(text);
	imhtml_paste_insert(data, tmp, TRUE);
	g_free(tmp);
}

static void paste_received_cb (GtkClipboard *clipboard, GtkSelectionData *selection_data, gpointer data)
{
	char *text;
	GtkIMHtml *imhtml = data;

	if (!gtk_text_view_get_editable(GTK_TEXT_VIEW(imhtml)))
		return;

	if (selection_data->length < 0) {
		gtk_clipboard_request_text(clipboard, paste_plaintext_received_cb, imhtml);
		return;
	} else {
#if 0
		/* Here's some debug code, for figuring out what sent to us over the clipboard. */
		{
		int i;

		gaim_debug_misc("gtkimhtml", "In paste_received_cb():\n\tformat = %d, length = %d\n\t",
	                        selection_data->format, selection_data->length);

		for (i = 0; i < (/*(selection_data->format / 8) **/ selection_data->length); i++) {
			if ((i % 70) == 0)
				printf("\n\t");
			if (selection_data->data[i] == '\0')
				printf(".");
			else
				printf("%c", selection_data->data[i]);
		}
		printf("\n");
		}
#endif
		text = g_malloc(selection_data->length);
		memcpy(text, selection_data->data, selection_data->length);
	}

	if (selection_data->length >= 2 &&
		(*(guint16 *)text == 0xfeff || *(guint16 *)text == 0xfffe)) {
		/* This is UCS-2 */
		char *tmp;
		char *utf8 = g_convert(text, selection_data->length, "UTF-8", "UCS-2", NULL, NULL, NULL);
		g_free(text);
		text = utf8;
		if (!text) {
			gaim_debug_warning("gtkimhtml", "g_convert from UCS-2 failed in paste_received_cb\n");
			return;
		}
		tmp = g_utf8_next_char(text);
		memmove(text, tmp, strlen(tmp) + 1);
	}

	if (!(*text) || !g_utf8_validate(text, -1, NULL)) {
		gaim_debug_warning("gtkimhtml", "empty string or invalid UTF-8 in paste_received_cb\n");
		g_free(text);
		return;
	}

	imhtml_paste_insert(imhtml, text, FALSE);
	g_free(text);
}

static void paste_clipboard_cb(GtkIMHtml *imhtml, gpointer blah)
{
#ifdef _WIN32
	/* If we're on windows, let's see if we can get data from the HTML Format
	   clipboard before we try to paste from the GTK buffer */
	HGLOBAL hdata;
	DWORD err;
	char *buffer;
	char *text;

	if (!gtk_text_view_get_editable(GTK_TEXT_VIEW(imhtml)))
		return;

	if (IsClipboardFormatAvailable(win_html_fmt)) {
		OpenClipboard(NULL);
		hdata = GetClipboardData(win_html_fmt);
		if (hdata == NULL) {
		    err = GetLastError();
			gaim_debug_info("html clipboard", "error number %u!  See http://msdn.microsoft.com/library/en-us/debug/base/system_error_codes.asp\n", err);
			CloseClipboard();
			return;
		}
		buffer = GlobalLock(hdata);
		if (buffer == NULL) {
			err = GetLastError();
			gaim_debug_info("html clipboard", "error number %u!  See http://msdn.microsoft.com/library/en-us/debug/base/system_error_codes.asp\n", err);
			CloseClipboard();
			return;
		}
		text = clipboard_win32_to_html(buffer);
		GlobalUnlock(hdata);
		CloseClipboard();

		imhtml_paste_insert(imhtml, text, FALSE);
		g_free(text);
	} else {
#endif
	GtkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(imhtml), GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_request_contents(clipboard, gdk_atom_intern("text/html", FALSE),
				       paste_received_cb, imhtml);
#ifdef _WIN32
	}
#endif
	g_signal_stop_emission_by_name(imhtml, "paste-clipboard");
}

static void imhtml_realized_remove_primary(GtkIMHtml *imhtml, gpointer unused)
{
	gtk_text_buffer_remove_selection_clipboard(GTK_IMHTML(imhtml)->text_buffer,
	                                            gtk_widget_get_clipboard(GTK_WIDGET(imhtml), GDK_SELECTION_PRIMARY));

}

static void imhtml_destroy_add_primary(GtkIMHtml *imhtml, gpointer unused)
{
	gtk_text_buffer_add_selection_clipboard(GTK_IMHTML(imhtml)->text_buffer,
	                                        gtk_widget_get_clipboard(GTK_WIDGET(imhtml), GDK_SELECTION_PRIMARY));
}

static void mark_set_so_update_selection_cb(GtkTextBuffer *buffer, GtkTextIter *arg1, GtkTextMark *mark, GtkIMHtml *imhtml)
{
	if (gtk_text_buffer_get_selection_bounds(buffer, NULL, NULL)) {
		gtk_clipboard_set_with_owner(gtk_widget_get_clipboard(GTK_WIDGET(imhtml), GDK_SELECTION_PRIMARY),
		                             selection_targets, sizeof(selection_targets) / sizeof(GtkTargetEntry),
		                             (GtkClipboardGetFunc)gtk_imhtml_clipboard_get,
		                             (GtkClipboardClearFunc)gtk_imhtml_primary_clipboard_clear, G_OBJECT(imhtml));
	}
}

static gboolean gtk_imhtml_button_press_event(GtkIMHtml *imhtml, GdkEventButton *event, gpointer unused)
{
	if (event->button == 2) {
		int x, y;
		GtkTextIter iter;
		GtkClipboard *clipboard = gtk_widget_get_clipboard(GTK_WIDGET(imhtml), GDK_SELECTION_PRIMARY);

		if (!imhtml->editable)
			return FALSE;

		gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(imhtml),
		                                      GTK_TEXT_WINDOW_TEXT,
		                                      event->x,
		                                      event->y,
		                                      &x,
		                                      &y);
		gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(imhtml), &iter, x, y);
		gtk_text_buffer_place_cursor(imhtml->text_buffer, &iter);

		gtk_clipboard_request_contents(clipboard, gdk_atom_intern("text/html", FALSE),
				       paste_received_cb, imhtml);

		return TRUE;
        }

	return FALSE;
}

static GtkTextViewClass *parent_class = NULL;

static void
gtk_imhtml_finalize (GObject *object)
{
	GtkIMHtml *imhtml = GTK_IMHTML(object);
	GList *scalables;
	GSList *l;

	g_hash_table_destroy(imhtml->smiley_data);
	gtk_smiley_tree_destroy(imhtml->default_smilies);
	gdk_cursor_unref(imhtml->hand_cursor);
	gdk_cursor_unref(imhtml->arrow_cursor);
	gdk_cursor_unref(imhtml->text_cursor);

	if(imhtml->tip_window){
		gtk_widget_destroy(imhtml->tip_window);
	}
	if(imhtml->tip_timer)
		gtk_timeout_remove(imhtml->tip_timer);

	for(scalables = imhtml->scalables; scalables; scalables = scalables->next) {
		GtkIMHtmlScalable *scale = GTK_IMHTML_SCALABLE(scalables->data);
		scale->free(scale);
	}

	for (l = imhtml->im_images; l; l = l->next) {
		int id;
		id = GPOINTER_TO_INT(l->data);
		if (imhtml->funcs->image_unref)
			imhtml->funcs->image_unref(id);
	}

	if (imhtml->clipboard_text_string) {
		g_free(imhtml->clipboard_text_string);
		g_free(imhtml->clipboard_html_string);
	}

	g_list_free(imhtml->scalables);
	g_slist_free(imhtml->im_images);
	G_OBJECT_CLASS(parent_class)->finalize (object);
}

/* Boring GTK stuff */
static void gtk_imhtml_class_init (GtkIMHtmlClass *klass)
{
	GtkWidgetClass *widget_class = (GtkWidgetClass *) klass;
	GtkObjectClass *object_class;
	GObjectClass   *gobject_class;
	object_class = (GtkObjectClass*) klass;
	gobject_class = (GObjectClass*) klass;
	parent_class = gtk_type_class(GTK_TYPE_TEXT_VIEW);
	signals[URL_CLICKED] = g_signal_new("url_clicked",
						G_TYPE_FROM_CLASS(gobject_class),
						G_SIGNAL_RUN_FIRST,
						G_STRUCT_OFFSET(GtkIMHtmlClass, url_clicked),
						NULL,
						0,
						g_cclosure_marshal_VOID__POINTER,
						G_TYPE_NONE, 1,
						G_TYPE_POINTER);
	signals[BUTTONS_UPDATE] = g_signal_new("format_buttons_update",
					       G_TYPE_FROM_CLASS(gobject_class),
					       G_SIGNAL_RUN_FIRST,
					       G_STRUCT_OFFSET(GtkIMHtmlClass, buttons_update),
					       NULL,
					       0,
					       g_cclosure_marshal_VOID__POINTER,
					       G_TYPE_NONE, 1,
					       G_TYPE_INT);
	signals[TOGGLE_FORMAT] = g_signal_new("format_function_toggle",
					      G_TYPE_FROM_CLASS(gobject_class),
					      G_SIGNAL_RUN_FIRST,
					      G_STRUCT_OFFSET(GtkIMHtmlClass, toggle_format),
					      NULL,
					      0,
					      g_cclosure_marshal_VOID__POINTER,
					      G_TYPE_NONE, 1, 
					      G_TYPE_INT);
	signals[CLEAR_FORMAT] = g_signal_new("format_function_clear",
					      G_TYPE_FROM_CLASS(gobject_class),
					      G_SIGNAL_RUN_FIRST,
					      G_STRUCT_OFFSET(GtkIMHtmlClass, clear_format),
					      NULL,
					      0,
						  g_cclosure_marshal_VOID__VOID,
						  G_TYPE_NONE, 0);
	signals[UPDATE_FORMAT] = g_signal_new("format_function_update",
							G_TYPE_FROM_CLASS(gobject_class),
							G_SIGNAL_RUN_FIRST,
							G_STRUCT_OFFSET(GtkIMHtmlClass, update_format),
							NULL,
							0,
							g_cclosure_marshal_VOID__VOID,
							G_TYPE_NONE, 0);
	gobject_class->finalize = gtk_imhtml_finalize;

	gtk_widget_class_install_style_property(widget_class, g_param_spec_boxed("hyperlink-color",
	                                        _("Hyperlink color"),
	                                        _("Color to draw hyperlinks."),
	                                        GDK_TYPE_COLOR, G_PARAM_READABLE));
}

static void gtk_imhtml_init (GtkIMHtml *imhtml)
{
	GtkTextIter iter;
	imhtml->text_buffer = gtk_text_buffer_new(NULL);
	gtk_text_buffer_get_end_iter (imhtml->text_buffer, &iter);
	imhtml->scrollpoint = gtk_text_buffer_create_mark(imhtml->text_buffer, NULL, &iter, FALSE);
	gtk_text_view_set_buffer(GTK_TEXT_VIEW(imhtml), imhtml->text_buffer);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(imhtml), GTK_WRAP_WORD_CHAR);
	gtk_text_view_set_pixels_below_lines(GTK_TEXT_VIEW(imhtml), 5);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(imhtml), 2);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(imhtml), 2);
	/*gtk_text_view_set_indent(GTK_TEXT_VIEW(imhtml), -15);*/
	/*gtk_text_view_set_justification(GTK_TEXT_VIEW(imhtml), GTK_JUSTIFY_FILL);*/

	/* These tags will be used often and can be reused--we create them on init and then apply them by name
	 * other tags (color, size, face, etc.) will have to be created and applied dynamically
	 * Note that even though we created STRIKE, SUB, SUP, and PRE tags here, we don't really
	 * apply them anywhere yet. */
	gtk_text_buffer_create_tag(imhtml->text_buffer, "BOLD", "weight", PANGO_WEIGHT_BOLD, NULL);
	gtk_text_buffer_create_tag(imhtml->text_buffer, "ITALICS", "style", PANGO_STYLE_ITALIC, NULL);
	gtk_text_buffer_create_tag(imhtml->text_buffer, "UNDERLINE", "underline", PANGO_UNDERLINE_SINGLE, NULL);
	gtk_text_buffer_create_tag(imhtml->text_buffer, "STRIKE", "strikethrough", TRUE, NULL);
	gtk_text_buffer_create_tag(imhtml->text_buffer, "SUB", "rise", -5000, NULL);
	gtk_text_buffer_create_tag(imhtml->text_buffer, "SUP", "rise", 5000, NULL);
	gtk_text_buffer_create_tag(imhtml->text_buffer, "PRE", "family", "Monospace", NULL);
	gtk_text_buffer_create_tag(imhtml->text_buffer, "search", "background", "#22ff00", "weight", "bold", NULL);

	/* When hovering over a link, we show the hand cursor--elsewhere we show the plain ol' pointer cursor */
	imhtml->hand_cursor = gdk_cursor_new (GDK_HAND2);
	imhtml->arrow_cursor = gdk_cursor_new (GDK_LEFT_PTR);
	imhtml->text_cursor = gdk_cursor_new (GDK_XTERM);

	imhtml->show_comments = TRUE;

	imhtml->zoom = 1.0;
	imhtml->original_fsize = 0;

	imhtml->smiley_data = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, (GDestroyNotify)gtk_smiley_tree_destroy);
	imhtml->default_smilies = gtk_smiley_tree_new();

	g_signal_connect(G_OBJECT(imhtml), "size-allocate", G_CALLBACK(gtk_size_allocate_cb), NULL);
	g_signal_connect(G_OBJECT(imhtml), "motion-notify-event", G_CALLBACK(gtk_motion_event_notify), NULL);
	g_signal_connect(G_OBJECT(imhtml), "leave-notify-event", G_CALLBACK(gtk_leave_event_notify), NULL);
	g_signal_connect(G_OBJECT(imhtml), "key_press_event", G_CALLBACK(gtk_key_pressed_cb), NULL);
	g_signal_connect(G_OBJECT(imhtml), "button_press_event", G_CALLBACK(gtk_imhtml_button_press_event), NULL);
	g_signal_connect(G_OBJECT(imhtml->text_buffer), "insert-text", G_CALLBACK(preinsert_cb), imhtml);
	g_signal_connect_after(G_OBJECT(imhtml->text_buffer), "insert-text", G_CALLBACK(insert_cb), imhtml);

	gtk_drag_dest_set(GTK_WIDGET(imhtml), 0,
			  link_drag_drop_targets, sizeof(link_drag_drop_targets) / sizeof(GtkTargetEntry),
			  GDK_ACTION_COPY);
	g_signal_connect(G_OBJECT(imhtml), "drag_data_received", G_CALLBACK(gtk_imhtml_link_drag_rcv_cb), imhtml);

	g_signal_connect(G_OBJECT(imhtml), "copy-clipboard", G_CALLBACK(copy_clipboard_cb), NULL);
	g_signal_connect(G_OBJECT(imhtml), "cut-clipboard", G_CALLBACK(cut_clipboard_cb), NULL);
	g_signal_connect(G_OBJECT(imhtml), "paste-clipboard", G_CALLBACK(paste_clipboard_cb), NULL);
	g_signal_connect_after(G_OBJECT(imhtml), "realize", G_CALLBACK(imhtml_realized_remove_primary), NULL);
	g_signal_connect(G_OBJECT(imhtml), "unrealize", G_CALLBACK(imhtml_destroy_add_primary), NULL);

	g_signal_connect_after(G_OBJECT(GTK_IMHTML(imhtml)->text_buffer), "mark-set",
		               G_CALLBACK(mark_set_so_update_selection_cb), imhtml);

	gtk_widget_add_events(GTK_WIDGET(imhtml), GDK_LEAVE_NOTIFY_MASK);

	imhtml->clipboard_text_string = NULL;
	imhtml->clipboard_html_string = NULL;

	imhtml->tip = NULL;
	imhtml->tip_timer = 0;
	imhtml->tip_window = NULL;

	imhtml->edit.bold = FALSE;
	imhtml->edit.italic = FALSE;
	imhtml->edit.underline = FALSE;
	imhtml->edit.forecolor = NULL;
	imhtml->edit.backcolor = NULL;
	imhtml->edit.fontface = NULL;
	imhtml->edit.fontsize = 0;
	imhtml->edit.link = NULL;


	imhtml->scalables = NULL;

	gtk_imhtml_set_editable(imhtml, FALSE);

	g_signal_connect(G_OBJECT(imhtml), "populate-popup", 
					 G_CALLBACK(hijack_menu_cb), NULL);

#ifdef _WIN32
	/* Register HTML Format as desired clipboard format */
	win_html_fmt = RegisterClipboardFormat("HTML Format");
#endif
}

GtkWidget *gtk_imhtml_new(void *a, void *b)
{
	return GTK_WIDGET(g_object_new(gtk_imhtml_get_type(), NULL));
}

GType gtk_imhtml_get_type()
{
	static GType imhtml_type = 0;

	if (!imhtml_type) {
		static const GTypeInfo imhtml_info = {
			sizeof(GtkIMHtmlClass),
			NULL,
			NULL,
			(GClassInitFunc) gtk_imhtml_class_init,
			NULL,
			NULL,
			sizeof (GtkIMHtml),
			0,
			(GInstanceInitFunc) gtk_imhtml_init
		};

		imhtml_type = g_type_register_static(gtk_text_view_get_type(),
				"GtkIMHtml", &imhtml_info, 0);
	}

	return imhtml_type;
}

struct url_data {
	GObject *object;
	gchar *url;
};

static void url_data_destroy(gpointer mydata)
{
	struct url_data *data = mydata;
	g_object_unref(data->object);
	g_free(data->url);
	g_free(data);
}

static void url_open(GtkWidget *w, struct url_data *data) {
	if(!data) return;
	g_signal_emit(data->object, signals[URL_CLICKED], 0, data->url);

}

static void url_copy(GtkWidget *w, gchar *url) {
	GtkClipboard *clipboard;

	clipboard = gtk_widget_get_clipboard(w, GDK_SELECTION_PRIMARY);
	gtk_clipboard_set_text(clipboard, url, -1);

	clipboard = gtk_widget_get_clipboard(w, GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text(clipboard, url, -1);
}

/* The callback for an event on a link tag. */
gboolean tag_event(GtkTextTag *tag, GObject *imhtml, GdkEvent *event, GtkTextIter *arg2, gpointer unused) {
	GdkEventButton *event_button = (GdkEventButton *) event;
	if (GTK_IMHTML(imhtml)->editable)
		return FALSE;
	if (event->type == GDK_BUTTON_RELEASE) {
		if ((event_button->button == 1) || (event_button->button == 2)) {
			GtkTextIter start, end;
			/* we shouldn't open a URL if the user has selected something: */
			if (gtk_text_buffer_get_selection_bounds(
						gtk_text_iter_get_buffer(arg2),	&start, &end))
				return FALSE;

			/* A link was clicked--we emit the "url_clicked" signal
			 * with the URL as the argument */
			g_object_ref(G_OBJECT(tag));
			g_signal_emit(imhtml, signals[URL_CLICKED], 0, g_object_get_data(G_OBJECT(tag), "link_url"));
			g_object_unref(G_OBJECT(tag));
			return FALSE;
		} else if(event_button->button == 3) {
			GtkWidget *img, *item, *menu;
			struct url_data *tempdata = g_new(struct url_data, 1);
			tempdata->object = g_object_ref(imhtml);
			tempdata->url = g_strdup(g_object_get_data(G_OBJECT(tag), "link_url"));

			/* Don't want the tooltip around if user right-clicked on link */
			if (GTK_IMHTML(imhtml)->tip_window) {
				gtk_widget_destroy(GTK_IMHTML(imhtml)->tip_window);
				GTK_IMHTML(imhtml)->tip_window = NULL;
			}
			if (GTK_IMHTML(imhtml)->tip_timer) {
				g_source_remove(GTK_IMHTML(imhtml)->tip_timer);
				GTK_IMHTML(imhtml)->tip_timer = 0;
			}
			if (GTK_IMHTML(imhtml)->editable)
				gdk_window_set_cursor(event_button->window, GTK_IMHTML(imhtml)->text_cursor);
			else
				gdk_window_set_cursor(event_button->window, GTK_IMHTML(imhtml)->arrow_cursor);
			menu = gtk_menu_new();
			g_object_set_data_full(G_OBJECT(menu), "x-imhtml-url-data", tempdata, url_data_destroy);

			/* buttons and such */

			if (!strncmp(tempdata->url, "mailto:", 7))
			{
				/* Copy E-Mail Address */
				img = gtk_image_new_from_stock(GTK_STOCK_COPY,
											   GTK_ICON_SIZE_MENU);
				item = gtk_image_menu_item_new_with_mnemonic(
					_("_Copy E-Mail Address"));
				gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
				g_signal_connect(G_OBJECT(item), "activate",
								 G_CALLBACK(url_copy), tempdata->url + 7);
				gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
			}
			else
			{
				/* Copy Link Location */
				img = gtk_image_new_from_stock(GTK_STOCK_COPY,
											   GTK_ICON_SIZE_MENU);
				item = gtk_image_menu_item_new_with_mnemonic(
					_("_Copy Link Location"));
				gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
				g_signal_connect(G_OBJECT(item), "activate",
								 G_CALLBACK(url_copy), tempdata->url);
				gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

				/* Open Link in Browser */
				img = gtk_image_new_from_stock(GTK_STOCK_JUMP_TO,
											   GTK_ICON_SIZE_MENU);
				item = gtk_image_menu_item_new_with_mnemonic(
					_("_Open Link in Browser"));
				gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
				g_signal_connect(G_OBJECT(item), "activate",
								 G_CALLBACK(url_open), tempdata);
				gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
			}


			gtk_widget_show_all(menu);
			gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
							event_button->button, event_button->time);

			return TRUE;
		}
	}
	if(event->type == GDK_BUTTON_PRESS && event_button->button == 3)
		return TRUE; /* Clicking the right mouse button on a link shouldn't
						be caught by the regular GtkTextView menu */
	else
		return FALSE; /* Let clicks go through if we didn't catch anything */
}

static void
gtk_imhtml_link_drag_rcv_cb(GtkWidget *widget, GdkDragContext *dc, guint x, guint y,
 			    GtkSelectionData *sd, guint info, guint t, GtkIMHtml *imhtml)
{
	if(gtk_imhtml_get_editable(imhtml) && sd->data){
		gchar **links;
		gchar *link;

		gaim_str_strip_cr(sd->data);

		links = g_strsplit(sd->data, "\n", 0);
		while((link = *links++) != NULL){
			if(gaim_str_has_prefix(link, "http://") ||
			   gaim_str_has_prefix(link, "https://") ||
			   gaim_str_has_prefix(link, "ftp://")){
				gtk_imhtml_insert_link(imhtml, gtk_text_buffer_get_insert(imhtml->text_buffer), link, link);
			} else if (link=='\0') {
				/* Ignore blank lines */
			} else {
				/* Special reasons, aka images being put in via other tag, etc. */
			}
		}

		gtk_drag_finish(dc, TRUE, (dc->action == GDK_ACTION_MOVE), t);
	} else {
		gtk_drag_finish(dc, FALSE, FALSE, t);
	}
}

/* this isn't used yet
static void
gtk_smiley_tree_remove (GtkSmileyTree     *tree,
			GtkIMHtmlSmiley   *smiley)
{
	GtkSmileyTree *t = tree;
	const gchar *x = smiley->smile;
	gint len = 0;

	while (*x) {
		gchar *pos;

		if (!t->values)
			return;

		pos = strchr (t->values->str, *x);
		if (pos)
			t = t->children [(int) pos - (int) t->values->str];
		else
			return;

		x++; len++;
	}

	if (t->image) {
		t->image = NULL;
	}
}
*/


static gint
gtk_smiley_tree_lookup (GtkSmileyTree *tree,
			const gchar   *text)
{
	GtkSmileyTree *t = tree;
	const gchar *x = text;
	gint len = 0;
	gchar *amp;
	gint alen;

	while (*x) {
		gchar *pos;

		if (!t->values)
			break;

		if(*x == '&' && gtk_imhtml_is_amp_escape(x, &amp, &alen)) {
		    len += alen - strlen(amp);
		    x += alen - strlen(amp);
		    pos = strchr (t->values->str, *amp);
		}
		else
		    pos = strchr (t->values->str, *x);

		if (pos)
			t = t->children [GPOINTER_TO_INT(pos) - GPOINTER_TO_INT(t->values->str)];
		else
			break;

		x++; len++;
	}

	if (t->image)
		return len;

	return 0;
}

void
gtk_imhtml_associate_smiley (GtkIMHtml       *imhtml,
			     gchar           *sml,
			     GtkIMHtmlSmiley *smiley)
{
	GtkSmileyTree *tree;
	g_return_if_fail (imhtml != NULL);
	g_return_if_fail (GTK_IS_IMHTML (imhtml));

	if (sml == NULL)
		tree = imhtml->default_smilies;
	else if ((tree = g_hash_table_lookup(imhtml->smiley_data, sml))) {
	} else {
		tree = gtk_smiley_tree_new();
		g_hash_table_insert(imhtml->smiley_data, g_strdup(sml), tree);
	}

	gtk_smiley_tree_insert (tree, smiley);
}

static gboolean
gtk_imhtml_is_smiley (GtkIMHtml   *imhtml,
		      GSList      *fonts,
		      const gchar *text,
		      gint        *len)
{
	GtkSmileyTree *tree;
	GtkIMHtmlFontDetail *font;
	char *sml = NULL;

	if (fonts) {
		font = fonts->data;
		sml = font->sml;
	}

	if (sml == NULL)
		tree = imhtml->default_smilies;
	else {
		tree = g_hash_table_lookup(imhtml->smiley_data, sml);
	}
	if (tree == NULL)
		return FALSE;

	*len = gtk_smiley_tree_lookup (tree, text);
	return (*len > 0);
}

GdkPixbufAnimation *
gtk_smiley_tree_image (GtkIMHtml     *imhtml,
		       const gchar   *sml,
		       const gchar   *text)
{
	GtkSmileyTree *t;
	const gchar *x = text;
	if (sml == NULL)
		t = imhtml->default_smilies;
	else
		t = g_hash_table_lookup(imhtml->smiley_data, sml);


	if (t == NULL)
		return sml ? gtk_smiley_tree_image(imhtml, NULL, text) : NULL;

	while (*x) {
		gchar *pos;

		if (!t->values) {
			return sml ? gtk_smiley_tree_image(imhtml, NULL, text) : NULL;
		}

		pos = strchr (t->values->str, *x);
		if (pos) {
			t = t->children [GPOINTER_TO_INT(pos) - GPOINTER_TO_INT(t->values->str)];
		} else {
			return sml ? gtk_smiley_tree_image(imhtml, NULL, text) : NULL;
		}
		x++;
	}

	if (!t->image->file)
		return NULL;

	if (!t->image->icon)
		t->image->icon = gdk_pixbuf_animation_new_from_file(t->image->file, NULL);

	return t->image->icon;
}

#define VALID_TAG(x)	if (!g_ascii_strncasecmp (string, x ">", strlen (x ">"))) {	\
				*tag = g_strndup (string, strlen (x));		\
				*len = strlen (x) + 1;				\
				return TRUE;					\
			}							\
			(*type)++

#define VALID_OPT_TAG(x)	if (!g_ascii_strncasecmp (string, x " ", strlen (x " "))) {	\
					const gchar *c = string + strlen (x " ");	\
					gchar e = '"';					\
					gboolean quote = FALSE;				\
					while (*c) {					\
						if (*c == '"' || *c == '\'') {		\
							if (quote && (*c == e))		\
								quote = !quote;		\
							else if (!quote) {		\
								quote = !quote;		\
								e = *c;			\
							}				\
						} else if (!quote && (*c == '>'))	\
							break;				\
						c++;					\
					}						\
					if (*c) {					\
						*tag = g_strndup (string, c - string);	\
						*len = c - string + 1;			\
						return TRUE;				\
					}						\
				}							\
				(*type)++


static gboolean
gtk_imhtml_is_amp_escape (const gchar *string,
			  gchar       **replace,
			  gint        *length)
{
	static char buf[7];
	g_return_val_if_fail (string != NULL, FALSE);
	g_return_val_if_fail (replace != NULL, FALSE);
	g_return_val_if_fail (length != NULL, FALSE);

	if (!g_ascii_strncasecmp (string, "&amp;", 5)) {
		*replace = "&";
		*length = 5;
	} else if (!g_ascii_strncasecmp (string, "&lt;", 4)) {
		*replace = "<";
		*length = 4;
	} else if (!g_ascii_strncasecmp (string, "&gt;", 4)) {
		*replace = ">";
		*length = 4;
	} else if (!g_ascii_strncasecmp (string, "&nbsp;", 6)) {
		*replace = " ";
		*length = 6;
	} else if (!g_ascii_strncasecmp (string, "&copy;", 6)) {
		*replace = "©";
		*length = 6;
	} else if (!g_ascii_strncasecmp (string, "&quot;", 6)) {
		*replace = "\"";
		*length = 6;
	} else if (!g_ascii_strncasecmp (string, "&reg;", 5)) {
		*replace = "®";
		*length = 5;
	} else if (!g_ascii_strncasecmp (string, "&apos;", 6)) {
		*replace = "\'";
		*length = 6;
	} else if (*(string + 1) == '#') {
		guint pound = 0;
		if ((sscanf (string, "&#%u;", &pound) == 1) && pound != 0) {
			int buflen;
			if (*(string + 3 + (gint)log10 (pound)) != ';')
				return FALSE;
			buflen = g_unichar_to_utf8((gunichar)pound, buf);
			buf[buflen] = '\0';
			*replace = buf;
			*length = 2;
			while (isdigit ((gint) string [*length])) (*length)++;
			if (string [*length] == ';') (*length)++;
		} else {
			return FALSE;
		}
	} else {
		return FALSE;
	}

	return TRUE;
}

static gboolean
gtk_imhtml_is_tag (const gchar *string,
		   gchar      **tag,
		   gint        *len,
		   gint        *type)
{
	char *close;
	*type = 1;


	if (!(close = strchr (string, '>')))
		return FALSE;

	VALID_TAG ("B");
	VALID_TAG ("BOLD");
	VALID_TAG ("/B");
	VALID_TAG ("/BOLD");
	VALID_TAG ("I");
	VALID_TAG ("ITALIC");
	VALID_TAG ("/I");
	VALID_TAG ("/ITALIC");
	VALID_TAG ("U");
	VALID_TAG ("UNDERLINE");
	VALID_TAG ("/U");
	VALID_TAG ("/UNDERLINE");
	VALID_TAG ("S");
	VALID_TAG ("STRIKE");
	VALID_TAG ("/S");
	VALID_TAG ("/STRIKE");
	VALID_TAG ("SUB");
	VALID_TAG ("/SUB");
	VALID_TAG ("SUP");
	VALID_TAG ("/SUP");
	VALID_TAG ("PRE");
	VALID_TAG ("/PRE");
	VALID_TAG ("TITLE");
	VALID_TAG ("/TITLE");
	VALID_TAG ("BR");
	VALID_TAG ("HR");
	VALID_TAG ("/FONT");
	VALID_TAG ("/A");
	VALID_TAG ("P");
	VALID_TAG ("/P");
	VALID_TAG ("H3");
	VALID_TAG ("/H3");
	VALID_TAG ("HTML");
	VALID_TAG ("/HTML");
	VALID_TAG ("BODY");
	VALID_TAG ("/BODY");
	VALID_TAG ("FONT");
	VALID_TAG ("HEAD");
	VALID_TAG ("/HEAD");
	VALID_TAG ("BINARY");
	VALID_TAG ("/BINARY");

	VALID_OPT_TAG ("HR");
	VALID_OPT_TAG ("FONT");
	VALID_OPT_TAG ("BODY");
	VALID_OPT_TAG ("A");
	VALID_OPT_TAG ("IMG");
	VALID_OPT_TAG ("P");
	VALID_OPT_TAG ("H3");
	VALID_OPT_TAG ("HTML");

	VALID_TAG ("CITE");
	VALID_TAG ("/CITE");
	VALID_TAG ("EM");
	VALID_TAG ("/EM");
	VALID_TAG ("STRONG");
	VALID_TAG ("/STRONG");

	VALID_OPT_TAG ("SPAN");
	VALID_TAG ("/SPAN");
	VALID_TAG ("BR/"); /* hack until gtkimhtml handles things better */
	VALID_TAG ("IMG");
	VALID_TAG("SPAN");
	VALID_OPT_TAG("BR");

	if (!g_ascii_strncasecmp(string, "!--", strlen ("!--"))) {
		gchar *e = strstr (string + strlen("!--"), "-->");
		if (e) {
			*len = e - string + strlen ("-->");
			*tag = g_strndup (string + strlen ("!--"), *len - strlen ("!---->"));
			return TRUE;
		}
	}

	*type = -1;
	*len = close - string + 1;
	*tag = g_strndup(string, *len - 1);
	return TRUE;
}

static gchar*
gtk_imhtml_get_html_opt (gchar       *tag,
			 const gchar *opt)
{
	gchar *t = tag;
	gchar *e, *a;
	gchar *val;
	gint len;
	gchar *c;
	GString *ret;

	while (g_ascii_strncasecmp (t, opt, strlen (opt))) {
		gboolean quote = FALSE;
		if (*t == '\0') break;
		while (*t && !((*t == ' ') && !quote)) {
			if (*t == '\"')
				quote = ! quote;
			t++;
		}
		while (*t && (*t == ' ')) t++;
	}

	if (!g_ascii_strncasecmp (t, opt, strlen (opt))) {
		t += strlen (opt);
	} else {
		return NULL;
	}

	if ((*t == '\"') || (*t == '\'')) {
		e = a = ++t;
		while (*e && (*e != *(t - 1))) e++;
		if  (*e == '\0') {
			return NULL;
		} else
			val = g_strndup(a, e - a);
	} else {
		e = a = t;
		while (*e && !isspace ((gint) *e)) e++;
		val = g_strndup(a, e - a);
	}

	ret = g_string_new("");
	e = val;
	while(*e) {
		if(gtk_imhtml_is_amp_escape(e, &c, &len)) {
			ret = g_string_append(ret, c);
			e += len;
		} else {
			ret = g_string_append_c(ret, *e);
			e++;
		}
	}

	g_free(val);

	return g_string_free(ret, FALSE);
}

/* Inline CSS Support - Douglas Thrift */
static gchar*
gtk_imhtml_get_css_opt (gchar       *style,
			 const gchar *opt)
{
	gchar *t = style;
	gchar *e, *a;
	gchar *val;
	gint len;
	gchar *c;
	GString *ret;

	while (g_ascii_strncasecmp (t, opt, strlen (opt))) {
/*		gboolean quote = FALSE; */
		if (*t == '\0') break;
		while (*t && !((*t == ' ') /*&& !quote*/)) {
/*			if (*t == '\"')
				quote = ! quote; */
			t++;
		}
		while (*t && (*t == ' ')) t++;
	}

	if (!g_ascii_strncasecmp (t, opt, strlen (opt))) {
		t += strlen (opt);
	} else {
		return NULL;
	}

/*	if ((*t == '\"') || (*t == '\'')) {
		e = a = ++t;
		while (*e && (*e != *(t - 1))) e++;
		if  (*e == '\0') {
			return NULL;
		} else
			val = g_strndup(a, e - a);
	} else {
		e = a = t;
		while (*e && !isspace ((gint) *e)) e++;
		val = g_strndup(a, e - a);
	}*/

	e = a = t;
	while (*e && *e != ';') e++;
	val = g_strndup(a, e - a);

	ret = g_string_new("");
	e = val;
	while(*e) {
		if(gtk_imhtml_is_amp_escape(e, &c, &len)) {
			ret = g_string_append(ret, c);
			e += len;
		} else {
			ret = g_string_append_c(ret, *e);
			e++;
		}
	}

	g_free(val);
	val = ret->str;
	g_string_free(ret, FALSE);
	return val;
}

static const char *accepted_protocols[] = {
	"http://",
	"https://",
	"ftp://"
};
                                                                                                              
static const int accepted_protocols_size = 3;

/* returns if the beginning of the text is a protocol. If it is the protocol, returns the length so
   the caller knows how long the protocol string is. */
int gtk_imhtml_is_protocol(const char *text)
{
	gint i;

	for(i=0; i<accepted_protocols_size; i++){
		if( strncasecmp(text, accepted_protocols[i], strlen(accepted_protocols[i])) == 0  ){
			return strlen(accepted_protocols[i]);
		}
	}
	return 0;
}

/*
 <KingAnt> marv: The two IM image functions in oscar are gaim_odc_send_im and gaim_odc_incoming


[19:58] <Robot101> marv: images go into the imgstore, a refcounted... well.. hash. :)
[19:59] <KingAnt> marv: I think the image tag used by the core is something like <img id="#"/>
[19:59] Ro0tSiEgE robert42 RobFlynn Robot101 ross22 roz 
[20:00] <KingAnt> marv: Where the ID is the what is returned when you add the image to the imgstore using gaim_imgstore_add
[20:00] <marv> Robot101: so how does the image get passed to serv_got_im() and serv_send_im()? just as the <img id="#" and then the prpl looks it up from the store?
[20:00] <KingAnt> marv: Right
[20:00] <marv> alright

Here's my plan with IMImages. make gtk_imhtml_[append|insert]_text_with_images instead just
gtkimhtml_[append|insert]_text (hrm maybe it should be called html instead of text), add a
function for gaim to register for look up images, i.e. gtk_imhtml_set_get_img_fnc, so that
images can be looked up like that, instead of passing a GSList of them.
 */

void gtk_imhtml_append_text_with_images (GtkIMHtml        *imhtml,
                                         const gchar      *text,
                                         GtkIMHtmlOptions  options,
					 GSList *unused)
{
	GtkTextIter iter, ins, sel;
	GdkRectangle rect;
	int y, height, ins_offset = 0, sel_offset = 0;
	gboolean fixins = FALSE, fixsel = FALSE;

	g_return_if_fail (imhtml != NULL);
	g_return_if_fail (GTK_IS_IMHTML (imhtml));
	g_return_if_fail (text != NULL);


	gtk_text_buffer_get_end_iter(imhtml->text_buffer, &iter);
	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &ins, gtk_text_buffer_get_insert(imhtml->text_buffer));
	if (gtk_text_iter_equal(&iter, &ins) && gtk_text_buffer_get_selection_bounds(imhtml->text_buffer, NULL, NULL)) {
		fixins = TRUE;
		ins_offset = gtk_text_iter_get_offset(&ins);
	}

	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &sel, gtk_text_buffer_get_selection_bound(imhtml->text_buffer));
	if (gtk_text_iter_equal(&iter, &sel) && gtk_text_buffer_get_selection_bounds(imhtml->text_buffer, NULL, NULL)) {
		fixsel = TRUE;
		sel_offset = gtk_text_iter_get_offset(&sel);
	}

	gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(imhtml), &rect);
	gtk_text_view_get_line_yrange(GTK_TEXT_VIEW(imhtml), &iter, &y, &height);


	if(((y + height) - (rect.y + rect.height)) > height
	   && gtk_text_buffer_get_char_count(imhtml->text_buffer)){
		options |= GTK_IMHTML_NO_SCROLL;
	}

	gtk_imhtml_insert_html_at_iter(imhtml, text, options, &iter);

	if (fixins) {
		gtk_text_buffer_get_iter_at_offset(imhtml->text_buffer, &ins, ins_offset);
		gtk_text_buffer_move_mark(imhtml->text_buffer, gtk_text_buffer_get_insert(imhtml->text_buffer), &ins);
	}

	if (fixsel) {
		gtk_text_buffer_get_iter_at_offset(imhtml->text_buffer, &sel, sel_offset);
		gtk_text_buffer_move_mark(imhtml->text_buffer, gtk_text_buffer_get_selection_bound(imhtml->text_buffer), &sel);
	}

	if (!(options & GTK_IMHTML_NO_SCROLL)) {
		gtk_imhtml_scroll_to_end(imhtml);
	}
}

void gtk_imhtml_scroll_to_end(GtkIMHtml *imhtml)
{
	GtkTextIter iter;
	/* If this seems backwards at first glance, well it's not.
	 * It means scroll such that the mark is closest to the top,
	 * and closest to the right as possible. Remember kids, you have
	 * to scroll left to move a given spot closest to the right,
	 * and scroll down to move a spot closest to the top.
	 */
	gtk_text_buffer_get_end_iter(imhtml->text_buffer, &iter);
	gtk_text_iter_set_line_offset(&iter, 0);
	gtk_text_buffer_move_mark(imhtml->text_buffer, imhtml->scrollpoint, &iter);
	gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(imhtml), imhtml->scrollpoint,
	                             0, TRUE, 1.0, 0.0);
}

void gtk_imhtml_insert_html_at_iter(GtkIMHtml        *imhtml,
                                    const gchar      *text,
                                    GtkIMHtmlOptions  options,
                                    GtkTextIter      *iter)
{
	GdkRectangle rect;
	gint pos = 0;
	gchar *ws;
	gchar *tag;
	gchar *bg = NULL;
	gint len;
	gint tlen, smilelen, wpos=0;
	gint type;
	const gchar *c;
	gchar *amp;
	gint len_protocol;

	guint	bold = 0,
		italics = 0,
		underline = 0,
		strike = 0,
		sub = 0,
		sup = 0,
		title = 0,
		pre = 0;

	GSList *fonts = NULL;
	GObject *object;
	GtkIMHtmlScalable *scalable = NULL;

	g_return_if_fail (imhtml != NULL);
	g_return_if_fail (GTK_IS_IMHTML (imhtml));
	g_return_if_fail (text != NULL);
	c = text;
	len = strlen(text);
	ws = g_malloc(len + 1);
	ws[0] = 0;

	while (pos < len) {
		if (*c == '<' && gtk_imhtml_is_tag (c + 1, &tag, &tlen, &type)) {
			c++;
			pos++;
			ws[wpos] = '\0';
			switch (type)
				{
				case 1:		/* B */
				case 2:		/* BOLD */
				case 54:	/* STRONG */

					gtk_text_buffer_insert(imhtml->text_buffer, iter, ws, wpos);

					if ((bold == 0) && (imhtml->format_functions & GTK_IMHTML_BOLD))
						gtk_imhtml_toggle_bold(imhtml);
					bold++;
					ws[0] = '\0'; wpos = 0;
					break;
				case 3:		/* /B */
				case 4:		/* /BOLD */
				case 55:	/* /STRONG */
					gtk_text_buffer_insert(imhtml->text_buffer, iter, ws, wpos);
					ws[0] = '\0'; wpos = 0;

					if (bold)
						bold--;
					if ((bold == 0) && (imhtml->format_functions & GTK_IMHTML_BOLD) && !imhtml->wbfo)
						gtk_imhtml_toggle_bold(imhtml);
					break;
				case 5:		/* I */
				case 6:		/* ITALIC */
				case 52:	/* EM */
					gtk_text_buffer_insert(imhtml->text_buffer, iter, ws, wpos);
					ws[0] = '\0'; wpos = 0;
					if ((italics == 0) && (imhtml->format_functions & GTK_IMHTML_ITALIC))
						gtk_imhtml_toggle_italic(imhtml);
					italics++;
					break;
				case 7:		/* /I */
				case 8:		/* /ITALIC */
				case 53:	/* /EM */
					gtk_text_buffer_insert(imhtml->text_buffer, iter, ws, wpos);
					ws[0] = '\0'; wpos = 0;
					if (italics)
						italics--;
					if ((italics == 0) && (imhtml->format_functions & GTK_IMHTML_ITALIC) && !imhtml->wbfo)
						gtk_imhtml_toggle_italic(imhtml);
					break;
				case 9:		/* U */
				case 10:	/* UNDERLINE */
					gtk_text_buffer_insert(imhtml->text_buffer, iter, ws, wpos);
					ws[0] = '\0'; wpos = 0;
					if ((underline == 0) && (imhtml->format_functions & GTK_IMHTML_UNDERLINE))
						gtk_imhtml_toggle_underline(imhtml);
					underline++;
					break;
				case 11:	/* /U */
				case 12:	/* /UNDERLINE */
					gtk_text_buffer_insert(imhtml->text_buffer, iter, ws, wpos);
					ws[0] = '\0'; wpos = 0;
					if (underline)
						underline--;
					if ((underline == 0) && (imhtml->format_functions & GTK_IMHTML_UNDERLINE) && !imhtml->wbfo)
						gtk_imhtml_toggle_underline(imhtml);
					break;
				case 13:	/* S */
				case 14:	/* STRIKE */
					/* FIXME: reimplement this */
					strike++;
					break;
				case 15:	/* /S */
				case 16:	/* /STRIKE */
					/* FIXME: reimplement this */
					if (strike)
						strike--;
					break;
				case 17:	/* SUB */
					/* FIXME: reimpliment this */
					sub++;
					break;
				case 18:	/* /SUB */
					/* FIXME: reimpliment this */
					if (sub)
						sub--;
					break;
				case 19:	/* SUP */
					/* FIXME: reimplement this */
					sup++;
				break;
				case 20:	/* /SUP */
					/* FIXME: reimplement this */
					if (sup)
						sup--;
					break;
				case 21:	/* PRE */
					/* FIXME: reimplement this */
					pre++;
					break;
				case 22:	/* /PRE */
					/* FIXME: reimplement this */
					if (pre)
						pre--;
					break;
				case 23:	/* TITLE */
					/* FIXME: what was this supposed to do anyway? */
					title++;
					break;
				case 24:	/* /TITLE */
					/* FIXME: make this undo whatever 23 was supposed to do */
					if (title) {
						if (options & GTK_IMHTML_NO_TITLE) {
							wpos = 0;
							ws [wpos] = '\0';
						}
						title--;
					}
					break;
				case 25:	/* BR */
				case 58:	/* BR/ */
				case 61:	/* BR (opt) */
					ws[wpos] = '\n';
					wpos++;
					break;
				case 26:        /* HR */
				case 42:        /* HR (opt) */
				{
					int minus;

					ws[wpos++] = '\n';
					gtk_text_buffer_insert(imhtml->text_buffer, iter, ws, wpos);

					scalable = gtk_imhtml_hr_new();
					gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(imhtml), &rect);
					scalable->add_to(scalable, imhtml, iter);
					minus = gtk_text_view_get_left_margin(GTK_TEXT_VIEW(imhtml)) +
					        gtk_text_view_get_right_margin(GTK_TEXT_VIEW(imhtml));
					scalable->scale(scalable, rect.width - minus, rect.height);
					imhtml->scalables = g_list_append(imhtml->scalables, scalable);
					ws[0] = '\0'; wpos = 0;
					ws[wpos++] = '\n';

					break;
				}
				case 27:	/* /FONT */
					if (fonts && !imhtml->wbfo) {
						GtkIMHtmlFontDetail *font = fonts->data;
						gtk_text_buffer_insert(imhtml->text_buffer, iter, ws, wpos);
						ws[0] = '\0'; wpos = 0;
						/* NEW_BIT (NEW_TEXT_BIT); */

						if (font->face && (imhtml->format_functions & GTK_IMHTML_FACE)) {
							gtk_imhtml_toggle_fontface(imhtml, NULL);
							g_free (font->face);
						}
						if (font->fore && (imhtml->format_functions & GTK_IMHTML_FORECOLOR)) {
							gtk_imhtml_toggle_forecolor(imhtml, NULL);
							g_free (font->fore);
						}
						if (font->back && (imhtml->format_functions & GTK_IMHTML_BACKCOLOR)) {
							gtk_imhtml_toggle_backcolor(imhtml, NULL);
							g_free (font->back);
						}
						if (font->sml)
							g_free (font->sml);
						g_free (font);

						if ((font->size != 3) && (imhtml->format_functions & (GTK_IMHTML_GROW|GTK_IMHTML_SHRINK)))
							gtk_imhtml_font_set_size(imhtml, 3);

						fonts = fonts->next;
						if (fonts) {
							GtkIMHtmlFontDetail *font = fonts->data;

							if (font->face && (imhtml->format_functions & GTK_IMHTML_FACE))
								gtk_imhtml_toggle_fontface(imhtml, font->face);
							if (font->fore && (imhtml->format_functions & GTK_IMHTML_FORECOLOR))
								gtk_imhtml_toggle_forecolor(imhtml, font->fore);
							if (font->back && (imhtml->format_functions & GTK_IMHTML_BACKCOLOR))
								gtk_imhtml_toggle_backcolor(imhtml, font->back);
							if ((font->size != 3) && (imhtml->format_functions & (GTK_IMHTML_GROW|GTK_IMHTML_SHRINK)))
								gtk_imhtml_font_set_size(imhtml, font->size);
						}
					}
						break;
				case 28:        /* /A    */
					gtk_text_buffer_insert(imhtml->text_buffer, iter, ws, wpos);
					gtk_imhtml_toggle_link(imhtml, NULL);
					ws[0] = '\0'; wpos = 0;
					break;

				case 29:	/* P */
				case 30:	/* /P */
				case 31:	/* H3 */
				case 32:	/* /H3 */
				case 33:	/* HTML */
				case 34:	/* /HTML */
				case 35:	/* BODY */
				case 36:	/* /BODY */
				case 37:	/* FONT */
				case 38:	/* HEAD */
				case 39:	/* /HEAD */
				case 40:	/* BINARY */
				case 41:	/* /BINARY */
					break;
				case 43:	/* FONT (opt) */
					{
						gchar *color, *back, *face, *size, *sml;
						GtkIMHtmlFontDetail *font, *oldfont = NULL;
						color = gtk_imhtml_get_html_opt (tag, "COLOR=");
						back = gtk_imhtml_get_html_opt (tag, "BACK=");
						face = gtk_imhtml_get_html_opt (tag, "FACE=");
						size = gtk_imhtml_get_html_opt (tag, "SIZE=");
						sml = gtk_imhtml_get_html_opt (tag, "SML=");
						if (!(color || back || face || size || sml))
							break;

						gtk_text_buffer_insert(imhtml->text_buffer, iter, ws, wpos);
						ws[0] = '\0'; wpos = 0;

						font = g_new0 (GtkIMHtmlFontDetail, 1);
						if (fonts)
							oldfont = fonts->data;

						if (color && !(options & GTK_IMHTML_NO_COLOURS) && (imhtml->format_functions & GTK_IMHTML_FORECOLOR)) {
							font->fore = color;
							gtk_imhtml_toggle_forecolor(imhtml, font->fore);
						}
						//else if (oldfont && oldfont->fore)
						//	font->fore = g_strdup(oldfont->fore);

						if (back && !(options & GTK_IMHTML_NO_COLOURS) && (imhtml->format_functions & GTK_IMHTML_BACKCOLOR)) {
							font->back = back;
							gtk_imhtml_toggle_backcolor(imhtml, font->back);
						}
						//else if (oldfont && oldfont->back)
						//	font->back = g_strdup(oldfont->back);

						if (face && !(options & GTK_IMHTML_NO_FONTS) && (imhtml->format_functions & GTK_IMHTML_FACE)) {
							font->face = face;
							gtk_imhtml_toggle_fontface(imhtml, font->face);
						}
						//else if (oldfont && oldfont->face)
						//		font->face = g_strdup(oldfont->face);

						if (sml)
							font->sml = sml;
						else if (oldfont && oldfont->sml)
							font->sml = g_strdup(oldfont->sml);

						if (size && !(options & GTK_IMHTML_NO_SIZES) && (imhtml->format_functions & (GTK_IMHTML_GROW|GTK_IMHTML_SHRINK))) {
							if (*size == '+') {
								sscanf (size + 1, "%hd", &font->size);
								font->size += 3;
							} else if (*size == '-') {
								sscanf (size + 1, "%hd", &font->size);
								font->size = MAX (0, 3 - font->size);
							} else if (isdigit (*size)) {
								sscanf (size, "%hd", &font->size);
					}
							if (font->size > 100)
								font->size = 100;
						} else if (oldfont)
							font->size = oldfont->size;
						else
							font->size = 3;
						if ((imhtml->format_functions & (GTK_IMHTML_GROW|GTK_IMHTML_SHRINK)))
							gtk_imhtml_font_set_size(imhtml, font->size);
						g_free(size);
						fonts = g_slist_prepend (fonts, font);
					}
					break;
				case 44:	/* BODY (opt) */
					if (!(options & GTK_IMHTML_NO_COLOURS)) {
						char *bgcolor = gtk_imhtml_get_html_opt (tag, "BGCOLOR=");
						if (bgcolor && (imhtml->format_functions & GTK_IMHTML_BACKCOLOR)) {
							gtk_text_buffer_insert(imhtml->text_buffer, iter, ws, wpos);
							ws[0] = '\0'; wpos = 0;
							/* NEW_BIT(NEW_TEXT_BIT); */
							if (bg)
								g_free(bg);
							bg = bgcolor;
							gtk_imhtml_toggle_backcolor(imhtml, bg);
						}
					}
					break;
				case 45:	/* A (opt) */
					{
						gchar *href = gtk_imhtml_get_html_opt (tag, "HREF=");
						if (href && (imhtml->format_functions & GTK_IMHTML_LINK)) {
							gtk_text_buffer_insert(imhtml->text_buffer, iter, ws, wpos);
							ws[0] = '\0'; wpos = 0;
							gtk_imhtml_toggle_link(imhtml, href);
						}
					}
					break;
				case 46:	/* IMG (opt) */
				case 59:	/* IMG */
					{
						const char *id;

						gtk_text_buffer_insert(imhtml->text_buffer, iter, ws, wpos);
						ws[0] = '\0'; wpos = 0;

						if (!(imhtml->format_functions & GTK_IMHTML_IMAGE))
							break;

						id = gtk_imhtml_get_html_opt(tag, "ID=");

						gtk_imhtml_insert_image_at_iter(imhtml, atoi(id), iter);
						break;
					}
				case 47:	/* P (opt) */
				case 48:	/* H3 (opt) */
				case 49:	/* HTML (opt) */
				case 50:	/* CITE */
				case 51:	/* /CITE */
				case 56:	/* SPAN (opt) */
					/* Inline CSS Support - Douglas Thrift
					 *
					 * color
					 * background
					 * font-family
					 * font-size
					 * text-decoration: underline
					 */
					{
						gchar *style, *color, *background, *family, *size;
						gchar *textdec;
						GtkIMHtmlFontDetail *font, *oldfont = NULL;
						style = gtk_imhtml_get_html_opt (tag, "style=");

						if (!style) break;

						color = gtk_imhtml_get_css_opt (style, "color: ");
						background = gtk_imhtml_get_css_opt (style, "background: ");
						family = gtk_imhtml_get_css_opt (style,
							"font-family: ");
						size = gtk_imhtml_get_css_opt (style, "font-size: ");
						textdec = gtk_imhtml_get_css_opt (style, "text-decoration: ");

						if (!(color || family || size || background || textdec)) {
							g_free(style);
							break;
						}


						gtk_text_buffer_insert(imhtml->text_buffer, iter, ws, wpos);
						ws[0] = '\0'; wpos = 0;
						/* NEW_BIT (NEW_TEXT_BIT); */

						font = g_new0 (GtkIMHtmlFontDetail, 1);
						if (fonts)
							oldfont = fonts->data;

						if (color && !(options & GTK_IMHTML_NO_COLOURS) && (imhtml->format_functions & GTK_IMHTML_FORECOLOR))
						{
							font->fore = color;
							gtk_imhtml_toggle_forecolor(imhtml, font->fore);
						}
						else if (oldfont && oldfont->fore)
							font->fore = g_strdup(oldfont->fore);

						if (background && !(options & GTK_IMHTML_NO_COLOURS) && (imhtml->format_functions & GTK_IMHTML_BACKCOLOR))
						{
							font->back = background;
							gtk_imhtml_toggle_backcolor(imhtml, font->back);
						}
						else if (oldfont && oldfont->back)
							font->back = g_strdup(oldfont->back);

						if (family && !(options & GTK_IMHTML_NO_FONTS) && (imhtml->format_functions & GTK_IMHTML_FACE))
						{
							font->face = family;
							gtk_imhtml_toggle_fontface(imhtml, font->face);
						}
						else if (oldfont && oldfont->face)
							font->face = g_strdup(oldfont->face);
						if (font->face && (atoi(font->face) > 100)) {
							/* WTF is this? */
							g_free(font->face);
							font->face = g_strdup("100");
						}

						if (oldfont && oldfont->sml)
							font->sml = g_strdup(oldfont->sml);

						if (size && !(options & GTK_IMHTML_NO_SIZES) && (imhtml->format_functions & (GTK_IMHTML_SHRINK|GTK_IMHTML_GROW))) {
							if (g_ascii_strcasecmp(size, "xx-small") == 0)
								font->size = 1;
							else if (g_ascii_strcasecmp(size, "smaller") == 0
								  || g_ascii_strcasecmp(size, "x-small") == 0)
								font->size = 2;
							else if (g_ascii_strcasecmp(size, "larger") == 0
								  || g_ascii_strcasecmp(size, "medium") == 0)
								font->size = 4;
							else if (g_ascii_strcasecmp(size, "large") == 0)
								font->size = 5;
							else if (g_ascii_strcasecmp(size, "x-large") == 0)
								font->size = 6;
							else if (g_ascii_strcasecmp(size, "xx-large") == 0)
								font->size = 7;
							else
								font->size = 3;
						    gtk_imhtml_font_set_size(imhtml, font->size);
						}
						else if (oldfont)
						{
						    font->size = oldfont->size;
						}

						if (oldfont)
						{
						    font->underline = oldfont->underline;
						}
						if (textdec && font->underline != 1
							&& g_ascii_strcasecmp(textdec, "underline") == 0
							&& (imhtml->format_functions & GTK_IMHTML_UNDERLINE))
						{
						    gtk_imhtml_toggle_underline(imhtml);
						    font->underline = 1;
						}

						g_free(style);
						g_free(size);
						fonts = g_slist_prepend (fonts, font);
					}
					break;
				case 57:	/* /SPAN */
					/* Inline CSS Support - Douglas Thrift */
					if (fonts && !imhtml->wbfo) {
						GtkIMHtmlFontDetail *oldfont = NULL;
						GtkIMHtmlFontDetail *font = fonts->data;
						gtk_text_buffer_insert(imhtml->text_buffer, iter, ws, wpos);
						ws[0] = '\0'; wpos = 0;
						/* NEW_BIT (NEW_TEXT_BIT); */
						fonts = g_slist_remove (fonts, font);
						if (fonts)
						    oldfont = fonts->data;

						if (!oldfont) {
							gtk_imhtml_font_set_size(imhtml, 3);
							if (font->underline)
							    gtk_imhtml_toggle_underline(imhtml);
							gtk_imhtml_toggle_fontface(imhtml, NULL);
							gtk_imhtml_toggle_forecolor(imhtml, NULL);
							gtk_imhtml_toggle_backcolor(imhtml, NULL);
						}
						else
						{

						    if (font->size != oldfont->size)
							    gtk_imhtml_font_set_size(imhtml, oldfont->size);

							if (font->underline != oldfont->underline)
							    gtk_imhtml_toggle_underline(imhtml);

							if (!oldfont->face || strcmp(font->face, oldfont->face) != 0)
							    gtk_imhtml_toggle_fontface(imhtml, oldfont->face);

							if (!oldfont->fore || strcmp(font->fore, oldfont->fore) != 0)
							    gtk_imhtml_toggle_forecolor(imhtml, oldfont->fore);

							if (!oldfont->back || strcmp(font->back, oldfont->back) != 0)
						      gtk_imhtml_toggle_backcolor(imhtml, oldfont->back);
						}

						g_free (font->face);
						g_free (font->fore);
						g_free (font->back);
						g_free (font->sml);

						g_free (font);
					}
					break;
				case 60:    /* SPAN */
					break;
				case 62:	/* comment */
					/* NEW_BIT (NEW_TEXT_BIT); */
					ws[wpos] = '\0';
					gtk_text_buffer_insert(imhtml->text_buffer, iter, ws, wpos);

					if (imhtml->show_comments)
						wpos = g_snprintf (ws, len, "%s", tag);
					/* NEW_BIT (NEW_COMMENT_BIT); */
					break;
				default:
					break;
				}
			c += tlen;
			pos += tlen;
			if(tag)
				g_free(tag); /* This was allocated back in VALID_TAG() */
		} else if (gtk_imhtml_is_smiley (imhtml, fonts, c, &smilelen) || gtk_imhtml_is_smiley(imhtml, NULL, c, &smilelen)) {
			GtkIMHtmlFontDetail *fd;

			gchar *sml = NULL;
			if (fonts) {
				fd = fonts->data;
				sml = fd->sml;
			}
			gtk_text_buffer_insert(imhtml->text_buffer, iter, ws, wpos);
			wpos = g_snprintf (ws, smilelen + 1, "%s", c);

			gtk_imhtml_insert_smiley_at_iter(imhtml, sml, ws, iter);

			c += smilelen;
			pos += smilelen;
			wpos = 0;
			ws[0] = 0;
		} else if (*c == '&' && gtk_imhtml_is_amp_escape (c, &amp, &tlen)) {
			while(*amp) {
				ws [wpos++] = *amp++;
			}
			c += tlen;
			pos += tlen;
		} else if (*c == '\n') {
			if (!(options & GTK_IMHTML_NO_NEWLINE)) {
				ws[wpos] = '\n';
				wpos++;
				gtk_text_buffer_insert(imhtml->text_buffer, iter, ws, wpos);
				ws[0] = '\0';
				wpos = 0;
				/* NEW_BIT (NEW_TEXT_BIT); */
			}
			c++;
			pos++;
		} else if ((len_protocol = gtk_imhtml_is_protocol(c)) > 0){
			while(len_protocol--){
				/* Skip the next len_protocol characters, but make sure they're
				   copied into the ws array.
				*/
				 ws [wpos++] = *c++;
				 pos++;
			}
		} else if (*c) {
			ws [wpos++] = *c++;
			pos++;
		} else {
			break;
		}
	}
	gtk_text_buffer_insert(imhtml->text_buffer, iter, ws, wpos);
	ws[0] = '\0'; wpos = 0;

	/* NEW_BIT(NEW_TEXT_BIT); */

	while (fonts) {
		GtkIMHtmlFontDetail *font = fonts->data;
		fonts = g_slist_remove (fonts, font);
		if (font->face)
			g_free (font->face);
		if (font->fore)
			g_free (font->fore);
		if (font->back)
			g_free (font->back);
		if (font->sml)
			g_free (font->sml);
		g_free (font);
	}

	g_free(ws);
	if (bg)
		g_free(bg);

	if (!imhtml->wbfo)
		gtk_imhtml_close_tags(imhtml, iter);

	object = g_object_ref(G_OBJECT(imhtml));
	g_signal_emit(object, signals[UPDATE_FORMAT], 0);
	g_object_unref(object);

}

void gtk_imhtml_remove_smileys(GtkIMHtml *imhtml)
{
	g_hash_table_destroy(imhtml->smiley_data);
	gtk_smiley_tree_destroy(imhtml->default_smilies);
	imhtml->smiley_data = g_hash_table_new_full(g_str_hash, g_str_equal,
			g_free, (GDestroyNotify)gtk_smiley_tree_destroy);
	imhtml->default_smilies = gtk_smiley_tree_new();
}

void       gtk_imhtml_show_comments    (GtkIMHtml        *imhtml,
					gboolean          show)
{
	imhtml->show_comments = show;
}

void       gtk_imhtml_html_shortcuts   (GtkIMHtml        *imhtml,
                    gboolean allow)
{
	imhtml->html_shortcuts = allow;
}

void       gtk_imhtml_smiley_shortcuts (GtkIMHtml        *imhtml,
                    gboolean allow)
{
	imhtml->smiley_shortcuts = allow;
}

void
gtk_imhtml_set_protocol_name(GtkIMHtml *imhtml, gchar *protocol_name) {
	imhtml->protocol_name = protocol_name;
}

void
gtk_imhtml_clear (GtkIMHtml *imhtml)
{
	GList *del;
	GtkTextIter start, end;
	GObject *object = g_object_ref(G_OBJECT(imhtml));

	gtk_text_buffer_get_start_iter(imhtml->text_buffer, &start);
	gtk_text_buffer_get_end_iter(imhtml->text_buffer, &end);
	gtk_text_buffer_delete(imhtml->text_buffer, &start, &end);

	for(del = imhtml->scalables; del; del = del->next) {
		GtkIMHtmlScalable *scale = del->data;
		scale->free(scale);
	}
	g_list_free(imhtml->scalables);
	imhtml->scalables = NULL;

	gtk_imhtml_close_tags(imhtml, &start);

	g_signal_emit(object, signals[CLEAR_FORMAT], 0);
	g_object_unref(object);
}

void gtk_imhtml_page_up (GtkIMHtml *imhtml)
{
	GdkRectangle rect;
	GtkTextIter iter;

	gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(imhtml), &rect);
	gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(imhtml), &iter, rect.x,
									   rect.y - rect.height);
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(imhtml), &iter, 0, TRUE, 0, 0);

}
void gtk_imhtml_page_down (GtkIMHtml *imhtml)
{
	GdkRectangle rect;
	GtkTextIter iter;

	gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(imhtml), &rect);
	gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(imhtml), &iter, rect.x,
									   rect.y + rect.height);
	gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(imhtml), &iter, 0, TRUE, 0, 0);
}

/* GtkIMHtmlScalable, gtk_imhtml_image, gtk_imhtml_hr */
GtkIMHtmlScalable *gtk_imhtml_image_new(GdkPixbuf *img, const gchar *filename, int id)
{
	GtkIMHtmlImage *im_image = g_malloc(sizeof(GtkIMHtmlImage));
	GtkImage *image = GTK_IMAGE(gtk_image_new_from_pixbuf(img));

	GTK_IMHTML_SCALABLE(im_image)->scale = gtk_imhtml_image_scale;
	GTK_IMHTML_SCALABLE(im_image)->add_to = gtk_imhtml_image_add_to;
	GTK_IMHTML_SCALABLE(im_image)->free = gtk_imhtml_image_free;

	im_image->pixbuf = img;
	im_image->image = image;
	im_image->width = gdk_pixbuf_get_width(img);
	im_image->height = gdk_pixbuf_get_height(img);
	im_image->mark = NULL;
	im_image->filename = filename ? g_strdup(filename) : NULL;
	im_image->id = id;

	g_object_ref(img);
	return GTK_IMHTML_SCALABLE(im_image);
}

void gtk_imhtml_image_scale(GtkIMHtmlScalable *scale, int width, int height)
{
	GtkIMHtmlImage *image = (GtkIMHtmlImage *)scale;

	if(image->width > width || image->height > height){
		GdkPixbuf *new_image = NULL;
		float factor;
		int new_width = image->width, new_height = image->height;

		if(image->width > (width - 2)){
			factor = (float)(width)/image->width;
			new_width = width;
			new_height = image->height * factor;
		}
		if(new_height >= (height - 2)){
			factor = (float)(height)/new_height;
			new_height = height;
			new_width = new_width * factor;
		}

		new_image = gdk_pixbuf_scale_simple(image->pixbuf, new_width, new_height, GDK_INTERP_BILINEAR);
		gtk_image_set_from_pixbuf(image->image, new_image);
		g_object_unref(G_OBJECT(new_image));
	}
}

static void write_img_to_file(GtkWidget *w, GtkFileSelection *sel)
{
	const gchar *filename = gtk_file_selection_get_filename(sel);
	gchar *dirname;
	GtkIMHtmlImage *image = g_object_get_data(G_OBJECT(sel), "GtkIMHtmlImage");
	gchar *type = NULL;
	GError *error = NULL;
#if GTK_CHECK_VERSION(2,2,0)
	GSList *formats = gdk_pixbuf_get_formats();
#else
	char *basename = g_path_get_basename(filename);
	char *ext = strrchr(basename, '.');
#endif

	if (g_file_test(filename, G_FILE_TEST_IS_DIR)) {
		/* append a / if needed */
		if (filename[strlen(filename) - 1] != '/') {
			dirname = g_strconcat(filename, "/", NULL);
		} else {
			dirname = g_strdup(filename);
		}
		gtk_file_selection_set_filename(sel, dirname);
		g_free(dirname);
		return;
	}

#if GTK_CHECK_VERSION(2,2,0)
	while(formats){
		GdkPixbufFormat *format = formats->data;
		gchar **extensions = gdk_pixbuf_format_get_extensions(format);
		gpointer p = extensions;

		while(gdk_pixbuf_format_is_writable(format) && extensions && extensions[0]){
			gchar *fmt_ext = extensions[0];
			const gchar* file_ext = filename + strlen(filename) - strlen(fmt_ext);

			if(!strcmp(fmt_ext, file_ext)){
				type = gdk_pixbuf_format_get_name(format);
				break;
			}

			extensions++;
		}

		g_strfreev(p);

		if(type)
			break;

		formats = formats->next;
	}

	g_slist_free(formats);
#else
	/* this is really ugly code, but I think it will work */
	if(ext) {
		ext++;
		if(!g_ascii_strcasecmp(ext, "jpeg") || !g_ascii_strcasecmp(ext, "jpg"))
			type = g_strdup("jpeg");
		else if(!g_ascii_strcasecmp(ext, "png"))
			type = g_strdup("png");
	}

	g_free(basename);
#endif

	/* If I can't find a valid type, I will just tell the user about it and then assume
	   it's a png */
	if(!type){
		gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
						_("Unable to guess the image type based on the file extension supplied.  Defaulting to PNG."));
		type = g_strdup("png");
	}

	gdk_pixbuf_save(image->pixbuf, filename, type, &error, NULL);

	if(error){
		gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
				_("Error saving image: %s"), error->message);
		g_error_free(error);
	}

	g_free(type);
}

static void gtk_imhtml_image_save(GtkWidget *w, GtkIMHtmlImage *image)
{
	GtkWidget *sel = gtk_file_selection_new(_("Save Image"));

	if (image->filename)
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(sel), image->filename);
	g_object_set_data(G_OBJECT(sel), "GtkIMHtmlImage", image);
	g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(sel)->ok_button), "clicked",
					 G_CALLBACK(write_img_to_file), sel);

	g_signal_connect_swapped(G_OBJECT(GTK_FILE_SELECTION(sel)->ok_button), "clicked",
							 G_CALLBACK(gtk_widget_destroy), sel);
	g_signal_connect_swapped(G_OBJECT(GTK_FILE_SELECTION(sel)->cancel_button), "clicked",
							 G_CALLBACK(gtk_widget_destroy), sel);

	gtk_widget_show(sel);
}

static gboolean gtk_imhtml_image_clicked(GtkWidget *w, GdkEvent *event, GtkIMHtmlImage *image)
{
	GdkEventButton *event_button = (GdkEventButton *) event;

	if (event->type == GDK_BUTTON_RELEASE) {
		if(event_button->button == 3) {
			GtkWidget *img, *item, *menu;
			gchar *text = g_strdup_printf(_("_Save Image..."));
			menu = gtk_menu_new();

			/* buttons and such */
			img = gtk_image_new_from_stock(GTK_STOCK_SAVE, GTK_ICON_SIZE_MENU);
			item = gtk_image_menu_item_new_with_mnemonic(text);
			gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), img);
			g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(gtk_imhtml_image_save), image);
			gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);

			gtk_widget_show_all(menu);
			gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
							event_button->button, event_button->time);

			g_free(text);
			return TRUE;
		}
	}
	if(event->type == GDK_BUTTON_PRESS && event_button->button == 3)
		return TRUE; /* Clicking the right mouse button on a link shouldn't
						be caught by the regular GtkTextView menu */
	else
		return FALSE; /* Let clicks go through if we didn't catch anything */

}
void gtk_imhtml_image_free(GtkIMHtmlScalable *scale)
{
	GtkIMHtmlImage *image = (GtkIMHtmlImage *)scale;

	g_object_unref(image->pixbuf);
	if (image->filename)
		g_free(image->filename);
	g_free(scale);
}

void gtk_imhtml_image_add_to(GtkIMHtmlScalable *scale, GtkIMHtml *imhtml, GtkTextIter *iter)
{
	GtkIMHtmlImage *image = (GtkIMHtmlImage *)scale;
	GtkWidget *box = gtk_event_box_new();
	char *tag;
	GtkTextChildAnchor *anchor = gtk_text_buffer_create_child_anchor(imhtml->text_buffer, iter);

	gtk_container_add(GTK_CONTAINER(box), GTK_WIDGET(image->image));

	gtk_widget_show(GTK_WIDGET(image->image));
	gtk_widget_show(box);

	tag = g_strdup_printf("<IMG ID=\"%d\">", image->id);
	g_object_set_data_full(G_OBJECT(anchor), "gtkimhtml_htmltext", tag, g_free);
	g_object_set_data(G_OBJECT(anchor), "gtkimhtml_plaintext", "[Image]");

	gtk_text_view_add_child_at_anchor(GTK_TEXT_VIEW(imhtml), box, anchor);
	g_signal_connect(G_OBJECT(box), "event", G_CALLBACK(gtk_imhtml_image_clicked), image);
}

GtkIMHtmlScalable *gtk_imhtml_hr_new()
{
	GtkIMHtmlHr *hr = g_malloc(sizeof(GtkIMHtmlHr));

	GTK_IMHTML_SCALABLE(hr)->scale = gtk_imhtml_hr_scale;
	GTK_IMHTML_SCALABLE(hr)->add_to = gtk_imhtml_hr_add_to;
	GTK_IMHTML_SCALABLE(hr)->free = gtk_imhtml_hr_free;

	hr->sep = gtk_hseparator_new();
	gtk_widget_set_size_request(hr->sep, 5000, 2);
	gtk_widget_show(hr->sep);

	return GTK_IMHTML_SCALABLE(hr);
}

void gtk_imhtml_hr_scale(GtkIMHtmlScalable *scale, int width, int height)
{
	gtk_widget_set_size_request(((GtkIMHtmlHr *)scale)->sep, width - 2, 2);
}

void gtk_imhtml_hr_add_to(GtkIMHtmlScalable *scale, GtkIMHtml *imhtml, GtkTextIter *iter)
{
	GtkIMHtmlHr *hr = (GtkIMHtmlHr *)scale;
	GtkTextChildAnchor *anchor = gtk_text_buffer_create_child_anchor(imhtml->text_buffer, iter);
	g_object_set_data(G_OBJECT(anchor), "gtkimhtml_htmltext", "<hr>");
	g_object_set_data(G_OBJECT(anchor), "gtkimhtml_plaintext", "\n---\n");
	gtk_text_view_add_child_at_anchor(GTK_TEXT_VIEW(imhtml), hr->sep, anchor);
}

void gtk_imhtml_hr_free(GtkIMHtmlScalable *scale)
{
	g_free(scale);
}

gboolean gtk_imhtml_search_find(GtkIMHtml *imhtml, const gchar *text)
{
	GtkTextIter iter, start, end;
	gboolean new_search = TRUE;

	g_return_val_if_fail(imhtml != NULL, FALSE);
	g_return_val_if_fail(text != NULL, FALSE);

	if (imhtml->search_string && !strcmp(text, imhtml->search_string))
		new_search = FALSE;

	if (new_search) {
		gtk_imhtml_search_clear(imhtml);
		gtk_text_buffer_get_start_iter(imhtml->text_buffer, &iter);
	} else {
		gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &iter,
						 gtk_text_buffer_get_mark(imhtml->text_buffer, "search"));
	}
	imhtml->search_string = g_strdup(text);

	if (gtk_source_iter_forward_search(&iter, imhtml->search_string,
					   GTK_SOURCE_SEARCH_VISIBLE_ONLY | GTK_SOURCE_SEARCH_CASE_INSENSITIVE,
					 &start, &end, NULL)) {

		gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(imhtml), &start, 0, TRUE, 0, 0);
		gtk_text_buffer_create_mark(imhtml->text_buffer, "search", &end, FALSE);
		if (new_search) {
			gtk_text_buffer_remove_tag_by_name(imhtml->text_buffer, "search", &iter, &end);
			do
				gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "search", &start, &end);
			while (gtk_source_iter_forward_search(&end, imhtml->search_string,
							      GTK_SOURCE_SEARCH_VISIBLE_ONLY |
							      GTK_SOURCE_SEARCH_CASE_INSENSITIVE,
							      &start, &end, NULL));
		}
		return TRUE;
	}

	gtk_imhtml_search_clear(imhtml);

	return FALSE;
}

void gtk_imhtml_search_clear(GtkIMHtml *imhtml)
{
	GtkTextIter start, end;

	g_return_if_fail(imhtml != NULL);

	gtk_text_buffer_get_start_iter(imhtml->text_buffer, &start);
	gtk_text_buffer_get_end_iter(imhtml->text_buffer, &end);

	gtk_text_buffer_remove_tag_by_name(imhtml->text_buffer, "search", &start, &end);
	if (imhtml->search_string)
		g_free(imhtml->search_string);
	imhtml->search_string = NULL;
}

static GtkTextTag *find_font_forecolor_tag(GtkIMHtml *imhtml, gchar *color)
{
	gchar str[18];
	GtkTextTag *tag;

	g_snprintf(str, sizeof(str), "FORECOLOR %s", color);

	tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(imhtml->text_buffer), str);
	if (!tag)
		tag = gtk_text_buffer_create_tag(imhtml->text_buffer, str, "foreground", color, NULL);

	return tag;
}

static GtkTextTag *find_font_backcolor_tag(GtkIMHtml *imhtml, gchar *color)
{
	gchar str[18];
	GtkTextTag *tag;

	g_snprintf(str, sizeof(str), "BACKCOLOR %s", color);

	tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(imhtml->text_buffer), str);
	if (!tag)
		tag = gtk_text_buffer_create_tag(imhtml->text_buffer, str, "background", color, NULL);

	return tag;
}

static GtkTextTag *find_font_face_tag(GtkIMHtml *imhtml, gchar *face)
{
	gchar str[256];
	GtkTextTag *tag;

	g_snprintf(str, sizeof(str), "FONT FACE %s", face);
	str[255] = '\0';

	tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(imhtml->text_buffer), str);
	if (!tag)
		tag = gtk_text_buffer_create_tag(imhtml->text_buffer, str, "family", face, NULL);

	return tag;
}

static void _init_original_fsize(GtkIMHtml *imhtml)
{
	GtkTextAttributes *attr;
	attr = gtk_text_view_get_default_attributes(GTK_TEXT_VIEW(imhtml));
	imhtml->original_fsize = pango_font_description_get_size(attr->font);
	gtk_text_attributes_unref(attr);
}

static void _recalculate_font_sizes(GtkTextTag *tag, gpointer imhtml)
{
	if (strncmp(tag->name, "FONT SIZE ", 10) == 0) {
		int size;

		size = strtol(tag->name + 10, NULL, 10);
		g_object_set(G_OBJECT(tag), "size",
		             (gint) (GTK_IMHTML(imhtml)->original_fsize *
		             ((double) _point_sizes[size-1] * GTK_IMHTML(imhtml)->zoom)), NULL);
	}


}

void gtk_imhtml_font_zoom(GtkIMHtml *imhtml, double zoom)
{
	GtkRcStyle *s;
	PangoFontDescription *font_desc = pango_font_description_new();

	imhtml->zoom = zoom;

	if (!imhtml->original_fsize)
		_init_original_fsize(imhtml);

	gtk_text_tag_table_foreach(gtk_text_buffer_get_tag_table(imhtml->text_buffer),
	                           _recalculate_font_sizes, imhtml);

	pango_font_description_set_size(font_desc, (gint)((double) imhtml->original_fsize * zoom));

	s = gtk_widget_get_modifier_style(GTK_WIDGET(imhtml));
	s->font_desc = font_desc;
	gtk_widget_modify_style(GTK_WIDGET(imhtml), s);
}

static GtkTextTag *find_font_size_tag(GtkIMHtml *imhtml, int size)
{
	gchar str[24];
	GtkTextTag *tag;

	if (!imhtml->original_fsize)
		_init_original_fsize(imhtml);

	g_snprintf(str, sizeof(str), "FONT SIZE %d", size);
	str[23] = '\0';

	tag = gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(imhtml->text_buffer), str);
	if (!tag) {
		/* For reasons I don't understand, setting "scale" here scaled based on some default
		 * size other than my theme's default size. Our size 4 was actually smaller than
		 * our size 3 for me. So this works around that oddity.
		 */
		tag = gtk_text_buffer_create_tag(imhtml->text_buffer, str, "size",
		                                 (gint) (imhtml->original_fsize *
		                                 ((double) _point_sizes[size-1] * imhtml->zoom)), NULL);
	}

	return tag;
}

static void remove_tag_by_prefix(GtkIMHtml *imhtml, const GtkTextIter *i, const GtkTextIter *e,
                                 const char *prefix, guint len, gboolean homo)
{
	GSList *tags, *l;
	GtkTextIter iter;

	tags = gtk_text_iter_get_tags(i);

	for (l = tags; l; l = l->next) {
		GtkTextTag *tag = l->data;

		if (tag->name && !strncmp(tag->name, prefix, len))
			gtk_text_buffer_remove_tag(imhtml->text_buffer, tag, i, e);
	}

	g_slist_free(tags);

	if (homo)
		return;

	iter = *i;

	while (gtk_text_iter_forward_char(&iter) && !gtk_text_iter_equal(&iter, e)) {
		if (gtk_text_iter_begins_tag(&iter, NULL)) {
			tags = gtk_text_iter_get_toggled_tags(&iter, TRUE);

			for (l = tags; l; l = l->next) {
				GtkTextTag *tag = l->data;

				if (tag->name && !strncmp(tag->name, prefix, len))
					gtk_text_buffer_remove_tag(imhtml->text_buffer, tag, &iter, e);
			}

			g_slist_free(tags);
		}
	}
}

static void remove_font_size(GtkIMHtml *imhtml, GtkTextIter *i, GtkTextIter *e, gboolean homo)
{
	remove_tag_by_prefix(imhtml, i, e, "FONT SIZE ", 10, homo);
}

static void remove_font_face(GtkIMHtml *imhtml, GtkTextIter *i, GtkTextIter *e, gboolean homo)
{
	remove_tag_by_prefix(imhtml, i, e, "FONT FACE ", 10, homo);
}

static void remove_font_forecolor(GtkIMHtml *imhtml, GtkTextIter *i, GtkTextIter *e, gboolean homo)
{
	remove_tag_by_prefix(imhtml, i, e, "FORECOLOR ", 10, homo);
}

static void remove_font_backcolor(GtkIMHtml *imhtml, GtkTextIter *i, GtkTextIter *e, gboolean homo)
{
	remove_tag_by_prefix(imhtml, i, e, "BACKCOLOR ", 10, homo);
}

static void remove_font_link(GtkIMHtml *imhtml, GtkTextIter *i, GtkTextIter *e, gboolean homo)
{
	remove_tag_by_prefix(imhtml, i, e, "LINK ", 5, homo);
}

/* Editable stuff */
static void preinsert_cb(GtkTextBuffer *buffer, GtkTextIter *iter, gchar *text, gint len, GtkIMHtml *imhtml)
{
	imhtml->insert_offset = gtk_text_iter_get_offset(iter);
}

static void insert_cb(GtkTextBuffer *buffer, GtkTextIter *end, gchar *text, gint len, GtkIMHtml *imhtml)
{
	GtkTextIter start;

	if (!len)
		return;

	start = *end;
	gtk_text_iter_set_offset(&start, imhtml->insert_offset);

	if (imhtml->edit.bold)
		gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "BOLD", &start, end);
	else
		gtk_text_buffer_remove_tag_by_name(imhtml->text_buffer, "BOLD", &start, end);

	if (imhtml->edit.italic)
		gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "ITALICS", &start, end);
	else
		gtk_text_buffer_remove_tag_by_name(imhtml->text_buffer, "ITALICS", &start, end);

	if (imhtml->edit.underline)
		gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "UNDERLINE", &start, end);
	else
		gtk_text_buffer_remove_tag_by_name(imhtml->text_buffer, "UNDERLINE", &start, end);

	if (imhtml->edit.forecolor) {
		remove_font_forecolor(imhtml, &start, end, TRUE);
		gtk_text_buffer_apply_tag(imhtml->text_buffer,
		                          find_font_forecolor_tag(imhtml, imhtml->edit.forecolor),
		                          &start, end);
	}

	if (imhtml->edit.backcolor) {
		remove_font_backcolor(imhtml, &start, end, TRUE);
		gtk_text_buffer_apply_tag(imhtml->text_buffer,
		                          find_font_backcolor_tag(imhtml, imhtml->edit.backcolor),
		                          &start, end);
	}

	if (imhtml->edit.fontface) {
		remove_font_face(imhtml, &start, end, TRUE);
		gtk_text_buffer_apply_tag(imhtml->text_buffer,
		                          find_font_face_tag(imhtml, imhtml->edit.fontface),
		                          &start, end);
	}

	if (imhtml->edit.fontsize) {
		remove_font_size(imhtml, &start, end, TRUE);
		gtk_text_buffer_apply_tag(imhtml->text_buffer,
		                          find_font_size_tag(imhtml, imhtml->edit.fontsize),
		                          &start, end);
	}

	if (imhtml->edit.link) {
		remove_font_link(imhtml, &start, end, TRUE);
		gtk_text_buffer_apply_tag(imhtml->text_buffer,
		                          imhtml->edit.link,
		                          &start, end);
	}

}

void gtk_imhtml_set_editable(GtkIMHtml *imhtml, gboolean editable)
{
	gtk_text_view_set_editable(GTK_TEXT_VIEW(imhtml), editable);
	/*
	 * We need a visible caret for accessibility, so mouseless
	 * people can highlight stuff.
	 */
	/* gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(imhtml), editable); */
	imhtml->editable = editable;
	imhtml->format_functions = GTK_IMHTML_ALL;

	if (editable)
		g_signal_connect_after(G_OBJECT(GTK_IMHTML(imhtml)->text_buffer), "mark-set",
		                       G_CALLBACK(mark_set_cb), imhtml);
}

void gtk_imhtml_set_whole_buffer_formatting_only(GtkIMHtml *imhtml, gboolean wbfo)
{
	g_return_if_fail(imhtml != NULL);

	imhtml->wbfo = wbfo;
}

void gtk_imhtml_set_format_functions(GtkIMHtml *imhtml, GtkIMHtmlButtons buttons)
{
	GObject *object = g_object_ref(G_OBJECT(imhtml));
	imhtml->format_functions = buttons;
	g_signal_emit(object, signals[BUTTONS_UPDATE], 0, buttons);
	g_object_unref(object);
}

GtkIMHtmlButtons gtk_imhtml_get_format_functions(GtkIMHtml *imhtml)
{
	return imhtml->format_functions;
}

void gtk_imhtml_get_current_format(GtkIMHtml *imhtml, gboolean *bold,
								   gboolean *italic, gboolean *underline)
{
	if (imhtml->edit.bold)
		(*bold) = TRUE;
	if (imhtml->edit.italic)
		(*italic) = TRUE;
	if (imhtml->edit.underline)
		(*underline) = TRUE;
}

char *
gtk_imhtml_get_current_fontface(GtkIMHtml *imhtml)
{
	if (imhtml->edit.fontface)
		return g_strdup(imhtml->edit.fontface);
	else
		return NULL;
}

char *
gtk_imhtml_get_current_forecolor(GtkIMHtml *imhtml)
{
	if (imhtml->edit.forecolor)
		return g_strdup(imhtml->edit.forecolor);
	else
		return NULL;
}

char *
gtk_imhtml_get_current_backcolor(GtkIMHtml *imhtml)
{
	if (imhtml->edit.backcolor)
		return g_strdup(imhtml->edit.backcolor);
	else
		return NULL;
}

gint
gtk_imhtml_get_current_fontsize(GtkIMHtml *imhtml)
{
	return imhtml->edit.fontsize;
}

gboolean gtk_imhtml_get_editable(GtkIMHtml *imhtml)
{
	return imhtml->editable;
}

/*
 * I had this crazy idea about changing the text cursor color to reflex the foreground color
 * of the text about to be entered. This is the place you'd do it, along with the place where
 * we actually set a new foreground color.
 * I may not do this, because people will bitch about Gaim overriding their gtk theme's cursor
 * colors.
 *
 * Just in case I do do this, I asked about what to set the secondary text cursor to.
 *
 * (12:45:27) ?? ???: secondary_cursor_color = (rgb(background) + rgb(primary_cursor_color) ) / 2
 * (12:45:55) ?? ???: understand?
 * (12:46:14) Tim: yeah. i didn't know there was an exact formula
 * (12:46:56) ?? ???: u might need to extract separate each color from RGB
 */

static void mark_set_cb(GtkTextBuffer *buffer, GtkTextIter *arg1, GtkTextMark *mark,
                           GtkIMHtml *imhtml)
{
	GSList *tags, *l;
	GtkTextIter iter;

	if (mark != gtk_text_buffer_get_insert(buffer))
		return;

	if (!gtk_text_buffer_get_char_count(buffer))
		return;

	imhtml->edit.bold = imhtml->edit.italic = imhtml->edit.underline = FALSE;
	if (imhtml->edit.forecolor)
		g_free(imhtml->edit.forecolor);
	imhtml->edit.forecolor = NULL;
	if (imhtml->edit.backcolor)
		g_free(imhtml->edit.backcolor);
	imhtml->edit.backcolor = NULL;
	if (imhtml->edit.fontface)
		g_free(imhtml->edit.fontface);
	imhtml->edit.fontface = NULL;
	imhtml->edit.fontsize = 0;
	imhtml->edit.link = NULL;

	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &iter, mark);


	if (gtk_text_iter_is_end(&iter))
		tags = gtk_text_iter_get_toggled_tags(&iter, FALSE);
	else
		tags = gtk_text_iter_get_tags(&iter);

	for (l = tags; l != NULL; l = l->next) {
		GtkTextTag *tag = GTK_TEXT_TAG(l->data);

		if (tag->name) {
			if (strcmp(tag->name, "BOLD") == 0)
				imhtml->edit.bold = TRUE;
			if (strcmp(tag->name, "ITALICS") == 0)
				imhtml->edit.italic = TRUE;
			if (strcmp(tag->name, "UNDERLINE") == 0)
				imhtml->edit.underline = TRUE;
			if (strncmp(tag->name, "FORECOLOR ", 10) == 0)
				imhtml->edit.forecolor = g_strdup(&(tag->name)[10]);
			if (strncmp(tag->name, "BACKCOLOR ", 10) == 0)
				imhtml->edit.backcolor = g_strdup(&(tag->name)[10]);
			if (strncmp(tag->name, "FONT FACE ", 10) == 0)
				imhtml->edit.fontface = g_strdup(&(tag->name)[10]);
			if (strncmp(tag->name, "FONT SIZE ", 10) == 0)
				imhtml->edit.fontsize = strtol(&(tag->name)[10], NULL, 10);
			if ((strncmp(tag->name, "LINK ", 5) == 0) && !gtk_text_iter_is_end(&iter))
				imhtml->edit.link = tag;
		}
	}

	g_slist_free(tags);
}

gboolean gtk_imhtml_toggle_bold(GtkIMHtml *imhtml)
{
	GObject *object;
	GtkTextIter start, end;

	imhtml->edit.bold = !imhtml->edit.bold;

	if (imhtml->wbfo) {
		gtk_text_buffer_get_bounds(imhtml->text_buffer, &start, &end);
		if (imhtml->edit.bold)
			gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "BOLD", &start, &end);
		else
			gtk_text_buffer_remove_tag_by_name(imhtml->text_buffer, "BOLD", &start, &end);
	} else if (imhtml->editable && gtk_text_buffer_get_selection_bounds(imhtml->text_buffer, &start, &end)) {
		if (imhtml->edit.bold)
			gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "BOLD", &start, &end);
		else
			gtk_text_buffer_remove_tag_by_name(imhtml->text_buffer, "BOLD", &start, &end);

	}
	object = g_object_ref(G_OBJECT(imhtml));
	g_signal_emit(object, signals[TOGGLE_FORMAT], 0, GTK_IMHTML_BOLD);
	g_object_unref(object);

	return (imhtml->edit.bold != FALSE);
}

gboolean gtk_imhtml_toggle_italic(GtkIMHtml *imhtml)
{
	GObject *object;
	GtkTextIter start, end;

	imhtml->edit.italic = !imhtml->edit.italic;

	if (imhtml->wbfo) {
		gtk_text_buffer_get_bounds(imhtml->text_buffer, &start, &end);
		if (imhtml->edit.italic)
			gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "ITALICS", &start, &end);
		else
			gtk_text_buffer_remove_tag_by_name(imhtml->text_buffer, "ITALICS", &start, &end);
	} else if (imhtml->editable && gtk_text_buffer_get_selection_bounds(imhtml->text_buffer, &start, &end)) {
		if (imhtml->edit.italic)
			gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "ITALICS", &start, &end);
		else
			gtk_text_buffer_remove_tag_by_name(imhtml->text_buffer, "ITALICS", &start, &end);
	}
	object = g_object_ref(G_OBJECT(imhtml));
	g_signal_emit(object, signals[TOGGLE_FORMAT], 0, GTK_IMHTML_ITALIC);
	g_object_unref(object);

	return imhtml->edit.italic != FALSE;
}

gboolean gtk_imhtml_toggle_underline(GtkIMHtml *imhtml)
{
	GObject *object;
	GtkTextIter start, end;

	imhtml->edit.underline = !imhtml->edit.underline;

	if (imhtml->wbfo) {
		gtk_text_buffer_get_bounds(imhtml->text_buffer, &start, &end);
		if (imhtml->edit.underline)
			gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "UNDERLINE", &start, &end);
		else
			gtk_text_buffer_remove_tag_by_name(imhtml->text_buffer, "UNDERLINE", &start, &end);
	} else if (imhtml->editable && gtk_text_buffer_get_selection_bounds(imhtml->text_buffer, &start, &end)) {
		if (imhtml->edit.underline)
			gtk_text_buffer_apply_tag_by_name(imhtml->text_buffer, "UNDERLINE", &start, &end);
		else
			gtk_text_buffer_remove_tag_by_name(imhtml->text_buffer, "UNDERLINE", &start, &end);
	}
	object = g_object_ref(G_OBJECT(imhtml));
	g_signal_emit(object, signals[TOGGLE_FORMAT], 0, GTK_IMHTML_UNDERLINE);
	g_object_unref(object);

	return imhtml->edit.underline != FALSE;
}

void gtk_imhtml_font_set_size(GtkIMHtml *imhtml, gint size)
{
	GObject *object;
	GtkTextIter start, end;
	GtkIMHtmlButtons b = 0;

	imhtml->edit.fontsize = size;


	if (imhtml->wbfo) {
		gtk_text_buffer_get_bounds(imhtml->text_buffer, &start, &end);
		remove_font_size(imhtml, &start, &end, TRUE);
		gtk_text_buffer_apply_tag(imhtml->text_buffer,
		                                  find_font_size_tag(imhtml, imhtml->edit.fontsize), &start, &end);
	} else if (imhtml->editable && gtk_text_buffer_get_selection_bounds(imhtml->text_buffer, &start, &end)) {
		remove_font_size(imhtml, &start, &end, FALSE);
		gtk_text_buffer_apply_tag(imhtml->text_buffer,
		                                  find_font_size_tag(imhtml, imhtml->edit.fontsize), &start, &end);
	}

	object = g_object_ref(G_OBJECT(imhtml));
	b |= GTK_IMHTML_SHRINK;
	b |= GTK_IMHTML_GROW;
	g_signal_emit(object, signals[TOGGLE_FORMAT], 0, b);
	g_object_unref(object);
}

void gtk_imhtml_font_shrink(GtkIMHtml *imhtml)
{
	GObject *object;
	GtkTextIter start, end;

	if (imhtml->edit.fontsize == 1)
		return;

	if (!imhtml->edit.fontsize)
		imhtml->edit.fontsize = 2;
	else
		imhtml->edit.fontsize--;

	if (imhtml->wbfo) {
		gtk_text_buffer_get_bounds(imhtml->text_buffer, &start, &end);
		remove_font_size(imhtml, &start, &end, TRUE);
		gtk_text_buffer_apply_tag(imhtml->text_buffer,
		                                  find_font_size_tag(imhtml, imhtml->edit.fontsize), &start, &end);
	} else if (imhtml->editable && gtk_text_buffer_get_selection_bounds(imhtml->text_buffer, &start, &end)) {
		remove_font_size(imhtml, &start, &end, FALSE);
		gtk_text_buffer_apply_tag(imhtml->text_buffer,
		                                  find_font_size_tag(imhtml, imhtml->edit.fontsize), &start, &end);
	}
	object = g_object_ref(G_OBJECT(imhtml));
	g_signal_emit(object, signals[TOGGLE_FORMAT], 0, GTK_IMHTML_SHRINK);
	g_object_unref(object);
}

void gtk_imhtml_font_grow(GtkIMHtml *imhtml)
{
	GObject *object;
	GtkTextIter start, end;

	if (imhtml->edit.fontsize == MAX_FONT_SIZE)
		return;

	if (!imhtml->edit.fontsize)
		imhtml->edit.fontsize = 4;
	else
		imhtml->edit.fontsize++;

	if (imhtml->wbfo) {
		gtk_text_buffer_get_bounds(imhtml->text_buffer, &start, &end);
		remove_font_size(imhtml, &start, &end, TRUE);
		gtk_text_buffer_apply_tag(imhtml->text_buffer,
		                                  find_font_size_tag(imhtml, imhtml->edit.fontsize), &start, &end);
	} else if (imhtml->editable && gtk_text_buffer_get_selection_bounds(imhtml->text_buffer, &start, &end)) {
		remove_font_size(imhtml, &start, &end, FALSE);
		gtk_text_buffer_apply_tag(imhtml->text_buffer,
		                                  find_font_size_tag(imhtml, imhtml->edit.fontsize), &start, &end);
	}
	object = g_object_ref(G_OBJECT(imhtml));
	g_signal_emit(object, signals[TOGGLE_FORMAT], 0, GTK_IMHTML_GROW);
	g_object_unref(object);
}

gboolean gtk_imhtml_toggle_forecolor(GtkIMHtml *imhtml, const char *color)
{
	GObject *object;
	GtkTextIter start, end;

	if (imhtml->edit.forecolor != NULL)
		g_free(imhtml->edit.forecolor);

	if (color && strcmp(color, "") != 0) {
		imhtml->edit.forecolor = g_strdup(color);
		if (imhtml->wbfo) {
			gtk_text_buffer_get_bounds(imhtml->text_buffer, &start, &end);
			remove_font_forecolor(imhtml, &start, &end, TRUE);
			gtk_text_buffer_apply_tag(imhtml->text_buffer,
		                                  find_font_forecolor_tag(imhtml, imhtml->edit.forecolor), &start, &end);
		} else if (imhtml->editable && gtk_text_buffer_get_selection_bounds(imhtml->text_buffer, &start, &end)) {
			remove_font_forecolor(imhtml, &start, &end, FALSE);
			gtk_text_buffer_apply_tag(imhtml->text_buffer,
		                                          find_font_forecolor_tag(imhtml, imhtml->edit.forecolor),
			                                  &start, &end);
		}
	} else {
		imhtml->edit.forecolor = NULL;
		if (imhtml->wbfo) {
			gtk_text_buffer_get_bounds(imhtml->text_buffer, &start, &end);
			remove_font_forecolor(imhtml, &start, &end, TRUE);
		}
	}

	object = g_object_ref(G_OBJECT(imhtml));
	g_signal_emit(object, signals[TOGGLE_FORMAT], 0, GTK_IMHTML_FORECOLOR);
	g_object_unref(object);

	return imhtml->edit.forecolor != NULL;
}

gboolean gtk_imhtml_toggle_backcolor(GtkIMHtml *imhtml, const char *color)
{
	GObject *object;
	GtkTextIter start, end;

	if (imhtml->edit.backcolor != NULL)
		g_free(imhtml->edit.backcolor);

	if (color && strcmp(color, "") != 0) {
		imhtml->edit.backcolor = g_strdup(color);

		if (imhtml->wbfo) {
			gtk_text_buffer_get_bounds(imhtml->text_buffer, &start, &end);
			remove_font_backcolor(imhtml, &start, &end, TRUE);
			gtk_text_buffer_apply_tag(imhtml->text_buffer,
		                                  find_font_backcolor_tag(imhtml, imhtml->edit.backcolor), &start, &end);
		} else if (imhtml->editable && gtk_text_buffer_get_selection_bounds(imhtml->text_buffer, &start, &end)) {
			remove_font_backcolor(imhtml, &start, &end, FALSE);
			gtk_text_buffer_apply_tag(imhtml->text_buffer,
			                                  find_font_backcolor_tag(imhtml,
			                                  imhtml->edit.backcolor), &start, &end);
		}
	} else {
		imhtml->edit.backcolor = NULL;
		if (imhtml->wbfo) {
			gtk_text_buffer_get_bounds(imhtml->text_buffer, &start, &end);
			remove_font_backcolor(imhtml, &start, &end, TRUE);
		}
	}

	object = g_object_ref(G_OBJECT(imhtml));
	g_signal_emit(object, signals[TOGGLE_FORMAT], 0, GTK_IMHTML_BACKCOLOR);
	g_object_unref(object);

	return imhtml->edit.backcolor != NULL;
}

gboolean gtk_imhtml_toggle_fontface(GtkIMHtml *imhtml, const char *face)
{
	GObject *object;
	GtkTextIter start, end;

	if (imhtml->edit.fontface != NULL)
		g_free(imhtml->edit.fontface);

	if (face && strcmp(face, "") != 0) {
		imhtml->edit.fontface = g_strdup(face);

		if (imhtml->wbfo) {
			gtk_text_buffer_get_bounds(imhtml->text_buffer, &start, &end);
			remove_font_face(imhtml, &start, &end, TRUE);
			gtk_text_buffer_apply_tag(imhtml->text_buffer,
		                                  find_font_face_tag(imhtml, imhtml->edit.fontface), &start, &end);
		} else if (imhtml->editable && gtk_text_buffer_get_selection_bounds(imhtml->text_buffer, &start, &end)) {
			remove_font_face(imhtml, &start, &end, FALSE);
			gtk_text_buffer_apply_tag(imhtml->text_buffer,
			                                  find_font_face_tag(imhtml, imhtml->edit.fontface),
			                                  &start, &end);
		}
	} else {
		imhtml->edit.fontface = NULL;
		if (imhtml->wbfo) {
			gtk_text_buffer_get_bounds(imhtml->text_buffer, &start, &end);
			remove_font_face(imhtml, &start, &end, TRUE);
		}
	}

	object = g_object_ref(G_OBJECT(imhtml));
	g_signal_emit(object, signals[TOGGLE_FORMAT], 0, GTK_IMHTML_FACE);
	g_object_unref(object);

	return imhtml->edit.fontface != NULL;
}

void gtk_imhtml_toggle_link(GtkIMHtml *imhtml, const char *url)
{
	GObject *object;
	GtkTextIter start, end;
	GtkTextTag *linktag;
	static guint linkno = 0;
	gchar str[48];
	GdkColor *color = NULL;

	imhtml->edit.link = NULL;



	if (url) {
		g_snprintf(str, sizeof(str), "LINK %d", linkno++);
		str[47] = '\0';

		gtk_widget_style_get(GTK_WIDGET(imhtml), "hyperlink-color", &color, NULL);
		if (color) {
			imhtml->edit.link = linktag = gtk_text_buffer_create_tag(imhtml->text_buffer, str, "foreground-gdk", color, "underline", PANGO_UNDERLINE_SINGLE, NULL);
			gdk_color_free(color);
		} else {
			imhtml->edit.link = linktag = gtk_text_buffer_create_tag(imhtml->text_buffer, str, "foreground", "blue", "underline", PANGO_UNDERLINE_SINGLE, NULL);
		}
		g_object_set_data_full(G_OBJECT(linktag), "link_url", g_strdup(url), g_free);
		g_signal_connect(G_OBJECT(linktag), "event", G_CALLBACK(tag_event), NULL);

		if (imhtml->editable && gtk_text_buffer_get_selection_bounds(imhtml->text_buffer, &start, &end)) {
			remove_font_link(imhtml, &start, &end, FALSE);
			gtk_text_buffer_apply_tag(imhtml->text_buffer, linktag, &start, &end);
		}
	}

	object = g_object_ref(G_OBJECT(imhtml));
	g_signal_emit(object, signals[TOGGLE_FORMAT], 0, GTK_IMHTML_LINK);
	g_object_unref(object);
}

void gtk_imhtml_insert_link(GtkIMHtml *imhtml, GtkTextMark *mark, const char *url, const char *text)
{
	GtkTextIter iter;

	gtk_imhtml_toggle_link(imhtml, url);
	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &iter, mark);
	gtk_text_buffer_insert(imhtml->text_buffer, &iter, text, -1);
	gtk_imhtml_toggle_link(imhtml, NULL);
}

void gtk_imhtml_insert_smiley(GtkIMHtml *imhtml, const char *sml, char *smiley)
{
	GtkTextMark *mark;
	GtkTextIter iter;

	mark = gtk_text_buffer_get_insert(imhtml->text_buffer);

	gtk_text_buffer_get_iter_at_mark(imhtml->text_buffer, &iter, mark);
	gtk_imhtml_insert_smiley_at_iter(imhtml, sml, smiley, &iter);
}

void gtk_imhtml_insert_smiley_at_iter(GtkIMHtml *imhtml, const char *sml, char *smiley, GtkTextIter *iter)
{
	GdkPixbuf *pixbuf = NULL;
	GdkPixbufAnimation *annipixbuf = NULL;
	GtkWidget *icon = NULL;
	GtkTextChildAnchor *anchor;
	char *unescaped = gaim_unescape_html(smiley);

	annipixbuf = gtk_smiley_tree_image(imhtml, sml, unescaped);
	if(annipixbuf) {
		if(gdk_pixbuf_animation_is_static_image(annipixbuf)) {
			pixbuf = gdk_pixbuf_animation_get_static_image(annipixbuf);
			if(pixbuf)
				icon = gtk_image_new_from_pixbuf(pixbuf);
		} else {
			icon = gtk_image_new_from_animation(annipixbuf);
		}
	}

	if (icon) {
		anchor = gtk_text_buffer_create_child_anchor(imhtml->text_buffer, iter);
		g_object_set_data_full(G_OBJECT(anchor), "gtkimhtml_plaintext", g_strdup(unescaped), g_free);
		g_object_set_data_full(G_OBJECT(anchor), "gtkimhtml_htmltext", g_strdup(smiley), g_free);

		gtk_widget_show(icon);
		gtk_text_view_add_child_at_anchor(GTK_TEXT_VIEW(imhtml), icon, anchor);
	} else {
		gtk_text_buffer_insert(imhtml->text_buffer, iter, smiley, -1);
	}

	g_free(unescaped);
}

void gtk_imhtml_insert_image_at_iter(GtkIMHtml *imhtml, int id, GtkTextIter *iter)
{
	GdkPixbuf *pixbuf = NULL;
	const char *filename = NULL;
	gpointer image;
	GdkRectangle rect;
	GtkIMHtmlScalable *scalable = NULL;
	int minus;

	if (!imhtml->funcs || !imhtml->funcs->image_get ||
	    !imhtml->funcs->image_get_size || !imhtml->funcs->image_get_data ||
	    !imhtml->funcs->image_get_filename || !imhtml->funcs->image_ref ||
	    !imhtml->funcs->image_unref)
		return;

	image = imhtml->funcs->image_get(id);

	if (image) {
		gpointer data;
		size_t len;

		data = imhtml->funcs->image_get_data(image);
		len = imhtml->funcs->image_get_size(image);

		if (data && len) {
			GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
			gdk_pixbuf_loader_write(loader, data, len, NULL);
			pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
			if (pixbuf)
				g_object_ref(G_OBJECT(pixbuf));
			gdk_pixbuf_loader_close(loader, NULL);
		}

	}

	if (pixbuf) {
		filename = imhtml->funcs->image_get_filename(image);
		imhtml->funcs->image_ref(id);
		imhtml->im_images = g_slist_prepend(imhtml->im_images, GINT_TO_POINTER(id));
	} else {
		pixbuf = gtk_widget_render_icon(GTK_WIDGET(imhtml), GTK_STOCK_MISSING_IMAGE,
						GTK_ICON_SIZE_BUTTON, "gtkimhtml-missing-image");
	}

	scalable = gtk_imhtml_image_new(pixbuf, filename, id);
	gtk_text_view_get_visible_rect(GTK_TEXT_VIEW(imhtml), &rect);
	scalable->add_to(scalable, imhtml, iter);
	minus = gtk_text_view_get_left_margin(GTK_TEXT_VIEW(imhtml)) +
		gtk_text_view_get_right_margin(GTK_TEXT_VIEW(imhtml));
	scalable->scale(scalable, rect.width - minus, rect.height);
	imhtml->scalables = g_list_append(imhtml->scalables, scalable);

	g_object_unref(G_OBJECT(pixbuf));
}

static const gchar *tag_to_html_start(GtkTextTag *tag)
{
	const gchar *name;
	static gchar buf[1024];

	name = tag->name;
	g_return_val_if_fail(name != NULL, "");

	if (strcmp(name, "BOLD") == 0) {
		return "<b>";
	} else if (strcmp(name, "ITALICS") == 0) {
		return "<i>";
	} else if (strcmp(name, "UNDERLINE") == 0) {
		return "<u>";
	} else if (strncmp(name, "LINK ", 5) == 0) {
		char *tmp = g_object_get_data(G_OBJECT(tag), "link_url");
		if (tmp) {
			g_snprintf(buf, sizeof(buf), "<a href=\"%s\">", tmp);
			buf[sizeof(buf)-1] = '\0';
			return buf;
		} else {
			return "";
		}
	} else if (strncmp(name, "FORECOLOR ", 10) == 0) {
		g_snprintf(buf, sizeof(buf), "<font color=\"%s\">", &name[10]);
		return buf;
	} else if (strncmp(name, "BACKCOLOR ", 10) == 0) {
		g_snprintf(buf, sizeof(buf), "<font back=\"%s\">", &name[10]);
		return buf;
	} else if (strncmp(name, "FONT FACE ", 10) == 0) {
		g_snprintf(buf, sizeof(buf), "<font face=\"%s\">", &name[10]);
		return buf;
	} else if (strncmp(name, "FONT SIZE ", 10) == 0) {
		g_snprintf(buf, sizeof(buf), "<font size=\"%s\">", &name[10]);
		return buf;
	} else {
		return "";
	}
}

static const gchar *tag_to_html_end(GtkTextTag *tag)
{
	const gchar *name;

	name = tag->name;
	g_return_val_if_fail(name != NULL, "");

	if (strcmp(name, "BOLD") == 0) {
		return "</b>";
	} else if (strcmp(name, "ITALICS") == 0) {
		return "</i>";
	} else if (strcmp(name, "UNDERLINE") == 0) {
		return "</u>";
	} else if (strncmp(name, "LINK ", 5) == 0) {
		return "</a>";
	} else if (strncmp(name, "FORECOLOR ", 10) == 0) {
		return "</font>";
	} else if (strncmp(name, "BACKCOLOR ", 10) == 0) {
		return "</font>";
	} else if (strncmp(name, "FONT FACE ", 10) == 0) {
		return "</font>";
	} else if (strncmp(name, "FONT SIZE ", 10) == 0) {
		return "</font>";
	} else {
		return "";
	}
}

static gboolean tag_ends_here(GtkTextTag *tag, GtkTextIter *iter, GtkTextIter *niter)
{
	return ((gtk_text_iter_has_tag(iter, GTK_TEXT_TAG(tag)) &&
	         !gtk_text_iter_has_tag(niter, GTK_TEXT_TAG(tag))) ||
	        gtk_text_iter_is_end(niter));
}

/* Basic notion here: traverse through the text buffer one-by-one, non-character elements, such
 * as smileys and IM images are represented by the Unicode "unknown" character.  Handle them.  Else
 * check for tags that are toggled on, insert their html form, and  push them on the queue. Then insert
 * the actual text. Then check for tags that are toggled off and insert them, after checking the queue.
 * Finally, replace <, >, &, and " with their HTML equivalent.
 */
char *gtk_imhtml_get_markup_range(GtkIMHtml *imhtml, GtkTextIter *start, GtkTextIter *end)
{
	gunichar c;
	GtkTextIter iter, nextiter;
	GString *str = g_string_new("");
	GSList *tags, *sl;
	GQueue *q, *r;
	GtkTextTag *tag;

	q = g_queue_new();
	r = g_queue_new();


	gtk_text_iter_order(start, end);
	nextiter = iter = *start;
	gtk_text_iter_forward_char(&nextiter);

	/* First add the tags that are already in progress */
	tags = gtk_text_iter_get_tags(start);

	for (sl = tags; sl; sl = sl->next) {
		tag = sl->data;
		if (!gtk_text_iter_toggles_tag(start, GTK_TEXT_TAG(tag))) {
		 	g_string_append(str, tag_to_html_start(GTK_TEXT_TAG(tag)));
			g_queue_push_tail(q, tag);
		}
	}
	g_slist_free(tags);

	while ((c = gtk_text_iter_get_char(&iter)) != 0 && !gtk_text_iter_equal(&iter, end)) {

		tags = gtk_text_iter_get_tags(&iter);

		for (sl = tags; sl; sl = sl->next) {
			tag = sl->data;
			if (gtk_text_iter_begins_tag(&iter, GTK_TEXT_TAG(tag))) {
		 		g_string_append(str, tag_to_html_start(GTK_TEXT_TAG(tag)));
				g_queue_push_tail(q, tag);
			}
		}


		if (c == 0xFFFC) {
			GtkTextChildAnchor* anchor = gtk_text_iter_get_child_anchor(&iter);
			char *text = NULL;
			if (anchor)
				text = g_object_get_data(G_OBJECT(anchor), "gtkimhtml_htmltext");
			if (text)
				str = g_string_append(str, text);
		} else 	if (c == '<') {
			str = g_string_append(str, "&lt;");
		} else if (c == '>') {
			str = g_string_append(str, "&gt;");
		} else if (c == '&') {
			str = g_string_append(str, "&amp;");
		} else if (c == '"') {
			str = g_string_append(str, "&quot;");
		} else if (c == '\n') {
			str = g_string_append(str, "<br>");
		} else {
			str = g_string_append_unichar(str, c);
		}

		tags = g_slist_reverse(tags);
		for (sl = tags; sl; sl = sl->next) {
			tag = sl->data;
			if (tag_ends_here(tag, &iter, &nextiter)) {

				GtkTextTag *tmp;

				while ((tmp = g_queue_pop_tail(q)) != tag) {
					if (tmp == NULL)
						break;

					if (!tag_ends_here(tmp, &iter, &nextiter))
						g_queue_push_tail(r, tmp);
		 			g_string_append(str, tag_to_html_end(GTK_TEXT_TAG(tmp)));
				}

				if (tmp == NULL)
					gaim_debug_warning("gtkimhtml", "empty queue, more closing tags than open tags!\n");
		 		else
					g_string_append(str, tag_to_html_end(GTK_TEXT_TAG(tag)));

				while ((tmp = g_queue_pop_head(r))) {
					g_string_append(str, tag_to_html_start(GTK_TEXT_TAG(tmp)));
					g_queue_push_tail(q, tmp);
				}
			}
		}

		g_slist_free(tags);
		gtk_text_iter_forward_char(&iter);
		gtk_text_iter_forward_char(&nextiter);
	}

	while ((tag = g_queue_pop_tail(q)))
		g_string_append(str, tag_to_html_end(GTK_TEXT_TAG(tag)));

	g_queue_free(q);
	g_queue_free(r);
	return g_string_free(str, FALSE);
}

void gtk_imhtml_close_tags(GtkIMHtml *imhtml, GtkTextIter *iter)
{
	if (imhtml->edit.bold)
		gtk_imhtml_toggle_bold(imhtml);

	if (imhtml->edit.italic)
		gtk_imhtml_toggle_italic(imhtml);

	if (imhtml->edit.underline)
		gtk_imhtml_toggle_underline(imhtml);

	if (imhtml->edit.forecolor)
		gtk_imhtml_toggle_forecolor(imhtml, NULL);

	if (imhtml->edit.backcolor)
		gtk_imhtml_toggle_backcolor(imhtml, NULL);

	if (imhtml->edit.fontface)
		gtk_imhtml_toggle_fontface(imhtml, NULL);

	imhtml->edit.fontsize = 0;

	if (imhtml->edit.link)
		gtk_imhtml_toggle_link(imhtml, NULL);

	gtk_text_buffer_remove_all_tags(imhtml->text_buffer, iter, iter);

}

char *gtk_imhtml_get_markup(GtkIMHtml *imhtml)
{
	GtkTextIter start, end;

	gtk_text_buffer_get_start_iter(imhtml->text_buffer, &start);
	gtk_text_buffer_get_end_iter(imhtml->text_buffer, &end);
	return gtk_imhtml_get_markup_range(imhtml, &start, &end);
}

char **gtk_imhtml_get_markup_lines(GtkIMHtml *imhtml)
{
	int i, j, lines;
	GtkTextIter start, end;
	char **ret;

	lines = gtk_text_buffer_get_line_count(imhtml->text_buffer);
	ret = g_new0(char *, lines + 1);
	gtk_text_buffer_get_start_iter(imhtml->text_buffer, &start);
	end = start;
	gtk_text_iter_forward_to_line_end(&end);

	for (i = 0, j = 0; i < lines; i++) {
		ret[j] = gtk_imhtml_get_markup_range(imhtml, &start, &end);
		if (ret[j] != NULL)
			j++;
		gtk_text_iter_forward_line(&start);
		end = start;
		gtk_text_iter_forward_to_line_end(&end);
	}

	return ret;
}

char *gtk_imhtml_get_text(GtkIMHtml *imhtml, GtkTextIter *start, GtkTextIter *stop)
{
	GString *str = g_string_new("");
	GtkTextIter iter, end;
	gunichar c;

	if (start == NULL)
		gtk_text_buffer_get_start_iter(imhtml->text_buffer, &iter);
	else
		iter = *start;

	if (stop == NULL)
		gtk_text_buffer_get_end_iter(imhtml->text_buffer, &end);
	else
		end = *stop;

	gtk_text_iter_order(&iter, &end);

	while ((c = gtk_text_iter_get_char(&iter)) != 0 && !gtk_text_iter_equal(&iter, &end)) {
		if (c == 0xFFFC) {
			GtkTextChildAnchor* anchor;
			char *text = NULL;

			anchor = gtk_text_iter_get_child_anchor(&iter);
			if (anchor)
				text = g_object_get_data(G_OBJECT(anchor), "gtkimhtml_plaintext");
			if (text)
				str = g_string_append(str, text);
		} else {
			g_string_append_unichar(str, c);
		}
		gtk_text_iter_forward_char(&iter);
	}

	return g_string_free(str, FALSE);
}

void gtk_imhtml_set_funcs(GtkIMHtml *imhtml, GtkIMHtmlFuncs *f)
{
	g_return_if_fail(imhtml != NULL);
	imhtml->funcs = f;
}
