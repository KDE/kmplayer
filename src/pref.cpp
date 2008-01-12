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
 * the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#undef Always

#include <qlayout.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qcheckbox.h>
#include <qstringlist.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <Q3GroupBox>
#include <qwhatsthis.h>
#include <qtabwidget.h>
#include <qslider.h>
#include <Q3ButtonGroup>
#include <qspinbox.h>
#include <qmessagebox.h>
#include <qmap.h>
#include <qtimer.h>
#include <qfont.h>
#include <Q3ListBox>
#include <QAbstractButton>

#include <klocale.h>
#include <kdebug.h>
#include <kfiledialog.h>
#include <kmessagebox.h>
#include <klineedit.h>
#include <kiconloader.h>
#include <kdeversion.h>
#include <kcombobox.h>
#include <kcolorbutton.h>
#include <kurlrequester.h>
#include <kfontdialog.h>
#include "pref.h"
#include "kmplayerpartbase.h"
#include "kmplayerprocess.h"
#include "mediaobject.h"
#include "kmplayerconfig.h"

using namespace KMPlayer;

KDE_NO_CDTOR_EXPORT Preferences::Preferences(PartBase * player, Settings * settings)
: KPageDialog (player->view ())
{
    setFaceType (KPageDialog::List);
    setCaption (i18n ("Preferences"));
    setButtons (KDialog::Ok | KDialog::Cancel | KDialog::Apply);
    setDefaultButton (KDialog::Ok);

    KVBox *page = new KVBox (this);
    KPageWidgetItem *item = addPage (page, i18n ("General Options"));
    item->setIcon (KIcon ("kmplayer"));
    QTabWidget *tab = new QTabWidget (page);
    m_GeneralPageGeneral = new PrefGeneralPageGeneral (tab, settings);
    tab->insertTab (m_GeneralPageGeneral, i18n("General"));
    m_GeneralPageLooks = new PrefGeneralPageLooks (tab, settings);
    tab->insertTab (m_GeneralPageLooks, i18n("Looks"));
    m_GeneralPageOutput = new PrefGeneralPageOutput
        (tab, settings->audiodrivers, settings->videodrivers);
    tab->insertTab (m_GeneralPageOutput, i18n("Output"));
    entries.insert (i18n("General Options"), tab);

    page = new KVBox (this);
    m_url_item = addPage (page, i18n ("Source"));
    m_url_item->setIcon (KIcon ("document-import"));
    tab = new QTabWidget (page);
    m_SourcePageURL = new PrefSourcePageURL (tab);
    tab->insertTab (m_SourcePageURL, i18n ("URL"));
    entries.insert (i18n("Source"), tab);

    page = new KVBox (this);
    m_record_item = addPage (page, i18n ("Recording"));
    m_record_item->setIcon (KIcon ("folder-video"));
    tab = new QTabWidget (page);

    int recorders_count = 3;
    m_MEncoderPage = new PrefMEncoderPage (tab, player);
    tab->insertTab (m_MEncoderPage, i18n ("MEncoder"));
    recorders = m_MEncoderPage;

    m_FFMpegPage = new PrefFFMpegPage (tab, player);
    tab->insertTab (m_FFMpegPage, i18n ("FFMpeg"));
    m_MEncoderPage->next = m_FFMpegPage;

    m_MPlayerDumpstreamPage = new PrefMPlayerDumpstreamPage (tab, player);
    // tab->insertTab (m_MPlayerDumpstreamPage, i18n ("MPlayer -dumpstream"));
    m_FFMpegPage->next = m_MPlayerDumpstreamPage;
#ifdef KMPLAYER_WITH_XINE
    recorders_count = 4;
    m_XinePage = new PrefXinePage (tab, player);
    // tab->insertTab (m_XinePage, i18n ("Xine"));
    m_MPlayerDumpstreamPage->next = m_XinePage;
#endif
    m_RecordPage = new PrefRecordPage (tab, player, recorders, recorders_count);
    tab->insertTab (m_RecordPage, i18n ("General"), 0);
    tab->setCurrentPage (0);
    entries.insert (i18n("Recording"), tab);

    page = new KVBox (this);
    item = addPage (page, i18n ("Output Plugins"));
    item->setIcon (KIcon ("folder-image"));
    tab = new QTabWidget (page);
    m_OPPagePostproc = new PrefOPPagePostProc (tab);
    tab->insertTab (m_OPPagePostproc, i18n ("Postprocessing"));
    entries.insert (i18n("Postprocessing"), tab);

    for (PreferencesPage * p = settings->pagelist; p; p = p->next)
        addPrefPage (p);

    connect (this, SIGNAL (defaultClicked ()), SLOT (confirmDefaults ()));
}

KDE_NO_EXPORT void Preferences::setPage (const char * name) {
    KPageWidgetItem *item = NULL;
    if (!strcmp (name, "RecordPage"))
        item = m_record_item;
    else if (!strcmp (name, "URLPage"))
        item = m_url_item;
    if (item) {
        setCurrentPage (item);
        KVBox *page = findChild <KVBox *> (name);
        if (!page)
            return;
        QWidget * w = page->parentWidget ();
        while (w && !qobject_cast <QTabWidget *> (w))
            w = w->parentWidget ();
        if (!w)
            return;
        QTabWidget *t = static_cast <QTabWidget*> (w);
        t->setCurrentPage (t->indexOf(page));
    }
}

KDE_NO_EXPORT void Preferences::addPrefPage (PreferencesPage * page) {
    QString item, subitem, icon;
    KPageWidgetItem *witem;
    QTabWidget *tab;
    page->prefLocation (item, icon, subitem);
    if (item.isEmpty ())
        return;
    QMap<QString, QTabWidget *>::iterator en_it = entries.find (item);
    if (en_it == entries.end ()) {
        KVBox *page = new KVBox (this);
        witem = addPage (page, item);
        witem->setIcon (KIcon (icon));
        tab = new QTabWidget (page);
        entries.insert (item, tab);
    } else
        tab = en_it.data ();
    QFrame *frame = page->prefPage (tab);
    tab->insertTab (frame, subitem);
}

KDE_NO_EXPORT void Preferences::removePrefPage(PreferencesPage * page) {
    QString item, subitem, icon;
    page->prefLocation (item, icon, subitem);
    if (item.isEmpty ())
        return;
    QMap<QString, QTabWidget *>::iterator en_it = entries.find (item);
    if (en_it == entries.end ())
        return;
    QTabWidget * tab = en_it.data ();
    for (int i = 0; i < tab->count (); i++)
        if (tab->label (i) == subitem) {
            QWidget * w = tab->page (i);
            tab->removePage (w);
            delete w;
            break;
        }
    if (!tab->count ()) {
        QWidget * w = tab->parentWidget ();
        while (w && !w->inherits ("QFrame"))
            w = w->parentWidget ();
        delete w;
        entries.erase (en_it);
    }
}

KDE_NO_CDTOR_EXPORT Preferences::~Preferences() {
}

KDE_NO_CDTOR_EXPORT PrefGeneralPageGeneral::PrefGeneralPageGeneral(QWidget *parent, Settings *)
: KVBox (parent)
{
    setMargin (5);
    setSpacing (2);

    Q3GroupBox *windowbox = new Q3GroupBox(1, Qt::Vertical, i18n("Window"), this);
    QWidget * wbox = new QWidget (windowbox);
    QWidget * bbox = new QWidget (wbox);
    QGridLayout * gridlayout = new QGridLayout (bbox, 2, 2);
    keepSizeRatio = new QCheckBox (i18n ("Keep size ratio"), bbox, 0);
    QWhatsThis::add(keepSizeRatio, i18n("When checked, movie will keep its aspect ratio\nwhen window is resized"));
    dockSysTray = new QCheckBox (i18n ("Dock in system tray"), bbox, 0);
    QWhatsThis::add (dockSysTray, i18n ("When checked, an icon of KMPlayer will be added to the system tray.\nWhen clicked it will hide KMPlayer's main window and removing KMPlayer's taskbar button."));
    autoResize = new QCheckBox (i18n ("Auto resize to video sizes"), bbox);
    QWhatsThis::add (autoResize, i18n("When checked, KMPlayer will resize to movie sizes\nwhen video starts"));
    gridlayout->addWidget (keepSizeRatio, 0, 0);
    gridlayout->addWidget (dockSysTray, 1, 0);
    gridlayout->addWidget (autoResize, 0, 1);
    sizesChoice = new Q3ButtonGroup (2, Qt::Vertical, wbox);
    new QRadioButton (i18n("Remember window size on exit"), sizesChoice);
    new QRadioButton (i18n("Always start with fixed size"), sizesChoice);
    QVBoxLayout * vbox = new QVBoxLayout (wbox, 2, 2);
    vbox->addWidget (bbox);
    vbox->addWidget (sizesChoice);

    Q3GroupBox *playbox =new Q3GroupBox(4, Qt::Vertical,i18n("Playing"),this);
    loop = new QCheckBox (i18n("Loop"), playbox);
    QWhatsThis::add(loop, i18n("Makes current movie loop"));
    framedrop = new QCheckBox (i18n ("Allow framedrops"), playbox);
    QWhatsThis::add (framedrop, i18n ("Allow dropping frames for better audio and video synchronization"));
    adjustvolume = new QCheckBox(i18n("Auto set volume on start"), playbox);
    QWhatsThis::add (adjustvolume, i18n ("When a new source is selected, the volume will be set according the volume control"));
    adjustcolors = new QCheckBox(i18n("Auto set colors on start"), playbox);
    QWhatsThis::add (adjustcolors, i18n ("When a movie starts, the colors will be set according the sliders for colors"));

    Q3GroupBox * gbox =new Q3GroupBox (1, Qt::Vertical, i18n("Control Panel"), this);
    bbox =new QWidget (gbox);
    //QGroupBox * bbox = gbox;
    gridlayout = new QGridLayout (bbox, 3, 2);
    showConfigButton = new QCheckBox(i18n("Show config button"), bbox);
    QWhatsThis::add (showConfigButton, i18n ("Add a button that will popup a config menu"));
    showPlaylistButton = new QCheckBox(i18n("Show playlist button"), bbox);
    QWhatsThis::add (showPlaylistButton, i18n ("Add a playlist button to the control buttons"));
    showRecordButton = new QCheckBox(i18n("Show record button"), bbox);
    QWhatsThis::add (showRecordButton, i18n ("Add a record button to the control buttons"));
    showBroadcastButton = new QCheckBox (i18n ("Show broadcast button"), bbox);
    QWhatsThis::add (showBroadcastButton, i18n ("Add a broadcast button to the control buttons"));
    gridlayout->addWidget (showConfigButton, 0, 0);
    gridlayout->addWidget (showPlaylistButton, 0, 1);
    gridlayout->addWidget (showRecordButton, 1, 0);
    gridlayout->addWidget (showBroadcastButton, 1, 1);
    //QWidget *seekingWidget = new QWidget (bbox);
    QHBoxLayout *seekLayout = new QHBoxLayout (bbox);
    seekLayout->addWidget(new QLabel(i18n("Forward/backward seek time:"),bbox));
    seekLayout->addItem(new QSpacerItem(0,0,QSizePolicy::Minimum, QSizePolicy::Minimum));
    seekTime = new QSpinBox(1, 600, 1, bbox);
    seekLayout->addWidget(seekTime);
    seekLayout->addItem(new QSpacerItem(0,0,QSizePolicy::Minimum, QSizePolicy::Minimum));
    gridlayout->addMultiCellLayout (seekLayout, 2, 2, 0, 1);

    layout()->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

KDE_NO_CDTOR_EXPORT PrefGeneralPageLooks::PrefGeneralPageLooks (QWidget *parent, Settings * settings)
 : KVBox (parent), colors (settings->colors), fonts (settings->fonts) {
    setMargin (5);
    setSpacing (2);

    Q3GroupBox *colorbox= new Q3GroupBox(2, Qt::Horizontal, i18n("Colors"), this);
    colorscombo = new QComboBox (colorbox);
    for (int i = 0; i < int (ColorSetting::last_target); i++)
        colorscombo->insertItem (colors[i].title);
    colorscombo->setCurrentItem (0);
    connect (colorscombo, SIGNAL (activated (int)),
            this, SLOT (colorItemChanged(int)));
    colorbutton = new KColorButton (colorbox);
    colorbutton->setColor (colors[0].color);
    connect (colorbutton, SIGNAL (changed (const QColor &)),
            this, SLOT (colorCanged (const QColor &)));

    Q3GroupBox *fontbox = new Q3GroupBox (2,Qt::Horizontal, i18n ("Fonts"), this);
    fontscombo = new QComboBox (fontbox);
    for (int i = 0; i < int (FontSetting::last_target); i++)
        fontscombo->insertItem (fonts[i].title);
    fontscombo->setCurrentItem (0);
    connect (fontscombo, SIGNAL (activated (int)),
            this, SLOT (fontItemChanged(int)));
    fontbutton = new QPushButton (i18n ("AaBbCc"), fontbox);
    fontbutton->setFlat (true);
    fontbutton->setFont (fonts[0].font);
    connect (fontbutton, SIGNAL (clicked ()), this, SLOT (fontClicked ()));

    layout()->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

KDE_NO_EXPORT void PrefGeneralPageLooks::colorItemChanged (int c) {
    if (c < int (ColorSetting::last_target))
        colorbutton->setColor (colors[c].newcolor);
}

KDE_NO_EXPORT void PrefGeneralPageLooks::colorCanged (const QColor & c) {
    if (colorscombo->currentItem () < int (ColorSetting::last_target))
        colors[colorscombo->currentItem ()].newcolor = c;
}

KDE_NO_EXPORT void PrefGeneralPageLooks::fontItemChanged (int f) {
    if (f < int (FontSetting::last_target))
        fontbutton->setFont (fonts[f].newfont);
}

KDE_NO_EXPORT void PrefGeneralPageLooks::fontClicked () {
    if (fontscombo->currentItem () < int (FontSetting::last_target)) {
        QFont myfont = fonts [fontscombo->currentItem ()].newfont;
        int res = KFontDialog::getFont (myfont, false, this);
        if (res == KFontDialog::Accepted) {
            fonts [fontscombo->currentItem ()].newfont = myfont;
            fontbutton->setFont (myfont);
        }
    }
}

KDE_NO_CDTOR_EXPORT PrefSourcePageURL::PrefSourcePageURL (QWidget *parent)
: KVBox (parent)
{
    setObjectName ("URLPage");
    setMargin (5);
    setSpacing (2);

    QHBoxLayout * urllayout = new QHBoxLayout ();
    QHBoxLayout * sub_urllayout = new QHBoxLayout ();
    QLabel *urlLabel = new QLabel (i18n ("Location:"), this, 0);
    urllist = new KComboBox (true, this);
    urllist->setMaxCount (20);
    urllist->setDuplicatesEnabled (false); // not that it helps much :(
    url = new KUrlRequester (urllist, this);
    QWhatsThis::add (url, i18n ("Location of the playable item"));
    //url->setShowLocalProtocol (true);
    url->setSizePolicy (QSizePolicy (QSizePolicy::Expanding, QSizePolicy::Preferred));
    QLabel *sub_urlLabel = new QLabel (i18n ("Sub title:"), this, 0);
    sub_urllist = new KComboBox (true, this);
    sub_urllist->setMaxCount (20);
    sub_urllist->setDuplicatesEnabled (false); // not that it helps much :(
    sub_url = new KUrlRequester (sub_urllist, this);
    QWhatsThis::add (sub_url, i18n ("Optional location of a file containing the subtitles of the URL above"));
    sub_url->setSizePolicy (QSizePolicy (QSizePolicy::Expanding, QSizePolicy::Preferred));
    backend = new Q3ListBox (this);
    clicktoplay = new QCheckBox (i18n ("Enable 'Click to Play' support"), this);
    QWhatsThis::add (clicktoplay, i18n ("When enabled, all embedded movies will start with a image that needs to be clicked to start the video playback"));
    urllayout->addWidget (urlLabel);
    urllayout->addWidget (url);
    static_cast <QBoxLayout *>(layout())->addLayout (urllayout);
    sub_urllayout->addWidget (sub_urlLabel);
    sub_urllayout->addWidget (sub_url);
    static_cast <QBoxLayout *>(layout())->addLayout (sub_urllayout);
    layout()->addItem (new QSpacerItem (0, 10, QSizePolicy::Minimum, QSizePolicy::Minimum));
    QGridLayout * gridlayout = new QGridLayout (2, 2);
    QLabel *backendLabel = new QLabel (i18n ("Use movie player:"), this, 0);
    //QWhatsThis::add (allowhref, i18n ("Explain this in a few lines"));
    gridlayout->addWidget (backendLabel, 0, 0);
    gridlayout->addWidget (backend, 1, 0);
    gridlayout->addMultiCell (new QSpacerItem (0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum), 0, 1, 1, 1);
    Q3GroupBox *cbox = new Q3GroupBox(1, Qt::Vertical, i18n("Network bandwidth"), this);
    QWidget * wbox = new QWidget (cbox);
    QGridLayout * bitratelayout = new QGridLayout (wbox, 2, 3, 5);
    prefBitRate = new QLineEdit (wbox);
    QWhatsThis::add (prefBitRate, i18n("Sometimes it is possible to choose between various streams given a particular bitrate.\nThis option sets how much bandwidth you would prefer to allocate to video."));
    maxBitRate = new QLineEdit (wbox);
    QWhatsThis::add (maxBitRate, i18n("Sometimes it is possible to choose between various streams given a particular bitrate.\nThis option sets the maximum bandwidth you have available for video."));
    bitratelayout->addWidget(new QLabel(i18n("Preferred bitrate:"), wbox), 0, 0);
    bitratelayout->addWidget (prefBitRate, 0, 1);
    bitratelayout->addWidget (new QLabel (i18n ("kbit/s"), wbox), 0, 2);
    bitratelayout->addWidget (new QLabel(i18n("Maximum bitrate:"), wbox), 1, 0);
    bitratelayout->addWidget (maxBitRate, 1, 1);
    bitratelayout->addWidget (new QLabel (i18n ("kbit/s"), wbox), 1, 2);
    static_cast <QBoxLayout *>(layout())->addLayout (gridlayout);
    layout()->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
    connect (urllist, SIGNAL(textChanged (const QString &)),
             this, SLOT (slotTextChanged (const QString &)));
    connect (sub_urllist, SIGNAL(textChanged (const QString &)),
             this, SLOT (slotTextChanged (const QString &)));
}

KDE_NO_EXPORT void PrefSourcePageURL::slotBrowse () {
}

KDE_NO_EXPORT void PrefSourcePageURL::slotTextChanged (const QString &) {
    changed = true;
}

KDE_NO_CDTOR_EXPORT PrefRecordPage::PrefRecordPage (QWidget *parent,
        PartBase * player, RecorderPage * rl, int rec_len)
 : KVBox (player->view ()),
   m_player (player),
   m_recorders (rl),
   m_recorders_length (rec_len),
   rec_timer (0) {
    setObjectName ("RecordPage");
    setMargin (5);
    setSpacing (2);

    layout()->addItem(new QSpacerItem (5, 0, QSizePolicy::Minimum, QSizePolicy::Minimum));

    QHBoxLayout * urllayout = new QHBoxLayout ();
    QLabel *urlLabel = new QLabel (i18n ("Output file:"), this);
    url = new KUrlRequester (this);
    urllayout->addWidget (urlLabel);
    urllayout->addWidget (url);
    static_cast <QBoxLayout *>(layout())->addLayout (urllayout);

    layout()->addItem (new QSpacerItem (5, 0, QSizePolicy::Minimum, QSizePolicy::Minimum));
    recordButton = new QPushButton (i18n ("Start &Recording"), this);
    connect (recordButton, SIGNAL (clicked ()), this, SLOT (slotRecord ()));
    QHBoxLayout *buttonlayout = new QHBoxLayout;
    buttonlayout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum));
    buttonlayout->addWidget (recordButton);
    source = new QLabel (i18n ("Current source: ") +
           (m_player->source () ? m_player->source ()->prettyName () : QString ()), this);
    recorder = new Q3ButtonGroup (m_recorders_length, Qt::Vertical, i18n ("Recorder"), this);
    for (RecorderPage * p = m_recorders; p; p = p->next)
        new QRadioButton (p->name (), recorder);
    recorder->setButton(0); // for now

    layout()->addItem(new QSpacerItem (5, 0, QSizePolicy::Minimum, QSizePolicy::Minimum));

    replay = new Q3ButtonGroup (4, Qt::Vertical, i18n ("Auto Playback"), this);
    new QRadioButton (i18n ("&No"), replay);
    new QRadioButton (i18n ("&When recording finished"), replay);
    new QRadioButton (i18n ("A&fter"), replay);
    QWidget * customreplay = new QWidget (replay);
    replaytime = new QLineEdit (customreplay);
    QHBoxLayout *replaylayout = new QHBoxLayout (customreplay);
    replaylayout->addWidget (new QLabel (i18n("Time (seconds):"), customreplay));
    replaylayout->addWidget (replaytime);
    replaylayout->addItem (new QSpacerItem (0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
    layout()->addItem(new QSpacerItem (5, 0, QSizePolicy::Minimum, QSizePolicy::Minimum));

    static_cast <QBoxLayout *>(layout())->addLayout (buttonlayout);
    layout()->addItem (new QSpacerItem (5, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
#ifdef KMPLAYER_WITH_XINE
    connect (recorder, SIGNAL (clicked(int)), this, SLOT(recorderClicked(int)));
#endif
    connect (replay, SIGNAL (clicked (int)), this, SLOT (replayClicked (int)));
    connect (player, SIGNAL (recording (bool)), this, SLOT (recording (bool)));
}

PrefRecordPage::~PrefRecordPage () {
    if (record_doc)
        record_doc->document ()->dispose ();
}

KDE_NO_EXPORT void PrefRecordPage::recording (bool on) {
    kDebug() << "PrefRecordPage::recording " << on << endl;
    recordButton->setText (on
            ? i18n ("Stop &Recording")
            : i18n ("Start &Recording"));
    url->setEnabled (!on);
    if (on) {
        topLevelWidget ()->hide ();
    } else if (record_doc && record_doc->active ()) {
        record_doc->deactivate ();
        if (replay->selectedId () != Settings::ReplayNo) {
            if (record_doc)
                record_doc->deactivate ();
            if (rec_timer)
                timerEvent (NULL);
            else
                m_player->openUrl (
                        convertNode <RecordDocument> (record_doc)->record_file);
        }
    }
}

KDE_NO_EXPORT void PrefRecordPage::showEvent (QShowEvent *e) {
    Source *src = m_player->source ();
    if (recordButton->text () == i18n ("Start &Recording") && src &&
            src->current ()) {
        int id = 0;
        int nr_recs = 0;
        for (RecorderPage * p = m_recorders; p; p = p->next, ++id) {
            QAbstractButton * radio = recorder->find (id);
            bool b = m_player->mediaManager ()->recorderInfos ()
                [p->recorderName ()]->supports (src->name ());
            radio->setEnabled (b);
            if (b) nr_recs++;
        }
        source_url = src->current ()->src;
        source->setText (i18n ("Current Source: ") + source_url);
        recordButton->setEnabled (nr_recs > 0);
    }
    KVBox::showEvent (e);
}

KDE_NO_EXPORT void PrefRecordPage::timerEvent (QTimerEvent *) {
    killTimer (rec_timer);
    rec_timer = 0;
    if (record_doc)
        m_player->openUrl(convertNode<RecordDocument>(record_doc)->record_file);
}

KDE_NO_EXPORT void PrefRecordPage::recorderClicked (int id) {
    bool b = recorder->find(id)->text().find (QString::fromLatin1("Xine")) > -1;
    replay->setEnabled (!b);
    if (b)
        replay->setButton (Settings::ReplayNo);

}

KDE_NO_EXPORT void PrefRecordPage::replayClicked (int id) {
    replaytime->setEnabled (id == Settings::ReplayAfter);
}

KDE_NO_EXPORT void PrefRecordPage::slotRecord () {
    if (!url->lineEdit()->text().isEmpty()) {
        m_player->source ()->document ()->reset ();
        kDebug() << "Source resetted" << endl;
        m_player->settings ()->recordfile = url->lineEdit()->text();
        m_player->settings ()->replaytime = replaytime->text ().toInt ();
#if KDE_IS_VERSION(3,1,90)
        int id = recorder->selectedId ();
        int replayid = replay->selectedId ();
#else
        int id = recorder->id (recorder->selected ());
        int replayid = replay->id (replay->selectedId ());
#endif
        m_player->settings ()->recorder = Settings::Recorder (id);
        m_player->settings ()->replayoption = Settings::ReplayOption (replayid);
        for (RecorderPage * p = m_recorders; p; p = p->next)
            if (id-- == 0) {
                if (record_doc) {
                    if (record_doc->active ())
                        record_doc->reset ();
                    record_doc->document ()->dispose ();
                }
                record_doc = new RecordDocument (
                        source_url,
                        url->lineEdit()->text(),
                        p->recorderName (),
                        !strcmp (p->recorderName (), "xine"), // FIXME
                        m_player->source ());
                p->startRecording ();
                record_doc->activate ();
                if (replay->selectedId () == Settings::ReplayAfter) {
                    double t = replaytime->text ().toDouble ();
                    if (t > 0.01)
                        rec_timer = startTimer (int (t * 1000));
                }
                break;
            }
    }
}

KDE_NO_CDTOR_EXPORT RecorderPage::RecorderPage (QWidget *parent, PartBase * player)
 : KVBox (parent), next (0L), m_player (player) {}

KDE_NO_CDTOR_EXPORT PrefMEncoderPage::PrefMEncoderPage (QWidget *parent, PartBase * player) : RecorderPage (parent, player) {
    setMargin (5);
    setSpacing (2);

    format = new Q3ButtonGroup (3, Qt::Vertical, i18n ("Format"), this);
    new QRadioButton (i18n ("Same as source"), format);
    new QRadioButton (i18n ("Custom"), format);
    QWidget * customopts = new QWidget (format);
    QGridLayout *gridlayout = new QGridLayout (customopts, 1, 2, 2);
    QLabel *argLabel = new QLabel (i18n("Mencoder arguments:"), customopts, 0);
    arguments = new QLineEdit ("", customopts);
    gridlayout->addWidget (argLabel, 0, 0);
    gridlayout->addWidget (arguments, 0, 1);
    layout()->addItem(new QSpacerItem(0,0, QSizePolicy::Minimum, QSizePolicy::Expanding));
    connect (format, SIGNAL (clicked (int)), this, SLOT (formatClicked (int)));
}

KDE_NO_EXPORT void PrefMEncoderPage::formatClicked (int id) {
    arguments->setEnabled (!!id);
}

KDE_NO_EXPORT void PrefMEncoderPage::startRecording () {
#if KDE_IS_VERSION(3,1,90)
    m_player->settings ()->recordcopy = !format->selectedId ();
#else
    m_player->settings ()->recordcopy = !format->id (format->selected ());
#endif
    m_player->settings ()->mencoderarguments = arguments->text ();
}

KDE_NO_EXPORT QString PrefMEncoderPage::name () {
    return i18n ("&MEncoder");
}

KDE_NO_CDTOR_EXPORT PrefMPlayerDumpstreamPage::PrefMPlayerDumpstreamPage (QWidget *parent, PartBase * player) : RecorderPage (parent, player) {
    hide();
}

KDE_NO_EXPORT QString PrefMPlayerDumpstreamPage::name () {
    return i18n ("MPlayer -&dumpstream");
}

KDE_NO_CDTOR_EXPORT PrefFFMpegPage::PrefFFMpegPage (QWidget *parent, PartBase * player) : RecorderPage (parent, player) {
    setMargin (5);
    setSpacing (2);

    QGridLayout *gridlayout = new QGridLayout (1, 2, 2);
    QLabel *argLabel = new QLabel (i18n("FFMpeg arguments:"), this);
    arguments = new QLineEdit ("", this);
    gridlayout->addWidget (argLabel, 0, 0);
    gridlayout->addWidget (arguments, 0, 1);
    static_cast <QBoxLayout *>(layout())->addLayout (gridlayout);
    layout()->addItem(new QSpacerItem(0,0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

KDE_NO_EXPORT void PrefFFMpegPage::startRecording () {
    m_player->settings ()->ffmpegarguments = arguments->text ();
}

KDE_NO_EXPORT QString PrefFFMpegPage::name () {
    return i18n ("&FFMpeg");
}

#ifdef KMPLAYER_WITH_XINE
KDE_NO_CDTOR_EXPORT PrefXinePage::PrefXinePage (QWidget *parent, PartBase * player) : RecorderPage (parent, player) {
    hide();
}

KDE_NO_EXPORT QString PrefXinePage::name () {
    return i18n ("&Xine");
}
#endif

KDE_NO_CDTOR_EXPORT PrefGeneralPageOutput::PrefGeneralPageOutput(QWidget *parent, OutputDriver * ad, OutputDriver * vd)
 : KVBox (parent) {
    setMargin (5);
    setSpacing (2);

    videoDriver = new Q3ListBox (this);
    for (int i = 0; vd[i].driver; i++)
        videoDriver->insertItem (vd[i].description, i);
    QWhatsThis::add(videoDriver, i18n("Sets video driver. Recommended is XVideo, or, if it is not supported, X11, which is slower."));

    audioDriver = new Q3ListBox (this);
    for (int i = 0; ad[i].driver; i++)
        audioDriver->insertItem (ad[i].description, i);
    layout()->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

KDE_NO_CDTOR_EXPORT PrefOPPageGeneral::PrefOPPageGeneral(QWidget *parent)
: KVBox(parent)
{
    QVBoxLayout *layout = new QVBoxLayout (this, 5);
    layout->setAutoAdd (true);
}

KDE_NO_CDTOR_EXPORT PrefOPPagePostProc::PrefOPPagePostProc(QWidget *parent) : KVBox(parent)
{
    setMargin (5);
    setSpacing (2);

    postProcessing = new QCheckBox (i18n ("Enable use of postprocessing filters"), this);
    postProcessing->setEnabled( true );
    disablePPauto = new QCheckBox (i18n ("Disable use of postprocessing when watching TV/DVD"), this);

    layout()->addItem (new QSpacerItem(5, 5, QSizePolicy::Minimum, QSizePolicy::Minimum));

    PostprocessingOptions = new QTabWidget( this, "PostprocessingOptions" );
    PostprocessingOptions->setEnabled (true);
    //PostprocessingOptions->setAutoMask (false);
    PostprocessingOptions->setTabPosition( QTabWidget::Top );
    PostprocessingOptions->setTabShape( QTabWidget::Rounded );
    PostprocessingOptions->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)1, (QSizePolicy::SizeType)1, PostprocessingOptions->sizePolicy().hasHeightForWidth() ) );

    QWidget *presetSelectionWidget = new QWidget( PostprocessingOptions, "presetSelectionWidget" );
    QGridLayout *presetSelectionWidgetLayout = new QGridLayout( presetSelectionWidget, 1, 1, 1);

    Q3ButtonGroup *presetSelection = new Q3ButtonGroup(3, Qt::Vertical, presetSelectionWidget);
    presetSelection->setInsideSpacing(KDialog::spacingHint());

    defaultPreset = new QRadioButton (i18n ("Default"), presetSelection);
    defaultPreset->setChecked( true );
    presetSelection->insert (defaultPreset);

    customPreset = new QRadioButton (i18n ("Custom"), presetSelection);
    presetSelection->insert (customPreset);

    fastPreset = new QRadioButton (i18n ("Fast"), presetSelection);
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

    Q3GroupBox *customFilters = new Q3GroupBox(0, Qt::Vertical, customFiltersWidget, "customFilters" );
    customFilters->setSizePolicy(QSizePolicy((QSizePolicy::SizeType)1, (QSizePolicy::SizeType)2));
    customFilters->setFlat(false);
    customFilters->setEnabled( false );
    customFilters->setInsideSpacing(7);

    QLayout *customFiltersLayout = customFilters->layout();
    QHBoxLayout *customFiltersLayout1 = new QHBoxLayout ( customFilters->layout() );

    HzDeblockFilter = new QCheckBox (i18n ("Horizontal deblocking"), customFilters);
    HzDeblockAQuality = new QCheckBox (i18n ("Auto quality"), customFilters);
    HzDeblockAQuality->setEnabled (false);
    HzDeblockCFiltering = new QCheckBox (i18n ("Chrominance filtering"), customFilters);
    HzDeblockCFiltering->setEnabled (false);

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

    VtDeblockFilter = new QCheckBox(i18n("Vertical deblocking"), customFilters);
    VtDeblockAQuality = new QCheckBox (i18n ("Auto quality"), customFilters);
    VtDeblockAQuality->setEnabled (false);
    VtDeblockCFiltering = new QCheckBox (i18n ("Chrominance filtering"), customFilters);
    VtDeblockCFiltering->setEnabled (false);

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

    DeringFilter = new QCheckBox (i18n ("Dering filter"), customFilters);
    DeringAQuality = new QCheckBox (i18n ("Auto quality"), customFilters);
    DeringAQuality->setEnabled (false);
    DeringCFiltering=new QCheckBox(i18n("Chrominance filtering"),customFilters);
    DeringCFiltering->setEnabled (false);

    customFiltersLayout3->addWidget( DeringFilter );
    customFiltersLayout3->addItem( new QSpacerItem( 0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum ) );
    customFiltersLayout3->addWidget( DeringAQuality );
    customFiltersLayout3->addWidget( DeringCFiltering );

    QFrame *line3 = new QFrame( customFilters, "line3" );
    line3->setFrameShape( QFrame::HLine );
    line3->setFrameShadow( QFrame::Sunken );
    line3->setFrameShape( QFrame::HLine );

    customFiltersLayout->add(line3);

    QHBoxLayout *customFiltersLayout4 =new QHBoxLayout(customFilters->layout());

    AutolevelsFilter = new QCheckBox (i18n ("Auto brightness/contrast"), customFilters);
    AutolevelsFullrange = new QCheckBox (i18n ("Stretch luminance to full range"), customFilters);
    AutolevelsFullrange->setEnabled (false);

    customFiltersLayout4->addWidget(AutolevelsFilter);
    customFiltersLayout4->addItem(new QSpacerItem( 0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum ));
    customFiltersLayout4->addWidget(AutolevelsFullrange);

    QHBoxLayout *customFiltersLayout5 = new QHBoxLayout (customFilters->layout());

    TmpNoiseFilter =new QCheckBox(i18n("Temporal noise reducer"),customFilters);
    /*	Note: Change TmpNoiseFilter text back to "Label:" if this slider gets reactivated
        TmpNoiseSlider = new QSlider( customFilters, "TmpNoiseSlider" );
        TmpNoiseSlider->setEnabled( false );
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
    Q3ButtonGroup *deinterlacingGroup = new Q3ButtonGroup(5, Qt::Vertical, deintSelectionWidget, "deinterlacingGroup" );

    LinBlendDeinterlacer = new QCheckBox (i18n ("Linear blend deinterlacer"), deinterlacingGroup);
    LinIntDeinterlacer = new QCheckBox (i18n ("Linear interpolating deinterlacer"), deinterlacingGroup);
    CubicIntDeinterlacer = new QCheckBox (i18n ("Cubic interpolating deinterlacer"), deinterlacingGroup);
    MedianDeinterlacer = new QCheckBox (i18n ("Median deinterlacer"), deinterlacingGroup);
    FfmpegDeinterlacer = new QCheckBox (i18n ("FFmpeg deinterlacer"), deinterlacingGroup);

    deinterlacingGroup->insert( LinBlendDeinterlacer );
    deinterlacingGroup->insert( LinIntDeinterlacer );
    deinterlacingGroup->insert( CubicIntDeinterlacer );
    deinterlacingGroup->insert( MedianDeinterlacer );
    deinterlacingGroup->insert( FfmpegDeinterlacer );

    deintSelectionWidgetLayout->addWidget( deinterlacingGroup, 0, 0 );

    PostprocessingOptions->insertTab( deintSelectionWidget, "" );

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

    QWhatsThis::add( defaultPreset, i18n( "Enable mplayer's default postprocessing filters" ) );
    QWhatsThis::add( customPreset, i18n( "Enable custom postprocessing filters (See: Custom preset -tab)" ) );
    QWhatsThis::add( fastPreset, i18n( "Enable mplayer's fast postprocessing filters" ) );
    PostprocessingOptions->changeTab( presetSelectionWidget, i18n( "General" ) );
    customFilters->setTitle (QString ());
    QWhatsThis::add( HzDeblockAQuality, i18n( "Filter is used if there is enough CPU" ) );
    QWhatsThis::add( VtDeblockAQuality, i18n( "Filter is used if there is enough CPU" ) );
    QWhatsThis::add( DeringAQuality, i18n( "Filter is used if there is enough CPU" ) );
    //QWhatsThis::add( TmpNoiseSlider, i18n( "Strength of the noise reducer" ) );
    QWhatsThis::add( AutolevelsFullrange, i18n( "Stretches luminance to full range (0..255)" ) );
    PostprocessingOptions->changeTab( customFiltersWidget, i18n( "Custom Preset" ) );
    deinterlacingGroup->setTitle (QString ());
    PostprocessingOptions->changeTab( deintSelectionWidget, i18n( "Deinterlacing" ) );
    PostprocessingOptions->adjustSize();
}

KDE_NO_EXPORT void Preferences::confirmDefaults() {
    // TODO: Switch to KMessageBox
    switch( QMessageBox::warning( this, i18n("Reset Settings?"),
                i18n("You are about to have all your settings overwritten with defaults.\nPlease confirm.\n"),
                i18n ("&OK"), i18n ("&Cancel"), QString (), 0, 1)) {
        case 0:	Preferences::setDefaults();
                break;
        case 1:	break;
    }
}

KDE_NO_EXPORT void Preferences::setDefaults() {
	m_GeneralPageGeneral->keepSizeRatio->setChecked(true);
	m_GeneralPageGeneral->loop->setChecked(false);
	m_GeneralPageGeneral->seekTime->setValue(10);

	m_GeneralPageOutput->videoDriver->setCurrentItem (0);
	m_GeneralPageOutput->audioDriver->setCurrentItem(0);

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
