/*
 * gaim
 *
 * Gaim is the legal property of its developers, whose names are too numerous
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "internal.h"
#include "conversation.h"
#include "debug.h"
#include "notify.h"
#include "prpl.h"
#include "request.h"
#include "util.h"

/**************************************************************************/
/** @name Protocol Plugin API  */
/**************************************************************************/
void
gaim_prpl_got_account_idle(GaimAccount *account, gboolean idle,
						   time_t idle_time)
{
	g_return_if_fail(account != NULL);
	g_return_if_fail(gaim_account_is_connected(account));

	gaim_presence_set_idle(gaim_account_get_presence(account),
						   idle, idle_time);
}

void
gaim_prpl_got_account_login_time(GaimAccount *account, const char *name,
		time_t login_time)
{
	GaimPresence *presence;

	g_return_if_fail(account != NULL);
	g_return_if_fail(name != NULL && *name != '\0');
	g_return_if_fail(gaim_account_is_connected(account));

	if (login_time == 0)
		login_time = time(NULL);

	presence = gaim_account_get_presence(account);

	gaim_presence_set_login_time(presence, login_time);
}

static gboolean
set_value_from_arg(GaimStatus *status, const char *id, va_list *args)
{
	GaimValue *value;

	value = gaim_status_get_attr_value(status, id);

	if (value == NULL)
	{
		gaim_debug_error("prpl",
						 "Attempted to set an unknown attribute %s on "
						 "status %s\n",
						 id, gaim_status_get_id(status));
		return FALSE;
	}

	switch (gaim_value_get_type(value))
	{
		case GAIM_TYPE_CHAR:
			gaim_value_set_char(value, (char)va_arg(*args, int));
			break;

		case GAIM_TYPE_UCHAR:
			gaim_value_set_uchar(value,
								 (unsigned char)va_arg(*args, unsigned int));
			break;

		case GAIM_TYPE_BOOLEAN:
			gaim_value_set_boolean(value, va_arg(*args, gboolean));
			break;

		case GAIM_TYPE_SHORT:
			gaim_value_set_short(value, (short)va_arg(*args, int));
			break;

		case GAIM_TYPE_USHORT:
			gaim_value_set_ushort(value,
					(unsigned short)va_arg(*args, unsigned int));
			break;

		case GAIM_TYPE_INT:
			gaim_value_set_int(value, va_arg(*args, int));
			break;

		case GAIM_TYPE_UINT:
			gaim_value_set_uint(value, va_arg(*args, unsigned int));
			break;

		case GAIM_TYPE_LONG:
			gaim_value_set_long(value, va_arg(*args, long));
			break;

		case GAIM_TYPE_ULONG:
			gaim_value_set_ulong(value, va_arg(*args, unsigned long));
			break;

		case GAIM_TYPE_INT64:
			gaim_value_set_int64(value, va_arg(*args, gint64));
			break;

		case GAIM_TYPE_UINT64:
			gaim_value_set_uint64(value, va_arg(*args, guint64));
			break;

		case GAIM_TYPE_STRING:
			gaim_value_set_string(value, va_arg(*args, char *));
			break;

		case GAIM_TYPE_OBJECT:
			gaim_value_set_object(value, va_arg(*args, void *));
			break;

		case GAIM_TYPE_POINTER:
			gaim_value_set_pointer(value, va_arg(*args, void *));
			break;

		case GAIM_TYPE_ENUM:
			gaim_value_set_enum(value, va_arg(*args, int));
			break;

		case GAIM_TYPE_BOXED:
			gaim_value_set_boxed(value, va_arg(*args, void *));
			break;

		default:
			return FALSE;
	}

	return TRUE;
}

void
gaim_prpl_got_account_status(GaimAccount *account, const char *status_id,
		const char *attr_id, ...)
{
	GaimPresence *presence;
	GaimStatus *status;

	g_return_if_fail(account   != NULL);
	g_return_if_fail(status_id != NULL);
	g_return_if_fail(gaim_account_is_connected(account));

	presence = gaim_account_get_presence(account);
	status   = gaim_presence_get_status(presence, status_id);

	g_return_if_fail(status != NULL);

	if (attr_id != NULL)
	{
		va_list args;

		va_start(args, attr_id);

		while (attr_id != NULL)
		{
			set_value_from_arg(status, attr_id, &args);

			attr_id = va_arg(args, char *);
		}

		va_end(args);
	}

	gaim_presence_set_status_active(presence, status_id, TRUE);
}

void
gaim_prpl_got_account_warning_level(GaimAccount *account, const char *username,
		unsigned int level)
{
	GaimPresence *presence;
	unsigned int old_level;
	gchar *buf;

	g_return_if_fail(account != NULL);

	presence = gaim_account_get_presence(account);

	gaim_signal_emit(gaim_accounts_get_handle(), "account-warned",
					 account, username, level);

	old_level = gaim_presence_get_warning_level(presence);
	gaim_presence_set_warning_level(presence, level);

	if (old_level >= level)
		return;

	if (username == NULL)
		buf = g_strdup_printf(_("%s has just been warned by an anonymous "
								"person.\nYour new warning level is %d%%"),
							  gaim_account_get_username(account),
							  level);
	else
		buf = g_strdup_printf(_("%s has just been warned by %s.\n"
								"Your new warning level is %d%%"),
							  gaim_account_get_username(account),
							  username, level);

	gaim_notify_info(NULL, NULL, buf, NULL);

	g_free(buf);
}

void
gaim_prpl_got_user_idle(GaimAccount *account, const char *name,
		gboolean idle, time_t idle_time)
{
	GaimBuddy *buddy;
	GaimPresence *presence;

	g_return_if_fail(account != NULL);
	g_return_if_fail(name    != NULL);
	g_return_if_fail(gaim_account_is_connected(account));

	if ((buddy = gaim_find_buddy(account, name)) == NULL)
		return;

	presence = gaim_buddy_get_presence(buddy);

	gaim_presence_set_idle(presence, idle, idle_time);
}

void
gaim_prpl_got_user_login_time(GaimAccount *account, const char *name,
		time_t login_time)
{
	GaimBuddy *buddy;
	GaimPresence *presence;

	g_return_if_fail(account != NULL);
	g_return_if_fail(name    != NULL);

	if ((buddy = gaim_find_buddy(account, name)) == NULL)
		return;

	if (login_time == 0)
		login_time = time(NULL);

	presence = gaim_buddy_get_presence(buddy);

	gaim_presence_set_login_time(presence, login_time);
}

void
gaim_prpl_got_user_status(GaimAccount *account, const char *name,
		const char *status_id, const char *attr_id, ...)
{
	GaimBuddy *buddy;
	GaimPresence *presence;
	GaimStatus *status;
	gboolean was_online = FALSE;

	g_return_if_fail(account   != NULL);
	g_return_if_fail(name      != NULL);
	g_return_if_fail(status_id != NULL);
	g_return_if_fail(gaim_account_is_connected(account));

	if ((buddy = gaim_find_buddy(account, name)) == NULL)
		return;

	presence = gaim_buddy_get_presence(buddy);
	status   = gaim_presence_get_status(presence, status_id);

	g_return_if_fail(status != NULL);

	if (attr_id != NULL)
	{
		va_list args;

		va_start(args, attr_id);

		while (attr_id != NULL)
		{
			set_value_from_arg(status, attr_id, &args);

			attr_id = va_arg(args, char *);
		}

		va_end(args);
	}

	gaim_presence_set_status_active(presence, status_id, TRUE);

	gaim_blist_update_buddy_presence(buddy, GAIM_BUDDY_IS_ONLINE(buddy));
	gaim_contact_compute_priority_buddy(gaim_buddy_get_contact(buddy));
}

void
gaim_prpl_got_user_warning_level(GaimAccount *account, const char *name,
		unsigned int level)
{
	GaimBuddy *buddy;
	GaimPresence *presence;

	g_return_if_fail(account != NULL);
	g_return_if_fail(name    != NULL);

	if ((buddy = gaim_find_buddy(account, name)) == NULL)
		return;

	presence = gaim_buddy_get_presence(buddy);

	gaim_presence_set_warning_level(presence, level);
}

void
gaim_prpl_set_account_status(GaimAccount *account, GaimStatus *status)
{
	GaimPlugin *prpl;
	GaimPluginProtocolInfo *prpl_info;

	g_return_if_fail(account != NULL);
	g_return_if_fail(status  != NULL);

	prpl = gaim_find_prpl(gaim_account_get_protocol_id(account));

	if (prpl == NULL)
		return;

	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);

	if (prpl_info->set_status != NULL)
		prpl_info->set_status(account, status);
}

GList *
gaim_prpl_get_statuses(GaimAccount *account, GaimPresence *presence)
{
	GaimPlugin *prpl;
	GaimPluginProtocolInfo *prpl_info;
	GList *statuses = NULL;
	GList *l;
	GaimStatus *status;

	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail(presence != NULL, NULL);

	prpl = gaim_find_prpl(gaim_account_get_protocol_id(account));
	
	if (prpl == NULL)
		return NULL;
	
	prpl_info = GAIM_PLUGIN_PROTOCOL_INFO(prpl);
	if (prpl_info == NULL || prpl_info->status_types == NULL)
		return NULL;

	for (l = prpl_info->status_types(account); l != NULL; l = l->next)
	{
		status = gaim_status_new((GaimStatusType *)l->data, presence);
		statuses = g_list_append(statuses, status);
	}

	return statuses;
}


/**************************************************************************
 * Protocol Plugin Subsystem API
 **************************************************************************/

GaimPlugin *
gaim_find_prpl(const char *id)
{
	GList *l;
	GaimPlugin *plugin;

	for (l = gaim_plugins_get_protocols(); l != NULL; l = l->next) {
		plugin = (GaimPlugin *)l->data;

		/* Just In Case (TM) */
		if (GAIM_IS_PROTOCOL_PLUGIN(plugin)) {
			if (!strcmp(plugin->info->id, id))
				return plugin;
		}
	}

	return NULL;
}
