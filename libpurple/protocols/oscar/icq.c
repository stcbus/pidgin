/* purple
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 *
 */

#include "icq.h"

#include "core.h"
#include "plugins.h"
#include "signals.h"

#include "oscarcommon.h"

static GHashTable *
icq_get_account_text_table(PurpleAccount *account)
{
	GHashTable *table;
	table = g_hash_table_new(g_str_hash, g_str_equal);
	g_hash_table_insert(table, "login_label", (gpointer)_("ICQ UIN..."));
	return table;
}

static gssize
icq_get_max_message_size(PurpleConversation *conv)
{
	/* XXX: got from pidgin-otr - verify and document it */
	return 2346;
}

static void
icq_protocol_init(ICQProtocol *self)
{
	PurpleProtocol *protocol = PURPLE_PROTOCOL(self);

	protocol->id   = "prpl-icq";
	protocol->name = "ICQ";

	oscar_init_account_options(protocol, TRUE);
}

static void
icq_protocol_class_init(ICQProtocolClass *klass)
{
	PurpleProtocolClass *protocol_class = PURPLE_PROTOCOL_CLASS(klass);

	protocol_class->list_icon = oscar_list_icon_icq;
}

static void
icq_protocol_class_finalize(G_GNUC_UNUSED ICQProtocolClass *klass)
{
}

static void
icq_protocol_client_iface_init(PurpleProtocolClientInterface *client_iface)
{
	client_iface->get_account_text_table = icq_get_account_text_table;
	client_iface->get_moods              = oscar_get_purple_moods;
	client_iface->get_max_message_size   = icq_get_max_message_size;
}

G_DEFINE_DYNAMIC_TYPE_EXTENDED(
        ICQProtocol, icq_protocol, OSCAR_TYPE_PROTOCOL, 0,

        G_IMPLEMENT_INTERFACE_DYNAMIC(PURPLE_TYPE_PROTOCOL_CLIENT,
                                      icq_protocol_client_iface_init));

/* This exists solely because the above macro makes icq_protocol_register_type
 * static. */
void
icq_protocol_register(PurplePlugin *plugin)
{
	icq_protocol_register_type(G_TYPE_MODULE(plugin));
}
