#include <stdio.h>
#include <string.h>
#include <math.h>
#include <config.h>
#include <dcopclient.h>
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
         callback (0L),
         wid (0L),
         window_created (false) {
    }
    char configfile[2048];
    double res_h, res_v;
    char * vo_driver;
    char * ao_driver;
    QString mrl;
    KMPlayerCallback_stub * callback;
    Window wid;
    bool window_created;
};

KXinePlayer * xineapp;

typedef int (*QX11EventFilter) (XEvent*);
extern QX11EventFilter qt_set_x11_event_filter (QX11EventFilter filter);
static QX11EventFilter oldFilter = 0;

static xine_t              *xine;
static xine_stream_t       *stream;
static xine_video_port_t   *vo_port;
static xine_audio_port_t   *ao_port;
static xine_event_queue_t  *event_queue;

static Display             *display;
static int                  screen;
static int                  completion_event;
static int                  xpos, ypos, width, height, fullscreen;
static double               pixel_aspect;

static int                  running = 0;


static void dest_size_cb(void *data, int video_width, int video_height, double video_pixel_aspect,
        int *dest_width, int *dest_height, double *dest_pixel_aspect)  {

    if(!running)
        return;

    *dest_width        = width;
    *dest_height       = height;
    *dest_pixel_aspect = pixel_aspect;
}

static void frame_output_cb(void *data, int video_width, int video_height,
        double video_pixel_aspect, int *dest_x, int *dest_y,
        int *dest_width, int *dest_height, 
        double *dest_pixel_aspect, int *win_x, int *win_y) {
    if(!running)
        return;

    *dest_x            = 0;
    *dest_y            = 0;
    *win_x             = xpos;
    *win_y             = ypos;
    *dest_width        = width;
    *dest_height       = height;
    *dest_pixel_aspect = pixel_aspect;
}

static void event_listener(void *user_data, const xine_event_t *event) {
    switch(event->type) { 
        case XINE_EVENT_UI_PLAYBACK_FINISHED:
            xineapp->stop ();
            break;

        case XINE_EVENT_PROGRESS:
            {
                xine_progress_data_t *pevent = (xine_progress_data_t *) event->data;

                printf("%s [%d%%]\n", pevent->description, pevent->percent);
            }
            break;

    }
}

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
}

void KMPlayerBackend::seek (int pos, bool absolute) {
}

void KMPlayerBackend::hue (int h, bool absolute) {
}

void KMPlayerBackend::saturation (int s, bool absolute) {
}

void KMPlayerBackend::contrast (int c, bool absolute) {
}

void KMPlayerBackend::brightness (int b, bool absolute) {
}

void KMPlayerBackend::volume (int v, bool absolute) {
}

void KMPlayerBackend::quit () {
    qApp->quit ();
}

KXinePlayer::KXinePlayer (int _argc, char ** _argv)
  : QApplication (_argc, _argv, true) {
    d = new KXinePlayerPrivate;
    for(int i = 1; i < argc (); i++) {
        if (!strcmp (argv ()[i], "-vo")) {
            d->vo_driver = argv ()[++i];
        } else if (!strcmp (argv ()[i], "-ao")) {
            d->ao_driver = argv ()[++i];
        } else if (!strcmp (argv ()[i], "-wid")) {
            d->wid = atol (argv ()[++i]);
        } else if (!strcmp (argv ()[i], "-cb")) {
            QString str = argv ()[++i];
            int pos = str.find ('/');
            if (pos > -1)
                d->callback = new KMPlayerCallback_stub 
                    (str.left (pos).ascii (), str.mid (pos + 1).ascii ());
        } else 
            d->mrl = QString (argv ()[i]);
    }
    printf("mrl: '%s'\n", d->mrl.ascii ());
    display = XOpenDisplay(NULL);
    screen  = XDefaultScreen(display);
    xpos    = 0;
    ypos    = 0;
    width   = 320;
    height  = 200;
    if (!d->wid) {
        d->wid = XCreateSimpleWindow(display, XDefaultRootWindow(display),
                xpos, ypos, width, height, 1, 0, 0);
        d->window_created = true;
    }
    XSelectInput (display, d->wid,
                  (ExposureMask | KeyPressMask | StructureNotifyMask));
    if (d->callback)
        d->callback->started ();
}

KXinePlayer::~KXinePlayer () {
    if (d->window_created)
        XDestroyWindow(display,  d->wid);
    XCloseDisplay (display);
}

void KXinePlayer::setURL (const QString & url) {
    d->mrl = url;
}

void KXinePlayer::play () {
    xine = xine_new();
    sprintf(d->configfile, "%s%s", xine_get_homedir(), "/.xine/config2");
    xine_config_load(xine, d->configfile);
    xine_init(xine);
    XLockDisplay(display);
    if (XShmQueryExtension(display) == True)
        completion_event = XShmGetEventBase(display) + ShmCompletion;
    else
        completion_event = -1;
    printf ("map %lu\n", d->wid);
    XMapRaised(display, d->wid);
    d->res_h = (DisplayWidth(display, screen) * 1000 / DisplayWidthMM(display, screen));
    d->res_v = (DisplayHeight(display, screen) * 1000 / DisplayHeightMM(display, screen));
    XSync(display, False);
    XUnlockDisplay(display);
    x11_visual_t vis;
    vis.display           = display;
    vis.screen            = screen;
    vis.d                 = d->wid;
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

    xine_gui_send_vo_data (stream, XINE_GUI_SEND_DRAWABLE_CHANGED, (void *) (d->wid));
    xine_gui_send_vo_data(stream, XINE_GUI_SEND_VIDEOWIN_VISIBLE, (void *) 1);

    if (!xine_open (stream, d->mrl.ascii ())) {
        printf("Unable to open mrl '%s'\n", d->mrl.ascii ());
        return;
    }
    running = 1;
    printf("video info %d,%d len %d\n", xine_get_stream_info(stream, XINE_STREAM_INFO_VIDEO_WIDTH), xine_get_stream_info(stream, XINE_STREAM_INFO_VIDEO_HEIGHT), xine_get_stream_info(stream, XINE_STREAM_INFO_FRAME_DURATION));
    if (!xine_play (stream, 0, 0)) {
        printf("Unable to play mrl '%s'\n", d->mrl.ascii ());
        return;
    }
}

void KXinePlayer::stop () {
    xine_close (stream);
    running = 0;
    xine_event_dispose_queue (event_queue);
    xine_dispose (stream);
    xine_close_audio_driver (xine, ao_port);  
    xine_close_video_driver (xine, vo_port);  
    xine_exit (xine);

    XLockDisplay (display);
    printf ("unmap %lu\n", d->wid);
    XUnmapWindow (display,  d->wid);
    XUnlockDisplay (display);


}

bool KXinePlayer::x11EventFilter (XEvent * e) {
    return false;
}

static int xine_x11_event_filter (XEvent * e) {
    bool processed = true;
    XEvent & xevent = *e;
    switch(xevent.type) {
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
            if(xevent.xexpose.count != 0)
                break;
            xine_gui_send_vo_data(stream, XINE_GUI_SEND_EXPOSE_EVENT, &xevent);
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
        default:
            processed = false;
    }

    if(xevent.type == completion_event) 
        xine_gui_send_vo_data(stream, XINE_GUI_SEND_COMPLETION_EVENT, &xevent);
    if (!processed && oldFilter)
        return oldFilter (e);
    return processed;
}

int main(int argc, char **argv) {

    if (argc <= 1) {
        printf ("specify an mrl\n");
        return 1;
    }
    if (!XInitThreads ()) {
        printf ("XInitThreads () failed\n");
        return 1;
    }

    xineapp = new KXinePlayer (argc, argv);

    DCOPClient dcopclient;
    dcopclient.registerAs ("kxineplayer");
    KMPlayerBackend player;

    //oldFilter = qt_set_x11_event_filter (xine_x11_event_filter);

    xineapp->exec ();
    printf("After app.exec\n");
    delete xineapp;

    //qt_set_x11_event_filter (oldFilter);
    return 0;
}

#include "xineplayer.moc"

#else //HAVE_XINE
int main() { return -1; }
#endif //HAVE_XINE
/*
 * Local variables:
 * compile-command: "gcc -Wall -O2 `xine-config --cflags` `xine-config --libs` -lX11 -lm -o  xinimin xinimin.c"
 * End:
 */
