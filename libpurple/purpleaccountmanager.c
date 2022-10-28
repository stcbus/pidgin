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

#include "purpleaccountmanager.h"
#include "purpleprivate.h"

#include "account.h"
#include "accounts.h"
#include "core.h"

enum {
	SIG_ADDED,
	SIG_REMOVED,
	N_SIGNALS,
};
static guint signals[N_SIGNALS] = {0, };

struct _PurpleAccountManager {
	GObject parent;

	GPtrArray *accounts;
};

static PurpleAccountManager *default_manager = NULL;

/******************************************************************************
 * GListModel Implementation
 *****************************************************************************/
static GType
purple_account_get_item_type(G_GNUC_UNUSED GListModel *list) {
	return PURPLE_TYPE_ACCOUNT;
}

static guint
purple_account_get_n_items(GListModel *list) {
	PurpleAccountManager *manager = PURPLE_ACCOUNT_MANAGER(list);

	return manager->accounts->len;
}

static gpointer
purple_account_get_item(GListModel *list, guint position) {
	PurpleAccountManager *manager = PURPLE_ACCOUNT_MANAGER(list);
	PurpleAccount *account = NULL;

	if(position < manager->accounts->len) {
		account = g_ptr_array_index(manager->accounts, position);
		g_object_ref(account);
	}

	return account;
}

static void
purple_account_manager_list_model_init(GListModelInterface *iface) {
	iface->get_item_type = purple_account_get_item_type;
	iface->get_n_items = purple_account_get_n_items;
	iface->get_item = purple_account_get_item;
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_TYPE_EXTENDED(PurpleAccountManager, purple_account_manager,
                       G_TYPE_OBJECT, 0,
                       G_IMPLEMENT_INTERFACE(G_TYPE_LIST_MODEL,
                                             purple_account_manager_list_model_init))

static void
purple_account_manager_finalize(GObject *obj) {
	PurpleAccountManager *manager = PURPLE_ACCOUNT_MANAGER(obj);

	if(manager->accounts != NULL) {
		g_ptr_array_free(manager->accounts, TRUE);
	}

	G_OBJECT_CLASS(purple_account_manager_parent_class)->finalize(obj);
}

static void
purple_account_manager_init(PurpleAccountManager *manager) {
	manager->accounts = g_ptr_array_new_full(0, (GDestroyNotify)g_object_unref);
}

static void
purple_account_manager_class_init(PurpleAccountManagerClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->finalize = purple_account_manager_finalize;

	/**
	 * PurpleAccountManager::added:
	 * @manager: The account manager instance.
	 * @account: The account that was added.
	 *
	 * Emitted after @account was added to @manager.
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
		PURPLE_TYPE_ACCOUNT);

	/**
	 * PurpleAccountManager::removed:
	 * @manager: The account manager instance.
	 * @account: The account that was removed.
	 *
	 * Emitted after @account was removed from @manager.
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
		PURPLE_TYPE_ACCOUNT);
}

/******************************************************************************
 * Private API
 *****************************************************************************/
void
purple_account_manager_startup(void) {
	if(!PURPLE_IS_ACCOUNT_MANAGER(default_manager)) {
		default_manager = g_object_new(PURPLE_TYPE_ACCOUNT_MANAGER, NULL);
	}
}

void
purple_account_manager_shutdown(void) {
	g_clear_object(&default_manager);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
PurpleAccountManager *
purple_account_manager_get_default(void) {
	return default_manager;
}

GListModel *
purple_account_manager_get_default_as_model(void) {
	return G_LIST_MODEL(default_manager);
}

void
purple_account_manager_add(PurpleAccountManager *manager,
                           PurpleAccount *account)
{
	g_return_if_fail(PURPLE_IS_ACCOUNT_MANAGER(manager));
	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	/* If the manager already knows about the account, we do nothing. */
	if(g_ptr_array_find(manager->accounts, account, NULL)) {
		return;
	}

	/* Since the manager doesn't know about the account, put the new account
	 * at the start of the list as that's likely to be the first one in user
	 * interfaces and the most likely to have configuration issues as it's a
	 * new account.
	 */
	g_ptr_array_insert(manager->accounts, 0, account);

	purple_accounts_schedule_save();

	g_signal_emit(manager, signals[SIG_ADDED], 0, account);
	g_list_model_items_changed(G_LIST_MODEL(manager), 0, 0, 1);
}

void
purple_account_manager_remove(PurpleAccountManager *manager,
                              PurpleAccount *account)
{
	guint index = 0;

	g_return_if_fail(PURPLE_IS_ACCOUNT_MANAGER(manager));
	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	if(g_ptr_array_find(manager->accounts, account, &index)) {
		g_ptr_array_steal_index(manager->accounts, index);
		g_list_model_items_changed(G_LIST_MODEL(manager), index, 1, 0);
	}

	purple_accounts_schedule_save();

	/* Clearing the error ensures that account-error-changed is emitted,
	 * which is the end of the guarantee that the error's pointer is valid.
	 */
	purple_account_clear_current_error(account);

	g_signal_emit(manager, signals[SIG_REMOVED], 0, account);
}

GList *
purple_account_manager_get_enabled(PurpleAccountManager *manager) {
	GList *enabled = NULL;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT_MANAGER(manager), NULL);

	for(guint index = 0; index < manager->accounts->len; index++) {
		PurpleAccount *account = g_ptr_array_index(manager->accounts, index);

		if(purple_account_get_enabled(account)) {
			enabled = g_list_append(enabled, account);
		}
	}

	return enabled;
}

GList *
purple_account_manager_get_disabled(PurpleAccountManager *manager) {
	GList *disabled = NULL;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT_MANAGER(manager), NULL);

	for(guint index = 0; index < manager->accounts->len; index++) {
		PurpleAccount *account = g_ptr_array_index(manager->accounts, index);

		if(!purple_account_get_enabled(account)) {
			disabled = g_list_append(disabled, account);
		}
	}

	return disabled;
}

void
purple_account_manager_reorder(PurpleAccountManager *manager,
                               PurpleAccount *account,
                               guint new_index)
{
	guint index = 0;

	g_return_if_fail(PURPLE_IS_ACCOUNT_MANAGER(manager));
	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	if(g_ptr_array_find(manager->accounts, account, &index)) {
		g_ptr_array_steal_index(manager->accounts, index);
		g_list_model_items_changed(G_LIST_MODEL(manager), index, 1, 0);

		/* If new_index is greater than the current index, we need to
		 * decrement new_index by 1 to account for the move as we'll be
		 * inserting into a list with one less item.
		 */
		if(new_index > index) {
			new_index--;
		}
	} else {
		g_critical("Unregistered account (%s) found during reorder!",
		           purple_account_get_username(account));
		return;
	}

	/* Insert the account into its new position. */
	g_ptr_array_insert(manager->accounts, new_index, account);
	g_list_model_items_changed(G_LIST_MODEL(manager), new_index, 0, 1);

	purple_accounts_schedule_save();
}

PurpleAccount *
purple_account_manager_find_by_id(PurpleAccountManager *manager,
                                  const gchar *id)
{
	g_return_val_if_fail(PURPLE_IS_ACCOUNT_MANAGER(manager), NULL);
	g_return_val_if_fail(id != NULL, NULL);

	for(guint index = 0; index < manager->accounts->len; index++) {
		PurpleAccount *account = g_ptr_array_index(manager->accounts, index);

		if(purple_strequal(purple_account_get_id(account), id)) {
			return account;
		}
	}

	return NULL;
}

PurpleAccount *
purple_account_manager_find(PurpleAccountManager *manager,
                            const gchar *username, const gchar *protocol_id)
{
	g_return_val_if_fail(PURPLE_IS_ACCOUNT_MANAGER(manager), NULL);
	g_return_val_if_fail(username != NULL, NULL);
	g_return_val_if_fail(protocol_id != NULL, NULL);

	for(guint index = 0; index < manager->accounts->len; index++) {
		PurpleAccount *account = g_ptr_array_index(manager->accounts, index);
		gchar *normalized = NULL;
		const gchar *existing_protocol_id = NULL;
		const gchar *existing_username = NULL;
		const gchar *existing_normalized = NULL;

		/* Check if the protocol id matches what the user asked for. */
		existing_protocol_id = purple_account_get_protocol_id(account);
		if(!purple_strequal(existing_protocol_id, protocol_id)) {
			continue;
		}

		/* Finally verify the username. */
		existing_username = purple_account_get_username(account);
		normalized = g_strdup(purple_normalize(account, username));
		existing_normalized = purple_normalize(account, existing_username);

		if(purple_strequal(existing_normalized, normalized)) {
			g_free(normalized);

			return account;
		}
		g_free(normalized);
	}

	return NULL;
}

PurpleAccount *
purple_account_manager_find_custom(PurpleAccountManager *manager,
                                   GEqualFunc func, gconstpointer data)
{
	guint index = 0;

	g_return_val_if_fail(PURPLE_IS_ACCOUNT_MANAGER(manager), NULL);
	g_return_val_if_fail(func != NULL, NULL);

	if(g_ptr_array_find_with_equal_func(manager->accounts, data, func, &index)) {
		PurpleAccount *account = g_ptr_array_index(manager->accounts, index);

		return account;
	}

	return NULL;
}

void
purple_account_manager_foreach(PurpleAccountManager *manager,
                               PurpleAccountManagerForeachFunc callback,
                               gpointer data)
{
	g_return_if_fail(PURPLE_IS_ACCOUNT_MANAGER(manager));
	g_return_if_fail(callback != NULL);

	for(guint index = 0; index < manager->accounts->len; index++) {
		PurpleAccount *account = g_ptr_array_index(manager->accounts, index);
		callback(account, data);
	}
}
