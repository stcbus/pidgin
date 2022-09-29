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

#if !defined(PURPLE_GLOBAL_HEADER_INSIDE) && !defined(PURPLE_COMPILATION)
# error "only <pidgin.h> may be included directly"
#endif

#ifndef PURPLE_CONTACT_H
#define PURPLE_CONTACT_H

#include <glib.h>
#include <glib-object.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libpurple/account.h>
#include <libpurple/purpletags.h>

G_BEGIN_DECLS

#define PURPLE_TYPE_CONTACT (purple_contact_get_type())
G_DECLARE_FINAL_TYPE(PurpleContact, purple_contact, PURPLE, CONTACT, GObject)

/**
 * PurpleContact:
 *
 * A representation of a user. Contacts are used everywhere you need to refer to
 * a user. Be it a chat, an direct message, a file transfer, etc.
 */

/**
 * purple_contact_new:
 * @account: The [class@Purple.Account] this contact is from.
 * @username: The username of the contact.
 *
 * Creates a new [class@Purple.Contact].
 */
PurpleContact *purple_contact_new(PurpleAccount *account, const gchar *username);

/**
 * purple_contact_get_account:
 * @contact: The instance.
 *
 * Gets the [class@Purple.Account] that @contact belongs to.
 *
 * Returns: (transfer none): The [class@Purple.Account] that @contact belongs
 *          to.
 *
 * Since: 3.0.0
 */
PurpleAccount *purple_contact_get_account(PurpleContact *contact);

/**
 * purple_contact_get_id:
 * @contact: The instance.
 *
 * Gets the id of @contact.
 *
 * If a protocol would like to set this, it should call
 * [ctor@GObject.Object.new] and pass in the id attribute manually.
 *
 * Returns: The id of the contact.
 *
 * Since: 3.0.0
 */
const gchar *purple_contact_get_id(PurpleContact *contact);

/**
 * purple_contact_get_username:
 * @contact: The instance.
 *
 * Gets the username of @contact.
 *
 * Returns: The username of @contact.
 *
 * Since: 3.0.0
 */
const gchar *purple_contact_get_username(PurpleContact *contact);

/**
 * purple_contact_set_username:
 * @contact: The instance.
 * @username: The new username.
 *
 * Sets the username of @contact to @username.
 *
 * This is primarily used by protocol plugins like IRC when a user changes
 * their "nick" which is their username.
 *
 * Since: 3.0.0
 */
void purple_contact_set_username(PurpleContact *contact, const gchar *username);

/**
 * purple_contact_get_display_name:
 * @contact: The instance.
 *
 * Gets the display name for @contact. The display name is typically set by the
 * contact and is handled by the protocol plugin.
 *
 * Returns: (nullable): The display name of @contact if one is set, otherwise
 *          %NULL will be returned.
 *
 * Since: 3.0.0
 */
const gchar *purple_contact_get_display_name(PurpleContact *contact);

/**
 * purple_contact_set_display_name:
 * @contact: The instance.
 * @display_name: (nullable): The new displayname.
 *
 * Sets the display name of @contact to @display_name.
 *
 * This should primarily only be used by protocol plugins and everyone else
 * should be using [method@Purple.Contact.set_alias].
 *
 * Since: 3.0.0
 */
void purple_contact_set_display_name(PurpleContact *contact, const gchar *display_name);

/**
 * purple_contact_get_alias:
 * @contact: The instance.
 *
 * Gets the alias for @contact.
 *
 * Returns: (nullable): The alias of @contact if one is set, otherwise %NULL.
 *
 * Since: 3.0.0
 */
const gchar *purple_contact_get_alias(PurpleContact *contact);

/**
 * purple_contact_set_alias:
 * @contact: The instance.
 * @alias: (nullable): The new alias.
 *
 * Sets the alias of @contact to @alias.
 *
 * Protocol plugins may use this value to synchronize across instances.
 *
 * Since: 3.0.0
 */
void purple_contact_set_alias(PurpleContact *contact, const gchar *alias);

/**
 * purple_contact_get_avatar:
 * @contact: The instance.
 *
 * Gets the avatar for @contact if one is set.
 *
 * Returns: (transfer none): The avatar if set, otherwise %NULL.
 *
 * Since: 3.0.0
 */
GdkPixbuf *purple_contact_get_avatar(PurpleContact *contact);

/**
 * purple_contact_set_avatar:
 * @contact: The instance.
 * @avatar: (nullable): The new avatar to set.
 *
 * Sets the avatar for @contact to @avatar. If @avatar is %NULL an existing
 * avatar will be removed.
 *
 * Typically this should only called by the protocol plugin.
 *
 * Since: 3.0.0
 */
void purple_contact_set_avatar(PurpleContact *contact, GdkPixbuf *avatar);

/**
 * purple_contact_get_presence:
 * @contact: The instance.
 *
 * Gets the [class@Purple.Presence] for @contact.
 *
 * Returns: (transfer none) (nullable): The presence for @contact if one is
 *          set, otherwise %NULL.
 *
 * Since: 3.0.0
 */
PurplePresence *purple_contact_get_presence(PurpleContact *contact);

/**
 * purple_contact_set_presence:
 * @contact: The instance.
 * @presence: (nullable): The new [class@Purple.Presence] to set.
 *
 * Sets the presence for @contact to @presence. If @presence is %NULL, the
 * existing presence will be cleared.
 *
 * Typically this should only be called by the protocol plugin.
 *
 * Since: 3.0.0
 */
void purple_contact_set_presence(PurpleContact *contact, PurplePresence *presence);

/**
 * purple_contact_get_tags:
 * @contact: The instance.
 *
 * Gets the [class@Purple.Tags] instance for @contact.
 *
 * Returns: (transfer none): The tags for @contact.
 *
 * Since: 3.0.0
 */
PurpleTags *purple_contact_get_tags(PurpleContact *contact);

G_END_DECLS

#endif /* PURPLE_CONTACT_H */
