/*
* Copyright (C) 2007  Koos Vriezen <koos.vriezen@gmail.com>
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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/* gcc -o knpplayer `pkg-config --libs --cflags gtk+-x11-2.0` `pkg-config --libs --cflags dbus-glib-1` `nspr-config --libs --cflags` npplayer.c

http://devedge-temp.mozilla.org/library/manuals/2002/plugin/1.0/
http://dbus.freedesktop.org/doc/dbus/libdbus-tutorial.html
*/

#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>

#include <glib/gprintf.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>

#define XP_UNIX
#define MOZ_X11
#include "moz-sdk/npupp.h"

typedef const char* (* NP_LOADDS NP_GetMIMEDescriptionUPP)();
typedef NPError (* NP_InitializeUPP)(NPNetscapeFuncs*, NPPluginFuncs*);
typedef NPError (* NP_ShutdownUPP)(void);

static gchar *plugin;
static gchar *object_url;
static gchar *mimetype;

static DBusConnection *dbus_connection;
static char *service_name;
static gchar *callback_service;
static gchar *callback_path;
static GModule *library;
static GtkWidget *xembed;
static Window socket_id;
static Window parent_id;
static int stdin_read_watch;

static NPPluginFuncs np_funcs;       /* plugin functions              */
static NPP npp;                      /* single instance of the plugin */
static NPSavedData *saved_data;
static GSList *stream_list;
static char stream_buf[4096];
typedef struct _StreamInfo {
    NPStream np_stream;
    void *notify_data;
    unsigned int stream_buf_pos;
    unsigned int stream_pos;
    unsigned int total;
    unsigned int reason;
    char *url;
    char *mimetype;
    char *target;
    char notify;
} StreamInfo;

static NP_GetMIMEDescriptionUPP npGetMIMEDescription;
static NP_InitializeUPP npInitialize;
static NP_ShutdownUPP npShutdown;

static StreamInfo *addStream (const char *url, const char *mime, const char *target, void *notify_data, int notify);
static gboolean requestStream (void * si);
static gboolean destroyStream (void * si);
static void callFunction (const char *func, const char *arg1);

/*----------------%<---------------------------------------------------------*/

static void freeStream (StreamInfo *si) {
    stream_list = g_slist_remove (stream_list, si);
    g_free (si->url);
    if (si->mimetype)
        g_free (si->mimetype);
    if (si->target)
        g_free (si->target);
    free (si);
}

/*----------------%<---------------------------------------------------------*/

static NPError nsGetURL (NPP instance, const char* url, const char* target) {
    (void)instance;
    g_printf ("nsGetURL %s %s\n", url, target);
    addStream (url, 0L, target, 0L, 1);
    return NPERR_NO_ERROR;
}

static NPError nsPostURL (NPP instance, const char *url,
        const char *target, uint32 len, const char *buf, NPBool file) {
    (void)instance; (void)len; (void)buf; (void)file;
    g_printf ("nsPostURL %s %s\n", url, target);
    addStream (url, 0L, target, 0L, 1);
    return NPERR_NO_ERROR;
}

static NPError nsRequestRead (NPStream *stream, NPByteRange *rangeList) {
    (void)stream; (void)rangeList;
    g_printf ("nsRequestRead\n");
    return NPERR_NO_ERROR;
}

static NPError nsNewStream (NPP instance, NPMIMEType type,
        const char *target, NPStream **stream) {
    (void)instance; (void)type; (void)stream; (void)target;
    g_printf ("nsNewStream\n");
    return NPERR_NO_ERROR;
}

static int32 nsWrite (NPP instance, NPStream* stream, int32 len, void *buf) {
    (void)instance; (void)len; (void)buf; (void)stream;
    g_printf ("nsWrite\n");
    return 0;
}

static NPError nsDestroyStream (NPP instance, NPStream *stream, NPError reason) {
    (void)instance; (void)stream; (void)reason;
    g_printf ("nsDestroyStream\n");
    g_timeout_add (0, destroyStream, g_slist_nth_data (stream_list, 0));
    return NPERR_NO_ERROR;
}

static void nsStatus (NPP instance, const char* message) {
    (void)instance;
    g_printf ("NPN_Status %s\n", message);
}

static const char* nsUserAgent (NPP instance) {
    (void)instance;
    g_printf ("NPN_UserAgent\n");
    return "";
}

static void *nsAlloc (uint32 size) {
    return malloc (size);
}

static void nsMemFree (void* ptr) {
    free (ptr);
}

static uint32 nsMemFlush (uint32 size) {
    (void)size;
    g_printf ("NPN_MemFlush\n");
    return 0;
}

static void nsReloadPlugins (NPBool reloadPages) {
    (void)reloadPages;
    g_printf ("NPN_ReloadPlugins\n");
}

static JRIEnv* nsGetJavaEnv () {
    g_printf ("NPN_GetJavaEnv\n");
    return NULL;
}

static jref nsGetJavaPeer (NPP instance) {
    (void)instance;
    g_printf ("NPN_GetJavaPeer\n");
    return NULL;
}

static NPError nsGetURLNotify (NPP instance, const char* url, const char* target, void *notify) {
    (void)instance;
    g_printf ("NPN_GetURLNotify %s %s\n", url, target);
    addStream (url, 0L, target, notify, 1);
    return NPERR_NO_ERROR;
}

static NPError nsPostURLNotify (NPP instance, const char* url, const char* target, uint32 len, const char* buf, NPBool file, void *notify) {
    (void)instance; (void)len; (void)buf; (void)file;
    g_printf ("NPN_PostURLNotify\n");
    addStream (url, 0L, target, notify, 1);
    return NPERR_NO_ERROR;
}

static NPError nsGetValue (NPP instance, NPNVariable variable, void *value) {
    (void)instance;
    g_printf ("NPN_GetValue %d\n", variable & ~NP_ABI_MASK);
    switch (variable) {
        case NPNVxDisplay:
            *(void**)value = (void*)(long) gdk_x11_get_default_xdisplay ();
            break;
        case NPNVxtAppContext:
            *(void**)value = NULL;
            break;
        case NPNVnetscapeWindow:
            g_printf ("NPNVnetscapeWindow\n");
            break;
        case NPNVjavascriptEnabledBool:
            *(int*)value = 1;
            break;
        case NPNVasdEnabledBool:
            *(int*)value = 0;
            break;
        case NPNVisOfflineBool:
            *(int*)value = 0;
            break;
        case NPNVserviceManager:
            *(int*)value = 0;
            break;
        case NPNVToolkit:
            *(int*)value = 2; /* ?? */
            break;
        case NPNVSupportsXEmbedBool:
            *(int*)value = 1;
            break;
        default:
            *(int*)value = 0;
            g_printf ("unknown value\n");
    }
    return NPERR_NO_ERROR;
}

static NPError nsSetValue (NPP instance, NPPVariable variable, void *value) {
    /* NPPVpluginWindowBool */
    (void)instance; (void)value;
    g_printf ("NPN_SetValue %d\n", variable & ~NP_ABI_MASK);
    return NPERR_NO_ERROR;
}

static void nsInvalidateRect (NPP instance, NPRect *invalidRect) {
    (void)instance; (void)invalidRect;
    g_printf ("NPN_InvalidateRect\n");
}

static void nsInvalidateRegion (NPP instance, NPRegion invalidRegion) {
    (void)instance; (void)invalidRegion;
    g_printf ("NPN_InvalidateRegion\n");
}

static void nsForceRedraw (NPP instance) {
    (void)instance;
    g_printf ("NPN_InvalidateRegion\n");
    g_printf ("NPN_ForceRedraw\n");
}

static NPIdentifier nsGetStringIdentifier (const NPUTF8* name) {
    (void)name;
    g_printf ("NPN_GetStringIdentifier\n");
    return NULL;
}

static void nsGetStringIdentifiers (const NPUTF8** names, int32_t nameCount, NPIdentifier* identifiers) {
    (void)names; (void)nameCount; (void)identifiers;
    g_printf ("NPN_GetStringIdentifiers\n");
}

/*----------------%<---------------------------------------------------------*/

static void shutDownPlugin() {
    if (npShutdown) {
        if (npp) {
            np_funcs.destroy (npp, &saved_data);
            free (npp);
            npp = 0L;
        }
        npShutdown();
        npShutdown = 0;
    }
}

static void removeStream (NPReason reason) {
    StreamInfo *si= (StreamInfo *) g_slist_nth_data (stream_list, 0);

    if (stdin_read_watch)
        gdk_input_remove (stdin_read_watch);
    stdin_read_watch = 0;

    if (si) {
        g_printf ("data received %d\n", si->stream_pos);
        np_funcs.destroystream (npp, &si->np_stream, reason);
        if (si->notify)
            np_funcs.urlnotify (npp, si->url, reason, si->notify_data);
        freeStream (si);
        si = (StreamInfo*) g_slist_nth_data (stream_list, 0);
        if (si && callback_service)
            g_timeout_add (0, requestStream, si);
    }
}

static StreamInfo *addStream (const char *url, const char *mime, const char *target, void *notify_data, int notify) {
    StreamInfo *si = (StreamInfo*) g_slist_nth_data (stream_list, 0);
    int req = !si ? 1 : 0;
    si = (StreamInfo *) malloc (sizeof (StreamInfo));

    g_printf ("new stream\n");
    memset (si, 0, sizeof (StreamInfo));
    si->url = g_strdup (url);
    si->np_stream.url = si->url;
    if (mime)
        si->mimetype = g_strdup (mime);
    if (target)
        si->target = g_strdup (target);
    si->notify_data = notify_data;
    si->notify = notify;
    stream_list = g_slist_append (stream_list, si);

    if (req)
        g_timeout_add (0, requestStream, si);

    return si;
}

static void readStdin (gpointer d, gint src, GdkInputCondition cond) {
    StreamInfo *si = (StreamInfo*) g_slist_nth_data (stream_list, 0);
    gsize count = read (src,
            stream_buf + si->stream_buf_pos,
            sizeof (stream_buf) - si->stream_buf_pos);
    (void)d;
    g_assert (si);
    if (count > 0) {
        int32 sz;
        si->stream_buf_pos += count;
        sz = np_funcs.writeready (npp, &si->np_stream);
        if (sz > 0) {
            sz = np_funcs.write (npp, &si->np_stream, si->stream_pos,
                    si->stream_buf_pos > sz ? sz : si->stream_buf_pos,
                    stream_buf);
            if (sz == si->stream_buf_pos)
                si->stream_buf_pos = 0;
            else if (sz > 0) {
                si->stream_buf_pos -= sz;
                memmove (stream_buf, stream_buf + sz, si->stream_buf_pos);
            } else {
            }
            si->stream_pos += sz > 0 ? sz : 0;
        } else {
        }
        if (si->stream_pos == si->total) {
            if (si->stream_pos)
                removeStream (si->reason);
            else
                g_timeout_add (0, destroyStream, si);
        }
    } else {
        removeStream (NPRES_DONE); /* only for 'cat foo | knpplayer' */
    }
}

static int initPlugin (const char *plugin_lib) {
    NPNetscapeFuncs ns_funcs;
    NPError np_err;

    g_printf ("starting %s with %s\n", plugin_lib, object_url);
    library = g_module_open (plugin_lib, G_MODULE_BIND_LAZY);
    if (!library) {
        g_printf ("failed to load %s\n", plugin_lib);
        return -1;
    }
    if (!g_module_symbol (library,
                "NP_GetMIMEDescription", (gpointer *)&npGetMIMEDescription)) {
        g_printf ("undefined reference to load NP_GetMIMEDescription\n");
        return -1;
    }
    if (!g_module_symbol (library,
                "NP_Initialize", (gpointer *)&npInitialize)) {
        g_printf ("undefined reference to load NP_Initialize\n");
        return -1;
    }
    if (!g_module_symbol (library,
                "NP_Shutdown", (gpointer *)&npShutdown)) {
        g_printf ("undefined reference to load NP_Shutdown\n");
        return -1;
    }
    g_printf ("startup succeeded %s\n", npGetMIMEDescription ());
    memset (&ns_funcs, 0, sizeof (NPNetscapeFuncs));
    ns_funcs.size = sizeof (NPNetscapeFuncs);
    ns_funcs.version = (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR;
    ns_funcs.geturl = nsGetURL;
    ns_funcs.posturl = nsPostURL;
    ns_funcs.requestread = nsRequestRead;
    ns_funcs.newstream = nsNewStream;
    ns_funcs.write = nsWrite;
    ns_funcs.destroystream = nsDestroyStream;
    ns_funcs.status = nsStatus;
    ns_funcs.uagent = nsUserAgent;
    ns_funcs.memalloc = nsAlloc;
    ns_funcs.memfree = nsMemFree;
    ns_funcs.memflush = nsMemFlush;
    ns_funcs.reloadplugins = nsReloadPlugins;
    ns_funcs.getJavaEnv = nsGetJavaEnv;
    ns_funcs.getJavaPeer = nsGetJavaPeer;
    ns_funcs.geturlnotify = nsGetURLNotify;
    ns_funcs.posturlnotify = nsPostURLNotify;
    ns_funcs.getvalue = nsGetValue;
    ns_funcs.setvalue = nsSetValue;
    ns_funcs.invalidaterect = nsInvalidateRect;
    ns_funcs.invalidateregion = nsInvalidateRegion;
    ns_funcs.forceredraw = nsForceRedraw;
    ns_funcs.getstringidentifier = nsGetStringIdentifier;
    ns_funcs.getstringidentifiers = nsGetStringIdentifiers;
    /*ns_funcs.getintidentifier;
    ns_funcs.identifierisstring;
    ns_funcs.utf8fromidentifier;
    ns_funcs.intfromidentifier;
    ns_funcs.createobject;
    ns_funcs.retainobject;
    ns_funcs.releaseobject;
    ns_funcs.invoke;
    ns_funcs.invokeDefault;
    ns_funcs.evaluate;
    ns_funcs.getproperty;
    ns_funcs.setproperty;
    ns_funcs.removeproperty;
    ns_funcs.hasproperty;
    ns_funcs.hasmethod;
    ns_funcs.releasevariantvalue;
    ns_funcs.setexception;
    ns_funcs.pushpopupsenabledstate;
    ns_funcs.poppopupsenabledstate;*/

    np_funcs.size = sizeof (NPPluginFuncs);

    np_err = npInitialize (&ns_funcs, &np_funcs);
    if (np_err != NPERR_NO_ERROR) {
        g_printf ("NP_Initialize failure %d\n", np_err);
        npShutdown = 0;
        return -1;
    }
    return 0;
}

static int newPlugin (NPMIMEType mime, int16 argc, char *argn[], char *argv[]) {
    NPWindow window;
    NPSetWindowCallbackStruct ws_info;
    NPError np_err;
    Display *display;
    int screen;
    int i;
    unsigned int width = 320, height = 240;

    npp = (NPP_t*)malloc (sizeof (NPP_t));
    memset (npp, 0, sizeof (NPP_t));
    /*np_err = np_funcs.getvalue ((void*)npp, NPPVpluginNeedsXEmbed, (void*)&np_value);
    if (np_err != NPERR_NO_ERROR || !np_value) {
        g_printf ("NPP_GetValue NPPVpluginNeedsXEmbed failure %d\n", np_err);
        shutDownPlugin();
        return -1;
    }*/
    for (i = 0; i < argc; i++) {
        if (!strcasecmp (argn[i], "width"))
            width = strtol (argv[i], 0L, 10);
        else if (!strcasecmp (argn[i], "height"))
            height = strtol (argv[i], 0L, 10);
    }
    np_err = np_funcs.newp (mime, npp, NP_EMBED, argc, argn, argv, saved_data);
    if (np_err != NPERR_NO_ERROR) {
        g_printf ("NPP_New failure %d %p %p\n", np_err, np_funcs, np_funcs.newp);
        return -1;
    }
    memset (&window, 0, sizeof (NPWindow));
    display = gdk_x11_get_default_xdisplay ();
    window.x = 0;
    window.y = 0;
    window.width = width;
    window.height = height;
    window.window = (void*)socket_id;
    ws_info.type = 1; /*NP_SetWindow;*/
    screen = DefaultScreen (display);
    ws_info.display = (void*)(long)display;
    ws_info.visual = (void*)(long)DefaultVisual (display, screen);
    ws_info.colormap = DefaultColormap (display, screen);
    ws_info.depth = DefaultDepth (display, screen);
    g_printf ("display %dx%d\n", width, height);
    window.ws_info = (void*)&ws_info;

    np_err = np_funcs.setwindow (npp, &window);
    return 0;
}

static gboolean requestStream (void * p) {
    StreamInfo *si = (StreamInfo*) p;
    g_printf ("requestStream\n");
    if (si && si == g_slist_nth_data (stream_list, 0)) {
        NPError err;
        uint16 stype = NP_NORMAL;
        si->np_stream.notifyData = si->notify_data;
        err = np_funcs.newstream (npp, si->mimetype, &si->np_stream, 0, &stype);
        if (err != NPERR_NO_ERROR) {
            g_printerr ("newstream error %d\n", err);
            freeStream (si);
            return 0;
        }
        g_assert (!stdin_read_watch);
        stdin_read_watch = gdk_input_add (0, GDK_INPUT_READ, readStdin, NULL);
        callFunction ("getUrl", si->url);
    }
    return 0; /* single shot */
}

static gboolean destroyStream (void * p) {
    StreamInfo *si = (StreamInfo*) p;
    g_printf ("destroyStream\n");
    if (si && si == g_slist_nth_data (stream_list, 0))
        callFunction ("finish", NULL);
    return 0; /* single shot */
}

static gpointer newStream (const char *url, const char *mime,
        int argc, char *argn[], char *argv[]) {
    StreamInfo *si;
    g_printf ("new stream %s %s %d\n", url, mime, g_main_depth ());
    if (!npp && (initPlugin (plugin) || newPlugin (mimetype, argc, argn, argv)))
        return 0L;
    si = addStream (url, mime, 0L, 0L, 0);
    return si;
}

/*----------------%<---------------------------------------------------------*/

static DBusHandlerResult dbusFilter (DBusConnection * connection,
        DBusMessage *msg, void * user_data) {
    DBusMessageIter args;
    const char *sender = dbus_message_get_sender (msg);
    const char *iface = "org.kde.kmplayer.backend";
    (void)user_data; (void)connection;
    if (!dbus_message_has_destination (msg, service_name))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    g_printf("dbusFilter %s %s\n", sender,dbus_message_get_interface (msg));
    if (dbus_message_is_method_call (msg, iface, "play")) {
        DBusMessageIter ait;
        char *param = 0;
        unsigned int params;
        char **argn = NULL;
        char **argv = NULL;
        int i;
        if (!dbus_message_iter_init (msg, &args) ||
                DBUS_TYPE_STRING != dbus_message_iter_get_arg_type (&args)) {
            g_printerr ("missing url arg");
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        dbus_message_iter_get_basic (&args, &param);
        object_url = g_strdup (param);
        if (!dbus_message_iter_next (&args) ||
                DBUS_TYPE_STRING != dbus_message_iter_get_arg_type (&args)) {
            g_printerr ("missing mimetype arg");
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        dbus_message_iter_get_basic (&args, &param);
        mimetype = g_strdup (param);
        if (!dbus_message_iter_next (&args) ||
                DBUS_TYPE_STRING != dbus_message_iter_get_arg_type (&args)) {
            g_printerr ("missing plugin arg");
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        dbus_message_iter_get_basic (&args, &param);
        plugin = g_strdup (param);
        if (!dbus_message_iter_next (&args) ||
                DBUS_TYPE_UINT32 != dbus_message_iter_get_arg_type (&args)) {
            g_printerr ("missing param count arg");
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        dbus_message_iter_get_basic (&args, &params);
        if (params > 0 && params < 100) {
            argn = (char**) malloc (params * sizeof (char *));
            argv = (char**) malloc (params * sizeof (char *));
        }
        if (!dbus_message_iter_next (&args) ||
                DBUS_TYPE_ARRAY != dbus_message_iter_get_arg_type (&args)) {
            g_printerr ("missing params array");
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        dbus_message_iter_recurse (&args, &ait);
        for (i = 0; i < params; i++) {
            char *key, *value;
            DBusMessageIter di;
            if (dbus_message_iter_get_arg_type (&ait) != DBUS_TYPE_DICT_ENTRY)
                break;
            dbus_message_iter_recurse (&ait, &di);
            if (DBUS_TYPE_STRING != dbus_message_iter_get_arg_type (&di))
                break;
            dbus_message_iter_get_basic (&di, &key);
            if (!dbus_message_iter_next (&di) ||
                    DBUS_TYPE_STRING != dbus_message_iter_get_arg_type (&di))
                break;
            dbus_message_iter_get_basic (&di, &value);
            argn[i] = g_strdup (strcasecmp (key, "flashvars") ? key : "flashlight");
            argv[i] = g_strdup (value);
            g_printf ("param %d:%s='%s'\n", i + 1, argn[i], value);
            if (!dbus_message_iter_next (&ait))
                params = i + 1;
        }
        g_printf ("play %s %s %s params:%d\n", object_url, mimetype, plugin, i);
        newStream (object_url, mimetype, i, argn, argv);
    } else if (dbus_message_is_method_call (msg, iface, "getUrlNotify")) {
        StreamInfo *si = (StreamInfo*) g_slist_nth_data (stream_list, 0);
        if (si && dbus_message_iter_init (msg, &args) && 
                DBUS_TYPE_UINT32 == dbus_message_iter_get_arg_type (&args)) {
            dbus_message_iter_get_basic (&args, &si->total);
            if (dbus_message_iter_next (&args) &&
                   DBUS_TYPE_UINT32 == dbus_message_iter_get_arg_type (&args)) {
                dbus_message_iter_get_basic (&args, &si->reason);
                g_printf ("getUrlNotify bytes:%d reason:%d\n", si->total, si->reason);
                if (si->stream_pos == si->total)
                    removeStream (si->reason);
            }
        }
    } else if (dbus_message_is_method_call (msg, iface, "quit")) {
        g_printf ("quit\n");
        shutDownPlugin();
        gtk_main_quit();
    }
    return DBUS_HANDLER_RESULT_HANDLED;
}

static void callFunction (const char *func, const char *arg1) {
    g_printf ("call %s(%s)\n", func, arg1 ? arg1 : "");
    if (callback_service) {
        DBusMessage *msg = dbus_message_new_method_call (
                callback_service,
                callback_path,
                "org.kde.kmplayer.callback",
                func);
        if (arg1)
            dbus_message_append_args (
                    msg, DBUS_TYPE_STRING, &arg1, DBUS_TYPE_INVALID);
        dbus_message_set_no_reply (msg, TRUE);
        dbus_connection_send (dbus_connection, msg, NULL);
        dbus_message_unref (msg);
        dbus_connection_flush (dbus_connection);
    }
}

/*----------------%<---------------------------------------------------------*/

static void pluginAdded (GtkSocket *socket, gpointer d) {
    (void)socket; (void)d;
    g_printf ("pluginAdded\n");
    callFunction ("plugged", NULL);
}

static void windowCreatedEvent (GtkWidget *w, gpointer d) {
    (void)d;
    g_printf ("windowCreatedEvent\n");
    socket_id = gtk_socket_get_id (GTK_SOCKET (xembed));
    if (parent_id) {
        /*gdk_window_withdraw (w->window);
        gdk_window_reparent(w->window, gdk_window_foreign_new(parent_id), 0, 0);*/
        XReparentWindow (gdk_x11_drawable_get_xdisplay (w->window),
                gdk_x11_drawable_get_xid (w->window),
                parent_id,
                0, 0);
    }
    if (!callback_service) {
        char *argn[] = { "WIDTH", "HEIGHT", "debug", "SRC" };
        char *argv[] = { "440", "330", g_strdup("yes"), g_strdup(object_url) };
        newStream (object_url, mimetype, 4, argn, argv);
    }
}

static gboolean windowCloseEvent (GtkWidget *w, GdkEvent *e, gpointer d) {
    (void)w; (void)e; (void)d;
    shutDownPlugin();
    return FALSE;
}

static void windowDestroyEvent (GtkWidget *w, gpointer d) {
    (void)w; (void)d;
    gtk_main_quit();
}

static gboolean initPlayer (void * p) {
    /*when called from kmplayer if (!callback_service) {*/
        GtkWidget *window;
        GdkColormap *color_map;
        GdkColor bg_color;
        (void)p;

        window = gtk_window_new (parent_id
                ? GTK_WINDOW_POPUP
                : GTK_WINDOW_TOPLEVEL);
        xembed = gtk_socket_new();
        /*if (parent_id)
            gtk_window_set_decorated (GTK_WINDOW (window), false);*/
        /*if (parent_id)
        //    gtk_widget_set_parent (window, gdk_window_foreign_new (parent_id));*/
        g_signal_connect (G_OBJECT (window), "delete_event",
                G_CALLBACK (windowCloseEvent), NULL);
        g_signal_connect (G_OBJECT (window), "destroy",
                G_CALLBACK (windowDestroyEvent), NULL);
        g_signal_connect (G_OBJECT (window), "realize",
                GTK_SIGNAL_FUNC (windowCreatedEvent), NULL);

        g_signal_connect (G_OBJECT (xembed), "plug-added",
                GTK_SIGNAL_FUNC (pluginAdded), NULL);
        color_map = gdk_colormap_get_system();
        gdk_colormap_query_color (color_map, 0, &bg_color);
        gtk_widget_modify_bg (xembed, GTK_STATE_NORMAL, &bg_color);

        gtk_container_add (GTK_CONTAINER (window), xembed);
        if (parent_id)
            gtk_widget_set_size_request (window, 1024, 1024);
        else
            gtk_widget_set_size_request (window, 440, 330);
        g_printf ("dis %p\n", gdk_display_get_default ());
        gtk_widget_show_all (window);
    /*} else {*/
    if (callback_service && callback_path) {
        DBusError dberr;
        const char *serv = "type='method_call',interface='org.kde.kmplayer.backend'";
        char myname[64];

        dbus_error_init (&dberr);
        dbus_connection = dbus_bus_get (DBUS_BUS_SESSION, &dberr);
        if (!dbus_connection) {
            g_printerr ("Failed to open connection to bus: %s\n",
                    dberr.message);
            exit (1);
        }
        g_sprintf (myname, "org.kde.kmplayer.npplayer-%d", getpid ());
        service_name = g_strdup (myname);
        g_printf ("using service %s was '%s'\n", service_name, dbus_bus_get_unique_name (dbus_connection));
        dbus_connection_setup_with_g_main (dbus_connection, 0L);
        dbus_bus_request_name (dbus_connection, service_name, 
                DBUS_NAME_FLAG_REPLACE_EXISTING, &dberr);
        if (dbus_error_is_set (&dberr)) {
            g_printerr ("Failed to register name: %s\n", dberr.message);
            dbus_connection_unref (dbus_connection);
            return -1;
        }
        dbus_bus_add_match (dbus_connection, serv, &dberr);
        if (dbus_error_is_set (&dberr)) {
            g_printerr ("dbus_bus_add_match error: %s\n", dberr.message);
            dbus_connection_unref (dbus_connection);
            return -1;
        }
        dbus_connection_add_filter (dbus_connection, dbusFilter, 0L, 0L);

        /* TODO: remove DBUS_BUS_SESSION and create a private connection */
        callFunction ("running", service_name);

        dbus_connection_flush (dbus_connection);
    }
    return 0; /* single shot */
}

int main (int argc, char **argv) {
    int i;
    gtk_init (&argc, &argv);

    for (i = 1; i < argc; i++) {
        if (!strcmp (argv[i], "-p") && ++i < argc) {
            plugin = g_strdup (argv[i]);
        } else if (!strcmp (argv[i], "-cb") && ++i < argc) {
            gchar *cb = g_strdup (argv[i]);
            gchar *path = strchr(cb, '/');
            if (path) {
                callback_path = g_strdup (path);
                *path = 0;
            }
            callback_service = g_strdup (cb);
            g_free (cb);
        } else if (!strcmp (argv[i], "-m") && ++i < argc) {
            mimetype = g_strdup (argv[i]);
        } else if (!strcmp (argv [i], "-wid") && ++i < argc) {
            parent_id = strtol (argv[i], 0L, 10);
        } else
            object_url = g_strdup (argv[i]);
    }
    if (!callback_service && !(object_url && mimetype && plugin)) {
        g_fprintf(stderr, "Usage: %s <-m mimetype -p plugin url|-cb service -wid id>\n", argv[0]);
        return 1;
    }
    g_timeout_add (0, initPlayer, NULL);

    fcntl (0, F_SETFL, fcntl (0, F_GETFL) | O_NONBLOCK);

    g_printf ("entering gtk_main\n");

    gtk_main();

    if (dbus_connection)
        dbus_connection_unref (dbus_connection);

    return 0;
}
