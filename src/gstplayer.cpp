/* This file is part of the KMPlayer application
   Copyright (C) 2004 Koos Vriezen <koos.vriezen@xs4all.nl>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "config-kmplayer.h"
#include <dcopclient.h>
#include <qcstring.h>
#include <qtimer.h>
#include <qfile.h>
#include <qurl.h>
#include <qthread.h>
#include <qmutex.h>
#include <qdom.h>
#include "kmplayer_backend.h"
#include "kmplayer_callback_stub.h"
#include "kmplayer_callback.h"
#include "gstplayer.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <gst/gst.h>
#include <gst/interfaces/xoverlay.h>
#include <gst/interfaces/colorbalance.h>

static char                 configfile[2048];

static Display             *display;
static KGStreamerPlayer   *gstapp;
static KMPlayer::Callback_stub * callback;
static Window               wid;
static QMutex               mutex (true);
static bool                 window_created = true;
static bool                 wants_config;
static bool                 verbose;
static bool                 notified_playing;
static int                  running;
static int                  movie_width;
static int                  movie_height;
static int                  movie_length;
static int                  repeat_count;
static int                  screen;
static const int            event_finished = QEvent::User;
static const int            event_playing = QEvent::User + 1;
static const int            event_size = QEvent::User + 2;
static const int            event_eos = QEvent::User + 3;
static const int            event_progress = QEvent::User + 4;
static const int            event_error = QEvent::User + 5;
static const int            event_video = QEvent::User + 6;
static QString              mrl;
static QString              sub_mrl;
static const char          *ao_driver;
static const char          *vo_driver;
static const char          *playbin_name = "player";
static const char          *dvd_device;
static const char          *vcd_device;
static GstElement          *gst_elm_play;
static GstBus              *gst_bus;
static unsigned int /*GstMessageType*/       ignore_messages_mask;
static GstXOverlay         *xoverlay;
static GstColorBalance     *color_balance;
static gulong               gst_bus_sync;
static gulong               gst_bus_async;
static QString elmentry ("entry");
static QString elmitem ("item");
static QString attname ("NAME");
static QString atttype ("TYPE");
static QString attdefault ("DEFAULT");
static QString attvalue ("VALUE");
static QString attstart ("START");
static QString attend ("END");
static QString valrange ("range");
static QString valnum ("num");
static QString valbool ("bool");
static QString valenum ("enum");
static QString valstring ("string");

extern "C" {
  // nothing yet
} // extern "C"


static bool gstPollForStateChange (GstElement *, GstState, gint64=GST_SECOND/2);

static void
cb_error (GstElement * play,
          GstElement * /*src*/,
          GError     *err,
          const char *debug,
          gpointer    /*data*/)
{
    fprintf (stderr, "cb_error: %s %s\n", err->message, debug);
    if (GST_STATE (play) == GST_STATE_PLAYING)
        gst_element_set_state (play, GST_STATE_READY);
    QApplication::postEvent (gstapp, new QEvent ((QEvent::Type)event_finished));
}

// NULL -> READY -> PAUSED -> PLAYING

static void
gstCapsSet (GstPad *pad,
            GParamSpec * /*pspec*/,
            gpointer /*data*/)
{
    GstCaps *caps = gst_pad_get_negotiated_caps (pad);
    if (!caps)
        return;
    QApplication::postEvent (gstapp, new QEvent ((QEvent::Type) event_video));
    const GstStructure * s = gst_caps_get_structure (caps, 0);
    if (s) {
        const GValue *par;

        gst_structure_get_int (s, "width", &movie_width);
        gst_structure_get_int (s, "height", &movie_height);
        if ((par = gst_structure_get_value (s, "pixel-aspect-ratio"))) {
            int num = gst_value_get_fraction_numerator (par),
                den = gst_value_get_fraction_denominator (par);

            if (num > den)
                movie_width = (int) ((float) num * movie_width / den);
            else
                movie_height = (int) ((float) den * movie_height / num);
        }
        QApplication::postEvent (gstapp, new GstSizeEvent (movie_length, movie_width, movie_height));
    }
    gst_caps_unref (caps);
}

static void gstStreamInfo (GObject *, GParamSpec *, gpointer /*data*/) {
    GstPad *videopad = 0L;
    GList *streaminfo = 0L;

    fprintf (stderr, "gstStreamInfo\n");
    g_object_get (gst_elm_play, "stream-info", &streaminfo, NULL);
    streaminfo = g_list_copy (streaminfo);
    g_list_foreach (streaminfo, (GFunc) g_object_ref, NULL);
    for ( ; streaminfo != NULL; streaminfo = streaminfo->next) {
        GObject *info = G_OBJECT (streaminfo->data);
        gint type;
        GParamSpec *pspec;
        GEnumValue *val;

        if (!info)
            continue;
        g_object_get (info, "type", &type, NULL);
        pspec = g_object_class_find_property (G_OBJECT_GET_CLASS(info), "type");
        val = g_enum_get_value (G_PARAM_SPEC_ENUM (pspec)->enum_class, type);

        if (!g_strcasecmp (val->value_nick, "video"))
            if (!videopad) {
                g_object_get (info, "object", &videopad, NULL);
                gstCapsSet (GST_PAD (videopad), 0L, 0L);
                g_signal_connect (videopad, "notify::caps", G_CALLBACK (gstCapsSet), 0L);
            }
    }

    GstMessage * msg = gst_message_new_application (GST_OBJECT (gst_elm_play),
            gst_structure_new ("notify-streaminfo", NULL));
    gst_element_post_message (gst_elm_play, msg);
    g_list_foreach (streaminfo, (GFunc) g_object_unref, NULL);
    g_list_free (streaminfo);
}

static void gstSource (GObject *, GParamSpec *, gpointer /*data*/) {
    GObject *source = 0L;
    fprintf (stderr, "gstSource\n");
    g_object_get (gst_elm_play, "source", &source, NULL);
    if (!source)
        return;
    GObjectClass *klass = G_OBJECT_GET_CLASS (source);
    if (mrl.startsWith ("dvd://") && dvd_device) {
        if (g_object_class_find_property (klass, "device"))
            g_object_set (source, "device", dvd_device, NULL);
    } else if (mrl.startsWith ("vcd://") && vcd_device) {
        if (g_object_class_find_property (klass, "device"))
            g_object_set (source, "device", vcd_device, NULL);
    }
    g_object_unref (source);
}

static void gstGetDuration () {
    GstFormat fmt = GST_FORMAT_TIME;
    gint64 len = -1; // usec
    if (gst_element_query_duration (gst_elm_play, &fmt, &len))
        if (movie_length != len / (GST_MSECOND * 100)) {
            movie_length = len / (GST_MSECOND * 100);
            fprintf (stderr, "new length %d\n", movie_length);
            QApplication::postEvent (gstapp, new GstSizeEvent (movie_length, movie_width, movie_height));
        }
}

static void gstTag (const GstTagList *, const gchar *tag, gpointer) {
    fprintf (stderr, "Tag: %s\n", tag);
}

//static bool gstStructure (GQuark field_id, const GValue *value, gpointer user_data);

static void gstBusMessage (GstBus *, GstMessage * message, gpointer) {
    GstMessageType msg_type = GST_MESSAGE_TYPE (message);
    /* somebody else is handling the message, probably in gstPolForStateChange*/
    if (ignore_messages_mask & msg_type)
        return;
    switch (msg_type) {
        case GST_MESSAGE_ERROR:
            fprintf (stderr, "error msg\n");
            QApplication::postEvent (gstapp, new QEvent ((QEvent::Type) event_error));
            if (gst_elm_play) {
                gst_element_set_state (gst_elm_play, GST_STATE_NULL);
                //gstPollForStateChange (gst_elm_play, GST_STATE_NULL);
            }
            break;
        case GST_MESSAGE_WARNING:
            fprintf (stderr, "warning msg\n");
            break;
        case GST_MESSAGE_TAG: {
            GstTagList *tag_list;
            //fprintf (stderr, "tag msg\n");
            gst_message_parse_tag (message, &tag_list);
            gst_tag_list_foreach (tag_list, gstTag, 0L);
            gst_tag_list_free (tag_list);
            break;
        }
        case GST_MESSAGE_EOS:
            fprintf (stderr, "eos msg\n");
            gst_element_set_state (gst_elm_play, GST_STATE_READY);
            break;
        case GST_MESSAGE_BUFFERING: {
            gint percent = 0;
            gst_structure_get_int (message->structure, "buffer-percent", &percent);
            QApplication::postEvent (gstapp, new GstProgressEvent (percent));
            //fprintf (stderr, "Buffering message (%u%%)\n", percent);
            break;
        }
        case GST_MESSAGE_APPLICATION: {
            const char * msg = gst_structure_get_name (message->structure);
            fprintf (stderr, "app msg %s\n", msg ? msg : "<unknown>");
            //gst_structure_foreach (message->structure, gstStructure, 0L);
            break;
        }
        case GST_MESSAGE_STATE_CHANGED: {
            GstState old_state, new_state;
            //gchar *src_name = gst_object_get_name (message->src);
            gst_message_parse_state_changed(message, &old_state, &new_state,0L);
            //fprintf (stderr, "%s changed state from %s to %s\n", src_name, gst_element_state_get_name (old_state), gst_element_state_get_name (new_state));
            if (GST_IS_ELEMENT (message->src) &&
                    GST_ELEMENT (message->src) == gst_elm_play) {
                if (old_state == GST_STATE_PAUSED &&
                        new_state >= GST_STATE_PLAYING) {
                    gstGetDuration ();
                    QApplication::postEvent (gstapp, new QEvent ((QEvent::Type) event_playing));
                } else if (old_state >= GST_STATE_PAUSED &&
                        new_state <= GST_STATE_READY) {
                    if (repeat_count-- > 0 &&
                      (gst_element_set_state(gst_elm_play, GST_STATE_PAUSED),
                       gstPollForStateChange(gst_elm_play, GST_STATE_PAUSED)))
                         gst_element_set_state(gst_elm_play, GST_STATE_PLAYING);
                    else
                        QApplication::postEvent (gstapp,
                                new QEvent ((QEvent::Type) event_finished));
                }
            }
            //g_free (src_name);
            break;
        }
        case GST_MESSAGE_DURATION:
            gstGetDuration ();
            break;
        case GST_MESSAGE_CLOCK_PROVIDE:
        case GST_MESSAGE_CLOCK_LOST:
        case GST_MESSAGE_NEW_CLOCK:
        case GST_MESSAGE_STATE_DIRTY:
             break;
        default:
             fprintf (stderr, "Unhandled msg %s (0x%x)\n",
                     gst_message_type_get_name (msg_type), msg_type);
             break;
    }
}

static void gstMessageElement (GstBus *, GstMessage *msg, gpointer /*data*/) {
    if (gst_structure_has_name (msg->structure, "prepare-xwindow-id")) {
        fprintf (stderr, "prepare-xwindow-id\n");
        if (xoverlay)
            gst_x_overlay_set_xwindow_id (xoverlay, wid);
    }
}

static bool gstPollForStateChange (GstElement *element, GstState state, gint64 timeout) {
    /*GstMessageType*/ unsigned int events, saved_events;
    GstBus *bus = gst_element_get_bus (element);
    GError **error = 0L;

    events = GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS;
    saved_events = ignore_messages_mask;

    if (element && element == gst_elm_play) {
        /* we do want the main handler to process state changed messages for
         * playbin as well, otherwise it won't hook up the timeout etc. */
        ignore_messages_mask |= (events ^ GST_MESSAGE_STATE_CHANGED);
    } else {
        ignore_messages_mask |= events;
    }

    while (true) {
        GstMessage *message;
        GstElement *src;

        message = gst_bus_poll (bus, (GstMessageType) events, timeout);
        if (!message)
            goto timed_out;

        src = (GstElement*)GST_MESSAGE_SRC (message);

        switch (GST_MESSAGE_TYPE (message)) {
            case GST_MESSAGE_STATE_CHANGED: {
                GstState olds, news, pending;
                if (src == element) {
                    gst_message_parse_state_changed (message, &olds, &news, &pending);
                    if (news == state) {
                        gst_message_unref (message);
                        goto success;
                    }
                }
                break;
            }
            case GST_MESSAGE_ERROR: {
                gchar *debug = NULL;
                GError *gsterror = NULL;
                gst_message_parse_error (message, &gsterror, &debug);
                fprintf (stderr, "Error: %s (%s)\n", gsterror->message, debug);
                gst_message_unref (message);
                g_error_free (gsterror);
                g_free (debug);
                goto error;
            }
            case GST_MESSAGE_EOS: {
                gst_message_unref (message);
                goto error;
            }
            default:
                g_assert_not_reached ();
                break;
        }
        gst_message_unref (message);
    }
    g_assert_not_reached ();

success:
    /* state change succeeded */
    fprintf (stderr, "state change to %s succeeded\n", gst_element_state_get_name (state));
    ignore_messages_mask = saved_events;
    return true;

timed_out:
    /* it's taking a long time to open -- just tell totem it was ok, this allows
     * the user to stop the loading process with the normal stop button */
    fprintf (stderr, "state change to %s timed out, returning success and handling errors asynchroneously\n", gst_element_state_get_name (state));
    ignore_messages_mask = saved_events;
    return true;

error:
    fprintf (stderr, "error while waiting for state change to %s: %s\n",
            gst_element_state_get_name (state),
            (error && *error) ? (*error)->message : "unknown");
    /* already set *error */
    ignore_messages_mask = saved_events;
    QApplication::postEvent (gstapp, new QEvent ((QEvent::Type) event_error));
    return false;
}

//-----------------------------------------------------------------------------

GstSizeEvent::GstSizeEvent (int l, int w, int h)
  : QEvent ((QEvent::Type) event_size),
    length (l), width (w), height (h) 
{}

GstProgressEvent::GstProgressEvent (const int p)
  : QEvent ((QEvent::Type) event_progress), progress (p) 
{}

//-----------------------------------------------------------------------------

using namespace KMPlayer;

Backend::Backend ()
    : DCOPObject (QCString ("Backend")) {
}

Backend::~Backend () {}

void Backend::setURL (QString url) {
    mrl = url;
}

void Backend::setSubTitleURL (QString url) {
    sub_mrl = url;
}

void Backend::play (int repeat) {
    gstapp->play (repeat);
}

void Backend::stop () {
    QTimer::singleShot (0, gstapp, SLOT (stop ()));
}

void Backend::pause () {
    gstapp->pause ();
}

void Backend::seek (int v, bool /*absolute*/) {
    gstapp->seek (v);
}

void Backend::hue (int h, bool) {
    gstapp->hue (h);
}

void Backend::saturation (int s, bool) {
    gstapp->saturation (s);
}

void Backend::contrast (int c, bool) {
    gstapp->contrast (c);
}

void Backend::brightness (int b, bool) {
    gstapp->brightness (b);
}

void Backend::volume (int v, bool) {
    gstapp->volume (v);
}

void Backend::frequency (int) {
}

void Backend::setAudioLang (int, QString) {
}

void Backend::setSubtitle (int, QString) {
}

void Backend::quit () {
    delete callback;
    callback = 0L;
    if (running)
        stop ();
    else
        QTimer::singleShot (0, qApp, SLOT (quit ()));
}

bool updateConfigEntry (const QString & name, const QString & value) {
    fprintf (stderr, "%s=%s\n", name.ascii (), (const char *) value.local8Bit ());
    return true;
}

void Backend::setConfig (QByteArray /*data*/) {
    /*QString err;
    int line, column;
    QDomDocument dom;
    if (dom.setContent (data, false, &err, &line, &column)) {
        if (dom.childNodes().length () == 1) {
            for (QDomNode node = dom.firstChild().firstChild();
                    !node.isNull ();
                    node = node.nextSibling ()) {
                QDomNamedNodeMap attr = node.attributes ();
                updateConfigEntry (attr.namedItem (attname).nodeValue (),
                                   attr.namedItem (attvalue).nodeValue ());
            }
        } else
            err = QString ("invalid data");
    }
    if (callback)
        callback->errorMessage (0, err);*/
}

bool Backend::isPlaying () {
    mutex.lock ();
    bool b = gst_elm_play && (GST_STATE (gst_elm_play) == GST_STATE_PLAYING);
    mutex.unlock ();
    return b;
}

KGStreamerPlayer::KGStreamerPlayer (int _argc, char ** _argv)
  : QApplication (_argc, _argv, false) {
}

void KGStreamerPlayer::init () {
    int xpos    = 0;
    int ypos    = 0;
    int width   = 320;
    int height  = 200;

    XLockDisplay(display);
    if (window_created)
        wid = XCreateSimpleWindow(display, XDefaultRootWindow(display),
                xpos, ypos, width, height, 1, 0, 0);
    fprintf (stderr, "init wid %u created:%d\n", wid, window_created);
    XSelectInput (display, wid,
                  (PointerMotionMask | ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask)); // | SubstructureNotifyMask));

    if (window_created) {
        //fprintf (stderr, "map %lu\n", wid);
        XMapRaised(display, wid);
        XSync(display, False);
    }
    XUnlockDisplay(display);
}

KGStreamerPlayer::~KGStreamerPlayer () {
    if (window_created) {
        XLockDisplay (display);
        fprintf (stderr, "unmap %lu\n", wid);
        XUnmapWindow (display,  wid);
        XDestroyWindow(display,  wid);
        XSync (display, False);
        XUnlockDisplay (display);
    }
    gstapp = 0L;
}

void getConfigEntries (QByteArray & buf) {
    QDomDocument doc;
    QDomElement root = doc.createElement (QString ("document"));
    doc.appendChild (root);
    QCString exp = doc.toCString ();
    buf = exp;
    buf.resize (exp.length ()); // strip terminating \0
}

void KGStreamerPlayer::play (int repeat) {
    GstElement *element;
    GstElement *videosink = 0L;
    GstElement *audiosink = 0L;
    bool success;
    fprintf (stderr, "play %s\n", mrl.isEmpty() ? "<empty>" : mrl.ascii ());
    if (gst_elm_play) {
        if (GST_STATE (gst_elm_play) == GST_STATE_PAUSED) {
            gst_element_set_state (gst_elm_play, GST_STATE_PLAYING);
            gstPollForStateChange (gst_elm_play, GST_STATE_PLAYING);
        }
        return;
    }
    notified_playing = false;
    if (mrl.isEmpty ())
        return;
    gchar *uri, *sub_uri = 0L;
    movie_length = movie_width = movie_height = 0;
    mutex.lock ();
    gst_elm_play = gst_element_factory_make ("playbin", playbin_name);
    if (!gst_elm_play) {
        fprintf (stderr, "couldn't create playbin\n");
        goto fail;
    }
    ignore_messages_mask = 0;
    gst_bus = gst_element_get_bus (gst_elm_play);

    gst_bus_add_signal_watch (gst_bus);

    gst_bus_async = g_signal_connect (gst_bus, "message",
                G_CALLBACK (gstBusMessage), 0L);
    if (ao_driver && !strncmp (ao_driver, "alsa", 4))
        audiosink = gst_element_factory_make ("alsasink", "audiosink");
    else if (ao_driver && !strncmp (ao_driver, "arts", 4))
        audiosink = gst_element_factory_make ("artsdsink", "audiosink");
    else if (ao_driver && !strncmp (ao_driver, "esd", 3))
        audiosink = gst_element_factory_make ("esdsink", "audiosink");
    else
        audiosink = gst_element_factory_make ("osssink", "audiosink");
    if (!audiosink)
        goto fail;
    if (vo_driver && !strncmp (vo_driver, "xv", 2))
        videosink = gst_element_factory_make ("xvimagesink", "videosink");
    else
        videosink = gst_element_factory_make ("ximagesink", "videosink");
    if (!videosink)
        goto fail;
    if (GST_IS_BIN (videosink))
        element = gst_bin_get_by_interface (GST_BIN (videosink),
                GST_TYPE_X_OVERLAY);
    else
        element = videosink;
    if (GST_IS_X_OVERLAY (element)) {
        xoverlay = GST_X_OVERLAY (element);
        gst_x_overlay_set_xwindow_id (xoverlay, wid);
    }
    gst_element_set_bus (videosink, gst_bus);
    gst_element_set_state (videosink, GST_STATE_READY);
    success = gstPollForStateChange (videosink, GST_STATE_READY);
    //if (!success) {
        /* Drop this video sink */
    //    gst_element_set_state (videosink, GST_STATE_NULL);
    //    gst_object_unref (videosink);
    if (audiosink) {
        gst_element_set_bus (audiosink, gst_bus);
        gst_element_set_state (audiosink, GST_STATE_READY);
        success = gstPollForStateChange (audiosink, GST_STATE_READY);
    }
    g_object_set (G_OBJECT (gst_elm_play),
            "video-sink",  videosink,
            "audio-sink",  audiosink,
            NULL);
    gst_bus_set_sync_handler (gst_bus, gst_bus_sync_signal_handler, 0L);
    gst_bus_sync = g_signal_connect (gst_bus, "sync-message::element",
            G_CALLBACK (gstMessageElement), 0L);
    g_signal_connect (gst_elm_play, "notify::source",
            G_CALLBACK (gstSource), 0L);
    g_signal_connect (gst_elm_play, "notify::stream-info",
            G_CALLBACK (gstStreamInfo), 0L);
    if (GST_IS_COLOR_BALANCE (videosink))
        color_balance = GST_COLOR_BALANCE (videosink);

    if (GST_STATE (gst_elm_play) > GST_STATE_READY)
        gst_element_set_state (gst_elm_play, GST_STATE_READY);

    if (mrl.startsWith (QChar ('/')))
        mrl = QString ("file://") + mrl;
    uri = g_strdup (mrl.local8Bit ());
    g_object_set (gst_elm_play, "uri", uri, NULL);
    if (!sub_mrl.isEmpty ()) {
        if (sub_mrl.startsWith (QChar ('/')))
            sub_mrl = QString ("file://") + sub_mrl;
        sub_uri = g_strdup (sub_mrl.local8Bit ());
        g_object_set (gst_elm_play, "suburi", sub_uri, NULL);
        g_free (sub_uri);
    }
    repeat_count = repeat;
    mutex.unlock ();
    gst_element_set_state (gst_elm_play, GST_STATE_PAUSED);
    if (gstPollForStateChange (gst_elm_play, GST_STATE_PAUSED)) {
        gst_element_set_state (gst_elm_play, GST_STATE_PLAYING);
        gstPollForStateChange (gst_elm_play, GST_STATE_PLAYING);
    }
    g_free (uri);
    QTimer::singleShot (500, this, SLOT (updatePosition ()));
    return;
fail:
    if (videosink) {
        gst_element_set_state (videosink, GST_STATE_NULL);
        gst_object_unref (videosink);
    }
    if (audiosink) {
        gst_element_set_state (audiosink, GST_STATE_NULL);
        gst_object_unref (audiosink);
    }
    mutex.unlock ();
    QApplication::postEvent (gstapp, new QEvent ((QEvent::Type)event_finished));
}

void KGStreamerPlayer::pause () {
    mutex.lock ();
    if (gst_elm_play) {
        GstState state = GST_STATE (gst_elm_play) == GST_STATE_PLAYING ?
            GST_STATE_PAUSED : GST_STATE_PLAYING;
        gst_element_set_state (gst_elm_play, state);
        gstPollForStateChange (gst_elm_play, state);
    }
    mutex.unlock ();
}

void KGStreamerPlayer::stop () {
    fprintf (stderr, "stop %s\n", mrl.isEmpty () ? "<empty>" : mrl.ascii ());
    mutex.lock ();
    repeat_count = 0;
    if (gst_elm_play) {
        GstState current_state;
        gst_element_get_state (gst_elm_play, &current_state, NULL, 0);
        if (current_state > GST_STATE_READY) {
            gst_element_set_state (gst_elm_play, GST_STATE_READY);
            mutex.unlock ();
            gstPollForStateChange (gst_elm_play, GST_STATE_READY, -1);
            mutex.lock ();
        }
        gst_element_set_state (gst_elm_play, GST_STATE_NULL);
        gst_element_get_state (gst_elm_play, NULL, NULL, -1);
    }
    mutex.unlock ();
    if (!gst_elm_play || (gst_elm_play && !notified_playing))
        QApplication::postEvent (gstapp, new QEvent ((QEvent::Type) event_finished));
}

void KGStreamerPlayer::finished () {
    QTimer::singleShot (10, this, SLOT (stop ()));
}

static void adjustColorSetting (const char * channel, int val) {
    //fprintf (stderr, "adjustColorSetting %s\n", channel);
    mutex.lock ();
    if (color_balance) {
        for (const GList *item =gst_color_balance_list_channels (color_balance);
                item != NULL; item = item->next) {
            GstColorBalanceChannel *chan = (GstColorBalanceChannel*) item->data;

            if (!strstr (chan->label, channel))
                gst_color_balance_set_value (color_balance, chan,
                        ((val + 100) * (chan->max_value - chan->min_value)/200 + chan->min_value));
        }
    }
    mutex.unlock ();
}

void KGStreamerPlayer::saturation (int s) {
    adjustColorSetting ("SATURATION", s);
}

void KGStreamerPlayer::hue (int h) {
    adjustColorSetting ("HUE", h);
}

void KGStreamerPlayer::contrast (int c) {
    adjustColorSetting ("CONTRAST", c);
}

void KGStreamerPlayer::brightness (int b) {
    adjustColorSetting ("BRIGHTNESS", b);
}

void KGStreamerPlayer::seek (int val /*offset_in_deciseconds*/) {
    //fprintf (stderr, "seek %d\n", val);
    mutex.lock ();
    if (gst_elm_play)
        gst_element_seek (gst_elm_play, 1.0, GST_FORMAT_TIME,
                (GstSeekFlags) (GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT),
                GST_SEEK_TYPE_SET, val * GST_MSECOND * 100,
                GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
    mutex.unlock ();
}

void KGStreamerPlayer::volume (int val) {
    //fprintf (stderr, "position %d\n", val);
    if (gst_elm_play)
        g_object_set (G_OBJECT (gst_elm_play), "volume", 1.0*val/100, 0L);
}

void KGStreamerPlayer::updatePosition () {
    if (gst_elm_play) {
        do {
            GstMessage * msg = gst_bus_poll (gst_bus, (GstMessageType) (GST_MESSAGE_STATE_CHANGED | GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_DURATION), GST_MSECOND * 10);
            if (!msg)
                break;
            gst_message_unref (msg);
        } while (gst_bus);

        mutex.lock ();
        if (gst_elm_play && callback) {
            GstFormat fmt = GST_FORMAT_TIME;
            gint64 val = 0; // usec
            if (gst_element_query_position (gst_elm_play, &fmt, &val))
                callback->moviePosition (int (val / (GST_MSECOND * 100)));
        }
        mutex.unlock ();
        QTimer::singleShot (500, this, SLOT (updatePosition ()));
    }
}

bool KGStreamerPlayer::event (QEvent * e) {
    switch (e->type()) {
        case event_finished: {
            fprintf (stderr, "event_finished\n");
            mutex.lock ();
            if (gst_elm_play) {
                gst_bus_set_flushing (gst_bus, true);
                if (gst_bus_sync)
                    g_signal_handler_disconnect (gst_bus, gst_bus_sync);
                if (gst_bus_async)
                    g_signal_handler_disconnect (gst_bus, gst_bus_async);
                gst_object_unref (gst_bus);
                gst_object_unref (GST_OBJECT (gst_elm_play));
                gst_bus = 0L;
                gst_elm_play = 0L;
                color_balance = 0L;
                gst_bus_sync = gst_bus_async = 0;
                xoverlay = 0L;
            }
            mutex.unlock ();
            if (callback)
                callback->finished ();
            else
                QTimer::singleShot (0, this, SLOT (quit ()));
            break;
        }
                //callback->movieParams (se->length/100, se->width, se->height, se->height ? 1.0*se->width/se->height : 1.0);
        case event_size: {
            GstSizeEvent * se = static_cast <GstSizeEvent *> (e);                
            fprintf (stderr, "movie parms: %d %d %d\n", se->length, se->width, se->height);
            if (callback) {
                if (se->length < 0) se->length = 0;
                callback->movieParams (se->length, se->width, se->height, se->height ? 1.0*se->width/se->height : 1.0, QStringList (), QStringList ());
            }
            if (window_created && movie_width > 0 && movie_height > 0) {
                XLockDisplay (display);
                XResizeWindow (display, wid, movie_width, movie_height);
                XFlush (display);
                XUnlockDisplay (display);
            }
            // fall through
        }
        case event_playing:
            notified_playing = true;
            if (callback)
                callback->playing ();
            break;
        case event_progress:
            if (callback)
                callback->loadingProgress
                    (static_cast <GstProgressEvent *> (e)->progress);
            break;
        case event_eos:
        case event_error:
            stop ();
            break;
        case event_video:
            if (callback)
                callback->statusMessage ((int) KMPlayer::Callback::stat_hasvideo, QString ());
            break;
        default:
            return false;
    }
    return true;
}
 
void KGStreamerPlayer::saveState (QSessionManager & sm) {
    if (callback)
        sm.setRestartHint (QSessionManager::RestartNever);
}

class XEventThread : public QThread {
protected:
    void run () {
        Time prev_click_time = 0;
        int prev_click_x = 0;
        int prev_click_y = 0;
        while (true) {
            XEvent   xevent;
            XNextEvent(display, &xevent);
            switch(xevent.type) {
                case ClientMessage:
                    if (xevent.xclient.format == 8 &&
                            !strncmp(xevent.xclient.data.b, "quit_now", 8)) {
                        fprintf(stderr, "request quit\n");
                        return;
                    }
                    break;
                case KeyPress: {
                    XKeyEvent  kevent;
                    KeySym     ksym;
                    char       kbuf[256];
                    int        len;
                    kevent = xevent.xkey;
                    XLockDisplay(display);
                    len = XLookupString(&kevent, kbuf, sizeof(kbuf), &ksym, NULL);
                    XUnlockDisplay(display);
                    fprintf(stderr, "keypressed 0x%x 0x%x\n", kevent.keycode, ksym);
                    switch (ksym) {
                        case XK_q:
                        case XK_Q:
                            gstapp->lock ();
                            gstapp->stop ();
                            gstapp->unlock ();
                            break;
                    }
                    break;
                }
                case Expose:
                    if (!xevent.xexpose.count && xevent.xexpose.window == wid) {
                        mutex.lock ();
                        if (gst_elm_play) {
                            GstElement *videosink;
                            g_object_get (gst_elm_play, "video-sink", &videosink, NULL);
                            if (videosink && GST_IS_X_OVERLAY (videosink)) {
                                gst_x_overlay_set_xwindow_id (GST_X_OVERLAY (videosink), wid);
                                gst_x_overlay_expose (GST_X_OVERLAY (videosink));
                                gst_object_unref (videosink);
                            }
                        }
                        mutex.unlock ();
                    }
                    break;

                case ConfigureNotify:
                    mutex.lock ();
                    if (xoverlay && GST_IS_X_OVERLAY (xoverlay))
                        gst_x_overlay_expose (xoverlay);
                    mutex.unlock ();
                    break;
                case ButtonPress: {
                    XButtonEvent *bev = (XButtonEvent *) &xevent;
                    int dx = prev_click_x - bev->x;
                    int dy = prev_click_y - bev->y;
                    if (bev->time - prev_click_time < 400 &&
                            (dx * dx + dy * dy) < 25) {
                        gstapp->lock ();
                        if (callback)
                            callback->toggleFullScreen ();
                        gstapp->unlock ();
                    }
                    prev_click_time = bev->time;
                    prev_click_x = bev->x;
                    prev_click_y = bev->y;
                    break;
                }
                default:
                    ; //if (xevent.type < LASTEvent)
                      //  fprintf (stderr, "event %d\n", xevent.type);
            }
        }
    }
};

int main(int argc, char **argv) {
    if (!XInitThreads ()) {
        fprintf (stderr, "XInitThreads () failed\n");
        return 1;
    }
    display = XOpenDisplay(NULL);
    screen  = XDefaultScreen(display);

    gst_init (NULL, NULL);

    gstapp = new KGStreamerPlayer (argc, argv);

    for(int i = 1; i < argc; i++) {
        if (!strcmp (argv [i], "-ao")) {
            ao_driver = argv [++i];
        } else if (!strcmp (argv [i], "-vo")) {
            vo_driver = argv [++i];
        } else if (!strcmp (argv [i], "-dvd-device") && ++i < argc) {
            dvd_device = argv [i];
        } else if (!strcmp (argv [i], "-vcd-device") && ++i < argc) {
            vcd_device = argv [i];
        } else if (!strcmp (argv [i], "-wid") || !strcmp (argv [i], "-window-id")) {
            wid = atol (argv [++i]);
            window_created = false;
        } else if (!strcmp (argv [i], "-root")) {
            wid =  XDefaultRootWindow (display);
            window_created = false;
        } else if (!strcmp (argv [i], "-window")) {
            ;
        } else if (!strcmp (argv [i], "-v")) {
            verbose = true;
        } else if (!strcmp (argv [i], "-c")) {
            wants_config = true;
        } else if (!strcmp (argv [i], "-f") && i < argc - 1) {
            strncpy (configfile, argv [++i], sizeof (configfile));
            configfile[sizeof (configfile) - 1] = 0;
        } else if (!strcmp (argv [i], "-loop") && i < argc - 1) {
            repeat_count = atol (argv [++i]);
        } else if (!strcmp (argv [i], "-cb")) {
            QString str = argv [++i];
            int pos = str.find ('/');
            if (pos > -1) {
                fprintf (stderr, "callback is %s %s\n", str.left (pos).ascii (), str.mid (pos + 1).ascii ());
                callback = new KMPlayer::Callback_stub 
                    (str.left (pos).ascii (), str.mid (pos + 1).ascii ());
            }
        } else if (!strncmp (argv [i], "-", 1)) {
            fprintf (stderr, "usage: %s [-vo (xv|xshm)] [-ao <audio driver>] [-f <config file>] [-dvd-device <device>] [-vcd-device <device>] [-v] [(-wid|-window-id) <window>] [(-root|-window)] [-cb <DCOP callback name> [-c]] [<url>]\n", argv[0]);
            delete gstapp;
            return 1;
        } else {
            mrl = QString::fromLocal8Bit (argv[i]);
        }
    }

    DCOPClient dcopclient;
    dcopclient.registerAs ("kgstreamerplayer");
    Backend * backend = new Backend;

    XEventThread * eventThread = new XEventThread;
    eventThread->start ();

    gstapp->init ();

    if (callback) {
        QByteArray buf;
        if (wants_config)
            getConfigEntries (buf);
        callback->started (dcopclient.appId (), buf);
    } else
        QTimer::singleShot (10, gstapp, SLOT (play (int)));

    gstapp->exec ();

    XLockDisplay(display);
    XClientMessageEvent ev = {
        ClientMessage, 0, true, display, wid, 
        XInternAtom (display, "XVIDEO", false), 8, {b: "quit_now"}
    };
    XSendEvent (display, wid, false, StructureNotifyMask, (XEvent *) & ev);
    XFlush (display);
    XUnlockDisplay(display);
    eventThread->wait (500);
    delete eventThread;

    gstapp->stop ();
    delete backend;
    delete gstapp;

    fprintf (stderr, "closing display\n");
    XCloseDisplay (display);
    fprintf (stderr, "done\n");
    fflush (stderr);
    return 0;
}

#include "gstplayer.moc"
