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

#include "pidginaccounteditor.h"

#include "pidginproxyoptions.h"

struct _PidginAccountEditor {
	GtkDialog parent;

	PurpleAccount *account;

	GtkWidget *notebook;
	GtkWidget *proxy_options;
};

enum {
	PROP_0,
	PROP_ACCOUNT,
	N_PROPERTIES,
};
static GParamSpec *properties[N_PROPERTIES] = {NULL, };

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
pidgin_account_editor_set_account(PidginAccountEditor *editor,
                                  PurpleAccount *account)
{
	if(g_set_object(&editor->account, account)) {
		PurpleProxyInfo *proxy_info = NULL;

		if(PURPLE_IS_ACCOUNT(account)) {
			proxy_info = purple_account_get_proxy_info(account);
		}

		pidgin_proxy_options_set_info(PIDGIN_PROXY_OPTIONS(editor->proxy_options),
		                              proxy_info);

		g_object_notify_by_pspec(G_OBJECT(editor), properties[PROP_ACCOUNT]);
	}
}

static void
pidgin_account_editor_save_account(PidginAccountEditor *editor) {
	PurpleAccountManager *manager = NULL;
	PurpleProxyInfo *info = NULL;
	gboolean new_account = FALSE;

	manager = purple_account_manager_get_default();

	if(!PURPLE_IS_ACCOUNT(editor->account)) {
		editor->account = purple_account_new("undefined", "undefined");
		new_account = TRUE;
	}

	info = pidgin_proxy_options_get_info(PIDGIN_PROXY_OPTIONS(editor->proxy_options));
	purple_account_set_proxy_info(editor->account, info);

	/* If this is a new account, add it to the account manager and bring it
	 * online.
	 */
	if(new_account) {
		const PurpleSavedStatus *saved_status;

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
pidgin_account_editor_response_cb(GtkDialog *dialog, gint response_id,
                                  G_GNUC_UNUSED gpointer data)
{
	if(response_id == GTK_RESPONSE_APPLY) {
		pidgin_account_editor_save_account(PIDGIN_ACCOUNT_EDITOR(dialog));
	}

	gtk_widget_destroy(GTK_WIDGET(dialog));
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

	G_OBJECT_CLASS(pidgin_account_editor_parent_class)->dispose(obj);
}

static void
pidgin_account_editor_init(PidginAccountEditor *account_editor) {
	GtkWidget *widget = GTK_WIDGET(account_editor);

	gtk_widget_init_template(widget);
}

static void
pidgin_account_editor_class_init(PidginAccountEditorClass *klass) {
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->get_property = pidgin_account_editor_get_property;
	obj_class->set_property = pidgin_account_editor_set_property;
	obj_class->dispose = pidgin_account_editor_dispose;

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

	gtk_widget_class_bind_template_child(widget_class, PidginAccountEditor,
	                                     notebook);
	gtk_widget_class_bind_template_child(widget_class, PidginAccountEditor,
	                                     proxy_options);

	gtk_widget_class_bind_template_callback(widget_class,
	                                        pidgin_account_editor_response_cb);
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
