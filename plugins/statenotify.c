#include "internal.h"

#include "blist.h"
#include "conversation.h"
#include "debug.h"
#include "signals.h"

static void
write_status(struct buddy *buddy, const char *message)
{
	GaimConversation *conv;
	const char *who;
	char buf[256];

	conv = gaim_find_conversation_with_account(buddy->name, buddy->account);

	if (conv == NULL)
		return;

	who = gaim_get_buddy_alias(buddy);

	g_snprintf(buf, sizeof(buf), "%s %s", who, message);

	gaim_conversation_write(conv, NULL, buf, -1, WFLAG_SYSTEM, time(NULL));
}

static void
buddy_away_cb(struct buddy *buddy, void *data)
{
	write_status(buddy, _("has gone away."));
}

static void
buddy_unaway_cb(struct buddy *buddy, void *data)
{
	write_status(buddy, _("is no longer away."));
}

static void
buddy_idle_cb(struct buddy *buddy, void *data)
{
	write_status(buddy, _("has become idle."));
}

static void
buddy_unidle_cb(struct buddy *buddy, void *data)
{
	write_status(buddy, _("is no longer idle."));
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	void *blist_handle = gaim_blist_get_handle();

	gaim_signal_connect(blist_handle, "buddy-away",
						plugin, GAIM_CALLBACK(buddy_away_cb), NULL);
	gaim_signal_connect(blist_handle, "buddy-back",
						plugin, GAIM_CALLBACK(buddy_unaway_cb), NULL);
	gaim_signal_connect(blist_handle, "buddy-idle",
						plugin, GAIM_CALLBACK(buddy_idle_cb), NULL);
	gaim_signal_connect(blist_handle, "buddy-unidle",
						plugin, GAIM_CALLBACK(buddy_unidle_cb), NULL);

	return TRUE;
}

static GaimPluginInfo info =
{
	2,                                                /**< api_version    */
	GAIM_PLUGIN_STANDARD,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	NULL,                                             /**< id             */
	N_("Buddy State Notification"),                   /**< name           */
	VERSION,                                          /**< version        */
	                                                  /**  summary        */
	N_("Notifies in a conversation window when a buddy goes or returns from "
	   "away or idle."),
	                                                  /**  description    */
	N_("Notifies in a conversation window when a buddy goes or returns from "
	   "away or idle."),
	"Christian Hammond <chipx86@gnupdate.org>",       /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	plugin_load,                                      /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	NULL                                              /**< extra_info     */
};

static void
init_plugin(GaimPlugin *plugin)
{
}

GAIM_INIT_PLUGIN(statenotify, init_plugin, info)
