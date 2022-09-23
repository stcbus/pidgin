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

#include "purplecontact.h"

struct _PurpleContact {
	GObject parent;

	gchar *id;
	PurpleAccount *account;

	gchar *username;
	gchar *display_name;
	gchar *alias;

	GdkPixbuf *avatar;

	PurplePresence *presence;

	PurpleTags *tags;
};

enum {
	PROP_0,
	PROP_ID,
	PROP_ACCOUNT,
	PROP_USERNAME,
	PROP_DISPLAY_NAME,
	PROP_ALIAS,
	PROP_AVATAR,
	PROP_PRESENCE,
	PROP_TAGS,
	N_PROPERTIES
};
static GParamSpec *properties[N_PROPERTIES] = {NULL, };

G_DEFINE_TYPE(PurpleContact, purple_contact, G_TYPE_OBJECT)

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
purple_contact_set_account(PurpleContact *contact, PurpleAccount *account) {
	g_return_if_fail(PURPLE_IS_CONTACT(contact));
	g_return_if_fail(PURPLE_IS_ACCOUNT(account));

	if(g_set_object(&contact->account, account)) {
		g_object_notify_by_pspec(G_OBJECT(contact), properties[PROP_ACCOUNT]);
	}
}

static void
purple_contact_set_id(PurpleContact *contact, const gchar *id) {
	g_return_if_fail(PURPLE_IS_CONTACT(contact));

	g_free(contact->id);

	if(id != NULL) {
		contact->id = g_strdup(id);
	} else {
		contact->id = g_uuid_string_random();
	}

	g_object_notify_by_pspec(G_OBJECT(contact), properties[PROP_ID]);
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
static void
purple_contact_get_property(GObject *obj, guint param_id, GValue *value,
                            GParamSpec *pspec)
{
	PurpleContact *contact = PURPLE_CONTACT(obj);

	switch(param_id) {
		case PROP_ID:
			g_value_set_string(value, purple_contact_get_id(contact));
			break;
		case PROP_ACCOUNT:
			g_value_set_object(value, purple_contact_get_account(contact));
			break;
		case PROP_USERNAME:
			g_value_set_string(value, purple_contact_get_username(contact));
			break;
		case PROP_DISPLAY_NAME:
			g_value_set_string(value, purple_contact_get_display_name(contact));
			break;
		case PROP_ALIAS:
			g_value_set_string(value, purple_contact_get_alias(contact));
			break;
		case PROP_AVATAR:
			g_value_set_object(value, purple_contact_get_avatar(contact));
			break;
		case PROP_PRESENCE:
			g_value_set_object(value, purple_contact_get_presence(contact));
			break;
		case PROP_TAGS:
			g_value_set_object(value, purple_contact_get_tags(contact));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_contact_set_property(GObject *obj, guint param_id, const GValue *value,
                            GParamSpec *pspec)
{
	PurpleContact *contact = PURPLE_CONTACT(obj);

	switch(param_id) {
		case PROP_ID:
			purple_contact_set_id(contact, g_value_get_string(value));
			break;
		case PROP_ACCOUNT:
			purple_contact_set_account(contact, g_value_get_object(value));
			break;
		case PROP_USERNAME:
			purple_contact_set_username(contact, g_value_get_string(value));
			break;
		case PROP_DISPLAY_NAME:
			purple_contact_set_display_name(contact, g_value_get_string(value));
			break;
		case PROP_ALIAS:
			purple_contact_set_alias(contact, g_value_get_string(value));
			break;
		case PROP_AVATAR:
			purple_contact_set_avatar(contact, g_value_get_object(value));
			break;
		case PROP_PRESENCE:
			purple_contact_set_presence(contact, g_value_get_object(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_contact_dispose(GObject *obj) {
	PurpleContact *contact = PURPLE_CONTACT(obj);

	g_clear_object(&contact->account);
	g_clear_object(&contact->avatar);
	g_clear_object(&contact->presence);
	g_clear_object(&contact->tags);

	G_OBJECT_CLASS(purple_contact_parent_class)->dispose(obj);
}

static void
purple_contact_finalize(GObject *obj) {
	PurpleContact *contact = PURPLE_CONTACT(obj);

	g_clear_pointer(&contact->id, g_free);
	g_clear_pointer(&contact->username, g_free);
	g_clear_pointer(&contact->display_name, g_free);
	g_clear_pointer(&contact->alias, g_free);

	G_OBJECT_CLASS(purple_contact_parent_class)->finalize(obj);
}

static void
purple_contact_constructed(GObject *obj) {
	PurpleContact *contact = NULL;

	G_OBJECT_CLASS(purple_contact_parent_class)->constructed(obj);

	contact = PURPLE_CONTACT(obj);
	if(contact->id == NULL) {
		purple_contact_set_id(contact, NULL);
	}
}

static void
purple_contact_init(PurpleContact *contact) {
	contact->tags = purple_tags_new();
}

static void
purple_contact_class_init(PurpleContactClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->get_property = purple_contact_get_property;
	obj_class->set_property = purple_contact_set_property;
	obj_class->constructed = purple_contact_constructed;
	obj_class->dispose = purple_contact_dispose;
	obj_class->finalize = purple_contact_finalize;

	/**
	 * PurpleContact:id:
	 *
	 * The protocol specific id for the contact.
	 *
	 * Since: 3.0.0
	 */
	properties[PROP_ID] = g_param_spec_string(
		"id", "id",
		"The id of the contact",
		NULL,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

	/**
	 * PurpleContact:account:
	 *
	 * The account that this contact belongs to.
	 *
	 * Since: 3.0.0
	 */
	properties[PROP_ACCOUNT] = g_param_spec_object(
		"account", "account",
		"The account this contact belongs to",
		PURPLE_TYPE_ACCOUNT,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

	/**
	 * PurpleContact:username:
	 *
	 * The username for this contact. In rare cases this can change, like when
	 * a user changes their "nick" on IRC which is their user name.
	 *
	 * Since: 3.0.0
	 */
	properties[PROP_USERNAME] = g_param_spec_string(
		"username", "username",
		"The username of the contact",
		NULL,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	/**
	 * PurpleContact:display-name:
	 *
	 * The display name for this contact. This is generally set by the person
	 * the contact is representing and controlled via the protocol plugin.
	 *
	 * Since: 3.0.0
	 */
	properties[PROP_DISPLAY_NAME] = g_param_spec_string(
		"display-name", "display-name",
		"The display name of the contact",
		NULL,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	/**
	 * PurpleContact:alias:
	 *
	 * The alias for this contact. This is controlled by the libpurple user and
	 * may be used by the protocol if it allows for aliasing.
	 *
	 * Since: 3.0.0
	 */
	properties[PROP_ALIAS] = g_param_spec_string(
		"alias", "alias",
		"The alias of the contact.",
		NULL,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	/**
	 * PurpleContact:avatar:
	 *
	 * The avatar for this contact. This is typically controlled by the protocol
	 * and should only be read by others.
	 *
	 * Since: 3.0.0
	 */
	properties[PROP_AVATAR] = g_param_spec_object(
		"avatar", "avatar",
		"The avatar of the contact",
		GDK_TYPE_PIXBUF,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	/**
	 * PurpleContact:presence:
	 *
	 * The [class@Purple.Presence] for this contact. This is typically
	 * controlled by the protocol and should only be read by others.
	 *
	 * Since: 3.0.0
	 */
	properties[PROP_PRESENCE] = g_param_spec_object(
		"presence", "presence",
		"The presence of the contact",
		PURPLE_TYPE_PRESENCE,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	/**
	 * PurpleContact:tags:
	 *
	 * The [class@Purple.Tags] for this contact.
	 *
	 * Since: 3.0.0
	 */
	properties[PROP_TAGS] = g_param_spec_object(
		"tags", "tags",
		"The tags for the contact",
		PURPLE_TYPE_TAGS,
		G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, N_PROPERTIES, properties);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
PurpleContact *
purple_contact_new(PurpleAccount *account, const gchar *username) {
	g_return_val_if_fail(PURPLE_IS_ACCOUNT(account), NULL);
	g_return_val_if_fail(username != NULL, NULL);

	return g_object_new(PURPLE_TYPE_CONTACT,
		"account", account,
		"username", username,
		NULL);
}

PurpleAccount *
purple_contact_get_account(PurpleContact *contact) {
	g_return_val_if_fail(PURPLE_IS_CONTACT(contact), NULL);

	return contact->account;
}

const gchar *
purple_contact_get_id(PurpleContact *contact) {
	g_return_val_if_fail(PURPLE_IS_CONTACT(contact), NULL);

	return contact->id;
}

const gchar *
purple_contact_get_username(PurpleContact *contact) {
	g_return_val_if_fail(PURPLE_IS_CONTACT(contact), NULL);

	return contact->username;
}

void
purple_contact_set_username(PurpleContact *contact, const gchar *username) {
	g_return_if_fail(PURPLE_IS_CONTACT(contact));
	g_return_if_fail(username != NULL);

	g_free(contact->username);
	contact->username = g_strdup(username);

	g_object_notify_by_pspec(G_OBJECT(contact), properties[PROP_USERNAME]);
}

const gchar *
purple_contact_get_display_name(PurpleContact *contact) {
	g_return_val_if_fail(PURPLE_IS_CONTACT(contact), NULL);

	return contact->display_name;
}

void
purple_contact_set_display_name(PurpleContact *contact,
                                const gchar *display_name)
{
	g_return_if_fail(PURPLE_IS_CONTACT(contact));

	g_free(contact->display_name);
	contact->display_name = g_strdup(display_name);

	g_object_notify_by_pspec(G_OBJECT(contact), properties[PROP_DISPLAY_NAME]);
}

const gchar *
purple_contact_get_alias(PurpleContact *contact) {
	g_return_val_if_fail(PURPLE_IS_CONTACT(contact), NULL);

	return contact->alias;
}

void
purple_contact_set_alias(PurpleContact *contact, const gchar *alias) {
	g_return_if_fail(PURPLE_IS_CONTACT(contact));

	g_free(contact->alias);
	contact->alias = g_strdup(alias);

	g_object_notify_by_pspec(G_OBJECT(contact), properties[PROP_ALIAS]);
}

GdkPixbuf *
purple_contact_get_avatar(PurpleContact *contact) {
	g_return_val_if_fail(PURPLE_IS_CONTACT(contact), NULL);

	return contact->avatar;
}

void
purple_contact_set_avatar(PurpleContact *contact, GdkPixbuf *avatar) {
	g_return_if_fail(PURPLE_IS_CONTACT(contact));

	if(g_set_object(&contact->avatar, avatar)) {
		g_object_notify_by_pspec(G_OBJECT(contact), properties[PROP_AVATAR]);
	}
}

PurplePresence *
purple_contact_get_presence(PurpleContact *contact) {
	g_return_val_if_fail(PURPLE_IS_CONTACT(contact), NULL);

	return contact->presence;
}

void
purple_contact_set_presence(PurpleContact *contact, PurplePresence *presence) {
	g_return_if_fail(PURPLE_IS_CONTACT(contact));

	if(g_set_object(&contact->presence, presence)) {
		g_object_notify_by_pspec(G_OBJECT(contact), properties[PROP_PRESENCE]);
	}
}

PurpleTags *
purple_contact_get_tags(PurpleContact *contact) {
	g_return_val_if_fail(PURPLE_IS_CONTACT(contact), NULL);

	return contact->tags;
}
