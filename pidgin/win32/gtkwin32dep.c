/*
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
 *
 */
#include <io.h>
#include <stdlib.h>
#include <stdio.h>

/* winsock2.h needs to be include before windows.h or else it throws a warning
 * saying it needs to be included first. We don't use it directly but it's
 * included indirectly.
 */
#include <winsock2.h>
#include <windows.h>
#include <winuser.h>
#include <shellapi.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <purple.h>

#include "resource.h"

#include "gtkwin32dep.h"
#include "gtkblist.h"
#include "gtkconv.h"
#include "gtkconn.h"

/*
 *  GLOBALS
 */
HINSTANCE exe_hInstance = 0;
HINSTANCE dll_hInstance = 0;
HWND messagewin_hwnd;

static gboolean pwm_handles_connections = TRUE;


/*
 *  PUBLIC CODE
 */

HINSTANCE winpidgin_exe_hinstance(void) {
	return exe_hInstance;
}

void winpidgin_set_exe_hinstance(HINSTANCE hint)
{
	exe_hInstance = hint;
}

HINSTANCE winpidgin_dll_hinstance(void) {
	return dll_hInstance;
}

int winpidgin_gz_decompress(const char* in, const char* out) {
	GFile *fin;
	GFile *fout;
	GInputStream *input;
	GOutputStream *output;
	GOutputStream *conv_out;
	GZlibDecompressor *decompressor;
	gssize size;
	GError *error = NULL;

	fin = g_file_new_for_path(in);
	input = G_INPUT_STREAM(g_file_read(fin, NULL, &error));
	g_object_unref(fin);

	if (input == NULL) {
		purple_debug_error("winpidgin_gz_decompress",
				"Failed to open: %s: %s\n",
				in, error->message);
		g_clear_error(&error);
		return 0;
	}

	fout = g_file_new_for_path(out);
	output = G_OUTPUT_STREAM(g_file_replace(fout, NULL, FALSE,
			G_FILE_CREATE_NONE, NULL, &error));
	g_object_unref(fout);

	if (output == NULL) {
		purple_debug_error("winpidgin_gz_decompress",
				"Error opening file: %s: %s\n",
				out, error->message);
		g_clear_error(&error);
		g_object_unref(input);
		return 0;
	}

	decompressor = g_zlib_decompressor_new(G_ZLIB_COMPRESSOR_FORMAT_GZIP);
	conv_out = g_converter_output_stream_new(output,
			G_CONVERTER(decompressor));
	g_object_unref(decompressor);
	g_object_unref(output);

	size = g_output_stream_splice(conv_out, input,
			G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE |
			G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET, NULL, &error);

	g_object_unref(input);
	g_object_unref(conv_out);

	if (size < 0) {
		purple_debug_error("wpurple_gz_decompress",
				"Error writing to file: %s\n",
				error->message);
		g_clear_error(&error);
		return 0;
	}

	return 1;
}

#define PIDGIN_WM_FOCUS_REQUEST (WM_APP + 13)
#define PIDGIN_WM_PROTOCOL_HANDLE (WM_APP + 14)

static void*
winpidgin_netconfig_changed_cb(GNetworkMonitor *monitor, gboolean available, gpointer data)
{
	pwm_handles_connections = FALSE;

	return NULL;
}

static gboolean
winpidgin_pwm_reconnect()
{
	g_signal_handlers_disconnect_by_func(g_network_monitor_get_default,
	                                     G_CALLBACK(winpidgin_netconfig_changed_cb),
	                                     NULL);
	if (pwm_handles_connections == TRUE) {
		PurpleConnectionUiOps *ui_ops = pidgin_connections_get_ui_ops();

		purple_debug_info("winpidgin", "Resumed from standby, reconnecting accounts.\n");

		if (ui_ops != NULL && ui_ops->network_connected != NULL)
			ui_ops->network_connected();
	} else {
		purple_debug_info("winpidgin", "Resumed from standby, gtkconn will handle reconnecting.\n");
		pwm_handles_connections = TRUE;
	}

	return FALSE;
}

static LRESULT CALLBACK message_window_handler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {

	if (msg == PIDGIN_WM_FOCUS_REQUEST) {
		PidginBuddyList *blist;
		purple_debug_info("winpidgin", "Got external Buddy List focus request.");
		blist = pidgin_blist_get_default_gtk_blist();
		if (blist != NULL && blist->window != NULL) {
			gtk_window_present(GTK_WINDOW(blist->window));
		}
		return TRUE;
	} else if (msg == PIDGIN_WM_PROTOCOL_HANDLE) {
		char *proto_msg = (char *) lparam;
		purple_debug_info("winpidgin", "Got protocol handler request: %s\n", proto_msg ? proto_msg : "");
		purple_got_protocol_handler_uri(proto_msg);
		return TRUE;
	} else if (msg == WM_POWERBROADCAST) {
		if (wparam == PBT_APMQUERYSUSPEND) {
			purple_debug_info("winpidgin", "Windows requesting permission to suspend.\n");
			return TRUE;
		} else if (wparam == PBT_APMSUSPEND) {
			PurpleConnectionUiOps *ui_ops = pidgin_connections_get_ui_ops();

			purple_debug_info("winpidgin", "Entering system standby, disconnecting accounts.\n");

			if (ui_ops != NULL && ui_ops->network_disconnected != NULL)
				ui_ops->network_disconnected();

			g_signal_connect(g_network_monitor_get_default(),
			                 "network-changed",
			                 G_CALLBACK(winpidgin_netconfig_changed_cb),
			                 NULL);

			return TRUE;
		} else if (wparam == PBT_APMRESUMESUSPEND) {
			purple_debug_info("winpidgin", "Resuming from system standby.\n");
			/* TODO: It seems like it'd be wise to use the NLA message, if possible, instead of this. */
			g_timeout_add_seconds(1, winpidgin_pwm_reconnect, NULL);
			return TRUE;
		}
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

static HWND winpidgin_message_window_init(void) {
	HWND win_hwnd;
	WNDCLASSEX wcx;
	LPCTSTR wname;

	wname = TEXT("WinpidginMsgWinCls");

	wcx.cbSize = sizeof(wcx);
	wcx.style = 0;
	wcx.lpfnWndProc = message_window_handler;
	wcx.cbClsExtra = 0;
	wcx.cbWndExtra = 0;
	wcx.hInstance = winpidgin_exe_hinstance();
	wcx.hIcon = NULL;
	wcx.hCursor = NULL;
	wcx.hbrBackground = NULL;
	wcx.lpszMenuName = NULL;
	wcx.lpszClassName = wname;
	wcx.hIconSm = NULL;

	RegisterClassEx(&wcx);

	/* Create the window */
	if(!(win_hwnd = CreateWindow(wname, TEXT("WinpidginMsgWin"), 0, 0, 0, 0, 0,
			NULL, NULL, winpidgin_exe_hinstance(), 0))) {
		purple_debug_error("winpidgin",
			"Unable to create message window.\n");
		return NULL;
	}

	return win_hwnd;
}

void winpidgin_init(void) {
	typedef void (__cdecl* LPFNSETLOGFILE)(const LPCSTR);
	LPFNSETLOGFILE MySetLogFile;
	gchar *exchndl_dll_path;

	if (purple_debug_is_verbose())
		purple_debug_misc("winpidgin", "winpidgin_init start\n");

	exchndl_dll_path = g_build_filename(wpurple_bin_dir(), "exchndl.dll", NULL);
	MySetLogFile = (LPFNSETLOGFILE) wpurple_find_and_loadproc(exchndl_dll_path, "SetLogFile");
	g_free(exchndl_dll_path);
	exchndl_dll_path = NULL;
	if (MySetLogFile) {
		gchar *debug_dir, *locale_debug_dir;

		debug_dir = g_build_filename(purple_cache_dir(), "pidgin.RPT", NULL);
		locale_debug_dir = g_locale_from_utf8(debug_dir, -1, NULL, NULL, NULL);

		purple_debug_info("winpidgin", "Setting exchndl.dll LogFile to %s\n", debug_dir);

		MySetLogFile(locale_debug_dir);

		g_free(debug_dir);
		g_free(locale_debug_dir);
	}

	messagewin_hwnd = winpidgin_message_window_init();

	if (purple_debug_is_verbose())
		purple_debug_misc("winpidgin", "winpidgin_init end\n");
}

/* Windows Cleanup */

void winpidgin_cleanup(void) {
	purple_debug_info("winpidgin", "winpidgin_cleanup\n");

	if(messagewin_hwnd)
		DestroyWindow(messagewin_hwnd);

}

/* DLL initializer */
/* suppress gcc "no previous prototype" warning */
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	dll_hInstance = hinstDLL;
	return TRUE;
}

typedef HRESULT (WINAPI* DwmIsCompositionEnabledFunction)(BOOL*);
typedef HRESULT (WINAPI* DwmGetWindowAttributeFunction)(HWND, DWORD, PVOID, DWORD);
#ifndef DWMWA_EXTENDED_FRAME_BOUNDS
#	define DWMWA_EXTENDED_FRAME_BOUNDS 9
#endif

DWORD winpidgin_get_lastactive() {
	DWORD result = 0;

	LASTINPUTINFO lii;
	memset(&lii, 0, sizeof(lii));
	lii.cbSize = sizeof(lii);
	if (GetLastInputInfo(&lii))
		result = lii.dwTime;

	return result;
}

