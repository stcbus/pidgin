/*
 * Purple - Internet Messaging Library
 * Copyright (C) Pidgin Developers <devel@pidgin.im>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <https://www.gnu.org/licenses/>.
 */

#include <glib/gi18n-lib.h>

#include "purpledemoprotocol.h"
#include "purpledemoprotocolactions.h"

/******************************************************************************
 * Action Implementations
 *****************************************************************************/
#define REAPER_BUDDY_NAME ("Gary")
#define DEFAULT_REAP_TIME (5)  /* seconds */
#define FATAL_TICK_STR N_("Reaping connection in %d second...")
#define FATAL_TICK_PLURAL_STR N_("Reaping connection in %d seconds...")
#define FATAL_DISCONNECT_STR N_("%s reaped the connection")
#define TEMPORARY_TICK_STR N_("Pruning connection in %d second...")
#define TEMPORARY_TICK_PLURAL_STR N_("Pruning connection in %d seconds...")
#define TEMPORARY_DISCONNECT_STR N_("%s pruned the connection")

static gboolean
purple_demo_protocol_failure_tick(gpointer data,
                                  const gchar *tick_str,
                                  const gchar *tick_plural_str,
                                  const gchar *disconnect_str)
{
	PurpleConnection *connection = PURPLE_CONNECTION(data);
	PurpleAccount *account = purple_connection_get_account(connection);
	gchar *message = NULL;
	gint timeout = 0;

	timeout = GPOINTER_TO_INT(g_object_steal_data(G_OBJECT(connection),
	                                              "reaping-time"));
	timeout--;
	if(timeout > 0) {
		g_object_set_data(G_OBJECT(connection), "reaping-time",
		                  GINT_TO_POINTER(timeout));

		message = g_strdup_printf(ngettext(tick_str, tick_plural_str, timeout),
		                          timeout);
		purple_protocol_got_user_status(account, REAPER_BUDDY_NAME,
		                                "available", "message", message, NULL);
		g_free(message);

		return G_SOURCE_CONTINUE;
	}

	message = g_strdup_printf(_(disconnect_str), REAPER_BUDDY_NAME);
	purple_connection_error(connection, PURPLE_CONNECTION_ERROR_OTHER_ERROR,
	                        message);
	g_free(message);

	return G_SOURCE_REMOVE;
}

static gboolean
purple_demo_protocol_fatal_failure_cb(gpointer data) {
	return purple_demo_protocol_failure_tick(data,
	                                         FATAL_TICK_STR,
	                                         FATAL_TICK_PLURAL_STR,
	                                         FATAL_DISCONNECT_STR);
}

static gboolean
purple_demo_protocol_temporary_failure_cb(gpointer data) {
	return purple_demo_protocol_failure_tick(data,
	                                         TEMPORARY_TICK_STR,
	                                         TEMPORARY_TICK_PLURAL_STR,
	                                         TEMPORARY_DISCONNECT_STR);
}

static void
purple_demo_protocol_failure_action_activate(GSimpleAction *action,
                                             GVariant *parameter,
                                             const gchar *tick_str,
                                             const gchar *tick_plural_str,
                                             GSourceFunc cb)
{
	PurpleConnection *connection = NULL;
	const gchar *account_id = NULL;
	PurpleAccountManager *manager = NULL;
	PurpleAccount *account = NULL;
	gchar *status = NULL;

	if(!g_variant_is_of_type(parameter, G_VARIANT_TYPE_STRING)) {
		g_critical("Demo failure action parameter is of incorrect type %s",
		           g_variant_get_type_string(parameter));
	}

	account_id = g_variant_get_string(parameter, NULL);
	manager = purple_account_manager_get_default();
	account = purple_account_manager_find_by_id(manager, account_id);
	connection = purple_account_get_connection(account);

	/* Do nothing if disconnected, or already in process of reaping. */
	if(!PURPLE_IS_CONNECTION(connection)) {
		return;
	}
	if(g_object_get_data(G_OBJECT(connection), "reaping-time")) {
		return;
	}

	purple_protocol_got_user_idle(account, REAPER_BUDDY_NAME, FALSE, 0);
	status = g_strdup_printf(
	        ngettext(tick_str, tick_plural_str, DEFAULT_REAP_TIME),
	        DEFAULT_REAP_TIME);
	purple_protocol_got_user_status(account, REAPER_BUDDY_NAME, "available",
	                                "message", status, NULL);
	g_free(status);

	g_object_set_data(G_OBJECT(connection), "reaping-time",
	                  GINT_TO_POINTER(DEFAULT_REAP_TIME));
	g_timeout_add_seconds(1, cb, connection);
}

static void
purple_demo_protocol_temporary_failure_action_activate(GSimpleAction *action,
                                                       GVariant *parameter,
                                                       G_GNUC_UNUSED gpointer data)
{
	purple_demo_protocol_failure_action_activate(action, parameter,
	                                             TEMPORARY_TICK_STR,
	                                             TEMPORARY_TICK_PLURAL_STR,
	                                             purple_demo_protocol_temporary_failure_cb);
}

static void
purple_demo_protocol_fatal_failure_action_activate(GSimpleAction *action,
                                                   GVariant *parameter,
                                                   G_GNUC_UNUSED gpointer data)
{
	purple_demo_protocol_failure_action_activate(action, parameter,
	                                             FATAL_TICK_STR,
	                                             FATAL_TICK_PLURAL_STR,
	                                             purple_demo_protocol_fatal_failure_cb);
}

/******************************************************************************
 * PurpleProtocolActions Implementation
 *****************************************************************************/
static GActionGroup *
purple_demo_protocol_get_action_group(PurpleProtocolActions *actions,
                                      PurpleConnection *connection)
{
	GSimpleActionGroup *group = NULL;
	GActionEntry entries[] = {
		{
			.name = "temporary-failure",
			.activate = purple_demo_protocol_temporary_failure_action_activate,
			.parameter_type = "s",
		},
		{
			.name = "fatal-failure",
			.activate = purple_demo_protocol_fatal_failure_action_activate,
			.parameter_type = "s",
		},
	};
	gsize nentries = G_N_ELEMENTS(entries);

	group = g_simple_action_group_new();
	g_action_map_add_action_entries(G_ACTION_MAP(group), entries, nentries,
	                                NULL);

	return G_ACTION_GROUP(group);
}

static GMenu *
purple_demo_protocol_get_menu(PurpleProtocolActions *actions)
{
	GMenu *menu = NULL;
	GMenuItem *item = NULL;

	menu = g_menu_new();

	item = g_menu_item_new(_("Trigger temporary connection failure..."),
	                       "prpl-demo.temporary-failure");
	g_menu_item_set_attribute(item, PURPLE_MENU_ATTRIBUTE_DYNAMIC_TARGET, "s",
	                          "account");
	g_menu_append_item(menu, item);
	g_object_unref(item);

	item = g_menu_item_new(_("Trigger fatal connection failure..."),
	                       "prpl-demo.fatal-failure");
	g_menu_item_set_attribute(item, PURPLE_MENU_ATTRIBUTE_DYNAMIC_TARGET, "s",
	                          "account");
	g_menu_append_item(menu, item);
	g_object_unref(item);

	return menu;
}

void
purple_demo_protocol_actions_init(PurpleProtocolActionsInterface *iface) {
	iface->get_action_group = purple_demo_protocol_get_action_group;
	iface->get_menu = purple_demo_protocol_get_menu;
}
