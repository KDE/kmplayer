/**
 * Copyright (C) 2011 by Koos Vriezen <koos.vriezen@gmail.com>
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

#ifndef KMPLAYER_PLAYMODEL_H
#define KMPLAYER_PLAYMODEL_H

#include "config-kmplayer.h"

#include <QAbstractItemModel>
#include <QModelIndex>
#include <QPixmap>

#include "kmplayerplaylist.h"

class QPixmap;
class KIconLoader;
struct TreeUpdate;

namespace KMPlayer {

class PlayModel; 
class TopPlayItem;

/*
 * An item in the playlist
 */
class KMPLAYER_NO_EXPORT PlayItem
{
public:
    PlayItem (Node *e, PlayItem *parent)
        : item_flags (Qt::ItemIsEnabled | Qt::ItemIsSelectable),
          node (e), parent_item (parent)
    {}
    PlayItem (Attribute *a, PlayItem *pa)
        : item_flags (Qt::ItemIsEnabled | Qt::ItemIsSelectable),
          attribute (a), parent_item (pa)
    {}
    virtual ~PlayItem () { deleteChildren (); }

    void deleteChildren () { qDeleteAll (child_items); child_items.clear (); }
    void appendChild (PlayItem *child) { child_items.append (child); }
    PlayItem *child (unsigned i) {
        return i < (unsigned) child_items.size() ? child_items.at (i) : NULL;
    }
    int childCount () const { return child_items.count(); }
    int row () const {
        return parent_item->child_items.indexOf (const_cast <PlayItem*>( this));
    }
    PlayItem *parent () { return parent_item; }
    TopPlayItem *rootItem () KMPLAYER_EXPORT;

    QString title;
    Qt::ItemFlags item_flags;

    NodePtrW node;
    AttributePtrW attribute;

    QList<PlayItem*> child_items;
    PlayItem *parent_item;
};

class KMPLAYER_NO_EXPORT TopPlayItem : public PlayItem
{
public:
    TopPlayItem (PlayModel *m, int _id, Node *e, int flags)
      : PlayItem (e, NULL),
        model (m),
        id (_id),
        root_flags (flags),
        show_all_nodes (false),
        have_dark_nodes (false)
    {}
    Qt::ItemFlags itemFlags () KMPLAYER_EXPORT;
    void add ();
    void remove ();
    QPixmap icon;
    QString source;
    PlayModel *model;
    int id;
    int root_flags;
    bool show_all_nodes;
    bool have_dark_nodes;
};

class KMPLAYER_EXPORT PlayModel : public QAbstractItemModel
{
    friend class TopPlayItem;

    Q_OBJECT

public:
    enum { UrlRole = Qt::UserRole + 1 };

    enum Flags {
        AllowDrops = 0x01, AllowDrag = 0x02,
        InPlaceEdit = 0x04, TreeEdit = 0x08,
        Moveable = 0x10, Deleteable = 0x20
    };

    PlayModel (QObject *parent, KIconLoader *);
    ~PlayModel ();

    QVariant data (const QModelIndex &index, int role) const;
    bool setData (const QModelIndex&, const QVariant& v, int role);
    Qt::ItemFlags flags (const QModelIndex &index) const;
    QVariant headerData (int section, Qt::Orientation orientation,
            int role = Qt::DisplayRole) const;
    QModelIndex index (int row, int column,
            const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent (const QModelIndex &index) const;
    int rowCount (const QModelIndex &parent = QModelIndex()) const;
    int columnCount (const QModelIndex &parent = QModelIndex()) const;

    PlayItem *rootItem () const { return root_item; }
    QModelIndex indexFromItem (PlayItem *item) const;
    PlayItem *itemFromIndex (const QModelIndex& index) const;

    int addTree (NodePtr r, const QString &src, const QString &ico, int flgs);
    PlayItem *updateTree (TopPlayItem *ritem, NodePtr active);
signals:
    void updating (const QModelIndex&);
    void updated (const QModelIndex&, const QModelIndex&, bool sel, bool exp);

public slots:
    void updateTree (int id, NodePtr root, NodePtr active, bool sel, bool open);

private slots:
    void updateTrees ();

private:
    PlayItem *populate (Node *e, Node *focus,
            TopPlayItem *root, PlayItem *item,
            PlayItem **curitem);
    SharedPtr <TreeUpdate> tree_update;
    QPixmap auxiliary_pix;
    QPixmap config_pix;
    QPixmap folder_pix;
    QPixmap img_pix;
    QPixmap info_pix;
    QPixmap menu_pix;
    QPixmap unknown_pix;
    QPixmap url_pix;
    QPixmap video_pix;
    PlayItem *root_item;
    int last_id;
};

}

#endif

