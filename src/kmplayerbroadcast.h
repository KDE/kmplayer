/* this file is part of the kmplayer application
   copyright (c) 2003 koos vriezen <koos.vriezen@xs4all.nl>

   this program is free software; you can redistribute it and/or
   modify it under the terms of the gnu general public
   license as published by the free software foundation; either
   version 2 of the license, or (at your option) any later version.

   this program is distributed in the hope that it will be useful,
   but without any warranty; without even the implied warranty of
   merchantability or fitness for a particular purpose.  see the gnu
    general public license for more details.

   you should have received a copy of the gnu general public license
   along with this program; see the file copying.  if not, write to
   the free software foundation, inc., 59 temple place - suite 330,
   boston, ma 02111-1307, usa.
*/

#ifndef _KMPLAYER_BROADCAST_SOURCE_H_
#define _KMPLAYER_BROADCAST_SOURCE_H_

#include <list>
#include <vector>

#include <qframe.h>
#include <qguardedptr.h>

#include "kmplayerappsource.h"
#include "kmplayerconfig.h"

class KMPlayerPrefBroadcastPage;        // broadcast
class KMPlayerPrefBroadcastFormatPage;  // broadcast format
class QListBox;
class QComboBox;
class QLineEdit;
class QTable;
class QPushButton;
class KLed;

namespace KMPlayer {
    class FFMpeg;
}

class FFServerSetting {
public:
    KDE_NO_CDTOR_EXPORT FFServerSetting () {}
    FFServerSetting (int i, const QString & n, const QString & f, const QString & ac, int abr, int asr, const QString & vc, int vbr, int q, int fr, int gs, int w, int h);
    KDE_NO_CDTOR_EXPORT FFServerSetting (const QStringList & sl) { *this = sl; }
    KDE_NO_CDTOR_EXPORT ~FFServerSetting () {}
    int index;
    QString name;
    QString format;
    QString audiocodec;
    QString audiobitrate;
    QString audiosamplerate;
    QString videocodec;
    QString videobitrate;
    QString quality;
    QString framerate;
    QString gopsize;
    QString width;
    QString height;
    QStringList acl;
    FFServerSetting & operator = (const QStringList &);
    FFServerSetting & operator = (const FFServerSetting & fs);
    const QStringList list ();
    QString & ffconfig (QString & buf);
};

typedef std::vector <FFServerSetting *> FFServerSettingList;


class KMPlayerPrefBroadcastPage : public QFrame {
    Q_OBJECT
public:
    KMPlayerPrefBroadcastPage (QWidget * parent);
    KDE_NO_CDTOR_EXPORT ~KMPlayerPrefBroadcastPage () {}

    QLineEdit * bindaddress;
    QLineEdit * port;
    QLineEdit * maxclients;
    QLineEdit * maxbandwidth;
    QLineEdit * feedfile;
    QLineEdit * feedfilesize;
};

class KMPlayerPrefBroadcastFormatPage : public QFrame {
    Q_OBJECT
public:
    KMPlayerPrefBroadcastFormatPage (QWidget * parent, FFServerSettingList &);
    KDE_NO_CDTOR_EXPORT ~KMPlayerPrefBroadcastFormatPage () {}

    QListBox * profilelist;
    QComboBox * format;
    QLineEdit * audiocodec;
    QLineEdit * audiobitrate;
    QLineEdit * audiosamplerate;
    QLineEdit * videocodec;
    QLineEdit * videobitrate;
    QLineEdit * quality;
    QLineEdit * framerate;
    QLineEdit * gopsize;
    QLineEdit * moviewidth;
    QLineEdit * movieheight;
    QLineEdit * profile;
    QPushButton * startbutton;
    KLed * serverled;
    KLed * feedled;
    void setSettings (const FFServerSetting &);
    void getSettings (FFServerSetting &);
private slots:
    void slotIndexChanged (int index);
    void slotItemHighlighted (int index);
    void slotTextChanged (const QString &);
    void slotLoad ();
    void slotSave ();
    void slotDelete ();
private:
    QTable * accesslist;
    QPushButton * load;
    QPushButton * save;
    QPushButton * del;
    FFServerSettingList & profiles;
};


class KMPlayerFFServerConfig : public KMPlayer::KMPlayerPreferencesPage {
public:
    KMPlayerFFServerConfig ();
    KDE_NO_CDTOR_EXPORT ~KMPlayerFFServerConfig () {}
    virtual void write (KConfig *);
    virtual void read (KConfig *);
    virtual void sync (bool fromUI);
    virtual void prefLocation (QString & item, QString & icon, QString & tab);
    virtual QFrame * prefPage (QWidget * parent);
    int ffserverport;
    int maxclients;
    int maxbandwidth;
    QString feedfile;
    int feedfilesize;
    QString bindaddress;
private:
    QGuardedPtr <KMPlayerPrefBroadcastPage> m_configpage;
};

class KMPlayerBroadcastConfig : public QObject, public KMPlayer::KMPlayerPreferencesPage {
    Q_OBJECT
public:
    KMPlayerBroadcastConfig (KMPlayer::PartBase * player, KMPlayerFFServerConfig * fsc);
    KDE_NO_CDTOR_EXPORT ~KMPlayerBroadcastConfig ();

    virtual void write (KConfig *);
    virtual void read (KConfig *);
    virtual void sync (bool fromUI);
    virtual void prefLocation (QString & item, QString & icon, QString & tab);
    virtual QFrame * prefPage (QWidget * parent);

    bool broadcasting () const;
    void stopServer ();
    KDE_NO_EXPORT const QString & serverURL () const { return m_ffserver_url; }

    FFServerSetting ffserversettings;
    FFServerSettingList ffserversettingprofiles;
signals:
    void broadcastStarted ();
    void broadcastStopped ();
private slots:
    void processOutput (KProcess *, char *, int);
    void processStopped (KProcess * process);
    void startServer ();
    void startFeed ();
    void feedFinished ();
    void sourceChanged (KMPlayer::Source *);
private:
    KMPlayer::PartBase * m_player;
    KMPlayerFFServerConfig * m_ffserverconfig;
    QGuardedPtr <KMPlayerPrefBroadcastFormatPage> m_configpage;
    KMPlayer::FFMpeg * m_ffmpeg_process;
    KProcess * m_ffserver_process;
    bool m_endserver;
    QString m_ffserver_out;
    QString m_ffserver_url;
};


#endif //_KMPLAYER_BROADCAST_SOURCE_H_
