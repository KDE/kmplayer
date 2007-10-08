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

#include <kxmlguiwindow.h>
#include <kaction.h>
#include <kurl.h>
#include "kmplayerplaylist.h"

static const int id_status_msg = 1;
static const int id_status_timer = 2;

class QMenu;
class QMenuItem;
class Q3ListViewItem;
class KProcess;
class KMPlayerBroadcastConfig;
class KMPlayerFFServerConfig;
class KSystemTrayIcon;
class KToggleAction;
class KRecentFilesAction;
class KSharedConfig;
template<class T> class KSharedPtr;
typedef KSharedPtr<KSharedConfig> KSharedConfigPtr;

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


class KMPlayerApp : public KXmlGuiWindow {
    Q_OBJECT
public:
    KMPlayerApp (QWidget* parent=NULL);
    ~KMPlayerApp ();
    void openDocumentFile (const KUrl& url=KUrl());
    void addURL (const KUrl& url);
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
    void saveProperties ();
    void readProperties ();
    void initActions ();
    void initStatusBar ();
    void initView ();
    virtual bool queryClose ();
    virtual bool queryExit ();

public slots:
    void slotFileNewWindow ();
    void slotFileOpen ();
    void slotFileOpenRecent (const KUrl& url);
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
    void playListItemSelected (Q3ListViewItem *);
    void playListItemDropped (QDropEvent * e, Q3ListViewItem * after);
    void playListItemMoved ();
    void menuDropInList ();
    void menuDropInGroup ();
    void menuCopyDrop ();
    void menuDeleteNode ();
    void menuMoveUpNode ();
    void menuMoveDownNode ();
    void preparePlaylistMenu (KMPlayer::PlayListItem *, QMenu *);

private:
    void menuItemClicked (QMenu * menu, int id);
    void minimalMode (bool deco=true);
    KSystemTrayIcon *m_systray;
    KMPlayer::PartBase * m_player;
    KMPlayer::View * m_view;
    KMPlayer::NodePtr recents;
    KMPlayer::NodePtr playlist;
    KMPlayer::NodePtrW manip_node;

    QAction * fileNewWindow;
    QAction * fileOpen;
    KRecentFilesAction * fileOpenRecent;
    QAction * fileClose;
    QAction * fileQuit;
    QAction * editVolumeInc;
    QAction * editVolumeDec;
    QAction * toggleView;
    QAction * viewSyncEditMode;
    QAction * viewEditMode;
    QAction * viewFullscreen;
    KToggleAction * viewToolBar;
    KToggleAction * viewStatusBar;
    KToggleAction * viewMenuBar;
    KToggleAction * viewKeepRatio;
    QMenuItem * m_sourcemenu;
    QMenu * m_dvdmenu;
    QMenu * m_dvdnavmenu;
    QMenu * m_vcdmenu;
    QMenu * m_audiocdmenu;
    QMenu * m_tvmenu;
    QMenu * m_dropmenu;
    KMPlayerFFServerConfig * m_ffserverconfig;
    KMPlayerBroadcastConfig * m_broadcastconfig;
    KUrl::List m_drop_list;
    Q3ListViewItem * m_drop_after;
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
