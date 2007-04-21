/**
 * @file gtkgaim-compat.h Gtk Gaim Compat macros
 *
 * pidgin
 *
 * Pidgin is the legal property of its developers, whose names are too numerous
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
#ifndef _GTKGAIM_COMPAT_H_
#define _GTKGAIM_COMPAT_H_

#define GaimConvPlacementFunc PidginConvPlacementFunc
#define gaim_get_gtkxfer_dialog pidgin_get_xfer_dialog
#define gaim_gtk_account_dialog_show pidgin_account_dialog_show
#define GaimGtkAccountDialogType PidginAccountDialogType
#define gaim_gtk_account_get_handle pidgin_account_get_handle
#define gaim_gtk_account_init pidgin_account_init
#define gaim_gtk_accounts_get_ui_ops pidgin_accounts_get_ui_ops
#define gaim_gtk_accounts_window_hide pidgin_accounts_window_hide
#define gaim_gtk_accounts_window_show pidgin_accounts_window_show
#define gaim_gtk_account_uninit pidgin_account_uninit
#define GAIM_GTK_ADD_ACCOUNT_DIALOG PIDGIN_ADD_ACCOUNT_DIALOG
#define gaim_gtk_append_blist_node_extended_menu pidgin_append_blist_node_extended_menu
#define gaim_gtk_append_blist_node_privacy_menu pidgin_append_blist_node_privacy_menu
#define gaim_gtk_append_blist_node_proto_menu pidgin_append_blist_node_proto_menu
#define gaim_gtk_blist_add_alert pidgin_blist_add_alert
#define gaim_gtk_blist_get_default_gtk_blist pidgin_blist_get_default_gtk_blist
#define gaim_gtk_blist_get_handle pidgin_blist_get_handle
#define gaim_gtk_blist_get_sort_methods pidgin_blist_get_sort_methods
#define gaim_gtk_blist_get_status_icon pidgin_blist_get_status_icon
#define gaim_gtk_blist_get_ui_ops pidgin_blist_get_ui_ops
#define gaim_gtk_blist_init pidgin_blist_init
#define gaim_gtk_blist_joinchat_is_showable pidgin_blist_joinchat_is_showable
#define gaim_gtk_blist_joinchat_show pidgin_blist_joinchat_show
#define gaim_gtk_blist_make_buddy_menu pidgin_blist_make_buddy_menu
#define gaim_gtk_blist_node_is_contact_expanded pidgin_blist_node_is_contact_expanded
#define gaim_gtk_blist_refresh pidgin_blist_refresh
#define gaim_gtk_blist_set_headline pidgin_blist_set_headline
#define gaim_gtk_blist_setup_sort_methods pidgin_blist_setup_sort_methods
#define gaim_gtk_blist_sort_function pidgin_blist_sort_function
#define gaim_gtk_blist_sort_method pidgin_blist_sort_method
#define GaimGtkBlistSortMethod PidginBlistSortMethod
#define gaim_gtk_blist_sort_method_reg pidgin_blist_sort_method_reg
#define gaim_gtk_blist_sort_method_set pidgin_blist_sort_method_set
#define gaim_gtk_blist_sort_method_unreg pidgin_blist_sort_method_unreg
#define gaim_gtk_blist_toggle_visibility pidgin_blist_toggle_visibility
#define gaim_gtk_blist_uninit pidgin_blist_uninit
#define gaim_gtk_blist_update_account_error_state pidgin_blist_update_account_error_state
#define gaim_gtk_blist_update_accounts_menu pidgin_blist_update_accounts_menu
#define gaim_gtk_blist_update_columns pidgin_blist_update_columns
#define gaim_gtk_blist_update_plugin_actions pidgin_blist_update_plugin_actions
#define gaim_gtk_blist_update_refresh_timeout pidgin_blist_update_refresh_timeout
#define gaim_gtk_blist_update_sort_methods pidgin_blist_update_sort_methods
#define gaim_gtk_blist_visibility_manager_add pidgin_blist_visibility_manager_add
#define gaim_gtk_blist_visibility_manager_remove pidgin_blist_visibility_manager_remove
#define GaimGtkCellRendererExpanderClass PidginCellRendererExpanderClass
#define gaim_gtk_cell_renderer_expander_get_type pidgin_cell_renderer_expander_get_type
#define gaim_gtk_cell_renderer_expander_new pidgin_cell_renderer_expander_new
#define GaimGtkCellRendererExpander PidginCellRendererExpander
#define GaimGtkCellRendererProgressClass PidginCellRendererProgressClass
#define gaim_gtk_cell_renderer_progress_get_type pidgin_cell_renderer_progress_get_type
#define gaim_gtk_cell_renderer_progress_new pidgin_cell_renderer_progress_new
#define GaimGtkCellRendererProgress PidginCellRendererProgress
#define GaimGtkChatPane PidginChatPane
#define GaimGtkChatPane PidginChatPane
#define gaim_gtk_connection_get_handle pidgin_connection_get_handle
#define gaim_gtk_connection_init pidgin_connection_init
#define gaim_gtk_connections_get_ui_ops pidgin_connections_get_ui_ops
#define gaim_gtk_connection_uninit pidgin_connection_uninit
#define GaimGtkConversation PidginConversation
#define gaim_gtk_conversations_fill_menu pidgin_conversations_fill_menu
#define gaim_gtk_conversations_find_unseen_list pidgin_conversations_find_unseen_list
#define gaim_gtk_conversations_get_conv_ui_ops pidgin_conversations_get_conv_ui_ops
#define gaim_gtk_conversations_get_handle pidgin_conversations_get_handle
#define gaim_gtk_conversations_init pidgin_conversations_init
#define gaim_gtk_conversations_uninit pidgin_conversations_uninit
#define gaim_gtkconv_get_tab_at_xy pidgin_conv_get_tab_at_xy
#define gaim_gtkconv_get_tab_icon pidgin_conv_get_tab_icon
#define gaim_gtkconv_get_window pidgin_conv_get_window
#define gaim_gtkconv_is_hidden pidgin_conv_is_hidden
#define gaim_gtkconv_new pidgin_conv_new
#define gaim_gtkconv_placement_add_fnc pidgin_conv_placement_add_fnc
#define gaim_gtkconv_placement_get_current_func pidgin_conv_placement_get_current_func
#define gaim_gtkconv_placement_get_fnc pidgin_conv_placement_get_fnc
#define gaim_gtkconv_placement_get_name pidgin_conv_placement_get_name
#define gaim_gtkconv_placement_get_options pidgin_conv_placement_get_options
#define gaim_gtkconv_placement_place pidgin_conv_placement_place
#define gaim_gtkconv_placement_remove_fnc pidgin_conv_placement_remove_fnc
#define gaim_gtkconv_placement_set_current_func pidgin_conv_placement_set_current_func
#define gaim_gtkconv_present_conversation pidgin_conv_present_conversation
#define gaim_gtkconv_switch_active_conversation pidgin_conv_switch_active_conversation
#define gaim_gtkconv_update_buddy_icon pidgin_conv_update_buddy_icon
#define gaim_gtkconv_update_buttons_by_protocol pidgin_conv_update_buttons_by_protocol
#define gaim_gtk_conv_window_add_gtkconv pidgin_conv_window_add_gtkconv
#define gaim_gtk_conv_window_destroy pidgin_conv_window_destroy
#define gaim_gtk_conv_window_first_with_type pidgin_conv_window_first_with_type
#define gaim_gtk_conv_window_get_active_conversation pidgin_conv_window_get_active_conversation
#define gaim_gtk_conv_window_get_active_gtkconv pidgin_conv_window_get_active_gtkconv
#define gaim_gtk_conv_window_get_at_xy pidgin_conv_window_get_at_xy
#define gaim_gtk_conv_window_get_gtkconv_at_index pidgin_conv_window_get_gtkconv_at_index
#define gaim_gtk_conv_window_get_gtkconv_count pidgin_conv_window_get_gtkconv_count
#define gaim_gtk_conv_window_get_gtkconvs pidgin_conv_window_get_gtkconvs
#define gaim_gtk_conv_window_has_focus pidgin_conv_window_has_focus
#define gaim_gtk_conv_window_hide pidgin_conv_window_hide
#define gaim_gtk_conv_window_is_active_conversation pidgin_conv_window_is_active_conversation
#define gaim_gtk_conv_window_last_with_type pidgin_conv_window_last_with_type
#define gaim_gtk_conv_window_new pidgin_conv_window_new
#define gaim_gtk_conv_window_raise pidgin_conv_window_raise
#define gaim_gtk_conv_window_remove_gtkconv pidgin_conv_window_remove_gtkconv
#define gaim_gtk_conv_windows_get_list pidgin_conv_windows_get_list
#define gaim_gtk_conv_window_show pidgin_conv_window_show
#define gaim_gtk_conv_window_switch_gtkconv pidgin_conv_window_switch_gtkconv
#define gaim_gtk_debug_get_handle pidgin_debug_get_handle
#define gaim_gtk_debug_get_ui_ops pidgin_debug_get_ui_ops
#define gaim_gtk_debug_init pidgin_debug_init
#define gaim_gtk_debug_uninit pidgin_debug_uninit
#define gaim_gtk_debug_window_hide pidgin_debug_window_hide
#define gaim_gtk_debug_window_show pidgin_debug_window_show
#define gaim_gtkdialogs_about pidgin_dialogs_about
#define gaim_gtkdialogs_alias_buddy pidgin_dialogs_alias_buddy
#define gaim_gtkdialogs_alias_chat pidgin_dialogs_alias_chat
#define gaim_gtkdialogs_alias_contact pidgin_dialogs_alias_contact
#define gaim_gtkdialogs_destroy_all pidgin_dialogs_destroy_all
#define gaim_gtkdialogs_im pidgin_dialogs_im
#define gaim_gtkdialogs_im_with_user pidgin_dialogs_im_with_user
#define gaim_gtkdialogs_info pidgin_dialogs_info
#define gaim_gtkdialogs_log pidgin_dialogs_log
#define gaim_gtkdialogs_merge_groups pidgin_dialogs_merge_groups
#define gaim_gtkdialogs_remove_buddy pidgin_dialogs_remove_buddy
#define gaim_gtkdialogs_remove_chat pidgin_dialogs_remove_chat
#define gaim_gtkdialogs_remove_contact pidgin_dialogs_remove_contact
#define gaim_gtkdialogs_remove_group pidgin_dialogs_remove_group
#define gaim_gtk_docklet_clicked pidgin_docklet_clicked
#define gaim_gtk_docklet_embedded pidgin_docklet_embedded
#define gaim_gtk_docklet_get_handle pidgin_docklet_get_handle
#define gaim_gtk_docklet_init pidgin_docklet_init
#define gaim_gtk_docklet_remove pidgin_docklet_remove
#define gaim_gtk_docklet_set_ui_ops pidgin_docklet_set_ui_ops
#define gaim_gtk_docklet_uninit pidgin_docklet_uninit
#define gaim_gtk_docklet_unload pidgin_docklet_unload
#define gaim_gtk_eventloop_get_ui_ops pidgin_eventloop_get_ui_ops
#define gaim_gtk_idle_get_ui_ops pidgin_idle_get_ui_ops
#define GaimGtkImPane PidginImPane
#define GaimGtkImPane PidginImPane
#define gaim_gtk_log_get_handle pidgin_log_get_handle
#define gaim_gtk_log_init pidgin_log_init
#define gaim_gtk_log_show_contact pidgin_log_show_contact
#define gaim_gtk_log_show pidgin_log_show
#define gaim_gtk_log_uninit pidgin_log_uninit
#define GaimGtkLogViewer PidginLogViewer
#define gaim_gtk_menu_tray_append pidgin_menu_tray_append
#define GaimGtkMenuTrayClass PidginMenuTrayClass
#define gaim_gtk_menu_tray_get_box pidgin_menu_tray_get_box
#define gaim_gtk_menu_tray_get_gtype pidgin_menu_tray_get_gtype
#define gaim_gtk_menu_tray_new pidgin_menu_tray_new
#define GaimGtkMenuTray PidginMenuTray
#define GaimGtkMenuTray PidginMenuTray
#define gaim_gtk_menu_tray_prepend pidgin_menu_tray_prepend
#define gaim_gtk_menu_tray_set_tooltip pidgin_menu_tray_set_tooltip
#define GAIM_GTK_MODIFY_ACCOUNT_DIALOG PIDGIN_MODIFY_ACCOUNT_DIALOG
#define gaim_gtk_notify_get_ui_ops pidgin_notify_get_ui_ops
#define gaim_gtk_plugin_dialog_show pidgin_plugin_dialog_show
#define gaim_gtk_plugin_get_config_frame pidgin_plugin_get_config_frame
#define gaim_gtk_plugin_pref_create_frame pidgin_plugin_pref_create_frame
#define gaim_gtk_plugins_save pidgin_plugins_save
#define GaimGtkPluginUiInfo PidginPluginUiInfo
#define gaim_gtk_pounce_editor_show pidgin_pounce_editor_show
#define gaim_gtk_pounces_get_handle pidgin_pounces_get_handle
#define gaim_gtk_pounces_init pidgin_pounces_init
#define gaim_gtk_pounces_manager_hide pidgin_pounces_manager_hide
#define gaim_gtk_pounces_manager_show pidgin_pounces_manager_show
#define gaim_gtk_prefs_checkbox pidgin_prefs_checkbox
#define gaim_gtk_prefs_dropdown_from_list pidgin_prefs_dropdown_from_list
#define gaim_gtk_prefs_dropdown pidgin_prefs_dropdown
#define gaim_gtk_prefs_init pidgin_prefs_init
#define gaim_gtk_prefs_labeled_entry pidgin_prefs_labeled_entry
#define gaim_gtk_prefs_labeled_spin_button pidgin_prefs_labeled_spin_button
#define gaim_gtk_prefs_show pidgin_prefs_show
#define gaim_gtk_prefs_update_old pidgin_prefs_update_old
#define gaim_gtk_privacy_dialog_hide pidgin_privacy_dialog_hide
#define gaim_gtk_privacy_dialog_show pidgin_privacy_dialog_show
#define gaim_gtk_privacy_get_ui_ops pidgin_privacy_get_ui_ops
#define gaim_gtk_privacy_init pidgin_privacy_init
#define gaim_gtk_request_add_block pidgin_request_add_block
#define gaim_gtk_request_add_permit pidgin_request_add_permit
#define gaim_gtk_request_get_ui_ops pidgin_request_get_ui_ops
#define gaim_gtk_roomlist_dialog_show pidgin_roomlist_dialog_show
#define gaim_gtk_roomlist_dialog_show_with_account pidgin_roomlist_dialog_show_with_account
#define gaim_gtk_roomlist_init pidgin_roomlist_init
#define gaim_gtk_roomlist_is_showable pidgin_roomlist_is_showable
#define gaim_gtk_session_end pidgin_session_end
#define gaim_gtk_session_init pidgin_session_init
#define gaim_gtk_sound_get_event_label pidgin_sound_get_event_label
#define gaim_gtk_sound_get_event_option pidgin_sound_get_event_option
#define gaim_gtk_sound_get_handle pidgin_sound_get_handle
#define gaim_gtk_sound_get_ui_ops pidgin_sound_get_ui_ops
#define gaim_gtk_status_editor_show pidgin_status_editor_show
#define gaim_gtk_status_get_handle pidgin_status_get_handle
#define gaim_gtk_status_init pidgin_status_init
#define gaim_gtk_status_menu pidgin_status_menu
#define gaim_gtk_status_uninit pidgin_status_uninit
#define gaim_gtk_status_window_hide pidgin_status_window_hide
#define gaim_gtk_status_window_show pidgin_status_window_show
#define gaim_gtk_stock_init pidgin_stock_init
#define gaim_gtk_syslog_show pidgin_syslog_show
#define gaim_gtkthemes_get_proto_smileys pidgin_themes_get_proto_smileys
#define gaim_gtkthemes_init pidgin_themes_init
#define gaim_gtkthemes_load_smiley_theme pidgin_themes_load_smiley_theme
#define gaim_gtkthemes_smileys_disabled pidgin_themes_smileys_disabled
#define gaim_gtkthemes_smiley_themeize pidgin_themes_smiley_themeize
#define gaim_gtkthemes_smiley_theme_probe pidgin_themes_smiley_theme_probe
#define gaim_gtk_whiteboard_get_ui_ops pidgin_whiteboard_get_ui_ops
#define GaimGtkWindow PidginWindow
#define gaim_gtkxfer_dialog_add_xfer pidgin_xfer_dialog_add_xfer
#define gaim_gtkxfer_dialog_cancel_xfer pidgin_xfer_dialog_cancel_xfer
#define gaim_gtkxfer_dialog_destroy pidgin_xfer_dialog_destroy
#define gaim_gtkxfer_dialog_hide pidgin_xfer_dialog_hide
#define gaim_gtkxfer_dialog_new pidgin_xfer_dialog_new
#define GaimGtkXferDialog PidginXferDialog
#define gaim_gtkxfer_dialog_remove_xfer pidgin_xfer_dialog_remove_xfer
#define gaim_gtkxfer_dialog_show pidgin_xfer_dialog_show
#define gaim_gtkxfer_dialog_update_xfer pidgin_xfer_dialog_update_xfer
#define gaim_gtk_xfers_get_ui_ops pidgin_xfers_get_ui_ops
#define gaim_gtk_xfers_init pidgin_xfers_init
#define gaim_gtk_xfers_uninit pidgin_xfers_uninit
#define GaimScrollBookClass PidginScrollBookClass
#define gaim_scroll_book_get_type pidgin_scroll_book_get_type
#define gaim_scroll_book_new pidgin_scroll_book_new
#define GaimScrollBook PidginScrollBook
#define gaim_set_gtkxfer_dialog pidgin_set_xfer_dialog
#define gaim_status_box_add pidgin_status_box_add
#define gaim_status_box_add_separator pidgin_status_box_add_separator
#define GaimStatusBoxClass PidginStatusBoxClass
#define gaim_status_box_get_buddy_icon pidgin_status_box_get_buddy_icon
#define gaim_status_box_get_message pidgin_status_box_get_message
#define gaim_status_box_get_type pidgin_status_box_get_type
#define gaim_status_box_new pidgin_status_box_new
#define gaim_status_box_new_with_account pidgin_status_box_new_with_account
#define GAIM_STATUS_BOX_NUM_TYPES PIDGIN_STATUS_BOX_NUM_TYPES
#define GaimStatusBox PidginStatusBox
#define gaim_status_box_pulse_connecting pidgin_status_box_pulse_connecting
#define gaim_status_box_set_buddy_icon pidgin_status_box_set_buddy_icon
#define gaim_status_box_set_connecting pidgin_status_box_set_connecting
#define gaim_status_box_set_network_available pidgin_status_box_set_network_available
#define GAIM_STATUS_BOX_TYPE_CUSTOM PIDGIN_STATUS_BOX_TYPE_CUSTOM
#define GAIM_STATUS_BOX_TYPE_POPULAR PIDGIN_STATUS_BOX_TYPE_POPULAR
#define GAIM_STATUS_BOX_TYPE_PRIMITIVE PIDGIN_STATUS_BOX_TYPE_PRIMITIVE
#define GAIM_STATUS_BOX_TYPE_SAVED PIDGIN_STATUS_BOX_TYPE_SAVED
#define GAIM_STATUS_BOX_TYPE_SEPARATOR PIDGIN_STATUS_BOX_TYPE_SEPARATOR
#define GAIM_STATUS_ICON_LARGE PIDGIN_STATUS_ICON_LARGE
#define GaimStatusIconSize PidginStatusIconSize
#define GAIM_STATUS_ICON_SMALL PIDGIN_STATUS_ICON_SMALL
#define GAIM_UNSEEN_EVENT PIDGIN_UNSEEN_EVENT
#define GAIM_UNSEEN_NICK PIDGIN_UNSEEN_NICK
#define GAIM_UNSEEN_NO_LOG PIDGIN_UNSEEN_NO_LOG
#define GAIM_UNSEEN_NONE PIDGIN_UNSEEN_NONE
#define GaimUnseenState PidginUnseenState
#define GAIM_UNSEEN_TEXT PIDGIN_UNSEEN_TEXT
#define GtkGaimStatusBoxItemType PidginStatusBoxItemType

#endif /* _GTKGAIM_COMPAT_H */
