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

#include <glib/gi18n-lib.h>

#include <adwaita.h>

#include "pidginaccounteditor.h"

#include "pidginprotocolchooser.h"

struct _PidginAccountEditor {
	GtkDialog parent;

	PurpleAccount *account;

	/* Login Options */
	GtkWidget *login_options;
	GtkWidget *protocol;
	GtkWidget *username;

	GList *user_split_entries;
	GList *user_split_rows;

	/* User Options */
	GtkWidget *alias;

	GtkFileChooserNative *avatar_dialog;
	GdkPixbuf *avatar_pixbuf;
	GtkWidget *avatar_row;
	GtkWidget *use_custom_avatar;
	GtkWidget *avatar;

	/* Advanced Options */
	GtkWidget *advanced_group;
	GtkWidget *advanced_toggle;

	GList *advanced_entries;
	GList *advanced_rows;

	/* Proxy Options */
	GtkWidget *proxy_type;
	GtkWidget *proxy_options;
	GtkWidget *proxy_host;
	GtkWidget *proxy_port;
	GtkWidget *proxy_username;
	GtkWidget *proxy_password;
};

enum {
	PROP_0,
	PROP_ACCOUNT,
	N_PROPERTIES,
};
static GParamSpec *properties[N_PROPERTIES] = {NULL, };


/******************************************************************************
 * Prototypes
 *****************************************************************************/
static void pidgin_account_editor_connection_changed_cb(GObject *obj,
                                                        GParamSpec *pspec,
                                                        gpointer data);

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
pidgin_account_editor_add_user_split(gpointer data, gpointer user_data) {
	PurpleAccountUserSplit *split = data;
	PidginAccountEditor *editor = user_data;
	GtkWidget *entry = NULL;

	if(!purple_account_user_split_is_constant(split)) {
		GtkWidget *row = NULL;

		row = adw_action_row_new();
		editor->user_split_rows = g_list_append(editor->user_split_rows, row);
		adw_preferences_group_add(ADW_PREFERENCES_GROUP(editor->login_options),
		                          row);

		adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row),
		                              purple_account_user_split_get_text(split));

		entry = gtk_entry_new();
		gtk_widget_set_hexpand(entry, TRUE);
		gtk_widget_set_valign(entry, GTK_ALIGN_CENTER);
		adw_action_row_add_suffix(ADW_ACTION_ROW(row), entry);
		adw_action_row_set_activatable_widget(ADW_ACTION_ROW(row), entry);
	}

	editor->user_split_entries = g_list_append(editor->user_split_entries,
	                                           entry);
}

static gboolean
pidgin_account_editor_update_login_options(PidginAccountEditor *editor,
                                           PurpleProtocol *protocol)
{
	GList *user_splits = NULL;
	GList *split_item = NULL;
	GList *entry_item = NULL;
	gchar *username = NULL;

	/* Clear out the old user splits from our list. */
	g_clear_pointer(&editor->user_split_entries, g_list_free);

	/* Now remove the rows we added to the preference group for each non
	 * constant user split.
	 */
	while(editor->user_split_rows != NULL) {
		adw_preferences_group_remove(ADW_PREFERENCES_GROUP(editor->login_options),
		                             editor->user_split_rows->data);

		editor->user_split_rows = g_list_delete_link(editor->user_split_rows,
		                                             editor->user_split_rows);
	}

	/* Add the user splits for the protocol. */
	user_splits = purple_protocol_get_user_splits(protocol);
	g_list_foreach(user_splits, pidgin_account_editor_add_user_split, editor);

	/* If we have an account, populate its values. */
	if(PURPLE_IS_ACCOUNT(editor->account)) {
		/* The username will be split apart below and eventually set as the text
		 * in the username entry.
		 */
		username = g_strdup(purple_account_get_username(editor->account));
	}

	/* Filling out the user splits is a pain. If we have an account, we created
	 * a copy of the username above. We then iterate the user splits backwards
	 * so we can insert a null terminator at the start of each split we find in
	 * the username.
	 */
	split_item = g_list_last(user_splits);
	entry_item = g_list_last(editor->user_split_entries);
	while(split_item != NULL && entry_item != NULL) {
		GtkWidget *entry = entry_item->data;
		PurpleAccountUserSplit *split = split_item->data;
		gchar *ptr = NULL;
		const gchar *value = NULL;

		if(username != NULL) {
			gchar sep = purple_account_user_split_get_separator(split);

			if(purple_account_user_split_get_reverse(split)) {
				ptr = strrchr(username, sep);
			} else {
				ptr = strchr(username, sep);
			}

			if(ptr != NULL) {
				/* Insert a null terminator in place of the separator. */
				*ptr = '\0';

				/* Set the value to the first byte after the separator. */
				value = ptr + 1;
			}
		}

		if(value == NULL) {
			value = purple_account_user_split_get_default_value(split);
		}

		if(value != NULL && GTK_IS_ENTRY(entry)) {
			gtk_editable_set_text(GTK_EDITABLE(entry), value);
		}

		split_item = split_item->prev;
		entry_item = entry_item->prev;
	}

	/* Free the user splits. */
	g_list_free_full(user_splits,
	                 (GDestroyNotify)purple_account_user_split_destroy);

	/* Set the username entry to the remaining text in username and free our
	 * copy of said username.
	 */
	if(username != NULL) {
		gtk_editable_set_text(GTK_EDITABLE(editor->username), username);
		g_free(username);
		return TRUE;
	}

	return FALSE;
}

static void
pidgin_account_editor_update_user_options(PidginAccountEditor *editor,
                                          PurpleProtocol *protocol)
{
	PurpleBuddyIconSpec *icon_spec = NULL;
	PurpleImage *image = NULL;
	gboolean show_avatar_opts = TRUE;
	const gchar *svalue = "";
	gboolean use_global = TRUE;

	/* Check if the protocol supports avatars. */
	icon_spec = purple_protocol_get_icon_spec(protocol);
	show_avatar_opts = (icon_spec != NULL && icon_spec->format != NULL);
	purple_buddy_icon_spec_free(icon_spec);

	gtk_widget_set_visible(editor->avatar_row, show_avatar_opts);

	/* Determine our values. */
	if(editor->account != NULL) {
		svalue = purple_account_get_private_alias(editor->account);
		image = purple_buddy_icons_find_account_icon(editor->account);
		use_global = purple_account_get_bool(editor->account,
		                                     "use-global-buddyicon", TRUE);
	}

	if(svalue == NULL) {
		svalue = "";
	}

	gtk_editable_set_text(GTK_EDITABLE(editor->alias), svalue);
	gtk_switch_set_active(GTK_SWITCH(editor->use_custom_avatar), !use_global);

	g_clear_object(&editor->avatar_pixbuf);
	if(PURPLE_IS_IMAGE(image)) {
		editor->avatar_pixbuf = purple_gdk_pixbuf_from_image(image);
		gtk_image_set_from_pixbuf(GTK_IMAGE(editor->avatar),
		                          editor->avatar_pixbuf);
	} else {
		gtk_image_set_from_icon_name(GTK_IMAGE(editor->avatar),
		                             "select-avatar");
	}
}

static gboolean
pidgin_account_editor_advanced_option_use_default(PidginAccountEditor *editor) {
	PurpleProtocol *protocol = NULL;

	/* If this is the new dialog, use the default value. */
	if(!PURPLE_IS_ACCOUNT(editor->account)) {
		return TRUE;
	}

	/* If we have an existing account, check if the protocol has changed. */
	protocol = pidgin_protocol_chooser_get_protocol(PIDGIN_PROTOCOL_CHOOSER(editor->protocol));
	if(protocol != purple_account_get_protocol(editor->account)) {
		return TRUE;
	}

	return FALSE;
}

static GtkWidget *
pidgin_account_editor_add_advanced_boolean(PidginAccountEditor *editor,
                                           PurpleAccountOption *option)
{
	GtkWidget *row = NULL;
	GtkWidget *toggle = NULL;
	gboolean value = FALSE;
	gchar *title = NULL;

	if(pidgin_account_editor_advanced_option_use_default(editor)) {
		value = purple_account_option_get_default_bool(option);
	} else {
		const gchar *setting = purple_account_option_get_setting(option);
		gboolean def_value = purple_account_option_get_default_bool(option);

		value = purple_account_get_bool(editor->account, setting, def_value);
	}

	/* Create the row and set its title with a mnemonic. */
	row = adw_action_row_new();
	g_object_bind_property(editor->advanced_toggle, "active", row, "visible",
	                       G_BINDING_SYNC_CREATE);
	adw_preferences_row_set_use_underline(ADW_PREFERENCES_ROW(row), TRUE);
	title = g_strdup_printf("_%s", purple_account_option_get_text(option));
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title);
	g_free(title);

	adw_preferences_group_add(ADW_PREFERENCES_GROUP(editor->advanced_group),
	                          row);

	/* Add the row to the editor's list of advanced rows. */
	editor->advanced_rows = g_list_append(editor->advanced_rows, row);

	/* Create the input widget. */
	toggle = gtk_switch_new();
	gtk_switch_set_active(GTK_SWITCH(toggle), value);
	gtk_widget_set_valign(toggle, GTK_ALIGN_CENTER);
	adw_action_row_add_suffix(ADW_ACTION_ROW(row), toggle);
	adw_action_row_set_activatable_widget(ADW_ACTION_ROW(row), toggle);

	return toggle;
}

static GtkWidget *
pidgin_account_editor_add_advanced_int(PidginAccountEditor *editor,
                                       PurpleAccountOption *option)
{
	GtkWidget *row = NULL;
	GtkWidget *entry = NULL;
	gint value = 0;
	gchar *title = NULL;
	gchar *svalue = NULL;

	if(pidgin_account_editor_advanced_option_use_default(editor)) {
		value = purple_account_option_get_default_int(option);
	} else {
		const gchar *setting = purple_account_option_get_setting(option);
		gint def_value = purple_account_option_get_default_int(option);

		value = purple_account_get_int(editor->account, setting, def_value);
	}

	/* Create the row and set its title with a mnemonic. */
	row = adw_action_row_new();
	g_object_bind_property(editor->advanced_toggle, "active", row, "visible",
	                       G_BINDING_SYNC_CREATE);
	adw_preferences_row_set_use_underline(ADW_PREFERENCES_ROW(row), TRUE);
	title = g_strdup_printf("_%s", purple_account_option_get_text(option));
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title);
	g_free(title);

	adw_preferences_group_add(ADW_PREFERENCES_GROUP(editor->advanced_group),
	                          row);

	/* Add the row to the editor's list of advanced rows. */
	editor->advanced_rows = g_list_append(editor->advanced_rows, row);

	/* Create the input widget. */
	entry = gtk_entry_new();
	gtk_entry_set_input_purpose(GTK_ENTRY(entry), GTK_INPUT_PURPOSE_DIGITS);
	svalue = g_strdup_printf("%d", value);
	gtk_editable_set_text(GTK_EDITABLE(entry), svalue);
	g_free(svalue);

	gtk_widget_set_hexpand(entry, TRUE);
	gtk_widget_set_valign(entry, GTK_ALIGN_CENTER);
	adw_action_row_add_suffix(ADW_ACTION_ROW(row), entry);
	adw_action_row_set_activatable_widget(ADW_ACTION_ROW(row), entry);

	return entry;
}

static GtkWidget *
pidgin_account_editor_add_advanced_string(PidginAccountEditor *editor,
                                          PurpleAccountOption *option)
{
	GtkWidget *row = NULL;
	GtkWidget *entry = NULL;
	gchar *title = NULL;
	const gchar *value = NULL;

	if(pidgin_account_editor_advanced_option_use_default(editor)) {
		value = purple_account_option_get_default_string(option);
	} else {
		const gchar *setting = purple_account_option_get_setting(option);
		const gchar *def_value = NULL;

		def_value = purple_account_option_get_default_string(option);

		value = purple_account_get_string(editor->account, setting, def_value);
	}

	/* Create the row and set its title with a mnemonic. */
	row = adw_action_row_new();
	g_object_bind_property(editor->advanced_toggle, "active", row, "visible",
	                       G_BINDING_SYNC_CREATE);
	adw_preferences_row_set_use_underline(ADW_PREFERENCES_ROW(row), TRUE);
	title = g_strdup_printf("_%s", purple_account_option_get_text(option));
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title);
	g_free(title);

	adw_preferences_group_add(ADW_PREFERENCES_GROUP(editor->advanced_group),
	                          row);

	/* Add the row to the editor's list of advanced rows. */
	editor->advanced_rows = g_list_append(editor->advanced_rows, row);

	/* Create the input widget. */
	if(purple_account_option_string_get_masked(option)) {
		entry = gtk_password_entry_new();
	} else {
		entry = gtk_entry_new();
	}

	if(value != NULL) {
		gtk_editable_set_text(GTK_EDITABLE(entry), value);
	}
	gtk_widget_set_hexpand(entry, TRUE);
	gtk_widget_set_valign(entry, GTK_ALIGN_CENTER);
	adw_action_row_add_suffix(ADW_ACTION_ROW(row), entry);
	adw_action_row_set_activatable_widget(ADW_ACTION_ROW(row), entry);

	return entry;
}

static GtkWidget *
pidgin_account_editor_add_advanced_list(PidginAccountEditor *editor,
                                        PurpleAccountOption *option)
{
	GtkWidget *row = NULL;
	GtkStringList *model = NULL;
	GList *data = NULL;
	GList *items = NULL;
	gchar *title = NULL;
	const gchar *value = FALSE;
	guint index = 0;
	guint position = 0;

	if(pidgin_account_editor_advanced_option_use_default(editor)) {
		value = purple_account_option_get_default_list_value(option);
	} else {
		const gchar *setting = purple_account_option_get_setting(option);
		const gchar *def_value = NULL;

		def_value = purple_account_option_get_default_list_value(option);

		value = purple_account_get_string(editor->account, setting, def_value);
	}

	/* Create the row and set its title with a mnemonic. */
	row = adw_combo_row_new();
	g_object_bind_property(editor->advanced_toggle, "active", row, "visible",
	                       G_BINDING_SYNC_CREATE);
	adw_preferences_row_set_use_underline(ADW_PREFERENCES_ROW(row), TRUE);
	adw_combo_row_set_use_subtitle(ADW_COMBO_ROW(row), TRUE);
	title = g_strdup_printf("_%s", purple_account_option_get_text(option));
	adw_preferences_row_set_title(ADW_PREFERENCES_ROW(row), title);
	g_free(title);

	adw_preferences_group_add(ADW_PREFERENCES_GROUP(editor->advanced_group),
	                          row);

	/* Add the row to the editor's list of advanced rows. */
	editor->advanced_rows = g_list_append(editor->advanced_rows, row);

	/* Create the model and data for the expression. */
	items = purple_account_option_get_list(option);
	model = gtk_string_list_new(NULL);

	for(GList *l = items; l != NULL; l = l->next) {
		PurpleKeyValuePair *kvp = l->data;

		if(kvp != NULL) {
			if(purple_strequal(kvp->value, value)) {
				position = index;
			}

			data = g_list_append(data, kvp->value);
			gtk_string_list_append(model, kvp->key);
		}

		index++;
	}

	adw_combo_row_set_model(ADW_COMBO_ROW(row), G_LIST_MODEL(model));
	adw_combo_row_set_selected(ADW_COMBO_ROW(row), position);
	g_object_set_data_full(G_OBJECT(row), "keys", data,
	                       (GDestroyNotify)g_list_free);

	return row;
}

static void
pidgin_account_editor_add_advanced_option(PidginAccountEditor *editor,
                                          PurpleAccountOption *option)
{
	PurplePrefType type;
	GtkWidget *widget = NULL;

	type = purple_account_option_get_pref_type(option);
	switch(type) {
		case PURPLE_PREF_BOOLEAN:
			widget = pidgin_account_editor_add_advanced_boolean(editor, option);
			break;
		case PURPLE_PREF_INT:
			widget = pidgin_account_editor_add_advanced_int(editor, option);
			break;
		case PURPLE_PREF_STRING:
			widget = pidgin_account_editor_add_advanced_string(editor, option);
			break;
		case PURPLE_PREF_STRING_LIST:
			widget = pidgin_account_editor_add_advanced_list(editor, option);
			break;
		default:
			purple_debug_error("PidginAccountEditor",
			                   "Invalid Account Option pref type (%d)", type);
			break;
	}

	if(GTK_IS_WIDGET(widget)) {
		g_object_set_data_full(G_OBJECT(widget), "option", option,
		                       (GDestroyNotify)purple_account_option_destroy);

		editor->advanced_entries = g_list_append(editor->advanced_entries,
		                                         widget);
	} else {
		purple_account_option_destroy(option);
	}
}

static void
pidgin_account_editor_update_advanced_options(PidginAccountEditor *editor,
                                              PurpleProtocol *protocol)
{
	GList *options = NULL;

	g_clear_pointer(&editor->advanced_entries, g_list_free);
	while(editor->advanced_rows != NULL) {
		adw_preferences_group_remove(ADW_PREFERENCES_GROUP(editor->advanced_group),
		                             GTK_WIDGET(editor->advanced_rows->data));

		editor->advanced_rows = g_list_delete_link(editor->advanced_rows,
		                                           editor->advanced_rows);
	}

	if(!PURPLE_IS_PROTOCOL(protocol)) {
		gtk_widget_set_visible(editor->advanced_group, FALSE);

		return;
	}

	options = purple_protocol_get_account_options(protocol);
	if(options == NULL) {
		gtk_widget_set_visible(editor->advanced_group, FALSE);

		return;
	}

	/* Iterate the options and call our helper which will take ownership of the
	 * option itself, but we'll delete the list item as we go.
	 */
	while(options != NULL) {
		pidgin_account_editor_add_advanced_option(editor, options->data);

		options = g_list_delete_link(options, options);
	}

	gtk_widget_set_visible(editor->advanced_group, TRUE);
}

static void
pidgin_account_editor_update_proxy_options(PidginAccountEditor *editor) {
	PurpleProxyInfo *info = NULL;
	GListModel *model = NULL;
	const gchar *svalue = NULL;
	gint ivalue = 0;
	guint position = 0;

	if(!PURPLE_IS_ACCOUNT(editor->account)) {
		return;
	}

	info = purple_account_get_proxy_info(editor->account);

	switch(purple_proxy_info_get_proxy_type(info)) {
		case PURPLE_PROXY_TYPE_USE_GLOBAL:
			svalue = "global";
			break;
		case PURPLE_PROXY_TYPE_NONE:
			svalue = "none";
			break;
		case PURPLE_PROXY_TYPE_SOCKS4:
			svalue = "socks4";
			break;
		case PURPLE_PROXY_TYPE_SOCKS5:
			svalue = "socks5";
			break;
		case PURPLE_PROXY_TYPE_TOR:
			svalue = "tor";
			break;
		case PURPLE_PROXY_TYPE_HTTP:
			svalue = "http";
			break;
		case PURPLE_PROXY_TYPE_USE_ENVVAR:
			svalue = "envvar";
			break;
	}

	model = adw_combo_row_get_model(ADW_COMBO_ROW(editor->proxy_type));
	for(guint i = 0; i < g_list_model_get_n_items(model); i++) {
		GtkStringObject *obj = g_list_model_get_item(model, i);
		if(purple_strequal(svalue, gtk_string_object_get_string(obj))) {
			position = i;
			break;
		}
	}
	adw_combo_row_set_selected(ADW_COMBO_ROW(editor->proxy_type), position);

	svalue = purple_proxy_info_get_hostname(info);
	if(svalue == NULL) {
		svalue = "";
	}
	gtk_editable_set_text(GTK_EDITABLE(editor->proxy_host), svalue);

	ivalue = purple_proxy_info_get_port(info);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(editor->proxy_port),
	                          (gdouble)ivalue);

	svalue = purple_proxy_info_get_username(info);
	if(svalue == NULL) {
		svalue = "";
	}
	gtk_editable_set_text(GTK_EDITABLE(editor->proxy_username), svalue);

	svalue = purple_proxy_info_get_password(info);
	if(svalue == NULL) {
		svalue = "";
	}
	gtk_editable_set_text(GTK_EDITABLE(editor->proxy_password), svalue);
}

static void
pidgin_account_editor_update(PidginAccountEditor *editor) {
	PurpleProtocol *protocol = NULL;
	gboolean sensitive = FALSE;

	if(PURPLE_IS_ACCOUNT(editor->account)) {
		PurpleConnection *connection = NULL;

		connection = purple_account_get_connection(editor->account);
		if(PURPLE_IS_CONNECTION(connection)) {
			gtk_widget_set_sensitive(editor->protocol, FALSE);
		}
	}

	protocol = pidgin_protocol_chooser_get_protocol(PIDGIN_PROTOCOL_CHOOSER(editor->protocol));

	sensitive = pidgin_account_editor_update_login_options(editor, protocol);
	pidgin_account_editor_update_user_options(editor, protocol);
	pidgin_account_editor_update_advanced_options(editor, protocol);
	pidgin_account_editor_update_proxy_options(editor);

	gtk_dialog_set_response_sensitive(GTK_DIALOG(editor), GTK_RESPONSE_APPLY,
	                                  sensitive);
}

static void
pidgin_account_editor_login_options_update_editable(PidginAccountEditor *editor)
{
	PurpleConnection *connection = NULL;
	gboolean editable = TRUE;

	if(PURPLE_IS_ACCOUNT(editor->account)) {

		connection = purple_account_get_connection(editor->account);

		/* If we have an active connection, we need to disable everything
		 * related to the protocol and username.
		 */
		if(PURPLE_IS_CONNECTION(connection)) {
			PidginProtocolChooser *chooser = NULL;
			PurpleProtocol *connected_protocol = NULL;
			PurpleProtocol *selected_protocol = NULL;
			editable = FALSE;

			/* Check if the user changed the protocol. If they did, switch it
			 * back and update the editor to reflect what settings are active.
			 */
			connected_protocol = purple_connection_get_protocol(connection);

			chooser = PIDGIN_PROTOCOL_CHOOSER(editor->protocol);
			selected_protocol = pidgin_protocol_chooser_get_protocol(chooser);
			if(connected_protocol != selected_protocol) {
				pidgin_protocol_chooser_set_protocol(chooser, connected_protocol);
				pidgin_account_editor_update(editor);
			}
		}

	}

	gtk_widget_set_sensitive(editor->protocol, editable);
	gtk_editable_set_editable(GTK_EDITABLE(editor->username), editable);
	for(GList *l = editor->user_split_entries; l != NULL; l = l->next) {
		GtkWidget *widget = l->data;

		gtk_editable_set_editable(GTK_EDITABLE(widget), editable);
	}
}

static void
pidgin_account_editor_set_account(PidginAccountEditor *editor,
                                  PurpleAccount *account)
{
	if(g_set_object(&editor->account, account)) {
		if(PURPLE_IS_ACCOUNT(account)) {
			g_signal_connect(account, "notify::connection",
			                 G_CALLBACK(pidgin_account_editor_connection_changed_cb),
			                 editor);
		}

		g_object_notify_by_pspec(G_OBJECT(editor), properties[PROP_ACCOUNT]);
	}

	pidgin_account_editor_update(editor);
}

static gboolean
pidgin_account_editor_save_login_options(PidginAccountEditor *editor) {
	PurpleProtocol *protocol = NULL;
	GList *split_item = NULL, *entry_item = NULL;
	GString *username = NULL;
	const gchar *protocol_id = NULL;
	gboolean new_account = FALSE;

	protocol = pidgin_protocol_chooser_get_protocol(PIDGIN_PROTOCOL_CHOOSER(editor->protocol));
	protocol_id = purple_protocol_get_id(protocol);

	username = g_string_new(gtk_editable_get_text(GTK_EDITABLE(editor->username)));

	split_item = purple_protocol_get_user_splits(protocol);
	entry_item = editor->user_split_entries;
	while(split_item != NULL && entry_item != NULL) {
		PurpleAccountUserSplit *split = split_item->data;
		GtkEntry *entry = entry_item->data;
		const gchar *value = "";
		gchar sep = '\0';

		sep = purple_account_user_split_get_separator(split);
		g_string_append_c(username, sep);

		if(GTK_IS_ENTRY(entry)) {
			value = gtk_editable_get_text(GTK_EDITABLE(entry));
		}

		if(value == NULL || *value == '\0') {
			value = purple_account_user_split_get_default_value(split);
		}

		g_string_append(username, value);

		split_item = split_item->next;
		entry_item = entry_item->next;
	}

	if(!PURPLE_IS_ACCOUNT(editor->account)) {
		editor->account = purple_account_new(username->str, protocol_id);
		new_account = TRUE;
	} else {
		purple_account_set_username(editor->account, username->str);
		purple_account_set_protocol_id(editor->account, protocol_id);
	}

	g_string_free(username, TRUE);

	return new_account;
}

static void
pidgin_account_editor_save_user_options(PidginAccountEditor *editor) {
	const gchar *svalue = NULL;
	gboolean bvalue = FALSE;

	/* Set the alias. */
	svalue = gtk_editable_get_text(GTK_EDITABLE(editor->alias));
	if(*svalue == '\0') {
		svalue = NULL;
	}
	purple_account_set_private_alias(editor->account, svalue);

	/* Set whether or not to use the global avatar. */
	bvalue = gtk_switch_get_active(GTK_SWITCH(editor->use_custom_avatar));
	purple_account_set_bool(editor->account, "use-global-buddyicon", !bvalue);

	if(bvalue) {
		if(GDK_IS_PIXBUF(editor->avatar_pixbuf)) {
			# warning implement this when buddy icons do not suck so bad.
		} else {
			purple_buddy_icons_set_account_icon(editor->account, NULL, 0);
		}
	} else {
		# warning set the global buddy icon when buddy icons do not suck so bad.
	}
}

static void
pidgin_account_editor_save_advanced_options(PidginAccountEditor *editor) {
	for(GList *l = editor->advanced_entries; l != NULL; l = l->next) {
		GtkWidget *widget = l->data;
		PurpleAccountOption *option = NULL;
		GList *keys = NULL;
		const gchar *setting = NULL;
		const gchar *svalue = NULL;
		gboolean bvalue = FALSE;
		gint ivalue = 0;
		guint selected = 0;

		option = g_object_get_data(G_OBJECT(widget), "option");
		setting = purple_account_option_get_setting(option);

		switch(purple_account_option_get_pref_type(option)) {
			case PURPLE_PREF_STRING:
				svalue = gtk_editable_get_text(GTK_EDITABLE(widget));
				purple_account_set_string(editor->account, setting, svalue);
				break;
			case PURPLE_PREF_INT:
				svalue = gtk_editable_get_text(GTK_EDITABLE(widget));
				ivalue = atoi(svalue);
				purple_account_set_int(editor->account, setting, ivalue);
				break;
			case PURPLE_PREF_BOOLEAN:
				bvalue = gtk_switch_get_active(GTK_SWITCH(widget));
				purple_account_set_bool(editor->account, setting, bvalue);
				break;
			case PURPLE_PREF_STRING_LIST:
				keys = g_object_get_data(G_OBJECT(widget), "keys");
				selected = adw_combo_row_get_selected(ADW_COMBO_ROW(widget));
				svalue = g_list_nth_data(keys, selected);
				purple_account_set_string(editor->account, setting, svalue);
				break;
			default:
				break;
		}
	}
}

static void
pidgin_account_editor_save_proxy(PidginAccountEditor *editor,
                                 gboolean new_account)
{
	PurpleProxyInfo *info = NULL;
	PurpleProxyType type = PURPLE_PROXY_TYPE_NONE;
	GObject *item = NULL;
	const gchar *svalue = NULL;
	gint ivalue = 0;

	/* Build the ProxyInfo object */
	if(new_account) {
		info = purple_proxy_info_new();
		purple_account_set_proxy_info(editor->account, info);
	} else {
		info = purple_account_get_proxy_info(editor->account);
	}

	item = adw_combo_row_get_selected_item(ADW_COMBO_ROW(editor->proxy_type));
	svalue = gtk_string_object_get_string(GTK_STRING_OBJECT(item));
	if(purple_strequal(svalue, "global")) {
		type = PURPLE_PROXY_TYPE_USE_GLOBAL;
	} else if(purple_strequal(svalue, "none")) {
		type = PURPLE_PROXY_TYPE_NONE;
	} else if(purple_strequal(svalue, "socks4")) {
		type = PURPLE_PROXY_TYPE_SOCKS4;
	} else if(purple_strequal(svalue, "socks5")) {
		type = PURPLE_PROXY_TYPE_SOCKS5;
	} else if(purple_strequal(svalue, "tor")) {
		type = PURPLE_PROXY_TYPE_TOR;
	} else if(purple_strequal(svalue, "http")) {
		type = PURPLE_PROXY_TYPE_HTTP;
	} else if(purple_strequal(svalue, "envvar")) {
		type = PURPLE_PROXY_TYPE_USE_ENVVAR;
	}
	purple_proxy_info_set_proxy_type(info, type);

	svalue = gtk_editable_get_text(GTK_EDITABLE(editor->proxy_host));
	purple_proxy_info_set_hostname(info, svalue);

	ivalue = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(editor->proxy_port));
	purple_proxy_info_set_port(info, ivalue);

	svalue = gtk_editable_get_text(GTK_EDITABLE(editor->proxy_username));
	purple_proxy_info_set_username(info, svalue);

	svalue = gtk_editable_get_text(GTK_EDITABLE(editor->proxy_password));
	purple_proxy_info_set_password(info, svalue);
}

static void
pidgin_account_editor_save_account(PidginAccountEditor *editor) {
	gboolean new_account = FALSE;

	new_account = pidgin_account_editor_save_login_options(editor);
	pidgin_account_editor_save_user_options(editor);
	pidgin_account_editor_save_advanced_options(editor);
	pidgin_account_editor_save_proxy(editor, new_account);

	/* If this is a new account, add it to the account manager and bring it
	 * online.
	 */
	if(new_account) {
		PurpleAccountManager *manager = NULL;
		const PurpleSavedStatus *saved_status;

		manager = purple_account_manager_get_default();

		purple_account_manager_add(manager, editor->account);

		saved_status = purple_savedstatus_get_current();
		if (saved_status != NULL) {
			purple_savedstatus_activate_for_account(saved_status,
			                                        editor->account);
			purple_account_set_enabled(editor->account, TRUE);
		}
	}
}

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
pidgin_account_editor_connection_changed_cb(G_GNUC_UNUSED GObject *obj,
                                            G_GNUC_UNUSED GParamSpec *pspec,
                                            gpointer data)
{
	PidginAccountEditor *editor = data;

	pidgin_account_editor_login_options_update_editable(editor);
}

static void
pidgin_account_editor_response_cb(GtkDialog *dialog, gint response_id,
                                  G_GNUC_UNUSED gpointer data)
{
	if(response_id == GTK_RESPONSE_APPLY) {
		pidgin_account_editor_save_account(PIDGIN_ACCOUNT_EDITOR(dialog));
	}

	gtk_window_destroy(GTK_WINDOW(dialog));
}

static void
pidgin_account_editor_protocol_changed_cb(G_GNUC_UNUSED GObject *obj,
                                          G_GNUC_UNUSED GParamSpec *pspec,
                                          gpointer data)
{
	pidgin_account_editor_update(data);
}

static void
pidgin_account_editor_username_changed_cb(GtkEditable *self, gpointer data) {
	PidginAccountEditor *editor = data;
	const gchar *text = gtk_editable_get_text(self);
	gboolean sensitive = FALSE;

	if(text != NULL && *text != '\0') {
		sensitive = TRUE;
	}

	gtk_dialog_set_response_sensitive(GTK_DIALOG(editor), GTK_RESPONSE_APPLY,
	                                  sensitive);
}

static void
pidgin_account_editor_avatar_response_cb(GtkNativeDialog *self,
                                         gint response_id, gpointer data)
{
	PidginAccountEditor *editor = data;

	if(response_id == GTK_RESPONSE_ACCEPT) {
		GError *error = NULL;
		GFile *file = NULL;
		gchar *filename = NULL;

		file = gtk_file_chooser_get_file(GTK_FILE_CHOOSER(self));
		filename = g_file_get_path(file);

		g_clear_object(&editor->avatar_pixbuf);
		editor->avatar_pixbuf = gdk_pixbuf_new_from_file(filename, &error);
		if(error != NULL) {
			g_warning("Failed to create pixbuf from file %s: %s", filename,
			          error->message);
			g_clear_error(&error);
		} else {
			gtk_image_set_from_pixbuf(GTK_IMAGE(editor->avatar),
			                          editor->avatar_pixbuf);
		}

		g_free(filename);
		g_object_unref(file);
	}

	g_clear_object(&editor->avatar_dialog);
}

static void
pidgin_account_editor_avatar_set_clicked_cb(G_GNUC_UNUSED GtkButton *self,
                                            gpointer data)
{
	PidginAccountEditor *editor = data;

	editor->avatar_dialog = gtk_file_chooser_native_new(_("Buddy Icon"),
	                                                    GTK_WINDOW(editor),
	                                                    GTK_FILE_CHOOSER_ACTION_OPEN,
	                                                    _("_Open"),
	                                                    _("_Cancel"));
	gtk_native_dialog_set_transient_for(GTK_NATIVE_DIALOG(editor->avatar_dialog),
	                                    GTK_WINDOW(editor));

	g_signal_connect(G_OBJECT(editor->avatar_dialog), "response",
	                 G_CALLBACK(pidgin_account_editor_avatar_response_cb),
	                 editor);

	gtk_native_dialog_show(GTK_NATIVE_DIALOG(editor->avatar_dialog));
}

static void
pidgin_account_editor_avatar_remove_clicked_cb(G_GNUC_UNUSED GtkButton *self,
                                               gpointer data)
{
	PidginAccountEditor *editor = data;

	gtk_image_set_from_icon_name(GTK_IMAGE(editor->avatar), "select-avatar");

	g_clear_object(&editor->avatar_pixbuf);
}

static gchar *
pidgin_account_editor_proxy_type_expression_cb(GObject *self,
                                               G_GNUC_UNUSED gpointer data)
{
	const gchar *text = "";
	const gchar *value = NULL;

	value = gtk_string_object_get_string(GTK_STRING_OBJECT(self));
	if(purple_strequal(value, "global")) {
		text = _("Use Global Proxy Settings");
	} else if(purple_strequal(value, "none")) {
		text = _("No proxy");
	} else if(purple_strequal(value, "socks4")) {
		text = _("SOCKS 4");
	} else if(purple_strequal(value, "socks5")) {
		text = _("SOCKS 5");
	} else if(purple_strequal(value, "tor")) {
		text = _("Tor/Privacy (SOCKS 5)");
	} else if(purple_strequal(value, "http")) {
		text = _("HTTP");
	} else if(purple_strequal(value, "envvar")) {
		text = _("Use Environmental Settings");
	}

	return g_strdup(text);
}

static void
pidgin_account_editor_proxy_type_changed_cb(G_GNUC_UNUSED GObject *obj,
                                            G_GNUC_UNUSED GParamSpec *pspec,
                                            gpointer data)
{
	PidginAccountEditor *editor = data;
	GObject *selected = NULL;
	gboolean show_options = TRUE;
	const gchar *value = NULL;

	selected = adw_combo_row_get_selected_item(ADW_COMBO_ROW(editor->proxy_type));
	value = gtk_string_object_get_string(GTK_STRING_OBJECT(selected));

	if(purple_strequal(value, "global") || purple_strequal(value, "none") ||
	   purple_strequal(value, "envvar"))
	{
		show_options = FALSE;
	}

	gtk_widget_set_visible(editor->proxy_options, show_options);
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_TYPE(PidginAccountEditor, pidgin_account_editor, GTK_TYPE_DIALOG)

static void
pidgin_account_editor_get_property(GObject *obj, guint param_id, GValue *value,
                                   GParamSpec *pspec)
{
	PidginAccountEditor *editor = PIDGIN_ACCOUNT_EDITOR(obj);

	switch(param_id) {
		case PROP_ACCOUNT:
			g_value_set_object(value,
			                   pidgin_account_editor_get_account(editor));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
pidgin_account_editor_set_property(GObject *obj, guint param_id,
                                   const GValue *value, GParamSpec *pspec)
{
	PidginAccountEditor *editor = PIDGIN_ACCOUNT_EDITOR(obj);

	switch(param_id) {
		case PROP_ACCOUNT:
			pidgin_account_editor_set_account(editor,
			                                  g_value_get_object(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
pidgin_account_editor_dispose(GObject *obj) {
	PidginAccountEditor *editor = PIDGIN_ACCOUNT_EDITOR(obj);

	g_clear_object(&editor->account);
	g_clear_object(&editor->avatar_dialog);
	g_clear_object(&editor->avatar_pixbuf);

	G_OBJECT_CLASS(pidgin_account_editor_parent_class)->dispose(obj);
}

static void
pidgin_account_editor_init(PidginAccountEditor *editor) {
	GtkCssProvider *css_provider = NULL;
	GtkStyleContext *context = NULL;
	GtkWidget *widget = GTK_WIDGET(editor);

	gtk_widget_init_template(widget);

	css_provider = gtk_css_provider_new();
	gtk_css_provider_load_from_resource(css_provider,
	                                    "/im/pidgin/Pidgin3/Accounts/entry.css");

	context = gtk_widget_get_style_context(GTK_WIDGET(editor));
	gtk_style_context_add_provider(context,
	                               GTK_STYLE_PROVIDER(css_provider),
	                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	g_clear_object(&css_provider);

	pidgin_account_editor_proxy_type_changed_cb(NULL, NULL, editor);
}

static void
pidgin_account_editor_constructed(GObject *obj) {
	PidginAccountEditor *editor = PIDGIN_ACCOUNT_EDITOR(obj);

	G_OBJECT_CLASS(pidgin_account_editor_parent_class)->constructed(obj);

	if(PURPLE_IS_ACCOUNT(editor->account)) {
		pidgin_protocol_chooser_set_protocol(PIDGIN_PROTOCOL_CHOOSER(editor->protocol),
		                                     purple_account_get_protocol(editor->account));
	}

	pidgin_account_editor_update(editor);
	pidgin_account_editor_login_options_update_editable(editor);
}

static void
pidgin_account_editor_finalize(GObject *obj) {
	PidginAccountEditor *editor = PIDGIN_ACCOUNT_EDITOR(obj);

	g_clear_pointer(&editor->user_split_entries, g_list_free);
	g_clear_pointer(&editor->user_split_rows, g_list_free);

	g_clear_pointer(&editor->advanced_entries, g_list_free);
	g_clear_pointer(&editor->advanced_rows, g_list_free);

	G_OBJECT_CLASS(pidgin_account_editor_parent_class)->finalize(obj);
}

static void
pidgin_account_editor_class_init(PidginAccountEditorClass *klass) {
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->get_property = pidgin_account_editor_get_property;
	obj_class->set_property = pidgin_account_editor_set_property;
	obj_class->constructed = pidgin_account_editor_constructed;
	obj_class->dispose = pidgin_account_editor_dispose;
	obj_class->finalize = pidgin_account_editor_finalize;

	/**
	 * PidginAccountEditor::account:
	 *
	 * The account that this editor is modifying.
	 *
	 * Since: 3.0.0
	 */
	properties[PROP_ACCOUNT] = g_param_spec_object(
		"account", "account",
		"The account to modify",
		PURPLE_TYPE_ACCOUNT,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, N_PROPERTIES, properties);

	gtk_widget_class_set_template_from_resource(widget_class,
	                                            "/im/pidgin/Pidgin3/Accounts/editor.ui");

	/* Dialog */
	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_account_editor_response_cb);

	/* Login Options */
	gtk_widget_class_bind_template_child(widget_class, PidginAccountEditor,
	                                     login_options);
	gtk_widget_class_bind_template_child(widget_class, PidginAccountEditor,
	                                     protocol);
	gtk_widget_class_bind_template_child(widget_class, PidginAccountEditor,
	                                     username);

	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_account_editor_protocol_changed_cb);
	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_account_editor_username_changed_cb);

	/* User Options */
	gtk_widget_class_bind_template_child(widget_class, PidginAccountEditor,
	                                     alias);
	gtk_widget_class_bind_template_child(widget_class, PidginAccountEditor,
	                                     avatar_row);
	gtk_widget_class_bind_template_child(widget_class, PidginAccountEditor,
	                                     use_custom_avatar);
	gtk_widget_class_bind_template_child(widget_class, PidginAccountEditor,
	                                     avatar);

	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_account_editor_avatar_set_clicked_cb);
	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_account_editor_avatar_remove_clicked_cb);

	/* Advanced Options */
	gtk_widget_class_bind_template_child(widget_class, PidginAccountEditor,
	                                     advanced_group);
	gtk_widget_class_bind_template_child(widget_class, PidginAccountEditor,
	                                     advanced_toggle);

	/* Proxy Options */
	gtk_widget_class_bind_template_child(widget_class, PidginAccountEditor,
	                                     proxy_type);
	gtk_widget_class_bind_template_child(widget_class, PidginAccountEditor,
	                                     proxy_options);
	gtk_widget_class_bind_template_child(widget_class, PidginAccountEditor,
	                                     proxy_host);
	gtk_widget_class_bind_template_child(widget_class, PidginAccountEditor,
	                                     proxy_port);
	gtk_widget_class_bind_template_child(widget_class, PidginAccountEditor,
	                                     proxy_username);
	gtk_widget_class_bind_template_child(widget_class, PidginAccountEditor,
	                                     proxy_password);

	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_account_editor_proxy_type_expression_cb);
	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_account_editor_proxy_type_changed_cb);
}

/******************************************************************************
 * API
 *****************************************************************************/
GtkWidget *
pidgin_account_editor_new(PurpleAccount *account) {
	return g_object_new(PIDGIN_TYPE_ACCOUNT_EDITOR, "account", account, NULL);
}

PurpleAccount *
pidgin_account_editor_get_account(PidginAccountEditor *editor) {
	g_return_val_if_fail(PIDGIN_IS_ACCOUNT_EDITOR(editor), NULL);

	return editor->account;
}
