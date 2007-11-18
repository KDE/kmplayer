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

#ifndef KMPLAYER_VDR_SOURCE_H
#define KMPLAYER_VDR_SOURCE_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <qframe.h>

#include <kurl.h>

#include "kmplayerappsource.h"
#include "kmplayerconfig.h"
#include "kmplayerprocess.h"


class KMPlayerApp;
class VDRCommand;
class KURLRequester;
class QButtonGroup;
class QMenuItem;
class QCheckBox;
class QLineEdit;
class KAction;
class QSocket;
class QTimerEvent;
class KListView;

/*
 * Preference page for VDR
 */
class KMPLAYER_NO_EXPORT KMPlayerPrefSourcePageVDR : public QFrame {
    Q_OBJECT
public:
    KMPlayerPrefSourcePageVDR (QWidget * parent, KMPlayer::PartBase * player);
    ~KMPlayerPrefSourcePageVDR ();
    KURLRequester * vcddevice;
    KListView * xv_port;
    QLineEdit * tcp_port;
    QButtonGroup * scale;
protected:
    void showEvent (QShowEvent *);
private:
    KMPlayer::PartBase * m_player;
};


/*
 * Source from VDR (XVideo actually) and socket connection
 */
class KMPLAYER_NO_EXPORT KMPlayerVDRSource : public KMPlayer::Source, public KMPlayer::PreferencesPage {
    Q_OBJECT
public:
    KMPlayerVDRSource (KMPlayerApp * app);
    virtual ~KMPlayerVDRSource ();
    virtual bool hasLength ();
    virtual bool isSeekable ();
    virtual QString prettyName ();
    virtual void write (KConfig *);
    virtual void read (KConfig *);
    virtual void sync (bool);
    virtual void prefLocation (QString & item, QString & icon, QString & tab);
    virtual QFrame * prefPage (QWidget * parent);
    virtual void stateElementChanged (KMPlayer::Node * node, KMPlayer::Node::State os, KMPlayer::Node::State ns);
    void waitForConnectionClose ();
public slots:
    void activate ();
    void deactivate ();
    void play (KMPlayer::Mrl *);
    void forward ();
    void backward ();
    void toggleConnected ();
    void volumeChanged (int);
private slots:
    void keyUp ();
    void keyDown ();
    void keyBack ();
    void keyOk ();
    void keySetup ();
    void keyChannels ();
    void keyMenu ();
    void key0 ();
    void key1 ();
    void key2 ();
    void key3 ();
    void key4 ();
    void key5 ();
    void key6 ();
    void key7 ();
    void key8 ();
    void key9 ();
    void keyRed ();
    void keyGreen ();
    void keyYellow ();
    void keyBlue ();
    void customCmd ();
    void connected ();
    void disconnected ();
    void readyRead ();
    void socketError (int);
    void processStopped ();
    void processStarted ();
    void configReceived ();
protected:
    void timerEvent (QTimerEvent *);
private:
    enum {
        act_up = 0, act_down, act_back, act_ok,
        act_setup, act_channels, act_menu,
        act_red, act_green, act_yellow, act_blue,
        act_0, act_1, act_2, act_3, act_4, act_5, act_6, act_7, act_8, act_9,
#if KDE_IS_VERSION(3, 1, 90)
        act_custom,
#endif
        act_last
    };
    void queueCommand (const char * cmd);
    void queueCommand (const char * cmd, int repeat_ms);
    void sendCommand ();
    void deleteCommands ();
    void jump (const QString & channel);
    KMPlayerApp * m_app;
    KMPlayerPrefSourcePageVDR * m_configpage;
    KAction * m_actions [act_last];
    KAction * m_fullscreen_actions [act_last];
    QSocket * m_socket;
    VDRCommand * commands;
    QString m_request_jump;
    KMPlayer::NodePtrW m_last_channel;
    int channel_timer;
    int timeout_timer;
    int finish_timer;
    int tcp_port;
    int m_stored_volume;
    int scale;
    int last_channel;
};

class KMPLAYER_NO_EXPORT XvProcessInfo : public KMPlayer::ProcessInfo {
public:
    XvProcessInfo (KMPlayer::MediaManager *);
    virtual KMPlayer::IProcess *create (KMPlayer::PartBase*,
            KMPlayer::ProcessInfo*, KMPlayer::AudioVideoMedia*);
};

class XVideo : public KMPlayer::CallbackProcess {
    Q_OBJECT
public:
    XVideo (QObject *, KMPlayer::ProcessInfo *, KMPlayer::Settings *);
    ~XVideo ();
public slots:
    virtual bool ready ();
};

#endif // KMPLAYER_VDR_SOURCE_H
