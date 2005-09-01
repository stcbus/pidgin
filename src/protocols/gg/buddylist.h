/**
 * @file buddylist.h
 *
 * gaim
 *
 * Copyright (C) 2005  Bartosz Oler <bartosz@bzimage.us>
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


#ifndef _GAIM_GG_BUDDYLIST_H
#define _GAIM_GG_BUDDYLIST_H

#include "connection.h"
#include "account.h"

void
ggp_buddylist_send(GaimConnection *gc);

/**
 * Load buddylist from server into the rooster.
 *
 * @param gc GaimConnection
 * @param buddylist Pointer to the buddylist that will be loaded.
 */
/* void ggp_buddylist_load(GaimConnection *gc, char *buddylist) {{{ */
void
ggp_buddylist_load(GaimConnection *gc, char *buddylist);

/**
 * Set offline status for all buddies.
 *
 * @param gc Connection handler
 */
void
ggp_buddylist_offline(GaimConnection *gc);

/**
 * Get all the buddies in the current account.
 *
 * @param account Current account.
 * 
 * @return List of buddies.
 */
char *
ggp_buddylist_dump(GaimAccount *account);


#endif /* _GAIM_GG_BUDDYLIST_H */

/* vim: set ts=4 sts=0 sw=4 noet: */
