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
 * Tests
 *****************************************************************************/
static void
test_purple_contact_new(void) {
	PurpleAccount *account = NULL;
	PurpleContact *contact = NULL;

	account = purple_account_new("test", "test");
	contact = purple_contact_new(account, "id");

	g_assert_true(purple_contact_get_account(contact) == account);
	g_assert_cmpstr(purple_contact_get_id(contact), ==, "id");

	g_clear_object(&contact);
	g_clear_object(&account);
}

static void
test_purple_contact_properties(void) {
	PurpleAccount *account = NULL;
	PurpleAccount *account1 = NULL;
	PurpleContact *contact = NULL;
	PurplePerson *person = NULL;
	PurplePerson *person1 = NULL;
	PurplePresence *presence = NULL;
	PurplePresence *presence1 = NULL;
	PurpleTags *tags = NULL;
	GdkPixbuf *avatar = NULL;
	GdkPixbuf *avatar1 = NULL;
	gchar *id = NULL;
	gchar *username = NULL;
	gchar *display_name = NULL;
	gchar *alias = NULL;

	account = purple_account_new("test", "test");
	avatar = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8, 1, 1);
	presence = g_object_new(PURPLE_TYPE_PRESENCE, NULL);
	person = purple_person_new();

	/* Use g_object_new so we can test setting properties by name. All of them
	 * call the setter methods, so by doing it this way we exercise more of the
	 * code.
	 */
	contact = g_object_new(
		PURPLE_TYPE_CONTACT,
		"account", account,
		"id", "id1",
		"username", "username",
		"display-name", "display-name",
		"alias", "alias",
		"avatar", avatar,
		"presence", presence,
		"person", person,
		NULL);

	/* Now use g_object_get to read all of the properties. */
	g_object_get(contact,
		"id", &id,
		"account", &account1,
		"username", &username,
		"display-name", &display_name,
		"alias", &alias,
		"avatar", &avatar1,
		"presence", &presence1,
		"tags", &tags,
		"person", &person1,
		NULL);

	/* Compare all the things. */
	g_assert_cmpstr(id, ==, "id1");
	g_assert_true(account1 == account);
	g_assert_cmpstr(username, ==, "username");
	g_assert_cmpstr(display_name, ==, "display-name");
	g_assert_cmpstr(alias, ==, "alias");
	g_assert_true(avatar1 == avatar);
	g_assert_true(presence1 == presence);
	g_assert_nonnull(tags);
	g_assert_true(person1 == person);

	/* Free/unref all the things. */
	g_clear_pointer(&id, g_free);
	g_clear_object(&account1);
	g_clear_pointer(&username, g_free);
	g_clear_pointer(&display_name, g_free);
	g_clear_pointer(&alias, g_free);
	g_clear_object(&avatar1);
	g_clear_object(&presence1);
	g_clear_object(&tags);
	g_clear_object(&person);
	g_clear_object(&person1);

	g_clear_object(&presence);
	g_clear_object(&avatar);
	g_clear_object(&contact);
	g_clear_object(&account);
}

/******************************************************************************
 * Main
 *****************************************************************************/
gint
main(gint argc, gchar *argv[]) {
	g_test_init(&argc, &argv, NULL);

	test_ui_purple_init();

	g_test_add_func("/contact/new",
	                test_purple_contact_new);
	g_test_add_func("/contact/properties",
	                test_purple_contact_properties);

	return g_test_run();
}