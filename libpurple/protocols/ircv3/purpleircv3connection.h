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

#ifndef PURPLE_IRCV3_CONNECTION_H
#define PURPLE_IRCV3_CONNECTION_H

#include <glib.h>
#include <glib-object.h>

#include <gplugin.h>
#include <gplugin-native.h>

#include <purple.h>

G_BEGIN_DECLS

#define PURPLE_IRCV3_TYPE_CONNECTION (purple_ircv3_connection_get_type())
G_DECLARE_FINAL_TYPE(PurpleIRCv3Connection, purple_ircv3_connection,
                     PURPLE_IRCV3, CONNECTION, PurpleConnection)

G_GNUC_INTERNAL void purple_ircv3_connection_register(GPluginNativePlugin *plugin);

G_GNUC_INTERNAL GCancellable *purple_ircv3_connection_get_cancellable(PurpleIRCv3Connection *connection);

G_GNUC_INTERNAL gboolean purple_ircv3_connection_valid(PurpleIRCv3Connection *connection, GError **error);

G_GNUC_INTERNAL void purple_ircv3_connection_connect(PurpleIRCv3Connection *connection);

G_GNUC_INTERNAL void purple_ircv3_connection_close(PurpleIRCv3Connection *connection);

G_GNUC_INTERNAL void purple_ircv3_connection_writef(PurpleIRCv3Connection *connection, const char *format, ...) G_GNUC_PRINTF(2, 3);

G_END_DECLS

#endif /* PURPLE_IRCV3_CONNECTION_H */
