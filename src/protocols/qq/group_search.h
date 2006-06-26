/**
* The QQ2003C protocol plugin
 *
 * for gaim
 *
 * Copyright (C) 2004 Puzzlebird
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

// START OF FILE
/*****************************************************************************/
#ifndef _QQ_GROUP_SEARCH_H_
#define _QQ_GROUP_SEARCH_H_

#include <glib.h>
#include "connection.h"		// GaimConnection

void qq_send_cmd_group_search_group(GaimConnection * gc, guint32 external_group_id);
void qq_process_group_cmd_search_group(guint8 * data, guint8 ** cursor, gint len, GaimConnection * gc);

#endif
/*****************************************************************************/
// END OF FILE
