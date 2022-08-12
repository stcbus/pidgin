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

#include "pidginconversationwindow.h"

#include "gtkconv.h"
#include "gtkdialogs.h"
#include "gtkutils.h"
#include "pidgininvitedialog.h"

enum {
	PIDGIN_CONVERSATION_WINDOW_COLUMN_OBJECT,
	PIDGIN_CONVERSATION_WINDOW_COLUMN_NAME,
	PIDGIN_CONVERSATION_WINDOW_COLUMN_ICON,
	PIDGIN_CONVERSATION_WINDOW_COLUMN_MARKUP,
};

enum {
	SIG_CONVERSATION_SWITCHED,
	N_SIGNALS,
};
static guint signals[N_SIGNALS] = {0, };

struct _PidginConversationWindow {
	GtkApplicationWindow parent;

	GtkWidget *vbox;

	GtkWidget *view;
	GtkTreeSelection *selection;
	GtkTreeStore *model;

	GtkWidget *stack;

	GtkWidget *notification_list;

	GtkTreePath *conversation_path;
};

G_DEFINE_TYPE(PidginConversationWindow, pidgin_conversation_window,
              GTK_TYPE_APPLICATION_WINDOW)

static GtkWidget *default_window = NULL;

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
pidgin_conversation_window_actions_set_enabled(GActionMap *map,
                                               const gchar **actions,
                                               gboolean enabled)
{
	gint i = 0;

	for(i = 0; actions[i] != NULL; i++) {
		GAction *action = NULL;
		const gchar *name = actions[i];

		action = g_action_map_lookup_action(map, name);
		if(action != NULL) {
			g_simple_action_set_enabled(G_SIMPLE_ACTION(action), enabled);
		} else {
			g_critical("Failed to find action named %s", name);
		}
	}
}

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
pidgin_conversation_window_invite_cb(GtkDialog *dialog, gint response_id,
                                     G_GNUC_UNUSED gpointer data)
{
	PidginInviteDialog *invite_dialog = PIDGIN_INVITE_DIALOG(dialog);
	PurpleChatConversation *chat = NULL;

	chat = pidgin_invite_dialog_get_conversation(invite_dialog);

	g_object_set_data(G_OBJECT(chat), "pidgin-invite-dialog", NULL);

	if(response_id == GTK_RESPONSE_ACCEPT) {
		const gchar *contact = NULL, *message = NULL;

		contact = pidgin_invite_dialog_get_contact(invite_dialog);
		message = pidgin_invite_dialog_get_message(invite_dialog);

		if(!purple_strequal(contact, "")) {
			PurpleConnection *connection = NULL;

			connection = purple_conversation_get_connection(PURPLE_CONVERSATION(chat));
			purple_serv_chat_invite(connection,
			                        purple_chat_conversation_get_id(chat),
			                        message, contact);
		}
	}

	gtk_window_destroy(GTK_WINDOW(invite_dialog));
}

/******************************************************************************
 * Actions
 *****************************************************************************/
static void
pidgin_conversation_window_alias(G_GNUC_UNUSED GSimpleAction *simple,
                                 G_GNUC_UNUSED GVariant *parameter,
                                 gpointer data)
{
	PidginConversationWindow *window = data;
	PurpleConversation *selected = NULL;

	selected = pidgin_conversation_window_get_selected(window);
	if(PURPLE_IS_CONVERSATION(selected)) {
		PurpleAccount *account;
		const gchar *name;

		account = purple_conversation_get_account(selected);
		name = purple_conversation_get_name(selected);

		if(PURPLE_IS_IM_CONVERSATION(selected)) {
			PurpleBuddy *buddy = purple_blist_find_buddy(account, name);

			if(PURPLE_IS_BUDDY(buddy)) {
				pidgin_dialogs_alias_buddy(buddy);
			}
		} else if(PURPLE_IS_CHAT_CONVERSATION(selected)) {
			PurpleChat *chat = purple_blist_find_chat(account, name);

			if(PURPLE_IS_CHAT(chat)) {
				pidgin_dialogs_alias_chat(chat);
			}
		}
	}
}

static void
pidgin_conversation_window_close_conversation(G_GNUC_UNUSED GSimpleAction *simple,
                                              G_GNUC_UNUSED GVariant *parameter,
                                              gpointer data)
{
	PidginConversationWindow *window = data;
	PurpleConversation *selected = NULL;

	selected = pidgin_conversation_window_get_selected(window);
	if(PURPLE_IS_CONVERSATION(selected)) {
		pidgin_conversation_window_remove(window, selected);
		pidgin_conversation_detach(selected);
	}
}

static void
pidgin_conversation_window_get_info(G_GNUC_UNUSED GSimpleAction *simple,
                                    G_GNUC_UNUSED GVariant *parameter,
                                    gpointer data)
{
	PidginConversationWindow *window = data;
	PurpleConversation *selected = NULL;

	selected = pidgin_conversation_window_get_selected(window);
	if(PURPLE_IS_CONVERSATION(selected)) {
		if(PURPLE_IS_IM_CONVERSATION(selected)) {
			PurpleConnection *connection = NULL;

			connection = purple_conversation_get_connection(selected);
			pidgin_retrieve_user_info(connection,
			                          purple_conversation_get_name(selected));
		}
	}
}

static void
pidgin_conversation_window_invite(G_GNUC_UNUSED GSimpleAction *simple,
                                  G_GNUC_UNUSED GVariant *parameter,
                                  gpointer data)
{
	PidginConversationWindow *window = data;
	PurpleConversation *selected = NULL;

	selected = pidgin_conversation_window_get_selected(window);
	if(PURPLE_IS_CHAT_CONVERSATION(selected)) {
		GtkWidget *invite_dialog = NULL;

		invite_dialog = g_object_get_data(G_OBJECT(selected),
		                                  "pidgin-invite-dialog");

		if(!GTK_IS_WIDGET(invite_dialog)) {
			invite_dialog = pidgin_invite_dialog_new(PURPLE_CHAT_CONVERSATION(selected));
			g_object_set_data(G_OBJECT(selected), "pidgin-invite-dialog",
			                  invite_dialog);

			gtk_window_set_transient_for(GTK_WINDOW(invite_dialog),
			                             GTK_WINDOW(window));
			gtk_window_set_destroy_with_parent(GTK_WINDOW(invite_dialog), TRUE);

			g_signal_connect(invite_dialog, "response",
			                 G_CALLBACK(pidgin_conversation_window_invite_cb),
			                 NULL);
		}

		gtk_widget_show(invite_dialog);
	}
}

static void
pidgin_conversation_window_send_file(G_GNUC_UNUSED GSimpleAction *simple,
                                     G_GNUC_UNUSED GVariant *parameter,
                                     gpointer data)
{
	PidginConversationWindow *window = data;
	PurpleConversation *selected = NULL;

	selected = pidgin_conversation_window_get_selected(window);
	if(PURPLE_IS_IM_CONVERSATION(selected)) {
		PurpleConnection *connection = NULL;

		connection = purple_conversation_get_connection(selected);
		purple_serv_send_file(connection,
		                      purple_conversation_get_name(selected),
		                      NULL);
	}
}

static GActionEntry win_entries[] = {
	{
		.name = "alias",
		.activate = pidgin_conversation_window_alias
	}, {
		.name = "close",
		.activate = pidgin_conversation_window_close_conversation
	}, {
		.name = "get-info",
		.activate = pidgin_conversation_window_get_info
	}, {
		.name = "invite",
		.activate = pidgin_conversation_window_invite
	}, {
		.name = "send-file",
		.activate = pidgin_conversation_window_send_file
	}
};

/*<private>
 * pidgin_conversation_window_conversation_actions:
 *
 * A list of action names that are only valid if a conversation is selected.
 */
static const gchar *pidgin_conversation_window_conversation_actions[] = {
	"alias",
	"close",
	"get-info",
	NULL
};

static const gchar *pidgin_conversation_window_im_conversation_actions[] = {
	"send-file",
	NULL
};

static const gchar *pidgin_conversation_window_chat_conversation_actions[] = {
	"invite",
	NULL
};

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
pidgin_conversation_window_selection_changed(GtkTreeSelection *selection,
                                             gpointer data)
{
	PidginConversationWindow *window = PIDGIN_CONVERSATION_WINDOW(data);
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	gboolean changed = FALSE;

	if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
		GObject *obj;
		gboolean is_conversation = FALSE;
		gboolean im_selected = FALSE, chat_selected = FALSE;
		gchar *name = NULL;

		gtk_tree_model_get(model, &iter,
		                   PIDGIN_CONVERSATION_WINDOW_COLUMN_NAME, &name,
		                   PIDGIN_CONVERSATION_WINDOW_COLUMN_OBJECT, &obj,
		                   -1);

		gtk_stack_set_visible_child_name(GTK_STACK(window->stack), name);
		g_free(name);

		changed = TRUE;

		/* If a conversation is selected, enable the generic conversation
		 * actions.
		 */
		is_conversation = PURPLE_IS_CONVERSATION(obj);
		pidgin_conversation_window_actions_set_enabled(G_ACTION_MAP(window),
							       pidgin_conversation_window_conversation_actions,
							       is_conversation);

		/* If an IM is selected, enable the IM-specific actions otherwise
		 * disable them.
		 */
		im_selected = PURPLE_IS_IM_CONVERSATION(obj);
		pidgin_conversation_window_actions_set_enabled(G_ACTION_MAP(window),
							       pidgin_conversation_window_im_conversation_actions,
							       im_selected);

		/* If a chat is selected, enable the chat-specific actions otherwise
		 * disable them.
		 */
		chat_selected = PURPLE_IS_CHAT_CONVERSATION(obj);
		pidgin_conversation_window_actions_set_enabled(G_ACTION_MAP(window),
							       pidgin_conversation_window_chat_conversation_actions,
							       chat_selected);

		g_clear_object(&obj);
	}

	if(!changed) {
		gtk_stack_set_visible_child_name(GTK_STACK(window->stack), "__conversations__");
	}
}

static gboolean
pidgin_conversation_window_key_pressed_cb(GtkEventControllerKey *controller,
                                          guint keyval,
                                          G_GNUC_UNUSED guint keycode,
                                          GdkModifierType state,
                                          gpointer data)
{
	PidginConversationWindow *window = data;

	/* If CTRL was held down... */
	if (state & GDK_CONTROL_MASK) {
		switch (keyval) {
			case GDK_KEY_Page_Down:
			case GDK_KEY_KP_Page_Down:
			case ']':
				pidgin_conversation_window_select_next(window);
				return TRUE;
				break;

			case GDK_KEY_Page_Up:
			case GDK_KEY_KP_Page_Up:
			case '[':
				pidgin_conversation_window_select_previous(window);
				return TRUE;
				break;
		} /* End of switch */
	}

	/* If ALT (or whatever) was held down... */
	else if (state & GDK_ALT_MASK) {
		if ('1' <= keyval && keyval <= '9') {
			guint switchto = keyval - '1';
			pidgin_conversation_window_select_nth(window, switchto);

			return TRUE;
		}
	}

	return FALSE;
}

/******************************************************************************
 * GObjectImplementation
 *****************************************************************************/
static void
pidgin_conversation_window_dispose(GObject *obj) {
	PidginConversationWindow *window = PIDGIN_CONVERSATION_WINDOW(obj);

	if(GTK_IS_TREE_MODEL(window->model)) {
		GtkTreeModel *model = GTK_TREE_MODEL(window->model);
		GtkTreeIter parent, iter;

		gtk_tree_model_get_iter(model, &parent, window->conversation_path);
		if(gtk_tree_model_iter_children(model, &iter, &parent)) {
			gboolean valid = FALSE;

			/* gtk_tree_store_remove moves the iter to the next item at the
			 * same level, so we abuse that to do our iteration.
			 */
			do {
				PurpleConversation *conversation = NULL;

				gtk_tree_model_get(model, &iter,
				                   PIDGIN_CONVERSATION_WINDOW_COLUMN_OBJECT, &conversation,
				                   -1);

				if(PURPLE_IS_CONVERSATION(conversation)) {
					pidgin_conversation_detach(conversation);

					valid = gtk_tree_store_remove(window->model, &iter);
				}
			} while(valid);
		}

		g_clear_pointer(&window->conversation_path, gtk_tree_path_free);
	}

	G_OBJECT_CLASS(pidgin_conversation_window_parent_class)->dispose(obj);
}

static void
pidgin_conversation_window_init(PidginConversationWindow *window) {
	GtkEventController *key = NULL;
	GtkTreeIter iter;

	gtk_widget_init_template(GTK_WIDGET(window));

	gtk_window_set_application(GTK_WINDOW(window),
	                           GTK_APPLICATION(g_application_get_default()));

	g_action_map_add_action_entries(G_ACTION_MAP(window), win_entries,
	                                G_N_ELEMENTS(win_entries), window);

	key = gtk_event_controller_key_new();
	gtk_event_controller_set_propagation_phase(key, GTK_PHASE_CAPTURE);
	g_signal_connect(G_OBJECT(key), "key-pressed",
	                 G_CALLBACK(pidgin_conversation_window_key_pressed_cb),
	                 window);
	gtk_widget_add_controller(GTK_WIDGET(window), key);

	/* Add our toplevels to the tree store. */
	gtk_tree_store_append(window->model, &iter, NULL);
	gtk_tree_store_set(window->model, &iter,
	                   PIDGIN_CONVERSATION_WINDOW_COLUMN_OBJECT, window->notification_list,
	                   PIDGIN_CONVERSATION_WINDOW_COLUMN_NAME, "__notifications__",
	                   PIDGIN_CONVERSATION_WINDOW_COLUMN_MARKUP, _("Notifications"),
	                   -1);

	gtk_tree_store_append(window->model, &iter, NULL);
	gtk_tree_store_set(window->model, &iter,
	                   PIDGIN_CONVERSATION_WINDOW_COLUMN_MARKUP, _("Conversations"),
	                   PIDGIN_CONVERSATION_WINDOW_COLUMN_NAME, "__conversations__",
	                   -1);
	gtk_tree_selection_select_iter(window->selection, &iter);
	window->conversation_path = gtk_tree_model_get_path(GTK_TREE_MODEL(window->model),
	                                                    &iter);
}

static void
pidgin_conversation_window_class_init(PidginConversationWindowClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	obj_class->dispose = pidgin_conversation_window_dispose;

	/**
	 * PidginConversationWindow::conversation-switched:
	 * @window: The conversation window.
	 * @new_conv: The now active conversation.
	 *
	 * Emitted when a window switched from one conversation to another.
	 */
	signals[SIG_CONVERSATION_SWITCHED] = g_signal_new_class_handler(
		"conversation-switched",
		G_OBJECT_CLASS_TYPE(obj_class),
		G_SIGNAL_RUN_LAST,
		NULL,
		NULL,
		NULL,
		NULL,
		G_TYPE_NONE,
		1,
		PURPLE_TYPE_CONVERSATION
	);

	gtk_widget_class_set_template_from_resource(
	    widget_class,
	    "/im/pidgin/Pidgin3/Conversations/window.ui"
	);

	gtk_widget_class_bind_template_child(widget_class, PidginConversationWindow,
	                                     vbox);

	gtk_widget_class_bind_template_child(widget_class, PidginConversationWindow,
	                                     model);
	gtk_widget_class_bind_template_child(widget_class, PidginConversationWindow,
	                                     view);
	gtk_widget_class_bind_template_child(widget_class, PidginConversationWindow,
	                                     selection);

	gtk_widget_class_bind_template_child(widget_class, PidginConversationWindow,
	                                     stack);
	gtk_widget_class_bind_template_child(widget_class, PidginConversationWindow,
	                                     notification_list);

	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_conversation_window_selection_changed);

	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_conversation_window_key_pressed_cb);
}

/******************************************************************************
 * API
 *****************************************************************************/
GtkWidget *
pidgin_conversation_window_get_default(void) {
	if(!GTK_IS_WIDGET(default_window)) {
		default_window = pidgin_conversation_window_new();
		g_object_add_weak_pointer(G_OBJECT(default_window),
		                          (gpointer)&default_window);
	}

	return default_window;
}

GtkWidget *
pidgin_conversation_window_new(void) {
	return GTK_WIDGET(g_object_new(PIDGIN_TYPE_CONVERSATION_WINDOW, NULL));
}

void
pidgin_conversation_window_add(PidginConversationWindow *window,
                               PurpleConversation *conversation)
{
	PidginConversation *gtkconv = NULL;
	GtkTreeIter parent, iter;
	GtkTreeModel *model = NULL;
	const gchar *markup = NULL;
	gboolean expand = FALSE;

	g_return_if_fail(PIDGIN_IS_CONVERSATION_WINDOW(window));
	g_return_if_fail(PURPLE_IS_CONVERSATION(conversation));

	model = GTK_TREE_MODEL(window->model);
	if(!gtk_tree_model_get_iter(model, &parent, window->conversation_path)) {
		/* If we can't find the conversation_path we have to bail. */
		g_warning("couldn't get an iterator to conversation_path");

		return;
	}

	if(!gtk_tree_model_iter_has_child(model, &parent)) {
		expand = TRUE;
	}

	markup = purple_conversation_get_name(conversation);

	gtkconv = PIDGIN_CONVERSATION(conversation);
	if(gtkconv != NULL) {
		GtkWidget *parent = gtk_widget_get_parent(gtkconv->tab_cont);

		if(GTK_IS_WIDGET(parent)) {
			g_object_ref(gtkconv->tab_cont);
			gtk_widget_unparent(gtkconv->tab_cont);
		}

		gtk_stack_add_named(GTK_STACK(window->stack), gtkconv->tab_cont, markup);
		gtk_widget_show(gtkconv->tab_cont);

		if(GTK_IS_WIDGET(parent)) {
			g_object_unref(gtkconv->tab_cont);
		}
	}

	gtk_tree_store_prepend(window->model, &iter, &parent);
	gtk_tree_store_set(window->model, &iter,
	                   PIDGIN_CONVERSATION_WINDOW_COLUMN_OBJECT, conversation,
	                   PIDGIN_CONVERSATION_WINDOW_COLUMN_NAME, markup,
	                   PIDGIN_CONVERSATION_WINDOW_COLUMN_MARKUP, markup,
	                   -1);

	/* If we just added the first child, expand the parent. */
	if(expand) {
		gtk_tree_view_expand_row(GTK_TREE_VIEW(window->view),
		                         window->conversation_path, FALSE);
	}


	if(!gtk_widget_is_visible(GTK_WIDGET(window))) {
		gtk_widget_show(GTK_WIDGET(window));
	}
}

void
pidgin_conversation_window_remove(PidginConversationWindow *window,
                                  PurpleConversation *conversation)
{
	GtkTreeIter parent, iter;
	GtkTreeModel *model = NULL;
	GObject *obj = NULL;

	g_return_if_fail(PIDGIN_IS_CONVERSATION_WINDOW(window));
	g_return_if_fail(PURPLE_IS_CONVERSATION(conversation));

	model = GTK_TREE_MODEL(window->model);

	if(!gtk_tree_model_get_iter(model, &parent, window->conversation_path)) {
		/* The path is somehow invalid, so bail... */
		return;
	}

	if(!gtk_tree_model_iter_children(model, &iter, &parent)) {
		/* The conversations iter has no children. */
		return;
	}

	do {
		gtk_tree_model_get(model, &iter,
		                   PIDGIN_CONVERSATION_WINDOW_COLUMN_OBJECT, &obj,
		                   -1);

		if(PURPLE_CONVERSATION(obj) == conversation) {
			GtkWidget *child = NULL;
			const gchar *name = NULL;

			name = purple_conversation_get_name(conversation);
			child = gtk_stack_get_child_by_name(GTK_STACK(window->stack),
			                                    name);
			if(GTK_IS_WIDGET(child)) {
				gtk_widget_unparent(child);
			}

			gtk_tree_store_remove(window->model, &iter);

			g_clear_object(&obj);

			break;
		}

		g_clear_object(&obj);
	} while(gtk_tree_model_iter_next(model, &iter));
}

guint
pidgin_conversation_window_get_count(PidginConversationWindow *window) {
	GtkSelectionModel *model = NULL;
	guint count = 0;

	g_return_val_if_fail(PIDGIN_IS_CONVERSATION_WINDOW(window), 0);

	model = gtk_stack_get_pages(GTK_STACK(window->stack));

	count = g_list_model_get_n_items(G_LIST_MODEL(model));

	g_object_unref(model);

	return count;
}

PurpleConversation *
pidgin_conversation_window_get_selected(PidginConversationWindow *window) {
	PurpleConversation *conversation = NULL;
	GtkTreeSelection *selection = NULL;
	GtkTreeIter iter;

	g_return_val_if_fail(PIDGIN_IS_CONVERSATION_WINDOW(window), NULL);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(window->view));
	if(gtk_tree_selection_get_selected(selection, NULL, &iter)) {

		gtk_tree_model_get(GTK_TREE_MODEL(window->model), &iter,
		                   PIDGIN_CONVERSATION_WINDOW_COLUMN_OBJECT, &conversation,
		                   -1);
	}

	return conversation;
}

void
pidgin_conversation_window_select(PidginConversationWindow *window,
                                  PurpleConversation *conversation)
{
	const gchar *name = NULL;

	g_return_if_fail(PIDGIN_IS_CONVERSATION_WINDOW(window));
	g_return_if_fail(PURPLE_IS_CONVERSATION(conversation));

	name = purple_conversation_get_name(conversation);
	gtk_stack_set_visible_child_name(GTK_STACK(window->stack), name);
}

void
pidgin_conversation_window_select_previous(PidginConversationWindow *window) {
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;
	GtkTreeSelection *selection = NULL;
	gboolean set = FALSE;

	g_return_if_fail(PIDGIN_IS_CONVERSATION_WINDOW(window));

	model = GTK_TREE_MODEL(window->model);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(window->view));
	if(gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		if(gtk_tree_model_iter_previous(model, &iter)) {
			gtk_tree_selection_select_iter(selection, &iter);
			set = TRUE;
		}
	}

	if(!set) {
		pidgin_conversation_window_select_last(window);
	}
}


void
pidgin_conversation_window_select_next(PidginConversationWindow *window) {
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;
	GtkTreeSelection *selection = NULL;
	gboolean set = FALSE;

	g_return_if_fail(PIDGIN_IS_CONVERSATION_WINDOW(window));

	model = GTK_TREE_MODEL(window->model);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(window->view));
	if(gtk_tree_selection_get_selected(selection, NULL, &iter)) {
		if(gtk_tree_model_iter_next(model, &iter)) {
			gtk_tree_selection_select_iter(selection, &iter);
			set = TRUE;
		}
	}

	if(!set) {
		pidgin_conversation_window_select_first(window);
	}
}

void
pidgin_conversation_window_select_first(PidginConversationWindow *window) {
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;

	g_return_if_fail(PIDGIN_IS_CONVERSATION_WINDOW(window));

	model = GTK_TREE_MODEL(window->model);

	if(gtk_tree_model_get_iter_first(model, &iter)) {
		GtkTreeSelection *selection = NULL;

		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(window->view));
		gtk_tree_selection_select_iter(selection, &iter);
	}
}

void
pidgin_conversation_window_select_last(PidginConversationWindow *window) {
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;
	gint count = 0;

	g_return_if_fail(PIDGIN_IS_CONVERSATION_WINDOW(window));

	model = GTK_TREE_MODEL(window->model);
	count = gtk_tree_model_iter_n_children(model, NULL);

	if(gtk_tree_model_iter_nth_child(model, &iter, NULL, count - 1)) {
		GtkTreeSelection *selection = NULL;

		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(window->view));
		gtk_tree_selection_select_iter(selection, &iter);
	}
}

void
pidgin_conversation_window_select_nth(PidginConversationWindow *window,
                                      guint nth)
{
	GtkTreeIter iter;
	GtkTreeModel *model = NULL;

	g_return_if_fail(PIDGIN_IS_CONVERSATION_WINDOW(window));

	model = GTK_TREE_MODEL(window->model);

	if(gtk_tree_model_iter_nth_child(model, &iter, NULL, nth)) {
		GtkTreeSelection *selection = NULL;

		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(window->view));
		gtk_tree_selection_select_iter(selection, &iter);
	}
}

gboolean
pidgin_conversation_window_conversation_is_selected(PidginConversationWindow *window,
                                                    PurpleConversation *conversation)
{
	const gchar *name = NULL, *visible = NULL;

	g_return_val_if_fail(PIDGIN_IS_CONVERSATION_WINDOW(window), FALSE);
	g_return_val_if_fail(PURPLE_IS_CONVERSATION(conversation), FALSE);

	name = purple_conversation_get_name(conversation);
	visible = gtk_stack_get_visible_child_name(GTK_STACK(window->stack));

	return purple_strequal(name, visible);
}
