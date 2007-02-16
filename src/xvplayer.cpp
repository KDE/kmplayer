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
#include "xvplayer.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>
#include <X11/extensions/Xvlib.h>


static char                 configfile[2048];

static Display             *display;
static KXVideoPlayer       *xvapp;
static KMPlayer::Callback_stub * callback;
static Window               wid;
static GC                   gc;
static bool                 window_created = true;
static bool                 wants_config;
static bool                 verbose;
static bool                 running;
static bool                 have_freq;
static bool                 xv_success;
static bool                 reset_xv_autopaint_colorkey;
static bool                 reset_xv_mute;
static int                  xvport;
static int                  xv_encoding = -1;
static QString              xv_norm;
static int                  xv_frequency;
static int                  screen;
static int                  movie_width;
static int                  movie_height;
static int                  current_volume;
static int                  tmp_volume;
static const int            start_vol_timeout = 100;
static const int            inc_vol_timeout = 20;
Atom xv_enc_atom;
Atom xv_hue_atom;
Atom xv_saturation_atom;
Atom xv_brightness_atom;
Atom xv_contrast_atom;
Atom xv_freq_atom;
Atom xv_volume_atom;
Atom xv_mute_atom;
Atom xv_autopaint_colorkey_atom;
enum {
    limit_volume = 0,
    limit_hue, limit_contrast, limit_brightness, limit_saturation,
    limit_last
};
static struct Limit { int min; int max; } xv_limits [limit_last];
static QString elmentry ("entry");
static QString elmitem ("item");
static QString attname ("NAME");
static QString atttype ("TYPE");
static QString attdefault ("DEFAULT");
static QString attvalue ("VALUE");
//static QString attstart ("START");
//static QString attend ("END");
//static QString valrange ("range");
//static QString valnum ("num");
//static QString valbool ("bool");
//static QString valenum ("enum");
//static QString valstring ("string");
static QString valtree ("tree");
static QByteArray config_buf;

extern "C" {

} // extern "C"

static void putVideo () {
    XWindowAttributes attr;
    XGetWindowAttributes (display, wid, &attr);
    XvPutVideo (display, xvport, wid, gc, 0, 0, attr.width, attr.height, 0, 0, attr.width, attr.height);
}

using namespace KMPlayer;

Backend::Backend ()
    : DCOPObject (QCString ("Backend")) {
}

Backend::~Backend () {}

void Backend::setURL (QString) {
}

void Backend::setSubTitleURL (QString) {
}

void Backend::play (int) {
    xvapp->play ();
}

void Backend::stop () {
    QTimer::singleShot (0, xvapp, SLOT (stop ()));
}

void Backend::pause () {
}

void Backend::seek (int, bool /*absolute*/) {
}

void Backend::hue (int h, bool) {
    if (xv_limits[limit_hue].max > xv_limits[limit_hue].min)
        xvapp->hue ((h + 100) * (xv_limits[limit_hue].max - xv_limits[limit_hue].min)/200 + xv_limits[limit_hue].min);
}

void Backend::saturation (int s, bool) {
    if (xv_limits[limit_saturation].max > xv_limits[limit_saturation].min)
        xvapp->saturation ((s + 100) * (xv_limits[limit_saturation].max - xv_limits[limit_saturation].min)/200 + xv_limits[limit_saturation].min);
}

void Backend::contrast (int c, bool) {
    if (xv_limits[limit_contrast].max > xv_limits[limit_contrast].min)
        xvapp->contrast ((c + 100)*(xv_limits[limit_contrast].max - xv_limits[limit_contrast].min)/200 + xv_limits[limit_contrast].min);
}

void Backend::brightness (int b, bool) {
    if (xv_limits[limit_brightness].max > xv_limits[limit_brightness].min)
        xvapp->brightness ((b + 100)*(xv_limits[limit_brightness].max - xv_limits[limit_brightness].min)/200 + xv_limits[limit_brightness].min);
}

void Backend::volume (int v, bool) {
    if (xv_limits[limit_volume].max > xv_limits[limit_volume].min)
        xvapp->volume (v*(xv_limits[limit_volume].max - xv_limits[limit_volume].min)/100 + xv_limits[limit_volume].min);
}

void Backend::frequency (int f) {
    xvapp->frequency (f);
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
        } else
            err = QString ("invalid data");
    }
    if (callback)
        callback->errorMessage (0, err);
}

bool Backend::isPlaying () {
    return running;
}

KXVideoPlayer::KXVideoPlayer (int _argc, char ** _argv)
  : QApplication (_argc, _argv, false), mute_timer (0) {
}

void KXVideoPlayer::init () {
    int xpos    = 0;
    int ypos    = 0;
    int width   = 320;
    int height  = 200;

    XLockDisplay(display);
    if (window_created)
        wid = XCreateSimpleWindow(display, XDefaultRootWindow(display),
                xpos, ypos, width, height, 1, 0, 0);
    if (!callback)
        QTimer::singleShot (10, this, SLOT (play ()));
    XSelectInput (display, wid,
                  (PointerMotionMask | ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask)); // | SubstructureNotifyMask));
    XvAdaptorInfo * ai;
    unsigned int adaptors;
    xv_success = true;
    QDomDocument doc;
    QDomElement root = doc.createElement (QString ("document"));
    if (XvQueryAdaptors (display, XDefaultRootWindow (display), &adaptors, &ai) == Success) {
        QDomElement elm = doc.createElement (elmentry);
        elm.setAttribute (attname, QString ("XVideo"));
        elm.setAttribute (atttype, valtree);
        for (unsigned i = 0; i < adaptors; i++) {
            if ((ai[i].type & XvInputMask) &&
                    (ai[i].type & XvVideoMask) &&
                    ai[i].base_id > 0) {
                int port = ai[i].base_id;
                fprintf (stderr, "xvport %d\n", port);
                bool freq_found = false;
                XvAttribute *attributes = 0L;
                int nr_attr, cur_val;
                attributes = XvQueryPortAttributes (display, port, &nr_attr);
                if (attributes) {
                    for (int i = 0; i < nr_attr; i++) {
                        if (!strcmp (attributes[i].name, "XV_FREQ"))
                            freq_found = true;
                        Atom atom = XInternAtom (display, attributes[i].name, false);
                        fprintf (stderr, "%s[%d] (%d .. %d)", attributes[i].name, ( int ) atom, attributes[i].min_value, attributes[i].max_value);
                        if ((attributes[i].flags & XvGettable) && XvGetPortAttribute (display, port, atom, &cur_val) == Success)
                            fprintf (stderr, " current: %d", cur_val);
                        fprintf (stderr, "\n");
                    }
                    XFree(attributes);
                }
                if (!xvport && ((xv_frequency > 0) == freq_found)) {
                    fprintf (stderr, "using xvport %d\n", port);
                    xvport = port;
                }
                if (xvport == port)
                    have_freq = freq_found;
                XvEncodingInfo * encodings = 0L;
                unsigned nr_encode;
                XvQueryEncodings (display, port, &nr_encode, &encodings);
                if (encodings) {
                    QDomElement port_item = doc.createElement (QString("Port"));
                    port_item.setAttribute (attvalue, QString::number (port));
                    if (freq_found)
                        port_item.setAttribute (QString("FREQ"), QString("1"));
                    for (unsigned i = 0; i < nr_encode; i++) {
                        if (strcmp (encodings[i].name, "XV_IMAGE")) {
                            if (xvport == port && xv_encoding < 0 && !xv_norm.isEmpty () && QString (encodings[i].name).lower ().startsWith(xv_norm.lower ()))
                                xv_encoding = encodings[i].encoding_id;
                            if (port == xvport && encodings[i].encoding_id == xv_encoding) {
                                movie_width = encodings[i].width;
                                movie_height = encodings[i].height;
                            }
                            QDomElement item = doc.createElement (QString ("Input"));
                            item.setAttribute (attvalue, QString::number (encodings[i].encoding_id));
                            item.setAttribute (attname, QString (encodings[i].name));
                            port_item.appendChild (item);
                            fprintf (stderr, " encoding: %d %s\n", ( int ) encodings[i].encoding_id, encodings[i].name);
                        }
                    }
                    elm.appendChild (port_item);
                    XvFreeEncodingInfo (encodings);
                }
            }
        }
        root.appendChild (elm);
        XvFreeAdaptorInfo(ai);
    }
    doc.appendChild (root);
    QCString exp = doc.toCString ();
    config_buf = exp;
    //fprintf (stderr, "%s\n", (const char *)exp);
    config_buf.resize (exp.length ()); // strip terminating \0

    if (xvport <= 0) {
        fprintf (stderr, "no valid xvport found\n");
        xv_success = false;
        return;
    }
    if (window_created) {
        fprintf (stderr, "map %lu\n", wid);
        if (movie_width > 0 && movie_height > 0)
            XResizeWindow (display, wid, movie_width, movie_height);
        XMapRaised(display, wid);
        XSync(display, False);
    }
    XUnlockDisplay(display);
    if (!xv_success)
        fprintf (stderr, "Failed to init %d port\n", xvport);
}

KXVideoPlayer::~KXVideoPlayer () {
    if (window_created) {
        XLockDisplay (display);
        fprintf (stderr, "unmap %lu\n", wid);
        XUnmapWindow (display,  wid);
        XDestroyWindow(display,  wid);
        XSync (display, False);
        XUnlockDisplay (display);
    }
    xvapp = 0L;
}

void getConfigEntries (QByteArray & buf) {
    QDomDocument doc;
    QDomElement root = doc.createElement (QString ("document"));
    doc.appendChild (root);
    QCString exp = doc.toCString ();
    buf = exp;
    buf.resize (exp.length ()); // strip terminating \0
}

void KXVideoPlayer::play () {
    fprintf (stderr, "play xv://%d:%d/%d\n", xvport, xv_encoding, xv_frequency);
    if (!xv_success)
        return;
    if (callback && movie_width > 0 && movie_height > 0)
        callback->movieParams (0, movie_width, movie_height, 1.0*movie_width/movie_height, QStringList (), QStringList ());
    XLockDisplay (display);
    if (!running && XvGrabPort (display, xvport, CurrentTime) == Success) {
        gc = XCreateGC (display, wid, 0, NULL);
        XvSelectPortNotify (display, xvport, 1);
        XvSelectVideoNotify (display, wid, 1);
        int nr, cur_val;
        if (XvGetPortAttribute (display, xvport, xv_autopaint_colorkey_atom, &cur_val) == Success && cur_val == 0) {
            fprintf (stderr, "XV_AUTOPAINT_COLORKEY is 0\n");
            XvSetPortAttribute (display, xvport, xv_autopaint_colorkey_atom, 1);
            reset_xv_autopaint_colorkey = true;
        }
        XvAttribute *attributes = XvQueryPortAttributes (display, xvport, &nr);
        if (attributes) {
            for (int i = 0; i < nr; i++) {
                Limit * limit = 0;
                Atom atom = XInternAtom (display, attributes[i].name, false);
                if (atom == xv_volume_atom) {
                    limit = xv_limits + limit_volume;
                    XvGetPortAttribute (display, xvport,
                            xv_volume_atom, &current_volume);
                } else if (atom == xv_hue_atom) {
                    limit = xv_limits + limit_hue;
                } else if (atom == xv_saturation_atom) {
                    limit = xv_limits + limit_saturation;
                } else if (atom == xv_brightness_atom) {
                    limit = xv_limits + limit_brightness;
                } else if (atom == xv_contrast_atom) {
                    limit = xv_limits + limit_contrast;
                } else
                    continue;
                limit->min = attributes[i].min_value;
                limit->max = attributes[i].max_value;
            }
            XFree (attributes);
        }
        if (xv_frequency > 0)
            XvSetPortAttribute (display, xvport, xv_freq_atom, int (1.0*xv_frequency/62.5));
        if (xv_encoding >= 0)
            XvSetPortAttribute (display, xvport, xv_enc_atom, xv_encoding);
        if (XvGetPortAttribute (display, xvport, xv_mute_atom, &cur_val) ==
                Success && cur_val == 1) {
            fprintf (stderr, "XV_MUTE is 1\n");
            if (xv_limits[limit_volume].min != xv_limits[limit_volume].max) {
                tmp_volume = xv_limits[limit_volume].min;
                XvSetPortAttribute(display, xvport, xv_volume_atom, tmp_volume);
                mute_timer = startTimer (start_vol_timeout);
            }
            XvSetPortAttribute (display, xvport, xv_mute_atom, 0);
            reset_xv_mute = true;
        }
        //XvGetVideo (..
        running = true;
    }
    if (running) {
        putVideo ();
        if (callback) {
            callback->playing ();
            callback->statusMessage ((int) KMPlayer::Callback::stat_hasvideo, QString ());
        }
    }
    XUnlockDisplay (display);
}

void KXVideoPlayer::stop () {
    if (mute_timer) {
        killTimer (mute_timer);
        mute_timer = 0;
    }
    if (running) {
        running = false;
        XLockDisplay (display);
        XvStopVideo (display, xvport, wid);
        if (reset_xv_autopaint_colorkey) {
            XvSetPortAttribute (display, xvport, xv_autopaint_colorkey_atom, 0);
            reset_xv_autopaint_colorkey = false;
        }
        if (reset_xv_mute) {
            XvSetPortAttribute (display, xvport, xv_mute_atom, 1);
            reset_xv_mute = false;
        }
        XvUngrabPort (display, xvport, CurrentTime);
        XFreeGC (display, gc);
        XClearArea (display, wid, 0, 0, 0, 0, true);
        XUnlockDisplay (display);
    }
    if (callback)
        callback->finished ();
    else
        QTimer::singleShot (0, qApp, SLOT (quit ()));
}

void KXVideoPlayer::finished () {
    QTimer::singleShot (10, this, SLOT (stop ()));
}

void KXVideoPlayer::saturation (int val) {
    XLockDisplay(display);
    XvSetPortAttribute (display, xvport, xv_saturation_atom, val);
    XFlush (display);
    XUnlockDisplay(display);
}

void KXVideoPlayer::hue (int val) {
    XLockDisplay(display);
    XvSetPortAttribute (display, xvport, xv_hue_atom, val);
    XFlush (display);
    XUnlockDisplay(display);
}

void KXVideoPlayer::contrast (int val) {
    XLockDisplay(display);
    XvSetPortAttribute (display, xvport, xv_contrast_atom, val);
    XFlush (display);
    XUnlockDisplay(display);
}

void KXVideoPlayer::brightness (int val) {
    XLockDisplay(display);
    XvSetPortAttribute (display, xvport, xv_brightness_atom, val);
    XFlush (display);
    XUnlockDisplay(display);
}

void KXVideoPlayer::volume (int val) {
    current_volume = val;
    if (mute_timer)
        return;
    XLockDisplay(display);
    XvSetPortAttribute (display, xvport, xv_volume_atom, val);
    XFlush (display);
    XUnlockDisplay(display);
}

void KXVideoPlayer::frequency (int val) {
    // this doesn't work, changing frequency kills audio for me
    if (mute_timer) {
        killTimer (mute_timer);
        mute_timer = 0;
    }
    xv_frequency = val;
    if (running && have_freq) {
        XLockDisplay(display);
        if (xv_limits[limit_volume].min != xv_limits[limit_volume].max) {
            tmp_volume = xv_limits[limit_volume].min;
            XvSetPortAttribute (display, xvport, xv_volume_atom, tmp_volume);
            mute_timer = startTimer (start_vol_timeout);
            XFlush (display);
            XvSetPortAttribute (display, xvport, xv_mute_atom, 0);
        }
        XvSetPortAttribute (display, xvport, xv_freq_atom, int (1.0*val/6.25));
        XFlush (display);
        XUnlockDisplay(display);
    }
}

void KXVideoPlayer::saveState (QSessionManager & sm) {
    if (callback)
        sm.setRestartHint (QSessionManager::RestartNever);
}

void KXVideoPlayer::timerEvent (QTimerEvent * e) {
    if (e->timerId () == mute_timer) {
        int step = (current_volume - xv_limits[limit_volume].min) / 20;
        if (step > 0 && tmp_volume == xv_limits[limit_volume].min) {
            killTimer (mute_timer);
            mute_timer = startTimer (inc_vol_timeout);
        }
        tmp_volume += step;
        if (tmp_volume >= current_volume || step <= 0) {
            tmp_volume = current_volume;
            killTimer (mute_timer);
            mute_timer = 0;
        }
        XLockDisplay(display);
        XvSetPortAttribute (display, xvport, xv_volume_atom, tmp_volume);
        XFlush (display);
        XUnlockDisplay(display);
    } else
        killTimer (e->timerId ());
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
                    fprintf(stderr, "keypressed 0x%x 0x%x\n", ( int ) kevent.keycode, ( int ) ksym);
                    switch (ksym) {
                        case XK_q:
                        case XK_Q:
                            xvapp->lock ();
                            xvapp->stop ();
                            xvapp->unlock ();
                            break;
                    }
                    break;
                }
                case Expose:
                    if(xevent.xexpose.count != 0 || xevent.xexpose.window != wid)
                        break;
                    break;

                case ConfigureNotify:
                    if (::running)
                        putVideo ();
                    break;
                case XvVideoNotify:
                    fprintf (stderr, "xvevent %lu\n", ((XvEvent*)&xevent)->xvvideo.reason);
                    break;
                case ButtonPress: {
                    XButtonEvent *bev = (XButtonEvent *) &xevent;
                    int dx = prev_click_x - bev->x;
                    int dy = prev_click_y - bev->y;
                    if (bev->time - prev_click_time < 400 &&
                            (dx * dx + dy * dy) < 25) {
                        xvapp->lock ();
                        if (callback)
                            callback->toggleFullScreen ();
                        xvapp->unlock ();
                    }
                    prev_click_time = bev->time;
                    prev_click_x = bev->x;
                    prev_click_y = bev->y;
                    break;
                }
                default:
                    if (xevent.type < LASTEvent) {
                        //fprintf (stderr, "event %d\n", xevent.type);
                    }
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

    unsigned int ver, rel, req, evb, err;
    if (XvQueryExtension (display, &ver, &rel, &req, &evb, &err) != Success) {
        fprintf (stderr, "XVideo not supported on display\n");
        XCloseDisplay (display);
        return 1;
    }
    xv_enc_atom = XInternAtom (display, "XV_ENCODING", false);
    xv_hue_atom = XInternAtom (display, "XV_HUE", false);
    xv_saturation_atom = XInternAtom (display, "XV_SATURATION", false);
    xv_brightness_atom = XInternAtom (display, "XV_BRIGHTNESS", false);
    xv_contrast_atom = XInternAtom (display, "XV_CONTRAST", false);
    xv_freq_atom = XInternAtom (display, "XV_FREQ", false);
    xv_volume_atom = XInternAtom (display, "XV_VOLUME", false);
    xv_mute_atom = XInternAtom (display, "XV_MUTE", false);
    xv_autopaint_colorkey_atom = XInternAtom (display, "XV_AUTOPAINT_COLORKEY", false);

    xvapp = new KXVideoPlayer (argc, argv);

    for(int i = 1; i < argc; i++) {
        if (!strcmp (argv [i], "-port")) {
            xvport = strtol (argv [++i], 0L, 10);
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
        } else if (!strcmp (argv [i], "-cb")) {
            QString str = argv [++i];
            int pos = str.find ('/');
            if (pos > -1) {
                fprintf (stderr, "callback is %s %s\n", str.left (pos).ascii (), str.mid (pos + 1).ascii ());
                callback = new KMPlayer::Callback_stub 
                    (str.left (pos).ascii (), str.mid (pos + 1).ascii ());
            }
        } else if (!strcmp (argv [i], "-enc")) {
            xv_encoding = strtol (argv [++i], 0L, 10);
        } else if (!strcmp (argv [i], "-norm")) {
            xv_norm = argv [++i];
        } else if (!strcmp (argv [i], "-freq")) {
            xv_frequency = strtol (argv [++i], 0L, 10);
        } else  {
            fprintf (stderr, "usage: %s [-port <xv port>] [-enc <encoding>] [-freq <frequency>] [-f <config file>] [-v] [(-wid|-window-id) <window>] [(-root|-window)] [-cb <DCOP callback name> [-c]]\n", argv[0]);
            delete xvapp;
            return 1;
        }
    }

    DCOPClient dcopclient;
    dcopclient.registerAs ("kxvideoplayer");
    Backend player;

    XEventThread * eventThread = new XEventThread;
    eventThread->start ();

    xvapp->init ();

    if (callback)
        callback->started (dcopclient.appId (), config_buf);

    xvapp->exec ();

    XLockDisplay(display);
    XClientMessageEvent ev = {
        ClientMessage, 0, true, display, wid, 
        XInternAtom (display, "XVIDEO", false), 8, {"quit_now"}
    };
    XSendEvent (display, wid, false, StructureNotifyMask, (XEvent *) & ev);
    XFlush (display);
    XUnlockDisplay(display);
    eventThread->wait (500);
    delete eventThread;

    xvapp->stop ();
    delete xvapp;

    fprintf (stderr, "closing display\n");
    XCloseDisplay (display);
    fprintf (stderr, "done\n");
    return 0;
}

#include "xvplayer.moc"
