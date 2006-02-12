/*
 * Gaim's oscar protocol plugin
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
 * Family 0x0003 - Old-style Buddylist Management (non-SSI).
 *
 */

#include "oscar.h"

#include <string.h>

/*
 * Subtype 0x0002 - Request rights.
 *
 * Request Buddy List rights.
 *
 */
faim_export int aim_buddylist_reqrights(OscarSession *sess, OscarConnection *conn)
{
	return aim_genericreq_n_snacid(sess, conn, 0x0003, 0x0002);
}

/*
 * Subtype 0x0003 - Rights.
 *
 */
static int rights(OscarSession *sess, aim_module_t *mod, FlapFrame *rx, aim_modsnac_t *snac, ByteStream *bs)
{
	aim_rxcallback_t userfunc;
	aim_tlvlist_t *tlvlist;
	guint16 maxbuddies = 0, maxwatchers = 0;
	int ret = 0;

	/*
	 * TLVs follow
	 */
	tlvlist = aim_tlvlist_read(bs);

	/*
	 * TLV type 0x0001: Maximum number of buddies.
	 */
	if (aim_tlv_gettlv(tlvlist, 0x0001, 1))
		maxbuddies = aim_tlv_get16(tlvlist, 0x0001, 1);

	/*
	 * TLV type 0x0002: Maximum number of watchers.
	 *
	 * Watchers are other users who have you on their buddy
	 * list.  (This is called the "reverse list" by a certain
	 * other IM protocol.)
	 *
	 */
	if (aim_tlv_gettlv(tlvlist, 0x0002, 1))
		maxwatchers = aim_tlv_get16(tlvlist, 0x0002, 1);

	/*
	 * TLV type 0x0003: Unknown.
	 *
	 * ICQ only?
	 */

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		ret = userfunc(sess, rx, maxbuddies, maxwatchers);

	aim_tlvlist_free(&tlvlist);

	return ret;
}

/*
 * Subtype 0x0004 - Add buddy to list.
 *
 * Adds a single buddy to your buddy list after login.
 * XXX This should just be an extension of setbuddylist()
 *
 */
faim_export int aim_buddylist_addbuddy(OscarSession *sess, OscarConnection *conn, const char *sn)
{
	FlapFrame *fr;
	aim_snacid_t snacid;

	if (!sn || !strlen(sn))
		return -EINVAL;

	if (!(fr = flap_frame_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10+1+strlen(sn))))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0003, 0x0004, 0x0000, sn, strlen(sn)+1);
	aim_putsnac(&fr->data, 0x0003, 0x0004, 0x0000, snacid);

	aimbs_put8(&fr->data, strlen(sn));
	aimbs_putstr(&fr->data, sn);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 * Subtype 0x0004 - Add multiple buddies to your buddy list.
 *
 * This just builds the "set buddy list" command then queues it.
 *
 * buddy_list = "Screen Name One&ScreenNameTwo&";
 *
 * XXX Clean this up.
 *
 */
faim_export int aim_buddylist_set(OscarSession *sess, OscarConnection *conn, const char *buddy_list)
{
	FlapFrame *fr;
	aim_snacid_t snacid;
	int len = 0;
	char *localcpy = NULL;
	char *tmpptr = NULL;

	if (!buddy_list || !(localcpy = strdup(buddy_list)))
		return -EINVAL;

	for (tmpptr = strtok(localcpy, "&"); tmpptr; ) {
		gaim_debug_misc("oscar", "---adding: %s (%d)\n", tmpptr, strlen(tmpptr));
		len += 1 + strlen(tmpptr);
		tmpptr = strtok(NULL, "&");
	}

	if (!(fr = flap_frame_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10+len)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0003, 0x0004, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x0003, 0x0004, 0x0000, snacid);

	strncpy(localcpy, buddy_list, strlen(buddy_list) + 1);

	for (tmpptr = strtok(localcpy, "&"); tmpptr; ) {

		gaim_debug_misc("oscar", "---adding: %s (%d)\n", tmpptr, strlen(tmpptr));

		aimbs_put8(&fr->data, strlen(tmpptr));
		aimbs_putstr(&fr->data, tmpptr);
		tmpptr = strtok(NULL, "&");
	}

	aim_tx_enqueue(sess, fr);

	free(localcpy);

	return 0;
}

/*
 * Subtype 0x0005 - Remove buddy from list.
 *
 * XXX generalise to support removing multiple buddies (basically, its
 * the same as setbuddylist() but with a different snac subtype).
 *
 */
faim_export int aim_buddylist_removebuddy(OscarSession *sess, OscarConnection *conn, const char *sn)
{
	FlapFrame *fr;
	aim_snacid_t snacid;

	if (!sn || !strlen(sn))
		return -EINVAL;

	if (!(fr = flap_frame_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10+1+strlen(sn))))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0003, 0x0005, 0x0000, sn, strlen(sn)+1);
	aim_putsnac(&fr->data, 0x0003, 0x0005, 0x0000, snacid);

	aimbs_put8(&fr->data, strlen(sn));
	aimbs_putstr(&fr->data, sn);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 * Subtype 0x000b
 *
 * XXX Why would we send this?
 *
 */
faim_export int aim_buddylist_oncoming(OscarSession *sess, OscarConnection *conn, aim_userinfo_t *info)
{
	FlapFrame *fr;
	aim_snacid_t snacid;

	if (!sess || !conn || !info)
		return -EINVAL;

	if (!(fr = flap_frame_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 1152)))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0003, 0x000b, 0x0000, NULL, 0);

	aim_putsnac(&fr->data, 0x0003, 0x000b, 0x0000, snacid);
	aim_putuserinfo(&fr->data, info);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/* 
 * Subtype 0x000c
 *
 * XXX Why would we send this?
 *
 */
faim_export int aim_buddylist_offgoing(OscarSession *sess, OscarConnection *conn, const char *sn)
{
	FlapFrame *fr;
	aim_snacid_t snacid;

	if (!sess || !conn || !sn)
		return -EINVAL;

	if (!(fr = flap_frame_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10+1+strlen(sn))))
		return -ENOMEM;

	snacid = aim_cachesnac(sess, 0x0003, 0x000c, 0x0000, NULL, 0);

	aim_putsnac(&fr->data, 0x0003, 0x000c, 0x0000, snacid);
	aimbs_put8(&fr->data, strlen(sn));
	aimbs_putstr(&fr->data, sn);

	aim_tx_enqueue(sess, fr);

	return 0;
}

/*
 * Subtypes 0x000b and 0x000c - Change in buddy status
 *
 * Oncoming Buddy notifications contain a subset of the
 * user information structure.  It's close enough to run
 * through aim_info_extract() however.
 *
 * Although the offgoing notification contains no information,
 * it is still in a format parsable by aim_info_extract().
 *
 */
static int buddychange(OscarSession *sess, aim_module_t *mod, FlapFrame *rx, aim_modsnac_t *snac, ByteStream *bs)
{
	int ret = 0;
	aim_userinfo_t userinfo;
	aim_rxcallback_t userfunc;

	aim_info_extract(sess, bs, &userinfo);

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		ret = userfunc(sess, rx, &userinfo);

	if (snac->subtype == 0x000b)
		aim_locate_requestuserinfo(sess, userinfo.sn);
	aim_info_free(&userinfo);

	return ret;
}

static int snachandler(OscarSession *sess, aim_module_t *mod, FlapFrame *rx, aim_modsnac_t *snac, ByteStream *bs)
{

	if (snac->subtype == 0x0003)
		return rights(sess, mod, rx, snac, bs);
	else if ((snac->subtype == 0x000b) || (snac->subtype == 0x000c))
		return buddychange(sess, mod, rx, snac, bs);

	return 0;
}

faim_internal int buddylist_modfirst(OscarSession *sess, aim_module_t *mod)
{

	mod->family = 0x0003;
	mod->version = 0x0001;
	mod->toolid = 0x0110;
	mod->toolversion = 0x0629;
	mod->flags = 0;
	strncpy(mod->name, "buddy", sizeof(mod->name));
	mod->snachandler = snachandler;

	return 0;
}
