/*
    This file is part of the KMPlayer application
    SPDX-FileCopyrightText: 2003 Koos Vriezen <koos.vriezen@xs4all.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QLayout>
#include <QLabel>
#include <QTimer>
#include <QPushButton>
#include <QCheckBox>
#include <QTableWidget>
#include <QStringList>
#include <QComboBox>
#include <QLineEdit>
#include <QGroupBox>
#include <QWhatsThis>
#include <QTabWidget>
#include <QMessageBox>
#include <QHeaderView>
#include <QMenu>
#include <QStandardPaths>
#include <QFontMetrics>

#include <KLocalizedString>
#include <KMessageBox>
#include <KLineEdit>
#include <KUrlRequester>
#include <KComboBox>
#include <KConfig>
#include <KConfigGroup>

#include "kmplayerapp_log.h"
#include "kmplayerpartbase.h"
#include "kmplayerprocess.h"
#include "kmplayerconfig.h"
#include "kmplayertvsource.h"
#include "playmodel.h"
#include "playlistview.h"
#include "viewarea.h"
#include "kmplayer.h"
#include "kmplayercontrolpanel.h"

static const char * strTV = "TV";
static const char * strTVDriver = "Driver";


TVDevicePage::TVDevicePage (QWidget *parent, KMPlayer::NodePtr dev)
: QFrame (parent), device_doc (dev) {
    setObjectName("PageTVDevice");
    TVDevice * device = KMPlayer::convertNode <TVDevice> (device_doc);
    QLabel* deviceLabel = new QLabel(i18n("Video device:") + device->src);
    QLabel* audioLabel = new QLabel(i18n("Audio device:"));
    audiodevice = new KUrlRequester (QUrl::fromUserInput (device->getAttribute ("audio")));
    QLabel* nameLabel = new QLabel(i18n("Name:"));
    name = new QLineEdit(device->title);
    QLabel *sizewidthLabel = new QLabel(i18n("Width:"));
    sizewidth = new QLineEdit(device->getAttribute(KMPlayer::Ids::attr_width));
    QLabel* sizeheightLabel = new QLabel (i18n ("Height:"));
    sizeheight = new QLineEdit(device->getAttribute(KMPlayer::Ids::attr_height));
    noplayback = new QCheckBox(i18n("Do not immediately play"));
    noplayback->setChecked (!device->getAttribute ("playback").toInt ());
    noplayback->setWhatsThis(i18n("Only start playing after clicking the play button"));
    inputsTab = new QTabWidget;
    for (KMPlayer::Node *ip = device->firstChild (); ip; ip = ip->nextSibling ()) {
        if (ip->id != id_node_tv_input)
            continue;
        TVInput * input = KMPlayer::convertNode <TVInput> (ip);
        QWidget* widget = new QWidget;
        QHBoxLayout* tablayout = new QHBoxLayout;
        if (!input->getAttribute ("tuner").isEmpty ()) {
            QHBoxLayout* horzlayout = new QHBoxLayout;
            QVBoxLayout* vertlayout = new QVBoxLayout;
            horzlayout->addWidget(new QLabel(i18n("Norm:")));
            QComboBox* norms = new QComboBox;
            norms->setObjectName("PageTVNorm");
            norms->addItem(QString("NTSC"));
            norms->addItem(QString("PAL"));
            norms->addItem(QString("SECAM"));
            norms->setCurrentIndex(norms->findText(input->getAttribute ("norm")));
            horzlayout->addWidget (norms);
            vertlayout->addLayout (horzlayout);
            vertlayout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
            QTableWidget* table = new QTableWidget(90, 2);
            table->setObjectName("PageTVChannels");
            table->setContentsMargins(0, 0, 0, 0);
            QFontMetrics metrics (table->font ());
            QStringList labels = QStringList() << i18n("Channel") << i18n("Frequency (MHz)");
            table->setHorizontalHeaderLabels(labels);
            int index = 0;
            int first_column_width = QFontMetrics(table->horizontalHeader()->font()).boundingRect(labels[0]).width() + 20;
            for (KMPlayer::Node *c=input->firstChild();c;c=c->nextSibling()) {
                if (c->id != id_node_tv_channel)
                    continue;
                int strwid = metrics.boundingRect (c->mrl ()->title).width ();
                if (strwid > first_column_width)
                    first_column_width = strwid + 4;
                table->setItem(index, 0, new QTableWidgetItem(c->mrl()->title));
                table->setItem(index++, 1, new QTableWidgetItem(KMPlayer::convertNode<TVChannel>(c)->getAttribute("frequency")));
            }
            table->setColumnWidth (0, first_column_width);
            table->horizontalHeader()->setStretchLastSection(true);
            tablayout->addWidget (table);
            tablayout->addLayout (vertlayout);
        }
        widget->setLayout(tablayout);
        inputsTab->addTab (widget, input->mrl ()->title);
    }
    QPushButton* delButton = new QPushButton(i18n("Delete"));
    connect (delButton, &QPushButton::clicked, this, &TVDevicePage::slotDelete);
    QGridLayout* gridlayout = new QGridLayout;
    gridlayout->addWidget (audioLabel, 0, 0);
    gridlayout->addWidget (audiodevice, 0, 0, 1, 3);
    gridlayout->addWidget (nameLabel, 1, 0);
    gridlayout->addWidget (name, 1, 1, 1, 3);
    gridlayout->addWidget (sizewidthLabel, 2, 0);
    gridlayout->addWidget (sizewidth, 2, 1);
    gridlayout->addWidget (sizeheightLabel, 2, 2);
    gridlayout->addWidget (sizeheight, 2, 3);
    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(deviceLabel);
    layout->addLayout(gridlayout);
    layout->addWidget (inputsTab);
    layout->addSpacing (5);
    layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum));
    QHBoxLayout *buttonlayout = new QHBoxLayout ();
    buttonlayout->addWidget (noplayback);
    buttonlayout->addItem (new QSpacerItem (0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
    buttonlayout->addWidget (delButton);
    layout->addLayout (buttonlayout);
    setLayout(layout);
}

void TVDevicePage::slotDelete () {
    if (KMessageBox::warningYesNo (this, i18n ("You are about to remove this device from the Source menu.\nContinue?"), i18n ("Confirm")) == KMessageBox::Yes)
        Q_EMIT deleted (this);
}

//-----------------------------------------------------------------------------

KMPlayerPrefSourcePageTV::KMPlayerPrefSourcePageTV (QWidget *parent, KMPlayerTVSource * tvsource)
: QFrame (parent), m_tvsource (tvsource) {
    notebook = new QTabWidget;
    notebook->setTabPosition (QTabWidget::South);
    QWidget * general = new QWidget (notebook);
    QLabel* driverLabel = new QLabel(i18n("Driver:"));
    driver = new QLineEdit;
    driver->setWhatsThis(i18n("dummy, v4l or bsdbt848"));
    QLabel *deviceLabel = new QLabel(i18n("Device:"));
    device = new KUrlRequester(QUrl::fromLocalFile("/dev/video"));
    device->setWhatsThis(i18n("Path to your video device, eg. /dev/video0"));
    scan = new QPushButton(i18n("Scan..."));
    QGridLayout *gridlayout = new QGridLayout;
    gridlayout->addWidget (driverLabel, 0, 0);
    gridlayout->addWidget (driver, 0, 1);
    gridlayout->addWidget (deviceLabel, 1, 0);
    gridlayout->addWidget (device, 1, 1);
    QHBoxLayout *buttonlayout = new QHBoxLayout;
    buttonlayout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum));
    buttonlayout->addWidget (scan);
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addLayout(gridlayout);
    layout->addLayout (buttonlayout);
    layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
    general->setLayout(layout);
    notebook->addTab(general, i18n("General"));
    QVBoxLayout* mainlayout = new QVBoxLayout;
    mainlayout->addWidget(notebook);
    setLayout(mainlayout);
}

void KMPlayerPrefSourcePageTV::showEvent (QShowEvent *) {
    m_tvsource->readXML ();
}

//-----------------------------------------------------------------------------

TVNode::TVNode (KMPlayer::NodePtr &d, const QString & s, const char * t, short id, const QString & n) : KMPlayer::GenericMrl (d, s, n, t) {
    this->id = id;
    editable = true;
}

void TVNode::setNodeName (const QString & nn) {
    title = nn;
    setAttribute (KMPlayer::Ids::attr_name, nn);
}

//-----------------------------------------------------------------------------

TVChannel::TVChannel (KMPlayer::NodePtr & d, const QString & n, double freq) : TVNode (d, QString ("tv://"), "channel", id_node_tv_channel, n) {
    setAttribute (KMPlayer::Ids::attr_name, n);
    setAttribute ("frequency", QString::number (freq, 'f', 2));
}

TVChannel::TVChannel (KMPlayer::NodePtr & d) : TVNode (d, QString ("tv://"), "channel", id_node_tv_channel) {
}

void TVChannel::closed () {
    title = getAttribute (KMPlayer::Ids::attr_name);
    Mrl::closed ();
}

//-----------------------------------------------------------------------------

TVInput::TVInput (KMPlayer::NodePtr & d, const QString & n, int id)
 : TVNode (d, QString ("tv://"), "input", id_node_tv_input, n) {
    setAttribute (KMPlayer::Ids::attr_name, n);
    setAttribute (KMPlayer::Ids::attr_id, QString::number (id));
}

TVInput::TVInput (KMPlayer::NodePtr & d) : TVNode (d, QString ("tv://"), "input", id_node_tv_input) {
}

KMPlayer::Node *TVInput::childFromTag (const QString & tag) {
    // qCDebug(LOG_KMPLAYER_APP) << nodeName () << " childFromTag " << tag;
    if (tag == QString::fromLatin1 ("channel")) {
        return new TVChannel (m_doc);
    } else
        return nullptr;
}

void TVInput::closed () {
    //title = getAttribute (KMPlayer::Ids::attr_name);
    Mrl::closed ();
}

void TVInput::setNodeName (const QString & name) {
    Node *p = parentNode ();
    QString nm (name);
    if (p && p->id == id_node_tv_device) {
        int pos = name.indexOf (QString (" - ") + p->mrl ()->title);
        if (pos > -1)
            nm.truncate (pos);
    }
    title = nm + QString (" - ") + title;
    TVNode::setNodeName (nm);
}

//-----------------------------------------------------------------------------

TVDevice::TVDevice (KMPlayer::NodePtr & doc, const QString & d) : TVNode (doc, d, "device", id_node_tv_device), zombie (false) {
    setAttribute ("path", d);
}

TVDevice::TVDevice (KMPlayer::NodePtr & doc)
    : TVNode (doc, i18n ("tv device"), "device", id_node_tv_device), zombie (false) {
}

TVDevice::~TVDevice () {
    if (device_page)
        device_page->deleteLater ();
}

KMPlayer::Node *TVDevice::childFromTag (const QString & tag) {
    // qCDebug(LOG_KMPLAYER_APP) << nodeName () << " childFromTag " << tag;
    if (tag == QString::fromLatin1 ("input"))
        return new TVInput (m_doc);
    return nullptr;
}

void TVDevice::closed () {
    updateNodeName ();
    Mrl::closed ();
}

void TVDevice::message (KMPlayer::MessageType msg, void *data) {
    if (KMPlayer::MsgChildFinished == msg)
        finish ();
    else
        TVNode::message (msg, data);
}

void *TVDevice::role (KMPlayer::RoleType msg, void *content)
{
    if (KMPlayer::RolePlaylist == msg)
        return nullptr;
    return TVNode::role (msg, content);
}

void TVDevice::setNodeName (const QString & name) {
    TVNode::setNodeName (name);
    updateNodeName ();
}

void TVDevice::updateNodeName () {
    title = getAttribute (KMPlayer::Ids::attr_name);
    src = getAttribute ("path");
    for (KMPlayer::Node *c = firstChild (); c; c = c->nextSibling ())
        if (c->id == id_node_tv_input) {
            TVInput * i = static_cast <TVInput *> (c);
            i->title = i->getAttribute (KMPlayer::Ids::attr_name) +
                QString (" - ") + title;
        }
}

void TVDevice::updateDevicePage () {
    if (!device_page)
        return;
    title = device_page->name->text ();
    setAttribute (KMPlayer::Ids::attr_name, title);
    setAttribute ("audio", device_page->audiodevice->lineEdit()->text ());
    setAttribute ("playback", device_page->noplayback->isChecked() ? "0" : "1");
    setAttribute (KMPlayer::Ids::attr_width, device_page->sizewidth->text ());
    setAttribute (KMPlayer::Ids::attr_height, device_page->sizeheight->text ());
    int i = 0;
    for (KMPlayer::Node *ip = firstChild(); ip; ip=ip->nextSibling(),++i) {
        if (ip->id != id_node_tv_input)
            continue;
        TVInput * input = KMPlayer::convertNode <TVInput> (ip);
        bool ok;
        if (input->getAttribute ("tuner").toInt (&ok) && ok) {
            QWidget* widget = device_page->inputsTab->widget(i);
            QTableWidget* table = static_cast<QTableWidget*>(widget->findChild<QTableWidget*>("PageTVChannels"));
            if (table) {
                input->clearChildren ();
                for (int j = 0; j<table->rowCount() && table->item (j, 1); ++j) {
                    input->appendChild (new TVChannel (m_doc, table->item (j, 0)->text (), table->item (j, 1)->text ().toDouble ()));
                }
            }
            QComboBox* norms = static_cast<QComboBox*>(widget->findChild<QComboBox*>("PageTVNorm"));
            if (norms) {
                input->setAttribute ("norm", norms->currentText ());
            }
        }
    }
}

//-----------------------------------------------------------------------------

TVDocument::TVDocument (KMPlayerTVSource * source)
    : FileDocument (id_node_tv_document, "tv://", source), m_source (source) {
    title = i18n ("Television");
    bookmarkable = false;
}

KMPlayer::Node *TVDocument::childFromTag (const QString & tag) {
    // qCDebug(LOG_KMPLAYER_APP) << nodeName () << " childFromTag " << tag;
    if (tag == QString::fromLatin1 ("device"))
        return new TVDevice (m_doc);
    return FileDocument::childFromTag (tag);
}

void TVDocument::message (KMPlayer::MessageType msg, void *data) {
    if (KMPlayer::MsgChildFinished == msg)
        finish ();
    else
        FileDocument::message (msg, data);
}

void TVDocument::defer () {
    if (!resolved) {
        resolved = true;
        readFromFile(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/kmplayer/tv.xml");
    }
}

//-----------------------------------------------------------------------------

KMPlayerTVSource::KMPlayerTVSource(KMPlayerApp* a)
    : KMPlayer::Source (i18n ("TV"), a->player(), "tvsource"), m_app(a), m_configpage(nullptr), scanner(nullptr), config_read(false) {
    m_url = QUrl("tv://");
    m_document = new TVDocument (this);
    m_player->settings ()->addPage (this);
    tree_id = m_player->playModel()->addTree (m_document, "tvsource", "video-television", KMPlayer::PlayModel::TreeEdit | KMPlayer::PlayModel::Moveable | KMPlayer::PlayModel::Deleteable);
}

KMPlayerTVSource::~KMPlayerTVSource () {
    static_cast <TVDocument *> (m_document.ptr ())->sync
        (QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/kmplayer/tv.xml");
}

void KMPlayerTVSource::activate () {
    m_identified = true;
    //if (m_player->settings ()->showbroadcastbutton)
    //    m_app->view()->controlPanel()->broadcastButton ()->show ();
    if (m_cur_tvdevice && !m_current) {
        for (KMPlayer::Node *i = m_cur_tvdevice->firstChild(); i && !m_current; i=i->nextSibling())
            if (i->id == id_node_tv_input) {
                TVInput * input = KMPlayer::convertNode <TVInput> (i);
                bool ok;
                m_cur_tvinput = i;
                if (input->getAttribute ("tuner").toInt (&ok) && ok) {
                    for (KMPlayer::Node *c = i->firstChild (); c; c = c->nextSibling ())
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
            QTimer::singleShot (0, m_player, &KMPlayer::PartBase::play);
    }
}
/* TODO: playback by
 * ffmpeg -vd /dev/video0 -r 25 -s 768x576 -f rawvideo - |mplayer -nocache -ao arts -rawvideo on:w=768:h=576:fps=25 -quiet -
 */

void KMPlayerTVSource::deactivate () {
    //if (m_player->view () && !m_app->view ()->controlPanel()->broadcastButton ()->isOn ())
    //    m_app->view ()->controlPanel()->broadcastButton ()->hide ();
    reset ();
}

void KMPlayerTVSource::play (KMPlayer::Mrl *mrl) {
    if (mrl && mrl->id == id_node_tv_document) {
        readXML ();
    } else {
        m_current = mrl;
        for (KMPlayer::Node *e = mrl; e; e = e->parentNode ()) {
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

KMPlayer::NodePtr KMPlayerTVSource::root () {
    return m_cur_tvinput;
}

void KMPlayerTVSource::setCurrent (KMPlayer::Mrl *mrl) {
    TVChannel * channel = nullptr;
    TVInput * input = nullptr;
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
    m_player->playModel()->updateTree(0, m_cur_tvinput, m_current, true, false);
    if (m_cur_tvdevice->id != id_node_tv_device) {
        return;
    }
    TVDevice * tvdevice = KMPlayer::convertNode <TVDevice> (m_cur_tvdevice);
    m_identified = true;
    m_audiodevice = tvdevice->getAttribute ("audio");
    m_videodevice = tvdevice->src;
    m_videonorm = input->getAttribute ("norm");
    m_tuner = input->getAttribute (KMPlayer::Ids::attr_name);
    QString xvport = tvdevice->getAttribute ("xvport");
    if (!xvport.isEmpty ())
        m_xvport = xvport.toInt ();
    QString xvenc = input->getAttribute ("xvenc");
    if (!xvenc.isEmpty ())
        m_xvencoding = xvenc.toInt ();
    QString command;
    command.sprintf ("device=%s:input=%s",
            tvdevice->src.toLatin1 ().data (),
            input->getAttribute (KMPlayer::Ids::attr_id).toLatin1 ().data ());
    if (channel) {
        QString freq = channel->getAttribute ("frequency");
        m_frequency = (int)(1000 * freq.toDouble ());
        command += QString (":freq=%1").arg (freq);
    } else
        m_frequency = 0;
    if (!m_videonorm.isEmpty ())
        command += QString (":norm=%1").arg (m_videonorm);
    m_app->setCaption (i18n ("TV: ") + (channel ? channel->mrl ()->title : input->mrl ()->title), false);
    setDimensions (m_cur_tvdevice,
            tvdevice->getAttribute (KMPlayer::Ids::attr_width).toInt (),
            tvdevice->getAttribute (KMPlayer::Ids::attr_height).toInt ());
    m_options.sprintf ("-tv noaudio:driver=%s:%s:width=%d:height=%d -slave -nocache -quiet", tvdriver.toLatin1 ().data (), command.toLatin1 ().data (), width (), height ());
    m_recordcmd.sprintf ("-tv %s:driver=%s:%s:width=%d:height=%d", m_audiodevice.isEmpty () ? "noaudio" : QString(QLatin1String ("forceaudio:adevice=") + m_audiodevice).toLatin1 ().data(), tvdriver.toLatin1 ().data (), command.toLatin1 ().data (), width (), height ());
}

void KMPlayerTVSource::menuClicked (int id) {
    KMPlayer::Node *elm = m_document->firstChild ();
    for (; id > 0; --id,  elm = elm->nextSibling ())
        ;
    m_cur_tvdevice = elm;
    m_cur_tvinput = elm->firstChild (); // FIXME
    m_current = nullptr;
    m_player->setSource (this);
}

QString KMPlayerTVSource::filterOptions () {
    if (! m_player->settings ()->disableppauto)
        return KMPlayer::Source::filterOptions ();
    return QString ("-vf pp=lb");
}

bool KMPlayerTVSource::hasLength () {
    return false;
}

bool KMPlayerTVSource::isSeekable () {
    return true;
}

QString KMPlayerTVSource::prettyName () {
    QString name (i18n ("TV"));
    //if (m_tvsource)
    //    name += ' ' + m_tvsource->title;
    return name;
}

void KMPlayerTVSource::write (KSharedConfigPtr m_config) {
    if (!config_read) return;
    KConfigGroup (m_config, strTV).writeEntry (strTVDriver, tvdriver);
    static_cast <TVDocument *> (m_document.ptr ())->writeToFile
        (QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/kmplayer/tv.xml");
    qCDebug(LOG_KMPLAYER_APP) << "KMPlayerTVSource::write XML";
}

void KMPlayerTVSource::readXML () {
    if (config_read) return;
    config_read = true;
    qCDebug(LOG_KMPLAYER_APP) << "KMPlayerTVSource::readXML";
    m_document->defer ();
    m_player->playModel()->updateTree (tree_id, m_document, nullptr, false, false);
    sync (false);
}

void KMPlayerTVSource::read (KSharedConfigPtr m_config) {
    tvdriver = KConfigGroup (m_config, strTV).readEntry (
            strTVDriver, QString ("v4l2"));
}

void KMPlayerTVSource::sync (bool fromUI) {
    if (!m_configpage) return;
    if (m_document && m_document->hasChildNodes ())
        m_app->showBroadcastConfig ();
    else
        m_app->hideBroadcastConfig ();
    if (fromUI) {
        tvdriver = m_configpage->driver->text ();
        for (KMPlayer::Node *d=m_document->firstChild();d; d=d->nextSibling())
            if (d->id == id_node_tv_device)
                static_cast <TVDevice *> (d)->updateDevicePage ();
        m_player->playModel()->updateTree(tree_id, m_document, nullptr, false, false);
    } else {
        m_configpage->driver->setText (tvdriver);
        for (KMPlayer::Node *dp = m_document->firstChild (); dp; dp = dp->nextSibling ())
            if (dp->id == id_node_tv_device)
                addTVDevicePage (KMPlayer::convertNode <TVDevice> (dp));
    }
}

void KMPlayerTVSource::prefLocation (QString & item, QString & icon, QString & tab) {
    item = i18n ("Source");
    icon = QString ("source");
    tab = i18n ("TV");
}

QFrame * KMPlayerTVSource::prefPage (QWidget * parent) {
    if (!m_configpage) {
        m_configpage = new KMPlayerPrefSourcePageTV (parent, this);
        scanner = new TVDeviceScannerSource (this);
        connect (m_configpage->scan, &QPushButton::clicked, this, &KMPlayerTVSource::slotScan);
    }
    return m_configpage;
}

static bool hasTVDevice (KMPlayer::NodePtr doc, const QString & devstr) {
    for (KMPlayer::Node *e = doc->firstChild (); e; e = e->nextSibling ())
        if (e->id == id_node_tv_device &&
                static_cast <TVDevice *> (e)->src == devstr)
            return true;
    return false;
}

void KMPlayerTVSource::slotScan () {
    QString devstr = m_configpage->device->lineEdit()->text ();
    if (!hasTVDevice(m_document, devstr)) {
        scanner->scan (devstr, m_configpage->driver->text());
        connect (scanner, &TVDeviceScannerSource::scanFinished,
                this, &KMPlayerTVSource::slotScanFinished);
    } else
        KMessageBox::error (m_configpage, i18n ("Device already present."),
                i18n ("Error"));
}

void KMPlayerTVSource::slotScanFinished (TVDevice * tvdevice) {
    disconnect (scanner, &TVDeviceScannerSource::scanFinished,
                this, &KMPlayerTVSource::slotScanFinished);
    if (tvdevice) {
        tvdevice->zombie = false;
        addTVDevicePage (tvdevice, true);
        m_player->playModel()->updateTree(tree_id, m_document, nullptr, false, false);
    } else
        KMessageBox::error(m_configpage,i18n("No device found."),i18n("Error"));
}

void KMPlayerTVSource::addTVDevicePage(TVDevice *dev, bool show) {
    if (dev->device_page)
        dev->device_page->deleteLater ();
    dev->device_page = new TVDevicePage (m_configpage->notebook, dev);
    m_configpage->notebook->addTab(dev->device_page, dev->title);
    connect (dev->device_page, &TVDevicePage::deleted,
             this, &KMPlayerTVSource::slotDeviceDeleted);
    if (show)
        m_configpage->notebook->setCurrentIndex(m_configpage->notebook->count()-1);
}

void KMPlayerTVSource::slotDeviceDeleted (TVDevicePage *devpage) {
    m_document->removeChild (devpage->device_doc);
    m_configpage->notebook->setCurrentIndex(0);
    m_player->playModel()->updateTree (tree_id, m_document, nullptr, false, false);
}

//-----------------------------------------------------------------------------

TVDeviceScannerSource::TVDeviceScannerSource (KMPlayerTVSource * src)
 : KMPlayer::Source (i18n ("TVScanner"), src->player (), "tvscanner"),
   m_tvsource (src),
   m_tvdevice (nullptr),
   m_process (nullptr),
   m_viewer (nullptr) {
}

void TVDeviceScannerSource::init () {
}

bool TVDeviceScannerSource::processOutput (const QString & line) {
    if (m_nameRegExp.indexIn(line) > -1) {
        m_tvdevice->title = m_nameRegExp.cap (1);
        m_tvdevice->setAttribute(KMPlayer::Ids::attr_name,m_tvdevice->title);
        qCDebug(LOG_KMPLAYER_APP) << "Name " << m_tvdevice->title;
    } else if (m_sizesRegExp.indexIn(line) > -1) {
        m_tvdevice->setAttribute (KMPlayer::Ids::attr_width,
                m_sizesRegExp.cap(1));
        m_tvdevice->setAttribute (KMPlayer::Ids::attr_height,
                m_sizesRegExp.cap(2));
        m_tvdevice->setAttribute ("minwidth", m_sizesRegExp.cap (1));
        m_tvdevice->setAttribute ("minheight", m_sizesRegExp.cap (2));
        m_tvdevice->setAttribute ("maxwidth", m_sizesRegExp.cap (3));
        m_tvdevice->setAttribute ("maxheight", m_sizesRegExp.cap (4));
    } else if (m_inputRegExp.indexIn(line) > -1) {
        KMPlayer::NodePtr doc = m_tvsource->document ();
        TVInput * input = new TVInput (doc, m_inputRegExp.cap(2).trimmed(),
                                       m_inputRegExp.cap (1).toInt ());
        if (m_inputRegExp.cap (3).toInt () == 1)
            input->setAttribute ("tuner", "1");
        m_tvdevice->appendChild (input);
        qCDebug(LOG_KMPLAYER_APP) << "Input " << input->mrl ()->title;
    } else if (m_inputRegExpV4l2.indexIn(line) > -1) {
        KMPlayer::NodePtr doc = m_tvsource->document ();
        QStringList sl = m_inputRegExpV4l2.cap(1).split (QChar (';'));
        const QStringList::iterator e = sl.end ();
        for (QStringList::iterator i = sl.begin (); i != e; ++i) {
            int pos = (*i).indexOf (QChar ('='));
            if (pos > 0) {
                int id = (*i).left (pos).trimmed ().toInt ();
                TVInput *input = new TVInput(doc,(*i).mid(pos+1).trimmed(), id);
                if (!id && m_caps.indexOf ("tuner") > -1)
                    input->setAttribute ("tuner", "1");
                m_tvdevice->appendChild (input);
            }
        }
    } else {
        int pos = line.indexOf ("Capabilites:");
        if (pos > 0)
            m_caps = line.mid (pos + 12);
        return false;
    }
    return true;
}

QString TVDeviceScannerSource::filterOptions () {
    return QString ("");
}

bool TVDeviceScannerSource::hasLength () {
    return false;
}

bool TVDeviceScannerSource::isSeekable () {
    return false;
}

bool TVDeviceScannerSource::scan (const QString & dev, const QString & dri) {
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
    play (m_tvdevice);
    return true;
}

void TVDeviceScannerSource::activate () {
    m_nameRegExp.setPattern ("Selected device:\\s*([^\\s].*)");
    m_sizesRegExp.setPattern ("Supported sizes:\\s*([0-9]+)x([0-9]+) => ([0-9]+)x([0-9]+)");
    m_inputRegExp.setPattern ("\\s*([0-9]+):\\s*([^:]+):[^\\(]*\\(tuner:([01]),\\s*norm:([^\\)]+)\\)");
    m_inputRegExpV4l2.setPattern ("inputs:((?:\\s*[0-9]+\\s*=\\s*[^;]+;)+)");
}

void TVDeviceScannerSource::deactivate () {
    qCDebug(LOG_KMPLAYER_APP) << "TVDeviceScannerSource::deactivate";
    if (m_tvdevice) {
        if (m_tvdevice->parentNode ())
            m_tvdevice->parentNode ()->removeChild (m_tvdevice);
        m_tvdevice = nullptr;
        delete m_process;
        Q_EMIT scanFinished (m_tvdevice);
    }
}

void TVDeviceScannerSource::play (KMPlayer::Mrl *) {
    if (!m_tvdevice)
        return;
    m_options.sprintf ("tv:// -tv driver=%s:device=%s -identify -frames 0", m_driver.toLatin1 ().data (), m_tvdevice->src.toLatin1 ().data ());
    m_tvsource->player ()->stop ();
    KMPlayer::Node *n = new KMPlayer::SourceDocument (this, QString ());
    setDocument (n, n);
    m_process = m_player->mediaManager()->processInfos()["mplayer"]->create (m_player, this);
    m_viewer = m_player->viewWidget ()->viewArea ()->createVideoWidget ();
    m_process->ready ();
}

void TVDeviceScannerSource::scanningFinished () {
    TVDevice * dev = nullptr;
    delete m_process;
    qCDebug(LOG_KMPLAYER_APP) << "scanning done " << m_tvdevice->hasChildNodes ();
    if (!m_tvdevice->hasChildNodes ()) {
        m_tvsource->document ()->removeChild (m_tvdevice);
    } else {
        dev = m_tvdevice;
        if (width () > 0 && height () > 0) {
            m_tvdevice->setAttribute (KMPlayer::Ids::attr_width,
                    QString::number (width ()));
            m_tvdevice->setAttribute (KMPlayer::Ids::attr_height,
                    QString::number (height ()));
        }
    }
    m_tvdevice = nullptr;
    m_player->setSource (m_old_source);
    Q_EMIT scanFinished (dev);
}

void TVDeviceScannerSource::stateChange (KMPlayer::IProcess *,
                   KMPlayer::IProcess::State os, KMPlayer::IProcess::State ns) {
    if (KMPlayer::IProcess::Ready == ns) {
        if (os > KMPlayer::IProcess::Ready)
            QTimer::singleShot (0, this, &TVDeviceScannerSource::scanningFinished);
        else if (m_process && os < KMPlayer::IProcess::Ready)
            m_process->play ();
    }
}

void TVDeviceScannerSource::processDestroyed (KMPlayer::IProcess *) {
    m_process = nullptr;
    KMPlayer::View *view = m_player->viewWidget ();
    if (view)
        view->viewArea ()->destroyVideoWidget (m_viewer);
    m_viewer = nullptr;
}

KMPlayer::IViewer *TVDeviceScannerSource::viewer () {
    return m_viewer;
}

KMPlayer::Mrl *TVDeviceScannerSource::getMrl () {
    return document ()->mrl ();
}
