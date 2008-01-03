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
#include <qdrawutil.h>
#include <qpainter.h>
#include <qiconset.h>
#include <qpixmap.h>
#include <Q3Header>
#include <QDropEvent>
#include <qstyle.h>
#include <qtimer.h>
#include <Q3DragObject>
#include <QDropEvent>
#include <Q3TextDrag>
#include <QPalette>
#include <qregexp.h>

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

namespace KMPlayer {

    KDE_NO_EXPORT bool isDragValid (QDropEvent * de) {
        KUrl::List uriList = KUrl::List::fromMimeData (de->mimeData ());
        if (!uriList.isEmpty ())
            return true;
        if (Q3TextDrag::canDecode (de)) {
            QString text;
            if (Q3TextDrag::decode (de, text) && KURL (text).isValid ())
                return true;
        }
        return false;
    }
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT PlayListItem::PlayListItem (Q3ListViewItem *p, const NodePtr & e, PlayListView * lv) : Q3ListViewItem (p), node (e), listview (lv) {}

KDE_NO_CDTOR_EXPORT PlayListItem::PlayListItem (Q3ListViewItem *p, const AttributePtr & a, PlayListView * lv) : Q3ListViewItem (p), m_attr (a), listview (lv) {}

KDE_NO_CDTOR_EXPORT
PlayListItem::PlayListItem (PlayListView *v, const NodePtr &e, Q3ListViewItem *b)
  : Q3ListViewItem (v, b), node (e), listview (v) {}

KDE_NO_CDTOR_EXPORT void PlayListItem::paintCell (QPainter * p, const QColorGroup & cg, int column, int width, int align) {
    if (node && node->state == Node::state_began) {
        QColorGroup mycg (cg);
        mycg.setColor (QColorGroup::Foreground, listview->activeColor ());
        mycg.setColor (QColorGroup::Text, listview->activeColor ());
        Q3ListViewItem::paintCell (p, mycg, column, width, align);
    } else
        Q3ListViewItem::paintCell (p, cg, column, width, align);
}

KDE_NO_CDTOR_EXPORT void PlayListItem::paintBranches (QPainter * p, const QColorGroup &, int w, int, int h) {
    p->fillRect (0, 0, w, h, listview->palette ().color (QPalette::Background));
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
RootPlayListItem::RootPlayListItem (int _id, PlayListView *v, const NodePtr & e, Q3ListViewItem * before, int flgs)
  : PlayListItem (v, e, before),
    id (_id),
    flags (flgs),
    show_all_nodes (false),
    have_dark_nodes (false) {}

KDE_NO_CDTOR_EXPORT void RootPlayListItem::paintCell (QPainter * p, const QColorGroup & cg, int column, int width, int align) {
    QColorGroup mycg (cg);
    mycg.setColor (QColorGroup::Base,
            listview->topLevelWidget()->palette ().color (QPalette::Background));
    mycg.setColor (QColorGroup::Highlight, mycg.base ());
    mycg.setColor (QColorGroup::Text,
            listview->topLevelWidget()->palette ().color (QPalette::Foreground));
    mycg.setColor (QColorGroup::HighlightedText, mycg.text ());
    Q3ListViewItem::paintCell (p, mycg, column, width, align);
    qDrawShadeRect (p, 0, 0, width -1, height () -1, mycg, !isOpen ());
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT PlayListView::PlayListView (QWidget * parent, View * view, KActionCollection * ac)
 : //K3ListView (parent),
   m_view (view),
   m_find_dialog (0L),
   m_active_color (30, 0, 255),
   last_id (0),
   last_drag_tree_id (0),
   m_ignore_expanded (false) {
    addColumn (QString ());
    header()->hide ();
    //setRootIsDecorated (true);
    setSorting (-1);
    setAcceptDrops (true);
    setDropVisualizer (true);
    setItemsRenameable (true);
    setItemMargin (2);
    QPalette palette;
    palette.setColor (foregroundRole(), QColor (0, 0, 0));
    palette.setColor (backgroundRole(), QColor (0xB2, 0xB2, 0xB2));
    setPalette (palette);
    m_itemmenu = new QMenu (this);
    folder_pix = KIconLoader::global ()->loadIcon (QString ("folder"), KIconLoader::Small);
    auxiliary_pix = KIconLoader::global ()->loadIcon (QString ("folder_grey"), KIconLoader::Small);
    video_pix = KIconLoader::global ()->loadIcon (QString ("video-x-generic"), KIconLoader::Small);
    info_pix = KIconLoader::global ()->loadIcon (QString ("dialog-info"), KIconLoader::Small);
    img_pix = KIconLoader::global ()->loadIcon (QString ("colorize"), KIconLoader::Small);
    unknown_pix = KIconLoader::global ()->loadIcon (QString ("unknown"), KIconLoader::Small);
    menu_pix = KIconLoader::global ()->loadIcon (QString ("view-media-playlist"), KIconLoader::Small);
    config_pix = KIconLoader::global ()->loadIcon (QString ("configure"), KIconLoader::Small);
    url_pix = KIconLoader::global ()->loadIcon (QString ("internet-web-browser"), KIconLoader::Small);
    m_find = KStandardAction::find (this, SLOT (slotFind ()), this);
    m_find_next = KStandardAction::findNext (this, SLOT(slotFindNext()), this);
    m_find_next->setEnabled (false);
    connect (this, SIGNAL (contextMenuRequested (Q3ListViewItem *, const QPoint &, int)), this, SLOT (contextMenuItem (Q3ListViewItem *, const QPoint &, int)));
    connect (this, SIGNAL (expanded (Q3ListViewItem *)),
             this, SLOT (itemExpanded (Q3ListViewItem *)));
    connect (this, SIGNAL (dropped (QDropEvent *, Q3ListViewItem *)),
             this, SLOT (itemDropped (QDropEvent *, Q3ListViewItem *)));
    connect (this, SIGNAL (itemRenamed (Q3ListViewItem *)),
             this, SLOT (itemIsRenamed (Q3ListViewItem *)));
    connect (this, SIGNAL (selectionChanged (Q3ListViewItem *)),
             this, SLOT (itemIsSelected (Q3ListViewItem *)));
}

KDE_NO_CDTOR_EXPORT PlayListView::~PlayListView () {
}

int PlayListView::addTree (NodePtr root, const QString & source, const QString & icon, int flags) {
    //kDebug () << "addTree " << source << " " << root->mrl()->src;
    RootPlayListItem * ritem = new RootPlayListItem (++last_id, this, root, lastChild(), flags);
    ritem->source = source;
    ritem->icon = icon;
    ritem->setPixmap (0, !ritem->icon.isEmpty ()
            ? KIconLoader::global ()->loadIcon (ritem->icon, KIconLoader::Small)
            : url_pix);
    updateTree (ritem, 0L, false);
    return last_id;
}

KDE_NO_EXPORT PlayListItem * PlayListView::populate
(NodePtr e, NodePtr focus, RootPlayListItem *root, PlayListItem * pitem, PlayListItem ** curitem) {
    root->have_dark_nodes |= !e->expose ();
    if (pitem && !root->show_all_nodes && !e->expose ()) {
        for (NodePtr c = e->lastChild (); c; c = c->previousSibling ())
            populate (c, focus, root, pitem, curitem);
        return pitem;
    }
    PlayListItem * item = pitem ? new PlayListItem (pitem, e, this) : root;
    Mrl * mrl = e->mrl ();
    QString text (e->nodeName());
    if (mrl && !root->show_all_nodes) {
        if (mrl->pretty_name.isEmpty ()) {
            if (!mrl->src.isEmpty())
                text = KUrl (mrl->src).pathOrUrl ();
            else if (e->isDocument ())
                text = e->hasChildNodes () ? i18n ("unnamed") : i18n ("none");
        } else
            text = mrl->pretty_name;
    } else if (e->id == id_node_text)
        text = e->nodeValue ();
    item->setText(0, text);
    if (focus == e)
        *curitem = item;
    if (e->active ())
        ensureItemVisible (item);
    for (NodePtr c = e->lastChild (); c; c = c->previousSibling ())
        populate (c, focus, root, item, curitem);
    if (e->isElementNode ()) {
        AttributePtr a = convertNode<Element> (e)->attributes ()->first ();
        if (a) {
            root->have_dark_nodes = true;
            if (root->show_all_nodes) {
                PlayListItem * as = new PlayListItem (item, e, this);
                as->setText (0, i18n ("[attributes]"));
                as->setPixmap (0, menu_pix);
                for (; a; a = a->nextSibling ()) {
                    PlayListItem * ai = new PlayListItem (as, a, this);
                    ai->setText (0, QString ("%1=%2").arg (
                                a->name ().toString ()).arg (a->value ()));
                    ai->setPixmap (0, config_pix);
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
                    pix = item->firstChild ()
                        ? e->auxiliaryNode ()
                          ? &auxiliary_pix : &folder_pix
                          : &unknown_pix;
        }
        item->setPixmap (0, *pix);
        if (root->flags & PlayListView::AllowDrag)
            item->setDragEnabled (true);
    }
    return item;
}

void PlayListView::updateTree (int id, NodePtr root, NodePtr active,
        bool select, bool open) {
    // TODO, if root is same as rootitems->node and treeversion is the same
    // and show all nodes is unchanged then only update the cells
    QWidget * w = focusWidget ();
    if (w && w != this)
        w->clearFocus ();
    //setSelected (firstChild (), true);
    RootPlayListItem * ritem = static_cast <RootPlayListItem *> (firstChild ());
    RootPlayListItem * before = 0L;
    for (; ritem; ritem =static_cast<RootPlayListItem*>(ritem->nextSibling())) {
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
            before = ritem;
    }
    if (!root) {
        delete ritem;
        return;
    }
    if (!ritem) {
        ritem =new RootPlayListItem(id, this, root, before,AllowDrops|TreeEdit);
        ritem->setPixmap (0, url_pix);
    } else
        ritem->node = root;
    m_find_next->setEnabled (!!m_current_find_elm);
    bool need_timer = !tree_update;
    tree_update = new TreeUpdate (ritem, active, select, open, tree_update);
    if (need_timer)
        QTimer::singleShot (0, this, SLOT (updateTrees ()));
}

KDE_NO_EXPORT void PlayListView::updateTrees () {
    for (; tree_update; tree_update = tree_update->next) {
        updateTree (tree_update->root_item, tree_update->node, tree_update->select);
        if (tree_update->open) // FIXME for non-root nodes lazy loading
            setOpen (tree_update->root_item, true);
    }
}

void PlayListView::updateTree (RootPlayListItem * ritem, NodePtr active, bool select) {
    bool set_open = ritem->id == 0 || (ritem ? ritem->isOpen () : false);
    m_ignore_expanded = true;
    PlayListItem * curitem = 0L;
    while (ritem->firstChild ())
        delete ritem->firstChild ();
    if (!ritem->node)
        return;
    if (!ritem->show_all_nodes)
        for (NodePtr n = active; n; n = n->parentNode ()) {
            active = n;
            if (n->expose ())
                break;
        }
    populate (ritem->node, active, ritem, 0L, &curitem);
    if (set_open && ritem->firstChild () && !ritem->isOpen ())
        setOpen (ritem, true);
    if (curitem && select) {
        setSelected (curitem, true);
        ensureItemVisible (curitem);
    }
    if (!ritem->have_dark_nodes && ritem->show_all_nodes && !m_view->editMode())
        toggleShowAllNodes (); // redo, because the user can't change it anymore
    m_ignore_expanded = false;
}

void PlayListView::selectItem (const QString & txt) {
    Q3ListViewItem * item = selectedItem ();
    if (item && item->text (0) == txt)
        return;
    item = findItem (txt, 0);
    if (item) {
        setSelected (item, true);
        ensureItemVisible (item);
    }
}

KDE_NO_EXPORT Q3DragObject * PlayListView::dragObject () {
    PlayListItem * item = static_cast <PlayListItem *> (selectedItem ());
    if (item && item->node) {
        QString txt = item->node->isPlayable ()
            ? item->node->mrl ()->src : item->node->outerXML ();
        Q3TextDrag * drag = new Q3TextDrag (txt, this);
        last_drag_tree_id = rootItem (item)->id;
        m_last_drag = item->node;
        drag->setPixmap (*item->pixmap (0));
        if (!item->node->isPlayable ())
            drag->setSubtype ("xml");
        return drag;
    }
    return 0;
}

KDE_NO_EXPORT void PlayListView::setFont (const QFont & fnt) {
    setTreeStepSize (QFontMetrics (fnt).boundingRect ('m').width ());
    K3ListView::setFont (fnt);
}

KDE_NO_EXPORT void PlayListView::contextMenuItem (Q3ListViewItem * vi, const QPoint & p, int) {
    if (vi) {
        PlayListItem * item = static_cast <PlayListItem *> (vi);
        if (item->node || item->m_attr) {
            RootPlayListItem * ritem = rootItem (vi);
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
            m_itemmenu->insertSeparator ();
            m_find->setVisible (true);
            m_find_next->setVisible (true);
            emit prepareMenu (item, m_itemmenu);
            m_itemmenu->exec (p);
        }
    } else
        m_view->controlPanel ()->popupMenu->exec (p);
}

void PlayListView::itemExpanded (Q3ListViewItem * item) {
    if (!m_ignore_expanded && item->childCount () == 1) {
        PlayListItem * child_item = static_cast<PlayListItem*>(item->firstChild ());
        child_item->setOpen (rootItem (item)->show_all_nodes ||
                (child_item->node && child_item->node->expose ()));
    }
}

RootPlayListItem * PlayListView::rootItem (Q3ListViewItem * item) const {
    if (!item)
        return 0L;
    while (item->parent ())
        item = item->parent ();
    return static_cast <RootPlayListItem *> (item);
}

RootPlayListItem * PlayListView::rootItem (int id) const {
    RootPlayListItem * ri = static_cast <RootPlayListItem *> (firstChild ());
    for (; ri; ri = static_cast <RootPlayListItem *> (ri->nextSibling ())) {
        if (ri->id == id)
            return ri;
    }
    return 0L;
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
        emit addBookMark (mrl->pretty_name.isEmpty () ? url.prettyUrl () : mrl->pretty_name, url.url ());
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
            if (!m_current_find_elm->expose ())
                m_current_find_elm = 0L;
            m_current_find_attr = 0L;
        }
    }
}

KDE_NO_EXPORT bool PlayListView::acceptDrag (QDropEvent * de) const {
    Q3ListViewItem * item = itemAt (contentsToViewport (de->pos ()));
    if (item && (de->source () == this || isDragValid (de))) {
        RootPlayListItem * ritem = rootItem (item);
        return ritem->flags & AllowDrops;
    }
    return false;
}

KDE_NO_EXPORT void PlayListView::itemDropped (QDropEvent * de, Q3ListViewItem *after) {
    if (!after) { // could still be a descendent
        after = itemAt (contentsToViewport (de->pos ()));
        if (after)
            after = after->parent ();
    }
    if (after) {
        RootPlayListItem * ritem = rootItem (after);
        if (ritem->id > 0)
            return;
        NodePtr n = static_cast <PlayListItem *> (after)->node;
        bool valid = n && (!n->isDocument () || n->hasChildNodes ());
        KUrl::List uris = KUrl::List::fromMimeData (de->mimeData());
        if (uris.isEmpty() && Q3TextDrag::canDecode (de)) {
            QString text;
            Q3TextDrag::decode (de, text);
            uris.push_back (KURL (text));
        }
        if (valid && uris.size () > 0) {
            bool as_child = n->isDocument () || n->hasChildNodes ();
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
    } else
        m_view->dropEvent (de);
}

KDE_NO_EXPORT void PlayListView::itemIsRenamed (Q3ListViewItem * qitem) {
    PlayListItem * item = static_cast <PlayListItem *> (qitem);
    if (item->node) {
        RootPlayListItem * ri = rootItem (qitem);
        if (!ri->show_all_nodes && item->node->isEditable ()) {
            item->node->setNodeName (item->text (0));
            if (item->node->mrl ()->pretty_name.isEmpty ())
                item->setText (0, KURL (item->node->mrl ()->src).pathOrUrl ());
        } else // restore damage ..
            updateTree (ri, item->node, true);
    } else if (item->m_attr) {
        QString txt = item->text (0);
        int pos = txt.find (QChar ('='));
        if (pos > -1) {
            item->m_attr->setName (txt.left (pos));
            item->m_attr->setValue (txt.mid (pos + 1));
        } else {
            item->m_attr->setName (txt);
            item->m_attr->setValue (QString (""));
        }
        PlayListItem * pi = static_cast <PlayListItem *> (item->parent ());
        if (pi && pi->node)
            pi->node->document ()->m_tree_version++;
    }
}

KDE_NO_EXPORT void PlayListView::itemIsSelected (Q3ListViewItem * qitem) {
    RootPlayListItem * ri = rootItem (qitem);
    setItemsRenameable (ri && (ri->flags & TreeEdit) && ri != qitem);
}

KDE_NO_EXPORT void PlayListView::rename (Q3ListViewItem * qitem, int c) {
    PlayListItem * item = static_cast <PlayListItem *> (qitem);
    if (rootItem (qitem)->show_all_nodes && item && item->m_attr) {
        PlayListItem * pi = static_cast <PlayListItem *> (qitem->parent ());
        if (pi && pi->node && pi->node->isEditable ())
            K3ListView::rename (item, c);
    } else if (item && item->node && item->node->isEditable ()) {
        if (!rootItem (qitem)->show_all_nodes &&
                item->node->isPlayable () &&
                item->node->mrl ()->pretty_name.isEmpty ())
            // populate() has crippled src, restore for editing 
            item->setText (0, item->node->mrl ()->src);
        K3ListView::rename (item, c);
    }
}

KDE_NO_EXPORT void PlayListView::editCurrent () {
    Q3ListViewItem * qitem = selectedItem ();
    if (qitem) {
        RootPlayListItem * ri = rootItem (qitem);
        if (ri && (ri->flags & TreeEdit) && ri != qitem)
            rename (qitem, 0);
    }
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

static Q3ListViewItem * findNodeInTree (NodePtr n, Q3ListViewItem * item) {
    //kDebug () << "item:" << item->text (0) << " n:" << (n ? n->nodeName () : "null" );
    PlayListItem * pi = static_cast <PlayListItem *> (item);
    if (!n || !pi->node)
        return 0L;
    if (n == pi->node)
        return item;
    for (Q3ListViewItem * ci = item->firstChild(); ci; ci = ci->nextSibling ()) {
        //kDebug () << "ci:" << ci->text (0) << " n:" << n->nodeName ();
        Q3ListViewItem * vi = findNodeInTree (n, ci);
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
    if (!m_current_find_elm) {
        PlayListItem * lvi = static_cast <PlayListItem *> (firstChild ());
        if (lvi)
            m_current_find_elm = lvi->node;
    }
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
    NodePtr node, n = m_current_find_elm;
    RootPlayListItem * ri = rootItem (current_find_tree_id);
    while (!found && n) {
        if (ri->show_all_nodes || n->expose ()) {
            bool elm = n->isElementNode ();
            QString val = n->nodeName ();
            if (elm && !ri->show_all_nodes) {
                Mrl * mrl = n->mrl ();
                if (mrl) {
                    if (mrl->pretty_name.isEmpty ()) {
                        if (!mrl->src.isEmpty())
                            val = KURL(mrl->src).prettyUrl();
                    } else
                        val = mrl->pretty_name;
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
                for (AttributePtr a = convertNode <Element> (n)->attributes ()->first (); a; a = a->nextSibling ()) {
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
        Q3ListViewItem * fc = findNodeInTree (node, ri);
        if (!fc) {
            m_current_find_elm = 0L;
            kDebug () << "node not found in tree tree:" << ri->id;
        } else {
            setSelected (fc, true);
            if (m_current_find_attr && fc->firstChild () && fc->firstChild ()->firstChild ())
                ensureItemVisible (fc->firstChild ()->firstChild ());
            ensureItemVisible (fc);
        }
    }
    m_find_next->setEnabled (!!m_current_find_elm);
}

#include "playlistview.moc"
