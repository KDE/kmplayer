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

class KMPlayerDoc;
class KMPlayerView;
class KMPlayer;
class KProcess;
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
    KMPlayerDoc *getDocument () const; 	
    KMPlayer * player () const { return m_player; }

protected:
    void saveOptions ();
    void readOptions ();
    void initActions ();
    void initStatusBar ();
    void initDocument ();
    void initView ();
    virtual bool queryClose ();
    virtual bool queryExit ();
    virtual void saveProperties (KConfig *_cfg);
    virtual void readProperties (KConfig *_cfg);

public slots:
    void slotFileNewWindow ();
    void slotFileNew ();
    void slotFileOpen ();
    void slotFileOpenRecent (const KURL& url);
    void slotFileClose ();
    void slotFileQuit ();
    void slotPreferences ();
    void slotViewMenuBar ();
    void slotStatusMsg (const QString &text);
    void playDVD ();
    void playVCD ();
    void playPipe ();
private slots:
    void finished ();
    void openDVD ();
    void openVCD ();
    void openPipe ();
    void playDisc ();
    void finishedOpenDVD ();
    void finishedOpenVCD ();
    void play ();
    void titleMenuClicked (int id);
    void subtitleMenuClicked (int id);
    void languageMenuClicked (int id);
    void chapterMenuClicked (int id);
    void trackMenuClicked (int id);
    void fullScreen ();
    void configChanged ();
    void keepSizeRatio ();
    void showConsoleOutput ();
    void startArtsControl();
    void loadingProgress (int percentage);
    void zoom50 ();
    void zoom100 ();
    void zoom150 ();
private:
    void resizePlayer (int percentage);
    void menuItemClicked (QPopupMenu * menu, int id);
    KConfig * config;
    KMPlayerView * view;
    KMPlayer * m_player;
    KMPlayerDoc * doc;

    KAction * fileNewWindow;
    KAction * fileNew;
    KAction * fileOpen;
    KRecentFilesAction * fileOpenRecent;
    KAction * fileClose;
    KAction * fileQuit;
    KToggleAction * viewMenuBar;
    KToggleAction * viewKeepRatio;
    KToggleAction * viewShowConsoleOutput;
    QMenuItem * m_sourcemenu;
    QPopupMenu * m_dvdmenu;
    QPopupMenu * m_dvdtitlemenu;
    QPopupMenu * m_dvdchaptermenu;
    QPopupMenu * m_dvdlanguagemenu;
    QPopupMenu * m_dvdsubtitlemenu;
    QPopupMenu * m_vcdmenu;
    QPopupMenu * m_vcdtrackmenu;
    QCString m_dcopName;
    QString m_pipe;
    int m_dvdmenuId;
    int m_vcdmenuId;
    bool m_havedvdmenu : 1;
    bool m_havevcdmenu : 1;
    bool m_opendvd : 1;
    bool m_openvcd : 1;
    bool m_openpipe : 1;
    bool m_showToolbar : 1;
    bool m_showStatusbar : 1;
    bool m_showMenubar : 1;
};

#endif // KMPLAYER_H
