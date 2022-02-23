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
 *
 */
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include <purple.h>

#include "gtkaccount.h"
#include "gtkblist.h"
#include "gtkconv.h"
#include "gtkdialogs.h"
#include "gtkxfer.h"
#include "gtkprivacy.h"
#include "gtkroomlist.h"
#include "gtkutils.h"
#include "pidgin/pidginaccountchooser.h"
#include "pidgin/pidginaccountfilterconnected.h"
#include "pidgin/pidginaccountstore.h"
#include "pidgin/pidginactiongroup.h"
#include "pidgin/pidginaddbuddydialog.h"
#include "pidgin/pidginaddchatdialog.h"
#include "pidgin/pidgincontactlistwindow.h"
#include "pidgin/pidgincore.h"
#include "pidgin/pidgindebug.h"
#include "pidgin/pidginmooddialog.h"
#include "pidgin/pidginplugininfo.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#define UI_DATA "ui-pidgin"

typedef struct
{
	PurpleAccount *account;
	GtkWidget *window;
	GtkBox *vbox;
	GtkWidget *account_menu;
	GtkSizeGroup *sg;
} PidginBlistRequestData;

typedef struct
{
	PidginBlistRequestData rq_data;
	gchar *default_chat_name;
	GList *entries;
} PidginChatData;

typedef struct
{
	PidginChatData chat_data;

	GtkWidget *alias_entry;
	GtkWidget *group_combo;
	GtkWidget *autojoin;
	GtkWidget *persistent;
} PidginAddChatData;

G_DEFINE_TYPE(PidginBuddyList, pidgin_buddy_list, PURPLE_TYPE_BUDDY_LIST)

enum {
	STATUS_ICON_COLUMN,
	STATUS_ICON_VISIBLE_COLUMN,
	NAME_COLUMN,
	BUDDY_ICON_COLUMN,
	NODE_COLUMN,
	EMBLEM_COLUMN,
	EMBLEM_VISIBLE_COLUMN,
	PROTOCOL_ICON_COLUMN,
	BLIST_COLUMNS
};

static gboolean editing_blist = FALSE;

static GList *pidgin_blist_sort_methods = NULL;
static struct _PidginBlistSortMethod *current_sort_method = NULL;
static void sort_method_none(PurpleBlistNode *node, PurpleBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter);

static void sort_method_alphabetical(PurpleBlistNode *node, PurpleBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter);
static void sort_method_status(PurpleBlistNode *node, PurpleBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter);

static PidginBuddyList *gtkblist = NULL;

static void pidgin_blist_update_buddy(PurpleBuddyList *list, PurpleBlistNode *node, gboolean status_change);
static void pidgin_blist_selection_changed(GtkTreeSelection *selection, gpointer data);
static void pidgin_blist_update(PurpleBuddyList *list, PurpleBlistNode *node);
static void pidgin_blist_update_group(PurpleBuddyList *list, PurpleBlistNode *node);
static void pidgin_blist_update_contact(PurpleBuddyList *list, PurpleBlistNode *node);
static char *pidgin_get_tooltip_text(PurpleBlistNode *node, gboolean full);
static gboolean get_iter_from_node(PurpleBlistNode *node, GtkTreeIter *iter);
static gboolean buddy_is_displayable(PurpleBuddy *buddy);
static void redo_buddy_list(PurpleBuddyList *list, gboolean remove, gboolean rerender);
static void pidgin_blist_collapse_contact_cb(GtkWidget *w, PurpleBlistNode *node);
static char *pidgin_get_group_title(PurpleBlistNode *gnode, gboolean expanded);
static void pidgin_blist_expand_contact_cb(GtkWidget *w, PurpleBlistNode *node);

typedef struct {
	GtkTreeRowReference *row;
	gboolean contact_expanded;
	gboolean recent_signonoff;
	gint recent_signonoff_timer;
	PurpleConversation *conv;
} PidginBlistNode;

/***************************************************
 *              Callbacks                          *
 ***************************************************/
static void
set_node_custom_icon_cb(GtkWidget *widget, gint response, gpointer data)
{
	if (response == GTK_RESPONSE_ACCEPT) {
		PurpleBlistNode *node = (PurpleBlistNode*)data;
		gchar *filename = NULL;

		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(widget));
		purple_buddy_icons_node_set_custom_icon_from_file(node, filename);
		g_free(filename);
	}

	g_object_set_data(G_OBJECT(data), "buddy-icon-chooser", NULL);
}

static void
chat_components_edit_ok(PurpleChat *chat, PurpleRequestFields *allfields)
{
	GList *groups, *fields;

	for (groups = purple_request_fields_get_groups(allfields); groups; groups = groups->next) {
		fields = purple_request_field_group_get_fields(groups->data);
		for (; fields; fields = fields->next) {
			PurpleRequestField *field = fields->data;
			const char *id;
			char *val;

			id = purple_request_field_get_id(field);
			if (purple_request_field_get_field_type(field) == PURPLE_REQUEST_FIELD_INTEGER)
				val = g_strdup_printf("%d", purple_request_field_int_get_value(field));
			else
				val = g_strdup(purple_request_field_string_get_value(field));

			if (!val) {
				g_hash_table_remove(purple_chat_get_components(chat), id);
			} else {
				g_hash_table_replace(purple_chat_get_components(chat), g_strdup(id), val);  /* val should not be free'd */
			}
		}
	}
}

/******************************************************************************
 * Helpers
 *****************************************************************************/
static PurpleBuddy *
pidgin_blist_get_selected_buddy(PidginBuddyList *blist) {
	PurpleBuddy *buddy = NULL;

	if(PURPLE_IS_CONTACT(blist->selected_node)) {
		buddy = purple_contact_get_priority_buddy(PURPLE_CONTACT(blist->selected_node));
	} else if(PURPLE_IS_BUDDY(blist->selected_node)) {
		buddy = PURPLE_BUDDY(blist->selected_node);
	}

	return buddy;
}

static void gtk_blist_join_chat(PurpleChat *chat)
{
	PurpleAccount *account;
	PurpleConversation *conv;
	PurpleConversationManager *manager;
	PurpleProtocol *protocol;
	GHashTable *components;
	const char *name;
	char *chat_name = NULL;

	account = purple_chat_get_account(chat);
	protocol = purple_account_get_protocol(account);

	components = purple_chat_get_components(chat);

	if(PURPLE_IS_PROTOCOL_CHAT(protocol)) {
		chat_name = purple_protocol_chat_get_name(PURPLE_PROTOCOL_CHAT(protocol),
		                                          components);
	}

	if(chat_name != NULL) {
		name = chat_name;
	} else {
		name = purple_chat_get_name(chat);
	}

	manager = purple_conversation_manager_get_default();
	conv = purple_conversation_manager_find(manager, account, name);

	if(PURPLE_IS_CONVERSATION(conv)) {
		pidgin_conv_attach_to_conversation(conv);
		purple_conversation_present(conv);
	}

	purple_serv_join_chat(purple_account_get_connection(account), components);
	g_free(chat_name);
}

static void
pidgin_blist_toggle_action(GSimpleAction *action,
                           G_GNUC_UNUSED GVariant *parameter,
                           G_GNUC_UNUSED gpointer data)
{
	GVariant *state = NULL;


	state = g_action_get_state(G_ACTION(action));
	g_action_change_state(
		G_ACTION(action),
		g_variant_new_boolean(!g_variant_get_boolean(state))
	);
	g_variant_unref(state);
}

/******************************************************************************
 * Actions
 *****************************************************************************/
static void
pidgin_blist_add_buddy_cb(G_GNUC_UNUSED GSimpleAction *action,
                          G_GNUC_UNUSED GVariant *parameter,
                          gpointer data)
{
	PidginBuddyList *blist = data;
	GtkTreeSelection *sel = NULL;
	GtkTreeIter iter;
	PurpleBlistNode *node;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(blist->treeview));

	if(gtk_tree_selection_get_selected(sel, NULL, &iter)){
		gtk_tree_model_get(GTK_TREE_MODEL(blist->treemodel), &iter, NODE_COLUMN, &node, -1);
		if (PURPLE_IS_BUDDY(node)) {
			PurpleGroup *group = purple_buddy_get_group(PURPLE_BUDDY(node));
			purple_blist_request_add_buddy(NULL, NULL, purple_group_get_name(group), NULL);
		} else if (PURPLE_IS_CONTACT(node) || PURPLE_IS_CHAT(node)) {
			PurpleGroup *group = purple_contact_get_group(PURPLE_CONTACT(node));
			purple_blist_request_add_buddy(NULL, NULL, purple_group_get_name(group), NULL);
		} else if (PURPLE_IS_GROUP(node)) {
			purple_blist_request_add_buddy(NULL, NULL, purple_group_get_name(PURPLE_GROUP(node)), NULL);
		}
	} else {
		purple_blist_request_add_buddy(NULL, NULL, NULL, NULL);
	}
}

static void
pidgin_blist_add_chat_cb(G_GNUC_UNUSED GSimpleAction *action,
                         G_GNUC_UNUSED GVariant *parameter,
                         gpointer data)
{
	PidginBuddyList *blist = data;
	GtkTreeSelection *sel = NULL;
	GtkTreeIter iter;
	PurpleBlistNode *node;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(blist->treeview));

	if(gtk_tree_selection_get_selected(sel, NULL, &iter)){
		gtk_tree_model_get(GTK_TREE_MODEL(blist->treemodel), &iter, NODE_COLUMN, &node, -1);
		if (PURPLE_IS_BUDDY(node))
			purple_blist_request_add_chat(NULL, purple_buddy_get_group(PURPLE_BUDDY(node)), NULL, NULL);
		if (PURPLE_IS_CONTACT(node) || PURPLE_IS_CHAT(node))
			purple_blist_request_add_chat(NULL, purple_contact_get_group(PURPLE_CONTACT(node)), NULL, NULL);
		else if (PURPLE_IS_GROUP(node))
			purple_blist_request_add_chat(NULL, (PurpleGroup*)node, NULL, NULL);
	} else {
		purple_blist_request_add_chat(NULL, NULL, NULL, NULL);
	}
}

static void
pidgin_blist_menu_alias_cb(G_GNUC_UNUSED GSimpleAction *action,
                           G_GNUC_UNUSED GVariant *parameter,
                           gpointer data)
{
	PidginBuddyList *blist = data;
	PurpleBlistNode *node = blist->selected_node;
	GtkTreeIter iter;
	GtkTreePath *path;

	if (!(get_iter_from_node(node, &iter))) {
		/* This is either a bug, or the buddy is in a collapsed contact */
		node = purple_blist_node_get_parent(node);
		if (!get_iter_from_node(node, &iter))
			/* Now it's definitely a bug */
			return;
	}

	gtk_widget_trigger_tooltip_query(blist->treeview);

	path = gtk_tree_model_get_path(GTK_TREE_MODEL(blist->treemodel), &iter);
	g_object_set(G_OBJECT(blist->text_rend), "editable", TRUE, NULL);
	gtk_tree_view_set_enable_search (GTK_TREE_VIEW(blist->treeview), FALSE);
	gtk_widget_grab_focus(blist->treeview);
	gtk_tree_view_set_cursor_on_cell(GTK_TREE_VIEW(blist->treeview), path,
	                                 blist->text_column, blist->text_rend,
	                                 TRUE);
	gtk_tree_path_free(path);
}

static void
pidgin_blist_menu_autojoin_cb(GSimpleAction *action, GVariant *state,
                              gpointer data)
{
	PidginBuddyList *blist = data;

	purple_blist_node_set_bool(blist->selected_node, "gtk-autojoin",
	                           g_variant_get_boolean(state));

	g_simple_action_set_state(action, state);
}

static void
pidgin_blist_menu_audio_call_cb(G_GNUC_UNUSED GSimpleAction *action,
                                G_GNUC_UNUSED GVariant *parameter,
                                gpointer data)
{
	PidginBuddyList *blist = data;
	PurpleBuddy *buddy = pidgin_blist_get_selected_buddy(blist);

	purple_protocol_initiate_media(purple_buddy_get_account(buddy),
	                               purple_buddy_get_name(buddy),
	                               PURPLE_MEDIA_AUDIO);
}

static void
pidgin_blist_menu_block_cb(GSimpleAction *action, GVariant *state,
                           gpointer data)
{
	PidginBuddyList *blist = data;
	PurpleBuddy *buddy;
	PurpleAccount *account;
	gboolean permitted;
	const char *name;

	buddy = pidgin_blist_get_selected_buddy(blist);

	if (!PURPLE_IS_BUDDY(buddy))
		return;

	account = purple_buddy_get_account(buddy);
	name = purple_buddy_get_name(buddy);

	permitted = purple_account_privacy_check(account, name);

	/* XXX: Perhaps ask whether to restore the previous lists where appropirate? */

	if (permitted)
		purple_account_privacy_deny(account, name);
	else
		purple_account_privacy_allow(account, name);

	pidgin_blist_update(PURPLE_BUDDY_LIST(blist), PURPLE_BLIST_NODE(buddy));

	g_simple_action_set_state(action, state);
}

static void
pidgin_blist_menu_chat_settings_cb(G_GNUC_UNUSED GSimpleAction *action,
                                   G_GNUC_UNUSED GVariant *parameter,
                                   gpointer data)
{
	PidginBuddyList *blist = data;
	PurpleRequestFields *fields = purple_request_fields_new();
	PurpleRequestFieldGroup *group = purple_request_field_group_new(NULL);
	PurpleRequestField *field;
	GList *parts, *iter;
	PurpleProtocolChatEntry *pce;
	PurpleProtocol *protocol;
	PurpleConnection *gc;
	PurpleChat *chat = PURPLE_CHAT(blist->selected_node);

	purple_request_fields_add_group(fields, group);

	gc = purple_account_get_connection(purple_chat_get_account(chat));
	protocol = purple_connection_get_protocol(gc);
	parts = purple_protocol_chat_info(PURPLE_PROTOCOL_CHAT(protocol), gc);

	for (iter = parts; iter; iter = iter->next) {
		pce = iter->data;
		if (pce->is_int) {
			int val;
			const char *str = g_hash_table_lookup(purple_chat_get_components(chat), pce->identifier);
			if (!str || sscanf(str, "%d", &val) != 1)
				val = pce->min;
			field = purple_request_field_int_new(pce->identifier, pce->label, val, INT_MIN, INT_MAX);
		} else {
			field = purple_request_field_string_new(pce->identifier, pce->label,
					g_hash_table_lookup(purple_chat_get_components(chat), pce->identifier), FALSE);
			if (pce->secret)
				purple_request_field_string_set_masked(field, TRUE);
		}

		if (pce->required)
			purple_request_field_set_required(field, TRUE);

		purple_request_field_group_add_field(group, field);
		g_free(pce);
	}

	g_list_free(parts);

	purple_request_fields(NULL, _("Edit Chat"), NULL, _("Please update the necessary fields."),
			fields, _("Save"), G_CALLBACK(chat_components_edit_ok), _("Cancel"), NULL,
			NULL, chat);
}

static void
pidgin_blist_menu_im_cb(G_GNUC_UNUSED GSimpleAction *action,
                        G_GNUC_UNUSED GVariant *parameter,
                        gpointer data)
{
	PidginBuddyList *blist = data;
	PurpleBuddy *buddy = pidgin_blist_get_selected_buddy(blist);

	pidgin_dialogs_im_with_user(purple_buddy_get_account(buddy),
	                            purple_buddy_get_name(buddy));
}

static void
pidgin_blist_menu_info_cb(G_GNUC_UNUSED GSimpleAction *action,
                          G_GNUC_UNUSED GVariant *parameter,
                          gpointer data)
{
	PidginBuddyList *blist = data;
	PurpleBuddy *buddy = pidgin_blist_get_selected_buddy(blist);
	PurpleAccount *account = purple_buddy_get_account(buddy);

	pidgin_retrieve_user_info(purple_account_get_connection(account),
	                          purple_buddy_get_name(buddy));
}

static void
pidgin_blist_menu_join_cb(G_GNUC_UNUSED GSimpleAction *action,
                          G_GNUC_UNUSED GVariant *parameter,
                          gpointer data)
{
	PidginBuddyList *blist = data;

	gtk_blist_join_chat(PURPLE_CHAT(blist->selected_node));
}

static void
pidgin_blist_menu_persistent_cb(GSimpleAction *action, GVariant *state,
                                gpointer data)
{
	PidginBuddyList *blist = data;

	purple_blist_node_set_bool(blist->selected_node, "gtk-persistent",
	                           g_variant_get_boolean(state));

	g_simple_action_set_state(action, state);
}

static void
pidgin_blist_menu_send_file_cb(G_GNUC_UNUSED GSimpleAction *action,
                               G_GNUC_UNUSED GVariant *parameter,
                               gpointer data)
{
	PidginBuddyList *blist = data;
	PurpleBuddy *buddy = pidgin_blist_get_selected_buddy(blist);
	PurpleAccount *account = purple_buddy_get_account(buddy);

	purple_serv_send_file(purple_account_get_connection(account),
	                      purple_buddy_get_name(buddy), NULL);
}

static void
pidgin_blist_remove_cb(G_GNUC_UNUSED GSimpleAction *action,
                       G_GNUC_UNUSED GVariant *parameter,
                       gpointer data)
{
	PidginBuddyList *blist = data;
	PurpleBlistNode *node = blist->selected_node;

	if(PURPLE_IS_BUDDY(node)) {
		pidgin_dialogs_remove_buddy(PURPLE_BUDDY(node));
	} else if(PURPLE_IS_CHAT(node)) {
		pidgin_dialogs_remove_chat(PURPLE_CHAT(node));
	} else if(PURPLE_IS_GROUP(node)) {
		pidgin_dialogs_remove_group(PURPLE_GROUP(node));
	} else if(PURPLE_IS_CONTACT(node)) {
		pidgin_dialogs_remove_contact(PURPLE_CONTACT(node));
	}
}

static void
pidgin_blist_remove_node_custom_icon(G_GNUC_UNUSED GSimpleAction *action,
                                     G_GNUC_UNUSED GVariant *parameter,
                                     gpointer data)
{
	PidginBuddyList *blist = data;

	purple_buddy_icons_node_set_custom_icon(blist->selected_node, NULL, 0);
}

static void
pidgin_blist_set_node_custom_icon(G_GNUC_UNUSED GSimpleAction *action,
                                  G_GNUC_UNUSED GVariant *parameter,
                                  gpointer data)
{
	PidginBuddyList *blist = data;
	PurpleBlistNode *node = blist->selected_node;
	GtkFileChooserNative *win = NULL;

	win = g_object_get_data(G_OBJECT(node), "buddy-icon-chooser");
	if(win == NULL) {
		win = gtk_file_chooser_native_new(_("Buddy Icon"), NULL,
		                                  GTK_FILE_CHOOSER_ACTION_OPEN,
		                                  _("_Open"), _("_Cancel"));

		g_signal_connect(win, "response", G_CALLBACK(set_node_custom_icon_cb),
		                 node);

		g_object_set_data_full(G_OBJECT(node), "buddy-icon-chooser", win,
		                       g_object_unref);
	}

	gtk_native_dialog_show(GTK_NATIVE_DIALOG(win));
}

static void
pidgin_blist_menu_video_call_cb(G_GNUC_UNUSED GSimpleAction *action,
                                G_GNUC_UNUSED GVariant *parameter,
                                gpointer data)
{
	PurpleAccount *account = NULL;
	PidginBuddyList *blist = data;
	PurpleBuddy *buddy = pidgin_blist_get_selected_buddy(blist);
	PurpleMediaCaps caps = 0;
	const gchar *buddy_name = NULL;

	account = purple_buddy_get_account(buddy);
	buddy_name = purple_buddy_get_name(buddy);

	/* if the buddy supports both audio and video, start a combined call,
	 otherwise start a pure video session */
	caps = purple_protocol_get_media_caps(account, buddy_name);

	if(caps & PURPLE_MEDIA_CAPS_AUDIO_VIDEO) {
		purple_protocol_initiate_media(account, buddy_name,
		                               PURPLE_MEDIA_AUDIO | PURPLE_MEDIA_VIDEO);
	} else {
		purple_protocol_initiate_media(account, buddy_name, PURPLE_MEDIA_VIDEO);
	}
}

static GActionEntry menu_actions[] = {
	{
		.name = "add-buddy",
		.activate = pidgin_blist_add_buddy_cb,
	}, {
		.name = "add-chat",
		.activate = pidgin_blist_add_chat_cb,
	}, {
		.name = "alias",
		.activate = pidgin_blist_menu_alias_cb,
	}, {
		.name = "auto-join-chat",
		.activate = pidgin_blist_toggle_action,
		.state = "false",
		.change_state = pidgin_blist_menu_autojoin_cb,
	}, {
		.name = "buddy-audio-call",
		.activate = pidgin_blist_menu_audio_call_cb,
	}, {
		.name = "buddy-block",
		.activate = pidgin_blist_toggle_action,
		.state = "false",
		.change_state = pidgin_blist_menu_block_cb,
	}, {
		.name = "buddy-get-info",
		.activate = pidgin_blist_menu_info_cb,
	}, {
		.name = "buddy-im",
		.activate = pidgin_blist_menu_im_cb,
	}, {
		.name = "buddy-send-file",
		.activate = pidgin_blist_menu_send_file_cb,
	}, {
		.name = "buddy-video-call",
		.activate = pidgin_blist_menu_video_call_cb,
	}, {
		.name = "chat-settings",
		.activate = pidgin_blist_menu_chat_settings_cb,
	}, {
		.name = "join-chat",
		.activate = pidgin_blist_menu_join_cb,
	}, {
		.name = "persistent-chat",
		.activate = pidgin_blist_toggle_action,
		.state = "false",
		.change_state = pidgin_blist_menu_persistent_cb,
	}, {
		.name = "remove",
		.activate = pidgin_blist_remove_cb,
	}, {
		.name = "remove-custom-icon",
		.activate = pidgin_blist_remove_node_custom_icon,
	}, {
		.name = "set-custom-icon",
		.activate = pidgin_blist_set_node_custom_icon,
	}
};

static void gtk_blist_renderer_editing_cancelled_cb(GtkCellRenderer *renderer, PurpleBuddyList *list)
{
	editing_blist = FALSE;
	g_object_set(G_OBJECT(renderer), "editable", FALSE, NULL);
	pidgin_blist_refresh(list);
}

static void gtk_blist_renderer_editing_started_cb(GtkCellRenderer *renderer,
		GtkCellEditable *editable,
		gchar *path_str,
		gpointer user_data)
{
	PidginBuddyList *gtkblist = PIDGIN_BUDDY_LIST(user_data);
	GtkTreeIter iter;
	GtkTreePath *path = NULL;
	PurpleBlistNode *node;
	const char *text = NULL;

	path = gtk_tree_path_new_from_string (path_str);
	gtk_tree_model_get_iter (GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);
	gtk_tree_path_free (path);
	gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);

	if (PURPLE_IS_CONTACT(node))
		text = purple_contact_get_alias(PURPLE_CONTACT(node));
	else if (PURPLE_IS_BUDDY(node))
		text = purple_buddy_get_alias(PURPLE_BUDDY(node));
	else if (PURPLE_IS_GROUP(node))
		text = purple_group_get_name(PURPLE_GROUP(node));
	else if (PURPLE_IS_CHAT(node))
		text = purple_chat_get_name(PURPLE_CHAT(node));
	else
		g_return_if_reached();

	if (GTK_IS_ENTRY (editable)) {
		GtkEntry *entry = GTK_ENTRY (editable);
		gtk_entry_set_text(entry, text);
	}
	editing_blist = TRUE;
}

static void
gtk_blist_do_personize(GList *merges)
{
	PurpleBlistNode *contact = NULL;
	int max = 0;
	GList *tmp;

	/* First, we find the contact to merge the rest of the buddies into.
	 * This will be the contact with the most buddies in it; ties are broken
	 * by which contact is higher in the list
	 */
	for (tmp = merges; tmp; tmp = tmp->next) {
		PurpleBlistNode *node = tmp->data;
		PurpleBlistNode *b;
		int i = 0;

		if (PURPLE_IS_BUDDY(node))
			node = purple_blist_node_get_parent(node);

		if (!PURPLE_IS_CONTACT(node))
			continue;

		for (b = purple_blist_node_get_first_child(node);
		     b;
		     b = purple_blist_node_get_sibling_next(b))
		{
			i++;
		}

		if (i > max) {
			contact = node;
			max = i;
		}
	}

	if (contact == NULL)
		return;

	/* Merge all those buddies into this contact */
	for (tmp = merges; tmp; tmp = tmp->next) {
		PurpleBlistNode *node = tmp->data;
		if (PURPLE_IS_BUDDY(node))
			node = purple_blist_node_get_parent(node);

		if (node == contact)
			continue;

		purple_contact_merge((PurpleContact *)node, contact);
	}

	/* And show the expanded contact, so the people know what's going on */
	pidgin_blist_expand_contact_cb(NULL, contact);
	g_list_free(merges);
}

static void
gtk_blist_auto_personize(PurpleBlistNode *group, const char *alias)
{
	PurpleBlistNode *contact;
	PurpleBlistNode *buddy;
	GList *merges = NULL;
	int i = 0;
	char *a = g_utf8_casefold(alias, -1);

	for (contact = purple_blist_node_get_first_child(group);
	     contact != NULL;
	     contact = purple_blist_node_get_sibling_next(contact)) {
		char *node_alias;
		if (!PURPLE_IS_CONTACT(contact))
			continue;

		node_alias = g_utf8_casefold(purple_contact_get_alias((PurpleContact *)contact), -1);
		if (node_alias && !g_utf8_collate(node_alias, a)) {
			merges = g_list_append(merges, contact);
			i++;
			g_free(node_alias);
			continue;
		}
		g_free(node_alias);

		for (buddy = purple_blist_node_get_first_child(contact);
		     buddy;
		     buddy = purple_blist_node_get_sibling_next(buddy))
		{
			if (!PURPLE_IS_BUDDY(buddy))
				continue;

			node_alias = g_utf8_casefold(purple_buddy_get_alias(PURPLE_BUDDY(buddy)), -1);
			if (node_alias && !g_utf8_collate(node_alias, a)) {
				merges = g_list_append(merges, buddy);
				i++;
				g_free(node_alias);
				break;
			}
			g_free(node_alias);
		}
	}
	g_free(a);

	if (i > 1)
	{
		char *msg = g_strdup_printf(ngettext("You have %d contact named %s. Would you like to merge them?", "You currently have %d contacts named %s. Would you like to merge them?", i), i, alias);
		purple_request_action(NULL, NULL, msg, _("Merging these contacts will cause them to share a single entry on the buddy list and use a single conversation window. "
							 "You can separate them again by choosing 'Expand' from the contact's context menu"), 0, NULL,
				      merges, 2, _("_Yes"), G_CALLBACK(gtk_blist_do_personize), _("_No"), G_CALLBACK(g_list_free));
		g_free(msg);
	} else
		g_list_free(merges);
}

static void gtk_blist_renderer_edited_cb(GtkCellRendererText *text_rend, char *arg1,
					 char *arg2, PurpleBuddyList *list)
{
	PidginBuddyList *gtkblist = PIDGIN_BUDDY_LIST(list);
	GtkTreeIter iter;
	GtkTreePath *path;
	PurpleBlistNode *node;
	PurpleGroup *dest;
	gchar *alias = NULL;

	editing_blist = FALSE;
	path = gtk_tree_path_new_from_string (arg1);
	gtk_tree_model_get_iter (GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);
	gtk_tree_path_free (path);
	gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);
	gtk_tree_view_set_enable_search (GTK_TREE_VIEW(gtkblist->treeview), TRUE);
	g_object_set(G_OBJECT(gtkblist->text_rend), "editable", FALSE, NULL);

	if (PURPLE_IS_CONTACT(node)) {
		PurpleContact *contact = PURPLE_CONTACT(node);
		PidginBlistNode *gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);

		/*
		 * Using purple_contact_get_alias here breaks because we
		 * specifically want to check the contact alias only (i.e. not
		 * the priority buddy, which purple_contact_get_alias does).
		 * The "alias" GObject property gives us just the alias.
		 */
		g_object_get(contact, "alias", &alias, NULL);

		if (alias || gtknode->contact_expanded) {
			purple_contact_set_alias(contact, arg2);
			gtk_blist_auto_personize(purple_blist_node_get_parent(node), arg2);
		} else {
			PurpleBuddy *buddy = purple_contact_get_priority_buddy(contact);
			purple_buddy_set_local_alias(buddy, arg2);
			purple_serv_alias_buddy(buddy);
			gtk_blist_auto_personize(purple_blist_node_get_parent(node), arg2);
		}
	} else if (PURPLE_IS_BUDDY(node)) {
		PurpleGroup *group = purple_buddy_get_group(PURPLE_BUDDY(node));

		purple_buddy_set_local_alias(PURPLE_BUDDY(node), arg2);
		purple_serv_alias_buddy(PURPLE_BUDDY(node));
		gtk_blist_auto_personize(PURPLE_BLIST_NODE(group), arg2);
	} else if (PURPLE_IS_GROUP(node)) {
		dest = purple_blist_find_group(arg2);
		if (dest != NULL && purple_utf8_strcasecmp(arg2, purple_group_get_name(PURPLE_GROUP(node)))) {
			pidgin_dialogs_merge_groups(PURPLE_GROUP(node), arg2);
		} else {
			purple_group_set_name(PURPLE_GROUP(node), arg2);
		}
	} else if (PURPLE_IS_CHAT(node)) {
		purple_chat_set_alias(PURPLE_CHAT(node), arg2);
	}

	g_free(alias);
	pidgin_blist_refresh(list);
}

static void
do_join_chat(PidginChatData *data)
{
	if (data)
	{
		GHashTable *components =
			g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
		GList *tmp;
		PurpleChat *chat;

		for (tmp = data->entries; tmp != NULL; tmp = tmp->next)
		{
			if (g_object_get_data(tmp->data, "is_spin"))
			{
				g_hash_table_replace(components,
					g_strdup(g_object_get_data(tmp->data, "identifier")),
					g_strdup_printf("%d",
							gtk_spin_button_get_value_as_int(tmp->data)));
			}
			else
			{
				g_hash_table_replace(components,
					g_strdup(g_object_get_data(tmp->data, "identifier")),
					g_strdup(gtk_entry_get_text(tmp->data)));
			}
		}

		chat = purple_chat_new(data->rq_data.account, NULL, components);
		gtk_blist_join_chat(chat);
		purple_blist_remove_chat(chat);
	}
}

static void
do_joinchat(GtkWidget *dialog, int id, PidginChatData *info)
{
	switch(id)
	{
		case GTK_RESPONSE_OK:
			do_join_chat(info);
			break;

		case 1:
			pidgin_roomlist_dialog_show_with_account(info->rq_data.account);
			return;

		break;
	}

	gtk_widget_destroy(GTK_WIDGET(dialog));
	g_list_free(info->entries);
	g_free(info);
}

/*
 * Check the values of all the text entry boxes.  If any required input
 * strings are empty then don't allow the user to click on "OK."
 */
static void
set_sensitive_if_input_chat_cb(GtkWidget *entry, gpointer user_data)
{
	PurpleProtocol *protocol;
	PurpleConnection *gc;
	PidginChatData *data;
	GList *tmp;
	const char *text;
	gboolean required;
	gboolean sensitive = TRUE;

	data = user_data;

	for (tmp = data->entries; tmp != NULL; tmp = tmp->next)
	{
		if (!g_object_get_data(tmp->data, "is_spin"))
		{
			required = GPOINTER_TO_INT(g_object_get_data(tmp->data, "required"));
			text = gtk_entry_get_text(tmp->data);
			if (required && (*text == '\0'))
				sensitive = FALSE;
		}
	}

	gtk_dialog_set_response_sensitive(GTK_DIALOG(data->rq_data.window), GTK_RESPONSE_OK, sensitive);

	gc = purple_account_get_connection(data->rq_data.account);
	protocol = (gc != NULL) ? purple_connection_get_protocol(gc) : NULL;
	sensitive = (protocol != NULL && PURPLE_PROTOCOL_IMPLEMENTS(protocol, ROOMLIST, get_list));

	gtk_dialog_set_response_sensitive(GTK_DIALOG(data->rq_data.window), 1, sensitive);
}

static void
pidgin_blist_update_privacy_cb(PurpleBuddy *buddy)
{
	PidginBlistNode *ui_data = g_object_get_data(G_OBJECT(buddy), UI_DATA);
	if (ui_data == NULL || ui_data->row == NULL)
		return;
	pidgin_blist_update_buddy(purple_blist_get_default(),
	                          PURPLE_BLIST_NODE(buddy), TRUE);
}

static gboolean
chat_account_filter_func(PurpleAccount *account)
{
	PurpleConnection *gc = purple_account_get_connection(account);
	PurpleProtocol *protocol = NULL;

	if (gc == NULL)
		return FALSE;

	protocol = purple_connection_get_protocol(gc);

	return (PURPLE_PROTOCOL_IMPLEMENTS(protocol, CHAT, info));
}

gboolean
pidgin_blist_joinchat_is_showable()
{
	GList *c;
	PurpleConnection *gc;

	for (c = purple_connections_get_all(); c != NULL; c = c->next) {
		gc = c->data;

		if (chat_account_filter_func(purple_connection_get_account(gc)))
			return TRUE;
	}

	return FALSE;
}

static GtkWidget *
make_blist_request_dialog(PidginBlistRequestData *data, PurpleAccount *account,
	const char *title, const char *window_role, const char *label_text,
	GCallback callback_func, PurpleFilterAccountFunc filter_func,
	GCallback response_cb)
{
	GtkWidget *label;
	GtkWidget *img;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWindow *blist_window;
	GtkTreeModel *model = NULL, *filter = NULL;
	PidginBuddyList *gtkblist;

	data->account = account;

	img = gtk_image_new_from_icon_name("dialog-question",
			GTK_ICON_SIZE_DIALOG);

	gtkblist = PIDGIN_BUDDY_LIST(purple_blist_get_default());
	blist_window = gtkblist ? GTK_WINDOW(gtkblist->window) : NULL;

	data->window = gtk_dialog_new();
	gtk_window_set_title(GTK_WINDOW(data->window), title);
	gtk_window_set_transient_for(GTK_WINDOW(data->window), blist_window);
	gtk_dialog_set_default_response(GTK_DIALOG(data->window), GTK_RESPONSE_OK);
	gtk_container_set_border_width(GTK_CONTAINER(data->window), 6);
	gtk_window_set_resizable(GTK_WINDOW(data->window), FALSE);
	gtk_box_set_spacing(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(data->window))),
	                    12);
	gtk_container_set_border_width(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(data->window))),
	                               6);
	gtk_window_set_role(GTK_WINDOW(data->window), window_role);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_container_add(GTK_CONTAINER(gtk_dialog_get_content_area(GTK_DIALOG(data->window))),
	                  hbox);
	gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
	gtk_widget_set_halign(img, GTK_ALIGN_START);
	gtk_widget_set_valign(img, GTK_ALIGN_START);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
	gtk_container_add(GTK_CONTAINER(hbox), vbox);

	label = gtk_label_new(label_text);

	gtk_widget_set_size_request(label, 400, -1);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_label_set_xalign(GTK_LABEL(label), 0);
	gtk_label_set_yalign(GTK_LABEL(label), 0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

	data->sg = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	model = GTK_TREE_MODEL(pidgin_account_store_new());
	filter = pidgin_account_filter_connected_new(model, NULL);
	g_object_unref(G_OBJECT(model));
	data->account_menu = pidgin_account_chooser_new();
	gtk_combo_box_set_model(GTK_COMBO_BOX(data->account_menu), filter);
	g_object_unref(G_OBJECT(filter));
	if(PURPLE_IS_ACCOUNT(account)) {
		pidgin_account_chooser_set_selected(PIDGIN_ACCOUNT_CHOOSER(
			data->account_menu), account);
	} else {
		gtk_combo_box_set_active(GTK_COMBO_BOX(data->account_menu), 0);
	}

	pidgin_account_chooser_set_filter_func(
	        PIDGIN_ACCOUNT_CHOOSER(data->account_menu), filter_func);
	pidgin_add_widget_to_vbox(GTK_BOX(vbox), _("A_ccount"), data->sg, data->account_menu, TRUE, NULL);
	g_signal_connect(data->account_menu, "changed",
	                 G_CALLBACK(callback_func), data);

	data->vbox = GTK_BOX(gtk_box_new(GTK_ORIENTATION_VERTICAL, 5));
	gtk_container_set_border_width(GTK_CONTAINER(data->vbox), 0);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(data->vbox), FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(data->window), "response", response_cb, data);

	g_object_unref(data->sg);

	return vbox;
}

static void
rebuild_chat_entries(PidginChatData *data, const char *default_chat_name)
{
	PurpleConnection *gc;
	PurpleProtocol *protocol;
	GList *list = NULL, *tmp;
	GHashTable *defaults = NULL;
	PurpleProtocolChatEntry *pce;
	gboolean focus = TRUE;

	g_return_if_fail(data->rq_data.account != NULL);

	gc = purple_account_get_connection(data->rq_data.account);
	protocol = purple_connection_get_protocol(gc);

	gtk_container_foreach(GTK_CONTAINER(data->rq_data.vbox), (GtkCallback)gtk_widget_destroy, NULL);

	g_list_free(data->entries);
	data->entries = NULL;

	list = purple_protocol_chat_info(PURPLE_PROTOCOL_CHAT(protocol), gc);
	defaults = purple_protocol_chat_info_defaults(PURPLE_PROTOCOL_CHAT(protocol),
	                                              gc, default_chat_name);

	for (tmp = list; tmp; tmp = tmp->next)
	{
		GtkWidget *input;

		pce = tmp->data;

		if (pce->is_int)
		{
			GtkAdjustment *adjust;
			adjust = GTK_ADJUSTMENT(gtk_adjustment_new(pce->min,
			                                           pce->min, pce->max,
			                                           1, 10, 10));
			input = gtk_spin_button_new(adjust, 1, 0);
			gtk_widget_set_size_request(input, 50, -1);
			pidgin_add_widget_to_vbox(GTK_BOX(data->rq_data.vbox), pce->label, data->rq_data.sg, input, FALSE, NULL);
		}
		else
		{
			char *value;
			input = gtk_entry_new();
			gtk_entry_set_activates_default(GTK_ENTRY(input), TRUE);
			value = g_hash_table_lookup(defaults, pce->identifier);
			if (value != NULL)
				gtk_entry_set_text(GTK_ENTRY(input), value);
			if (pce->secret)
			{
				gtk_entry_set_visibility(GTK_ENTRY(input), FALSE);
			}
			pidgin_add_widget_to_vbox(data->rq_data.vbox, pce->label, data->rq_data.sg, input, TRUE, NULL);
			g_signal_connect(G_OBJECT(input), "changed",
							 G_CALLBACK(set_sensitive_if_input_chat_cb), data);
		}

		/* Do the following for any type of input widget */
		if (focus)
		{
			gtk_widget_grab_focus(input);
			focus = FALSE;
		}
		g_object_set_data(G_OBJECT(input), "identifier", (gpointer)pce->identifier);
		g_object_set_data(G_OBJECT(input), "is_spin", GINT_TO_POINTER(pce->is_int));
		g_object_set_data(G_OBJECT(input), "required", GINT_TO_POINTER(pce->required));
		data->entries = g_list_append(data->entries, input);

		g_free(pce);
	}

	g_list_free(list);
	g_hash_table_destroy(defaults);

	/* Set whether the "OK" button should be clickable initially */
	set_sensitive_if_input_chat_cb(NULL, data);

	gtk_widget_show_all(GTK_WIDGET(data->rq_data.vbox));
}

static void
chat_select_account_cb(GObject *w, PidginChatData *data)
{
	PidginAccountChooser *chooser = PIDGIN_ACCOUNT_CHOOSER(w);
	PurpleAccount *account = pidgin_account_chooser_get_selected(chooser);

	g_return_if_fail(data != NULL);
	g_return_if_fail(account != NULL);

	if (purple_strequal(purple_account_get_protocol_id(data->rq_data.account),
	                    purple_account_get_protocol_id(account)))
	{
		data->rq_data.account = account;
	}
	else
	{
		data->rq_data.account = account;
		rebuild_chat_entries(data, data->default_chat_name);
	}
}

void
pidgin_blist_joinchat_show(void)
{
	PidginChatData *data = NULL;
	PidginAccountChooser *chooser = NULL;

	data = g_new0(PidginChatData, 1);

	make_blist_request_dialog((PidginBlistRequestData *)data, NULL,
		_("Join a Chat"), "join_chat",
		_("Please enter the appropriate information about the chat "
			"you would like to join.\n"),
		G_CALLBACK(chat_select_account_cb),
		chat_account_filter_func, (GCallback)do_joinchat);
	gtk_dialog_add_buttons(GTK_DIALOG(data->rq_data.window),
		_("Room _List"), 1,
		_("Cancel"), GTK_RESPONSE_CANCEL,
		_("_Join"), GTK_RESPONSE_OK, NULL);
	gtk_dialog_set_default_response(GTK_DIALOG(data->rq_data.window),
		GTK_RESPONSE_OK);
	data->default_chat_name = NULL;

	chooser = PIDGIN_ACCOUNT_CHOOSER(data->rq_data.account_menu);
	data->rq_data.account = pidgin_account_chooser_get_selected(chooser);

	rebuild_chat_entries(data, NULL);

	gtk_widget_show_all(data->rq_data.window);
}

static void gtk_blist_row_expanded_cb(GtkTreeView *tv, GtkTreeIter *iter, GtkTreePath *path, gpointer user_data)
{
	PidginBuddyList *gtkblist = PIDGIN_BUDDY_LIST(user_data);
	PurpleBlistNode *node;

	gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), iter, NODE_COLUMN, &node, -1);

	if (PURPLE_IS_GROUP(node)) {
		char *title;

		title = pidgin_get_group_title(node, TRUE);

		gtk_tree_store_set(gtkblist->treemodel, iter,
		   NAME_COLUMN, title,
		   -1);

		g_free(title);

		purple_blist_node_set_bool(node, "collapsed", FALSE);
		gtk_widget_trigger_tooltip_query(gtkblist->treeview);
	}
}

static void gtk_blist_row_collapsed_cb(GtkTreeView *tv, GtkTreeIter *iter, GtkTreePath *path, gpointer user_data)
{
	PidginBuddyList *gtkblist = PIDGIN_BUDDY_LIST(user_data);
	PurpleBlistNode *node;

	gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), iter, NODE_COLUMN, &node, -1);

	if (PURPLE_IS_GROUP(node)) {
		char *title;
		PidginBlistNode *gtknode;
		PurpleBlistNode *cnode;

		title = pidgin_get_group_title(node, FALSE);

		gtk_tree_store_set(gtkblist->treemodel, iter,
		   NAME_COLUMN, title,
		   -1);

		g_free(title);

		purple_blist_node_set_bool(node, "collapsed", TRUE);

		for(cnode = purple_blist_node_get_first_child(node); cnode; cnode = purple_blist_node_get_sibling_next(cnode)) {
			if (PURPLE_IS_CONTACT(cnode)) {
				gtknode = g_object_get_data(G_OBJECT(cnode), UI_DATA);
				if (!gtknode->contact_expanded)
					continue;
				gtknode->contact_expanded = FALSE;
				pidgin_blist_update_contact(NULL, cnode);
			}
		}
		gtk_widget_trigger_tooltip_query(gtkblist->treeview);
	} else if(PURPLE_IS_CONTACT(node)) {
		pidgin_blist_collapse_contact_cb(NULL, node);
	}
}

static void gtk_blist_row_activated_cb(GtkTreeView *tv, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data) {
	PidginBuddyList *gtkblist = PIDGIN_BUDDY_LIST(data);
	PurpleBlistNode *node;
	GtkTreeIter iter;

	gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);

	if(PURPLE_IS_CONTACT(node) || PURPLE_IS_BUDDY(node)) {
		PurpleBuddy *buddy;

		if(PURPLE_IS_CONTACT(node))
			buddy = purple_contact_get_priority_buddy((PurpleContact*)node);
		else
			buddy = (PurpleBuddy*)node;

		pidgin_dialogs_im_with_user(purple_buddy_get_account(buddy), purple_buddy_get_name(buddy));
	} else if (PURPLE_IS_CHAT(node)) {
		gtk_blist_join_chat((PurpleChat *)node);
	} else if (PURPLE_IS_GROUP(node)) {
/*		if (gtk_tree_view_row_expanded(tv, path))
			gtk_tree_view_collapse_row(tv, path);
		else
			gtk_tree_view_expand_row(tv,path,FALSE);*/
	}
}

struct _expand {
	GtkTreeView *treeview;
	GtkTreePath *path;
	PurpleBlistNode *node;
};

static gboolean
scroll_to_expanded_cell(gpointer data)
{
	struct _expand *ex = data;
	gtk_tree_view_scroll_to_cell(ex->treeview, ex->path, NULL, FALSE, 0, 0);
	pidgin_blist_update_contact(NULL, ex->node);

	gtk_tree_path_free(ex->path);
	g_free(ex);

	return FALSE;
}

static void
pidgin_blist_expand_contact_cb(GtkWidget *w, PurpleBlistNode *node)
{
	PidginBlistNode *gtknode;
	GtkTreeIter iter, parent;
	PurpleBlistNode *bnode;
	GtkTreePath *path;

	if(!PURPLE_IS_CONTACT(node)) {
		return;
	}

	gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);

	gtknode->contact_expanded = TRUE;

	for(bnode = purple_blist_node_get_first_child(node); bnode; bnode = purple_blist_node_get_sibling_next(bnode)) {
		pidgin_blist_update(NULL, bnode);
	}

	/* This ensures that the bottom buddy is visible, i.e. not scrolled off the alignment */
	if (get_iter_from_node(node, &parent)) {
		struct _expand *ex = g_new0(struct _expand, 1);

		gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(gtkblist->treemodel), &iter, &parent,
				  gtk_tree_model_iter_n_children(GTK_TREE_MODEL(gtkblist->treemodel), &parent) -1);
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);

		/* Let the treeview draw so it knows where to scroll */
		ex->treeview = GTK_TREE_VIEW(gtkblist->treeview);
		ex->path = path;
		ex->node = purple_blist_node_get_first_child(node);
		g_idle_add(scroll_to_expanded_cell, ex);
	}
}

static void
pidgin_blist_collapse_contact_cb(GtkWidget *w, PurpleBlistNode *node)
{
	PurpleBlistNode *bnode;
	PidginBlistNode *gtknode;

	if(!PURPLE_IS_CONTACT(node))
		return;

	gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);

	gtknode->contact_expanded = FALSE;

	for(bnode = purple_blist_node_get_first_child(node); bnode; bnode = purple_blist_node_get_sibling_next(bnode)) {
		pidgin_blist_update(NULL, bnode);
	}
}

static gboolean
pidgin_blist_key_press_cb(G_GNUC_UNUSED GtkEventControllerKey *controller,
                          guint keyval, G_GNUC_UNUSED guint keycode,
                          GdkModifierType state, gpointer data)
{
	PidginBuddyList *blist = PIDGIN_BUDDY_LIST(data);
	GtkTreeView *tv = NULL;
	GtkTreeModel *model = NULL;
	PurpleBlistNode *node;
	GtkTreeIter iter;
	GtkTreeSelection *sel;

	tv = GTK_TREE_VIEW(blist->treeview);
	sel = gtk_tree_view_get_selection(tv);

	if(!gtk_tree_selection_get_selected(sel, NULL, &iter))
		return FALSE;

	model = GTK_TREE_MODEL(blist->treemodel);
	gtk_tree_model_get(model, &iter, NODE_COLUMN, &node, -1);

	if(state & GDK_CONTROL_MASK && (keyval == 'o' || keyval == 'O')) {
		PurpleBuddy *buddy;

		if(PURPLE_IS_CONTACT(node)) {
			buddy = purple_contact_get_priority_buddy((PurpleContact*)node);
		} else if(PURPLE_IS_BUDDY(node)) {
			buddy = (PurpleBuddy*)node;
		} else {
			return FALSE;
		}
		if(buddy)
			pidgin_retrieve_user_info(purple_account_get_connection(purple_buddy_get_account(buddy)), purple_buddy_get_name(buddy));
	} else {
		GtkTreeIter parent;
		GtkTreePath *path = NULL;

		switch (keyval) {
			case GDK_KEY_F2:
				/* FIXME: gk 2022-05-27 */
				/*
				gtk_blist_menu_alias_cb(tv, node);
				 */
				break;

			case GDK_KEY_Left:
				path = gtk_tree_model_get_path(model, &iter);
				if (gtk_tree_view_row_expanded(tv, path)) {
					/* Collapse the Group */
					gtk_tree_view_collapse_row(tv, path);
					gtk_tree_path_free(path);
					return TRUE;
				} else {
					/* Select the Parent */
					if (gtk_tree_model_get_iter(model, &iter, path)) {
						if (gtk_tree_model_iter_parent(model, &parent, &iter)) {
							gtk_tree_path_free(path);
							path = gtk_tree_model_get_path(model, &parent);
							gtk_tree_view_set_cursor(tv, path, NULL, FALSE);
							gtk_tree_path_free(path);
							return TRUE;
						}
					}
				}
				gtk_tree_path_free(path);
				break;

			case GDK_KEY_Right:
				path = gtk_tree_model_get_path(model, &iter);
				if (!gtk_tree_view_row_expanded(tv, path)) {
					/* Expand the Group */
					if (PURPLE_IS_CONTACT(node)) {
						pidgin_blist_expand_contact_cb(NULL, node);
						gtk_tree_path_free(path);
						return TRUE;
					} else if (!PURPLE_IS_BUDDY(node)) {
						gtk_tree_view_expand_row(tv, path, FALSE);
						gtk_tree_path_free(path);
						return TRUE;
					}
				} else {
					/* Select the First Child */
					if (gtk_tree_model_get_iter(model, &parent, path)) {
						if (gtk_tree_model_iter_nth_child(model, &iter, &parent, 0)) {
							gtk_tree_path_free(path);
							path = gtk_tree_model_get_path(model, &iter);
							gtk_tree_view_set_cursor(tv, path, NULL, FALSE);
							gtk_tree_path_free(path);
							return TRUE;
						}
					}
				}
				gtk_tree_path_free(path);
				break;
		}
	}

	return FALSE;
}

static gboolean
pidgin_blist_show_context_menu(GtkWidget *tv, PurpleBlistNode *node, GdkEventButton *event)
{
	PidginBlistNode *gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);
	GAction *action = NULL;
	GActionMap *action_map = NULL;
	GtkApplication *gtk_application = NULL;
	GMenu *menu = NULL;
	gboolean enabled = FALSE;
	gboolean handled = FALSE;

	gtk_application = GTK_APPLICATION(g_application_get_default());

	action_map = G_ACTION_MAP(gtk_widget_get_action_group(tv, "menu"));

	/* Create a menu based on the thing we right-clicked on */
	if (PURPLE_IS_GROUP(node)) {
		menu = gtk_application_get_menu_by_id(gtk_application, "group");

		/* Add buddy is only enabled if there is an online account. */
		action = g_action_map_lookup_action(action_map, "add-buddy");
		enabled = purple_connections_get_all() != NULL;
		g_simple_action_set_enabled(G_SIMPLE_ACTION(action), enabled);

		/* Add chat is only enabled if at least one protocol supports chats. */
		action = g_action_map_lookup_action(action_map, "add-chat");
		enabled = pidgin_blist_joinchat_is_showable();
		g_simple_action_set_enabled(G_SIMPLE_ACTION(action), enabled);
	} else if (PURPLE_IS_CHAT(node)) {
		GVariant *variant = NULL;

		menu = gtk_application_get_menu_by_id(gtk_application, "chat");

		/* Set the auto-join state to the correct value. */
		action = g_action_map_lookup_action(action_map, "auto-join-chat");
		enabled = purple_blist_node_get_bool(node, "gtk-autojoin");
		variant = g_variant_new_boolean(enabled);
		g_simple_action_set_state(G_SIMPLE_ACTION(action), variant);

		/* Set the persistent state to the correct value. */
		action = g_action_map_lookup_action(action_map, "persistent-chat");
		enabled = purple_blist_node_get_bool(node, "gtk-persistent");
		variant = g_variant_new_boolean(enabled);
		g_simple_action_set_state(G_SIMPLE_ACTION(action), variant);
	} else if ((PURPLE_IS_CONTACT(node)) && (gtknode->contact_expanded)) {
		menu = gtk_application_get_menu_by_id(gtk_application, "contact");
	} else if (PURPLE_IS_CONTACT(node) || PURPLE_IS_BUDDY(node)) {
		PurpleAccount *account = NULL;
		PurpleBuddy *buddy = NULL;
		PurpleConnection *connection = NULL;
		PurpleProtocol *protocol = NULL;
		GVariant *variant = NULL;
		const gchar *buddy_name = NULL;

		menu = gtk_application_get_menu_by_id(gtk_application, "buddy");

		if(PURPLE_IS_CONTACT(node)) {
			buddy = purple_contact_get_priority_buddy(PURPLE_CONTACT(node));
		} else {
			buddy = PURPLE_BUDDY(node);
		}

		account = purple_buddy_get_account(buddy);
		buddy_name = purple_buddy_get_name(buddy);
		connection = purple_account_get_connection(account);
		protocol = purple_connection_get_protocol(connection);

		/* Get info is optional for protocols to support. */
		action = g_action_map_lookup_action(action_map, "buddy-get-info");
		enabled = protocol && PURPLE_PROTOCOL_IMPLEMENTS(protocol, SERVER,
		                                                 get_info);
		g_simple_action_set_enabled(G_SIMPLE_ACTION(action), enabled);

		/* File transfers are protocol dependent. */
		action = g_action_map_lookup_action(action_map,
		                                    "buddy-send-file");
		if(protocol && PURPLE_IS_PROTOCOL_XFER(protocol)) {
			enabled = purple_protocol_xfer_can_receive(PURPLE_PROTOCOL_XFER(protocol),
			                                           connection, buddy_name);
		} else {
			enabled = FALSE;
		}

		g_simple_action_set_enabled(G_SIMPLE_ACTION(action), enabled);

		if(protocol != NULL &&
		   PURPLE_PROTOCOL_IMPLEMENTS(protocol, MEDIA, get_caps))
		{
			PurpleMediaCaps caps = 0;

			caps = purple_protocol_get_media_caps(account, buddy_name);

			/* Voice calls are dependent on protocol. */
			action = g_action_map_lookup_action(action_map, "buddy-audio-call");
			enabled = (caps & PURPLE_MEDIA_CAPS_AUDIO);
			g_simple_action_set_enabled(G_SIMPLE_ACTION(action), enabled);

			/* Video calls are dependent on protocol. */
			action = g_action_map_lookup_action(action_map, "buddy-video-call");
			enabled = (caps & PURPLE_MEDIA_CAPS_AUDIO_VIDEO) ||
			          (caps & PURPLE_MEDIA_CAPS_VIDEO);
			g_simple_action_set_enabled(G_SIMPLE_ACTION(action), enabled);
		}

		/* Set the proper state of the block action. */
		action = g_action_map_lookup_action(action_map, "buddy-block");
		enabled = !purple_account_privacy_check(account, buddy_name);
		variant = g_variant_new_boolean(enabled);
		g_simple_action_set_state(G_SIMPLE_ACTION(action), variant);
	}

	action = g_action_map_lookup_action(action_map,
	                                    "remove-custom-icon");
	enabled = purple_buddy_icons_node_has_custom_icon(node);
	g_simple_action_set_enabled(G_SIMPLE_ACTION(action), enabled);

	/* Now display the menu */
	if (menu != NULL && event != NULL) {
		GtkWidget *popover_menu = gtk_popover_menu_new();

		gtk_popover_bind_model(GTK_POPOVER(popover_menu), G_MENU_MODEL(menu),
		                       NULL);
		gtk_popover_set_relative_to(GTK_POPOVER(popover_menu), tv);
		gtk_popover_set_position(GTK_POPOVER(popover_menu), GTK_POS_BOTTOM);
		gtk_popover_set_pointing_to(GTK_POPOVER(popover_menu),
		                            &(const GdkRectangle){(int)event->x, (int)event->y, 1, 1});

		gtk_popover_popup(GTK_POPOVER(popover_menu));

		handled = TRUE;
	}

	return handled;
}

static gboolean
gtk_blist_button_press_cb(GtkWidget *tv, GdkEventButton *event, gpointer user_data)
{
	GtkTreePath *path;
	PurpleBlistNode *node;
	GtkTreeIter iter;
	GtkTreeSelection *sel;
	PurpleProtocol *protocol = NULL;
	PidginBlistNode *gtknode;
	gboolean handled = FALSE;

	/* Here we figure out which node was clicked */
	if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(tv), event->x, event->y, &path, NULL, NULL, NULL))
		return FALSE;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), &iter, path);
	gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);
	gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);

	/* Right click draws a context menu */
	if (gdk_event_triggers_context_menu((GdkEvent *)event)) {
		handled = pidgin_blist_show_context_menu(tv, node, event);

	/* CTRL+middle click expands or collapse a contact */
	} else if ((event->button == GDK_BUTTON_MIDDLE) && (event->type == GDK_BUTTON_PRESS) &&
			   (event->state & GDK_CONTROL_MASK) && (PURPLE_IS_CONTACT(node))) {
		if (gtknode->contact_expanded)
			pidgin_blist_collapse_contact_cb(NULL, node);
		else
			pidgin_blist_expand_contact_cb(NULL, node);
		handled = TRUE;

	/* Double middle click gets info */
	} else if ((event->button == GDK_BUTTON_MIDDLE) && (event->type == GDK_2BUTTON_PRESS) &&
			   ((PURPLE_IS_CONTACT(node)) || (PURPLE_IS_BUDDY(node)))) {
		PurpleAccount *account;
		PurpleBuddy *b;
		if(PURPLE_IS_CONTACT(node))
			b = purple_contact_get_priority_buddy((PurpleContact*)node);
		else
			b = (PurpleBuddy *)node;

		account = purple_buddy_get_account(b);
		protocol = purple_account_get_protocol(account);

		if (protocol && PURPLE_PROTOCOL_IMPLEMENTS(protocol, SERVER, get_info))
			pidgin_retrieve_user_info(purple_account_get_connection(account), purple_buddy_get_name(b));
		handled = TRUE;
	}

#if (1)
	/*
	 * This code only exists because GTK doesn't work.  If we return
	 * FALSE here, as would be normal the event propoagates down and
	 * somehow gets interpreted as the start of a drag event.
	 *
	 * Um, isn't it _normal_ to return TRUE here?  Since the event
	 * was handled?  --Mark
	 */
	if(handled) {
		sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));
		gtk_tree_selection_select_path(sel, path);
		gtk_tree_path_free(path);
		return TRUE;
	}
#endif
	gtk_tree_path_free(path);

	return FALSE;
}

static gboolean
pidgin_blist_popup_menu_cb(GtkWidget *tv, void *user_data)
{
	PurpleBlistNode *node;
	GtkTreeIter iter;
	GtkTreeSelection *sel;
	gboolean handled = FALSE;

	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tv));
	if (!gtk_tree_selection_get_selected(sel, NULL, &iter))
		return FALSE;

	gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter, NODE_COLUMN, &node, -1);

	/* Shift+F10 draws a context menu */
	handled = pidgin_blist_show_context_menu(tv, node, NULL);

	return handled;
}

/* Altered from do_colorshift in gnome-panel */
static void
do_alphashift(GdkPixbuf *pixbuf, int shift)
{
	gint i, j;
	gint width, height, padding;
	guchar *pixels;
	int val;

	if (!gdk_pixbuf_get_has_alpha(pixbuf))
	  return;

	width = gdk_pixbuf_get_width(pixbuf);
	height = gdk_pixbuf_get_height(pixbuf);
	padding = gdk_pixbuf_get_rowstride(pixbuf) - width * 4;
	pixels = gdk_pixbuf_get_pixels(pixbuf);

	for (i = 0; i < height; i++) {
		for (j = 0; j < width; j++) {
			pixels++;
			pixels++;
			pixels++;
			val = *pixels - shift;
			*(pixels++) = CLAMP(val, 0, 255);
		}
		pixels += padding;
	}
}


static GdkPixbuf *pidgin_blist_get_buddy_icon(PurpleBlistNode *node,
                                              gboolean scaled, gboolean greyed)
{
	gsize len;
	PurpleBuddy *buddy = NULL;
	PurpleGroup *group = NULL;
	const guchar *data = NULL;
	GdkPixbuf *buf, *ret = NULL;
	PurpleBuddyIcon *icon = NULL;
	PurpleAccount *account = NULL;
	PurpleContact *contact = NULL;
	PurpleImage *custom_img;
	PurpleProtocol *protocol = NULL;
	PurpleBuddyIconSpec *icon_spec = NULL;
	gint orig_width, orig_height, scale_width, scale_height;

	if (PURPLE_IS_CONTACT(node)) {
		buddy = purple_contact_get_priority_buddy((PurpleContact*)node);
		contact = (PurpleContact*)node;
	} else if (PURPLE_IS_BUDDY(node)) {
		buddy = (PurpleBuddy*)node;
		contact = purple_buddy_get_contact(buddy);
	} else if (PURPLE_IS_GROUP(node)) {
		group = (PurpleGroup*)node;
	} else if (PURPLE_IS_CHAT(node)) {
		/* We don't need to do anything here. We just need to not fall
		 * into the else block and return. */
	} else {
		return NULL;
	}

	if (buddy) {
		account = purple_buddy_get_account(buddy);
	}

	if(account && purple_account_get_connection(account)) {
		protocol = purple_connection_get_protocol(purple_account_get_connection(account));
	}

	/* If we have a contact then this is either a contact or a buddy and
	 * we want to fetch the custom icon for the contact. If we don't have
	 * a contact then this is a group or some other type of node and we
	 * want to use that directly. */
	if (contact) {
		custom_img = purple_buddy_icons_node_find_custom_icon(PURPLE_BLIST_NODE(contact));
	} else {
		custom_img = purple_buddy_icons_node_find_custom_icon(node);
	}

	if (custom_img) {
		data = purple_image_get_data(custom_img);
		len = purple_image_get_data_size(custom_img);
	}

	if (data == NULL) {
		if (buddy) {
			/* Not sure I like this...*/
			if (!(icon = purple_buddy_icons_find(purple_buddy_get_account(buddy), purple_buddy_get_name(buddy))))
				return NULL;
			data = purple_buddy_icon_get_data(icon, &len);
		}

		if(data == NULL)
			return NULL;
	}

	buf = purple_gdk_pixbuf_from_data(data, len);
	purple_buddy_icon_unref(icon);
	if (!buf) {
		purple_debug_warning("gtkblist", "Couldn't load buddy icon on "
			"account %s (%s); buddyname=%s; custom_img_size=%" G_GSIZE_FORMAT,
			account ? purple_account_get_username(account) : "(no account)",
			account ? purple_account_get_protocol_id(account) : "(no account)",
			buddy ? purple_buddy_get_name(buddy) : "(no buddy)",
			custom_img ? purple_image_get_data_size(custom_img) : 0);
		if (custom_img)
			g_object_unref(custom_img);
		return NULL;
	}
	if (custom_img)
		g_object_unref(custom_img);

	if (greyed) {
		gboolean offline = FALSE, idle = FALSE;

		if (buddy) {
			PurplePresence *presence = purple_buddy_get_presence(buddy);
			if (!PURPLE_BUDDY_IS_ONLINE(buddy))
				offline = TRUE;
			if (purple_presence_is_idle(presence))
				idle = TRUE;
		} else if (group) {
			if (purple_counting_node_get_online_count(PURPLE_COUNTING_NODE(group)) == 0)
				offline = TRUE;
		}

		if (offline)
			gdk_pixbuf_saturate_and_pixelate(buf, buf, 0.0, FALSE);

		if (idle)
			gdk_pixbuf_saturate_and_pixelate(buf, buf, 0.25, FALSE);
	}

	/* I'd use the pidgin_buddy_icon_get_scale_size() thing, but it won't
	 * tell me the original size, which I need for scaling purposes. */
	scale_width = orig_width = gdk_pixbuf_get_width(buf);
	scale_height = orig_height = gdk_pixbuf_get_height(buf);

	if (protocol)
		icon_spec = purple_protocol_get_icon_spec(protocol);

	if (icon_spec && (icon_spec->scale_rules & PURPLE_ICON_SCALE_DISPLAY)) {
		purple_buddy_icon_spec_get_scaled_size(icon_spec, &scale_width,
		                                       &scale_height);
	}

	purple_buddy_icon_spec_free(icon_spec);

	if (scaled || scale_height > 200 || scale_width > 200) {
		GdkPixbuf *tmpbuf;
		float scale_size = scaled ? 32.0 : 200.0;
		if(scale_height > scale_width) {
			scale_width = scale_size * (double)scale_width / (double)scale_height;
			scale_height = scale_size;
		} else {
			scale_height = scale_size * (double)scale_height / (double)scale_width;
			scale_width = scale_size;
		}
		/* Scale & round before making square, so rectangular (but
		 * non-square) images get rounded corners too. */
		tmpbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, scale_width, scale_height);
		gdk_pixbuf_fill(tmpbuf, 0x00000000);
		gdk_pixbuf_scale(buf, tmpbuf, 0, 0, scale_width, scale_height, 0, 0, (double)scale_width/(double)orig_width, (double)scale_height/(double)orig_height, GDK_INTERP_BILINEAR);
		if (purple_gdk_pixbuf_is_opaque(tmpbuf))
			purple_gdk_pixbuf_make_round(tmpbuf);
		ret = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, scale_size, scale_size);
		gdk_pixbuf_fill(ret, 0x00000000);
		gdk_pixbuf_copy_area(tmpbuf, 0, 0, scale_width, scale_height, ret, (scale_size-scale_width)/2, (scale_size-scale_height)/2);
		g_object_unref(G_OBJECT(tmpbuf));
	} else {
		ret = gdk_pixbuf_scale_simple(buf,scale_width,scale_height, GDK_INTERP_BILINEAR);
	}
	g_object_unref(G_OBJECT(buf));

	return ret;
}

static gboolean buddy_is_displayable(PurpleBuddy *buddy)
{
	if(!buddy)
		return FALSE;

	return purple_account_is_connected(purple_buddy_get_account(buddy));
}

/* # - Status Icon
 * P - Protocol Icon
 * A - Buddy Icon
 * [ - SMALL_SPACE
 * = - LARGE_SPACE
 *                   +--- STATUS_SIZE                +--- td->avatar_width
 *                   |         +-- td->name_width    |
 *                +----+   +-------+            +---------+
 *                |    |   |       |            |         |
 *                +-------------------------------------------+
 *                |       [          =        [               |--- TOOLTIP_BORDER
 *name_height --+-| ######[BuddyName = PP     [   AAAAAAAAAAA |--+
 *              | | ######[          = PP     [   AAAAAAAAAAA |  |
 * STATUS SIZE -| | ######[[[[[[[[[[[[[[[[[[[[[   AAAAAAAAAAA |  |
 *           +--+-| ######[Account: So-and-so [   AAAAAAAAAAA |  |-- td->avatar_height
 *           |    |       [Idle: 4h 15m       [   AAAAAAAAAAA |  |
 *  height --+    |       [Foo: Bar, Baz      [   AAAAAAAAAAA |  |
 *           |    |       [Status: Awesome    [   AAAAAAAAAAA |--+
 *           +----|       [Stop: Hammer Time  [               |
 *                |       [                   [               |--- TOOLTIP_BORDER
 *                +-------------------------------------------+
 *                 |       |                |                |
 *                 |       +----------------+                |
 *                 |               |                         |
 *                 |               +-- td->width             |
 *                 |                                         |
 *                 +---- TOOLTIP_BORDER                      +---- TOOLTIP_BORDER
 *
 *
 */
#define STATUS_SIZE 16
#define TOOLTIP_BORDER 12
#define SMALL_SPACE 6
#define LARGE_SPACE 12

static void
add_tip_for_account(GtkWidget *grid, gint row, PurpleAccount *account)
{
	GdkPixbuf *protocol_icon = NULL;
	GtkWidget *image = NULL;
	GtkWidget *name = NULL;

	protocol_icon = pidgin_create_protocol_icon(account, PIDGIN_PROTOCOL_ICON_SMALL);
	if (purple_account_is_disconnected(account)) {
		gdk_pixbuf_saturate_and_pixelate(protocol_icon, protocol_icon, 0.0, FALSE);
	}
	image = gtk_image_new_from_pixbuf(protocol_icon);
	gtk_image_set_pixel_size(GTK_IMAGE(image), STATUS_SIZE);
	gtk_grid_attach(GTK_GRID(grid), image, 0, row, 1, 1);
	g_clear_object(&protocol_icon);

	name = gtk_label_new(purple_account_get_username(account));
	gtk_label_set_xalign(GTK_LABEL(name), 0);
	gtk_label_set_yalign(GTK_LABEL(name), 0);
	gtk_label_set_line_wrap(GTK_LABEL(name), TRUE);
	gtk_label_set_max_width_chars(GTK_LABEL(name), 36);
	gtk_grid_attach(GTK_GRID(grid), name, 1, row, 1, 1);
}

static void
add_tip_for_node(GtkWidget *grid, gint row, PurpleBlistNode *node, gboolean full)
{
	GdkPixbuf *status_icon = NULL;
	GtkWidget *image = NULL;
	GtkWidget *name = NULL;
	GdkPixbuf *avatar = NULL;
	GtkWidget *avatar_image = NULL;
	PurpleAccount *account = NULL;
	char *tmp = NULL, *node_name = NULL, *tooltip_text = NULL;

	if (PURPLE_IS_BUDDY(node)) {
		account = purple_buddy_get_account(PURPLE_BUDDY(node));
	} else if (PURPLE_IS_CHAT(node)) {
		account = purple_chat_get_account(PURPLE_CHAT(node));
	}

	status_icon = pidgin_blist_get_status_icon(node, PIDGIN_STATUS_ICON_LARGE);
	image = gtk_image_new_from_pixbuf(status_icon);
	gtk_image_set_pixel_size(GTK_IMAGE(image), STATUS_SIZE);
	gtk_grid_attach(GTK_GRID(grid), image, 0, row, 1, 1);
	g_clear_object(&status_icon);

	if (PURPLE_IS_BUDDY(node)) {
		tmp = g_markup_escape_text(purple_buddy_get_name(PURPLE_BUDDY(node)), -1);
	} else if (PURPLE_IS_CHAT(node)) {
		tmp = g_markup_escape_text(purple_chat_get_name(PURPLE_CHAT(node)), -1);
	} else if (PURPLE_IS_GROUP(node)) {
		tmp = g_markup_escape_text(purple_group_get_name(PURPLE_GROUP(node)), -1);
	} else {
		/* I don't believe this can happen currently, I think
		 * everything that calls this function checks for one of the
		 * above node types first. */
		tmp = g_strdup(_("Unknown node type"));
	}
	node_name = g_strdup_printf("<span size='x-large' weight='bold'>%s</span>",
								tmp ? tmp : "");
	g_free(tmp);

	name = gtk_label_new(NULL);
	gtk_label_set_xalign(GTK_LABEL(name), 0);
	gtk_label_set_yalign(GTK_LABEL(name), 0);
	gtk_label_set_markup(GTK_LABEL(name), node_name);
	gtk_label_set_max_width_chars(GTK_LABEL(name), 36);
	gtk_grid_attach(GTK_GRID(grid), name, 1, row, 1, 1);

	if (account != NULL) {
		GdkPixbuf *protocol_icon = pidgin_create_protocol_icon(
		        account, PIDGIN_PROTOCOL_ICON_SMALL);
		image = gtk_image_new_from_pixbuf(protocol_icon);
		gtk_image_set_pixel_size(GTK_IMAGE(image), STATUS_SIZE);
		gtk_widget_set_halign(image, GTK_ALIGN_END);
		gtk_grid_attach(GTK_GRID(grid), image, 2, row, 1, 1);
		g_clear_object(&protocol_icon);
	}

	tooltip_text = pidgin_get_tooltip_text(node, full);
	if (tooltip_text && *tooltip_text) {
		GtkWidget *contents = gtk_label_new(NULL);
		gtk_label_set_xalign(GTK_LABEL(contents), 0);
		gtk_label_set_yalign(GTK_LABEL(contents), 0);
		gtk_label_set_markup(GTK_LABEL(contents), tooltip_text);
		gtk_label_set_line_wrap(GTK_LABEL(contents), TRUE);
		gtk_label_set_max_width_chars(GTK_LABEL(contents), 36);
		gtk_grid_attach(GTK_GRID(grid), contents, 1, row+1, 2, 1);
	}

	avatar = pidgin_blist_get_buddy_icon(node, !full, FALSE);
#if 0  /* Protocol Icon as avatar */
	if(!avatar && full) {
		avatar = pidgin_create_protocol_icon(account, PIDGIN_PROTOCOL_ICON_LARGE);
	}
#endif

	if (avatar != NULL) {
		avatar_image = gtk_image_new_from_pixbuf(avatar);
		gtk_widget_set_halign(avatar_image, GTK_ALIGN_END);
		gtk_widget_set_valign(avatar_image, GTK_ALIGN_START);
		gtk_grid_attach(GTK_GRID(grid), avatar_image, 3, row, 1, 2);
		g_object_unref(avatar);
	}

	g_free(node_name);
	g_free(tooltip_text);
}

gboolean
pidgin_blist_query_tooltip_for_node(PurpleBlistNode *node, GtkTooltip *tooltip)
{
	GtkWidget *grid;

	grid = gtk_grid_new();

	if (PURPLE_IS_CHAT(node) || PURPLE_IS_BUDDY(node)) {
		add_tip_for_node(grid, 0, node, TRUE);

	} else if (PURPLE_IS_GROUP(node)) {
		PurpleGroup *group = PURPLE_GROUP(node);
		GSList *accounts;
		add_tip_for_node(grid, 0, node, TRUE);

		/* Accounts with buddies in group */
		accounts = purple_group_get_accounts(group);
		for (gint row = 2; accounts != NULL;
		     row++, accounts = g_slist_delete_link(accounts, accounts))
		{
			PurpleAccount *account = PURPLE_ACCOUNT(accounts->data);
			add_tip_for_account(grid, row, account);
		}

	} else if (PURPLE_IS_CONTACT(node)) {
		PurpleBlistNode *child;
		PurpleBuddy *b = purple_contact_get_priority_buddy(PURPLE_CONTACT(node));
		gint row = 0;

		for(child = node->child; child; child = child->next) {
			if(PURPLE_IS_BUDDY(child) && buddy_is_displayable(PURPLE_BUDDY(child))) {
				if (b == (PurpleBuddy *)child) {
					/* Priority buddy goes first (-2) and is more detailed. */
					add_tip_for_node(grid, -2, child, TRUE);
				} else {
					add_tip_for_node(grid, row, child, FALSE);
					row += 2;
				}
			}
		}

	} else {
		return FALSE;
	}

	gtk_widget_show_all(grid);
	gtk_tooltip_set_custom(tooltip, grid);

	return TRUE;
}

static gboolean
pidgin_blist_query_tooltip(GtkWidget *widget, int x, int y,
                           gboolean keyboard_mode, GtkTooltip *tooltip,
                           gpointer userdata)
{
	PidginBuddyList *blist = userdata;
	GtkTreeIter iter;
	PurpleBlistNode *node;
	gboolean editable = FALSE;
	GtkTreePath *path = NULL;

	/* If we're editing a cell (e.g. alias editing), don't show the tooltip */
	g_object_get(G_OBJECT(blist->text_rend), "editable", &editable, NULL);
	if (editable) {
		return FALSE;
	}

	if (keyboard_mode) {
		GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
		if (!gtk_tree_selection_get_selected(selection, NULL, &iter)) {
			return FALSE;
		}
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(blist->treemodel), &iter);
	} else {
		gint bx, by;

		gtk_tree_view_convert_widget_to_bin_window_coords(GTK_TREE_VIEW(widget),
		                                                  x, y, &bx, &by);
		gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), bx, by, &path,
		                              NULL, NULL, NULL);
		if (path == NULL) {
			return FALSE;
		}
		if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(blist->treemodel),
		                             &iter, path))
		{
			gtk_tree_path_free(path);
			return FALSE;
		}
	}

	gtk_tree_model_get(GTK_TREE_MODEL(blist->treemodel), &iter,
	                   NODE_COLUMN, &node, -1);
	if (pidgin_blist_query_tooltip_for_node(node, tooltip)) {
		gtk_tree_view_set_tooltip_row(GTK_TREE_VIEW(widget), tooltip, path);
		gtk_tree_path_free(path);
		return TRUE;
	} else {
		gtk_tree_path_free(path);
		return FALSE;
	}
}

/*********************************************************
 * Private Utility functions                             *
 *********************************************************/

static char *pidgin_get_tooltip_text(PurpleBlistNode *node, gboolean full)
{
	GString *str = g_string_new("");
	PurpleProtocol *protocol = NULL;
	char *tmp;

	if (PURPLE_IS_CHAT(node))
	{
		PurpleAccount *account;
		PurpleChat *chat;
		GList *connections;
		GList *cur = NULL;
		PurpleProtocolChatEntry *pce;
		char *name, *value;
		PurpleChatConversation *conv = NULL;
		PidginBlistNode *bnode = g_object_get_data(G_OBJECT(node), UI_DATA);

		chat = (PurpleChat *)node;
		account = purple_chat_get_account(chat);
		protocol = purple_account_get_protocol(account);

		connections = purple_connections_get_all();
		if (connections && connections->next)
		{
			tmp = g_markup_escape_text(purple_account_get_username(account), -1);
			g_string_append_printf(str, _("<b>Account:</b> %s"), tmp);
			g_free(tmp);
		}

		if (bnode && bnode->conv) {
			conv = PURPLE_CHAT_CONVERSATION(bnode->conv);
		} else {
			PurpleConversation *chat_conv;
			PurpleConversationManager *manager;
			char *chat_name;

			if (protocol && PURPLE_PROTOCOL_IMPLEMENTS(protocol, CHAT, get_name)) {
				chat_name = purple_protocol_chat_get_name(PURPLE_PROTOCOL_CHAT(protocol),
				                                          purple_chat_get_components(chat));
			} else {
				chat_name = g_strdup(purple_chat_get_name(chat));
			}

			manager = purple_conversation_manager_get_default();
			chat_conv = purple_conversation_manager_find_chat(manager, account,
			                                                  chat_name);
			g_free(chat_name);

			if(PURPLE_IS_CHAT_CONVERSATION(chat_conv)) {
				conv = PURPLE_CHAT_CONVERSATION(chat_conv);
			}
		}

		if (conv && !purple_chat_conversation_has_left(conv)) {
			g_string_append_printf(str, _("\n<b>Occupants:</b> %d"),
				purple_chat_conversation_get_users_count(conv));

			if (protocol && (purple_protocol_get_options(protocol) & OPT_PROTO_CHAT_TOPIC)) {
				const char *chattopic = purple_chat_conversation_get_topic(conv);
				char *topic = chattopic ? g_markup_escape_text(chattopic, -1) : NULL;
				g_string_append_printf(str, _("\n<b>Topic:</b> %s"), topic ? topic : _("(no topic set)"));
				g_free(topic);
			}
		}

		if(protocol) {
			cur = purple_protocol_chat_info(PURPLE_PROTOCOL_CHAT(protocol),
			                                purple_account_get_connection(account));
		}

		while (cur != NULL)
		{
			pce = cur->data;

			if (!pce->secret)
			{
				tmp = purple_text_strip_mnemonic(pce->label);
				name = g_markup_escape_text(tmp, -1);
				g_free(tmp);
				value = g_markup_escape_text(g_hash_table_lookup(
										purple_chat_get_components(chat), pce->identifier), -1);
				g_string_append_printf(str, "\n<b>%s</b> %s",
							name ? name : "",
							value ? value : "");
				g_free(name);
				g_free(value);
			}

			g_free(pce);
			cur = g_list_delete_link(cur, cur);
		}
	}
	else if (PURPLE_IS_CONTACT(node) || PURPLE_IS_BUDDY(node))
	{
		/* NOTE: THIS FUNCTION IS NO LONGER CALLED FOR CONTACTS.
		 * It is only called by create_tip_for_node(), and create_tip_for_node() is never called for a contact.
		 */
		PurpleAccount *account;
		PurpleContact *c;
		PurpleBuddy *b;
		PurplePresence *presence;
		PurpleNotifyUserInfo *user_info;
		GList *connections;
		char *tmp;
		gchar *alias;
		time_t idle_secs, signon;

		if (PURPLE_IS_CONTACT(node))
		{
			c = (PurpleContact *)node;
			b = purple_contact_get_priority_buddy(c);
		}
		else
		{
			b = (PurpleBuddy *)node;
			c = purple_buddy_get_contact(b);
		}

		account = purple_buddy_get_account(b);
		protocol = purple_account_get_protocol(account);

		presence = purple_buddy_get_presence(b);
		user_info = purple_notify_user_info_new();

		/* Account */
		connections = purple_connections_get_all();
		if (full && connections && connections->next)
		{
			purple_notify_user_info_add_pair_plaintext(user_info, _("Account"),
					purple_account_get_username(account));
		}

		/* Alias */
		/* If there's not a contact alias, the node is being displayed with
		 * this alias, so there's no point in showing it in the tooltip. */
		g_object_get(c, "alias", &alias, NULL);
		if (full && c && purple_buddy_get_local_alias(b) != NULL && purple_buddy_get_local_alias(b)[0] != '\0' &&
		    (alias != NULL && alias[0] != '\0') &&
		    !purple_strequal(alias, purple_buddy_get_local_alias(b)))
		{
			purple_notify_user_info_add_pair_plaintext(user_info,
					_("Buddy Alias"), purple_buddy_get_local_alias(b));
		}

		/* Nickname/Server Alias */
		/* I'd like to only show this if there's a contact or buddy
		 * alias, but people often set long nicknames, which
		 * get ellipsized, so the only way to see the whole thing is
		 * to look at the tooltip. */
		if (full && purple_buddy_get_server_alias(b))
		{
			purple_notify_user_info_add_pair_plaintext(user_info,
					_("Nickname"), purple_buddy_get_server_alias(b));
		}

		/* Logged In */
		signon = purple_presence_get_login_time(presence);
		if (full && PURPLE_BUDDY_IS_ONLINE(b) && signon > 0)
		{
			if (signon > time(NULL)) {
				/*
				 * They signed on in the future?!  Our local clock
				 * must be wrong, show the actual date instead of
				 * "4 days", etc.
				 */
				GDateTime *dt = NULL, *local = NULL;

				dt = g_date_time_new_from_unix_utc(signon);
				local = g_date_time_to_local(dt);
				g_date_time_unref(dt);

				tmp = g_date_time_format(local, _("%x %X"));
				g_date_time_unref(local);
			} else {
				tmp = purple_str_seconds_to_string(time(NULL) - signon);
			}
			purple_notify_user_info_add_pair_plaintext(user_info, _("Logged In"), tmp);
			g_free(tmp);
		}

		/* Idle */
		if (purple_presence_is_idle(presence))
		{
			idle_secs = purple_presence_get_idle_time(presence);
			if (idle_secs > 0)
			{
				tmp = purple_str_seconds_to_string(time(NULL) - idle_secs);
				purple_notify_user_info_add_pair_plaintext(user_info, _("Idle"), tmp);
				g_free(tmp);
			}
		}

		/* Last Seen */
		if (full && c && !PURPLE_BUDDY_IS_ONLINE(b))
		{
			PidginBlistNode *gtknode = g_object_get_data(G_OBJECT(c), UI_DATA);
			PurpleBlistNode *bnode;
			int lastseen = 0;

			if (gtknode && (!gtknode->contact_expanded || PURPLE_IS_CONTACT(node)))
			{
				/* We're either looking at a buddy for a collapsed contact or
				 * an expanded contact itself so we show the most recent
				 * (largest) last_seen time for any of the buddies under
				 * the contact. */
				for (bnode = ((PurpleBlistNode *)c)->child ; bnode != NULL ; bnode = bnode->next)
				{
					int value = purple_blist_node_get_int(bnode, "last_seen");
					if (value > lastseen)
						lastseen = value;
				}
			}
			else
			{
				/* We're dealing with a buddy under an expanded contact,
				 * so we show the last_seen time for the buddy. */
				lastseen = purple_blist_node_get_int(&b->node, "last_seen");
			}

			if (lastseen > 0)
			{
				tmp = purple_str_seconds_to_string(time(NULL) - lastseen);
				purple_notify_user_info_add_pair_plaintext(user_info, _("Last Seen"), tmp);
				g_free(tmp);
			}
		}


		/* Offline? */
		/* FIXME: Why is this status special-cased by the core? --rlaager
		 * FIXME: Alternatively, why not have the core do all of them? --rlaager */
		if (!PURPLE_BUDDY_IS_ONLINE(b)) {
			purple_notify_user_info_add_pair_plaintext(user_info, _("Status"), _("Offline"));
		}

		if (purple_account_is_connected(account) && protocol) {
			/* Additional text from the protocol */
			purple_protocol_client_tooltip_text(PURPLE_PROTOCOL_CLIENT(protocol), b, user_info, full);
		}

		/* These are Easter Eggs.  Patches to remove them will be rejected. */
		if (!g_ascii_strcasecmp(purple_buddy_get_name(b), "robflynn"))
			purple_notify_user_info_add_pair_plaintext(user_info, _("Description"), _("Spooky"));
		if (!g_ascii_strcasecmp(purple_buddy_get_name(b), "seanegn"))
			purple_notify_user_info_add_pair_plaintext(user_info, _("Status"), _("Awesome"));
		if (!g_ascii_strcasecmp(purple_buddy_get_name(b), "chipx86"))
			purple_notify_user_info_add_pair_plaintext(user_info, _("Status"), _("Rockin'"));

		tmp = purple_notify_user_info_get_text_with_newline(user_info, "\n");
		g_string_append(str, tmp);
		g_free(tmp);
		g_free(alias);

		purple_notify_user_info_destroy(user_info);
	} else if (PURPLE_IS_GROUP(node)) {
		gint count;
		PurpleGroup *group = (PurpleGroup*)node;
		PurpleNotifyUserInfo *user_info;

		user_info = purple_notify_user_info_new();

		count = purple_counting_node_get_online_count(PURPLE_COUNTING_NODE(group));
		if (count != 0) {
			/* Online buddies in group */
			char tmp2[12];
			sprintf(tmp2, "%d", count);
			purple_notify_user_info_add_pair_plaintext(user_info,
					_("Online Buddies"), tmp2);
		}

		count = purple_counting_node_get_current_size(PURPLE_COUNTING_NODE(group));
		if (count != 0) {
			/* Total buddies (from online accounts) in group */
			char tmp2[12];
			sprintf(tmp2, "%d", count);
			purple_notify_user_info_add_pair_html(user_info,
					_("Total Buddies"), tmp2);
		}

		tmp = purple_notify_user_info_get_text_with_newline(user_info, "\n");
		g_string_append(str, tmp);
		g_free(tmp);

		purple_notify_user_info_destroy(user_info);
	}

	purple_signal_emit(pidgin_blist_get_handle(), "drawing-tooltip",
	                   node, str, full);

	return g_string_free(str, FALSE);
}

static GHashTable *cached_emblems;

static void _cleanup_cached_emblem(gpointer data, GObject *obj) {
	g_hash_table_remove(cached_emblems, data);
}

static GdkPixbuf * _pidgin_blist_get_cached_emblem(gchar *path) {
	GdkPixbuf *pb = g_hash_table_lookup(cached_emblems, path);

	if (pb != NULL) {
		/* The caller gets a reference */
		g_object_ref(pb);
		g_free(path);
	} else {
		pb = purple_gdk_pixbuf_new_from_file(path);
		if (pb != NULL) {
			/* We don't want to own a ref to the pixbuf, but we need to keep clean up. */
			/* I'm not sure if it would be better to just keep our ref and not let the emblem ever be destroyed */
			g_object_weak_ref(G_OBJECT(pb), _cleanup_cached_emblem, path);
			g_hash_table_insert(cached_emblems, path, pb);
		} else
			g_free(path);
	}

	return pb;
}

GdkPixbuf *
pidgin_blist_get_emblem(PurpleBlistNode *node)
{
	PurpleAccount *account;
	PurpleBuddy *buddy = NULL;
	PidginBlistNode *gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);
	PurpleProtocol *protocol;
	const char *name = NULL;
	char *filename, *path;
	PurplePresence *p = NULL;
	PurpleStatus *tune;

	if(PURPLE_IS_CONTACT(node)) {
		if(!gtknode->contact_expanded) {
			buddy = purple_contact_get_priority_buddy((PurpleContact*)node);
		}
	} else if(PURPLE_IS_BUDDY(node)) {
		PidginBlistNode *pidgin_node = NULL;
		buddy = (PurpleBuddy*)node;

		p = purple_buddy_get_presence(buddy);
		if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_MOBILE)) {
			/* This emblem comes from the small emoticon set now,
			 * to reduce duplication. */
			path = g_build_filename(PURPLE_DATADIR, "pixmaps",
				"pidgin", "emotes", "small", "mobile.png", NULL);
			return _pidgin_blist_get_cached_emblem(path);
		}

		pidgin_node = g_object_get_data(G_OBJECT(node->parent), UI_DATA);
		if(pidgin_node->contact_expanded) {
			return NULL;
		}
	} else {
		return NULL;
	}

	g_return_val_if_fail(buddy != NULL, NULL);

	account = purple_buddy_get_account(buddy);
	if (!purple_account_privacy_check(account, purple_buddy_get_name(buddy))) {
		path = g_build_filename(PURPLE_DATADIR, "pidgin", "icons",
			"hicolor", "16x16", "emblems", "emblem-blocked.png",
			NULL);
		return _pidgin_blist_get_cached_emblem(path);
	}

	/* If we came through the contact code flow above, we didn't need
	 * to get the presence until now. */
	if (p == NULL)
		p = purple_buddy_get_presence(buddy);

	if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_MOBILE)) {
		/* This emblem comes from the small emoticon set now, to reduce duplication. */
		path = g_build_filename(PURPLE_DATADIR, "pixmaps", "pidgin",
			"emotes", "small", "mobile.png", NULL);
		return _pidgin_blist_get_cached_emblem(path);
	}

	tune = purple_presence_get_status(p, "tune");
	if (tune && purple_status_is_active(tune)) {
		/* TODO: Replace "Tune" with generalized "Media" in 3.0. */
		if (purple_status_get_attr_string(tune, "game") != NULL) {
			path = g_build_filename(PURPLE_DATADIR, "pidgin",
				"icons", "hicolor", "16x16", "emblems",
				"emblem-game.png", NULL);
			return _pidgin_blist_get_cached_emblem(path);
		}
		/* TODO: Replace "Tune" with generalized "Media" in 3.0. */
		if (purple_status_get_attr_string(tune, "office") != NULL) {
			path = g_build_filename(PURPLE_DATADIR, "pidgin",
				"icons", "hicolor", "16x16", "emblems",
				"emblem-office.png", NULL);
			return _pidgin_blist_get_cached_emblem(path);
		}
		/* Regular old "tune" is the only one in all protocols. */
		/* This emblem comes from the small emoticon set now, to reduce duplication. */
		path = g_build_filename(PURPLE_DATADIR, "pixmaps", "pidgin",
			"emotes", "small", "music.png", NULL);
		return _pidgin_blist_get_cached_emblem(path);
	}

	protocol = purple_account_get_protocol(account);
	if (!protocol)
		return NULL;

	name = purple_protocol_client_list_emblem(PURPLE_PROTOCOL_CLIENT(protocol), buddy);

	if (name == NULL) {
		PurpleStatus *status;

		if (!purple_presence_is_status_primitive_active(p, PURPLE_STATUS_MOOD))
			return NULL;

		status = purple_presence_get_status(p, "mood");
		name = purple_status_get_attr_string(status, PURPLE_MOOD_NAME);

		if (!(name && *name))
			return NULL;

		path = pidgin_mood_get_icon_path(name);
	} else {
		filename = g_strdup_printf("emblem-%s.png", name);
		path = g_build_filename(PURPLE_DATADIR, "pidgin", "icons",
			"hicolor", "16x16", "emblems", filename, NULL);
		g_free(filename);
	}

	/* _pidgin_blist_get_cached_emblem() assumes ownership of path */
	return _pidgin_blist_get_cached_emblem(path);
}


GdkPixbuf *
pidgin_blist_get_status_icon(PurpleBlistNode *node, PidginStatusIconSize size)
{
	GdkPixbuf *ret;
	const char *icon = NULL;
	gboolean trans = FALSE;
	PidginBlistNode *gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);
	PidginBlistNode *gtkbuddynode = NULL;
	PurpleBuddy *buddy = NULL;
	PurpleChat *chat = NULL;
	gint icon_size = (size == PIDGIN_STATUS_ICON_LARGE) ? 16 : 11;

	if(PURPLE_IS_CONTACT(node)) {
		if(!gtknode->contact_expanded) {
			buddy = purple_contact_get_priority_buddy((PurpleContact*)node);
			if(buddy != NULL) {
				gtkbuddynode = g_object_get_data(G_OBJECT(buddy), UI_DATA);
			}
		}
	} else if(PURPLE_IS_BUDDY(node)) {
		buddy = (PurpleBuddy*)node;
		gtkbuddynode = g_object_get_data(G_OBJECT(node), UI_DATA);
	} else if(PURPLE_IS_CHAT(node)) {
		chat = (PurpleChat*)node;
	} else {
		return NULL;
	}

	if(buddy || chat) {
		PurpleAccount *account;
		PurpleProtocol *protocol;

		if(buddy)
			account = purple_buddy_get_account(buddy);
		else
			account = purple_chat_get_account(chat);

		protocol = purple_account_get_protocol(account);
		if(!protocol)
			return NULL;
	}

	if(buddy) {
		PurplePresence *p = purple_buddy_get_presence(buddy);
		trans = purple_presence_is_idle(p);

		if (PURPLE_BUDDY_IS_ONLINE(buddy) && gtkbuddynode && gtkbuddynode->recent_signonoff) {
			icon = "log-in";
		} else if (gtkbuddynode && gtkbuddynode->recent_signonoff) {
			icon = "log-out";
		} else if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_UNAVAILABLE)) {
			icon = "pidgin-user-busy";
		} else if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_AWAY)) {
			icon = "pidgin-user-away";
		} else if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_EXTENDED_AWAY)) {
			icon = "pidgin-user-extended-away";
		} else if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_OFFLINE)) {
			icon = "pidgin-user-offline";
		} else if (purple_presence_is_status_primitive_active(p, PURPLE_STATUS_INVISIBLE)) {
			icon = "pidgin-user-invisible";
		} else {
			icon = "pidgin-user-available";
		}
	} else if (chat) {
		icon = "chat";
	} else {
		icon = "person";
	}

	ret = gtk_icon_theme_load_icon(gtk_icon_theme_get_default(), icon, icon_size, 0, NULL);
	if (trans) {
		GdkPixbuf *copy = gdk_pixbuf_copy(ret);
		g_object_unref(ret);
		do_alphashift(copy, 77);
		ret = copy;
	}

	return ret;
}

gchar *
pidgin_blist_get_name_markup(PurpleBuddy *b, gboolean selected, gboolean aliased)
{
	const char *name, *name_color, *status_color, *dim_grey;
	char *text = NULL;
	PurpleProtocol *protocol = NULL;
	PurpleContact *contact;
	PurplePresence *presence;
	PidginBlistNode *gtkcontactnode = NULL;
	char *idletime = NULL, *statustext = NULL, *nametext = NULL;
	gchar *contact_alias;

	/* XXX Good luck cleaning up this crap */
	contact = PURPLE_CONTACT(PURPLE_BLIST_NODE(b)->parent);
	if(contact) {
		gtkcontactnode = g_object_get_data(G_OBJECT(contact), UI_DATA);
	}

	g_object_get(contact, "alias", &contact_alias, NULL);

	/* Name */
	if (gtkcontactnode && !gtkcontactnode->contact_expanded && contact_alias)
		name = contact_alias;
	else
		name = purple_buddy_get_alias(b);

	nametext = g_markup_escape_text(name, strlen(name));

	presence = purple_buddy_get_presence(b);

	/* Name is all that is needed */
	{
		PurpleAccount *account = purple_buddy_get_account(b);

		/* Status Info */
		protocol = purple_account_get_protocol(account);

		if (protocol && PURPLE_PROTOCOL_IMPLEMENTS(protocol, CLIENT, status_text) &&
				purple_account_get_connection(account)) {
			char *tmp = purple_protocol_client_status_text(PURPLE_PROTOCOL_CLIENT(protocol), b);
			const char *end;

			if(tmp && !g_utf8_validate(tmp, -1, &end)) {
				char *new = g_strndup(tmp,
						g_utf8_pointer_to_offset(tmp, end));
				g_free(tmp);
				tmp = new;
			}
			if(tmp) {
				g_strdelimit(tmp, "\n", ' ');
				purple_str_strip_char(tmp, '\r');
			}
			statustext = tmp;
		}

		if(!purple_presence_is_online(presence) && !statustext)
				statustext = g_strdup(_("Offline"));

		/* Idle Text */
		if (purple_presence_is_idle(presence)) {
			time_t idle_secs = purple_presence_get_idle_time(presence);

			if (idle_secs > 0) {
				int iday, ihrs, imin;
				time_t t;

				time(&t);
				iday = (t - idle_secs) / (24 * 60 * 60);
				ihrs = ((t - idle_secs) / 60 / 60) % 24;
				imin = ((t - idle_secs) / 60) % 60;

				if (iday)
					idletime = g_strdup_printf(_("Idle %dd %dh %02dm"), iday, ihrs, imin);
				else if (ihrs)
					idletime = g_strdup_printf(_("Idle %dh %02dm"), ihrs, imin);
				else
					idletime = g_strdup_printf(_("Idle %dm"), imin);

			} else
				idletime = g_strdup(_("Idle"));
		}
	}

	dim_grey = "dim grey";

	/* choose the colors of the text */
	name_color = NULL;
	status_color = dim_grey;

	if(!selected) {
		if(purple_presence_is_idle(presence) ||
		   !purple_presence_is_online(presence))
		{
			name_color = dim_grey;
		}
	}

	if(aliased && selected) {
		name_color = NULL;
		status_color = NULL;
	}

	/* Put it all together */
	if(statustext || idletime) {
		/* using <span size='smaller'> breaks the status, so it must be separated into <small><span>*/
		if (name_color) {
			text = g_strdup_printf("<span foreground='%s'>%s</span>\n"
			                       "<small><span foreground='%s'>%s%s%s</span></small>",
			                       name_color, nametext, status_color,
			                       idletime != NULL ? idletime : "",
			                       (idletime != NULL && statustext != NULL) ? " - " : "",
			                       statustext != NULL ? statustext : "");
		} else if (status_color) {
			text = g_strdup_printf("%s\n<small><span foreground='%s'>%s%s%s</span></small>",
			                       nametext, status_color,
			                       idletime != NULL ? idletime : "",
			                       (idletime != NULL && statustext != NULL) ? " - " : "",
			                       statustext != NULL ? statustext : "");
		} else {
			text = g_strdup_printf("%s\n<small>%s%s%s</small>",
			                       nametext,
			                       idletime != NULL ? idletime : "",
			                       (idletime != NULL && statustext != NULL) ? " - " : "",
			                       statustext != NULL ? statustext : "");
		}
	} else {
		if (name_color) {
			text = g_strdup_printf("<span color='%s'>%s</span>",
			                       name_color, nametext);
		} else {
			text = g_strdup_printf("%s", nametext);
		}
	}
	g_free(nametext);
	g_free(statustext);
	g_free(idletime);
	g_free(contact_alias);

	return text;
}

static void pidgin_blist_hide_node(PurpleBuddyList *list, PurpleBlistNode *node, gboolean update)
{
	PidginBlistNode *gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);
	GtkTreeIter iter;

	if (!gtknode || !gtknode->row || !gtkblist)
		return;

	if(gtkblist->selected_node == node)
		gtkblist->selected_node = NULL;
	if (get_iter_from_node(node, &iter)) {
		gtk_tree_store_remove(gtkblist->treemodel, &iter);
		if(update && (PURPLE_IS_CONTACT(node) ||
			PURPLE_IS_BUDDY(node) || PURPLE_IS_CHAT(node))) {
			pidgin_blist_update(list, node->parent);
		}
	}
	gtk_tree_row_reference_free(gtknode->row);
	gtknode->row = NULL;
}

static void
conversation_updated_cb(PurpleConversation *conv, PurpleConversationUpdateType type,
                        PidginBuddyList *gtkblist)
{
	PurpleAccount *account = purple_conversation_get_account(conv);

	if (type != PURPLE_CONVERSATION_UPDATE_UNSEEN)
		return;

	if(account != NULL && purple_conversation_get_name(conv) != NULL) {
		PurpleBuddy *buddy = purple_blist_find_buddy(account, purple_conversation_get_name(conv));
		if(buddy != NULL)
			pidgin_blist_update_buddy(NULL, PURPLE_BLIST_NODE(buddy), TRUE);
	}
}

static void
conversation_deleting_cb(PurpleConversation *conv, PidginBuddyList *gtkblist)
{
	conversation_updated_cb(conv, PURPLE_CONVERSATION_UPDATE_UNSEEN, gtkblist);
}

static void
conversation_deleted_update_ui_cb(PurpleConversation *conv, PidginBlistNode *ui)
{
	if (ui->conv != conv)
		return;
	ui->conv = NULL;
}

static void
conversation_created_cb(PurpleConversation *conv, PidginBuddyList *gtkblist)
{
	PurpleAccount *account = purple_conversation_get_account(conv);

	if (PURPLE_IS_IM_CONVERSATION(conv)) {
		GSList *buddies = purple_blist_find_buddies(account, purple_conversation_get_name(conv));
		while (buddies) {
			PurpleBlistNode *buddy = buddies->data;
			PidginBlistNode *ui = g_object_get_data(G_OBJECT(buddy), UI_DATA);
			buddies = g_slist_delete_link(buddies, buddies);
			if (!ui)
				continue;
			ui->conv = conv;

			purple_signal_connect(purple_conversations_get_handle(), "deleting-conversation",
					ui, G_CALLBACK(conversation_deleted_update_ui_cb), ui);
		}
	} else if (PURPLE_IS_CHAT_CONVERSATION(conv)) {
		PurpleChat *chat = purple_blist_find_chat(account, purple_conversation_get_name(conv));
		PidginBlistNode *ui;
		if (!chat)
			return;
		ui = g_object_get_data(G_OBJECT(chat), UI_DATA);
		if (!ui)
			return;
		ui->conv = conv;

		purple_signal_connect(purple_conversations_get_handle(), "deleting-conversation",
				ui, G_CALLBACK(conversation_deleted_update_ui_cb), ui);
	}
}

/**********************************************************************************
 * Public API Functions                                                           *
 **********************************************************************************/
static void
pidgin_blist_node_free(PidginBlistNode *node) {
	if(node->recent_signonoff_timer > 0) {
		g_source_remove(node->recent_signonoff_timer);
	}

	purple_signals_disconnect_by_handle(node);

	g_free(node);
}

static void
pidgin_blist_new_node(PurpleBuddyList *list, PurpleBlistNode *node) {
	PidginBlistNode *pidgin_node = NULL;

	pidgin_node = g_new0(PidginBlistNode, 1);

	g_object_set_data_full(G_OBJECT(node), UI_DATA, pidgin_node,
	                       (GDestroyNotify)pidgin_blist_node_free);
}

gboolean
pidgin_blist_node_is_contact_expanded(PurpleBlistNode *node) {
	PidginBlistNode *pidgin_node = NULL;

	if (PURPLE_IS_BUDDY(node)) {
		node = node->parent;
		if (node == NULL)
			return FALSE;
	}

	g_return_val_if_fail(PURPLE_IS_CONTACT(node), FALSE);

	pidgin_node = g_object_get_data(G_OBJECT(node), UI_DATA);

	return pidgin_node->contact_expanded;
}

void pidgin_blist_setup_sort_methods()
{
	const char *id;

	pidgin_blist_sort_method_reg("none", _("Manually"), sort_method_none);
	pidgin_blist_sort_method_reg("alphabetical", _("Alphabetically"), sort_method_alphabetical);
	pidgin_blist_sort_method_reg("status", _("By status"), sort_method_status);

	id = purple_prefs_get_string(PIDGIN_PREFS_ROOT "/blist/sort_type");
	if (id == NULL) {
		purple_debug_warning("gtkblist", "Sort method was NULL, resetting to alphabetical\n");
		id = "alphabetical";
	}
	pidgin_blist_sort_method_set(id);
}

static void _prefs_change_sort_method(const char *pref_name, PurplePrefType type,
									  gconstpointer val, gpointer data)
{
	if(purple_strequal(pref_name, PIDGIN_PREFS_ROOT "/blist/sort_type"))
		pidgin_blist_sort_method_set(val);
}

/* This assumes there are not things like groupless buddies or multi-leveled groups.
 * I'm sure other things in this code assumes that also.
 */
static void
treeview_style_set(GtkWidget *widget,
		    gpointer data)
{
	PurpleBuddyList *list = data;
	PurpleBlistNode *node = purple_blist_get_root(list);
	while (node) {
		pidgin_blist_update_group(list, node);
		node = node->next;
	}
}

/******************************************/
/* End of connection error handling stuff */
/******************************************/

/* builds the blist layout according to to the current theme */
static void
pidgin_blist_build_layout(PurpleBuddyList *list)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *rend;

	column = gtkblist->text_column;

	gtk_tree_view_column_clear(column);

	/* status icons */
	rend = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, rend, FALSE);
	gtk_tree_view_column_set_attributes(column, rend,
					    "pixbuf", STATUS_ICON_COLUMN,
					    "visible", STATUS_ICON_VISIBLE_COLUMN,
					    NULL);
	g_object_set(rend, "xalign", 0.0, "xpad", 6, "ypad", 0, NULL);

	/* name */
	gtkblist->text_rend = rend = gtk_cell_renderer_text_new();
	gtk_tree_view_column_pack_start(column, rend, TRUE);
	gtk_tree_view_column_set_attributes(column, rend,
	                                    "markup", NAME_COLUMN,
	                                    NULL);
	g_signal_connect(G_OBJECT(rend), "editing-started",
	                 G_CALLBACK(gtk_blist_renderer_editing_started_cb), list);
	g_signal_connect(G_OBJECT(rend), "editing-canceled",
	                 G_CALLBACK(gtk_blist_renderer_editing_cancelled_cb), list);
	g_signal_connect(G_OBJECT(rend), "edited",
	                 G_CALLBACK(gtk_blist_renderer_edited_cb), list);
	g_object_set(rend, "ypad", 0, "yalign", 0.5, NULL);
	g_object_set(rend, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	/* emblem */
	rend = gtk_cell_renderer_pixbuf_new();
	g_object_set(rend, "xalign", 1.0, "yalign", 0.5, "ypad", 0, "xpad", 3,
	             NULL);
	gtk_tree_view_column_pack_start(column, rend, FALSE);
	gtk_tree_view_column_set_attributes(column, rend,
	                                    "pixbuf", EMBLEM_COLUMN,
	                                    "visible", EMBLEM_VISIBLE_COLUMN, NULL);

	/* protocol icon */
	rend = gtk_cell_renderer_pixbuf_new();
	gtk_tree_view_column_pack_start(column, rend, FALSE);
	gtk_tree_view_column_set_attributes(column, rend,
	                                    "pixbuf", PROTOCOL_ICON_COLUMN,
	                                    NULL);
	g_object_set(rend, "xalign", 0.0, "xpad", 3, "ypad", 0, NULL);

	/* buddy icon */
	rend = gtk_cell_renderer_pixbuf_new();
	g_object_set(rend, "xalign", 1.0, "ypad", 0, NULL);
	gtk_tree_view_column_pack_start(column, rend, FALSE);
	gtk_tree_view_column_set_attributes(column, rend,
	                                    "pixbuf", BUDDY_ICON_COLUMN,
	                                    NULL);
}

static gboolean
pidgin_blist_search_equal_func(GtkTreeModel *model, gint column,
			const gchar *key, GtkTreeIter *iter, gpointer data)
{
	PurpleBlistNode *node = NULL;
	gboolean res = TRUE;
	const char *compare = NULL;

	if (!pidgin_tree_view_search_equal_func(model, column, key, iter, data))
		return FALSE;

	/* If the search string does not match the displayed label, then look
	 * at the alternate labels for the nodes and search in them. Currently,
	 * alternate labels that make sense are usernames/email addresses for
	 * buddies (but only for the ones who don't have a local alias).
	 */

	gtk_tree_model_get(model, iter, NODE_COLUMN, &node, -1);
	if (!node)
		return TRUE;

	compare = NULL;
	if (PURPLE_IS_CONTACT(node)) {
		PurpleBuddy *b = purple_contact_get_priority_buddy(PURPLE_CONTACT(node));
		if (!purple_buddy_get_local_alias(b))
			compare = purple_buddy_get_name(b);
	} else if (PURPLE_IS_BUDDY(node)) {
		if (!purple_buddy_get_local_alias(PURPLE_BUDDY(node)))
			compare = purple_buddy_get_name(PURPLE_BUDDY(node));
	}

	if (compare) {
		char *tmp, *enteredstring;
		tmp = g_utf8_normalize(key, -1, G_NORMALIZE_DEFAULT);
		enteredstring = g_utf8_casefold(tmp, -1);
		g_free(tmp);

		if (g_str_has_prefix(compare, enteredstring)) {
			res = FALSE;
		}
		g_free(enteredstring);
	}

	return res;
}

static void
pidgin_blist_populate_menus(void) {
	GtkApplication *application = NULL;
	GMenu *source = NULL, *target = NULL;

	application = GTK_APPLICATION(g_application_get_default());

	/* Add the icon menu to all the menus that need it. */
	source = gtk_application_get_menu_by_id(application, "custom-icon");

	/* The group context menu. */
	target = gtk_application_get_menu_by_id(application, "group-custom-icon");
	g_menu_append_section(target, NULL, G_MENU_MODEL(source));

	/* The chat context menu. */
	target = gtk_application_get_menu_by_id(application, "chat-custom-icon");
	g_menu_append_section(target, NULL, G_MENU_MODEL(source));

	/* The contact context menu. */
	target = gtk_application_get_menu_by_id(application, "contact-custom-icon");
	g_menu_append_section(target, NULL, G_MENU_MODEL(source));

	/* The buddy context menu. */
	target = gtk_application_get_menu_by_id(application, "buddy-custom-icon");
	g_menu_append_section(target, NULL, G_MENU_MODEL(source));

	/* Add the voice and video menu to the buddy menu. */
	source = gtk_application_get_menu_by_id(application, "voice-video");
	target = gtk_application_get_menu_by_id(application, "buddy-voice-video");
	g_menu_append_section(target, NULL, G_MENU_MODEL(source));
}

static void pidgin_blist_show(PurpleBuddyList *list)
{
	GSimpleActionGroup *action_group = NULL;
	void *handle;
	GtkWidget *sep;
	GtkEventController *key_controller = NULL;
	GtkTreeSelection *selection;

	gtkblist = PIDGIN_BUDDY_LIST(list);

	gtkblist->window = pidgin_contact_list_window_new();

	/* the main vbox is already packed and shown via glade, we just need a
	 * reference to it to pack the rest of our widgets here.
	 */
	gtkblist->vbox = pidgin_contact_list_window_get_vbox(PIDGIN_CONTACT_LIST_WINDOW(gtkblist->window));

	/****************************** GtkTreeView **********************************/
	gtkblist->treemodel = gtk_tree_store_new(BLIST_COLUMNS,
						 GDK_TYPE_PIXBUF, /* Status icon */
						 G_TYPE_BOOLEAN,  /* Status icon visible */
						 G_TYPE_STRING,   /* Name */
						 GDK_TYPE_PIXBUF, /* Buddy icon */
						 G_TYPE_POINTER,  /* Node */
						 GDK_TYPE_PIXBUF, /* Emblem */
						 G_TYPE_BOOLEAN,  /* Emblem visible */
						 GDK_TYPE_PIXBUF /* Protocol icon */
						);

	gtkblist->treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(gtkblist->treemodel));

	gtk_widget_show(gtkblist->treeview);
	gtk_widget_set_name(gtkblist->treeview, "pidgin_blist_treeview");

	g_signal_connect(gtkblist->treeview,
			 "style-updated",
			 G_CALLBACK(treeview_style_set), list);
	/* Set up selection stuff */
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkblist->treeview));
	g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(pidgin_blist_selection_changed), NULL);

	/* Tooltips */
	gtk_widget_set_has_tooltip(gtkblist->treeview, TRUE);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "query-tooltip",
	                 G_CALLBACK(pidgin_blist_query_tooltip), gtkblist);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(gtkblist->treeview), FALSE);

	/* everything else column */
	gtkblist->text_column = gtk_tree_view_column_new ();
	gtk_tree_view_append_column(GTK_TREE_VIEW(gtkblist->treeview), gtkblist->text_column);
	pidgin_blist_build_layout(list);

	g_signal_connect(G_OBJECT(gtkblist->treeview), "row-activated",
	                 G_CALLBACK(gtk_blist_row_activated_cb), gtkblist);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "row-expanded",
	                 G_CALLBACK(gtk_blist_row_expanded_cb), gtkblist);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "row-collapsed",
	                 G_CALLBACK(gtk_blist_row_collapsed_cb), gtkblist);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "button-press-event", G_CALLBACK(gtk_blist_button_press_cb), NULL);
	key_controller = gtk_event_controller_key_new(gtkblist->treeview);
	g_signal_connect(G_OBJECT(key_controller), "key-pressed",
	                 G_CALLBACK(pidgin_blist_key_press_cb), gtkblist);
	g_object_set_data_full(G_OBJECT(gtkblist->treeview), "key-controller",
	                       key_controller, g_object_unref);
	g_signal_connect(G_OBJECT(gtkblist->treeview), "popup-menu", G_CALLBACK(pidgin_blist_popup_menu_cb), NULL);

	/* Enable CTRL+F searching */
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(gtkblist->treeview), NAME_COLUMN);
	gtk_tree_view_set_search_equal_func(GTK_TREE_VIEW(gtkblist->treeview),
			pidgin_blist_search_equal_func, NULL, NULL);

	gtk_box_pack_start(GTK_BOX(gtkblist->vbox),
		pidgin_make_scrollable(gtkblist->treeview, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC, -1, -1),
		TRUE, TRUE, 0);

	sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_box_pack_start(GTK_BOX(gtkblist->vbox), sep, FALSE, FALSE, 0);

	/* Update some dynamic things */
	pidgin_blist_update_sort_methods();

	/* OK... let's show this bad boy. */
	pidgin_blist_refresh(list);
	gtk_widget_show_all(GTK_WIDGET(gtkblist->vbox));
	purple_blist_set_visible(TRUE);

	handle = pidgin_blist_get_handle();

	/* sorting */
	purple_prefs_connect_callback(handle, PIDGIN_PREFS_ROOT "/blist/sort_type",
			_prefs_change_sort_method, NULL);

	/* Setup some purple signal handlers. */

	handle = purple_conversations_get_handle();
	purple_signal_connect(handle, "conversation-updated", gtkblist,
	                      G_CALLBACK(conversation_updated_cb),
	                      gtkblist);
	purple_signal_connect(handle, "deleting-conversation", gtkblist,
	                      G_CALLBACK(conversation_deleting_cb),
	                      gtkblist);
	purple_signal_connect(handle, "conversation-created", gtkblist,
	                      G_CALLBACK(conversation_created_cb),
	                      gtkblist);
	purple_signal_connect(handle, "chat-joined", gtkblist,
	                      G_CALLBACK(conversation_created_cb),
	                      gtkblist);

	/* emit our created signal */
	handle = pidgin_blist_get_handle();
	purple_signal_emit(handle, "gtkblist-created", list);

	action_group = g_simple_action_group_new();
	g_action_map_add_action_entries(G_ACTION_MAP(action_group), menu_actions,
	                                G_N_ELEMENTS(menu_actions), gtkblist);
	gtk_widget_insert_action_group(gtkblist->treeview, "menu",
	                               G_ACTION_GROUP(action_group));

	pidgin_blist_populate_menus();
}

static void redo_buddy_list(PurpleBuddyList *list, gboolean remove, gboolean rerender)
{
	PurpleBlistNode *node;

	gtkblist = PIDGIN_BUDDY_LIST(list);
	if(!gtkblist || !gtkblist->treeview)
		return;

	node = purple_blist_get_root(list);

	while (node)
	{
		/* This is only needed when we're reverting to a non-GTK sorted
		 * status.  We shouldn't need to remove otherwise.
		 */
		if (remove && !PURPLE_IS_GROUP(node))
			pidgin_blist_hide_node(list, node, FALSE);

		if (PURPLE_IS_BUDDY(node))
			pidgin_blist_update_buddy(list, node, rerender);
		else if (PURPLE_IS_CHAT(node))
			pidgin_blist_update(list, node);
		else if (PURPLE_IS_GROUP(node))
			pidgin_blist_update(list, node);
		node = purple_blist_node_next(node, FALSE);
	}

}

void pidgin_blist_refresh(PurpleBuddyList *list)
{
	redo_buddy_list(list, FALSE, TRUE);
}

static gboolean get_iter_from_node(PurpleBlistNode *node, GtkTreeIter *iter) {
	PidginBlistNode *gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);
	GtkTreePath *path;

	if (!gtknode) {
		return FALSE;
	}

	if (!gtkblist) {
		purple_debug_error("gtkblist", "get_iter_from_node was called, but we don't seem to have a blist\n");
		return FALSE;
	}

	if (!gtknode->row)
		return FALSE;

	if (!GTK_IS_TREE_MODEL(gtkblist->treemodel)) {
		return FALSE;
	}

	if ((path = gtk_tree_row_reference_get_path(gtknode->row)) == NULL)
		return FALSE;

	if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(gtkblist->treemodel), iter, path)) {
		gtk_tree_path_free(path);
		return FALSE;
	}
	gtk_tree_path_free(path);
	return TRUE;
}

static void
pidgin_blist_remove(PurpleBuddyList *list, PurpleBlistNode *node) {
	purple_request_close_with_handle(node);

	pidgin_blist_hide_node(list, node, TRUE);

	if(node->parent) {
		pidgin_blist_update(list, node->parent);
	}
}

static gboolean do_selection_changed(PurpleBlistNode *new_selection)
{
	PurpleBlistNode *old_selection = NULL;

	/* test for gtkblist because crazy timeout means we can be called after the blist is gone */
	if (gtkblist && new_selection != gtkblist->selected_node) {
		old_selection = gtkblist->selected_node;
		gtkblist->selected_node = new_selection;
		if(new_selection)
			pidgin_blist_update(NULL, new_selection);
		if(old_selection)
			pidgin_blist_update(NULL, old_selection);
	}

	return FALSE;
}

static void pidgin_blist_selection_changed(GtkTreeSelection *selection, gpointer data)
{
	PurpleBlistNode *new_selection = NULL;
	GtkTreeIter iter;

	if(gtk_tree_selection_get_selected(selection, NULL, &iter)){
		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter,
				NODE_COLUMN, &new_selection, -1);
	}

	/* we set this up as a timeout, otherwise the blist flickers ...
	 * but we don't do it for groups, because it causes total bizarness -
	 * the previously selected buddy node might rendered at half height.
	 */
	if ((new_selection != NULL) && PURPLE_IS_GROUP(new_selection)) {
		do_selection_changed(new_selection);
	} else {
		g_timeout_add(0, (GSourceFunc)do_selection_changed, new_selection);
	}
}

static gboolean insert_node(PurpleBuddyList *list, PurpleBlistNode *node, GtkTreeIter *iter)
{
	GtkTreeIter parent_iter = {0, NULL, NULL, NULL}, cur, *curptr = NULL;
	PidginBlistNode *gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);
	GtkTreePath *newpath;

	if(!iter)
		return FALSE;

	/* XXX: it's not necessary, but let's silence a warning*/
	memset(&parent_iter, 0, sizeof(parent_iter));

	if(node->parent && !get_iter_from_node(node->parent, &parent_iter))
		return FALSE;

	if(get_iter_from_node(node, &cur))
		curptr = &cur;

	if(PURPLE_IS_CONTACT(node) || PURPLE_IS_CHAT(node)) {
		current_sort_method->func(node, list, parent_iter, curptr, iter);
	} else {
		sort_method_none(node, list, parent_iter, curptr, iter);
	}

	if(gtknode != NULL) {
		gtk_tree_row_reference_free(gtknode->row);
	} else {
		pidgin_blist_new_node(list, node);
		gtknode = g_object_get_data(G_OBJECT(node), UI_DATA);
	}

	newpath = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel),
			iter);
	gtknode->row =
		gtk_tree_row_reference_new(GTK_TREE_MODEL(gtkblist->treemodel),
				newpath);

	gtk_tree_path_free(newpath);

	if (!editing_blist)
		gtk_tree_store_set(gtkblist->treemodel, iter,
				NODE_COLUMN, node,
				-1);

	if(node->parent) {
		GtkTreePath *expand = NULL;
		PidginBlistNode *gtkparentnode = g_object_get_data(G_OBJECT(node->parent), UI_DATA);

		if(PURPLE_IS_GROUP(node->parent)) {
			if(!purple_blist_node_get_bool(node->parent, "collapsed"))
				expand = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &parent_iter);
		} else if(PURPLE_IS_CONTACT(node->parent) &&
				gtkparentnode->contact_expanded) {
			expand = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &parent_iter);
		}
		if(expand) {
			gtk_tree_view_expand_row(GTK_TREE_VIEW(gtkblist->treeview), expand, FALSE);
			gtk_tree_path_free(expand);
		}
	}

	return TRUE;
}

/* This version of pidgin_blist_update_group can take the original buddy or a
 * group, but has much better algorithmic performance with a pre-known buddy.
 */
static void pidgin_blist_update_group(PurpleBuddyList *list,
                                      PurpleBlistNode *node)
{
	PurpleBlistNode* gnode;
	gboolean show = FALSE;

	g_return_if_fail(node != NULL);

	if (editing_blist)
		return;

	if (PURPLE_IS_GROUP(node))
		gnode = node;
	else if (PURPLE_IS_BUDDY(node))
		gnode = node->parent->parent;
	else if (PURPLE_IS_CONTACT(node) || PURPLE_IS_CHAT(node))
		gnode = node->parent;
	else
		return;

	show = TRUE;

	if (show) {
		gchar *title;
		GtkTreeIter iter;
		GtkTreePath *path;
		gboolean expanded;
		GdkPixbuf *avatar = NULL;

		if(!insert_node(list, gnode, &iter))
			return;

		path = gtk_tree_model_get_path(GTK_TREE_MODEL(gtkblist->treemodel), &iter);
		expanded = gtk_tree_view_row_expanded(GTK_TREE_VIEW(gtkblist->treeview), path);
		gtk_tree_path_free(path);

		title = pidgin_get_group_title(gnode, expanded);
		avatar = pidgin_blist_get_buddy_icon(gnode, TRUE, TRUE);

		gtk_tree_store_set(gtkblist->treemodel, &iter,
				   STATUS_ICON_VISIBLE_COLUMN, FALSE,
				   STATUS_ICON_COLUMN, NULL,
				   NAME_COLUMN, title,
				   NODE_COLUMN, gnode,
				   BUDDY_ICON_COLUMN, avatar,
				   EMBLEM_VISIBLE_COLUMN, FALSE,
				   -1);
		g_free(title);
	} else {
		pidgin_blist_hide_node(list, gnode, TRUE);
	}
}

static char *pidgin_get_group_title(PurpleBlistNode *gnode, gboolean expanded)
{
	PurpleGroup *group;
	char group_count[12] = "";
	char *mark, *esc;
	PurpleBlistNode *selected_node = NULL;
	GtkTreeIter iter;

	group = (PurpleGroup*)gnode;

	if (gtk_tree_selection_get_selected(gtk_tree_view_get_selection(GTK_TREE_VIEW(gtkblist->treeview)), NULL, &iter)) {
		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &iter,
				NODE_COLUMN, &selected_node, -1);
	}

	if (!expanded) {
		g_snprintf(group_count, sizeof(group_count), "%d/%d",
		           purple_counting_node_get_online_count(PURPLE_COUNTING_NODE(group)),
		           purple_counting_node_get_current_size(PURPLE_COUNTING_NODE(group)));
	}

	esc = g_markup_escape_text(purple_group_get_name(group), -1);

	mark = g_strdup_printf("<b>%s</b>%s%s%s",
	                       esc ? esc : "",
	                       !expanded ? " <span weight='light'>(</span>" : "",
	                       group_count,
	                       !expanded ? "<span weight='light'>)</span>" : "");

	g_free(esc);
	return mark;
}

static void buddy_node(PurpleBuddy *buddy, GtkTreeIter *iter, PurpleBlistNode *node)
{
	PurplePresence *presence = purple_buddy_get_presence(buddy);
	GdkPixbuf *status, *avatar, *emblem, *protocol_icon;
	char *mark;
	char *idle = NULL;
	gboolean selected = (gtkblist->selected_node == node);

	if(editing_blist) {
		return;
	}

	status = pidgin_blist_get_status_icon(PURPLE_BLIST_NODE(buddy),
	                                      PIDGIN_STATUS_ICON_LARGE);
	avatar = pidgin_blist_get_buddy_icon(PURPLE_BLIST_NODE(buddy), TRUE, TRUE);

	if(avatar != NULL) {
		if(!PURPLE_BUDDY_IS_ONLINE(buddy) || purple_presence_is_idle(presence)) {
			do_alphashift(avatar, 77);
		}
	}

	emblem = pidgin_blist_get_emblem(PURPLE_BLIST_NODE(buddy));
	mark = pidgin_blist_get_name_markup(buddy, selected, TRUE);

	protocol_icon = pidgin_create_protocol_icon(purple_buddy_get_account(buddy), PIDGIN_PROTOCOL_ICON_SMALL);

	gtk_tree_store_set(gtkblist->treemodel, iter,
			   STATUS_ICON_COLUMN, status,
			   STATUS_ICON_VISIBLE_COLUMN, TRUE,
			   NAME_COLUMN, mark,
			   BUDDY_ICON_COLUMN, avatar,
			   EMBLEM_COLUMN, emblem,
			   EMBLEM_VISIBLE_COLUMN, (emblem != NULL),
			   PROTOCOL_ICON_COLUMN, protocol_icon,
			-1);

	g_free(mark);
	g_free(idle);
	if(emblem)
		g_object_unref(emblem);
	if(status)
		g_object_unref(status);
	if(avatar)
		g_object_unref(avatar);
	if(protocol_icon)
		g_object_unref(protocol_icon);
}

/* This is a variation on the original gtk_blist_update_contact. Here we
	can know in advance which buddy has changed so we can just update that */
static void pidgin_blist_update_contact(PurpleBuddyList *list, PurpleBlistNode *node)
{
	PurpleBlistNode *cnode;
	PurpleContact *contact;
	PurpleBuddy *buddy;
	PidginBlistNode *gtknode;

	if (editing_blist)
		return;

	if (PURPLE_IS_BUDDY(node))
		cnode = node->parent;
	else
		cnode = node;

	g_return_if_fail(PURPLE_IS_CONTACT(cnode));

	/* First things first, update the group */
	if (PURPLE_IS_BUDDY(node))
		pidgin_blist_update_group(list, node);
	else
		pidgin_blist_update_group(list, cnode->parent);

	contact = (PurpleContact*)cnode;
	buddy = purple_contact_get_priority_buddy(contact);

	if (buddy_is_displayable(buddy))
	{
		GtkTreeIter iter;

		if(!insert_node(list, cnode, &iter))
			return;

		gtknode = g_object_get_data(G_OBJECT(cnode), UI_DATA);

		if(gtknode->contact_expanded) {
			GdkPixbuf *status;
			gchar *mark;

			mark = g_markup_escape_text(purple_contact_get_alias(contact), -1);

			status = pidgin_blist_get_status_icon(cnode,
			                                      PIDGIN_STATUS_ICON_LARGE);

			gtk_tree_store_set(gtkblist->treemodel, &iter,
					   STATUS_ICON_COLUMN, status,
					   STATUS_ICON_VISIBLE_COLUMN, TRUE,
					   NAME_COLUMN, mark,
					   BUDDY_ICON_COLUMN, NULL,
					-1);
			g_free(mark);
			if(status)
				g_object_unref(status);
		} else {
			buddy_node(buddy, &iter, cnode);
		}
	} else {
		pidgin_blist_hide_node(list, cnode, TRUE);
	}
}



static void pidgin_blist_update_buddy(PurpleBuddyList *list, PurpleBlistNode *node, gboolean status_change)
{
	PurpleBuddy *buddy;
	PidginBlistNode *gtkparentnode;

	g_return_if_fail(PURPLE_IS_BUDDY(node));

	if (node->parent == NULL)
		return;

	buddy = (PurpleBuddy*)node;

	/* First things first, update the contact */
	pidgin_blist_update_contact(list, node);

	gtkparentnode = g_object_get_data(G_OBJECT(node->parent), UI_DATA);

	if (gtkparentnode->contact_expanded && buddy_is_displayable(buddy))
	{
		GtkTreeIter iter;

		if (!insert_node(list, node, &iter))
			return;

		buddy_node(buddy, &iter, node);

	} else {
		pidgin_blist_hide_node(list, node, TRUE);
	}

}

static void pidgin_blist_update_chat(PurpleBuddyList *list, PurpleBlistNode *node)
{
	PurpleChat *chat;

	g_return_if_fail(PURPLE_IS_CHAT(node));

	if (editing_blist)
		return;

	/* First things first, update the group */
	pidgin_blist_update_group(list, node->parent);

	chat = (PurpleChat*)node;

	if(purple_account_is_connected(purple_chat_get_account(chat))) {
		GtkTreeIter iter;
		GdkPixbuf *status, *avatar, *emblem, *protocol_icon;
		const gchar *color = NULL;
		gchar *mark, *tmp;
		gboolean selected = (gtkblist->selected_node == node);
		gboolean nick_said = FALSE;

		if (!insert_node(list, node, &iter))
			return;

		status = pidgin_blist_get_status_icon(node, PIDGIN_STATUS_ICON_LARGE);
		emblem = pidgin_blist_get_emblem(node);
		avatar = pidgin_blist_get_buddy_icon(node, TRUE, FALSE);

		mark = g_markup_escape_text(purple_chat_get_name(chat), -1);

		if(selected ) {
			/* nick_said color is the same as gtkconv:tab-label-attention */
			color = (nick_said ? "#006aff" : NULL);
		}

		if(color) {
			tmp = g_strdup_printf("<span color='%s'>%s</span>", color, mark);
			g_free(mark);
			mark = tmp;
		}

		protocol_icon = pidgin_create_protocol_icon(purple_chat_get_account(chat), PIDGIN_PROTOCOL_ICON_SMALL);

		gtk_tree_store_set(gtkblist->treemodel, &iter,
				STATUS_ICON_COLUMN, status,
				STATUS_ICON_VISIBLE_COLUMN, TRUE,
				BUDDY_ICON_COLUMN, avatar,
				EMBLEM_COLUMN, emblem,
				EMBLEM_VISIBLE_COLUMN, emblem != NULL,
				PROTOCOL_ICON_COLUMN, protocol_icon,
				NAME_COLUMN, mark,
				-1);

		g_free(mark);
		if(emblem)
			g_object_unref(emblem);
		if(status)
			g_object_unref(status);
		if(avatar)
			g_object_unref(avatar);
		if(protocol_icon)
			g_object_unref(protocol_icon);

	} else {
		pidgin_blist_hide_node(list, node, TRUE);
	}
}

static void pidgin_blist_update(PurpleBuddyList *list, PurpleBlistNode *node)
{
	if(list) {
		gtkblist = PIDGIN_BUDDY_LIST(list);
	}
	if(!gtkblist || !gtkblist->treeview || !node) {
		return;
	}

	if(g_object_get_data(G_OBJECT(node), UI_DATA) == NULL) {
		pidgin_blist_new_node(list, node);
	}

	if(PURPLE_IS_GROUP(node)) {
		pidgin_blist_update_group(list, node);
	} else if(PURPLE_IS_CONTACT(node)) {
		pidgin_blist_update_contact(list, node);
	} else if(PURPLE_IS_BUDDY(node)) {
		pidgin_blist_update_buddy(list, node, TRUE);
	} else if(PURPLE_IS_CHAT(node)) {
		pidgin_blist_update_chat(list, node);
	}
}

static void pidgin_blist_set_visible(PurpleBuddyList *list, gboolean show)
{
	if (!(gtkblist && gtkblist->window))
		return;

	if (show) {
		gtk_widget_show(gtkblist->window);
	} else {
		if (!gtk_widget_get_visible(gtkblist->window))
			gtk_widget_show(gtkblist->window);
		gtk_window_iconify(GTK_WINDOW(gtkblist->window));
	}
}

static void
pidgin_blist_request_add_buddy(PurpleBuddyList *list, PurpleAccount *account,
                               const char *username, const char *group,
                               const char *alias)
{
	GtkWidget *dialog = NULL;

	dialog = pidgin_add_buddy_dialog_new(account, username, alias, NULL, group);

	gtk_widget_show(dialog);
}

static void
pidgin_blist_request_add_chat(PurpleBuddyList *list, PurpleAccount *account,
                              PurpleGroup *group, const char *alias,
                              const char *name)
{
	GtkWidget *dialog = NULL;

	dialog = pidgin_add_chat_dialog_new(account, group, alias, name);

	gtk_widget_show(dialog);
}

static void
add_group_cb(PurpleConnection *gc, const char *group_name)
{
	PurpleGroup *group;

	if ((group_name == NULL) || (*group_name == '\0'))
		return;

	group = purple_group_new(group_name);
	purple_blist_add_group(group, NULL);
}

static void
pidgin_blist_request_add_group(PurpleBuddyList *list)
{
	purple_request_input(NULL, _("Add Group"), NULL,
					   _("Please enter the name of the group to be added."),
					   NULL, FALSE, FALSE, NULL,
					   _("Add"), G_CALLBACK(add_group_cb),
					   _("Cancel"), NULL,
					   NULL, NULL);
}

PidginBuddyList *
pidgin_blist_get_default_gtk_blist(void)
{
	return gtkblist;
}

static gboolean autojoin_cb(PurpleConnection *gc, gpointer data)
{
	PurpleAccount *account = purple_connection_get_account(gc);
	PurpleBlistNode *gnode, *cnode;
	for (gnode = purple_blist_get_default_root(); gnode;
	     gnode = gnode->next) {
		if(!PURPLE_IS_GROUP(gnode))
			continue;
		for(cnode = gnode->child; cnode; cnode = cnode->next)
		{
			PurpleChat *chat;

			if(!PURPLE_IS_CHAT(cnode))
				continue;

			chat = (PurpleChat *)cnode;

			if(purple_chat_get_account(chat) != account)
				continue;

			if (purple_blist_node_get_bool(PURPLE_BLIST_NODE(chat), "gtk-autojoin"))
				purple_serv_join_chat(gc, purple_chat_get_components(chat));
		}
	}

	/* Stop processing; we handled the autojoins. */
	return TRUE;
}

void *
pidgin_blist_get_handle() {
	static int handle;

	return &handle;
}

static gboolean buddy_signonoff_timeout_cb(PurpleBuddy *buddy)
{
	PidginBlistNode *gtknode = g_object_get_data(G_OBJECT(buddy), UI_DATA);
	PurpleAccount *account = NULL;

	gtknode->recent_signonoff = FALSE;
	gtknode->recent_signonoff_timer = 0;

	account = purple_buddy_get_account(buddy);
	if(purple_account_is_connected(account)) {
		pidgin_blist_update(NULL, PURPLE_BLIST_NODE(buddy));
	}

	return FALSE;
}

static void
buddy_signonoff_cb(PurpleBuddy *buddy) {
	PidginBlistNode *gtknode = g_object_get_data(G_OBJECT(buddy), UI_DATA);

	if(gtknode == NULL) {
		pidgin_blist_new_node(purple_blist_get_default(),
		                      PURPLE_BLIST_NODE(buddy));

		gtknode = g_object_get_data(G_OBJECT(buddy), UI_DATA);
	}

	gtknode->recent_signonoff = TRUE;

	if(gtknode->recent_signonoff_timer > 0)
		g_source_remove(gtknode->recent_signonoff_timer);

	g_object_ref(buddy);
	gtknode->recent_signonoff_timer = g_timeout_add_seconds_full(
			G_PRIORITY_DEFAULT, 10,
			(GSourceFunc)buddy_signonoff_timeout_cb,
			buddy, g_object_unref);
}

void pidgin_blist_init(void)
{
	void *gtk_blist_handle = pidgin_blist_get_handle();

	cached_emblems = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	/* Remove old prefs */
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/show_buddy_icons");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/show_empty_groups");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/show_idle_time");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/show_offline_buddies");
	purple_prefs_remove(PIDGIN_PREFS_ROOT "/blist/show_protocol_icons");

	/* Initialize prefs */
	purple_prefs_add_none(PIDGIN_PREFS_ROOT "/blist");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/blist/sort_type", "alphabetical");
	purple_prefs_add_string(PIDGIN_PREFS_ROOT "/blist/theme", "");

	/* Register our signals */
	purple_signal_register(gtk_blist_handle, "gtkblist-created",
	                     purple_marshal_VOID__POINTER, G_TYPE_NONE, 1,
	                     PURPLE_TYPE_BUDDY_LIST);

	purple_signal_register(gtk_blist_handle, "drawing-tooltip",
	                     purple_marshal_VOID__POINTER_POINTER_UINT, G_TYPE_NONE,
	                     3, PURPLE_TYPE_BLIST_NODE,
	                     G_TYPE_POINTER, /* pointer to a (GString *) */
	                     G_TYPE_BOOLEAN);

	purple_signal_connect(purple_blist_get_handle(), "buddy-signed-on",
			gtk_blist_handle, G_CALLBACK(buddy_signonoff_cb), NULL);
	purple_signal_connect(purple_blist_get_handle(), "buddy-signed-off",
			gtk_blist_handle, G_CALLBACK(buddy_signonoff_cb), NULL);
	purple_signal_connect(purple_blist_get_handle(), "buddy-privacy-changed",
			gtk_blist_handle, G_CALLBACK(pidgin_blist_update_privacy_cb), NULL);

	purple_signal_connect_priority(purple_connections_get_handle(), "autojoin",
	                               gtk_blist_handle, G_CALLBACK(autojoin_cb),
	                               NULL, PURPLE_SIGNAL_PRIORITY_HIGHEST);
}

void
pidgin_blist_uninit(void) {
	g_hash_table_destroy(cached_emblems);

	purple_signals_unregister_by_instance(pidgin_blist_get_handle());
	purple_signals_disconnect_by_handle(pidgin_blist_get_handle());

	gtkblist = NULL;
}

/**************************************************************************
 * GTK Buddy list GObject code
 **************************************************************************/
static void
pidgin_buddy_list_init(PidginBuddyList *self)
{
}

static void
pidgin_buddy_list_finalize(GObject *obj)
{
	PidginBuddyList *gtkblist = PIDGIN_BUDDY_LIST(obj);

	purple_signals_disconnect_by_handle(gtkblist);

	gtk_widget_destroy(gtkblist->window);

	gtkblist->window = gtkblist->vbox = gtkblist->treeview = NULL;
	g_clear_object(&gtkblist->treemodel);

	purple_prefs_disconnect_by_handle(pidgin_blist_get_handle());

	G_OBJECT_CLASS(pidgin_buddy_list_parent_class)->finalize(obj);
}

static void
pidgin_buddy_list_class_init(PidginBuddyListClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	PurpleBuddyListClass *purple_blist_class;

	obj_class->finalize = pidgin_buddy_list_finalize;

	purple_blist_class = PURPLE_BUDDY_LIST_CLASS(klass);
	purple_blist_class->new_node = pidgin_blist_new_node;
	purple_blist_class->show = pidgin_blist_show;
	purple_blist_class->update = pidgin_blist_update;
	purple_blist_class->remove = pidgin_blist_remove;
	purple_blist_class->set_visible = pidgin_blist_set_visible;
	purple_blist_class->request_add_buddy = pidgin_blist_request_add_buddy;
	purple_blist_class->request_add_chat = pidgin_blist_request_add_chat;
	purple_blist_class->request_add_group = pidgin_blist_request_add_group;
}

/*********************************************************************
 * Buddy List sorting functions                                      *
 *********************************************************************/

GList *pidgin_blist_get_sort_methods()
{
	return pidgin_blist_sort_methods;
}

void pidgin_blist_sort_method_reg(const char *id, const char *name, pidgin_blist_sort_function func)
{
	struct _PidginBlistSortMethod *method;

	g_return_if_fail(id != NULL);
	g_return_if_fail(name != NULL);
	g_return_if_fail(func != NULL);

	method = g_new0(struct _PidginBlistSortMethod, 1);
	method->id = g_strdup(id);
	method->name = g_strdup(name);
	method->func = func;
	pidgin_blist_sort_methods = g_list_append(pidgin_blist_sort_methods, method);
	pidgin_blist_update_sort_methods();
}

void pidgin_blist_sort_method_unreg(const char *id)
{
	GList *l = pidgin_blist_sort_methods;

	g_return_if_fail(id != NULL);

	while(l) {
		struct _PidginBlistSortMethod *method = l->data;
		if(purple_strequal(method->id, id)) {
			pidgin_blist_sort_methods = g_list_delete_link(pidgin_blist_sort_methods, l);
			g_free(method->id);
			g_free(method->name);
			g_free(method);
			break;
		}
		l = l->next;
	}
	pidgin_blist_update_sort_methods();
}

void pidgin_blist_sort_method_set(const char *id){
	GList *l = pidgin_blist_sort_methods;

	if(!id)
		id = "none";

	while (l && !purple_strequal(((struct _PidginBlistSortMethod*)l->data)->id, id))
		l = l->next;

	if (l) {
		current_sort_method = l->data;
	} else if (!current_sort_method) {
		pidgin_blist_sort_method_set("none");
		return;
	}
	if (purple_strequal(id, "none")) {
		redo_buddy_list(purple_blist_get_default(), TRUE, FALSE);
	} else {
		redo_buddy_list(purple_blist_get_default(), FALSE, FALSE);
	}
}

/******************************************
 ** Sort Methods
 ******************************************/

static void sort_method_none(PurpleBlistNode *node, PurpleBuddyList *blist, GtkTreeIter parent_iter, GtkTreeIter *cur, GtkTreeIter *iter)
{
	PurpleBlistNode *sibling = node->prev;
	GtkTreeIter sibling_iter;

	if (cur != NULL) {
		*iter = *cur;
		return;
	}

	while (sibling && !get_iter_from_node(sibling, &sibling_iter)) {
		sibling = sibling->prev;
	}

	gtk_tree_store_insert_after(gtkblist->treemodel, iter,
			node->parent ? &parent_iter : NULL,
			sibling ? &sibling_iter : NULL);
}

static void sort_method_alphabetical(PurpleBlistNode *node, PurpleBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter)
{
	GtkTreeIter more_z;

	const char *my_name;

	if(PURPLE_IS_CONTACT(node)) {
		my_name = purple_contact_get_alias((PurpleContact*)node);
	} else if(PURPLE_IS_CHAT(node)) {
		my_name = purple_chat_get_name((PurpleChat*)node);
	} else {
		sort_method_none(node, blist, groupiter, cur, iter);
		return;
	}

	if (!gtk_tree_model_iter_children(GTK_TREE_MODEL(gtkblist->treemodel), &more_z, &groupiter)) {
		gtk_tree_store_insert(gtkblist->treemodel, iter, &groupiter, 0);
		return;
	}

	do {
		PurpleBlistNode *n;
		const char *this_name;
		int cmp;

		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &more_z, NODE_COLUMN, &n, -1);

		if(PURPLE_IS_CONTACT(n)) {
			this_name = purple_contact_get_alias((PurpleContact*)n);
		} else if(PURPLE_IS_CHAT(n)) {
			this_name = purple_chat_get_name((PurpleChat*)n);
		} else {
			this_name = NULL;
		}

		cmp = purple_utf8_strcasecmp(my_name, this_name);

		if(this_name && (cmp < 0 || (cmp == 0 && node < n))) {
			if(cur) {
				gtk_tree_store_move_before(gtkblist->treemodel, cur, &more_z);
				*iter = *cur;
				return;
			} else {
				gtk_tree_store_insert_before(gtkblist->treemodel, iter,
						&groupiter, &more_z);
				return;
			}
		}
	} while (gtk_tree_model_iter_next (GTK_TREE_MODEL(gtkblist->treemodel), &more_z));

	if(cur) {
		gtk_tree_store_move_before(gtkblist->treemodel, cur, NULL);
		*iter = *cur;
		return;
	} else {
		gtk_tree_store_append(gtkblist->treemodel, iter, &groupiter);
		return;
	}
}

static void sort_method_status(PurpleBlistNode *node, PurpleBuddyList *blist, GtkTreeIter groupiter, GtkTreeIter *cur, GtkTreeIter *iter)
{
	GtkTreeIter more_z;

	PurpleBuddy *my_buddy, *this_buddy;

	if(PURPLE_IS_CONTACT(node)) {
		my_buddy = purple_contact_get_priority_buddy((PurpleContact*)node);
	} else if(PURPLE_IS_CHAT(node)) {
		if (cur != NULL) {
			*iter = *cur;
			return;
		}

		gtk_tree_store_append(gtkblist->treemodel, iter, &groupiter);
		return;
	} else {
		sort_method_alphabetical(node, blist, groupiter, cur, iter);
		return;
	}


	if (!gtk_tree_model_iter_children(GTK_TREE_MODEL(gtkblist->treemodel), &more_z, &groupiter)) {
		gtk_tree_store_insert(gtkblist->treemodel, iter, &groupiter, 0);
		return;
	}

	do {
		PurpleBlistNode *n;
		gint name_cmp;
		gint presence_cmp;

		gtk_tree_model_get(GTK_TREE_MODEL(gtkblist->treemodel), &more_z, NODE_COLUMN, &n, -1);

		if(PURPLE_IS_CONTACT(n)) {
			this_buddy = purple_contact_get_priority_buddy((PurpleContact*)n);
		} else {
			this_buddy = NULL;
		}

		name_cmp = purple_utf8_strcasecmp(
			purple_contact_get_alias(purple_buddy_get_contact(my_buddy)),
			(this_buddy
			 ? purple_contact_get_alias(purple_buddy_get_contact(this_buddy))
			 : NULL));

		presence_cmp = purple_buddy_presence_compare(
			PURPLE_BUDDY_PRESENCE(purple_buddy_get_presence(my_buddy)),
			this_buddy ? PURPLE_BUDDY_PRESENCE(purple_buddy_get_presence(this_buddy)) : NULL);

		if (this_buddy == NULL ||
			(presence_cmp < 0 ||
			 (presence_cmp == 0 &&
			  (name_cmp < 0 || (name_cmp == 0 && node < n)))))
		{
			if (cur != NULL)
			{
				gtk_tree_store_move_before(gtkblist->treemodel, cur, &more_z);
				*iter = *cur;
				return;
			}
			else
			{
				gtk_tree_store_insert_before(gtkblist->treemodel, iter,
											 &groupiter, &more_z);
				return;
			}
		}
	}
	while (gtk_tree_model_iter_next(GTK_TREE_MODEL(gtkblist->treemodel),
									&more_z));

	if (cur) {
		gtk_tree_store_move_before(gtkblist->treemodel, cur, NULL);
		*iter = *cur;
		return;
	} else {
		gtk_tree_store_append(gtkblist->treemodel, iter, &groupiter);
		return;
	}
}

void
pidgin_blist_update_sort_methods(void)
{
	GApplication *application = NULL;
	GMenu *menu = NULL;
	GList *l;

	if(gtkblist == NULL || !PIDGIN_IS_CONTACT_LIST_WINDOW(gtkblist->window)) {
		return;
	}

	/* get the menu and clear any existing entries. */
	application = g_application_get_default();
	menu = gtk_application_get_menu_by_id(GTK_APPLICATION(application),
	                                      "sort-buddies");
	g_menu_remove_all(menu);

	/* Walk through the sort methods and add them to the menu. */
	for (l = pidgin_blist_sort_methods; l; l = l->next) {
		PidginBlistSortMethod *method = NULL;
		GMenuItem *item = NULL;
		GVariant *value = NULL;
		gchar *action = NULL;

		method = (PidginBlistSortMethod *)l->data;
		value = g_variant_new_string(method->id);

		action = g_action_print_detailed_name("blist.sort-method", value);
		item = g_menu_item_new(method->name, action);

		g_free(action);
		g_variant_unref(value);

		g_menu_append_item(menu, item);
		g_object_unref(item);
	}
}
