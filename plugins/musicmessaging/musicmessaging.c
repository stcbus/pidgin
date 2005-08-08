/*
 * Music messaging plugin for Gaim
 *
 * Copyright (C) 2005 Christian Muise.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include "internal.h"
#include "gtkgaim.h"

#include "conversation.h"

#include "gtkconv.h"
#include "gtkplugin.h"
#include "gtkutils.h"

#include "notify.h"
#include "version.h"
#include "debug.h"

#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include "dbus-maybe.h"
#include "dbus-bindings.h"
#include "dbus-server.h"

#define MUSICMESSAGING_PLUGIN_ID "gtk-hazure-musicmessaging"
#define MUSICMESSAGING_PREFIX "##MM##"
#define MUSICMESSAGING_START_TAG "<a href=mm-plugin>"

/* Globals */
/* List of sessions */
GList *conversations;

/* Pointer to this plugin */
GaimPlugin *plugin_pointer;

/* Define types needed for DBus */

/* Define the functions to export for use with DBus */
DBUS_EXPORT void music_messaging_change_request (const char *command, const char *parameters);
DBUS_EXPORT void music_messaging_change_confirmed (const char *command, const char *parameters);
DBUS_EXPORT void music_messaging_change_failed (const char *id, const char *command, const char *parameters);
DBUS_EXPORT void music_messaging_done_session (void);

/* This file has been generated by the #dbus-analize-functions.py
   script.  It contains dbus wrappers for the four functions declared
   above. */
#include "music-messaging-bindings.c"

/* Exported functions */
void music_messaging_change_request(const char *command, const char *parameters)
{
	gaim_notify_message(plugin_pointer, GAIM_NOTIFY_MSG_INFO, command,
                        parameters, NULL, NULL, NULL);
}

void music_messaging_change_confirmed(const char *command, const char *parameters)
{
	gaim_notify_message(plugin_pointer, GAIM_NOTIFY_MSG_INFO, command,
                        parameters, NULL, NULL, NULL);
}

void music_messaging_change_failed(const char *id, const char *command, const char *parameters)
{
	gaim_notify_message(plugin_pointer, GAIM_NOTIFY_MSG_INFO, command,
                        parameters, NULL, NULL, NULL);
}

void music_messaging_done_session(void)
{
	gaim_notify_message(plugin_pointer, GAIM_NOTIFY_MSG_INFO, "Session",
						"Session Complete", NULL, NULL, NULL);
}

typedef struct {
	GaimConversation *conv; /* pointer to the conversation */
	GtkWidget *seperator; /* seperator in the conversation */
	GtkWidget *button; /* button in the conversation */
	GPid pid; /* the pid of the score editor */
	
	gboolean started; /* session has started and editor run */
	gboolean originator; /* started the mm session */
	gboolean requested; /* received a request to start a session */
	
} MMConversation;

static gboolean start_session(MMConversation *mmconv);
static void run_editor(MMConversation *mmconv);
static void kill_editor(MMConversation *mmconv);
static void add_button (MMConversation *mmconv);
static void remove_widget (GtkWidget *button);
static void init_conversation (GaimConversation *conv);
static void conv_destroyed(GaimConversation *conv);
static void intercept_sent(GaimAccount *account, GaimConversation *conv, char **message, void* pData);
static void intercept_received(GaimAccount *account, char **sender, char **message, GaimConversation *conv, int *flags);


G_BEGIN_DECLS
DBusConnection *gaim_dbus_get_connection(void);
G_END_DECLS

void send_change_request (const char *id, const char *command, const char *parameters)
{
	DBusMessage *signal;
	signal = dbus_message_new_signal("/org/gscore/GScoreObject", "org.gscore.GScoreInterface", "gscore_change_request");
	
	dbus_message_append_args(signal,
			DBUS_TYPE_STRING, &id,
			DBUS_TYPE_STRING, &command,
			DBUS_TYPE_STRING, &parameters,
			DBUS_TYPE_INVALID);
	
	dbus_connection_send(gaim_dbus_get_connection(), signal, NULL);
	dbus_message_unref(signal);
}

void send_change_confirmed (const char *command, const char *parameters)
{
	DBusMessage *signal;
	signal = dbus_message_new_signal("/org/gscore/GScoreObject", "org.gscore.GScoreInterface", "gscore_change_confirmed");
	
	dbus_message_append_args(signal,
			DBUS_TYPE_STRING, &command,
			DBUS_TYPE_STRING, &parameters,
			DBUS_TYPE_INVALID);
	
	dbus_connection_send(gaim_dbus_get_connection(), signal, NULL);
	dbus_message_unref(signal);
}


static gboolean
plugin_load(GaimPlugin *plugin) {
    /* First, we have to register our four exported functions with the
       main gaim dbus loop.  Without this statement, the gaim dbus
       code wouldn't know about our functions. */
    GAIM_DBUS_REGISTER_BINDINGS(plugin);
	
	gaim_notify_message(plugin, GAIM_NOTIFY_MSG_INFO, "Welcome",
                        "Welcome to music messaging.", NULL, NULL, NULL);
	/* Keep the plugin for reference (needed for notify's) */
	plugin_pointer = plugin;
	
	/* Add the button to all the current conversations */
	gaim_conversation_foreach (init_conversation);
	
	/* Listen for any new conversations */
	void *conv_list_handle = gaim_conversations_get_handle();
	
	gaim_signal_connect(conv_list_handle, "conversation-created", 
					plugin, GAIM_CALLBACK(init_conversation), NULL);
	
	/* Listen for conversations that are ending */
	gaim_signal_connect(conv_list_handle, "deleting-conversation",
					plugin, GAIM_CALLBACK(conv_destroyed), NULL);
					
	/* Listen for sending/receiving messages to replace tags */
	gaim_signal_connect(conv_list_handle, "writing-im-msg",
					plugin, GAIM_CALLBACK(intercept_sent), NULL);
	gaim_signal_connect(conv_list_handle, "receiving-im-msg",
					plugin, GAIM_CALLBACK(intercept_received), NULL);
	
	return TRUE;
}

static gboolean
plugin_unload(GaimPlugin *plugin) {
	
	gaim_notify_message(plugin, GAIM_NOTIFY_MSG_INFO, "Unloaded",
						DATADIR, NULL, NULL, NULL);
	
	MMConversation *mmconv = NULL;
	while (g_list_length(conversations) > 0)
	{
		mmconv = g_list_first(conversations)->data;
		conv_destroyed(mmconv->conv);
	}
	return TRUE;
}

static void intercept_sent(GaimAccount *account, GaimConversation *conv, char **message, void* pData)
{    
	GaimConvIm *imData = gaim_conversation_get_im_data(conv);
	GaimConnection *connection = gaim_conversation_get_gc(conv);
	const char *convName = gaim_conversation_get_name(conv);
	const char *who = gaim_account_get_username(account);
	
	if (0 == strncmp(*message, MUSICMESSAGING_PREFIX, strlen(MUSICMESSAGING_PREFIX)))
	{
		message = 0;
		gaim_debug(GAIM_DEBUG_MISC, "gaim-musicmessaging", "Received MM Message\n");
		send_change_confirmed("the command", "the params");
	}
	else if (0 == strncmp(*message, MUSICMESSAGING_START_TAG, strlen(MUSICMESSAGING_START_TAG)))
	{
		
	}
	else
	{
		serv_send_im(connection, convName, *message, GAIM_MESSAGE_SEND);
		gaim_conv_im_write (imData, NULL, *message, GAIM_MESSAGE_SYSTEM, time(NULL));
	}
}

static void intercept_received(GaimAccount *account, char **sender, char **message, GaimConversation *conv, int *flags)
{
	
}


static gboolean
start_session(MMConversation *mmconv)
{
	mmconv->started = TRUE;
	run_editor(mmconv);
	return TRUE;
}

static void music_button_toggled (GtkWidget *widget, gpointer data)
{
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget))) 
    {
		start_session((MMConversation *) data);    
    } else {
        kill_editor((MMConversation *) data);
    }
}

static void set_editor_path (GtkWidget *button, GtkWidget *text_field)
{
	const char * path = gtk_entry_get_text((GtkEntry*)text_field);
	gaim_prefs_set_string("/plugins/gtk/musicmessaging/editor_path", path);
	
}

static void run_editor (MMConversation *mmconv)
{
	GError *spawn_error = NULL;
	gchar * args[2];
	args[0] = (gchar *)gaim_prefs_get_string("/plugins/gtk/musicmessaging/editor_path");
	args[1] = NULL;
	if (!(g_spawn_async (".", args, NULL, 12, NULL, NULL, &(mmconv->pid), &spawn_error)))
	{
		gaim_notify_error(plugin_pointer, "Error Running Editor",
						"The following error has occured:", spawn_error->message);
	}
}

static void kill_editor (MMConversation *mmconv)
{
	if (mmconv->pid)
	{
		kill(mmconv->pid, SIGINT);
		mmconv->pid = 0;
	}
}

static void init_conversation (GaimConversation *conv)
{
	MMConversation *mmconv;
	mmconv = g_malloc(sizeof(MMConversation));
	
	mmconv->conv = conv;
	mmconv->started = FALSE;
	mmconv->originator = FALSE;
	mmconv->requested = FALSE;
	
	add_button(mmconv);
	
	conversations = g_list_append(conversations, mmconv);
}

static void conv_destroyed (GaimConversation *conv)
{
	MMConversation *mmconv_current = NULL;
	MMConversation *mmconv = NULL;
	guint i;
	
	for (i = 0; i < g_list_length(conversations); i++)
	{
		mmconv_current = (MMConversation *)g_list_nth_data(conversations, i);
		if (conv == mmconv_current->conv)
		{
			mmconv = mmconv_current;
		}
	}
	
	remove_widget(mmconv->button);
	remove_widget(mmconv->seperator);
	if (mmconv->started)
	{
		kill_editor(mmconv);
	}
	conversations = g_list_remove(conversations, mmconv);
}

static void add_button (MMConversation *mmconv)
{
	GaimConversation *conv = mmconv->conv;
	
	GtkWidget *button, *image, *sep;

	button = gtk_toggle_button_new();
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);

	g_signal_connect(G_OBJECT(button), "toggled", G_CALLBACK(music_button_toggled), mmconv);

	gchar *file_path = g_build_filename (DATADIR, "pixmaps", "gaim", "buttons", "music.png", NULL);
	image = gtk_image_new_from_file("/usr/local/share/pixmaps/gaim/buttons/music.png");

	gtk_container_add((GtkContainer *)button, image);
	
	sep = gtk_vseparator_new();
	
	mmconv->seperator = sep;
	mmconv->button = button;
	
	gtk_widget_show(sep);
	gtk_widget_show(image);
	gtk_widget_show(button);
	
	gtk_box_pack_start(GTK_BOX(GAIM_GTK_CONVERSATION(conv)->toolbar), sep, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(GAIM_GTK_CONVERSATION(conv)->toolbar), button, FALSE, FALSE, 0);
}

static void remove_widget (GtkWidget *button)
{
	gtk_widget_hide(button);
	gtk_widget_destroy(button);		
}

static GtkWidget *
get_config_frame(GaimPlugin *plugin)
{
	GtkWidget *ret;
	GtkWidget *vbox;
	
	GtkWidget *editor_path;
	GtkWidget *editor_path_label;
	GtkWidget *editor_path_button;
	
	/* Outside container */
	ret = gtk_vbox_new(FALSE, 18);
	gtk_container_set_border_width(GTK_CONTAINER(ret), 10);

	/* Configuration frame */
	vbox = gaim_gtk_make_frame(ret, _("Music Messaging Configuration"));
	
	/* Path to the score editor */
	editor_path = gtk_entry_new();
	editor_path_label = gtk_label_new("Score Editor Path");
	editor_path_button = gtk_button_new_with_mnemonic(_("_Apply"));
	
	gtk_entry_set_text((GtkEntry*)editor_path, "/usr/local/bin/gscore");
	
	g_signal_connect(G_OBJECT(editor_path_button), "clicked",
					 G_CALLBACK(set_editor_path), editor_path);
					 
	gtk_box_pack_start(GTK_BOX(vbox), editor_path_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), editor_path, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), editor_path_button, FALSE, FALSE, 0);
	
	gtk_widget_show_all(ret);

	return ret;
}

static GaimGtkPluginUiInfo ui_info =
{
	get_config_frame
};

static GaimPluginInfo info = {
    GAIM_PLUGIN_MAGIC,
    GAIM_MAJOR_VERSION,
    GAIM_MINOR_VERSION,
    GAIM_PLUGIN_STANDARD,
    GAIM_GTK_PLUGIN_TYPE,
    0,
    NULL,
    GAIM_PRIORITY_DEFAULT,

    MUSICMESSAGING_PLUGIN_ID,
    "Music Messaging",
    VERSION,
    "Music Messaging Plugin for collabrative composition.",
    "The Music Messaging Plugin allows a number of users to simultaniously work on a piece of music by editting a common score in real-time.",
    "Christian Muise <christian.muise@gmail.com>",
    GAIM_WEBSITE,
    plugin_load,
    plugin_unload,
    NULL,
    &ui_info,
    NULL,
    NULL,
    NULL
};

static void
init_plugin(GaimPlugin *plugin) {
	gaim_prefs_add_none("/plugins/gtk/musicmessaging");
	gaim_prefs_add_string("/plugins/gtk/musicmessaging/editor_path", "/usr/local/bin/gscore");
}

GAIM_INIT_PLUGIN(musicmessaging, init_plugin, info);
