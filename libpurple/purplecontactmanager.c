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

#include "purplecontactmanager.h"

#include "purplegdkpixbuf.h"
#include "purpleprivate.h"
#include "util.h"

enum {
	SIG_ADDED,
	SIG_REMOVED,
	N_SIGNALS,
};
static guint signals[N_SIGNALS] = {0, };

struct _PurpleContactManager {
	GObject parent;

	GHashTable *accounts;
};

static PurpleContactManager *default_manager = NULL;

/******************************************************************************
 * Helpers
 *****************************************************************************/
static gboolean
purple_contact_manager_find_with_username_helper(gconstpointer a,
                                                 gconstpointer b)
{
	PurpleContact *contact_a = (gpointer)a;
	PurpleContact *contact_b = (gpointer)b;
	const gchar *username_a = NULL;
	const gchar *username_b = NULL;

	username_a = purple_contact_get_username(contact_a);
	username_b = purple_contact_get_username(contact_b);

	return purple_strequal(username_a, username_b);
}

static gboolean
purple_contact_manager_find_with_id_helper(gconstpointer a, gconstpointer b) {
	PurpleContact *contact_a = (gpointer)a;
	PurpleContact *contact_b = (gpointer)b;
	const gchar *id_a = NULL;
	const gchar *id_b = NULL;

	id_a = purple_contact_get_id(contact_a);
	id_b = purple_contact_get_id(contact_b);

	return purple_strequal(id_a, id_b);
}

static gboolean
purple_contact_manager_convert_icon_to_avatar(GBinding *binding,
                                              const GValue *from_value,
                                              GValue *to_value,
                                              gpointer user_data)
{
	PurpleBuddyIcon *icon = g_value_get_pointer(from_value);
	GdkPixbuf *avatar = NULL;
	gconstpointer data = NULL;
	size_t len;

	if(icon == NULL) {
		g_value_set_object(to_value, NULL);
		return TRUE;
	}

	data = purple_buddy_icon_get_data(icon, &len);

	avatar = purple_gdk_pixbuf_from_data(data, len);

	g_value_take_object(to_value, avatar);

	return TRUE;
}

static gboolean
purple_contact_manager_convert_avatar_to_icon(GBinding *binding,
                                              const GValue *from_value,
                                              GValue *to_value,
                                              gpointer user_data)
{
	PurpleBuddyIcon *icon = NULL;
	GdkPixbuf *avatar = g_value_get_object(from_value);
	gchar *buffer = NULL;
	gsize len;
	gboolean result = FALSE;

	if(!GDK_IS_PIXBUF(avatar)) {
		g_value_set_pointer(to_value, NULL);

		return TRUE;
	}

	result = gdk_pixbuf_save_to_buffer(avatar, &buffer, &len, "png", NULL,
	                                   "compression", "9", NULL);
	if(!result) {
		return FALSE;
	}

	purple_buddy_icon_set_data(icon, (guchar *)buffer, len, NULL);

	g_free(buffer);

	return TRUE;
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_TYPE(PurpleContactManager, purple_contact_manager, G_TYPE_OBJECT)

static void
purple_contact_manager_dispose(GObject *obj) {
	PurpleContactManager *manager = NULL;

	manager = PURPLE_CONTACT_MANAGER(obj);

	g_hash_table_remove_all(manager->accounts);

	G_OBJECT_CLASS(purple_contact_manager_parent_class)->dispose(obj);
}

static void
purple_contact_manager_finalize(GObject *obj) {
	PurpleContactManager *manager = NULL;

	manager = PURPLE_CONTACT_MANAGER(obj);

	g_clear_pointer(&manager->accounts, g_hash_table_destroy);

	G_OBJECT_CLASS(purple_contact_manager_parent_class)->finalize(obj);
}

static void
purple_contact_manager_init(PurpleContactManager *manager) {
	manager->accounts = g_hash_table_new_full(g_direct_hash, g_direct_equal,
	                                          g_object_unref, g_object_unref);
}

static void
purple_contact_manager_class_init(PurpleContactManagerClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->dispose = purple_contact_manager_dispose;
	obj_class->finalize = purple_contact_manager_finalize;

	/**
	 * PurpleContactManager::added:
	 * @manager: The instance.
	 * @contact: The [class@Purple.Contact] that was registered.
	 *
	 * Emitted after @contact has been added to @manager.
	 *
	 * Since: 3.0.0
	 */
	signals[SIG_ADDED] = g_signal_new_class_handler(
		"added",
		G_OBJECT_CLASS_TYPE(klass),
		G_SIGNAL_RUN_LAST,
		NULL,
		NULL,
		NULL,
		NULL,
		G_TYPE_NONE,
		1,
		PURPLE_TYPE_CONTACT);

	/**
	 * PurpleContactManager::removed:
	 * @manager: The instance.
	 * @contact: The [class@Purple.Contact] that was removed.
	 *
	 * Emitted after @contact has been removed from @manager.
	 *
	 * Since: 3.0.0
	 */
	signals[SIG_REMOVED] = g_signal_new_class_handler(
		"removed",
		G_OBJECT_CLASS_TYPE(klass),
		G_SIGNAL_RUN_LAST,
		NULL,
		NULL,
		NULL,
		NULL,
		G_TYPE_NONE,
		1,
		PURPLE_TYPE_CONTACT);
}

/******************************************************************************
 * Private API
 *****************************************************************************/
void
purple_contact_manager_startup(void) {
	if(default_manager == NULL) {
		default_manager = g_object_new(PURPLE_TYPE_CONTACT_MANAGER, NULL);
		g_object_add_weak_pointer(G_OBJECT(default_manager),
		                          (gpointer)&default_manager);
	}
}

void
purple_contact_manager_shutdown(void) {
	g_clear_object(&default_manager);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
PurpleContactManager *
purple_contact_manager_get_default(void) {
	if(G_UNLIKELY(!PURPLE_IS_CONTACT_MANAGER(default_manager))) {
		g_warning("The default contact manager was unexpectedly NULL");
	}

	return default_manager;
}

void
purple_contact_manager_add(PurpleContactManager *manager,
                           PurpleContact *contact)
{
	PurpleAccount *account = NULL;
	GListStore *contacts = NULL;
	gboolean added = FALSE;

	g_return_if_fail(PURPLE_IS_CONTACT_MANAGER(manager));
	g_return_if_fail(PURPLE_IS_CONTACT(contact));

	account = purple_contact_get_account(contact);
	contacts = g_hash_table_lookup(manager->accounts, account);
	if(!G_IS_LIST_STORE(contacts)) {
		contacts = g_list_store_new(PURPLE_TYPE_CONTACT);
		g_hash_table_insert(manager->accounts, g_object_ref(account), contacts);

		g_list_store_append(contacts, contact);

		added = TRUE;
	} else {
		if(g_list_store_find(contacts, contact, NULL)) {
			const gchar *username = purple_contact_get_username(contact);
			const gchar *id = purple_contact_get_id(contact);

			g_warning("double add detected for contact %s:%s", id, username);

			return;
		}

		g_list_store_append(contacts, contact);
		added = TRUE;
	}

	if(added) {
		g_signal_emit(manager, signals[SIG_ADDED], 0, contact);
	}
}

gboolean
purple_contact_manager_remove(PurpleContactManager *manager,
                              PurpleContact *contact)
{
	PurpleAccount *account = NULL;
	GListStore *contacts = NULL;
	guint position = 0;

	g_return_val_if_fail(PURPLE_IS_CONTACT_MANAGER(manager), FALSE);
	g_return_val_if_fail(PURPLE_IS_CONTACT(contact), FALSE);

	account = purple_contact_get_account(contact);
	contacts = g_hash_table_lookup(manager->accounts, account);
	if(!G_IS_LIST_STORE(contacts)) {
		return FALSE;
	}

	if(g_list_store_find(contacts, contact, &position)) {
		gboolean removed = FALSE;
		guint len = 0;

		/* Ref the contact to make sure that the instance is valid when we emit
		 * the removed signal.
		 */
		g_object_ref(contact);

		len = g_list_model_get_n_items(G_LIST_MODEL(contacts));
		g_list_store_remove(contacts, position);
		if(g_list_model_get_n_items(G_LIST_MODEL(contacts)) < len) {
			removed = TRUE;
		}

		if(removed) {
			g_signal_emit(manager, signals[SIG_REMOVED], 0, contact);
		}

		g_object_unref(contact);

		return removed;
	}

	return FALSE;
}

gboolean
purple_contact_manager_remove_all(PurpleContactManager *manager,
                                  PurpleAccount *account)
{
	GListStore *contacts = NULL;

	g_return_val_if_fail(PURPLE_IS_CONTACT_MANAGER(manager), FALSE);
	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), FALSE);

	/* If there are any contacts for this account, manually iterate them and
	 * emit the removed signal. This is more efficient than calling remove on
	 * each one individually as that would require updating the backing
	 * GListStore for each individual removal.
	 */
	contacts = g_hash_table_lookup(manager->accounts, account);
	if(G_IS_LIST_STORE(contacts)) {
		guint n_items = g_list_model_get_n_items(G_LIST_MODEL(contacts));
		for(guint i = 0; i < n_items; i++) {
			PurpleContact *contact = NULL;

			contact = g_list_model_get_item(G_LIST_MODEL(contacts), i);

			g_signal_emit(manager, signals[SIG_REMOVED], 0, contact);

			g_clear_object(&contact);
		}
	}

	return g_hash_table_remove(manager->accounts, account);
}

GListModel *
purple_contact_manager_get_all(PurpleContactManager *manager,
                               PurpleAccount *account)
{
	g_return_val_if_fail(PURPLE_IS_CONTACT_MANAGER(manager), FALSE);
	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), FALSE);

	return g_hash_table_lookup(manager->accounts, account);
}

PurpleContact *
purple_contact_manager_find_with_username(PurpleContactManager *manager,
                                          PurpleAccount *account,
                                          const gchar *username)
{
	PurpleContact *needle = NULL;
	GListStore *contacts = NULL;
	guint position = 0;
	gboolean found = FALSE;

	g_return_val_if_fail(PURPLE_IS_CONTACT_MANAGER(manager), FALSE);
	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), FALSE);
	g_return_val_if_fail(username != NULL, FALSE);

	contacts = g_hash_table_lookup(manager->accounts, account);
	if(!G_IS_LIST_STORE(contacts)) {
		return NULL;
	}

	needle = purple_contact_new(account, NULL);
	purple_contact_set_username(needle, username);
	found = g_list_store_find_with_equal_func(contacts, needle,
	                                          purple_contact_manager_find_with_username_helper,
	                                          &position);
	g_clear_object(&needle);

	if(found) {
		return g_list_model_get_item(G_LIST_MODEL(contacts), position);
	}

	return NULL;
}

PurpleContact *
purple_contact_manager_find_with_id(PurpleContactManager *manager,
                                    PurpleAccount *account, const gchar *id)
{
	PurpleContact *needle = NULL;
	GListStore *contacts = NULL;
	guint position = 0;
	gboolean found = FALSE;

	g_return_val_if_fail(PURPLE_IS_CONTACT_MANAGER(manager), FALSE);
	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), FALSE);
	g_return_val_if_fail(id != NULL, FALSE);

	contacts = g_hash_table_lookup(manager->accounts, account);
	if(!G_IS_LIST_STORE(contacts)) {
		return NULL;
	}

	needle = purple_contact_new(account, id);
	found = g_list_store_find_with_equal_func(contacts, needle,
	                                          purple_contact_manager_find_with_id_helper,
	                                          &position);
	g_clear_object(&needle);

	if(found) {
		return g_list_model_get_item(G_LIST_MODEL(contacts), position);
	}

	return NULL;
}

/******************************************************************************
 * Migration API
 *****************************************************************************/
void
purple_contact_manager_add_buddy(PurpleContactManager *manager,
                                 PurpleBuddy *buddy)
{
	PurpleAccount *account = NULL;
	PurpleContact *contact = NULL;
	PurplePresence *buddy_presence = NULL;
	PurplePresence *contact_presence = NULL;
	const gchar *id = NULL;

	g_return_if_fail(PURPLE_IS_CONTACT_MANAGER(manager));
	g_return_if_fail(PURPLE_IS_BUDDY(buddy));

	/* Create the new contact. */
	account = purple_buddy_get_account(buddy);
	id = purple_buddy_get_id(buddy);
	contact = purple_contact_new(account, id);

	/* Bind all of the properties. */
	g_object_bind_property(buddy, "name", contact, "username",
	                       G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	g_object_bind_property(buddy, "local-alias", contact, "alias",
	                       G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	g_object_bind_property(buddy, "server-alias", contact, "display-name",
	                       G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

	buddy_presence = purple_buddy_get_presence(buddy);
	contact_presence = purple_contact_get_presence(contact);

	g_object_bind_property(buddy_presence, "idle", contact_presence, "idle",
	                       G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	g_object_bind_property(buddy_presence, "idle-time", contact_presence,
	                       "idle-time",
	                       G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	g_object_bind_property(buddy_presence, "login-time", contact_presence,
	                       "login-time",
	                       G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	g_object_bind_property(buddy_presence, "active-status", contact_presence,
	                       "active-status",
	                       G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

	g_object_bind_property_full(buddy, "icon", contact, "avatar",
	                            G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL,
	                            purple_contact_manager_convert_icon_to_avatar,
	                            purple_contact_manager_convert_avatar_to_icon,
	                            NULL, NULL);

	/* Finally add it to the manager. */
	purple_contact_manager_add(manager, contact);

	/* purple_contact_manager_add adds its own reference, so free our copy. */
	g_clear_object(&contact);
}
