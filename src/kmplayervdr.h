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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
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
class QPopupMenu;
class QButtonGroup;
class QMenuItem;
class QCheckBox;
class QLineEdit;
class KAction;
class QSocket;
class QTimerEvent;
class KListView;

class KMPlayerPrefSourcePageVDR : public QFrame {
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


class KMPlayerVDRSource : public KMPlayerMenuSource, public KMPlayer::PreferencesPage {
    Q_OBJECT
public:
    KMPlayerVDRSource (KMPlayerApp * app, QPopupMenu *);
    virtual ~KMPlayerVDRSource ();
    virtual bool hasLength ();
    virtual bool isSeekable ();
    virtual QString prettyName ();
    virtual void write (KConfig *);
    virtual void read (KConfig *);
    virtual void sync (bool);
    virtual void prefLocation (QString & item, QString & icon, QString & tab);
    virtual QFrame * prefPage (QWidget * parent);
public slots:
    void activate ();
    void deactivate ();
    void jump (KMPlayer::ElementPtr e);
    void forward ();
    void backward ();
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
    void toggleConnected ();
    void configReceived ();
protected:
    void timerEvent (QTimerEvent *);
private:
    void queueCommand (const char * cmd);
    void queueCommand (const char * cmd, int repeat_ms);
    void sendCommand ();
    void deleteCommands ();
    KMPlayerPrefSourcePageVDR * m_configpage;
    KAction * act_up;
    KAction * act_down;
    KAction * act_back;
    KAction * act_ok;
    KAction * act_setup;
    KAction * act_channels;
    KAction * act_menu;
    KAction * act_red;
    KAction * act_green;
    KAction * act_yellow;
    KAction * act_blue;
    QSocket * m_socket;
    VDRCommand * commands;
    int channel_timer;
    int timeout_timer;
    int tcp_port;
    int scale;
};

class XVideo : public KMPlayer::CallbackProcess {
    Q_OBJECT
public:
    struct Input {
        Input (int e, const QString & n, Input * i = 0L)
            : encoding (e), name (n), next (i) {}
        int encoding;
        QString name;
        Input * next;
    };
    struct Port {
        Port (int p, bool t, Port * n)
            : port (p), has_tuner (t), inputs (0L), next (n) {}
        int port;
        bool has_tuner;
        Input * inputs;
        Port * next;
    };
    XVideo (KMPlayer::PartBase * player);
    ~XVideo ();
    QString menuName () const;
    void initProcess ();
    void setStarted (QCString dcopname, QByteArray & data);
    Port * ports () const { return m_ports; }
public slots:
    virtual bool play ();
    virtual bool quit ();
protected:
    virtual void runForConfig ();
private slots:
    void processStopped (KProcess *);
    void processOutput (KProcess *, char *, int);
private:
    Port * m_ports;
};

#endif // KMPLAYER_VDR_SOURCE_H
