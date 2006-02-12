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
 * Family 0x000f - Newer Search Method
 *
 * Used for searching for other AIM users by email address, name,
 * location, commmon interests, and a few other similar things.
 *
 */

#include "oscar.h"

/**
 * Subtype 0x0002 - Submit a User Search Request
 *
 * Search for an AIM screen name based on their email address.
 *
 * @param sess The oscar session.
 * @param region Should be "us-ascii" unless you know what you're doing.
 * @param email The email address you want to search for.
 * @return Return 0 if no errors, otherwise return the error number.
 */
faim_export int aim_odir_email(OscarSession *sess, const char *region, const char *email)
{
	OscarConnection *conn;
	FlapFrame *fr;
	aim_snacid_t snacid;
	aim_tlvlist_t *tl = NULL;

	if (!sess || !(conn = aim_conn_findbygroup(sess, 0x000f)) || !region || !email)
		return -EINVAL;

	/* Create a TLV chain, write it to the outgoing frame, then free the chain */
	aim_tlvlist_add_str(&tl, 0x001c, region);
	aim_tlvlist_add_16(&tl, 0x000a, 0x0001); /* Type of search */
	aim_tlvlist_add_str(&tl, 0x0005, email);

	if (!(fr = flap_frame_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10+aim_tlvlist_size(&tl))))
		return -ENOMEM;
	snacid = aim_cachesnac(sess, 0x000f, 0x0002, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x000f, 0x0002, 0x0000, snacid);

	aim_tlvlist_write(&fr->data, &tl);
	aim_tlvlist_free(&tl);

	aim_tx_enqueue(sess, fr);

	return 0;
}


/**
 * Subtype 0x0002 - Submit a User Search Request
 *
 * Search for an AIM screen name based on various info
 * about the person.
 *
 * @param sess The oscar session.
 * @param region Should be "us-ascii" unless you know what you're doing.
 * @param first The first name of the person you want to search for.
 * @param middle The middle name of the person you want to search for.
 * @param last The last name of the person you want to search for.
 * @param maiden The maiden name of the person you want to search for.
 * @param nick The nick name of the person you want to search for.
 * @param city The city where the person you want to search for resides.
 * @param state The state where the person you want to search for resides.
 * @param country The country where the person you want to search for resides.
 * @param zip The zip code where the person you want to search for resides.
 * @param address The street address where the person you want to seach for resides.
 * @return Return 0 if no errors, otherwise return the error number.
 */
faim_export int aim_odir_name(OscarSession *sess, const char *region, const char *first, const char *middle, const char *last, const char *maiden, const char *nick, const char *city, const char *state, const char *country, const char *zip, const char *address)
{
	OscarConnection *conn;
	FlapFrame *fr;
	aim_snacid_t snacid;
	aim_tlvlist_t *tl = NULL;

	if (!sess || !(conn = aim_conn_findbygroup(sess, 0x000f)) || !region)
		return -EINVAL;

	/* Create a TLV chain, write it to the outgoing frame, then free the chain */
	aim_tlvlist_add_str(&tl, 0x001c, region);
	aim_tlvlist_add_16(&tl, 0x000a, 0x0000); /* Type of search */
	if (first)
		aim_tlvlist_add_str(&tl, 0x0001, first);
	if (last)
		aim_tlvlist_add_str(&tl, 0x0002, last);
	if (middle)
		aim_tlvlist_add_str(&tl, 0x0003, middle);
	if (maiden)
		aim_tlvlist_add_str(&tl, 0x0004, maiden);
	if (country)
		aim_tlvlist_add_str(&tl, 0x0006, country);
	if (state)
		aim_tlvlist_add_str(&tl, 0x0007, state);
	if (city)
		aim_tlvlist_add_str(&tl, 0x0008, city);
	if (nick)
		aim_tlvlist_add_str(&tl, 0x000c, nick);
	if (zip)
		aim_tlvlist_add_str(&tl, 0x000d, zip);
	if (address)
		aim_tlvlist_add_str(&tl, 0x0021, address);

	if (!(fr = flap_frame_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10+aim_tlvlist_size(&tl))))
		return -ENOMEM;
	snacid = aim_cachesnac(sess, 0x000f, 0x0002, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x000f, 0x0002, 0x0000, snacid);

	aim_tlvlist_write(&fr->data, &tl);
	aim_tlvlist_free(&tl);

	aim_tx_enqueue(sess, fr);

	return 0;
}


/**
 * Subtype 0x0002 - Submit a User Search Request
 *
 * @param sess The oscar session.
 * @param interest1 An interest you want to search for.
 * @return Return 0 if no errors, otherwise return the error number.
 */
faim_export int aim_odir_interest(OscarSession *sess, const char *region, const char *interest)
{
	OscarConnection *conn;
	FlapFrame *fr;
	aim_snacid_t snacid;
	aim_tlvlist_t *tl = NULL;

	if (!sess || !(conn = aim_conn_findbygroup(sess, 0x000f)) || !region)
		return -EINVAL;

	/* Create a TLV chain, write it to the outgoing frame, then free the chain */
	aim_tlvlist_add_str(&tl, 0x001c, region);
	aim_tlvlist_add_16(&tl, 0x000a, 0x0001); /* Type of search */
	if (interest)
		aim_tlvlist_add_str(&tl, 0x0001, interest);

	if (!(fr = flap_frame_new(sess, conn, AIM_FRAMETYPE_FLAP, 0x02, 10+aim_tlvlist_size(&tl))))
		return -ENOMEM;
	snacid = aim_cachesnac(sess, 0x000f, 0x0002, 0x0000, NULL, 0);
	aim_putsnac(&fr->data, 0x000f, 0x0002, 0x0000, snacid);

	aim_tlvlist_write(&fr->data, &tl);
	aim_tlvlist_free(&tl);

	aim_tx_enqueue(sess, fr);

	return 0;
}


/**
 * Subtype 0x0003 - Receive Reply From a User Search
 *
 */
static int parseresults(OscarSession *sess, aim_module_t *mod, FlapFrame *rx, aim_modsnac_t *snac, ByteStream *bs)
{
	int ret = 0;
	aim_rxcallback_t userfunc;
	guint16 tmp, numresults;
	struct aim_odir *results = NULL;

	tmp = aimbs_get16(bs); /* Unknown */
	tmp = aimbs_get16(bs); /* Unknown */
	aim_bstream_advance(bs, tmp);

	numresults = aimbs_get16(bs); /* Number of results to follow */

	/* Allocate a linked list, 1 node per result */
	while (numresults) {
		struct aim_odir *new;
		aim_tlvlist_t *tl = aim_tlvlist_readnum(bs, aimbs_get16(bs));
		new = (struct aim_odir *)malloc(sizeof(struct aim_odir));
		new->first = aim_tlv_getstr(tl, 0x0001, 1);
		new->last = aim_tlv_getstr(tl, 0x0002, 1);
		new->middle = aim_tlv_getstr(tl, 0x0003, 1);
		new->maiden = aim_tlv_getstr(tl, 0x0004, 1);
		new->email = aim_tlv_getstr(tl, 0x0005, 1);
		new->country = aim_tlv_getstr(tl, 0x0006, 1);
		new->state = aim_tlv_getstr(tl, 0x0007, 1);
		new->city = aim_tlv_getstr(tl, 0x0008, 1);
		new->sn = aim_tlv_getstr(tl, 0x0009, 1);
		new->interest = aim_tlv_getstr(tl, 0x000b, 1);
		new->nick = aim_tlv_getstr(tl, 0x000c, 1);
		new->zip = aim_tlv_getstr(tl, 0x000d, 1);
		new->region = aim_tlv_getstr(tl, 0x001c, 1);
		new->address = aim_tlv_getstr(tl, 0x0021, 1);
		new->next = results;
		results = new;
		numresults--;
	}

	if ((userfunc = aim_callhandler(sess, rx->conn, snac->family, snac->subtype)))
		ret = userfunc(sess, rx, results);

	/* Now free everything from above */
	while (results) {
		struct aim_odir *del = results;
		results = results->next;
		free(del->first);
		free(del->last);
		free(del->middle);
		free(del->maiden);
		free(del->email);
		free(del->country);
		free(del->state);
		free(del->city);
		free(del->sn);
		free(del->interest);
		free(del->nick);
		free(del->zip);
		free(del->region);
		free(del->address);
		free(del);
	}

	return ret;
}

static int snachandler(OscarSession *sess, aim_module_t *mod, FlapFrame *rx, aim_modsnac_t *snac, ByteStream *bs)
{

	if (snac->subtype == 0x0003)
		return parseresults(sess, mod, rx, snac, bs);

	return 0;
}

faim_internal int odir_modfirst(OscarSession *sess, aim_module_t *mod)
{

	mod->family = 0x000f;
	mod->version = 0x0001;
	mod->toolid = 0x0010;
	mod->toolversion = 0x0629;
	mod->flags = 0;
	strncpy(mod->name, "odir", sizeof(mod->name));
	mod->snachandler = snachandler;

	return 0;
}
