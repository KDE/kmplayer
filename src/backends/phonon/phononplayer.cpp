/*
    This file belong to the KMPlayer project, a movie player plugin for Konqueror
    SPDX-FileCopyrightText: 2007 Koos Vriezen <koos.vriezen@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <sys/types.h>
#include <unistd.h>

#include "phononplayer.h"

#include <QApplication>
#include <QBoxLayout>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QMap>
#include <QTimer>
#include <QUrl>
#include <QX11Info>

#include <phonon/audiooutput.h>
#include <phonon/mediaobject.h>
#include <phonon/videoplayer.h>
#include <phonon/videowidget.h>

#include "agentadaptor.h"
#include "streamagentadaptor.h"

#include <xcb/xcb.h>

static QString control_service;
static QString control_path;
static Agent *agent;
typedef QMap <uint64_t, Stream *> AgentMap;
static AgentMap agent_map;

Agent::Agent ()
    : stay_alive_timer (0)
{
    QString service = QString ("org.kde.kphononplayer-%1").arg (getpid ());
    (void) new AgentAdaptor (this);
    QDBusConnection::sessionBus().registerObject ("/phonon", this);
    if (QDBusConnection::sessionBus().interface()->registerService (service) ==
            QDBusConnectionInterface::ServiceNotRegistered) {
        qDebug ("%s", qPrintable(QDBusConnection::sessionBus().lastError().message()));
        service = QDBusConnection::sessionBus().baseService ();
    }
    qDebug ("register as %s", qPrintable (service));
    if (!control_service.isEmpty ()) {
        QDBusMessage msg = QDBusMessage::createMethodCall (
                control_service, control_path, "org.kde.kmplayer.Master",
                "running");
        msg << service;
        QDBusConnection::sessionBus().send (msg);
    }
}

void Agent::newStream (const QString &url, uint64_t wid)
{
    if (stay_alive_timer) {
        killTimer (stay_alive_timer);
        stay_alive_timer = 0;
    }
    agent_map.insert (wid, new Stream (nullptr, url, wid));
}

void Agent::quit ()
{
    qDebug ("quit");
    qApp->quit ();
}

void Agent::streamDestroyed (uint64_t wid)
{
    agent_map.remove (wid);
    if (!agent_map.size ())
        stay_alive_timer = startTimer (5000);
}

void Agent::timerEvent (QTimerEvent *)
{
    quit ();
}

Stream::Stream (QWidget *parent, const QString &url, unsigned long wid)
    : QX11EmbedWidget (parent), m_url (url), video_handle (wid)
    //: QWidget (parent), video_handle (wid)
{
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_DontCreateNativeAncestors);
    createWinId();
    xcb_reparent_window(QX11Info::connection(), winId(), wid, 0, 0);
    //embedInto (wid);
    show ();
    m_master_stream_path = QString ("%1/stream_%2").arg(control_path).arg (wid);
    QTimer::singleShot (0, this, &Stream::init);
    qDebug ("newStream xembed cont: %lu", wid);
}

void Stream::init () {
    (void) new StreamAgentAdaptor (this);
    QDBusConnection::sessionBus().registerObject (
            QString ("/stream_%1").arg (video_handle), this);

    m_media = new Phonon::MediaObject(this);
    // might need VideoCategory here
    m_aoutput = new Phonon::AudioOutput (Phonon::MusicCategory, this);
    m_vwidget = new Phonon::VideoWidget(this);
    Phonon::createPath (m_media, m_aoutput);
    Phonon::createPath (m_media, m_vwidget);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins (0, 0, 0, 0);
    layout->addWidget(m_vwidget);
    m_vwidget->hide();

    connect(m_media, &Phonon::MediaObject::hasVideoChanged, this, &Stream::hasVideoChanged);
    connect (m_media, &Phonon::MediaObject::bufferStatus, this, &Stream::bufferStatus);
    connect (m_media, &Phonon::MediaObject::metaDataChanged, this, &Stream::metaDataChanged);
    connect (m_media, &Phonon::MediaObject::tick, this, &Stream::tick);
    connect (m_media, &Phonon::MediaObject::stateChanged, this, &Stream::stateChanged);
    connect (m_media, &Phonon::MediaObject::finished, this, &Stream::finished);

    if (m_url.startsWith ("dvd:"))
        m_media->setCurrentSource (Phonon::Dvd);
    else if (m_url.startsWith ("vcd:"))
        m_media->setCurrentSource (Phonon::Vcd);
    else if (m_url.startsWith ("cdda:"))
        m_media->setCurrentSource (Phonon::Cd);
    else if (m_url.startsWith ("/"))
        m_media->setCurrentSource (m_url);
    else
        m_media->setCurrentSource (QUrl (m_url));
    play ();
}

Stream::~Stream () {
    delete m_media;
    agent->streamDestroyed (video_handle);
}

/*bool Stream::x11Event (XEvent *event) {
    switch (event->type) {
        case PropertyNotify:
            return QWidget::x11Event (event);
    }
    return QX11EmbedWidget::x11Event (event);
}*/

void Stream::play () {
    qDebug ("play %s@%lu", qPrintable (m_url), video_handle);
    m_media->setTickInterval (500);
    m_media->play ();
    m_vwidget->setVisible (m_media->hasVideo ());
}

void Stream::pause () {
    if (m_media->state () == Phonon::PausedState)
        m_media->play ();
    else
        m_media->pause ();
}

void Stream::stop () {
    qDebug() << "stop" << video_handle;
    m_media->stop ();
    delete this;
}

void Stream::seek (uint64_t position, bool /*absolute*/) {
    m_media->seek (position * 100);
}

void Stream::volume (int value) {
    m_aoutput->setVolume (1.0 * value / 100);
}

void Stream::hasVideoChanged (bool hasVideo) {
    qDebug ("hasVideoChanged %d", hasVideo);
    m_vwidget->setVisible (hasVideo);
}

void Stream::bufferStatus (int percent_filled) {
    QDBusMessage msg = QDBusMessage::createMethodCall (
            control_service, m_master_stream_path,
            "org.kde.kmplayer.StreamMaster", "loading");
    msg << percent_filled;
    QDBusConnection::sessionBus().send (msg);
}

void Stream::metaDataChanged () {
    QString info;

    QString artist = m_media->metaData (Phonon::ArtistMetaData).join (" ");
    QString title = m_media->metaData (Phonon::TitleMetaData).join (" ");
    QString desc = m_media->metaData (Phonon::DescriptionMetaData).join (" ");
    qDebug ("metadata artist:%s title:%s desc:%s",
            artist.toUtf8 ().data (), title.toUtf8 ().data (), desc.toUtf8 ().data ());
    if (!title.isEmpty ()) {
        if (artist.isEmpty ())
            info = QString ("<b>") + title + QString ("</b>");
        else
            info = QString ("<b>") + artist + QString( " - " ) + title + QString ("</b>");
    }
    if (!desc.isEmpty ()) {
        if (!info.isEmpty ())
            info += QString ("<hr>");
        info += QString ("<i>") + desc + QString ("</i>");
    }

    QDBusMessage msg = QDBusMessage::createMethodCall (
            control_service, m_master_stream_path,
            "org.kde.kmplayer.StreamMaster", "streamMetaInfo");
    msg << info;
    QDBusConnection::sessionBus().send (msg);
}

void Stream::tick (qint64 t) {
    QDBusMessage msg = QDBusMessage::createMethodCall (
            control_service, m_master_stream_path,
            "org.kde.kmplayer.StreamMaster", "progress");
    msg << (qulonglong) t/100;
    QDBusConnection::sessionBus().send (msg);
}

void Stream::stateChanged (Phonon::State newstate, Phonon::State) {
    if (Phonon::PlayingState == newstate) {
        qDebug() << "playing" << video_handle << "len:" << m_media->totalTime();
        QDBusMessage msg = QDBusMessage::createMethodCall (
                control_service, m_master_stream_path,
                "org.kde.kmplayer.StreamMaster", "streamInfo");
        msg << (qulonglong) m_media->totalTime()/100 << (double)0.0; //FIXME:
        QDBusConnection::sessionBus().send (msg);

        QDBusMessage msg2 = QDBusMessage::createMethodCall (
                control_service, m_master_stream_path,
                "org.kde.kmplayer.StreamMaster", "playing");
        QDBusConnection::sessionBus().send (msg2);
    }
}

void Stream::finished () {
    qDebug ("finished %lu", video_handle);
    QDBusMessage msg = QDBusMessage::createMethodCall (
            control_service, m_master_stream_path,
            "org.kde.kmplayer.StreamMaster", "eof");
    QDBusConnection::sessionBus().send (msg);
    delete this;
}

int main (int argc, char **argv) {
    QString url;
    QApplication app (argc, argv);
    QApplication::setApplicationName ("Phonon Agent");
    for (int i = 1; i < argc; i++) {
        if (!strcmp (argv[i], "-cb") && ++i < argc) {
            QString s (argv[i]);
            int p = s.indexOf (QChar ('/'));
            if (p > -1) {
                control_service = s.left (p);
                control_path = s.mid (p);
            }
        } else {
            url = argv[i];
        }
    }
    agent = new Agent;
    if (!url.isEmpty ()) {
        new Stream (nullptr, url, 0);
        //mw.show ();
        //mw.play ();
    }
    return app.exec ();
}
