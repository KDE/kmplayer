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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <config.h>
#include <dcopclient.h>
#include <qcstring.h>
#include <qtimer.h>
#include <qthread.h>
#include <qmutex.h>
#include <qdom.h>
#include "kmplayer_backend.h"
#include "kmplayer_callback_stub.h"
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

class KXinePlayerPrivate {
public:
    KXinePlayerPrivate()
       : vo_driver ("auto"),
         ao_driver ("auto"),
         brightness (0), contrast (0), hue (0), saturation (0), volume (0) {
    }
    char * vo_driver;
    char * ao_driver;
    int brightness, contrast, hue, saturation, volume;
};

static KXinePlayer * xineapp;
static KMPlayerCallback_stub * callback;
static QMutex mutex (true);

static xine_t              *xine;
static xine_stream_t       *stream;
static xine_stream_t       *sub_stream;
static xine_video_port_t   *vo_port;
static xine_audio_port_t   *ao_port;
static xine_event_queue_t  *event_queue;
static x11_visual_t         vis;
static char                *dvd_device;
static char                *vcd_device;
static char                 configfile[2048];

static Display             *display;
static Window               wid;
static bool                 window_created;
static bool                 xine_verbose;
static bool                 wants_config;
static int                  screen;
static int                  completion_event;
static int                  xpos, ypos, width, height;
static int                  movie_width, movie_height, movie_length;
static double               pixel_aspect;

static int                  running = 0;
static int                  firstframe = 0;

static QString mrl;
static QString sub_mrl;

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
        fprintf(stderr, "first frame\n");
        if (callback) {
            xineapp->lock ();
            xineapp->updatePosition ();
            xineapp->unlock ();
        }
        firstframe = 0;
    }

    *dest_x            = 0;
    *dest_y            = 0;
    *win_x             = xpos;
    *win_y             = ypos;
    *dest_width        = width;
    *dest_height       = height;
    *dest_pixel_aspect = pixel_aspect;
}

static void event_listener(void * /*user_data*/, const xine_event_t *event) {
    if (event->stream != stream)
        return; // not interested in sub_stream events
    switch(event->type) { 
        case XINE_EVENT_UI_PLAYBACK_FINISHED:
            fprintf (stderr, "XINE_EVENT_UI_PLAYBACK_FINISHED\n");
            xineapp->lock ();
            xineapp->finished ();
            xineapp->unlock ();
            break;

        case XINE_EVENT_PROGRESS:
            {
                xine_progress_data_t *pevent = (xine_progress_data_t *) event->data;
                if (callback) {
                    xineapp->lock ();
                    callback->loadingProgress ((int) pevent->percent);
                    xineapp->unlock ();
                } else
                    fprintf(stderr, "%s [%d%%]\n", pevent->description, pevent->percent);
            }
            break;
        case XINE_EVENT_MRL_REFERENCE:
            fprintf(stderr, "XINE_EVENT_MRL_REFERENCE %s\n", 
            ((xine_mrl_reference_data_t*)event->data)->mrl);
            if (callback) {
                xineapp->lock ();
                callback->setURL (QString (((xine_mrl_reference_data_t*)event->data)->mrl));
                xineapp->unlock ();
            }
            break;
        case XINE_EVENT_FRAME_FORMAT_CHANGE:
            fprintf (stderr, "XINE_EVENT_FRAME_FORMAT_CHANGE\n");
            break;
        case XINE_EVENT_UI_SET_TITLE:
            {
                xine_ui_data_t * data = (xine_ui_data_t *) event->data;
                fprintf (stderr, "Set title event %s\n", data->str);
            }
            break;
        case XINE_EVENT_UI_CHANNELS_CHANGED: {
            fprintf (stderr, "Channel changed event\n");
            mutex.lock ();
            int w = xine_get_stream_info(stream, XINE_STREAM_INFO_VIDEO_WIDTH);
            int h = xine_get_stream_info(stream, XINE_STREAM_INFO_VIDEO_HEIGHT);
            int pos, l;
            xine_get_pos_length (stream, 0, &pos, &l);
            mutex.unlock ();
            if (w > 0 && h > 0 &&
                (l != movie_length || w != movie_width || h != movie_height)) {
                movie_width = w;
                movie_height = h;
                movie_length = l;
                if (callback) {
                    xineapp->lock ();
                    callback->movieParams (l/100, w, h, 1.0*w/h);
                    xineapp->unlock ();
                } else if (window_created) {
                    XLockDisplay (display);
                    XResizeWindow (display, wid, movie_width, movie_height);
                    XFlush (display);
                    XUnlockDisplay (display);
                }
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

KMPlayerBackend::KMPlayerBackend ()
    : DCOPObject (QCString ("KMPlayerBackend")) {
}

KMPlayerBackend::~KMPlayerBackend () {}

void KMPlayerBackend::setURL (QString url) {
    mrl = url;
}

void KMPlayerBackend::setSubTitleURL (QString url) {
    sub_mrl = url;
}

void KMPlayerBackend::play () {
    xineapp->play ();
}

void KMPlayerBackend::stop () {
    xineapp->stop ();
}

void KMPlayerBackend::pause () {
    xineapp->pause ();
}

void KMPlayerBackend::seek (int pos, bool /*absolute*/) {
    xineapp->seek (pos);
}

void KMPlayerBackend::hue (int h, bool) {
    xineapp->hue (h);
}

void KMPlayerBackend::saturation (int s, bool) {
    xineapp->saturation (s);
}

void KMPlayerBackend::contrast (int c, bool) {
    xineapp->contrast (c);
}

void KMPlayerBackend::brightness (int b, bool) {
    xineapp->brightness (b);
}

void KMPlayerBackend::volume (int v, bool) {
    xineapp->volume (v);
}

void KMPlayerBackend::quit () {
    delete callback;
    callback = 0L;
    xineapp->stop ();
    QTimer::singleShot (10, qApp, SLOT (quit ()));
}

bool updateConfigEntry (const QString & name, const QString & value) {
    fprintf (stderr, "%s=%s\n", name.ascii (), value.ascii ());
    xine_cfg_entry_t cfg_entry;
    if (!xine_config_lookup_entry (xine, name.ascii (), &cfg_entry))
        return false;
    if (cfg_entry.type == XINE_CONFIG_TYPE_STRING ||
            cfg_entry.type == XINE_CONFIG_TYPE_UNKNOWN)
        cfg_entry.str_value = (char *) value.ascii ();
    else
        cfg_entry.num_value = value.toInt ();
    xine_config_update_entry (xine,  &cfg_entry);
    return true;
}

void KMPlayerBackend::setConfig (QByteArray data) {
    QString err;
    QString attvalue ("VALUE");
    QString attname ("NAME");
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
    d = new KXinePlayerPrivate;
    for(int i = 1; i < argc (); i++) {
        if (!strcmp (argv ()[i], "-vo")) {
            d->vo_driver = argv ()[++i];
        } else if (!strcmp (argv ()[i], "-ao")) {
            d->ao_driver = argv ()[++i];
        } else if (!strcmp (argv ()[i], "-dvd-device")) {
            dvd_device = argv ()[++i];
        } else if (!strcmp (argv ()[i], "-vcd-device")) {
            vcd_device = argv ()[++i];
        } else if (!strcmp (argv ()[i], "-wid")) {
            wid = atol (argv ()[++i]);
        } else if (!strcmp (argv ()[i], "-sub")) {
            sub_mrl = QString (argv ()[++i]);
        } else if (!strcmp (argv ()[i], "-v")) {
            xine_verbose = true;
        } else if (!strcmp (argv ()[i], "-c")) {
            wants_config = true;
        } else if (!strcmp (argv ()[i], "-cb")) {
            QString str = argv ()[++i];
            int pos = str.find ('/');
            if (pos > -1) {
                fprintf (stderr, "callback is %s %s\n", str.left (pos).ascii (), str.mid (pos + 1).ascii ());
                callback = new KMPlayerCallback_stub 
                    (str.left (pos).ascii (), str.mid (pos + 1).ascii ());
            }
        } else 
            mrl = QString (argv ()[i]);
    }
    fprintf(stderr, "mrl: '%s'\n", mrl.ascii ());
    xpos    = 0;
    ypos    = 0;
    width   = 320;
    height  = 200;

    if (!wid) {
        wid = XCreateSimpleWindow(display, XDefaultRootWindow(display),
                xpos, ypos, width, height, 1, 0, 0);
        window_created = true;
        QTimer::singleShot (10, this, SLOT (play ()));
    }
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

    vo_port = xine_open_video_driver(xine, d->vo_driver, XINE_VISUAL_TYPE_X11, (void *) &vis);

    ao_port = xine_open_audio_driver (xine, d->ao_driver, NULL);
}

KXinePlayer::~KXinePlayer () {
    if (xine) {
        mutex.lock ();
        xine_close_audio_driver (xine, ao_port);  
        xine_close_video_driver (xine, vo_port);  
        mutex.unlock ();
    }
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
    int pos = 0;
    int size = 1024;
    buf.resize (size);
    pos += snprintf (buf.data () + pos, size - pos, "<document>\n");
    for (int i = xine_config_get_first_entry (xine, &entry); i; i = xine_config_get_next_entry (xine, &entry)) {
        pos += snprintf (buf.data () + pos, size - pos, "<entry NAME=\"%s\" TYPE=", entry.key);
        switch (entry.type) {
            case XINE_CONFIG_TYPE_STRING:
            case XINE_CONFIG_TYPE_UNKNOWN:
                pos += snprintf (buf.data () + pos, size - pos, "\"string\" VALUE=\"%s\"/>", entry.str_value);
                break;
            case XINE_CONFIG_TYPE_RANGE:
                pos += snprintf (buf.data () + pos, size - pos, "\"range\" START=\"%d\" END=\"%d\" VALUE=\"%d\"/>", entry.range_min, entry.range_max, entry.num_value);
                break;
            case XINE_CONFIG_TYPE_ENUM:
                pos += snprintf (buf.data () + pos, size - pos, "\"enum\" VALUE=\"%d\" DEFAULT=\"%d\">", entry.num_value, entry.num_default);
                for (int i = 0; entry.enum_values[i]; i++)
                    pos += snprintf (buf.data () + pos, size - pos, "<item VALUE=\"%s\"/>", entry.enum_values[i]);
                pos += snprintf (buf.data () + pos, size - pos, "</entry>");
                break;
            case XINE_CONFIG_TYPE_NUM:
                pos += snprintf (buf.data () + pos, size - pos, "\"num\" VALUE=\"%d\" DEFAULT=\"%d\"/>", entry.num_value, entry.num_default);
                break;
            case XINE_CONFIG_TYPE_BOOL:
                pos += snprintf (buf.data () + pos, size - pos, "\"bool\" VALUE=\"%d\" DEFAULT=\"%d\"/>", entry.num_value, entry.num_default);
                break;
        }
        pos += snprintf (buf.data () + pos, size - pos, "\n");
        if (size - pos < 300) {
            size *= 2;
            buf.resize (size);
        }
    }
    pos += snprintf (buf.data () + pos, size - pos, "</document>");
    buf.resize (pos);
}

void KXinePlayer::play () {
    mutex.lock ();
    if (running) {
        if (xine_get_status (stream) == XINE_STATUS_PLAY &&
            xine_get_param (stream, XINE_PARAM_SPEED) == XINE_SPEED_PAUSE)
            xine_set_param( stream, XINE_PARAM_SPEED, XINE_SPEED_NORMAL);
        mutex.unlock ();
        return;
    }
    movie_width = 0;
    movie_height = 0;

    stream = xine_stream_new (xine, ao_port, vo_port);
    event_queue = xine_event_new_queue (stream);
    xine_event_create_listener_thread (event_queue, event_listener, NULL);

    xine_gui_send_vo_data(stream, XINE_GUI_SEND_VIDEOWIN_VISIBLE, (void *) 1);

    if (d->saturation)
        xine_set_param( stream, XINE_PARAM_VO_SATURATION, d->saturation);
    if (d->brightness)
        xine_set_param (stream, XINE_PARAM_VO_BRIGHTNESS, d->brightness);
    if (d->contrast)
        xine_set_param (stream, XINE_PARAM_VO_CONTRAST, d->contrast);
    if (d->hue)
        xine_set_param (stream, XINE_PARAM_VO_HUE, d->hue);
    running = 1;
    if (!xine_open (stream, mrl.local8Bit ())) {
        fprintf(stderr, "Unable to open mrl '%s'\n", mrl.ascii ());
        mutex.unlock ();
        finished ();
        return;
    }
    if (!sub_mrl.isEmpty ()) {
        fprintf(stderr, "Using subtitles from '%s'\n", sub_mrl.ascii ());
        sub_stream = xine_stream_new (xine, NULL, vo_port);
        if (xine_open (sub_stream, sub_mrl.local8Bit ())) {
            xine_stream_master_slave (stream, sub_stream,
                    XINE_MASTER_SLAVE_PLAY | XINE_MASTER_SLAVE_STOP);
        } else {
            fprintf(stderr, "Unable to open subtitles from '%s'\n", sub_mrl.ascii ());
            xine_dispose (sub_stream);
            sub_stream = 0L;
        }
    }
    if (!xine_play (stream, 0, 0)) {
        fprintf(stderr, "Unable to play mrl '%s'\n", mrl.ascii ());
        mutex.unlock ();
        finished ();
        return;
    }
    mutex.unlock ();
    if (callback)
        firstframe = 1;
}

void KXinePlayer::stop () {
    if (!running) return;
    fprintf(stderr, "stop\n");
    mutex.lock ();
    if (sub_stream) {
        xine_stop (sub_stream);
        xine_dispose (sub_stream);
        sub_stream = 0L;
    }
    xine_stop (stream);
    running = 0;
    firstframe = 0;
    xine_event_dispose_queue (event_queue);
    xine_dispose (stream);
    stream = 0L;
    mutex.unlock ();
    XLockDisplay (display);
    fprintf (stderr, "painting\n");
    unsigned int u, w, h;
    int x, y;
    Window root;
    XGetGeometry (display, wid, &root, &x, &y, &w, &h, &u, &u);
    XSetForeground (display, DefaultGC (display, screen),
                    BlackPixel (display, screen));
    XFillRectangle (display, wid, DefaultGC (display, screen), x, y, w, h);
    XUnlockDisplay (display);
    if (callback) callback->finished ();
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

void KXinePlayer::updatePosition () {
    if (!running) return;
    int pos;
    mutex.lock ();
    xine_get_pos_length (stream, 0, &pos, &movie_length);
    if (firstframe) {
        movie_width = xine_get_stream_info(stream, XINE_STREAM_INFO_VIDEO_WIDTH);
        movie_height = xine_get_stream_info(stream, XINE_STREAM_INFO_VIDEO_HEIGHT);
    }
    mutex.unlock ();
    if (firstframe) {
        fprintf(stderr, "movieParams %dx%d %d\n", movie_width, movie_height, movie_length/100);
        if (movie_height > 0)
            callback->movieParams (movie_length/100, movie_width, movie_height, 1.0*movie_width/movie_height);
        callback->playing ();
    } else {
        callback->moviePosition (pos/100);
    }
    QTimer::singleShot (500, this, SLOT (updatePosition ()));
}

void KXinePlayer::saturation (int val) {
    d->saturation = val;
    if (running) {
        mutex.lock ();
        xine_set_param (stream, XINE_PARAM_VO_SATURATION, val);
        mutex.unlock ();
    }
}

void KXinePlayer::hue (int val) {
    d->saturation = val;
    if (running) {
        mutex.lock ();
        xine_set_param (stream, XINE_PARAM_VO_HUE, val);
        mutex.unlock ();
    }
}

void KXinePlayer::contrast (int val) {
    d->saturation = val;
    if (running) {
        mutex.lock ();
        xine_set_param (stream, XINE_PARAM_VO_CONTRAST, val);
        mutex.unlock ();
    }
}

void KXinePlayer::brightness (int val) {
    d->saturation = val;
    if (running) {
        mutex.lock ();
        xine_set_param (stream, XINE_PARAM_VO_BRIGHTNESS, val);
        mutex.unlock ();
    }
}

void KXinePlayer::volume (int val) {
    d->saturation = val;
    if (running) {
        mutex.lock ();
        xine_set_param( stream, XINE_PARAM_AUDIO_VOLUME, val);
        mutex.unlock ();
    }
}

void KXinePlayer::seek (int val) {
    if (running) {
        mutex.lock ();
        if (!xine_play (stream, 0, val * 100)) {
            fprintf(stderr, "Unable to seek to %d :-(\n", val);
        }
        mutex.unlock ();
    }
}

//static bool translateCoordinates (int wx, int wy, int mx, int my) {
//    movie_width
class XEventThread : public QThread {
public:
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
                                qApp->lock ();
                                qApp->quit();
                                qApp->unlock ();
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
                    fprintf (stderr, "NoExpose %lu\n", xevent.xnoexpose.drawable);
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

    xineapp = new KXinePlayer (argc, argv);

    if (!callback && mrl.isEmpty ()) {
        fprintf (stderr, "usage: %s [-vo (xv|xshm)] [-ao (arts|esd|..)] [-dvd-device <device>] [-vcd-device <device>] [-wid <X11 Window>] [-sub <subtitle url>] [-v] [-cb <DCOP callback name> [-c]] [<url>]\n", argv[0]);
        delete xineapp;
        return 1;
    }

    DCOPClient dcopclient;
    dcopclient.registerAs ("kxineplayer");
    KMPlayerBackend player;

    XEventThread eventThread;
    eventThread.start ();

    xine = xine_new();
    if (xine_verbose)
        xine_engine_set_param (xine, XINE_ENGINE_PARAM_VERBOSITY, XINE_VERBOSITY_LOG);
    sprintf(configfile, "%s%s", xine_get_homedir(), "/.xine/config2");
    xine_config_load(xine, configfile);
    xine_init(xine);

    xineapp->init ();

    if (dvd_device)
        updateConfigEntry (QString ("input.dvd_device"), QString (dvd_device));
    if (vcd_device)
        updateConfigEntry (QString ("input.vcd_device"), QString (vcd_device));

    if (callback) {
        QByteArray buf;
        if (wants_config)
            getConfigEntries (buf);
        callback->started (buf);
    }
    xineapp->exec ();

    XLockDisplay(display);
    XClientMessageEvent ev = {
        ClientMessage, 0, true, display, wid, 
        XInternAtom (display, "XINE", false), 8, "quit_now"
    };
    XSendEvent (display, wid, FALSE, StructureNotifyMask, (XEvent *) & ev);
    XFlush (display);
    XUnlockDisplay(display);
    eventThread.wait (500);

    xineapp->stop ();
    delete xineapp;

    xine_exit (xine);

    fprintf (stderr, "closing display\n");
    XCloseDisplay (display);
    fprintf (stderr, "done\n");
    return 0;
}

#include "xineplayer.moc"
