/*
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
 *
 */

#include "prpl.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "pixmaps/ok.xpm"
#include "pixmaps/cancel.xpm"
#include "pixmaps/register.xpm"

GSList *protocols = NULL;

static GtkWidget *regdialog = NULL;
static GtkWidget *regbox = NULL;
static struct prpl *regprpl = NULL;

GtkWidget *protomenu = NULL;

struct _prompt {
	GtkWidget *window;
	GtkWidget *entry;
	void (*doit)(void *, char *);
	void (*dont)(void *);
	void *data;
};

struct prpl *find_prpl(int prot)
{
	GSList *e = protocols;
	struct prpl *r;

	while (e) {
		r = (struct prpl *)e->data;
		if (r->protocol == prot)
			return r;
		e = e->next;
	}

	return NULL;
}

static gint proto_compare(struct prpl *a, struct prpl *b)
{
	/* neg if a before b, 0 if equal, pos if a after b */
	return a->protocol - b->protocol;
}

void load_protocol(proto_init pi, int size)
{
	struct prpl *p;
	struct prpl *old;
	if (size != sizeof(struct prpl)) {
		do_error_dialog(_("You have attempted to load a protocol which was not compiled"
				" from the same version of the source as this application was."
				" Unfortunately, because it is not the same version I cannot"
				" safely tell you which one it was. Needless to say, it was not"
				" successfully loaded."), _("Protocol Error"));
		return;
	}
	p = g_new0(struct prpl, 1);
	pi(p);
	if ((old = find_prpl(p->protocol)) == NULL)
		unload_protocol(old);
	protocols = g_slist_insert_sorted(protocols, p, (GCompareFunc)proto_compare);
	if (regdialog)
		gtk_widget_destroy(regdialog);
}

void unload_protocol(struct prpl *p)
{
	GSList *c = connections;
	struct gaim_connection *g;
	while (c) {
		g = (struct gaim_connection *)c->data;
		if (g->prpl == p) {
			char buf[256];
			g_snprintf(buf, sizeof buf, _("%s was using %s, which got removed."
						      " %s is now offline."), g->username,
				   (*p->name)(), g->username);
			do_error_dialog(buf, _("Disconnect"));
			signoff(g);
			c = connections;
		} else
			c = c->next;
	}
	protocols = g_slist_remove(protocols, p);
	g_free(p);
}

STATIC_PROTO_INIT

static void des_win(GtkWidget *a, GtkWidget *b)
{
	gtk_widget_destroy(b);
}

static void rem_win(GtkWidget *a, GtkWidget *b)
{
	void *d = gtk_object_get_user_data(GTK_OBJECT(a));
	if (d)
		gtk_signal_disconnect_by_data(GTK_OBJECT(b), d);
	gtk_widget_destroy(b);
}

void do_ask_dialog(const char *text, void *data, void *doit, void *dont)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *button;

	window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_wmclass(GTK_WINDOW(window), "accept", "Gaim");
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, TRUE);
	gtk_window_set_title(GTK_WINDOW(window), _("Accept?"));
	gtk_widget_realize(window);
	aol_icon(window->window);
	if (dont)
		gtk_signal_connect(GTK_OBJECT(window), "destroy", GTK_SIGNAL_FUNC(dont), data);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	label = gtk_label_new(text);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	button = picture_button(window, _("Cancel"), cancel_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(des_win), window);

	button = picture_button(window, _("Accept"), ok_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	if (dont)
		gtk_object_set_user_data(GTK_OBJECT(button), data);
	if (doit)
		gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(doit), data);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(rem_win), window);

	gtk_widget_show_all(window);
}

static void des_prompt(GtkWidget *w, struct _prompt *p)
{
	if (p->dont)
		(p->dont)(p->data);
	gtk_widget_destroy(p->window);
	g_free(p);
}

static void act_prompt(GtkWidget *w, struct _prompt *p)
{
	if (p->doit)
		(p->doit)(p->data, gtk_entry_get_text(GTK_ENTRY(p->entry)));
	gtk_widget_destroy(p->window);
}

void do_prompt_dialog(const char *text, void *data, void *doit, void *dont)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *button;
	struct _prompt *p;

	p = g_new0(struct _prompt, 1);
	p->data = data;
	p->doit = doit;
	p->dont = dont;

	window = gtk_window_new(GTK_WINDOW_DIALOG);
	p->window = window;
	gtk_window_set_wmclass(GTK_WINDOW(window), "prompt", "Gaim");
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, TRUE);
	gtk_window_set_title(GTK_WINDOW(window), _("Gaim - Prompt"));
	gtk_signal_connect(GTK_OBJECT(window), "destroy", GTK_SIGNAL_FUNC(des_prompt), p);
	gtk_widget_realize(window);
	aol_icon(window->window);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(text);
	gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(entry), "activate", GTK_SIGNAL_FUNC(act_prompt), p);
	p->entry = entry;

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	button = picture_button(window, _("Cancel"), cancel_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(des_win), window);

	button = picture_button(window, _("Accept"), ok_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(act_prompt), p);

	gtk_widget_show_all(window);
}

static void delete_reg(gpointer a, gpointer b)
{
	GtkWidget *tmp = regdialog;
	if (regdialog) {
		regdialog = NULL;
		gtk_widget_destroy(tmp);
	}
}

void prepare_regbox_for_next()
{
	while (GTK_BOX(regbox)->children)
		gtk_container_remove(GTK_CONTAINER(regbox),
				     ((GtkBoxChild *)GTK_BOX(regbox)->children->data)->widget);
}

static void reg_prpl(gpointer a, struct prpl *p)
{
	prepare_regbox_for_next();
	regprpl = p;
	(*regprpl->draw_new_user)(regbox);
}

static void do_reg(gpointer a, gpointer b)
{
	if (regprpl->do_new_user)
		(*regprpl->do_new_user)();
}

void register_user(gpointer a, gpointer b)
{
	GSList *pr = protocols;
	struct prpl *p = NULL, *q;
	GtkWidget *box;
	GtkWidget *frame;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *optmenu;
	GtkWidget *menu;
	GtkWidget *opt;
	GtkWidget *button;

	if (regdialog)
		return;

	while (pr) {
		p = pr->data;
		if (p->draw_new_user)
			break;
		pr = pr->next;
		p = NULL;
	}
	if (p == NULL)
		/* this should never happen because I said so. Hi mom. */
		return;
	pr = protocols;

	regdialog = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_wmclass(GTK_WINDOW(regdialog), "registration", "Gaim");
	gtk_container_set_border_width(GTK_CONTAINER(regdialog), 5);
	gtk_window_set_title(GTK_WINDOW(regdialog), _("Gaim - New User Registration"));
	gtk_signal_connect(GTK_OBJECT(regdialog), "destroy", GTK_SIGNAL_FUNC(delete_reg), NULL);
	gtk_widget_realize(regdialog);
	aol_icon(regdialog->window);

	box = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(regdialog), box);
	gtk_widget_show(box);

	frame = gtk_frame_new(_("New User Registration"));
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, FALSE, 5);
	gtk_widget_show(frame);

	vbox = gtk_vbox_new(FALSE, 5);
	gtk_container_add(GTK_CONTAINER(frame), vbox);
	gtk_widget_show(vbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	label = gtk_label_new(_("Register new user for"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 5);
	gtk_widget_show(label);

	optmenu = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(hbox), optmenu, FALSE, FALSE, 5);
	gtk_widget_show(optmenu);

	menu = gtk_menu_new();

	while (pr) {
		q = pr->data;
		if (q->draw_new_user) {
			opt = gtk_menu_item_new_with_label((*q->name)());
			gtk_signal_connect(GTK_OBJECT(opt), "activate", GTK_SIGNAL_FUNC(reg_prpl), q);
			gtk_menu_append(GTK_MENU(menu), opt);
			gtk_widget_show(opt);
		}
		pr = pr->next;
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), 0);
	regprpl = p;

	regbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), regbox, FALSE, FALSE, 5);
	gtk_widget_show(regbox);

	(*regprpl->draw_new_user)(regbox);

	hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_end(GTK_BOX(box), hbox, FALSE, FALSE, 5);
	gtk_widget_show(hbox);

	button = picture_button(regdialog, _("Close"), cancel_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 5);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(delete_reg), NULL);

	button = picture_button(regdialog, _("Register"), register_xpm);
	gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 5);
	gtk_signal_connect(GTK_OBJECT(button), "clicked", GTK_SIGNAL_FUNC(do_reg), NULL);

	gtk_widget_show(regdialog);
}

static void proto_act(GtkObject *obj, struct gaim_connection *gc)
{
	char *act = gtk_object_get_user_data(obj);
	(*gc->prpl->do_action)(gc, act);
}

void do_proto_menu()
{
	GtkWidget *menuitem;
	GtkWidget *submenu;
	GList *l;
	GSList *c = connections;
	struct gaim_connection *gc = NULL;
	int count = 0;
	char buf[256];

	if (!protomenu)
		return;

	l = gtk_container_children(GTK_CONTAINER(protomenu));
	while (l) {
		gtk_container_remove(GTK_CONTAINER(protomenu), GTK_WIDGET(l->data));
		l = l->next;
	}

	while (c) {
		gc = c->data;
		if (gc->prpl->actions && gc->prpl->do_action)
			count++;
		c = g_slist_next(c);
	}
	c = connections;

	if (!count) {
		g_snprintf(buf, sizeof(buf), "No actions available");
		menuitem = gtk_menu_item_new_with_label(buf);
		gtk_menu_append(GTK_MENU(protomenu), menuitem);
		gtk_widget_show(menuitem);
		return;
	}

	if (count == 1) {
		GList *tmp, *act;
		while (c) {
			gc = c->data;
			if (gc->prpl->actions && gc->prpl->do_action)
				break;
			c = g_slist_next(c);
		}

		tmp = act = (*gc->prpl->actions)();

		while (act) {
			if (act->data == NULL) {
				gaim_separator(protomenu);
				act = g_list_next(act);
				continue;
			}

			menuitem = gtk_menu_item_new_with_label(act->data);
			gtk_object_set_user_data(GTK_OBJECT(menuitem), act->data);
			gtk_menu_append(GTK_MENU(protomenu), menuitem);
			gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
					   GTK_SIGNAL_FUNC(proto_act), gc);
			gtk_widget_show(menuitem);

			act = g_list_next(act);
		}

		g_list_free(tmp);
	} else {
		while (c) {
			GList *tmp, *act;
			gc = c->data;
			if (!gc->prpl->actions || !gc->prpl->do_action) {
				c = g_slist_next(c);
				continue;
			}

			g_snprintf(buf, sizeof(buf), "%s (%s)", gc->username, (*gc->prpl->name)());
			menuitem = gtk_menu_item_new_with_label(buf);
			gtk_menu_append(GTK_MENU(protomenu), menuitem);
			gtk_widget_show(menuitem);

			submenu = gtk_menu_new();
			gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem), submenu);
			gtk_widget_show(submenu);

			tmp = act = (*gc->prpl->actions)();

			while (act) {
				if (act->data == NULL) {
					gaim_separator(submenu);
					act = g_list_next(act);
					continue;
				}

				menuitem = gtk_menu_item_new_with_label(act->data);
				gtk_object_set_user_data(GTK_OBJECT(menuitem), act->data);
				gtk_menu_append(GTK_MENU(submenu), menuitem);
				gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
						   GTK_SIGNAL_FUNC(proto_act), gc);
				gtk_widget_show(menuitem);

				act = g_list_next(act);
			}

			g_list_free(tmp);
			c = g_slist_next(c);
		}
	}
}

static void des_email_win(GtkWidget *w, struct gaim_connection *yd) {
	gtk_widget_destroy(yd->email_win);
	if (yd->email_win == w)
		yd->email_win = NULL;
	yd->email_label = NULL;
}

void connection_has_mail(struct gaim_connection *gc, int count, const char *from, const char *subject)
{
	char buf[2048];

	if (!(gc->user->options & OPT_USR_MAIL_CHECK))
		return;

	if (count < 0 && from && subject) {
		g_snprintf(buf, sizeof buf, "%s has mail from %s: %s",
				gc->username, from, subject);
		if (!gc->email_win) {
			GtkWidget *close;

			gc->email_win = gtk_dialog_new();
			gtk_window_set_policy(GTK_WINDOW(gc->email_win), 0, 0, 1);
			gtk_container_set_border_width(GTK_CONTAINER(gc->email_win), 5);
			gtk_window_set_title(GTK_WINDOW(gc->email_win), "New Mail");
			gtk_signal_connect(GTK_OBJECT(gc->email_win), "destroy",
					   GTK_SIGNAL_FUNC(des_email_win), gc);
			gtk_widget_realize(gc->email_win);
			aol_icon(gc->email_win->window);

			gc->email_label = gtk_label_new(buf);
			gtk_box_pack_start(GTK_BOX(GTK_DIALOG(gc->email_win)->vbox),
					gc->email_label, 0, 0, 5);
			gtk_widget_show(gc->email_label);

			close = picture_button(gc->email_win, _("Close"), cancel_xpm);
			gtk_box_pack_start(GTK_BOX(GTK_DIALOG(gc->email_win)->action_area),
					close, 0, 0, 5);
			gtk_signal_connect(GTK_OBJECT(close), "clicked",                      
					   GTK_SIGNAL_FUNC(des_email_win), gc);

			gtk_widget_show(gc->email_win);
		}
		gtk_label_set_text(GTK_LABEL(gc->email_label), buf);
	} else if (count) {
		g_snprintf(buf, sizeof buf, "%s has %d new message%s.",
				gc->username, count, count == 1 ? "" : "s");
		if (!gc->email_win) {
			GtkWidget *close;

			gc->email_win = gtk_dialog_new();
			gtk_window_set_policy(GTK_WINDOW(gc->email_win), 0, 0, 1);
			gtk_container_set_border_width(GTK_CONTAINER(gc->email_win), 5);
			gtk_window_set_title(GTK_WINDOW(gc->email_win), "New Mail");
			gtk_signal_connect(GTK_OBJECT(gc->email_win), "destroy",
					   GTK_SIGNAL_FUNC(des_email_win), gc);
			gtk_widget_realize(gc->email_win);
			aol_icon(gc->email_win->window);

			gc->email_label = gtk_label_new(buf);
			gtk_box_pack_start(GTK_BOX(GTK_DIALOG(gc->email_win)->vbox),
					gc->email_label, 0, 0, 5);
			gtk_widget_show(gc->email_label);

			close = picture_button(gc->email_win, _("Close"), cancel_xpm);
			gtk_box_pack_start(GTK_BOX(GTK_DIALOG(gc->email_win)->action_area),
					close, 0, 0, 5);
			gtk_signal_connect(GTK_OBJECT(close), "clicked",                      
					   GTK_SIGNAL_FUNC(des_email_win), gc);

			gtk_widget_show(gc->email_win);
		}
		gtk_label_set_text(GTK_LABEL(gc->email_label), buf);
	} else if (gc->email_win)
		gtk_widget_destroy(gc->email_win);
}

/*
struct ft_req {
	struct gaim_connection *gc;
	gboolean send;
	gboolean multiple;
	guint size;
	char *name;
	ft_callback cb;
	gpointer data;
	GtkWidget *fd;
};

static void ftrrno(gpointer w, struct ft_req *fr)
{
	if (fr->fd && (w != fr->fd)) {
		gtk_widget_destroy(fr->fd);
		return;
	}
	if (fr->cb)
		fr->cb(fr->gc, NULL, TRUE, fr->data);
	if (fr->name)
		g_free(fr->name);
	g_free(fr);
}

static void do_exist_dialog(const char *name, unsigned long size, struct ft_req *fr)
{
	\*
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *button;
	char buf[8192];

	g_snprintf(buf, sizeof(buf), "It appears that %s already exists. Do you want to "
			"overwrite the file%s or cancel the transfer?", name,
			(size <= fr->size) ? ", resume the download," : "");

	window = gtk_window_new(GTK_WINDOW_DIALOG);
	*\
}

static void ftgotfile(gpointer w, struct ft_req *fr)
{
	const char *fname = gtk_file_selection_get_filename(GTK_FILE_SELECTION(fr->fd));
	if (!fr->multiple && file_is_dir(fname, fr->fd))
		return;

	if (!fr->multiple && !fr->send) {
		struct stat st;
		if (stat(fname, &st) == 0) {
			do_exist_dialog(fname, st.st_size, fr);
			return;
		}
	}

	fr->cb(fr->gc, fname, FT_EXIST_DNE, fr->data);
	fr->cb = NULL;

	gtk_widget_destroy(fr->fd);
}

static void ftrrok(gpointer w, struct ft_req *ft)
{
	\* ft is going to be free'd as soon as we leave this function, so we'll copy it *\
	struct ft_req *fr = g_memdup(ft, sizeof(struct ft_req));
	char buf[256];

	if (fr->send)
		fr->fd = gtk_file_selection_new(_("Gaim - Select File"));
	else
		fr->fd = gtk_file_selection_new(_("Gaim - Send File"));

	g_snprintf(buf, sizeof(buf), "%s/%s", g_get_home_dir(), fr->name ? fr->name : "");
	gtk_file_selection_set_filename(GTK_FILE_SELECTION(fr->fd), buf);

	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fr->fd)->ok_button), "clicked",
			   GTK_SIGNAL_FUNC(ftgotfile), fr);
	gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(fr->fd)->cancel_button), "clicked",
			   GTK_SIGNAL_FUNC(ftrrno), fr);
	gtk_signal_connect(GTK_OBJECT(fr->fd), "destroy", GTK_SIGNAL_FUNC(ftrrno), fr);

	gtk_widget_show(fr->fd);
}

void ft_receive_request(struct gaim_connection *gc, const char *who, gboolean send, gboolean multiple,
		char *name, guint size, ft_callback cb, gpointer data)
{
	char buf[8192];
	struct ft_req *fr = g_new0(struct ft_req, 1);

	fr->gc = gc;
	fr->send = send;
	fr->multiple = multiple;
	fr->size = size;
	if (name)
		fr->name = g_strdup(name);
	fr->cb = cb;
	fr->data = data;

	if (send)
		g_snprintf(buf, sizeof(buf), "%s has just asked %s to send a file.",
				who, gc->username);
	else if (multiple)
		g_snprintf(buf, sizeof(buf), "%s has just asked %s to receive some files.",
				who, gc->username);
	else if (name && size)
		g_snprintf(buf, sizeof(buf), "%s has just asked %s to receive %s (%d bytes).",
				who, gc->username, name, size);
	else if (name)
		g_snprintf(buf, sizeof(buf), "%s has just asked %s to receive %s.",
				who, gc->username, name);
	else
		g_snprintf(buf, sizeof(buf), "%s has just asked %s to receive a file.",
				who, gc->username);

	do_ask_dialog(buf, fr, ftrrok, ftrrno);
}
*/
