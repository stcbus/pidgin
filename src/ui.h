/**
 * @file ui.h Main GTK+ UI include file
 * @defgroup gtkui GTK+ User Interface
 *
 * gaim
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
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
 */

#ifndef _UI_H_
#define _UI_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "core.h"
#include "multi.h"
#include "gtkconv.h"
#include "pounce.h"
#include "gtkft.h"
#include "gtkprefs.h"
#include "gtkutils.h"
#include "stock.h"

#define GAIM_DIALOG(x)	x = gtk_window_new(GTK_WINDOW_TOPLEVEL); \
			gtk_window_set_type_hint(GTK_WINDOW(x), GDK_WINDOW_TYPE_HINT_DIALOG)
#define GAIM_WINDOW_ICONIFIED(x) (gdk_window_get_state(GTK_WIDGET(x)->window) & GDK_WINDOW_STATE_ICONIFIED)

/* This is backwards-compatibility code for old versions of GTK+ (2.2.1 and
 * earlier).  It defines the new wrap behavior (unknown in earlier versions)
 * as the old (slightly buggy) wrap behavior.
 */
#ifndef GTK_WRAP_WORD_CHAR
#define GTK_WRAP_WORD_CHAR GTK_WRAP_WORD
#endif

#define DEFAULT_FONT_FACE "Helvetica"

#define FACE_ANGEL 0
#define FACE_BIGSMILE 1
#define FACE_BURP 2
#define FACE_CROSSEDLIPS 3
#define FACE_CRY 4
#define FACE_EMBARRASSED 5
#define FACE_KISS 6
#define FACE_MONEYMOUTH 7
#define FACE_SAD 8
#define FACE_SCREAM 9
#define FACE_SMILE 10
#define FACE_SMILE8 11
#define FACE_THINK 12
#define FACE_TONGUE 13
#define FACE_WINK 14
#define FACE_YELL 15
#define FACE_TOTAL 16

#define GAIM_LOGO 0
#define GAIM_ERROR 1
#define GAIM_WARNING 2
#define GAIM_INFO 3

typedef enum
{
	GAIM_BUTTON_HORIZONTAL,
	GAIM_BUTTON_VERTICAL

} GaimButtonOrientation;

typedef enum
{
	GAIM_BUTTON_NONE = 0,
	GAIM_BUTTON_TEXT,
	GAIM_BUTTON_IMAGE,
	GAIM_BUTTON_TEXT_IMAGE

} GaimButtonStyle;

/* XXX CUI: save_pos and window_size are used by gaimrc.c which is core.
 * Need to figure out options saving. Same goes for several global variables as well. */
struct save_pos {
        int x;
        int y;
        int width;
        int height;
};

struct window_size {
	int width;
	int height;
	int entry_height;
};

struct log_conversation {
	char name[80];
	char filename[512];
        struct log_conversation *next;
};

/* XXX CUI: away messages aren't really anything more than char* but we need two char*'s
 * for the UI so that people can name their away messages when they save them. So these
 * are really a UI function and struct away_message should be removed from the core. */
struct away_message {
	char name[80];
	char message[2048];
};



/* this is used for queuing messages received while away. This is really a UI function
 * which is why the struct is here. */

struct queued_message {
	char name[80];
	char *message;
	time_t tm;
	GaimAccount *account;
	int flags;
	int len;
};

struct smiley_theme {
	char *path;
	char *name;
	char *desc;
	char *icon;
	char *author;
	
	struct smiley_list *list;
};


#define EDIT_GC    0
#define EDIT_GROUP 1
#define EDIT_BUDDY 2

/* Globals in aim.c */
extern GList *log_conversations; /* this should be moved to conversations.c */
extern GSList *away_messages; /* this should be moved to away.c */
extern GtkWidget *mainwindow;
extern int docklet_count;

/* Globals in away.c */
extern struct away_message *awaymessage;
extern struct away_message *default_away;
extern int auto_away;
extern GtkWidget *awaymenu;
extern GtkWidget *awayqueue;
extern GtkListStore *awayqueuestore;

/* Globals in dialog.c */
extern char fontxfld[256];
extern int smiley_array[FACE_TOTAL];
extern GtkWidget *fgcseld;
extern GtkWidget *bgcseld;
void build_allow_list();
void build_block_list();

/* Globals in pounce.c */
extern GtkWidget *bpmenu;

/* Globals in prefs.c */
extern struct debug_window *dw;

/* Globals in prpl.c */
extern GtkWidget *protomenu;

/* Globals in session.c */
extern gboolean session_managed;

/* Globals in themes.c */
extern struct smiley_theme *current_smiley_theme;
extern GSList *smiley_themes;


/* Functions in about.c */
extern void show_about(GtkWidget *, void *);
extern void gaim_help(GtkWidget *, void *);

/* Functions in aim.c */
extern void show_login();
extern void gaim_setup(GaimConnection *gc);

/* Functions in away.c */
extern void rem_away_mess(GtkWidget *, struct away_message *);
extern void do_away_message(GtkWidget *, struct away_message *);
extern void do_away_menu();
extern void away_list_unclicked(GtkWidget *, struct away_message *);
extern void away_list_clicked(GtkWidget *, struct away_message *);
extern void toggle_away_queue();
extern void purge_away_queue(GSList**);

/* Functions in browser.c */
extern void open_url(GtkWidget *, char *);
extern void add_bookmark(GtkWidget *, char *);

/* Functions in buddy.c */
extern void handle_group_rename(struct group *, char *);
extern void handle_buddy_rename(struct buddy *, char *);
extern void destroy_buddy();
extern void update_button_pix();
extern void toggle_show_empty_groups();
extern void update_all_buddies();
extern void update_num_groups(void);
extern void show_buddy_list();
extern void signoff_all();
extern void do_im_back();
extern void set_buddy(GaimConnection *, struct buddy *);
extern void build_edit_tree();
extern void do_bp_menu();
extern void ui_add_buddy(GaimConnection *, struct group *, struct buddy *);
extern void ui_remove_buddy(struct buddy *);
extern void ui_add_group(struct group *);
extern void ui_remove_group(struct group *);
extern void toggle_buddy_pixmaps();
extern void gaim_separator(GtkWidget *);
extern void set_blist_tab();
extern void hide_buddy_list();
extern void unhide_buddy_list();
extern void docklet_add();
extern void docklet_remove();
extern void docklet_toggle();
extern void update_idle_times();
extern void build_imchat_box(gboolean);
extern void show_xfer_dialog();

/* Functions in dialogs.c */
extern void alias_dialog_bud(struct buddy *);
extern void alias_dialog_chat(struct chat *);
extern void show_warn_dialog(GaimConnection *, char *);
extern void show_im_dialog();
extern void show_info_dialog();
extern void show_add_buddy(GaimConnection *, char *, char *, char *);
extern void show_add_chat(GaimAccount *, struct group *);
extern void show_add_group(GaimConnection *);
extern void show_add_perm(GaimConnection *, char *, gboolean);
extern void destroy_all_dialogs();
extern void show_import_dialog();
extern void show_export_dialog();
extern void conv_show_log(GtkWidget *, gpointer);
extern void chat_show_log(GtkWidget *, gpointer);
extern void show_log(char *);
extern void show_log_dialog(GaimConversation *);
extern void show_fgcolor_dialog(GaimConversation *c, GtkWidget *color);
extern void show_bgcolor_dialog(GaimConversation *c, GtkWidget *color);
extern void cancel_fgcolor(GtkWidget *widget, GaimConversation *c);
extern void cancel_bgcolor(GtkWidget *widget, GaimConversation *c);
extern void create_away_mess(GtkWidget *, void *);
extern void show_ee_dialog(int);
extern void show_insert_link(GtkWidget *,GaimConversation *);
extern void show_smiley_dialog(GaimConversation *, GtkWidget *);
extern void close_smiley_dialog(GtkWidget *widget, GaimConversation *c);
extern void set_smiley_array(GtkWidget *widget, int smiley_type);
extern void insert_smiley_text(GtkWidget *widget, GaimConversation *c);
extern void cancel_log(GtkWidget *, GaimConversation *);
extern void cancel_link(GtkWidget *, GaimConversation *);
extern void show_font_dialog(GaimConversation *c, GtkWidget *font);
extern void cancel_font(GtkWidget *widget, GaimConversation *c);
extern void apply_font(GtkWidget *widget, GtkFontSelection *fontsel);
extern void set_color_selection(GtkWidget *selection, GdkColor color);
extern void show_rename_group(GtkWidget *, struct group *);
extern void show_rename_buddy(GtkWidget *, struct buddy *);
extern void load_perl_script();
extern GtkWidget *gaim_pixbuf_button(char *, char *, GaimButtonOrientation);
extern GtkWidget *gaim_pixbuf_button_from_stock(const char *, const char *, GaimButtonOrientation);
extern GtkWidget *gaim_pixbuf_toolbar_button_from_stock(char *);
extern int file_is_dir(const char *, GtkWidget *);
extern void update_privacy_connections();
extern void show_privacy_options();
extern void build_allow_list();
extern void build_block_list();
extern void destroy_fontsel(GtkWidget *w, gpointer d);
extern void join_chat();

/* Functions in multi.c */
extern void account_editor(GtkWidget *, GtkWidget *);

/* Functions in plugins.c */
#ifdef GAIM_PLUGINS
extern void show_plugins(GtkWidget *, gpointer);
extern void update_show_plugins(); /* this is a hack and will be removed */
#endif

/* Functions in prpl.c */
extern void register_dialog();

/* Functions in server.c */
/* server.c is in desperate need need of a split */
extern int find_queue_total_by_name(char *);

/* Functions in session.c */
extern void session_init(gchar *, gchar *);
extern void session_end();

/* Functions in themes.c */
extern void smiley_themeize(GtkWidget *);
extern void smiley_theme_probe();
extern void load_smiley_theme(const char *file, gboolean load);
extern GSList *get_proto_smileys(int protocol);

#endif /* _UI_H_ */
