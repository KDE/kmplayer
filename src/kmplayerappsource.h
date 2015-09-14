/***************************************************************************
                          kmplayersource.h  -  description
                             -------------------
    begin                : Sat Mar  24 16:14:51 CET 2003
    copyright            : (C) 2003 by Koos Vriezen
    email                : 
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KMPLAYERAPPSOURCE_H
#define KMPLAYERAPPSOURCE_H
 

#include "config-kmplayer.h"

#include <qframe.h>

#include <kurl.h>

#include "kmplayersource.h"
#include "kmplayerconfig.h"


class KMPlayerApp;
class KUrlRequester;
class QMenu;
class QCheckBox;

/*
 * Preference page for DVD
 */
class KMPLAYER_NO_EXPORT KMPlayerPrefSourcePageDVD : public QFrame {
    Q_OBJECT
public:
    KMPlayerPrefSourcePageDVD (QWidget * parent);
    ~KMPlayerPrefSourcePageDVD () {}

    QCheckBox * autoPlayDVD;
    KUrlRequester * dvddevice;
};

/*
 * Source from DVD
 */
class KMPLAYER_NO_EXPORT KMPlayerDVDSource : public KMPlayer::Source, public KMPlayer::PreferencesPage {
    Q_OBJECT
public:
    KMPlayerDVDSource(KMPlayerApp* app);
    virtual ~KMPlayerDVDSource ();
    virtual bool processOutput (const QString & line);
    virtual QString filterOptions ();
    virtual void setIdentified (bool b = true);
    virtual QString prettyName ();
    virtual void write (KSharedConfigPtr);
    virtual void read (KSharedConfigPtr);
    virtual void sync (bool);
    virtual void prefLocation (QString & item, QString & icon, QString & tab);
    virtual QFrame * prefPage (QWidget * parent);
    virtual void activate ();
    virtual void deactivate ();

private:
    void setCurrent (KMPlayer::Mrl *);
    void play (KMPlayer::Mrl *);
    KMPlayerApp* m_app;
    KMPlayer::NodePtr disks;
    KMPlayerPrefSourcePageDVD * m_configpage;
    int m_current_title;
    bool m_start_play;
};


/*
 * Preference page for VCD
 */
class KMPLAYER_NO_EXPORT KMPlayerPrefSourcePageVCD : public QFrame {
    Q_OBJECT
public:
    KMPlayerPrefSourcePageVCD (QWidget * parent);
    ~KMPlayerPrefSourcePageVCD () {}
    KUrlRequester * vcddevice;
    QCheckBox *autoPlayVCD;
};


/*
 * Source from VCD
 */
class KMPLAYER_NO_EXPORT KMPlayerVCDSource : public KMPlayer::Source, public KMPlayer::PreferencesPage {
    Q_OBJECT
public:
    KMPlayerVCDSource(KMPlayerApp* app);
    virtual ~KMPlayerVCDSource ();
    virtual bool processOutput (const QString & line);
    virtual void setIdentified (bool b = true);
    virtual QString prettyName ();
    virtual void write (KSharedConfigPtr);
    virtual void read (KSharedConfigPtr);
    virtual void sync (bool);
    virtual void prefLocation (QString & item, QString & icon, QString & tab);
    virtual QFrame * prefPage (QWidget * parent);
    virtual void activate ();
    virtual void deactivate ();
private:
    void setCurrent (KMPlayer::Mrl *);
    KMPlayerApp* m_app;
    KMPlayerPrefSourcePageVCD * m_configpage;
    bool m_start_play;
};


/*
 * Source from AudoCD
 */
class KMPLAYER_NO_EXPORT KMPlayerAudioCDSource : public KMPlayer::Source {
    Q_OBJECT
public:
    KMPlayerAudioCDSource(KMPlayerApp* app);
    virtual ~KMPlayerAudioCDSource ();
    virtual bool processOutput (const QString & line);
    virtual void setIdentified (bool b = true);
    virtual QString prettyName ();
    virtual void activate ();
    virtual void deactivate ();
private:
    void setCurrent (KMPlayer::Mrl *);
    KMPlayerApp* m_app;
};


/*
 * Source from stdin (for the backends, not kmplayer)
 */
class KMPLAYER_NO_EXPORT KMPlayerPipeSource : public KMPlayer::Source {
    Q_OBJECT
public:
    KMPlayerPipeSource (KMPlayerApp * app);
    virtual ~KMPlayerPipeSource ();
    virtual bool hasLength ();
    virtual bool isSeekable ();
    void setCommand (const QString & cmd);
    virtual QString prettyName ();
    virtual void activate ();
    virtual void deactivate ();
private:
    KMPlayerApp * m_app;
};

#endif // KMPLAYERAPPSOURCE_H
