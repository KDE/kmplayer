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
#include <qtooltip.h>
#include <qtabwidget.h>
#include <qbuttongroup.h>
#include <qmessagebox.h>
#include <qpopupmenu.h>

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

struct TableInserter {
    QTable * table;
    int index;
    TableInserter (QTable * t) : table (t), index (0) {}
    void operator () (TVChannel * channel) {
        table->setItem (index, 0, new QTableItem (table, QTableItem::Always,
                        channel->name));
        table->setItem (index++, 1, new QTableItem (table, QTableItem::Always,
                        QString::number (channel->frequency)));
    }
};

//-----------------------------------------------------------------------------

KMPlayerPrefSourcePageTVDevice::KMPlayerPrefSourcePageTVDevice (QWidget *parent, TVDevice * dev)
: QFrame (parent, "PageTVDevice"), device (dev) {
    QVBoxLayout *layout = new QVBoxLayout (this, 5, 2);
    QLabel * deviceLabel = new QLabel (QString (i18n ("Video device:")) + device->device, this, 0);
    layout->addWidget (deviceLabel);
    QGridLayout *gridlayout = new QGridLayout (layout, 5, 4);
    QLabel * audioLabel = new QLabel (i18n ("Audio device:"), this);
    audiodevice = new KURLRequester (device->audiodevice, this);
    QLabel * nameLabel = new QLabel (i18n ("Name:"), this, 0);
    name = new QLineEdit ("", this, 0);
    QLabel *sizewidthLabel = new QLabel (i18n ("Width:"), this, 0);
    sizewidth = new QLineEdit ("", this, 0);
    QLabel *sizeheightLabel = new QLabel (i18n ("Height:"), this, 0);
    sizeheight = new QLineEdit ("", this, 0);
    noplayback = new QCheckBox (i18n ("Do not immediately play"), this);
    QToolTip::add (noplayback, i18n ("Only start playing after clicking the play button"));
    inputsTab = new QTabWidget (this);
    TVInputList::iterator iit = device->inputs.begin ();
    for (; iit != device->inputs.end (); ++iit) {
        TVInput * input = *iit;
        QWidget * widget = new QWidget (this);
        QVBoxLayout *tablayout = new QVBoxLayout (widget, 5, 2);
        QLabel * inputLabel = new QLabel (input->name, widget);
        tablayout->addWidget (inputLabel);
        if (input->hastuner) {
            QHBoxLayout *horzlayout = new QHBoxLayout ();
            horzlayout->addWidget (new QLabel (i18n ("Norm:"), widget));
            QComboBox * norms = new QComboBox (widget, "PageTVNorm");
            norms->insertItem (QString ("NTSC"), 0);
            norms->insertItem (QString ("PAL"), 1);
            norms->insertItem (QString ("SECAM"), 2);
            norms->setCurrentText (input->norm);
            horzlayout->addWidget (norms);
            tablayout->addLayout (horzlayout);
            QTable * table = new QTable (90, 2, widget, "PageTVChannels");
            table->setColumnWidth (0, 250);
            table->setColumnWidth (1, 150);
            QHeader *header = table->horizontalHeader();
            header->setLabel (0, i18n ("Channel"));
            header->setLabel (1, i18n ("Frequency"));
            std::for_each (input->channels.begin(),
                           input->channels.end(),
                           TableInserter(table));
            tablayout->addSpacing (5);
            tablayout->addWidget (table);
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
    gridlayout->addMultiCellWidget (noplayback, 3, 3, 0, 3);
    layout->addWidget (inputsTab);
    layout->addSpacing (5);
    layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum));
    QHBoxLayout *buttonlayout = new QHBoxLayout ();
    buttonlayout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum));
    buttonlayout->addWidget (delButton);
    layout->addLayout (buttonlayout);
}

void KMPlayerPrefSourcePageTVDevice::slotDelete () {
    if (KMessageBox::warningYesNo (this, i18n ("You're about to remove this device from the Source menu.\nContinue?"), i18n ("Confirm")) == KMessageBox::Yes)
        emit deleted (this);
}

void KMPlayerPrefSourcePageTVDevice::updateTVDevice () {
    device->name = name->text ();
    device->audiodevice = audiodevice->lineEdit()->text ();
    device->noplayback = noplayback->isChecked ();
    device->size = QSize(sizewidth->text().toInt(), sizeheight->text().toInt());
    TVInputList::iterator iit = device->inputs.begin ();
    for (int i = 0; iit != device->inputs.end (); ++iit, i++) {
        TVInput * input = *iit;
        if (input->hastuner) {
            QWidget * widget = inputsTab->page (i);
            QTable * table = static_cast <QTable *> (widget->child ("PageTVChannels", "QTable"));
            if (table) {
                input->clear ();
                for (int j = 0; j < table->numRows (); ++j) {
                    if (table->item (j, 0) && table->item (j, 1) && !table->item (j, 0)->text ().isEmpty ())
                        input->channels.push_back (new TVChannel (table->item (j, 0)->text (), table->item (j, 1)->text ().toInt ()));
                }
            }
            QComboBox * norms = static_cast <QComboBox *> (widget->child ("PageTVNorm", "QComboBox"));
            if (norms) {
                input->norm = norms->currentText ();
            }
        }
    }
}

//-----------------------------------------------------------------------------

KMPlayerPrefSourcePageTV::KMPlayerPrefSourcePageTV (QWidget *parent)
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
    QToolTip::add (driver, i18n ("dummy, v4l or bsdbt848"));
    QLabel *deviceLabel = new QLabel (i18n ("Device:"), general, 0);
    device = new KURLRequester ("/dev/video", general);
    QToolTip::add (device, i18n("Path to your video device, eg. /dev/video0"));
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

struct TVDevicePageAdder {
    KMPlayerPrefSourcePageTV * page;
    bool show;
    TVDevicePageAdder (KMPlayerPrefSourcePageTV * p, bool s = false) : page (p), show (s) {}
    void operator () (TVDevice * device);
};

void TVDevicePageAdder::operator () (TVDevice * device) {
    KMPlayerPrefSourcePageTVDevice * devpage = new KMPlayerPrefSourcePageTVDevice (page->tab, device);
    page->tab->insertTab (devpage, device->name);
    devpage->name->setText (device->name);
    devpage->sizewidth->setText (QString::number (device->size.width ()));
    devpage->sizeheight->setText (QString::number (device->size.height ()));
    devpage->noplayback->setChecked (device->noplayback);
    page->connect (devpage, SIGNAL (deleted (KMPlayerPrefSourcePageTVDevice *)),
                   page, SLOT (slotDeviceDeleted (KMPlayerPrefSourcePageTVDevice *)));
    page->m_devicepages.push_back (devpage);
    if (show)
        page->tab->setCurrentPage (page->tab->indexOf (devpage));
}

void KMPlayerPrefSourcePageTV::setTVDevices (TVDeviceList * devs) {
    m_devices = devs;
    std::for_each (addeddevices.begin(), addeddevices.end(), Deleter<TVDevice>);
    addeddevices.clear ();
    deleteddevices.clear ();
    std::for_each (m_devicepages.begin(), m_devicepages.end(), Deleter<QFrame>);
    m_devicepages.clear ();
    std::for_each(m_devices->begin(), m_devices->end(),TVDevicePageAdder(this));
}

void KMPlayerPrefSourcePageTV::slotDeviceDeleted (KMPlayerPrefSourcePageTVDevice * devpage) {
    if (std::find (addeddevices.begin (), addeddevices.end (), devpage->device) != addeddevices.end ())
        addeddevices.remove (devpage->device);
    deleteddevices.push_back (devpage->device);
    m_devicepages.remove (devpage);
    devpage->deleteLater ();
    tab->setCurrentPage (0);
}

void KMPlayerPrefSourcePageTV::slotScan () {
    TVDeviceList::iterator dit = std::find (m_devices->begin (),
                                            m_devices->end (), device->lineEdit()->text ());
    if (dit != m_devices->end ()) {
        dit = std::find (deleteddevices.begin (),
                         deleteddevices.end (), device->lineEdit()->text ());
        if (dit == deleteddevices.end ()) {
            KMessageBox::error (this, i18n ("Device already present."),
                                      i18n ("Error"));
            return;
        }
    }
    scanner->scan (device->lineEdit()->text (), driver->text());
    connect (scanner, SIGNAL (scanFinished (TVDevice *)),
             this, SLOT (slotScanFinished (TVDevice *)));
}

void KMPlayerPrefSourcePageTV::slotScanFinished (TVDevice * _device) {
    disconnect (scanner, SIGNAL (scanFinished (TVDevice *)),
                this, SLOT (slotScanFinished (TVDevice *)));
    if (!_device) {
        KMessageBox::error (this, i18n ("No device found."), i18n ("Error"));
    } else {
        addeddevices.push_back (_device);
        TVDevicePageAdder (this, true) (_device);
    }
}

void KMPlayerPrefSourcePageTV::updateTVDevices () {
    TVDevicePageList::iterator pit = m_devicepages.begin ();
    for (; pit != m_devicepages.end (); ++pit)
            (*pit)->updateTVDevice ();
    // remove deleted devices
    TVDeviceList::iterator deldit = deleteddevices.begin ();
    for (; deldit != deleteddevices.end (); ++deldit) {
        TVDeviceList::iterator dit = std::find (m_devices->begin (), m_devices->end (), (*deldit)->device);
        if (dit != m_devices->end ())
            m_devices->erase (dit);
        delete *dit;
    }
    deleteddevices.clear ();
    // move added devices to device list
    m_devices->splice (m_devices->end (), addeddevices);
}

//-----------------------------------------------------------------------------

TVChannel::TVChannel (const QString & n, int f) : name (n), frequency (f) {}

TVInput::TVInput (const QString & n, int _id) : name (n), id (_id) {}

void TVInput::clear () {
    std::for_each (channels.begin (), channels.end (), Deleter <TVChannel>);
    channels.clear ();
}

TVDevice::TVDevice (const QString & d, const QSize & s)
    : device (d), size (s), noplayback (false) {
}

void TVDevice::clear () {
    std::for_each (inputs.begin (), inputs.end (), Deleter <TVInput>);
    inputs.clear ();
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

KMPlayerTVSource::KMPlayerTVSource (KMPlayerApp * a, QPopupMenu * m)
    : KMPlayerMenuSource (i18n ("TV"), a, m), m_configpage (0L) {
    m_tvsource = 0L;
    m_menu->insertTearOffHandle ();
    setURL (KURL ("tv://"));
    m_player->settings ()->pagelist.push_back (this);
}

KMPlayerTVSource::~KMPlayerTVSource () {
    std::for_each (tvdevices.begin (), tvdevices.end (), Deleter <TVDevice>);
}

void KMPlayerTVSource::activate () {
    m_identified = true;
    m_player->setProcess (m_player->mplayer ());
    buildArguments ();
    if (m_player->settings ()->showbroadcastbutton)
        m_app->view()->broadcastButton ()->show ();
}
/* TODO: playback by
 * ffmpeg -vd /dev/video0 -r 25 -s 768x576 -f rawvideo - |mplayer -nocache -ao arts -rawvideo on:w=768:h=576:fps=25 -quiet -
 */
void KMPlayerTVSource::buildArguments () {
    if (!m_tvsource)
        return;
    m_identified = true;
    KMPlayerSettings * config = m_player->settings ();
    m_app->setCaption (QString (i18n ("TV: ")) + m_tvsource->title, false);
    setWidth (m_tvsource->size.width ());
    setHeight (m_tvsource->size.height ());
    m_options.sprintf ("-tv noaudio:driver=%s:%s:width=%d:height=%d -slave -nocache -quiet", tvdriver.ascii (), m_tvsource->command.ascii (), width (), height ());
    if (config->mplayerpost090)
        m_recordcmd.sprintf ("-tv %s:driver=%s:%s:width=%d:height=%d", m_tvsource->audiodevice.isEmpty () ? "noaudio" : (QString ("forceaudio:adevice=") + m_tvsource->audiodevice).ascii(), tvdriver.ascii (), m_tvsource->command.ascii (), width (), height ());
    else
        m_recordcmd.sprintf ("-tv on:%s:driver=%s:%s:width=%d:height=%d", m_tvsource->audiodevice.isEmpty () ? "noaudio" : (QString ("forceaudio:adevice=") + m_tvsource->audiodevice).ascii(), tvdriver.ascii (), m_tvsource->command.ascii (), width (), height ());
    if (!m_app->broadcasting ())
        m_app->resizePlayer (100);
    m_audiodevice = m_tvsource->audiodevice;
    m_videodevice = m_tvsource->videodevice;
    m_videonorm = m_tvsource->norm;
    m_frequency = m_tvsource->frequency;
}

void KMPlayerTVSource::deactivate () {
    if (m_app->view () && !m_app->view ()->broadcastButton ()->isOn ())
        m_app->view ()->broadcastButton ()->hide ();
}

void KMPlayerTVSource::buildMenu () {
    QString currentcommand;
    if (m_tvsource)
        currentcommand = m_tvsource->command;
    CommandMap::iterator it = commands.begin ();
    for ( ; it != commands.end (); ++it)
        delete it.data ();
    commands.clear ();
    m_menu->clear ();
    m_menu->insertTearOffHandle ();
    m_tvsource = 0L;
    int counter = 0;
    TVDeviceList::iterator dit = tvdevices.begin ();
    for (; dit != tvdevices.end (); ++dit) {
        TVDevice * device = *dit;
        QPopupMenu * devmenu = new QPopupMenu (m_app);
        TVInputList::iterator iit = device->inputs.begin ();
        for (; iit != device->inputs.end (); ++iit) {
            TVInput * input = *iit;
            if (input->channels.size () <= 0) {
                TVSource * source = new TVSource;
                devmenu->insertItem (input->name, this, SLOT (menuClicked (int)), 0, counter);
                source->videodevice = device->device;
                source->audiodevice = device->audiodevice;
                source->noplayback = device->noplayback;
                source->frequency = -1;
                source->command.sprintf ("device=%s:input=%d", device->device.ascii (), input->id);
                if (currentcommand == source->command)
                    m_tvsource = source;
                source->size = device->size;
                source->title = device->name + QString ("-") + input->name;
                commands.insert (counter++, source);
            } else {
                QPopupMenu * inputmenu = new QPopupMenu (m_app);
                inputmenu->insertTearOffHandle ();
                TVChannelList::iterator it = input->channels.begin ();
                for (; it != input->channels.end (); ++it) {
                    TVSource * source = new TVSource;
                    source->videodevice = device->device;
                    source->audiodevice = device->audiodevice;
                    source->noplayback = device->noplayback;
                    source->frequency = (*it)->frequency;
                    source->size = device->size;
                    source->norm = input->norm;
                    inputmenu->insertItem ((*it)->name, this, SLOT(menuClicked (int)), 0, counter);
                    source->command.sprintf ("device=%s:input=%d:freq=%d", device->device.ascii (), input->id, (*it)->frequency);
                    if (!source->norm.isEmpty ())
                        source->command += QString (":norm=") + source->norm;
                    source->title = device->name + QString("-") + (*it)->name;
                    if (currentcommand == source->command)
                        m_tvsource = source;
                    commands.insert (counter++, source);
                }
                devmenu->insertItem (input->name, inputmenu, 0, input->id);
            }
        }
        m_menu->insertItem (device->name, devmenu);
    }
}

void KMPlayerTVSource::menuClicked (int id) {
    CommandMap::iterator it = commands.find (id);
    if (it != commands.end ()) {
        TVSource * prevsource = m_tvsource;
        m_tvsource = it.data ();
        bool playing = prevsource && 
                       (prevsource->videodevice == m_tvsource->videodevice) &&
                       m_player->playing ();
        buildArguments ();
        if (m_player->process ()->source () != this) {
            m_player->setSource (this);
            playing = false;
        }
        if (m_app->broadcasting ())
            QTimer::singleShot (0, m_app, SLOT (startFeed ()));
        else {
            m_player->stop ();
            if (!m_tvsource->noplayback || playing)
                QTimer::singleShot (0, m_player, SLOT (play ()));
        }
    }
}

QString KMPlayerTVSource::filterOptions () {
    if (! m_player->settings ()->disableppauto)
        return KMPlayerSource::filterOptions ();
    return QString ("-vop pp=lb");
}

bool KMPlayerTVSource::hasLength () {
    return false;
}

bool KMPlayerTVSource::isSeekable () {
    return false;
}

QString KMPlayerTVSource::prettyName () {
    QString name (i18n ("TV"));
    if (m_tvsource)
        name += ' ' + m_tvsource->title;
    return name;
}

void KMPlayerTVSource::write (KConfig * m_config) {
    //TV stuff
    m_config->setGroup (strTV);
    QStringList devicelist = m_config->readListEntry (strTVDevices, ';');
    for (unsigned i = 0; i < devicelist.size (); i++)
        m_config->deleteGroup (*devicelist.at (i));
    devicelist.clear ();
    if (m_configpage)
        m_configpage->updateTVDevices ();
    QString sep = QString (":");
    TVDeviceList::iterator dit = tvdevices.begin ();
    for (; dit != tvdevices.end (); ++dit) {
        TVDevice * device = *dit;
        kdDebug() << " writing " << device->device << endl;
        devicelist.append (device->device);
        m_config->setGroup (device->device);
        m_config->writeEntry (strTVSize, device->size);
        m_config->writeEntry (strTVMinSize, device->minsize);
        m_config->writeEntry (strTVMaxSize, device->maxsize);
        m_config->writeEntry (strTVNoPlayback, device->noplayback);
        m_config->writeEntry (strTVDeviceName, device->name);
        m_config->writeEntry (strTVAudioDevice, device->audiodevice);
        QStringList inputlist;
        TVInputList::iterator iit = device->inputs.begin ();
        for (; iit != device->inputs.end (); ++iit) {
            TVInput * input = *iit;
            inputlist.append (QString::number (input->id) + sep + input->name);
            if (input->hastuner) {
                QStringList channellist;
                TVChannelList::iterator it = input->channels.begin ();
                for (; it != input->channels.end (); ++it) {
                    channellist.append ((*it)->name + sep + QString::number ((*it)->frequency));
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

void KMPlayerTVSource::read (KConfig * m_config) {
    std::for_each (tvdevices.begin (), tvdevices.end (), Deleter <TVDevice>);
    tvdevices.clear ();
    m_config->setGroup(strTV);
    tvdriver = m_config->readEntry (strTVDriver, "v4l");
    QStrList devlist;
    int deviceentries = m_config->readListEntry (strTVDevices, devlist, ';');
    for (int i = 0; i < deviceentries; i++) {
        m_config->setGroup (devlist.at (i));
        TVDevice * device = new TVDevice (devlist.at (i),
                                          m_config->readSizeEntry (strTVSize));
        device->name = m_config->readEntry (strTVDeviceName, "/dev/video");
        device->audiodevice = m_config->readEntry (strTVAudioDevice, "");
        device->minsize = m_config->readSizeEntry (strTVMinSize);
        device->maxsize = m_config->readSizeEntry (strTVMaxSize);
        device->noplayback = m_config->readBoolEntry (strTVNoPlayback, false);
        QStrList inputlist;
        int inputentries = m_config->readListEntry (strTVInputs, inputlist,';');
        kdDebug() << device->device << " has " << inputentries << " inputs" << endl;
        for (int j = 0; j < inputentries; j++) {
            QString inputstr = inputlist.at (j);
            int pos = inputstr.find (':');
            if (pos < 0) {
                kdError () << "Wrong input: " << inputstr << endl;
                continue;
            }
            TVInput * input = new TVInput (inputstr.mid (pos + 1),
                                           inputstr.left (pos).toInt ());
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
                TVChannel * channel = new TVChannel (freqstr.left (pos),
                                                  freqstr.mid (pos+1).toInt ());
                kdDebug() << freqstr.left (pos) << " at " << freqstr.mid (pos+1).toInt() << endl;
                input->channels.push_back (channel);
            }
            if (input->hastuner) // what if multiple tuners?
                input->norm = m_config->readEntry (strTVNorm, "PAL");
            device->inputs.push_back (input);
        }
        tvdevices.push_back (device);
    }
}

void KMPlayerTVSource::sync (bool fromUI) {
    if (fromUI) {
        tvdriver = m_configpage->driver->text ();
    } else {
        m_configpage->driver->setText (tvdriver);
        m_configpage->setTVDevices (&tvdevices);
    }
}

void KMPlayerTVSource::prefLocation (QString & item, QString & icon, QString & tab) {
    item = i18n ("Source");
    icon = QString ("source");
    tab = i18n ("TV");
}

QFrame * KMPlayerTVSource::prefPage (QWidget * parent) {
    if (!m_configpage) {
        m_configpage = new KMPlayerPrefSourcePageTV (parent);
        m_configpage->scanner = new TVDeviceScannerSource (m_player);
    }
    return m_configpage;
}

//-----------------------------------------------------------------------------

TVDeviceScannerSource::TVDeviceScannerSource (KMPlayer * player)
    : KMPlayerSource (i18n ("TVScanner"), player), m_tvdevice (0) {}

void TVDeviceScannerSource::init () {
    ;
}

bool TVDeviceScannerSource::processOutput (const QString & line) {
    if (m_nameRegExp.search (line) > -1) {
        m_tvdevice->name = m_nameRegExp.cap (1);
        kdDebug() << "Name " << m_tvdevice->name << endl;
    } else if (m_sizesRegExp.search (line) > -1) {
        m_tvdevice->minsize = QSize (m_sizesRegExp.cap (1).toInt (),
                                     m_sizesRegExp.cap (2).toInt ());
        m_tvdevice->maxsize = QSize (m_sizesRegExp.cap (3).toInt (),
                                     m_sizesRegExp.cap (4).toInt ());
        kdDebug() << "MinSize (" << m_tvdevice->minsize.width () << "," << m_tvdevice->minsize.height () << ")" << endl;
    } else if (m_inputRegExp.search (line) > -1) {
        TVInput * input = new TVInput (m_inputRegExp.cap (2).stripWhiteSpace (),
                                       m_inputRegExp.cap (1).toInt ());
        input->hastuner = m_inputRegExp.cap (3).toInt () == 1;
        m_tvdevice->inputs.push_back (input);
        kdDebug() << "Input " << input->id << ": " << input->name << endl;
    } else
        return false;
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
    m_tvdevice = new TVDevice (dev, QSize ());
    m_driver = dri;
    m_source = m_player->process ()->source ();
    m_player->setSource (this);
    play ();
    return !!m_tvdevice;
}

void TVDeviceScannerSource::activate () {
    m_player->setProcess (m_player->mplayer ());
    m_nameRegExp.setPattern ("Selected device:\\s*([^\\s].*)");
    m_sizesRegExp.setPattern ("Supported sizes:\\s*([0-9]+)x([0-9]+) => ([0-9]+)x([0-9]+)");
    m_inputRegExp.setPattern ("\\s*([0-9]+):\\s*([^:]+):[^\\(]*\\(tuner:([01]),\\s*norm:([^\\)]+)\\)");
}

void TVDeviceScannerSource::deactivate () {
    disconnect (m_player, SIGNAL (stopPlaying ()), this, SLOT (finished ()));
    if (m_tvdevice) {
        delete m_tvdevice;
        m_tvdevice = 0L;
        emit scanFinished (m_tvdevice);
    }
}

void TVDeviceScannerSource::play () {
    if (!m_tvdevice)
        return;
    QString args;
    args.sprintf ("tv:// -tv driver=%s:device=%s -identify -frames 0", m_driver.ascii (), m_tvdevice->device.ascii ());
    m_player->stop ();
    m_player->mplayer ()->initProcess ();
    if (m_player->mplayer ()->run (args.ascii()))
        connect (m_player, SIGNAL (stopPlaying ()), this, SLOT (finished ()));
    else
        deactivate ();
}

void TVDeviceScannerSource::finished () {
    TVDevice * dev = 0L;
    if (!m_tvdevice->inputs.size ())
        delete m_tvdevice;
    else
        dev = m_tvdevice;
    m_tvdevice = 0L;
    m_player->setSource (m_source);
    emit scanFinished (dev);
}

#include "kmplayertvsource.moc"
