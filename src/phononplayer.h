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

#ifndef _KMPLAYER_PHONON_PLAYER_H_
#define _KMPLAYER_PHONON_PLAYER_H_

#include <stdint.h>

#include <QtGui/QX11EmbedWidget>

#include <phonon/phononnamespace.h>

class KUrl;

namespace Phonon
{
    class VideoWidget;
    class AudioOutput;
    class MediaObject;
} // namespace Phonon

class Slave : public QObject {
    Q_OBJECT
public:
    Slave ();

    void newStream (const QString &url, uint64_t wid);
    void quit ();

    void streamDestroyed (uint64_t wid);

protected:
    void timerEvent (QTimerEvent *e);

private:
    int stay_alive_timer;
};

class Stream : public QX11EmbedWidget { // QWidget {
    Q_OBJECT
public:
    Stream (QWidget *parent, const QString &url, unsigned long wid);
    ~Stream ();

    void play ();
    void pause ();
    void stop ();
    void seek (uint64_t position, bool absolute);

protected:
    bool x11Event (XEvent *event);

private Q_SLOTS:
    void init ();

    void hasVideoChanged (bool hasVideo);
    void bufferStatus (int percentFilled);
    void tick (qint64 time);
    void stateChanged (Phonon::State newstate, Phonon::State oldstate);
    void finished ();

private:
    Phonon::VideoWidget *m_vwidget;
    Phonon::AudioOutput *m_aoutput;
    Phonon::MediaObject *m_media;
    QString m_url;
    QString m_master_stream_path;
    unsigned long video_handle;
};

#endif
