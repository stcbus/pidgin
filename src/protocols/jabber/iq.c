/*
 * gaim - Jabber Protocol Plugin
 *
 * Copyright (C) 2003, Nathan Walp <faceprint@faceprint.com>
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
 *
 */
#include "internal.h"
#include "debug.h"
#include "prefs.h"

#include "buddy.h"
#include "disco.h"
#include "iq.h"
#include "oob.h"
#include "roster.h"
#include "si.h"

#ifdef _WIN32
#include "utsname.h"
#endif

JabberIq *jabber_iq_new(JabberStream *js, JabberIqType type)
{
	JabberIq *iq;

	iq = g_new0(JabberIq, 1);

	iq->type = type;

	iq->node = xmlnode_new("iq");
	switch(iq->type) {
		case JABBER_IQ_SET:
			xmlnode_set_attrib(iq->node, "type", "set");
			break;
		case JABBER_IQ_GET:
			xmlnode_set_attrib(iq->node, "type", "get");
			break;
		case JABBER_IQ_ERROR:
			xmlnode_set_attrib(iq->node, "type", "error");
			break;
		case JABBER_IQ_RESULT:
			xmlnode_set_attrib(iq->node, "type", "result");
			break;
		case JABBER_IQ_NONE:
			/* this shouldn't ever happen */
			break;
	}

	iq->js = js;

	if(type == JABBER_IQ_GET || type == JABBER_IQ_SET) {
		iq->id = jabber_get_next_id(js);
		xmlnode_set_attrib(iq->node, "id", iq->id);
	}

	return iq;
}

JabberIq *jabber_iq_new_query(JabberStream *js, JabberIqType type,
		const char *xmlns)
{
	JabberIq *iq = jabber_iq_new(js, type);
	xmlnode *query;

	query = xmlnode_new_child(iq->node, "query");
	xmlnode_set_attrib(query, "xmlns", xmlns);

	return iq;
}

typedef struct _JabberCallbackData {
	JabberIqCallback *callback;
	gpointer data;
} JabberCallbackData;

void
jabber_iq_set_callback(JabberIq *iq, JabberIqCallback *callback, gpointer data)
{
	iq->callback = callback;
	iq->callback_data = data;
}

void jabber_iq_set_id(JabberIq *iq, const char *id)
{
	if(iq->id)
		g_free(iq->id);

	if(id) {
		xmlnode_set_attrib(iq->node, "id", id);
		iq->id = g_strdup(id);
	} else {
		xmlnode_remove_attrib(iq->node, "id");
		iq->id = NULL;
	}
}

void jabber_iq_send(JabberIq *iq)
{
	JabberCallbackData *jcd;
	g_return_if_fail(iq != NULL);

	jabber_send(iq->js, iq->node);

	if(iq->id && iq->callback) {
		jcd = g_new0(JabberCallbackData, 1);
		jcd->callback = iq->callback;
		jcd->data = iq->callback_data;
		g_hash_table_insert(iq->js->iq_callbacks, g_strdup(iq->id), jcd);
	}

	jabber_iq_free(iq);
}

void jabber_iq_free(JabberIq *iq)
{
	g_return_if_fail(iq != NULL);

	g_free(iq->id);
	xmlnode_free(iq->node);
	g_free(iq);
}

static void jabber_iq_last_parse(JabberStream *js, xmlnode *packet)
{
	JabberIq *iq;
	const char *type;
	const char *from;
	const char *id;
	xmlnode *query;
	char *idle_time;

	type = xmlnode_get_attrib(packet, "type");
	from = xmlnode_get_attrib(packet, "from");
	id = xmlnode_get_attrib(packet, "id");

	if(type && !strcmp(type, "get")) {
		iq = jabber_iq_new_query(js, JABBER_IQ_RESULT, "jabber:iq:last");
		jabber_iq_set_id(iq, id);
		xmlnode_set_attrib(iq->node, "to", from);

		query = xmlnode_get_child(iq->node, "query");

		idle_time = g_strdup_printf("%ld", js->idle ? time(NULL) - js->idle : 0);
		xmlnode_set_attrib(query, "seconds", idle_time);
		g_free(idle_time);

		jabber_iq_send(iq);
	}
}

static void jabber_iq_time_parse(JabberStream *js, xmlnode *packet)
{
	const char *type, *from, *id;
	JabberIq *iq;
	char buf[1024];
	xmlnode *query;
	time_t now_t;
	struct tm *now;

	time(&now_t);
	now = localtime(&now_t);

	type = xmlnode_get_attrib(packet, "type");
	from = xmlnode_get_attrib(packet, "from");
	id = xmlnode_get_attrib(packet, "id");

	if(type && !strcmp(type, "get")) {

		iq = jabber_iq_new_query(js, JABBER_IQ_RESULT, "jabber:iq:time");
		jabber_iq_set_id(iq, id);
		xmlnode_set_attrib(iq->node, "to", from);

		query = xmlnode_get_child(iq->node, "query");

		strftime(buf, sizeof(buf), "%Y%m%dT%T", now);
		xmlnode_insert_data(xmlnode_new_child(query, "utc"), buf, -1);
		strftime(buf, sizeof(buf), "%Z", now);
		xmlnode_insert_data(xmlnode_new_child(query, "tz"), buf, -1);
		strftime(buf, sizeof(buf), "%d %b %Y %T", now);
		xmlnode_insert_data(xmlnode_new_child(query, "display"), buf, -1);

		jabber_iq_send(iq);
	}
}

static void jabber_iq_version_parse(JabberStream *js, xmlnode *packet)
{
	JabberIq *iq;
	const char *type, *from, *id;
	xmlnode *query;
	char *os = NULL;

	type = xmlnode_get_attrib(packet, "type");

	if(type && !strcmp(type, "get")) {

		if(!gaim_prefs_get_bool("/plugins/prpl/jabber/hide_os")) {
			struct utsname osinfo;

			uname(&osinfo);
			os = g_strdup_printf("%s %s %s", osinfo.sysname, osinfo.release,
					osinfo.machine);
		}

		from = xmlnode_get_attrib(packet, "from");
		id = xmlnode_get_attrib(packet, "id");

		iq = jabber_iq_new_query(js, JABBER_IQ_RESULT, "jabber:iq:version");
		xmlnode_set_attrib(iq->node, "to", from);
		jabber_iq_set_id(iq, id);

		query = xmlnode_get_child(iq->node, "query");

		xmlnode_insert_data(xmlnode_new_child(query, "name"), PACKAGE, -1);
		xmlnode_insert_data(xmlnode_new_child(query, "version"), VERSION, -1);
		if(os) {
			xmlnode_insert_data(xmlnode_new_child(query, "os"), os, -1);
			g_free(os);
		}

		jabber_iq_send(iq);
	}
}

void jabber_iq_parse(JabberStream *js, xmlnode *packet)
{
	JabberCallbackData *jcd;
	xmlnode *query, *error, *x;
	const char *xmlns;
	const char *type, *id, *from;

	query = xmlnode_get_child(packet, "query");
	type = xmlnode_get_attrib(packet, "type");
	from = xmlnode_get_attrib(packet, "from");
	id = xmlnode_get_attrib(packet, "id");

	/* First, lets see if a special callback got registered */

	if(type && (!strcmp(type, "result") || !strcmp(type, "error"))) {
		if(id && *id && (jcd = g_hash_table_lookup(js->iq_callbacks, id))) {
			jcd->callback(js, packet, jcd->data);
			g_hash_table_remove(js->iq_callbacks, id);
			return;
		}
	}

	/* Apparently not, so lets see if we have a pre-defined handler */

	if(type && query && (xmlns = xmlnode_get_attrib(query, "xmlns"))) {
		if(!strcmp(type, "set")) {
			if(!strcmp(xmlns, "jabber:iq:roster")) {
				jabber_roster_parse(js, packet);
				return;
			} else if(!strcmp(xmlns, "jabber:iq:oob")) {
				jabber_oob_parse(js, packet);
				return;
			} else if(!strcmp(xmlns, "http://jabber.org/protocol/bytestreams")) {
				jabber_bytestreams_parse(js, packet);
				return;
			}
		} else if(!strcmp(type, "get")) {
			if(!strcmp(xmlns, "jabber:iq:last")) {
				jabber_iq_last_parse(js, packet);
				return;
			} else if(!strcmp(xmlns, "jabber:iq:time")) {
				jabber_iq_time_parse(js, packet);
				return;
			} else if(!strcmp(xmlns, "jabber:iq:version")) {
				jabber_iq_version_parse(js, packet);
				return;
			} else if(!strcmp(xmlns, "http://jabber.org/protocol/disco#info")) {
				jabber_disco_info_parse(js, packet);
				return;
			} else if(!strcmp(xmlns, "http://jabber.org/protocol/disco#items")) {
				jabber_disco_items_parse(js, packet);
				return;
			}
		} else if(!strcmp(type, "result")) {
			if(!strcmp(xmlns, "jabber:iq:roster")) {
				jabber_roster_parse(js, packet);
				return;
			} else if(!strcmp(xmlns, "jabber:iq:register")) {
				jabber_register_parse(js, packet);
				return;
			} else if(!strcmp(xmlns, "http://jabber.org/protocol/disco#info")) {
				jabber_disco_info_parse(js, packet);
				return;
			}
		}
	} else {
		if(xmlnode_get_child_with_namespace(packet, "si", "http://jabber.org/protocol/si")) {
			jabber_si_parse(js, packet);
			return;
		}
	}


	/* If we get here, send the default error reply mandated by XMPP-CORE */

	if(type && (!strcmp(type, "set") || !strcmp(type, "get"))) {
		JabberIq *iq = jabber_iq_new(js, JABBER_IQ_ERROR);

		xmlnode_free(iq->node);
		iq->node = xmlnode_copy(packet);
		xmlnode_set_attrib(iq->node, "to", from);
		xmlnode_set_attrib(iq->node, "type", "error");
		error = xmlnode_new_child(iq->node, "error");
		xmlnode_set_attrib(error, "type", "cancel");
		xmlnode_set_attrib(error, "code", "501");
		x = xmlnode_new_child(error, "feature-not-implemented");
		xmlnode_set_attrib(x, "xmlns", "urn:ietf:params:xml:ns:xmpp-stanzas");

		jabber_iq_send(iq);
	}
}

