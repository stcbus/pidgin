/**
 * @file soap.h SOAP handling
 *
 * purple
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#ifndef MSN_SOAP_H
#define MSN_SOAP_H

#include "xmlnode.h"

#include "session.h"

typedef struct _MsnSoapMessage MsnSoapMessage;

typedef void (*MsnSoapCallback)(MsnSoapMessage *request,
	MsnSoapMessage *response, gpointer cb_data);

MsnSoapMessage *
msn_soap_message_new(const gchar *action, xmlnode *xml);

xmlnode *
msn_soap_message_get_xml(MsnSoapMessage *message);

void
msn_soap_message_send(MsnSession *session, MsnSoapMessage *message,
	const gchar *host, const gchar *path, gboolean secure,
	MsnSoapCallback cb, gpointer cb_data);

#endif /* MSN_SOAP_H */
