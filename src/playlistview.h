/**
 * Copyright (C) 2006 by Koos Vriezen <koos.vriezen@gmail.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 **/

#ifndef PLAYLISTVIEW_H
#define PLAYLISTVIEW_H

#include "config-kmplayer.h"

#include <QtGui/QTreeWidget>
#include <QtGui/QTreeWidgetItem>
#include <QModelIndex>

#include "kmplayerplaylist.h"

class QFont;
class QPixmap;
class QPainter;
class QMenu;
class QDropEvent;
class QStyleOptionViewItem;
class KActionCollection;
class KFindDialog;

namespace KMPlayer {

class View;
class PlayListView;


/*
 * An item in the playlist
 */
class KMPLAYER_NO_EXPORT PlayListItem : public QTreeWidgetItem {
public:
    PlayListItem (QTreeWidgetItem *p, Node *e, PlayListView *lv);
    PlayListItem (QTreeWidgetItem *p, Attribute *a, PlayListView *lv);
    KDE_NO_CDTOR_EXPORT ~PlayListItem () {}
    void paintBranches(QPainter *p, const QColorGroup &cg, int w, int y, int h);
    PlayListView * playListView () const;
    void setRenameEnabled (bool enable);
    NodePtrW node;
    AttributePtrW m_attr;
    PlayListView * listview;
protected:
    QTreeWidgetItem *clone () const;
    PlayListItem (PlayListView *v, Node *doc, int type);
};

class KMPLAYER_NO_EXPORT RootPlayListItem : public PlayListItem {
public:
    RootPlayListItem (int id, PlayListView *v, Node *doc, int flags);
    RootPlayListItem (int id, PlayListView *v, Node *doc, int idx, int flags);
    RootPlayListItem (const RootPlayListItem &other);
    KDE_NO_CDTOR_EXPORT ~RootPlayListItem () {}
    Qt::ItemFlags itemFlags ();
    QString source;
    QString icon;
    int id;
    int flags;
    bool show_all_nodes;
    bool have_dark_nodes;
protected:
    QTreeWidgetItem *clone () const;
};

/*
 * The playlist GUI
 */
class KMPLAYER_EXPORT PlayListView : public QTreeWidget {
    Q_OBJECT
public:
    enum Flags {
        AllowDrops = 0x01, AllowDrag = 0x02,
        InPlaceEdit = 0x04, TreeEdit = 0x08,
        Moveable = 0x10, Deleteable = 0x20
    };
    PlayListView (QWidget * parent, View * view, KActionCollection * ac);
    ~PlayListView ();
    void selectItem (const QString & txt);
    void showAllNodes (RootPlayListItem *, bool show=true);
    void setActiveForegroundColor (const QColor & c) { m_active_color = c; }
    const QColor & activeColor () const { return m_active_color; }
    int addTree (NodePtr r, const QString & src, const QString & ico, int flgs);
    RootPlayListItem * rootItem (QTreeWidgetItem * item) const;
    RootPlayListItem * rootItem (int id) const;
    void setFont (const QFont &);
    PlayListItem *currentPlayListItem () const;
    PlayListItem *selectedItem () const;
    NodePtr lastDragNode () const { return m_last_drag; }
    int lastDragTreeId () const { return last_drag_tree_id; }
    bool isDragValid (QDropEvent *de);
    void paintCell (const QAbstractItemDelegate *,
                    QPainter *, const QStyleOptionViewItem&, const QModelIndex);
signals:
    void addBookMark (const QString & title, const QString & url);
    void prepareMenu (KMPlayer::PlayListItem * item, QMenu * menu);
    void dropped (QDropEvent *event, KMPlayer::PlayListItem *item);
protected:
    void dragEnterEvent (QDragEnterEvent *event);
    void dropEvent (QDropEvent *event);
    void dragMoveEvent (QDragMoveEvent *event);
    void drawBranches(QPainter *, const QRect &, const QModelIndex &) const {}
    void contextMenuEvent (QContextMenuEvent *event);
public slots:
    void updateTree (int id, NodePtr root, NodePtr active, bool sel, bool open);
private slots:
    void slotItemExpanded (QTreeWidgetItem *);
    void copyToClipboard ();
    void addBookMark ();
    void toggleShowAllNodes ();
    void slotItemChanged (QTreeWidgetItem *item, int column);
    void slotCurrentItemChanged (QTreeWidgetItem *cur, QTreeWidgetItem *prev);
    void renameSelected ();
    void updateTrees ();
    void slotFind ();
    void slotFindOk ();
    void slotFindNext ();
private:
    void updateTree (RootPlayListItem * ritem, NodePtr active, bool select);
    PlayListItem * populate (Node *e, Node *focus, RootPlayListItem *root, PlayListItem * item, PlayListItem ** curitem);
    struct KMPLAYER_NO_EXPORT TreeUpdate {
        KDE_NO_CDTOR_EXPORT TreeUpdate (RootPlayListItem *ri, NodePtr n, bool s, bool o, SharedPtr <TreeUpdate> &nx) : root_item (ri), node (n), select (s), open (o), next (nx) {}
        KDE_NO_CDTOR_EXPORT ~TreeUpdate () {}
        RootPlayListItem * root_item;
        NodePtrW node;
        bool select;
        bool open;
        SharedPtr <TreeUpdate> next;
    };
    SharedPtr <TreeUpdate> tree_update;
    View * m_view;
    QMenu * m_itemmenu;
    KAction * m_find;
    KAction * m_find_next;
    QAction * m_edit_playlist_item;
    KFindDialog * m_find_dialog;
    QPixmap folder_pix;
    QPixmap auxiliary_pix;
    QPixmap video_pix;
    QPixmap unknown_pix;
    QPixmap menu_pix;
    QPixmap config_pix;
    QPixmap url_pix;
    QPixmap info_pix;
    QPixmap img_pix;
    QColor m_active_color;
    NodePtrW m_current_find_elm;
    NodePtrW m_last_drag;
    AttributePtrW m_current_find_attr;
    int last_id;
    int last_drag_tree_id;
    int current_find_tree_id;
    bool m_ignore_expanded;
    bool m_ignore_update;
};

KDE_NO_EXPORT inline PlayListView * PlayListItem::playListView () const {
    return static_cast <PlayListView *> (treeWidget ());
}

KDE_NO_EXPORT inline PlayListItem * PlayListView::currentPlayListItem () const {
    return static_cast <PlayListItem *> (currentItem ());
}

} // namespace

#endif // PLAYLISTVIEW_H
