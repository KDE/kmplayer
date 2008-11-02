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

/* gcc -o knpplayer `pkg-config --libs --cflags gtk+-x11-2.0` `pkg-config --libs --cflags dbus-glib-1` `pkg-config --libs --cflags gthread-2.0` npplayer.c

http://devedge-temp.mozilla.org/library/manuals/2002/plugin/1.0/
http://dbus.freedesktop.org/doc/dbus/libdbus-tutorial.html
*/

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>

#include <glib/gprintf.h>
#include <glib/gthread.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include <dbus/dbus-glib-lowlevel.h>
#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>

#define XP_UNIX
#define MOZ_X11
#include "moz-sdk/npupp.h"

#define INITIAL_WINDOW_WIDTH 1920

typedef const char* (* NP_LOADDS NP_GetMIMEDescriptionUPP)();
typedef NPError (* NP_GetValueUPP)(void *inst, NPPVariable var, void *value);
typedef NPError (* NP_InitializeUPP)(NPNetscapeFuncs*, NPPluginFuncs*);
typedef NPError (* NP_ShutdownUPP)(void);

static gchar *plugin;
static gchar *object_url;
static gchar *mimetype;

static DBusConnection *dbus_connection;
static DBusObjectPathVTable stream_vtable;
static char *service_name;
static gchar *callback_service;
static gchar *callback_path;
static GModule *library;
static GtkWidget *xembed;
static Window socket_id;
static Window parent_id;
static int top_w, top_h;
static int update_dimension_timer;
static int stdin_read_watch;

static NPPluginFuncs np_funcs;       /* plugin functions              */
static NPP npp;                      /* single instance of the plugin */
static NPWindow np_window;
static NPObject *js_window;
static NPSavedData *saved_data;
static NPClass js_class;
static GTree *stream_list;
static gpointer current_stream_id;
static uint32_t stream_chunk_size;
static char stream_buf[32 * 1024];
static unsigned int stream_buf_pos;
static int stream_id_counter;
static GTree *identifiers;
typedef struct _StreamInfo {
    NPStream np_stream;
    /*unsigned int stream_buf_pos;*/
    unsigned int stream_pos;
    unsigned int total;
    unsigned int reason;
    char *url;
    char *mimetype;
    char *target;
    bool notify;
    bool called_plugin;
    bool destroyed;
} StreamInfo;
struct JsObject;
typedef struct _JsObject {
    NPObject npobject;
    struct _JsObject * parent;
    char * name;
} JsObject;

static NP_GetMIMEDescriptionUPP npGetMIMEDescription;
static NP_GetValueUPP npGetValue;
static NP_InitializeUPP npInitialize;
static NP_ShutdownUPP npShutdown;

static void callFunction(int stream, const char *func, int first_arg_type, ...);
static void readStdin (gpointer d, gint src, GdkInputCondition cond);
static char *evaluate (const char *script, bool store);

static
DBusHandlerResult dbusStreamMessage(DBusConnection *c, DBusMessage *m, void *u);
static void dbusStreamUnregister (DBusConnection *conn, void *user_data);

/*----------------%<---------------------------------------------------------*/

static void print (const char * format, ...) {
    va_list vl;
    va_start (vl, format);
    vprintf (format, vl);
    va_end (vl);
    fflush (stdout);
}

static void *nsAlloc (uint32 size) {
    return g_malloc (size);
}

static void nsMemFree (void* ptr) {
    g_free (ptr);
}

static void createPath (int stream, char *buf, int buf_len) {
    strncpy (buf, callback_path, buf_len -1);
    buf [buf_len -1] = 0;
    if (stream > -1) {
        int len = strlen (buf);
        snprintf (buf + len, buf_len - len, "/stream_%d", stream);
    }
}

/*----------------%<---------------------------------------------------------*/

static gint streamCompare (gconstpointer a, gconstpointer b) {
    return (long)a - (long)b;
}

static void freeStream (StreamInfo *si) {
    char stream_name[64];
    sprintf (stream_name, "/stream_%d", (long) si->np_stream.ndata);
    if (!g_tree_remove (stream_list, si->np_stream.ndata))
        print ("WARNING freeStream not in tree\n");
    else
        dbus_connection_unregister_object_path (dbus_connection, stream_name);
    g_free (si->url);
    if (si->mimetype)
        g_free (si->mimetype);
    if (si->target)
        g_free (si->target);
    nsMemFree (si);
}

static gboolean requestStream (void * p) {
    StreamInfo *si = (StreamInfo *) g_tree_lookup (stream_list, p);
    if (si) {
        char *path = (char *)nsAlloc (64);
        char *target = si->target ? si->target : g_strdup ("");
        if (!callback_service)
            current_stream_id = p;
        if (!stdin_read_watch)
            stdin_read_watch = gdk_input_add (0, GDK_INPUT_READ, readStdin, NULL);
        createPath ((int)(long)p, path, 64);
        callFunction (-1, "request_stream",
                DBUS_TYPE_STRING, &path,
                DBUS_TYPE_STRING, &si->url,
                DBUS_TYPE_STRING, &target, DBUS_TYPE_INVALID);
        nsMemFree (path);
        if (!si->target)
            g_free (target);
    } else {
        print ("requestStream %d not found", (long) p);
    }
    return 0; /* single shot */
}

static gboolean destroyStream (void * p) {
    StreamInfo *si = (StreamInfo *) g_tree_lookup (stream_list, p);
    print ("FIXME destroyStream\n");
    if (si)
        callFunction ((int)(long)p, "destroy", DBUS_TYPE_INVALID);
    return 0; /* single shot */
}

static void removeStream (void * p) {
    StreamInfo *si = (StreamInfo *) g_tree_lookup (stream_list, p);

    if (si) {
        print ("removeStream %d rec:%d reason %d %dx%d\n", (long) p, si->stream_pos, si->reason, top_w, top_h);
        if (!si->destroyed) {
            if (si->called_plugin && !si->target) {
                si->np_stream.end = si->total;
                np_funcs.destroystream (npp, &si->np_stream, si->reason);
            }
            if (si->notify)
                np_funcs.urlnotify (npp,
                        si->url, si->reason, si->np_stream.notifyData);
        }
        freeStream (si);
    }
}

static int32_t writeStream (gpointer p, char *buf, uint32_t count) {
    int32_t sz = -1;
    StreamInfo *si = (StreamInfo *) g_tree_lookup (stream_list, p);
    /*print ("writeStream found %d count %d\n", !!si, count);*/
    if (si) {
        if (si->reason > NPERR_NO_ERROR) {
            sz = count; /* stream closed, skip remainings */
        } else {
            if (!si->called_plugin) {
                uint16 stype = NP_NORMAL;
                NPError err = np_funcs.newstream (npp, si->mimetype
                        ?  si->mimetype
                        : "text/plain",
                        &si->np_stream, 0, &stype);
                if (err != NPERR_NO_ERROR) {
                    g_printerr ("newstream error %d\n", err);
                    destroyStream (p);
                    return count; /* stream not accepted, skip remainings */
                }
                print ("newStream %d type:%d\n", (long) p, stype);
                si->called_plugin = true;
            }
            if (count) /* urls with a target returns zero bytes */
                sz = np_funcs.writeready (npp, &si->np_stream);
            if (sz > 0) {
                sz = np_funcs.write (npp, &si->np_stream, si->stream_pos,
                        (int32_t) count > sz ? sz : (int32_t) count, buf);
                if (sz < 0) { /*FIXME plugin destroys stream here*/
                    si->reason = NPERR_INVALID_PLUGIN_ERROR;
                    g_timeout_add (0, destroyStream, p);
                }
            } else {
                sz = 0;
            }
            si->stream_pos += sz;
            if (si->stream_pos == si->total) {
                if (si->stream_pos || !count) {
                    si->reason = NPRES_DONE;
                    removeStream (p);
                } else {
                    g_timeout_add (0, destroyStream, p);
                }
            }
        }
    }
    return sz;
}

static StreamInfo *addStream (const char *url, const char *mime, const char *target, void *notify_data, bool notify) {
    StreamInfo *si = (StreamInfo *) nsAlloc (sizeof (StreamInfo));
    char stream_name[64];

    memset (si, 0, sizeof (StreamInfo));
    si->url = g_strdup (url);
    si->np_stream.url = si->url;
    if (mime)
        si->mimetype = g_strdup (mime);
    if (target)
        si->target = g_strdup (target);
    si->np_stream.notifyData = notify_data;
    si->notify = notify;
    si->np_stream.ndata = (void *) (long) (stream_id_counter++);
    print ("add stream %d\n", (long) si->np_stream.ndata);
    sprintf (stream_name, "/stream_%d", (long) si->np_stream.ndata);
    if (!dbus_connection_register_object_path (dbus_connection, stream_name,
                &stream_vtable, si))
        g_printerr ("dbus_connection_register_object_path error\n");
    g_tree_insert (stream_list, si->np_stream.ndata, si);

    g_timeout_add (0, requestStream, si->np_stream.ndata);

    return si;
}

/*----------------%<---------------------------------------------------------*/

static void createJsName (JsObject * obj, char **name, uint32_t * len) {
    int slen = strlen (obj->name);
    if (obj->parent) {
        *len += slen + 1;
        createJsName (obj->parent, name, len);
    } else {
        *name = (char *) nsAlloc (*len + slen + 1);
        *(*name + *len + slen) = 0;
        *len = 0;
    }
    if (obj->parent) {
        *(*name + *len) = '.';
        *len += 1;
    }
    memcpy (*name + *len, obj->name, slen);
    *len += slen;
}

static char *nsVariant2Str (const NPVariant *value) {
    char *str;
    switch (value->type) {
        case NPVariantType_String:
            str = (char *) nsAlloc (value->value.stringValue.utf8length + 3);
            str[0] = str[value->value.stringValue.utf8length + 1] = '\'';
            strncpy (str + 1, value->value.stringValue.utf8characters,
                    value->value.stringValue.utf8length);
            str[value->value.stringValue.utf8length + 2] = 0;
            break;
        case NPVariantType_Int32:
            str = (char *) nsAlloc (16);
            snprintf (str, 15, "%d", value->value.intValue);
            break;
        case NPVariantType_Double:
            str = (char *) nsAlloc (64);
            snprintf (str, 63, "%f", value->value.doubleValue);
            break;
        case NPVariantType_Bool:
            str = strdup (value->value.boolValue ? "true" : "false");
            break;
        case NPVariantType_Null:
            str = strdup ("null");
            break;
        case NPVariantType_Object:
            if (&js_class == value->value.objectValue->_class) {
                JsObject *jv = (JsObject *) value->value.objectValue;
                char *val;
                uint32_t vlen = 0;
                createJsName (jv, &val, &vlen);
                str = strdup (val);
                nsMemFree (val);
            } else {
                str = strdup ("null"); /* TODO track plugin objects */
            }
            break;
        default:
            str = strdup ("");
            break;
    }
    return str;
}

/*----------------%<---------------------------------------------------------*/

static NPObject * nsCreateObject (NPP instance, NPClass *aClass) {
    NPObject *obj;
    if (aClass && aClass->allocate) {
        obj = aClass->allocate (instance, aClass);
    } else {
        obj = (NPObject *) nsAlloc (sizeof (NPObject));
        memset (obj, 0, sizeof (NPObject));
        obj->_class = aClass;
        /*obj = js_class.allocate (instance, &js_class);/ *add null class*/
        print ("NPN_CreateObject\n");
    }
    obj->referenceCount = 1;
    return obj;
}

static NPObject *nsRetainObject (NPObject *npobj) {
    /*print( "nsRetainObject %p\n", npobj);*/
    npobj->referenceCount++;
    return npobj;
}

static void nsReleaseObject (NPObject *obj) {
    /*print ("NPN_ReleaseObject\n");*/
    if (! (--obj->referenceCount))
        obj->_class->deallocate (obj);
}

static NPError nsGetURL (NPP instance, const char* url, const char* target) {
    (void)instance;
    print ("nsGetURL %s %s\n", url, target ? target : "");
    addStream (url, 0L, target, 0L, false);
    return NPERR_NO_ERROR;
}

static NPError nsPostURL (NPP instance, const char *url,
        const char *target, uint32 len, const char *buf, NPBool file) {
    (void)instance; (void)len; (void)buf; (void)file;
    print ("nsPostURL %s %s\n", url, target ? target : "");
    addStream (url, 0L, target, 0L, false);
    return NPERR_NO_ERROR;
}

static NPError nsRequestRead (NPStream *stream, NPByteRange *rangeList) {
    (void)stream; (void)rangeList;
    print ("nsRequestRead\n");
    return NPERR_NO_ERROR;
}

static NPError nsNewStream (NPP instance, NPMIMEType type,
        const char *target, NPStream **stream) {
    (void)instance; (void)type; (void)stream; (void)target;
    print ("nsNewStream\n");
    return NPERR_NO_ERROR;
}

static int32 nsWrite (NPP instance, NPStream* stream, int32 len, void *buf) {
    (void)instance; (void)len; (void)buf; (void)stream;
    print ("nsWrite\n");
    return 0;
}

static NPError nsDestroyStream (NPP instance, NPStream *stream, NPError reason) {
    StreamInfo *si = (StreamInfo *) g_tree_lookup (stream_list, stream->ndata);
    (void)instance;
    print ("nsDestroyStream\n");
    if (si) {
        si->reason = reason;
        si->destroyed = true;
        g_timeout_add (0, destroyStream, stream->ndata);
        return NPERR_NO_ERROR;
    }
    return NPERR_NO_DATA;
}

static void nsStatus (NPP instance, const char* message) {
    (void)instance;
    print ("NPN_Status %s\n", message ? message : "-");
}

static const char* nsUserAgent (NPP instance) {
    (void)instance;
    print ("NPN_UserAgent\n");
    return "";
}

static uint32 nsMemFlush (uint32 size) {
    (void)size;
    print ("NPN_MemFlush\n");
    return 0;
}

static void nsReloadPlugins (NPBool reloadPages) {
    (void)reloadPages;
    print ("NPN_ReloadPlugins\n");
}

static JRIEnv* nsGetJavaEnv () {
    print ("NPN_GetJavaEnv\n");
    return NULL;
}

static jref nsGetJavaPeer (NPP instance) {
    (void)instance;
    print ("NPN_GetJavaPeer\n");
    return NULL;
}

static NPError nsGetURLNotify (NPP instance, const char* url, const char* target, void *notify) {
    (void)instance;
    print ("NPN_GetURLNotify %s %s\n", url, target ? target : "");
    addStream (url, 0L, target, notify, true);
    return NPERR_NO_ERROR;
}

static NPError nsPostURLNotify (NPP instance, const char* url, const char* target, uint32 len, const char* buf, NPBool file, void *notify) {
    (void)instance; (void)len; (void)buf; (void)file;
    print ("NPN_PostURLNotify\n");
    addStream (url, 0L, target, notify, true);
    return NPERR_NO_ERROR;
}

static NPError nsGetValue (NPP instance, NPNVariable variable, void *value) {
    print ("NPN_GetValue %d\n", variable & ~NP_ABI_MASK);
    switch (variable) {
        case NPNVxDisplay:
            *(void**)value = (void*)(long) gdk_x11_get_default_xdisplay ();
            break;
        case NPNVxtAppContext:
            *(void**)value = NULL;
            break;
        case NPNVnetscapeWindow:
            print ("NPNVnetscapeWindow\n");
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
            *(int*)value = NPNVGtk2;
            break;
        case NPNVSupportsXEmbedBool:
            *(int*)value = 1;
            break;
        case NPNVWindowNPObject:
            if (!js_window) {
                JsObject *jo = (JsObject*) nsCreateObject (instance, &js_class);
                jo->name = g_strdup ("window");
                js_window = (NPObject *) jo;
            }
            *(NPObject**)value = nsRetainObject (js_window);
            break;
        case NPNVPluginElementNPObject: {
            JsObject * obj = (JsObject *) nsCreateObject (instance, &js_class);
            obj->name = g_strdup ("this");
            *(NPObject**)value = (NPObject *) obj;
            break;
        }
        default:
            *(int*)value = 0;
            print ("unknown value\n");
            return NPERR_GENERIC_ERROR;
    }
    return NPERR_NO_ERROR;
}

static NPError nsSetValue (NPP instance, NPPVariable variable, void *value) {
    /* NPPVpluginWindowBool */
    (void)instance; (void)value;
    print ("NPN_SetValue %d\n", variable & ~NP_ABI_MASK);
    return NPERR_NO_ERROR;
}

static void nsInvalidateRect (NPP instance, NPRect *invalidRect) {
    (void)instance; (void)invalidRect;
    print ("NPN_InvalidateRect\n");
}

static void nsInvalidateRegion (NPP instance, NPRegion invalidRegion) {
    (void)instance; (void)invalidRegion;
    print ("NPN_InvalidateRegion\n");
}

static void nsForceRedraw (NPP instance) {
    (void)instance;
    print ("NPN_ForceRedraw\n");
}

static NPIdentifier nsGetStringIdentifier (const NPUTF8* name) {
    /*print ("NPN_GetStringIdentifier %s\n", name);*/
    gpointer id = g_tree_lookup (identifiers, name);
    if (!id) {
        id = strdup (name);
        g_tree_insert (identifiers, id, id);
    }
    return id;
}

static void nsGetStringIdentifiers (const NPUTF8** names, int32_t nameCount,
        NPIdentifier* ids) {
    (void)names; (void)nameCount; (void)ids;
    print ("NPN_GetStringIdentifiers\n");
}

static NPIdentifier nsGetIntIdentifier (int32_t intid) {
    print ("NPN_GetIntIdentifier %d\n", intid);
    return (NPIdentifier) (long) intid;
}

static bool nsIdentifierIsString (NPIdentifier name) {
    print ("NPN_IdentifierIsString\n");
    return !!g_tree_lookup (identifiers, name);
}

static NPUTF8 * nsUTF8FromIdentifier (NPIdentifier name) {
    char *str = g_tree_lookup (identifiers, name);
    print ("NPN_UTF8FromIdentifier %s\n", str ? str : "not found");
    if (str)
        return strdup (str);
    return NULL;
}

static int32_t nsIntFromIdentifier (NPIdentifier identifier) {
    print ("NPN_IntFromIdentifier\n");
    return (int32_t) (long) identifier;
}

static bool nsInvoke (NPP instance, NPObject * npobj, NPIdentifier method,
        const NPVariant *args, uint32_t arg_count, NPVariant *result) {
    (void)instance;
    /*print ("NPN_Invoke %s\n", id);*/
    return npobj->_class->invoke (npobj, method, args, arg_count, result);
}

static bool nsInvokeDefault (NPP instance, NPObject * npobj,
        const NPVariant * args, uint32_t arg_count, NPVariant * result) {
    (void)instance;
    return npobj->_class->invokeDefault (npobj,args, arg_count, result);
}

static bool str2NPVariant (NPP instance, const char *str, NPVariant *result) {
    if (!str || !*str)
        return false;
    if (!strncmp (str, "o:", 2)) {
        JsObject *jo = (JsObject *)nsCreateObject (instance, &js_class);
        result->type = NPVariantType_Object;
        jo->name = g_strdup (str + 2);
        result->value.objectValue = (NPObject *)jo;
        print ("object\n");
    } else if (!strncmp (str, "s:", 2)) {
        result->type = NPVariantType_String;
        result->value.stringValue.utf8characters= g_strdup(str+2);
        result->value.stringValue.utf8length = strlen (str) - 2;
        print ("string %s\n", str + 2);
    } else if (!strncmp (str, "u:", 2)) {
        result->type = NPVariantType_Null;
        print ("null\n");
    } else if (!strncmp (str, "n:", 2)) {
        char *eptr;
        long l = strtol (str + 2, &eptr, 10);
        if (*eptr && *eptr == '.') {
            result->type = NPVariantType_Double;
            result->value.doubleValue = strtod (str + 2, NULL);
            print ("double %f\n", result->value.doubleValue);
        } else if (eptr != str + 2) {
            result->type = NPVariantType_Int32;
            result->value.intValue = (int)l;
            print ("int32 %d\n", l);
        } else {
            result->type = NPVariantType_Null;
            return false;
        }
    } else if (!strncmp (str, "b:", 2)) {
        result->type = NPVariantType_Bool;
        if (!strcasecmp (str + 2, "true")) {
            result->value.boolValue = true;
        } else {
            char *eptr;
            long l = strtol (str + 2, &eptr, 10);
            result->value.boolValue = eptr != str ? !!l : false;
        }
        print ("bool %d\n", result->value.boolValue);
    } else {
        return false;
    }
    return true;
}

static bool doEvaluate (NPP instance, NPObject * npobj, NPString * script,
        NPVariant * result) {
    char *result_string;
    bool success = false;
    (void) npobj; /*FIXME scope, search npobj window*/

    result_string = evaluate (script->utf8characters, true);

    if (result_string) {
        success = str2NPVariant (instance, result_string, result);
        g_free (result_string);
    }

    return success;
}

static bool nsEvaluate (NPP instance, NPObject * npobj, NPString * script,
        NPVariant * result) {
    NPString str;
    char *jsscript;
    char *escaped;
    bool res;

    print ("NPN_Evaluate:");
    escaped = g_strescape (script->utf8characters, "");
    str.utf8length = strlen (escaped) + 9;
    jsscript = (char *) nsAlloc (str.utf8length);
    sprintf (jsscript, "eval(\"%s\")", escaped);
    str.utf8characters = jsscript;

    res = doEvaluate (instance, npobj, &str, result);

    nsMemFree (jsscript);
    g_free (escaped);

    return res;
}

static bool nsGetProperty (NPP instance, NPObject * npobj,
        NPIdentifier property, NPVariant * result) {
    (void)instance;
    return npobj->_class->getProperty (npobj, property, result);
}

static bool nsSetProperty (NPP instance, NPObject * npobj,
        NPIdentifier property, const NPVariant *value) {
    (void)instance;
    return npobj->_class->setProperty (npobj, property, value);
}

static bool nsRemoveProperty (NPP inst, NPObject * npobj, NPIdentifier prop) {
    (void)inst;
    return npobj->_class->removeProperty (npobj, prop);
}

static bool nsHasProperty (NPP instance, NPObject * npobj, NPIdentifier prop) {
    (void)instance;
    return npobj->_class->hasProperty (npobj, prop);
}

static bool nsHasMethod (NPP instance, NPObject * npobj, NPIdentifier method) {
    (void)instance;
    return npobj->_class->hasMethod (npobj, method);
}

static void nsReleaseVariantValue (NPVariant * variant) {
    /*print ("NPN_ReleaseVariantValue\n");*/
    switch (variant->type) {
        case NPVariantType_String:
            if (variant->value.stringValue.utf8characters)
                g_free ((char *) variant->value.stringValue.utf8characters);
            break;
        case NPVariantType_Object:
            if (variant->value.objectValue)
                nsReleaseObject (variant->value.objectValue);
            break;
        default:
            break;
    }
    variant->type = NPVariantType_Null;
}

static void nsSetException (NPObject *npobj, const NPUTF8 *message) {
    (void)npobj;
    print ("NPN_SetException %s\n", message ? message : "-");
}

static bool nsPushPopupsEnabledState (NPP instance, NPBool enabled) {
    (void)instance;
    print ("NPN_PushPopupsEnabledState %d\n", enabled);
    return false;
}

static bool nsPopPopupsEnabledState (NPP instance) {
    (void)instance;
    print ("NPN_PopPopupsEnabledState\n");
    return false;
}

/*----------------%<---------------------------------------------------------*/

static bool doInvoke (uint32_t obj, const char *func, GSList *arglst,
        uint32_t arg_count, char **resultstring) {
    NPObject *npobj;
    NPVariant result;
    NPVariant *args = NULL;

    *resultstring = NULL;
    if (!obj) { /*TODO NPObject tracking */
        NPError np_err = np_funcs.getvalue ((void*)npp,
                NPPVpluginScriptableNPObject, (void*)&npobj);
        if (np_err == NPERR_NO_ERROR && npobj) {
            NPIdentifier method = nsGetStringIdentifier (func);

            if (nsHasMethod (npp, npobj, method)) {
                GSList *sl;
                int i;
                if (arg_count) {
                    args = (NPVariant *) nsAlloc (arg_count * sizeof (NPVariant));
                    memset (args, 0, arg_count * sizeof (NPVariant));
                    for (sl = arglst, i = 0; sl; sl = sl->next, i++)
                        str2NPVariant (npp, (const char *) sl->data, args + i);
                }
                if (nsInvoke (npp, npobj, method, args, arg_count, &result)) {
                    *resultstring = nsVariant2Str (&result);
                    nsReleaseVariantValue (&result);
                    print ("nsInvoke succes %s\n", *resultstring);
                } else {
                    print ("nsInvoke failure\n");
                }
                if (args) {
                    for (sl = arglst, i = 0; sl; sl = sl->next, i++)
                        nsReleaseVariantValue (args + i);
                    nsMemFree (args);
                }
            }
            nsReleaseObject (npobj);
        } else {
            print("no obj %d\n", obj);
        }
    }
    if (!*resultstring) {
        *resultstring = g_strdup ("error");
        return false;
    }
    return true;
}

static bool doGet (uint32_t obj, const char *prop, char **resultstring) {
    NPObject *npobj;
    NPVariant result;

    *resultstring = NULL;
    if (!obj) { /*TODO NPObject tracking */
        NPError np_err = np_funcs.getvalue ((void*)npp,
                NPPVpluginScriptableNPObject, (void*)&npobj);
        if (np_err == NPERR_NO_ERROR && npobj) {
            NPIdentifier identifier = nsGetStringIdentifier (prop);

            if (nsHasMethod (npp, npobj, identifier)) {
                *resultstring = g_strdup ("o:function");
            } else if (nsHasMethod (npp, npobj, identifier)) {
                if (nsGetProperty (npp, npobj, identifier, &result)) {
                    *resultstring = nsVariant2Str (&result);
                    nsReleaseVariantValue (&result);
                }
            }
            nsReleaseObject (npobj);
        }
    }
    if (!*resultstring) {
        *resultstring = g_strdup ("error");
        return false;
    }
    return true;
}

/*----------------%<---------------------------------------------------------*/

static NPObject * windowClassAllocate (NPP instance, NPClass *aClass) {
    (void)instance;
    /*print ("windowClassAllocate\n");*/
    JsObject * jo = (JsObject *) nsAlloc (sizeof (JsObject));
    memset (jo, 0, sizeof (JsObject));
    jo->npobject._class = aClass;
    return (NPObject *) jo;
}

static void windowClassDeallocate (NPObject *npobj) {
    JsObject *jo = (JsObject *) npobj;
    /*print ("windowClassDeallocate\n");*/
    if (jo->parent) {
        nsReleaseObject ((NPObject *) jo->parent);
    } else if (jo->name && !strncmp (jo->name, "this.__kmplayer__obj_", 21)) {
        char *script = (char *) nsAlloc (strlen (jo->name) + 7);
        char *result;
        sprintf (script, "%s=null;", jo->name);
        result = evaluate (script, false);
        nsMemFree (script);
        g_free (result);
    }
    if (jo->name)
        g_free (jo->name);
    if (npobj == js_window) {
        print ("WARNING deleting window object\n");
        js_window = NULL;
    }
    nsMemFree (npobj);
}

static void windowClassInvalidate (NPObject *npobj) {
    (void)npobj;
    print ("windowClassInvalidate\n");
}

static bool windowClassHasMethod (NPObject *npobj, NPIdentifier name) {
    (void)npobj; (void)name;
    print ("windowClassHasMehtod\n");
    return false;
}

static bool windowClassInvoke (NPObject *npobj, NPIdentifier method,
        const NPVariant *args, uint32_t arg_count, NPVariant *result) {
    JsObject * jo = (JsObject *) npobj;
    NPString str = { NULL, 0 };
    char buf[4096];
    int pos, i;
    bool res;
    char * id = (char *) g_tree_lookup (identifiers, method);
    /*print ("windowClassInvoke\n");*/

    result->type = NPVariantType_Null;
    result->value.objectValue = NULL;

    if (!id) {
        print ("Invoke invalid id\n");
        return false;
    }
    print ("Invoke %s\n", id);
    createJsName (jo, (char **)&str.utf8characters, &str.utf8length);
    pos = snprintf (buf, sizeof (buf), "%s.%s(", str.utf8characters, id);
    nsMemFree ((char *) str.utf8characters);
    for (i = 0; i < arg_count; i++) {
        char *arg = nsVariant2Str (args + i);
        pos += snprintf (buf + pos, sizeof (buf) - pos, i ? ",%s" : "%s", arg);
        nsMemFree (arg);
    }
    pos += snprintf (buf + pos, sizeof (buf) - pos, ")");

    str.utf8characters = buf;
    str.utf8length = pos;
    res = doEvaluate (npp, npobj, &str, result);

    return true;
}

static bool windowClassInvokeDefault (NPObject *npobj,
        const NPVariant *args, uint32_t arg_count, NPVariant *result) {
    (void)npobj; (void)args; (void)arg_count; (void)result;
    print ("windowClassInvokeDefault\n");
    return false;
}

static bool windowClassHasProperty (NPObject *npobj, NPIdentifier name) {
    (void)npobj; (void)name;
    print ("windowClassHasProperty\n");
    return false;
}

static bool windowClassGetProperty (NPObject *npobj, NPIdentifier property,
        NPVariant *result) {
    char * id = (char *) g_tree_lookup (identifiers, property);
    JsObject jo;
    NPString fullname = { NULL, 0 };
    bool res;

    print ("GetProperty %s\n", id);
    result->type = NPVariantType_Null;
    result->value.objectValue = NULL;

    if (!id)
        return false;

    if (!strcmp (((JsObject *) npobj)->name, "window") &&
                !strcmp (id, "top")) {
        result->type = NPVariantType_Object;
        result->value.objectValue = nsRetainObject (js_window);
        return true;
    }

    jo.name = id;
    jo.parent = (JsObject *) npobj;
    createJsName (&jo, (char **)&fullname.utf8characters, &fullname.utf8length);

    res = doEvaluate (npp, npobj, &fullname, result);

    nsMemFree ((char *) fullname.utf8characters);

    return res;
}

static bool windowClassSetProperty (NPObject *npobj, NPIdentifier property,
        const NPVariant *value) {
    char *id = (char *) g_tree_lookup (identifiers, property);
    char *script, *var_name, *var_val, *res;
    JsObject jo;
    uint32_t len = 0;

    if (!id)
        return false;

    jo.name = id;
    jo.parent = (JsObject *) npobj;
    createJsName (&jo, &var_name, &len);

    var_val = nsVariant2Str (value);
    script = (char *) nsAlloc (len + strlen (var_val) + 3);
    sprintf (script, "%s=%s;", var_name, var_val);
    nsMemFree (var_name);
    nsMemFree (var_val);
    print ("SetProperty %s\n", script);

    res = evaluate (script, false);
    if (res)
        g_free (res);
    nsMemFree (script);


    return true;
}

static bool windowClassRemoveProperty (NPObject *npobj, NPIdentifier name) {
    (void)npobj; (void)name;
    print ("windowClassRemoveProperty\n");
    return false;
}


/*----------------%<---------------------------------------------------------*/

static void shutDownPlugin() {
    if (npShutdown) {
        if (npp) {
            np_funcs.destroy (npp, &saved_data);
            nsMemFree (npp);
            npp = 0L;
        }
        npShutdown();
        npShutdown = 0;
    }
}

static void readStdin (gpointer p, gint src, GdkInputCondition cond) {
    char *buf_ptr = stream_buf;
    gsize bytes_read = read (src,
            stream_buf + stream_buf_pos,
            sizeof (stream_buf) - stream_buf_pos);
    (void)cond; (void)p;
    if (bytes_read > 0)
        stream_buf_pos += bytes_read;

    /*print ("readStdin %d\n", bytes_read);*/
    while (buf_ptr < stream_buf + stream_buf_pos) {
        uint32_t write_len;
        int32_t bytes_written;

        if (callback_service && !stream_chunk_size) {
            /* read header info */
            if (stream_buf + stream_buf_pos < buf_ptr + 2 * sizeof (uint32_t))
                break; /* need more data */
            current_stream_id = (gpointer)(long)*(uint32_t*)(buf_ptr);
            stream_chunk_size = *((uint32_t *)(buf_ptr + sizeof (uint32_t)));
        /*print ("header %d %d\n",(long)current_stream_id, stream_chunk_size);*/
            buf_ptr += 2 * sizeof (uint32_t);
            if (stream_chunk_size && stream_buf + stream_buf_pos == buf_ptr) {
                stream_buf_pos = 0;
                break; /* only read the header for chunk with data */
            }
        }
        /* feed it to the stream */
        write_len = stream_buf + stream_buf_pos - buf_ptr;
        if (callback_service && write_len > stream_chunk_size)
            write_len = stream_chunk_size;
        bytes_written = writeStream (current_stream_id, buf_ptr, write_len);
        if (bytes_written < 0) {
            print ("couldn't write to stream %d\n", (long)current_stream_id);
            bytes_written = write_len; /* assume stream destroyed, skip */
        }

        /* update chunk status */
        if (bytes_written > 0) {
            buf_ptr += bytes_written;
           /*print ("update chunk %d %d\n", bytes_written, stream_chunk_size);*/
            stream_chunk_size -= bytes_written;
        } else {
            /* FIXME if plugin didn't accept the data retry later, suspend stdin reading */
            break;
        }

    }
    /* update buffer */
    /*print ("buffer written:%d bufpos:%d\n", buf_ptr-stream_buf, stream_buf_pos);*/
    if (stream_buf + stream_buf_pos == buf_ptr) {
        stream_buf_pos = 0;
    } else {
        g_assert (buf_ptr < stream_buf + stream_buf_pos);
        stream_buf_pos -= (stream_buf + stream_buf_pos - buf_ptr);
        memmove (stream_buf, buf_ptr, stream_buf_pos);
    }
    if (bytes_read <= 0) { /* eof of stdin, only for 'cat foo | knpplayer' */
        StreamInfo*si=(StreamInfo*)g_tree_lookup(stream_list,current_stream_id);
        si->reason = NPRES_DONE;
        removeStream (current_stream_id);
        if (stdin_read_watch) {
            gdk_input_remove (stdin_read_watch);
            stdin_read_watch = 0;
        }
    }
}

static int initPlugin (const char *plugin_lib) {
    NPNetscapeFuncs ns_funcs;
    NPError np_err;
    char *pname;

    print ("starting %s with %s\n", plugin_lib, object_url);
    library = g_module_open (plugin_lib, G_MODULE_BIND_LAZY);
    if (!library) {
        print ("failed to load %s %s\n", plugin_lib, g_module_error ());
        return -1;
    }
    if (!g_module_symbol (library,
                "NP_GetMIMEDescription", (gpointer *)&npGetMIMEDescription)) {
        print ("undefined reference to load NP_GetMIMEDescription\n");
        return -1;
    }
    if (!g_module_symbol (library,
                "NP_GetValue", (gpointer *)&npGetValue)) {
        print ("undefined reference to load NP_GetValue\n");
    }
    if (!g_module_symbol (library,
                "NP_Initialize", (gpointer *)&npInitialize)) {
        print ("undefined reference to load NP_Initialize\n");
        return -1;
    }
    if (!g_module_symbol (library,
                "NP_Shutdown", (gpointer *)&npShutdown)) {
        print ("undefined reference to load NP_Shutdown\n");
        return -1;
    }
    print ("startup succeeded %s\n", npGetMIMEDescription ());
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
    ns_funcs.getintidentifier = nsGetIntIdentifier;
    ns_funcs.identifierisstring = nsIdentifierIsString;
    ns_funcs.utf8fromidentifier = nsUTF8FromIdentifier;
    ns_funcs.intfromidentifier = nsIntFromIdentifier;
    ns_funcs.createobject = nsCreateObject;
    ns_funcs.retainobject = nsRetainObject;
    ns_funcs.releaseobject = nsReleaseObject;
    ns_funcs.invoke = nsInvoke;
    ns_funcs.invokeDefault = nsInvokeDefault;
    ns_funcs.evaluate = nsEvaluate;
    ns_funcs.getproperty = nsGetProperty;
    ns_funcs.setproperty = nsSetProperty;
    ns_funcs.removeproperty = nsRemoveProperty;
    ns_funcs.hasproperty = nsHasProperty;
    ns_funcs.hasmethod = nsHasMethod;
    ns_funcs.releasevariantvalue = nsReleaseVariantValue;
    ns_funcs.setexception = nsSetException;
    ns_funcs.pushpopupsenabledstate = nsPushPopupsEnabledState;
    ns_funcs.poppopupsenabledstate = nsPopPopupsEnabledState;

    js_class.structVersion = NP_CLASS_STRUCT_VERSION;
    js_class.allocate = windowClassAllocate;
    js_class.deallocate = windowClassDeallocate;
    js_class.invalidate = windowClassInvalidate;
    js_class.hasMethod = windowClassHasMethod;
    js_class.invoke = windowClassInvoke;
    js_class.invokeDefault = windowClassInvokeDefault;
    js_class.hasProperty = windowClassHasProperty;
    js_class.getProperty = windowClassGetProperty;
    js_class.setProperty = windowClassSetProperty;
    js_class.removeProperty = windowClassRemoveProperty;

    np_funcs.size = sizeof (NPPluginFuncs);

    np_err = npInitialize (&ns_funcs, &np_funcs);
    if (np_err != NPERR_NO_ERROR) {
        print ("NP_Initialize failure %d\n", np_err);
        npShutdown = 0;
        return -1;
    }
    np_err = npGetValue (NULL, NPPVpluginNameString, &pname);
    if (np_err == NPERR_NO_ERROR)
        print ("NP_GetValue Name %s\n", pname);
    np_err = npGetValue (NULL, NPPVpluginDescriptionString, &pname);
    if (np_err == NPERR_NO_ERROR)
        print ("NP_GetValue Description %s\n", pname);
    return 0;
}

static int newPlugin (NPMIMEType mime, int16 argc, char *argn[], char *argv[]) {
    NPSetWindowCallbackStruct ws_info;
    NPError np_err;
    Display *display;
    int screen;
    int i;
    int needs_xembed;
    uint32_t width = 0, height = 0;

    for (i = 0; i < argc; i++) {
        if (!strcasecmp (argn[i], "width"))
            width = strtol (argv[i], 0L, 10);
        else if (!strcasecmp (argn[i], "height"))
            height = strtol (argv[i], 0L, 10);
    }
    if (width > 0 && height > 0)
        callFunction (-1, "dimension",
                DBUS_TYPE_INT32, &width, DBUS_TYPE_INT32, &height,
                DBUS_TYPE_INVALID);

    npp = (NPP_t*)nsAlloc (sizeof (NPP_t));
    memset (npp, 0, sizeof (NPP_t));
    np_err = np_funcs.newp (mime, npp, NP_EMBED, argc, argn, argv, saved_data);
    if (np_err != NPERR_NO_ERROR) {
        print ("NPP_New failure %d %p %p\n", np_err, np_funcs, np_funcs.newp);
        return -1;
    }
    if (np_funcs.getvalue) {
        char *pname;
        void *iid;
        np_err = np_funcs.getvalue ((void*)npp,
                NPPVpluginNameString, (void*)&pname);
        if (np_err == NPERR_NO_ERROR)
            print ("plugin name %s\n", pname);
        np_err = np_funcs.getvalue ((void*)npp,
                NPPVpluginNeedsXEmbed, (void*)&needs_xembed);
        if (np_err != NPERR_NO_ERROR || !needs_xembed) {
            print ("NPP_GetValue NPPVpluginNeedsXEmbed failure %d\n", np_err);
            shutDownPlugin();
            return -1;
        }
        np_err = np_funcs.getvalue ((void*)npp,
                NPPVpluginScriptableIID, (void*)&iid);
    }
    memset (&np_window, 0, sizeof (NPWindow));
    display = gdk_x11_get_default_xdisplay ();
    np_window.x = 0;
    np_window.y = 0;
    np_window.width = INITIAL_WINDOW_WIDTH;
    np_window.height = 1200;
    np_window.window = (void*)socket_id;
    np_window.type = NPWindowTypeWindow;
    ws_info.type = NP_SETWINDOW;
    screen = DefaultScreen (display);
    ws_info.display = (void*)(long)display;
    ws_info.visual = (void*)(long)DefaultVisual (display, screen);
    ws_info.colormap = DefaultColormap (display, screen);
    ws_info.depth = DefaultDepth (display, screen);
    print ("display %u %dx%d\n", socket_id, width, height);
    np_window.ws_info = (void*)&ws_info;

    GtkAllocation allocation;
    allocation.x = 0;
    allocation.y = 0;
    allocation.width = np_window.width;
    allocation.height = np_window.height;
    gtk_widget_size_allocate (xembed, &allocation);

    np_err = np_funcs.setwindow (npp, &np_window);

    return 0;
}

static gpointer startPlugin (const char *url, const char *mime,
        int argc, char *argn[], char *argv[]) {
    StreamInfo *si;
    if (!npp && (initPlugin (plugin) || newPlugin (mimetype, argc, argn, argv)))
        return 0L;
    si = addStream (url, mime, 0L, 0L, false);
    return si;
}

/*----------------%<---------------------------------------------------------*/

static StreamInfo *getStreamInfo (const char *path, gpointer *stream_id) {
    const char *p = strrchr (path, '_');
    *stream_id = p ? (gpointer) strtol (p+1, NULL, 10) : NULL;
    return (StreamInfo *) g_tree_lookup (stream_list, *stream_id);
}

static void defaultReply (DBusConnection *conn, DBusMessage *msg) {
    if (!dbus_message_get_no_reply (msg)) {
        DBusMessage *rmsg = dbus_message_new_method_return (msg);
        dbus_connection_send (conn, rmsg, NULL);
        dbus_connection_flush (conn);
        dbus_message_unref (rmsg);
    }
}

static bool dbusMsgIterGet (DBusMessage *msg, DBusMessageIter *it,
        int arg_type, void *p, bool first) {
    if ((first && dbus_message_iter_init (msg, it)) ||
            (!first && dbus_message_iter_has_next (it) &&
             dbus_message_iter_next (it)) &&
            dbus_message_iter_get_arg_type (it) == arg_type) {
        dbus_message_iter_get_basic (it, p);
        return true;
    }
    return false;
}

static DBusHandlerResult dbusStreamMessage (DBusConnection *conn,
        DBusMessage *msg, void *user_data) {
    DBusMessageIter args;
    const char *iface = "org.kde.kmplayer.backend";
    gpointer stream_id;
    StreamInfo *si;

    print ("dbusStreamMessage %s %s %s\n", dbus_message_get_interface (msg),
            dbus_message_get_member (msg), dbus_message_get_signature (msg));
    if (dbus_message_is_method_call (msg, iface, "redirected")) {
        char *url = 0;
        si = getStreamInfo(dbus_message_get_path (msg), &stream_id);
        if (dbusMsgIterGet (msg, &args, DBUS_TYPE_STRING, &url, true)) {
            if (si) {
                dbus_message_iter_get_basic (&args, &url);
                nsMemFree (si->url);
                si->url = g_strdup (url);
                si->np_stream.url = si->url;
                print ("redirect %d (had data %d) to %s\n", (long)stream_id, si->called_plugin, url);
            } else {
                print ("redirect %d not found\n", (long)stream_id);
            }
        }
        defaultReply (conn, msg);
    } else if (dbus_message_is_method_call (msg, iface, "eof")) {
        unsigned int total;
        if (dbusMsgIterGet (msg, &args, DBUS_TYPE_UINT32, &total, true)) {
            unsigned int reason;
            if (dbusMsgIterGet (msg, &args, DBUS_TYPE_UINT32, &reason, false)) {
                si = getStreamInfo(dbus_message_get_path (msg), &stream_id);
                if (si) {
                    si->total = total;
                    si->reason = reason;
                    print ("eof %d bytes:%d reason:%d\n", (long)stream_id, si->total, si->reason);
                    if (si->stream_pos == si->total || si->destroyed)
                        removeStream (stream_id);
                } else {
                    print ("stream %d not found\n", stream_id);
                }
            }
        }
        defaultReply (conn, msg);
    } else if (dbus_message_is_method_call (msg, iface, "streamInfo")) {
        const char *mime;
        if (dbusMsgIterGet (msg, &args, DBUS_TYPE_STRING, &mime, true)) {
            uint32_t length;
            si = getStreamInfo(dbus_message_get_path (msg), &stream_id);
            if (si && *mime) {
                if (si->mimetype)
                    g_free (si->mimetype);
                si->mimetype = g_strdup (mime);
            }
            if (dbusMsgIterGet (msg, &args, DBUS_TYPE_UINT32, &length, false)) {
                if (si)
                    si->np_stream.end = length;
            }
            print ("streamInfo %d size:%d mime:%s\n", (long)stream_id, length,
                    mime ? mime : "");
        }
        defaultReply (conn, msg);
    }
    return DBUS_HANDLER_RESULT_HANDLED;
}

static void dbusStreamUnregister (DBusConnection *conn, void *user_data) {
    print( "dbusStreamUnregister\n");
}

static void dbusPluginUnregister (DBusConnection *conn, void *user_data) {
    print( "dbusPluginUnregister\n");
}

static const char *plugin_inspect =
    "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\""
    " \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">"
    "<node>"
    " <interface name=\"org.freedesktop.DBus.Introspectable\">"
    "  <method name=\"Introspect\">"
    "   <arg name=\"xml_data\" type=\"s\" direction=\"out\"/>"
    "  </method>"
    " </interface>"
    "  <interface name=\"org.kde.kmplayer.backend\">"
    "   <method name=\"play\">"
    "    <arg name=\"url\" type=\"s\" direction=\"in\"/>"
    "    <arg name=\"mimetype\" type=\"s\" direction=\"in\"/>"
    "    <arg name=\"plugin\" type=\"s\" direction=\"in\"/>"
    "    <arg name=\"arguments\" type=\"a{sv}\" direction=\"in\"/>"
    "   </method>"
    "  </interface>"
    "</node>";

static DBusHandlerResult dbusPluginMessage (DBusConnection *conn,
        DBusMessage *msg, void *user_data) {

    DBusMessageIter args;
    const char *iface = "org.kde.kmplayer.backend";
    (void) user_data;

    print ("dbusPluginMessage %s %s %s\n", dbus_message_get_interface (msg),
            dbus_message_get_member (msg), dbus_message_get_signature (msg));
    if (dbus_message_is_method_call (msg,
                "org.freedesktop.DBus.Introspectable", "Introspect")) {
        DBusMessage * rmsg = dbus_message_new_method_return (msg);
        dbus_message_append_args (rmsg,
                DBUS_TYPE_STRING, &plugin_inspect, DBUS_TYPE_INVALID);
        dbus_connection_send (conn, rmsg, NULL);
        dbus_connection_flush (conn);
        dbus_message_unref (rmsg);
    } else if (dbus_message_is_method_call (msg, iface, "play")) {
        DBusMessageIter ait;
        char *param = 0;
        unsigned int params = 0;
        char **argn = NULL;
        char **argv = NULL;
        GSList *arglst = NULL;
        if (!dbusMsgIterGet (msg, &args, DBUS_TYPE_STRING, &param, true)) {
            g_printerr ("missing url arg");
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        object_url = g_strdup (param);
        if (!dbusMsgIterGet (msg, &args, DBUS_TYPE_STRING, &param, false)) {
            g_printerr ("missing mimetype arg");
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        mimetype = g_strdup (param);
        if (!dbusMsgIterGet (msg, &args, DBUS_TYPE_STRING, &param, false)) {
            g_printerr ("missing plugin arg");
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        plugin = g_strdup (param);
        if (!dbus_message_iter_next (&args) ||
                DBUS_TYPE_ARRAY != dbus_message_iter_get_arg_type (&args)) {
            g_printerr ("missing params array");
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        dbus_message_iter_recurse (&args, &ait);
        do {
            char *key, *value;
            DBusMessageIter di;
            int arg_type;
            if (dbus_message_iter_get_arg_type (&ait) != DBUS_TYPE_DICT_ENTRY)
                break;
            dbus_message_iter_recurse (&ait, &di);
            if (DBUS_TYPE_STRING != dbus_message_iter_get_arg_type (&di))
                break;
            dbus_message_iter_get_basic (&di, &key);
            if (!dbus_message_iter_next (&di))
                break;
            arg_type = dbus_message_iter_get_arg_type (&di);
            if (DBUS_TYPE_STRING == arg_type) {
                dbus_message_iter_get_basic (&di, &value);
            } else if (DBUS_TYPE_VARIANT == arg_type) {
                DBusMessageIter vi;
                dbus_message_iter_recurse (&di, &vi);
                if (DBUS_TYPE_STRING != dbus_message_iter_get_arg_type (&vi))
                    break;
                dbus_message_iter_get_basic (&vi, &value);
            } else {
                break;
            }
            arglst = g_slist_append (arglst, g_strdup (key));
            arglst = g_slist_append (arglst, g_strdup (value));
            params++;
            print ("param %d:%s='%s'\n", params, key, value);
        } while (dbus_message_iter_has_next (&ait) &&
                dbus_message_iter_next (&ait));
        if (params > 0 && params < 100) {
            int i;
            GSList *sl = arglst;
            argn = (char**) nsAlloc (params * sizeof (char *));
            argv = (char**) nsAlloc (params * sizeof (char *));
            for (i = 0; sl; i++) {
                argn[i] = (gchar *)sl->data;
                sl = sl->next;
                argv[i] = (gchar *)sl->data;
                sl = sl->next;
            }
            g_slist_free (arglst);
        }
        print ("play %s %s %s params:%d\n", object_url,
                mimetype ? mimetype : "", plugin, params);
        startPlugin (object_url, mimetype, params, argn, argv);
        defaultReply (conn, msg);
    } else if (dbus_message_is_method_call (msg, iface, "get")) {
        DBusMessage * rmsg;
        uint32_t object;
        char *prop;
        if (dbusMsgIterGet (msg, &args, DBUS_TYPE_UINT32, &object, true) &&
                dbusMsgIterGet (msg, &args, DBUS_TYPE_STRING, &prop, false)) {
            char *result = NULL;
            doGet (object, prop, &result);
            print ("get %s => %s\n", prop, result ? result : "NULL");
            rmsg = dbus_message_new_method_return (msg);
            dbus_message_append_args (rmsg,
                    DBUS_TYPE_STRING, &result, DBUS_TYPE_INVALID);
            dbus_connection_send (conn, rmsg, NULL);
            dbus_connection_flush (conn);
            dbus_message_unref (rmsg);
            g_free (result);
        }
    } else if (dbus_message_is_method_call (msg, iface, "call")) {
        DBusMessage * rmsg;
        DBusMessageIter ait;
        uint32_t object;
        char *func;
        GSList *arglst = NULL;
        GSList *sl;
        uint32_t arg_count = 0;
        char *result = NULL;
        if (dbusMsgIterGet (msg, &args, DBUS_TYPE_UINT32, &object, true) &&
                dbusMsgIterGet (msg, &args, DBUS_TYPE_STRING, &func, false)) {
            if (!dbus_message_iter_next (&args) ||
                    DBUS_TYPE_ARRAY != dbus_message_iter_get_arg_type (&args)) {
                g_printerr ("missing arguments array");
                return DBUS_HANDLER_RESULT_HANDLED;
            }
            dbus_message_iter_recurse (&args, &ait);
            print ("call %d:%s(", object, func);
            do {
                char *arg;
                if (dbus_message_iter_get_arg_type (&ait) != DBUS_TYPE_STRING)
                    break;
                dbus_message_iter_get_basic (&ait, &arg);
                print ("%s, ", arg);
                arglst = g_slist_append (arglst, g_strdup (arg));
                arg_count++;
            } while (dbus_message_iter_has_next (&ait) &&
                    dbus_message_iter_next (&ait));
            doInvoke (object, func, arglst, arg_count, &result);
            print (") %s\n", result ? result : "NULL");
            rmsg = dbus_message_new_method_return (msg);
            dbus_message_append_args (rmsg,
                    DBUS_TYPE_STRING, &result, DBUS_TYPE_INVALID);
            dbus_connection_send (conn, rmsg, NULL);
            dbus_connection_flush (conn);
            dbus_message_unref (rmsg);
            g_free (result);
            if (arglst) {
                for (sl = arglst; sl; sl = sl->next)
                    g_free ((char *)sl->data);
                g_slist_free (arglst);
            }
        }
    } else if (dbus_message_is_method_call (msg, iface, "quit")) {
        print ("quit\n");
        shutDownPlugin();
        defaultReply (conn, msg);
        gtk_main_quit();
    }
    return DBUS_HANDLER_RESULT_HANDLED;
}

static void callFunction(int stream,const char *func, int first_arg_type, ...) {
    char path[64];
    createPath (stream, path, sizeof (path));
    print ("call %s.%s()\n", path, func);
    if (callback_service) {
        va_list var_args;
        DBusMessage *msg = dbus_message_new_method_call (
                callback_service,
                path,
                "org.kde.kmplayer.callback",
                func);
        if (first_arg_type != DBUS_TYPE_INVALID) {
            va_start (var_args, first_arg_type);
            dbus_message_append_args_valist (msg, first_arg_type, var_args);
            va_end (var_args);
        }
        dbus_message_set_no_reply (msg, TRUE);
        dbus_connection_send (dbus_connection, msg, NULL);
        dbus_message_unref (msg);
        dbus_connection_flush (dbus_connection);
    }
}

static char *evaluate (const char *script, bool store) {
    char * ret = NULL;
    print ("evaluate %s", script);
    if (callback_service) {
        DBusMessage *rmsg;
        DBusMessage *msg = dbus_message_new_method_call (
                callback_service,
                callback_path,
                "org.kde.kmplayer.callback",
                "evaluate");
        int bool_val = store;
        dbus_message_append_args (msg,
                DBUS_TYPE_STRING, &script,
                DBUS_TYPE_BOOLEAN, &bool_val,
                DBUS_TYPE_INVALID);
        rmsg = dbus_connection_send_with_reply_and_block (dbus_connection,
                msg, 2000, NULL);
        if (rmsg) {
            DBusMessageIter it;
            if (dbus_message_iter_init (rmsg, &it) &&
                    DBUS_TYPE_STRING == dbus_message_iter_get_arg_type (&it)) {
                char * param;
                dbus_message_iter_get_basic (&it, &param);
                ret = g_strdup (param);
            }
            dbus_message_unref (rmsg);
            print ("  => %s\n", ret);
        }
        dbus_message_unref (msg);
    } else {
        print ("  => NA\n");
    }
    return ret;
}

/*----------------%<---------------------------------------------------------*/

static void pluginAdded (GtkSocket *socket, gpointer d) {
    /*(void)socket;*/ (void)d;
    print ("pluginAdded\n");
    if (socket->plug_window) {
        gpointer user_data = NULL;
        gdk_window_get_user_data (socket->plug_window, &user_data);
        if (!user_data) {
            /**
             * GtkSocket resets plugins XSelectInput in
             * _gtk_socket_add_window
             *   _gtk_socket_windowing_select_plug_window_input
             **/
            XSelectInput (gdk_x11_get_default_xdisplay (),
                    gdk_x11_drawable_get_xid (socket->plug_window),
                    KeyPressMask | KeyReleaseMask |
                    ButtonPressMask | ButtonReleaseMask |
                    KeymapStateMask |
                    ButtonMotionMask |
                    PointerMotionMask |
                    /*EnterWindowMask | LeaveWindowMask |*/
                    FocusChangeMask |
                    ExposureMask |
                    StructureNotifyMask | SubstructureNotifyMask |
                    /*SubstructureRedirectMask |*/
                    PropertyChangeMask
                    );
        }
    }
    callFunction (-1, "plugged", DBUS_TYPE_INVALID);
}

static void windowCreatedEvent (GtkWidget *w, gpointer d) {
    (void)d;
    print ("windowCreatedEvent\n");
    socket_id = gtk_socket_get_id (GTK_SOCKET (xembed));
    if (parent_id) {
        print ("windowCreatedEvent %p\n", GTK_PLUG (w)->socket_window);
        if (!GTK_PLUG (w)->socket_window)
            gtk_plug_construct (GTK_PLUG (w), parent_id);
        /*gdk_window_reparent( w->window,
                GTK_PLUG (w)->socket_window
                    ? GTK_PLUG (w)->socket_window
                    : gdk_window_foreign_new (parent_id),
                0, 0);*/
        gtk_widget_show_all (w);
        /*XReparentWindow (gdk_x11_drawable_get_xdisplay (w->window),
                gdk_x11_drawable_get_xid (w->window),
                parent_id,
                0, 0);*/
    }
    if (!callback_service) {
        char *argn[] = { "WIDTH", "HEIGHT", "debug", "SRC" };
        char *argv[] = { "440", "330", g_strdup("yes"), g_strdup(object_url) };
        startPlugin (object_url, mimetype, 4, argn, argv);
    }
}

static void embeddedEvent (GtkPlug *plug, gpointer d) {
    (void)plug; (void)d;
    print ("embeddedEvent\n");
}

static gboolean updateDimension (void * p) {
    (void)p;
    if (np_window.window) {
        if (np_window.width != top_w || np_window.height != top_h) {
            np_window.width = top_w;
            np_window.height = top_h;
            np_funcs.setwindow (npp, &np_window);
        }
        update_dimension_timer = 0;
        return 0; /* single shot */
    } else {
        return 1;
    }
}

static gboolean configureEvent(GtkWidget *w, GdkEventConfigure *e, gpointer d) {
    static int first_configure_pre_size;
    (void)w; (void)d;
    print("configureEvent %dx%d\n", e->width, e->height);
    if (!first_configure_pre_size && e->width == INITIAL_WINDOW_WIDTH) {
        first_configure_pre_size = 1;
        return FALSE;
    }
    if (e->width != top_w || e->height != top_h) {
        top_w = e->width;
        top_h = e->height;
        if (!update_dimension_timer)
            update_dimension_timer = g_timeout_add (100, updateDimension, NULL);
    }
    return FALSE;
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
    static DBusObjectPathVTable pluginVTable;
    GtkWidget *window;
    GdkColormap *color_map;
    GdkColor bg_color;
    (void)p;

    window = callback_service
        ? gtk_plug_new (parent_id)
        : gtk_window_new (GTK_WINDOW_TOPLEVEL);
    g_signal_connect (G_OBJECT (window), "delete_event",
            G_CALLBACK (windowCloseEvent), NULL);
    g_signal_connect (G_OBJECT (window), "destroy",
            G_CALLBACK (windowDestroyEvent), NULL);
    g_signal_connect_after (G_OBJECT (window), "realize",
            GTK_SIGNAL_FUNC (windowCreatedEvent), NULL);
    g_signal_connect (G_OBJECT (window), "configure-event",
            GTK_SIGNAL_FUNC (configureEvent), NULL);

    xembed = gtk_socket_new();
    g_signal_connect (G_OBJECT (xembed), "plug-added",
            GTK_SIGNAL_FUNC (pluginAdded), NULL);

    color_map = gdk_colormap_get_system();
    gdk_colormap_query_color (color_map, 0, &bg_color);
    gtk_widget_modify_bg (xembed, GTK_STATE_NORMAL, &bg_color);

    gtk_container_add (GTK_CONTAINER (window), xembed);

    if (!parent_id) {
        gtk_widget_set_size_request (window, 440, 330);
        gtk_widget_show_all (window);
    } else {
        g_signal_connect (G_OBJECT (window), "embedded",
                GTK_SIGNAL_FUNC (embeddedEvent), NULL);
        gtk_widget_set_size_request (window, INITIAL_WINDOW_WIDTH, 1200);
        gtk_widget_realize (window);
    }

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
        print ("using service %s was '%s'\n", service_name, dbus_bus_get_unique_name (dbus_connection));
        dbus_connection_setup_with_g_main (dbus_connection, 0L);
        dbus_bus_request_name (dbus_connection, service_name,
                DBUS_NAME_FLAG_REPLACE_EXISTING, &dberr);
        if (dbus_error_is_set (&dberr)) {
            g_printerr ("Failed to register name: %s\n", dberr.message);
            dbus_connection_unref (dbus_connection);
            return -1;
        }

        pluginVTable.unregister_function = dbusPluginUnregister;
        pluginVTable.message_function = dbusPluginMessage;
        if (!dbus_connection_register_object_path (dbus_connection, "/plugin",
                    &pluginVTable, NULL))
            g_printerr ("dbus_connection_register_object_path error\n");


        /* TODO: remove DBUS_BUS_SESSION and create a private connection */
        callFunction (-1, "running",
                DBUS_TYPE_STRING, &service_name, DBUS_TYPE_INVALID);

        dbus_connection_flush (dbus_connection);
    }
    return 0; /* single shot */
}

int main (int argc, char **argv) {
    int i;

    g_thread_init (NULL);
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

    identifiers = g_tree_new (strcmp);
    stream_list = g_tree_new (streamCompare);
    stream_vtable.unregister_function = dbusStreamUnregister;
    stream_vtable.message_function = dbusStreamMessage;

    g_timeout_add (0, initPlayer, NULL);

    fcntl (0, F_SETFL, fcntl (0, F_GETFL) | O_NONBLOCK);

    print ("entering gtk_main\n");

    gtk_main();

    if (dbus_connection)
        dbus_connection_unref (dbus_connection);

    return 0;
}
