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
#include "purpleircv3core.h"

/******************************************************************************
 * Fallback
 *****************************************************************************/
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

/******************************************************************************
 * Ping
 *****************************************************************************/
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

/******************************************************************************
 * Capabilities
 *****************************************************************************/
static gboolean
purple_ircv3_messager_handler_cap_list(guint n_params, GStrv params,
                                       GError **error, gpointer data)
{
	PurpleIRCv3Connection *connection = data;

	/* Check if we have more messages coming. */
	if(n_params > 1 && purple_strequal(params[0], "*")) {
		purple_ircv3_connection_append_capabilities(connection, params[1]);
	} else {
		purple_ircv3_connection_append_capabilities(connection, params[0]);

		g_message("**** capabilities-list: %s",
		          purple_ircv3_connection_get_capabilities(connection));

		purple_ircv3_connection_writef(connection, "CAP END");
	}

	return TRUE;
}

gboolean
purple_ircv3_messager_handler_cap(GHashTable *tags, const char *source,
                                  const char *command, guint n_params,
                                  GStrv params, GError **error, gpointer data)
{
	const char *subcommand = NULL;
	guint n_subparams = 0;
	GStrv subparams = NULL;

	if(n_params < 2) {
		return FALSE;
	}

	/* Initialize some variables to make it easier to call our sub command
	 * handlers.
	 *
	 * params[0] is the nick or * if it hasn't been negotiated yet, we don't
	 * have a need for this, so we ignore it.
	 *
	 * params[1] is the CAP subcommand sent from the server. We use it here
	 * purely for dispatching to our subcommand handlers.
	 *
	 * params[2] and higher are the parameters to the subcommand. To make the
	 * code a bit easier all around, we subtract 2 from n_params to remove
	 * references to the nick and subcommand name. Like wise, we add 2 to the
	 * params GStrv which will now point to the second item in the array again
	 * ignoring the nick and subcommand.
	 */
	subcommand = params[1];
	n_subparams = n_params - 2;
	subparams = params + 2;

	/* Dispatch the subcommand. */
	if(purple_strequal(subcommand, "LS") ||
	   purple_strequal(subcommand, "LIST"))
	{
		return purple_ircv3_messager_handler_cap_list(n_subparams, subparams,
		                                              error, data);
	}

	g_set_error(error, PURPLE_IRCV3_DOMAIN, 0,
	            "No handler for CAP subcommand %s", subcommand);

	return FALSE;
}
