/*
 * Purple - Internet Messaging Library
 * Copyright (C) Pidgin Developers <devel@pidgin.im>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <https://www.gnu.org/licenses/>.
 */

#include <glib.h>

#include <purple.h>

#include "test_ui.h"

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
test_purple_contact_manager_increment_cb(G_GNUC_UNUSED PurpleContactManager *manager,
                                         G_GNUC_UNUSED PurpleContact *contact,
                                         gpointer data)
{
	gint *called = data;

	*called = *called + 1;
}

/******************************************************************************
 * Tests
 *****************************************************************************/
static void
test_purple_contact_manager_get_default(void) {
	PurpleContactManager *manager1 = NULL, *manager2 = NULL;

	manager1 = purple_contact_manager_get_default();
	g_assert_true(PURPLE_IS_CONTACT_MANAGER(manager1));

	manager2 = purple_contact_manager_get_default();
	g_assert_true(PURPLE_IS_CONTACT_MANAGER(manager2));

	g_assert_true(manager1 == manager2);
}

static void
test_purple_contact_manager_add_remove(void) {
	PurpleAccount *account = NULL;
	PurpleContactManager *manager = NULL;
	PurpleContact *contact = NULL;
	GListModel *contacts = NULL;
	gint added_called = 0, removed_called = 0;

	manager = g_object_new(PURPLE_TYPE_CONTACT_MANAGER, NULL);

	g_assert_true(PURPLE_IS_CONTACT_MANAGER(manager));

	/* Wire up our signals. */
	g_signal_connect(manager, "added",
	                 G_CALLBACK(test_purple_contact_manager_increment_cb),
	                 &added_called);
	g_signal_connect(manager, "removed",
	                 G_CALLBACK(test_purple_contact_manager_increment_cb),
	                 &removed_called);

	/* Create our account. */
	account = purple_account_new("test", "test");

	/* Create the contact and add it to the manager. */
	contact = purple_contact_new(account, "test-user");

	/* Add the contact to the manager. */
	purple_contact_manager_add(manager, contact);

	/* Make sure the contact is available. */
	contacts = purple_contact_manager_get_all(manager, account);
	g_assert_nonnull(contacts);
	g_assert_cmpuint(g_list_model_get_n_items(contacts), ==, 1);

	/* Make sure the added signal was called. */
	g_assert_cmpint(added_called, ==, 1);

	/* Remove the contact. */
	purple_contact_manager_remove(manager, contact);
	g_assert_cmpint(removed_called, ==, 1);

	/* Clean up.*/
	g_clear_object(&contact);
	g_clear_object(&account);
	g_clear_object(&manager);
}

static void
test_purple_contact_manager_double_add(void) {
	if(g_test_subprocess()) {
		PurpleAccount *account = NULL;
		PurpleContactManager *manager = NULL;
		PurpleContact *contact = NULL;

		manager = g_object_new(PURPLE_TYPE_CONTACT_MANAGER, NULL);

		account = purple_account_new("test", "test");

		contact = purple_contact_new(account, "test-user");

		purple_contact_manager_add(manager, contact);
		purple_contact_manager_add(manager, contact);

		/* This will never get called as the double add outputs a g_warning()
		 * that causes the test to fail. This is left to avoid a false postive
		 * in static analysis.
		 */
		g_clear_object(&account);
		g_clear_object(&contact);
		g_clear_object(&manager);
	}

	g_test_trap_subprocess(NULL, 0, 0);
	g_test_trap_assert_stderr("*Purple-WARNING*double add detected for contact*");
}

static void
test_purple_contact_manager_double_remove(void) {
	PurpleAccount *account = NULL;
	PurpleContactManager *manager = NULL;
	PurpleContact *contact = NULL;
	gint removed_called = 0;

	manager = g_object_new(PURPLE_TYPE_CONTACT_MANAGER, NULL);
	g_signal_connect(manager, "removed",
	                 G_CALLBACK(test_purple_contact_manager_increment_cb),
	                 &removed_called);

	account = purple_account_new("test", "test");

	contact = purple_contact_new(account, "test-user");

	purple_contact_manager_add(manager, contact);

	g_assert_true(purple_contact_manager_remove(manager, contact));
	g_assert_false(purple_contact_manager_remove(manager, contact));

	g_assert_cmpint(removed_called, ==, 1);

	g_clear_object(&account);
	g_clear_object(&contact);
	g_clear_object(&manager);
}

static void
test_purple_contact_manager_remove_all(void) {
	PurpleAccount *account = NULL;
	PurpleContact *contact = NULL;
	PurpleContactManager *manager = NULL;
	GListModel *contacts = NULL;

	manager = g_object_new(PURPLE_TYPE_CONTACT_MANAGER, NULL);

	account = purple_account_new("test", "test");

	contacts = purple_contact_manager_get_all(manager, account);
	g_assert_null(contacts);

	contact = purple_contact_new(account, "test-user");
	purple_contact_manager_add(manager, contact);

	contacts = purple_contact_manager_get_all(manager, account);
	g_assert_nonnull(contacts);
	g_assert_cmpuint(g_list_model_get_n_items(contacts), ==, 1);

	g_assert_true(purple_contact_manager_remove_all(manager, account));
	contacts = purple_contact_manager_get_all(manager, account);
	g_assert_null(contacts);

	/* Cleanup */
	g_clear_object(&account);
	g_clear_object(&contact);
	g_clear_object(&manager);
}

static void
test_purple_contact_manager_find_with_username(void) {
	PurpleAccount *account = NULL;
	PurpleContact *contact1 = NULL;
	PurpleContact *contact2 = NULL;
	PurpleContact *found = NULL;
	PurpleContactManager *manager = NULL;

	manager = g_object_new(PURPLE_TYPE_CONTACT_MANAGER, NULL);

	account = purple_account_new("test", "test");

	contact1 = purple_contact_new(account, "user1");
	purple_contact_manager_add(manager, contact1);

	contact2 = purple_contact_new(account, "user2");
	purple_contact_manager_add(manager, contact2);

	found = purple_contact_manager_find_with_username(manager, account,
	                                                  "user1");
	g_assert_nonnull(found);
	g_assert_true(found == contact1);
	g_clear_object(&found);

	found = purple_contact_manager_find_with_username(manager, account,
	                                                  "user2");
	g_assert_nonnull(found);
	g_assert_true(found == contact2);
	g_clear_object(&found);

	/* Cleanup. */
	g_clear_object(&account);
	g_clear_object(&contact1);
	g_clear_object(&contact2);
	g_clear_object(&manager);
}

static void
test_purple_contact_manager_find_with_id(void) {
	PurpleAccount *account = NULL;
	PurpleContact *contact1 = NULL;
	PurpleContact *contact2 = NULL;
	PurpleContact *found = NULL;
	PurpleContactManager *manager = NULL;

	manager = g_object_new(PURPLE_TYPE_CONTACT_MANAGER, NULL);

	account = purple_account_new("test", "test");

	contact1 = g_object_new(PURPLE_TYPE_CONTACT, "account", account, "id",
	                        "id-1", NULL);
	purple_contact_manager_add(manager, contact1);

	contact2 = g_object_new(PURPLE_TYPE_CONTACT, "account", account, "id",
	                        "id-2", NULL);
	purple_contact_manager_add(manager, contact2);

	found = purple_contact_manager_find_with_id(manager, account, "id-1");
	g_assert_nonnull(found);
	g_assert_true(found == contact1);
	g_clear_object(&found);

	found = purple_contact_manager_find_with_id(manager, account, "id-2");
	g_assert_nonnull(found);
	g_assert_true(found == contact2);
	g_clear_object(&found);

	/* Cleanup. */
	g_clear_object(&account);
	g_clear_object(&contact1);
	g_clear_object(&contact2);
	g_clear_object(&manager);
}

/******************************************************************************
 * Main
 *****************************************************************************/
gint
main(gint argc, gchar *argv[]) {
	g_test_init(&argc, &argv, NULL);

	test_ui_purple_init();

	g_test_add_func("/contact-manager/get-default",
	                test_purple_contact_manager_get_default);
	g_test_add_func("/contact-manager/add-remove",
	                test_purple_contact_manager_add_remove);
	g_test_add_func("/contact-manager/double-add",
	                test_purple_contact_manager_double_add);
	g_test_add_func("/contact-manager/double-remove",
	                test_purple_contact_manager_double_remove);

	g_test_add_func("/contact-manager/remove-all",
	                test_purple_contact_manager_remove_all);

	g_test_add_func("/contact-manager/find/with-username",
	                test_purple_contact_manager_find_with_username);
	g_test_add_func("/contact-manager/find/with-id",
	                test_purple_contact_manager_find_with_id);

	return g_test_run();
}
