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

#include "backend-iface.h"

enum {
	S_ERROR,
	CANDIDATES_PREPARED,
	CODECS_CHANGED,
	NEW_CANDIDATE,
	ACTIVE_CANDIDATE_PAIR,
	LAST_SIGNAL
};

static guint purple_media_backend_signals[LAST_SIGNAL] = {0};

static void
purple_media_backend_base_init(gpointer iface)
{
	static gboolean is_initialized = FALSE;

	if (is_initialized) {
		return;
	}

	g_object_interface_install_property(iface,
		g_param_spec_string("conference-type",
			"Conference Type",
			"The type of conference that this backend "
			"has been created to provide.",
			NULL,
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
	g_object_interface_install_property(iface,
		g_param_spec_object(
			"media", "Purple Media",
			"The media object that this backend is bound to.",
			PURPLE_TYPE_MEDIA,
			G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
	purple_media_backend_signals[S_ERROR] =
			g_signal_new("error", G_TYPE_FROM_CLASS(iface),
			G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
			G_TYPE_NONE, 1, G_TYPE_STRING);
	purple_media_backend_signals[CANDIDATES_PREPARED] =
			g_signal_new("candidates-prepared",
			G_TYPE_FROM_CLASS(iface),
			G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
			G_TYPE_NONE, 2, G_TYPE_STRING,
			G_TYPE_STRING);
	purple_media_backend_signals[CODECS_CHANGED] =
			g_signal_new("codecs-changed",
			G_TYPE_FROM_CLASS(iface),
			G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
			G_TYPE_NONE, 1, G_TYPE_STRING);
	purple_media_backend_signals[NEW_CANDIDATE] =
			g_signal_new("new-candidate",
			G_TYPE_FROM_CLASS(iface),
			G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
			G_TYPE_NONE, 3, G_TYPE_POINTER,
			G_TYPE_POINTER, PURPLE_MEDIA_TYPE_CANDIDATE);
	purple_media_backend_signals[ACTIVE_CANDIDATE_PAIR] =
			g_signal_new("active-candidate-pair",
			G_TYPE_FROM_CLASS(iface),
			G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL,
			G_TYPE_NONE, 4, G_TYPE_STRING, G_TYPE_STRING,
			PURPLE_MEDIA_TYPE_CANDIDATE,
			PURPLE_MEDIA_TYPE_CANDIDATE);

	is_initialized = TRUE;
}

GType
purple_media_backend_get_type(void)
{
	static GType iface_type = 0;
	if (iface_type == 0) {
		static const GTypeInfo info = {
			sizeof(PurpleMediaBackendInterface),
			purple_media_backend_base_init,
			NULL,
			NULL,
			NULL,
			NULL,
			0,
			0,
			NULL,
			NULL
		};

		iface_type = g_type_register_static (G_TYPE_INTERFACE,
				"PurpleMediaBackend", &info, 0);
	}

	return iface_type;
}

gboolean
purple_media_backend_add_stream(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *who, PurpleMediaSessionType type,
		gboolean initiator, const gchar *transmitter, GHashTable *params)
{
	g_return_val_if_fail(PURPLE_MEDIA_IS_BACKEND(self), FALSE);
	return PURPLE_MEDIA_BACKEND_GET_INTERFACE(self)->add_stream(self,
			sess_id, who, type, initiator, transmitter, params);
}

void
purple_media_backend_add_remote_candidates(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant,
		GList *remote_candidates)
{
	g_return_if_fail(PURPLE_MEDIA_IS_BACKEND(self));
	PURPLE_MEDIA_BACKEND_GET_INTERFACE(self)->add_remote_candidates(self,
			sess_id, participant, remote_candidates);
}

gboolean
purple_media_backend_codecs_ready(PurpleMediaBackend *self,
		const gchar *sess_id)
{
	g_return_val_if_fail(PURPLE_MEDIA_IS_BACKEND(self), FALSE);
	return PURPLE_MEDIA_BACKEND_GET_INTERFACE(self)->codecs_ready(self,
			sess_id);
}

GList *
purple_media_backend_get_codecs(PurpleMediaBackend *self,
		const gchar *sess_id)
{
	g_return_val_if_fail(PURPLE_MEDIA_IS_BACKEND(self), NULL);
	return PURPLE_MEDIA_BACKEND_GET_INTERFACE(self)->get_codecs(self,
			sess_id);
}

GList *
purple_media_backend_get_local_candidates(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant)
{
	g_return_val_if_fail(PURPLE_MEDIA_IS_BACKEND(self), NULL);
	return PURPLE_MEDIA_BACKEND_GET_INTERFACE(self)->
			get_local_candidates(self,
			sess_id, participant);
}

gboolean
purple_media_backend_set_remote_codecs(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant,
		GList *codecs)
{
	g_return_val_if_fail(PURPLE_MEDIA_IS_BACKEND(self), FALSE);
	return PURPLE_MEDIA_BACKEND_GET_INTERFACE(self)->set_remote_codecs(
			self, sess_id, participant, codecs);
}

gboolean
purple_media_backend_set_send_codec(PurpleMediaBackend *self,
		const gchar *sess_id, PurpleMediaCodec *codec)
{
	g_return_val_if_fail(PURPLE_MEDIA_IS_BACKEND(self), FALSE);
	return PURPLE_MEDIA_BACKEND_GET_INTERFACE(self)->set_send_codec(self,
			sess_id, codec);
}

gboolean
purple_media_backend_set_encryption_parameters(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *cipher,
		const gchar *auth, const gchar *key, gsize key_len)
{
	PurpleMediaBackendInterface *backend_iface;

	g_return_val_if_fail(PURPLE_MEDIA_IS_BACKEND(self), FALSE);
	backend_iface = PURPLE_MEDIA_BACKEND_GET_INTERFACE(self);
	g_return_val_if_fail(backend_iface->set_encryption_parameters, FALSE);
	return backend_iface->set_encryption_parameters(self,
			sess_id, cipher, auth, key, key_len);
}

gboolean
purple_media_backend_set_decryption_parameters(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant,
		const gchar *cipher, const gchar *auth,
		const gchar *key, gsize key_len)
{
	PurpleMediaBackendInterface *backend_iface;

	g_return_val_if_fail(PURPLE_MEDIA_IS_BACKEND(self), FALSE);
	backend_iface = PURPLE_MEDIA_BACKEND_GET_INTERFACE(self);
	g_return_val_if_fail(backend_iface->set_decryption_parameters, FALSE);
	return backend_iface->set_decryption_parameters(self,
			sess_id, participant, cipher, auth, key, key_len);
}

gboolean
purple_media_backend_set_require_encryption(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant,
		gboolean require_encryption)
{
	PurpleMediaBackendInterface *backend_iface;

	g_return_val_if_fail(PURPLE_MEDIA_IS_BACKEND(self), FALSE);
	backend_iface = PURPLE_MEDIA_BACKEND_GET_INTERFACE(self);

	if (!backend_iface->set_require_encryption) {
		return FALSE;
	}

	return backend_iface->set_require_encryption(self,
			sess_id, participant, require_encryption);
}

void
purple_media_backend_set_params(PurpleMediaBackend *self, GHashTable *params)
{
	g_return_if_fail(PURPLE_MEDIA_IS_BACKEND(self));
	PURPLE_MEDIA_BACKEND_GET_INTERFACE(self)->set_params(self, params);
}

const gchar **
purple_media_backend_get_available_params(PurpleMediaBackend *self)
{
	static const gchar *NULL_ARRAY[] = { NULL };

	g_return_val_if_fail(PURPLE_MEDIA_IS_BACKEND(self), NULL_ARRAY);
	return PURPLE_MEDIA_BACKEND_GET_INTERFACE(self)->get_available_params();
}

gboolean
purple_media_backend_set_send_rtcp_mux(PurpleMediaBackend *self,
		const gchar *sess_id, const gchar *participant, gboolean send_rtcp_mux)
{
	PurpleMediaBackendInterface *backend_iface;

	g_return_val_if_fail(PURPLE_MEDIA_IS_BACKEND(self), FALSE);
	backend_iface = PURPLE_MEDIA_BACKEND_GET_INTERFACE(self);
	g_return_val_if_fail(backend_iface->set_send_rtcp_mux, FALSE);
	return backend_iface->set_send_rtcp_mux(self,
			sess_id, participant, send_rtcp_mux);
}
