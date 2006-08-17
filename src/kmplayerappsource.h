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
 

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <qframe.h>

#include <kurl.h>

#include "kmplayersource.h"
#include "kmplayerconfig.h"


class KMPlayerApp;
class KURLRequester;
class QPopupMenu;
class QMenuItem;
class QCheckBox;
class QLineEdit;
class TVInput;
class TVChannel;

/*
 * Base class for sources having a sub menu in the application
 */
class KMPLAYER_NO_EXPORT KMPlayerMenuSource : public KMPlayer::Source {
    Q_OBJECT
public:
    KMPlayerMenuSource (const QString & n, KMPlayerApp * app, QPopupMenu * m, const char * src);
    virtual ~KMPlayerMenuSource ();
protected:
    void menuItemClicked (QPopupMenu * menu, int id);
    QPopupMenu * m_menu;
    KMPlayerApp * m_app;
};

/*
 * Preference page for DVD
 */
class KMPLAYER_NO_EXPORT KMPlayerPrefSourcePageDVD : public QFrame {
    Q_OBJECT
public:
    KMPlayerPrefSourcePageDVD (QWidget * parent);
    ~KMPlayerPrefSourcePageDVD () {}

    QCheckBox * autoPlayDVD;
    KURLRequester * dvddevice;
};

/*
 * Source from DVD
 */
class KMPLAYER_NO_EXPORT KMPlayerDVDSource : public KMPlayerMenuSource, public KMPlayer::PreferencesPage {
    Q_OBJECT
public:
    KMPlayerDVDSource (KMPlayerApp * app, QPopupMenu * m);
    virtual ~KMPlayerDVDSource ();
    virtual bool processOutput (const QString & line);
    virtual QString filterOptions ();
    virtual void setIdentified (bool b = true);
    virtual QString prettyName ();
    virtual void write (KConfig *);
    virtual void read (KConfig *);
    virtual void sync (bool);
    virtual void prefLocation (QString & item, QString & icon, QString & tab);
    virtual QFrame * prefPage (QWidget * parent);
public slots:
    virtual void activate ();
    virtual void deactivate ();

    void titleMenuClicked (int id);
    void subtitleMenuClicked (int id);
    void languageMenuClicked (int id);
    void chapterMenuClicked (int id);
private:
    void buildArguments ();
    void play ();
    QPopupMenu * m_dvdtitlemenu;
    QPopupMenu * m_dvdchaptermenu;
    QPopupMenu * m_dvdlanguagemenu;
    QPopupMenu * m_dvdsubtitlemenu;
    KMPlayer::NodePtr disks;
    KMPlayerPrefSourcePageDVD * m_configpage;
    int m_current_title;
    bool m_start_play;
};


/*
 * Source from DVDNav
 */
class KMPLAYER_NO_EXPORT KMPlayerDVDNavSource : public KMPlayerMenuSource {
    Q_OBJECT
public:
    KMPlayerDVDNavSource (KMPlayerApp * app, QPopupMenu * m);
    virtual ~KMPlayerDVDNavSource ();
    virtual QString prettyName ();
public slots:
    virtual void activate ();
    virtual void deactivate ();
    virtual void play ();

    void finished ();
    void navMenuClicked (int id);
};


/*
 * Preference page for VCD
 */
class KMPLAYER_NO_EXPORT KMPlayerPrefSourcePageVCD : public QFrame {
    Q_OBJECT
public:
    KMPlayerPrefSourcePageVCD (QWidget * parent);
    ~KMPlayerPrefSourcePageVCD () {}
    KURLRequester * vcddevice;
    QCheckBox *autoPlayVCD;
};


/*
 * Source from VCD
 */
class KMPLAYER_NO_EXPORT KMPlayerVCDSource : public KMPlayerMenuSource, public KMPlayer::PreferencesPage {
    Q_OBJECT
public:
    KMPlayerVCDSource (KMPlayerApp * app, QPopupMenu * m);
    virtual ~KMPlayerVCDSource ();
    virtual bool processOutput (const QString & line);
    virtual void setIdentified (bool b = true);
    virtual QString prettyName ();
    virtual void write (KConfig *);
    virtual void read (KConfig *);
    virtual void sync (bool);
    virtual void prefLocation (QString & item, QString & icon, QString & tab);
    virtual QFrame * prefPage (QWidget * parent);
public slots:
    virtual void activate ();
    virtual void deactivate ();
private:
    void buildArguments ();
    KMPlayerPrefSourcePageVCD * m_configpage;
    bool m_start_play;
};


/*
 * Source from AudoCD
 */
class KMPLAYER_NO_EXPORT KMPlayerAudioCDSource : public KMPlayerMenuSource {
    Q_OBJECT
public:
    KMPlayerAudioCDSource (KMPlayerApp * app, QPopupMenu * m);
    virtual ~KMPlayerAudioCDSource ();
    virtual bool processOutput (const QString & line);
    virtual void setIdentified (bool b = true);
    virtual QString prettyName ();
public slots:
    virtual void activate ();
    virtual void deactivate ();
private:
    void buildArguments ();
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
public slots:
    virtual void activate ();
    virtual void deactivate ();
private:
    KMPlayerApp * m_app;
};

#endif // KMPLAYERAPPSOURCE_H
