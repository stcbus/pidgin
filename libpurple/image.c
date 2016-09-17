/* purple
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
 */

#include "internal.h"
#include "glibcompat.h"

#include "debug.h"
#include "image.h"

#define PURPLE_IMAGE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_IMAGE, PurpleImagePrivate))

typedef struct {
	gchar *path;

	GBytes *contents;

	const gchar *extension;
	const gchar *mime;
	gchar *gen_filename;
	gchar *friendly_filename;
} PurpleImagePrivate;

enum {
	PROP_0,
	PROP_LAST
};

static GObjectClass *parent_class;
static GParamSpec *properties[PROP_LAST];

/******************************************************************************
 * Object stuff
 ******************************************************************************/
static void
purple_image_init(PurpleImage *image) {
}

static void
purple_image_finalize(GObject *obj) {
	PurpleImagePrivate *priv = PURPLE_IMAGE_GET_PRIVATE(obj);

	g_bytes_unref(priv->contents);

	g_free(priv->path);
	g_free(priv->gen_filename);
	g_free(priv->friendly_filename);

	G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void
purple_image_set_property(GObject *object, guint par_id,
                          const GValue *value, GParamSpec *pspec)
{
	switch (par_id) {
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, par_id, pspec);
			break;
	}
}

static void
purple_image_get_property(GObject *object, guint par_id, GValue *value,
                          GParamSpec *pspec)
{
	switch (par_id) {
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, par_id, pspec);
			break;
	}
}

static void
purple_image_class_init(PurpleImageClass *klass) {
	GObjectClass *gobj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	gobj_class->finalize = purple_image_finalize;
	gobj_class->get_property = purple_image_get_property;
	gobj_class->set_property = purple_image_set_property;

	g_object_class_install_properties(gobj_class, PROP_LAST, properties);
}

G_DEFINE_TYPE_WITH_PRIVATE(PurpleImage, purple_image, G_TYPE_OBJECT);

/******************************************************************************
 * API
 ******************************************************************************/
PurpleImage *
purple_image_new_from_bytes(GBytes *bytes, GError **error) {
	return g_object_new(
		PURPLE_TYPE_IMAGE,
		NULL
	);
}

PurpleImage *
purple_image_new_from_file(const gchar *path) {
	PurpleImage *image = NULL;
	GBytes *bytes = NULL;
	gchar *contents = NULL;
	gsize length = 0;

	if(!g_file_test(path, G_FILE_TEST_EXISTS)) {
		return NULL;
	}

	if(!g_file_get_contents(path, &contents, &length, NULL)) {
		return NULL;
	}

	bytes = g_bytes_new_take(contents, length);

	image = purple_image_new_from_bytes(bytes, NULL);

	g_bytes_unref(bytes);

	return image;
}

PurpleImage *
purple_image_new_from_data(const guint8 *data, gsize length) {
	PurpleImage *image;
	GBytes *bytes = NULL;

	bytes = g_bytes_new(data, length);

	image = purple_image_new_from_bytes(bytes, NULL);

	g_bytes_unref(bytes);

	return image;
}

PurpleImage *
purple_image_new_from_data_take(guint8 *data, gsize length) {
	PurpleImage *image;
	GBytes *bytes = NULL;

	bytes = g_bytes_new(data, length);

	image = purple_image_new_from_bytes(bytes, NULL);

	g_bytes_unref(bytes);

	return image;
}

gboolean
purple_image_save(PurpleImage *image, const gchar *path) {
	PurpleImagePrivate *priv = PURPLE_IMAGE_GET_PRIVATE(image);
	gconstpointer data;
	gsize len;
	gboolean succ;

	g_return_val_if_fail(priv != NULL, FALSE);
	g_return_val_if_fail(path != NULL, FALSE);
	g_return_val_if_fail(path[0] != '\0', FALSE);

	data = purple_image_get_data(image);
	len = purple_image_get_size(image);

	g_return_val_if_fail(data != NULL, FALSE);
	g_return_val_if_fail(len > 0, FALSE);

	succ = purple_util_write_data_to_file_absolute(path, data, len);
	if (succ && priv->path == NULL)
		priv->path = g_strdup(path);

	return succ;
}

const gchar *
purple_image_get_path(PurpleImage *image) {
	PurpleImagePrivate *priv = PURPLE_IMAGE_GET_PRIVATE(image);

	g_return_val_if_fail(priv != NULL, NULL);

	return priv->path;
}

gsize
purple_image_get_size(PurpleImage *image) {
	PurpleImagePrivate *priv;

	g_return_val_if_fail(PURPLE_IS_IMAGE(image), 0);

	priv = PURPLE_IMAGE_GET_PRIVATE(image);

	return g_bytes_get_size(priv->contents);
}

gconstpointer
purple_image_get_data(PurpleImage *image) {
	PurpleImagePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_IMAGE(image), NULL);

	priv = PURPLE_IMAGE_GET_PRIVATE(image);

	return g_bytes_get_data(priv->contents, NULL);
}

const gchar *
purple_image_get_extension(PurpleImage *image) {
	PurpleImagePrivate *priv = PURPLE_IMAGE_GET_PRIVATE(image);
	gconstpointer data;

	g_return_val_if_fail(priv != NULL, NULL);

	if (priv->extension)
		return priv->extension;

	if (purple_image_get_size(image) < 4)
		return NULL;

	data = purple_image_get_data(image);
	g_assert(data != NULL);

	if (memcmp(data, "GIF8", 4) == 0)
		return priv->extension = "gif";
	if (memcmp(data, "\xff\xd8\xff", 3) == 0) /* 4th may be e0 through ef */
		return priv->extension = "jpg";
	if (memcmp(data, "\x89PNG", 4) == 0)
		return priv->extension = "png";
	if (memcmp(data, "MM", 2) == 0)
		return priv->extension = "tif";
	if (memcmp(data, "II", 2) == 0)
		return priv->extension = "tif";
	if (memcmp(data, "BM", 2) == 0)
		return priv->extension = "bmp";
	if (memcmp(data, "\x00\x00\x01\x00", 4) == 0)
		return priv->extension = "ico";

	return NULL;
}

const gchar *
purple_image_get_mimetype(PurpleImage *image) {
	PurpleImagePrivate *priv = PURPLE_IMAGE_GET_PRIVATE(image);
	const gchar *ext = purple_image_get_extension(image);

	g_return_val_if_fail(priv != NULL, NULL);

	if (priv->mime)
		return priv->mime;

	g_return_val_if_fail(ext != NULL, NULL);

	if (g_strcmp0(ext, "gif") == 0)
		return priv->mime = "image/gif";
	if (g_strcmp0(ext, "jpg") == 0)
		return priv->mime = "image/jpeg";
	if (g_strcmp0(ext, "png") == 0)
		return priv->mime = "image/png";
	if (g_strcmp0(ext, "tif") == 0)
		return priv->mime = "image/tiff";
	if (g_strcmp0(ext, "bmp") == 0)
		return priv->mime = "image/bmp";
	if (g_strcmp0(ext, "ico") == 0)
		return priv->mime = "image/vnd.microsoft.icon";

	return NULL;
}

const gchar *
purple_image_generate_filename(PurpleImage *image) {
	PurpleImagePrivate *priv = PURPLE_IMAGE_GET_PRIVATE(image);
	gconstpointer data;
	gsize len;
	const gchar *ext;
	gchar *checksum;

	g_return_val_if_fail(priv != NULL, NULL);

	if (priv->gen_filename)
		return priv->gen_filename;

	ext = purple_image_get_extension(image);
	data = purple_image_get_data(image);
	len = purple_image_get_size(image);

	g_return_val_if_fail(ext != NULL, NULL);
	g_return_val_if_fail(data != NULL, NULL);
	g_return_val_if_fail(len > 0, NULL);

	checksum = g_compute_checksum_for_data(G_CHECKSUM_SHA1, data, len);
	priv->gen_filename = g_strdup_printf("%s.%s", checksum, ext);
	g_free(checksum);

	return priv->gen_filename;
}

void
purple_image_set_friendly_filename(PurpleImage *image, const gchar *filename) {
	PurpleImagePrivate *priv = PURPLE_IMAGE_GET_PRIVATE(image);
	gchar *newname;
	const gchar *escaped;

	g_return_if_fail(priv != NULL);

	newname = g_path_get_basename(filename);
	escaped = purple_escape_filename(newname);
	g_free(newname);
	newname = NULL;

	if (g_strcmp0(escaped, "") == 0 || g_strcmp0(escaped, ".") == 0 ||
		g_strcmp0(escaped, G_DIR_SEPARATOR_S) == 0 ||
		g_strcmp0(escaped, "/") == 0 || g_strcmp0(escaped, "\\") == 0)
	{
		escaped = NULL;
	}

	g_free(priv->friendly_filename);
	priv->friendly_filename = g_strdup(escaped);
}

const gchar *
purple_image_get_friendly_filename(PurpleImage *image) {
	PurpleImagePrivate *priv = NULL;

	g_return_val_if_fail(PURPLE_IS_IMAGE(image), NULL);

	priv = PURPLE_IMAGE_GET_PRIVATE(image);

	if(priv->friendly_filename) {
		return priv->friendly_filename;
	}

	return purple_image_get_friendly_filename(image);
}
 