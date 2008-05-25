/**
  This file belong to the KMPlayer project, a movie player plugin for Konqueror
  Copyright (C) 2007  Koos Vriezen <koos.vriezen@gmail.com>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
**/

#include <sys/types.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <fixx11h.h>

#include "phononplayer.h"

#include <QtGui/QApplication>
#include <QtGui/QBoxLayout>
#include <QtDBus/QtDBus>
#include <QtCore/QMap>
#include <QtCore/QTimer>
#include <QtCore/QUrl>

#include <phonon/audiooutput.h>
#include <phonon/mediaobject.h>
#include <phonon/videoplayer.h>
#include <phonon/videowidget.h>

#include "slaveadaptor.h"
#include "streamslaveadaptor.h"

static QString control_service;
static QString control_path;
static Slave *slave;
typedef QMap <uint64_t, Stream *> SlaveMap;
static SlaveMap slave_map;

Slave::Slave () : stay_alive_timer (0) {
    QString service = QString ("org.kde.kphononplayer-%1").arg (getpid ());
    (void) new SlaveAdaptor (this);
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

void Slave::newStream (const QString &url, uint64_t wid) {
    if (stay_alive_timer) {
        killTimer (stay_alive_timer);
        stay_alive_timer = 0;
    }
    slave_map.insert (wid, new Stream (NULL, url, wid));
}

void Slave::quit () {
    qDebug ("quit");
    qApp->quit ();
}

void Slave::streamDestroyed (uint64_t wid) {
    slave_map.remove (wid);
    if (!slave_map.size ())
        stay_alive_timer = startTimer (5000);
}

void Slave::timerEvent (QTimerEvent *e) {
    quit ();
}

Stream::Stream (QWidget *parent, const QString &url, unsigned long wid)
    : QX11EmbedWidget (parent), m_url (url), video_handle (wid)
    //: QWidget (parent), video_handle (wid)
{
    embedInto (wid);
    show ();
    m_master_stream_path = QString ("%1/stream_%2").arg(control_path).arg (wid);
    QTimer::singleShot (0, this, SLOT (init ()));
    qDebug ("newStream xembed cont: %lu", containerWinId ());
}

void Stream::init () {
    (void) new StreamSlaveAdaptor (this);
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

    connect(m_media,SIGNAL(hasVideoChanged(bool)), SLOT(hasVideoChanged(bool)));
    connect (m_media, SIGNAL (bufferStatus (int)), SLOT (bufferStatus (int)));
    connect (m_media, SIGNAL (metaDataChanged ()), SLOT (metaDataChanged ()));
    connect (m_media, SIGNAL (tick (qint64)), SLOT (tick (qint64)));
    connect (m_media, SIGNAL (stateChanged(Phonon::State, Phonon::State)),
            SLOT (stateChanged (Phonon::State, Phonon::State)));
    connect (m_media, SIGNAL (finished ()), SLOT (finished ()));

    if (m_url.startsWith ("dvd:"))
        m_media->setCurrentSource (Phonon::Dvd);
    else if (m_url.startsWith ("vcd:"))
        m_media->setCurrentSource (Phonon::Vcd);
    else if (m_url.startsWith ("cdda:"))
        m_media->setCurrentSource (Phonon::Cd);
    else
        m_media->setCurrentSource (QUrl (m_url));
    play ();
}

Stream::~Stream () {
    delete m_media;
    slave->streamDestroyed (video_handle);
}

bool Stream::x11Event (XEvent *event) {
    switch (event->type) {
        case PropertyNotify:
            return QWidget::x11Event (event);
    }
    return QX11EmbedWidget::x11Event (event);
}

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
    qDebug ("stop %ul", video_handle);
    m_media->stop ();
    delete this;
}

void Stream::seek (uint64_t position, bool absolute) {
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
        qDebug ("playing %lu len:%lu", video_handle, m_media->totalTime());
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
    QApplication::setApplicationName ("Phonon Slave");
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
    slave = new Slave;
    if (!url.isEmpty ()) {
        new Stream (NULL, url, 0);
        //mw.show ();
        //mw.play ();
    }
    return app.exec ();
}

#include "phononplayer.moc"
