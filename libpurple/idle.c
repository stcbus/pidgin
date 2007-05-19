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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include "internal.h"

#include "connection.h"
#include "debug.h"
#include "idle.h"
#include "log.h"
#include "prefs.h"
#include "savedstatuses.h"
#include "signals.h"

#define IDLEMARK 600 /* 10 minutes! */

typedef enum
{
	PURPLE_IDLE_NOT_AWAY = 0,
	PURPLE_IDLE_AUTO_AWAY,
	PURPLE_IDLE_AWAY_BUT_NOT_AUTO_AWAY

} PurpleAutoAwayState;

static PurpleIdleUiOps *idle_ui_ops = NULL;

/**
 * This is needed for the I'dle Mak'er plugin to work correctly.  We
 * use it to determine if we're the ones who set our accounts idle
 * or if someone else did it (the I'dle Mak'er plugin, for example).
 * Basically we just keep track of which accounts were set idle by us,
 * and then we'll only set these specific accounts unidle when the
 * user returns.
 */
static GList *idled_accts = NULL;

static guint idle_timer = 0;

static time_t last_active_time = 0;

static void
set_account_idle(PurpleAccount *account, int time_idle)
{
	PurplePresence *presence;

	presence = purple_account_get_presence(account);

	if (purple_presence_is_idle(presence))
		/* This account is already idle! */
		return;

	purple_debug_info("idle", "Setting %s idle %d seconds\n",
			   purple_account_get_username(account), time_idle);
	purple_presence_set_idle(presence, TRUE, time(NULL) - time_idle);
	idled_accts = g_list_prepend(idled_accts, account);
}

static void
set_account_unidle(PurpleAccount *account)
{
	PurplePresence *presence;

	presence = purple_account_get_presence(account);

	idled_accts = g_list_remove(idled_accts, account);

	if (!purple_presence_is_idle(presence))
		/* This account is already unidle! */
		return;

	purple_debug_info("idle", "Setting %s unidle\n",
			   purple_account_get_username(account));
	purple_presence_set_idle(presence, FALSE, 0);
}


static gint time_until_next_idle_event;
/*
 * This function should be called when you think your idle state
 * may have changed.  Maybe you're over the 10-minute mark and
 * Purple should start reporting idle time to the server.  Maybe
 * you've returned from being idle.  Maybe your auto-away message
 * should be set.
 *
 * There is no harm to calling this many many times, other than
 * it will be kinda slow.  This is called every 5 seconds by a
 * timer set when Purple starts.  It is also called when
 * you send an IM, a chat, etc.
 *
 * This function has 3 sections.
 * 1. Get your idle time.  It will query XScreenSaver or Windows
 *    or use the Purple idle time.  Whatever.
 * 2. Set or unset your auto-away message.
 * 3. Report your current idle time to the IM server.
 */

static void
check_idleness(void)
{
	time_t time_idle;
	gboolean auto_away;
	const gchar *idle_reporting;
	gboolean report_idle;
	GList *l;
	gint away_seconds = 0;
	static int no_away = 0;

	purple_signal_emit(purple_blist_get_handle(), "update-idle");

	idle_reporting = purple_prefs_get_string("/purple/away/idle_reporting");
	report_idle = TRUE;
	if (!strcmp(idle_reporting, "system") &&
		(idle_ui_ops != NULL) && (idle_ui_ops->get_time_idle != NULL))
	{
		/* Use system idle time (mouse or keyboard movement, etc.) */
		time_idle = idle_ui_ops->get_time_idle();
	}
	else if (!strcmp(idle_reporting, "purple"))
	{
		/* Use 'Purple idle' */
		time_idle = time(NULL) - last_active_time;
	}
	else
	{
		/* Don't report idle time */
		time_idle = 0;
		report_idle = FALSE;
	}

	/* Auto-away stuff */
	auto_away = purple_prefs_get_bool("/purple/away/away_when_idle");

	/* If we're not reporting idle, we can still do auto-away.
	 * First try "system" and if that isn't possible, use "purple" */
	if (!report_idle && auto_away) {
		if ((idle_ui_ops != NULL) && (idle_ui_ops->get_time_idle != NULL))
			time_idle = idle_ui_ops->get_time_idle();
		else
			time_idle = time(NULL) - last_active_time;
	}

	time_until_next_idle_event = IDLEMARK - time_idle; /* reasonable start upperbound */

	if (auto_away || !no_away)
		away_seconds = 60 * purple_prefs_get_int("/purple/away/mins_before_away");

	if (auto_away && time_idle > away_seconds)
	{
		purple_savedstatus_set_idleaway(TRUE);
		no_away = 0;
	}
	else if (!no_away && time_idle < away_seconds)
	{
		purple_savedstatus_set_idleaway(FALSE);
		no_away = 1;
		if ((away_seconds - time_idle) < time_until_next_idle_event)
			time_until_next_idle_event = away_seconds - time_idle;
	}

	/* Idle reporting stuff */
	if (report_idle && (time_idle >= IDLEMARK))
	{
		for (l = purple_connections_get_all(); l != NULL; l = l->next)
		{
			PurpleConnection *gc = l->data;
			set_account_idle(purple_connection_get_account(gc), time_idle);
		}
	}
	else if (!report_idle || (time_idle < IDLEMARK))
	{
		while (idled_accts != NULL)
			set_account_unidle(idled_accts->data);
	}
	
	if (time_until_next_idle_event < 0)
		time_until_next_idle_event = IDLEMARK;
}


/*
 * Check idle and set the timer to fire at the next idle-worth event 
 */
static gint
check_idleness_timer()
{
	check_idleness();
	idle_timer = purple_timeout_add(1000 * (time_until_next_idle_event + 1), check_idleness_timer, NULL);
	return FALSE;
}

static void
im_msg_sent_cb(PurpleAccount *account, const char *receiver,
			   const char *message, void *data)
{
	/* Check our idle time after an IM is sent */
	check_idleness();
}

static void
signing_on_cb(PurpleConnection *gc, void *data)
{
	/* When signing on a new account, check if the account should be idle */
	check_idleness();
}

static void
signing_off_cb(PurpleConnection *gc, void *data)
{
	PurpleAccount *account;

	account = purple_connection_get_account(gc);
	set_account_unidle(account);
}

void
purple_idle_touch()
{
	time(&last_active_time);
}

void
purple_idle_set(time_t time)
{
	last_active_time = time;
}

void
purple_idle_set_ui_ops(PurpleIdleUiOps *ops)
{
	idle_ui_ops = ops;
}

PurpleIdleUiOps *
purple_idle_get_ui_ops(void)
{
	return idle_ui_ops;
}

static void *
purple_idle_get_handle()
{
	static int handle;

	return &handle;
}

void
purple_idle_init()
{
	/* Add the timer to check if we're idle */
	idle_timer = purple_timeout_add(1000 * (IDLEMARK + 1), check_idleness_timer, NULL);

	purple_signal_connect(purple_conversations_get_handle(), "sent-im-msg",
						purple_idle_get_handle(),
						PURPLE_CALLBACK(im_msg_sent_cb), NULL);
	purple_signal_connect(purple_connections_get_handle(), "signing-on",
						purple_idle_get_handle(),
						PURPLE_CALLBACK(signing_on_cb), NULL);
	purple_signal_connect(purple_connections_get_handle(), "signing-off",
						purple_idle_get_handle(),
						PURPLE_CALLBACK(signing_off_cb), NULL);

	purple_idle_touch();
}

void
purple_idle_uninit()
{
	purple_signals_disconnect_by_handle(purple_idle_get_handle());

	/* Remove the idle timer */
	if (idle_timer > 0)
		purple_timeout_remove(idle_timer);
	idle_timer = 0;
}
