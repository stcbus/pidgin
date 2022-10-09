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

#include "purpleircv3messagehandlers.h"

#include "purpleircv3connection.h"

gboolean
purple_ircv3_messager_handler_fallback(GHashTable *tags, const char *source,
                                       const char *command, guint n_params,
                                       GStrv params, GError **error,
                                       gpointer data)
{
	gchar *joined = g_strjoinv(" ", params);

	g_message("-- unhandled message --");
	g_message("source: %s", source);
	g_message("command: %s", command);
	g_message("params: %s", joined);
	g_message("-- end of unhandled message --");

	g_free(joined);

	return TRUE;
}

gboolean
purple_ircv3_messager_handler_ping(GHashTable *tags, const char *source,
                                   const char *command, guint n_params,
                                   GStrv params, GError **error,
                                   gpointer data)
{
	PurpleIRCv3Connection *connection = data;

	if(n_params == 1) {
		purple_ircv3_connection_writef(connection, "PONG %s", params[0]);
	} else {
		purple_ircv3_connection_writef(connection, "PONG");
	}

	return TRUE;
}
