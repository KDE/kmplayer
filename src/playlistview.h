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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif 

#include <kdockwidget.h>
#include <klistview.h>

#include "kmplayerplaylist.h"

class QFont;
class QPixmap;
class QPainter;
class QPopupMenu;
class QDropEvent;

namespace KMPlayer {

class View;
class PlayListView;

bool isDragValid (QDropEvent * de);

/*
 * An item in the playlist
 */
class KMPLAYER_EXPORT PlayListItem : public QListViewItem {
public:
    PlayListItem (QListViewItem *p, const NodePtr & e, PlayListView * lv);
    PlayListItem (QListViewItem *p, const AttributePtr & e, PlayListView * lv);
    PlayListItem (PlayListView *v, const NodePtr & d, QListViewItem * b);
    KDE_NO_CDTOR_EXPORT ~PlayListItem () {}
    void paintCell (QPainter * p, const QColorGroup & cg, int column, int width, int align);
    void paintBranches(QPainter *p, const QColorGroup &cg, int w, int y, int h);
    NodePtrW node;
    AttributePtrW m_attr;
    PlayListView * listview;
protected:
    PlayListItem (PlayListView *v, const NodePtr & e);
};

class KMPLAYER_EXPORT RootPlayListItem : public PlayListItem {
public:
    RootPlayListItem (int id, PlayListView *v, const NodePtr & d, QListViewItem * b, int flags);
    KDE_NO_CDTOR_EXPORT ~RootPlayListItem () {}
    void paintCell (QPainter * p, const QColorGroup & cg, int column, int width, int align);
    NodePtrW m_doc;
    PlayListView * listview;
    QString source;
    QString icon;
    int id;
    int flags;
    bool show_all_nodes;
    bool have_dark_nodes;
};

/*
 * The playlist GUI
 */
class KMPLAYER_EXPORT PlayListView : public KListView {
    Q_OBJECT
public:
    enum Flags {
        AllowDrops = 0x01, AllowDrag = 0x02, InPlaceEdit = 0x04, TreeEdit = 0x08
    };
    PlayListView (QWidget * parent, View * view, KActionCollection * ac);
    ~PlayListView ();
    void selectItem (const QString & txt);
    void showAllNodes (bool show=true);
    void setActiveForegroundColor (const QColor & c) { m_active_color = c; }
    const QColor & activeColor () const { return m_active_color; }
    int addTree (NodePtr r, const QString & src, const QString & ico, int flgs);
    void setFont (const QFont &);
signals:
    void addBookMark (const QString & title, const QString & url);
protected:
    bool acceptDrag (QDropEvent* event) const;
    QDragObject * dragObject ();
public slots:
    void editCurrent ();
    void rename (QListViewItem * item, int c);
    void updateTree (int id, NodePtr root, NodePtr active);
private slots:
    void contextMenuItem (QListViewItem *, const QPoint &, int);
    void itemExpanded (QListViewItem *);
    void copyToClipboard ();
    void addBookMark ();
    void toggleShowAllNodes ();
    void itemDropped (QDropEvent * e, QListViewItem * after);
    void itemIsRenamed (QListViewItem * item);
    void slotFind ();
    void slotFindOk ();
    void slotFindNext ();
private:
    void updateTree (RootPlayListItem * ritem, NodePtr active);
    PlayListItem * populate (NodePtr e, NodePtr focus, RootPlayListItem *root, PlayListItem * item, PlayListItem ** curitem);
    bool findNodeInTree (NodePtr n, QListViewItem *& item);
    View * m_view;
    QPopupMenu * m_itemmenu;
    KAction * m_find_next;
    KFindDialog * m_find_dialog;
    QPixmap folder_pix;
    QPixmap auxiliary_pix;
    QPixmap video_pix;
    QPixmap unknown_pix;
    QPixmap menu_pix;
    QPixmap config_pix;
    QPixmap url_pix;
    QColor m_active_color;
    NodePtrW m_current_find_elm;
    AttributePtrW m_current_find_attr;
    int last_id;
    bool m_ignore_expanded;
};

} // namespace

#endif // PLAYLISTVIEW_H
