#include <stdio.h>
#include <string.h>
#include <math.h>
#include <config.h>
#include <dcopclient.h>
#include <qtimer.h>
#include <qthread.h>
#include <qmutex.h>
#include "kmplayer_backend.h"
#include "kmplayer_callback_stub.h"
#include "xineplayer.h"
#ifdef HAVE_XINE
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
         brightness (0), contrast (0), hue (0), saturation (0), volume (0),
         window_created (false) {
    }
    char configfile[2048];
    double res_h, res_v;
    char * vo_driver;
    char * ao_driver;
    QString mrl;
    int brightness, contrast, hue, saturation, volume;
    bool window_created;
};

KXinePlayer * xineapp;
KMPlayerCallback_stub * callback;
QMutex mutex;

static xine_t              *xine;
static xine_stream_t       *stream;
static xine_video_port_t   *vo_port;
static xine_audio_port_t   *ao_port;
static xine_event_queue_t  *event_queue;

static Display             *display;
static Window               wid;
static int                  screen;
static int                  completion_event;
static int                  xpos, ypos, width, height;
static double               pixel_aspect;

static int                  running = 0;
static int                  firstframe = 0;

extern "C" {

static void dest_size_cb(void * /*data*/, int /*video_width*/, int /*video_height*/, double /*video_pixel_aspect*/,
        int *dest_width, int *dest_height, double *dest_pixel_aspect)  {

    if(!running)
        return;

    *dest_width        = width;
    *dest_height       = height;
    *dest_pixel_aspect = pixel_aspect;
}

static void frame_output_cb(void * /*data*/, int /*video_width*/, int /*video_height*/,
        double /*video_pixel_aspect*/, int *dest_x, int *dest_y,
        int *dest_width, int *dest_height, 
        double *dest_pixel_aspect, int *win_x, int *win_y) {
    if(!running)
        return;
    if (firstframe) {
        printf("first frame\n");
        xineapp->lock ();
        xineapp->updatePosition ();
        xineapp->unlock ();
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
    switch(event->type) { 
        case XINE_EVENT_UI_PLAYBACK_FINISHED:
            printf ("XINE_EVENT_UI_PLAYBACK_FINISHED\n");
            xineapp->lock ();
            xineapp->finished ();
            xineapp->unlock ();
            break;

        case XINE_EVENT_PROGRESS:
            {
                xine_progress_data_t *pevent = (xine_progress_data_t *) event->data;
                printf("%s [%d%%]\n", pevent->description, pevent->percent);
            }
            break;
        case XINE_EVENT_MRL_REFERENCE:
            printf("XINE_EVENT_MRL_REFERENCE %s\n", 
            ((xine_mrl_reference_data_t*)event->data)->mrl);
            if (callback) {
                xineapp->lock ();
                callback->setURL (QString (((xine_mrl_reference_data_t*)event->data)->mrl));
                xineapp->unlock ();
            }
            break;
        case XINE_EVENT_FRAME_FORMAT_CHANGE:
            printf ("XINE_EVENT_FRAME_FORMAT_CHANGE\n");
            break;
        default:
            printf ("event_listener %d\n", event->type);

    }
}

} // extern "C"

KMPlayerBackend::KMPlayerBackend ()
    : DCOPObject (QCString ("KMPlayerBackend")) {
}

KMPlayerBackend::~KMPlayerBackend () {}

void KMPlayerBackend::setURL (QString url) {
    xineapp->setURL (url);
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

void KMPlayerBackend::seek (int pos, bool absolute) {
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

KXinePlayer::KXinePlayer (int _argc, char ** _argv)
  : QApplication (_argc, _argv, false) {
    d = new KXinePlayerPrivate;
    for(int i = 1; i < argc (); i++) {
        if (!strcmp (argv ()[i], "-vo")) {
            d->vo_driver = argv ()[++i];
        } else if (!strcmp (argv ()[i], "-ao")) {
            d->ao_driver = argv ()[++i];
        } else if (!strcmp (argv ()[i], "-wid")) {
            wid = atol (argv ()[++i]);
        } else if (!strcmp (argv ()[i], "-cb")) {
            QString str = argv ()[++i];
            int pos = str.find ('/');
            if (pos > -1) {
                printf ("callback is %s %s\n", str.left (pos).ascii (), str.mid (pos + 1).ascii ());
                callback = new KMPlayerCallback_stub 
                    (str.left (pos).ascii (), str.mid (pos + 1).ascii ());
            }
        } else 
            d->mrl = QString (argv ()[i]);
    }
    printf("mrl: '%s'\n", d->mrl.ascii ());
    xpos    = 0;
    ypos    = 0;
    width   = 320;
    height  = 200;
    if (!wid) {
        wid = XCreateSimpleWindow(display, XDefaultRootWindow(display),
                xpos, ypos, width, height, 1, 0, 0);
        d->window_created = true;
    }
    XSelectInput (display, wid,
                  (ExposureMask | KeyPressMask | StructureNotifyMask | SubstructureNotifyMask));
}

KXinePlayer::~KXinePlayer () {
    if (d->window_created)
        XDestroyWindow(display,  wid);
    xineapp = 0L;
}

void KXinePlayer::setURL (const QString & url) {
    d->mrl = url;
}

void KXinePlayer::play () {
    if (running) {
        mutex.lock ();
        if (xine_get_status (stream) == XINE_STATUS_PLAY &&
            xine_get_param (stream, XINE_PARAM_SPEED) == XINE_SPEED_PAUSE)
            xine_set_param( stream, XINE_PARAM_SPEED, XINE_SPEED_NORMAL);
        mutex.unlock ();
        return;
    }
    XWindowAttributes attr;
    XGetWindowAttributes(display, wid, &attr);
    width = attr.width;
    height = attr.height;
    printf ("trying lock 1\n");
    mutex.lock ();
    printf ("lock 1\n");
    xine = xine_new();
    sprintf(d->configfile, "%s%s", xine_get_homedir(), "/.xine/config2");
    xine_config_load(xine, d->configfile);
    xine_init(xine);
    XLockDisplay(display);
    if (XShmQueryExtension(display) == True)
        completion_event = XShmGetEventBase(display) + ShmCompletion;
    else
        completion_event = -1;
    if (d->window_created) {
        printf ("map %lu\n", wid);
        XMapRaised(display, wid);
    }
    d->res_h = (DisplayWidth(display, screen) * 1000 / DisplayWidthMM(display, screen));
    d->res_v = (DisplayHeight(display, screen) * 1000 / DisplayHeightMM(display, screen));
    XSync(display, False);
    XUnlockDisplay(display);
    x11_visual_t vis;
    vis.display           = display;
    vis.screen            = screen;
    vis.d                 = wid;
    vis.dest_size_cb      = dest_size_cb;
    vis.frame_output_cb   = frame_output_cb;
    vis.user_data         = NULL;
    pixel_aspect          = d->res_v / d->res_h;

    if(fabs(pixel_aspect - 1.0) < 0.01)
        pixel_aspect = 1.0;

    vo_port = xine_open_video_driver(xine, d->vo_driver, XINE_VISUAL_TYPE_X11, (void *) &vis);

    ao_port = xine_open_audio_driver (xine, d->ao_driver, NULL);

    stream = xine_stream_new (xine, ao_port, vo_port);
    event_queue = xine_event_new_queue (stream);
    xine_event_create_listener_thread (event_queue, event_listener, NULL);

    xine_gui_send_vo_data (stream, XINE_GUI_SEND_DRAWABLE_CHANGED, (void *)wid);
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
    if (!xine_open (stream, d->mrl.ascii ())) {
        printf("Unable to open mrl '%s'\n", d->mrl.ascii ());
        mutex.unlock ();
        finished ();
        return;
    }
    if (callback)
        firstframe = 1;
    if (!xine_play (stream, 0, 0)) {
        printf("Unable to play mrl '%s'\n", d->mrl.ascii ());
        mutex.unlock ();
        finished ();
        return;
    }
    mutex.unlock ();
}

void KXinePlayer::stop () {
    if (!running) return;
    printf("stop\n");
    printf ("trying lock 2\n");
    mutex.lock ();
    printf ("lock 2\n");
    xine_close (stream);
    running = 0;
    xine_event_dispose_queue (event_queue);
    xine_dispose (stream);
    stream = 0L;
    xine_close_audio_driver (xine, ao_port);  
    xine_close_video_driver (xine, vo_port);  
    xine_exit (xine);
    mutex.unlock ();

    Window root;
    XLockDisplay (display);
    if (d->window_created) {
        printf ("unmap %lu\n", wid);
        XUnmapWindow (display,  wid);
    } else {
        printf ("painting\n");
        unsigned int u, w, h;
        int x, y;
        XGetGeometry (display, wid, &root, &x, &y, &w, &h, &u, &u);
        XSetForeground (display, DefaultGC (display, screen),
                BlackPixel (display, screen));
        XFillRectangle (display, wid, DefaultGC (display, screen), x, y, w, h);
    }
    XSync (display, False);
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
    int h, w, pos, len;
    mutex.lock ();
    xine_get_pos_length (stream, 0, &pos, &len);
    if (firstframe) {
        w = xine_get_stream_info(stream, XINE_STREAM_INFO_VIDEO_WIDTH);
        h = xine_get_stream_info(stream, XINE_STREAM_INFO_VIDEO_HEIGHT);

    }
    mutex.unlock ();
    if (firstframe) {
        printf("movieParams %dx%d %d\n", w, h, len/100);
        if (h > 0)
            callback->movieParams (len/100, w, h, 1.0*w/h);
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

class XEventThread : public QThread {
public:
    void run () {
        while (true) {
            XEvent   xevent;
            XNextEvent(display, &xevent);
            if (xevent.type < LASTEvent)
                printf ("event %d\n", xevent.type);
            switch(xevent.type) {
                case ClientMessage:
                    if (xevent.xclient.format == 8 &&
                            !strncmp(xevent.xclient.data.b, "quit_now", 8))
                        printf("request quit\n");
                        return;
                case KeyPress:
                    {
                        printf("keypressed\n");
                        XKeyEvent  kevent;
                        KeySym     ksym;
                        char       kbuf[256];
                        int        len;

                        kevent = xevent.xkey;

                        XLockDisplay(display);
                        len = XLookupString(&kevent, kbuf, sizeof(kbuf), &ksym, NULL);
                        XUnlockDisplay(display);

                        switch (ksym) {

                            case XK_q:
                            case XK_Q:
                                qApp->quit();
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
                    if(xevent.xexpose.count != 0 || !stream)
                        break;
                    printf ("trying lock 3\n");
                    mutex.lock ();
                    printf ("lock 3\n");
                    if (stream)
                        xine_gui_send_vo_data(stream, XINE_GUI_SEND_EXPOSE_EVENT, &xevent);
                    mutex.unlock ();
                    break;

                case ConfigureNotify:
                    {
                        printf("ConfigureNotify\n");
                        XConfigureEvent *cev = (XConfigureEvent *) &xevent;
                        Window           tmp_win;

                        width  = cev->width;
                        height = cev->height;
                        printf("ConfigureNotify %d,%d\n", width, height);
                        if((cev->x == 0) && (cev->y == 0)) {
                            XLockDisplay(display);
                            XTranslateCoordinates(display, cev->window,
                                    DefaultRootWindow(cev->display),
                                    0, 0, &xpos, &ypos, &tmp_win);
                            XUnlockDisplay(display);
                        }
                        else {
                            xpos = cev->x;
                            ypos = cev->y;
                        }
                    }
                    break;
            }

            if(xevent.type == completion_event) 
                xine_gui_send_vo_data(stream, XINE_GUI_SEND_COMPLETION_EVENT, &xevent);
        }
    }
};

int main(int argc, char **argv) {
    if (!XInitThreads ()) {
        printf ("XInitThreads () failed\n");
        return 1;
    }
    display = XOpenDisplay(NULL);
    screen  = XDefaultScreen(display);

    xineapp = new KXinePlayer (argc, argv);

    DCOPClient dcopclient;
    dcopclient.registerAs ("kxineplayer");
    KMPlayerBackend player;

    XEventThread eventThread;
    eventThread.start ();

    if (callback) callback->started ();
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

    delete xineapp;

    printf ("closing display\n");
    XCloseDisplay (display);
    printf ("done\n");
    return 0;
}

#include "xineplayer.moc"

#else //HAVE_XINE
int main() { return -1; }
#endif //HAVE_XINE
