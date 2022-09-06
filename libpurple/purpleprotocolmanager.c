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

#include "purpleprotocolmanager.h"
#include "purpleprivate.h"

enum {
	SIG_REGISTERED,
	SIG_UNREGISTERED,
	N_SIGNALS,
};
static guint signals[N_SIGNALS] = {0, };

struct _PurpleProtocolManager {
	GObject parent;

	GHashTable *protocols;
	GArray *list;
};

static PurpleProtocolManager *default_manager = NULL;

/******************************************************************************
 * GListModel Implementation
 *****************************************************************************/
static GType
purple_protocol_manager_get_item_type(GListModel *list) {
	return PURPLE_TYPE_PROTOCOL;
}

static guint
purple_protocol_manager_get_n_items(GListModel *list) {
	PurpleProtocolManager *manager = PURPLE_PROTOCOL_MANAGER(list);

	return manager->list->len;
}

static gpointer
purple_protocol_manager_get_item(GListModel *list, guint position) {
	PurpleProtocolManager *manager = PURPLE_PROTOCOL_MANAGER(list);
	PurpleProtocol *protocol = NULL;

	protocol = g_array_index(manager->list, PurpleProtocol *, position);
	return g_object_ref(protocol);
}

static void
purple_protocol_manager_list_model_iface_init(GListModelInterface *iface) {
	iface->get_item_type = purple_protocol_manager_get_item_type;
	iface->get_n_items = purple_protocol_manager_get_n_items;
	iface->get_item = purple_protocol_manager_get_item;
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_TYPE_WITH_CODE(PurpleProtocolManager, purple_protocol_manager,
                        G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(G_TYPE_LIST_MODEL,
                                              purple_protocol_manager_list_model_iface_init));

static void
purple_protocol_manager_finalize(GObject *obj) {
	PurpleProtocolManager *manager = NULL;

	manager = PURPLE_PROTOCOL_MANAGER(obj);

	g_clear_pointer(&manager->protocols, g_hash_table_destroy);
	g_clear_pointer(&manager->list, g_array_unref);

	G_OBJECT_CLASS(purple_protocol_manager_parent_class)->finalize(obj);
}

static void
purple_protocol_manager_init(PurpleProtocolManager *manager) {
	manager->protocols = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
	                                           g_object_unref);
	manager->list = g_array_new(FALSE, FALSE, sizeof(PurpleProtocol *));
}

static void
purple_protocol_manager_class_init(PurpleProtocolManagerClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->finalize = purple_protocol_manager_finalize;

	/**
	 * PurpleProtocolManager::registered:
	 * @manager: The #PurpleProtocolManager instance.
	 * @protocol: The #PurpleProtocol that was registered.
	 *
	 * Emitted after @protocol has been registered in @manager.
	 *
	 * Since: 3.0.0
	 */
	signals[SIG_REGISTERED] = g_signal_new_class_handler(
		"registered",
		G_OBJECT_CLASS_TYPE(klass),
		G_SIGNAL_RUN_LAST,
		NULL,
		NULL,
		NULL,
		NULL,
		G_TYPE_NONE,
		1,
		PURPLE_TYPE_PROTOCOL);

	/**
	 * PurpleProtocolManager::unregistered:
	 * @manager: The #PurpleProtocolManager instance.
	 * @protocol: The #PurpleProtocol that was unregistered.
	 *
	 * Emitted after @protocol has been unregistered from @manager.
	 *
	 * Since: 3.0.0
	 */
	signals[SIG_UNREGISTERED] = g_signal_new_class_handler(
		"unregistered",
		G_OBJECT_CLASS_TYPE(klass),
		G_SIGNAL_RUN_LAST,
		NULL,
		NULL,
		NULL,
		NULL,
		G_TYPE_NONE,
		1,
		PURPLE_TYPE_PROTOCOL);
}

/******************************************************************************
 * Private API
 *****************************************************************************/
void
purple_protocol_manager_startup(void) {
	if(default_manager == NULL) {
		default_manager = g_object_new(PURPLE_TYPE_PROTOCOL_MANAGER, NULL);
	}
}

void
purple_protocol_manager_shutdown(void) {
	g_clear_object(&default_manager);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
PurpleProtocolManager *
purple_protocol_manager_get_default(void) {
	return default_manager;
}

gboolean
purple_protocol_manager_register(PurpleProtocolManager *manager,
                                 PurpleProtocol *protocol, GError **error)
{
	const gchar *id = NULL;

	g_return_val_if_fail(PURPLE_IS_PROTOCOL_MANAGER(manager), FALSE);
	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), FALSE);

	id = purple_protocol_get_id(protocol);
	if(g_hash_table_lookup(manager->protocols, id) != NULL) {
		g_set_error(error, PURPLE_PROTOCOL_MANAGER_DOMAIN, 0,
		            _("protocol %s is already registered"), id);

		return FALSE;
	}

	g_hash_table_insert(manager->protocols, g_strdup(id),
	                    g_object_ref(protocol));
	g_array_append_val(manager->list, protocol);
	g_list_model_items_changed(G_LIST_MODEL(manager), manager->list->len - 1, 0, 1);

	g_signal_emit(G_OBJECT(manager), signals[SIG_REGISTERED], 0, protocol);

	return TRUE;
}

gboolean
purple_protocol_manager_unregister(PurpleProtocolManager *manager,
                                   PurpleProtocol *protocol, GError **error)
{
	const gchar *id = NULL;
	gboolean ret = FALSE;

	g_return_val_if_fail(PURPLE_IS_PROTOCOL_MANAGER(manager), FALSE);
	g_return_val_if_fail(PURPLE_IS_PROTOCOL(protocol), FALSE);

	/* We need to hold a reference on the protocol as typically we will be
	 * holding the only reference on the protocol when this is called and we
	 * will need to pass it to the signal emission after it's removed from the
	 * hash table that'll unref it.
	 */
	g_object_ref(G_OBJECT(protocol));

	id = purple_protocol_get_id(protocol);

	if(g_hash_table_remove(manager->protocols, id)) {
		for(guint i = 0; i < manager->list->len; i++) {
			if(g_array_index(manager->list, PurpleProtocol *, i) == protocol) {
				g_array_remove_index(manager->list, i);
				g_list_model_items_changed(G_LIST_MODEL(manager), i, 1, 0);
				break;
			}
		}

		g_signal_emit(G_OBJECT(manager), signals[SIG_UNREGISTERED], 0,
		              protocol);

		ret = TRUE;
	} else {
		g_set_error(error, PURPLE_PROTOCOL_MANAGER_DOMAIN, 0,
		            _("protocol %s is not registered"), id);

		ret = FALSE;
	}

	g_object_unref(G_OBJECT(protocol));

	return ret;
}

PurpleProtocol *
purple_protocol_manager_find(PurpleProtocolManager *manager, const gchar *id) {
	gpointer value = NULL;

	g_return_val_if_fail(PURPLE_IS_PROTOCOL_MANAGER(manager), NULL);
	g_return_val_if_fail(id != NULL, NULL);

	value = g_hash_table_lookup(manager->protocols, id);
	if(value == NULL) {
		return NULL;
	}

	return PURPLE_PROTOCOL(value);
}

void
purple_protocol_manager_foreach(PurpleProtocolManager *manager,
                                PurpleProtocolManagerForeachFunc func,
                                gpointer data)
{
	GHashTableIter iter;
	gpointer value;

	g_return_if_fail(PURPLE_IS_PROTOCOL_MANAGER(manager));
	g_return_if_fail(func != NULL);

	g_hash_table_iter_init(&iter, manager->protocols);
	while(g_hash_table_iter_next(&iter, NULL, &value)) {
		func(PURPLE_PROTOCOL(value), data);
	}
}

GList *
purple_protocol_manager_get_all(PurpleProtocolManager *manager) {
	g_return_val_if_fail(PURPLE_IS_PROTOCOL_MANAGER(manager), NULL);

	return g_hash_table_get_values(manager->protocols);
}
