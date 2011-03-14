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

#include <stdio.h>

#include "config-kmplayer.h"
// include files for Qt
#include <qapplication.h>
#include <qclipboard.h>
#include <QMenu>
#include <QtGui/QIcon>
#include <qdrawutil.h>
#include <qpainter.h>
#include <qiconset.h>
#include <qpixmap.h>
#include <QAbstractItemDelegate>
#include <QDropEvent>
#include <qstyle.h>
#include <qtimer.h>
#include <QDropEvent>
#include <QPalette>
#include <qregexp.h>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QList>
#include <QtGui/QItemSelectionModel>

#include <kiconloader.h>
#include <kstandardaction.h>
#include <kfinddialog.h>
#include <kfind.h>
#include <kaction.h>
#include <klocale.h>
#include <kdebug.h>
#include <KActionCollection>

#include "playlistview.h"
#include "kmplayerview.h"
#include "kmplayercontrolpanel.h"

using namespace KMPlayer;

//-------------------------------------------------------------------------

TopPlayItem *PlayItem::rootItem ()
{
    PlayItem *r = NULL;
    for (PlayItem *p = this; p->parent_item; p = p->parent_item)
        r = p;
    return static_cast <TopPlayItem *> (r);
}

Qt::ItemFlags TopPlayItem::itemFlags ()
{
    Qt::ItemFlags itemflags = Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;
    if (root_flags & PlayListView::AllowDrag)
        itemflags |= Qt::ItemIsDragEnabled;
    if (root_flags & PlayListView::InPlaceEdit)
        itemflags |= Qt::ItemIsEditable;
    return itemflags;
}

//-----------------------------------------------------------------------------

struct KMPLAYER_NO_EXPORT TreeUpdate {
    KDE_NO_CDTOR_EXPORT TreeUpdate (TopPlayItem *ri, NodePtr n, bool s, bool o, SharedPtr <TreeUpdate> &nx) : root_item (ri), node (n), select (s), open (o), next (nx) {}
    KDE_NO_CDTOR_EXPORT ~TreeUpdate () {}
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
    root_item (new PlayItem ((Node *)NULL, NULL)),
    last_id (0)
{
    TopPlayItem *ritem = new TopPlayItem (this,
            0, NULL, PlayListView::AllowDrops | PlayListView::TreeEdit);
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
        int pos = ntext.find (QChar ('='));
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
        emit dataChanged (i, i);
        return true;
    }
    return false;
}

Qt::ItemFlags PlayModel::flags (const QModelIndex &index) const
{
    if (!index.isValid ())
        return 0;

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
        return NULL;
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

int PlayModel::rowCount (const QModelIndex &parent) const
{
    PlayItem *parent_item;
    if (parent.column() > 0)
        return 0;

    if (!parent.isValid())
        parent_item = root_item;
    else
        parent_item = static_cast<PlayItem*>(parent.internalPointer());

    return parent_item->childCount();
}

int PlayModel::columnCount (const QModelIndex&) const
{
    return 1;
}

void dumpTree( PlayItem *p, const QString &indent ) {
    qDebug( "%s%s", qPrintable(indent),qPrintable(p->title));
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
                as->title = i18n ("[attributes]");
                for (; a; a = a->nextSibling ()) {
                    PlayItem * ai = new PlayItem (a, as);
                    //pitem->setFlags(root->itemFlags() &=~Qt::ItemIsDragEnabled);
                    if (root->id > 0)
                        ai->item_flags |= Qt::ItemIsEditable;
                    ai->title = QString ("%1=%2").arg (
                                a->name ().toString ()).arg (a->value ());
                }
            }
        }
    }
        //if (root->flags & PlayListView::AllowDrag)
        //    item->setDragEnabled (true);
    return item;
}

int PlayModel::addTree (NodePtr doc, const QString &source, const QString &icon, int flags) {
    TopPlayItem *ritem = new TopPlayItem(this, ++last_id, doc, flags);
    ritem->source = source;
    ritem->icon = KIconLoader::global ()->loadIcon (icon, KIconLoader::Small);
    PlayItem *curitem = 0L;
    populate (doc, false, ritem, 0L, &curitem);
    ritem->add ();
    return last_id;
}

void PlayModel::updateTree (int id, NodePtr root, NodePtr active,
        bool select, bool open) {
    // TODO, if root is same as rootitems->node and treeversion is the same
    // and show all nodes is unchanged then only update the cells
    int root_item_count = root_item->childCount ();
    TopPlayItem *ritem = NULL;
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
        qDebug ("updateTree root item not found");
}

KDE_NO_EXPORT void PlayModel::updateTrees () {
    for (; tree_update; tree_update = tree_update->next) {
        emit updating (indexFromItem (tree_update->root_item));
        PlayItem *cur = updateTree (tree_update->root_item, tree_update->node);
        emit updated (indexFromItem (tree_update->root_item),
                indexFromItem (cur), tree_update->select, tree_update->open);
    }
}

PlayItem *PlayModel::updateTree (TopPlayItem *ritem, NodePtr active) {
    PlayItem *curitem = 0L;

    ritem->remove ();
    ritem->deleteChildren ();
    if (ritem->node) {
        if (!ritem->show_all_nodes)
            for (NodePtr n = active; n; n = n->parentNode ()) {
                active = n;
                if (n->role (RolePlaylist))
                    break;
            }
        populate (ritem->node, active, ritem, 0L, &curitem);
    }
    ritem->add ();

    return curitem;
}

//-----------------------------------------------------------------------------

namespace {

class ItemDelegate : public QAbstractItemDelegate
{
    QAbstractItemDelegate *default_item_delegate;
    PlayListView *playlist_view;
public:
    ItemDelegate (PlayListView *v, QAbstractItemDelegate *def)
        : QAbstractItemDelegate (v),
          default_item_delegate (def),
          playlist_view (v)
    {}
    QWidget *createEditor (QWidget *w, const QStyleOptionViewItem &o, const QModelIndex &i) const
    {
        return default_item_delegate->createEditor (w, o, i);
    }
    bool editorEvent (QEvent *e, QAbstractItemModel *m, const QStyleOptionViewItem &o, const QModelIndex &i)
    {
        return default_item_delegate->editorEvent (e, m, o, i);
    }
    bool eventFilter (QObject *editor, QEvent *event)
    {
        return default_item_delegate->eventFilter (editor, event);
    }
    void paint (QPainter *p, const QStyleOptionViewItem &o, const QModelIndex &i) const
    {
        playlist_view->paintCell (default_item_delegate, p, o, i);
    }
    void setEditorData (QWidget *e, const QModelIndex &i) const
    {
        default_item_delegate->setEditorData (e, i);
    }
    void setModelData (QWidget *e, QAbstractItemModel *m, const QModelIndex &i) const
    {
        default_item_delegate->setModelData (e, m, i);
    }
    QSize sizeHint (const QStyleOptionViewItem &o, const QModelIndex &i) const
    {
        QSize size = default_item_delegate->sizeHint (o, i);
        return QSize (size.width (), size.height () + 2);
    }
    void updateEditorGeometry (QWidget *e, const QStyleOptionViewItem &o, const QModelIndex &i) const
    {
        default_item_delegate->updateEditorGeometry (e, o, i);
    }
};

}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT PlayListView::PlayListView (QWidget *, View *view, KActionCollection * ac)
 : //QTreeView (parent),
   m_view (view),
   m_find_dialog (0L),
   m_active_color (30, 0, 255),
   last_drag_tree_id (0),
   m_ignore_expanded (false) {
    setHeaderHidden (true);
    setSortingEnabled (false);
    setAcceptDrops (true);
    setDragDropMode (DragDrop);
    setDropIndicatorShown (true);
    setDragDropOverwriteMode (false);
    setRootIsDecorated (false);
    setSelectionMode (SingleSelection);
    setSelectionBehavior (SelectItems);
    setIndentation (4);
    setModel (new PlayModel (this, KIconLoader::global ()));
    //setItemsExpandable (false);
    //setAnimated (true);
    setUniformRowHeights (true);
    setItemDelegateForColumn (0, new ItemDelegate (this, itemDelegate ()));
    QPalette palette;
    palette.setColor (foregroundRole(), QColor (0, 0, 0));
    palette.setColor (backgroundRole(), QColor (0xB2, 0xB2, 0xB2));
    setPalette (palette);
    m_itemmenu = new QMenu (this);
    m_find = KStandardAction::find (this, SLOT (slotFind ()), this);
    m_find_next = KStandardAction::findNext (this, SLOT(slotFindNext()), this);
    m_find_next->setEnabled (false);
    m_edit_playlist_item = ac->addAction ("edit_playlist_item");
    m_edit_playlist_item->setText (i18n ("Edit &item"));
    connect (m_edit_playlist_item, SIGNAL (triggered (bool)),
             this, SLOT (renameSelected ()));
    connect (this, SIGNAL (expanded (const QModelIndex&)),
             this, SLOT (slotItemExpanded (const QModelIndex&)));
    connect (model (), SIGNAL (updating (const QModelIndex &)),
             this, SLOT(modelUpdating (const QModelIndex &)));
    connect (model (), SIGNAL (updated (const QModelIndex&, const QModelIndex&, bool, bool)),
             this, SLOT(modelUpdated (const QModelIndex&, const QModelIndex&, bool, bool)));
    connect (selectionModel(), SIGNAL(currentChanged(QModelIndex,QModelIndex)),
             this, SLOT(slotCurrentItemChanged(QModelIndex,QModelIndex)));
}

KDE_NO_CDTOR_EXPORT PlayListView::~PlayListView () {
}

void PlayListView::paintCell (const QAbstractItemDelegate *def,
        QPainter *p, const QStyleOptionViewItem &o, const QModelIndex i)
{
    PlayItem *item = playModel ()->itemFromIndex (i);
    if (item) {
        TopPlayItem *ritem = item->rootItem ();
        if (ritem == item) {
            QStyleOptionViewItem option (o);
            if (currentIndex () == i) {
                // no highlighting for the top items
                option.palette.setColor (QPalette::Highlight,
                        topLevelWidget()->palette ().color (QPalette::Background));
                option.palette.setColor (QPalette::HighlightedText,
                        topLevelWidget()->palette ().color (QPalette::Foreground));
            } else {
                p->fillRect(o.rect, QBrush (topLevelWidget()->palette ().color (QPalette::Background)));
                option.palette.setColor (QPalette::Text,
                        topLevelWidget()->palette ().color (QPalette::Foreground));
            }
            option.font = topLevelWidget()->font ();
            def->paint (p, option, i);
            qDrawShadeRect (p, o.rect, option.palette, !isExpanded (i));
        } else {
            QStyleOptionViewItem option (o);
            option.palette.setColor (QPalette::Text,
                    item->node && item->node->state == Node::state_began
                    ? m_active_color
                    : palette ().color (foregroundRole ()));
            def->paint (p, option, i);
        }
    }
}

void PlayListView::modelUpdating (const QModelIndex &)
{
    m_ignore_expanded = true;
    QModelIndex index = selectionModel()->currentIndex ();
    if (index.isValid ())
        closePersistentEditor(index);
}

void PlayListView::modelUpdated (const QModelIndex& r, const QModelIndex& i, bool sel, bool exp)
{
    if (exp)
        setExpanded (r, true);
    if (i.isValid () && sel) {
        setCurrentIndex (i);
        scrollTo (i);
    }
    m_find_next->setEnabled (!!m_current_find_elm);
    TopPlayItem *ti = static_cast<TopPlayItem*>(playModel()->itemFromIndex(r));
    if (!ti->have_dark_nodes && ti->show_all_nodes && !m_view->editMode())
        toggleShowAllNodes (); // redo, because the user can't change it anymore
    m_ignore_expanded = false;
}

QModelIndex PlayListView::index (PlayItem *item) const
{
    return playModel ()->indexFromItem (item);
}

void PlayListView::selectItem (const QString & txt) {
    /*QTreeWidgetItem * item = selectedItem ();
    if (item && item->text (0) == txt)
        return;
    item = findItem (txt, 0);
    if (item) {
        item->setSelected (true);
        //ensureItemVisible (item);
    }*/
}

/*KDE_NO_EXPORT Q3DragObject * PlayListView::dragObject () {
    PlayItem * item = static_cast <PlayItem *> (selectedItem ());
    if (item && item->node) {
        QString txt = item->node->isPlayable ()
            ? item->node->mrl ()->src : item->node->outerXML ();
        Q3TextDrag * drag = new Q3TextDrag (txt, this);
        last_drag_tree_id = rootItem (item)->id;
        m_last_drag = item->node;
        drag->setIcon (*item->pixmap (0));
        if (!item->node->isPlayable ())
            drag->setSubtype ("xml");
        return drag;
    }
    return 0;
}*/

KDE_NO_EXPORT void PlayListView::setFont (const QFont & fnt) {
    //setTreeStepSize (QFontMetrics (fnt).boundingRect ('m').width ());
    QTreeView::setFont (fnt);
}

KDE_NO_EXPORT void PlayListView::contextMenuEvent (QContextMenuEvent *event)
{
    PlayItem *item = playModel ()->itemFromIndex (indexAt (event->pos ()));
    if (item) {
        if (item->node || item->attribute) {
            TopPlayItem *ritem = item->rootItem ();
            if (m_itemmenu->count () > 0) {
                m_find->setVisible (false);
                m_find_next->setVisible (false);
                m_itemmenu->clear ();
            }
            m_itemmenu->insertItem (KIcon ("edit-copy"),
                    i18n ("&Copy to Clipboard"),
                    this, SLOT (copyToClipboard ()), 0, 0);
            if (item->attribute ||
                    (item->node && (item->node->isPlayable () ||
                                    item->node->isDocument ()) &&
                     item->node->mrl ()->bookmarkable))
                m_itemmenu->insertItem (KIcon ("bookmark-new"),
                        i18n ("&Add Bookmark"),
                        this, SLOT (addBookMark ()), 0, 1);
            if (ritem->have_dark_nodes) {
                m_itemmenu->insertItem (i18n ("&Show all"),
                        this, SLOT (toggleShowAllNodes ()), 0, 2);
                m_itemmenu->setItemChecked (2, ritem->show_all_nodes);
            }
            if (item->item_flags & Qt::ItemIsEditable)
                m_itemmenu->addAction (m_edit_playlist_item);
            m_itemmenu->insertSeparator ();
            m_find->setVisible (true);
            m_find_next->setVisible (true);
            emit prepareMenu (item, m_itemmenu);
            m_itemmenu->exec (event->globalPos ());
        }
    } else {
        m_view->controlPanel ()->popupMenu->exec (event->globalPos ());
    }
}

void PlayListView::slotItemExpanded (const QModelIndex &index) {
    int chlds = model ()->rowCount (index);
    if (chlds > 0) {
        if (!m_ignore_expanded && chlds == 1)
            setExpanded (model ()->index (0, 0, index), true);
        scrollTo (model ()->index (chlds - 1, 0, index));
        scrollTo (index);
    }
}

TopPlayItem * PlayListView::rootItem (int id) const {
    PlayItem *root_item = playModel ()->rootItem ();
    return static_cast<TopPlayItem*>(root_item->child (id));
}

PlayItem *PlayListView::selectedItem () const {
    return playModel ()->itemFromIndex (currentIndex ());
}

void PlayListView::copyToClipboard () {
    PlayItem * item = selectedItem ();
    QString text = item->title;
    if (item->node) {
        Mrl * mrl = item->node->mrl ();
        if (mrl && !mrl->src.isEmpty ())
            text = mrl->src;
    }
    QApplication::clipboard()->setText (text);
}

void PlayListView::addBookMark () {
    PlayItem * item = selectedItem ();
    if (item->node) {
        Mrl * mrl = item->node->mrl ();
        KURL url (mrl ? mrl->src : QString (item->node->nodeName ()));
        emit addBookMark (mrl->title.isEmpty () ? url.prettyUrl () : mrl->title, url.url ());
    }
}

void PlayListView::toggleShowAllNodes () {
    PlayItem * cur_item = selectedItem ();
    if (cur_item) {
        TopPlayItem *ritem = cur_item->rootItem ();
        showAllNodes (ritem, !ritem->show_all_nodes);
    }
}

KDE_NO_EXPORT void PlayListView::showAllNodes(TopPlayItem *ri, bool show) {
    if (ri && ri->show_all_nodes != show) {
        PlayItem * cur_item = selectedItem ();
        ri->show_all_nodes = show;
        playModel()->updateTree (ri->id, ri->node, cur_item->node, true, false);
        if (m_current_find_elm &&
                ri->node->document() == m_current_find_elm->document() &&
                !ri->show_all_nodes) {
            if (!m_current_find_elm->role (RolePlaylist))
                m_current_find_elm = 0L;
            m_current_find_attr = 0L;
        }
    }
}

KDE_NO_EXPORT bool PlayListView::isDragValid (QDropEvent *event) {
    if (event->source() == this &&
            event->mimeData ()
                ->hasFormat ("application/x-qabstractitemmodeldatalist"))
        return true;
    if (event->mimeData()->hasFormat ("text/uri-list")) {
        KUrl::List uriList = KUrl::List::fromMimeData (event->mimeData ());
        if (!uriList.isEmpty ())
            return true;
    } else {
        QString text = event->mimeData ()->text ();
        if (!text.isEmpty () && KUrl (text).isValid ())
            return true;
    }
    return false;
}

KDE_NO_EXPORT void PlayListView::dragMoveEvent (QDragMoveEvent *event)
{
    PlayItem *itm = playModel ()->itemFromIndex (indexAt (event->pos ()));
    if (itm) {
        TopPlayItem *ritem = itm->rootItem ();
        if (ritem->itemFlags() & AllowDrops && isDragValid (event))
            event->accept ();
        else
            event->ignore();
    }
}

void PlayListView::dragEnterEvent (QDragEnterEvent *event)
{
    if (isDragValid (event))
        event->accept ();
    else
        event->ignore();
}

KDE_NO_EXPORT void PlayListView::dropEvent (QDropEvent *event) {
    PlayItem *itm = playModel ()->itemFromIndex (indexAt (event->pos ()));
    if (itm && itm->node) {
        TopPlayItem *ritem = itm->rootItem ();
        NodePtr n = itm->node;
        if (ritem->id > 0 || n->isDocument ()) {
            emit dropped (event, itm);
        } else {
            KUrl::List uris = KUrl::List::fromMimeData (event->mimeData());
            if (uris.isEmpty ()) {
                KUrl url (event->mimeData ()->text ());
                if (url.isValid ())
                    uris.push_back (url);
            }
            if (uris.size () > 0) {
                bool as_child = itm->node->hasChildNodes ();
                NodePtr d = n->document ();
                for (int i = uris.size (); i > 0; i--) {
                    Node * ni = new KMPlayer::GenericURL (d, uris[i-1].url ());
                    if (as_child)
                        n->insertBefore (ni, n->firstChild ());
                    else
                        n->parentNode ()->insertBefore (ni, n->nextSibling ());
                }
                PlayItem * citem = selectedItem ();
                NodePtr cn;
                if (citem)
                    cn = citem->node;
                m_ignore_expanded = true;
                citem = playModel()->updateTree (ritem, cn);
                modelUpdated (playModel()->indexFromItem(ritem), playModel()->indexFromItem(citem), true, false);
                m_ignore_expanded = false;
            }
        }
    }
}

KDE_NO_EXPORT void PlayListView::renameSelected () {
    QModelIndex i = currentIndex ();
    PlayItem *itm = playModel ()->itemFromIndex (i);
    if (itm && itm->item_flags & Qt::ItemIsEditable)
        edit (i);
}

KDE_NO_EXPORT void PlayListView::slotCurrentItemChanged (QModelIndex /*cur*/, QModelIndex)
{
    //TopPlayItem * ri = rootItem (qitem);
    //setItemsRenameable (ri && (ri->item_flagsTreeEdit) && ri != qitem);
}

KDE_NO_EXPORT void PlayListView::slotFind () {
    /*m_current_find_elm = 0L;
    if (!m_find_dialog) {
        m_find_dialog = new KFindDialog (this, KFind::CaseSensitive);
        m_find_dialog->setHasSelection (false);
        connect(m_find_dialog, SIGNAL(okClicked ()), this, SLOT(slotFindOk ()));
    } else
        m_find_dialog->setPattern (QString ());
    m_find_dialog->show ();*/
}

/*static QTreeWidgetItem * findNodeInTree (NodePtr n, QTreeWidgetItem * item) {
    //kDebug () << "item:" << item->text (0) << " n:" << (n ? n->nodeName () : "null" );
    PlayItem * pi = static_cast <PlayItem *> (item);
    if (!n || !pi->node)
        return 0L;
    if (n == pi->node)
        return item;
    for (int i = 0; i < item->childCount (); ++i) {
        //kDebug () << "ci:" << ci->text (0) << " n:" << n->nodeName ();
        QTreeWidgetItem *vi = findNodeInTree (n, item->child (i));
        if (vi)
            return vi;
    }
    return 0L;

}*/

KDE_NO_EXPORT void PlayListView::slotFindOk () {
    /*if (!m_find_dialog)
        return;
    m_find_dialog->hide ();
    long opt = m_find_dialog->options ();
    current_find_tree_id = 0;
    if (opt & KFind::FromCursor && currentItem ()) {
        PlayItem * lvi = selectedItem ();
        if (lvi && lvi->node) {
             m_current_find_elm = lvi->node;
             current_find_tree_id = rootItem (lvi)->id;
        } else if (lvi && lvi->attribute) {
            PlayItem*pi=static_cast<PlayItem*>(currentItem()->parent());
            if (pi) {
                m_current_find_attr = lvi->attribute;
                m_current_find_elm = pi->node;
            }
        }
    } else if (!(opt & KFind::FindIncremental))
        m_current_find_elm = 0L;
    if (!m_current_find_elm && topLevelItemCount ())
        m_current_find_elm = static_cast <PlayItem*>(topLevelItem(0))->node;
    if (m_current_find_elm)
        slotFindNext ();*/
}

/* A bit tricky, but between the find's PlayItems might be gone, so
 * try to match on the generated tree following the source's document tree
 */
KDE_NO_EXPORT void PlayListView::slotFindNext () {
    /*if (!m_find_dialog)
        return;
    QString str = m_find_dialog->pattern();
    if (!m_current_find_elm || str.isEmpty ())
        return;
    long opt = m_find_dialog->options ();
    QRegExp regexp;
    if (opt & KFind::RegularExpression)
        regexp = QRegExp (str);
    bool cs = (opt & KFind::CaseSensitive);
    bool found = false;
    Node *node = NULL, *n = m_current_find_elm.ptr ();
    TopPlayItem * ri = rootItem (current_find_tree_id);
    while (!found && n) {
        if (ri->show_all_nodes || n->role (RolePlaylist)) {
            bool elm = n->isElementNode ();
            QString val = n->nodeName ();
            if (elm && !ri->show_all_nodes) {
                Mrl * mrl = n->mrl ();
                if (mrl) {
                    if (mrl->title.isEmpty ()) {
                        if (!mrl->src.isEmpty())
                            val = KURL(mrl->src).prettyUrl();
                    } else
                        val = mrl->title;
                }
            } else if (!elm)
                val = n->nodeValue ();
            if (((opt & KFind::RegularExpression) &&
                    val.find (regexp, 0) > -1) ||
                    (!(opt & KFind::RegularExpression) &&
                     val.find (str, 0, cs) > -1)) {
                node = n;
                m_current_find_attr = 0L;
                found = true;
            } else if (elm && ri->show_all_nodes) {
                for (Attribute *a = static_cast <Element *> (n)->attributes ().first (); a; a = a->nextSibling ()) {
                    QString attr = a->name ().toString ();
                    if (((opt & KFind::RegularExpression) &&
                                (attr.find (regexp, 0) || a->value ().find (regexp, 0) > -1)) ||
                                (!(opt & KFind::RegularExpression) &&
                                 (attr.find (str, 0, cs) > -1 || a->value ().find (str, 0, cs) > -1))) {
                        node = n;
                        m_current_find_attr = a;
                        found = true;
                        break;
                    }
                }
            }
        }
        if (n) { //set pointer to next
            if (opt & KFind::FindBackwards) {
                if (n->lastChild ()) {
                    n = n->lastChild ();
                } else if (n->previousSibling ()) {
                    n = n->previousSibling ();
                } else {
                    for (n = n->parentNode (); n; n = n->parentNode ())
                        if (n->previousSibling ()) {
                            n = n->previousSibling ();
                            break;
                        }
                    while (!n && current_find_tree_id > 0) {
                        ri = rootItem (--current_find_tree_id);
                        if (ri)
                            n = ri->node;
                    }
                }
            } else {
                if (n->firstChild ()) {
                    n = n->firstChild ();
                } else if (n->nextSibling ()) {
                    n = n->nextSibling ();
                } else {
                    for (n = n->parentNode (); n; n = n->parentNode ())
                        if (n->nextSibling ()) {
                            n = n->nextSibling ();
                            break;
                        }
                    while (!n) {
                        ri = rootItem (++current_find_tree_id);
                        if (!ri)
                            break;
                        n = ri->node;
                    }
                }
            }
        }
    }
    m_current_find_elm = n;
    kDebug () << " search for " << str << "=" << (node ? node->nodeName () : "not found") << " next:" << (n ? n->nodeName () : " not found");
    if (found) {
        QTreeWidgetItem *fc = findNodeInTree (node, ri);
        if (!fc) {
            m_current_find_elm = 0L;
            kDebug () << "node not found in tree tree:" << ri->id;
        } else {
            fc->setSelected (true);
            if (m_current_find_attr && fc->childCount () && fc->child (0)->childCount ())
                scrollToItem (fc->child (0)->child (0));
            scrollToItem (fc);
        }
    }
    m_find_next->setEnabled (!!m_current_find_elm);*/
}

#include "playlistview.moc"
