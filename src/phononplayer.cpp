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

#include "phononplayer.h"

#include <QtGui/QApplication>
#include <QtGui/QBoxLayout>
#include <QtDBus/QtDBus>

#include <phonon/audiooutput.h>
#include <phonon/mediaobject.h>
#include <phonon/videoplayer.h>
#include <phonon/videowidget.h>

#include <kurl.h>

#include "slaveadaptor.h"

using namespace Phonon;

static QString control_service;
static QString control_path;

Slave::Slave () {
    (void) new SlaveAdaptor (this);
    QDBusConnection::sessionBus().registerObject ("/phonon", this);
    QString service = QDBusConnection::sessionBus().baseService ();
    QDBusMessage msg = QDBusMessage::createMethodCall (
            control_service, control_path, "org.kde.kmplayer.Master",
            "running");
    msg << service;
    QDBusConnection::sessionBus().send (msg);
}

void Slave::new_stream (const QString &url, uint64_t wid) {
}

void Slave::quit () {
}

Stream::Stream (QWidget *parent, unsigned long wid)
    //: QX11EmbedWidget (parent), video_handle (wid)
    : QWidget (parent), video_handle (wid)
{
    m_media = new MediaObject(this);
    // might need VideoCategory here
    m_aoutput = new AudioOutput(Phonon::MusicCategory, this);
    m_vwidget = new VideoWidget(this);
    createPath (m_media, m_aoutput);
    createPath (m_media, m_vwidget);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_vwidget);
    m_vwidget->hide();

    connect(m_media, SIGNAL(hasVideoChanged(bool)), SLOT(hasVideoChanged(bool)));
    connect (m_media, SIGNAL (bufferStatus (int)), SLOT (bufferStatus (int)));
}

void Stream::setUrl (const KUrl &url) {
    m_media->setCurrentSource (url.url ());
    m_media->play ();
    m_vwidget->setVisible (m_media->hasVideo ());
}

void Stream::play () {
}

void Stream::pause () {
}

void Stream::stop () {
}

void Stream::seek (uint64_t position, bool absolute) {
}

void Stream::hasVideoChanged (bool hasVideo) {
    m_vwidget->setVisible (hasVideo);
}

void Stream::bufferStatus(int percentFilled) {
}

int main (int argc, char **argv) {
    QApplication app (argc, argv);
    QApplication::setApplicationName ("Phonon Slave");
    Stream mw (NULL, 0);
    mw.show ();
    if (argc > 1)
        mw.setUrl (KUrl (argv[1]));
    return app.exec ();
}

#include "phononplayer.moc"
