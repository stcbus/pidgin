/**
 * @file jabber.h
 *
 * gaim
 *
 * Copyright (C) 2003 Nathan Walp <faceprint@faceprint.com>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef _GAIM_JABBER_H_
#define _GAIM_JABBER_H_

#include <glib.h>
#include "connection.h"
#include "sslconn.h"

#include "jutil.h"
#include "xmlnode.h"

typedef enum {
	JABBER_STREAM_OFFLINE,
	JABBER_STREAM_CONNECTING,
	JABBER_STREAM_INITIALIZING,
	JABBER_STREAM_AUTHENTICATING,
	JABBER_STREAM_REINITIALIZING,
	JABBER_STREAM_CONNECTED
} JabberStreamState;

typedef struct _JabberStream
{
	int fd;

	GMarkupParseContext *context;
	xmlnode *current;

	enum {
		JABBER_PROTO_0_9,
		JABBER_PROTO_1_0
	} protocol_version;
	char *stream_id;
	JabberStreamState state;

	/* SASL authentication */
	char *expected_rspauth;

	GHashTable *buddies;
	gboolean roster_parsed;

	GHashTable *chats;

	GHashTable *callbacks;
	int next_id;

	time_t idle;

	JabberID *user;
	GaimConnection *gc;
	GaimSslConnection *gsc;
} JabberStream;

typedef void (JabberCallback)(JabberStream *js, xmlnode *packet);

void jabber_process_packet(JabberStream *js, xmlnode *packet);
void jabber_send(JabberStream *js, xmlnode *data);
void jabber_send_raw(JabberStream *js, const char *data);

void jabber_stream_set_state(JabberStream *js, JabberStreamState state);

char *jabber_get_next_id(JabberStream *js);

#endif /* _GAIM_JABBER_H_ */
