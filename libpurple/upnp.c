/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
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
#include <gio/gio.h>

#include <libgupnp/gupnp-context-manager.h>
#include <libgupnp/gupnp-control-point.h>
#include <libgupnp/gupnp-service-info.h>
#include <libgupnp/gupnp-service-proxy.h>
#include <libgupnp-igd/gupnp-simple-igd.h>

#include "upnp.h"

#include "glibcompat.h"
#include "soupcompat.h"

#include "debug.h"
#include "eventloop.h"
#include "network.h"
#include "proxy.h"
#include "purplegio.h"
#include "signals.h"
#include "util.h"
#include "xmlnode.h"

/***************************************************************
** General Defines                                             *
****************************************************************/
#define PORT_MAPPING_LEASE_TIME 0
#define PORT_MAPPING_DESCRIPTION "PURPLE_UPNP_PORT_FORWARD"

typedef enum {
	PURPLE_UPNP_STATUS_UNDISCOVERED = -1,
	PURPLE_UPNP_STATUS_UNABLE_TO_DISCOVER,
	PURPLE_UPNP_STATUS_DISCOVERING,
	PURPLE_UPNP_STATUS_DISCOVERED
} PurpleUPnPStatus;

typedef struct {
	PurpleUPnPStatus status;
	gchar *control_url;
	gchar *service_type;
	char publicip[16];
	char internalip[16];
	gint64 lookup_time;
} PurpleUPnPControlInfo;

typedef struct {
	unsigned short portmap;
	gchar protocol[4];
	gboolean add;
	PurpleUPnPCallback cb;
	gpointer cb_data;
	gboolean success;
	guint tima; /* g_timeout_add handle */
} PurpleUPnPMappingAddRemove;

static PurpleUPnPControlInfo control_info = {
	PURPLE_UPNP_STATUS_UNDISCOVERED,
	NULL, "\0", "\0", "\0", 0};

static GUPnPContextManager *manager = NULL;
static GUPnPSimpleIgd *simple_igd = NULL;
static GSList *discovery_callbacks = NULL;

static void lookup_internal_ip(void);

static gboolean
fire_ar_cb_async_and_free(gpointer data)
{
	PurpleUPnPMappingAddRemove *ar = data;
	if (ar) {
		if (ar->cb)
			ar->cb(ar->success, ar->cb_data);
		g_free(ar);
	}

	return FALSE;
}

static void
fire_discovery_callbacks(gboolean success)
{
	while(discovery_callbacks) {
		gpointer data;
		PurpleUPnPCallback cb = discovery_callbacks->data;
		discovery_callbacks = g_slist_delete_link(discovery_callbacks, discovery_callbacks);
		data = discovery_callbacks->data;
		discovery_callbacks = g_slist_delete_link(discovery_callbacks, discovery_callbacks);
		cb(success, data);
	}
}

static void
upnp_service_proxy_call_action_cb(GObject *source, GAsyncResult *result,
                                  G_GNUC_UNUSED gpointer data)
{
	GError *error = NULL;
	char *ip = NULL;
	GUPnPServiceProxyAction *action = NULL;

	action = gupnp_service_proxy_call_action_finish(GUPNP_SERVICE_PROXY(source),
	                                                result, &error);
	if (error != NULL) {
		control_info.publicip[0] = '\0';

		purple_debug_error("upnp",
		                   "Failed to call GetExternalIPAddress action: %s",
		                   error->message);
		g_error_free(error);
		return;
	}

	gupnp_service_proxy_action_get_result(action, &error,
	                                      "NewExternalIPAddress",
	                                      G_TYPE_STRING, &ip, NULL);
	if (error == NULL) {
		g_strlcpy(control_info.publicip, ip, sizeof(control_info.publicip));

		purple_debug_info("upnp", "NAT Returned IP: %s", control_info.publicip);
		g_free(ip);
	} else {
		control_info.publicip[0] = '\0';

		purple_debug_error("upnp",
		                   "Failed to get result from GetExternalIPAddress: %s",
		                   error->message);
		g_error_free(error);
	}

	gupnp_service_proxy_action_unref(action);
}

static void
upnp_service_proxy_available_cb(G_GNUC_UNUSED GUPnPControlPoint *cp,
                                GUPnPServiceProxy *proxy,
                                G_GNUC_UNUSED gpointer data)
{
	gchar *control_url = NULL;
	const gchar *service_type = NULL;

	control_url = gupnp_service_info_get_control_url(GUPNP_SERVICE_INFO(proxy));
	service_type = gupnp_service_info_get_service_type(GUPNP_SERVICE_INFO(proxy));
	purple_debug_info("upnp",
	                  "Service proxy available for %s on control URL %s",
	                  service_type,
	                  control_url);

	control_info.lookup_time = g_get_monotonic_time();
	control_info.control_url = control_url;
	control_info.service_type = g_strdup(service_type);

	if(control_url) {
		GUPnPServiceProxyAction *action = NULL;

		control_info.status = PURPLE_UPNP_STATUS_DISCOVERED;
		fire_discovery_callbacks(TRUE);

		lookup_internal_ip();
		action = gupnp_service_proxy_action_new("GetExternalIPAddress", NULL);
		gupnp_service_proxy_call_action_async(proxy, action, NULL,
		                                      upnp_service_proxy_call_action_cb,
		                                      NULL);
	} else {
		control_info.status = PURPLE_UPNP_STATUS_UNABLE_TO_DISCOVER;
		fire_discovery_callbacks(FALSE);
	}
}

static void
upnp_context_unavailable_cb(G_GNUC_UNUSED GUPnPContextManager *manager,
                            GUPnPContext *context, G_GNUC_UNUSED gpointer data)
{
	purple_debug_info("upnp",
	                  "UPnP context no longer available for interface %s",
	                  gssdp_client_get_interface(GSSDP_CLIENT(context)));

	/* Delete these to prevent them owning refs back on the context. */
	g_object_set_data(G_OBJECT(context), "WANIPConnection", NULL);
	g_object_set_data(G_OBJECT(context), "WANPPPConnection", NULL);
}

static void
upnp_context_available_cb(G_GNUC_UNUSED GUPnPContextManager *manager,
                          GUPnPContext *context, G_GNUC_UNUSED gpointer data)
{
	GUPnPControlPoint *cp;

	purple_debug_info("upnp", "UPnP context now available for interface %s",
	                  gssdp_client_get_interface(GSSDP_CLIENT(context)));

	cp = gupnp_control_point_new(context,
	                             "urn:schemas-upnp-org:service:WANIPConnection:1");
	g_signal_connect(cp, "service-proxy-available",
	                 G_CALLBACK(upnp_service_proxy_available_cb), NULL);
	gssdp_resource_browser_set_active(GSSDP_RESOURCE_BROWSER(cp), TRUE);
	g_object_set_data_full(G_OBJECT(context), "WANIPConnection", cp,
	                       g_object_unref);

	cp = gupnp_control_point_new(context,
	                             "urn:schemas-upnp-org:service:WANPPPConnection:1");
	g_signal_connect(cp, "service-proxy-available",
	                 G_CALLBACK(upnp_service_proxy_available_cb), NULL);
	gssdp_resource_browser_set_active(GSSDP_RESOURCE_BROWSER(cp), TRUE);
	g_object_set_data_full(G_OBJECT(context), "WANPPPConnection", cp,
	                       g_object_unref);
}

void
purple_upnp_discover(PurpleUPnPCallback cb, gpointer cb_data)
{
	if (cb) {
		discovery_callbacks = g_slist_append(discovery_callbacks, cb);
		discovery_callbacks = g_slist_append(discovery_callbacks, cb_data);
	}

	if (control_info.status == PURPLE_UPNP_STATUS_DISCOVERING) {
		return;
	}

	purple_debug_info("upnp",
	                  "Starting discovery on all available interfaces");

	g_clear_object(&manager);
	manager = gupnp_context_manager_create(0);
	g_signal_connect(manager, "context-available",
	                 G_CALLBACK(upnp_context_available_cb), NULL);
	g_signal_connect(manager, "context-unavailable",
	                 G_CALLBACK(upnp_context_unavailable_cb), NULL);

	control_info.status = PURPLE_UPNP_STATUS_DISCOVERING;
}

const gchar *
purple_upnp_get_public_ip()
{
	if (control_info.status == PURPLE_UPNP_STATUS_DISCOVERED
			&& *control_info.publicip)
		return control_info.publicip;

	/* Trigger another UPnP discovery if 5 minutes have elapsed since the
	 * last one, and it wasn't successful */
	if (control_info.status < PURPLE_UPNP_STATUS_DISCOVERING &&
	    (g_get_monotonic_time() - control_info.lookup_time) >
	            300 * G_USEC_PER_SEC) {
		purple_upnp_discover(NULL, NULL);
	}

	return NULL;
}

/* TODO: This could be exported */
static const gchar *
purple_upnp_get_internal_ip(void)
{
	if (control_info.status == PURPLE_UPNP_STATUS_DISCOVERED
			&& *control_info.internalip)
		return control_info.internalip;

	/* Trigger another UPnP discovery if 5 minutes have elapsed since the
	 * last one, and it wasn't successful */
	if (control_info.status < PURPLE_UPNP_STATUS_DISCOVERING &&
	    (g_get_monotonic_time() - control_info.lookup_time) >
	            300 * G_USEC_PER_SEC) {
		purple_upnp_discover(NULL, NULL);
	}

	return NULL;
}

static void
looked_up_internal_ip_cb(GObject *source, GAsyncResult *result,
                         G_GNUC_UNUSED gpointer user_data)
{
	GSocketConnection *conn;
	GSocketAddress *addr;
	GInetSocketAddress *inetsockaddr;
	GError *error = NULL;

	conn = g_socket_client_connect_to_host_finish(G_SOCKET_CLIENT(source),
	                                              result, &error);
	if (conn == NULL) {
		purple_debug_error("upnp", "Unable to look up local IP: %s",
		                   error->message);
		g_clear_error(&error);
		return;
	}

	g_strlcpy(control_info.internalip, "0.0.0.0",
	          sizeof(control_info.internalip));

	addr = g_socket_connection_get_local_address(conn, &error);
	if ((inetsockaddr = G_INET_SOCKET_ADDRESS(addr)) != NULL) {
		GInetAddress *inetaddr =
		        g_inet_socket_address_get_address(inetsockaddr);
		if (g_inet_address_get_family(inetaddr) == G_SOCKET_FAMILY_IPV4 &&
		    !g_inet_address_get_is_loopback(inetaddr))
		{
			gchar *ip = g_inet_address_to_string(inetaddr);
			g_strlcpy(control_info.internalip, ip,
			          sizeof(control_info.internalip));
			g_free(ip);
		}
	} else {
		purple_debug_error(
		        "upnp", "Unable to get local address of connection: %s",
		        error ? error->message : "unknown socket address type");
		g_clear_error(&error);
	}
	g_object_unref(addr);

	purple_debug_info("upnp", "Local IP: %s", control_info.internalip);
	g_object_unref(conn);
}

static void
lookup_internal_ip(void)
{
	gchar *host;
	gint port;
	GSocketClient *client;
	GError *error = NULL;

	if (!g_uri_split_network(control_info.control_url, G_URI_FLAGS_NONE, NULL,
	                         &host, &port, &error))
	{
		purple_debug_error("upnp",
			"lookup_internal_ip(): Failed In Parse URL: %s",
			error->message);
		return;
	}

	client = purple_gio_socket_client_new(NULL, &error);
	if (client == NULL) {
		purple_debug_error("upnp", "Get Local IP Connect to %s:%d Failed: %s",
		                   host, port, error->message);
		g_clear_error(&error);
		g_free(host);
		return;
	}

	purple_debug_info("upnp", "Attempting connection to %s:%u\n", host, port);
	g_socket_client_connect_to_host_async(client, host, port, NULL,
	                                      looked_up_internal_ip_cb, NULL);

	g_object_unref(client);
	g_free(host);
}

static void
upnp_mapped_external_port_cb(GUPnPSimpleIgd *igd, const gchar *protocol,
                             G_GNUC_UNUSED const gchar *external_ip,
                             G_GNUC_UNUSED const gchar *replaces_external_ip,
                             guint16 external_port,
                             G_GNUC_UNUSED const gchar *local_ip,
                             guint16 local_port,
                             G_GNUC_UNUSED const gchar *description,
                             gpointer data)
{
	PurpleUPnPMappingAddRemove *ar = data;

	if(!purple_strequal(ar->protocol, protocol)) {
		return;
	}
	if(external_port != ar->portmap) {
		return;
	}

	purple_debug_info("upnp", "Successfully completed port mapping operation");

	ar->success = TRUE;
	ar->tima = g_timeout_add(0, fire_ar_cb_async_and_free, ar);

	g_signal_handlers_disconnect_by_data(igd, ar);
}

static void
upnp_error_mapping_port_cb(GUPnPSimpleIgd *igd, GError *error,
                           const gchar *protocol,
                           guint16 external_port,
                           G_GNUC_UNUSED const gchar *local_ip,
                           G_GNUC_UNUSED guint16 local_port,
                           G_GNUC_UNUSED const gchar *description,
                           gpointer data)
{
	PurpleUPnPMappingAddRemove *ar = data;

	if(!purple_strequal(ar->protocol, protocol)) {
		return;
	}
	if(external_port != ar->portmap) {
		return;
	}

	purple_debug_error("upnp", "purple_upnp_set_port_mapping(): Failed: %s",
	                   error->message);

	ar->success = FALSE;
	ar->tima = g_timeout_add(0, fire_ar_cb_async_and_free, ar);

	g_signal_handlers_disconnect_by_data(igd, ar);
}

static void
do_port_mapping_cb(gboolean has_control_mapping, gpointer data)
{
	PurpleUPnPMappingAddRemove *ar = data;

	if (!has_control_mapping) {
		ar->success = FALSE;
		ar->tima = g_timeout_add(0, fire_ar_cb_async_and_free, ar);
		return;
	}

	if(ar->add) {
		const gchar *internal_ip;
		/* get the internal IP */
		if(!(internal_ip = purple_upnp_get_internal_ip())) {
			purple_debug_error("upnp",
				"purple_upnp_set_port_mapping(): couldn't get local ip\n");
			ar->success = FALSE;
			ar->tima = g_timeout_add(0, fire_ar_cb_async_and_free, ar);
			return;
		}

		if(simple_igd == NULL) {
			simple_igd = gupnp_simple_igd_new();
		}

		gupnp_simple_igd_add_port(simple_igd, ar->protocol, ar->portmap,
		                          internal_ip, ar->portmap,
		                          PORT_MAPPING_LEASE_TIME,
		                          PORT_MAPPING_DESCRIPTION);
	} else {
		if(simple_igd == NULL) {
			simple_igd = gupnp_simple_igd_new();
		}

		gupnp_simple_igd_remove_port(simple_igd, ar->protocol, ar->portmap);
	}

	g_signal_connect(simple_igd, "mapped-external-port",
	                 G_CALLBACK(upnp_mapped_external_port_cb), ar);
	g_signal_connect(simple_igd, "error-mapping-port",
	                 G_CALLBACK(upnp_error_mapping_port_cb), ar);
}

static gboolean
fire_port_mapping_failure_cb(gpointer data)
{
	PurpleUPnPMappingAddRemove *ar = data;

	ar->tima = 0;
	do_port_mapping_cb(FALSE, data);
	return FALSE;
}

void
purple_upnp_set_port_mapping(unsigned short portmap, const gchar *protocol,
                             PurpleUPnPCallback cb, gpointer cb_data)
{
	PurpleUPnPMappingAddRemove *ar;

	ar = g_new0(PurpleUPnPMappingAddRemove, 1);
	ar->cb = cb;
	ar->cb_data = cb_data;
	ar->add = TRUE;
	ar->portmap = portmap;
	g_strlcpy(ar->protocol, protocol, sizeof(ar->protocol));

	switch(control_info.status) {
		case PURPLE_UPNP_STATUS_UNABLE_TO_DISCOVER:
			if (g_get_monotonic_time() - control_info.lookup_time >
				300 * G_USEC_PER_SEC) {
				/* If we haven't had a successful UPnP discovery, check if 5 minutes
				 * has elapsed since the last try, try again */
				purple_upnp_discover(do_port_mapping_cb, ar);
			} else if (cb) {
				/* Asynchronously trigger a failed response */
				ar->tima = g_timeout_add(10, fire_port_mapping_failure_cb, ar);
			} else {
				/* No need to do anything if nobody expects a response*/
				g_free(ar);
				ar = NULL;
			}
			break;

		case PURPLE_UPNP_STATUS_UNDISCOVERED:
		case PURPLE_UPNP_STATUS_DISCOVERING:
			purple_upnp_discover(do_port_mapping_cb, ar);
			break;

		case PURPLE_UPNP_STATUS_DISCOVERED:
		default:
			do_port_mapping_cb(TRUE, ar);
			break;
	}
}

void
purple_upnp_remove_port_mapping(unsigned short portmap, const char *protocol,
                                PurpleUPnPCallback cb, gpointer cb_data)
{
	PurpleUPnPMappingAddRemove *ar;

	ar = g_new0(PurpleUPnPMappingAddRemove, 1);
	ar->cb = cb;
	ar->cb_data = cb_data;
	ar->add = FALSE;
	ar->portmap = portmap;
	g_strlcpy(ar->protocol, protocol, sizeof(ar->protocol));

	switch(control_info.status) {
		case PURPLE_UPNP_STATUS_UNABLE_TO_DISCOVER:
			if (g_get_monotonic_time() - control_info.lookup_time >
				300 * G_USEC_PER_SEC) {
				/* If we haven't had a successful UPnP discovery, check if 5 minutes
				 * has elapsed since the last try, try again */
				purple_upnp_discover(do_port_mapping_cb, ar);
			} else if (cb) {
				/* Asynchronously trigger a failed response */
				ar->tima = g_timeout_add(10, fire_port_mapping_failure_cb, ar);
			} else {
				/* No need to do anything if nobody expects a response*/
				g_free(ar);
				ar = NULL;
			}
			break;

		case PURPLE_UPNP_STATUS_DISCOVERING:
		case PURPLE_UPNP_STATUS_UNDISCOVERED:
			purple_upnp_discover(do_port_mapping_cb, ar);
			break;

		case PURPLE_UPNP_STATUS_DISCOVERED:
		default:
			do_port_mapping_cb(TRUE, ar);
			break;
	}
}

static void
purple_upnp_network_config_changed_cb(GNetworkMonitor *monitor, gboolean available, gpointer data)
{
	/* Reset the control_info to default values */
	control_info.status = PURPLE_UPNP_STATUS_UNDISCOVERED;
	g_clear_pointer(&control_info.control_url, g_free);
	g_clear_pointer(&control_info.service_type, g_free);
	control_info.publicip[0] = '\0';
	control_info.internalip[0] = '\0';
	control_info.lookup_time = 0;
}

void
purple_upnp_init()
{
	g_signal_connect(g_network_monitor_get_default(),
	                 "network-changed",
	                 G_CALLBACK(purple_upnp_network_config_changed_cb),
	                 NULL);
}

void
purple_upnp_uninit(void)
{
	g_clear_object(&simple_igd);
	g_clear_pointer(&control_info.control_url, g_free);
	g_clear_pointer(&control_info.service_type, g_free);
	g_clear_object(&manager);
}
