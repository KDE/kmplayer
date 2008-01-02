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

#include "phononplayer.h"

#include <QtGui/QApplication>
#include <QtGui/QBoxLayout>
#include <QtDBus/QtDBus>
#include <QtCore/QMap>

#include <phonon/audiooutput.h>
#include <phonon/mediaobject.h>
#include <phonon/videoplayer.h>
#include <phonon/videowidget.h>

#include "slaveadaptor.h"
#include "streamslaveadaptor.h"

using namespace Phonon;

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
    QDBusMessage msg = QDBusMessage::createMethodCall (
            control_service, control_path, "org.kde.kmplayer.Master",
            "running");
    msg << service;
    QDBusConnection::sessionBus().send (msg);
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
    : QX11EmbedWidget (parent), video_handle (wid)
    //: QWidget (parent), video_handle (wid)
{
    embedInto (wid);
    show ();
    qDebug ("newStream xembed cont: %lu", containerWinId ());

    (void) new StreamSlaveAdaptor (this);
    QDBusConnection::sessionBus().registerObject (
            QString ("/stream_%1").arg (wid), this);

    m_media = new MediaObject(this);
    // might need VideoCategory here
    m_aoutput = new AudioOutput (Phonon::MusicCategory, this);
    m_vwidget = new VideoWidget(this);
    createPath (m_media, m_aoutput);
    createPath (m_media, m_vwidget);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_vwidget);
    m_vwidget->hide();

    connect(m_media,SIGNAL(hasVideoChanged(bool)), SLOT(hasVideoChanged(bool)));
    connect (m_media, SIGNAL (bufferStatus (int)), SLOT (bufferStatus (int)));
    connect (m_media, SIGNAL (stateChanged(Phonon::State, Phonon::State)),
            SLOT (stateChanged (Phonon::State, Phonon::State)));
    connect (m_media, SIGNAL (finished ()), SLOT (finished ()));

    m_media->setCurrentSource (url);
    play ();
}

Stream::~Stream () {
    delete m_media;
    slave->streamDestroyed (video_handle);
}

void Stream::play () {
    m_media->play ();
    m_vwidget->setVisible (m_media->hasVideo ());
}

void Stream::pause () {
}

void Stream::stop () {
    qDebug ("stop %ul", video_handle);
    m_media->stop ();
    delete this;
}

void Stream::seek (uint64_t position, bool absolute) {
}

void Stream::hasVideoChanged (bool hasVideo) {
    qDebug ("hasVideoChanged %d", hasVideo);
    m_vwidget->setVisible (hasVideo);
}

void Stream::bufferStatus (int percentFilled) {
    qDebug ("buffering %d", percentFilled);
}

void Stream::stateChanged (Phonon::State newstate, Phonon::State) {
    if (Phonon::PlayingState == newstate) {
        qDebug ("playing %lu", video_handle);
        QDBusMessage msg = QDBusMessage::createMethodCall (
                control_service, QString ("/stream_%1").arg (video_handle),
                "org.kde.kmplayer.StreamMaster", "playing");
        QDBusConnection::sessionBus().send (msg);
        show ();
        m_vwidget->setVisible (m_media->hasVideo ());
    }
}

void Stream::finished () {
    qDebug ("finished %lu", video_handle);
    QDBusMessage msg = QDBusMessage::createMethodCall (
            control_service, QString ("/stream_%1").arg (video_handle),
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
        Stream mw (NULL, url, 0);
        //mw.show ();
        //mw.play ();
    }
    return app.exec ();
}

#include "phononplayer.moc"
