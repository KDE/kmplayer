/***************************************************************************
                          kmplayer.h  -  description
                             -------------------
    begin                : Sat Dec  7 16:14:51 CET 2002
    copyright            : (C) 2002 by Koos Vriezen
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

#ifndef KMPLAYER_H
#define KMPLAYER_H
 

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <kapp.h>
#include <kmainwindow.h>
#include <kaccel.h>
#include <kaction.h>
#include <kurl.h>

class KMPlayerView;
class KMPlayer;
class KProcess;
class KMPlayerAppURLSource;
class KMPlayerSource;
class KMPlayerDVDSource;
class KMPlayerDVDNavSource;
class KMPlayerVCDSource;
class KMPlayerPipeSource;
class KMPlayerTVSource;
class QPopupMenu;
class QMenuItem;


class KMPlayerApp : public KMainWindow
{
    Q_OBJECT

    friend class KMPlayerView;

public:
    KMPlayerApp (QWidget* parent=0, const char* name=0);
    ~KMPlayerApp ();
    void openDocumentFile (const KURL& url=0);
    KMPlayer * player () const { return m_player; }
    void resizePlayer (int percentage);
    KRecentFilesAction * recentFiles () const { return fileOpenRecent; }
    bool broadcasting () const;
    KMPlayerView *view () const { return m_view; }
protected:
    void saveOptions ();
    void readOptions ();
    void initActions ();
    void initStatusBar ();
    void initView ();
    virtual bool queryClose ();
    virtual bool queryExit ();

public slots:
    void slotFileNewWindow ();
    void slotFileOpen ();
    void slotFileOpenRecent (const KURL& url);
    void slotFileClose ();
    void slotFileQuit ();
    void slotPreferences ();
    void slotViewToolBar ();
    void slotViewStatusBar ();
    void slotViewMenuBar ();
    void slotStatusMsg (const QString &text);
    void slotSourceChanged (KMPlayerSource *);
    void startFeed ();
private slots:
    void dvdNav ();
    void openDVD ();
    void openVCD ();
    void openPipe ();
    void fullScreen ();
    void configChanged ();
    void keepSizeRatio ();
    void showConsoleOutput ();
    void startArtsControl();
    void loadingProgress (int percentage);
    void zoom50 ();
    void zoom100 ();
    void zoom150 ();
    void broadcastClicked ();
    void processOutput (KProcess *, char *, int);
    void processStopped (KProcess * process);
private:
    void menuItemClicked (QPopupMenu * menu, int id);
    KConfig * config;
    KMPlayerView * m_view;
    KMPlayer * m_player;

    KAction * fileNewWindow;
    KAction * fileOpen;
    KRecentFilesAction * fileOpenRecent;
    KAction * fileClose;
    KAction * fileQuit;
    KToggleAction * viewToolBar;
    KToggleAction * viewStatusBar;
    KToggleAction * viewMenuBar;
    KToggleAction * viewKeepRatio;
    KToggleAction * viewShowConsoleOutput;
    QMenuItem * m_sourcemenu;
    QPopupMenu * m_dvdmenu;
    QPopupMenu * m_dvdnavmenu;
    QPopupMenu * m_vcdmenu;
    QPopupMenu * m_tvmenu;
    KMPlayerAppURLSource * m_urlsource;
    KMPlayerDVDSource * m_dvdsource;
    KMPlayerDVDNavSource * m_dvdnavsource;
    KMPlayerVCDSource * m_vcdsource;
    KMPlayerPipeSource * m_pipesource;
    KMPlayerTVSource * m_tvsource;
    QCString m_dcopName;
    KProcess * m_ffmpeg_process;
    KProcess * m_ffserver_process;
    QString m_ffserver_out;
    bool m_endserver : 1;
    bool m_showToolbar : 1;
    bool m_showStatusbar : 1;
    bool m_showMenubar : 1;
};

#endif // KMPLAYER_H
