#include "gtkinternal.h"

#include "conversation.h"
#include "debug.h"
#include "prpl.h"
#include "gtkplugin.h"
#include "gtkutils.h"

#ifdef MAX
# undef MAX
# undef MIN
#endif

#include "protocols/jabber/jabber.h"
#include "protocols/msn/session.h"

#define RAW_PLUGIN_ID "gtk-raw"

static GtkWidget *window = NULL;
static GaimAccount *account = NULL;
static GaimPlugin *my_plugin = NULL;

static int
window_closed_cb()
{
	gaim_plugin_unload(my_plugin);

	return FALSE;
}

static void
text_sent_cb(GtkEntry *entry)
{
	const char *txt;
	GaimConnection *gc;

	if (account == NULL)
		return;

	gc = gaim_account_get_connection(account);

	txt = gtk_entry_get_text(entry);

	switch (gaim_account_get_protocol(account)) {
		case GAIM_PROTO_TOC:
			{
				int *a = (int *)gc->proto_data;
				unsigned short seqno = htons(a[1]++ & 0xffff);
				unsigned short len = htons(strlen(txt) + 1);
				write(*a, "*\002", 2);
				write(*a, &seqno, 2);
				write(*a, &len, 2);
				write(*a, txt, ntohs(len));
				gaim_debug(GAIM_DEBUG_MISC, "raw", "TOC C: %s\n", txt);
			}
			break;

		case GAIM_PROTO_MSN:
			{
				MsnSession *session = gc->proto_data;
				char buf[strlen(txt) + 3];

				g_snprintf(buf, sizeof(buf), "%s\r\n", txt);
				msn_servconn_write(session->notification_conn, buf, strlen(buf));
			}
			break;

		case GAIM_PROTO_IRC:
			write(*(int *)gc->proto_data, txt, strlen(txt));
			write(*(int *)gc->proto_data, "\r\n", 2);
			gaim_debug(GAIM_DEBUG_MISC, "raw", "IRC C: %s\n", txt);
			break;

		case GAIM_PROTO_JABBER:
			jab_send_raw(*(jconn *)gc->proto_data, txt);
			break;

		default:
			break;
	}

	gtk_entry_set_text(entry, "");
}

static void
account_changed_cb(GtkWidget *dropdown, GaimAccount *new_account,
				   void *user_data)
{
	account = new_account;
}

static gboolean
plugin_load(GaimPlugin *plugin)
{
	GtkWidget *hbox;
	GtkWidget *entry;
	GtkWidget *dropdown;

	/* Setup the window. */
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width(GTK_CONTAINER(window), 6);

	g_signal_connect(G_OBJECT(window), "delete_event",
					 G_CALLBACK(window_closed_cb), NULL);

	/* Main hbox */
	hbox = gtk_hbox_new(FALSE, 6);
	gtk_container_add(GTK_CONTAINER(window), hbox);

	/* Account drop-down menu. */
	dropdown = gaim_gtk_account_option_menu_new(NULL, FALSE,
			G_CALLBACK(account_changed_cb), NULL);

	gtk_box_pack_start(GTK_BOX(hbox), dropdown, FALSE, FALSE, 0);

	/* Entry box */
	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(entry), "activate",
					 G_CALLBACK(text_sent_cb), NULL);

	gtk_widget_show_all(window);

	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin)
{
	if (window)
		gtk_widget_destroy(window);

	window = NULL;

	return TRUE;
}

static GaimPluginInfo info =
{
	2,
	GAIM_PLUGIN_STANDARD,
	GAIM_GTK_PLUGIN_TYPE,
	0,
	NULL,
	GAIM_PRIORITY_DEFAULT,
	RAW_PLUGIN_ID,
	N_("Raw"),
	VERSION,
	N_("Lets you send raw input to text-based protocols."),
	N_("Lets you send raw input to text-based protocols (Jabber, MSN, IRC, "
	   "TOC). Hit 'Enter' in the entry box to send. Watch the debug window."),
	"Eric Warmenhoven <eric@warmenhoven.org>",
	GAIM_WEBSITE,
	plugin_load,
	plugin_unload,
	NULL,
	NULL,
	NULL
};

static void
init_plugin(GaimPlugin *plugin)
{
	my_plugin = plugin;
}

GAIM_INIT_PLUGIN(raw, init_plugin, info)
