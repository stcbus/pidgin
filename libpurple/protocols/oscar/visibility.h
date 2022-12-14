/*
 * Purple's oscar protocol plugin
 * This file is the legal property of its developers.
 * Please see the AUTHORS file distributed alongside this file.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
*/

#ifndef PURPLE_OSCAR_VISIBILITY_H
#define PURPLE_OSCAR_VISIBILITY_H

#include "oscar.h"
#include "plugins.h"
#include "action.h"

PurpleActionMenu * create_visibility_menu_item(OscarData *od, const char *bname);
void oscar_show_visible_list(PurpleProtocolAction *action);
void oscar_show_invisible_list(PurpleProtocolAction *action);

#endif /* PURPLE_OSCAR_VISIBILITY_H */
