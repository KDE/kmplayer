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

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <libgen.h>
#include <dcopclient.h>
#include <qcstring.h>
#include <qtimer.h>
#include <qfile.h>
#include <qurl.h>
#include <qthread.h>
#include <qmutex.h>
#include <qdom.h>
#include <qmap.h>
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

typedef struct {
    xine_stream_t       *stream;
    xine_stream_t       *sub_stream;
    xine_video_port_t   *vo_port;
    xine_audio_port_t   *ao_port;
    xine_event_queue_t  *event_queue;
    xine_post_t         *post_plugin;
    x11_visual_t         vis;
    Window               wid;
    int                  xpos, ypos, width, height;
    int                  movie_width, movie_height, movie_length, movie_pos;
    int                  repeat_count;
    int                  movie_brightness;
    int                  movie_contrast;
    int                  movie_hue;
    int                  movie_saturation;
    int                  movie_volume;
    QString              mrl;
    QString              sub_mrl;
    QString              rec_mrl;
    QString              alang, slang;
    QStringList          alanglist, slanglist;
    bool                 running;
    enum { AutoAudioVis, NoAudioVis, AudioVis} audio_vis;
    bool                 finished;
    volatile bool        firstframe; /*yes this sucks */
} StreamInfo;

typedef QMap <Window, StreamInfo *> StreamInfoMap;

static KXinePlayer * xineapp;
static KMPlayer::Callback_stub * callback;

static xine_t              *xine;
static QString              vo_driver ("auto");
static QString              ao_driver ("auto");
static QString              alang, slang;
static StreamInfoMap        stream_info_map;
static QMutex               stream_list_mutex (true);
static xine_cfg_entry_t     audio_vis_cfg_entry;
static char                 configfile[2048];
static Atom                 quit_atom;

static Display             *display;
static bool                 window_created;
static bool                 xine_verbose;
static bool                 xine_vverbose;
static bool                 wants_config;
static int                  screen;
static int                  completion_event;
static double               pixel_aspect;

static const int            event_finished = QEvent::User;
static const int            event_progress = QEvent::User + 2;
static const int            event_url = QEvent::User + 3;
static const int            event_size = QEvent::User + 4;
static const int            event_title = QEvent::User + 5;
static const int            event_video = QEvent::User + 6;

static QString elmentry ("entry");
static QString elmitem ("item");
static QString attname ("name");
static QString atttype ("type");
static QString attdefault ("DEFAULT");
static QString attvalue ("value");
static QString attstart ("START");
static QString attend ("end");
static QString valrange ("range");
static QString valnum ("num");
static QString valbool ("bool");
static QString valenum ("enum");
static QString valstring ("string");

extern "C" {
static void dest_size_cb(void * d, int w, int h, double pixasp,
        int *dest_width, int *dest_height, double *dest_pixel_aspect);
static void frame_output_cb (void *d, int w, int h, double pixasp,
        int *dest_x, int *dest_y, int *dest_width, int *dest_height,
        double *dest_pixel_aspect, int *win_x, int *win_y);
}

static void initStreamInfo (StreamInfo *si, unsigned long wid) {
    si->wid = wid;
    si->vis.display = display;
    si->vis.screen = screen;
    si->vis.d = wid;
    si->vis.dest_size_cb = dest_size_cb;
    si->vis.frame_output_cb = frame_output_cb;
    si->vis.user_data = si;
    si->sub_stream = NULL;
    si->event_queue = NULL;
    si->post_plugin = NULL;
    si->running = false;
    si->audio_vis = StreamInfo::AutoAudioVis;
    si->finished = false;
    si->firstframe = false;
    si->movie_brightness = 32767;
    si->movie_contrast = 32767;
    si->movie_hue = 32767;
    si->movie_saturation = 32767;
    si->movie_volume = 32767;
    si->slang = slang;
    si->alang = alang;
}

static StreamInfo *getStreamInfo (Window wid, const char *notfoundmsg=NULL) {
    StreamInfo *si;
    stream_list_mutex.lock ();
    StreamInfoMap::iterator i = stream_info_map.find (wid);
    if (i == stream_info_map.end ()) {
        stream_list_mutex.unlock ();
        if (notfoundmsg) {
            fprintf (stderr, "WARNING %s:stream info not found\n", notfoundmsg);
            return NULL;
        }
        XWindowAttributes attr;
        si = new StreamInfo;
        initStreamInfo (si, wid);

        QStringList vos = QStringList::split (',', vo_driver);
        for (int i = 0; i < vos.size (); i++) {
            if (vos[i] == "x11")
                vos[i] = "xshm";
            else if (vos[i] == "gl")
                vos[i] = "opengl";
            fprintf (stderr, "trying video driver %s ..\n", vos[i].ascii ());
            si->vo_port = xine_open_video_driver(xine, vos[i].ascii (),
                    XINE_VISUAL_TYPE_X11, (void *) &si->vis);
            if (si->vo_port)
                break;
        }
        if (!si->vo_port)
            fprintf (stderr, "no video driver found\n");
        QStringList aos = QStringList::split (',', ao_driver);
        for (int i = 0; i < aos.size (); i++) {
            fprintf (stderr, "trying audio driver %s ..\n", aos[i].ascii ());
            si->ao_port = xine_open_audio_driver (xine, aos[i].ascii (), NULL);
            if (si->ao_port)
                break;
        }
        if (!si->ao_port)
            fprintf (stderr, "audio driver initialisation failed\n");
        si->stream = xine_stream_new (xine, si->ao_port, si->vo_port);

        si->xpos = si->ypos = 0;
        XLockDisplay(display);
        XSelectInput (display, wid,
                (PointerMotionMask | ExposureMask |
                 KeyPressMask | ButtonPressMask |
                 StructureNotifyMask)); // | SubstructureNotifyMask));
        XGetWindowAttributes(display, wid, &attr);
        XUnlockDisplay(display);
        si->width = attr.width;
        si->height = attr.height;

        stream_list_mutex.lock ();
        stream_info_map.insert (wid, si);
    } else {
        si = i.data ();
    }
    stream_list_mutex.unlock ();
    return si;
}

extern "C" {

static void dest_size_cb (void *data, int /*video_width*/, int /*video_height*/, double /*video_pixel_aspect*/,
        int *dest_width, int *dest_height, double *dest_pixel_aspect)  {

    StreamInfo *si = (StreamInfo *)data;
    if (si) {
        *dest_width        = si->width;
        *dest_height       = si->height;
    }
    *dest_pixel_aspect = pixel_aspect;
}

static void frame_output_cb (void *data, int /*video_width*/, int /*video_height*/,
        double /*video_pixel_aspect*/, int *dest_x, int *dest_y,
        int *dest_width, int *dest_height,
        double *dest_pixel_aspect, int *win_x, int *win_y) {
    StreamInfo *si = (StreamInfo *)data;
    if (si->running && si->firstframe) {
        si->firstframe = false;
        int pos;
        fprintf(stderr, "first frame\n");
        xine_get_pos_length (si->stream, 0, &pos, &si->movie_length);
        si->movie_width = xine_get_stream_info (si->stream,
                XINE_STREAM_INFO_VIDEO_WIDTH);
        si->movie_height = xine_get_stream_info (si->stream,
                XINE_STREAM_INFO_VIDEO_HEIGHT);
        QApplication::postEvent (xineapp, new XineMovieParamEvent (si->wid,
                    si->movie_length, si->movie_width, si->movie_height,
                    si->alanglist, si->slanglist, true));
    }

    *dest_x            = 0;
    *dest_y            = 0;
    *win_x             = si->xpos;
    *win_y             = si->ypos;
    *dest_width        = si->width;
    *dest_height       = si->height;
    *dest_pixel_aspect = pixel_aspect;
}

static void xine_config_cb (void *data, xine_cfg_entry_t * entry) {
    StreamInfo *si = (StreamInfo *)data;
    fprintf (stderr, "xine_config_cb %s\n", entry->enum_values[entry->num_value]);
    if (!si || !si->stream)
        return;
    if (si->post_plugin) {
        xine_post_wire_audio_port (xine_get_audio_source (si->stream), si->ao_port);
        xine_post_dispose (xine, si->post_plugin);
        si->post_plugin = 0L;
    }
    if (StreamInfo::AudioVis == si->audio_vis && !si->finished &&
            strcmp (entry->enum_values[entry->num_value], "none")) {
        si->post_plugin = xine_post_init (xine,
                entry->enum_values[entry->num_value], 0,
                &si->ao_port, &si->vo_port);
        xine_post_wire (xine_get_audio_source (si->stream),
                (xine_post_in_t *) xine_post_input (si->post_plugin,
                    (char *) "audio in"));
    }
}

static void event_listener(void *data, const xine_event_t *event) {
    StreamInfo *si = (StreamInfo *)data;
    if (!si || event->stream != si->stream)
        return; // not interested in sub_stream events
    switch(event->type) {
        case XINE_EVENT_UI_PLAYBACK_FINISHED:
            fprintf (stderr, "XINE_EVENT_UI_PLAYBACK_FINISHED\n");
            if (si->repeat_count-- > 0) {
                xine_play (si->stream, 0, 0);
            } else {
                QApplication::postEvent(xineapp,new XineEvent(event_finished, si->wid));
            }
            break;
        case XINE_EVENT_PROGRESS:
            QApplication::postEvent (xineapp, new XineProgressEvent (si->wid,
                        ((xine_progress_data_t *) event->data)->percent));
            break;
        case XINE_EVENT_MRL_REFERENCE:
            fprintf(stderr, "XINE_EVENT_MRL_REFERENCE %s\n",
            ((xine_mrl_reference_data_t*)event->data)->mrl);
            QApplication::postEvent (xineapp, new XineURLEvent (si->wid,
                    QString::fromLocal8Bit (
                        ((xine_mrl_reference_data_t*)event->data)->mrl)));
            break;
        case XINE_EVENT_FRAME_FORMAT_CHANGE:
            fprintf (stderr, "XINE_EVENT_FRAME_FORMAT_CHANGE\n");
            break;
        case XINE_EVENT_UI_SET_TITLE: {
             xine_ui_data_t *d = (xine_ui_data_t *) event->data;
             QApplication::postEvent (xineapp,
                     new XineTitleEvent (si->wid, d->str));
             fprintf (stderr, "Set title event %s\n", d->str);
             break;
        }
        case XINE_EVENT_UI_CHANNELS_CHANGED: {
            fprintf (stderr, "Channel changed event %d\n", si->firstframe);
            int w = xine_get_stream_info(si->stream, XINE_STREAM_INFO_VIDEO_WIDTH);
            int h = xine_get_stream_info(si->stream, XINE_STREAM_INFO_VIDEO_HEIGHT);
            int pos, l, nr;
            xine_get_pos_length (si->stream, 0, &pos, &l);
            char * langstr = new char [66];
            si->alanglist.clear ();
            si->slanglist.clear ();

            nr =xine_get_stream_info(si->stream,XINE_STREAM_INFO_MAX_AUDIO_CHANNEL);
            // if nrch > 25) nrch = 25
            for (int i = 0; i < nr; ++i) {
                if (!xine_get_audio_lang (si->stream, i, langstr))
                    continue;
                QString ls = QString::fromLocal8Bit (langstr).stripWhiteSpace();
                if (ls.isEmpty ())
                    continue;
                if (!si->slang.isEmpty () && alang == ls)
                    xine_set_param(si->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL, i);
                si->alanglist.push_back (ls);
                fprintf (stderr, "alang %s\n", langstr);
            }
            nr = xine_get_stream_info(si->stream, XINE_STREAM_INFO_MAX_SPU_CHANNEL);
            // if nrch > 25) nrch = 25
            for (int i = 0; i < nr; ++i) {
                if (!xine_get_spu_lang (si->stream, i, langstr))
                    continue;
                QString ls = QString::fromLocal8Bit (langstr).stripWhiteSpace();
                if (ls.isEmpty ())
                    continue;
                if (!si->slang.isEmpty () && slang == ls)
                    xine_set_param (si->stream, XINE_PARAM_SPU_CHANNEL, i);
                si->slanglist.push_back (ls);
                fprintf (stderr, "slang %s\n", langstr);
            }
            delete langstr;
            si->movie_width = w;
            si->movie_height = h;
            si->movie_length = l;
            QApplication::postEvent (xineapp, new XineMovieParamEvent (si->wid, l, w, h, si->alanglist, si->slanglist, si->firstframe));
            if (si->running && si->firstframe)
                si->firstframe = 0;
            if (window_created && w > 0 && h > 0) {
                XLockDisplay (display);
                XResizeWindow (display, si->wid, si->movie_width, si->movie_height);
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

void Backend::setURL (unsigned long wid, QString url) {
    StreamInfo *si = getStreamInfo (wid);
    si->mrl = url;
}

void Backend::setSubTitleURL (unsigned long wid, QString url) {
    StreamInfo *si = getStreamInfo (wid, "set sub title");
    if (!si)
        return;
    si->sub_mrl = url;
}

void Backend::play (unsigned long wid, int repeat_count) {
    xineapp->play (wid, repeat_count);
}

void Backend::stop (unsigned long wid) {
    xineapp->stop (wid);
}

void Backend::pause (unsigned long wid) {
    xineapp->pause (wid);
}

void Backend::seek (unsigned long wid, int pos, bool /*absolute*/) {
    xineapp->seek (wid, pos);
}

void Backend::hue (unsigned long wid, int h, bool) {
    xineapp->hue (wid, 65535 * (h + 100) / 200);
}

void Backend::saturation (unsigned long wid, int s, bool) {
    xineapp->saturation (wid, 65535 * (s + 100) / 200);
}

void Backend::contrast (unsigned long wid, int c, bool) {
    xineapp->contrast (wid, 65535 * (c + 100) / 200);
}

void Backend::brightness (unsigned long wid, int b, bool) {
    xineapp->brightness (wid, 65535 * (b + 100) / 200);
}

void Backend::volume (unsigned long wid, int v, bool) {
    xineapp->volume (wid, v);
}

void Backend::property (unsigned long wid, QString prop, QString val) {
    StreamInfo *si = getStreamInfo (wid, "property");
    if (!si)
        return;
    fprintf (stderr, "property %d\n", prop == "audiovisualization");
    if (prop == "audiovisualization")
        si->audio_vis = val.toInt ()
            ? StreamInfo::AutoAudioVis : StreamInfo::NoAudioVis;
}

void Backend::setAudioLang (unsigned long wid, int id, QString al) {
    xineapp->setAudioLang (wid, id, al);
}

void Backend::setSubtitle (unsigned long wid, int id, QString sl) {
    xineapp->setSubtitle (wid, id, sl);
}

void Backend::quit () {
    bool quit_app = true;
    delete callback;
    callback = 0L;
    stream_list_mutex.lock ();
    const StreamInfoMap::iterator e = stream_info_map.end ();
    for (StreamInfoMap::iterator i = stream_info_map.begin (); i != e; ++i)
        if (i.data ()->running) {
            stop (i.data ()->wid);
            quit_app = false;
        }
    stream_list_mutex.unlock ();
    if (quit_app)
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
        callback->errorMessage (0, 0, err);
}

bool Backend::isPlaying (unsigned long wid) {
    StreamInfo *si = getStreamInfo (wid, "playing");
    bool b = si->running &&
        (xine_get_status (si->stream) == XINE_STATUS_PLAY) &&
        (xine_get_param (si->stream, XINE_PARAM_SPEED) != XINE_SPEED_PAUSE);
    return b;
}

KXinePlayer::KXinePlayer (int _argc, char ** _argv)
  : QApplication (_argc, _argv, false) {
}

void KXinePlayer::init () {
    XLockDisplay(display);
    if (XShmQueryExtension(display) == True)
        completion_event = XShmGetEventBase(display) + ShmCompletion;
    else
        completion_event = -1;
    //double d->res_h = 1.0 * DisplayWidth(display, screen) / DisplayWidthMM(display, screen);
    //double d->res_v = 1.0 * DisplayHeight(display, screen) / DisplayHeightMM(display, screen);
    XUnlockDisplay(display);
    //pixel_aspect          = d->res_v / d->res_h;

    //if(fabs(pixel_aspect - 1.0) < 0.01)
        pixel_aspect = 1.0;

    const char *const * pp = xine_list_post_plugins_typed (xine, XINE_POST_TYPE_AUDIO_VISUALIZATION);
    int i;
    for (i = 0; pp[i]; i++) {}
    const char ** options = new const char * [i+2];
    options[0] = "none";
    for (i = 0; pp[i]; i++)
        options[i+1] = pp[i];
    options[i+1] = 0L;
    xine_config_register_enum (xine, "audio.visualization", 0, (char ** ) options, 0L, 0L, 0, xine_config_cb, 0L);
}

KXinePlayer::~KXinePlayer () {
    /*if (window_created) {
        XLockDisplay (display);
        fprintf (stderr, "unmap %lu\n", wid);
        XUnmapWindow (display,  wid);
        XDestroyWindow(display,  wid);
        XSync (display, False);
        XUnlockDisplay (display);
    }*/
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
    QString exp = doc.toString ();
    QCString cexp = exp.utf8 ();
    buf.duplicate (cexp);
    buf.resize (cexp.length ()); // strip terminating \0
}

void KXinePlayer::play (unsigned long wid, int repeat) {
    StreamInfo *si = getStreamInfo (wid, "play");
    if (!si)
        return;

    fprintf (stderr, "play mrl: '%s' %d\n", (const char *) si->mrl.local8Bit (), wid);
    si->repeat_count = repeat;
    if (xine_get_status (si->stream) == XINE_STATUS_PLAY &&
            xine_get_param (si->stream, XINE_PARAM_SPEED) == XINE_SPEED_PAUSE) {
        xine_set_param (si->stream, XINE_PARAM_SPEED, XINE_SPEED_NORMAL);
        return;
    }
    si->movie_pos = 0;
    si->movie_width = 0;
    si->movie_height = 0;

    if (si->mrl.startsWith ("cdda://"))
        si->mrl = QString ("cdda:/") + si->mrl.mid (7);
    //stream = xine_stream_new (xine, ao_port, vo_port);
    si->event_queue = xine_event_new_queue (si->stream);
    xine_event_create_listener_thread (si->event_queue, event_listener, si);
    if (si->mrl == "cdda:/") {
        int nr;
        char ** mrls = xine_get_autoplay_mrls (xine, "CD", &nr);
        si->running = 1;
        for (int i = 0; i < nr; i++) {
            QString m (mrls[i]);
            QString title;
            if (xine_open (si->stream, mrls[i])) {
                const char * t = xine_get_meta_info (si->stream, XINE_META_INFO_TITLE);
                if (t && t[0])
                    title = QString::fromUtf8 (t);
                xine_close (si->stream);
            }
            if (callback)
                callback->subMrl (0, m, title);
            else
                printf ("track %s\n", m.utf8 ().data ());
        }
        finished (si->wid);
        return;
    }

    xine_gui_send_vo_data(si->stream, XINE_GUI_SEND_VIDEOWIN_VISIBLE, (void *) 1);

    si->running = true;
    QString mrlsetup = si->mrl;
    if (!si->rec_mrl.isEmpty ()) {
        char * rm = strdup (si->rec_mrl.local8Bit ());
        char *bn = basename (rm);
        char *dn = dirname (rm);
        if (bn)
            updateConfigEntry (QString ("media.capture.save_dir"), QString::fromLocal8Bit (dn));
        mrlsetup += QString ("#save:") + QString::fromLocal8Bit (bn);
        free (rm);
    }
    if (!xine_open (si->stream, (const char *) mrlsetup.local8Bit ())) {
        fprintf(stderr, "Unable to open mrl '%s'\n", (const char *) si->mrl.local8Bit ());
        finished (si->wid);
        return;
    }
    xine_set_param (si->stream, XINE_PARAM_VO_SATURATION, si->movie_saturation);
    xine_set_param (si->stream, XINE_PARAM_VO_BRIGHTNESS, si->movie_brightness);
    xine_set_param (si->stream, XINE_PARAM_VO_CONTRAST, si->movie_contrast);
    xine_set_param (si->stream, XINE_PARAM_VO_HUE, si->movie_hue);

    if (!si->sub_mrl.isEmpty ()) {
        fprintf(stderr, "Using subtitles from '%s'\n", (const char *) si->sub_mrl.local8Bit ());
        si->sub_stream = xine_stream_new (xine, NULL, si->vo_port);
        if (xine_open (si->sub_stream, (const char *) si->sub_mrl.local8Bit ())) {
            xine_stream_master_slave (si->stream, si->sub_stream,
                    XINE_MASTER_SLAVE_PLAY | XINE_MASTER_SLAVE_STOP);
        } else {
            fprintf(stderr, "Unable to open subtitles from '%s'\n", (const char *) si->sub_mrl.local8Bit ());
            xine_dispose (si->sub_stream);
            si->sub_stream = 0L;
        }
    }
    if (!xine_play (si->stream, 0, 0)) {
        fprintf(stderr, "Unable to play mrl '%s'\n", (const char *) si->mrl.local8Bit ());
        finished (si->wid);
        return;
    }
    if (xine_get_stream_info (si->stream, XINE_STREAM_INFO_HAS_VIDEO))
        QApplication::postEvent(xineapp, new XineEvent(event_video, wid));
    else if (StreamInfo::AutoAudioVis == si->audio_vis)
        si->audio_vis = xine_config_lookup_entry
            (xine, "audio.visualization", &audio_vis_cfg_entry)
                ? StreamInfo::AudioVis
                : StreamInfo::NoAudioVis;
    if (StreamInfo::AudioVis == si->audio_vis)
        xine_config_cb (si, &audio_vis_cfg_entry);
    if (callback)
        si->firstframe = true;
}

void KXinePlayer::play () {
    fprintf(stderr, "play standalone\n");
    stream_list_mutex.lock ();
    StreamInfoMap::iterator i = stream_info_map.begin ();
    if (i != stream_info_map.end ())
        play (i.key (), 0);
    stream_list_mutex.unlock ();
}

void KXinePlayer::stop (unsigned long wid) {
    StreamInfo *si = getStreamInfo (wid, "stop");
    if (!si)
        return;
    fprintf(stderr, "stop\n");
    fflush(stderr);
    si->repeat_count = 0;
    if (si->sub_stream)
        xine_stop (si->sub_stream);
    xine_stop (si->stream);
    QApplication::postEvent (xineapp, new XineEvent (event_finished, si->wid));
}

void KXinePlayer::pause (unsigned long wid) {
    StreamInfo *si = getStreamInfo (wid, "pause");
    if (!si || !si->running)
        return;
    if (xine_get_status (si->stream) == XINE_STATUS_PLAY) {
        if (xine_get_param (si->stream, XINE_PARAM_SPEED) == XINE_SPEED_PAUSE)
            xine_set_param (si->stream, XINE_PARAM_SPEED, XINE_SPEED_NORMAL);
        else
            xine_set_param (si->stream, XINE_PARAM_SPEED, XINE_SPEED_PAUSE);
    }
}

void KXinePlayer::finished (unsigned long wid) {
    stop (wid);
}

void KXinePlayer::setAudioLang (unsigned long wid, int id, const QString & al) {
    StreamInfo *si = getStreamInfo (wid, "set audio lang");
    if (!si)
        return;
    si->alang = al;
    if (xine_get_status (si->stream) == XINE_STATUS_PLAY)
        xine_set_param(si->stream, XINE_PARAM_AUDIO_CHANNEL_LOGICAL, id);
}

void KXinePlayer::setSubtitle (unsigned long wid, int id, const QString & sl) {
    StreamInfo *si = getStreamInfo (wid, "set subtitle");
    if (!si)
        return;
    si->slang = sl;
    if (xine_get_status (si->stream) == XINE_STATUS_PLAY)
        xine_set_param (si->stream, XINE_PARAM_SPU_CHANNEL, id);
}

void KXinePlayer::updatePosition () {
    /*if (!running || !callback) return;
    int pos;
    mutex->lock ();
    xine_get_pos_length (stream, 0, &pos, &movie_length);
    mutex->unlock ();
    if (movie_pos != pos) {
        movie_pos = pos;
        callback->moviePosition (0, pos/100);
    }
    QTimer::singleShot (500, this, SLOT (updatePosition ()));*/
}

void KXinePlayer::saturation (unsigned long wid, int val) {
    StreamInfo *si = getStreamInfo (wid, "saturation");
    if (!si)
        return;
    si->movie_saturation = val;
    if (si->running) {
        xine_set_param (si->stream, XINE_PARAM_VO_SATURATION, val);
    }
}

void KXinePlayer::hue (unsigned long wid, int val) {
    StreamInfo *si = getStreamInfo (wid, "hue");
    if (!si)
        return;
    si->movie_hue = val;
    if (si->running) {
        xine_set_param (si->stream, XINE_PARAM_VO_HUE, val);
    }
}

void KXinePlayer::contrast (unsigned long wid, int val) {
    StreamInfo *si = getStreamInfo (wid, "contrast");
    if (!si)
        return;
    si->movie_contrast = val;
    if (si->running) {
        xine_set_param (si->stream, XINE_PARAM_VO_CONTRAST, val);
    }
}

void KXinePlayer::brightness (unsigned long wid, int val) {
    StreamInfo *si = getStreamInfo (wid, "brightness");
    if (!si)
        return;
    si->movie_brightness = val;
    if (si->running) {
        xine_set_param (si->stream, XINE_PARAM_VO_BRIGHTNESS, val);
    }
}

void KXinePlayer::volume (unsigned long wid, int val) {
    StreamInfo *si = getStreamInfo (wid, "valume");
    if (!si)
        return;
    si->movie_volume = val;
    if (si->running) {
        xine_set_param( si->stream, XINE_PARAM_AUDIO_VOLUME, val);
    }
}

void KXinePlayer::seek (unsigned long wid, int val) {
    StreamInfo *si = getStreamInfo (wid, "seek");
    if (!si)
        return;
    if (si->running) {
        fprintf(stderr, "seek %d\n", val);
        if (!xine_play (si->stream, 0, val * 100)) {
            fprintf(stderr, "Unable to seek to %d :-(\n", val);
        }
    }
}

bool KXinePlayer::event (QEvent * e) {
    switch (e->type()) {
        case event_finished: {
            XineEvent *fe = static_cast <XineEvent *> (e);
            StreamInfo *si = getStreamInfo (fe->wid, "finished event");
            if (!si)
                return true;
            fprintf (stderr, "event_finished\n");
            si->finished = true;
            if (StreamInfo::AudioVis == si->audio_vis)
                xine_config_cb (si, &audio_vis_cfg_entry);
            if (si->sub_stream)
                xine_dispose (si->sub_stream);
            if (si->stream) {
                xine_event_dispose_queue (si->event_queue);
                xine_dispose (si->stream);
            }
            if (si->ao_port)
                xine_close_audio_driver (xine, si->ao_port);
            if (si->vo_port)
                xine_close_video_driver (xine, si->vo_port);
            //XLockDisplay (display);
            //XClearWindow (display, wid);
            //XUnlockDisplay (display);
            if (callback)
                callback->finished (si->wid);
            else
                QTimer::singleShot (0, this, SLOT (quit ()));
            stream_list_mutex.lock ();
            stream_info_map.remove (si->wid);
            stream_list_mutex.unlock ();
            delete si;
            break;
        }
        case event_size: {
            if (callback) {
                XineMovieParamEvent *se = static_cast <XineMovieParamEvent*>(e);
                if (se->length < 0)
                    se->length = 0;
                callback->movieParams (se->wid, se->length/100,
                        se->width, se->height,
                        se->height ? 1.0*se->width/se->height : 1.0,
                        se->alang, se->slang);
                if (se->first_frame) {
                    callback->playing (se->wid);
                    QTimer::singleShot (500, this, SLOT (updatePosition ()));
                }
            }
            break;
        }
        case event_progress: {
            XineProgressEvent * pe = static_cast <XineProgressEvent *> (e);
            if (callback)
                callback->loadingProgress (pe->wid, pe->progress);
            break;
        }
        case event_url: {
            XineURLEvent * ue = static_cast <XineURLEvent *> (e);
            if (callback)
                callback->subMrl (ue->wid, ue->url, QString ());
            break;
        }
        case event_title: {
            XineTitleEvent * ue = static_cast <XineTitleEvent *> (e);
            if (callback)
                callback->statusMessage (ue->wid,
                        (int) KMPlayer::Callback::stat_newtitle, ue->title);
            break;
        }
        case event_video: {
            XineEvent *ve = static_cast <XineEvent *> (e);
            if (callback)
                callback->statusMessage (ve->wid, (int) KMPlayer::Callback::stat_hasvideo, QString ());
            break;
        }
        default:
            return false;
    }
    return true;
}

void KXinePlayer::saveState (QSessionManager & sm) {
    if (callback)
        sm.setRestartHint (QSessionManager::RestartNever);
}

XineMovieParamEvent::XineMovieParamEvent (unsigned long id,
        int l, int w, int h,
        const QStringList &a, const QStringList &s, bool ff)
  : QEvent ((QEvent::Type) event_size),
    length (l), width (w), height (h),
    alang (a), slang (s),
    first_frame (ff),
    wid (id)
{}

XineURLEvent::XineURLEvent (unsigned long id, const QString & u)
  : QEvent ((QEvent::Type) event_url), url (u), wid (id)
{}

XineTitleEvent::XineTitleEvent (unsigned long id, const char * t)
  : QEvent ((QEvent::Type) event_title), title (QString::fromUtf8 (t)), wid (id)
{
    QUrl::decode (title);
}

XineProgressEvent::XineProgressEvent (unsigned long id, const int p)
  : QEvent ((QEvent::Type) event_progress), progress (p), wid (id)
{}

XineEvent::XineEvent (int event, unsigned long id)
  : QEvent ((QEvent::Type) event), wid (id)
{}

//static bool translateCoordinates (int wx, int wy, int mx, int my) {
//    movie_width
class XEventThread : public QThread {
protected:
    void run () {
        Time prev_click_time = 0;
        int prev_click_x = 0;
        int prev_click_y = 0;
        while (true) {
            XEvent   xevent;
            XNextEvent(display, &xevent);
            switch (xevent.type) {
                case ClientMessage:
                    if (xevent.xclient.message_type == quit_atom) {
                        fprintf(stderr, "request quit\n");
                        return;
                    }
                    break;
                case KeyPress: {
                    XKeyEvent  kevent;
                    KeySym     ksym;
                    StreamInfo *si = getStreamInfo (xevent.xany.window, "key event");
                    char       kbuf[256];
                    int        len;

                    if (!si)
                        break;
                    kevent = xevent.xkey;

                    XLockDisplay (display);
                    len = XLookupString(&kevent, kbuf, sizeof(kbuf), &ksym, NULL);
                    XUnlockDisplay (display);
                    fprintf(stderr, "keypressed 0x%x 0x%x\n", kevent.keycode, ksym);

                    switch (ksym) {

                        case XK_q:
                        case XK_Q:
                            xineapp->lock ();
                            xineapp->quit ();
                            xineapp->unlock ();
                            break;

                        case XK_p: // previous
                            qApp->lock ();
                            if (si->stream) {
                                xine_event_t xine_event =  {
                                    XINE_EVENT_INPUT_PREVIOUS,
                                    si->stream, 0L, 0, { 0, 0 }
                                };
                                xine_event_send (si->stream, &xine_event);
                            }
                            qApp->unlock ();
                            break;

                        case XK_n: // next
                            qApp->lock ();
                            if (si->stream) {
                                xine_event_t xine_event =  {
                                    XINE_EVENT_INPUT_NEXT,
                                    si->stream, 0L, 0, { 0, 0 }
                                };
                                xine_event_send (si->stream, &xine_event);
                            }
                            qApp->unlock ();
                            break;

                        case XK_u: // up menu
                            qApp->lock ();
                            if (si->stream) {
                                xine_event_t xine_event =  {
                                    XINE_EVENT_INPUT_MENU1,
                                    si->stream, 0L, 0, { 0, 0 }
                                };
                                xine_event_send (si->stream, &xine_event);
                            }
                            qApp->unlock ();
                            break;

                        case XK_r: // root menu
                            qApp->lock ();
                            if (si->stream) {
                                xine_event_t xine_event =  {
                                    XINE_EVENT_INPUT_MENU3,
                                    si->stream, 0L, 0, { 0, 0 }
                                };
                                xine_event_send (si->stream, &xine_event);
                            }
                            qApp->unlock ();
                            break;

                        case XK_Up:
                            xine_set_param(si->stream, XINE_PARAM_AUDIO_VOLUME,
                                    (xine_get_param (si->stream,
                                                 XINE_PARAM_AUDIO_VOLUME) + 1));
                            break;

                        case XK_Down:
                            xine_set_param(si->stream, XINE_PARAM_AUDIO_VOLUME,
                                    (xine_get_param (si->stream,
                                                 XINE_PARAM_AUDIO_VOLUME) - 1));
                            break;

                        case XK_plus:
                            xine_set_param(si->stream,
                                    XINE_PARAM_AUDIO_CHANNEL_LOGICAL,
                                    (xine_get_param (si->stream,
                                        XINE_PARAM_AUDIO_CHANNEL_LOGICAL) + 1));
                            break;

                        case XK_minus:
                            xine_set_param(si->stream,
                                    XINE_PARAM_AUDIO_CHANNEL_LOGICAL,
                                    (xine_get_param (si->stream,
                                        XINE_PARAM_AUDIO_CHANNEL_LOGICAL) - 1));
                            break;

                        case XK_space:
                            if(xine_get_param (si->stream
                                        , XINE_PARAM_SPEED) != XINE_SPEED_PAUSE)
                                xine_set_param (si->stream,
                                        XINE_PARAM_SPEED, XINE_SPEED_PAUSE);
                            else
                                xine_set_param (si->stream,
                                        XINE_PARAM_SPEED, XINE_SPEED_NORMAL);
                            break;

                    }
                    break;
                }

                case Expose: {
                    StreamInfo *si = getStreamInfo (xevent.xany.window, "expose");
                    if (!si || xevent.xexpose.count != 0 || !si->stream)
                        break;
                    qApp->lock ();
                    xine_gui_send_vo_data (si->stream,
                            XINE_GUI_SEND_EXPOSE_EVENT, &xevent);
                    qApp->unlock ();
                    break;
                }
                case ConfigureNotify: {
                    StreamInfo *si = getStreamInfo (xevent.xany.window, "expose");
                    Window           tmp_win;

                    if (!si)
                        break;
                    si->width  = xevent.xconfigure.width;
                    si->height = xevent.xconfigure.height;
                    if((xevent.xconfigure.x == 0) && (xevent.xconfigure.y == 0)) {
                        XLockDisplay(display);
                        XTranslateCoordinates(display, xevent.xconfigure.window,
                                DefaultRootWindow(xevent.xconfigure.display),
                                0, 0, &si->xpos, &si->ypos, &tmp_win);
                        XUnlockDisplay(display);
                    } else {
                        si->xpos = xevent.xconfigure.x;
                        si->ypos = xevent.xconfigure.y;
                    }
                    break;
                }
                case MotionNotify: {
                    StreamInfo *si = getStreamInfo (xevent.xany.window, "motion");
                    xine_input_data_t data;
                    XMotionEvent *mev = (XMotionEvent *) &xevent;

                    if (!si)
                        break;

                    x11_rectangle_t rect = { mev->x, mev->y, 0, 0 };
                    if (xine_gui_send_vo_data (si->stream,
                                XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO,
                                (void*) &rect) == -1)
                            break;
                    data.x = rect.x;
                    data.y = rect.y;
                    data.button = 0;
                    xine_event_t xine_event = {
                        XINE_EVENT_INPUT_MOUSE_MOVE,
                        si->stream, &data, sizeof (xine_input_data_t),
                        { 0 , 0 }
                    };
                    qApp->lock ();
                    xine_event_send (si->stream, &xine_event);
                    qApp->unlock ();
                    break;
                }
                case ButtonPress: {
                    StreamInfo *si = getStreamInfo (xevent.xany.window, "buttonpress");
                    XButtonEvent *bev = (XButtonEvent *) &xevent;
                    int dx = prev_click_x - bev->x;
                    int dy = prev_click_y - bev->y;

                    if (!si)
                        break;

                    if (bev->time - prev_click_time < 400 &&
                            (dx * dx + dy * dy) < 25) {
                        xineapp->lock ();
                        if (callback)
                            callback->toggleFullScreen (si->wid);
                        xineapp->unlock ();
                    }
                    prev_click_time = bev->time;
                    prev_click_x = bev->x;
                    prev_click_y = bev->y;
                    fprintf(stderr, "ButtonPress\n");
                    x11_rectangle_t rect = { bev->x, bev->y, 0, 0 };
                    if (xine_gui_send_vo_data (si->stream,
                                XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO,
                                (void*) &rect) == -1)
                            break;
                    xine_input_data_t data;
                    data.x = rect.x;
                    data.y = rect.y;
                    data.button = 1;
                    xine_event_t xine_event =  {
                        XINE_EVENT_INPUT_MOUSE_BUTTON,
                        si->stream, &data, sizeof (xine_input_data_t),
                        { 0, 0 }
                    };
                    qApp->lock ();
                    xine_event_send (si->stream, &xine_event);
                    qApp->unlock ();
                    break;
                }
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

            if (xevent.type == completion_event) {
                StreamInfo *si = getStreamInfo (xevent.xany.window, "completion");
                if (si)
                    xine_gui_send_vo_data (si->stream,
                            XINE_GUI_SEND_COMPLETION_EVENT, &xevent);
            }
        }
    }
};

int main(int argc, char **argv) {
    const char *dvd_device = 0L;
    const char *vcd_device = 0L;
    const char *grab_device = 0L;
    QString mrl, sub_mrl, rec_mrl;
    int repeat_count = 0;
    Window wid = 0;

    if (!XInitThreads ()) {
        fprintf (stderr, "XInitThreads () failed\n");
        return 1;
    }
    display = XOpenDisplay(NULL);
    screen  = XDefaultScreen(display);
    quit_atom = XInternAtom (display, "kxineplayer_quit", false);

    snprintf(configfile, sizeof (configfile), "%s%s", xine_get_homedir(), "/.xine/config2");
    xineapp = new KXinePlayer (argc, argv);
    window_created = true;
    for (int i = 1; i < argc; i++) {
        if (!strcmp (argv [i], "-vo") && ++i < argc) {
            vo_driver = argv [i];
        } else if (!strcmp (argv [i], "-ao") && ++i < argc) {
            ao_driver = argv [i];
        } else if (!strcmp (argv [i], "-dvd-device") && ++i < argc) {
            dvd_device = argv [i];
        } else if (!strcmp (argv [i], "-vcd-device") && ++i < argc) {
            vcd_device = argv [i];
        } else if (!strcmp (argv [i], "-vd") && ++i < argc) {
            grab_device = argv [i];
        } else if ((!strcmp (argv [i], "-wid") ||
                    !strcmp (argv [i], "-window-id")) && ++i < argc) {
            wid = atol (argv [i]);
            window_created = false;
        } else if (!strcmp (argv [i], "-root")) {
            wid =  XDefaultRootWindow (display);
            window_created = false;
        } else if (!strcmp (argv [i], "-window")) {
            ;
        } else if (!strcmp (argv [i], "-sub") && ++i < argc) {
            sub_mrl = QString (argv [i]);
        } else if (!strcmp (argv [i], "-lang") && ++i < argc) {
            slang = alang = QString (argv [i]);
        } else if (!strcmp (argv [i], "-v")) {
            xine_verbose = true;
        } else if (!strcmp (argv [i], "-vv")) {
            xine_verbose = xine_vverbose = true;
        } else if (!strcmp (argv [i], "-c")) {
            wants_config = true;
        } else if (!strcmp (argv [i], "-f") && ++i < argc) {
            strncpy (configfile, argv [i], sizeof (configfile));
            configfile[sizeof (configfile) - 1] = 0;
        } else if (!strcmp (argv [i], "-cb") && ++i < argc) {
            QString str = argv [i];
            int pos = str.find ('/');
            if (pos > -1) {
                fprintf (stderr, "callback is %s %s\n", str.left (pos).ascii (), str.mid (pos + 1).ascii ());
                callback = new KMPlayer::Callback_stub 
                    (str.left (pos).ascii (), str.mid (pos + 1).ascii ());
            }
        } else if (!strcmp (argv [i], "-rec") && i < argc - 1) {
            rec_mrl = QString::fromLocal8Bit (argv [++i]);
        } else if (!strcmp (argv [i], "-loop") && i < argc - 1) {
            repeat_count = atol (argv [++i]);
        } else {
            if (mrl.startsWith ("-session")) {
                delete xineapp;
                return 1;
            }
            mrl = QString::fromLocal8Bit (argv [i]);
        }
    }
    bool config_changed = !QFile (configfile).exists ();

    if (!callback && mrl.isEmpty ()) {
        fprintf (stderr, "usage: %s [-vo (xv|xshm)] [-ao (arts|esd|..)] "
                "[-f <xine config file>] [-dvd-device <device>] "
                "[-vcd-device <device>] [-vd <video device>] "
                "[-wid <X11 Window>|-window-id <X11 Window>|-root] "
                "[-sub <subtitle url>] [-lang <lang>] [(-v|-vv)] "
                "[-cb <DCOP callback name> [-c]] "
                "[-loop <repeat>] [<url>]\n", argv[0]);
        delete xineapp;
        return 1;
    }

    XEventThread * eventThread = new XEventThread;
    eventThread->start ();

    DCOPClient dcopclient;
    dcopclient.registerAs ("kxineplayer");
    Backend player;

    if (!wid && !callback) {
        XLockDisplay(display);
        wid = XCreateSimpleWindow(display, XDefaultRootWindow(display),
                0, 0, 320, 240, 1, 0, 0);
        XMapRaised(display, wid);
        XSync(display, False);
        XUnlockDisplay(display);
    }

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
    if (grab_device)
        config_changed |= updateConfigEntry (QString ("media.video4linux.video_device"), QString (grab_device));

    if (config_changed)
        xine_config_save (xine, configfile);

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
    if (callback) {
        callback->started (dcopclient.appId (), buf);
    } else {
        StreamInfo *si = getStreamInfo (wid);
        si->mrl = mrl;
        si->sub_mrl = sub_mrl;
        si->rec_mrl = rec_mrl;
        si->repeat_count = repeat_count;
        QTimer::singleShot (10, xineapp, SLOT (play ()));
    }

    xineapp->exec ();

    /*if (sub_stream)
        xine_dispose (sub_stream);
    if (stream) {
        xine_event_dispose_queue (event_queue);
        xine_dispose (stream);
    }
        */
    XLockDisplay(display);
    XEvent ev;
    ev.xclient.type = ClientMessage;
    ev.xclient.serial = 0;
    ev.xclient.send_event = true;
    ev.xclient.display = display;
    ev.xclient.window = wid;
    ev.xclient.message_type = quit_atom;
    ev.xclient.format = 8;
    ev.xclient.data.b[0] = 0;
    XSendEvent (display, wid, false, StructureNotifyMask, &ev);
    XFlush (display);
    XUnlockDisplay(display);
    eventThread->wait (500);
    delete eventThread;

    //xineapp->stop ();
    delete xineapp;

    xine_exit (xine);

    fprintf (stderr, "closing display\n");
    XCloseDisplay (display);
    fprintf (stderr, "done\n");
    return 0;
}

#include "xineplayer.moc"
