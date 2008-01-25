/* This file is part of the KMPlayer application
   Copyright (C) 2003 Koos Vriezen <koos.vriezen@xs4all.nl>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include <qlayout.h>
#include <qlabel.h>
#include <qtimer.h>
#include <qpushbutton.h>
#include <qcheckbox.h>
#include <Q3Table>
#include <qstringlist.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <qgroupbox.h>
#include <qwhatsthis.h>
#include <qtabwidget.h>
#include <qmessagebox.h>
#include <QMenu>
#include <qfontmetrics.h>

#include <klocale.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <klineedit.h>
#include <kurlrequester.h>
#include <kcombobox.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#include <kstandarddirs.h>

#include "kmplayerpartbase.h"
#include "kmplayerprocess.h"
#include "kmplayerconfig.h"
#include "kmplayertvsource.h"
#include "playlistview.h"
#include "kmplayer.h"
#include "kmplayercontrolpanel.h"

static const char * strTV = "TV";
static const char * strTVDriver = "Driver";


KDE_NO_CDTOR_EXPORT TVDevicePage::TVDevicePage (QWidget *parent, KMPlayer::NodePtr dev)
: QFrame (parent, "PageTVDevice"), device_doc (dev) {
    TVDevice * device = KMPlayer::convertNode <TVDevice> (device_doc);
    QVBoxLayout *layout = new QVBoxLayout (this, 5, 2);
    QLabel * deviceLabel = new QLabel (i18n ("Video device:") + device->src, this, 0);
    layout->addWidget (deviceLabel);
    QGridLayout *gridlayout = new QGridLayout (layout, 3, 4);
    QLabel * audioLabel = new QLabel (i18n ("Audio device:"), this);
    audiodevice = new KUrlRequester (KUrl (device->getAttribute ("audio")), this);
    QLabel * nameLabel = new QLabel (i18n ("Name:"), this, 0);
    name = new QLineEdit (device->pretty_name, this, 0);
    QLabel *sizewidthLabel = new QLabel (i18n ("Width:"), this, 0);
    sizewidth = new QLineEdit (device->getAttribute (KMPlayer::StringPool::attr_width), this, 0);
    QLabel *sizeheightLabel = new QLabel (i18n ("Height:"), this, 0);
    sizeheight = new QLineEdit (device->getAttribute (KMPlayer::StringPool::attr_height), this, 0);
    noplayback = new QCheckBox (i18n ("Do not immediately play"), this);
    noplayback->setChecked (!device->getAttribute ("playback").toInt ());
    QWhatsThis::add (noplayback, i18n ("Only start playing after clicking the play button"));
    inputsTab = new QTabWidget (this);
    for (KMPlayer::NodePtr ip = device->firstChild (); ip; ip = ip->nextSibling ()) {
        if (ip->id != id_node_tv_input)
            continue;
        TVInput * input = KMPlayer::convertNode <TVInput> (ip);
        QWidget * widget = new QWidget (this);
        QHBoxLayout *tablayout = new QHBoxLayout (widget, 5, 2);
        if (!input->getAttribute ("tuner").isEmpty ()) {
            QHBoxLayout *horzlayout = new QHBoxLayout ();
            QVBoxLayout *vertlayout = new QVBoxLayout ();
            horzlayout->addWidget (new QLabel (i18n ("Norm:"), widget));
            QComboBox * norms = new QComboBox (widget, "PageTVNorm");
            norms->insertItem (QString ("NTSC"), 0);
            norms->insertItem (QString ("PAL"), 1);
            norms->insertItem (QString ("SECAM"), 2);
            norms->setCurrentText (input->getAttribute ("norm"));
            horzlayout->addWidget (norms);
            vertlayout->addLayout (horzlayout);
            vertlayout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
            Q3Table * table = new Q3Table (90, 2, widget, "PageTVChannels");
            QFontMetrics metrics (table->font ());
            Q3Header *header = table->horizontalHeader();
            header->setLabel (0, i18n ("Channel"));
            header->setLabel (1, i18n ("Frequency (MHz)"));
            int index = 0;
            int first_column_width = QFontMetrics (header->font ()).boundingRect (header->label (0)).width () + 20;
            for (KMPlayer::NodePtr c=input->firstChild();c;c=c->nextSibling()) {
                if (c->id != id_node_tv_channel)
                    continue;
                int strwid = metrics.boundingRect (c->mrl ()->pretty_name).width ();
                if (strwid > first_column_width)
                    first_column_width = strwid + 4;
                table->setItem (index, 0, new Q3TableItem (table, Q3TableItem::Always, c->mrl ()->pretty_name));
                table->setItem (index++, 1, new Q3TableItem (table, Q3TableItem::Always, KMPlayer::convertNode<TVChannel>(c)->getAttribute ("frequency")));
            }
            table->setColumnWidth (0, first_column_width);
            table->setColumnStretchable (1, true);
            tablayout->addWidget (table);
            tablayout->addLayout (vertlayout);
        }
        inputsTab->addTab (widget, input->mrl ()->pretty_name);
    }
    QPushButton * delButton = new QPushButton (i18n ("Delete"), this);
    connect (delButton, SIGNAL (clicked ()), this, SLOT (slotDelete ()));
    gridlayout->addWidget (audioLabel, 0, 0);
    gridlayout->addMultiCellWidget (audiodevice, 0, 0, 1, 3);
    gridlayout->addWidget (nameLabel, 1, 0);
    gridlayout->addMultiCellWidget (name, 1, 1, 1, 3);
    gridlayout->addWidget (sizewidthLabel, 2, 0);
    gridlayout->addWidget (sizewidth, 2, 1);
    gridlayout->addWidget (sizeheightLabel, 2, 2);
    gridlayout->addWidget (sizeheight, 2, 3);
    layout->addWidget (inputsTab);
    layout->addSpacing (5);
    layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum));
    QHBoxLayout *buttonlayout = new QHBoxLayout ();
    buttonlayout->addWidget (noplayback);
    buttonlayout->addItem (new QSpacerItem (0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
    buttonlayout->addWidget (delButton);
    layout->addLayout (buttonlayout);
}

KDE_NO_EXPORT void TVDevicePage::slotDelete () {
    if (KMessageBox::warningYesNo (this, i18n ("You are about to remove this device from the Source menu.\nContinue?"), i18n ("Confirm")) == KMessageBox::Yes)
        emit deleted (this);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT KMPlayerPrefSourcePageTV::KMPlayerPrefSourcePageTV (QWidget *parent, KMPlayerTVSource * tvsource)
: QFrame (parent), m_tvsource (tvsource) {
    QVBoxLayout * mainlayout = new QVBoxLayout (this, 5);
    notebook = new QTabWidget (this);
    notebook->setTabPosition (QTabWidget::Bottom);
    mainlayout->addWidget (notebook);
    QWidget * general = new QWidget (notebook);
    QVBoxLayout *layout = new QVBoxLayout (general);
    QGridLayout *gridlayout = new QGridLayout (layout, 2, 2, 2);
    QLabel *driverLabel = new QLabel (i18n ("Driver:"), general, 0);
    driver = new QLineEdit ("", general, 0);
    QWhatsThis::add (driver, i18n ("dummy, v4l or bsdbt848"));
    QLabel *deviceLabel = new QLabel (i18n ("Device:"), general, 0);
    device = new KUrlRequester (KUrl ("/dev/video"), general);
    QWhatsThis::add(device, i18n("Path to your video device, eg. /dev/video0"));
    scan = new QPushButton (i18n ("Scan..."), general);
    gridlayout->addWidget (driverLabel, 0, 0);
    gridlayout->addWidget (driver, 0, 1);
    gridlayout->addWidget (deviceLabel, 1, 0);
    gridlayout->addWidget (device, 1, 1);
    QHBoxLayout *buttonlayout = new QHBoxLayout ();
    buttonlayout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum));
    buttonlayout->addWidget (scan);
    layout->addLayout (buttonlayout);
    layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
    notebook->insertTab (general, i18n ("General"));
}

KDE_NO_EXPORT void KMPlayerPrefSourcePageTV::showEvent (QShowEvent *) {
    m_tvsource->readXML ();
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT TVNode::TVNode (KMPlayer::NodePtr &d, const QString & s, const char * t, short id, const QString & n) : KMPlayer::GenericMrl (d, s, n, t) {
    this->id = id;
    editable = true;
}

KDE_NO_EXPORT void TVNode::setNodeName (const QString & nn) {
    pretty_name = nn;
    setAttribute (KMPlayer::StringPool::attr_name, nn);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT TVChannel::TVChannel (KMPlayer::NodePtr & d, const QString & n, double freq) : TVNode (d, QString ("tv://"), "channel", id_node_tv_channel, n) {
    setAttribute (KMPlayer::StringPool::attr_name, n);
    setAttribute ("frequency", QString::number (freq, 'f', 2));
}

KDE_NO_CDTOR_EXPORT TVChannel::TVChannel (KMPlayer::NodePtr & d) : TVNode (d, QString ("tv://"), "channel", id_node_tv_channel) {
}

KDE_NO_EXPORT void TVChannel::closed () {
    pretty_name = getAttribute (KMPlayer::StringPool::attr_name);
}

//-----------------------------------------------------------------------------

TVInput::TVInput (KMPlayer::NodePtr & d, const QString & n, int id)
 : TVNode (d, QString ("tv://"), "input", id_node_tv_input, n) {
    setAttribute (KMPlayer::StringPool::attr_name, n);
    setAttribute (KMPlayer::StringPool::attr_id, QString::number (id));
}

KDE_NO_CDTOR_EXPORT TVInput::TVInput (KMPlayer::NodePtr & d) : TVNode (d, QString ("tv://"), "input", id_node_tv_input) {
}

KDE_NO_EXPORT KMPlayer::NodePtr TVInput::childFromTag (const QString & tag) {
    // kDebug () << nodeName () << " childFromTag " << tag;
    if (tag == QString::fromLatin1 ("channel")) {
        return new TVChannel (m_doc);
    } else
        return 0L;
}

KDE_NO_EXPORT void TVInput::closed () {
    //pretty_name = getAttribute (KMPlayer::StringPool::attr_name);
}

KDE_NO_EXPORT void TVInput::setNodeName (const QString & name) {
    Node * p = parentNode ().ptr ();
    QString nm (name);
    if (p && p->id == id_node_tv_device) {
        int pos = name.find (QString (" - ") + p->mrl ()->pretty_name);
        if (pos > -1)
            nm.truncate (pos);
    }
    pretty_name = nm + QString (" - ") + pretty_name;
    TVNode::setNodeName (nm);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT TVDevice::TVDevice (KMPlayer::NodePtr & doc, const QString & d) : TVNode (doc, d, "device", id_node_tv_device), zombie (false) {
    setAttribute ("path", d);
}

KDE_NO_CDTOR_EXPORT TVDevice::TVDevice (KMPlayer::NodePtr & doc)
    : TVNode (doc, i18n ("tv device"), "device", id_node_tv_device), zombie (false) {
}

KDE_NO_CDTOR_EXPORT TVDevice::~TVDevice () {
    if (device_page)
        device_page->deleteLater ();
}

KDE_NO_EXPORT KMPlayer::NodePtr TVDevice::childFromTag (const QString & tag) {
    // kDebug () << nodeName () << " childFromTag " << tag;
    if (tag == QString::fromLatin1 ("input"))
        return new TVInput (m_doc);
    return 0L;
}

KDE_NO_EXPORT void TVDevice::closed () {
    updateNodeName ();
}

KDE_NO_EXPORT void TVDevice::childDone (KMPlayer::NodePtr) {
    finish ();
}

KDE_NO_EXPORT void TVDevice::setNodeName (const QString & name) {
    TVNode::setNodeName (name);
    updateNodeName ();
}

KDE_NO_EXPORT void TVDevice::updateNodeName () {
    pretty_name = getAttribute (KMPlayer::StringPool::attr_name);
    src = getAttribute ("path");
    for (KMPlayer::NodePtr c = firstChild (); c; c = c->nextSibling ())
        if (c->id == id_node_tv_input) {
            TVInput * i = static_cast <TVInput *> (c.ptr ());
            i->pretty_name = i->getAttribute (KMPlayer::StringPool::attr_name) +
                QString (" - ") + pretty_name;
        }
}

KDE_NO_EXPORT void TVDevice::updateDevicePage () {
    if (!device_page)
        return;
    pretty_name = device_page->name->text ();
    setAttribute (KMPlayer::StringPool::attr_name, pretty_name);
    setAttribute ("audio", device_page->audiodevice->lineEdit()->text ());
    setAttribute ("playback", device_page->noplayback->isChecked() ? "0" : "1");
    setAttribute (KMPlayer::StringPool::attr_width, device_page->sizewidth->text ());
    setAttribute (KMPlayer::StringPool::attr_height, device_page->sizeheight->text ());
    int i = 0;
    for (KMPlayer::NodePtr ip = firstChild(); ip; ip=ip->nextSibling(),++i) {
        if (ip->id != id_node_tv_input)
            continue;
        TVInput * input = KMPlayer::convertNode <TVInput> (ip);
        bool ok;
        if (input->getAttribute ("tuner").toInt (&ok) && ok) {
            QWidget * widget = device_page->inputsTab->page (i);
            Q3Table * table = static_cast <Q3Table *> (widget->child ("PageTVChannels", "Q3Table"));
            if (table) {
                input->clearChildren ();
                for (int j = 0; j<table->numRows() && table->item (j, 1); ++j) {
                    input->appendChild (new TVChannel (m_doc, table->item (j, 0)->text (), table->item (j, 1)->text ().toDouble ()));
                }
            }
            QComboBox * norms = static_cast <QComboBox *> (widget->child ("PageTVNorm", "QComboBox"));
            if (norms) {
                input->setAttribute ("norm", norms->currentText ());
            }
        }
    }
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT
TVDocument::TVDocument (KMPlayerTVSource * source)
    : FileDocument (id_node_tv_document, "tv://", source), m_source (source) {
    pretty_name = i18n ("Television");
}

KDE_NO_EXPORT KMPlayer::NodePtr TVDocument::childFromTag (const QString & tag) {
    // kDebug () << nodeName () << " childFromTag " << tag;
    if (tag == QString::fromLatin1 ("device"))
        return new TVDevice (m_doc);
    return FileDocument::childFromTag (tag);
}

KDE_NO_EXPORT void TVDocument::childDone (KMPlayer::NodePtr) {
    finish ();
}

KDE_NO_EXPORT void TVDocument::defer () {
    if (!resolved) {
        resolved = true;
        readFromFile (KStandardDirs::locateLocal ("data", "kmplayer/tv.xml"));
    }
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT KMPlayerTVSource::KMPlayerTVSource (KMPlayerApp * a, QMenu * m)
    : KMPlayerMenuSource (i18n ("TV"), a, m, "tvsource"), m_configpage (0L), scanner (0L), config_read (false) {
    m_url = "tv://";
    m_menu->insertTearOffHandle ();
    connect (m_menu, SIGNAL (aboutToShow ()), this, SLOT (menuAboutToShow ()));
    m_document = new TVDocument (this);
    m_player->settings ()->addPage (this);
    tree_id = static_cast <KMPlayer::View*>(m_player->view ())->playList ()->addTree (m_document, "tvsource", "video-television", KMPlayer::PlayListView::TreeEdit | KMPlayer::PlayListView::Moveable | KMPlayer::PlayListView::Deleteable);
}

KDE_NO_CDTOR_EXPORT KMPlayerTVSource::~KMPlayerTVSource () {
}

KDE_NO_EXPORT void KMPlayerTVSource::activate () {
    m_identified = true;
    //if (m_player->settings ()->showbroadcastbutton)
    //    m_app->view()->controlPanel()->broadcastButton ()->show ();
    if (m_cur_tvdevice && !m_current) {
        for (KMPlayer::NodePtr i = m_cur_tvdevice->firstChild(); i && !m_current; i=i->nextSibling())
            if (i->id == id_node_tv_input) {
                TVInput * input = KMPlayer::convertNode <TVInput> (i);
                bool ok;
                m_cur_tvinput = i;
                if (input->getAttribute ("tuner").toInt (&ok) && ok) {
                    for (KMPlayer::NodePtr c = i->firstChild (); c; c = c->nextSibling ())
                        if (c->id == id_node_tv_channel) {
                            setCurrent (c->mrl ());
                            break;
                        }
                } else
                    m_current = i;
            }
    } else if (!m_cur_tvdevice)
        KMPlayer::Source::reset ();
    if (m_cur_tvdevice) {
        QString playback = static_cast <KMPlayer::Element *> (m_cur_tvdevice.ptr ())->getAttribute (QString::fromLatin1 ("playback"));
        if (playback.isEmpty () || playback.toInt ())
            QTimer::singleShot (0, m_player, SLOT (play ()));
    }
}
/* TODO: playback by
 * ffmpeg -vd /dev/video0 -r 25 -s 768x576 -f rawvideo - |mplayer -nocache -ao arts -rawvideo on:w=768:h=576:fps=25 -quiet -
 */

KDE_NO_EXPORT void KMPlayerTVSource::deactivate () {
    //if (m_player->view () && !m_app->view ()->controlPanel()->broadcastButton ()->isOn ())
    //    m_app->view ()->controlPanel()->broadcastButton ()->hide ();
    reset ();
}

KDE_NO_EXPORT void KMPlayerTVSource::buildMenu () {
    m_menu->clear ();
    int counter = 0;
    for (KMPlayer::NodePtr dp = m_document->firstChild (); dp; dp = dp->nextSibling ())
        if (dp->id == id_node_tv_device)
            m_menu->insertItem (KMPlayer::convertNode <TVDevice> (dp)->pretty_name, this, SLOT (menuClicked (int)), 0, counter++);
}

KDE_NO_EXPORT void KMPlayerTVSource::menuAboutToShow () {
    readXML ();
}

void KMPlayerTVSource::play (KMPlayer::Mrl *mrl) {
    if (mrl && mrl->id == id_node_tv_document) {
        readXML ();
    } else {
        m_current = mrl;
        for (KMPlayer::NodePtr e = mrl; e; e = e->parentNode ()) {
            if (e->id == id_node_tv_device) {
                m_cur_tvdevice = e;
                break;
            } else if (e->id == id_node_tv_input)
                m_cur_tvinput = e;
        }
        if (m_player->source () != this)
            m_player->setSource (this);
        else
            KMPlayer::Source::play (mrl);
        /*else if (m_player->process ()->playing ()) {
            //m_back_request = m_current;
            m_player->process ()->stop ();
        } else {
            buildArguments ();
            if (m_app->broadcasting ())
                QTimer::singleShot (0, m_app->broadcastConfig (), SLOT (startFeed ()));
            else
                KMPlayer::Source::play (mrl);
        }*/
    }
}

KDE_NO_EXPORT KMPlayer::NodePtr KMPlayerTVSource::root () {
    return m_cur_tvinput;
}

KDE_NO_EXPORT void KMPlayerTVSource::setCurrent (KMPlayer::Mrl *mrl) {
    TVChannel * channel = 0L;
    TVInput * input = 0L;
    m_current = mrl;
    KMPlayer::NodePtr elm = m_current;
    if (elm && elm->id == id_node_tv_channel) {
        channel = KMPlayer::convertNode <TVChannel> (elm);
        elm = elm->parentNode ();
    }
    if (elm && elm->id == id_node_tv_input)
        input = KMPlayer::convertNode <TVInput> (elm);
    if (!(channel || (input && input->getAttribute ("tuner").isEmpty ())))
        return;
    m_cur_tvinput = input;
    m_cur_tvdevice = input->parentNode ();
    static_cast <KMPlayer::View*>(m_player->view ())->playList ()->updateTree (0, m_cur_tvinput, m_current, true, false);
    if (m_cur_tvdevice->id != id_node_tv_device) {
        return;
    }
    TVDevice * tvdevice = KMPlayer::convertNode <TVDevice> (m_cur_tvdevice);
    m_identified = true;
    m_audiodevice = tvdevice->getAttribute ("audio");
    m_videodevice = tvdevice->src;
    m_videonorm = input->getAttribute ("norm");
    m_tuner = input->getAttribute (KMPlayer::StringPool::attr_name);
    QString xvport = tvdevice->getAttribute ("xvport");
    if (!xvport.isEmpty ())
        m_xvport = xvport.toInt ();
    QString xvenc = input->getAttribute ("xvenc");
    if (!xvenc.isEmpty ())
        m_xvencoding = xvenc.toInt ();
    QString command;
    command.sprintf ("device=%s:input=%s",
            tvdevice->src.ascii (),
            input->getAttribute (KMPlayer::StringPool::attr_id).ascii ());
    if (channel) {
        QString freq = channel->getAttribute ("frequency");
        m_frequency = (int)(1000 * freq.toDouble ());
        command += QString (":freq=%1").arg (freq);
    } else
        m_frequency = 0;
    if (!m_videonorm.isEmpty ())
        command += QString (":norm=%1").arg (m_videonorm);
    m_app->setCaption (i18n ("TV: ") + (channel ? channel->mrl ()->pretty_name : input->mrl ()->pretty_name), false);
    setDimensions (m_cur_tvdevice,
            tvdevice->getAttribute (KMPlayer::StringPool::attr_width).toInt (),
            tvdevice->getAttribute (KMPlayer::StringPool::attr_height).toInt ());
    m_options.sprintf ("-tv noaudio:driver=%s:%s:width=%d:height=%d -slave -nocache -quiet", tvdriver.ascii (), command.ascii (), width (), height ());
    if (m_player->settings ()->mplayerpost090)
        m_recordcmd.sprintf ("-tv %s:driver=%s:%s:width=%d:height=%d", m_audiodevice.isEmpty () ? "noaudio" : (QString ("forceaudio:adevice=") + m_audiodevice).ascii(), tvdriver.ascii (), command.ascii (), width (), height ());
    else
        m_recordcmd.sprintf ("-tv on:%s:driver=%s:%s:width=%d:height=%d", m_audiodevice.isEmpty () ? "noaudio" : (QString ("forceaudio:adevice=") + m_audiodevice).ascii(), tvdriver.ascii (), command.ascii (), width (), height ());
}

KDE_NO_EXPORT void KMPlayerTVSource::menuClicked (int id) {
    KMPlayer::NodePtr elm = m_document->firstChild ();
    for (; id > 0; --id,  elm = elm->nextSibling ())
        ;
    m_cur_tvdevice = elm;
    m_cur_tvinput = elm->firstChild (); // FIXME
    m_current = 0L;
    m_player->setSource (this);
}

KDE_NO_EXPORT QString KMPlayerTVSource::filterOptions () {
    if (! m_player->settings ()->disableppauto)
        return KMPlayer::Source::filterOptions ();
    return QString ("-vf pp=lb");
}

KDE_NO_EXPORT bool KMPlayerTVSource::hasLength () {
    return false;
}

KDE_NO_EXPORT bool KMPlayerTVSource::isSeekable () {
    return true;
}

KDE_NO_EXPORT QString KMPlayerTVSource::prettyName () {
    QString name (i18n ("TV"));
    //if (m_tvsource)
    //    name += ' ' + m_tvsource->title;
    return name;
}

KDE_NO_EXPORT void KMPlayerTVSource::write (KSharedConfigPtr m_config) {
    if (!config_read) return;
    KConfigGroup (m_config, strTV).writeEntry (strTVDriver, tvdriver);
    static_cast <TVDocument *> (m_document.ptr ())->writeToFile
        (KStandardDirs::locateLocal ("data", "kmplayer/tv.xml"));
    kDebug () << "KMPlayerTVSource::write XML";
}

KDE_NO_EXPORT void KMPlayerTVSource::readXML () {
    if (config_read) return;
    config_read = true;
    kDebug () << "KMPlayerTVSource::readXML";
    m_document->defer ();
    static_cast <KMPlayer::View*>(m_player->view ())->playList ()->updateTree (tree_id, m_document, 0, false, false);
    buildMenu ();
    sync (false);
}

KDE_NO_EXPORT void KMPlayerTVSource::read (KSharedConfigPtr m_config) {
    tvdriver = KConfigGroup (m_config, strTV).readEntry (
            strTVDriver, QString ("v4l"));
}

KDE_NO_EXPORT void KMPlayerTVSource::sync (bool fromUI) {
    if (!m_configpage) return;
    if (m_document && m_document->hasChildNodes ())
        m_app->showBroadcastConfig ();
    else
        m_app->hideBroadcastConfig ();
    if (fromUI) {
        tvdriver = m_configpage->driver->text ();
        for (KMPlayer::NodePtr d=m_document->firstChild();d; d=d->nextSibling())
            if (d->id == id_node_tv_device)
                static_cast <TVDevice *> (d.ptr ())->updateDevicePage ();
    } else {
        m_configpage->driver->setText (tvdriver);
        for (KMPlayer::NodePtr dp = m_document->firstChild (); dp; dp = dp->nextSibling ())
            if (dp->id == id_node_tv_device)
                addTVDevicePage (KMPlayer::convertNode <TVDevice> (dp));
    }
}

KDE_NO_EXPORT void KMPlayerTVSource::prefLocation (QString & item, QString & icon, QString & tab) {
    item = i18n ("Source");
    icon = QString ("source");
    tab = i18n ("TV");
}

KDE_NO_EXPORT QFrame * KMPlayerTVSource::prefPage (QWidget * parent) {
    if (!m_configpage) {
        m_configpage = new KMPlayerPrefSourcePageTV (parent, this);
        scanner = new TVDeviceScannerSource (this);
        connect (m_configpage->scan, SIGNAL(clicked()), this, SLOT(slotScan()));
    }
    return m_configpage;
}

static bool hasTVDevice (KMPlayer::NodePtr doc, const QString & devstr) {
    for (KMPlayer::NodePtr e = doc->firstChild (); e; e = e->nextSibling ())
        if (e->id == id_node_tv_device &&
                KMPlayer::convertNode <TVDevice> (e)->src == devstr)
            return true;
    return false;
}

KDE_NO_EXPORT void KMPlayerTVSource::slotScan () {
    QString devstr = m_configpage->device->lineEdit()->text ();
    if (!hasTVDevice(m_document, devstr)) {
        scanner->scan (devstr, m_configpage->driver->text());
        connect (scanner, SIGNAL (scanFinished (TVDevice *)),
                this, SLOT (slotScanFinished (TVDevice *)));
    } else
        KMessageBox::error (m_configpage, i18n ("Device already present."),
                i18n ("Error"));
}

KDE_NO_EXPORT void KMPlayerTVSource::slotScanFinished (TVDevice * tvdevice) {
    disconnect (scanner, SIGNAL (scanFinished (TVDevice *)),
                this, SLOT (slotScanFinished (TVDevice *)));
    if (tvdevice) {
        tvdevice->zombie = false;
        addTVDevicePage (tvdevice, true);
    } else
        KMessageBox::error(m_configpage,i18n("No device found."),i18n("Error"));
}

KDE_NO_EXPORT void KMPlayerTVSource::addTVDevicePage(TVDevice *dev, bool show) {
    if (dev->device_page)
        dev->device_page->deleteLater ();
    dev->device_page = new TVDevicePage (m_configpage->notebook, dev);
    m_configpage->notebook->insertTab (dev->device_page, dev->pretty_name);
    connect (dev->device_page, SIGNAL (deleted (TVDevicePage *)),
             this, SLOT (slotDeviceDeleted (TVDevicePage *)));
    if (show)
        m_configpage->notebook->setCurrentPage (m_configpage->notebook->count ()-1);
}

KDE_NO_EXPORT void KMPlayerTVSource::slotDeviceDeleted (TVDevicePage *devpage) {
    m_document->removeChild (devpage->device_doc);
    m_configpage->notebook->setCurrentPage (0);
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT TVDeviceScannerSource::TVDeviceScannerSource (KMPlayerTVSource * src)
 : KMPlayer::Source (i18n ("TVScanner"), src->player (), "tvscanner"), m_tvsource (src), m_tvdevice (0L) {
}

KDE_NO_EXPORT void TVDeviceScannerSource::init () {
}

KDE_NO_EXPORT bool TVDeviceScannerSource::processOutput (const QString & line) {
    if (m_nameRegExp.search (line) > -1) {
        m_tvdevice->pretty_name = m_nameRegExp.cap (1);
        m_tvdevice->setAttribute(KMPlayer::StringPool::attr_name,m_tvdevice->pretty_name);
        kDebug() << "Name " << m_tvdevice->pretty_name;
    } else if (m_sizesRegExp.search (line) > -1) {
        m_tvdevice->setAttribute (KMPlayer::StringPool::attr_width,
                m_sizesRegExp.cap(1));
        m_tvdevice->setAttribute (KMPlayer::StringPool::attr_height,
                m_sizesRegExp.cap(2));
        m_tvdevice->setAttribute ("minwidth", m_sizesRegExp.cap (1));
        m_tvdevice->setAttribute ("minheight", m_sizesRegExp.cap (2));
        m_tvdevice->setAttribute ("maxwidth", m_sizesRegExp.cap (3));
        m_tvdevice->setAttribute ("maxheight", m_sizesRegExp.cap (4));
    } else if (m_inputRegExp.search (line) > -1) {
        KMPlayer::NodePtr doc = m_tvsource->document ();
        TVInput * input = new TVInput (doc, m_inputRegExp.cap (2).stripWhiteSpace (),
                                       m_inputRegExp.cap (1).toInt ());
        if (m_inputRegExp.cap (3).toInt () == 1)
            input->setAttribute ("tuner", "1");
        m_tvdevice->appendChild (input);
        kDebug() << "Input " << input->mrl ()->pretty_name;
    } else
        return false;
    return true;
}

KDE_NO_EXPORT QString TVDeviceScannerSource::filterOptions () {
    return QString ("");
}

KDE_NO_EXPORT bool TVDeviceScannerSource::hasLength () {
    return false;
}

KDE_NO_EXPORT bool TVDeviceScannerSource::isSeekable () {
    return false;
}

KDE_NO_EXPORT bool TVDeviceScannerSource::scan (const QString & dev, const QString & dri) {
    if (m_tvdevice)
        return false;
    setUrl ("tv://");
    KMPlayer::NodePtr doc = m_tvsource->document ();
    m_tvdevice = new TVDevice (doc, dev);
    m_tvsource->document ()->appendChild (m_tvdevice);
    m_tvdevice->zombie = true; // not for real yet
    m_driver = dri;
    m_old_source = m_tvsource->player ()->source ();
    m_tvsource->player ()->setSource (this);
    m_identified = true;
    play ();
    return true;
}

KDE_NO_EXPORT void TVDeviceScannerSource::activate () {
    m_nameRegExp.setPattern ("Selected device:\\s*([^\\s].*)");
    m_sizesRegExp.setPattern ("Supported sizes:\\s*([0-9]+)x([0-9]+) => ([0-9]+)x([0-9]+)");
    m_inputRegExp.setPattern ("\\s*([0-9]+):\\s*([^:]+):[^\\(]*\\(tuner:([01]),\\s*norm:([^\\)]+)\\)");
}

KDE_NO_EXPORT void TVDeviceScannerSource::deactivate () {
    kDebug () << "TVDeviceScannerSource::deactivate";
    if (m_tvdevice) {
        if (m_tvdevice->parentNode ())
            m_tvdevice->parentNode ()->removeChild (m_tvdevice);
        m_tvdevice = 0L;
        emit scanFinished (m_tvdevice);
    }
}

KDE_NO_EXPORT void TVDeviceScannerSource::play () {
    if (!m_tvdevice)
        return;
    QString args;
    args.sprintf ("tv:// -tv driver=%s:device=%s -identify -frames 0", m_driver.ascii (), m_tvdevice->src.ascii ());
    m_tvsource->player ()->stop ();
    //m_tvsource->player ()->process ()->initProcess ();
    //KMPlayer::Process *proc = m_tvsource->player ()->players () ["mplayer"];
    //proc->setSource (this);
    //if (!static_cast <KMPlayer::MPlayer *> (proc)->run (args.ascii()))
    //    deactivate ();
}
/*
KDE_NO_EXPORT void TVDeviceScannerSource::stateChange (KMPlayer::Process * p, KMPlayer::Process::State os, KMPlayer::Process::State ns) {
    if (m_tvdevice &&  // can be deactivated
            ns == KMPlayer::Process::Ready && os > KMPlayer::Process::Ready) {
        TVDevice * dev = 0L;
        kDebug () << "scanning done " << m_tvdevice->hasChildNodes ();
        if (!m_tvdevice->hasChildNodes ())
            m_tvsource->document ()->removeChild (m_tvdevice);
        else
            dev = m_tvdevice;
        m_tvdevice = 0L;
        m_player->setSource (m_old_source);
        emit scanFinished (dev);
    }
    //KMPlayer::Source::stateChange (p, os, ns);
}*/

#include "kmplayertvsource.moc"
