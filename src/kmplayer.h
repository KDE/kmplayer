/*
    SPDX-FileCopyrightText: 2002 Koos Vriezen

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
class QSystemTrayIcon;
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
    ~KMPlayerApp () override;
    void openDocumentFile (const KUrl& url=KUrl());
    void addUrl (const KUrl& url);
    KMPlayer::PartBase * player () const { return m_player; }
    void resizePlayer (int percentage);
    KRecentFilesAction * recentFiles () const { return fileOpenRecent; }
    KMPlayer::View *view () const { return m_view; }
    bool broadcasting () const;
    void showBroadcastConfig ();
    void hideBroadcastConfig ();
    KMPlayerBroadcastConfig * broadcastConfig () const { return m_broadcastconfig; }
    /* After createGUI() some menu's have to readded again */
    void initMenu ();
    void restoreFromConfig ();
protected:
    void saveOptions ();
    void readOptions ();
    void saveProperties (KConfigGroup&) override;
    void readProperties (const KConfigGroup&) override;
    void initActions ();
    void initStatusBar ();
    void initView ();
    bool queryClose () override;

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
    QSystemTrayIcon *m_systray;
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
class KMPlayerPrefSourcePageDVD : public QFrame
{
    Q_OBJECT
public:
    KMPlayerPrefSourcePageDVD (QWidget * parent);
    ~KMPlayerPrefSourcePageDVD () override {}

    QCheckBox * autoPlayDVD;
    KUrlRequester * dvddevice;
};

/*
 * Source from DVD
 */
class KMPlayerDVDSource : public KMPlayer::Source, public KMPlayer::PreferencesPage
{
    Q_OBJECT
public:
    KMPlayerDVDSource(KMPlayerApp* app);
    ~KMPlayerDVDSource () override;
    bool processOutput (const QString & line) override;
    QString filterOptions () override;
    void setIdentified (bool b = true) override;
    QString prettyName () override;
    void write (KSharedConfigPtr) override;
    void read (KSharedConfigPtr) override;
    void sync (bool) override;
    void prefLocation (QString & item, QString & icon, QString & tab) override;
    QFrame * prefPage (QWidget * parent) override;
    void activate () override;
    void deactivate () override;

private:
    void setCurrent (KMPlayer::Mrl *) override;
    void play (KMPlayer::Mrl *) override;
    KMPlayerApp* m_app;
    KMPlayer::NodePtr disks;
    KMPlayerPrefSourcePageDVD * m_configpage;
    int m_current_title;
    bool m_start_play;
};


/*
 * Preference page for VCD
 */
class KMPlayerPrefSourcePageVCD : public QFrame
{
    Q_OBJECT
public:
    KMPlayerPrefSourcePageVCD (QWidget * parent);
    ~KMPlayerPrefSourcePageVCD () override {}
    KUrlRequester * vcddevice;
    QCheckBox *autoPlayVCD;
};


/*
 * Source from VCD
 */
class KMPlayerVCDSource : public KMPlayer::Source, public KMPlayer::PreferencesPage
{
    Q_OBJECT
public:
    KMPlayerVCDSource(KMPlayerApp* app);
    ~KMPlayerVCDSource () override;
    bool processOutput (const QString & line) override;
    void setIdentified (bool b = true) override;
    QString prettyName () override;
    void write (KSharedConfigPtr) override;
    void read (KSharedConfigPtr) override;
    void sync (bool) override;
    void prefLocation (QString & item, QString & icon, QString & tab) override;
    QFrame * prefPage (QWidget * parent) override;
    void activate () override;
    void deactivate () override;
private:
    void setCurrent (KMPlayer::Mrl *) override;
    KMPlayerApp* m_app;
    KMPlayerPrefSourcePageVCD * m_configpage;
    bool m_start_play;
};


/*
 * Source from AudoCD
 */
class KMPlayerAudioCDSource : public KMPlayer::Source
{
    Q_OBJECT
public:
    KMPlayerAudioCDSource(KMPlayerApp* app);
    ~KMPlayerAudioCDSource () override;
    bool processOutput (const QString & line) override;
    void setIdentified (bool b = true) override;
    QString prettyName () override;
    void activate () override;
    void deactivate () override;
private:
    void setCurrent (KMPlayer::Mrl *) override;
    KMPlayerApp* m_app;
};


/*
 * Source from stdin (for the backends, not kmplayer)
 */
class KMPlayerPipeSource : public KMPlayer::Source
{
    Q_OBJECT
public:
    KMPlayerPipeSource (KMPlayerApp * app);
    ~KMPlayerPipeSource () override;
    bool hasLength () override;
    bool isSeekable () override;
    void setCommand (const QString & cmd);
    QString prettyName () override;
    void activate () override;
    void deactivate () override;
private:
    KMPlayerApp * m_app;
};

#endif // KMPLAYER_H
