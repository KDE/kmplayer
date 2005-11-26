/**
 * Copyright (C) 2002-2003 by Koos Vriezen <koos ! vriezen ? xs4all ! nl>
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
#include <math.h>

#include <config.h>
// include files for Qt
#include <qstyle.h>
#include <qtimer.h>
#include <qpainter.h>
#include <qmetaobject.h>
#include <qlayout.h>
#include <qpixmap.h>
#include <qtextedit.h>
#include <qtooltip.h>
#include <qapplication.h>
#include <qiconset.h>
#include <qaccel.h>
#include <qcursor.h>
#include <qkeysequence.h>
#include <qslider.h>
#include <qlabel.h>
#include <qdatastream.h>
#include <qpixmap.h>
#include <qpainter.h>
#include <qwidgetstack.h>
#include <qheader.h>
#include <qcursor.h>
#include <qclipboard.h>

#include <kiconloader.h>
#include <kstaticdeleter.h>
#include <kstatusbar.h>
#include <kdebug.h>
#include <klocale.h>
#include <kapplication.h>
#include <kactioncollection.h>
#include <kstdaction.h>
#include <kshortcut.h>
#include <kurldrag.h>
#include <kfinddialog.h>
#include <dcopclient.h>
#include <kglobalsettings.h>

#include "kmplayerview.h"
#include "kmplayercontrolpanel.h"
#include "kmplayersource.h"

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
static const int XKeyPress = KeyPress;
#undef KeyPress
#undef Always
#undef Never
#undef Status
#undef Unsorted
#undef Bool

extern const char * normal_window_xpm[];
extern const char * playlist_xpm[];


/* mouse invisible: define the time (in 1/1000 seconds) before mouse goes invisible */
#define MOUSE_INVISIBLE_DELAY 2000


using namespace KMPlayer;

//-------------------------------------------------------------------------

static bool isDragValid (QDropEvent * de) {
    if (KURLDrag::canDecode (de))
        return true;
    if (QTextDrag::canDecode (de)) {
        QString text;
        if (KURL (QTextDrag::decode (de, text)).isValid ())
            return true;
    }
    return false;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT ViewArea::ViewArea (QWidget * parent, View * view)
 : QWidget (parent, "kde_kmplayer_viewarea", WResizeNoErase | WRepaintNoErase),
   m_parent (parent),
   m_view (view),
   m_painter (0L),
   m_paint_buffer (0L),
   m_collection (new KActionCollection (this)),
   m_mouse_invisible_timer (0),
   m_repaint_timer (0),
   m_fullscreen_scale (100),
   scale_lbl_id (-1),
   scale_slider_id (-1),
   m_fullscreen (false),
   m_minimal (false) {
    setEraseColor (QColor (0, 0, 0));
    setAcceptDrops (true);
    new KAction (i18n ("Fullscreen"), KShortcut (Qt::Key_F), this, SLOT (accelActivated ()), m_collection, "view_fullscreen_toggle");
    setMouseTracking (true);
}

KDE_NO_CDTOR_EXPORT ViewArea::~ViewArea () {
    delete m_painter;
    delete m_paint_buffer;
}

KDE_NO_EXPORT void ViewArea::fullScreen () {
    killTimers ();
    m_mouse_invisible_timer = m_repaint_timer = 0;
    if (m_fullscreen) {
        showNormal ();
        reparent (m_parent, 0, QPoint (0, 0), true);
        static_cast <KDockWidget *> (m_parent)->setWidget (this);
        for (unsigned i = 0; i < m_collection->count (); ++i)
            m_collection->action (i)->setEnabled (false);
        if (scale_lbl_id != -1) {
            m_view->controlPanel ()->popupMenu ()->removeItem (scale_lbl_id);
            m_view->controlPanel ()->popupMenu ()->removeItem (scale_slider_id);
            scale_lbl_id = scale_slider_id = -1;
        }
        m_view->controlPanel ()->button (ControlPanel::button_playlist)->setIconSet (QIconSet (QPixmap (playlist_xpm)));
    } else {
        m_topwindow_rect = topLevelWidget ()->geometry ();
        reparent (0L, 0, qApp->desktop()->screenGeometry(this).topLeft(), true);
        showFullScreen ();
        for (unsigned i = 0; i < m_collection->count (); ++i)
            m_collection->action (i)->setEnabled (true);
        QPopupMenu * menu = m_view->controlPanel ()->popupMenu ();
        QLabel * lbl = new QLabel (i18n ("Scale:"), menu);
        scale_lbl_id = menu->insertItem (lbl, -1, 4);
        QSlider * slider = new QSlider (50, 150, 10, m_fullscreen_scale, Qt::Horizontal, menu);
        connect (slider, SIGNAL (valueChanged (int)), this, SLOT (scale (int)));
        scale_slider_id = menu->insertItem (slider, -1, 5);
        m_view->controlPanel ()->button (ControlPanel::button_playlist)->setIconSet (QIconSet (QPixmap (normal_window_xpm)));
    }
    m_fullscreen = !m_fullscreen;
    m_view->controlPanel()->popupMenu ()->setItemChecked (ControlPanel::menu_fullscreen, m_fullscreen);

    if (m_fullscreen) {
        m_mouse_invisible_timer = startTimer(MOUSE_INVISIBLE_DELAY);
    } else {
        if (m_mouse_invisible_timer) {
            killTimer (m_mouse_invisible_timer);
            m_mouse_invisible_timer = 0;
        }
        unsetCursor();
    }
}

void ViewArea::minimalMode () {
    m_minimal = !m_minimal;
    killTimers ();
    m_mouse_invisible_timer = m_repaint_timer = 0;
    if (m_minimal) {
        m_view->setViewOnly ();
        m_view->setControlPanelMode (KMPlayer::View::CP_AutoHide);
        m_view->setNoInfoMessages (true);
        m_view->controlPanel ()->button (ControlPanel::button_playlist)->setIconSet (QIconSet (QPixmap (normal_window_xpm)));
    } else {
        m_view->setControlPanelMode (KMPlayer::View::CP_Show);
        m_view->setNoInfoMessages (false);
        m_view->controlPanel ()->button (ControlPanel::button_playlist)->setIconSet (QIconSet (QPixmap (playlist_xpm)));
    }
    m_topwindow_rect = topLevelWidget ()->geometry ();
}

KDE_NO_EXPORT void ViewArea::accelActivated () {
    m_view->controlPanel()->popupMenu ()->activateItemAt (m_view->controlPanel()->popupMenu ()->indexOf (ControlPanel::menu_fullscreen)); 
}

KDE_NO_EXPORT void ViewArea::mousePressEvent (QMouseEvent * e) {
    if (eventListener && eventListener->handleEvent(new PointerEvent(event_pointer_clicked,e->x(), e->y())))
        e->accept ();
}

KDE_NO_EXPORT void ViewArea::mouseMoveEvent (QMouseEvent * e) {
    if (e->state () == Qt::NoButton) {
        int vert_buttons_pos = height ();
        int cp_height = m_view->controlPanel ()->maximumSize ().height ();
        m_view->delayedShowButtons (e->y() > vert_buttons_pos-cp_height &&
                                    e->y() < vert_buttons_pos);
    }
    if (eventListener && eventListener->handleEvent(new PointerEvent(event_pointer_moved,e->x(), e->y())))
        e->accept ();
    mouseMoved (); // for m_mouse_invisible_timer
}

KDE_NO_EXPORT void ViewArea::syncVisual (QRect rect) {
    if (!eventListener) {
        repaint (rect, false);
        return;
    }
#define PAINT_BUFFER_HEIGHT 128
    if (!m_paint_buffer) {
        m_paint_buffer = new QPixmap (width (), PAINT_BUFFER_HEIGHT);
        m_painter = new QPainter ();
    } else if (((QPixmap *)m_paint_buffer)->width () < width ())
        ((QPixmap *)m_paint_buffer)->resize (width (), PAINT_BUFFER_HEIGHT);
    int py=0;
    int ex = rect.x ();
    int ey = rect.y ();
    int ew = rect.width ();
    int eh = rect.height ();
    while (py < eh) {
        int ph = eh-py < PAINT_BUFFER_HEIGHT ? eh-py : PAINT_BUFFER_HEIGHT;
        m_painter->begin (m_paint_buffer);
        m_painter->translate(-ex, -ey-py);
        m_painter->fillRect (ex, ey+py, ew, ph, QBrush (paletteBackgroundColor ()));
        eventListener->handleEvent(new PaintEvent(*m_painter, ex, ey+py,ew,ph));
        m_painter->end();
        bitBlt (this, ex, ey+py, m_paint_buffer, 0, 0, ew, ph);
        py += PAINT_BUFFER_HEIGHT;
    }
    XFlush (qt_xdisplay ());
}

KDE_NO_EXPORT void ViewArea::paintEvent (QPaintEvent * pe) {
    if (eventListener)
        scheduleRepaint (pe->rect ().x (), pe->rect ().y (), pe->rect ().width (), pe->rect ().height ());
    else
        QWidget::paintEvent (pe);
}

KDE_NO_EXPORT void ViewArea::scale (int val) {
    m_fullscreen_scale = val;
    resizeEvent (0L);
}

KDE_NO_EXPORT void ViewArea::resizeEvent (QResizeEvent *) {
    if (!m_view->controlPanel ()) return;
    int x =0, y = 0;
    int w = width ();
    int h = height ();
    int hcp = m_view->controlPanel ()->isVisible () ? (m_view->controlPanelMode () == View::CP_Only ? h : m_view->controlPanel()->maximumSize ().height ()) : 0;
    int hsb = m_view->statusBar ()->isVisible () ? (m_view->statusBarMode () == View::SB_Only ? h : m_view->statusBar()->maximumSize ().height ()) : 0;
    int wws = w;
    // move controlpanel over video when autohiding and playing
    int hws = h - (m_view->controlPanelMode () == View::CP_AutoHide && m_view->widgetStack ()->visibleWidget () == m_view->viewer () ? 0 : hcp) - hsb;

    // now scale the regions and check if video region is already sized
    bool av_geometry_changed = false;
    if (eventListener && wws > 0 && hws > 0) {
        m_av_geometry = QRect (0, 0, 0, 0);
        eventListener->handleEvent (new SizeEvent (x, y, wws, hws, m_view->keepSizeRatio ()));
        av_geometry_changed = (m_av_geometry != QRect (0, 0, 0, 0));
        x = m_av_geometry.x ();
        y = m_av_geometry.y ();
        wws = m_av_geometry.width ();
        hws = m_av_geometry.height ();
            //m_view->viewer ()->setAspect (region->w / region->h);
    } else
        m_av_geometry = QRect (x, y, wws, hws);

    // finally resize controlpanel and video widget
    if (m_view->controlPanel ()->isVisible ())
        m_view->controlPanel ()->setGeometry (0, h-hcp-hsb, w, hcp);
    if (m_view->statusBar ()->isVisible ())
        m_view->statusBar ()->setGeometry (0, h-hsb, w, hsb);
    if (m_fullscreen && wws == w && hws == h) {
        wws = wws * m_fullscreen_scale / 100;
        hws = hws * m_fullscreen_scale / 100;
        x += (w - wws) / 2;
        y += (h - hws) / 2;
    }
    if (!av_geometry_changed)
        setAudioVideoGeometry (x, y, wws, hws, 0L);
}

KDE_NO_EXPORT
void ViewArea::setAudioVideoGeometry (int x, int y, int w, int h, unsigned int * bg_color) {
    if (m_view->controlPanelMode() == View::CP_Only) {
        w = h = 0;
    } else if (m_view->keepSizeRatio ()) { // scale video widget inside region
        int hfw = m_view->viewer ()->heightForWidth (w);
        if (hfw > 0)
            if (hfw > h) {
                int old_w = w;
                w = int ((1.0 * h * w)/(1.0 * hfw));
                x += (old_w - w) / 2;
            } else {
                y += (h - hfw) / 2;
                h = hfw;
            }
    }
    m_av_geometry = QRect (x, y, w, h);
    QRect rect = m_view->widgetStack ()->geometry ();
    if (m_av_geometry != rect) {
        m_view->widgetStack ()->setGeometry (x, y, w, h);
        rect.unite (m_av_geometry);
        scheduleRepaint (rect.x (), rect.y (), rect.width (), rect.height ());
    }
    if (bg_color)
        if (QColor (QRgb (*bg_color)) != (m_view->viewer ()->paletteBackgroundColor ())) {
            m_view->viewer()->setCurrentBackgroundColor (QColor (QRgb (*bg_color)));
            scheduleRepaint (x, y, w, h);
        }
}

KDE_NO_EXPORT void ViewArea::setEventListener (NodePtr el) {
    if (eventListener != el) {
        eventListener = el;
        resizeEvent (0L);
        if (m_repaint_timer) {
            killTimer (m_repaint_timer);
            m_repaint_timer = 0;
        }
        m_view->viewer()->resetBackgroundColor ();
        repaint ();
    }
}

KDE_NO_EXPORT void ViewArea::showEvent (QShowEvent *) {
    resizeEvent (0L);
}

KDE_NO_EXPORT void ViewArea::dropEvent (QDropEvent * de) {
    m_view->dropEvent (de);
}

KDE_NO_EXPORT void ViewArea::dragEnterEvent (QDragEnterEvent* dee) {
    m_view->dragEnterEvent (dee);
}

KDE_NO_EXPORT void ViewArea::contextMenuEvent (QContextMenuEvent * e) {
    m_view->controlPanel ()->popupMenu ()->exec (e->globalPos ());
}

KDE_NO_EXPORT void ViewArea::mouseMoved () {
    if (m_fullscreen) {
        if (m_mouse_invisible_timer)
            killTimer (m_mouse_invisible_timer);
        unsetCursor ();
        m_mouse_invisible_timer = startTimer (MOUSE_INVISIBLE_DELAY);
    }
}

KDE_NO_EXPORT void ViewArea::scheduleRepaint (int x, int y, int w, int h) {
    if (m_repaint_timer)
        m_repaint_rect = m_repaint_rect.unite (QRect (x, y, w, h));
    else {
        m_repaint_rect = QRect (x, y, w, h);
        m_repaint_timer = startTimer (10); // 100 per sec should do
    }
}

KDE_NO_EXPORT
void ViewArea::moveRect (int x, int y, int w, int h, int x1, int y1) {
    QRect r (x, y, w, h);
    if (m_repaint_timer && m_repaint_rect.intersects (r)) {
        m_repaint_rect = m_repaint_rect.unite (QRect (x1, y1, w, h).unite (r));
    } else if (m_view->viewer()->frameGeometry ().intersects (r)) {
        QRect r2 (QRect (x1, y1, w, h).unite (r));
        scheduleRepaint (r.x (), r.y (), r.width (), r.height ());
    } else {
        bitBlt (this, x1, y1, this, x, y, w, h);
        if (x1 > x)
            syncVisual (QRect (x, y, x1 - x, h));
        else if (x > x1)
            syncVisual (QRect (x1 + w, y, x - x1, h));
        if (y1 > y)
            syncVisual (QRect (x, y, w, y1 - y));
        else if (y > y1)
            syncVisual (QRect (x, y1 + h, w, y - y1));
    }
}

KDE_NO_EXPORT void ViewArea::timerEvent (QTimerEvent * e) {
    if (e->timerId () == m_mouse_invisible_timer) {
        killTimer (m_mouse_invisible_timer);
        m_mouse_invisible_timer = 0;
        if (m_fullscreen)
            setCursor (BlankCursor);
    } else if (e->timerId () == m_repaint_timer) {
        killTimer (m_repaint_timer);
        m_repaint_timer = 0;
        //repaint (m_repaint_rect, false);
        syncVisual (m_repaint_rect);
    } else {
        kdError () << "unknown timer " << e->timerId () << " " << m_repaint_timer << endl;
        killTimer (e->timerId ());
    }
}

KDE_NO_EXPORT void ViewArea::closeEvent (QCloseEvent * e) {
    //kdDebug () << "closeEvent" << endl;
    if (m_fullscreen) {
        fullScreen ();
        if (!m_parent->topLevelWidget ()->isVisible ())
            m_parent->topLevelWidget ()->show ();
        e->ignore ();
    } else
        QWidget::closeEvent (e);
}

//-----------------------------------------------------------------------------

namespace KMPlayer {

class KMPlayerPictureWidget : public QWidget {
    View * m_view;
public:
    KDE_NO_CDTOR_EXPORT KMPlayerPictureWidget (QWidget * parent, View * view)
        : QWidget (parent), m_view (view) {}
    KDE_NO_CDTOR_EXPORT ~KMPlayerPictureWidget () {}
protected:
    void mousePressEvent (QMouseEvent *);
};

} // namespace

KDE_NO_EXPORT void KMPlayerPictureWidget::mousePressEvent (QMouseEvent *) {
    m_view->emitPictureClicked ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT ListViewItem::ListViewItem (QListViewItem *p, const NodePtr & e, PlayListView * lv) : QListViewItem (p), m_elm (e), listview (lv) {}

KDE_NO_CDTOR_EXPORT ListViewItem::ListViewItem (QListViewItem *p, const AttributePtr & a, PlayListView * lv) : QListViewItem (p), m_attr (a), listview (lv) {}

KDE_NO_CDTOR_EXPORT ListViewItem::ListViewItem (PlayListView *v, const NodePtr & e) : QListViewItem (v), m_elm (e), listview (v) {}

KDE_NO_CDTOR_EXPORT void ListViewItem::paintCell (QPainter * p, const QColorGroup & cg, int column, int width, int align) {
    if (m_elm && m_elm->state == Node::state_began) {
        QColorGroup mycg (cg);
        mycg.setColor (QColorGroup::Foreground, listview->activeColor ());
        mycg.setColor (QColorGroup::Text, listview->activeColor ());
        QListViewItem::paintCell (p, mycg, column, width, align);
    } else
        QListViewItem::paintCell (p, cg, column, width, align);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT PlayListView::PlayListView (QWidget * parent, View * view, KActionCollection * ac)
 : KListView (parent, "kde_kmplayer_playlist"),
   m_view (view),
   m_find_dialog (0L),
   m_active_color (255, 255, 255),
   m_show_all_nodes (false),
   m_ignore_expanded (false) {
    addColumn (QString::null);
    header()->hide ();
    setTreeStepSize (15);
    //setRootIsDecorated (true);
    setSorting (-1);
    setAcceptDrops (true);
    setDropVisualizer (true);
    setItemsRenameable (true);
    m_itemmenu = new QPopupMenu (this);
    folder_pix = KGlobal::iconLoader ()->loadIcon (QString ("folder"), KIcon::Small);
    auxiliary_pix = KGlobal::iconLoader ()->loadIcon (QString ("folder_grey"), KIcon::Small);
    video_pix = KGlobal::iconLoader ()->loadIcon (QString ("video"), KIcon::Small);
    unknown_pix = KGlobal::iconLoader ()->loadIcon (QString ("unknown"), KIcon::Small);
    menu_pix = KGlobal::iconLoader ()->loadIcon (QString ("player_playlist"), KIcon::Small);
    config_pix = KGlobal::iconLoader ()->loadIcon (QString ("configure"), KIcon::Small);
    url_pix = KGlobal::iconLoader ()->loadIcon (QString ("www"), KIcon::Small);
    m_itemmenu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("editcopy"), KIcon::Small, 0, true), i18n ("&Copy to Clipboard"), this, SLOT (copyToClipboard ()), 0, 0);
    m_itemmenu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("bookmark_add"), KIcon::Small, 0, true), i18n ("&Add Bookmark"), this, SLOT (addBookMark ()), 0, 1);
    m_itemmenu->insertItem (i18n ("&Show all"), this, SLOT (toggleShowAllNodes ()), 0, 2);
    m_itemmenu->insertSeparator ();
    KAction * find = KStdAction::find (this, SLOT (slotFind ()), ac, "find");
    m_find_next = KStdAction::findNext (this, SLOT(slotFindNext()), ac, "next");
    m_find_next->setEnabled (false);
    find->plug (m_itemmenu);
    m_find_next->plug (m_itemmenu);
    connect (this, SIGNAL (contextMenuRequested (QListViewItem *, const QPoint &, int)), this, SLOT (contextMenuItem (QListViewItem *, const QPoint &, int)));
    connect (this, SIGNAL (expanded (QListViewItem *)),
             this, SLOT (itemExpanded (QListViewItem *)));
    connect (this, SIGNAL (dropped (QDropEvent *, QListViewItem *)),
             this, SLOT (itemDropped (QDropEvent *, QListViewItem *)));
    connect (this, SIGNAL (itemRenamed (QListViewItem *)),
             this, SLOT (itemIsRenamed (QListViewItem *)));
}

KDE_NO_CDTOR_EXPORT PlayListView::~PlayListView () {
}

void PlayListView::populate (NodePtr e, NodePtr focus, QListViewItem * item, QListViewItem ** curitem) {
    m_have_dark_nodes |= !e->expose ();
    if (!m_show_all_nodes && !e->expose ()) {
        QListViewItem * up = item->parent ();
        if (up) {
            delete item;
            for (NodePtr c = e->lastChild (); c; c = c->previousSibling ())
                populate (c, focus, new ListViewItem (up, c, this), curitem);
            return;
        }
    }
    Mrl * mrl = e->mrl ();
    QString text (e->nodeName());
    if (mrl && !m_show_all_nodes) {
        if (mrl->pretty_name.isEmpty ()) {
            if (!mrl->src.isEmpty())
                text = KURL(mrl->src).prettyURL();
            else if (e->isDocument ())
                text = e->hasChildNodes () ? i18n ("unnamed") : i18n ("empty");
        } else
            text = mrl->pretty_name;
    } else if (!strcmp (e->nodeName (), "#text"))
        text = e->nodeValue ();
    item->setText(0, text);
    if (focus == e)
        *curitem = item;
    if (e->active ())
        ensureItemVisible (item);
    for (NodePtr c = e->lastChild (); c; c = c->previousSibling ())
        populate (c, focus, new ListViewItem (item, c, this), curitem);
    if (e->isElementNode ()) {
        AttributePtr a = convertNode<Element> (e)->attributes ()->first ();
        if (a) {
            m_have_dark_nodes = true;
            if (m_show_all_nodes) {
                ListViewItem * as = new ListViewItem (item, e, this);
                as->setText (0, i18n ("[attributes]"));
                as->setPixmap (0, menu_pix);
                for (; a; a = a->nextSibling ()) {
                    ListViewItem * ai = new ListViewItem (as, a, this);
                    ai->setText (0, QString ("%1=%2").arg (a->nodeName ()).arg (a->nodeValue ()));
                    ai->setPixmap (0, config_pix);
                }
            }
        }
    }
    QPixmap & pix = e->isMrl() ? video_pix : (item->firstChild ()) ? (e->auxiliaryNode () ? auxiliary_pix : folder_pix) : unknown_pix;
    item->setPixmap (0, pix);
}

void PlayListView::updateTree (NodePtr root, NodePtr active) {
    // TODO, if root is same as rootitems->m_elm and treeversion is the same
    // and show all nodes is unchanged then only update the cells
    m_ignore_expanded = true;
    m_have_dark_nodes = false;
    QWidget * w = focusWidget ();
    if (w && w != this)
        w->clearFocus ();
    //setSelected (firstChild (), true);
    clear ();
    if (m_current_find_elm && m_current_find_elm->document () != root) {
        m_current_find_elm = 0L;
        m_current_find_attr = 0L;
        m_find_next->setEnabled (false);
    }
    if (!root) return;
    QListViewItem * curitem = 0L;
    ListViewItem * rootitem = new ListViewItem (this, root);
    populate (root, active, rootitem, &curitem);
    if (rootitem->firstChild () && !rootitem->isOpen ())
        setOpen (rootitem, true);
    rootitem->setPixmap (0, url_pix);
    if (curitem) {
        setSelected (curitem, true);
        ensureItemVisible (curitem);
    }
    m_itemmenu->setItemEnabled (2, m_have_dark_nodes);
    if (!m_have_dark_nodes && m_show_all_nodes)
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

KDE_NO_EXPORT void PlayListView::contextMenuItem (QListViewItem * vi, const QPoint & p, int) {
    if (vi) {
        ListViewItem * item = static_cast <ListViewItem *> (vi);
        if (item->m_elm || item->m_attr) {
            m_itemmenu->setItemEnabled (1, item->m_attr || (item->m_elm && (item->m_elm->isMrl () || item->m_elm->isDocument ()) && item->m_elm->mrl ()->bookmarkable));
            m_itemmenu->exec (p);
        }
    } else
        m_view->controlPanel ()->popupMenu ()->exec (p);
}

void PlayListView::itemExpanded (QListViewItem * item) {
    if (!m_ignore_expanded && item->childCount () == 1) {
        ListViewItem * child_item = static_cast<ListViewItem*>(item->firstChild ());
        child_item->setOpen (m_show_all_nodes || (child_item->m_elm && child_item->m_elm->expose ()));
    }
}

void PlayListView::copyToClipboard () {
    ListViewItem * item = static_cast <ListViewItem *> (currentItem ());
    QString text = item->text (0);
    if (item->m_elm) {
        Mrl * mrl = item->m_elm->mrl ();
        if (mrl)
            text = mrl->src;
    }
    QApplication::clipboard()->setText (text);
}

void PlayListView::addBookMark () {
    ListViewItem * item = static_cast <ListViewItem *> (currentItem ());
    if (item->m_elm) {
        Mrl * mrl = item->m_elm->mrl ();
        KURL url (mrl ? mrl->src : QString (item->m_elm->nodeName ()));
        emit addBookMark (mrl->pretty_name.isEmpty () ? url.prettyURL () : mrl->pretty_name, url.url ());
    }
}

void PlayListView::toggleShowAllNodes () {
    m_show_all_nodes = !m_show_all_nodes;
    m_itemmenu->setItemChecked (2, m_show_all_nodes);
    ListViewItem * first_item = static_cast <ListViewItem *> (firstChild ());
    if (first_item) {
        NodePtr root = first_item->m_elm;
        NodePtr cur;
        ListViewItem * cur_item = static_cast <ListViewItem *> (currentItem ());
        if (cur_item)
            cur = cur_item->m_elm;
        updateTree (root, cur);
    }
    if (m_current_find_elm && !m_show_all_nodes) {
        if (!m_current_find_elm->expose ())
            m_current_find_elm = 0L;
        m_current_find_attr = 0L;
    }
}

KDE_NO_EXPORT bool PlayListView::acceptDrag (QDropEvent * de) const {
    return isDragValid (de);
}

KDE_NO_EXPORT void PlayListView::itemDropped (QDropEvent * de, QListViewItem *after) {
    if (after) {
        NodePtr n = static_cast <ListViewItem *> (after)->m_elm;
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
            ListViewItem * citem = static_cast <ListViewItem*> (currentItem ());
            for (int i = sl.size (); i > 0; i--) {
                Node * ni = new KMPlayer::GenericURL (d, sl[i-1].url ());
                if (as_child)
                    n->insertBefore (ni, n->firstChild ());
                else
                    n->parentNode ()->insertBefore (ni, n->nextSibling ());
            }
            ListViewItem * ritem = static_cast<ListViewItem*>(firstChild());
            if (ritem)
                updateTree (ritem->m_elm, citem ? citem->m_elm : NodePtrW ());
            return;
        }
    }
    m_view->dropEvent (de);
}

KDE_NO_EXPORT void PlayListView::itemIsRenamed (QListViewItem * qitem) {
    ListViewItem * item = static_cast <ListViewItem *> (qitem);
    if (item->m_elm) {
        if (!item->m_elm->isEditable ()) {
            item->setText (0, QString (item->m_elm->nodeName()));
        } else
            item->m_elm->setNodeName (item->text (0));
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
        ListViewItem * pi = static_cast <ListViewItem *> (item->parent ());
        if (pi && pi->m_elm)
            pi->m_elm->document ()->m_tree_version++;
    }
}

KDE_NO_EXPORT void PlayListView::rename (QListViewItem * qitem, int c) {
    ListViewItem * item = static_cast <ListViewItem *> (qitem);
    if (m_show_all_nodes && item && item->m_attr) {
        ListViewItem * pi = static_cast <ListViewItem *> (qitem->parent ());
        if (pi && pi->m_elm && pi->m_elm->isEditable ())
            KListView::rename (item, c);
    } else if (item && item->m_elm && item->m_elm->isEditable ())
        KListView::rename (item, c);
}

KDE_NO_EXPORT void PlayListView::editCurrent () {
    QListViewItem * qitem = selectedItem ();
    if (qitem)
        rename (qitem, 0);
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

KDE_NO_EXPORT bool PlayListView::findNodeInTree (NodePtr n, QListViewItem *& item) {
    //kdDebug () << "item:" << item->text (0) << " n:" << (n ? n->nodeName () : "null" )  <<endl;
    if (!n)
        return true;
    if (!findNodeInTree (n->parentNode (), item)) // get right item
        return false; // hmpf
    if (static_cast <ListViewItem *> (item)->m_elm == n)  // top node
        return true;
    for (QListViewItem * ci = item->firstChild(); ci; ci = ci->nextSibling ()) {
        //kdDebug () << "ci:" << ci->text (0) << " n:" << n->nodeName () <<endl;
        if (static_cast <ListViewItem *> (ci)->m_elm == n) {
            item = ci;
            return true;
        }
    }
    return !m_show_all_nodes;
    
}

KDE_NO_EXPORT void PlayListView::slotFindOk () {
    if (!m_find_dialog)
        return;
    m_find_dialog->hide ();
    long opt = m_find_dialog->options ();
    if (opt & KFindDialog::FromCursor && currentItem ()) {
        ListViewItem * lvi = static_cast <ListViewItem *> (currentItem ());
        if (lvi && lvi->m_elm)
             m_current_find_elm = lvi->m_elm;
        else if (lvi && lvi->m_attr) {
            ListViewItem*pi=static_cast<ListViewItem*>(currentItem()->parent());
            if (pi) {
                m_current_find_attr = lvi->m_attr;
                m_current_find_elm = pi->m_elm;
            }
        }
    } else if (!(opt & KFindDialog::FindIncremental))
        m_current_find_elm = 0L;
    if (!m_current_find_elm) {
        ListViewItem * lvi = static_cast <ListViewItem *> (firstChild ());
        if (lvi)
            m_current_find_elm = lvi->m_elm;
    }
    if (m_current_find_elm)
        slotFindNext ();
}

/* A bit tricky, but between the find's ListViewItems might be gone, so
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
    while (!found && n) {
        if (m_show_all_nodes || n->expose ()) {
            bool elm = n->isElementNode ();
            QString val = n->nodeName ();
            if (elm && !m_show_all_nodes) {
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
            } else if (elm && m_show_all_nodes) {
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
                }
            }
        }
    }
    m_current_find_elm = n;
    kdDebug () << " search for " << str << "=" << (node ? node->nodeName () : "not found") << " next:" << (n ? n->nodeName () : " not found") << endl;
    QListViewItem * fc = firstChild ();
    if (found) {
        if (!findNodeInTree (node, fc)) {
            m_current_find_elm = 0L;
            kdDebug () << "node not found in tree" << endl;
        } else if (fc) {
            setSelected (fc, true);
            if (m_current_find_attr && fc->firstChild () && fc->firstChild ()->firstChild ())
                ensureItemVisible (fc->firstChild ()->firstChild ());
            ensureItemVisible (fc);
        } else
            kdDebug () << "node not found" << endl;
    }
    m_find_next->setEnabled (!!m_current_find_elm);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT TextEdit::TextEdit (QWidget * parent, View * view) : QTextEdit (parent, "kde_kmplayer_console"), m_view (view) {
    setReadOnly (true);
    setPaper (QBrush (QColor (0, 0, 0)));
    setColor (QColor (0xB2, 0xB2, 0xB2));
}

KDE_NO_EXPORT void TextEdit::contextMenuEvent (QContextMenuEvent * e) {
    m_view->controlPanel ()->popupMenu ()->exec (e->globalPos ());
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT InfoWindow::InfoWindow (QWidget * parent, View * view) : QTextEdit (parent, "kde_kmplayer_console"), m_view (view) {
    setReadOnly (true);
    setLinkUnderline (false);
}

KDE_NO_EXPORT void InfoWindow::contextMenuEvent (QContextMenuEvent * e) {
    m_view->controlPanel ()->popupMenu ()->exec (e->globalPos ());
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT View::View (QWidget *parent, const char *name)
  : KMediaPlayer::View (parent, name),
    m_image (0L),
    m_control_panel (0L),
    m_status_bar (0L),
    m_volume_slider (0L),
    m_mixer_object ("kicker"),
    m_controlpanel_mode (CP_Show),
    m_old_controlpanel_mode (CP_Show),
    m_statusbar_mode (SB_Hide),
    controlbar_timer (0),
    m_keepsizeratio (false),
    m_playing (false),
    m_mixer_init (false),
    m_inVolumeUpdate (false),
    m_tmplog_needs_eol (false),
    m_revert_fullscreen (false),
    m_no_info (false)
{
    setEraseColor (QColor (0, 0, 255));
}

KDE_NO_EXPORT void View::dropEvent (QDropEvent * de) {
    KURL::List sl;
    if (KURLDrag::canDecode (de)) {
        KURLDrag::decode (de, sl);
    } else if (QTextDrag::canDecode (de)) {
        QString text;
        QTextDrag::decode (de, text);
        sl.push_back (KURL (text));
    }
    if (sl.size () > 0) {
        for (unsigned i = 0; i < sl.size (); i++)
            sl [i] = KURL::decode_string (sl [i].url ());
        m_widgetstack->visibleWidget ()->setFocus ();
        emit urlDropped (sl);
        de->accept ();
    }
}

KDE_NO_EXPORT void View::dragEnterEvent (QDragEnterEvent* dee) {
    if (isDragValid (dee))
        dee->accept ();
}

KDE_NO_EXPORT void View::init (KActionCollection * action_collection) {
    //setBackgroundMode(Qt::NoBackground);
    QPalette pal (QColor (64, 64,64), QColor (32, 32, 32));
    QVBoxLayout * viewbox = new QVBoxLayout (this, 0, 0);
    m_dockarea = new KDockArea (this, "kde_kmplayer_dock_area");
    m_dock_video = new KDockWidget (m_dockarea->manager (), 0, KGlobal::iconLoader ()->loadIcon (QString ("kmplayer"), KIcon::Small), m_dockarea);
    m_dock_video->setDockSite (KDockWidget::DockLeft | KDockWidget::DockBottom | KDockWidget::DockRight | KDockWidget::DockTop);
    m_dock_video->setEnableDocking(KDockWidget::DockNone);
    m_view_area = new ViewArea (m_dock_video, this);
    m_dock_video->setWidget (m_view_area);
    m_dockarea->setMainDockWidget (m_dock_video);
    m_dock_playlist = m_dockarea->createDockWidget (i18n ("Play List"), KGlobal::iconLoader ()->loadIcon (QString ("player_playlist"), KIcon::Small));
    m_playlist = new PlayListView (m_dock_playlist, this, action_collection);
    m_playlist->setPaletteBackgroundColor (QColor (0, 0, 0));
    m_playlist->setPaletteForegroundColor (QColor (0xB2, 0xB2, 0xB2));
    m_dock_playlist->setWidget (m_playlist);
    viewbox->addWidget (m_dockarea);
    m_widgetstack = new QWidgetStack (m_view_area);
    m_control_panel = new ControlPanel (m_view_area, this);
    m_control_panel->setMaximumSize (2500, controlPanel ()->maximumSize ().height ());
    m_status_bar = new StatusBar (this);
    m_status_bar->insertItem (QString (""), 0);
    QSize sbsize = m_status_bar->sizeHint ();
    m_status_bar->hide ();
    m_status_bar->setMaximumSize (2500, sbsize.height ());
    m_viewer = new Viewer (m_widgetstack, this);
    m_widgettypes [WT_Video] = m_viewer;
#if KDE_IS_VERSION(3,1,90)
    setVideoWidget (m_view_area);
#endif

    m_multiedit = new TextEdit (m_widgetstack, this);
    m_multiedit->setTextFormat (Qt::PlainText);
    QFont fnt = KGlobalSettings::fixedFont ();
    m_multiedit->setFont (fnt);
    m_widgettypes[WT_Console] = m_multiedit;

    m_widgettypes[WT_Picture] = new KMPlayerPictureWidget (m_widgetstack, this);

    m_dock_infopanel = m_dockarea->createDockWidget ("infopanel", KGlobal::iconLoader ()->loadIcon (QString ("info"), KIcon::Small));
    m_infopanel = new InfoWindow (m_dock_infopanel, this);
    m_dock_infopanel->setWidget (m_infopanel);

    m_widgetstack->addWidget (m_viewer);
    m_widgetstack->addWidget (m_multiedit);
    m_widgetstack->addWidget (m_widgettypes[WT_Picture]);

    setFocusPolicy (QWidget::ClickFocus);

    setAcceptDrops (true);
    m_view_area->resizeEvent (0L);
    kdDebug() << "View " << (unsigned long) (m_viewer->embeddedWinId()) << endl;

    XSelectInput (qt_xdisplay (), m_viewer->embeddedWinId (), 
               //KeyPressMask | KeyReleaseMask |
               KeyPressMask |
               //EnterWindowMask | LeaveWindowMask |
               //FocusChangeMask |
               ExposureMask |
               StructureNotifyMask |
               PointerMotionMask
              );
    kapp->installX11EventFilter (this);
}

KDE_NO_CDTOR_EXPORT View::~View () {
    delete m_image;
    if (m_view_area->parent () != this)
        delete m_view_area;
}

void View::setInfoMessage (const QString & msg) {
    bool ismain = m_dockarea->getMainDockWidget () == m_dock_infopanel;
    if (msg.isEmpty ()) {
        if (!ismain)
            m_dock_infopanel->undock ();
       m_infopanel->clear ();
    } else if (ismain || !m_no_info) {
        if (m_dock_infopanel->mayBeShow ())
          m_dock_infopanel->manualDock(m_dock_video,KDockWidget::DockBottom,80);
        m_infopanel->setText (msg);
    }
}

void View::setStatusMessage (const QString & msg) {
    if (m_statusbar_mode != SB_Hide)
        m_status_bar->changeItem (msg, 0);
}

void View::toggleShowPlaylist () {
    if (m_controlpanel_mode == CP_Only)
        return;
    if (m_dock_playlist->mayBeShow ()) {
        if (m_dock_playlist->isDockBackPossible ())
            m_dock_playlist->dockBack ();
        else {
            bool horz = true;
            QStyle & style = m_playlist->style ();
            int h = style.pixelMetric (QStyle::PM_ScrollBarExtent, m_playlist);
            h += style.pixelMetric(QStyle::PM_DockWindowFrameWidth, m_playlist);
            h +=style.pixelMetric(QStyle::PM_DockWindowHandleExtent,m_playlist);
            for (QListViewItem *i=m_playlist->firstChild();i;i=i->itemBelow()) {
                h += i->height ();
                if (h > int (0.5 * height ())) {
                    horz = false;
                    break;
                }
            }
            int perc = 30;
            if (horz && 100 * h / height () < perc)
                perc = 100 * h / height ();
            m_dock_playlist->manualDock (m_dock_video, horz ? KDockWidget::DockTop : KDockWidget::DockLeft, perc);
        }
    } else
        m_dock_playlist->undock ();
}

void View::setViewOnly () {
    if (m_dock_playlist->mayBeHide ())
        m_dock_playlist->undock ();
    if (m_dock_infopanel->mayBeHide ())
       m_dock_infopanel->undock ();
}

void View::setInfoPanelOnly () {
    if (m_dock_playlist->mayBeHide ())
        m_dock_playlist->undock ();
    m_dock_video->setEnableDocking (KDockWidget::DockCenter);
    m_dock_video->undock ();
    m_dock_infopanel->setEnableDocking (KDockWidget::DockNone);
    m_dockarea->setMainDockWidget (m_dock_infopanel);
}

void View::setPlaylistOnly () {
    if (m_dock_infopanel->mayBeHide ())
       m_dock_infopanel->undock ();
    m_dock_video->setEnableDocking (KDockWidget::DockCenter);
    m_dock_video->undock ();
    m_dock_playlist->setEnableDocking (KDockWidget::DockNone);
    m_dockarea->setMainDockWidget (m_dock_playlist);
}

bool View::setPicture (const QString & path) {
    delete m_image;
    if (path.isEmpty ())
        m_image = 0L;
    else {
        m_image = new QPixmap (path);
        if (m_image->isNull ()) {
            delete m_image;
            m_image = 0L;
            kdDebug() << "View::setPicture failed " << path << endl;
        }
    }
    if (!m_image) {
        m_widgetstack->raiseWidget (m_viewer);
    } else {
        m_widgettypes[WT_Picture]->setPaletteBackgroundPixmap (*m_image);
        m_widgetstack->raiseWidget (m_widgettypes[WT_Picture]);
    }
    return m_image;
}

KDE_NO_EXPORT void View::updateVolume () {
    if (m_mixer_init && !m_volume_slider)
        return;
    QByteArray data, replydata;
    QCString replyType;
    int volume;
    bool has_mixer = kapp->dcopClient ()->call (m_mixer_object, "Mixer0",
            "masterVolume()", data, replyType, replydata);
    if (!has_mixer) {
        m_mixer_object = "kmix";
        has_mixer = kapp->dcopClient ()->call (m_mixer_object, "Mixer0",
                "masterVolume()", data, replyType, replydata);
    }
    if (has_mixer) {
        QDataStream replystream (replydata, IO_ReadOnly);
        replystream >> volume;
        if (!m_mixer_init) {
            QLabel * mixer_label = new QLabel (i18n ("Volume:"), m_control_panel->popupMenu ());
            m_control_panel->popupMenu ()->insertItem (mixer_label, -1, 4);
            m_volume_slider = new QSlider (0, 100, 10, volume, Qt::Horizontal, m_control_panel->popupMenu ());
            connect(m_volume_slider, SIGNAL(valueChanged(int)), this,SLOT(setVolume(int)));
            m_control_panel->popupMenu ()->insertItem (m_volume_slider, ControlPanel::menu_volume, 5);
            m_control_panel->popupMenu ()->insertSeparator (6);
        } else {
            m_inVolumeUpdate = true;
            m_volume_slider->setValue (volume);
            m_inVolumeUpdate = false;
        }
    } else if (m_volume_slider) {
        m_control_panel->popupMenu ()->removeItemAt (6);
        m_control_panel->popupMenu ()->removeItemAt (5);
        m_control_panel->popupMenu ()->removeItemAt (4);
        m_volume_slider = 0L;
    }
    m_mixer_init = true;
}

void View::showWidget (WidgetType wt) {
    m_widgetstack->raiseWidget (m_widgettypes [wt]);
    if (m_widgetstack->visibleWidget () == m_widgettypes[WT_Console])
        addText (QString (""), false);
    updateLayout ();
}

void View::toggleVideoConsoleWindow () {
    WidgetType wt = WT_Console;
    if (m_widgetstack->visibleWidget () == m_widgettypes[WT_Console]) {
        wt = WT_Video;
        m_control_panel->popupMenu ()->changeItem (ControlPanel::menu_video, KGlobal::iconLoader ()->loadIconSet (QString ("konsole"), KIcon::Small, 0, true), i18n ("Con&sole"));
    } else
        m_control_panel->popupMenu ()->changeItem (ControlPanel::menu_video, KGlobal::iconLoader ()->loadIconSet (QString ("video"), KIcon::Small, 0, true), i18n ("V&ideo"));
    showWidget (wt);
    emit windowVideoConsoleToggled (int (wt));
}

void View::setControlPanelMode (ControlPanelMode m) {
    killTimer (controlbar_timer);
    controlbar_timer = 0L;
    m_old_controlpanel_mode = m_controlpanel_mode = m;
    if (m_playing && isFullScreen())
        m_controlpanel_mode = CP_AutoHide;
    if (m_control_panel)
        if (m_controlpanel_mode == CP_Show || m_controlpanel_mode == CP_Only)
            m_control_panel->show ();
        else if (m_controlpanel_mode == CP_AutoHide) { 
            if (m_playing)
                delayedShowButtons (false);
            else
                m_control_panel->show ();
        } else
            m_control_panel->hide ();
    //m_view_area->setMouseTracking (m_controlpanel_mode == CP_AutoHide && m_playing);
    m_view_area->resizeEvent (0L);
}

void View::setStatusBarMode (StatusBarMode m) {
    m_statusbar_mode = m;
    if (m == SB_Hide)
        m_status_bar->hide ();
    else
        m_status_bar->show ();
    m_view_area->resizeEvent (0L);
}

KDE_NO_EXPORT void View::delayedShowButtons (bool show) {
    if (m_controlpanel_mode != CP_AutoHide || controlbar_timer ||
        (m_control_panel &&
         (show && m_control_panel->isVisible ()) || 
         (!show && !m_control_panel->isVisible ())))
        return;
    controlbar_timer = startTimer (500);
}

KDE_NO_EXPORT void View::setVolume (int vol) {
    if (m_inVolumeUpdate) return;
    QByteArray data;
    QDataStream arg( data, IO_WriteOnly );
    arg << vol;
    if (!kapp->dcopClient()->send (m_mixer_object, "Mixer0", "setMasterVolume(int)", data))
        kdWarning() << "Failed to update volume" << endl;
}

KDE_NO_EXPORT void View::updateLayout () {
    if (m_controlpanel_mode == CP_Only)
        m_control_panel->setMaximumSize (2500, height ());
    m_view_area->resizeEvent (0L);
}

void View::setKeepSizeRatio (bool b) {
    if (m_keepsizeratio != b) {
        m_keepsizeratio = b;
        updateLayout ();
        m_view_area->update ();
    }
}

KDE_NO_EXPORT void View::timerEvent (QTimerEvent * e) {
    if (e->timerId () == controlbar_timer) {
        controlbar_timer = 0;
        if (!m_playing)
            return;
        int vert_buttons_pos = m_view_area->height ();
        int mouse_pos = m_view_area->mapFromGlobal (QCursor::pos ()).y();
        int cp_height = m_control_panel->maximumSize ().height ();
        bool mouse_on_buttons = (//m_view_area->hasMouse () && 
                mouse_pos >= vert_buttons_pos-cp_height &&
                mouse_pos <= vert_buttons_pos);
        if (m_control_panel)
            if (mouse_on_buttons && !m_control_panel->isVisible ())
                m_control_panel->show ();
            else if (!mouse_on_buttons && m_control_panel->isVisible ())
                m_control_panel->hide ();
    }
    killTimer (e->timerId ());
    m_view_area->resizeEvent (0L);
}

void View::addText (const QString & str, bool eol) {
    if (m_tmplog_needs_eol)
        tmplog += QChar ('\n');
    tmplog += str;
    m_tmplog_needs_eol = eol;
    if (m_widgetstack->visibleWidget () != m_widgettypes[WT_Console] &&
            tmplog.length () < 7500)
        return;
    if (eol) {
        m_multiedit->append (tmplog);
        tmplog.truncate (0);
        m_tmplog_needs_eol = false;
    } else {
        int pos = tmplog.findRev (QChar ('\n'));
        if (pos >= 0) {
            m_multiedit->append (tmplog.left (pos));
            tmplog = tmplog.mid (pos+1);
        }
    }
    int p = m_multiedit->paragraphs ();
    if (5000 < p) {
        m_multiedit->setSelection (0, 0, p - 4499, 0);
        m_multiedit->removeSelectedText ();
    }
    m_multiedit->setCursorPosition (m_multiedit->paragraphs () - 1, 0);
}

/* void View::print (QPrinter *pPrinter)
{
    QPainter printpainter;
    printpainter.begin (pPrinter);

    // TODO: add your printing code here

    printpainter.end ();
}*/

KDE_NO_EXPORT void View::videoStart () {
    if (m_playing) return; //FIXME: make symetric with videoStop
    if (m_widgetstack->visibleWidget () == m_widgettypes[WT_Picture])
        m_widgetstack->raiseWidget (m_viewer);
    m_playing = true;
    m_revert_fullscreen = !isFullScreen();
    setControlPanelMode (m_old_controlpanel_mode);
}

KDE_NO_EXPORT void View::videoStop () {
    if (m_control_panel && m_controlpanel_mode == CP_AutoHide) {
        m_control_panel->show ();
        //m_view_area->setMouseTracking (false);
    }
    m_playing = false;
    XClearWindow (qt_xdisplay(), m_viewer->embeddedWinId ());
    m_view_area->resizeEvent (0L);
}

KDE_NO_EXPORT void View::leaveEvent (QEvent *) {
    if (m_controlpanel_mode == CP_AutoHide && m_playing)
        delayedShowButtons (false);
}

KDE_NO_EXPORT void View::reset () {
    if (m_revert_fullscreen && isFullScreen())
        m_control_panel->popupMenu ()->activateItemAt (m_control_panel->popupMenu ()->indexOf (ControlPanel::menu_fullscreen)); 
        //m_view_area->fullScreen ();
    videoStop ();
    m_viewer->show ();
}

bool View::isFullScreen () const {
    return m_view_area->isFullScreen ();
}

void View::fullScreen () {
    if (!m_view_area->isFullScreen()) {
        m_sreensaver_disabled = false;
        QByteArray data, replydata;
        QCString replyType;
        if (kapp->dcopClient ()->call ("kdesktop", "KScreensaverIface",
                    "isEnabled()", data, replyType, replydata)) {
            bool enabled;
            QDataStream replystream (replydata, IO_ReadOnly);
            replystream >> enabled;
            if (enabled)
                m_sreensaver_disabled = kapp->dcopClient()->send
                    ("kdesktop", "KScreensaverIface", "enable(bool)", "false");
        }
        //if (m_keepsizeratio && m_viewer->aspect () < 0.01)
        //    m_viewer->setAspect (1.0 * m_viewer->width() / m_viewer->height());
        m_view_area->fullScreen();
        m_control_panel->popupMenu ()->setItemVisible (ControlPanel::menu_zoom, false);
        m_widgetstack->visibleWidget ()->setFocus ();
    } else {
        if (m_sreensaver_disabled)
            m_sreensaver_disabled = !kapp->dcopClient()->send
                ("kdesktop", "KScreensaverIface", "enable(bool)", "true");
        m_view_area->fullScreen();
        m_control_panel->popupMenu ()->setItemVisible (ControlPanel::menu_zoom, true);
    }
    setControlPanelMode (m_old_controlpanel_mode);
    emit fullScreenChanged ();
}

KDE_NO_EXPORT bool View::x11Event (XEvent * e) {
    switch (e->type) {
        case UnmapNotify:
            if (e->xunmap.event == m_viewer->embeddedWinId ()) {
                videoStart ();
                //hide();
            }
            break;
        case XKeyPress:
            if (e->xkey.window == m_viewer->embeddedWinId ()) {
                KeySym ksym;
                char kbuf[16];
                XLookupString (&e->xkey, kbuf, sizeof(kbuf), &ksym, NULL);
                switch (ksym) {
                    case XK_f:
                    case XK_F:
                        //fullScreen ();
                        break;
                };
            }
            break;
        /*case ColormapNotify:
            fprintf (stderr, "colormap notify\n");
            return true;*/
        case MotionNotify:
            if (m_playing && e->xmotion.window == m_viewer->embeddedWinId ())
                delayedShowButtons (e->xmotion.y > m_view_area->height () -
                                    m_control_panel->maximumSize ().height ());
            m_view_area->mouseMoved ();
            break;
        case MapNotify:
            if (e->xmap.event == m_viewer->embeddedWinId ()) {
                show ();
                QTimer::singleShot (10, m_viewer, SLOT (sendConfigureEvent ()));
            }
            break;
        /*case ConfigureNotify:
            break;
            //return true;*/
        default:
            break;
    }
    return false;
}

//----------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT Viewer::Viewer (QWidget *parent, View * view)
  : QXEmbed (parent), m_bgcolor (0), m_aspect (0.0),
    m_view (view) {
    /*XWindowAttributes xwa;
    XGetWindowAttributes (qt_xdisplay(), winId (), &xwa);
    XSetWindowAttributes xswa;
    xswa.background_pixel = 0;
    xswa.border_pixel = 0;
    xswa.colormap = xwa.colormap;
    create (XCreateWindow (qt_xdisplay (), parent->winId (), 0, 0, 10, 10, 0, 
                           x11Depth (), InputOutput, (Visual*)x11Visual (),
                           CWBackPixel | CWBorderPixel | CWColormap, &xswa));*/
    setAcceptDrops (true);
#if KDE_IS_VERSION(3,1,1)
    setProtocol(QXEmbed::XPLAIN);
#endif
    int scr = DefaultScreen (qt_xdisplay ());
    embed (XCreateSimpleWindow (qt_xdisplay(), view->winId (), 0, 0, width(), height(), 1, BlackPixel (qt_xdisplay(), scr), BlackPixel (qt_xdisplay(), scr)));
    XClearWindow (qt_xdisplay(), embeddedWinId ());
}

KDE_NO_CDTOR_EXPORT Viewer::~Viewer () {
}
    
KDE_NO_EXPORT void Viewer::mouseMoveEvent (QMouseEvent * e) {
    if (e->state () == Qt::NoButton) {
        int cp_height = m_view->controlPanel ()->maximumSize ().height ();
        m_view->delayedShowButtons (e->y () > height () - cp_height);
    }
    m_view->viewArea ()->mouseMoved ();
}

void Viewer::setAspect (float a) {
    m_aspect = a;
}

KDE_NO_EXPORT int Viewer::heightForWidth (int w) const {
    if (m_aspect <= 0.01)
        return 0;
    return int (w/m_aspect); 
}

KDE_NO_EXPORT void Viewer::dropEvent (QDropEvent * de) {
    m_view->dropEvent (de);
}

KDE_NO_EXPORT void Viewer::dragEnterEvent (QDragEnterEvent* dee) {
    m_view->dragEnterEvent (dee);
}
/*
*/
void Viewer::sendKeyEvent (int key) {
    char buf[2] = { char (key), '\0' }; 
    KeySym keysym = XStringToKeysym (buf);
    XKeyEvent event = {
        XKeyPress, 0, true,
        qt_xdisplay (), embeddedWinId (), qt_xrootwin(), embeddedWinId (),
        /*time*/ 0, 0, 0, 0, 0,
        0, XKeysymToKeycode (qt_xdisplay (), keysym), true
    };
    XSendEvent (qt_xdisplay(), embeddedWinId (), FALSE, KeyPressMask, (XEvent *) &event);
    XFlush (qt_xdisplay ());
}

KDE_NO_EXPORT void Viewer::sendConfigureEvent () {
    XConfigureEvent c = {
        ConfigureNotify, 0UL, True,
        qt_xdisplay (), embeddedWinId (), winId (),
        x (), y (), width (), height (),
        0, None, False
    };
    XSendEvent(qt_xdisplay(), c.event, TRUE, StructureNotifyMask, (XEvent*) &c);
    XFlush (qt_xdisplay ());
}

KDE_NO_EXPORT void Viewer::contextMenuEvent (QContextMenuEvent * e) {
    m_view->controlPanel ()->popupMenu ()->exec (e->globalPos ());
}

KDE_NO_EXPORT void Viewer::setBackgroundColor (const QColor & c) {
    if (m_bgcolor != c.rgb ()) {
        m_bgcolor = c.rgb ();
        setCurrentBackgroundColor (c);
    }
}

KDE_NO_EXPORT void Viewer::resetBackgroundColor () {
    setCurrentBackgroundColor (m_bgcolor);
}

KDE_NO_EXPORT void Viewer::setCurrentBackgroundColor (const QColor & c) {
    setPaletteBackgroundColor (c);
    XSetWindowBackground (qt_xdisplay (), embeddedWinId (), c.rgb ());
    XFlush (qt_xdisplay ());
}

#include "kmplayerview.moc"
