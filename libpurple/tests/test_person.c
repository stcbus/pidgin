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
test_purple_person_items_changed_cb(G_GNUC_UNUSED GListModel *model,
                                    G_GNUC_UNUSED guint position,
                                    G_GNUC_UNUSED guint removed,
                                    G_GNUC_UNUSED guint added,
                                    gpointer data)
{
	gboolean *called = data;

	*called = TRUE;
}

static void
test_purple_person_notify_cb(G_GNUC_UNUSED GObject *obj,
                             G_GNUC_UNUSED GParamSpec *pspec,
                             gpointer data)
{
	gboolean *called = data;

	*called = TRUE;
}

/******************************************************************************
 * Tests
 *****************************************************************************/
static void
test_purple_person_new(void) {
	PurplePerson *person = NULL;

	person = purple_person_new();

	g_assert_true(PURPLE_IS_PERSON(person));

	g_clear_object(&person);
}

static void
test_purple_person_properties(void) {
	PurpleContact *person = NULL;
	PurpleTags *tags = NULL;
	GdkPixbuf *avatar = NULL;
	GdkPixbuf *avatar1 = NULL;
	gchar *id = NULL;
	gchar *alias = NULL;

	/* Create our avatar for testing. */
	avatar = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 1, 1);

	/* Use g_object_new so we can test setting properties by name. All of them
	 * call the setter methods, so by doing it this way we exercise more of the
	 * code.
	 */
	person = g_object_new(
		PURPLE_TYPE_PERSON,
		"alias", "alias",
		"avatar", avatar,
		NULL);

	/* Now use g_object_get to read all of the properties. */
	g_object_get(person,
		"id", &id,
		"alias", &alias,
		"avatar", &avatar1,
		"tags", &tags,
		NULL);

	/* Compare all the things. */
	g_assert_nonnull(id);
	g_assert_cmpstr(alias, ==, "alias");
	g_assert_true(avatar1 == avatar);
	g_assert_nonnull(tags);

	/* Free/unref all the things. */
	g_clear_pointer(&id, g_free);
	g_clear_pointer(&alias, g_free);
	g_clear_object(&avatar1);
	g_clear_object(&tags);

	g_clear_object(&avatar);
	g_clear_object(&person);
}

static void
test_purple_person_contacts_single(void) {
	PurpleAccount *account = NULL;
	PurpleContact *contact = NULL;
	PurplePerson *person = NULL;
	guint n_items = 0;
	gboolean removed = FALSE;
	gboolean changed = FALSE;

	account = purple_account_new("test", "test");
	contact = purple_contact_new(account, "username");
	person = purple_person_new();
	g_signal_connect(person, "items-changed",
	                 G_CALLBACK(test_purple_person_items_changed_cb), &changed);

	n_items = g_list_model_get_n_items(G_LIST_MODEL(person));
	g_assert_cmpuint(n_items, ==, 0);
	purple_person_add_contact(person, contact);
	n_items = g_list_model_get_n_items(G_LIST_MODEL(person));
	g_assert_cmpuint(n_items, ==, 1);
	g_assert_true(changed);

	changed = FALSE;

	removed = purple_person_remove_contact(person, contact);
	g_assert_true(removed);
	n_items = g_list_model_get_n_items(G_LIST_MODEL(person));
	g_assert_cmpuint(n_items, ==, 0);
	g_assert_true(changed);

	g_clear_object(&person);
	g_clear_object(&account);
}

static void
test_purple_person_contacts_multiple(void) {
	PurpleAccount *account = NULL;
	PurplePerson *person = NULL;
	GPtrArray *contacts = NULL;
	guint n_items = 0;
	const gint n_contacts = 5;
	gboolean changed = FALSE;

	account = purple_account_new("test", "test");
	person = purple_person_new();
	g_signal_connect(person, "items-changed",
	                 G_CALLBACK(test_purple_person_items_changed_cb), &changed);

	contacts = g_ptr_array_new_full(n_contacts, g_object_unref);
	for(gint i = 0; i < n_contacts; i++) {
		PurpleContact *contact = NULL;
		gchar *username = NULL;

		changed = FALSE;

		n_items = g_list_model_get_n_items(G_LIST_MODEL(person));
		g_assert_cmpuint(n_items, ==, i);

		username = g_strdup_printf("username%d", i);
		contact = purple_contact_new(account, username);
		g_free(username);

		/* Add the contact to the ptr array so we can remove it below. */
		g_ptr_array_add(contacts, contact);

		/* Add the contact to the person and make sure that all the magic
		 * happened.
		 */
		purple_person_add_contact(person, contact);
		n_items = g_list_model_get_n_items(G_LIST_MODEL(person));
		g_assert_cmpuint(n_items, ==, i + 1);
		g_assert_true(changed);
	}

	for(gint i = 0; i < n_contacts; i++) {
		PurpleContact *contact = contacts->pdata[i];
		gboolean removed = FALSE;

		changed = FALSE;

		n_items = g_list_model_get_n_items(G_LIST_MODEL(person));
		g_assert_cmpuint(n_items, ==, n_contacts - i);

		removed = purple_person_remove_contact(person, contact);
		g_assert_true(removed);

		n_items = g_list_model_get_n_items(G_LIST_MODEL(person));
		g_assert_cmpuint(n_items, ==, n_contacts - (i + 1));

		g_assert_true(changed);
	}

	/* Final sanity check that the person has no more contacts. */
	n_items = g_list_model_get_n_items(G_LIST_MODEL(person));
	g_assert_cmpuint(n_items, ==, 0);

	g_ptr_array_free(contacts, TRUE);

	g_clear_object(&person);
	g_clear_object(&account);
}

static void
test_purple_person_priority_single(void) {
	PurpleAccount *account = NULL;
	PurpleContact *contact = NULL;
	PurpleContact *priority = NULL;
	PurplePerson *person = NULL;
	PurplePresence *presence = NULL;
	PurpleStatus *status = NULL;
	PurpleStatusType *status_type = NULL;
	gboolean called = FALSE;

	account = purple_account_new("test", "test");

	person = purple_person_new();
	g_signal_connect(person, "notify::priority-contact",
	                 G_CALLBACK(test_purple_person_notify_cb), &called);
	priority = purple_person_get_priority_contact(person);
	g_assert_null(priority);

	/* Build the presence and status for the contact. */
	presence = g_object_new(PURPLE_TYPE_PRESENCE, NULL);
	status_type = purple_status_type_new(PURPLE_STATUS_AVAILABLE, "available",
	                                     "Available", FALSE);
	status = purple_status_new(status_type, presence);
	g_object_set(G_OBJECT(presence), "active-status", status, NULL);
	g_clear_object(&status);

	/* Now create a real contact. */
	contact = purple_contact_new(account, "username");
	purple_contact_set_presence(contact, presence);
	purple_person_add_contact(person, contact);

	g_assert_true(called);

	priority = purple_person_get_priority_contact(person);
	g_assert_true(priority == contact);

	purple_status_type_destroy(status_type);
	g_clear_object(&account);
	g_clear_object(&person);
	g_clear_object(&contact);
	g_clear_object(&presence);
}

static void
test_purple_person_priority_multiple_with_change(void) {
	PurpleAccount *account = NULL;
	PurpleContact *priority = NULL;
	PurpleContact *first = NULL;
	PurpleContact *sorted_contact = NULL;
	PurplePerson *person = NULL;
	PurplePresence *sorted_presence = NULL;
	PurpleStatus *status = NULL;
	PurpleStatusType *available = NULL;
	PurpleStatusType *offline = NULL;
	gboolean changed = FALSE;
	gint n_contacts = 5;
	guint n_items = 0;

	/* This unit test is a bit complicated, but it adds 5 contacts to a person
	 * all whose presences are set to offline. After adding all the contacts,
	 * we verify that the first contact we added is the priority contact. Then
	 * we flip the active status of the n_contacts - 2 contact to available.
	 * This should make it the priority contact which we then assert.
	 */

	account = purple_account_new("test", "test");

	/* Create our status types. */
	available = purple_status_type_new(PURPLE_STATUS_AVAILABLE, "available",
	                                   "Available", FALSE);
	offline = purple_status_type_new(PURPLE_STATUS_OFFLINE, "offline",
	                                 "Offline", FALSE);

	/* Create the person and connected to the notify signal for the
	 * priority-contact property.
	 */
	person = purple_person_new();
	g_signal_connect(person, "notify::priority-contact",
	                 G_CALLBACK(test_purple_person_notify_cb), &changed);
	priority = purple_person_get_priority_contact(person);
	g_assert_null(priority);

	/* Create and add all contacts. */
	for(gint i = 0; i < n_contacts; i++) {
		PurpleContact *contact = NULL;
		PurplePresence *presence = NULL;
		gchar *username = NULL;

		/* Set changed to false as it shouldn't be changed. */
		changed = FALSE;

		/* Build the presence and status for the contact. */
		presence = g_object_new(PURPLE_TYPE_PRESENCE, NULL);
		status = purple_status_new(offline, presence);
		g_object_set(G_OBJECT(presence), "active-status", status, NULL);
		g_clear_object(&status);

		/* Now create a real contact. */
		username = g_strdup_printf("username%d", i + 1);
		contact = purple_contact_new(account, username);
		g_free(username);
		purple_contact_set_presence(contact, presence);

		purple_person_add_contact(person, contact);

		if(i == 0) {
			first = g_object_ref(contact);
			g_assert_true(changed);
		} else {
			g_assert_false(changed);

			if(i == n_contacts - 2) {
				sorted_contact = g_object_ref(contact);
				sorted_presence = g_object_ref(presence);
			}
		}

		g_clear_object(&contact);
		g_clear_object(&presence);
	}

	n_items = g_list_model_get_n_items(G_LIST_MODEL(person));
	g_assert_cmpuint(n_items, ==, n_contacts);

	priority = purple_person_get_priority_contact(person);
	g_assert_true(priority == first);
	g_clear_object(&first);

	/* Now set the second from the last contact's status to available, and
	 * verify that that contact is now the priority contact.
	 */
	changed = FALSE;
	status = purple_status_new(available, sorted_presence);
	g_object_set(G_OBJECT(sorted_presence), "active-status", status, NULL);
	g_clear_object(&status);
	g_assert_true(changed);
	priority = purple_person_get_priority_contact(person);
	g_assert_true(priority == sorted_contact);

	/* Cleanup. */
	purple_status_type_destroy(offline);
	purple_status_type_destroy(available);

	g_clear_object(&sorted_contact);
	g_clear_object(&sorted_presence);

	g_clear_object(&account);
	g_clear_object(&person);
}

/******************************************************************************
 * Main
 *****************************************************************************/
gint
main(gint argc, gchar *argv[]) {
	g_test_init(&argc, &argv, NULL);

	test_ui_purple_init();

	g_test_add_func("/person/new",
	                test_purple_person_new);
	g_test_add_func("/person/properties",
	                test_purple_person_properties);
	g_test_add_func("/person/contacts/single",
	                test_purple_person_contacts_single);
	g_test_add_func("/person/contacts/multiple",
	                test_purple_person_contacts_multiple);

	g_test_add_func("/person/priority/single",
	                test_purple_person_priority_single);
	g_test_add_func("/person/priority/multiple-with-change",
	                test_purple_person_priority_multiple_with_change);

	return g_test_run();
}