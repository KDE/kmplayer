/*
    SPDX-FileCopyrightText: 2006 Koos Vriezen <koos.vriezen@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#include <cstdio>

#include "config-kmplayer.h"
// include files for Qt
#include <QApplication>
#include <QClipboard>
#include <QMenu>
#include <QIcon>
#include <qdrawutil.h>
#include <QPainter>
#include <QAbstractItemDelegate>
#include <QDropEvent>
#include <QStyle>
#include <QDropEvent>
#include <QPalette>
#include <QRegExp>
#include <QAbstractItemModel>
#include <QList>
#include <QItemSelectionModel>
#include <QMimeData>

#include <KIconLoader>
#include <KStandardAction>
#include <KFindDialog>
#include <KFind>
#include <KLocalizedString>
#include <KActionCollection>

#include "kmplayercommon_log.h"
#include "playlistview.h"
#include "playmodel.h"
#include "kmplayerview.h"
#include "kmplayercontrolpanel.h"

using namespace KMPlayer;

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
    QWidget *createEditor (QWidget *w, const QStyleOptionViewItem &o, const QModelIndex &i) const override
    {
        return default_item_delegate->createEditor (w, o, i);
    }
    bool editorEvent (QEvent *e, QAbstractItemModel *m, const QStyleOptionViewItem &o, const QModelIndex &i) override
    {
        return default_item_delegate->editorEvent (e, m, o, i);
    }
    bool eventFilter (QObject *editor, QEvent *event) override
    {
        return default_item_delegate->eventFilter (editor, event);
    }
    void paint (QPainter *p, const QStyleOptionViewItem &o, const QModelIndex &i) const override
    {
        playlist_view->paintCell (default_item_delegate, p, o, i);
    }
    void setEditorData (QWidget *e, const QModelIndex &i) const override
    {
        default_item_delegate->setEditorData (e, i);
    }
    void setModelData (QWidget *e, QAbstractItemModel *m, const QModelIndex &i) const override
    {
        default_item_delegate->setModelData (e, m, i);
    }
    QSize sizeHint (const QStyleOptionViewItem &o, const QModelIndex &i) const override
    {
        QSize size = default_item_delegate->sizeHint (o, i);
        return QSize (size.width (), size.height () + 2);
    }
    void updateEditorGeometry (QWidget *e, const QStyleOptionViewItem &o, const QModelIndex &i) const override
    {
        default_item_delegate->updateEditorGeometry (e, o, i);
    }
};

}

//-----------------------------------------------------------------------------

PlayListView::PlayListView (QWidget *, View *view, KActionCollection * ac)
 : //QTreeView (parent),
   m_view (view),
   m_find_dialog (nullptr),
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
    //setItemsExpandable (false);
    //setAnimated (true);
    setUniformRowHeights (true);
    setItemDelegateForColumn (0, new ItemDelegate (this, itemDelegate ()));
    setEditTriggers (EditKeyPressed);
    QPalette palette;
    palette.setColor (foregroundRole(), QColor (0, 0, 0));
    palette.setColor (backgroundRole(), QColor (0xB2, 0xB2, 0xB2));
    setPalette (palette);
    m_itemmenu = new QMenu (this);
    m_find = KStandardAction::find (this, &PlayListView::slotFind, this);
    m_find_next = KStandardAction::findNext (this, &PlayListView::slotFindNext, this);
    m_find_next->setEnabled (false);
    m_edit_playlist_item = ac->addAction ("edit_playlist_item");
    m_edit_playlist_item->setText (i18n ("Edit &item"));
    connect (m_edit_playlist_item, &QAction::triggered,
             this, &PlayListView::renameSelected);
    connect (this, &QTreeView::expanded,
             this, &PlayListView::slotItemExpanded);
}

PlayListView::~PlayListView () {
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

void PlayListView::selectItem(const QString&) {
    /*QTreeWidgetItem * item = selectedItem ();
    if (item && item->text (0) == txt)
        return;
    item = findItem (txt, 0);
    if (item) {
        item->setSelected (true);
        //ensureItemVisible (item);
    }*/
}

/*Q3DragObject * PlayListView::dragObject () {
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

void PlayListView::setFont (const QFont & fnt) {
    //setTreeStepSize (QFontMetrics (fnt).boundingRect ('m').width ());
    QTreeView::setFont (fnt);
}

void PlayListView::contextMenuEvent (QContextMenuEvent *event)
{
    PlayItem *item = playModel ()->itemFromIndex (indexAt (event->pos ()));
    if (item) {
        if (item->node || item->attribute) {
            TopPlayItem *ritem = item->rootItem ();
            if (m_itemmenu->actions().count () > 0) {
                m_find->setVisible (false);
                m_find_next->setVisible (false);
                m_itemmenu->clear ();
            }
            m_itemmenu->addAction (QIcon::fromTheme("edit-copy"),
                    i18n ("&Copy to Clipboard"),
                    this, &PlayListView::copyToClipboard);
            if (item->attribute ||
                    (item->node && (item->node->isPlayable () ||
                                    item->node->isDocument ()) &&
                     item->node->mrl ()->bookmarkable))
                m_itemmenu->addAction (QIcon::fromTheme("bookmark-new"),
                        i18n ("&Add Bookmark"),
                        this, QOverload<>::of(&PlayListView::addBookMark));
            if (ritem->have_dark_nodes) {
                QAction *act = m_itemmenu->addAction (i18n ("&Show all"),
                        this, &PlayListView::toggleShowAllNodes);
                act->setCheckable (true);
                act->setChecked (ritem->show_all_nodes);
            }
            if (item->item_flags & Qt::ItemIsEditable)
                m_itemmenu->addAction (m_edit_playlist_item);
            m_itemmenu->addSeparator ();
            m_find->setVisible (true);
            m_find_next->setVisible (true);
            Q_EMIT prepareMenu (item, m_itemmenu);
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
    QModelIndex i = currentIndex ();
    if (i.isValid ()) {
        QString s;

        QVariant v = i.data (PlayModel::UrlRole);
        if (v.isValid ())
            s = v.toString ();
        if (s.isEmpty ())
            s = i.data ().toString ();

        if (!s.isEmpty ())
            QApplication::clipboard()->setText (s);
    }
}

void PlayListView::addBookMark () {
    PlayItem * item = selectedItem ();
    if (item->node) {
        Mrl * mrl = item->node->mrl ();
        const QUrl url = QUrl::fromUserInput(mrl ? mrl->src : QString (item->node->nodeName ()));
        Q_EMIT addBookMark (mrl->title.isEmpty () ? url.toDisplayString() : mrl->title, url.url ());
    }
}

void PlayListView::toggleShowAllNodes () {
    PlayItem * cur_item = selectedItem ();
    if (cur_item) {
        TopPlayItem *ritem = cur_item->rootItem ();
        showAllNodes (ritem, !ritem->show_all_nodes);
    }
}

void PlayListView::showAllNodes(TopPlayItem *ri, bool show) {
    if (ri && ri->show_all_nodes != show) {
        PlayItem * cur_item = selectedItem ();
        ri->show_all_nodes = show;
        playModel()->updateTree (ri->id, ri->node, cur_item->node, true, false);
        if (m_current_find_elm &&
                ri->node->document() == m_current_find_elm->document() &&
                !ri->show_all_nodes) {
            if (!m_current_find_elm->role (RolePlaylist))
                m_current_find_elm = nullptr;
            m_current_find_attr = nullptr;
        }
    }
}

bool PlayListView::isDragValid (QDropEvent *event) {
    if (event->source() == this &&
            event->mimeData ()
                ->hasFormat ("application/x-qabstractitemmodeldatalist"))
        return true;
    if (event->mimeData()->hasUrls()) {
        const QList<QUrl> uriList = event->mimeData()->urls();
        if (!uriList.isEmpty ())
            return true;
    } else {
        QString text = event->mimeData ()->text ();
        if (!text.isEmpty () && QUrl::fromUserInput(text).isValid ())
            return true;
    }
    return false;
}

void PlayListView::dragMoveEvent (QDragMoveEvent *event)
{
    PlayItem *itm = playModel ()->itemFromIndex (indexAt (event->pos ()));
    if (itm) {
        TopPlayItem *ritem = itm->rootItem ();
        if (ritem->itemFlags() & PlayModel::AllowDrops && isDragValid (event))
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

void PlayListView::dropEvent (QDropEvent *event) {
    PlayItem *itm = playModel ()->itemFromIndex (indexAt (event->pos ()));
    if (itm && itm->node) {
        TopPlayItem *ritem = itm->rootItem ();
        NodePtr n = itm->node;
        if (ritem->id > 0 || n->isDocument ()) {
            Q_EMIT dropped (event, itm);
        } else {
            QList<QUrl> uris = event->mimeData()->urls();
            if (uris.isEmpty ()) {
                const QUrl url = QUrl::fromUserInput(event->mimeData ()->text ());
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

PlayModel *PlayListView::playModel () const
{
    return static_cast <PlayModel *> (model());
}


void PlayListView::renameSelected () {
    QModelIndex i = currentIndex ();
    PlayItem *itm = playModel ()->itemFromIndex (i);
    if (itm && itm->item_flags & Qt::ItemIsEditable)
        edit (i);
}

void PlayListView::slotCurrentItemChanged (QModelIndex /*cur*/, QModelIndex)
{
    //TopPlayItem * ri = rootItem (qitem);
    //setItemsRenameable (ri && (ri->item_flagsTreeEdit) && ri != qitem);
}

void PlayListView::slotFind () {
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
    //qCDebug(LOG_KMPLAYER_COMMON) << "item:" << item->text (0) << " n:" << (n ? n->nodeName () : "null" );
    PlayItem * pi = static_cast <PlayItem *> (item);
    if (!n || !pi->node)
        return 0L;
    if (n == pi->node)
        return item;
    for (int i = 0; i < item->childCount (); ++i) {
        //qCDebug(LOG_KMPLAYER_COMMON) << "ci:" << ci->text (0) << " n:" << n->nodeName ();
        QTreeWidgetItem *vi = findNodeInTree (n, item->child (i));
        if (vi)
            return vi;
    }
    return 0L;

}*/

void PlayListView::slotFindOk () {
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
void PlayListView::slotFindNext () {
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
    qCDebug(LOG_KMPLAYER_COMMON) << " search for " << str << "=" << (node ? node->nodeName () : "not found") << " next:" << (n ? n->nodeName () : " not found");
    if (found) {
        QTreeWidgetItem *fc = findNodeInTree (node, ri);
        if (!fc) {
            m_current_find_elm = 0L;
            qCDebug(LOG_KMPLAYER_COMMON) << "node not found in tree tree:" << ri->id;
        } else {
            fc->setSelected (true);
            if (m_current_find_attr && fc->childCount () && fc->child (0)->childCount ())
                scrollToItem (fc->child (0)->child (0));
            scrollToItem (fc);
        }
    }
    m_find_next->setEnabled (!!m_current_find_elm);*/
}
