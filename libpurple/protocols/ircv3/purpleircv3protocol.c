/*
 * Purple - Internet Messaging Library
 * Copyright (C) Pidgin Developers <devel@pidgin.im>
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

#include "purpleircv3protocol.h"

#include "purpleircv3connection.h"
#include "purpleircv3core.h"

typedef struct {
	gboolean dummy;
} PurpleIRCv3ProtocolPrivate;

/******************************************************************************
 * PurpleProtocol Implementation
 *****************************************************************************/
static GList *
purple_ircv3_protocol_get_user_splits(G_GNUC_UNUSED PurpleProtocol *protocol) {
	PurpleAccountUserSplit *split = NULL;
	GList *splits = NULL;

	split = purple_account_user_split_new(_("Server"),
	                                      PURPLE_IRCV3_DEFAULT_SERVER,
	                                      '@');
	splits = g_list_append(splits, split);

	return splits;
}

static GList *
purple_ircv3_protocol_get_account_options(G_GNUC_UNUSED PurpleProtocol *protocol)
{
	PurpleAccountOption *option;
	GList *options = NULL;

	option = purple_account_option_int_new(_("Port"), "port",
	                                       PURPLE_IRCV3_DEFAULT_TLS_PORT);
	options = g_list_append(options, option);

	option = purple_account_option_bool_new(_("Use TLS"), "use-tls", TRUE);
	options = g_list_append(options, option);

	option = purple_account_option_string_new(_("Ident name"), "ident", "");
	options = g_list_append(options, option);

	option = purple_account_option_string_new(_("Real name"), "real-name", "");
	options = g_list_append(options, option);

	option = purple_account_option_string_new(_("SASL login name"),
	                                          "sasl-login-name", "");
	options = g_list_append(options, option);

	option = purple_account_option_bool_new(_("Allow plaintext SASL auth over "
	                                          "unencrypted connection"),
	                                        "plain-sasl-in-clear", FALSE);
	options = g_list_append(options, option);

	option = purple_account_option_int_new(_("Seconds between sending "
	                                         "messages"),
	                                       "rate-limit-interval", 2);
	options = g_list_append(options, option);

	option = purple_account_option_int_new(_("Maximum messages to send at "
	                                         "once"),
	                                       "rate-limit-burst", 5);
	options = g_list_append(options, option);

	return options;
}

static void
purple_ircv3_protocol_login(G_GNUC_UNUSED PurpleProtocol *protocol,
                            PurpleAccount *account)
{
	PurpleIRCv3Connection *connection = NULL;
	PurpleConnection *purple_connection = NULL;
	GError *error = NULL;

	purple_connection = purple_account_get_connection(account);

	connection = purple_ircv3_connection_new(account);
	if(!purple_ircv3_connection_valid(connection, &error)) {
		purple_connection_take_error(purple_connection, error);

		return;
	}

	g_object_set_data_full(G_OBJECT(purple_connection),
	                       PURPLE_IRCV3_CONNECTION_KEY,
	                       connection, g_object_unref);

	purple_ircv3_connection_connect(connection);
}

static void
purple_ircv3_protocol_close(G_GNUC_UNUSED PurpleProtocol *protocol,
                            PurpleConnection *purple_connection)
{
	PurpleIRCv3Connection *connection = NULL;

	connection = g_object_get_data(G_OBJECT(purple_connection),
	                               PURPLE_IRCV3_CONNECTION_KEY);

	purple_ircv3_connection_close(connection);

	/* Set our connection data to NULL which will remove the last reference. */
	g_object_set_data(G_OBJECT(purple_connection), PURPLE_IRCV3_CONNECTION_KEY,
	                  NULL);
}

static GList *
purple_ircv3_protocol_status_types(G_GNUC_UNUSED PurpleProtocol *protocol,
                                   PurpleAccount *account)
{
	PurpleStatusType *type = NULL;
	GList *types = NULL;

	type = purple_status_type_new(PURPLE_STATUS_AVAILABLE, NULL, NULL, TRUE);
	types = g_list_append(types, type);

	type = purple_status_type_new_with_attrs(
		PURPLE_STATUS_AWAY, NULL, NULL, TRUE, TRUE, FALSE,
		"message", _("Message"), purple_value_new(G_TYPE_STRING),
		NULL);
	types = g_list_append(types, type);

	type = purple_status_type_new(PURPLE_STATUS_OFFLINE, NULL, NULL, TRUE);
	types = g_list_append(types, type);

	return types;
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_DYNAMIC_TYPE_EXTENDED(
	PurpleIRCv3Protocol, purple_ircv3_protocol, PURPLE_TYPE_PROTOCOL, 0,
	G_ADD_PRIVATE_DYNAMIC(PurpleIRCv3Protocol))

static void
purple_ircv3_protocol_init(PurpleIRCv3Protocol *protocol) {
}

static void
purple_ircv3_protocol_class_finalize(PurpleIRCv3ProtocolClass *klass) {
}

static void
purple_ircv3_protocol_class_init(PurpleIRCv3ProtocolClass *klass) {
	PurpleProtocolClass *protocol_class = PURPLE_PROTOCOL_CLASS(klass);

	protocol_class->get_user_splits = purple_ircv3_protocol_get_user_splits;
	protocol_class->get_account_options =
		purple_ircv3_protocol_get_account_options;
	protocol_class->login = purple_ircv3_protocol_login;
	protocol_class->close = purple_ircv3_protocol_close;
	protocol_class->status_types = purple_ircv3_protocol_status_types;
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
void
purple_ircv3_protocol_register(GPluginNativePlugin *plugin) {
	purple_ircv3_protocol_register_type(G_TYPE_MODULE(plugin));
}

PurpleProtocol *
purple_ircv3_protocol_new(void) {
	return g_object_new(
		PURPLE_IRCV3_TYPE_PROTOCOL,
		"id", "prpl-irv3",
		"name", "IRCv3",
		"description", _("Version 3 of Internet Relay Chat (IRC)."),
		"icon-name", "im-ircv3",
		"icon-resource-path", "/im/pidgin/libpurple/ircv3/icons",
		"options", OPT_PROTO_CHAT_TOPIC | OPT_PROTO_PASSWORD_OPTIONAL |
		           OPT_PROTO_SLASH_COMMANDS_NATIVE,
		NULL);
}
