/*
 * Purple - Internet Messaging Library
 * Copyright (C) Pidgin Developers <devel@pidgin.im>
 *
 * Purple is the legal property of its developers, whose names are too numerous
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

#ifndef PURPLE_SOUPCOMPAT_H
#define PURPLE_SOUPCOMPAT_H
/*
 * This file is internal to libpurple. Do not use!
 * Also, any public API should not depend on this file.
 */

#include <libsoup/soup.h>

#if SOUP_MAJOR_VERSION < 3

static inline const char *
soup_message_get_reason_phrase(SoupMessage *msg) {
	return msg->reason_phrase;
}

static inline SoupMessageHeaders *
soup_message_get_response_headers(SoupMessage *msg) {
	return msg->response_headers;
}

static inline SoupMessageHeaders *
soup_message_get_request_headers(SoupMessage *msg) {
	return msg->request_headers;
}

static inline SoupStatus
soup_message_get_status(SoupMessage *msg) {
	return msg->status_code;
}

static inline SoupMessage *
soup_message_new_from_encoded_form(const gchar *method,
                                   const gchar *uri_string,
                                   gchar *encoded_form)
{
	SoupMessage *msg = NULL;
	SoupURI *uri;

	g_return_val_if_fail(method != NULL, NULL);
	g_return_val_if_fail(uri_string != NULL, NULL);
	g_return_val_if_fail(encoded_form != NULL, NULL);

	uri = soup_uri_new(uri_string);
	if (!uri || !uri->host) {
		g_free(encoded_form);
		soup_uri_free(uri);
		return NULL;
	}

	if (strcmp(method, "GET") == 0) {
		g_free(uri->query);
		uri->query = encoded_form;
		msg = soup_message_new_from_uri(method, uri);
	} else if (strcmp (method, "POST") == 0 || strcmp (method, "PUT") == 0) {
		msg = soup_message_new_from_uri(method, uri);
		soup_message_body_append_take(msg->request_body,
		                              (guchar *)encoded_form,
		                              strlen(encoded_form));
	} else {
		g_free(encoded_form);
	}

	soup_uri_free(uri);

	return msg;
}

static inline void
soup_message_set_request_body_from_bytes(SoupMessage *msg,
                                         const gchar *content_type,
                                         GBytes *bytes)
{
	gconstpointer data = NULL;
	gsize length = 0;

	data = g_bytes_get_data(bytes, &length);
	soup_message_set_request(msg, content_type, SOUP_MEMORY_COPY,
	                         data, length);
}

static inline void
soup_session_send_and_read_async_cancel_cb(GCancellable *cancellable,
                                           gpointer data)
{
	GTask *task = data;
	SoupSession *session = g_task_get_source_object(task);
	SoupMessage *msg = g_task_get_task_data(task);

	soup_session_cancel_message(session, msg, SOUP_STATUS_CANCELLED);
}

static inline void
soup_session_send_and_read_sync_cb(SoupSession *session, SoupMessage *msg,
                                   gpointer data)
{
	GTask *task = data;

	if(SOUP_STATUS_IS_SUCCESSFUL(msg->status_code)) {
		GBytes *bytes = g_bytes_new(msg->response_body->data,
		                            msg->response_body->length);
		g_task_return_pointer(task, bytes, (GDestroyNotify)g_bytes_unref);
	} else {
		g_task_return_new_error(task, SOUP_HTTP_ERROR, msg->status_code,
		                        "SoupMessage returned failure: (%d) %s",
		                        msg->status_code, msg->reason_phrase);
	}

	g_object_unref(task);
}

static inline GBytes *
soup_session_send_and_read_finish(SoupSession *session, GAsyncResult *result,
                                  GError **error)
{
	g_return_val_if_fail(SOUP_IS_SESSION(session), NULL);

	return g_task_propagate_pointer(G_TASK(result), error);
}

static inline void
soup_session_send_and_read_async(SoupSession *session, SoupMessage *msg,
                                 gint priority, GCancellable *cancellable,
                                 GAsyncReadyCallback cb, gpointer data)
{
	GTask *task = NULL;

	task = g_task_new(session, cancellable, cb, data);
	g_task_set_priority(task, priority);
	g_task_set_task_data(task, g_object_ref(msg), g_object_unref);
	g_cancellable_connect(cancellable,
	                      G_CALLBACK(soup_session_send_and_read_async_cancel_cb),
	                      g_object_ref(task), g_object_unref);

	soup_session_queue_message(session, msg,
	                           soup_session_send_and_read_sync_cb, task);
}

#endif /* SOUP_MAJOR_VERSION < 3 */

#endif /* PURPLE_SOUPCOMPAT_H */
