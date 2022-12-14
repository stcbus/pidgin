/* pidgin
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#if !defined(PIDGIN_GLOBAL_HEADER_INSIDE) && !defined(PIDGIN_COMPILATION)
# error "only <pidgin.h> may be included directly"
#endif

#ifndef _PIDGINCONN_H_
#define _PIDGINCONN_H_

G_BEGIN_DECLS

/**************************************************************************/
/* GTK Connection API                                                     */
/**************************************************************************/

/**
 * pidgin_connections_get_ui_ops:
 *
 * Gets GTK Connection UI ops
 *
 * Returns: UI operations struct
 */
PurpleConnectionUiOps *pidgin_connections_get_ui_ops(void);

/**
 * pidgin_connection_init:
 *
 * Initializes the GTK connection system.
 */
void pidgin_connection_init(void);

/**
 * pidgin_connection_uninit:
 *
 * Uninitializes the GTK connection system.
 */
void pidgin_connection_uninit(void);

G_END_DECLS

#endif /* _PIDGINCONN_H_ */
