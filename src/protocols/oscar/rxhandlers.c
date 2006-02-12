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
 * rxhandlers.c
 *
 * This file contains most all of the incoming packet handlers, along
 * with aim_rxdispatch(), the Rx dispatcher.  Queue/list management is
 * actually done in aim_rxqueue.c.
 *
 */

#include "oscar.h"
#include "peer.h"

struct aim_rxcblist_s {
	guint16 family;
	guint16 type;
	aim_rxcallback_t handler;
	guint16 flags;
	struct aim_rxcblist_s *next;
};

faim_internal aim_module_t *aim__findmodulebygroup(OscarSession *sess, guint16 group)
{
	aim_module_t *cur;

	for (cur = (aim_module_t *)sess->modlistv; cur; cur = cur->next) {
		if (cur->family == group)
			return cur;
	}

	return NULL;
}

faim_internal aim_module_t *aim__findmodule(OscarSession *sess, const char *name)
{
	aim_module_t *cur;

	for (cur = (aim_module_t *)sess->modlistv; cur; cur = cur->next) {
		if (strcmp(name, cur->name) == 0)
			return cur;
	}

	return NULL;
}

faim_internal int aim__registermodule(OscarSession *sess, int (*modfirst)(OscarSession *, aim_module_t *))
{
	aim_module_t *mod;

	if (!sess || !modfirst)
		return -1;

	if (!(mod = malloc(sizeof(aim_module_t))))
		return -1;
	memset(mod, 0, sizeof(aim_module_t));

	if (modfirst(sess, mod) == -1) {
		free(mod);
		return -1;
	}

	if (aim__findmodule(sess, mod->name)) {
		if (mod->shutdown)
			mod->shutdown(sess, mod);
		free(mod);
		return -1;
	}

	mod->next = (aim_module_t *)sess->modlistv;
	sess->modlistv = mod;

	gaim_debug_misc("oscar", "registered module %s (family 0x%04x, version = 0x%04x, tool 0x%04x, tool version 0x%04x)\n", mod->name, mod->family, mod->version, mod->toolid, mod->toolversion);

	return 0;
}

faim_internal void aim__shutdownmodules(OscarSession *sess)
{
	aim_module_t *cur;

	for (cur = (aim_module_t *)sess->modlistv; cur; ) {
		aim_module_t *tmp;

		tmp = cur->next;

		if (cur->shutdown)
			cur->shutdown(sess, cur);

		free(cur);

		cur = tmp;
	}

	sess->modlistv = NULL;

	return;
}

static int consumesnac(OscarSession *sess, FlapFrame *rx)
{
	aim_module_t *cur;
	aim_modsnac_t snac;

	if (aim_bstream_empty(&rx->data) < 10)
		return 0;

	snac.family = aimbs_get16(&rx->data);
	snac.subtype = aimbs_get16(&rx->data);
	snac.flags = aimbs_get16(&rx->data);
	snac.id = aimbs_get32(&rx->data);

	/* SNAC flags are apparently uniform across all SNACs, so we handle them here */
	if (snac.flags & 0x0001) {
		/*
		 * This means the SNAC will be followed by another SNAC with 
		 * related information.  We don't need to do anything about 
		 * this here.
		 */
	}
	if (snac.flags & 0x8000) {
		/*
		 * This packet contains the version of the family that this SNAC is 
		 * in.  You get this when your SSI module is version 2 or higher.  
		 * For now we have no need for this, but you could always save 
		 * it as a part of aim_modnsac_t, or something.  The format is...
		 * 2 byte length of total mini-header (which is 6 bytes), then TLV 
		 * of  type 0x0001, length 0x0002, value is the 2 byte version 
		 * number
		 */
		aim_bstream_advance(&rx->data, aimbs_get16(&rx->data));
	}

	for (cur = (aim_module_t *)sess->modlistv; cur; cur = cur->next) {

		if (!(cur->flags & AIM_MODFLAG_MULTIFAMILY) &&
				(cur->family != snac.family))
			continue;

		if (cur->snachandler(sess, cur, rx, &snac, &rx->data))
			return 1;

	}

	return 0;
}

static int consumenonsnac(OscarSession *sess, FlapFrame *rx, guint16 family, guint16 subtype)
{
	aim_module_t *cur;
	aim_modsnac_t snac;

	snac.family = family;
	snac.subtype = subtype;
	snac.flags = snac.id = 0;

	for (cur = (aim_module_t *)sess->modlistv; cur; cur = cur->next) {

		if (!(cur->flags & AIM_MODFLAG_MULTIFAMILY) &&
				(cur->family != snac.family))
			continue;

		if (cur->snachandler(sess, cur, rx, &snac, &rx->data))
			return 1;

	}

	return 0;
}

static int negchan_middle(OscarSession *sess, FlapFrame *fr)
{
	aim_tlvlist_t *tlvlist;
	char *msg = NULL;
	guint16 code = 0;
	aim_rxcallback_t userfunc;
	int ret = 1;

	if (aim_bstream_empty(&fr->data) == 0) {
		/* XXX should do something with this */
		return 1;
	}

	/* Used only by the older login protocol */
	/* XXX remove this special case? */
	if (fr->conn->type == AIM_CONN_TYPE_AUTH)
		return consumenonsnac(sess, fr, 0x0017, 0x0003);

	tlvlist = aim_tlvlist_read(&fr->data);

	if (aim_tlv_gettlv(tlvlist, 0x0009, 1))
		code = aim_tlv_get16(tlvlist, 0x0009, 1);

	if (aim_tlv_gettlv(tlvlist, 0x000b, 1))
		msg = aim_tlv_getstr(tlvlist, 0x000b, 1);

	if ((userfunc = aim_callhandler(sess, fr->conn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR)))
		ret = userfunc(sess, fr, code, msg);

	aim_tlvlist_free(&tlvlist);

	free(msg);

	return ret;
}

/*
 * Bleck functions get called when there's no non-bleck functions
 * around to cleanup the mess...
 */
static int bleck(OscarSession *sess, FlapFrame *frame, ...)
{
	guint16 family, subtype;
	guint16 maxf, maxs;

	static const char *channels[6] = {
		"Invalid (0)",
		"FLAP Version",
		"SNAC",
		"Invalid (3)",
		"Negotiation",
		"FLAP NOP"
	};
	static const int maxchannels = 5;
	
	/* XXX: this is ugly. and big just for debugging. */
	static const char *literals[14][25] = {
		{"Invalid", 
		 NULL
		},
		{"General", 
		 "Invalid",
		 "Error",
		 "Client Ready",
		 "Server Ready",
		 "Service Request",
		 "Redirect",
		 "Rate Information Request",
		 "Rate Information",
		 "Rate Information Ack",
		 NULL,
		 "Rate Information Change",
		 "Server Pause",
		 NULL,
		 "Server Resume",
		 "Request Personal User Information",
		 "Personal User Information",
		 "Evil Notification",
		 NULL,
		 "Migration notice",
		 "Message of the Day",
		 "Set Privacy Flags",
		 "Well Known URL",
		 "NOP"
		},
		{"Location", 
		 "Invalid",
		 "Error",
		 "Request Rights",
		 "Rights Information", 
		 "Set user information", 
		 "Request User Information", 
		 "User Information", 
		 "Watcher Sub Request",
		 "Watcher Notification"
		},
		{"Buddy List Management", 
		 "Invalid", 
		 "Error", 
		 "Request Rights",
		 "Rights Information",
		 "Add Buddy", 
		 "Remove Buddy", 
		 "Watcher List Query", 
		 "Watcher List Response", 
		 "Watcher SubRequest", 
		 "Watcher Notification", 
		 "Reject Notification", 
		 "Oncoming Buddy", 
		 "Offgoing Buddy"
		},
		{"Messeging", 
		 "Invalid",
		 "Error", 
		 "Add ICBM Parameter",
		 "Remove ICBM Parameter", 
		 "Request Parameter Information",
		 "Parameter Information",
		 "Outgoing Message", 
		 "Incoming Message",
		 "Evil Request",
		 "Evil Reply", 
		 "Missed Calls",
		 "Message Error", 
		 "Host Ack"
		},
		{"Advertisements", 
		 "Invalid", 
		 "Error", 
		 "Request Ad",
		 "Ad Data (GIFs)"
		},
		{"Invitation / Client-to-Client", 
		 "Invalid",
		 "Error",
		 "Invite a Friend",
		 "Invitation Ack"
		},
		{"Administrative", 
		 "Invalid",
		 "Error",
		 "Information Request",
		 "Information Reply",
		 "Information Change Request",
		 "Information Chat Reply",
		 "Account Confirm Request",
		 "Account Confirm Reply",
		 "Account Delete Request",
		 "Account Delete Reply"
		},
		{"Popups", 
		 "Invalid",
		 "Error",
		 "Display Popup"
		},
		{"BOS", 
		 "Invalid",
		 "Error",
		 "Request Rights",
		 "Rights Response",
		 "Set group permission mask",
		 "Add permission list entries",
		 "Delete permission list entries",
		 "Add deny list entries",
		 "Delete deny list entries",
		 "Server Error"
		},
		{"User Lookup", 
		 "Invalid",
		 "Error",
		 "Search Request",
		 "Search Response"
		},
		{"Stats", 
		 "Invalid",
		 "Error",
		 "Set minimum report interval",
		 "Report Events"
		},
		{"Translate", 
		 "Invalid",
		 "Error",
		 "Translate Request",
		 "Translate Reply",
		},
		{"Chat Navigation", 
		 "Invalid",
		 "Error",
		 "Request rights",
		 "Request Exchange Information",
		 "Request Room Information",
		 "Request Occupant List",
		 "Search for Room",
		 "Outgoing Message", 
		 "Incoming Message",
		 "Evil Request", 
		 "Evil Reply", 
		 "Chat Error",
		}
	};

	maxf = sizeof(literals) / sizeof(literals[0]);
	maxs = sizeof(literals[0]) / sizeof(literals[0][0]);

	if (frame->hdr.flap.channel == 0x02) {

		family = aimbs_get16(&frame->data);
		subtype = aimbs_get16(&frame->data);
		
		if ((family < maxf) && (subtype+1 < maxs) && (literals[family][subtype] != NULL))
			gaim_debug_misc("oscar", "bleck: channel %s: null handler for %04x/%04x (%s)\n", channels[frame->hdr.flap.channel], family, subtype, literals[family][subtype+1]);
		else
			gaim_debug_misc("oscar", "bleck: channel %s: null handler for %04x/%04x (no literal)\n", channels[frame->hdr.flap.channel], family, subtype);
	} else {

		if (frame->hdr.flap.channel <= maxchannels)
			gaim_debug_misc("oscar", "bleck: channel %s (0x%02x)\n", channels[frame->hdr.flap.channel], frame->hdr.flap.channel);
		else
			gaim_debug_misc("oscar", "bleck: unknown channel 0x%02x\n", frame->hdr.flap.channel);

	}
		
	return 1;
}

faim_export int aim_conn_addhandler(OscarSession *sess, OscarConnection *conn, guint16 family, guint16 type, aim_rxcallback_t newhandler, guint16 flags)
{
	struct aim_rxcblist_s *newcb;

	if (!conn)
		return -1;

	gaim_debug_misc("oscar", "aim_conn_addhandler: adding for %04x/%04x\n", family, type);

	if (!(newcb = (struct aim_rxcblist_s *)calloc(1, sizeof(struct aim_rxcblist_s))))
		return -1;

	newcb->family = family;
	newcb->type = type;
	newcb->flags = flags;
	newcb->handler = newhandler ? newhandler : bleck;
	newcb->next = NULL;

	if (!conn->handlerlist)
		conn->handlerlist = (void *)newcb;
	else {
		struct aim_rxcblist_s *cur;

		for (cur = (struct aim_rxcblist_s *)conn->handlerlist; cur->next; cur = cur->next)
			;
		cur->next = newcb;
	}

	return 0;
}

faim_export int aim_clearhandlers(OscarConnection *conn)
{
	struct aim_rxcblist_s *cur;

	if (!conn)
		return -1;

	for (cur = (struct aim_rxcblist_s *)conn->handlerlist; cur; ) {
		struct aim_rxcblist_s *tmp;

		tmp = cur->next;
		free(cur);
		cur = tmp;
	}
	conn->handlerlist = NULL;

	return 0;
}

faim_internal aim_rxcallback_t aim_callhandler(OscarSession *sess, OscarConnection *conn, guint16 family, guint16 type)
{
	struct aim_rxcblist_s *cur;

	if (!conn)
		return NULL;

	/* gaim_debug_misc("oscar", "aim_callhandler: calling for %04x/%04x\n", family, type); */

	for (cur = (struct aim_rxcblist_s *)conn->handlerlist; cur; cur = cur->next) {
		if ((cur->family == family) && (cur->type == type))
			return cur->handler;
	}

	return NULL;
}

faim_internal void aim_clonehandlers(OscarSession *sess, OscarConnection *dest, OscarConnection *src)
{
	struct aim_rxcblist_s *cur;

	for (cur = (struct aim_rxcblist_s *)src->handlerlist; cur; cur = cur->next) {
		aim_conn_addhandler(sess, dest, cur->family, cur->type,
						cur->handler, cur->flags);
	}

	return;
}

faim_internal int aim_callhandler_noparam(OscarSession *sess, OscarConnection *conn,guint16 family, guint16 type, FlapFrame *ptr)
{
	aim_rxcallback_t userfunc;

	if ((userfunc = aim_callhandler(sess, conn, family, type)))
		return userfunc(sess, ptr);

	return 1; /* XXX */
}

/*
 * aim_rxdispatch()
 *
 * Basically, heres what this should do:
 *   1) Determine correct packet handler for this packet
 *   2) Mark the packet handled (so it can be dequeued in purge_queue())
 *   3) Send the packet to the packet handler
 *   4) Go to next packet in the queue and start over
 *   5) When done, run purge_queue() to purge handled commands
 *
 * TODO: Clean up.
 * TODO: More support for mid-level handlers.
 * TODO: Allow for NULL handlers.
 *
 */
faim_export void aim_rxdispatch(OscarSession *sess)
{
	int i;
	FlapFrame *cur;

	for (cur = sess->queue_incoming, i = 0; cur; cur = cur->next, i++) {

		/*
		 * XXX: This is still fairly ugly.
		 */

		if (cur->handled)
			continue;

		if (cur->hdrtype == AIM_FRAMETYPE_FLAP) {
			if (cur->hdr.flap.channel == 0x01) {
				cur->handled = aim_callhandler_noparam(sess, cur->conn, AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_FLAPVER, cur); /* XXX use consumenonsnac */
				continue;

			} else if (cur->hdr.flap.channel == 0x02) {
				if ((cur->handled = consumesnac(sess, cur)))
					continue;

			} else if (cur->hdr.flap.channel == 0x04) {
				cur->handled = negchan_middle(sess, cur);
				continue;

			} else if (cur->hdr.flap.channel == 0x05) {

			}

		} else if (cur->hdrtype == AIM_FRAMETYPE_OFT) {
			if (cur->conn->type == AIM_CONN_TYPE_RENDEZVOUS) {
				aim_rxdispatch_rendezvous(sess, cur);
				cur->handled = 1;
				continue;

			} else if (cur->conn->type == AIM_CONN_TYPE_LISTENER) {
				/* not possible */
				gaim_debug_misc("oscar", "rxdispatch called on LISTENER connection!\n");
				cur->handled = 1;
				continue;
			}
		}

		if (!cur->handled) {
			consumenonsnac(sess, cur, 0xffff, 0xffff); /* last chance! */
			cur->handled = 1;
		}
	}

	/*
	 * This doesn't have to be called here.  It could easily be done
	 * by a separate thread or something. It's an administrative operation,
	 * and can take a while. Though the less you call it the less memory
	 * you'll have :)
	 */
	aim_purge_rxqueue(sess);

	return;
}

faim_internal int aim_parse_unknown(OscarSession *sess, FlapFrame *frame, ...)
{
	int i;

	gaim_debug_misc("oscar", "\nRecieved unknown packet:");

	for (i = 0; aim_bstream_empty(&frame->data); i++) {
		if ((i % 8) == 0)
			gaim_debug_misc("oscar", "\n\t");

		gaim_debug_misc("oscar", "0x%2x ", aimbs_get8(&frame->data));
	}

	gaim_debug_misc("oscar", "\n\n");

	return 1;
}
