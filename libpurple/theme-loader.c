/*
 * ThemeLoaders for LibPurple
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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

#include "theme-loader.h"

#define PURPLE_THEME_LOADER_GET_PRIVATE(PurpleThemeLoader) \
	((PurpleThemeLoaderPrivate *) ((PurpleThemeLoader)->priv))


/******************************************************************************
 * Structs
 *****************************************************************************/
typedef struct {
	gchar *type;
} PurpleThemeLoaderPrivate;

/******************************************************************************
 * Globals
 *****************************************************************************/

/******************************************************************************
 * Enums
 *****************************************************************************/
#define PROP_TYPE_S "type"

enum {
	PROP_ZERO = 0,
	PROP_TYPE,
};

/******************************************************************************
 * GObject Stuff                                                              *
 *****************************************************************************/

static void
purple_theme_loader_get_property(GObject *obj, guint param_id, GValue *value,
						 GParamSpec *psec)
{
	PurpleThemeLoader *theme_loader = PURPLE_THEME_LOADER(obj);

	switch(param_id) {
		case PROP_TYPE:
			g_value_set_string(value, purple_theme_loader_get_type_string(theme_loader));
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, param_id, psec);
			break;
	}
}

static void
purple_theme_loader_class_init (PurpleThemeLoaderClass *klass)
{
	GObjectClass *obj_class = G_OBJECT_CLASS(klass);
	GParamSpec *pspec;

	/* 2.4
	 * g_type_class_add_private(klass, sizeof(PurpleThemePrivate)); */
	

	obj_class->get_property = purple_theme_loader_get_property;
	
	/* TYPE STRING (read only) */
	pspec = g_param_spec_string(PROP_TYPE_S, "Type",
				    "The string represtenting the type of the theme",
				    NULL,
				    G_PARAM_READABLE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property(obj_class, PROP_TYPE, pspec);
}


GType 
purple_theme_loader_get_type (void)
{
  static GType type = 0;
  if (type == 0) {
    static const GTypeInfo info = {
      sizeof (PurpleThemeLoaderClass),
      NULL,   /* base_init */
      NULL,   /* base_finalize */
      (GClassInitFunc)purple_theme_loader_class_init,   /* class_init */
      NULL,   /* class_finalize */
      NULL,   /* class_data */
      sizeof (PurpleThemeLoader),
      0,      /* n_preallocs */
      NULL,    /* instance_init */
      NULL,   /* value table */
    };
    type = g_type_register_static (G_TYPE_OBJECT,
                                   "PurpleThemeLoaderType",
                                   &info, G_TYPE_FLAG_ABSTRACT);
  }
  return type;
}


/*****************************************************************************
 * Public API functions                                                      *
 *****************************************************************************/


gchar *
purple_theme_loader_get_type_string (PurpleThemeLoader *theme_loader)
{
	PurpleThemeLoaderPrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_THEME_LOADER(theme_loader), NULL);

	priv = PURPLE_THEME_LOADER_GET_PRIVATE(theme_loader);
	return priv->type;
}

PurpleTheme *
purple_theme_loader_build (PurpleThemeLoader *loader, const gchar *dir)
{
	return PURPLE_THEME_LOADER_GET_CLASS(loader)->_purple_theme_loader_build(dir);
}
