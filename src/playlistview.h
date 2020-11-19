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

#include <QTreeView>
#include <QModelIndex>

#include "kmplayerplaylist.h"

class QFont;
class QPainter;
class QMenu;
class QDropEvent;
class QStyleOptionViewItem;
class QAction;
class KActionCollection;
class KFindDialog;

namespace KMPlayer {

class View;
class PlayItem;
class PlayModel;
class TopPlayItem;

/*
 * The playlist GUI
 */
class KMPLAYERCOMMON_EXPORT PlayListView : public QTreeView
{
    Q_OBJECT
public:
    PlayListView(QWidget* parent, View* view, KActionCollection* ac);
    ~PlayListView() override;
    void selectItem (const QString & txt);
    void showAllNodes(TopPlayItem*, bool show=true) KMPLAYERCOMMON_NO_EXPORT;
    void setActiveForegroundColor (const QColor & c) { m_active_color = c; }
    const QColor & activeColor () const { return m_active_color; }
    TopPlayItem *rootItem (int id) const;
    void setFont(const QFont&) KMPLAYERCOMMON_NO_EXPORT;
    PlayItem *selectedItem () const;
    NodePtr lastDragNode () const { return m_last_drag; }
    int lastDragTreeId () const { return last_drag_tree_id; }
    bool isDragValid(QDropEvent* de) KMPLAYERCOMMON_NO_EXPORT;
    void paintCell (const QAbstractItemDelegate *,
                    QPainter *, const QStyleOptionViewItem&, const QModelIndex);
    QModelIndex index (PlayItem *item) const;
    PlayModel *playModel () const;
signals:
    void addBookMark (const QString & title, const QString & url);
    void prepareMenu (KMPlayer::PlayItem * item, QMenu * menu);
    void dropped (QDropEvent *event, KMPlayer::PlayItem *item);
protected:
    void dragEnterEvent(QDragEnterEvent* event) override KMPLAYERCOMMON_NO_EXPORT;
    void dropEvent(QDropEvent* event) override KMPLAYERCOMMON_NO_EXPORT;
    void dragMoveEvent(QDragMoveEvent* event) override KMPLAYERCOMMON_NO_EXPORT;
    void drawBranches(QPainter*, const QRect&, const QModelIndex&) const override KMPLAYERCOMMON_NO_EXPORT {}
    void contextMenuEvent(QContextMenuEvent* event) override KMPLAYERCOMMON_NO_EXPORT;
private slots:
    void slotItemExpanded(const QModelIndex&) KMPLAYERCOMMON_NO_EXPORT;
    void copyToClipboard() KMPLAYERCOMMON_NO_EXPORT;
    void addBookMark() KMPLAYERCOMMON_NO_EXPORT;
    void toggleShowAllNodes() KMPLAYERCOMMON_NO_EXPORT;
    void slotCurrentItemChanged(QModelIndex, QModelIndex) KMPLAYERCOMMON_NO_EXPORT;
    void modelUpdating(const QModelIndex&) KMPLAYERCOMMON_NO_EXPORT;
    void modelUpdated(const QModelIndex&, const QModelIndex&, bool, bool) KMPLAYERCOMMON_NO_EXPORT;
    void renameSelected() KMPLAYERCOMMON_NO_EXPORT;
    void slotFind() KMPLAYERCOMMON_NO_EXPORT;
    void slotFindOk() KMPLAYERCOMMON_NO_EXPORT;
    void slotFindNext() KMPLAYERCOMMON_NO_EXPORT;
private:
    View * m_view;
    QMenu * m_itemmenu;
    QAction * m_find;
    QAction * m_find_next;
    QAction * m_edit_playlist_item;
    KFindDialog * m_find_dialog;
    QColor m_active_color;
    NodePtrW m_current_find_elm;
    NodePtrW m_last_drag;
    AttributePtrW m_current_find_attr;
    int last_drag_tree_id;
    int current_find_tree_id;
    bool m_ignore_expanded;
};

} // namespace

#endif // PLAYLISTVIEW_H
