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

#include "purpleperson.h"

struct _PurplePerson {
	GObject parent;

	gchar *id;

	gchar *alias;
	GdkPixbuf *avatar;
	PurpleTags *tags;

	GPtrArray *contacts;
};

enum {
	PROP_0,
	PROP_ID,
	PROP_ALIAS,
	PROP_AVATAR,
	PROP_TAGS,
	PROP_PRIORITY_CONTACT,
	N_PROPERTIES
};
static GParamSpec *properties[N_PROPERTIES] = {NULL, };

/******************************************************************************
 * Helpers
 *****************************************************************************/
static void
purple_person_set_id(PurplePerson *person, const gchar *id) {
	g_return_if_fail(PURPLE_IS_PERSON(person));

	g_free(person->id);

	if(id != NULL) {
		person->id = g_strdup(id);
	} else {
		person->id = g_uuid_string_random();
	}

	g_object_notify_by_pspec(G_OBJECT(person), properties[PROP_ID]);
}

static gint
purple_person_contact_compare(gconstpointer a, gconstpointer b) {
	PurpleContact *c1 = *(PurpleContact **)a;
	PurpleContact *c2 = *(PurpleContact **)b;
	PurplePresence *p1 = NULL;
	PurplePresence *p2 = NULL;

	p1 = purple_contact_get_presence(c1);
	p2 = purple_contact_get_presence(c2);

	return purple_presence_compare(p1, p2);
}

static void
purple_person_sort_contacts(PurplePerson *person) {
	PurpleContact *original_priority = NULL;
	PurpleContact *new_priority = NULL;
	guint n_items = person->contacts->len;

	if(n_items <= 1) {
		g_object_notify_by_pspec(G_OBJECT(person),
		                         properties[PROP_PRIORITY_CONTACT]);

		g_list_model_items_changed(G_LIST_MODEL(person), 0, n_items, n_items);

		return;
	}

	original_priority = purple_person_get_priority_contact(person);

	g_ptr_array_sort(person->contacts, purple_person_contact_compare);

	/* Tell the list we update our stuff. */
	g_list_model_items_changed(G_LIST_MODEL(person), 0, n_items, n_items);

	/* See if the priority contact changed. */
	new_priority = g_ptr_array_index(person->contacts, 0);
	if(original_priority != new_priority) {
		g_object_notify_by_pspec(G_OBJECT(person),
		                         properties[PROP_PRIORITY_CONTACT]);
	}
}

/******************************************************************************
 * Callbacks
 *****************************************************************************/
static void
purple_person_presence_notify_cb(G_GNUC_UNUSED GObject *obj,
                                 G_GNUC_UNUSED GParamSpec *pspec,
                                 gpointer data)
{
	purple_person_sort_contacts(data);
}

static void
purple_person_contact_notify_cb(GObject *obj,
                                G_GNUC_UNUSED GParamSpec *pspec,
                                gpointer data)
{
	PurpleContact *contact = PURPLE_CONTACT(obj);

	g_signal_connect_object(contact, "notify",
	                        G_CALLBACK(purple_person_presence_notify_cb),
	                        data, 0);
}

/******************************************************************************
 * GListModel Implementation
 *****************************************************************************/
static GType
purple_person_get_item_type(G_GNUC_UNUSED GListModel *list) {
	return PURPLE_TYPE_CONTACT;
}

static guint
purple_person_get_n_items(GListModel *list) {
	PurplePerson *person = PURPLE_PERSON(list);

	return person->contacts->len;
}

static gpointer
purple_person_get_item(GListModel *list, guint position) {
	PurplePerson *person = PURPLE_PERSON(list);
	PurpleContact *contact = NULL;

	if(position < person->contacts->len) {
		contact = g_ptr_array_index(person->contacts, position);
		g_object_ref(contact);
	}

	return contact;
}

static void
purple_person_list_model_init(GListModelInterface *iface) {
	iface->get_item_type = purple_person_get_item_type;
	iface->get_n_items = purple_person_get_n_items;
	iface->get_item = purple_person_get_item;
}

/******************************************************************************
 * GObject Implementation
 *****************************************************************************/
G_DEFINE_TYPE_EXTENDED(PurplePerson, purple_person, G_TYPE_OBJECT, 0,
                       G_IMPLEMENT_INTERFACE(G_TYPE_LIST_MODEL,
                                             purple_person_list_model_init))

static void
purple_person_get_property(GObject *obj, guint param_id, GValue *value,
                           GParamSpec *pspec)
{
	PurplePerson *person = PURPLE_PERSON(obj);

	switch(param_id) {
		case PROP_ID:
			g_value_set_string(value, purple_person_get_id(person));
			break;
		case PROP_ALIAS:
			g_value_set_string(value, purple_person_get_alias(person));
			break;
		case PROP_AVATAR:
			g_value_set_object(value, purple_person_get_avatar(person));
			break;
		case PROP_TAGS:
			g_value_set_object(value, purple_person_get_tags(person));
			break;
		case PROP_PRIORITY_CONTACT:
			g_value_set_object(value,
			                   purple_person_get_priority_contact(person));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_person_set_property(GObject *obj, guint param_id, const GValue *value,
                            GParamSpec *pspec)
{
	PurplePerson *person = PURPLE_PERSON(obj);

	switch(param_id) {
		case PROP_ID:
			purple_person_set_id(person, g_value_get_string(value));
			break;
		case PROP_ALIAS:
			purple_person_set_alias(person, g_value_get_string(value));
			break;
		case PROP_AVATAR:
			purple_person_set_avatar(person, g_value_get_object(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

static void
purple_person_dispose(GObject *obj) {
	PurplePerson *person = PURPLE_PERSON(obj);

	g_clear_object(&person->avatar);
	g_clear_object(&person->tags);

	if(person->contacts != NULL) {
		g_ptr_array_free(person->contacts, TRUE);
		person->contacts = NULL;
	}

	G_OBJECT_CLASS(purple_person_parent_class)->dispose(obj);
}

static void
purple_person_finalize(GObject *obj) {
	PurplePerson *person = PURPLE_PERSON(obj);

	g_clear_pointer(&person->id, g_free);
	g_clear_pointer(&person->alias, g_free);

	G_OBJECT_CLASS(purple_person_parent_class)->finalize(obj);
}

static void
purple_person_constructed(GObject *obj) {
	PurplePerson *person = NULL;

	G_OBJECT_CLASS(purple_person_parent_class)->constructed(obj);

	person = PURPLE_PERSON(obj);
	if(person->id == NULL) {
		purple_person_set_id(person, NULL);
	}
}

static void
purple_person_init(PurplePerson *person) {
	person->tags = purple_tags_new();
	person->contacts = g_ptr_array_new_full(0, (GDestroyNotify)g_object_unref);
}

static void
purple_person_class_init(PurplePersonClass *klass) {
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->get_property = purple_person_get_property;
	obj_class->set_property = purple_person_set_property;
	obj_class->constructed = purple_person_constructed;
	obj_class->dispose = purple_person_dispose;
	obj_class->finalize = purple_person_finalize;

	/**
	 * PurplePerson:id:
	 *
	 * The protocol specific id for the contact.
	 *
	 * Since: 3.0.0
	 */
	properties[PROP_ID] = g_param_spec_string(
		"id", "id",
		"The id of this contact",
		NULL,
		G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

	/**
	 * PurplePerson:alias:
	 *
	 * The alias for this person. This is controlled by the libpurple user.
	 *
	 * Since: 3.0.0
	 */
	properties[PROP_ALIAS] = g_param_spec_string(
		"alias", "alias",
		"The alias of this person.",
		NULL,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	/**
	 * PurplePerson:avatar:
	 *
	 * The avatar for this person. This is controlled by the libpurple user,
	 * which they can use to set a custom avatar.
	 *
	 * Since: 3.0.0
	 */
	properties[PROP_AVATAR] = g_param_spec_object(
		"avatar", "avatar",
		"The avatar of this person",
		GDK_TYPE_PIXBUF,
		G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	/**
	 * PurplePerson:tags:
	 *
	 * The [class@Purple.Tags] for this person.
	 *
	 * Since: 3.0.0
	 */
	properties[PROP_TAGS] = g_param_spec_object(
		"tags", "tags",
		"The tags for this person",
		PURPLE_TYPE_TAGS,
		G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	/**
	 * PurplePerson:priority-contact:
	 *
	 * The [class@Purple.Contact] that currently has the highest priority.
	 *
	 * This is used by user interfaces to determine which [class@Purple.Contact]
	 * to use when messaging and so on.
	 *
	 * Since: 3.0.0
	 */
	properties[PROP_PRIORITY_CONTACT] = g_param_spec_object(
		"priority-contact", "priority-contact",
		"The priority contact for the person",
		PURPLE_TYPE_CONTACT,
		G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, N_PROPERTIES, properties);
}

/******************************************************************************
 * Public API
 *****************************************************************************/
PurplePerson *
purple_person_new(void) {
	return g_object_new(PURPLE_TYPE_PERSON, NULL);
}

const gchar *
purple_person_get_id(PurplePerson *person) {
	g_return_val_if_fail(PURPLE_IS_PERSON(person), NULL);

	return person->id;
}

const gchar *
purple_person_get_alias(PurplePerson *person) {
	g_return_val_if_fail(PURPLE_IS_PERSON(person), NULL);

	return person->alias;
}

void
purple_person_set_alias(PurplePerson *person, const gchar *alias) {
	g_return_if_fail(PURPLE_IS_PERSON(person));

	g_free(person->alias);
	person->alias = g_strdup(alias);

	g_object_notify_by_pspec(G_OBJECT(person), properties[PROP_ALIAS]);
}

GdkPixbuf *
purple_person_get_avatar(PurplePerson *person) {
	g_return_val_if_fail(PURPLE_IS_PERSON(person), NULL);

	return person->avatar;
}

void
purple_person_set_avatar(PurplePerson *person, GdkPixbuf *avatar) {
	g_return_if_fail(PURPLE_IS_PERSON(person));

	if(g_set_object(&person->avatar, avatar)) {
		g_object_notify_by_pspec(G_OBJECT(person), properties[PROP_AVATAR]);
	}
}

PurpleTags *
purple_person_get_tags(PurplePerson *person) {
	g_return_val_if_fail(PURPLE_IS_PERSON(person), NULL);

	return person->tags;
}

void
purple_person_add_contact(PurplePerson *person, PurpleContact *contact) {
	PurplePresence *presence = NULL;

	g_return_if_fail(PURPLE_IS_PERSON(person));
	g_return_if_fail(PURPLE_IS_CONTACT(contact));

	g_ptr_array_add(person->contacts, g_object_ref(contact));

	g_signal_connect_object(contact, "notify::presence",
	                        G_CALLBACK(purple_person_contact_notify_cb),
	                        person, 0);

	presence = purple_contact_get_presence(contact);
	if(PURPLE_IS_PRESENCE(presence)) {
		g_signal_connect_object(presence, "notify",
		                        G_CALLBACK(purple_person_presence_notify_cb),
		                        person, 0);
	}

	purple_person_sort_contacts(person);
}

gboolean
purple_person_remove_contact(PurplePerson *person, PurpleContact *contact) {
	gboolean removed = FALSE;

	g_return_val_if_fail(PURPLE_IS_PERSON(person), FALSE);
	g_return_val_if_fail(PURPLE_IS_CONTACT(contact), FALSE);

	/* Ref the contact to avoid a use-after free. */
	g_object_ref(contact);

	/* g_ptr_array_remove calls g_object_unref because we passed it in as a
	 * GDestroyNotify.
	 */
	removed = g_ptr_array_remove(person->contacts, contact);

	if(removed) {
		PurplePresence *presence = purple_contact_get_presence(contact);

		/* Disconnect our signal handlers. */
		g_signal_handlers_disconnect_by_func(contact,
		                                     purple_person_contact_notify_cb,
		                                     person);

		if(PURPLE_IS_PRESENCE(presence)) {
			g_signal_handlers_disconnect_by_func(presence,
			                                     purple_person_presence_notify_cb,
			                                     person);
		}

		purple_person_sort_contacts(person);
	}

	/* Remove our reference. */
	g_object_unref(contact);

	return removed;
}

PurpleContact *
purple_person_get_priority_contact(PurplePerson *person) {
	g_return_val_if_fail(PURPLE_IS_PERSON(person), NULL);

	if(person->contacts->len == 0) {
		return NULL;
	}

	return g_ptr_array_index(person->contacts, 0);
}
