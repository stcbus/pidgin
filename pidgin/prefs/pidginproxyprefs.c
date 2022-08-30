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

#include <purple.h>

#include <adwaita.h>

#include "pidginproxyprefs.h"
#include "pidginprefsinternal.h"

struct _PidginProxyPrefs {
	AdwPreferencesPage parent;

	/* GNOME version */
	GtkWidget *gnome;
	GtkWidget *gnome_not_found;
	GtkWidget *gnome_program;
	gchar *gnome_program_path;

	/* Non-GNOME version */
	GtkWidget *nongnome;
	GtkWidget *socks4_remotedns;
	PidginPrefCombo type;
	GtkWidget *options;
	GtkWidget *host;
	GtkWidget *port;
	GtkWidget *username;
	GtkWidget *password;
};

G_DEFINE_TYPE(PidginProxyPrefs, pidgin_proxy_prefs, ADW_TYPE_PREFERENCES_PAGE)

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
proxy_changed_cb(const gchar *name, PurplePrefType type, gconstpointer value,
                 gpointer data)
{
	PidginProxyPrefs *prefs = data;
	const char *proxy = value;

	gtk_widget_set_visible(prefs->options,
	                       !purple_strequal(proxy, "none") &&
	                       !purple_strequal(proxy, "envvar"));
}

static void
proxy_print_option(GtkWidget *entry, gpointer data)
{
	PidginProxyPrefs *prefs = data;

	if (entry == prefs->host) {
		purple_prefs_set_string("/purple/proxy/host",
		                        gtk_editable_get_text(GTK_EDITABLE(entry)));
	} else if (entry == prefs->port) {
		purple_prefs_set_int("/purple/proxy/port",
				gtk_spin_button_get_value_as_int(
					GTK_SPIN_BUTTON(entry)));
	} else if (entry == prefs->username) {
		purple_prefs_set_string("/purple/proxy/username",
		                        gtk_editable_get_text(GTK_EDITABLE(entry)));
	} else if (entry == prefs->password) {
		purple_prefs_set_string("/purple/proxy/password",
		                        gtk_editable_get_text(GTK_EDITABLE(entry)));
	}
}

static void
proxy_row_activated_cb(G_GNUC_UNUSED AdwActionRow *row, gpointer data)
{
	PidginProxyPrefs *prefs = data;
	GError *err = NULL;

	if (g_spawn_command_line_async(prefs->gnome_program_path, &err)) {
		return;
	}

	purple_notify_error(NULL, NULL, _("Cannot start proxy configuration program."), err->message, NULL);
	g_error_free(err);
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
pidgin_proxy_prefs_finalize(GObject *obj)
{
	PidginProxyPrefs *prefs = PIDGIN_PROXY_PREFS(obj);

	purple_prefs_disconnect_by_handle(obj);

	g_free(prefs->gnome_program_path);

	G_OBJECT_CLASS(pidgin_proxy_prefs_parent_class)->finalize(obj);
}

static void
pidgin_proxy_prefs_class_init(PidginProxyPrefsClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	obj_class->finalize = pidgin_proxy_prefs_finalize;

	gtk_widget_class_set_template_from_resource(
		widget_class,
		"/im/pidgin/Pidgin3/Prefs/proxy.ui"
	);

	gtk_widget_class_bind_template_child(
			widget_class, PidginProxyPrefs, gnome);
	gtk_widget_class_bind_template_child(
			widget_class, PidginProxyPrefs, gnome_not_found);
	gtk_widget_class_bind_template_child(
			widget_class, PidginProxyPrefs, gnome_program);

	gtk_widget_class_bind_template_child(
			widget_class, PidginProxyPrefs, nongnome);
	gtk_widget_class_bind_template_child(
			widget_class, PidginProxyPrefs, socks4_remotedns);
	gtk_widget_class_bind_template_child(
			widget_class, PidginProxyPrefs, type.combo);
	gtk_widget_class_bind_template_child(
			widget_class, PidginProxyPrefs, options);
	gtk_widget_class_bind_template_child(
			widget_class, PidginProxyPrefs, host);
	gtk_widget_class_bind_template_child(
			widget_class, PidginProxyPrefs, port);
	gtk_widget_class_bind_template_child(
			widget_class, PidginProxyPrefs, username);
	gtk_widget_class_bind_template_child(
			widget_class, PidginProxyPrefs, password);
	gtk_widget_class_bind_template_callback(widget_class,
	                                        proxy_row_activated_cb);
	gtk_widget_class_bind_template_callback(widget_class,
			proxy_print_option);
}

static void
pidgin_proxy_prefs_init(PidginProxyPrefs *prefs)
{
	PurpleProxyInfo *proxy_info;

	gtk_widget_init_template(GTK_WIDGET(prefs));

	if(purple_running_gnome()) {
		gchar *path = NULL;

		gtk_widget_set_visible(prefs->gnome, TRUE);
		gtk_widget_set_visible(prefs->nongnome, FALSE);

		path = g_find_program_in_path("gnome-network-properties");
		if (path == NULL) {
			path = g_find_program_in_path("gnome-network-preferences");
		}
		if (path == NULL) {
			path = g_find_program_in_path("gnome-control-center");
			if (path != NULL) {
				char *tmp = g_strdup_printf("%s network", path);
				g_free(path);
				path = tmp;
			}
		}

		prefs->gnome_program_path = path;
		gtk_widget_set_visible(prefs->gnome_not_found, path == NULL);
		gtk_widget_set_visible(prefs->gnome_program, path != NULL);

	} else {
		gtk_widget_set_visible(prefs->gnome, FALSE);
		gtk_widget_set_visible(prefs->nongnome, TRUE);

		/* This is a global option that affects SOCKS4 usage even with
		 * account-specific proxy settings */
		pidgin_prefs_bind_switch("/purple/proxy/socks4_remotedns",
		                         prefs->socks4_remotedns);

		prefs->type.type = PURPLE_PREF_STRING;
		prefs->type.key = "/purple/proxy/type";
		pidgin_prefs_bind_dropdown(&prefs->type);
		proxy_info = purple_global_proxy_get_info();

		purple_prefs_connect_callback(prefs, "/purple/proxy/type",
				proxy_changed_cb, prefs);

		if (proxy_info != NULL) {
			const gchar *value = NULL;

			value = purple_proxy_info_get_hostname(proxy_info);
			if(value != NULL) {
				gtk_editable_set_text(GTK_EDITABLE(prefs->host), value);
			}

			if (purple_proxy_info_get_port(proxy_info) != 0) {
				gtk_spin_button_set_value(
						GTK_SPIN_BUTTON(prefs->port),
						purple_proxy_info_get_port(proxy_info));
			}

			value = purple_proxy_info_get_username(proxy_info);
			if(value != NULL) {
				gtk_editable_set_text(GTK_EDITABLE(prefs->username), value);
			}

			value = purple_proxy_info_get_password(proxy_info);
			if(value != NULL) {
				gtk_editable_set_text(GTK_EDITABLE(prefs->password), value);
			}
		}

		proxy_changed_cb("/purple/proxy/type", PURPLE_PREF_STRING,
			purple_prefs_get_string("/purple/proxy/type"),
			prefs);
	}
}

/******************************************************************************
 * API
 *****************************************************************************/
GtkWidget *
pidgin_proxy_prefs_new(void) {
	return GTK_WIDGET(g_object_new(PIDGIN_TYPE_PROXY_PREFS, NULL));
}
