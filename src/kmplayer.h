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
#include "kmplayerplaylist.h"

static const int id_status_msg = 1;
static const int id_status_timer = 2;

class QPopupMenu;
class QMenuItem;
class QListViewItem;
class KProcess;
class KMPlayerBroadcastConfig;
class KMPlayerFFServerConfig;
class KSystemTray;

namespace KMPlayer {
    class View;
    class PartBase;
    class Source;
    class KMPlayerDVDSource;
    class KMPlayerDVDNavSource;
    class KMPlayerVCDSource;
    class KMPlayerPipeSource;
    class KMPlayerTVSource;
    class FFMpeg;
    class PlayListItem;
} // namespace


class KMPlayerApp : public KMainWindow
{
    Q_OBJECT

public:
    KMPlayerApp (QWidget* parent=0, const char* name=0);
    ~KMPlayerApp ();
    void openDocumentFile (const KURL& url=KURL());
    void addURL (const KURL& url);
    KMPlayer::PartBase * player () const { return m_player; }
    void resizePlayer (int percentage);
    KDE_NO_EXPORT KRecentFilesAction * recentFiles () const { return fileOpenRecent; }
    KDE_NO_EXPORT KMPlayer::View *view () const { return m_view; }
    bool broadcasting () const;
    void showBroadcastConfig ();
    void hideBroadcastConfig ();
    KDE_NO_EXPORT KMPlayerBroadcastConfig * broadcastConfig () const { return m_broadcastconfig; }
    /* After createGUI() some menu's have to readded again */
    void initMenu ();
    void restoreFromConfig ();
protected:
    void saveOptions ();
    void readOptions ();
    void saveProperties (KConfig * config);
    void readProperties (KConfig * config);
    void initActions ();
    void initStatusBar ();
    void initView ();
    virtual bool queryClose ();
    virtual bool queryExit ();

public slots:
    void slotFileNewWindow ();
    void slotFileOpen ();
    void slotFileOpenRecent (const KURL& url);
    void slotSaveAs ();
    void slotFileClose ();
    void slotFileQuit ();
    void slotPreferences ();
    void slotViewToolBar ();
    void slotViewStatusBar ();
    void slotViewMenuBar ();
    void slotStatusMsg (const QString &text);
    void slotSourceChanged (KMPlayer::Source *, KMPlayer::Source *);
private slots:
    void dvdNav ();
    void openDVD ();
    void openVCD ();
    void openAudioCD ();
    void openPipe ();
    void openVDR ();
    void fullScreen ();
    void configChanged ();
    void keepSizeRatio ();
    void startArtsControl();
    void loadingProgress (int percentage);
    void positioned (int pos, int length);
    void zoom50 ();
    void zoom100 ();
    void zoom150 ();
    void editMode ();
    void syncEditMode ();
    void broadcastClicked ();
    void broadcastStarted ();
    void broadcastStopped ();
    void playerStarted ();
    void slotMinimalMode ();
    void slotConfigureKeys();
    void slotConfigureToolbars ();
    void slotClearHistory ();
    void windowVideoConsoleToggled (int wt);
    void playListItemSelected (QListViewItem *);
    void playListItemDropped (QDropEvent * e, QListViewItem * after);
    void playListItemMoved ();
    void menuDropInList ();
    void menuDropInGroup ();
    void menuCopyDrop ();
    void menuDeleteNode ();
    void menuMoveUpNode ();
    void menuMoveDownNode ();
    void preparePlaylistMenu (KMPlayer::PlayListItem *, QPopupMenu *);

private:
    void menuItemClicked (QPopupMenu * menu, int id);
    void minimalMode (bool deco=true);
    KConfig * config;
    KSystemTray * m_systray;
    KMPlayer::PartBase * m_player;
    KMPlayer::View * m_view;
    KMPlayer::NodePtr recents;
    KMPlayer::NodePtr playlist;
    KMPlayer::NodePtrW manip_node;

    KAction * fileNewWindow;
    KAction * fileOpen;
    KRecentFilesAction * fileOpenRecent;
    KAction * fileClose;
    KAction * fileQuit;
    KAction * editVolumeInc;
    KAction * editVolumeDec;
    KAction * toggleView;
    KAction * viewSyncEditMode;
#if KDE_IS_VERSION(3,1,90)
    KToggleAction * viewFullscreen;
#else
    KAction * viewFullscreen;
#endif
    KToggleAction * viewEditMode;
    KToggleAction * viewToolBar;
    KToggleAction * viewStatusBar;
    KToggleAction * viewMenuBar;
    KToggleAction * viewKeepRatio;
    QMenuItem * m_sourcemenu;
    QPopupMenu * m_dvdmenu;
    QPopupMenu * m_dvdnavmenu;
    QPopupMenu * m_vcdmenu;
    QPopupMenu * m_audiocdmenu;
    QPopupMenu * m_tvmenu;
    QPopupMenu * m_dropmenu;
    KMPlayerFFServerConfig * m_ffserverconfig;
    KMPlayerBroadcastConfig * m_broadcastconfig;
    QCString m_dcopName;
    KURL::List m_drop_list;
    QListViewItem * m_drop_after;
    int edit_tree_id;
    int manip_tree_id;
    int last_time_left;
    int recents_id;
    int playlist_id;
    bool m_showToolbar;
    bool m_showStatusbar;
    bool m_showMenubar;
    bool m_played_intro;
    bool m_played_exit;
    bool m_minimal_mode;
    bool m_auto_resize;
};

class KMPLAYER_NO_EXPORT FileDocument : public KMPlayer::Document {
public:
    FileDocument (short id, const QString &, KMPlayer::PlayListNotify * notify = 0L);
    KMPlayer::NodePtr childFromTag (const QString & tag);
    void readFromFile (const QString & file);
    void writeToFile (const QString & file);
};

#endif // KMPLAYER_H
