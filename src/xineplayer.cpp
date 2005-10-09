/* This file is part of the KMPlayer application
   Copyright (C) 2003 Koos Vriezen <koos.vriezen@xs4all.nl>

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
#include <config.h>
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
#include "xineplayer.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

#include <xine.h>
#include <xine/xineutils.h>

#ifndef XShmGetEventBase
extern int XShmGetEventBase(Display *);
#endif

#define MWM_HINTS_DECORATIONS   (1L << 1)
#define PROP_MWM_HINTS_ELEMENTS 5
typedef struct {
    uint32_t  flags;
    uint32_t  functions;
    uint32_t  decorations;
    int32_t   input_mode;
    uint32_t  status;
} MWMHints;


static KXinePlayer * xineapp;
static KMPlayer::Callback_stub * callback;
static QMutex mutex (true);

static xine_t              *xine;
static xine_stream_t       *stream;
static xine_stream_t       *sub_stream;
static xine_video_port_t   *vo_port;
static xine_audio_port_t   *ao_port;
static xine_post_t         *post_plugin;
static xine_event_queue_t  *event_queue;
static xine_cfg_entry_t     audio_vis_cfg_entry;
static x11_visual_t         vis;
static char                *dvd_device;
static char                *vcd_device;
static char                *vo_driver = "auto";
static char                *ao_driver = "auto";
static char                 configfile[2048];

static Display             *display;
static Window               wid;
static bool                 window_created;
static bool                 xine_verbose;
static bool                 xine_vverbose;
static bool                 wants_config;
static bool                 audio_vis;
static int                  screen;
static int                  completion_event;
static int                  xpos, ypos, width, height;
static int                  movie_width, movie_height, movie_length, movie_pos;
static int                  movie_brightness = 32767;
static int                  movie_contrast = 32767;
static int                  movie_hue = 32767;
static int                  movie_saturation = 32767;
static int                  movie_volume = 32767;
static double               pixel_aspect;

static int                  running = 0;
static volatile int         firstframe = 0;
static const int            event_finished = QEvent::User;
static const int            event_progress = QEvent::User + 2;
static const int            event_url = QEvent::User + 3;
static const int            event_size = QEvent::User + 4;
static const int            event_title = QEvent::User + 5;
static QString mrl;
static QString sub_mrl;
static QString alang, slang;

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

static void dest_size_cb(void * /*data*/, int /*video_width*/, int /*video_height*/, double /*video_pixel_aspect*/,
        int *dest_width, int *dest_height, double *dest_pixel_aspect)  {

    *dest_width        = width;
    *dest_height       = height;
    *dest_pixel_aspect = pixel_aspect;
}

static void frame_output_cb(void * /*data*/, int /*video_width*/, int /*video_height*/,
        double /*video_pixel_aspect*/, int *dest_x, int *dest_y,
        int *dest_width, int *dest_height, 
        double *dest_pixel_aspect, int *win_x, int *win_y) {
    if (running && firstframe) {
        firstframe = 0;
       /* int pos;
        fprintf(stderr, "first frame\n");
        mutex.lock ();
        xine_get_pos_length (stream, 0, &pos, &movie_length);
        movie_width = xine_get_stream_info(stream, XINE_STREAM_INFO_VIDEO_WIDTH);
        movie_height = xine_get_stream_info(stream, XINE_STREAM_INFO_VIDEO_HEIGHT);
        mutex.unlock ();
        QApplication::postEvent (xineapp, new XineMovieParamEvent (movie_length, movie_width, movie_height, true));
        */
    }

    *dest_x            = 0;
    *dest_y            = 0;
    *win_x             = xpos;
    *win_y             = ypos;
    *dest_width        = width;
    *dest_height       = height;
    *dest_pixel_aspect = pixel_aspect;
}

static void xine_config_cb (void * /*user_data*/, xine_cfg_entry_t * entry) {
    fprintf (stderr, "xine_config_cb %s\n", entry->enum_values[entry->num_value]);
    if (!stream)
        return;
    mutex.lock ();
    if (post_plugin) {
        xine_post_wire_audio_port (xine_get_audio_source (stream), ao_port);
        xine_post_dispose (xine, post_plugin);
        post_plugin = 0L;
    }
    if (audio_vis && strcmp (entry->enum_values[entry->num_value], "none")) {
        post_plugin = xine_post_init (xine, entry->enum_values[entry->num_value], 0, &ao_port, &vo_port);
        xine_post_wire (xine_get_audio_source (stream), (xine_post_in_t *) xine_post_input (post_plugin, (char *) "audio in"));
    }
    mutex.unlock ();
}

static void event_listener(void * /*user_data*/, const xine_event_t *event) {
    if (event->stream != stream)
        return; // not interested in sub_stream events
    switch(event->type) { 
        case XINE_EVENT_UI_PLAYBACK_FINISHED:
            fprintf (stderr, "XINE_EVENT_UI_PLAYBACK_FINISHED\n");
            QApplication::postEvent (xineapp, new QEvent ((QEvent::Type) event_finished));
            break;
        case XINE_EVENT_PROGRESS:
            QApplication::postEvent (xineapp, new XineProgressEvent (((xine_progress_data_t *) event->data)->percent));
            break;
        case XINE_EVENT_MRL_REFERENCE:
            fprintf(stderr, "XINE_EVENT_MRL_REFERENCE %s\n", 
            ((xine_mrl_reference_data_t*)event->data)->mrl);
            QApplication::postEvent (xineapp, new XineURLEvent (QString::fromLocal8Bit (((xine_mrl_reference_data_t*)event->data)->mrl)));
            break;
        case XINE_EVENT_FRAME_FORMAT_CHANGE:
            fprintf (stderr, "XINE_EVENT_FRAME_FORMAT_CHANGE\n");
            break;
        case XINE_EVENT_UI_SET_TITLE:
            {
                xine_ui_data_t * data = (xine_ui_data_t *) event->data;
                QApplication::postEvent(xineapp, new XineTitleEvent(data->str));
                fprintf (stderr, "Set title event %s\n", data->str);
            }
            break;
        case XINE_EVENT_UI_CHANNELS_CHANGED: {
            fprintf (stderr, "Channel changed event\n");
            mutex.lock ();
            int w = xine_get_stream_info(stream, XINE_STREAM_INFO_VIDEO_WIDTH);
            int h = xine_get_stream_info(stream, XINE_STREAM_INFO_VIDEO_HEIGHT);
            int pos, l, nr;
            xine_get_pos_length (stream, 0, &pos, &l);
            char * langstr = new char [66];
            QStringList alanglist, slanglist;

            nr =xine_get_stream_info(stream,XINE_STREAM_INFO_MAX_AUDIO_CHANNEL);
            // if nrch > 25) nrch = 25
            for (int i = 0; i < nr; ++i) {
                xine_get_audio_lang (stream, i, langstr);
                QString ls = QString::fromLocal8Bit (langstr).stripWhiteSpace();
                if (ls.isEmpty ())
                    continue;
                if (!slang.isEmpty () && alang == ls)
                    xine_set_param(stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL, i);
                alanglist.push_back (ls);
                fprintf (stderr, "alang %s\n", langstr);
            }
            nr = xine_get_stream_info(stream, XINE_STREAM_INFO_MAX_SPU_CHANNEL);
            // if nrch > 25) nrch = 25
            for (int i = 0; i < nr; ++i) {
                xine_get_spu_lang (stream, i, langstr);
                QString ls = QString::fromLocal8Bit (langstr).stripWhiteSpace();
                if (ls.isEmpty ())
                    continue;
                if (!slang.isEmpty () && slang == ls)
                    xine_set_param (stream, XINE_PARAM_SPU_CHANNEL, i);
                slanglist.push_back (ls);
                fprintf (stderr, "slang %s\n", langstr);
            }
            delete langstr;
            mutex.unlock ();
            movie_width = w;
            movie_height = h;
            movie_length = l;
            QApplication::postEvent (xineapp, new XineMovieParamEvent (l, w, h, alanglist, slanglist, firstframe));
            if (running && firstframe)
                firstframe = 0;
            if (window_created && w > 0 && h > 0) {
                XLockDisplay (display);
                XResizeWindow (display, wid, movie_width, movie_height);
                XFlush (display);
                XUnlockDisplay (display);
            }
            break;
        }
        case XINE_EVENT_INPUT_MOUSE_MOVE:
            break;
        default:
            fprintf (stderr, "event_listener %d\n", event->type);

    }
}

} // extern "C"

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

void Backend::play () {
    xineapp->play ();
}

void Backend::stop () {
    QTimer::singleShot (0, xineapp, SLOT (stop ()));
}

void Backend::pause () {
    xineapp->pause ();
}

void Backend::seek (int pos, bool /*absolute*/) {
    xineapp->seek (pos);
}

void Backend::hue (int h, bool) {
    xineapp->hue (65535 * (h + 100) / 200);
}

void Backend::saturation (int s, bool) {
    xineapp->saturation (65535 * (s + 100) / 200);
}

void Backend::contrast (int c, bool) {
    xineapp->contrast (65535 * (c + 100) / 200);
}

void Backend::brightness (int b, bool) {
    xineapp->brightness (65535 * (b + 100) / 200);
}

void Backend::volume (int v, bool) {
    xineapp->volume (v);
}

void Backend::frequency (int) {
}

void Backend::setAudioLang (int id, QString al) {
    xineapp->setAudioLang (id, al);
}

void Backend::setSubtitle (int id, QString sl) {
    xineapp->setSubtitle (id, sl);
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
    bool changed = false;
    xine_cfg_entry_t cfg_entry;
    if (!xine_config_lookup_entry (xine, name.ascii (), &cfg_entry))
        return false;
    if (cfg_entry.type == XINE_CONFIG_TYPE_STRING ||
            cfg_entry.type == XINE_CONFIG_TYPE_UNKNOWN) {
        changed = strcmp (cfg_entry.str_value, value.ascii ());
        cfg_entry.str_value = (char *) value.ascii ();
    } else {
        changed = cfg_entry.num_value != value.toInt ();
        cfg_entry.num_value = value.toInt ();
    }
    xine_config_update_entry (xine,  &cfg_entry);
    return changed;
}

void Backend::setConfig (QByteArray data) {
    QString err;
    int line, column;
    QDomDocument dom;
    if (dom.setContent (data, false, &err, &line, &column)) {
        if (dom.childNodes().length() == 1) {
            for (QDomNode node = dom.firstChild().firstChild();
                    !node.isNull ();
                    node = node.nextSibling ()) {
                QDomNamedNodeMap attr = node.attributes ();
                updateConfigEntry (attr.namedItem (attname).nodeValue (),
                                   attr.namedItem (attvalue).nodeValue ());
            }
            xine_config_save (xine, configfile);
        } else
            err = QString ("invalid data");
    }
    if (callback)
        callback->errorMessage (0, err);
}

KXinePlayer::KXinePlayer (int _argc, char ** _argv)
  : QApplication (_argc, _argv, false) {
    window_created = true;
    for(int i = 1; i < argc (); i++) {
        if (!strcmp (argv ()[i], "-vo")) {
            vo_driver = argv ()[++i];
            if (!strcmp (vo_driver, "x11"))
                vo_driver = (char *) "xshm";
        } else if (!strcmp (argv ()[i], "-ao")) {
            ao_driver = argv ()[++i];
        } else if (!strcmp (argv ()[i], "-dvd-device")) {
            dvd_device = argv ()[++i];
        } else if (!strcmp (argv ()[i], "-vcd-device")) {
            vcd_device = argv ()[++i];
        } else if (!strcmp (argv ()[i], "-wid") || !strcmp (argv ()[i], "-window-id")) {
            wid = atol (argv ()[++i]);
            window_created = false;
        } else if (!strcmp (argv ()[i], "-root")) {
            wid =  XDefaultRootWindow (display);
            window_created = false;
        } else if (!strcmp (argv ()[i], "-window")) {
            ;
        } else if (!strcmp (argv ()[i], "-sub")) {
            sub_mrl = QString (argv ()[++i]);
        } else if (!strcmp (argv ()[i], "-lang")) {
            slang = alang = QString (argv ()[++i]);
        } else if (!strcmp (argv ()[i], "-v")) {
            xine_verbose = true;
        } else if (!strcmp (argv ()[i], "-vv")) {
            xine_verbose = xine_vverbose = true;
        } else if (!strcmp (argv ()[i], "-c")) {
            wants_config = true;
        } else if (!strcmp (argv ()[i], "-f") && i < argc () - 1) {
            strncpy (configfile, argv ()[++i], sizeof (configfile));
            configfile[sizeof (configfile) - 1] = 0;
        } else if (!strcmp (argv ()[i], "-cb")) {
            QString str = argv ()[++i];
            int pos = str.find ('/');
            if (pos > -1) {
                fprintf (stderr, "callback is %s %s\n", str.left (pos).ascii (), str.mid (pos + 1).ascii ());
                callback = new KMPlayer::Callback_stub 
                    (str.left (pos).ascii (), str.mid (pos + 1).ascii ());
            }
        } else 
            mrl = QString::fromLocal8Bit (argv ()[i]);
    }
    xpos    = 0;
    ypos    = 0;
    width   = 320;
    height  = 200;

    if (window_created)
        wid = XCreateSimpleWindow(display, XDefaultRootWindow(display),
                xpos, ypos, width, height, 1, 0, 0);
    if (!callback)
        QTimer::singleShot (10, this, SLOT (play ()));
    XSelectInput (display, wid,
                  (PointerMotionMask | ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask)); // | SubstructureNotifyMask));
}

void KXinePlayer::init () {
    XLockDisplay(display);
    XWindowAttributes attr;
    XGetWindowAttributes(display, wid, &attr);
    width = attr.width;
    height = attr.height;
    if (XShmQueryExtension(display) == True)
        completion_event = XShmGetEventBase(display) + ShmCompletion;
    else
        completion_event = -1;
    if (window_created) {
        fprintf (stderr, "map %lu\n", wid);
        XMapRaised(display, wid);
        XSync(display, False);
    }
    //double d->res_h = 1.0 * DisplayWidth(display, screen) / DisplayWidthMM(display, screen);
    //double d->res_v = 1.0 * DisplayHeight(display, screen) / DisplayHeightMM(display, screen);
    XUnlockDisplay(display);
    vis.display           = display;
    vis.screen            = screen;
    vis.d                 = wid;
    vis.dest_size_cb      = dest_size_cb;
    vis.frame_output_cb   = frame_output_cb;
    vis.user_data         = NULL;
    //pixel_aspect          = d->res_v / d->res_h;

    //if(fabs(pixel_aspect - 1.0) < 0.01)
        pixel_aspect = 1.0;

    const char *const * pp = xine_list_post_plugins_typed (xine, XINE_POST_TYPE_AUDIO_VISUALIZATION);
    int i;
    for (i = 0; pp[i]; i++);
    const char ** options = new const char * [i+2];
    options[0] = "none";
    for (i = 0; pp[i]; i++)
        options[i+1] = pp[i];
    options[i+1] = 0L;
    xine_config_register_enum (xine, "audio.visualization", 0, (char ** ) options, 0L, 0L, 0, xine_config_cb, 0L);
}

KXinePlayer::~KXinePlayer () {
    if (window_created) {
        XLockDisplay (display);
        fprintf (stderr, "unmap %lu\n", wid);
        XUnmapWindow (display,  wid);
        XDestroyWindow(display,  wid);
        XSync (display, False);
        XUnlockDisplay (display);
    }
    xineapp = 0L;
}

void getConfigEntries (QByteArray & buf) {
    xine_cfg_entry_t entry;
    QDomDocument doc;
    QDomElement root = doc.createElement (QString ("document"));
    for (int i = xine_config_get_first_entry (xine, &entry);
            i;
            i = xine_config_get_next_entry (xine, &entry)) {
        QDomElement elm = doc.createElement (elmentry);
        elm.setAttribute (attname, QString (entry.key));
        if (entry.type == XINE_CONFIG_TYPE_STRING || entry.type == XINE_CONFIG_TYPE_UNKNOWN) {
            elm.setAttribute (atttype, valstring);
            elm.setAttribute (attvalue, QString (entry.str_value));
        } else {
            elm.setAttribute (attdefault, QString::number (entry.num_default));
            elm.setAttribute (attvalue, QString::number (entry.num_value));
            switch (entry.type) {
                case XINE_CONFIG_TYPE_RANGE:
                    elm.setAttribute (atttype, valrange);
                    elm.setAttribute (attstart, QString::number (entry.range_min));
                    elm.setAttribute (attend, QString::number (entry.range_max));
                    break;
                case XINE_CONFIG_TYPE_ENUM:
                    elm.setAttribute (atttype, valenum);
                    for (int i = 0; entry.enum_values[i]; i++) {
                        QDomElement item = doc.createElement (elmitem);
                        item.setAttribute (attvalue, QString (entry.enum_values[i]));
                        elm.appendChild (item);
                    }
                    break;
                case XINE_CONFIG_TYPE_NUM:
                    elm.setAttribute (atttype, valnum);
                    break;
                case XINE_CONFIG_TYPE_BOOL:
                    elm.setAttribute (atttype, valbool);
                    break;
                default:
                    fprintf (stderr, "unhandled config type: %d\n", entry.type);
            }
        }
        if (entry.help)
            elm.appendChild (doc.createTextNode (QString::fromUtf8 (entry.help)));
        root.appendChild (elm);
    }
    doc.appendChild (root);
    QCString exp = doc.toCString ();
    buf = exp;
    buf.resize (exp.length ()); // strip terminating \0
}

void KXinePlayer::play () {
    fprintf (stderr, "play mrl: '%s'\n", (const char *) mrl.local8Bit ());
    mutex.lock ();
    if (running) {
        if (xine_get_status (stream) == XINE_STATUS_PLAY &&
            xine_get_param (stream, XINE_PARAM_SPEED) == XINE_SPEED_PAUSE)
            xine_set_param( stream, XINE_PARAM_SPEED, XINE_SPEED_NORMAL);
        mutex.unlock ();
        return;
    }
    movie_pos = 0;
    movie_width = 0;
    movie_height = 0;

    stream = xine_stream_new (xine, ao_port, vo_port);
    event_queue = xine_event_new_queue (stream);
    xine_event_create_listener_thread (event_queue, event_listener, NULL);

    xine_gui_send_vo_data(stream, XINE_GUI_SEND_VIDEOWIN_VISIBLE, (void *) 1);

    running = 1;
    if (!xine_open (stream, (const char *) mrl.local8Bit ())) {
        fprintf(stderr, "Unable to open mrl '%s'\n", (const char *) mrl.local8Bit ());
        mutex.unlock ();
        finished ();
        return;
    }
    xine_set_param (stream, XINE_PARAM_VO_SATURATION, movie_saturation);
    xine_set_param (stream, XINE_PARAM_VO_BRIGHTNESS, movie_brightness);
    xine_set_param (stream, XINE_PARAM_VO_CONTRAST, movie_contrast);
    xine_set_param (stream, XINE_PARAM_VO_HUE, movie_hue);

    if (!sub_mrl.isEmpty ()) {
        fprintf(stderr, "Using subtitles from '%s'\n", (const char *) sub_mrl.local8Bit ());
        sub_stream = xine_stream_new (xine, NULL, vo_port);
        if (xine_open (sub_stream, (const char *) sub_mrl.local8Bit ())) {
            xine_stream_master_slave (stream, sub_stream,
                    XINE_MASTER_SLAVE_PLAY | XINE_MASTER_SLAVE_STOP);
        } else {
            fprintf(stderr, "Unable to open subtitles from '%s'\n", (const char *) sub_mrl.local8Bit ());
            xine_dispose (sub_stream);
            sub_stream = 0L;
        }
    } 
    if (!xine_play (stream, 0, 0)) {
        fprintf(stderr, "Unable to play mrl '%s'\n", (const char *) mrl.local8Bit ());
        mutex.unlock ();
        finished ();
        return;
    }
    audio_vis = !xine_get_stream_info (stream, XINE_STREAM_INFO_HAS_VIDEO);
    audio_vis &= xine_config_lookup_entry (xine, "audio.visualization", &audio_vis_cfg_entry);
    mutex.unlock ();
    if (audio_vis)
        xine_config_cb (0L, &audio_vis_cfg_entry);
    if (callback)
        firstframe = 1;
}

void KXinePlayer::stop () {
    if (!running) return;
    fprintf(stderr, "stop\n");
    mutex.lock ();
    if (sub_stream)
        xine_stop (sub_stream);
    xine_stop (stream);
    mutex.unlock ();
    QTimer::singleShot (10, this, SLOT (postFinished ()));
}

void KXinePlayer::postFinished () {
    QApplication::postEvent (xineapp, new QEvent ((QEvent::Type) event_finished));
}

void KXinePlayer::pause () {
    if (!running) return;
    mutex.lock ();
    if (xine_get_status (stream) == XINE_STATUS_PLAY)
        xine_set_param( stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE);
    mutex.unlock ();
}

void KXinePlayer::finished () {
    QTimer::singleShot (10, this, SLOT (stop ()));
}

void KXinePlayer::setAudioLang (int id, const QString & al) {
    alang = al;
    mutex.lock ();
    if (xine_get_status (stream) == XINE_STATUS_PLAY)
        xine_set_param(stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL, id);
    mutex.unlock ();
}

void KXinePlayer::setSubtitle (int id, const QString & sl) {
    slang = sl;
    mutex.lock ();
    if (xine_get_status (stream) == XINE_STATUS_PLAY)
        xine_set_param (stream, XINE_PARAM_SPU_CHANNEL, id);
    mutex.unlock ();
}

void KXinePlayer::updatePosition () {
    if (!running || !callback) return;
    int pos;
    mutex.lock ();
    xine_get_pos_length (stream, 0, &pos, &movie_length);
    mutex.unlock ();
    if (movie_pos != pos) {
        movie_pos = pos;
        callback->moviePosition (pos/100);
    }
    QTimer::singleShot (500, this, SLOT (updatePosition ()));
}

void KXinePlayer::saturation (int val) {
    movie_saturation = val;
    if (running) {
        mutex.lock ();
        xine_set_param (stream, XINE_PARAM_VO_SATURATION, val);
        mutex.unlock ();
    }
}

void KXinePlayer::hue (int val) {
    movie_hue = val;
    if (running) {
        mutex.lock ();
        xine_set_param (stream, XINE_PARAM_VO_HUE, val);
        mutex.unlock ();
    }
}

void KXinePlayer::contrast (int val) {
    movie_contrast = val;
    if (running) {
        mutex.lock ();
        xine_set_param (stream, XINE_PARAM_VO_CONTRAST, val);
        mutex.unlock ();
    }
}

void KXinePlayer::brightness (int val) {
    movie_brightness = val;
    if (running) {
        mutex.lock ();
        xine_set_param (stream, XINE_PARAM_VO_BRIGHTNESS, val);
        mutex.unlock ();
    }
}

void KXinePlayer::volume (int val) {
    movie_volume = val;
    if (running) {
        mutex.lock ();
        xine_set_param( stream, XINE_PARAM_AUDIO_VOLUME, val);
        mutex.unlock ();
    }
}

void KXinePlayer::seek (int val) {
    if (running) {
        fprintf(stderr, "seek %d\n", val);
        mutex.lock ();
        if (!xine_play (stream, 0, val * 100)) {
            fprintf(stderr, "Unable to seek to %d :-(\n", val);
        }
        mutex.unlock ();
    }
}

bool KXinePlayer::event (QEvent * e) {
    switch (e->type()) {
        case event_finished: {
            fprintf (stderr, "event_finished\n");
            if (audio_vis) {
                audio_vis_cfg_entry.num_value = 0;
                xine_config_cb (0L, &audio_vis_cfg_entry);
            }
            mutex.lock ();
            running = 0;
            firstframe = 0;
            if (sub_stream) {
                xine_dispose (sub_stream);
                sub_stream = 0L;
            }
            if (stream) {
                xine_event_dispose_queue (event_queue);
                xine_dispose (stream);
                stream = 0L;
            }
            mutex.unlock ();
            //XLockDisplay (display);
            //XClearWindow (display, wid);
            //XUnlockDisplay (display);
            if (callback)
                callback->finished ();
            else
                QTimer::singleShot (0, this, SLOT (quit ()));
            break;
        }
        case event_size: {
            if (callback) {
                XineMovieParamEvent * se = static_cast <XineMovieParamEvent *> (e);
                if (se->length < 0) se->length = 0;
                callback->movieParams (se->length/100, se->width, se->height, se->height ? 1.0*se->width/se->height : 1.0, se->alang, se->slang);
                if (se->first_frame) {
                    callback->playing ();
                    QTimer::singleShot (500, this, SLOT (updatePosition ()));
                }
            }
            break;
        }
        case event_progress: {
            XineProgressEvent * pe = static_cast <XineProgressEvent *> (e);                
            if (callback)
                callback->loadingProgress (pe->progress);
            break;
        }
        case event_url: {
            XineURLEvent * ue = static_cast <XineURLEvent *> (e);                
            if (callback)
                callback->statusMessage ((int) KMPlayer::Callback::stat_addurl, ue->url);
            break;
        }
        case event_title: {
            XineTitleEvent * ue = static_cast <XineTitleEvent *> (e);                
            if (callback)
                callback->statusMessage ((int) KMPlayer::Callback::stat_newtitle, ue->title);
            break;
        }
        default:
            return false;
    }
    return true;
}

XineMovieParamEvent::XineMovieParamEvent(int l, int w, int h, const QStringList & a, const QStringList & s, bool ff)
  : QEvent ((QEvent::Type) event_size),
    length (l), width (w), height (h), alang (a), slang (s) , first_frame (ff)
{}

XineURLEvent::XineURLEvent (const QString & u)
  : QEvent ((QEvent::Type) event_url), url (u) 
{}

XineTitleEvent::XineTitleEvent (const char * t)
  : QEvent ((QEvent::Type) event_title), title (QString::fromUtf8 (t)) 
{
    QUrl::decode (title);
}

XineProgressEvent::XineProgressEvent (const int p)
  : QEvent ((QEvent::Type) event_progress), progress (p) 
{}

//static bool translateCoordinates (int wx, int wy, int mx, int my) {
//    movie_width
class XEventThread : public QThread {
protected:
    void run () {
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
                case KeyPress:
                    {
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
                                xineapp->lock ();
                                xineapp->stop ();
                                xineapp->unlock ();
                                break;

                            case XK_p: // previous
                                mutex.lock ();
                                if (stream) {
                                    xine_event_t xine_event =  { 
                                        XINE_EVENT_INPUT_PREVIOUS,
                                        stream, 0L, 0, { 0, 0 }
                                    };
                                    xine_event_send (stream, &xine_event);
                                } 
                                mutex.unlock ();
                                break;

                            case XK_n: // next
                                mutex.lock ();
                                if (stream) {
                                    xine_event_t xine_event =  { 
                                        XINE_EVENT_INPUT_NEXT,
                                        stream, 0L, 0, { 0, 0 }
                                    };
                                    xine_event_send (stream, &xine_event);
                                } 
                                mutex.unlock ();
                                break;

                            case XK_u: // up menu
                                mutex.lock ();
                                if (stream) {
                                    xine_event_t xine_event =  { 
                                        XINE_EVENT_INPUT_MENU1,
                                        stream, 0L, 0, { 0, 0 }
                                    };
                                    xine_event_send (stream, &xine_event);
                                } 
                                mutex.unlock ();
                                break;

                            case XK_r: // root menu
                                mutex.lock ();
                                if (stream) {
                                    xine_event_t xine_event =  { 
                                        XINE_EVENT_INPUT_MENU3,
                                        stream, 0L, 0, { 0, 0 }
                                    };
                                    xine_event_send (stream, &xine_event);
                                } 
                                mutex.unlock ();
                                break;

                            case XK_Up:
                                xine_set_param(stream, XINE_PARAM_AUDIO_VOLUME,
                                        (xine_get_param(stream, XINE_PARAM_AUDIO_VOLUME) + 1));
                                break;

                            case XK_Down:
                                xine_set_param(stream, XINE_PARAM_AUDIO_VOLUME,
                                        (xine_get_param(stream, XINE_PARAM_AUDIO_VOLUME) - 1));
                                break;

                            case XK_plus:
                                xine_set_param(stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL, 
                                        (xine_get_param(stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL) + 1));
                                break;

                            case XK_minus:
                                xine_set_param(stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL, 
                                        (xine_get_param(stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL) - 1));
                                break;

                            case XK_space:
                                if(xine_get_param(stream, XINE_PARAM_SPEED) != XINE_SPEED_PAUSE)
                                    xine_set_param(stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE);
                                else
                                    xine_set_param(stream, XINE_PARAM_SPEED, XINE_SPEED_NORMAL);
                                break;

                        }
                    }
                    break;

                case Expose:
                    if(xevent.xexpose.count != 0 || !stream || xevent.xexpose.window != wid)
                        break;
                    mutex.lock ();
                    xine_gui_send_vo_data(stream, XINE_GUI_SEND_EXPOSE_EVENT, &xevent);
                    mutex.unlock ();
                    break;

                case ConfigureNotify:
                    {
                        Window           tmp_win;

                        width  = xevent.xconfigure.width;
                        height = xevent.xconfigure.height;
                        if((xevent.xconfigure.x == 0) && (xevent.xconfigure.y == 0)) {
                            XLockDisplay(display);
                            XTranslateCoordinates(display, xevent.xconfigure.window,
                                    DefaultRootWindow(xevent.xconfigure.display),
                                    0, 0, &xpos, &ypos, &tmp_win);
                            XUnlockDisplay(display);
                        }
                        else {
                            xpos = xevent.xconfigure.x;
                            ypos = xevent.xconfigure.y;
                        }
                    }

                    break;
                case MotionNotify:
                    if (stream) {
                        XMotionEvent *mev = (XMotionEvent *) &xevent;
                        x11_rectangle_t rect = { mev->x, mev->y, 0, 0 };
                        if (xine_gui_send_vo_data (stream, XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO, (void*) &rect) == -1)
                            break;
                        xine_input_data_t data;
                        data.x = rect.x;
                        data.y = rect.y;
                        data.button = 0;
                        xine_event_t xine_event =  { 
                                XINE_EVENT_INPUT_MOUSE_MOVE,
                                stream, &data, sizeof (xine_input_data_t),
                                { 0 , 0 }
                        };
                        mutex.lock ();
                        xine_event_send (stream, &xine_event);
                        mutex.unlock ();
                    }
                    break;
                case ButtonPress:
                    if (stream) {
                        fprintf(stderr, "ButtonPress\n");
                        XButtonEvent *bev = (XButtonEvent *) &xevent;
                        x11_rectangle_t rect = { bev->x, bev->y, 0, 0 };
                        if (xine_gui_send_vo_data (stream, XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO, (void*) &rect) == -1)
                            break;
                        xine_input_data_t data;
                        data.x = rect.x;
                        data.y = rect.y;
                        data.button = 1;
                        xine_event_t xine_event =  { 
                                XINE_EVENT_INPUT_MOUSE_BUTTON,
                                stream, &data, sizeof (xine_input_data_t),
                                { 0, 0 }
                        };
                        mutex.lock ();
                        xine_event_send (stream, &xine_event);
                        mutex.unlock ();
                    }
                    break;
                case NoExpose:
                    //fprintf (stderr, "NoExpose %lu\n", xevent.xnoexpose.drawable);
                    break;
                case CreateNotify:
                    fprintf (stderr, "CreateNotify: %lu %lu %d,%d %dx%d\n",
                            xevent.xcreatewindow.window, xevent.xcreatewindow.parent,
                            xevent.xcreatewindow.x, xevent.xcreatewindow.y,
                            xevent.xcreatewindow.width, xevent.xcreatewindow.height);
                    break;
                case DestroyNotify:
                    fprintf (stderr, "DestroyNotify: %lu\n", xevent.xdestroywindow.window);
                    break;
                default:
                    if (xevent.type < LASTEvent)
                        fprintf (stderr, "event %d\n", xevent.type);
            }

            if(xevent.type == completion_event && stream)
                xine_gui_send_vo_data(stream, XINE_GUI_SEND_COMPLETION_EVENT, &xevent);
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

    snprintf(configfile, sizeof (configfile), "%s%s", xine_get_homedir(), "/.xine/config2");
    xineapp = new KXinePlayer (argc, argv);
    bool config_changed = !QFile (configfile).exists ();

    if (!callback && mrl.isEmpty ()) {
        fprintf (stderr, "usage: %s [-vo (xv|xshm)] [-ao (arts|esd|..)] [-f <xine config file>] [-dvd-device <device>] [-vcd-device <device>] [-wid <X11 Window>|-window-id <X11 Window>|-root] [-sub <subtitle url>] [-lang <lang>] [(-v|-vv)] [-cb <DCOP callback name> [-c]] [<url>]\n", argv[0]);
        delete xineapp;
        return 1;
    }

    DCOPClient dcopclient;
    dcopclient.registerAs ("kxineplayer");
    Backend player;

    XEventThread * eventThread = new XEventThread;
    eventThread->start ();

    xine = xine_new();
    if (xine_verbose)
        xine_engine_set_param (xine, XINE_ENGINE_PARAM_VERBOSITY, xine_vverbose ? XINE_VERBOSITY_DEBUG : XINE_VERBOSITY_LOG);
    xine_config_load(xine, configfile);
    xine_init(xine);

    xineapp->init ();

    if (dvd_device)
        config_changed |= updateConfigEntry (QString ("input.dvd_device"), QString (dvd_device));
    if (vcd_device)
        config_changed |= updateConfigEntry (QString ("input.vcd_device"), QString (vcd_device));

    if (config_changed)
        xine_config_save (xine, configfile);

    vo_port = xine_open_video_driver(xine, vo_driver, XINE_VISUAL_TYPE_X11, (void *) &vis);
    ao_port = xine_open_audio_driver (xine, ao_driver, NULL);
    stream = xine_stream_new (xine, ao_port, vo_port);

    QByteArray buf;
    if (wants_config) {
        /* TODO? Opening the output drivers in front, will add more config
                 settings. Unfortunately, that also adds a second in startup..
        const char *const * aops = xine_list_audio_output_plugins (xine);
        for (const char *const* aop = aops; *aop; aop++) {
            xine_audio_port_t * ap = xine_open_audio_driver (xine, *aop, 0L);
            xine_close_audio_driver (xine, ap);
            fprintf (stderr, "audio output: %s\n", *aop);
        }
        const char *const * vops = xine_list_video_output_plugins (xine);
        for (const char *const* vop = vops; *vop; vop++) {
            xine_video_port_t * vp = xine_open_video_driver (xine, *vop, XINE_VISUAL_TYPE_NONE, 0L);
            xine_close_video_driver (xine, vp);
            fprintf (stderr, "vidio output: %s\n", *vop);
        }*/
        getConfigEntries (buf);
    }
    if (callback)
        callback->started (dcopclient.appId (), buf);
    else
        ;//printf ("%s\n", QString (buf).ascii ());
    xineapp->exec ();

    if (sub_stream)
        xine_dispose (sub_stream);
    if (stream) {
        xine_event_dispose_queue (event_queue);
        xine_dispose (stream);
    }
    if (ao_port)
        xine_close_audio_driver (xine, ao_port);  
    if (vo_port)
        xine_close_video_driver (xine, vo_port);  
    XLockDisplay(display);
    XClientMessageEvent ev = {
        ClientMessage, 0, true, display, wid, 
        XInternAtom (display, "XINE", false), 8, {b: "quit_now"}
    };
    XSendEvent (display, wid, FALSE, StructureNotifyMask, (XEvent *) & ev);
    XFlush (display);
    XUnlockDisplay(display);
    eventThread->wait (500);
    delete eventThread;

    xineapp->stop ();
    delete xineapp;

    xine_exit (xine);

    fprintf (stderr, "closing display\n");
    XCloseDisplay (display);
    fprintf (stderr, "done\n");
    return 0;
}

#include "xineplayer.moc"
