/**
 * @file simple.c
 *
 * gaim
 *
 * Copyright (C) 2005 Thomas Butter <butter@uni-mannheim.de>
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

#include "internal.h"

#include "accountopt.h"
#include "blist.h"
#include "conversation.h"
#include "debug.h"
#include "notify.h"
#include "prpl.h"
#include "plugin.h"
#include "util.h"
#include "version.h"
#include "network.h"
#include "xmlnode.h"

#include "simple.h"
#include "sipmsg.h"
#include "srvresolve.h"

static char *gentag() {
	return g_strdup_printf("%04d%04d", rand() & 0xFFFF, rand() & 0xFFFF);
}

static char *genbranch() {
	        return g_strdup_printf("z9hG4bK%04X%04X%04X%04X%04X",
                        rand() & 0xFFFF,
                        rand() & 0xFFFF,
                        rand() & 0xFFFF,
                        rand() & 0xFFFF,
                        rand() & 0xFFFF);
}

static char *gencallid() {
	        return g_strdup_printf("%04Xg%04Xa%04Xi%04Xm%04Xt%04Xb%04Xx%04Xx",
                        rand() & 0xFFFF,
                        rand() & 0xFFFF,
                        rand() & 0xFFFF,
                        rand() & 0xFFFF,
                        rand() & 0xFFFF,
                        rand() & 0xFFFF,
                        rand() & 0xFFFF,
                        rand() & 0xFFFF);
}

static const char *simple_list_icon(GaimAccount *a, GaimBuddy *b) {
	        return "simple";
}

static void simple_keep_alive(GaimConnection *gc) {
	struct simple_account_data *sip = gc->proto_data;
	if(sip->udp) { // in case of UDP send a packet only with a 0 byte to
			// stay in the NAT table
		gchar buf[2]={0,0};
		gaim_debug_info("simple", "sending keep alive\n");
		sendto(sip->fd, buf, 1, 0, (struct sockaddr*)&sip->serveraddr, sizeof(struct sockaddr_in));
	}
	return; 
}

static gboolean process_register_response(struct simple_account_data *sip, struct sipmsg *msg, struct transaction *tc);
static void send_notify(struct simple_account_data *sip, struct simple_watcher *);

static void send_publish(struct simple_account_data *sip);

static void do_notifies(struct simple_account_data *sip) {
	GSList *tmp = sip->watcher;
	gaim_debug_info("simple", "do_notifies()\n");
	if((sip->republish != -1) || sip->republish < time(NULL))
		send_publish(sip);

	while(tmp) {
		gaim_debug_info("simple", "notifying %s\n", ((struct simple_watcher*)tmp->data)->name);
		send_notify(sip, tmp->data);
		tmp = tmp->next;
	}
}

static void simple_set_status(GaimAccount *account, GaimStatus *status) {
        GaimStatusPrimitive primitive = gaim_status_type_get_primitive(gaim_status_get_type(status));
	struct simple_account_data *sip = NULL;
	if (!gaim_status_is_active(status))
		return;

	if(account->gc) sip = account->gc->proto_data;
	if(sip) {
		if(sip->status) g_free(sip->status);
		if(primitive == GAIM_STATUS_AVAILABLE) sip->status = g_strdup("available");
		else sip->status = g_strdup("busy");

		do_notifies(sip);
	}
	if ((primitive != GAIM_STATUS_OFFLINE) 
			&& (!gaim_account_is_connected(account))) {
		gaim_account_connect(account);
	}
}

static struct sip_connection *connection_find(struct simple_account_data *sip, int fd) {
	struct sip_connection *ret = NULL;
	GSList *entry = sip->openconns;
	while(entry) {
		ret = entry->data;
		if(ret->fd == fd) return ret;
		entry = entry->next;
	}
	return NULL;
}

static struct simple_watcher *watcher_find(struct simple_account_data *sip, gchar *name) {
	struct simple_watcher *watcher;
	GSList *entry = sip->watcher;
	while(entry) {
		watcher = entry->data;
		if(!strcmp(name, watcher->name)) return watcher;
		entry = entry->next;
	}
	return NULL;
}

static struct simple_watcher *watcher_create(struct simple_account_data *sip, gchar *name, gchar *callid, gchar *ourtag, gchar *theirtag) {
	struct simple_watcher *watcher = g_new0(struct simple_watcher,1);
	watcher->name = g_strdup(name);
	watcher->dialog.callid = g_strdup(callid);
	watcher->dialog.ourtag = g_strdup(ourtag);
	watcher->dialog.theirtag = g_strdup(theirtag);
	sip->watcher = g_slist_append(sip->watcher, watcher);
	return watcher;
}

static void watcher_remove(struct simple_account_data *sip, gchar *name) {
	struct simple_watcher *watcher = watcher_find(sip, name);
	sip->watcher = g_slist_remove(sip->watcher, watcher);
	g_free(watcher->name);
	g_free(watcher->dialog.callid);
	g_free(watcher->dialog.ourtag);
	g_free(watcher->dialog.theirtag);
	g_free(watcher);
}

static struct sip_connection *connection_create(struct simple_account_data *sip, int fd) {
	struct sip_connection *ret = g_new0(struct sip_connection,1);
	ret->fd = fd;
	sip->openconns = g_slist_append(sip->openconns, ret);
	return ret;
}

static void connection_remove(struct simple_account_data *sip, int fd) {
	struct sip_connection *conn = connection_find(sip, fd);
	sip->openconns = g_slist_remove(sip->openconns, conn);
	if(conn->inputhandler) gaim_input_remove(conn->inputhandler);
	if(conn->inbuf) g_free(conn->inbuf);
	g_free(conn);
}

static void simple_add_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group)
{
	struct simple_account_data *sip = (struct simple_account_data *)gc->proto_data;
	struct simple_buddy *b;
	if(strncmp("sip:", buddy->name,4)) {
		gchar *buf = g_strdup_printf(_("Could not add the buddy %s because every simple user has to start with 'sip:'."), buddy->name);
		gaim_notify_error(gc, NULL, _("Unable To Add"), buf);
		g_free(buf);
		gaim_blist_remove_buddy(buddy);
		return;
	}
       	if(!g_hash_table_lookup(sip->buddies, buddy->name)) {
		b = g_new0(struct simple_buddy, 1);
	        gaim_debug_info("simple","simple_add_buddy %s\n",buddy->name);
		b->name = g_strdup(buddy->name);
		g_hash_table_insert(sip->buddies, b->name, b);
	} else {
		gaim_debug_info("simple","buddy %s already in internal list\n", buddy->name);
	}
}

static void simple_get_buddies(GaimConnection *gc) {
	GaimBlistNode *gnode, *cnode, *bnode;

	gaim_debug_info("simple","simple_get_buddies\n");
	
	for(gnode = gaim_get_blist()->root; gnode; gnode = gnode->next) {
		if(!GAIM_BLIST_NODE_IS_GROUP(gnode)) continue;
		for(cnode = gnode->child; cnode; cnode = cnode->next) {
			if(!GAIM_BLIST_NODE_IS_CONTACT(cnode)) continue;
			for(bnode = cnode->child; bnode; bnode = bnode->next) {
				if(!GAIM_BLIST_NODE_IS_BUDDY(bnode)) continue;
				if(((GaimBuddy*)bnode)->account == gc->account)
					simple_add_buddy(gc, (GaimBuddy*)bnode, (GaimGroup *)gnode);
			}
		}
	}
}

static void simple_remove_buddy(GaimConnection *gc, GaimBuddy *buddy, GaimGroup *group)
{
	struct simple_account_data *sip = (struct simple_account_data *)gc->proto_data;
	struct simple_buddy *b = g_hash_table_lookup(sip->buddies, buddy->name);
	g_hash_table_remove(sip->buddies, buddy->name);
	g_free(b->name);
	g_free(b);
}

static GList *simple_status_types(GaimAccount *acc) {
	GaimStatusType *type;
	GList *types = NULL;
	gaim_debug_info("simple","called simple_status_types\n");
	type = gaim_status_type_new(GAIM_STATUS_OFFLINE, "offline", _("Offline"), FALSE);
	types = g_list_append(types, type);

	type = gaim_status_type_new(GAIM_STATUS_ONLINE, "online", _("Online"), FALSE);
	types = g_list_append(types, type);

        type = gaim_status_type_new_with_attrs(
	                GAIM_STATUS_AVAILABLE, "available", _("Available"),
		                TRUE, TRUE, FALSE,
                "message", _("Message"), gaim_value_new(GAIM_TYPE_STRING), NULL);
        types = g_list_append(types, type);
	
	return types;
}

static void simple_input_cb(gpointer data, gint source, GaimInputCondition cond);

static void send_later_cb(gpointer data, gint source, GaimInputCondition cond) {
	GaimConnection *gc = data;
	struct simple_account_data *sip = gc->proto_data;
	struct sip_connection *conn;

	if( source < 0 ) {
		gaim_connection_error(gc,"Could not connect");
		return;
	}

	sip->fd = source;
	sip->connecting = 0;
	write(sip->fd, sip->sendlater, strlen(sip->sendlater));
	conn = connection_create(sip, source);
	conn->inputhandler = gaim_input_add(sip->fd, GAIM_INPUT_READ, simple_input_cb, gc);
	g_free(sip->sendlater);
	sip->sendlater = 0;
}


static void sendlater(GaimConnection *gc, const char *buf) {
	struct getserver_return *serveradr;
	struct simple_account_data *sip = gc->proto_data;
	int error = 0;
	if(!sip->connecting) {
	        serveradr = getserver(sip->servername, "_sip._tcp");
	        gaim_debug_info("simple","connecting to %s port %d", serveradr->name, serveradr->port);
	        error = gaim_proxy_connect(sip->account, serveradr->name, serveradr->port, send_later_cb, gc);
	        if(error) {
	                gaim_connection_error(gc, _("Couldn't create socket"));
	        }
		sip->connecting = 1;
	}
	if(sip->sendlater) {
		gchar *old = sip->sendlater;
		sip->sendlater = g_strdup_printf("%s\r\n%s",old, buf);
	} else {
		sip->sendlater = g_strdup(buf);
	}
}

static int sendout_pkt(GaimConnection *gc, const char *buf) {
	struct simple_account_data *sip = gc->proto_data;
	time_t currtime = time(NULL);
	int ret = 0;
	
	gaim_debug(GAIM_DEBUG_MISC, "simple", "\n\nsending - %s\n######\n%s\n######\n\n", ctime(&currtime), buf);
	if(sip->udp) {
		if(sendto(sip->fd, buf, strlen(buf), 0, (struct sockaddr*)&sip->serveraddr, sizeof(struct sockaddr_in)) < strlen(buf)) {
			gaim_debug_info("simple", "could not send packet\n");
		}
	} else {
		if(sip->fd <0 ) {
			sendlater(gc, buf);
		      	return 0;
		}
		ret = write(sip->fd, buf, strlen(buf));
		if(ret < 0) {
			sendlater(gc,buf);
			return 0;
		}
	}
	return ret;
}

static void sendout_sipmsg(struct simple_account_data *sip, struct sipmsg *msg) {
	gchar *oldstr;
	gchar *outstr = g_strdup_printf("%s %s SIP/2.0\r\n", msg->method, msg->target);
	gchar *name;
	gchar *value;
	GSList *tmp = msg->headers;
	while(tmp) {
		oldstr = outstr;
                name = ((struct siphdrelement*)(tmp->data))->name;
                value = ((struct siphdrelement*)(tmp->data))->value;
                outstr = g_strdup_printf("%s%s: %s\r\n",oldstr, name, value);
		g_free(oldstr);
		tmp = g_slist_next(tmp);
	}
	oldstr = outstr;
	if(msg->body) outstr = g_strdup_printf("%s\r\n%s", outstr, msg->body);
	else outstr = g_strdup_printf("%s\r\n", outstr);
	g_free(oldstr);
	sendout_pkt(sip->gc, outstr);
        g_free(outstr);
}

static void send_sip_response(GaimConnection *gc, struct sipmsg *msg, int code, char *text, char *body) {
	GSList *tmp = msg->headers;
	char *oldstr;
	char *name;
	char *value;
	char *outstr = g_strdup_printf("SIP/2.0 %d %s\r\n",code, text);
	while(tmp) {
		oldstr = outstr;
		name = ((struct siphdrelement*)(tmp->data))->name;
		value = ((struct siphdrelement*)(tmp->data))->value;
		outstr = g_strdup_printf("%s%s: %s\r\n",oldstr, name, value);
		g_free(oldstr);
		tmp = g_slist_next(tmp);
	}
	oldstr = outstr;
	if(body) outstr = g_strdup_printf("%s\r\n%s",outstr,body);
	else outstr = g_strdup_printf("%s\r\n",outstr);
	g_free(oldstr);
	sendout_pkt(gc, outstr);
	g_free(outstr);
}

static void transactions_remove(struct simple_account_data *sip, struct transaction *trans) {
	if(trans->msg) sipmsg_free(trans->msg);
	sip->transactions = g_slist_remove(sip->transactions, trans);
	g_free(trans);
}

static void transactions_add_buf(struct simple_account_data *sip, gchar *buf, void *callback) {
	struct transaction *trans = g_new0(struct transaction, 1);
	trans->time = time(NULL);
	trans->msg = sipmsg_parse_msg(buf);
	trans->cseq = sipmsg_find_header(trans->msg, "CSeq");
	trans->callback = callback;
	sip->transactions = g_slist_append(sip->transactions, trans);
}

static struct transaction *transactions_find(struct simple_account_data *sip, struct sipmsg *msg) {
	struct transaction *trans;
	GSList *transactions = sip->transactions;
	gchar *cseq = sipmsg_find_header(msg, "CSeq");

	while(transactions) {
		trans = transactions->data;
		if(!strcmp(trans->cseq, cseq)) {
			return trans;
		}
		transactions = transactions->next;
	}
	
	return (struct transaction *)NULL;
}

static void send_sip_request(GaimConnection *gc, gchar *method, gchar *url, gchar *to, gchar *addheaders, gchar *body, struct sip_dialog *dialog, TransCallback tc) {
	struct simple_account_data *sip = gc->proto_data;
	char *callid= dialog ? g_strdup(dialog->callid) : gencallid();
	char *auth="";
	char *addh="";
	gchar *branch = genbranch();
	char *buf;
	HASHHEX response;
	HASHHEX HA2;	
       
	if(addheaders) addh=addheaders;
	if(sip->registrar.nonce && !strcmp(method,"REGISTER") && sip->registrar.fouroseven<4) {
		gchar noncecount[90];
		sprintf(noncecount,"%08d",sip->registrar.nc++);
		DigestCalcResponse(sip->registrar.HA1, sip->registrar.nonce, noncecount, "", "", method, url, HA2, response);
		gaim_debug(GAIM_DEBUG_MISC, "simple", "response %s", response);	
		auth = g_strdup_printf("Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", nc=\"%s\", response=\"%s\"\r\n",sip->username, sip->registrar.realm, sip->registrar.nonce, url, noncecount, response);
		gaim_debug(GAIM_DEBUG_MISC, "simple", "header %s", auth);
	}

	if(sip->proxy.nonce && strcmp(method,"REGISTER")) {
		gchar noncecount[90];
		sprintf(noncecount, "%08d", sip->proxy.nc++);
		DigestCalcResponse(sip->proxy.HA1, sip->proxy.nonce, noncecount, "", "", method, url, HA2, response);
		gaim_debug(GAIM_DEBUG_MISC, "simple", "response %s", response);	
		auth = g_strdup_printf("Proxy-Authorization: Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", nc=\"%s\", response=\"%s\"\r\n",sip->username, sip->proxy.realm, sip->proxy.nonce, url, noncecount, response);
		gaim_debug(GAIM_DEBUG_MISC, "simple", "header %s", auth);
	}

	
	buf = g_strdup_printf("%s %s SIP/2.0\r\n"
			"Via: SIP/2.0/%s %s:%d;branch=%s\r\n"
			"From: <sip:%s@%s>;tag=%s\r\n"
			"To: <%s>%s%s\r\n"
			"Max-Forwards: 10\r\n"
			"CSeq: %d %s\r\n"
			"User-Agent: Gaim SIP/SIMPLE Plugin\r\n"
			"Call-ID: %s\r\n"
			"%s%s"
			"Content-Length: %d\r\n\r\n%s",
			method,
			url,
			sip->udp ? "UDP" : "TCP",
			sip->ip,	
			sip->listenport,
			branch,
			sip->username,
			sip->servername,
			dialog ? dialog->ourtag : gentag(),
			to,
			dialog ? ";tag=" : "",
			dialog ? dialog->theirtag : "",
			++sip->cseq,
			method,
			callid,
			auth,
			addh,
			strlen(body),
			body);
	g_free(branch);
	g_free(callid);

	// add to running transactions
	
	transactions_add_buf(sip, buf, tc);
	
	sendout_pkt(gc,buf);
	
	g_free(buf);
}

static void do_register_exp(struct simple_account_data *sip, int expire) {
	sip->registerstatus = 1;
	
	char *uri = g_strdup_printf("sip:%s",sip->servername);
	char *to = g_strdup_printf("sip:%s@%s",sip->username,sip->servername);
	char *contact = g_strdup_printf("Contact: <sip:%s@%s:%d;transport=%s>;methods=\"MESSAGE, SUBSCRIBE, NOTIFY\"\r\nExpires: %d\r\n", sip->username, sip->ip, sip->listenport, sip->udp ? "udp" : "tcp", expire);
	
	// allow one auth try per register
	sip->proxy.fouroseven = 0;
	sip->registrar.fouroseven = 0;
	
	if(expire) {
		sip->reregister = time(NULL) + expire - 50;
	} else {
		sip->reregister = time(NULL) + 600;
	}
	send_sip_request(sip->gc,"REGISTER",uri,to, contact, "", NULL, process_register_response);
	g_free(uri);
	g_free(to);
}

static void do_register(struct simple_account_data *sip) {
	do_register_exp(sip, sip->registerexpire);
}

static gchar *parse_from(gchar *hdr) {
	gchar *from = hdr;
	gchar *tmp;

	if(!from) return NULL;
	gaim_debug_info("simple", "parsing address out of %s\n",from);	
	tmp = strchr(from, '<');

	// i hate the different SIP UA behaviours...
	if(tmp) { // sip address in <...>
		from = tmp+1;
		tmp = strchr(from,'>');
		if(tmp) {
			from = g_strndup(from,tmp-from);
		} else {
			gaim_debug_info("simple", "found < without > in From\n");
			return NULL;
		}
	} else {
		tmp = strchr(from, ';');
		if(tmp) {
			from = g_strndup(from,tmp-from);
		}
	}
	gaim_debug_info("simple", "got %s\n",from);
	return from;
}

static gboolean process_subscribe_response(struct simple_account_data *sip, struct sipmsg *msg, struct transaction *tc) {
	gchar *to = parse_from(sipmsg_find_header(tc->msg,"To")); // cant be NULL since it is our own msg
	
	
	if(msg->response==200 || msg->response==202) {
		return TRUE;
	}

	// we can not subscribe -> user is offline (TODO unknown status?)
	
	gaim_prpl_got_user_status(sip->account, to, "offline", NULL);
	g_free(to);
	return TRUE;
}

static void simple_subscribe(struct simple_account_data *sip, struct simple_buddy *buddy) {
	gchar *contact = "Expires: 900\r\nAccept: application/pidf+xml\r\nEvent: presence\r\n";
	gchar *to;
       	if(strstr(buddy->name,"sip:")) to = g_strdup(buddy->name);
	else to = g_strdup_printf("sip:%s",buddy->name);
	// subscribe to buddy presence
	// we dont need to know the status so we do not need a callback
	 
	send_sip_request(sip->gc, "SUBSCRIBE",to, to, contact, "", NULL, process_subscribe_response);
	
	g_free(to);
	
	// resubscribe before of subscription expires
	// add some jitter
	buddy->resubscribe = time(NULL)+550+(rand()%50);
}

static void simple_buddy_resub(char *name, struct simple_buddy *buddy, struct simple_account_data *sip) {
	time_t curtime = time(NULL);

	if(buddy->resubscribe < curtime) {
        	gaim_debug(GAIM_DEBUG_MISC, "simple", "simple_buddy_resub %s\n",name);
		simple_subscribe(sip, buddy);
	}
}

static gboolean resend_timeout(struct simple_account_data *sip) {
	GSList *tmp = sip->transactions;
	time_t currtime = time(NULL);
	while(tmp) {
		struct transaction *trans = tmp->data;
		tmp = tmp->next;
		gaim_debug_info("simple", "have open transaction age: %d\n", currtime- trans->time);
		if((currtime - trans->time > 5) && trans->retries >= 1) {
			// TODO 408
		} else {
			if((currtime - trans->time > 2) && trans->retries == 0) {
				trans->retries++;
				sendout_sipmsg(sip, trans->msg);
			}
		}
	}
	return TRUE;
}

static gboolean register_timeout(struct simple_account_data *sip) {
	GSList *tmp;
	time_t curtime = time(NULL);
	// register again if first registration expires
	if(sip->reregister < curtime) {
		do_register(sip);
	}
	
	// check for every subscription if we need to resubscribe
	g_hash_table_foreach(sip->buddies, (GHFunc)simple_buddy_resub, (gpointer)sip);

	// remove a timed out suscriber
	
	tmp = sip->watcher;
	while(tmp) {
		struct simple_watcher *watcher = tmp->data;
		if(watcher->expire < curtime) {
			watcher_remove(sip, watcher->name);
			tmp = sip->watcher;
		}
		if(tmp) tmp = tmp->next;
	}
	
	return TRUE;
}

static void simple_send_message(struct simple_account_data *sip, char *to, char *msg, char *type) {
	gchar *hdr;
       	if(type) {
		hdr = g_strdup_printf("Content-Type: %s\r\n",type);
	} else {
		hdr = g_strdup("Content-Type: text/plain\r\n");
	}
	send_sip_request(sip->gc, "MESSAGE", to, to, hdr, msg, NULL, NULL);
	g_free(hdr);
}

static int simple_im_send(GaimConnection *gc, const char *who, const char *what, GaimConvImFlags flags) {
	struct simple_account_data *sip = gc->proto_data;
	char *to = g_strdup(who);
	char *text = g_strdup(what);
	simple_send_message(sip, to, text, NULL);
	g_free(to);
	g_free(text);
	return 1;
}

static void process_incoming_message(struct simple_account_data *sip, struct sipmsg *msg) {
	gchar *from;
	gchar *contenttype;
	gboolean found = FALSE;

	from = parse_from(sipmsg_find_header(msg, "From"));

	if(!from) return;
	
	gaim_debug(GAIM_DEBUG_MISC, "simple", "got message from %s: %s\n", from, msg->body);

	contenttype = sipmsg_find_header(msg, "Content-Type");
	if(!contenttype || !strncmp(contenttype, "text/plain", 10) || !strncmp(contenttype, "text/html", 9)) {	
		serv_got_im(sip->gc, from, msg->body, 0, time(NULL));
		send_sip_response(sip->gc, msg, 200, "OK", NULL);
		found = TRUE;
	}
	if(!strncmp(contenttype, "application/im-iscomposing+xml",30)) {	
	        xmlnode *isc = xmlnode_from_str(msg->body, msg->bodylen);
		xmlnode *state;
		gchar *statedata;

		if(!isc) {
	                gaim_debug_info("simple","process_incoming_message: can not parse iscomposing\n");
	                return;
	        }

	        state = xmlnode_get_child(isc, "state");

	        if(!state) {
	                gaim_debug_info("simple","process_incoming_message: no state found\n");
	                return;
	        }

	        statedata = xmlnode_get_data(state);
		if(statedata) {
	                if(strstr(statedata,"active")) serv_got_typing(sip->gc, from, 0, GAIM_TYPING);
			else serv_got_typing_stopped(sip->gc, from);
		}
		xmlnode_free(isc);
                send_sip_response(sip->gc, msg, 200, "OK", NULL);
		found = TRUE;
	}
	if(!found) {
		gaim_debug_info("simple", "got unknown mime-type");
		send_sip_response(sip->gc, msg, 415, "Unsupported media type", NULL);
	}
	g_free(from);
}

static void fill_auth(struct simple_account_data *sip, gchar *hdr, struct sip_auth *auth) {
	if(!hdr) {
		gaim_debug_info("simple", "fill_auth: hdr==NULL\n");
		return;
	}
	int i=0;
	gchar **parts = g_strsplit(hdr, " ", 0);
	while(parts[i]) {
		if(!strncmp(parts[i],"nonce",5)) {
			auth->nonce = g_strndup(parts[i]+7,strlen(parts[i]+7)-1);
		}
		if(!strncmp(parts[i],"realm",5)) {
			auth->realm = g_strndup(parts[i]+7,strlen(parts[i]+7)-2);
		}
		i++;
	}
	
	gaim_debug(GAIM_DEBUG_MISC, "simple", "nonce: %s realm: %s ", auth->nonce, auth->realm);
	
	DigestCalcHA1("md5", sip->username, auth->realm, sip->password, auth->nonce, "", auth->HA1);

	auth->nc=1;
}


gboolean process_register_response(struct simple_account_data *sip, struct sipmsg *msg, struct transaction *tc) {
	gchar *tmp;
	gaim_debug(GAIM_DEBUG_MISC, "simple", "in process register response response: %d\n", msg->response);
	switch (msg->response) {
		case 200:
			if(sip->registerstatus<3) { // registered
				send_publish(sip);	
			}
			sip->registerstatus=3;
			gaim_connection_set_state(sip->gc, GAIM_CONNECTED);
			register_timeout(sip);
			break;
		case 401:
			if(sip->registerstatus!=2) {
				tmp = sipmsg_find_header(msg, "WWW-Authenticate");
				fill_auth(sip, tmp, &sip->registrar);
				sip->registerstatus=2;
				gaim_debug(GAIM_DEBUG_MISC, "simple", "HA1: %s\n",sip->registrar.HA1);
				do_register(sip);
			}
			break;
		}
	return TRUE;
}

static void process_incoming_notify(struct simple_account_data *sip, struct sipmsg *msg) {
	gchar *from;
	gchar *fromhdr;
	gchar *tmp2;
	xmlnode *pidf;
	xmlnode *basicstatus;
	gboolean isonline = FALSE;

	fromhdr = sipmsg_find_header(msg,"From");
	from = parse_from(fromhdr);
	if(!from) return;

	pidf = xmlnode_from_str(msg->body, msg->bodylen);
	
	if(!pidf) {
		gaim_debug_info("simple","process_incoming_notify: no parseable pidf\n");
		return;
	}

	
	basicstatus = xmlnode_get_child(xmlnode_get_child(xmlnode_get_child(pidf,"tuple"),"status"), "basic");

	if(!basicstatus) {
		gaim_debug_info("simple","process_incoming_notify: no basic found\n");
		return;
	}
	
	tmp2 = xmlnode_get_data(basicstatus);
	
	if(!tmp2) {
		gaim_debug_info("simple","process_incoming_notify: no basic data found\n");
		return;
	}
	
	if(strstr(tmp2, "open")) {
		isonline = TRUE;
	}

	if(isonline) gaim_prpl_got_user_status(sip->account, from, "available", NULL);
	else gaim_prpl_got_user_status(sip->account, from, "offline", NULL);

	xmlnode_free(pidf);

	g_free(from);	
	send_sip_response(sip->gc, msg, 200, "OK", NULL);
}

static int simple_typing(GaimConnection *gc, const char *name, int typing) {
	struct simple_account_data *sip = gc->proto_data;

	gchar *xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			"<isComposing xmlns=\"urn:ietf:params:xml:ns:im-iscomposing\"\n"
			"xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"\n"
			"xsi:schemaLocation=\"urn:ietf:params:xml:ns:im-composing iscomposing.xsd\">\n"
			"<state>%s</state>\n"
			"<contenttype>text/plain</contenttype>\n"
			"<refresh>60</refresh>\n"
			"</isComposing>";
	gchar *recv = g_strdup(name);
	if(typing == GAIM_TYPING) {
		gchar *msg = g_strdup_printf(xml, "active");
		simple_send_message(sip, recv, msg, "application/im-iscomposing+xml");
		g_free(msg);
	} else {
		gchar *msg = g_strdup_printf(xml, "idle");
		simple_send_message(sip, recv, msg, "application/im-iscomposing+xml");
		g_free(msg);
	}
	g_free(recv);
	return 1;
}

static gchar *find_tag(gchar *hdr) {
	gchar *tmp = strstr(hdr, ";tag=");
	gchar *tmp2;
	if(!tmp) return NULL;
	tmp += 5;
	if((tmp2 = strchr(tmp, ';'))) {
		tmp2[0] = '\0';
		tmp = g_strdup(tmp);
		tmp2[0] = ';';
		return tmp;
	}
	return g_strdup(tmp);
}

static gchar* gen_pidf(struct simple_account_data *sip) {
	gchar *doc = g_strdup_printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		   "<presence xmlns=\"urn:ietf:params:xml:ns:pidf\"\n"
		   "xmlns:im=\"urn:ietf:params:xml:ns:pidf:im\"\n"
		   "entity=\"sip:%s@%s\">\n"
		   "<tuple id=\"bs35r9f\">\n"
			"<status>\n"
			"<basic>open</basic>\n"
			"<im:im>%s</im:im>\n"
			"</status>\n"
			"</tuple>\n"
			"</presence>",
			sip->username,
			sip->servername,
			sip->status);
	return doc;
}

static void send_notify(struct simple_account_data *sip, struct simple_watcher *watcher) {
	gchar *doc = gen_pidf(sip);
	send_sip_request(sip->gc, "NOTIFY", watcher->name, watcher->name, "Event: presence\r\nContent-Type: application/pidf+xml\r\n", doc, &watcher->dialog, NULL);
	g_free(doc);
}

static gboolean process_publish_response(struct simple_account_data *sip, struct sipmsg *msg, struct transaction *tc) {
	if(msg->response != 200) {
		// never send again
		sip->republish = -1;
	}
	return TRUE;
}

static void send_publish(struct simple_account_data *sip) {
	gchar *uri = g_strdup_printf("sip:%s@%s", sip->username, sip->servername);
	gchar *doc = gen_pidf(sip);
	send_sip_request(sip->gc, "PUBLISH", uri, uri, "Expires: 600\r\nEvent: presence\r\nContent-Type: application/pidf+xml\r\nAccept: application/pidf+xml\r\n", doc, NULL, process_publish_response);
	sip->republish = time(NULL) + 500;
	g_free(doc);
}

static void process_incoming_subscribe(struct simple_account_data *sip, struct sipmsg *msg) {
	gchar *from = parse_from(sipmsg_find_header(msg, "From"));
	gchar *theirtag = find_tag(sipmsg_find_header(msg, "From"));
	gchar *ourtag = find_tag(sipmsg_find_header(msg, "To"));
	gboolean tagadded = FALSE;
	gchar *callid = sipmsg_find_header(msg, "Call-ID");
	gchar *expire = sipmsg_find_header(msg, "Expire");
	gchar *tmp;
	struct simple_watcher *watcher = watcher_find(sip, from);
	if(!ourtag) {
		tagadded = TRUE;
		ourtag = gentag();
	}
	if(!watcher) { // new subscription
		watcher = watcher_create(sip, from, callid, ourtag, theirtag);
	}
	if(tagadded) {
		gchar *to = g_strdup_printf("%s;tag=%s", sipmsg_find_header(msg, "To"), ourtag);
		sipmsg_remove_header(msg, "To");
		sipmsg_add_header(msg, "To", to);
	}
	if(expire)
		watcher->expire = time(NULL) + strtol(expire, NULL, 10);
	else
		watcher->expire = time(NULL) + 600;
	sipmsg_remove_header(msg, "Contact");
	tmp = g_strdup_printf("<%s@%s>",sip->username, sip->servername);
	sipmsg_add_header(msg, "Contact", tmp);
	gaim_debug_info("simple","got subscribe: name %s ourtag %s theirtag %s callid %s\n", watcher->name, watcher->dialog.ourtag, watcher->dialog.theirtag, watcher->dialog.callid);
	send_sip_response(sip->gc, msg, 200, "Ok", NULL);
	g_free(tmp);
	send_notify(sip, watcher);
}

static void process_input_message(struct simple_account_data *sip, struct sipmsg *msg) {
	int found = 0;
	if( msg->response == 0 ) { // request
		if(!strcmp(msg->method, "MESSAGE")) {
			process_incoming_message(sip, msg);
			found = 1;
		}
		if(!strcmp(msg->method, "NOTIFY")) {
			process_incoming_notify(sip, msg);
			found = 1;
		}
		if(!strcmp(msg->method, "SUBSCRIBE")) {
			process_incoming_subscribe(sip, msg);
			found = 1;
		}
		if(!found) {
	                send_sip_response(sip->gc, msg, 501, "Not implemented", NULL);
		}
	} else { // response
		struct transaction *trans = transactions_find(sip, msg);
		if(trans) {
			if(msg->response == 407) {
				if(sip->proxy.fouroseven>3) return;
				sip->proxy.fouroseven++;
				// do proxy authentication

				gchar *ptmp = sipmsg_find_header(msg,"Proxy-Authenticate");
				gchar *resend;
				gchar *auth;
					
			        HASHHEX HA2;
				HASHHEX response;
				gchar noncecount[90];
				fill_auth(sip, ptmp, &sip->proxy);
				sprintf(noncecount, "%08d", sip->proxy.nc++);
	
				DigestCalcResponse(sip->proxy.HA1, sip->proxy.nonce, noncecount, "", "", trans->msg->method, trans->msg->target, HA2, response);
				gaim_debug(GAIM_DEBUG_MISC, "simple", "response %s\n", response);
				auth = g_strdup_printf("Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", nc=\"%s\", response=\"%s\"\r\n",sip->username, sip->proxy.realm, sip->proxy.nonce, trans->msg->target, noncecount, response);
				sipmsg_remove_header(msg, "Proxy-Authorization");
				sipmsg_add_header(trans->msg, "Proxy-Authorization", auth);
				g_free(auth);
				resend = sipmsg_to_string(trans->msg);
				// resend request
				sendout_pkt(sip->gc, resend);
				g_free(resend);
			} else {
				sip->proxy.fouroseven = 0;
				if(msg->response == 401) sip->registrar.fouroseven++;
				else sip->registrar.fouroseven = 0;
				if(trans->callback) {
					// call the callback to process response
					(trans->callback)(sip, msg, trans);
				}
				transactions_remove(sip, trans);
			}
			found = 1;
		} else {
			gaim_debug(GAIM_DEBUG_MISC, "simple", "received response to unknown transaction");
		}
	}
	if(!found) {
		gaim_debug(GAIM_DEBUG_MISC, "simple", "received a unknown sip message with method %sand response %d\n",msg->method, msg->response);
	}
}

static void process_input(struct simple_account_data *sip, struct sip_connection *conn)
{
	char *cur;
	char *dummy;
	struct sipmsg *msg;
	int restlen;
	
	cur = conn->inbuf;

	// according to the RFC remove CRLF at the beginning
	while(*cur == '\r' || *cur == '\n') {
		cur++;
	}
	if(cur != conn->inbuf) {
		memmove(conn->inbuf, cur, conn->inbufused-(cur-conn->inbuf));
		conn->inbufused=strlen(conn->inbuf);
	}

	// Received a full Header?
	if((cur = strstr(conn->inbuf, "\r\n\r\n"))!=NULL) {
		time_t currtime = time(NULL);
		cur += 2;
		cur[0] = '\0';
		gaim_debug_info("simple","\n\nreceived - %s\n######\n%s\n#######\n\n",ctime(&currtime), conn->inbuf);
		msg = sipmsg_parse_header(conn->inbuf);
		cur[0] = '\r';
		cur += 2;
		restlen = conn->inbufused - (cur-conn->inbuf);
		if(restlen>=msg->bodylen) {
			dummy = g_malloc(msg->bodylen+1);
			memcpy(dummy, cur, msg->bodylen);
			dummy[msg->bodylen]='\0';
			msg->body = dummy;
			cur+=msg->bodylen;
			memmove(conn->inbuf, cur, conn->inbuflen);
			conn->inbufused=strlen(conn->inbuf);
		} else {
			sipmsg_free(msg);
			return;
		}
	//	sipmsg_print(msg);
        	gaim_debug(GAIM_DEBUG_MISC, "simple", "in process response response: %d\n", msg->response);
		process_input_message(sip,msg);
	} else {
		gaim_debug(GAIM_DEBUG_MISC, "simple", "received a incomplete sip msg: %s\n", conn->inbuf);
	}
}

static void simple_udp_process(gpointer data, gint source, GaimInputCondition con) {
	GaimConnection *gc = data;
	struct simple_account_data *sip = gc->proto_data;
	struct sipmsg *msg;
	int len;
	time_t currtime;

	static char buffer[65536];
	len = recv(source, buffer, 65536, 0);
	buffer[len] = 0;
	gaim_debug_info("simple","\n\nreceived - %s\n######\n%s\n#######\n\n",ctime(&currtime), buffer);
	msg = sipmsg_parse_msg(buffer);
	if(msg) process_input_message(sip, msg);
}

static void simple_input_cb(gpointer data, gint source, GaimInputCondition cond)
{
	GaimConnection *gc = data;
	struct simple_account_data *sip = gc->proto_data;
	int len;
	struct sip_connection *conn = connection_find(sip, source);
	if(!conn) {
		gaim_debug_error("simple", "Connection not found!\n");
		return;
	}

	if (conn->inbuflen < conn->inbufused + SIMPLE_BUF_INC) {
		conn->inbuflen += SIMPLE_BUF_INC;
		conn->inbuf = g_realloc(conn->inbuf, conn->inbuflen);
	}

	if ((len = read(source, conn->inbuf + conn->inbufused, SIMPLE_BUF_INC - 1)) <= 0) {
		gaim_debug_info("simple","simple_input_cb: read error\n");
		connection_remove(sip, source);
		if(sip->fd == source) sip->fd = -1;
//		gaim_connection_error(gc, _("Read error"));
		return;
	}
	if(len == 0) {
		// connection was closed
		connection_remove(sip, source);
		if(sip->fd == source) sip->fd = -1;
	}

	conn->inbufused += len;
	conn->inbuf[conn->inbufused]='\0';
	
	process_input(sip, conn);
}

/* Callback for new connections on incoming TCP port */
static void simple_newconn_cb(gpointer data, gint source, GaimInputCondition cond) {
	GaimConnection *gc = data;
	struct simple_account_data *sip = gc->proto_data;
	struct sip_connection *conn;


	int newfd = accept(source, NULL, NULL);

	conn = connection_create(sip, newfd);
	
	conn->inputhandler = gaim_input_add(newfd, GAIM_INPUT_READ, simple_input_cb, gc);
}

static void login_cb(gpointer data, gint source, GaimInputCondition cond) {
	GaimConnection *gc = data;
	struct simple_account_data *sip = gc->proto_data;
	struct sip_connection *conn;

	if( source < 0 ) {
		gaim_connection_error(gc,"Could not connect");
		return;
	}

	sip->fd = source;

	// get buddies from blist
	simple_get_buddies(gc);

	conn = connection_create(sip, source);
	
	// get the local ip
	sip->ip = g_strdup(gaim_network_get_my_ip(source));
	
	do_register(sip);
	
	conn->inputhandler = gaim_input_add(sip->fd, GAIM_INPUT_READ, simple_input_cb, gc);
}

static guint simple_ht_hash_nick(const char *nick) {
	char *lc = g_utf8_strdown(nick, -1);
	guint bucket = g_str_hash(lc);
	g_free(lc);
	
	return bucket;
}

static gboolean simple_ht_equals_nick(const char *nick1, const char *nick2) {
	return (gaim_utf8_strcasecmp(nick1, nick2) == 0);
}

static void simple_login(GaimAccount *account, GaimStatus *status)
{
	GaimConnection *gc;
	struct simple_account_data *sip;
	gchar **userserver;
	int error=0;
	struct getserver_return *serveradr;
	gchar *hosttoconnect;
	       
	const char *username = gaim_account_get_username(account);

	gc = gaim_account_get_connection(account);
	
	gc->proto_data = sip = g_new0(struct simple_account_data,1);
	sip->gc=gc;
	sip->account = account;
	sip->registerexpire = 900;
	sip->udp = gaim_account_get_bool(account, "udp", FALSE);	
        if (strpbrk(username, " \t\v\r\n") != NULL) {
                gaim_connection_error(gc, _("SIP usernames may not contain whitespaces or @ symbols"));
       	        return;
        }

	userserver = g_strsplit(username, "@", 2);
	
	gaim_connection_set_display_name(gc,userserver[0]);
	sip->username = g_strdup(userserver[0]);
	sip->servername = g_strdup(userserver[1]);
	sip->password = g_strdup(gaim_connection_get_password(gc));
	g_strfreev(userserver);

	sip->buddies = g_hash_table_new((GHashFunc)simple_ht_hash_nick, (GEqualFunc)simple_ht_equals_nick);
	
        gaim_connection_update_progress(gc, _("Connecting"), 1, 2);

	sip->status = g_strdup("available");

	if(!gaim_account_get_bool(account, "useproxy", FALSE)) {
		hosttoconnect = g_strdup(sip->servername);
	} else {
		hosttoconnect = g_strdup(gaim_account_get_string(account, "proxy", sip->servername));
	}
	
	// TCP case
	if(! sip->udp) {	
		// search for SRV record
		serveradr = getserver(hosttoconnect, "_sip._tcp");
	        gaim_debug_info("simple","connecting to %s port %d", serveradr->name, serveradr->port);

		// open tcp connection to the server
		error = gaim_proxy_connect(account, serveradr->name, serveradr->port, login_cb, gc);
		if(error) {
			gaim_connection_error(gc, _("Couldn't create socket"));
		}

		// create socket for incoming connections
		sip->listenfd = gaim_network_listen_range(5060, 5080);
		if(sip->listenfd == -1) {
			gaim_connection_error(gc, _("Could not create listen socket"));
			return;
		}
		sip->listenport = gaim_network_get_port_from_fd(sip->listenfd);
		gaim_input_add(sip->listenfd, GAIM_INPUT_READ, simple_newconn_cb, gc);
	} else { // UDP
		// search for SRV record
		struct sockaddr_in addr;
		struct hostent *h;
		
		serveradr = getserver(hosttoconnect, "_sip._udp");
		gaim_debug_info("simple", "using udp with server %s and port %d", serveradr->name, serveradr->port);
		sip->fd = socket(AF_INET, SOCK_DGRAM, 0);

		addr.sin_family = AF_INET;
		addr.sin_port = htons(5060);
		addr.sin_addr.s_addr = INADDR_ANY;
		while((bind(sip->fd, (struct sockaddr*)&addr, sizeof(struct sockaddr_in)) <0) && ntohs(addr.sin_port)<5160) {
			addr.sin_port = htons(ntohs(addr.sin_port)+1);
		}
		sip->listenport = ntohs(addr.sin_port);
		sip->listenfd = sip->fd;

		gaim_input_add(sip->fd, GAIM_INPUT_READ, simple_udp_process, gc);
		// TODO - change to new SRV impl.
		sip->serveraddr.sin_family = AF_INET;
		sip->serveraddr.sin_port = htons(serveradr->port);
		
		h = gethostbyname(serveradr->name);
		sip->serveraddr.sin_addr.s_addr = ((struct in_addr*)h->h_addr)->s_addr;
	        sip->ip = g_strdup(gaim_network_get_my_ip(sip->listenfd));
		sip->resendtimeout = gaim_timeout_add(2500, (GSourceFunc)resend_timeout, sip);
	        do_register(sip);
		
	}
	g_free(hosttoconnect);
		
	// register timeout callback for register / subscribe renewal
	sip->registertimeout = gaim_timeout_add((rand()%100)+10*1000, (GSourceFunc)register_timeout, sip);
}

static void simple_close(GaimConnection *gc)
{
	struct simple_account_data *sip = gc->proto_data;

	// unregister
	do_register_exp(sip, 0);
//	if(sip) {
	if(0) {
		if(sip->servername) g_free(sip->servername);
		if(sip->username) g_free(sip->username);
		if(sip->password) g_free(sip->password);
		if(sip->registrar.nonce) g_free(sip->registrar.nonce);
		if(sip->registrar.realm) g_free(sip->registrar.nonce);
		if(sip->proxy.nonce) g_free(sip->proxy.nonce);
		if(sip->proxy.realm) g_free(sip->proxy.realm);
//		if(sip->registertimeout) gaim_timeout_remove(sip->registertimeout);
		if(sip->sendlater) g_free(sip->sendlater);
		if(sip->ip) g_free(sip->ip);
		sip->servername = sip->username = sip->password = sip->registrar.nonce = sip->registrar.realm = sip->proxy.nonce = sip->proxy.realm = sip->sendlater = sip->ip = 0;
	}
//	if(gc->proto_data) g_free(gc->proto_data);
//	gc->proto_data = 0;
	// TODO free connections
}

static GaimPluginProtocolInfo prpl_info =
{
	0,
	NULL,					/* user_splits */
	NULL,					/* protocol_options */
	NO_BUDDY_ICONS,		/* icon_spec */
	simple_list_icon,		/* list_icon */
	NULL,	/* list_emblems */
	NULL,					/* status_text */
	NULL,					/* tooltip_text */
	simple_status_types,	/* away_states */
	NULL,					/* blist_node_menu */
	NULL,	/* chat_info */
	NULL,	/* chat_info_defaults */
	simple_login,		/* login */
	simple_close,		/* close */
	simple_im_send,		/* send_im */
	NULL,					/* set_info */
	simple_typing,					/* send_typing */
	NULL,		/* get_info */
	simple_set_status,		/* set_status */
	NULL,					/* set_idle */
	NULL,					/* change_passwd */
	simple_add_buddy,		/* add_buddy */
	NULL,					/* add_buddies */
	simple_remove_buddy,	/* remove_buddy */
	NULL,					/* remove_buddies */
	NULL,					/* add_permit */
	NULL,					/* add_deny */
	NULL,					/* rem_permit */
	NULL,					/* rem_deny */
	NULL,					/* set_permit_deny */
	NULL,		/* join_chat */
	NULL,					/* reject_chat */
	NULL,	/* get_chat_name */
	NULL,	/* chat_invite */
	NULL,		/* chat_leave */
	NULL,					/* chat_whisper */
	NULL,		/* chat_send */
	simple_keep_alive,					/* keepalive */
	NULL,					/* register_user */
	NULL,					/* get_cb_info */
	NULL,					/* get_cb_away */
	NULL,					/* alias_buddy */
	NULL,					/* group_buddy */
	NULL,					/* rename_group */
	NULL,					/* buddy_free */
	NULL,					/* convo_closed */
	NULL,					/* normalize */
	NULL,					/* set_buddy_icon */
	NULL,					/* remove_group */
	NULL,					/* get_cb_real_name */
	NULL,	/* set_chat_topic */
	NULL,					/* find_blist_chat */
	NULL,	/* roomlist_get_list */
	NULL,	/* roomlist_cancel */
	NULL,					/* roomlist_expand_category */
	NULL,					/* can_receive_file */
	NULL	/* send_file */
};


static GaimPluginInfo info =
{
	GAIM_PLUGIN_MAGIC,
	GAIM_MAJOR_VERSION,
	GAIM_MINOR_VERSION,
	GAIM_PLUGIN_PROTOCOL,                             /**< type           */
	NULL,                                             /**< ui_requirement */
	0,                                                /**< flags          */
	NULL,                                             /**< dependencies   */
	GAIM_PRIORITY_DEFAULT,                            /**< priority       */

	"prpl-simple",                                       /**< id             */
	"SIMPLE",                                            /**< name           */
	VERSION,                                          /**< version        */
	N_("SIP/SIMPLE Protocol Plugin"),                        /**  summary        */
	N_("The SIP/SIMPLE Protocol Plugin"),    /**  description    */
	N_("Thomas Butter <butter@uni-mannheim.de>"),         /**< author         */
	GAIM_WEBSITE,                                     /**< homepage       */

	NULL,                                             /**< load           */
	NULL,                                             /**< unload         */
	NULL,                                             /**< destroy        */

	NULL,                                             /**< ui_info        */
	&prpl_info,                                       /**< extra_info     */
	NULL,
	NULL
};

static void _init_plugin(GaimPlugin *plugin)
{
        GaimAccountUserSplit *split;
	GaimAccountOption *option;

        split = gaim_account_user_split_new(_("Server"), "blubb.com", '@');
        prpl_info.user_splits = g_list_append(prpl_info.user_splits, split);

	option = gaim_account_option_bool_new(_("Use UDP"), "udp", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	option = gaim_account_option_bool_new(_("Use Proxy"), "useproxy", FALSE);
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
	option = gaim_account_option_string_new(_("Proxy"), "proxy", "");
	prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
//	_simple_plugin = plugin;
}

GAIM_INIT_PLUGIN(simple, _init_plugin, info);
