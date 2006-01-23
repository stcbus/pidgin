/*
 * @file gtkstatusbox.c GTK+ Status Selection Widget
 * @ingroup gtkui
 *
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
 */

#include <gdk/gdkkeysyms.h>

#include "account.h"
#include "internal.h"
#include "savedstatuses.h"
#include "status.h"
#include "debug.h"

#include "gtkgaim.h"
#include "gtksavedstatuses.h"
#include "gtkstock.h"
#include "gtkstatusbox.h"
#include "gtkutils.h"

#ifdef USE_GTKSPELL
#  include <gtkspell/gtkspell.h>
#  ifdef _WIN32
#    include "wspell.h"
#  endif
#endif

#define TYPING_TIMEOUT 4000

static void imhtml_changed_cb(GtkTextBuffer *buffer, void *data);
static void imhtml_format_changed_cb(GtkIMHtml *imhtml, GtkIMHtmlButtons buttons, void *data);
static void remove_typing_cb(GtkGaimStatusBox *box);
static void update_size (GtkGaimStatusBox *box);
static gint get_statusbox_index(GtkGaimStatusBox *box, GaimSavedStatus *saved_status);

static void gtk_gaim_status_box_pulse_typing(GtkGaimStatusBox *status_box);
static void gtk_gaim_status_box_refresh(GtkGaimStatusBox *status_box);
static void gtk_gaim_status_box_regenerate(GtkGaimStatusBox *status_box);
static void gtk_gaim_status_box_changed(GtkComboBox *box);
static void gtk_gaim_status_box_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void gtk_gaim_status_box_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static gboolean gtk_gaim_status_box_expose_event (GtkWidget *widget, GdkEventExpose *event);
static void gtk_gaim_status_box_forall (GtkContainer *container, gboolean include_internals, GtkCallback callback, gpointer callback_data);

static void (*combo_box_size_request)(GtkWidget *widget, GtkRequisition *requisition);
static void (*combo_box_size_allocate)(GtkWidget *widget, GtkAllocation *allocation);
static void (*combo_box_forall) (GtkContainer *container, gboolean include_internals, GtkCallback callback, gpointer callback_data);

enum {
	TYPE_COLUMN,  /* A GtkGaimStatusBoxItemType */
	ICON_COLUMN,  /* This is a GdkPixbuf (the other columns are strings) */
	TEXT_COLUMN,  /* A string */
	TITLE_COLUMN, /* The plain-English title of this item */
	DESC_COLUMN,  /* A plain-English description of this item */
	DATA_COLUMN,  /* Keep track of the creation time of popular
						statuses, and also the GaimStatusPrimitive */
	NUM_COLUMNS
};

enum {
	PROP_0,
	PROP_ACCOUNT
};

GtkComboBoxClass *parent_class = NULL;

static void gtk_gaim_status_box_class_init (GtkGaimStatusBoxClass *klass);
static void gtk_gaim_status_box_init (GtkGaimStatusBox *status_box);

GType
gtk_gaim_status_box_get_type (void)
{
	static GType status_box_type = 0;

	if (!status_box_type)
	{
		static const GTypeInfo status_box_info =
		{
			sizeof (GtkGaimStatusBoxClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) gtk_gaim_status_box_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (GtkGaimStatusBox),
			0,
			(GInstanceInitFunc) gtk_gaim_status_box_init,
			NULL  /* value_table */
		};

		status_box_type = g_type_register_static(GTK_TYPE_COMBO_BOX,
												 "GtkGaimStatusBox",
												 &status_box_info,
												 0);
	}

	return status_box_type;
}

static void
gtk_gaim_status_box_get_property(GObject *object, guint param_id,
                                 GValue *value, GParamSpec *psec)
{
	GtkGaimStatusBox *statusbox = GTK_GAIM_STATUS_BOX(object);

	switch (param_id) {
	case PROP_ACCOUNT:
		g_value_set_pointer(value, statusbox->account);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, psec);
		break;
	}
}

static void
update_to_reflect_account_status(GtkGaimStatusBox *status_box, GaimAccount *account, GaimStatus *newstatus)
{
	const GList *l;
	int status_no = -1;
	const GaimStatusType *statustype = NULL;
	const char *message;

	statustype = gaim_status_type_find_with_id((GList *)gaim_account_get_status_types(account),
	                                           (char *)gaim_status_type_get_id(gaim_status_get_type(newstatus)));

	for (l = gaim_account_get_status_types(account); l != NULL; l = l->next) {
		GaimStatusType *status_type = (GaimStatusType *)l->data;

		if (!gaim_status_type_is_user_settable(status_type))
			continue;
		status_no++;
		if (statustype == status_type)
			break;
	}

	if (status_no != -1) {
		gtk_widget_set_sensitive(GTK_WIDGET(status_box), FALSE);
		gtk_combo_box_set_active(GTK_COMBO_BOX(status_box), status_no);

		message = gaim_status_get_attr_string(newstatus, "message");

		if (!message || !*message)
		{
			gtk_widget_hide_all(status_box->vbox);
			status_box->imhtml_visible = FALSE;
		}
		else
		{
			gtk_widget_show_all(status_box->vbox);
			status_box->imhtml_visible = TRUE;
			gtk_imhtml_clear(GTK_IMHTML(status_box->imhtml));
			gtk_imhtml_clear_formatting(GTK_IMHTML(status_box->imhtml));
			gtk_imhtml_append_text(GTK_IMHTML(status_box->imhtml), message, 0);
		}
		gtk_widget_set_sensitive(GTK_WIDGET(status_box), TRUE);
		gtk_gaim_status_box_refresh(status_box);
	}
}

static void
account_status_changed_cb(GaimAccount *account, GaimStatus *oldstatus, GaimStatus *newstatus, GtkGaimStatusBox *status_box)
{
	if (status_box->account == account)
		update_to_reflect_account_status(status_box, account, newstatus);
}

static void
gtk_gaim_status_box_set_property(GObject *object, guint param_id,
                                 const GValue *value, GParamSpec *pspec)
{
	GtkGaimStatusBox *statusbox = GTK_GAIM_STATUS_BOX(object);

	switch (param_id) {
	case PROP_ACCOUNT:
		statusbox->account = g_value_get_pointer(value);

		if (statusbox->status_changed_signal) {
			gaim_signal_disconnect(gaim_accounts_get_handle(), "account-status-changed",
			                        statusbox, GAIM_CALLBACK(account_status_changed_cb));
			statusbox->status_changed_signal = 0;
		}

		if (statusbox->account)
			statusbox->status_changed_signal = gaim_signal_connect(gaim_accounts_get_handle(), "account-status-changed",
			                                                       statusbox, GAIM_CALLBACK(account_status_changed_cb),
			                                                       statusbox);
		gtk_gaim_status_box_regenerate(statusbox);

		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, param_id, pspec);
		break;
	}
}

static void
gtk_gaim_status_box_finalize(GObject *obj)
{
	GtkGaimStatusBox *statusbox = GTK_GAIM_STATUS_BOX(obj);

	if (statusbox->status_changed_signal) {
		gaim_signal_disconnect(gaim_accounts_get_handle(), "account-status-changed",
								statusbox, GAIM_CALLBACK(account_status_changed_cb));
		statusbox->status_changed_signal = 0;
	}
	gaim_prefs_disconnect_by_handle(statusbox);

	G_OBJECT_CLASS(parent_class)->finalize(obj);
}

static void
gtk_gaim_status_box_class_init (GtkGaimStatusBoxClass *klass)
{
	GObjectClass *object_class;
	GtkComboBoxClass *combo_class;
	GtkWidgetClass *widget_class;
	GtkContainerClass *container_class = (GtkContainerClass*)klass;

	parent_class = g_type_class_peek_parent(klass);

	combo_class = (GtkComboBoxClass*)klass;
	combo_class->changed = gtk_gaim_status_box_changed;

	widget_class = (GtkWidgetClass*)klass;
	combo_box_size_request = widget_class->size_request;
	widget_class->size_request = gtk_gaim_status_box_size_request;
	combo_box_size_allocate = widget_class->size_allocate;
	widget_class->size_allocate = gtk_gaim_status_box_size_allocate;
	widget_class->expose_event = gtk_gaim_status_box_expose_event;

	combo_box_forall = container_class->forall;
	container_class->forall = gtk_gaim_status_box_forall;

	object_class = (GObjectClass *)klass;

	object_class->finalize = gtk_gaim_status_box_finalize;

	object_class->get_property = gtk_gaim_status_box_get_property;
	object_class->set_property = gtk_gaim_status_box_set_property;

	g_object_class_install_property(object_class,
	                                PROP_ACCOUNT,
	                                g_param_spec_pointer("account",
	                                                     "Account",
	                                                     "The account, or NULL for all accounts",
	                                                      G_PARAM_READWRITE
	                                                     )
	                               );
}

static void
gtk_gaim_status_box_refresh(GtkGaimStatusBox *status_box)
{
	char *text = NULL, *title;
	char aa_color[8];
	GdkPixbuf *pixbuf;
	GtkTreePath *path;
	GtkStyle *style;

	style = gtk_widget_get_style(GTK_WIDGET(status_box));
	snprintf(aa_color, sizeof(aa_color), "#%02x%02x%02x",
		 style->text_aa[GTK_STATE_NORMAL].red >> 8,
		 style->text_aa[GTK_STATE_NORMAL].green >> 8,
		 style->text_aa[GTK_STATE_NORMAL].blue >> 8);

	title = status_box->title;
	if (!title)
		title = "";

	if (status_box->error) {
		text = g_strdup_printf("<span size=\"smaller\" weight=\"bold\" color=\"red\">%s</span>",
							   status_box->error);
	} else if (status_box->typing) {
		text = g_strdup_printf("<span size=\"smaller\" color=\"%s\">%s</span>",
							   aa_color, _("Typing"));
	} else if (status_box->connecting) {
		text = g_strdup_printf("<span size=\"smaller\" color=\"%s\">%s</span>",
							   aa_color, _("Connecting"));
	} else if (status_box->desc) {
		text = g_strdup_printf("<span size=\"smaller\" color=\"%s\">%s</span>",
							   aa_color, status_box->desc);
	}

	if (status_box->account) {
		char *text2 = g_strdup_printf("%s\n<span size=\"smaller\">%s</span>",
						gaim_account_get_username(status_box->account),
						text ? text : title);
		g_free(text);
		text = text2;
	} else if (text) {
		char *text2 = g_strdup_printf("%s\n%s", title, text);
		g_free(text);
		text = text2;
	} else {
		text = g_strdup(title);
	}

	if (status_box->connecting)
		pixbuf = status_box->connecting_pixbufs[status_box->connecting_index];
	else if (status_box->error)
		pixbuf = status_box->error_pixbuf;
	else if (status_box->typing)
		pixbuf = status_box->typing_pixbufs[status_box->typing_index];
	else
		pixbuf = status_box->pixbuf;

	gtk_list_store_set(status_box->store, &(status_box->iter),
			   TYPE_COLUMN, -1, /* This field is not used in this list store */
			   ICON_COLUMN, pixbuf,
			   TEXT_COLUMN, text,
			   TITLE_COLUMN, title,
			   DESC_COLUMN, status_box->desc,
			   DATA_COLUMN, -1, /* This field is not used in this list store */
			   -1);
	path = gtk_tree_path_new_from_string("0");
	gtk_cell_view_set_displayed_row(GTK_CELL_VIEW(status_box->cell_view), path);
	gtk_tree_path_free(path);

	g_free(text);
	update_size(status_box);
}

/**
 * This updates the GtkTreeView so that it correctly shows the state
 * we are currently using.  It is used when the current state is
 * updated from somewhere other than the GtkStatusBox (from a plugin,
 * or when signing on with the "-n" option, for example).  It is
 * also used when the user selects the "Custom..." option.
 *
 * Maybe we could accomplish this by triggering off the mouse and
 * keyboard signals instead of the changed signal?
 */
static void
update_to_reflect_current_status(GtkGaimStatusBox *status_box)
{
	GaimSavedStatus *saved_status;
	gint index;
	const char *message;
	GList *list;

	/* this function is inappropriate for ones with accounts */
	if (status_box->account)
		return;

	saved_status = gaim_savedstatus_get_current();

	/*
	 * Suppress the "changed" signal because the status
	 * was changed programmatically.
	 */
	gtk_widget_set_sensitive(GTK_WIDGET(status_box), FALSE);

	list = gaim_savedstatuses_get_popular(6);
	if ((index = g_list_index(list, saved_status)) > -1)
		index += 5;
	else
	{
		if (gaim_savedstatus_has_substatuses(saved_status))
			index = GTK_LIST_STORE(status_box->dropdown_store)->length - 2;
		else
		{
			index = get_statusbox_index(status_box, saved_status);
		}
	}
	g_list_free(list);
	gtk_combo_box_set_active(GTK_COMBO_BOX(status_box), index);

	message = gaim_savedstatus_get_message(saved_status);
	if (!message || !*message)
	{
		status_box->imhtml_visible = FALSE;
		gtk_widget_hide_all(status_box->vbox);
	}
	else
	{
		status_box->imhtml_visible = TRUE;
		gtk_widget_show_all(status_box->vbox);

		/*
		 * Suppress the "changed" signal because the status
		 * was changed programmatically.
		 */
		gtk_widget_set_sensitive(GTK_WIDGET(status_box->imhtml), FALSE);

		gtk_imhtml_clear(GTK_IMHTML(status_box->imhtml));
		gtk_imhtml_clear_formatting(GTK_IMHTML(status_box->imhtml));
		gtk_imhtml_append_text(GTK_IMHTML(status_box->imhtml), message, 0);
		gtk_widget_set_sensitive(GTK_WIDGET(status_box->imhtml), TRUE);
	}

	update_size(status_box);

	/* Stop suppressing the "changed" signal. */
	gtk_widget_set_sensitive(GTK_WIDGET(status_box), TRUE);
}

static void
add_popular_statuses(GtkGaimStatusBox *statusbox)
{
	GList *list, *cur;
	GtkIconSize icon_size;
	GdkPixbuf *away_pix, *avl_pix, *pixbuf;

	list = gaim_savedstatuses_get_popular(6);
	if (list == NULL)
		/* Odd... oh well, nothing we can do about it. */
		return;

	if (gaim_prefs_get_bool("/gaim/gtk/blist/show_buddy_icons"))
		icon_size = gtk_icon_size_from_name(GAIM_ICON_SIZE_STATUS);
	else
		icon_size = gtk_icon_size_from_name(GAIM_ICON_SIZE_STATUS_SMALL);

	away_pix = gtk_widget_render_icon(GTK_WIDGET(statusbox->vbox), GAIM_STOCK_STATUS_AWAY,
									icon_size, "GtkGaimStatusBox");
	avl_pix = gtk_widget_render_icon(GTK_WIDGET(statusbox->vbox), GAIM_STOCK_STATUS_ONLINE,
									icon_size, "GtkGaimStatusBox");

	gtk_gaim_status_box_add_separator(statusbox);

	for (cur = list; cur != NULL; cur = cur->next)
	{
		GaimSavedStatus *saved = cur->data;

		switch (gaim_savedstatus_get_type(saved))
		{
			case GAIM_STATUS_AVAILABLE:
				pixbuf = avl_pix;
				break;
			default:
				pixbuf = away_pix;
				break;
		}
		gtk_gaim_status_box_add(statusbox, GTK_GAIM_STATUS_BOX_TYPE_POPULAR,
				pixbuf,	gaim_savedstatus_get_title(saved), NULL,
				GINT_TO_POINTER(gaim_savedstatus_get_creation_time(saved)));
	}

	g_list_free(list);
}

static void
gtk_gaim_status_box_regenerate(GtkGaimStatusBox *status_box)
{
	GaimAccount *account;
	GdkPixbuf *pixbuf, *pixbuf2, *pixbuf3, *pixbuf4;
	GtkIconSize icon_size;

	if (gaim_prefs_get_bool("/gaim/gtk/blist/show_buddy_icons"))
		icon_size = gtk_icon_size_from_name(GAIM_ICON_SIZE_STATUS);
	else
		icon_size = gtk_icon_size_from_name(GAIM_ICON_SIZE_STATUS_SMALL);

	/* Unset the model while clearing it */
	gtk_combo_box_set_model(GTK_COMBO_BOX(status_box), NULL);
	gtk_list_store_clear(status_box->dropdown_store);
	gtk_combo_box_set_model(GTK_COMBO_BOX(status_box), GTK_TREE_MODEL(status_box->dropdown_store));

	account = GTK_GAIM_STATUS_BOX(status_box)->account;
	if (account == NULL)
	{
		/* Global */
		pixbuf = gtk_widget_render_icon (GTK_WIDGET(status_box->vbox), GAIM_STOCK_STATUS_ONLINE,
		                                 icon_size, "GtkGaimStatusBox");
		pixbuf2 = gtk_widget_render_icon (GTK_WIDGET(status_box->vbox), GAIM_STOCK_STATUS_AWAY,
		                                  icon_size, "GtkGaimStatusBox");
		pixbuf3 = gtk_widget_render_icon (GTK_WIDGET(status_box->vbox), GAIM_STOCK_STATUS_OFFLINE,
		                                  icon_size, "GtkGaimStatusBox");
		pixbuf4 = gtk_widget_render_icon (GTK_WIDGET(status_box->vbox), GAIM_STOCK_STATUS_INVISIBLE,
		                                  icon_size, "GtkGaimStatusBox");

		gtk_gaim_status_box_add(GTK_GAIM_STATUS_BOX(status_box), GTK_GAIM_STATUS_BOX_TYPE_PRIMITIVE, pixbuf, _("Available"), NULL, GINT_TO_POINTER(GAIM_STATUS_AVAILABLE));
		gtk_gaim_status_box_add(GTK_GAIM_STATUS_BOX(status_box), GTK_GAIM_STATUS_BOX_TYPE_PRIMITIVE, pixbuf2, _("Away"), NULL, GINT_TO_POINTER(GAIM_STATUS_AWAY));
		gtk_gaim_status_box_add(GTK_GAIM_STATUS_BOX(status_box), GTK_GAIM_STATUS_BOX_TYPE_PRIMITIVE, pixbuf4, _("Invisible"), NULL, GINT_TO_POINTER(GAIM_STATUS_INVISIBLE));
		gtk_gaim_status_box_add(GTK_GAIM_STATUS_BOX(status_box), GTK_GAIM_STATUS_BOX_TYPE_PRIMITIVE, pixbuf3, _("Offline"), NULL, GINT_TO_POINTER(GAIM_STATUS_OFFLINE));

		add_popular_statuses(status_box);

		gtk_gaim_status_box_add_separator(GTK_GAIM_STATUS_BOX(status_box));
		gtk_gaim_status_box_add(GTK_GAIM_STATUS_BOX(status_box), GTK_GAIM_STATUS_BOX_TYPE_CUSTOM, pixbuf, _("Custom..."), NULL, NULL);
		gtk_gaim_status_box_add(GTK_GAIM_STATUS_BOX(status_box), GTK_GAIM_STATUS_BOX_TYPE_SAVED, pixbuf, _("Saved..."), NULL, NULL);

		update_to_reflect_current_status(status_box);

	} else {
		/* Per-account */
		const GList *l;

		for (l = gaim_account_get_status_types(account); l != NULL; l = l->next)
		{
			GaimStatusType *status_type = (GaimStatusType *)l->data;

			if (!gaim_status_type_is_user_settable(status_type))
				continue;

			gtk_gaim_status_box_add(GTK_GAIM_STATUS_BOX(status_box),
									gaim_status_type_get_primitive(status_type),
									gaim_gtk_create_prpl_icon_with_status(account, status_type),
									gaim_status_type_get_name(status_type),
									NULL, NULL);
		}

		update_to_reflect_account_status(status_box, account, gaim_account_get_active_status(account));
	}
}

static gboolean combo_box_scroll_event_cb(GtkWidget *w, GdkEventScroll *event, GtkIMHtml *imhtml)
{
	gtk_combo_box_popup(GTK_COMBO_BOX(w));
	return TRUE;
}

static gboolean imhtml_scroll_event_cb(GtkWidget *w, GdkEventScroll *event, GtkIMHtml *imhtml)
{
	if (event->direction == GDK_SCROLL_UP)
		gtk_imhtml_page_up(imhtml);
	else if (event->direction == GDK_SCROLL_DOWN)
		gtk_imhtml_page_down(imhtml);
	return TRUE;
}

static int imhtml_remove_focus(GtkWidget *w, GdkEventKey *event, GtkGaimStatusBox *box)
{
	if (event->keyval == GDK_Tab || event->keyval == GDK_KP_Tab)
	{
		/* If last inserted character is a tab, then remove the focus from here */
		GtkWidget *top = gtk_widget_get_toplevel(w);
		g_signal_emit_by_name(G_OBJECT(top), "move_focus",
				(event->state & GDK_SHIFT_MASK) ?
				                  GTK_DIR_TAB_BACKWARD: GTK_DIR_TAB_FORWARD);
		return TRUE;
	}
	if (!box->typing)
		return FALSE;
	gtk_gaim_status_box_pulse_typing(box);
	g_source_remove(box->typing);
	box->typing = g_timeout_add(TYPING_TIMEOUT, (GSourceFunc)remove_typing_cb, box);

	return FALSE;
}

#if GTK_CHECK_VERSION(2,6,0)
static gboolean
dropdown_store_row_separator_func(GtkTreeModel *model,
								  GtkTreeIter *iter, gpointer data)
{
	GtkGaimStatusBoxItemType type;

	gtk_tree_model_get(model, iter, TYPE_COLUMN, &type, -1);

	if (type == GTK_GAIM_STATUS_BOX_TYPE_SEPARATOR)
		return TRUE;

	return FALSE;
}
#endif

static void
cache_pixbufs(GtkGaimStatusBox *status_box)
{
	GtkIconSize icon_size;

	if (gaim_prefs_get_bool("/gaim/gtk/blist/show_buddy_icons"))
		icon_size = gtk_icon_size_from_name(GAIM_ICON_SIZE_STATUS_TWO_LINE);
	else
		icon_size = gtk_icon_size_from_name(GAIM_ICON_SIZE_STATUS_SMALL_TWO_LINE);

	if (status_box->error_pixbuf != NULL)
		gdk_pixbuf_unref(status_box->error_pixbuf);

	status_box->error_pixbuf = gtk_widget_render_icon (GTK_WIDGET(status_box->vbox), GAIM_STOCK_STATUS_OFFLINE,
							   icon_size, "GtkGaimStatusBox");
	if (status_box->connecting_pixbufs[0] != NULL)
		gdk_pixbuf_unref(status_box->connecting_pixbufs[0]);
	if (status_box->connecting_pixbufs[1] != NULL)
		gdk_pixbuf_unref(status_box->connecting_pixbufs[1]);
	if (status_box->connecting_pixbufs[2] != NULL)
		gdk_pixbuf_unref(status_box->connecting_pixbufs[2]);
	if (status_box->connecting_pixbufs[3] != NULL)
		gdk_pixbuf_unref(status_box->connecting_pixbufs[3]);

	status_box->connecting_index = 0;
	status_box->connecting_pixbufs[0] = gtk_widget_render_icon (GTK_WIDGET(status_box->vbox), GAIM_STOCK_STATUS_CONNECT0,
								     icon_size, "GtkGaimStatusBox");
	status_box->connecting_pixbufs[1] = gtk_widget_render_icon (GTK_WIDGET(status_box->vbox), GAIM_STOCK_STATUS_CONNECT1,
								     icon_size, "GtkGaimStatusBox");
	status_box->connecting_pixbufs[2] = gtk_widget_render_icon (GTK_WIDGET(status_box->vbox), GAIM_STOCK_STATUS_CONNECT2,
								     icon_size, "GtkGaimStatusBox");
	status_box->connecting_pixbufs[3] = gtk_widget_render_icon (GTK_WIDGET(status_box->vbox), GAIM_STOCK_STATUS_CONNECT3,
								     icon_size, "GtkGaimStatusBox");

	if (status_box->typing_pixbufs[0] != NULL)
		gdk_pixbuf_unref(status_box->typing_pixbufs[0]);
	if (status_box->typing_pixbufs[1] != NULL)
		gdk_pixbuf_unref(status_box->typing_pixbufs[1]);
	if (status_box->typing_pixbufs[2] != NULL)
		gdk_pixbuf_unref(status_box->typing_pixbufs[2]);
	if (status_box->typing_pixbufs[3] != NULL)
		gdk_pixbuf_unref(status_box->typing_pixbufs[3]);

	status_box->typing_index = 0;
	status_box->typing_pixbufs[0] =  gtk_widget_render_icon (GTK_WIDGET(status_box->vbox), GAIM_STOCK_STATUS_TYPING0,
								     icon_size, "GtkGaimStatusBox");
	status_box->typing_pixbufs[1] =  gtk_widget_render_icon (GTK_WIDGET(status_box->vbox), GAIM_STOCK_STATUS_TYPING1,
								     icon_size, "GtkGaimStatusBox");
	status_box->typing_pixbufs[2] =  gtk_widget_render_icon (GTK_WIDGET(status_box->vbox), GAIM_STOCK_STATUS_TYPING2,
								     icon_size, "GtkGaimStatusBox");
	status_box->typing_pixbufs[3] =  gtk_widget_render_icon (GTK_WIDGET(status_box->vbox), GAIM_STOCK_STATUS_TYPING3,
								     icon_size, "GtkGaimStatusBox");
}

static void
current_status_pref_changed_cb(const char *name, GaimPrefType type,
							   gconstpointer val, gpointer data)
{
	GtkGaimStatusBox *box = data;

	/* Make sure our current status is added to the list of popular statuses */
	gtk_gaim_status_box_regenerate(box);

	if (box->account)
		update_to_reflect_account_status(box, box->account,
						gaim_account_get_active_status(box->account));
	else
		update_to_reflect_current_status(box);
}

static void
buddy_list_details_pref_changed_cb(const char *name, GaimPrefType type,
								   gconstpointer val, gpointer data)
{
	GtkGaimStatusBox *status_box = (GtkGaimStatusBox *)data;

	cache_pixbufs(status_box);
	gtk_gaim_status_box_regenerate(status_box);
	update_size(status_box);
}

static void
spellcheck_prefs_cb(const char *name, GaimPrefType type,
					gconstpointer value, gpointer data)
{
#ifdef USE_GTKSPELL
	GtkGaimStatusBox *status_box = (GtkGaimStatusBox *)data;

	if (value)
		gaim_gtk_setup_gtkspell(GTK_TEXT_VIEW(status_box->imhtml));
	else
	{
		GtkSpell *spell;
		spell = gtkspell_get_from_text_view(GTK_TEXT_VIEW(status_box->imhtml));
		gtkspell_detach(spell);
	}
#endif
}

#if 0
static gboolean button_released_cb(GtkWidget *widget, GdkEventButton *event, GtkGaimStatusBox *box)
{

	if (event->button != 1)
		return FALSE;
	gtk_combo_box_popdown(GTK_COMBO_BOX(box));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(box->toggle_button), FALSE);
	if (!box->imhtml_visible)
		g_signal_emit_by_name(G_OBJECT(box), "changed", NULL, NULL);
	return TRUE;
}

static gboolean button_pressed_cb(GtkWidget *widget, GdkEventButton *event, GtkGaimStatusBox *box)
{
	if (event->button != 1)
		return FALSE;
	gtk_combo_box_popup(GTK_COMBO_BOX(box));
	// Disabled until button_released_cb works
	// gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(box->toggle_button), TRUE);
	return TRUE;
}
#endif

static void
toggled_cb(GtkWidget *widget, GtkGaimStatusBox *box)
{
	gtk_combo_box_popup(GTK_COMBO_BOX(box));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(box->toggle_button), FALSE);
}

static void
gtk_gaim_status_box_init (GtkGaimStatusBox *status_box)
{
	GtkCellRenderer *text_rend;
	GtkCellRenderer *icon_rend;
	GtkTextBuffer *buffer;
	GtkTreePath *path;

	text_rend = gtk_cell_renderer_text_new();
	icon_rend = gtk_cell_renderer_pixbuf_new();

	status_box->imhtml_visible = FALSE;
	status_box->connecting = FALSE;
	status_box->typing = FALSE;
	status_box->title = NULL;
	status_box->pixbuf = NULL;
	status_box->toggle_button = gtk_toggle_button_new();
	status_box->hbox = gtk_hbox_new(FALSE, 6);
	status_box->cell_view = gtk_cell_view_new();
	status_box->vsep = gtk_vseparator_new();
	status_box->arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);

	status_box->store = gtk_list_store_new(NUM_COLUMNS, G_TYPE_INT, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
	status_box->dropdown_store = gtk_list_store_new(NUM_COLUMNS, G_TYPE_INT, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_POINTER);
	gtk_combo_box_set_model(GTK_COMBO_BOX(status_box), GTK_TREE_MODEL(status_box->dropdown_store));
	gtk_cell_view_set_model(GTK_CELL_VIEW(status_box->cell_view), GTK_TREE_MODEL(status_box->store));
	gtk_combo_box_set_wrap_width(GTK_COMBO_BOX(status_box), 0);
	gtk_list_store_append(status_box->store, &(status_box->iter));
	gtk_gaim_status_box_refresh(status_box);
	path = gtk_tree_path_new_from_string("0");
	gtk_cell_view_set_displayed_row(GTK_CELL_VIEW(status_box->cell_view), path);
	gtk_tree_path_free(path);

	gtk_container_add(GTK_CONTAINER(status_box->toggle_button), status_box->hbox);
	gtk_box_pack_start(GTK_BOX(status_box->hbox), status_box->cell_view, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(status_box->hbox), status_box->vsep, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(status_box->hbox), status_box->arrow, FALSE, FALSE, 0);
	gtk_widget_show_all(status_box->toggle_button);
#if GTK_CHECK_VERSION(2,4,0)
	gtk_button_set_focus_on_click(GTK_BUTTON(status_box->toggle_button), FALSE);
#endif
	status_box->icon_rend = gtk_cell_renderer_pixbuf_new();
	status_box->text_rend = gtk_cell_renderer_text_new();

	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(status_box), icon_rend, FALSE);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(status_box), text_rend, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(status_box), icon_rend, "pixbuf", ICON_COLUMN, NULL);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(status_box), text_rend, "markup", TEXT_COLUMN, NULL);

	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(status_box->cell_view), status_box->icon_rend, FALSE);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(status_box->cell_view), status_box->text_rend, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(status_box->cell_view), status_box->icon_rend, "pixbuf", ICON_COLUMN, NULL);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(status_box->cell_view), status_box->text_rend, "markup", TEXT_COLUMN, NULL);

	g_object_set(G_OBJECT(status_box->icon_rend), "xpad", 6, NULL);

	status_box->vbox = gtk_vbox_new(0, FALSE);
	status_box->sw = gaim_gtk_create_imhtml(FALSE, &status_box->imhtml, NULL);
	gtk_imhtml_set_editable(GTK_IMHTML(status_box->imhtml), TRUE);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(status_box->imhtml));
#if 0
	g_signal_connect(G_OBJECT(status_box->toggle_button), "button-press-event",
			 G_CALLBACK(button_pressed_cb), status_box);
	g_signal_connect(G_OBJECT(status_box->toggle_button), "button-release-event",
			 G_CALLBACK(button_released_cb), status_box);
#endif
	g_signal_connect(G_OBJECT(status_box->toggle_button), "toggled",
	                 G_CALLBACK(toggled_cb), status_box);
	g_signal_connect(G_OBJECT(buffer), "changed", G_CALLBACK(imhtml_changed_cb), status_box);
	g_signal_connect(G_OBJECT(status_box->imhtml), "format_function_toggle",
			 G_CALLBACK(imhtml_format_changed_cb), status_box);
	g_signal_connect(G_OBJECT(status_box->imhtml), "key_press_event",
			 G_CALLBACK(imhtml_remove_focus), status_box);
	g_signal_connect_swapped(G_OBJECT(status_box->imhtml), "message_send", G_CALLBACK(remove_typing_cb), status_box);
	gtk_imhtml_set_editable(GTK_IMHTML(status_box->imhtml), TRUE);
#ifdef USE_GTKSPELL
	if (gaim_prefs_get_bool("/gaim/gtk/conversations/spellcheck"))
		gaim_gtk_setup_gtkspell(GTK_TEXT_VIEW(status_box->imhtml));
#endif
	gtk_widget_set_parent(status_box->vbox, GTK_WIDGET(status_box));
	gtk_widget_set_parent(status_box->toggle_button, GTK_WIDGET(status_box));
	GTK_BIN(status_box)->child = status_box->toggle_button;

	gtk_box_pack_start(GTK_BOX(status_box->vbox), status_box->sw, TRUE, TRUE, 0);

	g_signal_connect(G_OBJECT(status_box), "scroll_event", G_CALLBACK(combo_box_scroll_event_cb), NULL);
	g_signal_connect(G_OBJECT(status_box->imhtml), "scroll_event",
					G_CALLBACK(imhtml_scroll_event_cb), status_box->imhtml);

#if GTK_CHECK_VERSION(2,6,0)
	gtk_combo_box_set_row_separator_func(GTK_COMBO_BOX(status_box), dropdown_store_row_separator_func, NULL, NULL);
#endif

	cache_pixbufs(status_box);
	gtk_gaim_status_box_regenerate(status_box);

	gaim_prefs_connect_callback(status_box, "/core/savedstatus/current",
								current_status_pref_changed_cb, status_box);
	gaim_prefs_connect_callback(status_box, "/gaim/gtk/blist/show_buddy_icons",
								buddy_list_details_pref_changed_cb, status_box);
	gaim_prefs_connect_callback(status_box, "/gaim/gtk/conversations/spellcheck",
								spellcheck_prefs_cb, status_box);
}

static void
gtk_gaim_status_box_size_request(GtkWidget *widget,
								 GtkRequisition *requisition)
{
	GtkRequisition box_req;
	combo_box_size_request(widget, requisition);
	requisition->height += 3;

	gtk_widget_size_request(GTK_GAIM_STATUS_BOX(widget)->vbox, &box_req);
	if (box_req.height > 1)
		requisition->height = requisition->height + box_req.height + 3;

	requisition->width = 1;
}

static void
gtk_gaim_status_box_size_allocate(GtkWidget *widget,
								  GtkAllocation *allocation)
{
	GtkRequisition req = {0,0};
	GtkAllocation parent_alc, box_alc;

	parent_alc = *allocation;
	box_alc = *allocation;
	combo_box_size_request(widget, &req);

	box_alc.height = MAX(1, (allocation->height - req.height - 6));
	box_alc.y = box_alc.y + req.height + 6;

	box_alc.width -= 6;
	box_alc.x += 3;

	gtk_widget_size_allocate((GTK_GAIM_STATUS_BOX(widget))->vbox, &box_alc);

	parent_alc.height = MAX(1,req.height);
	parent_alc.width -= 6;
	parent_alc.x += 3;
	parent_alc.y += 3;

	combo_box_size_allocate(widget, &parent_alc);
	gtk_widget_size_allocate((GTK_GAIM_STATUS_BOX(widget))->toggle_button, &parent_alc);
	widget->allocation = *allocation;
}

static gboolean
gtk_gaim_status_box_expose_event(GtkWidget *widget,
				 GdkEventExpose *event)
{
	GtkGaimStatusBox *status_box = GTK_GAIM_STATUS_BOX(widget);
	gtk_container_propagate_expose(GTK_CONTAINER(widget), status_box->vbox, event);
	gtk_container_propagate_expose(GTK_CONTAINER(widget), status_box->toggle_button, event);
	return FALSE;
}

static void
gtk_gaim_status_box_forall(GtkContainer *container,
						   gboolean include_internals,
						   GtkCallback callback,
						   gpointer callback_data)
{
	GtkGaimStatusBox *status_box = GTK_GAIM_STATUS_BOX (container);

	if (include_internals)
	{
		(* callback) (status_box->vbox, callback_data);
		(* callback) (status_box->toggle_button, callback_data);
		(* callback) (status_box->arrow, callback_data);
	}

	combo_box_forall(container, include_internals, callback, callback_data);
}

GtkWidget *
gtk_gaim_status_box_new()
{
	return g_object_new(GTK_GAIM_TYPE_STATUS_BOX, NULL);
}

GtkWidget *
gtk_gaim_status_box_new_with_account(GaimAccount *account)
{
	return g_object_new(GTK_GAIM_TYPE_STATUS_BOX, "account", account, NULL);
}

void
gtk_gaim_status_box_add(GtkGaimStatusBox *status_box, GtkGaimStatusBoxItemType type, GdkPixbuf *pixbuf, const char *text, const char *sec_text, gpointer data)
{
	GtkTreeIter iter;
	char *t;

	if (sec_text) {
		char aa_color[8];
		GtkStyle *style = gtk_widget_get_style(GTK_WIDGET(status_box));
		snprintf(aa_color, sizeof(aa_color), "#%02x%02x%02x",
			 style->text_aa[GTK_STATE_NORMAL].red >> 8,
			 style->text_aa[GTK_STATE_NORMAL].green >> 8,
			 style->text_aa[GTK_STATE_NORMAL].blue >> 8);
		t = g_strdup_printf("%s\n<span color=\"%s\">%s</span>", text, aa_color, sec_text);
	} else {
		t = g_strdup(text);
	}

	gtk_list_store_append(status_box->dropdown_store, &iter);
	gtk_list_store_set(status_box->dropdown_store, &iter,
			   TYPE_COLUMN, type,
			   ICON_COLUMN, pixbuf,
			   TEXT_COLUMN, t,
			   TITLE_COLUMN, text,
			   DESC_COLUMN, sec_text,
			   DATA_COLUMN, data,
			   -1);
	g_free(t);
}

void
gtk_gaim_status_box_add_separator(GtkGaimStatusBox *status_box)
{
	/* Don't do anything unless GTK actually supports
	 * gtk_combo_box_set_row_separator_func */
#if GTK_CHECK_VERSION(2,6,0)
	GtkTreeIter iter;

	gtk_list_store_append(status_box->dropdown_store, &iter);
	gtk_list_store_set(status_box->dropdown_store, &iter,
			   TYPE_COLUMN, GTK_GAIM_STATUS_BOX_TYPE_SEPARATOR,
			   -1);
#endif
}

void
gtk_gaim_status_box_set_error(GtkGaimStatusBox *status_box, const gchar *error)
{
	if (status_box->error)
		g_free(status_box->error);
	status_box->error = NULL;
	if (error != NULL)
		status_box->error = g_strdup(error);
	gtk_gaim_status_box_refresh(status_box);
}

void
gtk_gaim_status_box_set_connecting(GtkGaimStatusBox *status_box, gboolean connecting)
{
	if (!status_box)
		return;
	status_box->connecting = connecting;
	gtk_gaim_status_box_refresh(status_box);
}

void
gtk_gaim_status_box_pulse_connecting(GtkGaimStatusBox *status_box)
{
	if (!status_box)
		return;
	if (status_box->connecting_index == 3)
		status_box->connecting_index = 0;
	else
		status_box->connecting_index++;
	gtk_gaim_status_box_refresh(status_box);
}

static void
gtk_gaim_status_box_pulse_typing(GtkGaimStatusBox *status_box)
{
	if (status_box->typing_index == 3)
		status_box->typing_index = 0;
	else
		status_box->typing_index++;
	gtk_gaim_status_box_refresh(status_box);
}

static GaimStatusType
*find_status_type_by_index(const GaimAccount *account, gint active)
{
	const GList *l = gaim_account_get_status_types(account);
	gint i;

	for (i = 0; l; l = l->next) {
		GaimStatusType *status_type = l->data;
		if (!gaim_status_type_is_user_settable(status_type))
			continue;

		if (active == i)
			return status_type;
		i++;
	}

	return NULL;
}

static gboolean
message_changed(const char *one, const char *two)
{
	if (one == NULL && two == NULL)
		return FALSE;

	if (one == NULL || two == NULL)
		return TRUE;

	return (g_utf8_collate(one, two) != 0);
}

static void
activate_currently_selected_status(GtkGaimStatusBox *status_box)
{
	GtkGaimStatusBoxItemType type;
	gpointer data;
	gchar *title;
	GtkTreeIter iter;
	char *message;
	GaimSavedStatus *saved_status;
	gboolean changed = TRUE;

	if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(status_box), &iter))
		return;

	gtk_tree_model_get(GTK_TREE_MODEL(status_box->dropdown_store), &iter,
					   TYPE_COLUMN, &type,
					   DATA_COLUMN, &data,
					   -1);

	/*
	 * If the currently selected status is "Custom..." or
	 * "Saved..." or a popular status then do nothing.
	 * Custom statuses are
	 * activated elsewhere, and we update the status_box
	 * accordingly by monitoring the preference
	 * "/core/savedstatus/current" and then calling
	 * update_to_reflect_current_status()
	 */
	if (type != GTK_GAIM_STATUS_BOX_TYPE_PRIMITIVE)
		return;

	gtk_tree_model_get(GTK_TREE_MODEL(status_box->dropdown_store), &iter,
					   TITLE_COLUMN, &title, -1);

	message = gtk_gaim_status_box_get_message(status_box);
	if (!message || !*message)
	{
		gtk_widget_hide_all(status_box->vbox);
		status_box->imhtml_visible = FALSE;
	}

	if (status_box->account == NULL) {
		/* Global */
		/* Save the newly selected status to prefs.xml and status.xml */

		/* Has the status been really changed? */
		saved_status = gaim_savedstatus_get_current();
		if (gaim_savedstatus_get_type(saved_status) == GPOINTER_TO_INT(data))
		{
			if (!message_changed(gaim_savedstatus_get_message(saved_status), message))
				changed = FALSE;
		}

		if (changed)
		{
			/* If we've used this type+message before, lookup the transient status */
			saved_status = gaim_savedstatus_find_by_type_and_message(
										GPOINTER_TO_INT(data), message);


			/* If this type+message is unique then create a new transient saved status */
			if (saved_status == NULL)
			{
				saved_status = gaim_savedstatus_new(NULL, GPOINTER_TO_INT(data));
				gaim_savedstatus_set_message(saved_status, message);
			}

			/* Set the status for each account */
			gaim_savedstatus_activate(saved_status);
		}
	} else {
		/* Per-account */
		gint active;
		GaimStatusType *status_type;
		GaimStatus *status;
		const char *id = NULL;

		status = gaim_account_get_active_status(status_box->account);

		g_object_get(G_OBJECT(status_box), "active", &active, NULL);

		status_type = find_status_type_by_index(status_box->account, active);
		id = gaim_status_type_get_id(status_type);

		if (strncmp(id, gaim_status_get_id(status), strlen(id)) == 0)
		{
			/* Selected status and previous status is the same */
			if (!message_changed(message, gaim_status_get_attr_string(status, "message")))
				changed = FALSE;
		}

		if (changed)
		{
			if (message)
				gaim_account_set_status(status_box->account, id,
										TRUE, "message", message, NULL);
			else
				gaim_account_set_status(status_box->account, id,
										TRUE, NULL);
		}
	}

	g_free(title);
	g_free(message);
}

static void update_size(GtkGaimStatusBox *status_box)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	int wrapped_lines;
	int lines;
	GdkRectangle oneline;
	int height;
	int pad_top, pad_inside, pad_bottom;

	if (!status_box->imhtml_visible)
	{
		if (status_box->vbox != NULL)
			gtk_widget_set_size_request(status_box->vbox, -1, -1);
		return;
	}

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(status_box->imhtml));

	wrapped_lines = 1;
	gtk_text_buffer_get_start_iter(buffer, &iter);
	while (gtk_text_view_forward_display_line(GTK_TEXT_VIEW(status_box->imhtml), &iter))
		wrapped_lines++;

	lines = gtk_text_buffer_get_line_count(buffer);

	/* Show a maximum of 4 lines */
	lines = MIN(lines, 4);
	wrapped_lines = MIN(wrapped_lines, 4);

	gtk_text_buffer_get_start_iter(buffer, &iter);
	gtk_text_view_get_iter_location(GTK_TEXT_VIEW(status_box->imhtml), &iter, &oneline);

	pad_top = gtk_text_view_get_pixels_above_lines(GTK_TEXT_VIEW(status_box->imhtml));
	pad_bottom = gtk_text_view_get_pixels_below_lines(GTK_TEXT_VIEW(status_box->imhtml));
	pad_inside = gtk_text_view_get_pixels_inside_wrap(GTK_TEXT_VIEW(status_box->imhtml));

	height = (oneline.height + pad_top + pad_bottom) * lines;
	height += (oneline.height + pad_inside) * (wrapped_lines - lines);

	gtk_widget_set_size_request(status_box->vbox, -1, height + GAIM_HIG_BOX_SPACE);
}

static void remove_typing_cb(GtkGaimStatusBox *status_box)
{
	if (status_box->typing == 0)
	{
		/* Nothing has changed, so we don't need to do anything */
		update_to_reflect_current_status(status_box);
		return;
	}

	activate_currently_selected_status(status_box);

	g_source_remove(status_box->typing);
	status_box->typing = 0;
	gtk_gaim_status_box_refresh(status_box);
}

static void gtk_gaim_status_box_changed(GtkComboBox *box)
{
	GtkGaimStatusBox *status_box;
	GtkTreeIter iter;
	GtkGaimStatusBoxItemType type;
	char *text, *sec_text;
	GdkPixbuf *pixbuf;
	gpointer data;
	GList *accounts = NULL, *node;

	status_box = GTK_GAIM_STATUS_BOX(box);

	if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(status_box), &iter))
		return;
	gtk_tree_model_get(GTK_TREE_MODEL(status_box->dropdown_store), &iter,
			   TYPE_COLUMN, &type,
			   TITLE_COLUMN, &text,
			   DESC_COLUMN, &sec_text,
			   ICON_COLUMN, &pixbuf,
			   DATA_COLUMN, &data,
			   -1);
	if (status_box->title)
		g_free(status_box->title);
	status_box->title = text;
	if (status_box->desc && sec_text)
		g_free(status_box->desc);
	status_box->desc = sec_text;
	if (status_box->pixbuf)
		g_object_unref(status_box->pixbuf);
	status_box->pixbuf = pixbuf;
	if (status_box->typing)
		g_source_remove(status_box->typing);
	status_box->typing = 0;

	if (GTK_WIDGET_IS_SENSITIVE(GTK_WIDGET(status_box)))
	{
		if (type == GTK_GAIM_STATUS_BOX_TYPE_POPULAR)
		{
			GaimSavedStatus *saved;
			saved = gaim_savedstatus_find_by_creation_time(GPOINTER_TO_INT(data));
			g_return_if_fail(saved != NULL);
			gaim_savedstatus_activate(saved);
			return;
		}

		if (type == GTK_GAIM_STATUS_BOX_TYPE_CUSTOM)
		{
			gaim_gtk_status_editor_show(NULL);
			update_to_reflect_current_status(status_box);
			return;
		}

		if (type == GTK_GAIM_STATUS_BOX_TYPE_SAVED)
		{
			gaim_gtk_status_window_show();
			update_to_reflect_current_status(status_box);
			return;
		}
	}

	/*
	 * Show the message box whenever the primitive allows for a
	 * message attribute on any protocol that is enabled,
	 * or our protocol, if we have account set
	 */
	if (status_box->account)
		accounts = g_list_prepend(accounts, status_box->account);
	else
		accounts = gaim_accounts_get_all_active();
	status_box->imhtml_visible = FALSE;
	for (node = accounts; node != NULL; node = node->next)
	{
		GaimAccount *account;
		GaimStatusType *status_type;

		account = node->data;
		status_type = gaim_account_get_status_type_with_primitive(account, GPOINTER_TO_INT(data));
		if ((status_type != NULL) &&
			(gaim_status_type_get_attr(status_type, "message") != NULL))
		{
			status_box->imhtml_visible = TRUE;
			break;
		}
	}
	g_list_free(accounts);

	if (GTK_WIDGET_IS_SENSITIVE(GTK_WIDGET(status_box)))
	{
		if (status_box->imhtml_visible)
		{
			GtkTextBuffer *buf;
			GtkTextIter start, end;
			gtk_widget_show_all(status_box->vbox);
			if (GTK_WIDGET_IS_SENSITIVE(GTK_WIDGET(status_box))) {
				status_box->typing = g_timeout_add(TYPING_TIMEOUT, (GSourceFunc)remove_typing_cb, status_box);
			}
			gtk_widget_grab_focus(status_box->imhtml);
			buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(status_box->imhtml));
			gtk_text_buffer_get_start_iter(buf, &start);
			gtk_text_buffer_get_end_iter(buf, &end);
			gtk_text_buffer_move_mark_by_name(buf, "insert", &end);
			gtk_text_buffer_move_mark_by_name(buf, "selection_bound", &start);
		}
		else
		{
			gtk_widget_hide_all(status_box->vbox);
			if (GTK_WIDGET_IS_SENSITIVE(GTK_WIDGET(status_box)))
				activate_currently_selected_status(status_box); /* This is where we actually set the status */
		}
	}
	gtk_gaim_status_box_refresh(status_box);
}

static gint
get_statusbox_index(GtkGaimStatusBox *box, GaimSavedStatus *saved_status)
{
	gint index;

	switch (gaim_savedstatus_get_type(saved_status))
	{
		case GAIM_STATUS_AVAILABLE:
			index = 0;
			break;
		case GAIM_STATUS_AWAY:
			index = 1;
			break;
		case GAIM_STATUS_INVISIBLE:
			index = 2;
			break;
		case GAIM_STATUS_OFFLINE:
			index = 3;
			break;
		default:
			index = GTK_LIST_STORE(box->dropdown_store)->length - 2;
			break;
	}

	return index;
}

static void imhtml_changed_cb(GtkTextBuffer *buffer, void *data)
{
	GtkGaimStatusBox *box = (GtkGaimStatusBox*)data;
	if (GTK_WIDGET_IS_SENSITIVE(GTK_WIDGET(box)))
	{
		if (box->typing) {
			gtk_gaim_status_box_pulse_typing(box);
			g_source_remove(box->typing);
		} else {
			gint index;
			GaimSavedStatus *saved = gaim_savedstatus_get_current();

			index = get_statusbox_index(box, saved);

			gtk_widget_set_sensitive(GTK_WIDGET(box), FALSE);
			gtk_combo_box_set_active(GTK_COMBO_BOX(box), index);
			gtk_widget_set_sensitive(GTK_WIDGET(box), TRUE);
			gtk_widget_grab_focus(box->imhtml);
		}
		box->typing = g_timeout_add(TYPING_TIMEOUT, (GSourceFunc)remove_typing_cb, box);
	}
	gtk_gaim_status_box_refresh(box);
}

static void imhtml_format_changed_cb(GtkIMHtml *imhtml, GtkIMHtmlButtons buttons, void *data)
{
	imhtml_changed_cb(NULL, data);
}

char *gtk_gaim_status_box_get_message(GtkGaimStatusBox *status_box)
{
	if (status_box->imhtml_visible)
		return gtk_imhtml_get_markup(GTK_IMHTML(status_box->imhtml));
	else
		return NULL;
}
