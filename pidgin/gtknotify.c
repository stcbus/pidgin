/* pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include <glib/gi18n-lib.h>

#include <gdk/gdkkeysyms.h>
#include <talkatu.h>

#include <purple.h>

#include "gtkblist.h"
#include "gtknotify.h"
#include "gtkutils.h"
#include "pidgincore.h"

typedef struct
{
	GtkWidget *window;
	int count;
} PidginUserInfo;

typedef struct
{
	PurpleAccount *account;
	GtkListStore *model;
	GtkWidget *treeview;
	GtkWidget *window;
	gpointer user_data;
	PurpleNotifySearchResults *results;

} PidginNotifySearchResultsData;

typedef struct
{
	PurpleNotifySearchButton *button;
	PidginNotifySearchResultsData *data;

} PidginNotifySearchResultsButtonData;

static void pidgin_close_notify(PurpleNotifyType type, void *ui_handle);

static void
message_response_cb(GtkDialog *dialog, gint id, GtkWidget *widget)
{
	purple_notify_close(PURPLE_NOTIFY_MESSAGE, widget);
}

static gboolean
formatted_close_cb(GtkWidget *win, GdkEvent *event, void *user_data)
{
	purple_notify_close(PURPLE_NOTIFY_FORMATTED, win);
	return FALSE;
}

static gboolean
searchresults_close_cb(PidginNotifySearchResultsData *data, GdkEvent *event, gpointer user_data)
{
	purple_notify_close(PURPLE_NOTIFY_SEARCHRESULTS, data);
	return FALSE;
}

static void
searchresults_callback_wrapper_cb(GtkWidget *widget, PidginNotifySearchResultsButtonData *bd)
{
	PidginNotifySearchResultsData *data = bd->data;

	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	PurpleNotifySearchButton *button;
	GList *row = NULL;
	gchar *str;
	int i;

	g_return_if_fail(data != NULL);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->treeview));

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		for (i = 1; i < gtk_tree_model_get_n_columns(GTK_TREE_MODEL(model)); i++) {
			gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, i, &str, -1);
			row = g_list_append(row, str);
		}
	}

	button = bd->button;
	button->callback(purple_account_get_connection(data->account), row, data->user_data);
	g_list_free_full(row, g_free);
}

/* copy-paste from gtkrequest.c */
static void
pidgin_widget_decorate_account(GtkWidget *cont, PurpleAccount *account)
{
	GtkWidget *image;
	GdkPixbuf *pixbuf;

	if (!account)
		return;

	pixbuf = pidgin_create_protocol_icon(account, PIDGIN_PROTOCOL_ICON_SMALL);
	image = gtk_image_new_from_pixbuf(pixbuf);
	g_object_unref(G_OBJECT(pixbuf));

	gtk_widget_set_tooltip_text(image,
		purple_account_get_username(account));

	if (GTK_IS_BOX(cont)) {
		gtk_widget_set_halign(image, GTK_ALIGN_START);
		gtk_widget_set_valign(image, GTK_ALIGN_START);
		gtk_box_pack_end(GTK_BOX(cont), image, FALSE, TRUE, 0);
	}
	gtk_widget_show(image);
}

static void *
pidgin_notify_message(PurpleNotifyMessageType type, const char *title,
	const char *primary, const char *secondary,
	PurpleRequestCommonParameters *cpar)
{
	GtkWidget *dialog;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *img = NULL;
	char label_text[2048];
	const char *icon_name = NULL;
	char *primary_esc, *secondary_esc;

	switch (type)
	{
		case PURPLE_NOTIFY_MSG_ERROR:
			icon_name = "dialog-error";
			break;

		case PURPLE_NOTIFY_MSG_WARNING:
			icon_name = "dialog-warning";
			break;

		case PURPLE_NOTIFY_MSG_INFO:
			icon_name = "dialog-information";
			break;

		default:
			icon_name = NULL;
			break;
	}

	if (icon_name != NULL)
	{
		img = gtk_image_new_from_icon_name(icon_name,
				GTK_ICON_SIZE_DIALOG);
		gtk_widget_set_halign(img, GTK_ALIGN_START);
		gtk_widget_set_valign(img, GTK_ALIGN_START);
	}

	dialog = gtk_dialog_new_with_buttons(title ? title : PIDGIN_ALERT_TITLE,
										 NULL, 0, _("Close"),
										 GTK_RESPONSE_CLOSE, NULL);

	gtk_window_set_role(GTK_WINDOW(dialog), "notify_dialog");

	g_signal_connect(G_OBJECT(dialog), "response",
					 G_CALLBACK(message_response_cb), dialog);

	gtk_container_set_border_width(GTK_CONTAINER(dialog), 12);
	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_box_set_spacing(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
	                    12);
	gtk_container_set_border_width(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
	                               6);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
	                  hbox);

	if (img != NULL)
		gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);

	pidgin_widget_decorate_account(hbox,
		purple_request_cpar_get_account(cpar));

	primary_esc = g_markup_escape_text(primary, -1);
	secondary_esc = (secondary != NULL) ? g_markup_escape_text(secondary, -1) : NULL;
	g_snprintf(label_text, sizeof(label_text),
			   "<span weight=\"bold\" size=\"larger\">%s</span>%s%s",
			   primary_esc, (secondary ? "\n\n" : ""),
			   (secondary ? secondary_esc : ""));
	g_free(primary_esc);
	g_free(secondary_esc);

	label = gtk_label_new(NULL);

	gtk_label_set_markup(GTK_LABEL(label), label_text);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_xalign(GTK_LABEL(label), 0);
	gtk_label_set_yalign(GTK_LABEL(label), 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	g_object_set_data(G_OBJECT(dialog), "pidgin-parent-from",
		purple_request_cpar_get_parent_from(cpar));
	pidgin_auto_parent_window(dialog);

	gtk_widget_show_all(dialog);

	return dialog;
}

static gboolean
formatted_input_cb(GtkWidget *win, GdkEventKey *event, gpointer data)
{
	if (event->keyval == GDK_KEY_Escape)
	{
		purple_notify_close(PURPLE_NOTIFY_FORMATTED, win);

		return TRUE;
	}

	return FALSE;
}

static void *
pidgin_notify_formatted(const char *title, const char *primary,
						  const char *secondary, const char *text)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *button;
	GtkWidget *sw;
	GtkWidget *view;
	GtkTextBuffer *buffer;
	char label_text[2048];
	char *linked_text, *primary_esc, *secondary_esc;

	window = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(window), title);
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);

	g_signal_connect(G_OBJECT(window), "delete_event",
					 G_CALLBACK(formatted_close_cb), NULL);

	/* Setup the main vbox */
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(window));

	/* Setup the descriptive label */
	primary_esc = g_markup_escape_text(primary, -1);
	secondary_esc = (secondary != NULL) ? g_markup_escape_text(secondary, -1) : NULL;
	g_snprintf(label_text, sizeof(label_text),
			   "<span weight=\"bold\" size=\"larger\">%s</span>%s%s",
			   primary_esc,
			   (secondary ? "\n" : ""),
			   (secondary ? secondary_esc : ""));
	g_free(primary_esc);
	g_free(secondary_esc);

	label = gtk_label_new(NULL);

	gtk_label_set_markup(GTK_LABEL(label), label_text);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_xalign(GTK_LABEL(label), 0);
	gtk_label_set_yalign(GTK_LABEL(label), 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	/* Add the view */
	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX(vbox), sw, TRUE, TRUE, 0);

	buffer = talkatu_html_buffer_new();
	view = talkatu_view_new_with_buffer(buffer);
	gtk_container_add(GTK_CONTAINER(sw), view);
	gtk_widget_set_name(view, "pidgin_notify_view");
	gtk_widget_set_size_request(view, 300, 250);
	gtk_widget_show_all(sw);

	/* Add the Close button. */
	button = gtk_dialog_add_button(GTK_DIALOG(window), _("Close"), GTK_RESPONSE_CLOSE);
	gtk_widget_grab_focus(button);

	g_signal_connect_swapped(G_OBJECT(button), "clicked",
							 G_CALLBACK(formatted_close_cb), window);
	g_signal_connect(G_OBJECT(window), "key_press_event",
					 G_CALLBACK(formatted_input_cb), NULL);

	/* Make sure URLs are clickable */
	linked_text = purple_markup_linkify(text);
	talkatu_markup_set_html(TALKATU_BUFFER(buffer), linked_text, -1);
	g_free(linked_text);

	g_object_set_data(G_OBJECT(window), "view-widget", view);

	/* Show the window */
	pidgin_auto_parent_window(window);

	gtk_widget_show(window);

	return window;
}

static void
pidgin_notify_searchresults_new_rows(PurpleConnection *gc, PurpleNotifySearchResults *results,
									   void *data_)
{
	PidginNotifySearchResultsData *data = data_;
	GtkListStore *model = data->model;
	GtkTreeIter iter;
	GdkPixbuf *pixbuf;
	GList *row, *column;
	guint n;

	gtk_list_store_clear(data->model);

	pixbuf = pidgin_create_protocol_icon(purple_connection_get_account(gc), PIDGIN_PROTOCOL_ICON_SMALL);

	for (row = results->rows; row != NULL; row = row->next) {

		gtk_list_store_append(model, &iter);
		gtk_list_store_set(model, &iter, 0, pixbuf, -1);

		n = 1;
		for (column = row->data; column != NULL; column = column->next) {
			GValue v;

			v.g_type = 0;
			g_value_init(&v, G_TYPE_STRING);
			g_value_set_string(&v, column->data);
			gtk_list_store_set_value(model, &iter, n, &v);
			n++;
		}
	}

	if (pixbuf != NULL)
		g_object_unref(pixbuf);
}

static void *
pidgin_notify_searchresults(PurpleConnection *gc, const char *title,
							  const char *primary, const char *secondary,
							  PurpleNotifySearchResults *results, gpointer user_data)
{
	GtkWidget *window;
	GtkWidget *treeview;
	GtkWidget *close_button;
	GType *col_types;
	GtkListStore *model;
	GtkCellRenderer *renderer;
	guint col_num;
	GList *columniter;
	guint i;
	GList *l;

	GtkWidget *vbox;
	GtkWidget *label;
	PidginNotifySearchResultsData *data;
	char *label_text;
	char *primary_esc, *secondary_esc;

	g_return_val_if_fail(gc != NULL, NULL);
	g_return_val_if_fail(results != NULL, NULL);

	data = g_new0(PidginNotifySearchResultsData, 1);
	data->user_data = user_data;
	data->results = results;

	/* Create the window */
	window = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(window), title ? title :_("Search Results"));
	gtk_container_set_border_width(GTK_CONTAINER(window), 12);
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);

	g_signal_connect_swapped(G_OBJECT(window), "delete_event",
							 G_CALLBACK(searchresults_close_cb), data);

	/* Setup the main vbox */
	vbox = gtk_dialog_get_content_area(GTK_DIALOG(window));

	/* Setup the descriptive label */
	primary_esc = (primary != NULL) ? g_markup_escape_text(primary, -1) : NULL;
	secondary_esc = (secondary != NULL) ? g_markup_escape_text(secondary, -1) : NULL;
	label_text = g_strdup_printf(
			"<span weight=\"bold\" size=\"larger\">%s</span>%s%s",
			(primary ? primary_esc : ""),
			(primary && secondary ? "\n" : ""),
			(secondary ? secondary_esc : ""));
	g_free(primary_esc);
	g_free(secondary_esc);
	label = gtk_label_new(NULL);
	gtk_label_set_markup(GTK_LABEL(label), label_text);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_xalign(GTK_LABEL(label), 0);
	gtk_label_set_yalign(GTK_LABEL(label), 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);
	g_free(label_text);

	/* +1 is for the automagically created Status column. */
	col_num = g_list_length(results->columns) + 1;

	/* Setup the list model */
	col_types = g_new0(GType, col_num);

	/* There always is this first column. */
	col_types[0] = GDK_TYPE_PIXBUF;
	for (i = 1; i < col_num; i++) {
		col_types[i] = G_TYPE_STRING;
	}
	model = gtk_list_store_newv(col_num, col_types);
	g_free(col_types);

	/* Setup the treeview */
	treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
	g_object_unref(G_OBJECT(model));
	gtk_widget_set_size_request(treeview, 500, 400);
	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)),
								GTK_SELECTION_SINGLE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(treeview), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox),
		pidgin_make_scrollable(treeview, GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS, -1, -1),
		TRUE, TRUE, 0);
	gtk_widget_show(treeview);

	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview),
					-1, "", renderer, "pixbuf", 0, NULL);

	i = 1;
	for (columniter = results->columns; columniter != NULL; columniter = columniter->next) {
		PurpleNotifySearchColumn *column = columniter->data;
		renderer = gtk_cell_renderer_text_new();

		gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview), -1,
				purple_notify_searchresult_column_get_title(column), renderer, "text", i, NULL);

		if (!purple_notify_searchresult_column_is_visible(column))
			gtk_tree_view_column_set_visible(gtk_tree_view_get_column(GTK_TREE_VIEW(treeview), i), FALSE);

		i++;
	}

	for (l = results->buttons; l; l = l->next) {
		PurpleNotifySearchButton *b = l->data;
		GtkWidget *button = NULL;
		switch (b->type) {
			case PURPLE_NOTIFY_BUTTON_LABELED:
				if(b->label) {
					button = gtk_dialog_add_button(GTK_DIALOG(window), b->label, GTK_RESPONSE_NONE);
				} else {
					purple_debug_warning("gtknotify", "Missing button label\n");
				}
				break;
			case PURPLE_NOTIFY_BUTTON_CONTINUE:
				button = gtk_dialog_add_button(GTK_DIALOG(window), _("Forward"), GTK_RESPONSE_NONE);
				break;
			case PURPLE_NOTIFY_BUTTON_ADD:
				button = gtk_dialog_add_button(GTK_DIALOG(window), _("Add"), GTK_RESPONSE_NONE);
				break;
			case PURPLE_NOTIFY_BUTTON_INFO:
				button = gtk_dialog_add_button(GTK_DIALOG(window), _("_Get Info"), GTK_RESPONSE_NONE);
				break;
			case PURPLE_NOTIFY_BUTTON_IM:
				button = gtk_dialog_add_button(GTK_DIALOG(window), _("I_M"), GTK_RESPONSE_NONE);
				break;
			case PURPLE_NOTIFY_BUTTON_JOIN:
				button = gtk_dialog_add_button(GTK_DIALOG(window), _("_Join"), GTK_RESPONSE_NONE);
				break;
			case PURPLE_NOTIFY_BUTTON_INVITE:
				button = gtk_dialog_add_button(GTK_DIALOG(window), _("_Invite"), GTK_RESPONSE_NONE);
				break;
			default:
				purple_debug_warning("gtknotify", "Incorrect button type: %d\n", b->type);
		}
		if (button != NULL) {
			PidginNotifySearchResultsButtonData *bd;

			bd = g_new0(PidginNotifySearchResultsButtonData, 1);
			bd->button = b;
			bd->data = data;

			g_signal_connect(G_OBJECT(button), "clicked",
			                 G_CALLBACK(searchresults_callback_wrapper_cb), bd);
			g_signal_connect_swapped(G_OBJECT(button), "destroy", G_CALLBACK(g_free), bd);
		}
	}

	/* Add the Close button */
	close_button = gtk_dialog_add_button(GTK_DIALOG(window), _("Close"), GTK_RESPONSE_CLOSE);

	g_signal_connect_swapped(G_OBJECT(close_button), "clicked",
	                         G_CALLBACK(searchresults_close_cb), data);

	data->account = purple_connection_get_account(gc);
	data->model = model;
	data->treeview = treeview;
	data->window = window;

	/* Insert rows. */
	pidgin_notify_searchresults_new_rows(gc, results, data);

	/* Show the window */
	pidgin_auto_parent_window(window);

	gtk_widget_show(window);
	return data;
}

/* Xerox'ed from Finch! How the tables have turned!! ;) */
/* User information. */
static GHashTable *userinfo;

static char *
userinfo_hash(PurpleAccount *account, const char *who)
{
	char key[256];
	g_snprintf(key, sizeof(key), "%s - %s",
	           purple_account_get_username(account),
	           purple_normalize(account, who));
	return g_utf8_strup(key, -1);
}

static void
remove_userinfo(GtkWidget *widget, gpointer key)
{
	PidginUserInfo *pinfo = g_hash_table_lookup(userinfo, key);

	while (pinfo->count--)
		purple_notify_close(PURPLE_NOTIFY_USERINFO, widget);

	g_hash_table_remove(userinfo, key);
}

static void *
pidgin_notify_userinfo(PurpleConnection *gc, const char *who,
						 PurpleNotifyUserInfo *user_info)
{
	char *info;
	void *ui_handle;
	char *key = userinfo_hash(purple_connection_get_account(gc), who);
	PidginUserInfo *pinfo = NULL;

	if (!userinfo) {
		userinfo = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	}

	info = purple_notify_user_info_get_text_with_newline(user_info, "<br />");
	pinfo = g_hash_table_lookup(userinfo, key);
	if (pinfo != NULL) {
		GtkWidget *view = g_object_get_data(G_OBJECT(pinfo->window), "view-widget");
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(view));
		char *linked_text = purple_markup_linkify(info);
		talkatu_markup_set_html(TALKATU_BUFFER(buffer), linked_text, -1);
		g_free(linked_text);
		g_free(key);
		ui_handle = pinfo->window;
		pinfo->count++;
	} else {
		char *primary = g_strdup_printf(_("Info for %s"), who);
		ui_handle = pidgin_notify_formatted(_("Buddy Information"), primary, NULL, info);
		g_signal_handlers_disconnect_by_func(G_OBJECT(ui_handle), G_CALLBACK(formatted_close_cb), NULL);
		g_signal_connect(G_OBJECT(ui_handle), "destroy", G_CALLBACK(remove_userinfo), key);
		g_free(primary);
		pinfo = g_new0(PidginUserInfo, 1);
		pinfo->window = ui_handle;
		pinfo->count = 1;
		g_hash_table_insert(userinfo, key, pinfo);
	}
	g_free(info);
	return ui_handle;
}

static void
pidgin_close_notify(PurpleNotifyType type, void *ui_handle)
{
	if (type == PURPLE_NOTIFY_SEARCHRESULTS)
	{
		PidginNotifySearchResultsData *data = (PidginNotifySearchResultsData *)ui_handle;

		gtk_widget_destroy(data->window);
		purple_notify_searchresults_free(data->results);

		g_free(data);
	}
	else if (ui_handle != NULL)
		gtk_widget_destroy(GTK_WIDGET(ui_handle));
}

static void *
pidgin_notify_uri(const char *uri) {
	gtk_show_uri_on_window(NULL, uri, GDK_CURRENT_TIME, NULL);

	return NULL;
}

static void*
pidgin_notify_get_handle(void)
{
	static int handle;
	return &handle;
}

void pidgin_notify_init(void)
{
}

void pidgin_notify_uninit(void)
{
	purple_signals_disconnect_by_handle(pidgin_notify_get_handle());
}

static PurpleNotifyUiOps ops =
{
	pidgin_notify_message,
	pidgin_notify_formatted,
	pidgin_notify_searchresults,
	pidgin_notify_searchresults_new_rows,
	pidgin_notify_userinfo,
	pidgin_notify_uri,
	pidgin_close_notify,
	NULL,
	NULL,
	NULL,
	NULL
};

PurpleNotifyUiOps *
pidgin_notify_get_ui_ops(void)
{
	return &ops;
}
