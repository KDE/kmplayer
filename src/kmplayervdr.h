/*
    This file is part of the KMPlayer application
    SPDX-FileCopyrightText: 2004 Koos Vriezen <koos.vriezen@xs4all.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KMPLAYER_VDR_SOURCE_H
#define KMPLAYER_VDR_SOURCE_H

#include "config-kmplayer.h"

#include <qframe.h>

#include "kmplayerappsource.h"
#include "kmplayerconfig.h"
#include "kmplayerprocess.h"


class KMPlayerApp;
class VDRCommand;
class KUrlRequester;
class Q3ButtonGroup;
class QLineEdit;
class KAction;
class QSocket;
class QTimerEvent;
class KListView;

/*
 * Preference page for VDR
 */
class KMPlayerPrefSourcePageVDR : public QFrame
{
    Q_OBJECT
public:
    KMPlayerPrefSourcePageVDR (QWidget * parent, KMPlayer::PartBase * player);
    ~KMPlayerPrefSourcePageVDR ();
    KUrlRequester * vcddevice;
    KListView * xv_port;
    QLineEdit * tcp_port;
    Q3ButtonGroup * scale;
protected:
    void showEvent (QShowEvent *);
private:
    KMPlayer::PartBase * m_player;
};


/*
 * Source from VDR (XVideo actually) and socket connection
 */
class KMPlayerVDRSource : public KMPlayer::Source, public KMPlayer::PreferencesPage
{
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
public Q_SLOTS:
    void activate ();
    void deactivate ();
    void play (KMPlayer::Mrl *);
    void forward ();
    void backward ();
    void toggleConnected ();
    void volumeChanged (int);
private Q_SLOTS:
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
        act_custom,
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

class XvProcessInfo : public KMPlayer::CallbackProcessInfo
{
public:
    XvProcessInfo (KMPlayer::MediaManager *);

    virtual KMPlayer::IProcess *create (KMPlayer::PartBase*,
            KMPlayer::AudioVideoMedia*);
    virtual bool startBackend ();
};

class XVideo : public KMPlayer::CallbackProcess {
    Q_OBJECT
public:
    XVideo (QObject *, KMPlayer::ProcessInfo *, KMPlayer::Settings *);
    ~XVideo ();
public Q_SLOTS:
    virtual bool ready ();
};

#endif // KMPLAYER_VDR_SOURCE_H
