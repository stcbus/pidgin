/*
 * Pidgin - Internet Messenger
 * Copyright (C) Pidgin Developers <devel@pidgin.im>
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
 * along with this program; if not, see <https://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include <purple.h>

#include <pidgin.h>

#include "gtkdisco.h"
#include "xmppdisco.h"

GList *dialogs = NULL;

enum {
	PIXBUF_COLUMN = 0,
	NAME_COLUMN,
	DESCRIPTION_COLUMN,
	SERVICE_COLUMN,
	NUM_OF_COLUMNS
};

static PidginDiscoList *
pidgin_disco_list_new(void) {
	return g_rc_box_new0(PidginDiscoList);
}

static void
pidgin_disco_list_destroy(PidginDiscoList *list)
{
	g_hash_table_destroy(list->services);
	if (list->dialog && list->dialog->discolist == list) {
		list->dialog->discolist = NULL;
	}

	g_free((gchar*)list->server);
}

PidginDiscoList *pidgin_disco_list_ref(PidginDiscoList *list)
{
	g_return_val_if_fail(list != NULL, NULL);

	purple_debug_misc("xmppdisco", "reffing list");
	return g_rc_box_acquire(list);
}

void pidgin_disco_list_unref(PidginDiscoList *list)
{
	g_return_if_fail(list != NULL);

	purple_debug_misc("xmppdisco", "unreffing list");
	g_rc_box_release_full(list, (GDestroyNotify)pidgin_disco_list_destroy);
}

void pidgin_disco_list_set_in_progress(PidginDiscoList *list, gboolean in_progress)
{
	PidginDiscoDialog *dialog = list->dialog;

	if (!dialog) {
		return;
	}

	list->in_progress = in_progress;

	if (in_progress) {
		gtk_widget_set_sensitive(dialog->account_chooser, FALSE);
		g_simple_action_set_enabled(dialog->stop_action, TRUE);
		g_simple_action_set_enabled(dialog->browse_action, FALSE);
	} else {
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(dialog->progress), 0.0);

		gtk_widget_set_sensitive(dialog->account_chooser, TRUE);

		g_simple_action_set_enabled(dialog->stop_action, FALSE);
		g_simple_action_set_enabled(dialog->browse_action, TRUE);
/*
		g_simple_action_set_enabled(dialog->register_action, FALSE);
		g_simple_action_set_enabled(dialog->add_action, FALSE);
*/
	}
}

static GdkPixbuf *
pidgin_disco_load_icon(XmppDiscoService *service, const char *size)
{
	GdkPixbuf *pixbuf = NULL;
	char *filename = NULL;
	gchar *tmp_size;

	g_return_val_if_fail(service != NULL, NULL);
	g_return_val_if_fail(size != NULL, NULL);

	tmp_size = g_strdup_printf("%sx%s", size, size);

	if (service->type == XMPP_DISCO_SERVICE_TYPE_GATEWAY && service->gateway_type) {
		char *tmp = g_strconcat("im-", service->gateway_type,
				".png", NULL);

		filename = g_build_filename(PURPLE_DATADIR,
			"pidgin", "icons", "hicolor", tmp_size, "apps",
			tmp, NULL);
		g_free(tmp);
#if 0
	} else if (service->type == XMPP_DISCO_SERVICE_TYPE_USER) {
		filename = g_build_filename(PURPLE_DATADIR,
			"pixmaps", "pidgin", "status", size, "person.png", NULL);
#endif
	} else if (service->type == XMPP_DISCO_SERVICE_TYPE_CHAT) {
		filename = g_build_filename(PURPLE_DATADIR,
			"pidgin", "icons", "hicolor", tmp_size, "status",
			"chat.png", NULL);
	}

	g_free(tmp_size);

	if (filename) {
		pixbuf = gdk_pixbuf_new_from_file(filename, NULL);
		g_free(filename);
	}

	return pixbuf;
}

static void
dialog_select_account_cb(GtkWidget *chooser, PidginDiscoDialog *dialog)
{
	PurpleAccount *account = pidgin_account_chooser_get_selected(PIDGIN_ACCOUNT_CHOOSER(chooser));
	gboolean change = (account != dialog->account);
	dialog->account = account;
	g_simple_action_set_enabled(dialog->browse_action, account != NULL);

	if (change) {
		g_clear_pointer(&dialog->discolist, pidgin_disco_list_unref);
	}
}

static void
activate_register(G_GNUC_UNUSED GSimpleAction *action,
                  G_GNUC_UNUSED GVariant *parameter,
                  gpointer data)
{
	PidginDiscoDialog *dialog = data;
	XmppDiscoService *service = dialog->selected;

	g_return_if_fail(service != NULL);

	xmpp_disco_service_register(service);
}

static void discolist_cancel_cb(PidginDiscoList *pdl, const char *server)
{
	pdl->dialog->prompt_handle = NULL;

	pidgin_disco_list_set_in_progress(pdl, FALSE);
	pidgin_disco_list_unref(pdl);
}

static void discolist_ok_cb(PidginDiscoList *pdl, const char *server)
{
	pdl->dialog->prompt_handle = NULL;
	g_simple_action_set_enabled(pdl->dialog->browse_action, TRUE);

	if (!server || !*server) {
		purple_notify_error(my_plugin, _("Invalid Server"), _("Invalid Server"),
			NULL, purple_request_cpar_from_connection(pdl->pc));

		pidgin_disco_list_set_in_progress(pdl, FALSE);
		pidgin_disco_list_unref(pdl);
		return;
	}

	pdl->server = g_strdup(server);
	pidgin_disco_list_set_in_progress(pdl, TRUE);
	xmpp_disco_start(pdl);
}

static void
activate_browse(G_GNUC_UNUSED GSimpleAction *action,
                G_GNUC_UNUSED GVariant *parameter,
                gpointer data)
{
	PidginDiscoDialog *dialog = data;
	PurpleConnection *pc;
	PidginDiscoList *pdl;
	const char *username;
	const char *at, *slash;
	char *server = NULL;

	pc = purple_account_get_connection(dialog->account);
	if (!pc) {
		return;
	}

	g_simple_action_set_enabled(dialog->browse_action, FALSE);
	g_simple_action_set_enabled(dialog->add_action, FALSE);
	g_simple_action_set_enabled(dialog->register_action, FALSE);

	g_clear_pointer(&dialog->discolist, pidgin_disco_list_unref);
	gtk_tree_store_clear(dialog->model);

	pdl = dialog->discolist = pidgin_disco_list_new();
	pdl->services = g_hash_table_new_full(NULL, NULL, NULL,
			(GDestroyNotify)gtk_tree_row_reference_free);
	pdl->pc = pc;
	/* We keep a copy... */
	pidgin_disco_list_ref(pdl);

	pdl->dialog = dialog;

	gtk_widget_set_sensitive(dialog->account_chooser, FALSE);

	username = purple_account_get_username(dialog->account);
	at = strchr(username, '@');
	slash = strchr(username, '/');
	if (at && !slash) {
		server = g_strdup(at + 1);
	} else if (at && slash && at + 1 < slash) {
		server = g_strdup_printf("%.*s", (int)(slash - (at + 1)), at + 1);
	}

	if (server == NULL) {
		/* This shouldn't ever happen since the account is connected */
		server = g_strdup("jabber.org");
	}

	/* Translators: The string "Enter an XMPP Server" is asking the user to
	   type the name of an XMPP server which will then be queried */
	dialog->prompt_handle = purple_request_input(my_plugin, _("Server name request"), _("Enter an XMPP Server"),
			_("Select an XMPP server to query"),
			server, FALSE, FALSE, NULL,
			_("Find Services"), G_CALLBACK(discolist_ok_cb),
			_("Cancel"), G_CALLBACK(discolist_cancel_cb),
			purple_request_cpar_from_connection(pc), pdl);

	g_free(server);
}

static void
activate_add_to_blist(G_GNUC_UNUSED GSimpleAction *action,
                      G_GNUC_UNUSED GVariant *parameter,
                      gpointer data)
{
	PidginDiscoDialog *dialog = data;
	XmppDiscoService *service = dialog->selected;
	PurpleAccount *account;
	const char *jid;

	g_return_if_fail(service != NULL);

	account = purple_connection_get_account(service->list->pc);
	jid = service->jid;

	if (service->type == XMPP_DISCO_SERVICE_TYPE_CHAT) {
		purple_blist_request_add_chat(account, NULL, NULL, jid);
	} else {
		purple_blist_request_add_buddy(account, jid, NULL, NULL);
	}
}

static gboolean
service_click_cb(GtkTreeView *tree, GdkEventButton *event, gpointer user_data)
{
	PidginDiscoDialog *dialog = user_data;
	XmppDiscoService *service;

	GtkTreePath *path;
	GtkTreeIter iter;
	GValue val;

	GdkRectangle rect;

	if (!gdk_event_triggers_context_menu((GdkEvent *)event)) {
		return FALSE;
	}

	/* Figure out what was clicked */
	if (!gtk_tree_view_get_path_at_pos(tree, event->x, event->y, &path,
		                               NULL, NULL, NULL))
	{
		return FALSE;
	}
	gtk_tree_model_get_iter(GTK_TREE_MODEL(dialog->model), &iter, path);
	gtk_tree_path_free(path);
	val.g_type = 0;
	gtk_tree_model_get_value(GTK_TREE_MODEL(dialog->model), &iter,
	                         SERVICE_COLUMN, &val);
	service = g_value_get_pointer(&val);

	if (!service) {
		return FALSE;
	}

	gtk_tree_view_convert_bin_window_to_widget_coords(dialog->tree,
	                                                  (gint)event->x,
	                                                  (gint)event->y,
	                                                  &rect.x, &rect.y);
	rect.width = rect.height = 1;

	gtk_popover_set_pointing_to(GTK_POPOVER(dialog->popover), &rect);
	gtk_popover_popup(GTK_POPOVER(dialog->popover));

	return FALSE;
}

static void
selection_changed_cb(GtkTreeSelection *selection, PidginDiscoDialog *dialog)
{
	GtkTreeIter iter;
	GValue val;
	gboolean allow_add = FALSE, allow_register = FALSE;

	if (gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		val.g_type = 0;
		gtk_tree_model_get_value(GTK_TREE_MODEL(dialog->model), &iter,
		                         SERVICE_COLUMN, &val);
		dialog->selected = g_value_get_pointer(&val);
		if (dialog->selected != NULL) {
			allow_add = (dialog->selected->flags & XMPP_DISCO_ADD);
			allow_register = (dialog->selected->flags & XMPP_DISCO_REGISTER);
		}
	}

	g_simple_action_set_enabled(dialog->add_action, allow_add);
	g_simple_action_set_enabled(dialog->register_action, allow_register);
}

static void
row_expanded_cb(GtkTreeView *tree, GtkTreeIter *arg1, GtkTreePath *rg2,
                gpointer user_data)
{
	PidginDiscoDialog *dialog = user_data;
	XmppDiscoService *service;
	GValue val;

	val.g_type = 0;
	gtk_tree_model_get_value(GTK_TREE_MODEL(dialog->model), arg1,
	                         SERVICE_COLUMN, &val);
	service = g_value_get_pointer(&val);
	xmpp_disco_service_expand(service);
}

static void
row_activated_cb(GtkTreeView       *tree_view,
                 GtkTreePath       *path,
                 GtkTreeViewColumn *column,
                 gpointer           user_data)
{
	PidginDiscoDialog *dialog = user_data;
	GtkTreeIter iter;
	XmppDiscoService *service;
	GValue val;

	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(dialog->model), &iter, path)) {
		return;
	}

	val.g_type = 0;
	gtk_tree_model_get_value(GTK_TREE_MODEL(dialog->model), &iter,
	                         SERVICE_COLUMN, &val);
	service = g_value_get_pointer(&val);

	if (service->flags & XMPP_DISCO_BROWSE) {
		if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(dialog->tree), path)) {
			gtk_tree_view_collapse_row(GTK_TREE_VIEW(dialog->tree), path);
		} else {
			gtk_tree_view_expand_row(GTK_TREE_VIEW(dialog->tree), path, FALSE);
		}
	} else if (service->flags & XMPP_DISCO_REGISTER) {
		g_action_activate(G_ACTION(dialog->register_action), NULL);
	} else if (service->flags & XMPP_DISCO_ADD) {
		g_action_activate(G_ACTION(dialog->add_action), NULL);
	}
}

static void
destroy_win_cb(GtkWidget *window, G_GNUC_UNUSED gpointer data)
{
	PidginDiscoDialog *dialog = PIDGIN_DISCO_DIALOG(window);
	PidginDiscoList *list = dialog->discolist;

	if (dialog->prompt_handle) {
		purple_request_close(PURPLE_REQUEST_INPUT, dialog->prompt_handle);
	}

	if (list) {
		list->dialog = NULL;

		if (list->in_progress) {
			list->in_progress = FALSE;
		}

		pidgin_disco_list_unref(list);
	}

	dialogs = g_list_remove(dialogs, dialog);
}

static void
activate_stop(G_GNUC_UNUSED GSimpleAction *action,
              G_GNUC_UNUSED GVariant *parameter,
              gpointer data)
{
	PidginDiscoDialog *dialog = data;

	pidgin_disco_list_set_in_progress(dialog->discolist, FALSE);
}

static void close_button_cb(GtkButton *button, PidginDiscoDialog *dialog)
{
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

static gboolean
disco_query_tooltip(GtkWidget *widget, int x, int y, gboolean keyboard_mode,
                    GtkTooltip *tooltip, gpointer data)
{
	PidginDiscoDialog *dialog = data;
	GtkTreePath *path = NULL;
	GtkTreeIter iter;
	XmppDiscoService *service;
	GValue val;
	const char *type = NULL;
	char *markup, *jid, *name, *desc = NULL;

	if (keyboard_mode) {
		GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
		if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) {
			return FALSE;
		}
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(dialog->model), &iter);
	} else {
		gint bx, by;

		gtk_tree_view_convert_widget_to_bin_window_coords(GTK_TREE_VIEW(widget),
		                                                  x, y, &bx, &by);
		gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), bx, by, &path,
		                              NULL, NULL, NULL);
		if (path == NULL) {
			return FALSE;
		}

		if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(dialog->model), &iter, path)) {
			gtk_tree_path_free(path);
			return FALSE;
		}
	}

	val.g_type = 0;
	gtk_tree_model_get_value(GTK_TREE_MODEL(dialog->model), &iter,
	                         SERVICE_COLUMN, &val);
	service = g_value_get_pointer(&val);
	if (!service) {
		gtk_tree_path_free(path);
		return FALSE;
	}

	switch (service->type) {
		case XMPP_DISCO_SERVICE_TYPE_UNSET:
			type = _("Unknown");
			break;

		case XMPP_DISCO_SERVICE_TYPE_GATEWAY:
			type = _("Gateway");
			break;

		case XMPP_DISCO_SERVICE_TYPE_DIRECTORY:
			type = _("Directory");
			break;

		case XMPP_DISCO_SERVICE_TYPE_CHAT:
			type = _("Chat");
			break;

		case XMPP_DISCO_SERVICE_TYPE_PUBSUB_COLLECTION:
			type = _("PubSub Collection");
			break;

		case XMPP_DISCO_SERVICE_TYPE_PUBSUB_LEAF:
			type = _("PubSub Leaf");
			break;

		case XMPP_DISCO_SERVICE_TYPE_OTHER:
			type = _("Other");
			break;
	}

	name = g_markup_escape_text(service->name, -1);
	jid = g_markup_escape_text(service->jid, -1);
	if (service->description != NULL) {
		desc = g_markup_escape_text(service->description, -1);
	}

	markup = g_strdup_printf("<span size='x-large' weight='bold'>%s</span>\n<b>%s:</b> %s%s%s",
	                         name, type, jid,
	                         service->description ? _("\n<b>Description:</b> ") : "",
	                         service->description ? desc : "");

	gtk_tooltip_set_markup(tooltip, markup);
	gtk_tree_view_set_tooltip_row(GTK_TREE_VIEW(widget), tooltip, path);

	g_free(markup);
	g_free(jid);
	g_free(name);
	g_free(desc);
	gtk_tree_path_free(path);

	return TRUE;
}

void pidgin_disco_signed_off_cb(PurpleConnection *pc)
{
	GList *node;

	for (node = dialogs; node; node = node->next) {
		PidginDiscoDialog *dialog = node->data;
		PidginDiscoList *list = dialog->discolist;

		if (list && list->pc == pc) {
			PurpleAccount *account = NULL;

			if (list->in_progress) {
				pidgin_disco_list_set_in_progress(list, FALSE);
			}

			gtk_tree_store_clear(dialog->model);

			pidgin_disco_list_unref(list);
			dialog->discolist = NULL;

			account = pidgin_account_chooser_get_selected(
				PIDGIN_ACCOUNT_CHOOSER(dialog->account_chooser));

			g_simple_action_set_enabled(dialog->browse_action, account != NULL);
			g_simple_action_set_enabled(dialog->add_action, FALSE);
			g_simple_action_set_enabled(dialog->register_action, FALSE);
		}
	}
}

void pidgin_disco_dialogs_destroy_all(void)
{
	while (dialogs) {
		GtkWidget *dialog = dialogs->data;

		gtk_widget_destroy(dialog);
		/* destroy_win_cb removes the dialog from the list */
	}
}

/******************************************************************************
 * GObject implementation
 *****************************************************************************/

G_DEFINE_DYNAMIC_TYPE(PidginDiscoDialog, pidgin_disco_dialog, GTK_TYPE_DIALOG)

static void
pidgin_disco_dialog_class_init(PidginDiscoDialogClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	gtk_widget_class_set_template_from_resource(
	        widget_class, "/im/pidgin/Pidgin3/Plugin/XMPPDisco/disco.ui");

	gtk_widget_class_bind_template_child(widget_class, PidginDiscoDialog,
	                                     account_chooser);
	gtk_widget_class_bind_template_child(widget_class, PidginDiscoDialog,
	                                     progress);
	gtk_widget_class_bind_template_child(widget_class, PidginDiscoDialog,
	                                     tree);
	gtk_widget_class_bind_template_child(widget_class, PidginDiscoDialog,
	                                     model);
	gtk_widget_class_bind_template_child(widget_class, PidginDiscoDialog,
	                                     popover);
	gtk_widget_class_bind_template_child(widget_class, PidginDiscoDialog,
	                                     popover_menu);

	gtk_widget_class_bind_template_callback(widget_class, destroy_win_cb);
	gtk_widget_class_bind_template_callback(widget_class, close_button_cb);
	gtk_widget_class_bind_template_callback(widget_class,
	                                        dialog_select_account_cb);
	gtk_widget_class_bind_template_callback(widget_class, row_activated_cb);
	gtk_widget_class_bind_template_callback(widget_class, row_expanded_cb);
	gtk_widget_class_bind_template_callback(widget_class, service_click_cb);
	gtk_widget_class_bind_template_callback(widget_class,
	                                        selection_changed_cb);
}

static void
pidgin_disco_dialog_class_finalize(PidginDiscoDialogClass *klass)
{
}

static void
pidgin_disco_dialog_init(PidginDiscoDialog *dialog)
{
	GActionEntry entries[] = {
		{ .name = "stop", .activate = activate_stop },
		{ .name = "browse", .activate = activate_browse },
		{ .name = "register", .activate = activate_register },
		{ .name = "add", .activate = activate_add_to_blist },
	};
	GSimpleActionGroup *action_group = NULL;
	GActionMap *action_map = NULL;

	dialogs = g_list_prepend(dialogs, dialog);

	gtk_widget_init_template(GTK_WIDGET(dialog));

	/* accounts dropdown list */
	dialog->account = pidgin_account_chooser_get_selected(
		PIDGIN_ACCOUNT_CHOOSER(dialog->account_chooser));

	gtk_widget_set_has_tooltip(GTK_WIDGET(dialog->tree), TRUE);
	g_signal_connect(G_OBJECT(dialog->tree), "query-tooltip",
	                 G_CALLBACK(disco_query_tooltip), dialog);

	action_group = g_simple_action_group_new();
	action_map = G_ACTION_MAP(action_group);
	g_action_map_add_action_entries(action_map, entries, G_N_ELEMENTS(entries),
	                                dialog);

	dialog->stop_action = G_SIMPLE_ACTION(
			g_action_map_lookup_action(action_map, "stop"));
	g_simple_action_set_enabled(dialog->stop_action, FALSE);

	dialog->browse_action = G_SIMPLE_ACTION(
			g_action_map_lookup_action(action_map, "browse"));
	g_simple_action_set_enabled(dialog->browse_action, dialog->account != NULL);

	dialog->register_action = G_SIMPLE_ACTION(
			g_action_map_lookup_action(action_map, "register"));
	g_simple_action_set_enabled(dialog->register_action, FALSE);

	dialog->add_action = G_SIMPLE_ACTION(
			g_action_map_lookup_action(action_map, "add"));
	g_simple_action_set_enabled(dialog->add_action, FALSE);

	gtk_widget_insert_action_group(GTK_WIDGET(dialog), "disco",
	                               G_ACTION_GROUP(action_group));

	gtk_popover_bind_model(GTK_POPOVER(dialog->popover), dialog->popover_menu,
	                       NULL);
}

/******************************************************************************
 * Public API
 *****************************************************************************/

void
pidgin_disco_dialog_register(PurplePlugin *plugin)
{
	pidgin_disco_dialog_register_type(G_TYPE_MODULE(plugin));
}

PidginDiscoDialog *
pidgin_disco_dialog_new(void)
{
	PidginDiscoDialog *dialog = g_object_new(PIDGIN_TYPE_DISCO_DIALOG, NULL);
	gtk_widget_show_all(GTK_WIDGET(dialog));
	return dialog;
}

void pidgin_disco_add_service(PidginDiscoList *pdl, XmppDiscoService *service, XmppDiscoService *parent)
{
	PidginDiscoDialog *dialog;
	GtkTreeIter iter, parent_iter, child;
	GdkPixbuf *pixbuf = NULL;
	gboolean append = TRUE;

	dialog = pdl->dialog;
	g_return_if_fail(dialog != NULL);

	if (service != NULL) {
		purple_debug_info("xmppdisco", "Adding service \"%s\"", service->name);
	} else {
		purple_debug_info("xmppdisco", "Service \"%s\" has no children",
		                  parent->name);
	}

	gtk_progress_bar_pulse(GTK_PROGRESS_BAR(dialog->progress));

	if (parent) {
		GtkTreeRowReference *rr;
		GtkTreePath *path;

		rr = g_hash_table_lookup(pdl->services, parent);
		path = gtk_tree_row_reference_get_path(rr);
		if (path) {
			gtk_tree_model_get_iter(GTK_TREE_MODEL(dialog->model),
			                        &parent_iter, path);
			gtk_tree_path_free(path);

			if (gtk_tree_model_iter_children(
			            GTK_TREE_MODEL(dialog->model), &child,
			            &parent_iter)) {
				PidginDiscoList *tmp;
				gtk_tree_model_get(
				        GTK_TREE_MODEL(dialog->model), &child,
				        SERVICE_COLUMN, &tmp, -1);
				if (!tmp) {
					append = FALSE;
				}
			}
		}
	}

	if (service == NULL) {
		if (parent != NULL && !append) {
			gtk_tree_store_remove(dialog->model, &child);
		}
		return;
	}

	if (append) {
		gtk_tree_store_append(dialog->model, &iter,
		                      (parent ? &parent_iter : NULL));
	} else {
		iter = child;
	}

	if (service->flags & XMPP_DISCO_BROWSE) {
		GtkTreeRowReference *rr;
		GtkTreePath *path;

		gtk_tree_store_append(dialog->model, &child, &iter);

		path = gtk_tree_model_get_path(GTK_TREE_MODEL(dialog->model),
		                               &iter);
		rr = gtk_tree_row_reference_new(GTK_TREE_MODEL(dialog->model),
		                                path);
		g_hash_table_insert(pdl->services, service, rr);
		gtk_tree_path_free(path);
	}

	pixbuf = pidgin_disco_load_icon(service, "16");

	gtk_tree_store_set(dialog->model, &iter, PIXBUF_COLUMN, pixbuf,
	                   NAME_COLUMN, service->name, DESCRIPTION_COLUMN,
	                   service->description, SERVICE_COLUMN, service, -1);

	if (pixbuf) {
		g_object_unref(pixbuf);
	}
}

