/*
 * purple
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

#include "blistnode.h"
#include "buddy.h"

typedef struct _PurpleBlistNodePrivate  PurpleBlistNodePrivate;

/* Private data of a buddy list node */
struct _PurpleBlistNodePrivate {
	GHashTable *settings;  /* per-node settings                            */
	gboolean transient;    /* node should not be saved with the buddy list */
};

/* Blist node property enums */
enum
{
	BLNODE_PROP_0,
	BLNODE_PROP_TRANSIENT,
	BLNODE_PROP_LAST
};

static GParamSpec *bn_properties[BLNODE_PROP_LAST];

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(PurpleBlistNode, purple_blist_node,
		G_TYPE_OBJECT);

/**************************************************************************/
/* Buddy list node API                                                    */
/**************************************************************************/

static PurpleBlistNode *get_next_node(PurpleBlistNode *node, gboolean godeep)
{
	if (node == NULL)
		return NULL;

	if (godeep && node->child)
		return node->child;

	if (node->next)
		return node->next;

	return get_next_node(node->parent, FALSE);
}

PurpleBlistNode *purple_blist_node_next(PurpleBlistNode *node, gboolean offline)
{
	PurpleBlistNode *ret = node;

	if (offline)
		return get_next_node(ret, TRUE);
	do
	{
		ret = get_next_node(ret, TRUE);
	} while (ret && PURPLE_IS_BUDDY(ret) &&
			!purple_account_is_connected(purple_buddy_get_account((PurpleBuddy *)ret)));

	return ret;
}

PurpleBlistNode *purple_blist_node_get_parent(PurpleBlistNode *node)
{
	return node ? node->parent : NULL;
}

PurpleBlistNode *purple_blist_node_get_first_child(PurpleBlistNode *node)
{
	return node ? node->child : NULL;
}

PurpleBlistNode *purple_blist_node_get_sibling_next(PurpleBlistNode *node)
{
	return node? node->next : NULL;
}

PurpleBlistNode *purple_blist_node_get_sibling_prev(PurpleBlistNode *node)
{
	return node? node->prev : NULL;
}

void purple_blist_node_remove_setting(PurpleBlistNode *node, const char *key)
{
	PurpleBlistNodePrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_BLIST_NODE(node));
	g_return_if_fail(key != NULL);

	priv = purple_blist_node_get_instance_private(node);
	g_return_if_fail(priv->settings != NULL);

	g_hash_table_remove(priv->settings, key);

	purple_blist_save_node(purple_blist_get_default(), node);
}

void
purple_blist_node_set_transient(PurpleBlistNode *node, gboolean transient)
{
	PurpleBlistNodePrivate *priv = NULL;

	g_return_if_fail(PURPLE_IS_BLIST_NODE(node));

	priv = purple_blist_node_get_instance_private(node);
	priv->transient = transient;

	g_object_notify_by_pspec(G_OBJECT(node),
			bn_properties[BLNODE_PROP_TRANSIENT]);
}

gboolean
purple_blist_node_is_transient(PurpleBlistNode *node)
{
	PurpleBlistNodePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_BLIST_NODE(node), 0);

	priv = purple_blist_node_get_instance_private(node);
	return priv->transient;
}

GHashTable *
purple_blist_node_get_settings(PurpleBlistNode *node)
{
	PurpleBlistNodePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_BLIST_NODE(node), NULL);

	priv = purple_blist_node_get_instance_private(node);
	return priv->settings;
}

gboolean
purple_blist_node_has_setting(PurpleBlistNode* node, const char *key)
{
	PurpleBlistNodePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_BLIST_NODE(node), FALSE);
	g_return_val_if_fail(key != NULL, FALSE);

	priv = purple_blist_node_get_instance_private(node);
	g_return_val_if_fail(priv->settings != NULL, FALSE);

	/* Boxed type, so it won't ever be NULL, so no need for _extended */
	return (g_hash_table_lookup(priv->settings, key) != NULL);
}

void
purple_blist_node_set_bool(PurpleBlistNode* node, const char *key, gboolean data)
{
	PurpleBlistNodePrivate *priv = NULL;
	GValue *value;

	g_return_if_fail(PURPLE_IS_BLIST_NODE(node));
	g_return_if_fail(key != NULL);

	priv = purple_blist_node_get_instance_private(node);
	g_return_if_fail(priv->settings != NULL);

	value = purple_value_new(G_TYPE_BOOLEAN);
	g_value_set_boolean(value, data);

	g_hash_table_replace(priv->settings, g_strdup(key), value);

	purple_blist_save_node(purple_blist_get_default(), node);
}

gboolean
purple_blist_node_get_bool(PurpleBlistNode* node, const char *key)
{
	PurpleBlistNodePrivate *priv = NULL;
	GValue *value;

	g_return_val_if_fail(PURPLE_IS_BLIST_NODE(node), FALSE);
	g_return_val_if_fail(key != NULL, FALSE);

	priv = purple_blist_node_get_instance_private(node);
	g_return_val_if_fail(priv->settings != NULL, FALSE);

	value = g_hash_table_lookup(priv->settings, key);

	if (value == NULL)
		return FALSE;

	g_return_val_if_fail(G_VALUE_HOLDS_BOOLEAN(value), FALSE);

	return g_value_get_boolean(value);
}

void
purple_blist_node_set_int(PurpleBlistNode* node, const char *key, int data)
{
	PurpleBlistNodePrivate *priv = NULL;
	GValue *value;

	g_return_if_fail(PURPLE_IS_BLIST_NODE(node));
	g_return_if_fail(key != NULL);

	priv = purple_blist_node_get_instance_private(node);
	g_return_if_fail(priv->settings != NULL);

	value = purple_value_new(G_TYPE_INT);
	g_value_set_int(value, data);

	g_hash_table_replace(priv->settings, g_strdup(key), value);

	purple_blist_save_node(purple_blist_get_default(), node);
}

int
purple_blist_node_get_int(PurpleBlistNode* node, const char *key)
{
	PurpleBlistNodePrivate *priv = NULL;
	GValue *value;

	g_return_val_if_fail(PURPLE_IS_BLIST_NODE(node), 0);
	g_return_val_if_fail(key != NULL, 0);

	priv = purple_blist_node_get_instance_private(node);
	g_return_val_if_fail(priv->settings != NULL, 0);

	value = g_hash_table_lookup(priv->settings, key);

	if (value == NULL)
		return 0;

	g_return_val_if_fail(G_VALUE_HOLDS_INT(value), 0);

	return g_value_get_int(value);
}

void
purple_blist_node_set_string(PurpleBlistNode* node, const char *key, const char *data)
{
	PurpleBlistNodePrivate *priv = NULL;
	GValue *value;

	g_return_if_fail(PURPLE_IS_BLIST_NODE(node));
	g_return_if_fail(key != NULL);

	priv = purple_blist_node_get_instance_private(node);
	g_return_if_fail(priv->settings != NULL);

	value = purple_value_new(G_TYPE_STRING);
	g_value_set_string(value, data);

	g_hash_table_replace(priv->settings, g_strdup(key), value);

	purple_blist_save_node(purple_blist_get_default(), node);
}

const char *
purple_blist_node_get_string(PurpleBlistNode* node, const char *key)
{
	PurpleBlistNodePrivate *priv = NULL;
	GValue *value;

	g_return_val_if_fail(PURPLE_IS_BLIST_NODE(node), NULL);
	g_return_val_if_fail(key != NULL, NULL);

	priv = purple_blist_node_get_instance_private(node);
	g_return_val_if_fail(priv->settings != NULL, NULL);

	value = g_hash_table_lookup(priv->settings, key);

	if (value == NULL)
		return NULL;

	g_return_val_if_fail(G_VALUE_HOLDS_STRING(value), NULL);

	return g_value_get_string(value);
}

GList *
purple_blist_node_get_extended_menu(PurpleBlistNode *n)
{
	GList *menu = NULL;

	g_return_val_if_fail(n != NULL, NULL);

	purple_signal_emit(purple_blist_get_handle(), "blist-node-extended-menu",
			n, &menu);
	return menu;
}

/**************************************************************************
 * GObject code for PurpleBlistNode
 **************************************************************************/

/* Set method for GObject properties */
static void
purple_blist_node_set_property(GObject *obj, guint param_id, const GValue *value,
		GParamSpec *pspec)
{
	PurpleBlistNode *node = PURPLE_BLIST_NODE(obj);

	switch (param_id) {
		case BLNODE_PROP_TRANSIENT:
			purple_blist_node_set_transient(node, g_value_get_boolean(value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* Get method for GObject properties */
static void
purple_blist_node_get_property(GObject *obj, guint param_id, GValue *value,
		GParamSpec *pspec)
{
	PurpleBlistNode *node = PURPLE_BLIST_NODE(obj);

	switch (param_id) {
		case BLNODE_PROP_TRANSIENT:
			g_value_set_boolean(value, purple_blist_node_is_transient(node));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, pspec);
			break;
	}
}

/* GObject initialization function */
static void
purple_blist_node_init(PurpleBlistNode *node)
{
	PurpleBlistNodePrivate *priv =
			purple_blist_node_get_instance_private(node);

	priv->settings = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
			(GDestroyNotify)purple_value_free);
}

/* GObject finalize function */
static void
purple_blist_node_finalize(GObject *object)
{
	PurpleBlistNodePrivate *priv = purple_blist_node_get_instance_private(
			PURPLE_BLIST_NODE(object));

	g_hash_table_destroy(priv->settings);

	G_OBJECT_CLASS(purple_blist_node_parent_class)->finalize(object);
}

/* Class initializer function */
static void
purple_blist_node_class_init(PurpleBlistNodeClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);

	obj_class->finalize = purple_blist_node_finalize;

	/* Setup properties */
	obj_class->get_property = purple_blist_node_get_property;
	obj_class->set_property = purple_blist_node_set_property;

	bn_properties[BLNODE_PROP_TRANSIENT] = g_param_spec_boolean("transient",
				"Transient",
				"Whether node should not be saved with the buddy list.",
				FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	g_object_class_install_properties(obj_class, BLNODE_PROP_LAST,
				bn_properties);
}

