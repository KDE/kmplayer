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


#include "config-kmplayer.h"

#include <QModelIndex>
#include <qframe.h>

#include <kxmlguiwindow.h>
#include <ksharedconfig.h>

#include "kmplayerconfig.h"
#include "kmplayerpartbase.h"


class KUrlRequester;
class QAction;
class QCheckBox;
class QLabel;
class QMenu;
class QMenuItem;
class QTreeWidgetItem;
class KMPlayerBroadcastConfig;
class KMPlayerFFServerConfig;
class KSystemTrayIcon;
class KRecentFilesAction;
class KSharedConfig;
template<class T> class KSharedPtr;

namespace KMPlayer {
    class View;
    class PartBase;
    class Source;
    class PlayItem;
} // namespace


class KMPlayerApp : public KXmlGuiWindow {
    Q_OBJECT
public:
    KMPlayerApp (QWidget* parent=nullptr);
    ~KMPlayerApp ();
    void openDocumentFile (const KUrl& url=KUrl());
    void addUrl (const KUrl& url);
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
    void saveProperties (KConfigGroup&);
    void readProperties (const KConfigGroup&);
    void initActions ();
    void initStatusBar ();
    void initView ();
    virtual bool queryClose ();

public slots:
    void slotFileNewWindow ();
    void slotFileOpen ();
    void slotFileOpenRecent(const QUrl& url);
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
    void windowVideoConsoleToggled (bool show);
    void playListItemActivated (const QModelIndex&);
    void playListItemDropped (QDropEvent *e, KMPlayer::PlayItem *after);
    void playListItemMoved ();
    void menuDropInList ();
    void menuDropInGroup ();
    void menuCopyDrop ();
    void menuDeleteNode ();
    void menuMoveUpNode ();
    void menuMoveDownNode ();
    void preparePlaylistMenu (KMPlayer::PlayItem *, QMenu *);
    void slotGeneratorMenu ();
    void slotGenerator ();

private:
    void aboutToCloseWindow ();
    void minimalMode (bool deco=true);
    KSystemTrayIcon *m_systray;
    KMPlayer::PartBase * m_player;
    KMPlayer::View * m_view;
    KMPlayer::NodePtr recents;
    KMPlayer::NodePtr playlist;
    KMPlayer::NodePtrW manip_node;
    KMPlayer::NodePtrW current_generator;
    KMPlayer::NodeStoreList generators;

    QLabel* playtime_info;
    QAction * dropAdd;
    QAction * dropAddGroup;
    QAction * dropCopy;
    QAction * dropDelete;
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
    QAction * viewToolBar;
    QAction * viewStatusBar;
    QAction * viewMenuBar;
    QAction * viewKeepRatio;
    QMenuItem * m_sourcemenu;
    QMenu * m_dropmenu;
    QMenu * m_generatormenu;
    KMPlayerFFServerConfig * m_ffserverconfig;
    KMPlayerBroadcastConfig * m_broadcastconfig;
    QList<QUrl> m_drop_list;
    KMPlayer::PlayItem *m_drop_after;
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

#endif // KMPLAYER_H
