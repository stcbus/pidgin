/*
 * @file jingle.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor Boston, MA 02110-1301,  USA
 */
 
#ifndef JINGLE_H
#define JINGLE_H

#include "jabber.h"

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#ifdef __cplusplus
extern "C" {
#endif

#define JINGLE "urn:xmpp:jingle:0"
#define JINGLE_ERROR "urn:xmpp:jingle:errors:0"
#define JINGLE_APP_FT "urn:xmpp:jingle:apps:file-transfer:0"
#define JINGLE_APP_RTP "urn:xmpp:jingle:apps:rtp:1"
#define JINGLE_APP_RTP_ERROR "urn:xmpp:jingle:apps:rtp:errors:1"
#define JINGLE_APP_RTP_INFO "urn:xmpp:jingle:apps:rtp:info:1"
#define JINGLE_APP_RTP_SUPPORT_AUDIO "urn:xmpp:jingle:apps:rtp:audio"
#define JINGLE_APP_RTP_SUPPORT_VIDEO "urn:xmpp:jingle:apps:rtp:video"
#define JINGLE_APP_XML "urn:xmpp:tmp:jingle:apps:xmlstream"
#define JINGLE_DTMF "urn:xmpp:jingle:dtmf:0"
#define JINGLE_TRANSPORT_SOCKS "urn:xmpp:jingle:transports:bytestreams:0"
#define JINGLE_TRANSPORT_IBB "urn:xmpp:jingle:transports:ibb:0"
#define JINGLE_TRANSPORT_ICEUDP "urn:xmpp:jingle:transports:ice-udp:0"
#define JINGLE_TRANSPORT_RAWUDP "urn:xmpp:jingle:transports:raw-udp:0"
#define JINGLE_TRANSPORT_RAWUDP_INFO "urn:xmpp:jingle:transports:raw-udp:info:0"

typedef enum {
	JINGLE_UNKNOWN_TYPE,
	JINGLE_CONTENT_ACCEPT,
	JINGLE_CONTENT_ADD,
	JINGLE_CONTENT_MODIFY,
	JINGLE_CONTENT_REJECT,
	JINGLE_CONTENT_REMOVE,
	JINGLE_DESCRIPTION_INFO,
	JINGLE_SESSION_ACCEPT,
	JINGLE_SESSION_INFO,
	JINGLE_SESSION_INITIATE,
	JINGLE_SESSION_TERMINATE,
	JINGLE_TRANSPORT_ACCEPT,
	JINGLE_TRANSPORT_INFO,
	JINGLE_TRANSPORT_REJECT,
	JINGLE_TRANSPORT_REPLACE,
} JingleActionType;

const gchar *jingle_get_action_name(JingleActionType action);
JingleActionType jingle_get_action_type(const gchar *action);

GType jingle_get_type(const gchar *type);

void jingle_parse(JabberStream *js, xmlnode *packet);

void jingle_terminate_sessions(JabberStream *js);

/* create a GParam array given autoconfigured STUN (and later perhaps TURN).
	if google_talk is TRUE, set compatability mode to GOOGLE_TALK */
GParameter *jingle_get_params(JabberStream *js, guint *num_params);

#ifdef __cplusplus
}
#endif

G_END_DECLS

#endif /* JINGLE_H */
