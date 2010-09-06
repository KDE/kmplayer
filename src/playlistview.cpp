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

KDE_NO_CDTOR_EXPORT
PlayListItem::PlayListItem (QTreeWidgetItem *p, Node *e, PlayListView *v)
    : QTreeWidgetItem (p, QTreeWidgetItem::UserType), node (e), listview (v)
{}

KDE_NO_CDTOR_EXPORT
PlayListItem::PlayListItem (QTreeWidgetItem *p, Attribute *a, PlayListView *v)
    : QTreeWidgetItem (p, QTreeWidgetItem::UserType), m_attr (a), listview (v)
{}

KDE_NO_CDTOR_EXPORT
PlayListItem::PlayListItem (PlayListView *v, Node *d, int type)
  : QTreeWidgetItem (type), node (d), listview (v)
{}

KDE_NO_CDTOR_EXPORT void PlayListItem::paintBranches (QPainter * p, const QColorGroup &, int w, int, int h) {
    p->fillRect (0, 0, w, h, listview->palette ().color (QPalette::Background));
}

KDE_NO_EXPORT void PlayListItem::setRenameEnabled (bool enable)
{
    setFlags (enable
            ? flags () | Qt::ItemIsEditable
            : flags () & ~Qt::ItemIsEditable);
}

QTreeWidgetItem *PlayListItem::clone () const
{
    PlayListItem *item = new PlayListItem (listview, node.ptr (), QTreeWidgetItem::UserType);
    item->m_attr = m_attr;
    int child_count = childCount ();
    for (int i = 0; i < child_count; ++i)
        item->addChild (child (i)->clone ());
    return item;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
RootPlayListItem::RootPlayListItem (int _id, PlayListView *v, Node *e, int flgs)
 : PlayListItem (v, e, QTreeWidgetItem::UserType+1),
   id (_id),
   flags (flgs),
   show_all_nodes (false),
   have_dark_nodes (false) {
    setForeground (0, QBrush (listview->topLevelWidget()->palette ().color (QPalette::Foreground)));
    setBackground (0, QBrush (listview->topLevelWidget()->palette ().color (QPalette::Background)));
    v->addTopLevelItem (this);
}

KDE_NO_CDTOR_EXPORT
RootPlayListItem::RootPlayListItem (int _id, PlayListView *v, Node *e, int index, int flgs)
 : PlayListItem (v, e, QTreeWidgetItem::UserType+1),
   id (_id),
   flags (flgs),
   show_all_nodes (false),
   have_dark_nodes (false) {
    setForeground (0, QBrush (listview->topLevelWidget()->palette ().color (QPalette::Foreground)));
    setBackground (0, QBrush (listview->topLevelWidget()->palette ().color (QPalette::Background)));
    v->insertTopLevelItem (index, this);
}

KDE_NO_CDTOR_EXPORT
RootPlayListItem::RootPlayListItem (const RootPlayListItem &item)
 : PlayListItem (item.listview, item.node.ptr (), QTreeWidgetItem::UserType+1),
    id (item.id),
    flags (item.flags),
    show_all_nodes (item.show_all_nodes),
    have_dark_nodes (item.have_dark_nodes)
{
    setForeground (0, QBrush (listview->topLevelWidget()->palette ().color (QPalette::Foreground)));
    setBackground (0, QBrush (listview->topLevelWidget()->palette ().color (QPalette::Background)));
}

Qt::ItemFlags RootPlayListItem::itemFlags ()
{
    Qt::ItemFlags itemflags = Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsEnabled;
    if (flags & PlayListView::AllowDrag)
        itemflags |= Qt::ItemIsDragEnabled;
    if (flags & PlayListView::InPlaceEdit)
        itemflags |= Qt::ItemIsEditable;
    return itemflags;
}

QTreeWidgetItem *RootPlayListItem::clone () const
{
    RootPlayListItem *item = new RootPlayListItem (*this);
    int child_count = childCount ();
    for (int i = 0; i < child_count; ++i)
        item->addChild (child (i)->clone ());
    return item;
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
 : //QTreeWidget (parent),
   m_view (view),
   m_find_dialog (0L),
   m_active_color (30, 0, 255),
   last_id (0),
   last_drag_tree_id (0),
   m_ignore_expanded (false),
   m_ignore_update (false) {
    setColumnCount (1);
    headerItem ()->setHidden (true);
    setSortingEnabled (false);
    setAcceptDrops (true);
    setDragDropMode (DragDrop);
    setDropIndicatorShown (true);
    setDragDropOverwriteMode (false);
    setRootIsDecorated (false);
    setSelectionMode (SingleSelection);
    setIndentation (4);
    //setItemsExpandable (false);
    //setAnimated (true);
    setUniformRowHeights (true);
    setItemDelegateForColumn (0, new ItemDelegate (this, itemDelegate ()));
    QPalette palette;
    palette.setColor (foregroundRole(), QColor (0, 0, 0));
    palette.setColor (backgroundRole(), QColor (0xB2, 0xB2, 0xB2));
    setPalette (palette);
    m_itemmenu = new QMenu (this);
    folder_pix = KIconLoader::global ()->loadIcon (QString ("folder"), KIconLoader::Small);
    auxiliary_pix = KIconLoader::global ()->loadIcon (QString ("folder-grey"), KIconLoader::Small);
    video_pix = KIconLoader::global ()->loadIcon (QString ("video-x-generic"), KIconLoader::Small);
    info_pix = KIconLoader::global ()->loadIcon (QString ("dialog-info"), KIconLoader::Small);
    img_pix = KIconLoader::global ()->loadIcon (QString ("image-png"), KIconLoader::Small);
    unknown_pix = KIconLoader::global ()->loadIcon (QString ("unknown"), KIconLoader::Small);
    menu_pix = KIconLoader::global ()->loadIcon (QString ("view-media-playlist"), KIconLoader::Small);
    config_pix = KIconLoader::global ()->loadIcon (QString ("configure"), KIconLoader::Small);
    url_pix = KIconLoader::global ()->loadIcon (QString ("internet-web-browser"), KIconLoader::Small);
    m_find = KStandardAction::find (this, SLOT (slotFind ()), this);
    m_find_next = KStandardAction::findNext (this, SLOT(slotFindNext()), this);
    m_find_next->setEnabled (false);
    m_edit_playlist_item = ac->addAction ("edit_playlist_item");
    m_edit_playlist_item->setText (i18n ("Edit &item"));
    connect (m_edit_playlist_item, SIGNAL (triggered (bool)),
             this, SLOT (renameSelected ()));
    connect (this, SIGNAL (itemExpanded (QTreeWidgetItem *)),
             this, SLOT (slotItemExpanded (QTreeWidgetItem *)));
    connect (this, SIGNAL (itemChanged (QTreeWidgetItem *, int)),
             this, SLOT (slotItemChanged (QTreeWidgetItem *, int)));
    connect (this, SIGNAL (currentItemChanged (QTreeWidgetItem *, QTreeWidgetItem *)),
             this, SLOT (slotCurrentItemChanged (QTreeWidgetItem *, QTreeWidgetItem *)));
}

KDE_NO_CDTOR_EXPORT PlayListView::~PlayListView () {
}

void PlayListView::paintCell (const QAbstractItemDelegate *def,
        QPainter *p, const QStyleOptionViewItem &o, const QModelIndex i)
{
    PlayListItem *item = static_cast <PlayListItem *> (itemFromIndex (i));
    if (item) {
        RootPlayListItem *ritem = rootItem (item);
        if (ritem == item) {
            QStyleOptionViewItem option (o);
            if (item->isSelected ()) {
                // no highlighting for the top items
                option.palette.setColor (QPalette::Highlight,
                        topLevelWidget()->palette ().color (QPalette::Background));
                option.palette.setColor (QPalette::HighlightedText,
                        topLevelWidget()->palette ().color (QPalette::Foreground));
            }
            option.font = topLevelWidget()->font ();
            def->paint (p, option, i);
            qDrawShadeRect (p, o.rect, option.palette, !item->isExpanded ());
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

int PlayListView::addTree (NodePtr root, const QString & source, const QString & icon, int flags) {
    //kDebug () << "addTree " << source << " " << root->mrl()->src;
    RootPlayListItem *ritem = new RootPlayListItem (++last_id, this, root, flags);
    ritem->source = source;
    ritem->icon = icon;
    ritem->setIcon (0, !ritem->icon.isEmpty ()
            ? KIconLoader::global ()->loadIcon (ritem->icon, KIconLoader::Small)
            : url_pix);
    updateTree (ritem, 0L, false);
    return last_id;
}

KDE_NO_EXPORT PlayListItem * PlayListView::populate
(Node *e, Node *focus, RootPlayListItem *root, PlayListItem * pitem, PlayListItem ** curitem) {
    root->have_dark_nodes |= !e->role (RolePlaylist);
    if (pitem && !root->show_all_nodes && !e->role (RolePlaylist)) {
        for (Node *c = e->firstChild (); c; c = c->nextSibling ())
            populate (c, focus, root, pitem, curitem);
        return pitem;
    }
    PlayListItem * item = pitem ? new PlayListItem (pitem, e, this) : root;
    item->setFlags (root->itemFlags ());
    PlaylistRole *title = (PlaylistRole *) e->role (RolePlaylist);
    QString text (title ? title->caption () : "");
    if (text.isEmpty ()) {
        text = id_node_text == e->id ? e->nodeValue () : e->nodeName ();
        if (e->isDocument ())
            text = e->hasChildNodes () ? i18n ("unnamed") : i18n ("none");
    }
    item->setText(0, text);
    if (title)
        item->setRenameEnabled (!root->show_all_nodes && title->editable);
    if (focus == e)
        *curitem = item;
    if (e->active ())
        scrollToItem (item);
    for (Node *c = e->firstChild (); c; c = c->nextSibling ())
        populate (c, focus, root, item, curitem);
    if (e->isElementNode ()) {
        Attribute *a = static_cast <Element *> (e)->attributes ().first ();
        if (a) {
            root->have_dark_nodes = true;
            if (root->show_all_nodes) {
                PlayListItem * as = new PlayListItem (item, e, this);
                as->setText (0, i18n ("[attributes]"));
                as->setIcon (0, menu_pix);
                for (; a; a = a->nextSibling ()) {
                    PlayListItem * ai = new PlayListItem (as, a, this);
                    pitem->setFlags(root->itemFlags() &=~Qt::ItemIsDragEnabled);
                    if (root->id > 0)
                        ai->setRenameEnabled (true);
                    ai->setText (0, QString ("%1=%2").arg (
                                a->name ().toString ()).arg (a->value ()));
                    ai->setIcon (0, config_pix);
                }
            }
        }
    }
    if (item != root) {
        Node::PlayType pt = e->playType ();
        QPixmap * pix;
        switch (pt) {
            case Node::play_type_image:
                pix = &img_pix;
                break;
            case Node::play_type_info:
                pix = &info_pix;
                break;
            default:
                if (pt > Node::play_type_none)
                    pix = &video_pix;
                else
                    pix = item->childCount ()
                        ? e->auxiliaryNode ()
                          ? &auxiliary_pix : &folder_pix
                          : &unknown_pix;
        }
        item->setIcon (0, *pix);
        //if (root->flags & PlayListView::AllowDrag)
        //    item->setDragEnabled (true);
    }
    return item;
}

void PlayListView::updateTree (int id, NodePtr root, NodePtr active,
        bool select, bool open) {
    // TODO, if root is same as rootitems->node and treeversion is the same
    // and show all nodes is unchanged then only update the cells
    //QWidget * w = focusWidget ();
    //if (w && w != this)
    //    w->clearFocus ();
    //setSelected (firstChild (), true);
    m_ignore_update = true;
    //setAnimated (false);
    RootPlayListItem *ritem = NULL;
    int index = 0;
    for (int i = 0; i < topLevelItemCount (); ++i) {
        ritem = static_cast<RootPlayListItem*>(topLevelItem(i));
        if (ritem->id == id) {
            if (!root)
                root = ritem->node;
            break;  // found based on id
        }
        if (id == -1) { // wildcard id
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
        if (ritem->id < id)
            index++;
    }
    if (!root) {
        delete ritem;
    } else {
        if (!ritem || ritem->id != id) {
            ritem = new RootPlayListItem (id, this, root, index, AllowDrops|TreeEdit);
            ritem->setIcon (0, url_pix);
        } else {
            ritem->node = root;
        }
        m_find_next->setEnabled (!!m_current_find_elm);
        bool need_timer = !tree_update;
        tree_update = new TreeUpdate (ritem, active, select, open, tree_update);
        if (need_timer)
            QTimer::singleShot (0, this, SLOT (updateTrees ()));
    }
    m_ignore_update = false;
    //setAnimated (true);
}

KDE_NO_EXPORT void PlayListView::updateTrees () {
    for (; tree_update; tree_update = tree_update->next) {
        updateTree (tree_update->root_item, tree_update->node, tree_update->select);
        if (tree_update->open) // FIXME for non-root nodes lazy loading
            tree_update->root_item->setExpanded (true);
    }
}

void PlayListView::updateTree (RootPlayListItem * ritem, NodePtr active, bool select) {
    bool set_open = ritem->id == 0 || (ritem ? ritem->isExpanded () : false);
    m_ignore_expanded = true;
    m_ignore_update = true;
    //setAnimated (false);
    PlayListItem * curitem = 0L;
    while (ritem->childCount ())
        delete ritem->child (0);
    if (ritem->node) {
        if (!ritem->show_all_nodes)
            for (NodePtr n = active; n; n = n->parentNode ()) {
                active = n;
                if (n->role (RolePlaylist))
                    break;
            }
        populate (ritem->node, active, ritem, 0L, &curitem);
        if (set_open && ritem->childCount () && !ritem->isExpanded ())
            ritem->setExpanded (true);
        if (curitem && select) {
            curitem->setSelected (true);
            scrollToItem (curitem);
        }
        if (!ritem->have_dark_nodes && ritem->show_all_nodes && !m_view->editMode())
            toggleShowAllNodes (); // redo, because the user can't change it anymore
    }
    m_ignore_expanded = false;
    m_ignore_update = false;
    //setAnimated (true);
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
    PlayListItem * item = static_cast <PlayListItem *> (selectedItem ());
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
    QTreeWidget::setFont (fnt);
}

KDE_NO_EXPORT void PlayListView::contextMenuEvent (QContextMenuEvent *event)
{
    PlayListItem *item = static_cast <PlayListItem *> (itemAt (event->pos ()));
    if (item) {
        if (item->node || item->m_attr) {
            RootPlayListItem * ritem = rootItem (item);
            if (m_itemmenu->count () > 0) {
                m_find->setVisible (false);
                m_find_next->setVisible (false);
                m_itemmenu->clear ();
            }
            m_itemmenu->insertItem (KIcon ("edit-copy"),
                    i18n ("&Copy to Clipboard"),
                    this, SLOT (copyToClipboard ()), 0, 0);
            if (item->m_attr ||
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
            if (item->flags () & Qt::ItemIsEditable)
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

void PlayListView::slotItemExpanded (QTreeWidgetItem *item) {
    if (!m_ignore_expanded && item->childCount () == 1) {
        PlayListItem * child_item = static_cast<PlayListItem*>(item->child (0));
        child_item->setExpanded (rootItem (item)->show_all_nodes ||
                (child_item->node && child_item->node->role (RolePlaylist)));
    }
    scrollTo (indexFromItem (item->child (item->childCount () - 1)));
    scrollTo (indexFromItem (item));
}

RootPlayListItem *PlayListView::rootItem (QTreeWidgetItem *item) const {
    if (!item)
        return 0L;
    while (item->parent ())
        item = item->parent ();
    return static_cast <RootPlayListItem *> (item);
}

RootPlayListItem * PlayListView::rootItem (int id) const {
    for (int i = 0; i < topLevelItemCount (); ++i) {
        RootPlayListItem *ri = static_cast<RootPlayListItem*>(topLevelItem (i));
        if (ri->id == id)
            return ri;
    }
    return 0L;
}

PlayListItem *PlayListView::selectedItem () const {
    QList <QTreeWidgetItem *> sels = selectedItems ();
    if (sels.size ())
        return static_cast <PlayListItem *> (sels[0]);
    return NULL;
}

void PlayListView::copyToClipboard () {
    PlayListItem * item = currentPlayListItem ();
    QString text = item->text (0);
    if (item->node) {
        Mrl * mrl = item->node->mrl ();
        if (mrl && !mrl->src.isEmpty ())
            text = mrl->src;
    }
    QApplication::clipboard()->setText (text);
}

void PlayListView::addBookMark () {
    PlayListItem * item = currentPlayListItem ();
    if (item->node) {
        Mrl * mrl = item->node->mrl ();
        KURL url (mrl ? mrl->src : QString (item->node->nodeName ()));
        emit addBookMark (mrl->title.isEmpty () ? url.prettyUrl () : mrl->title, url.url ());
    }
}

void PlayListView::toggleShowAllNodes () {
    PlayListItem * cur_item = currentPlayListItem ();
    if (cur_item) {
        RootPlayListItem * ritem = rootItem (cur_item);
        showAllNodes (rootItem (cur_item), !ritem->show_all_nodes);
    }
}

KDE_NO_EXPORT void PlayListView::showAllNodes(RootPlayListItem *ri, bool show) {
    if (ri && ri->show_all_nodes != show) {
        PlayListItem * cur_item = currentPlayListItem ();
        ri->show_all_nodes = show;
        updateTree (ri->id, ri->node, cur_item->node, true, false);
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
    PlayListItem *item = static_cast <PlayListItem *> (itemAt (event->pos ()));
    if (item) {
        RootPlayListItem *ritem = rootItem (item);
        if (ritem->flags & AllowDrops && isDragValid (event))
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
    PlayListItem *item = static_cast <PlayListItem *> (itemAt (event->pos ()));
    if (item && item->node) {
        RootPlayListItem *ritem = rootItem (item);
        NodePtr n = item->node;
        if (ritem->id > 0 || n->isDocument ()) {
            emit dropped (event, item);
        } else {
            KUrl::List uris = KUrl::List::fromMimeData (event->mimeData());
            if (uris.isEmpty ()) {
                KUrl url (event->mimeData ()->text ());
                if (url.isValid ())
                    uris.push_back (url);
            }
            if (uris.size () > 0) {
                bool as_child = item->node->hasChildNodes ();
                NodePtr d = n->document ();
                for (int i = uris.size (); i > 0; i--) {
                    Node * ni = new KMPlayer::GenericURL (d, uris[i-1].url ());
                    if (as_child)
                        n->insertBefore (ni, n->firstChild ());
                    else
                        n->parentNode ()->insertBefore (ni, n->nextSibling ());
                }
                PlayListItem * citem = currentPlayListItem ();
                NodePtr cn;
                if (citem)
                    cn = citem->node;
                updateTree (ritem, cn, true);
            }
        }
    }
}

KDE_NO_EXPORT void PlayListView::slotItemChanged (QTreeWidgetItem *qitem, int) {
    if (!m_ignore_update) {
        PlayListItem *item = static_cast <PlayListItem *> (qitem);
        RootPlayListItem *ri = rootItem (qitem);
        if (ri->show_all_nodes && item->m_attr) {
            QString txt = item->text (0);
            int pos = txt.find (QChar ('='));
            if (pos > -1) {
                item->m_attr->setName (txt.left (pos));
                item->m_attr->setValue (txt.mid (pos + 1));
            } else {
                item->m_attr->setName (txt);
                item->m_attr->setValue (QString (""));
            }
            PlayListItem *pi = static_cast <PlayListItem *> (item->parent ());
            if (pi && pi->node) {
                pi->node->document ()->m_tree_version++;
                pi->node->closed ();
            }
        } else if (item->node) {
            PlaylistRole *title = (PlaylistRole *) item->node->role (RolePlaylist);
            if (title && !ri->show_all_nodes && title->editable) {
                QString ntext = item->text (0);
                if (ntext.isEmpty ()) {
                    ntext = item->node->mrl ()
                        ? item->node->mrl ()->src
                        : title->caption ();
                    m_ignore_update = true;
                    item->setText (0, ntext);
                    m_ignore_update = false;
                }
                if (title->caption () != ntext) {
                    title->setCaption (ntext);
                    item->node->setNodeName (ntext);
                    item->node->document ()->m_tree_version++;
                }
                //} else { // restore damage ..
                // cannot update because of crashing, shouldn't get here anyhow
                //updateTree (ri, item->node, true);
        }
        }
    }
}

KDE_NO_EXPORT void PlayListView::renameSelected () {
    PlayListItem *item = static_cast <PlayListItem *> (currentItem ());
    qDebug ("renameSelected %s %d", item->text (0).toAscii().data(), !!(item->flags () & Qt::ItemIsEditable));
    if (item->flags () & Qt::ItemIsEditable)
        item->listview->editItem (item, 0);
}

KDE_NO_EXPORT void PlayListView::slotCurrentItemChanged (
        QTreeWidgetItem * current, QTreeWidgetItem * previous )
{
    //RootPlayListItem * ri = rootItem (qitem);
    //setItemsRenameable (ri && (ri->flags & TreeEdit) && ri != qitem);
}

KDE_NO_EXPORT void PlayListView::slotFind () {
    m_current_find_elm = 0L;
    if (!m_find_dialog) {
        m_find_dialog = new KFindDialog (this, KFind::CaseSensitive);
        m_find_dialog->setHasSelection (false);
        connect(m_find_dialog, SIGNAL(okClicked ()), this, SLOT(slotFindOk ()));
    } else
        m_find_dialog->setPattern (QString ());
    m_find_dialog->show ();
}

static QTreeWidgetItem * findNodeInTree (NodePtr n, QTreeWidgetItem * item) {
    //kDebug () << "item:" << item->text (0) << " n:" << (n ? n->nodeName () : "null" );
    PlayListItem * pi = static_cast <PlayListItem *> (item);
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

}

KDE_NO_EXPORT void PlayListView::slotFindOk () {
    if (!m_find_dialog)
        return;
    m_find_dialog->hide ();
    long opt = m_find_dialog->options ();
    current_find_tree_id = 0;
    if (opt & KFind::FromCursor && currentItem ()) {
        PlayListItem * lvi = currentPlayListItem ();
        if (lvi && lvi->node) {
             m_current_find_elm = lvi->node;
             current_find_tree_id = rootItem (lvi)->id;
        } else if (lvi && lvi->m_attr) {
            PlayListItem*pi=static_cast<PlayListItem*>(currentItem()->parent());
            if (pi) {
                m_current_find_attr = lvi->m_attr;
                m_current_find_elm = pi->node;
            }
        }
    } else if (!(opt & KFind::FindIncremental))
        m_current_find_elm = 0L;
    if (!m_current_find_elm && topLevelItemCount ())
        m_current_find_elm = static_cast <PlayListItem*>(topLevelItem(0))->node;
    if (m_current_find_elm)
        slotFindNext ();
}

/* A bit tricky, but between the find's PlayListItems might be gone, so
 * try to match on the generated tree following the source's document tree
 */
KDE_NO_EXPORT void PlayListView::slotFindNext () {
    if (!m_find_dialog)
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
    RootPlayListItem * ri = rootItem (current_find_tree_id);
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
    m_find_next->setEnabled (!!m_current_find_elm);
}

#include "playlistview.moc"
