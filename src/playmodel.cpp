/*
    SPDX-FileCopyrightText: 2011 Koos Vriezen <koos.vriezen@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include "playmodel.h"
#include "playlistview.h"
#include "kmplayercommon_log.h"

#include <qpixmap.h>
#include <qtimer.h>

#include <klocalizedstring.h>
#include <kiconloader.h>

using namespace KMPlayer;

TopPlayItem *PlayItem::rootItem ()
{
    PlayItem *r = nullptr;
    for (PlayItem *p = this; p->parent_item; p = p->parent_item)
        r = p;
    return static_cast <TopPlayItem *> (r);
}

Qt::ItemFlags TopPlayItem::itemFlags ()
{
    Qt::ItemFlags itemflags = Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;
    if (root_flags & PlayModel::AllowDrag)
        itemflags |= Qt::ItemIsDragEnabled;
    if (root_flags & PlayModel::InPlaceEdit)
        itemflags |= Qt::ItemIsEditable;
    return itemflags;
}

//-----------------------------------------------------------------------------

struct TreeUpdate {
    TreeUpdate (TopPlayItem *ri, NodePtr n, bool s, bool o, SharedPtr <TreeUpdate> &nx) : root_item (ri), node (n), select (s), open (o), next (nx) {}
    ~TreeUpdate () {}
    TopPlayItem * root_item;
    NodePtrW node;
    bool select;
    bool open;
    SharedPtr <TreeUpdate> next;
};

PlayModel::PlayModel (QObject *parent, KIconLoader *loader)
  : QAbstractItemModel (parent),
    auxiliary_pix (loader->loadIcon (QString ("folder-grey"), KIconLoader::Small)),
    config_pix (loader->loadIcon (QString ("configure"), KIconLoader::Small)),
    folder_pix (loader->loadIcon (QString ("folder"), KIconLoader::Small)),
    img_pix (loader->loadIcon (QString ("image-png"), KIconLoader::Small)),
    info_pix (loader->loadIcon (QString ("dialog-info"), KIconLoader::Small)),
    menu_pix (loader->loadIcon (QString ("view-media-playlist"), KIconLoader::Small)),
    unknown_pix (loader->loadIcon (QString ("unknown"), KIconLoader::Small)),
    url_pix (loader->loadIcon (QString ("internet-web-browser"), KIconLoader::Small)),
    video_pix (loader->loadIcon (QString ("video-x-generic"), KIconLoader::Small)),
    root_item (new PlayItem ((Node *)nullptr, nullptr)),
    last_id (0)
{
    TopPlayItem *ritem = new TopPlayItem (this,
            0, nullptr, PlayModel::AllowDrops | PlayModel::TreeEdit);
    ritem->parent_item = root_item;
    root_item->child_items.append (ritem);
    ritem->icon = url_pix;
}

PlayModel::~PlayModel ()
{
    delete root_item;
}

QVariant PlayModel::data (const QModelIndex &index, int role) const
{
    if (!index.isValid ())
        return QVariant ();

    PlayItem *item = static_cast<PlayItem*> (index.internalPointer ());
    switch (role) {
    case Qt::DisplayRole:
        return item->title;

    case Qt::DecorationRole:
        if (item->parent () == root_item)
            return static_cast <TopPlayItem *> (item)->icon;
        if (item->attribute)
            return config_pix;
        if (item->childCount() > 0)
            if (item->child (0)->attribute)
                return menu_pix;
        if (item->node) {
            Node::PlayType pt = item->node->playType ();
            switch (pt) {
            case Node::play_type_image:
                return img_pix;
            case Node::play_type_info:
                return info_pix;
            default:
                if (pt > Node::play_type_none)
                    return video_pix;
                else
                    return item->childCount ()
                        ? item->node->auxiliaryNode ()
                          ? auxiliary_pix : folder_pix
                          : unknown_pix;
            }
        }
        return unknown_pix;

    case UrlRole:
        if (item->node) {
            Mrl *mrl = item->node->mrl ();
            if (mrl && !mrl->src.isEmpty ())
                return mrl->src;
        }
        return QVariant ();

    case Qt::EditRole:
        if (item->item_flags & Qt::ItemIsEditable)
            return item->title;

    default:
        return QVariant ();
    }
}

bool PlayModel::setData (const QModelIndex& i, const QVariant& v, int role)
{
    if (role != Qt::EditRole || !i.isValid ())
        return false;

    bool changed = false;
    PlayItem *item = static_cast <PlayItem *> (i.internalPointer ());
    QString ntext = v.toString ();

    TopPlayItem *ri = item->rootItem ();
    if (ri->show_all_nodes && item->attribute) {
        int pos = ntext.indexOf (QChar ('='));
        if (pos > -1) {
            item->attribute->setName (ntext.left (pos));
            item->attribute->setValue (ntext.mid (pos + 1));
        } else {
            item->attribute->setName (ntext);
            item->attribute->setValue (QString (""));
        }
        PlayItem *pi = item->parent ();
        if (pi && pi->node) {
            pi->node->document ()->m_tree_version++;
            pi->node->closed ();
        }
        changed = true;
    } else if (item->node) {
        PlaylistRole *title = (PlaylistRole *) item->node->role (RolePlaylist);
        if (title && !ri->show_all_nodes && title->editable) {
            if (ntext.isEmpty ()) {
                ntext = item->node->mrl ()
                    ? item->node->mrl ()->src
                    : title->caption ();
                changed = true;
            }
            if (title->caption () != ntext) {
                item->node->setNodeName (ntext);
                item->node->document ()->m_tree_version++;
                ntext = title->caption ();
                changed = true;
            }
            //} else { // restore damage ..
            // cannot update because of crashing, shouldn't get here anyhow
            //updateTree (ri, item->node, true);
        }
    }

    if (changed) {
        item->title = ntext;
        Q_EMIT dataChanged (i, i);
        return true;
    }
    return false;
}

Qt::ItemFlags PlayModel::flags (const QModelIndex &index) const
{
    if (!index.isValid ())
        return {};

    return static_cast<PlayItem*>(index.internalPointer())->item_flags;
}

QVariant PlayModel::headerData (int, Qt::Orientation, int) const
{
    return QVariant ();
}

QModelIndex PlayModel::index (int row, int col, const QModelIndex &parent) const
{
    if (!hasIndex(row, col, parent))
        return QModelIndex();

    PlayItem *parent_item;

    if (!parent.isValid())
        parent_item = root_item;
    else
        parent_item = static_cast<PlayItem*>(parent.internalPointer());

    PlayItem *childItem = parent_item->child (row);
    if (childItem)
        return createIndex (row, col, childItem);
    else
        return QModelIndex();
}

QModelIndex PlayModel::indexFromItem (PlayItem *item) const
{
    if (!item || item == root_item)
        return QModelIndex();

    return createIndex (item->row(), 0, item);
}

PlayItem *PlayModel::itemFromIndex (const QModelIndex& index) const
{
    if (!index.isValid ())
        return nullptr;
    return static_cast <PlayItem*> (index.internalPointer ());
}

QModelIndex PlayModel::parent (const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    PlayItem *childItem = static_cast <PlayItem*> (index.internalPointer ());
    PlayItem *parent_item = childItem->parent ();

    if (parent_item == root_item)
        return QModelIndex ();

    return createIndex (parent_item->row(), 0, parent_item);
}

bool PlayModel::hasChildren (const QModelIndex& parent) const
{
    if (parent.column() > 0)
        return false;

    if (!parent.isValid())
        return root_item->childCount();

    PlayItem *pitem = static_cast<PlayItem*>(parent.internalPointer());
    int count = pitem->childCount();
    if (!count
            && pitem->parent_item == root_item
            && static_cast <TopPlayItem *> (pitem)->id > 0
            && !pitem->node->mrl()->resolved) {
        return true;
    }
    return count;
}

int PlayModel::rowCount (const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        return root_item->childCount();

    PlayItem *pitem = static_cast<PlayItem*>(parent.internalPointer());
    int count = pitem->childCount();
    if (!count && pitem->parent_item == root_item) {
        TopPlayItem *ritem = static_cast <TopPlayItem *> (pitem);
        if (ritem->id > 0 && !pitem->node->mrl()->resolved) {
            pitem->node->defer ();
            if (!pitem->node->mrl()->resolved)
                return 0;
            PlayItem *curitem = nullptr;
            ritem->model->populate (ritem->node, nullptr, ritem, nullptr, &curitem);
            count = ritem->childCount();
            if (count) {
                ritem->model->beginInsertRows (parent, 0, count-1);
                ritem->model->endInsertRows ();
            }
        }
    }
    return count;
}

int PlayModel::columnCount (const QModelIndex&) const
{
    return 1;
}

void dumpTree( PlayItem *p, const QString &indent ) {
    qCDebug(LOG_KMPLAYER_COMMON, "%s%s", qPrintable(indent),qPrintable(p->title));
    for (int i=0; i < p->childCount(); i++)
        dumpTree(p->child(i), indent+"  ");
}

void TopPlayItem::add ()
{
    model->beginInsertRows (QModelIndex(), id, id);

    parent_item = model->root_item;
    if (id >= model->root_item->childCount ())
        model->root_item->child_items.append (this);
    else
        model->root_item->child_items.insert (id, this);

    model->endInsertRows();

    if (id !=row())
        qWarning("Invalid root tree");
}

void TopPlayItem ::remove ()
{
    model->beginRemoveRows (QModelIndex (), id, id);
    if (id < parent_item->childCount ())
        parent_item->child_items.takeAt (id);
    else
        qWarning( "TopPlayItem::remove");
    model->endRemoveRows();
}

PlayItem *PlayModel::populate (Node *e, Node *focus,
        TopPlayItem *root, PlayItem *pitem,
        PlayItem ** curitem)
{
    root->have_dark_nodes |= !e->role (RolePlaylist);
    if (pitem && !root->show_all_nodes && !e->role (RolePlaylist)) {
        for (Node *c = e->firstChild (); c; c = c->nextSibling ())
            populate (c, focus, root, pitem, curitem);
        return pitem;
    }
    PlayItem *item = root;
    if (pitem) {
        item = new PlayItem (e, pitem);
        pitem->appendChild (item);
    }
    item->item_flags |= root->itemFlags ();
    PlaylistRole *title = (PlaylistRole *) e->role (RolePlaylist);
    QString text (title ? title->caption () : "");
    if (text.isEmpty ()) {
        text = id_node_text == e->id ? e->nodeValue () : e->nodeName ();
        if (e->isDocument ())
            text = e->hasChildNodes () ? i18n ("unnamed") : i18n ("none");
    }
    item->title = text;
    if (title && !root->show_all_nodes && title->editable)
        item->item_flags |= Qt::ItemIsEditable;
    if (focus == e)
        *curitem = item;
    //if (e->active ())
        //scrollToItem (item);
    for (Node *c = e->firstChild (); c; c = c->nextSibling ())
        populate (c, focus, root, item, curitem);
    if (e->isElementNode ()) {
        Attribute *a = static_cast <Element *> (e)->attributes ().first ();
        if (a) {
            root->have_dark_nodes = true;
            if (root->show_all_nodes) {
                PlayItem *as = new PlayItem (e, item);
                item->appendChild (as);
                as->title = i18n ("[attributes]");
                for (; a; a = a->nextSibling ()) {
                    PlayItem * ai = new PlayItem (a, as);
                    as->appendChild (ai);
                    //pitem->setFlags(root->itemFlags() &=~Qt::ItemIsDragEnabled);
                    if (root->id > 0)
                        ai->item_flags |= Qt::ItemIsEditable;
                    ai->title = QString ("%1=%2").arg (
                                a->name ().toString ()).arg (a->value ());
                }
            }
        }
    }
        //if (root->flags & PlayModel::AllowDrag)
        //    item->setDragEnabled (true);
    return item;
}

int PlayModel::addTree (NodePtr doc, const QString &source, const QString &icon, int flags) {
    TopPlayItem *ritem = new TopPlayItem(this, ++last_id, doc, flags);
    ritem->source = source;
    ritem->icon = KIconLoader::global ()->loadIcon (icon, KIconLoader::Small);
    PlayItem *curitem = nullptr;
    populate (doc, nullptr, ritem, nullptr, &curitem);
    ritem->add ();
    return last_id;
}

void PlayModel::updateTree (int id, NodePtr root, NodePtr active,
        bool select, bool open) {
    // TODO, if root is same as rootitems->node and treeversion is the same
    // and show all nodes is unchanged then only update the cells
    int root_item_count = root_item->childCount ();
    TopPlayItem *ritem = nullptr;
    if (id == -1) { // wildcard id
        for (int i = 0; i < root_item_count; ++i) {
            ritem = static_cast<TopPlayItem*>(root_item->child (i));
            for (NodePtr n = root; n; n = n->parentNode ())
                if (n == ritem->node) {
                    root = n;
                    break;
                }
            if (root == ritem->node) {
                id = ritem->id;
                break;  // found based on matching (ancestor) node
            }
        }
    } else if (id < root_item_count) {
        ritem = static_cast<TopPlayItem*>(root_item->child (id));
        if (!root)
            root = ritem->node;
    }
    if (ritem) {
        ritem->node = root;
        bool need_timer = !tree_update;
        tree_update = new TreeUpdate (ritem, active, select, open, tree_update);
        if (need_timer)
            QTimer::singleShot (0, this, SLOT (updateTrees ()));
    } else
        qCDebug(LOG_KMPLAYER_COMMON) << "updateTree root item not found";
}

void PlayModel::updateTrees () {
    for (; tree_update; tree_update = tree_update->next) {
        Q_EMIT updating (indexFromItem (tree_update->root_item));
        PlayItem *cur = updateTree (tree_update->root_item, tree_update->node);
        Q_EMIT updated (indexFromItem (tree_update->root_item),
                indexFromItem (cur), tree_update->select, tree_update->open);
    }
}

PlayItem *PlayModel::updateTree (TopPlayItem *ritem, NodePtr active) {
    PlayItem *curitem = nullptr;

    ritem->remove ();
    ritem->deleteChildren ();
    if (ritem->node) {
        if (!ritem->show_all_nodes)
            for (NodePtr n = active; n; n = n->parentNode ()) {
                active = n;
                if (n->role (RolePlaylist))
                    break;
            }
        populate (ritem->node, active, ritem, nullptr, &curitem);
    }
    ritem->add ();

    return curitem;
}


#include <stdio.h>

