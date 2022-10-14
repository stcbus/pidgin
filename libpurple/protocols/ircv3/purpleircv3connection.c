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

#include "purpleircv3connection.h"

#include "purpleircv3core.h"
#include "purpleircv3parser.h"

enum {
	PROP_0,
	PROP_CANCELLABLE,
	PROP_CAPABILITIES,
	N_PROPERTIES,
};
static GParamSpec *properties[N_PROPERTIES] = {NULL, };

struct _PurpleIRCv3Connection {
	PurpleConnection parent;

	GError *validate_error;

	GSocketConnection *connection;
	GCancellable *cancellable;

	gchar *server_name;

	GDataInputStream *input;
	PurpleQueuedOutputStream *output;

	PurpleIRCv3Parser *parser;

	char *capabilities;
};

G_DEFINE_DYNAMIC_TYPE(PurpleIRCv3Connection, purple_ircv3_connection,
                      PURPLE_TYPE_CONNECTION)

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
purple_ircv3_connection_send_user_command(PurpleIRCv3Connection *connection) {
	PurpleAccount *account = NULL;
	const char *identname = NULL;
	const char *nickname = NULL;
	const char *realname = NULL;

	account = purple_connection_get_account(PURPLE_CONNECTION(connection));
	nickname = purple_connection_get_display_name(PURPLE_CONNECTION(connection));

	/* The stored value could be an empty string, so pass a default of empty
	 * string and then if it was empty, set our correct fallback.
	 */
	identname = purple_account_get_string(account, "ident", "");
	if(identname == NULL || *identname == '\0') {
		identname = nickname;
	}

	realname = purple_account_get_string(account, "real-name", "");
	if(realname == NULL || *realname == '\0') {
		realname = nickname;
	}

	purple_ircv3_connection_writef(connection, "USER %s 0 * :%s", identname,
	                               realname);
}

static void
purple_ircv3_connection_send_nick_command(PurpleIRCv3Connection *connection) {
	const char *nickname = NULL;

	nickname = purple_connection_get_display_name(PURPLE_CONNECTION(connection));

	purple_ircv3_connection_writef(connection, "NICK %s", nickname);
}

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
purple_ircv3_connection_read_cb(GObject *source, GAsyncResult *result,
                                gpointer data)
{
	PurpleIRCv3Connection *connection = data;
	GDataInputStream *istream = G_DATA_INPUT_STREAM(source);
	GError *error = NULL;
	gchar *line = NULL;
	gsize length;

	line = g_data_input_stream_read_line_finish(istream, result, &length,
	                                            &error);
	if(line == NULL || error != NULL) {
		if(error == NULL) {
			g_set_error_literal(&error, PURPLE_CONNECTION_ERROR,
			                    PURPLE_CONNECTION_ERROR_NETWORK_ERROR,
			                    _("Server closed the connection"));
		} else {
			g_prefix_error(&error, "%s", _("Lost connection with server: "));
		}

		purple_connection_take_error(PURPLE_CONNECTION(connection), error);

		/* In the off chance that line was returned, make sure we free it. */
		g_free(line);

		return;
	}

	purple_ircv3_parser_parse(connection->parser, line, &error, connection);

	g_free(line);

	/* Call read_line_async again to continue reading lines. */
	g_data_input_stream_read_line_async(connection->input,
	                                    G_PRIORITY_DEFAULT,
	                                    connection->cancellable,
	                                    purple_ircv3_connection_read_cb,
	                                    connection);
}

static void
purple_ircv3_connection_write_cb(GObject *source, GAsyncResult *result,
                                 gpointer data)
{
	PurpleIRCv3Connection *connection = data;
	PurpleQueuedOutputStream *stream = PURPLE_QUEUED_OUTPUT_STREAM(source);
	GError *error = NULL;
	gboolean success = FALSE;

	success = purple_queued_output_stream_push_bytes_finish(stream, result,
	                                                        &error);

	if(!success) {
		purple_queued_output_stream_clear_queue(stream);

		g_prefix_error(&error, "%s", _("Lost connection with server: "));

		purple_connection_take_error(PURPLE_CONNECTION(connection), error);

		return;
	}
}

static void
purple_ircv3_connection_connected_cb(GObject *source, GAsyncResult *result,
                                     gpointer data)
{
	PurpleIRCv3Connection *connection = data;
	GError *error = NULL;
	GInputStream *istream = NULL;
	GOutputStream *ostream = NULL;
	GSocketClient *client = G_SOCKET_CLIENT(source);
	GSocketConnection *conn = NULL;

	/* Finish the async method. */
	conn = g_socket_client_connect_to_host_finish(client, result, &error);
	if(conn == NULL || error != NULL) {
		g_prefix_error(&error, "%s", _("Unable to connect: "));

		purple_connection_take_error(PURPLE_CONNECTION(connection), error);

		return;
	}

	purple_connection_set_state(PURPLE_CONNECTION(connection),
	                            PURPLE_CONNECTION_CONNECTED);

	g_message("Successfully connected to %s", connection->server_name);

	/* Save our connection and setup our input and outputs. */
	connection->connection = conn;

	/* Create our parser. */
	connection->parser = purple_ircv3_parser_new();
	purple_ircv3_parser_add_default_handlers(connection->parser);

	ostream = g_io_stream_get_output_stream(G_IO_STREAM(conn));
	connection->output = purple_queued_output_stream_new(ostream);

	istream = g_io_stream_get_input_stream(G_IO_STREAM(conn));
	connection->input = g_data_input_stream_new(istream);
	g_data_input_stream_set_newline_type(G_DATA_INPUT_STREAM(connection->input),
	                                     G_DATA_STREAM_NEWLINE_TYPE_CR_LF);

	/* Add our read callback. */
	g_data_input_stream_read_line_async(connection->input,
	                                    G_PRIORITY_DEFAULT,
	                                    connection->cancellable,
	                                    purple_ircv3_connection_read_cb,
	                                    connection);

	/* Send our registration commands. */
	purple_ircv3_connection_writef(connection, "CAP LS %s",
	                               PURPLE_IRCV3_CONNECTION_CAP_VERSION);
	purple_ircv3_connection_send_user_command(connection);
	purple_ircv3_connection_send_nick_command(connection);
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
purple_ircv3_connection_get_property(GObject *obj, guint param_id,
                                     GValue *value, GParamSpec *pspec)
{
	PurpleIRCv3Connection *connection = PURPLE_IRCV3_CONNECTION(obj);

	switch(param_id) {
		case PROP_CANCELLABLE:
			g_value_set_object(value,
			                   purple_ircv3_connection_get_cancellable(connection));
			break;
		case PROP_CAPABILITIES:
			g_value_set_string(value,
			                   purple_ircv3_connection_get_capabilities(connection));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_ircv3_connection_set_property(GObject *obj, guint param_id,
                                     const GValue *value, GParamSpec *pspec)
{
	switch(param_id) {
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_ircv3_connection_dispose(GObject *obj) {
	PurpleIRCv3Connection *connection = PURPLE_IRCV3_CONNECTION(obj);

	g_clear_object(&connection->cancellable);

	g_clear_object(&connection->input);
	g_clear_object(&connection->output);
	g_clear_object(&connection->connection);

	g_clear_object(&connection->parser);

	G_OBJECT_CLASS(purple_ircv3_connection_parent_class)->dispose(obj);
}

static void
purple_ircv3_connection_finalize(GObject *obj) {
	PurpleIRCv3Connection *connection = PURPLE_IRCV3_CONNECTION(obj);

	g_clear_error(&connection->validate_error);
	g_clear_pointer(&connection->server_name, g_free);

	g_clear_pointer(&connection->capabilities, g_free);

	G_OBJECT_CLASS(purple_ircv3_connection_parent_class)->finalize(obj);
}

static void
purple_ircv3_connection_constructed(GObject *obj) {
	PurpleIRCv3Connection *connection = PURPLE_IRCV3_CONNECTION(obj);
	PurpleAccount *account = NULL;
	gchar **userparts = NULL;
	const gchar *username = NULL;

	G_OBJECT_CLASS(purple_ircv3_connection_parent_class)->constructed(obj);

	account = purple_connection_get_account(PURPLE_CONNECTION(connection));

	/* Make sure the username (which includes the servername via usersplits),
	 * does not contain any whitespace.
	 */
	username = purple_account_get_username(account);
	if(strpbrk(username, " \t\v\r\n") != NULL) {
		g_set_error(&connection->validate_error,
		            PURPLE_CONNECTION_ERROR,
		            PURPLE_CONNECTION_ERROR_INVALID_SETTINGS,
		            _("IRC nick and server may not contain whitespace"));

		return;
	}

	/* Split the username into nick and server and store the values. */
	userparts = g_strsplit(username, "@", 2);
	purple_connection_set_display_name(PURPLE_CONNECTION(connection),
	                                   userparts[0]);
	connection->server_name = g_strdup(userparts[1]);
	g_strfreev(userparts);

	/* Finally create our cancellable. */
	connection->cancellable = g_cancellable_new();
}

static void
purple_ircv3_connection_init(PurpleIRCv3Connection *connection) {
}

static void
purple_ircv3_connection_class_finalize(PurpleIRCv3ConnectionClass *klass) {
}

static void
purple_ircv3_connection_class_init(PurpleIRCv3ConnectionClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->get_property = purple_ircv3_connection_get_property;
	obj_class->set_property = purple_ircv3_connection_set_property;
	obj_class->constructed = purple_ircv3_connection_constructed;
	obj_class->dispose = purple_ircv3_connection_dispose;
	obj_class->finalize = purple_ircv3_connection_finalize;

	/**
	 * PurpleIRCv3Connection:cancellable:
	 *
	 * The [class@Gio.Cancellable] for this connection.
	 *
	 * Since: 3.0.0
	 */
	properties[PROP_CANCELLABLE] = g_param_spec_object(
		"cancellable", "cancellable",
		"The cancellable for this connection",
		G_TYPE_CANCELLABLE,
		G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	/**
	 * PurpleIRCv3Connection:capabilities:
	 *
	 * The capabilities that the server supports.
	 *
	 * This is created during registration of the connection and is useful for
	 * troubleshooting or just reporting them to end users.
	 *
	 * Since: 3.0.0
	 */
	properties[PROP_CAPABILITIES] = g_param_spec_string(
		"capabilities", "capabilities",
		"The capabilities that the server supports",
		NULL,
		G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, N_PROPERTIES, properties);
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
void
purple_ircv3_connection_register(GPluginNativePlugin *plugin) {
	purple_ircv3_connection_register_type(G_TYPE_MODULE(plugin));
}

GCancellable *
purple_ircv3_connection_get_cancellable(PurpleIRCv3Connection *connection) {
	g_return_val_if_fail(PURPLE_IRCV3_IS_CONNECTION(connection), NULL);

	return connection->cancellable;
}

gboolean
purple_ircv3_connection_valid(PurpleIRCv3Connection *connection,
                              GError **error)
{
	g_return_val_if_fail(PURPLE_IRCV3_IS_CONNECTION(connection), FALSE);

	if(connection->validate_error != NULL) {
		g_propagate_error(error, connection->validate_error);
		connection->validate_error = NULL;

		return FALSE;
	}

	return TRUE;
}

void
purple_ircv3_connection_connect(PurpleIRCv3Connection *connection) {
	PurpleAccount *account = NULL;
	GError *error = NULL;
	GSocketClient *client = NULL;
	gint default_port = PURPLE_IRCV3_DEFAULT_TLS_PORT;
	gint port = 0;
	gboolean use_tls = TRUE;

	g_return_if_fail(PURPLE_IRCV3_IS_CONNECTION(connection));
	g_return_if_fail(connection->connection == NULL);

	account = purple_connection_get_account(PURPLE_CONNECTION(connection));
	client = purple_gio_socket_client_new(account, &error);
	if(!G_IS_SOCKET_CLIENT(client)) {
		purple_connection_take_error(PURPLE_CONNECTION(connection), error);

		return;
	}

	/* Turn on TLS if requested. */
	use_tls = purple_account_get_bool(account, "use-tls", TRUE);
	g_socket_client_set_tls(client, use_tls);

	/* If TLS is not being used, set the default port to the plain port. */
	if(!use_tls) {
		default_port = PURPLE_IRCV3_DEFAULT_PLAIN_PORT;
	}
	port = purple_account_get_int(account, "port", default_port);

	/* Finally start the async connection. */
	g_socket_client_connect_to_host_async(client, connection->server_name,
	                                      port, connection->cancellable,
	                                      purple_ircv3_connection_connected_cb,
	                                      connection);

	g_clear_object(&client);
}

void
purple_ircv3_connection_close(PurpleIRCv3Connection *connection) {
	g_return_if_fail(PURPLE_IRCV3_IS_CONNECTION(connection));

	/* TODO: send QUIT command. */

	/* Cancel the cancellable to tell everyone we're shutting down. */
	if(G_IS_CANCELLABLE(connection->cancellable)) {
		g_cancellable_cancel(connection->cancellable);

		g_clear_object(&connection->cancellable);
	}

	if(G_IS_SOCKET_CONNECTION(connection->connection)) {
		GInputStream *istream = G_INPUT_STREAM(connection->input);
		GOutputStream *ostream = G_OUTPUT_STREAM(connection->output);

		purple_gio_graceful_close(G_IO_STREAM(connection->connection),
		                          istream, ostream);
	}

	g_clear_object(&connection->input);
	g_clear_object(&connection->output);
	g_clear_object(&connection->connection);
}

void
purple_ircv3_connection_writef(PurpleIRCv3Connection *connection,
                               const char *format, ...)
{
	GBytes *bytes = NULL;
	GString *msg = NULL;
	va_list vargs;

	g_return_if_fail(PURPLE_IRCV3_IS_CONNECTION(connection));
	g_return_if_fail(format != NULL);

	/* Create our string and append our format to it. */
	msg = g_string_new("");

	va_start(vargs, format);
	g_string_vprintf(msg, format, vargs);
	va_end(vargs);

	/* Next add the trailing carriage return line feed. */
	g_string_append(msg, "\r\n");

	/* Finally turn the string into bytes and send it! */
	bytes = g_bytes_new_take(msg->str, msg->len);
	g_string_free(msg, FALSE);

	purple_queued_output_stream_push_bytes_async(connection->output, bytes,
	                                             G_PRIORITY_DEFAULT,
	                                             connection->cancellable,
	                                             purple_ircv3_connection_write_cb,
	                                             connection);

	g_bytes_unref(bytes);
}

const char *
purple_ircv3_connection_get_capabilities(PurpleIRCv3Connection *connection) {
	g_return_val_if_fail(PURPLE_IRCV3_IS_CONNECTION(connection), NULL);

	return connection->capabilities;
}

void
purple_ircv3_connection_append_capabilities(PurpleIRCv3Connection *connection,
                                            const char *capabilities)
{
	g_return_if_fail(PURPLE_IRCV3_IS_CONNECTION(connection));
	g_return_if_fail(capabilities != NULL);

	if(connection->capabilities == NULL) {
		connection->capabilities = g_strdup(capabilities);
	} else {
		char *tmp = connection->capabilities;

		connection->capabilities = g_strdup_printf("%s %s",
		                                           connection->capabilities,
		                                           capabilities);

		g_free(tmp);
	}

	g_object_notify_by_pspec(G_OBJECT(connection),
	                         properties[PROP_CAPABILITIES]);
}
