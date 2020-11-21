/*
    This file belong to the KMPlayer project, a movie player plugin for Konqueror
    SPDX-FileCopyrightText: 2007 Koos Vriezen <koos.vriezen@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _KMPLAYER_PHONON_PLAYER_H_
#define _KMPLAYER_PHONON_PLAYER_H_

#include <stdint.h>

#include <QWidget>
//#include <QX11EmbedWidget>
typedef QWidget QX11EmbedWidget;

#include <phonon/phononnamespace.h>


namespace Phonon
{
    class VideoWidget;
    class AudioOutput;
    class MediaObject;
} // namespace Phonon

class Agent : public QObject
{
    Q_OBJECT
public:
    Agent();

    void newStream (const QString &url, uint64_t wid);
    void quit ();

    void streamDestroyed (uint64_t wid);

protected:
    void timerEvent (QTimerEvent *e) override;

private:
    int stay_alive_timer;
};

class Stream : public QX11EmbedWidget { // QWidget {
    Q_OBJECT
public:
    Stream (QWidget *parent, const QString &url, unsigned long wid);
    ~Stream () override;

    void play ();
    void pause ();
    void stop ();
    void seek (uint64_t position, bool absolute);
    void volume (int value);

//protected:
//    bool x11Event (XEvent *event);

private Q_SLOTS:
    void init ();

    void hasVideoChanged (bool hasVideo);
    void bufferStatus (int percentFilled);
    void metaDataChanged ();
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
