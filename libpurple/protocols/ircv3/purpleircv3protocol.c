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

typedef struct {
	gboolean dummy;
} PurpleIRCv3ProtocolPrivate;

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
