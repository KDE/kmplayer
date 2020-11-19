/*
    SPDX-FileCopyrightText: 2011 Koos Vriezen <koos.vriezen@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

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
class PlayItem
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
    TopPlayItem *rootItem () KMPLAYERCOMMON_EXPORT;

    QString title;
    Qt::ItemFlags item_flags;

    NodePtrW node;
    AttributePtrW attribute;

    QList<PlayItem*> child_items;
    PlayItem *parent_item;
};

class TopPlayItem : public PlayItem
{
public:
    TopPlayItem (PlayModel *m, int _id, Node *e, int flags)
      : PlayItem (e, nullptr),
        model (m),
        id (_id),
        root_flags (flags),
        show_all_nodes (false),
        have_dark_nodes (false)
    {}
    Qt::ItemFlags itemFlags () KMPLAYERCOMMON_EXPORT;
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

class KMPLAYERCOMMON_EXPORT PlayModel : public QAbstractItemModel
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

    PlayModel(QObject *parent, KIconLoader *);
    ~PlayModel() override;

    QVariant data (const QModelIndex &index, int role) const override KMPLAYERCOMMON_NO_EXPORT;
    bool setData (const QModelIndex&, const QVariant& v, int role) override KMPLAYERCOMMON_NO_EXPORT;
    Qt::ItemFlags flags (const QModelIndex &index) const override KMPLAYERCOMMON_NO_EXPORT;
    QVariant headerData (int section, Qt::Orientation orientation,
            int role = Qt::DisplayRole) const override KMPLAYERCOMMON_NO_EXPORT;
    QModelIndex index (int row, int column,
            const QModelIndex &parent = QModelIndex()) const override KMPLAYERCOMMON_NO_EXPORT;
    QModelIndex parent (const QModelIndex &index) const override KMPLAYERCOMMON_NO_EXPORT;
    bool hasChildren (const QModelIndex& parent = QModelIndex ()) const override KMPLAYERCOMMON_NO_EXPORT;
    int rowCount (const QModelIndex &parent = QModelIndex()) const override KMPLAYERCOMMON_NO_EXPORT;
    int columnCount (const QModelIndex &parent = QModelIndex()) const override KMPLAYERCOMMON_NO_EXPORT;

    PlayItem *rootItem () const KMPLAYERCOMMON_NO_EXPORT { return root_item; }
    QModelIndex indexFromItem (PlayItem *item) const KMPLAYERCOMMON_NO_EXPORT;
    PlayItem *itemFromIndex (const QModelIndex& index) const KMPLAYERCOMMON_NO_EXPORT;

    int addTree (NodePtr r, const QString &src, const QString &ico, int flgs);
    PlayItem *updateTree (TopPlayItem *ritem, NodePtr active);
signals:
    void updating (const QModelIndex&);
    void updated (const QModelIndex&, const QModelIndex&, bool sel, bool exp);

public slots:
    void updateTree (int id, NodePtr root, NodePtr active, bool sel, bool open);

private slots:
    void updateTrees() KMPLAYERCOMMON_NO_EXPORT;

private:
    PlayItem *populate (Node *e, Node *focus,
            TopPlayItem *root, PlayItem *item,
            PlayItem **curitem) KMPLAYERCOMMON_NO_EXPORT;
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

