/**
 * Copyright (C) 2003 Joonas Koivunen <rzei@mbnet.fi>
 * Copyright (C) 2003 Koos Vriezen <koos.vriezen@xs4all.nl>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#undef Always

#include <algorithm>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <qlayout.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qcheckbox.h>
#include <qtable.h>
#include <qstringlist.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <qgroupbox.h>
#include <qtooltip.h>
#include <qtabwidget.h>
#include <qslider.h>
#include <qbuttongroup.h>
#include <qspinbox.h>
#include <qmessagebox.h>

#include <klocale.h>
#include <kdebug.h>
#include <kfiledialog.h>
#include <kmessagebox.h>
#include <klineedit.h>
#include <kiconloader.h>
#include <kdeversion.h>
#include <kcombobox.h>

#include "pref.h"
#include "kmplayerpartbase.h"
#include "kmplayerprocess.h"
#include "kmplayerconfig.h"

KMPlayerPreferences::KMPlayerPreferences(KMPlayer * player, MPlayerAudioDriver * ad, FFServerSettingList & ffs)
: KDialogBase (IconList, i18n ("KMPlayer Preferences"),
		Help|Default|Ok|Apply|Cancel, Ok, player->view (), 0, false)
{
    QFrame *frame;
    QTabWidget * tab;
    QStringList hierarchy; // typo? :)
    QVBoxLayout *vlay;

    frame = addPage(i18n("General Options"), QString::null, KGlobal::iconLoader()->loadIcon (QString ("kmplayer"), KIcon::NoGroup, 32));
    vlay = new QVBoxLayout(frame, marginHint(), spacingHint());
    tab = new QTabWidget (frame);
    vlay->addWidget (tab);
    m_GeneralPageGeneral = new KMPlayerPrefGeneralPageGeneral (tab);
    tab->insertTab (m_GeneralPageGeneral, i18n("General"));
    m_GeneralPageOutput = new KMPlayerPrefGeneralPageOutput (tab, ad);
    tab->insertTab (m_GeneralPageOutput, i18n("Output"));
    m_GeneralPageAdvanced = new KMPlayerPrefGeneralPageAdvanced (tab);
    tab->insertTab (m_GeneralPageAdvanced, i18n("Advanced"));

    frame = addPage (i18n ("Source"), QString::null, KGlobal::iconLoader()->loadIcon (QString ("source"), KIcon::NoGroup, 32));
    vlay = new QVBoxLayout (frame, marginHint(), spacingHint());
    tab = new QTabWidget (frame);
    vlay->addWidget (tab);
    m_SourcePageURL = new KMPlayerPrefSourcePageURL (tab);
    tab->insertTab (m_SourcePageURL, i18n ("URL"));
    m_GeneralPageDVD = new KMPlayerPrefGeneralPageDVD (tab);
    tab->insertTab (m_GeneralPageDVD, i18n ("DVD"));
    m_GeneralPageVCD = new KMPlayerPrefGeneralPageVCD (tab);
    tab->insertTab (m_GeneralPageVCD, i18n ("VCD"));
    m_SourcePageTV = new KMPlayerPrefSourcePageTV (tab, this);
    tab->insertTab (m_SourcePageTV, i18n ("TV"));

    frame = addPage (i18n ("Recording"), QString::null, KGlobal::iconLoader()->loadIcon (QString ("video"), KIcon::NoGroup, 32));
    vlay = new QVBoxLayout (frame, marginHint(), spacingHint());
    tab = new QTabWidget (frame);
    vlay->addWidget (tab);
    m_MEncoderPage = new KMPlayerPrefMEncoderPage (tab, player);
    tab->insertTab (m_MEncoderPage, i18n ("MEncoder"));
    recorders.push_back (m_MEncoderPage);
    m_FFMpegPage = new KMPlayerPrefFFMpegPage (tab, player);
    tab->insertTab (m_FFMpegPage, i18n ("FFMpeg"));
    recorders.push_back (m_FFMpegPage);
    m_RecordPage = new KMPlayerPrefRecordPage (tab, player, recorders);
    tab->insertTab (m_RecordPage, i18n ("General"), 0);
    tab->setCurrentPage (0);

    frame = addPage (i18n ("Broadcasting"), QString::null, KGlobal::iconLoader()->loadIcon (QString ("share"), KIcon::NoGroup, 32));
    vlay = new QVBoxLayout (frame, marginHint(), spacingHint());
    tab = new QTabWidget (frame);
    vlay->addWidget (tab);
    m_BroadcastFormatPage = new KMPlayerPrefBroadcastFormatPage (tab, ffs);
    tab->insertTab (m_BroadcastFormatPage, i18n ("Profiles"));
    m_BroadcastPage = new KMPlayerPrefBroadcastPage (tab);
    tab->insertTab (m_BroadcastPage, i18n ("FFServer"));

    /*
     * not yet needed...
     */

	/*hierarchy.clear();
	hierarchy << i18n("Output plugins") << i18n("General");
	frame = addPage(hierarchy, i18n("Output Plugin Options || NOT YET USED == FIXME!"));
	vlay = new QVBoxLayout(frame, marginHint(), spacingHint());
	m_OPPageGeneral = new KMPlayerPrefOPPageGeneral(frame);
	vlay->addWidget(m_OPPageGeneral);*/

    frame = addPage (i18n ("Output plugins"), QString::null, KGlobal::iconLoader()->loadIcon (QString ("image"), KIcon::NoGroup, 32));
    vlay = new QVBoxLayout(frame, marginHint(), spacingHint());
    tab = new QTabWidget (frame);
    vlay->addWidget (tab);
    m_OPPagePostproc = new KMPlayerPrefOPPagePostProc (tab);
    tab->insertTab (m_OPPagePostproc, i18n ("Postprocessing"));

    connect (this, SIGNAL (defaultClicked ()), SLOT (confirmDefaults ()));
}

void KMPlayerPreferences::setPage (const char * name) {
    QObject * o = child (name, "QFrame");
    if (!o) return;
    QFrame * page = static_cast <QFrame *> (o);
    QWidget * w = page->parentWidget ();
    while (w && !w->inherits ("QTabWidget"))
        w = w->parentWidget ();
    if (!w) return;
    QTabWidget * t = static_cast <QTabWidget*> (w);
    t->setCurrentPage (t->indexOf(page));
    if (!t->parentWidget() || !t->parentWidget()->inherits ("QFrame"))
        return;
    showPage (pageIndex (t->parentWidget ()));
}

KMPlayerPreferences::~KMPlayerPreferences() {
}

KMPlayerPrefGeneralPageGeneral::KMPlayerPrefGeneralPageGeneral(QWidget *parent)
: QFrame (parent, "GeneralPage")
{
	QVBoxLayout *layout = new QVBoxLayout(this, 5, 2);

	keepSizeRatio = new QCheckBox (i18n("Keep size ratio"), this, 0);
	QToolTip::add(keepSizeRatio, i18n("When checked, movie will keep its aspect ratio\nwhen window is resized"));
	showConsoleOutput = new QCheckBox (i18n("Show console output"), this, 0);
	QToolTip::add(showConsoleOutput, i18n("Shows output from mplayer before and after playing the movie"));
	loop = new QCheckBox (i18n("Loop"), this, 0);
	QToolTip::add(loop, i18n("Makes current movie loop"));
	showControlButtons = new QCheckBox (i18n("Show control buttons"), this, 0);
	QToolTip::add(showControlButtons, i18n("Small buttons will be shown above statusbar to control movie"));
	autoHideControlButtons = new QCheckBox (i18n("Auto hide control buttons"), this, 0);
	QToolTip::add(autoHideControlButtons, i18n("When checked, control buttons will get hidden automatically"));
	showPositionSlider	= new QCheckBox (i18n("Show position slider"), this, 0);
	QToolTip::add(showPositionSlider, i18n("When enabled, will show a seeking slider under the control buttons"));
	showRecordButton = new QCheckBox (i18n ("Show record button"), this);
	QToolTip::add (showRecordButton, i18n ("Add a record button to the control buttons"));
	showBroadcastButton = new QCheckBox (i18n ("Show broadcast button"), this);
	QToolTip::add (showBroadcastButton, i18n ("Add a broadcast button to the control buttons"));
	//autoHideSlider = new QCheckBox (i18n("Auto hide position slider"), this, 0);
	alwaysBuildIndex = new QCheckBox ( i18n("Build new index when possible"), this);
	QToolTip::add(alwaysBuildIndex, i18n("Allows seeking in indexed files (AVIs)"));
	framedrop = new QCheckBox (i18n ("Allow framedrops"), this);
	QToolTip::add (framedrop, i18n ("Allow dropping frames for better audio and video synchronization"));

	QWidget *seekingWidget = new QWidget(this);
	QHBoxLayout *seekingWidgetLayout = new QHBoxLayout(seekingWidget);
	seekingWidgetLayout->addWidget(new QLabel(i18n("Forward/backward seek time:"),seekingWidget));
	seekingWidgetLayout->addItem(new QSpacerItem(0,0,QSizePolicy::Minimum, QSizePolicy::Minimum));
	seekTime = new QSpinBox(1, 600, 1, seekingWidget);
	seekingWidgetLayout->addWidget(seekTime);
	seekingWidgetLayout->addItem(new QSpacerItem(0,0,QSizePolicy::Minimum, QSizePolicy::Minimum));
	layout->addWidget(keepSizeRatio);
	layout->addWidget(showConsoleOutput);
	layout->addWidget(loop);
	layout->addWidget (framedrop);
	layout->addWidget(showControlButtons);
	layout->addWidget(autoHideControlButtons);
	layout->addWidget(showPositionSlider);
	layout->addWidget (showRecordButton);
	layout->addWidget (showBroadcastButton);
	//layout->addWidget(autoHideSlider);
	layout->addWidget(alwaysBuildIndex);
	layout->addItem (new QSpacerItem (0, 5));
	layout->addWidget(seekingWidget);
        layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

KMPlayerPrefSourcePageURL::KMPlayerPrefSourcePageURL (QWidget *parent)
: QFrame (parent, "URLPage")
{
    QVBoxLayout *layout = new QVBoxLayout (this, 5, 5);
    QHBoxLayout * urllayout = new QHBoxLayout ();
    QHBoxLayout * sub_urllayout = new QHBoxLayout ();
    QLabel *urlLabel = new QLabel (i18n ("URL:"), this, 0);
    urllist = new KComboBox (true, this);
    urllist->setMaxCount (20);
    urllist->setDuplicatesEnabled (false); // not that it helps much :(
    url = new KURLRequester (urllist, this);
    //url->setShowLocalProtocol (true);
    url->setSizePolicy (QSizePolicy (QSizePolicy::Expanding, QSizePolicy::Preferred));
    QLabel *sub_urlLabel = new QLabel (i18n ("Sub Title:"), this, 0);
    sub_urllist = new KComboBox (true, this);
    sub_urllist->setMaxCount (20);
    sub_urllist->setDuplicatesEnabled (false); // not that it helps much :(
    sub_url = new KURLRequester (sub_urllist, this);
    sub_url->setSizePolicy (QSizePolicy (QSizePolicy::Expanding, QSizePolicy::Preferred));
    backend = new QComboBox (this);
    backend->insertItem (QString ("MPlayer"), 0);
    backend->insertItem (QString ("Xine"), 1);
    allowhref = new QCheckBox (i18n ("Enable 'Click to Play' support"), this);
    layout->addWidget (allowhref);
    urllayout->addWidget (urlLabel);
    urllayout->addWidget (url);
    layout->addLayout (urllayout);
    sub_urllayout->addWidget (sub_urlLabel);
    sub_urllayout->addWidget (sub_url);
    layout->addLayout (sub_urllayout);
    layout->addItem (new QSpacerItem (0, 10, QSizePolicy::Minimum, QSizePolicy::Minimum));
#ifdef HAVE_XINE
    QHBoxLayout * backendlayout = new QHBoxLayout ();
    QLabel *backendLabel = new QLabel (i18n ("Use Movie Player:"), this, 0);
    //QToolTip::add (allowhref, i18n ("Explain this in a few lines"));
    backendlayout->addWidget (backendLabel);
    backendlayout->addWidget (backend);
    layout->addLayout (backendlayout);
#else
    backend->hide ();
#endif
    layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

void KMPlayerPrefSourcePageURL::slotBrowse () {
}

KMPlayerPrefGeneralPageDVD::KMPlayerPrefGeneralPageDVD(QWidget *parent) : QFrame(parent)
{
    QVBoxLayout *layout = new QVBoxLayout (this, 5, 2);

    autoPlayDVD = new QCheckBox (i18n ("Auto play after opening DVD"), this, 0);
    QToolTip::add(autoPlayDVD, i18n ("Start playing DVD right after opening DVD"));
    QLabel *dvdDevicePathLabel = new QLabel (i18n("DVD device:"), this, 0);
    dvdDevicePath = new KURLRequester ("/dev/dvd", this, 0);
    QToolTip::add(dvdDevicePath, i18n ("Path to your DVD device, you must have read rights to this device"));
    layout->addWidget (autoPlayDVD);
    layout->addItem (new QSpacerItem (0, 10, QSizePolicy::Minimum, QSizePolicy::Minimum));
    layout->addWidget (dvdDevicePathLabel);
    layout->addWidget (dvdDevicePath);
    layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

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

KMPlayerPrefSourcePageTV::KMPlayerPrefSourcePageTV (QWidget *parent, KMPlayerPreferences * pref)
: QFrame (parent), m_preference (pref) {
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

KMPlayerPrefRecordPage::KMPlayerPrefRecordPage (QWidget *parent, KMPlayer * player, RecorderList & rl) : QFrame (parent, "RecordPage"), m_player (player), m_recorders (rl) {
    QVBoxLayout *layout = new QVBoxLayout (this, 5, 5);
    QHBoxLayout * urllayout = new QHBoxLayout ();
    QLabel *urlLabel = new QLabel (i18n ("Output File:"), this);
    url = new KURLRequester ("", this);
    url->setShowLocalProtocol (true);
    urllayout->addWidget (urlLabel);
    urllayout->addWidget (url);
    recordButton = new QPushButton (i18n ("Start &Recording"), this);
    connect (recordButton, SIGNAL (clicked ()), this, SLOT (slotRecord ()));
    QHBoxLayout *buttonlayout = new QHBoxLayout;
    buttonlayout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum));
    buttonlayout->addWidget (recordButton);
    source = new QLabel (i18n ("Current Source: ") + m_player->process ()->source ()->prettyName (), this);
    recorder = new QButtonGroup (m_recorders.size (), Qt::Vertical, i18n ("Recorder"), this);
    RecorderList::iterator it = m_recorders.begin ();
    for (; it != m_recorders.end (); ++it) {
        QRadioButton * radio = new QRadioButton ((*it)->name (), recorder);
        radio->setEnabled ((*it)->sourceSupported (m_player->process ()->source ()));
    }
    recorder->setButton(0); // for now
    replay = new QButtonGroup (4, Qt::Vertical, i18n ("Auto Playback"), this);
    new QRadioButton (i18n ("&No"), replay);
    new QRadioButton (i18n ("&When recording finished"), replay);
    new QRadioButton (i18n ("A&fter"), replay);
    QWidget * customreplay = new QWidget (replay);
    replaytime = new QLineEdit (customreplay);
    QHBoxLayout *replaylayout = new QHBoxLayout (customreplay);
    replaylayout->addWidget (new QLabel (i18n("Time (seconds):"), customreplay));
    replaylayout->addWidget (replaytime);
    replaylayout->addItem (new QSpacerItem (0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
    layout->addWidget (source);
    layout->addItem (new QSpacerItem (5, 0, QSizePolicy::Minimum, QSizePolicy::Minimum));
    layout->addLayout (urllayout);
    layout->addItem (new QSpacerItem (5, 0, QSizePolicy::Minimum, QSizePolicy::Minimum));
    layout->addWidget (recorder);
    layout->addItem (new QSpacerItem (5, 0, QSizePolicy::Minimum, QSizePolicy::Minimum));
    layout->addWidget (replay);
    layout->addItem (new QSpacerItem (5, 0, QSizePolicy::Minimum, QSizePolicy::Minimum));
    layout->addLayout (buttonlayout);
    layout->addItem (new QSpacerItem (5, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
    connect (m_player, SIGNAL (sourceChanged(KMPlayerSource*)), this, SLOT (sourceChanged(KMPlayerSource*)));
    connect(m_player, SIGNAL(startRecording()), this, SLOT(recordingStarted()));
    connect(m_player, SIGNAL(stopRecording()),this,SLOT (recordingFinished()));
    connect (replay, SIGNAL (clicked (int)), this, SLOT (replayClicked (int)));
}

void KMPlayerPrefRecordPage::recordingStarted () {
    recordButton->setText (i18n ("Stop Recording"));
    url->setEnabled (false);
    topLevelWidget ()->hide ();
}

void KMPlayerPrefRecordPage::recordingFinished () {
    recordButton->setText (i18n ("Start Recording"));
    url->setEnabled (true);
}

void KMPlayerPrefRecordPage::sourceChanged (KMPlayerSource * src) {
    source->setText (i18n ("Current Source: ") + src->prettyName ());
    RecorderList::iterator it = m_recorders.begin ();
    for (int id = 0; it != m_recorders.end (); ++it, ++id) {
        QButton * radio = recorder->find (id);
        radio->setEnabled ((*it)->sourceSupported (src));
    }
}

void KMPlayerPrefRecordPage::replayClicked (int id) {
    replaytime->setEnabled (id == KMPlayerSettings::ReplayAfter);
}

void KMPlayerPrefRecordPage::slotRecord () {
    if (!url->lineEdit()->text().isEmpty()) {
        m_player->stop ();
        m_player->settings ()->recordfile = url->lineEdit()->text();
        m_player->settings ()->replaytime = replaytime->text ().toInt ();
#if KDE_IS_VERSION(3,1,90)
        int id = recorder->selectedId ();
#else
        int id = recorder->id (recorder->selected ());
#endif
        m_player->settings ()->recorder = KMPlayerSettings::Recorder (id);
        RecorderList::iterator it = m_recorders.begin ();
        for (; id > 0 && it != m_recorders.end (); ++it, --id)
            ;
        (*it)->record ();
    }
}

RecorderPage::RecorderPage (QWidget *parent, KMPlayer * player)
 : QFrame (parent), m_player (player) {}

KMPlayerPrefMEncoderPage::KMPlayerPrefMEncoderPage (QWidget *parent, KMPlayer * player) : RecorderPage (parent, player) {
    QVBoxLayout *layout = new QVBoxLayout (this, 5, 5);
    format = new QButtonGroup (3, Qt::Vertical, i18n ("Format"), this);
    new QRadioButton (i18n ("Same as Source"), format);
    new QRadioButton (i18n ("Custom"), format);
    QWidget * customopts = new QWidget (format);
    QGridLayout *gridlayout = new QGridLayout (customopts, 1, 2, 2);
    QLabel *argLabel = new QLabel (i18n("Mencoder arguments:"), customopts, 0);
    arguments = new QLineEdit ("", customopts);
    gridlayout->addWidget (argLabel, 0, 0);
    gridlayout->addWidget (arguments, 0, 1);
    layout->addWidget (format);
    layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
    connect (format, SIGNAL (clicked (int)), this, SLOT (formatClicked (int)));
}

void KMPlayerPrefMEncoderPage::formatClicked (int id) {
    arguments->setEnabled (!!id);
}

void KMPlayerPrefMEncoderPage::record () {
    m_player->setRecorder (m_player->mencoder ());
    if (!m_player->mencoder()->playing ()) {
        m_player->settings ()->mencoderarguments = arguments->text ();
#if KDE_IS_VERSION(3,1,90)
        m_player->settings ()->recordcopy = !format->selectedId ();
#else
        m_player->settings ()->recordcopy = !format->id (format->selected ());
#endif
        m_player->mencoder ()->setURL (KURL (m_player->settings ()->recordfile));
        m_player->mencoder ()->play ();
    } else
        m_player->mencoder ()->stop ();
}

QString KMPlayerPrefMEncoderPage::name () {
    return i18n ("&MEncoder");
}

bool KMPlayerPrefMEncoderPage::sourceSupported (KMPlayerSource *) {
    return true;
}

KMPlayerPrefFFMpegPage::KMPlayerPrefFFMpegPage (QWidget *parent, KMPlayer * player) : RecorderPage (parent, player) {
    QVBoxLayout *layout = new QVBoxLayout (this, 5, 5);
    QGridLayout *gridlayout = new QGridLayout (1, 2, 2);
    QLabel *argLabel = new QLabel (i18n("FFMpeg arguments:"), this);
    arguments = new QLineEdit ("", this);
    gridlayout->addWidget (argLabel, 0, 0);
    gridlayout->addWidget (arguments, 0, 1);
    layout->addLayout (gridlayout);
    layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

void KMPlayerPrefFFMpegPage::record () {
    kdDebug() << "KMPlayerPrefFFMpegPage::record" << endl;
    m_player->setRecorder (m_player->ffmpeg ());
    m_player->ffmpeg ()->setURL (KURL (m_player->settings ()->recordfile));
    m_player->ffmpeg ()->setArguments (arguments->text ());
    m_player->recorder ()->play ();
}

QString KMPlayerPrefFFMpegPage::name () {
    return i18n ("&FFMpeg");
}

bool KMPlayerPrefFFMpegPage::sourceSupported (KMPlayerSource * source) {
    QString protocol = source->url ().protocol ();
    return !source->audioDevice ().isEmpty () ||
           !source->videoDevice ().isEmpty () ||
           !(protocol.startsWith (QString ("dvd")) ||
             protocol.startsWith (QString ("vcd")));
}

KMPlayerPrefBroadcastPage::KMPlayerPrefBroadcastPage (QWidget *parent) : QFrame (parent) {
    QVBoxLayout *layout = new QVBoxLayout (this, 5);
    QGridLayout *gridlayout = new QGridLayout (layout, 6, 2, 2);
    QLabel *label = new QLabel (i18n ("Bind address:"), this);
    bindaddress = new QLineEdit ("", this);
    QToolTip::add (bindaddress, i18n ("If you have multiple network devices, you can limit access"));
    gridlayout->addWidget (label, 0, 0);
    gridlayout->addWidget (bindaddress, 0, 1);
    label = new QLabel (i18n ("Listen port:"), this);
    port = new QLineEdit ("", this);
    gridlayout->addWidget (label, 1, 0);
    gridlayout->addWidget (port, 1, 1);
    label = new QLabel (i18n ("Maximum connections:"), this);
    maxclients = new QLineEdit ("", this);
    gridlayout->addWidget (label, 2, 0);
    gridlayout->addWidget (maxclients, 2, 1);
    label = new QLabel (i18n ("Maximum bandwidth (kbit):"), this);
    maxbandwidth = new QLineEdit ("", this);
    gridlayout->addWidget (label, 3, 0);
    gridlayout->addWidget (maxbandwidth, 3, 1);
    label = new QLabel (i18n ("Temporary feed file:"), this);
    feedfile = new QLineEdit ("", this);
    gridlayout->addWidget (label, 4, 0);
    gridlayout->addWidget (feedfile, 4, 1);
    label = new QLabel (i18n ("Feed file size (kB):"), this);
    feedfilesize = new QLineEdit ("", this);
    gridlayout->addWidget (label, 5, 0);
    gridlayout->addWidget (feedfilesize, 5, 1);
    layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}
#undef ADDPROPERTY
#define ADDPROPERTY(label,qedit,gridlayout,row,parent)                  \
    qedit = new QLineEdit ("", parent);                                 \
    gridlayout->addWidget (new QLabel (qedit, label, parent), row, 0);  \
    gridlayout->addWidget (qedit, row, 1);

KMPlayerPrefBroadcastFormatPage::KMPlayerPrefBroadcastFormatPage (QWidget *parent, FFServerSettingList & ffs) : QFrame (parent), profiles (ffs) 
{
    QHBoxLayout *layout = new QHBoxLayout (this, 5);
    QGridLayout *formatlayout = new QGridLayout (11, 2, 2);
    formatlayout->setAlignment (Qt::AlignTop);
    QVBoxLayout *rightlayout = new QVBoxLayout (5);
    format = new QComboBox (this);
    QLabel * label = new QLabel (format, i18n ("Format:"), this);
    format->clear ();
    format->insertItem (QString ("asf"));
    format->insertItem (QString ("avi"));
    format->insertItem (QString ("mpjpeg"));
    format->insertItem (QString ("mpeg"));
    format->insertItem (QString ("rm"));
    format->insertItem (QString ("swf"));
    QToolTip::add (format, i18n ("Only avi, mpeg and rm work for mplayer playback"));
    formatlayout->addWidget (label, 0, 0);
    formatlayout->addWidget (format, 0, 1);
    ADDPROPERTY (i18n ("Audio codec:"), audiocodec, formatlayout, 1, this);
    ADDPROPERTY (i18n ("Audio bit rate (kbit):"), audiobitrate, formatlayout, 2, this);
    ADDPROPERTY (i18n ("Audio sample rate (Hz):"), audiosamplerate, formatlayout, 3, this);
    ADDPROPERTY (i18n ("Video codec:"), videocodec, formatlayout, 4, this);
    ADDPROPERTY (i18n ("Video bit rate (kbit):"), videobitrate, formatlayout, 5, this);
    ADDPROPERTY (i18n ("Quality (1-31):"), quality, formatlayout, 6, this);
    ADDPROPERTY (i18n ("Frame rate (Hz):"), framerate, formatlayout, 7, this);
    ADDPROPERTY (i18n ("Gop size:"), gopsize, formatlayout, 8, this);
    ADDPROPERTY (i18n ("Width (pixels):"), moviewidth, formatlayout, 9, this);
    ADDPROPERTY (i18n ("Height (pixels):"), movieheight, formatlayout, 10, this);
    label = new QLabel (i18n ("Allow Access from:"), this);
    accesslist = new QTable (40, 1, this);
    accesslist->setColumnWidth (0, 250);
    QToolTip::add (accesslist, i18n ("'Single IP' or 'start-IP end-IP' for IP ranges"));
    QHeader *header = accesslist->horizontalHeader ();
    header->setLabel (0, i18n ("Host/IP or IP range"));
    QFrame *profileframe = new QFrame (this);
    QGridLayout *profileslayout = new QGridLayout (profileframe, 4, 2, 2);
    profile = new QLineEdit ("", profileframe);
    connect (profile, SIGNAL(textChanged (const QString &)),
             this, SLOT (slotTextChanged (const QString &)));
    profilelist = new QListBox (profileframe);
    for (int i = 0; i < (int) profiles.size (); i++)
        profilelist->insertItem (profiles[i]->name, i);
    connect (profilelist, SIGNAL (selected (int)),
             this, SLOT (slotIndexChanged (int)));
    connect (profilelist, SIGNAL (highlighted (int)),
             this, SLOT (slotItemHighlighted (int)));
    load = new QPushButton (i18n ("Load"), profileframe);
    save = new QPushButton (i18n ("Save"), profileframe);
    del = new QPushButton (i18n ("Delete"), profileframe);
    load->setEnabled (false);
    save->setEnabled (false);
    del->setEnabled (false);
    connect (load, SIGNAL (clicked ()), this, SLOT (slotLoad ()));
    connect (save, SIGNAL (clicked ()), this, SLOT (slotSave ()));
    connect (del, SIGNAL (clicked ()), this, SLOT (slotDelete ()));
    profileslayout->addWidget (profile, 0, 0);
    profileslayout->addMultiCellWidget (profilelist, 1, 3, 0, 0);
    profileslayout->addWidget (load, 1, 1);
    profileslayout->addWidget (save, 2, 1);
    profileslayout->addWidget (del, 3, 1);
    rightlayout->addWidget (profileframe);
    QFrame * line = new QFrame (this);
    line->setFrameShape (QFrame::HLine);
    rightlayout->addWidget (line);
    rightlayout->addWidget (label);
    rightlayout->addWidget (accesslist);
    rightlayout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
    layout->addLayout (rightlayout);
    line = new QFrame (this);
    line->setFrameShape (QFrame::VLine);
    layout->addWidget (line);
    layout->addLayout (formatlayout);
    layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

#undef ADDPROPERTY

void KMPlayerPrefBroadcastFormatPage::setSettings (const FFServerSetting & fs) {
    if (!fs.format.isEmpty ())
        format->setCurrentText (fs.format);
    audiocodec->setText (fs.audiocodec);
    audiobitrate->setText (fs.audiobitrate);
    audiosamplerate->setText (fs.audiosamplerate);
    videocodec->setText (fs.videocodec);
    videobitrate->setText (fs.videobitrate);
    quality->setText (fs.quality);
    framerate->setText (fs.framerate);
    gopsize->setText (fs.gopsize);
    moviewidth->setText (fs.width);
    movieheight->setText (fs.height);
    accesslist->setNumRows (0);
    accesslist->setNumRows (50);
    QStringList::const_iterator it = fs.acl.begin ();
    for (int i = 0; it != fs.acl.end (); ++i, ++it)
        accesslist->setItem (i, 0, new QTableItem (accesslist, QTableItem::Always, *it));
}

void KMPlayerPrefBroadcastFormatPage::getSettings (FFServerSetting & fs) {
    fs.format = format->currentText ();
    fs.audiocodec = audiocodec->text ();
    fs.audiobitrate = audiobitrate->text ();
    fs.audiosamplerate = audiosamplerate->text ();
    fs.videocodec = videocodec->text ();
    fs.videobitrate = videobitrate->text ();
    fs.quality = quality->text ();
    fs.framerate = framerate->text ();
    fs.gopsize = gopsize->text ();
    fs.width = moviewidth->text ();
    fs.height = movieheight->text ();
    fs.acl.clear ();
    for (int i = 0; i < accesslist->numRows (); ++i) {
        if (accesslist->item (i, 0) && !accesslist->item (i, 0)->text ().isEmpty ())
            fs.acl.push_back (accesslist->item (i, 0)->text ());
    }
}

void KMPlayerPrefBroadcastFormatPage::slotIndexChanged (int index) {
    slotItemHighlighted (index);
    if (index > 0 && index < (int) profiles.size ())
        setSettings (*profiles[index]);
}

void KMPlayerPrefBroadcastFormatPage::slotTextChanged (const QString & txt) {
    save->setEnabled (txt.length ());
}

void KMPlayerPrefBroadcastFormatPage::slotItemHighlighted (int index) {
    if (index < 0 || index >= (int) profiles.size ()) {
        load->setEnabled (false);
        del->setEnabled (false);
    } else {
        profile->setText (profiles[profilelist->currentItem ()]->name);
        load->setEnabled (true);
        del->setEnabled (true);
        slotTextChanged (profilelist->currentText ());
    }
}

void KMPlayerPrefBroadcastFormatPage::slotSave () {
    for (int i = 0; i < (int) profiles.size (); ++i)
        if (profiles[i]->name == profile->text ()) {
            getSettings (*profiles[i]);
            return;
        }
    FFServerSetting * fs = new FFServerSetting;
    fs->name = profile->text ();
    getSettings (*fs);
    profiles.push_back (fs);
    profilelist->insertItem (fs->name);
}

void KMPlayerPrefBroadcastFormatPage::slotLoad () {
    setSettings (*profiles[profilelist->currentItem ()]);
}

void KMPlayerPrefBroadcastFormatPage::slotDelete () {
    FFServerSettingList::iterator it = profiles.begin();
    for (int i = 0; i < profilelist->currentItem (); i++)
        ++it;
    delete *it;
    profiles.erase (it);
    profilelist->removeItem (profilelist->currentItem ());
    load->setEnabled (false);
    del->setEnabled (false);
}

KMPlayerPrefGeneralPageVCD::KMPlayerPrefGeneralPageVCD(QWidget *parent) : QFrame(parent)
{
	QVBoxLayout *layout = new QVBoxLayout (this, 5, 2);

	autoPlayVCD = new QCheckBox (i18n ("Auto play after opening a VCD"), this, 0);
	QToolTip::add(autoPlayVCD, i18n ("Start playing VCD right after opening VCD")); // i don't know about this

	QLabel *vcdDevicePathLabel = new QLabel (i18n ("VCD (CDROM) device:"), this, 0);
	vcdDevicePath = new KURLRequester ("/dev/cdrom", this, 0);
	QToolTip::add(vcdDevicePath, i18n ("Path to your CDROM/DVD device, you must have read rights to this device"));

	layout->addWidget (autoPlayVCD);
	layout->addItem (new QSpacerItem (0, 10, QSizePolicy::Minimum, QSizePolicy::Minimum));
	layout->addWidget (vcdDevicePathLabel);
	layout->addWidget (vcdDevicePath);
	layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

KMPlayerPrefGeneralPageOutput::KMPlayerPrefGeneralPageOutput(QWidget *parent, MPlayerAudioDriver * ad) : QFrame(parent)
{
    QVBoxLayout *layout = new QVBoxLayout (this, 5, 5);
    QHBoxLayout *childLayout1 = new QHBoxLayout (layout);

    videoDriver = new QComboBox(this);
    videoDriver->insertItem(VDRIVER_XV, VDRIVER_XV_INDEX);
    videoDriver->insertItem(VDRIVER_X11, VDRIVER_X11_INDEX);
    videoDriver->insertItem(VDRIVER_XVIDIX, VDRIVER_XVIDIX_INDEX);
    // by mok: remove this comment when you check if i18n fix is OK.
    //	QToolTip::add(videoDriver, i18n("Sets video driver, currently only XVideo and X11 work. Unless\nyou haven't got XVideo compatible drivers you should X11, which is much slower."));
    QToolTip::add(videoDriver, i18n("Sets video driver. Recommended is XVideo, or, if it is not supported, X11, which is slower."));
    childLayout1->addWidget(new QLabel(i18n("Video driver:"),this));
    childLayout1->addWidget(videoDriver);

    QHBoxLayout *childLayout2 = new QHBoxLayout (layout);
    audioDriver = new QComboBox(this);
    for (int i = 0; ad[i].audiodriver; i++)
        audioDriver->insertItem (ad[i].description, i);
    childLayout2->addWidget(new QLabel(i18n("Audio driver:"),this));
    childLayout2->addWidget(audioDriver);
    layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));

}

KMPlayerPrefOPPageGeneral::KMPlayerPrefOPPageGeneral(QWidget *parent)
: QFrame(parent)
{
    QVBoxLayout *layout = new QVBoxLayout (this, 5);
    layout->setAutoAdd (true);
}

KMPlayerPrefOPPagePostProc::KMPlayerPrefOPPagePostProc(QWidget *parent) : QFrame(parent)
{

	QVBoxLayout *tabLayout = new QVBoxLayout (this, 5);
	postProcessing = new QCheckBox( i18n("postProcessing"), this, 0 );
	postProcessing->setEnabled( TRUE );
	disablePPauto = new QCheckBox (i18n("disablePostProcessingAutomatically"), this, 0);

	tabLayout->addWidget( postProcessing );
	tabLayout->addWidget( disablePPauto );
	tabLayout->addItem ( new QSpacerItem( 5, 5, QSizePolicy::Minimum, QSizePolicy::Minimum ) );

	PostprocessingOptions = new QTabWidget( this, "PostprocessingOptions" );
	PostprocessingOptions->setEnabled( TRUE );
	PostprocessingOptions->setAutoMask( FALSE );
	PostprocessingOptions->setTabPosition( QTabWidget::Top );
	PostprocessingOptions->setTabShape( QTabWidget::Rounded );
	PostprocessingOptions->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)1, (QSizePolicy::SizeType)1, PostprocessingOptions->sizePolicy().hasHeightForWidth() ) );

	QWidget *presetSelectionWidget = new QWidget( PostprocessingOptions, "presetSelectionWidget" );
	QGridLayout *presetSelectionWidgetLayout = new QGridLayout( presetSelectionWidget, 1, 1, 1);

	QButtonGroup *presetSelection = new QButtonGroup(3, Qt::Vertical, presetSelectionWidget);
	presetSelection->setInsideSpacing(KDialog::spacingHint());

	defaultPreset = new QRadioButton( presetSelection, "defaultPreset" );
	defaultPreset->setChecked( TRUE );
	presetSelection->insert (defaultPreset);

	customPreset = new QRadioButton( presetSelection, "customPreset" );
	presetSelection->insert (customPreset);

	fastPreset = new QRadioButton( presetSelection, "fastPreset" );
	presetSelection->insert (fastPreset);
	presetSelection->setRadioButtonExclusive ( true);
	presetSelectionWidgetLayout->addWidget( presetSelection, 0, 0 );
	PostprocessingOptions->insertTab( presetSelectionWidget, "" );

	//
	// SECOND!!!
	//
	/* I JUST WASN'T ABLE TO GET THIS WORKING WITH QGridLayouts */

	QWidget *customFiltersWidget = new QWidget( PostprocessingOptions, "customFiltersWidget" );
	QVBoxLayout *customFiltersWidgetLayout = new QVBoxLayout( customFiltersWidget );

	QGroupBox *customFilters = new QGroupBox(0, Qt::Vertical, customFiltersWidget, "customFilters" );
	customFilters->setSizePolicy(QSizePolicy((QSizePolicy::SizeType)1, (QSizePolicy::SizeType)2));
	customFilters->setFlat(false);
	customFilters->setEnabled( FALSE );
	customFilters->setInsideSpacing(7);

	QLayout *customFiltersLayout = customFilters->layout();
	QHBoxLayout *customFiltersLayout1 = new QHBoxLayout ( customFilters->layout() );

	HzDeblockFilter = new QCheckBox( customFilters, "HzDeblockFilter" );
	HzDeblockAQuality = new QCheckBox( customFilters, "HzDeblockAQuality" );
	HzDeblockAQuality->setEnabled( FALSE );
	HzDeblockCFiltering = new QCheckBox( customFilters, "HzDeblockCFiltering" );
	HzDeblockCFiltering->setEnabled( FALSE );

	customFiltersLayout1->addWidget( HzDeblockFilter );
	customFiltersLayout1->addItem( new QSpacerItem( 0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum ) );
	customFiltersLayout1->addWidget( HzDeblockAQuality );
	customFiltersLayout1->addWidget( HzDeblockCFiltering );

	QFrame *line1 = new QFrame( customFilters, "line1" );
	line1->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)1, (QSizePolicy::SizeType)2 ) );
	line1->setFrameShape( QFrame::HLine );
	line1->setFrameShadow( QFrame::Sunken );
	customFiltersLayout->add(line1);

	QHBoxLayout *customFiltersLayout2 = new QHBoxLayout ( customFilters->layout() );

	VtDeblockFilter = new QCheckBox( customFilters, "VtDeblockFilter" );
	VtDeblockAQuality = new QCheckBox( customFilters, "VtDeblockAQuality" );
	VtDeblockAQuality->setEnabled( FALSE );
	VtDeblockCFiltering = new QCheckBox( customFilters, "VtDeblockCFiltering" );
	VtDeblockCFiltering->setEnabled( FALSE );

	customFiltersLayout2->addWidget( VtDeblockFilter );
	customFiltersLayout2->addItem( new QSpacerItem( 0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum ) );
	customFiltersLayout2->addWidget( VtDeblockAQuality );
	customFiltersLayout2->addWidget( VtDeblockCFiltering );

	QFrame *line2 = new QFrame( customFilters, "line2" );

	line2->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)1, (QSizePolicy::SizeType)2 ) );
	line2->setFrameShape( QFrame::HLine );
	line2->setFrameShadow( QFrame::Sunken );
	customFiltersLayout->add(line2);

	QHBoxLayout *customFiltersLayout3  = new QHBoxLayout ( customFilters->layout() );

	DeringFilter = new QCheckBox( customFilters, "DeringFilter" );
	DeringAQuality = new QCheckBox( customFilters, "DeringAQuality" );
	DeringAQuality->setEnabled( FALSE );
	DeringCFiltering = new QCheckBox( customFilters, "DeringCFiltering" );
	DeringCFiltering->setEnabled( FALSE );

	customFiltersLayout3->addWidget( DeringFilter );
	customFiltersLayout3->addItem( new QSpacerItem( 0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum ) );
	customFiltersLayout3->addWidget( DeringAQuality );
	customFiltersLayout3->addWidget( DeringCFiltering );


	QFrame *line3 = new QFrame( customFilters, "line3" );

	line3->setFrameShape( QFrame::HLine );
	line3->setFrameShadow( QFrame::Sunken );
	line3->setFrameShape( QFrame::HLine );

	customFiltersLayout->add(line3);

	QHBoxLayout *customFiltersLayout4 = new QHBoxLayout (customFilters->layout());

	AutolevelsFilter = new QCheckBox( customFilters, "AutolevelsFilter" );
	AutolevelsFullrange = new QCheckBox( customFilters, "AutolevelsFullrange" );
	AutolevelsFullrange->setEnabled( FALSE );

	customFiltersLayout4->addWidget(AutolevelsFilter);
	customFiltersLayout4->addItem(new QSpacerItem( 0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum ));
	customFiltersLayout4->addWidget(AutolevelsFullrange);

	QHBoxLayout *customFiltersLayout5 = new QHBoxLayout (customFilters->layout());

	TmpNoiseFilter = new QCheckBox( customFilters, "TmpNoiseFilter" );
/*	TmpNoiseSlider = new QSlider( customFilters, "TmpNoiseSlider" );
	TmpNoiseSlider->setEnabled( FALSE );
	TmpNoiseSlider->setMinValue( 1 );
	TmpNoiseSlider->setMaxValue( 3 );
	TmpNoiseSlider->setValue( 1 );
	TmpNoiseSlider->setOrientation( QSlider::Horizontal );
	TmpNoiseSlider->setTickmarks( QSlider::Left );
	TmpNoiseSlider->setTickInterval( 1 );
	TmpNoiseSlider->setSizePolicy(QSizePolicy( (QSizePolicy::SizeType)1, (QSizePolicy::SizeType)1));*/

	/*customFiltersLayout->addWidget(TmpNoiseFilter,7,0);
	customFiltersLayout->addWidget(TmpNoiseSlider,7,2);*/
	customFiltersLayout5->addWidget(TmpNoiseFilter);
	customFiltersLayout5->addItem(new QSpacerItem( 0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum ));
	//customFiltersLayout5->addWidget(TmpNoiseSlider);
	customFiltersWidgetLayout->addWidget( customFilters );
	PostprocessingOptions->insertTab( customFiltersWidget, "" );
	//
	//THIRD!!!
	//
	QWidget *deintSelectionWidget = new QWidget( PostprocessingOptions, "deintSelectionWidget" );
	QVBoxLayout *deintSelectionWidgetLayout = new QVBoxLayout( deintSelectionWidget);
	QButtonGroup *deinterlacingGroup = new QButtonGroup(5, Qt::Vertical, deintSelectionWidget, "deinterlacingGroup" );

	LinBlendDeinterlacer = new QCheckBox( deinterlacingGroup, "LinBlendDeinterlacer" );
	LinIntDeinterlacer = new QCheckBox( deinterlacingGroup, "LinIntDeinterlacer" );
	CubicIntDeinterlacer = new QCheckBox( deinterlacingGroup, "CubicIntDeinterlacer" );
	MedianDeinterlacer = new QCheckBox( deinterlacingGroup, "MedianDeinterlacer" );
	FfmpegDeinterlacer = new QCheckBox( deinterlacingGroup, "FfmpegDeinterlacer" );

	deinterlacingGroup->insert( LinBlendDeinterlacer );

	deinterlacingGroup->insert( LinIntDeinterlacer );

	deinterlacingGroup->insert( CubicIntDeinterlacer );

	deinterlacingGroup->insert( MedianDeinterlacer );

	deinterlacingGroup->insert( FfmpegDeinterlacer );


	deintSelectionWidgetLayout->addWidget( deinterlacingGroup, 0, 0 );

	PostprocessingOptions->insertTab( deintSelectionWidget, "" );

	tabLayout->addWidget( PostprocessingOptions/*, 1, 0*/ );

	PostprocessingOptions->setEnabled(false);
	connect( customPreset, SIGNAL (toggled(bool) ), customFilters, SLOT(setEnabled(bool)));
	connect( postProcessing, SIGNAL( toggled(bool) ), PostprocessingOptions, SLOT( setEnabled(bool) ) );
	connect( HzDeblockFilter, SIGNAL( toggled(bool) ), HzDeblockAQuality, SLOT( setEnabled(bool) ) );
	connect( HzDeblockFilter, SIGNAL( toggled(bool) ), HzDeblockCFiltering, SLOT( setEnabled(bool) ) );
	connect( VtDeblockFilter, SIGNAL( toggled(bool) ), VtDeblockCFiltering, SLOT( setEnabled(bool) ) );
	connect( VtDeblockFilter, SIGNAL( toggled(bool) ), VtDeblockAQuality, SLOT( setEnabled(bool) ) );
	connect( DeringFilter, SIGNAL( toggled(bool) ), DeringAQuality, SLOT( setEnabled(bool) ) );
	connect( DeringFilter, SIGNAL( toggled(bool) ), DeringCFiltering, SLOT( setEnabled(bool) ) );
	//connect( TmpNoiseFilter, SIGNAL( toggled(bool) ), TmpNoiseSlider, SLOT( setEnabled(bool) ) );

	connect( AutolevelsFilter, SIGNAL( toggled(bool) ), AutolevelsFullrange, SLOT( setEnabled(bool) ) );

	postProcessing->setText( i18n( "Enable use of postprocessing filters" ) );
	disablePPauto->setText( i18n( "Disable use of postprocessing when watching TV/DVD" ) );
	defaultPreset->setText( i18n( "Default" ) );
	QToolTip::add( defaultPreset, i18n( "Enable mplayer's default postprocessing filters" ) );
	customPreset->setText( i18n( "Custom" ) );
	QToolTip::add( customPreset, i18n( "Enable custom postprocessing filters (See: Custom preset -tab)" ) );
	fastPreset->setText( i18n( "Fast" ) );
	QToolTip::add( fastPreset, i18n( "Enable mplayer's fast postprocessing filters" ) );
	PostprocessingOptions->changeTab( presetSelectionWidget, i18n( "General" ) );
	customFilters->setTitle( QString::null );
	HzDeblockFilter->setText( i18n( "Horizontal deblocking" ) );
	VtDeblockFilter->setText( i18n( "Vertical deblocking" ) );
	DeringFilter->setText( i18n( "Dering filter" ) );
	HzDeblockAQuality->setText( i18n( "Auto quality" ) );
	QToolTip::add( HzDeblockAQuality, i18n( "Filter is used if there's enough CPU" ) );
	VtDeblockAQuality->setText( i18n( "Auto quality" ) );
	QToolTip::add( VtDeblockAQuality, i18n( "Filter is used if there's enough CPU" ) );
	DeringAQuality->setText( i18n( "Auto quality" ) );
	QToolTip::add( DeringAQuality, i18n( "Filter is used if there's enough CPU" ) );
	//QToolTip::add( TmpNoiseSlider, i18n( "Strength of the noise reducer" ) );
	AutolevelsFilter->setText( i18n( "Auto brightness/contrast" ) );
	AutolevelsFullrange->setText( i18n( "Stretch luminance to full range" ) );
	QToolTip::add( AutolevelsFullrange, i18n( "Stretches luminance to full range (0..255)" ) );
	HzDeblockCFiltering->setText( i18n( "Chrominance filtering" ) );
	VtDeblockCFiltering->setText( i18n( "Chrominance filtering" ) );
	DeringCFiltering->setText( i18n( "Chrominance filtering" ) );
	TmpNoiseFilter->setText( i18n( "Temporal noise reducer:" ) );
	PostprocessingOptions->changeTab( customFiltersWidget, i18n( "Custom Preset" ) );
	deinterlacingGroup->setTitle( QString::null );
	LinBlendDeinterlacer->setText( i18n( "Linear blend deinterlacer" ) );
	CubicIntDeinterlacer->setText( i18n( "Cubic interpolating deinterlacer" ) );
	LinIntDeinterlacer->setText( i18n( "Linear interpolating deinterlacer" ) );
	MedianDeinterlacer->setText( i18n( "Median deinterlacer" ) );
	FfmpegDeinterlacer->setText( i18n( "FFmpeg deinterlacer" ) );
	PostprocessingOptions->changeTab( deintSelectionWidget, i18n( "Deinterlacing" ) );
	PostprocessingOptions->adjustSize();
}

KMPlayerPrefGeneralPageAdvanced::KMPlayerPrefGeneralPageAdvanced(QWidget *parent) : QFrame(parent)
{
    QVBoxLayout *layout = new QVBoxLayout (this, 0, 5);
    QGroupBox *realGroupBox = new QGroupBox (i18n ("Pattern Matching"), this, "realGroupBox");

    realGroupBox->setFlat (false);
    realGroupBox->setInsideMargin (7);
    QVBoxLayout *realGroupBoxLayout = new QVBoxLayout (realGroupBox->layout());

    QGridLayout *groupBoxLayout = new QGridLayout (realGroupBoxLayout, 1, 1, 2);

    QLabel *langPattLabel = new QLabel (i18n("DVD language pattern:"), realGroupBox, 0);
    dvdLangPattern = new QLineEdit (realGroupBox);
    groupBoxLayout->addWidget(langPattLabel,0,0);
    groupBoxLayout->addWidget(dvdLangPattern,0,2);

    QLabel *titlePattLabel = new QLabel (i18n("DVD titles pattern:"), realGroupBox, 0);
    dvdTitlePattern = new QLineEdit (realGroupBox);
    groupBoxLayout->addWidget(titlePattLabel,1,0);
    groupBoxLayout->addWidget(dvdTitlePattern,1,2);

    QLabel *subPattLabel = new QLabel (i18n("DVD subtitle pattern:"), realGroupBox, 0);
    dvdSubPattern = new QLineEdit (realGroupBox);
    groupBoxLayout->addWidget(subPattLabel,2,0);
    groupBoxLayout->addWidget(dvdSubPattern,2,2);

    QLabel *chapPattLabel = new QLabel (i18n("DVD chapters pattern:"), realGroupBox, 0);
    dvdChapPattern = new QLineEdit (realGroupBox);
    groupBoxLayout->addWidget(chapPattLabel,3,0);
    groupBoxLayout->addWidget(dvdChapPattern,3,2);

    QLabel *trackPattLabel = new QLabel (i18n("VCD track pattern:"), realGroupBox, 0);
    vcdTrackPattern = new QLineEdit (realGroupBox);
    groupBoxLayout->addWidget(trackPattLabel,4,0);
    groupBoxLayout->addWidget(vcdTrackPattern,4,2);

    QLabel *sizePattLabel = new QLabel (i18n("Size pattern:"), realGroupBox, 0);
    sizePattern = new QLineEdit (realGroupBox);
    groupBoxLayout->addWidget(sizePattLabel,5,0);
    groupBoxLayout->addWidget(sizePattern,5,2);

    QLabel *cachePattLabel = new QLabel (i18n("Cache pattern:"), realGroupBox, 0);
    cachePattern = new QLineEdit (realGroupBox);
    groupBoxLayout->addWidget(cachePattLabel,6,0);
    groupBoxLayout->addWidget(cachePattern,6,2);

    QLabel *indexPattLabel = new QLabel (i18n("Index pattern:"), realGroupBox, 0);
    indexPattern = new QLineEdit (realGroupBox);
    groupBoxLayout->addWidget(indexPattLabel,7,0);
    groupBoxLayout->addWidget(indexPattern,7,2);

    QLabel *label = new QLabel (i18n("Start pattern:"), realGroupBox, 0);
    startPattern = new QLineEdit (realGroupBox);
    groupBoxLayout->addWidget(label,8,0);
    groupBoxLayout->addWidget(startPattern,8,2);

    label = new QLabel (i18n ("Reference URL pattern:"), realGroupBox, 0);
    referenceURLPattern = new QLineEdit (realGroupBox);
    groupBoxLayout->addWidget (label, 9, 0);
    groupBoxLayout->addWidget (referenceURLPattern, 9, 2);

    label = new QLabel (i18n ("Reference pattern:"), realGroupBox, 0);
    referencePattern = new QLineEdit (realGroupBox);
    groupBoxLayout->addWidget (label, 10, 0);
    groupBoxLayout->addWidget (referencePattern, 10, 2);

    groupBoxLayout->addColSpacing(1, 11);
    layout->addWidget(realGroupBox);


    layout->addWidget(new QLabel (i18n("Additional command line arguments:"),this));
    additionalArguments = new QLineEdit(this);
    layout->addWidget(additionalArguments);


    QHBoxLayout *addLayout2 = new QHBoxLayout (layout);
    addLayout2->addWidget(new QLabel (i18n("Cache size:"),this));
    cacheSize = new QSpinBox (0, 32767, 32, this);
    addLayout2->addWidget(cacheSize);
    addLayout2->addWidget(new QLabel (i18n("kB"),this));

    layout->addItem(new QSpacerItem(1,1, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

void KMPlayerPreferences::confirmDefaults() {
	switch( QMessageBox::warning( this, "KMPlayer",
        i18n("You are about to have all your settings overwritten with defaults.\nPlease confirm.\n"),
        i18n("Ok"), i18n("Cancel"), QString::null, 0, 1 ) ){
    		case 0:	KMPlayerPreferences::setDefaults();
        		break;
    		case 1:	break;
	}

}

void KMPlayerPreferences::setDefaults() {
	m_GeneralPageGeneral->keepSizeRatio->setChecked(true);
	m_GeneralPageGeneral->showConsoleOutput->setChecked(false);
	m_GeneralPageGeneral->loop->setChecked(false);
	m_GeneralPageGeneral->showControlButtons->setChecked(true);
	m_GeneralPageGeneral->autoHideControlButtons->setChecked(false);
	m_GeneralPageGeneral->showPositionSlider->setChecked(true);
	m_GeneralPageGeneral->seekTime->setValue(10);

	m_GeneralPageDVD->autoPlayDVD->setChecked(true);
	m_GeneralPageDVD->dvdDevicePath->lineEdit()->setText("/dev/dvd");

	m_GeneralPageVCD->autoPlayVCD->setChecked(true);
	m_GeneralPageVCD->vcdDevicePath->lineEdit()->setText("/dev/cdrom");

	m_GeneralPageOutput->videoDriver->setCurrentItem(VDRIVER_XV_INDEX);
	m_GeneralPageOutput->audioDriver->setCurrentItem(0);

	m_GeneralPageAdvanced->dvdLangPattern->setText("\\[open].*audio.*language: ([A-Za-z]+).*aid.*[^0-9]([0-9]+)");
	m_GeneralPageAdvanced->dvdTitlePattern->setText("There are ([0-9]+) titles");
	m_GeneralPageAdvanced->dvdSubPattern->setText("\\[open].*subtitle.*[^0-9]([0-9]+).*language: ([A-Za-z]+)");
	m_GeneralPageAdvanced->dvdChapPattern->setText("There are ([0-9]+) chapters");
	m_GeneralPageAdvanced->vcdTrackPattern->setText("track ([0-9]+):");
	m_GeneralPageAdvanced->sizePattern->setText("VO:.*[^0-9]([0-9]+)x([0-9]+)");
	m_GeneralPageAdvanced->cachePattern->setText("Cache fill:[^0-9]*([0-9\\.]+)%");
	m_GeneralPageAdvanced->startPattern->setText("Start[^ ]* play");
	m_GeneralPageAdvanced->additionalArguments->setText("");
	m_GeneralPageAdvanced->cacheSize->setValue(256);

	m_OPPagePostproc->postProcessing->setChecked(false);
	m_OPPagePostproc->disablePPauto->setChecked(true);

	m_OPPagePostproc->defaultPreset->setChecked(true);

	m_OPPagePostproc->LinBlendDeinterlacer->setChecked(false);
	m_OPPagePostproc->LinIntDeinterlacer->setChecked(false);
	m_OPPagePostproc->CubicIntDeinterlacer->setChecked(false);
	m_OPPagePostproc->MedianDeinterlacer->setChecked(false);
	m_OPPagePostproc->FfmpegDeinterlacer->setChecked(false);

}
#include "pref.moc"
