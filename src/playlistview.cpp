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

#include <config.h>
// include files for Qt
#include <qapplication.h>
#include <qclipboard.h>
#include <qpopupmenu.h>
#include <qdrawutil.h>
#include <qpainter.h>
#include <qiconset.h>
#include <qpixmap.h>
#include <qheader.h>
#include <qstyle.h>
#include <qtimer.h>

#include <kiconloader.h>
#include <kfinddialog.h>
#include <kurldrag.h>
#include <kaction.h>
#include <klocale.h>
#include <kdebug.h>

#include "playlistview.h"
#include "kmplayerview.h"
#include "kmplayercontrolpanel.h"

using namespace KMPlayer;

//-------------------------------------------------------------------------

namespace KMPlayer {

    KDE_NO_EXPORT bool isDragValid (QDropEvent * de) {
        if (KURLDrag::canDecode (de))
            return true;
        if (QTextDrag::canDecode (de)) {
            QString text;
            if (QTextDrag::decode (de, text) && KURL (text).isValid ())
                return true;
        }
        return false;
    }
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT PlayListItem::PlayListItem (QListViewItem *p, const NodePtr & e, PlayListView * lv) : QListViewItem (p), node (e), listview (lv) {}

KDE_NO_CDTOR_EXPORT PlayListItem::PlayListItem (QListViewItem *p, const AttributePtr & a, PlayListView * lv) : QListViewItem (p), m_attr (a), listview (lv) {}

KDE_NO_CDTOR_EXPORT
PlayListItem::PlayListItem (PlayListView *v, const NodePtr &e, QListViewItem *b)
  : QListViewItem (v, b), node (e), listview (v) {}

KDE_NO_CDTOR_EXPORT void PlayListItem::paintCell (QPainter * p, const QColorGroup & cg, int column, int width, int align) {
    if (node && node->state == Node::state_began) {
        QColorGroup mycg (cg);
        mycg.setColor (QColorGroup::Foreground, listview->activeColor ());
        mycg.setColor (QColorGroup::Text, listview->activeColor ());
        QListViewItem::paintCell (p, mycg, column, width, align);
    } else
        QListViewItem::paintCell (p, cg, column, width, align);
}

KDE_NO_CDTOR_EXPORT void PlayListItem::paintBranches (QPainter * p, const QColorGroup &, int w, int, int h) {
    p->fillRect (0, 0, w, h, listview->paletteBackgroundColor());
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
RootPlayListItem::RootPlayListItem (int _id, PlayListView *v, const NodePtr & e, QListViewItem * before, int flgs)
  : PlayListItem (v, e, before),
    id (_id),
    flags (flgs),
    show_all_nodes (false),
    have_dark_nodes (false) {}

KDE_NO_CDTOR_EXPORT void RootPlayListItem::paintCell (QPainter * p, const QColorGroup & cg, int column, int width, int align) {
    QColorGroup mycg (cg);
    mycg.setColor (QColorGroup::Base, listview->topLevelWidget()->paletteBackgroundColor());
    mycg.setColor (QColorGroup::Highlight, mycg.base ());
    mycg.setColor (QColorGroup::Text, listview->topLevelWidget()->paletteForegroundColor());
    mycg.setColor (QColorGroup::HighlightedText, mycg.text ());
    QListViewItem::paintCell (p, mycg, column, width, align);
    qDrawShadeRect (p, 0, 0, width -1, height () -1, mycg, !isOpen ());
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT PlayListView::PlayListView (QWidget * parent, View * view, KActionCollection * ac)
 : KListView (parent, "kde_kmplayer_playlist"),
   m_view (view),
   m_find_dialog (0L),
   m_active_color (30, 0, 255),
   last_id (0),
   last_drag_tree_id (0),
   m_ignore_expanded (false) {
    addColumn (QString::null);
    header()->hide ();
    //setRootIsDecorated (true);
    setSorting (-1);
    setAcceptDrops (true);
    setDropVisualizer (true);
    setItemsRenameable (true);
    setItemMargin (2);
    m_itemmenu = new QPopupMenu (this);
    folder_pix = KGlobal::iconLoader ()->loadIcon (QString ("folder"), KIcon::Small);
    auxiliary_pix = KGlobal::iconLoader ()->loadIcon (QString ("folder_grey"), KIcon::Small);
    video_pix = KGlobal::iconLoader ()->loadIcon (QString ("video"), KIcon::Small);
    info_pix = KGlobal::iconLoader ()->loadIcon (QString ("centrejust"), KIcon::Small);
    img_pix = KGlobal::iconLoader ()->loadIcon (QString ("colorize"), KIcon::Small);
    unknown_pix = KGlobal::iconLoader ()->loadIcon (QString ("unknown"), KIcon::Small);
    menu_pix = KGlobal::iconLoader ()->loadIcon (QString ("player_playlist"), KIcon::Small);
    config_pix = KGlobal::iconLoader ()->loadIcon (QString ("configure"), KIcon::Small);
    url_pix = KGlobal::iconLoader ()->loadIcon (QString ("www"), KIcon::Small);
    m_find = KStdAction::find (this, SLOT (slotFind ()), ac, "find");
    m_find_next = KStdAction::findNext (this, SLOT(slotFindNext()), ac, "next");
    m_find_next->setEnabled (false);
    connect (this, SIGNAL (contextMenuRequested (QListViewItem *, const QPoint &, int)), this, SLOT (contextMenuItem (QListViewItem *, const QPoint &, int)));
    connect (this, SIGNAL (expanded (QListViewItem *)),
             this, SLOT (itemExpanded (QListViewItem *)));
    connect (this, SIGNAL (dropped (QDropEvent *, QListViewItem *)),
             this, SLOT (itemDropped (QDropEvent *, QListViewItem *)));
    connect (this, SIGNAL (itemRenamed (QListViewItem *)),
             this, SLOT (itemIsRenamed (QListViewItem *)));
    connect (this, SIGNAL (selectionChanged (QListViewItem *)),
             this, SLOT (itemIsSelected (QListViewItem *)));
}

KDE_NO_CDTOR_EXPORT PlayListView::~PlayListView () {
}

int PlayListView::addTree (NodePtr root, const QString & source, const QString & icon, int flags) {
    //kdDebug () << "addTree " << source << " " << root->mrl()->src << endl;
    RootPlayListItem * ritem = new RootPlayListItem (++last_id, this, root, lastChild(), flags);
    ritem->source = source;
    ritem->icon = icon;
    ritem->setPixmap (0, !ritem->icon.isEmpty ()
            ? KGlobal::iconLoader ()->loadIcon (ritem->icon, KIcon::Small)
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
                text = KURL(mrl->src).prettyURL (0, KURL::StripFileProtocol);
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
                    ai->setText (0, QString ("%1=%2").arg (a->nodeName ()).arg (a->nodeValue ()));
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

KDE_NO_EXPORT void PlayListView::updateTree (RootPlayListItem * ritem, NodePtr active, bool select) {
    bool set_open = ritem->id == 0 || (ritem ? ritem->isOpen () : false);
    m_ignore_expanded = true;
    PlayListItem * curitem = 0L;
    while (ritem->firstChild ())
        delete ritem->firstChild ();
    if (!ritem->node)
        return;
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
    QListViewItem * item = selectedItem ();
    if (item && item->text (0) == txt)
        return;
    item = findItem (txt, 0);
    if (item) {
        setSelected (item, true);
        ensureItemVisible (item);
    }
}

KDE_NO_EXPORT QDragObject * PlayListView::dragObject () {
    PlayListItem * item = static_cast <PlayListItem *> (selectedItem ());
    if (item && item->node) {
        QString txt = item->node->isPlayable ()
            ? item->node->mrl ()->src : item->node->outerXML ();
        QTextDrag * drag = new QTextDrag (txt, this);
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
    KListView::setFont (fnt);
}

KDE_NO_EXPORT void PlayListView::contextMenuItem (QListViewItem * vi, const QPoint & p, int) {
    if (vi) {
        PlayListItem * item = static_cast <PlayListItem *> (vi);
        if (item->node || item->m_attr) {
            RootPlayListItem * ritem = rootItem (vi);
            if (m_itemmenu->count () > 0) {
                m_find->unplug (m_itemmenu);
                m_find_next->unplug (m_itemmenu);
                m_itemmenu->clear ();
            }
            m_itemmenu->insertItem (KGlobal::iconLoader ()->loadIconSet
                    (QString ("editcopy"), KIcon::Small, 0, true),
                    i18n ("&Copy to Clipboard"),
                    this, SLOT (copyToClipboard ()), 0, 0);
            if (item->m_attr ||
                    (item->node && (item->node->isPlayable () ||
                                    item->node->isDocument ()) &&
                     item->node->mrl ()->bookmarkable))
                m_itemmenu->insertItem (KGlobal::iconLoader ()->loadIconSet
                        (QString ("bookmark_add"), KIcon::Small, 0, true),
                        i18n ("&Add Bookmark"),
                        this, SLOT (addBookMark ()), 0, 1);
            if (ritem->have_dark_nodes) {
                m_itemmenu->insertItem (i18n ("&Show all"),
                        this, SLOT (toggleShowAllNodes ()), 0, 2);
                m_itemmenu->setItemChecked (2, ritem->show_all_nodes);
            }
            m_itemmenu->insertSeparator ();
            m_find->plug (m_itemmenu);
            m_find_next->plug (m_itemmenu);
            emit prepareMenu (item, m_itemmenu);
            m_itemmenu->exec (p);
        }
    } else
        m_view->controlPanel ()->popupMenu ()->exec (p);
}

void PlayListView::itemExpanded (QListViewItem * item) {
    if (!m_ignore_expanded && item->childCount () == 1) {
        PlayListItem * child_item = static_cast<PlayListItem*>(item->firstChild ());
        child_item->setOpen (rootItem (item)->show_all_nodes ||
                (child_item->node && child_item->node->expose ()));
    }
}

RootPlayListItem * PlayListView::rootItem (QListViewItem * item) const {
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
        emit addBookMark (mrl->pretty_name.isEmpty () ? url.prettyURL () : mrl->pretty_name, url.url ());
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
    QListViewItem * item = itemAt (contentsToViewport (de->pos ()));
    if (item && (de->source () == this || isDragValid (de))) {
        RootPlayListItem * ritem = rootItem (item);
        return ritem->flags & AllowDrops;
    }
    return false;
}

KDE_NO_EXPORT void PlayListView::itemDropped (QDropEvent * de, QListViewItem *after) {
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
        KURL::List sl;
        if (KURLDrag::canDecode (de)) {
            KURLDrag::decode (de, sl);
        } else if (QTextDrag::canDecode (de)) {
            QString text;
            QTextDrag::decode (de, text);
            sl.push_back (KURL (text));
        }
        if (valid && sl.size () > 0) {
            bool as_child = n->isDocument () || n->hasChildNodes ();
            NodePtr d = n->document ();
            for (int i = sl.size (); i > 0; i--) {
                Node * ni = new KMPlayer::GenericURL (d, sl[i-1].url ());
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

KDE_NO_EXPORT void PlayListView::itemIsRenamed (QListViewItem * qitem) {
    PlayListItem * item = static_cast <PlayListItem *> (qitem);
    if (item->node) {
        RootPlayListItem * ri = rootItem (qitem);
        if (!ri->show_all_nodes && item->node->isEditable ()) {
            item->node->setNodeName (item->text (0));
            if (item->node->mrl ()->pretty_name.isEmpty ())
                item->setText (0, KURL (item->node->mrl ()->src).prettyURL (0, KURL::StripFileProtocol));
        } else // restore damage ..
            updateTree (ri, item->node, true);
    } else if (item->m_attr) {
        QString txt = item->text (0);
        int pos = txt.find (QChar ('='));
        if (pos > -1) {
            item->m_attr->setNodeName (txt.left (pos));
            item->m_attr->setNodeValue (txt.mid (pos + 1));
        } else {
            item->m_attr->setNodeName (txt);
            item->m_attr->setNodeValue (QString (""));
        }
        PlayListItem * pi = static_cast <PlayListItem *> (item->parent ());
        if (pi && pi->node)
            pi->node->document ()->m_tree_version++;
    }
}

KDE_NO_EXPORT void PlayListView::itemIsSelected (QListViewItem * qitem) {
    RootPlayListItem * ri = rootItem (qitem);
    setItemsRenameable (ri && (ri->flags & TreeEdit) && ri != qitem);
}

KDE_NO_EXPORT void PlayListView::rename (QListViewItem * qitem, int c) {
    PlayListItem * item = static_cast <PlayListItem *> (qitem);
    if (rootItem (qitem)->show_all_nodes && item && item->m_attr) {
        PlayListItem * pi = static_cast <PlayListItem *> (qitem->parent ());
        if (pi && pi->node && pi->node->isEditable ())
            KListView::rename (item, c);
    } else if (item && item->node && item->node->isEditable ()) {
        if (!rootItem (qitem)->show_all_nodes &&
                item->node->isPlayable () &&
                item->node->mrl ()->pretty_name.isEmpty ())
            // populate() has crippled src, restore for editing 
            item->setText (0, item->node->mrl ()->src);
        KListView::rename (item, c);
    }
}

KDE_NO_EXPORT void PlayListView::editCurrent () {
    QListViewItem * qitem = selectedItem ();
    if (qitem) {
        RootPlayListItem * ri = rootItem (qitem);
        if (ri && (ri->flags & TreeEdit) && ri != qitem)
            rename (qitem, 0);
    }
}

KDE_NO_EXPORT void PlayListView::slotFind () {
    m_current_find_elm = 0L;
    if (!m_find_dialog) {
        m_find_dialog = new KFindDialog (false, this, "kde_kmplayer_find", KFindDialog::CaseSensitive);
        m_find_dialog->setHasSelection (false);
        connect(m_find_dialog, SIGNAL(okClicked ()), this, SLOT(slotFindOk ()));
    } else
        m_find_dialog->setPattern (QString::null);
    m_find_dialog->show ();
}

static QListViewItem * findNodeInTree (NodePtr n, QListViewItem * item) {
    //kdDebug () << "item:" << item->text (0) << " n:" << (n ? n->nodeName () : "null" )  <<endl;
    PlayListItem * pi = static_cast <PlayListItem *> (item);
    if (!n || !pi->node)
        return 0L;
    if (n == pi->node)
        return item;
    for (QListViewItem * ci = item->firstChild(); ci; ci = ci->nextSibling ()) {
        //kdDebug () << "ci:" << ci->text (0) << " n:" << n->nodeName () <<endl;
        QListViewItem * vi = findNodeInTree (n, ci);
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
    if (opt & KFindDialog::FromCursor && currentItem ()) {
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
    } else if (!(opt & KFindDialog::FindIncremental))
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
    if (opt & KFindDialog::RegularExpression)
        regexp = str;
    bool cs = (opt & KFindDialog::CaseSensitive);
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
                            val = KURL(mrl->src).prettyURL();
                    } else
                        val = mrl->pretty_name;
                }
            } else if (!elm)
                val = n->nodeValue ();
            if (((opt & KFindDialog::RegularExpression) &&
                    val.find (regexp, 0) > -1) ||
                    (!(opt & KFindDialog::RegularExpression) &&
                     val.find (str, 0, cs) > -1)) {
                node = n;
                m_current_find_attr = 0L;
                found = true;
            } else if (elm && ri->show_all_nodes) {
                for (AttributePtr a = convertNode <Element> (n)->attributes ()->first (); a; a = a->nextSibling ())
                    if (((opt & KFindDialog::RegularExpression) &&
                                (QString::fromLatin1 (a->nodeName ()).find (regexp, 0) || a->nodeValue ().find (regexp, 0) > -1)) ||
                                (!(opt & KFindDialog::RegularExpression) &&
                                 (QString::fromLatin1 (a->nodeName ()).find (str, 0, cs) > -1 || a->nodeValue ().find (str, 0, cs) > -1))) {
                        node = n;
                        m_current_find_attr = a;
                        found = true;
                        break;
                    }
            }
        }
        if (n) { //set pointer to next
            if (opt & KFindDialog::FindBackwards) {
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
    kdDebug () << " search for " << str << "=" << (node ? node->nodeName () : "not found") << " next:" << (n ? n->nodeName () : " not found") << endl;
    if (found) {
        QListViewItem * fc = findNodeInTree (node, ri);
        if (!fc) {
            m_current_find_elm = 0L;
            kdDebug () << "node not found in tree tree:" << ri->id << endl;
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
