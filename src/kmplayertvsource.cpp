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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <algorithm>

#include <qlayout.h>
#include <qlabel.h>
#include <qtimer.h>
#include <qpushbutton.h>
#include <qcheckbox.h>
#include <qtable.h>
#include <qstringlist.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <qgroupbox.h>
#include <qwhatsthis.h>
#include <qtabwidget.h>
#include <qbuttongroup.h>
#include <qmessagebox.h>
#include <qpopupmenu.h>
#include <qfontmetrics.h>

#include <klocale.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <klineedit.h>
#include <kurlrequester.h>
#include <kcombobox.h>
#include <kconfig.h>

#include "kmplayerpartbase.h"
#include "kmplayerprocess.h"
#include "kmplayerconfig.h"
#include "kmplayertvsource.h"
#include "kmplayer.h"
#include "kmplayerbroadcast.h"

static const char * strTV = "TV";
static const char * strTVDevices = "Devices";
static const char * strTVDeviceName = "Name";
static const char * strTVAudioDevice = "Audio Device";
static const char * strTVInputs = "Inputs";
static const char * strTVSize = "Size";
static const char * strTVMinSize = "Minimum Size";
static const char * strTVMaxSize = "Maximum Size";
static const char * strTVNoPlayback = "No Playback";
static const char * strTVNorm = "Norm";
static const char * strTVDriver = "Driver";

KDE_NO_CDTOR_EXPORT KMPlayerPrefSourcePageTVDevice::KMPlayerPrefSourcePageTVDevice (QWidget *parent, KMPlayer::ElementPtr dev)
: QFrame (parent, "PageTVDevice"), device_doc (dev) {
    TVDevice * device = KMPlayer::convertNode <TVDevice> (device_doc);
    QVBoxLayout *layout = new QVBoxLayout (this, 5, 2);
    QLabel * deviceLabel = new QLabel (i18n ("Video device:") + device->src, this, 0);
    layout->addWidget (deviceLabel);
    QGridLayout *gridlayout = new QGridLayout (layout, 3, 4);
    QLabel * audioLabel = new QLabel (i18n ("Audio device:"), this);
    audiodevice = new KURLRequester (device->audiodevice, this);
    QLabel * nameLabel = new QLabel (i18n ("Name:"), this, 0);
    name = new QLineEdit (device->pretty_name, this, 0);
    QLabel *sizewidthLabel = new QLabel (i18n ("Width:"), this, 0);
    sizewidth = new QLineEdit (QString::number (device->size.width()), this, 0);
    QLabel *sizeheightLabel = new QLabel (i18n ("Height:"), this, 0);
    sizeheight = new QLineEdit(QString::number(device->size.height()), this, 0);
    noplayback = new QCheckBox (i18n ("Do not immediately play"), this);
    noplayback->setChecked (device->noplayback);
    QWhatsThis::add (noplayback, i18n ("Only start playing after clicking the play button"));
    inputsTab = new QTabWidget (this);
    for (KMPlayer::ElementPtr ip = device->firstChild (); ip; ip = ip->nextSibling ()) {
        TVInput * input = KMPlayer::convertNode <TVInput> (ip);
        QWidget * widget = new QWidget (this);
        QHBoxLayout *tablayout = new QHBoxLayout (widget, 5, 2);
        if (input->hastuner) {
            QHBoxLayout *horzlayout = new QHBoxLayout ();
            QVBoxLayout *vertlayout = new QVBoxLayout ();
            horzlayout->addWidget (new QLabel (i18n ("Norm:"), widget));
            QComboBox * norms = new QComboBox (widget, "PageTVNorm");
            norms->insertItem (QString ("NTSC"), 0);
            norms->insertItem (QString ("PAL"), 1);
            norms->insertItem (QString ("SECAM"), 2);
            norms->setCurrentText (input->norm);
            horzlayout->addWidget (norms);
            vertlayout->addLayout (horzlayout);
            vertlayout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
            QTable * table = new QTable (90, 2, widget, "PageTVChannels");
            QFontMetrics metrics (table->font ());
            QHeader *header = table->horizontalHeader();
            header->setLabel (0, i18n ("Channel"));
            header->setLabel (1, i18n ("Frequency"));
            int index = 0;
            int first_column_width = QFontMetrics (header->font ()).boundingRect (header->label (0)).width () + 20;
            for (KMPlayer::ElementPtr cp=input->firstChild(); cp; cp=cp->nextSibling()) {
                TVChannel * c = KMPlayer::convertNode <TVChannel> (cp);
                int strwid = metrics.boundingRect (c->name).width ();
                if (strwid > first_column_width)
                    first_column_width = strwid + 4;
                table->setItem (index, 0, new QTableItem (table, QTableItem::Always, c->name));
                table->setItem (index++, 1, new QTableItem (table, QTableItem::Always, QString::number (c->frequency)));
            }
            table->setColumnWidth (0, first_column_width);
            table->setColumnStretchable (1, true);
            tablayout->addWidget (table);
            tablayout->addLayout (vertlayout);
        }
        inputsTab->addTab (widget, input->name);
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

KDE_NO_EXPORT void KMPlayerPrefSourcePageTVDevice::slotDelete () {
    if (KMessageBox::warningYesNo (this, i18n ("You're about to remove this device from the Source menu.\nContinue?"), i18n ("Confirm")) == KMessageBox::Yes)
        emit deleted (this);
}

KDE_NO_EXPORT void KMPlayerPrefSourcePageTVDevice::updateTVDevice () {
    TVDevice * device = KMPlayer::convertNode <TVDevice> (device_doc);
    device->pretty_name = name->text ();
    device->audiodevice = audiodevice->lineEdit()->text ();
    device->noplayback = noplayback->isChecked ();
    device->size = QSize(sizewidth->text().toInt(), sizeheight->text().toInt());
    int i = 0;
    for (KMPlayer::ElementPtr ip = device->firstChild(); ip; ip=ip->nextSibling(),++i) {
        TVInput * input = KMPlayer::convertNode <TVInput> (ip);
        if (input->hastuner) {
            QWidget * widget = inputsTab->page (i);
            QTable * table = static_cast <QTable *> (widget->child ("PageTVChannels", "QTable"));
            if (table) {
                input->clear ();
                for (int j = 0; j < table->numRows() && table->item (j, 1); ++j)
                    input->appendChild ((new TVChannel (device->document ()->self (), table->item (j, 0)->text (), table->item (j, 1)->text ().toInt ()))->self());
            }
            QComboBox * norms = static_cast <QComboBox *> (widget->child ("PageTVNorm", "QComboBox"));
            if (norms) {
                input->norm = norms->currentText ();
            }
        }
    }
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT KMPlayerPrefSourcePageTV::KMPlayerPrefSourcePageTV (QWidget *parent)
: QFrame (parent) {
    QVBoxLayout * mainlayout = new QVBoxLayout (this, 5);
    tab = new QTabWidget (this);
    tab->setTabPosition (QTabWidget::Bottom);
    mainlayout->addWidget (tab);
    QWidget * general = new QWidget (tab);
    QVBoxLayout *layout = new QVBoxLayout (general);
    QGridLayout *gridlayout = new QGridLayout (layout, 2, 2, 2);
    QLabel *driverLabel = new QLabel (i18n ("Driver:"), general, 0);
    driver = new QLineEdit ("", general, 0);
    QWhatsThis::add (driver, i18n ("dummy, v4l or bsdbt848"));
    QLabel *deviceLabel = new QLabel (i18n ("Device:"), general, 0);
    device = new KURLRequester ("/dev/video", general);
    QWhatsThis::add(device, i18n("Path to your video device, eg. /dev/video0"));
    QPushButton * scan = new QPushButton (i18n ("Scan..."), general);
    connect (scan, SIGNAL (clicked ()), this, SLOT (slotScan ()));
    gridlayout->addWidget (driverLabel, 0, 0);
    gridlayout->addWidget (driver, 0, 1);
    gridlayout->addWidget (deviceLabel, 1, 0);
    gridlayout->addWidget (device, 1, 1);
    QHBoxLayout *buttonlayout = new QHBoxLayout ();
    buttonlayout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum));
    buttonlayout->addWidget (scan);
    layout->addLayout (buttonlayout);
    layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
    tab->insertTab (general, i18n ("General"));
}

KDE_NO_EXPORT void KMPlayerPrefSourcePageTV::addTVDevicePage (TVDevice * dev, bool show) {
    KMPlayerPrefSourcePageTVDevice * devpage = new KMPlayerPrefSourcePageTVDevice (tab, dev->self ());
    tab->insertTab (devpage, dev->pretty_name);
    connect (devpage, SIGNAL (deleted (KMPlayerPrefSourcePageTVDevice *)),
             this, SLOT (slotDeviceDeleted (KMPlayerPrefSourcePageTVDevice *)));
    m_devicepages.push_back (devpage);
    if (show)
        tab->setCurrentPage (tab->indexOf (devpage));
}

KDE_NO_EXPORT void KMPlayerPrefSourcePageTV::setTVDocument (KMPlayer::ElementPtr doc) {
    m_document = doc;
    deleteddevices = (new KMPlayer::GenericURL (doc))->self ();
    addeddevices = (new KMPlayer::GenericURL (doc))->self ();
    std::for_each (m_devicepages.begin(), m_devicepages.end(), KMPlayer::Deleter<QFrame>);
    m_devicepages.clear ();
    for (KMPlayer::ElementPtr dp = m_document->firstChild (); dp; dp = dp->nextSibling ())
        addTVDevicePage (KMPlayer::convertNode <TVDevice> (dp));
}

KDE_NO_EXPORT void KMPlayerPrefSourcePageTV::slotDeviceDeleted (KMPlayerPrefSourcePageTVDevice * devpage) {
    if (devpage->device_doc->parentNode () == addeddevices)
        addeddevices->removeChild (devpage->device_doc);
    else
        m_document->removeChild (devpage->device_doc);
    deleteddevices->appendChild (devpage->device_doc);
    m_devicepages.remove (devpage);
    devpage->deleteLater ();
    tab->setCurrentPage (0);
}

KDE_NO_EXPORT void KMPlayerPrefSourcePageTV::slotScan () {
    for (KMPlayer::ElementPtr e=m_document->firstChild();e;e= e->nextSibling())
        if (KMPlayer::convertNode <TVDevice> (e)->src == device->lineEdit()->text ()) {
            KMessageBox::error (this, i18n ("Device already present."),
                    i18n ("Error"));
            return;
        }
    scanner->scan (device->lineEdit()->text (), driver->text());
    connect (scanner, SIGNAL (scanFinished (TVDevice *)),
             this, SLOT (slotScanFinished (TVDevice *)));
}

KDE_NO_EXPORT void KMPlayerPrefSourcePageTV::slotScanFinished (TVDevice * _device) {
    disconnect (scanner, SIGNAL (scanFinished (TVDevice *)),
                this, SLOT (slotScanFinished (TVDevice *)));
    if (!_device) {
        KMessageBox::error (this, i18n ("No device found."), i18n ("Error"));
    } else {
        addeddevices->appendChild (_device->self ());
        addTVDevicePage (_device, true);
    }
}

KDE_NO_EXPORT void KMPlayerPrefSourcePageTV::updateTVDevices () {
    TVDevicePageList::iterator pit = m_devicepages.begin ();
    for (; pit != m_devicepages.end (); ++pit)
            (*pit)->updateTVDevice ();
    // remove deleted devices
    deleteddevices->clear ();
    // move added devices to device list
    for (KMPlayer::ElementPtr dp=addeddevices->firstChild(); dp; dp=addeddevices->firstChild()) {
        addeddevices->removeChild (dp);
        m_document->appendChild (dp);
    }
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT TVChannel::TVChannel (KMPlayer::ElementPtr d, const QString & n, int f) : KMPlayer::GenericURL (d, QString ("tv://"), n), name (n), frequency (f) {}

KDE_NO_CDTOR_EXPORT TVInput::TVInput (KMPlayer::ElementPtr d, const QString & n, int _id) : KMPlayer::GenericURL (d, QString ("tv://"), n), name (n), id (_id) {}

KDE_NO_CDTOR_EXPORT TVDevice::TVDevice (KMPlayer::ElementPtr doc, const QString & d, const QSize & s)
    : KMPlayer::GenericURL (doc, d), size (s), noplayback (false) {
}

//-----------------------------------------------------------------------------
/*
 * [TV]
 * Devices=/dev/video0;/dev/video1
 * Driver=v4l
 *
 * [/dev/video0]
 * Inputs=0:Television;1:Composite1;2:S-Video;3:Composite3
 * Size=768,576
 * Television=Ned1:216;Ned2:184;Ned3:192
 *
 * [/dev/video1]
 * Inputs=0:Webcam
 * Size=640,480
 */

KDE_NO_CDTOR_EXPORT KMPlayerTVSource::KMPlayerTVSource (KMPlayerApp * a, QPopupMenu * m)
    : KMPlayerMenuSource (i18n ("TV"), a, m, "tvsource"), m_configpage (0L) {
    m_menu->insertTearOffHandle ();
    setURL (KURL ("tv://"));
    m_player->settings ()->addPage (this);
}

KDE_NO_CDTOR_EXPORT KMPlayerTVSource::~KMPlayerTVSource () {
}

KDE_NO_EXPORT void KMPlayerTVSource::activate () {
    m_identified = true;
    buildArguments ();
    if (m_player->settings ()->showbroadcastbutton)
        m_app->view()->buttonBar()->broadcastButton ()->show ();
    KMPlayer::Source::next ();
    if (m_current)
        jump (m_current);
}
/* TODO: playback by
 * ffmpeg -vd /dev/video0 -r 25 -s 768x576 -f rawvideo - |mplayer -nocache -ao arts -rawvideo on:w=768:h=576:fps=25 -quiet -
 */
KDE_NO_EXPORT void KMPlayerTVSource::buildArguments () {
    if (!m_current)
        return;
    m_identified = true;
    TVChannel * channel = 0L;
    TVInput * input = 0L;
    TVDevice * tvdevice =KMPlayer::convertNode <TVDevice> (m_cur_tvdevice);
    KMPlayer::ElementPtr elm = m_current;
    if (!strcmp (elm->nodeName (), "tvchannel")) {
        channel = KMPlayer::convertNode <TVChannel> (elm);
        elm = elm->parentNode ();
    }
    if (elm && elm->nodeName (), "tvinput")
        input = KMPlayer::convertNode <TVInput> (elm);
    m_audiodevice = tvdevice->audiodevice;
    m_videodevice = tvdevice->src;
    m_videonorm = input->norm;
    m_frequency = channel ? channel->frequency : 0;
    QString command;
    command.sprintf ("device=%s:input=%d", tvdevice->src.ascii (), input->id);
    if (channel)
        command += QString (":freq=%1").arg (channel->frequency);
    if (!input->norm.isEmpty ())
        command += QString (":norm=%1").arg (input->norm);
    m_app->setCaption (QString (i18n ("TV: ")) + (channel ? channel->name : input->name), false);
    setWidth (tvdevice->size.width ());
    setHeight (tvdevice->size.height ());
    m_options.sprintf ("-tv noaudio:driver=%s:%s:width=%d:height=%d -slave -nocache -quiet", tvdriver.ascii (), command.ascii (), width (), height ());
    if (m_player->settings ()->mplayerpost090)
        m_recordcmd.sprintf ("-tv %s:driver=%s:%s:width=%d:height=%d", m_audiodevice.isEmpty () ? "noaudio" : (QString ("forceaudio:adevice=") + m_audiodevice).ascii(), tvdriver.ascii (), command.ascii (), width (), height ());
    else
        m_recordcmd.sprintf ("-tv on:%s:driver=%s:%s:width=%d:height=%d", m_audiodevice.isEmpty () ? "noaudio" : (QString ("forceaudio:adevice=") + m_audiodevice).ascii(), tvdriver.ascii (), command.ascii (), width (), height ());
    if (!m_app->broadcasting ())
        m_app->resizePlayer (100);
}

KDE_NO_EXPORT void KMPlayerTVSource::deactivate () {
    if (m_player->view () && !m_app->view ()->buttonBar()->broadcastButton ()->isOn ())
        m_app->view ()->buttonBar()->broadcastButton ()->hide ();
}

KDE_NO_EXPORT void KMPlayerTVSource::buildMenu () {
    m_menu->clear ();
    int counter = 0;
    for (KMPlayer::ElementPtr dp = m_document->firstChild (); dp; dp = dp->nextSibling ())
         m_menu->insertItem (KMPlayer::convertNode <TVDevice> (dp)->pretty_name, this, SLOT (menuClicked (int)), 0, counter++);
}

KDE_NO_EXPORT void KMPlayerTVSource::jump (KMPlayer::ElementPtr e) {
    QString new_dev, old_dev;
    if (m_cur_tvdevice)
        old_dev = KMPlayer::convertNode <TVDevice> (m_cur_tvdevice)->src;
    m_current = e;
    KMPlayer::ElementPtr elm = e;
    while (elm && strcmp (elm->nodeName (), "tvdevice"))
        elm = elm->parentNode ();
    m_cur_tvdevice = elm;
    if (elm)
        new_dev = KMPlayer::convertNode <TVDevice> (elm)->src;
    bool playing = (old_dev == new_dev) && m_player->playing ();
    buildArguments ();
    m_player->updateTree (m_cur_tvdevice, m_current);
    if (m_app->broadcasting ())
        QTimer::singleShot (0, m_app->broadcastConfig (), SLOT (startFeed ()));
    else {
        m_player->stop ();
        if (!KMPlayer::convertNode <TVDevice> (m_cur_tvdevice)->noplayback || playing)
            QTimer::singleShot (0, m_player, SLOT (play ()));
    }
}

KDE_NO_EXPORT void KMPlayerTVSource::menuClicked (int id) {
    KMPlayer::ElementPtr elm = m_document->firstChild ();
    for (; id > 0; --id,  elm = elm->nextSibling ())
        ;
    m_cur_tvdevice = elm;
    m_current = 0L;
    if (m_player->process ()->source () != this) {
        m_player->setSource (this);
    }
}

KDE_NO_EXPORT QString KMPlayerTVSource::filterOptions () {
    if (! m_player->settings ()->disableppauto)
        return KMPlayer::Source::filterOptions ();
    return QString ("-vop pp=lb");
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

KDE_NO_EXPORT void KMPlayerTVSource::write (KConfig * m_config) {
    //TV stuff
    m_config->setGroup (strTV);
    QStringList devicelist = m_config->readListEntry (strTVDevices, ';');
    for (unsigned i = 0; i < devicelist.size (); i++)
        m_config->deleteGroup (*devicelist.at (i));
    devicelist.clear ();
    if (m_configpage)
        m_configpage->updateTVDevices ();
    QString sep = QString (":");
    for (KMPlayer::ElementPtr dp = m_document->firstChild (); dp; dp = dp->nextSibling ()) {
        TVDevice * device = KMPlayer::convertNode <TVDevice> (dp);
        kdDebug() << " writing " << device->src << endl;
        devicelist.append (device->src);
        m_config->setGroup (device->src);
        m_config->writeEntry (strTVSize, device->size);
        m_config->writeEntry (strTVMinSize, device->minsize);
        m_config->writeEntry (strTVMaxSize, device->maxsize);
        m_config->writeEntry (strTVNoPlayback, device->noplayback);
        m_config->writeEntry (strTVDeviceName, device->pretty_name);
        m_config->writeEntry (strTVAudioDevice, device->audiodevice);
        QStringList inputlist;
        for (KMPlayer::ElementPtr ip = dp->firstChild (); ip; ip = ip->nextSibling ()) {
            TVInput * input = KMPlayer::convertNode <TVInput> (ip);
            inputlist.append (QString::number (input->id) + sep + input->name);
            if (input->hastuner) {
                QStringList channellist;
                for (KMPlayer::ElementPtr cp = ip->firstChild (); cp; cp = cp->nextSibling ()) {
                    TVChannel * c = KMPlayer::convertNode <TVChannel> (cp);
                    channellist.append (c->name + sep + QString::number (c->frequency));
                }
                if (!channellist.size ())
                    channellist.append (QString ("none"));
                m_config->writeEntry (input->name, channellist, ';');
                m_config->writeEntry (strTVNorm, input->norm);
            }
        }
        m_config->writeEntry (strTVInputs, inputlist, ';');
    }
    m_config->setGroup (strTV);
    m_config->writeEntry (strTVDevices, devicelist, ';');
    m_config->writeEntry (strTVDriver, tvdriver);
    // end TV stuff
}

KDE_NO_EXPORT void KMPlayerTVSource::read (KConfig * m_config) {
    setURL ("tv://");
    m_config->setGroup(strTV);
    tvdriver = m_config->readEntry (strTVDriver, "v4l");
    QStrList devlist;
    int deviceentries = m_config->readListEntry (strTVDevices, devlist, ';');
    for (int i = 0; i < deviceentries; i++) {
        m_config->setGroup (devlist.at (i));
        TVDevice * device = new TVDevice (m_document, devlist.at (i),
                                          m_config->readSizeEntry (strTVSize));
        device->pretty_name = m_config->readEntry (strTVDeviceName, "/dev/video");
        device->audiodevice = m_config->readEntry (strTVAudioDevice, "");
        device->minsize = m_config->readSizeEntry (strTVMinSize);
        device->maxsize = m_config->readSizeEntry (strTVMaxSize);
        device->noplayback = m_config->readBoolEntry (strTVNoPlayback, false);
        QStrList inputlist;
        int inputentries = m_config->readListEntry (strTVInputs, inputlist,';');
        kdDebug() << device->src << " has " << inputentries << " inputs" << endl;
        for (int j = 0; j < inputentries; j++) {
            QString inputstr = inputlist.at (j);
            int pos = inputstr.find (':');
            if (pos < 0) {
                kdError () << "Wrong input: " << inputstr << endl;
                continue;
            }
            TVInput * input = new TVInput (m_document, inputstr.mid (pos + 1), inputstr.left (pos).toInt ());
            QStrList freqlist;
            int freqentries = m_config->readListEntry(input->name,freqlist,';');
            kdDebug() << input->name<< " has " << freqentries << " freqs" << endl;
            input->hastuner = (freqentries > 0);
            for (int k = 0; k < freqentries; k++) {
                QString freqstr = freqlist.at (k);
                int pos = freqstr.find (':');
                if (pos < 0) {
                    kdWarning () << "Wrong frequency or none defined: " << freqstr << endl;
                    continue;
                }
                input->appendChild ((new TVChannel (m_document, freqstr.left (pos), freqstr.mid (pos+1).toInt ()))->self ());
                kdDebug() << freqstr.left (pos) << " at " << freqstr.mid (pos+1).toInt() << endl;
            }
            if (input->hastuner) // what if multiple tuners?
                input->norm = m_config->readEntry (strTVNorm, "PAL");
            device->appendChild (input->self ());
        }
        m_document->appendChild (device->self ());
    }
}

KDE_NO_EXPORT void KMPlayerTVSource::sync (bool fromUI) {
    if (m_document && m_document->hasChildNodes ())
        m_app->showBroadcastConfig ();
    else
        m_app->hideBroadcastConfig ();
    if (fromUI) {
        tvdriver = m_configpage->driver->text ();
    } else {
        m_configpage->driver->setText (tvdriver);
        m_configpage->setTVDocument (m_document);
    }
}

KDE_NO_EXPORT void KMPlayerTVSource::prefLocation (QString & item, QString & icon, QString & tab) {
    item = i18n ("Source");
    icon = QString ("source");
    tab = i18n ("TV");
}

KDE_NO_EXPORT QFrame * KMPlayerTVSource::prefPage (QWidget * parent) {
    if (!m_configpage) {
        m_configpage = new KMPlayerPrefSourcePageTV (parent);
        m_configpage->scanner = new TVDeviceScannerSource (m_document,m_player);
    }
    return m_configpage;
}

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT TVDeviceScannerSource::TVDeviceScannerSource (KMPlayer::ElementPtr d, KMPlayer::PartBase * player)
 : KMPlayer::Source (i18n ("TVScanner"), player, "tvscanner"), m_doc (d), m_tvdevice (0) {
    setURL (KURL ("tv://"));
}

KDE_NO_EXPORT void TVDeviceScannerSource::init () {
}

KDE_NO_EXPORT bool TVDeviceScannerSource::processOutput (const QString & line) {
    if (m_nameRegExp.search (line) > -1) {
        m_tvdevice->pretty_name = m_nameRegExp.cap (1);
        kdDebug() << "Name " << m_tvdevice->pretty_name << endl;
    } else if (m_sizesRegExp.search (line) > -1) {
        m_tvdevice->minsize = QSize (m_sizesRegExp.cap (1).toInt (),
                                     m_sizesRegExp.cap (2).toInt ());
        m_tvdevice->maxsize = QSize (m_sizesRegExp.cap (3).toInt (),
                                     m_sizesRegExp.cap (4).toInt ());
        kdDebug() << "MinSize (" << m_tvdevice->minsize.width () << "," << m_tvdevice->minsize.height () << ")" << endl;
    } else if (m_inputRegExp.search (line) > -1) {
        TVInput * input = new TVInput (m_doc, m_inputRegExp.cap (2).stripWhiteSpace (),
                                       m_inputRegExp.cap (1).toInt ());
        input->hastuner = m_inputRegExp.cap (3).toInt () == 1;
        m_tvdevice->appendChild (input->self ());
        kdDebug() << "Input " << input->id << ": " << input->name << endl;
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
    m_tvdevice = new TVDevice (m_doc, dev, QSize ());
    m_driver = dri;
    m_source = m_player->process ()->source ();
    m_player->setSource (this);
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
    disconnect (m_player, SIGNAL (stopPlaying ()), this, SLOT (finished ()));
    if (m_tvdevice) {
        delete m_tvdevice;
        m_tvdevice = 0L;
        emit scanFinished (m_tvdevice);
    }
}

KDE_NO_EXPORT void TVDeviceScannerSource::play () {
    if (!m_tvdevice)
        return;
    QString args;
    args.sprintf ("tv:// -tv driver=%s:device=%s -identify -frames 0", m_driver.ascii (), m_tvdevice->src.ascii ());
    m_player->stop ();
    m_player->process ()->initProcess ();
    if (static_cast <KMPlayer::MPlayer *> (m_player->players () ["mplayer"])->run (args.ascii()))
        connect (m_player, SIGNAL (stopPlaying ()), this, SLOT (finished ()));
    else
        deactivate ();
}

KDE_NO_EXPORT void TVDeviceScannerSource::finished () {
    TVDevice * dev = 0L;
    kdDebug () << "TVDeviceScannerSource::finished " << m_tvdevice->hasChildNodes () << endl;
    if (!m_tvdevice->hasChildNodes ())
        delete m_tvdevice;
    else
        dev = m_tvdevice;
    m_tvdevice = 0L;
    m_player->setSource (m_source);
    emit scanFinished (dev);
}

#include "kmplayertvsource.moc"
