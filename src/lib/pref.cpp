/*
    SPDX-FileCopyrightText: 2003 Joonas Koivunen <rzei@mbnet.fi>
    SPDX-FileCopyrightText: 2003 Koos Vriezen <koos.vriezen@xs4all.nl>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QIcon>
#include <QVBoxLayout>
#include <QLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QCheckBox>
#include <QStringList>
#include <QComboBox>
#include <QLineEdit>
#include <QWhatsThis>
#include <QTabWidget>
#include <QSlider>
#include <QSpinBox>
#include <QMap>
#include <QTimer>
#include <QFont>
#include <QAbstractButton>
#include <QButtonGroup>
#include <QGroupBox>
#include <QListWidget>

#include <KLocalizedString>
#include <KMessageBox>
#include <KLineEdit>
#include <KIconLoader>
#include <KComboBox>
#include <KColorButton>
#include <KUrlRequester>
#include <KPluralHandlingSpinBox>
#include <KFontChooserDialog>
#include "pref.h"
#include "kmplayercommon_log.h"
#include "kmplayerpartbase.h"
#include "kmplayerprocess.h"
#include "mediaobject.h"
#include "kmplayerconfig.h"

using namespace KMPlayer;

Preferences::Preferences(PartBase * player, Settings * settings)
: KPageDialog (player->view ())
{
    setFaceType (KPageDialog::List);
    setWindowTitle(i18n("Preferences"));
    setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);
    button(QDialogButtonBox::Ok)->setDefault(true);

    QWidget *page = new QWidget (this);
    QVBoxLayout* pageLayout = new QVBoxLayout(page);
    KPageWidgetItem *item = addPage (page, i18n ("General Options"));
    item->setIcon(QIcon::fromTheme("kmplayer"));
    QTabWidget *tab = new QTabWidget (page);
    m_GeneralPageGeneral = new PrefGeneralPageGeneral (tab, settings);
    tab->addTab (m_GeneralPageGeneral, i18n("General"));
    m_GeneralPageLooks = new PrefGeneralPageLooks (tab, settings);
    tab->addTab (m_GeneralPageLooks, i18n("Looks"));
    m_GeneralPageOutput = new PrefGeneralPageOutput
        (tab, settings->audiodrivers, settings->videodrivers);
    tab->addTab (m_GeneralPageOutput, i18n("Output"));
    pageLayout->addWidget(tab);
    entries.insert (i18n("General Options"), tab);

    page = new QWidget (this);
    pageLayout = new QVBoxLayout(page);
    m_url_item = addPage (page, i18n ("Source"));
    m_url_item->setIcon(QIcon::fromTheme("document-import"));
    tab = new QTabWidget (page);
    m_SourcePageURL = new PrefSourcePageURL (tab);
    tab->addTab (m_SourcePageURL, i18n ("URL"));
    pageLayout->addWidget(tab);
    entries.insert (i18n("Source"), tab);

    page = new QWidget (this);
    pageLayout = new QVBoxLayout(page);
    m_record_item = addPage (page, i18n ("Recording"));
    m_record_item->setIcon(QIcon::fromTheme("folder-video"));
    tab = new QTabWidget (page);

    int recorders_count = 3;
    m_MEncoderPage = new PrefMEncoderPage (tab, player);
    tab->addTab (m_MEncoderPage, i18n ("MEncoder"));
    recorders = m_MEncoderPage;

    m_FFMpegPage = new PrefFFMpegPage (tab, player);
    tab->addTab (m_FFMpegPage, i18n ("FFMpeg"));
    m_MEncoderPage->next = m_FFMpegPage;

    m_MPlayerDumpstreamPage = new PrefMPlayerDumpstreamPage (tab, player);
    // tab->addTab (m_MPlayerDumpstreamPage, i18n ("MPlayer -dumpstream"));
    m_FFMpegPage->next = m_MPlayerDumpstreamPage;
#ifdef KMPLAYER_WITH_XINE
    recorders_count = 4;
    m_XinePage = new PrefXinePage (tab, player);
    // tab->addTab (m_XinePage, i18n ("Xine"));
    m_MPlayerDumpstreamPage->next = m_XinePage;
#endif
    m_RecordPage = new PrefRecordPage (tab, player, recorders, recorders_count);
    tab->insertTab (0, m_RecordPage, i18n ("General"));
    tab->setCurrentIndex (0);
    pageLayout->addWidget(tab);
    entries.insert (i18n("Recording"), tab);

    page = new QWidget (this);
    pageLayout = new QVBoxLayout(page);
    item = addPage (page, i18n ("Output Plugins"));
    item->setIcon(QIcon::fromTheme("folder-image"));
    tab = new QTabWidget (page);
    m_OPPagePostproc = new PrefOPPagePostProc (tab);
    tab->addTab (m_OPPagePostproc, i18n ("Postprocessing"));
    pageLayout->addWidget(tab);
    entries.insert (i18n("Postprocessing"), tab);

    for (PreferencesPage * p = settings->pagelist; p; p = p->next)
        addPrefPage (p);

    //connect (this, SIGNAL (defaultClicked ()), SLOT (confirmDefaults ()));
}

void Preferences::setPage (const char * name) {
    KPageWidgetItem *item = nullptr;
    if (!strcmp (name, "RecordPage"))
        item = m_record_item;
    else if (!strcmp (name, "URLPage"))
        item = m_url_item;
    if (item) {
        setCurrentPage (item);
        QWidget* page = findChild<QWidget*>(name);
        if (!page)
            return;
        QWidget * w = page->parentWidget ();
        while (w && !qobject_cast <QTabWidget *> (w))
            w = w->parentWidget ();
        if (!w)
            return;
        QTabWidget *t = static_cast <QTabWidget*> (w);
        t->setCurrentIndex (t->indexOf(page));
    }
}

void Preferences::addPrefPage (PreferencesPage * page) {
    QString item, subitem, icon;
    KPageWidgetItem *witem;
    QTabWidget *tab;
    page->prefLocation (item, icon, subitem);
    if (item.isEmpty ())
        return;
    QMap<QString, QTabWidget *>::iterator en_it = entries.find (item);
    if (en_it == entries.end ()) {
        QWidget *page = new QWidget(this);
        QVBoxLayout* pageLayout = new QVBoxLayout(page);
        witem = addPage (page, item);
        witem->setIcon(QIcon::fromTheme(icon));
        tab = new QTabWidget (page);
        pageLayout->addWidget(tab);
        entries.insert (item, tab);
    } else
        tab = en_it.value ();
    QFrame *frame = page->prefPage (tab);
    tab->addTab (frame, subitem);
}

void Preferences::removePrefPage(PreferencesPage * page) {
    QString item, subitem, icon;
    page->prefLocation (item, icon, subitem);
    if (item.isEmpty ())
        return;
    QMap<QString, QTabWidget *>::iterator en_it = entries.find (item);
    if (en_it == entries.end ())
        return;
    QTabWidget * tab = en_it.value ();
    for (int i = 0; i < tab->count (); i++)
        if (tab->tabText (i) == subitem) {
            QWidget* w = tab->widget (i);
            tab->removeTab (i);
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

Preferences::~Preferences() {
}

PrefGeneralPageGeneral::PrefGeneralPageGeneral(QWidget *parent, Settings *)
: QWidget(parent)
{
    QGroupBox *windowbox = new QGroupBox(i18n("Window"));
    QWidget * bbox = new QWidget (windowbox);
    QGridLayout * gridlayout = new QGridLayout (bbox/*, 2, 2*/);
    keepSizeRatio = new QCheckBox (i18n ("Keep size ratio"), bbox);
    keepSizeRatio->setWhatsThis(i18n("When checked, the movie will keep its aspect ratio\nwhen the window is resized."));
    dockSysTray = new QCheckBox (i18n ("Dock in system tray"), bbox);
    dockSysTray->setWhatsThis(i18n ("When checked, an icon for KMPlayer will be added to the system tray.\nWhen clicked, it will hide KMPlayer's main window and remove KMPlayer's task bar button."));
    autoResize = new QCheckBox (i18n ("Auto resize to video sizes"), bbox);
    autoResize->setWhatsThis(i18n("When checked, KMPlayer will resize to movie sizes\nwhen video starts."));
    gridlayout->addWidget (keepSizeRatio, 0, 0);
    gridlayout->addWidget (dockSysTray, 1, 0);
    gridlayout->addWidget (autoResize, 0, 1);
    QRadioButton* sizeexit = new QRadioButton(i18n("Remember window size on exit"));
    QRadioButton* sizefixed = new QRadioButton (i18n("Always start with fixed size"));
    sizesChoice = new QButtonGroup(windowbox);
    sizesChoice->addButton(sizeexit, 0);
    sizesChoice->addButton(sizefixed, 1);
    QVBoxLayout* vbox = new QVBoxLayout;
    vbox->addWidget(bbox);
    vbox->addWidget(sizeexit);
    vbox->addWidget(sizefixed);
    windowbox->setLayout(vbox);

    QGroupBox *playbox = new QGroupBox(i18n("Playing"));
    loop = new QCheckBox (i18n("Loop"));
    loop->setWhatsThis(i18n("Makes current movie loop"));
    framedrop = new QCheckBox (i18n ("Allow frame drops"));
    framedrop->setWhatsThis(i18n ("Allow dropping frames for better audio and video synchronization"));
    adjustvolume = new QCheckBox(i18n("Auto set volume on start"));
    adjustvolume->setWhatsThis(i18n ("When a new source is selected, the volume will be set according the volume control"));
    adjustcolors = new QCheckBox(i18n("Auto set colors on start"));
    adjustcolors->setWhatsThis(i18n ("When a movie starts, the colors will be set according the sliders for colors"));
    vbox = new QVBoxLayout;
    vbox->addWidget(loop);
    vbox->addWidget(framedrop);
    vbox->addWidget(adjustvolume);
    vbox->addWidget(adjustcolors);
    playbox->setLayout(vbox);

    QGroupBox* controlbox = new QGroupBox(i18n("Control Panel"));
    showConfigButton = new QCheckBox(i18n("Show config button"));
    showConfigButton->setWhatsThis(i18n ("Add a button that will popup a config menu"));
    showPlaylistButton = new QCheckBox(i18n("Show playlist button"));
    showPlaylistButton->setWhatsThis(i18n ("Add a playlist button to the control buttons"));
    showRecordButton = new QCheckBox(i18n("Show record button"));
    showRecordButton->setWhatsThis(i18n ("Add a record button to the control buttons"));
    showBroadcastButton = new QCheckBox (i18n ("Show broadcast button"));
    showBroadcastButton->setWhatsThis(i18n ("Add a broadcast button to the control buttons"));
    gridlayout = new QGridLayout;
    gridlayout->addWidget (showConfigButton, 0, 0);
    gridlayout->addWidget (showPlaylistButton, 0, 1);
    gridlayout->addWidget (showRecordButton, 1, 0);
    gridlayout->addWidget (showBroadcastButton, 1, 1);
    QHBoxLayout *seekLayout = new QHBoxLayout;
    seekLayout->addWidget(new QLabel(i18n("Forward/backward seek time:")));
    seekLayout->addItem(new QSpacerItem(0,0,QSizePolicy::Minimum, QSizePolicy::Minimum));
    KPluralHandlingSpinBox* pluralSeekBox = new KPluralHandlingSpinBox;
    pluralSeekBox->setRange(1, 600);
    pluralSeekBox->setSingleStep(1);
    pluralSeekBox->setValue(10);
    pluralSeekBox->setSuffix(ki18np(" second", " seconds"));
    seekTime = pluralSeekBox;
    seekLayout->addWidget(seekTime);
    seekLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum));
    gridlayout->addLayout (seekLayout, 2, 0, 1, 2);
    controlbox->setLayout(gridlayout);

    QVBoxLayout* pagelayout = new QVBoxLayout;
    pagelayout->setMargin(5);
    pagelayout->setSpacing(2);
    pagelayout->addWidget(windowbox);
    pagelayout->addWidget(playbox);
    pagelayout->addWidget(controlbox);
    pagelayout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
    setLayout(pagelayout);
}

PrefGeneralPageLooks::PrefGeneralPageLooks (QWidget *parent, Settings * settings)
 : QWidget(parent), colors (settings->colors), fonts (settings->fonts)
{
    QGroupBox *colorbox= new QGroupBox(i18n("Colors"));
    colorscombo = new QComboBox;
    for (int i = 0; i < int (ColorSetting::last_target); i++)
        colorscombo->addItem (colors[i].title);
    colorscombo->setCurrentIndex (0);
    connect (colorscombo, QOverload<int>::of(&QComboBox::activated),
            this, &PrefGeneralPageLooks::colorItemChanged);
    colorbutton = new KColorButton;
    colorbutton->setColor (colors[0].color);
    connect (colorbutton, &KColorButton::changed,
            this, &PrefGeneralPageLooks::colorCanged);
    QHBoxLayout* hbox = new QHBoxLayout;
    hbox->addWidget(colorscombo);
    hbox->addWidget(colorbutton);
    colorbox->setLayout(hbox);

    QGroupBox* fontbox = new QGroupBox(i18n ("Fonts"));
    fontscombo = new QComboBox;
    for (int i = 0; i < int (FontSetting::last_target); i++)
        fontscombo->addItem (fonts[i].title);
    fontscombo->setCurrentIndex (0);
    connect (fontscombo, QOverload<int>::of(&QComboBox::activated),
            this, &PrefGeneralPageLooks::fontItemChanged);
    fontbutton = new QPushButton(i18n ("AaBbCc"));
    fontbutton->setFlat (true);
    fontbutton->setFont (fonts[0].font);
    connect (fontbutton, &QPushButton::clicked, this, &PrefGeneralPageLooks::fontClicked);
    hbox = new QHBoxLayout;
    hbox->addWidget(fontscombo);
    hbox->addWidget(fontbutton);
    fontbox->setLayout(hbox);

    QVBoxLayout* vbox = new QVBoxLayout;
    vbox->setMargin (5);
    vbox->setSpacing (2);
    vbox->addWidget(colorbox);
    vbox->addWidget(fontbox);
    vbox->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
    setLayout(vbox);
}

void PrefGeneralPageLooks::colorItemChanged (int c) {
    if (c < int (ColorSetting::last_target))
        colorbutton->setColor (colors[c].newcolor);
}

void PrefGeneralPageLooks::colorCanged (const QColor & c) {
    if (colorscombo->currentIndex () < int (ColorSetting::last_target))
        colors[colorscombo->currentIndex ()].newcolor = c;
}

void PrefGeneralPageLooks::fontItemChanged (int f) {
    if (f < int (FontSetting::last_target))
        fontbutton->setFont (fonts[f].newfont);
}

void PrefGeneralPageLooks::fontClicked () {
    if (fontscombo->currentIndex () < int (FontSetting::last_target)) {
        QFont myfont = fonts [fontscombo->currentIndex ()].newfont;
        int res = KFontChooserDialog::getFont (myfont, KFontChooser::NoDisplayFlags, this);
        if (res == QDialog::Accepted) {
            fonts [fontscombo->currentIndex ()].newfont = myfont;
            fontbutton->setFont (myfont);
        }
    }
}

PrefSourcePageURL::PrefSourcePageURL (QWidget *parent)
: QWidget(parent)
{
    setObjectName ("URLPage");
    QHBoxLayout* urllayout = new QHBoxLayout;
    QHBoxLayout* sub_urllayout = new QHBoxLayout;
    QLabel *urlLabel = new QLabel(i18n("Location:"));
    urllist = new KComboBox (true);
    urllist->setMaxCount (20);
    urllist->setDuplicatesEnabled (false); // not that it helps much :(
    url = new KUrlRequester(urllist, nullptr);
    url->setWhatsThis(i18n ("Location of the playable item"));
    //url->setShowLocalProtocol (true);
    url->setSizePolicy (QSizePolicy (QSizePolicy::Expanding, QSizePolicy::Preferred));
    QLabel *sub_urlLabel = new QLabel(i18n("Sub title:"));
    sub_urllist = new KComboBox(true);
    sub_urllist->setMaxCount (20);
    sub_urllist->setDuplicatesEnabled (false); // not that it helps much :(
    sub_url = new KUrlRequester(sub_urllist, nullptr);
    sub_url->setWhatsThis(i18n ("Optional location of a file containing the subtitles of the URL above"));
    sub_url->setSizePolicy (QSizePolicy (QSizePolicy::Expanding, QSizePolicy::Preferred));
    backend = new QListWidget;
    clicktoplay = new QCheckBox(i18n("Load on demand"));
    clicktoplay->setWhatsThis(i18n ("When enabled, all embedded movies will start with a image that needs to be clicked to start the video playback"));
    grabhref = new QCheckBox(i18n("Grab image when 'Click to Play' detected"));
    grabhref->setWhatsThis(i18n ("When enabled and a HTML object has a HREF attribute, grab and save an image of the first frame of initial link. This image will be shown instead of a default picture."));
    urllayout->addWidget (urlLabel);
    urllayout->addWidget (url);
    sub_urllayout->addWidget (sub_urlLabel);
    sub_urllayout->addWidget (sub_url);

    QGridLayout * gridlayout = new QGridLayout (/*2, 2*/);
    QLabel *backendLabel = new QLabel(i18n ("Use movie player:"));
    //QWhatsThis::add (allowhref, i18n ("Explain this in a few lines"));
    gridlayout->addWidget (backendLabel, 0, 0);
    gridlayout->addWidget (backend, 1, 0);
    gridlayout->addItem (new QSpacerItem (0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum), 0, 1, 1, 1);

    QGroupBox *bandwidthbox = new QGroupBox(i18n("Network bandwidth"));
    prefBitRate = new QLineEdit;
    prefBitRate->setValidator( new QIntValidator( prefBitRate ) );
    prefBitRate->setWhatsThis(i18n("Sometimes it is possible to choose between various streams given a particular bitrate.\nThis option sets how much bandwidth you would prefer to allocate to video."));
    maxBitRate = new QLineEdit;
    maxBitRate->setValidator( new QIntValidator( maxBitRate ) );
    maxBitRate->setWhatsThis(i18n("Sometimes it is possible to choose between various streams given a particular bitrate.\nThis option sets the maximum bandwidth you have available for video."));
    QGridLayout* bitratelayout = new QGridLayout;
    bitratelayout->addWidget(new QLabel(i18n("Preferred bitrate:")), 0, 0);
    bitratelayout->addWidget (prefBitRate, 0, 1);
    bitratelayout->addWidget (new QLabel (i18n ("kbit/s")), 0, 2);
    bitratelayout->addWidget (new QLabel(i18n("Maximum bitrate:")), 1, 0);
    bitratelayout->addWidget (maxBitRate, 1, 1);
    bitratelayout->addWidget (new QLabel (i18n ("kbit/s")), 1, 2);
    bandwidthbox->setLayout(bitratelayout);

    QVBoxLayout* vbox = new QVBoxLayout;
    vbox->setMargin(5);
    vbox->setSpacing(2);
    vbox->addLayout(urllayout);
    vbox->addLayout(sub_urllayout);
    vbox->addItem (new QSpacerItem (0, 10, QSizePolicy::Minimum, QSizePolicy::Minimum));
    vbox->addWidget(clicktoplay);
    vbox->addWidget(grabhref);
    vbox->addItem (new QSpacerItem (0, 10, QSizePolicy::Minimum, QSizePolicy::Minimum));
    vbox->addWidget(bandwidthbox);
    vbox->addItem (new QSpacerItem (0, 10, QSizePolicy::Minimum, QSizePolicy::Minimum));
    vbox->addLayout(gridlayout);
    vbox->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
    setLayout(vbox);

    connect (url, &KUrlRequester::textChanged,
             this, &PrefSourcePageURL::slotTextChanged);
    connect (sub_url, &KUrlRequester::textChanged,
             this, &PrefSourcePageURL::slotTextChanged);
}

void PrefSourcePageURL::slotBrowse () {
}

void PrefSourcePageURL::slotTextChanged (const QString &) {
    changed = true;
}

PrefRecordPage::PrefRecordPage(QWidget* parent,
        PartBase * player, RecorderPage * rl, int rec_len)
 : QWidget(parent),
   m_player (player),
   m_recorders (rl),
   m_recorders_length (rec_len)
{
    setObjectName ("RecordPage");
    QHBoxLayout * urllayout = new QHBoxLayout ();
    QLabel* urlLabel = new QLabel(i18n("Output file:"));
    url = new KUrlRequester;
    urllayout->addWidget (urlLabel);
    urllayout->addWidget (url);

    source = new QLabel (i18n ("Current source: ") +
           (m_player->source () ? m_player->source ()->prettyName () : QString ()));
    QGroupBox* group = new QGroupBox(i18n("Recorder"));
    QVBoxLayout *vbox = new QVBoxLayout;
    recorder = new QButtonGroup;
    int id = 0;
    for (RecorderPage* p = m_recorders; p; p = p->next) {
        QRadioButton* button = new QRadioButton(p->name());
        vbox->addWidget(button);
        recorder->addButton(button, id++);
    }
    recorder->button(0)->setChecked(true); // for now
    group->setLayout(vbox);

    QGroupBox* autogroup = new QGroupBox(i18n("Auto Playback"));
    vbox = new QVBoxLayout;
    replay = new QButtonGroup;
    QRadioButton* radio = new QRadioButton (i18n ("&No"));
    vbox->addWidget(radio);
    replay->addButton(radio, 0);
    radio = new QRadioButton (i18n ("&When recording finished"));
    vbox->addWidget(radio);
    replay->addButton(radio, 1);
    radio = new QRadioButton (i18n ("A&fter"));
    vbox->addWidget(radio);
    replay->addButton(radio, 2);
    QWidget* customreplay = new QWidget;
    KPluralHandlingSpinBox* pluralReplayBox = new KPluralHandlingSpinBox;
    pluralReplayBox = new KPluralHandlingSpinBox;
    pluralReplayBox->setSuffix(ki18np(" second", " seconds"));
    replaytime = pluralReplayBox;
    QHBoxLayout *replaylayout = new QHBoxLayout;
    replaylayout->addWidget(new QLabel(i18n("Time:")));
    replaylayout->addWidget (replaytime);
    replaylayout->addItem (new QSpacerItem (0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));
    customreplay->setLayout(replaylayout);
    vbox->addWidget(customreplay);
    autogroup->setLayout(vbox);

    recordButton = new QPushButton (i18n ("Start &Recording"));
    QHBoxLayout *buttonlayout = new QHBoxLayout;
    buttonlayout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum));
    buttonlayout->addWidget (recordButton);
#ifdef KMPLAYER_WITH_XINE
    connect (recorder, SIGNAL (clicked(int)), this, SLOT(recorderClicked(int)));
#endif
    //connect(replay, SIGNAL(buttonClicked (int)), this, SLOT (replayClicked (int)));
    connect (player, &PartBase::recording, this, &PrefRecordPage::recording);
    connect(recordButton, &QPushButton::clicked, this, &PrefRecordPage::slotRecord);

    QVBoxLayout* pagelayout = new QVBoxLayout;
    pagelayout->setMargin(5);
    pagelayout->setSpacing(2);
    pagelayout->addItem(new QSpacerItem(5, 0, QSizePolicy::Minimum, QSizePolicy::Minimum));
    pagelayout->addLayout(urllayout);
    pagelayout->addItem(new QSpacerItem(5, 0, QSizePolicy::Minimum, QSizePolicy::Minimum));
    pagelayout->addWidget(source);
    pagelayout->addItem(new QSpacerItem(5, 0, QSizePolicy::Minimum, QSizePolicy::Minimum));
    pagelayout->addWidget(group);
    pagelayout->addItem(new QSpacerItem(5, 0, QSizePolicy::Minimum, QSizePolicy::Minimum));
    pagelayout->addWidget(autogroup);
    pagelayout->addLayout(buttonlayout);
    pagelayout->addItem(new QSpacerItem(5, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
    setLayout(pagelayout);
}

PrefRecordPage::~PrefRecordPage () {
}

void PrefRecordPage::recording (bool on) {
    qCDebug(LOG_KMPLAYER_COMMON) << "PrefRecordPage::recording " << on << endl;
    recordButton->setText (on
            ? i18n ("Stop &Recording")
            : i18n ("Start &Recording"));
    url->setEnabled (!on);
    if (on)
        topLevelWidget ()->hide ();
}

void PrefRecordPage::showEvent (QShowEvent *e) {
    Source *src = m_player->source ();
    if (recordButton->text () == i18n ("Start &Recording") && src &&
            src->current ()) {
        int id = 0;
        int nr_recs = 0;
        for (RecorderPage * p = m_recorders; p; p = p->next, ++id) {
            QAbstractButton * radio = recorder->button(id);
            bool b = m_player->mediaManager ()->recorderInfos ()
                [p->recorderName ()]->supports (src->name ());
            radio->setEnabled (b);
            if (b) nr_recs++;
        }
        source_url = src->current ()->src;
        source->setText (i18n ("Current Source: ") + source_url);
        recordButton->setEnabled (nr_recs > 0);
    }
    QWidget::showEvent (e);
}

void PrefRecordPage::recorderClicked (int /*id*/) {
    /*bool b = recorder->button(id)->text().indexOf (QString::fromLatin1("Xine")) > -1;
    replay->setEnabled (!b);
    if (b)
        replay->setButton (Settings::ReplayNo);*/

}

void PrefRecordPage::replayClicked (int id) {
    replaytime->setEnabled (id == Settings::ReplayAfter);
}

void PrefRecordPage::slotRecord () {
    if (m_player->isRecording ()) {
        m_player->stopRecording ();
    } else if (!url->lineEdit()->text().isEmpty()) {
        m_player->source ()->document ()->reset ();
        m_player->settings ()->recordfile = url->lineEdit()->text();
        m_player->settings ()->replaytime = replaytime->value();
        int id = recorder->checkedId ();
        int replayid = replay->checkedId ();
        m_player->settings ()->recorder = Settings::Recorder (id);
        m_player->settings ()->replayoption = Settings::ReplayOption (replayid);
        for (RecorderPage * p = m_recorders; p; p = p->next)
            if (id-- == 0) {
                int start_after = 0;
                if (replay->checkedId () == Settings::ReplayAfter) {
                    int t = replaytime->value ();
                    if (t > 0)
                        start_after = 1000 * t;
                } else if (replay->checkedId () != Settings::ReplayNo) {
                    start_after = -1;
                }
                p->startRecording ();
                m_player->record (source_url, url->lineEdit()->text(),
                        p->recorderName (), start_after);
                break;
            }
    }
}

RecorderPage::RecorderPage (QWidget *parent, PartBase * player)
 : QWidget(parent), next(nullptr), m_player(player) {}

PrefMEncoderPage::PrefMEncoderPage(QWidget* parent, PartBase* player)
    : RecorderPage (parent, player)
{
    QGroupBox* formatbox = new QGroupBox(i18n("Format"));
    QVBoxLayout* vbox = new QVBoxLayout;
    format = new QButtonGroup(this);
    QRadioButton* radio = new QRadioButton (i18n ("Same as source"));
    vbox->addWidget(radio);
    format->addButton(radio, 0);
    radio = new QRadioButton (i18n ("Custom"));
    vbox->addWidget(radio);
    format->addButton(radio, 1);
    QGridLayout* gridlayout = new QGridLayout;
    QLabel *argLabel = new QLabel (i18n("Mencoder arguments:"));
    arguments = new QLineEdit ("");
    gridlayout->addWidget (argLabel, 0, 0);
    gridlayout->addWidget (arguments, 0, 1);
    vbox->addLayout(gridlayout);
    formatbox->setLayout(vbox);
    connect (format, QOverload<int>::of(&QButtonGroup::buttonClicked), this, &PrefMEncoderPage::formatClicked);

    QVBoxLayout* pagelayout = new QVBoxLayout;
    pagelayout->setMargin(5);
    pagelayout->setSpacing(2);
    pagelayout->addWidget(formatbox);
    pagelayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
    setLayout(pagelayout);
}

void PrefMEncoderPage::formatClicked (int id) {
    arguments->setEnabled (!!id);
}

void PrefMEncoderPage::startRecording () {
    m_player->settings ()->recordcopy = !format->checkedId ();
    m_player->settings ()->mencoderarguments = arguments->text ();
}

QString PrefMEncoderPage::name () {
    return i18n ("&MEncoder");
}

PrefMPlayerDumpstreamPage::PrefMPlayerDumpstreamPage (QWidget *parent, PartBase * player) : RecorderPage (parent, player) {
    hide();
}

QString PrefMPlayerDumpstreamPage::name () {
    return i18n ("MPlayer -&dumpstream");
}

PrefFFMpegPage::PrefFFMpegPage(QWidget* parent, PartBase* player)
    : RecorderPage(parent, player)
{
    QGridLayout *gridlayout = new QGridLayout (/*1, 2, 2*/);
    QLabel *argLabel = new QLabel (i18n("FFMpeg arguments:"));
    arguments = new QLineEdit ("");
    gridlayout->addWidget (argLabel, 0, 0);
    gridlayout->addWidget (arguments, 0, 1);

    QVBoxLayout* pagelayout = new QVBoxLayout;
    pagelayout->setMargin(5);
    pagelayout->setSpacing(2);
    pagelayout->addLayout (gridlayout);
    pagelayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
    setLayout(pagelayout);
}

void PrefFFMpegPage::startRecording () {
    m_player->settings ()->ffmpegarguments = arguments->text ();
}

QString PrefFFMpegPage::name () {
    return i18n ("&FFMpeg");
}

#ifdef KMPLAYER_WITH_XINE
PrefXinePage::PrefXinePage (QWidget *parent, PartBase * player) : RecorderPage (parent, player) {
    hide();
}

QString PrefXinePage::name () {
    return i18n ("&Xine");
}
#endif

PrefGeneralPageOutput::PrefGeneralPageOutput(QWidget *parent, OutputDriver * ad, OutputDriver * vd)
 : QWidget(parent)
{
    videoDriver = new QListWidget;
    for (int i = 0; vd[i].driver; i++)
        videoDriver->addItem(vd[i].description);
    videoDriver->setWhatsThis(i18n("Sets video driver. Recommended is XVideo, or, if it is not supported, X11, which is slower."));

    audioDriver = new QListWidget;
    for (int i = 0; ad[i].driver; i++)
        audioDriver->addItem(ad[i].description);

    QVBoxLayout* pagelayout = new QVBoxLayout;
    pagelayout->setMargin(5);
    pagelayout->setSpacing(2);
    pagelayout->addWidget(videoDriver);
    pagelayout->addWidget(audioDriver);
    pagelayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
    setLayout(pagelayout);
}

PrefOPPagePostProc::PrefOPPagePostProc(QWidget *parent)
    : QWidget(parent)
{
    postProcessing = new QCheckBox(i18n("Enable use of postprocessing filters"));
    postProcessing->setEnabled( true );
    disablePPauto = new QCheckBox(i18n("Disable use of postprocessing when watching TV/DVD"));

    PostprocessingOptions = new QTabWidget;
    PostprocessingOptions->setEnabled (true);
    //PostprocessingOptions->setAutoMask (false);
    PostprocessingOptions->setTabShape( QTabWidget::Rounded );
    PostprocessingOptions->setSizePolicy( QSizePolicy( QSizePolicy::Minimum, QSizePolicy::Minimum ));

    QGroupBox* presetSelection = new QGroupBox;
    defaultPreset = new QRadioButton (i18n ("Default"));
    customPreset = new QRadioButton (i18n ("Custom"));
    fastPreset = new QRadioButton (i18n ("Fast"));
    defaultPreset->setChecked( true );
    QVBoxLayout* vbox = new QVBoxLayout;
    vbox->addWidget(defaultPreset);
    vbox->addWidget(customPreset);
    vbox->addWidget(fastPreset);
    QButtonGroup* buttongroup = new QButtonGroup(presetSelection);
    buttongroup->addButton(defaultPreset, 0);
    buttongroup->addButton(customPreset, 1);
    buttongroup->addButton(fastPreset, 2);
    vbox->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
    presetSelection->setLayout(vbox);
    PostprocessingOptions->addTab(presetSelection, i18n("General"));

    //
    // SECOND!!!
    //
    /* I JUST WASN'T ABLE TO GET THIS WORKING WITH QGridLayouts */
    QVBoxLayout *customFiltersLayout = new QVBoxLayout;

    QGroupBox *customFilters = new QGroupBox;
    customFilters->setSizePolicy(QSizePolicy(QSizePolicy::Minimum, QSizePolicy::Maximum));
    customFilters->setFlat(false);
    customFilters->setEnabled( false );
    //customFilters->setInsideSpacing(7);

    QHBoxLayout* hbox = new QHBoxLayout;
    HzDeblockFilter = new QCheckBox (i18n ("Horizontal deblocking"));
    HzDeblockAQuality = new QCheckBox (i18n ("Auto quality"));
    HzDeblockAQuality->setEnabled (false);
    HzDeblockCFiltering = new QCheckBox (i18n ("Chrominance filtering"));
    HzDeblockCFiltering->setEnabled (false);
    hbox->addWidget( HzDeblockFilter );
    hbox->addItem( new QSpacerItem( 0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum ) );
    hbox->addWidget( HzDeblockAQuality );
    hbox->addWidget( HzDeblockCFiltering );
    customFiltersLayout->addLayout(hbox);

    QFrame* line = new QFrame;
    line->setSizePolicy( QSizePolicy( QSizePolicy::Minimum, QSizePolicy::Maximum ) );
    line->setFrameShape( QFrame::HLine );
    line->setFrameShadow( QFrame::Sunken );
    customFiltersLayout->addWidget(line);

    hbox = new QHBoxLayout;
    VtDeblockFilter = new QCheckBox(i18n("Vertical deblocking"));
    VtDeblockAQuality = new QCheckBox (i18n ("Auto quality"));
    VtDeblockAQuality->setEnabled (false);
    VtDeblockCFiltering = new QCheckBox (i18n ("Chrominance filtering"));
    VtDeblockCFiltering->setEnabled (false);
    hbox->addWidget( VtDeblockFilter );
    hbox->addItem( new QSpacerItem( 0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum ) );
    hbox->addWidget( VtDeblockAQuality );
    hbox->addWidget( VtDeblockCFiltering );
    customFiltersLayout->addLayout(hbox);

    line = new QFrame;
    line->setSizePolicy( QSizePolicy( QSizePolicy::Minimum, QSizePolicy::Maximum ) );
    line->setFrameShape( QFrame::HLine );
    line->setFrameShadow( QFrame::Sunken );
    customFiltersLayout->addWidget(line);

    hbox = new QHBoxLayout;
    DeringFilter = new QCheckBox (i18n ("Dering filter"));
    DeringAQuality = new QCheckBox (i18n ("Auto quality"));
    DeringAQuality->setEnabled (false);
    DeringCFiltering=new QCheckBox(i18n("Chrominance filtering"));
    DeringCFiltering->setEnabled (false);
    hbox->addWidget( DeringFilter );
    hbox->addItem( new QSpacerItem( 0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum ) );
    hbox->addWidget( DeringAQuality );
    hbox->addWidget( DeringCFiltering );
    customFiltersLayout->addLayout(hbox);

    line = new QFrame;
    line->setFrameShape( QFrame::HLine );
    line->setFrameShadow( QFrame::Sunken );
    line->setFrameShape( QFrame::HLine );
    customFiltersLayout->addWidget(line);

    hbox = new QHBoxLayout;
    AutolevelsFilter = new QCheckBox (i18n ("Auto brightness/contrast"));
    AutolevelsFullrange = new QCheckBox (i18n ("Stretch luminance to full range"));
    AutolevelsFullrange->setEnabled (false);
    hbox->addWidget(AutolevelsFilter);
    hbox->addItem(new QSpacerItem( 0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum ));
    hbox->addWidget(AutolevelsFullrange);
    customFiltersLayout->addLayout(hbox);

    hbox = new QHBoxLayout;
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
        TmpNoiseSlider->setSizePolicy(QSizePolicy( QSizePolicy::Minimum, QSizePolicy::Minimum);*/

    /*customFiltersLayout->addWidget(TmpNoiseFilter,7,0);
      customFiltersLayout->addWidget(TmpNoiseSlider,7,2);*/
    hbox->addWidget(TmpNoiseFilter);
    hbox->addItem(new QSpacerItem( 0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum ));
    //hbox->addWidget(TmpNoiseSlider);
    customFiltersLayout->addLayout(hbox);

    customFiltersLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
    customFilters->setLayout(customFiltersLayout);

    PostprocessingOptions->addTab(customFilters, i18n("Custom Preset"));
    //
    //THIRD!!!
    //
    QGroupBox* deintSelectionWidget = new QGroupBox;
    vbox = new QVBoxLayout;
    LinBlendDeinterlacer = new QCheckBox(i18n ("Linear blend deinterlacer"));
    LinIntDeinterlacer = new QCheckBox(i18n ("Linear interpolating deinterlacer"));
    CubicIntDeinterlacer = new QCheckBox(i18n ("Cubic interpolating deinterlacer"));
    MedianDeinterlacer = new QCheckBox(i18n ("Median deinterlacer"));
    FfmpegDeinterlacer = new QCheckBox(i18n ("FFmpeg deinterlacer"));
    vbox->addWidget(LinBlendDeinterlacer);
    vbox->addWidget(LinIntDeinterlacer);
    vbox->addWidget(CubicIntDeinterlacer);
    vbox->addWidget(MedianDeinterlacer);
    vbox->addWidget(FfmpegDeinterlacer);
    vbox->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
    deintSelectionWidget->setLayout(vbox);
    PostprocessingOptions->addTab(deintSelectionWidget, i18n( "Deinterlacing"));

    QVBoxLayout* pagelayout = new QVBoxLayout;
    pagelayout->setMargin(5);
    pagelayout->setSpacing(2);
    pagelayout->addWidget(postProcessing);
    pagelayout->addWidget(disablePPauto);
    pagelayout->addItem(new QSpacerItem(5, 5, QSizePolicy::Minimum, QSizePolicy::Minimum));
    pagelayout->addWidget(PostprocessingOptions);
    pagelayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
    setLayout(pagelayout);

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

    defaultPreset->setWhatsThis(i18n( "Enable mplayer's default postprocessing filters" ) );
    customPreset->setWhatsThis(i18n( "Enable custom postprocessing filters (See: Custom preset -tab)" ) );
    fastPreset->setWhatsThis(i18n( "Enable mplayer's fast postprocessing filters" ) );
    HzDeblockAQuality->setWhatsThis(i18n( "Filter is used if there is enough CPU" ) );
    VtDeblockAQuality->setWhatsThis(i18n( "Filter is used if there is enough CPU" ) );
    DeringAQuality->setWhatsThis(i18n( "Filter is used if there is enough CPU" ) );
    //QWhatsThis::add( TmpNoiseSlider, i18n( "Strength of the noise reducer" ) );
    AutolevelsFullrange->setWhatsThis(i18n( "Stretches luminance to full range (0..255)" ) );
    PostprocessingOptions->adjustSize();
}

void Preferences::confirmDefaults() {
    switch( KMessageBox::warningContinueCancel( this,
                i18n("You are about to have all your settings overwritten with defaults.\nPlease confirm.\n"),
                i18n("Reset Settings?"))) {
        case KMessageBox::Continue:
                Preferences::setDefaults();
                break;
        case KMessageBox::Cancel:
        default: // avoid warnings for the unhandled enum values
                break;
    }
}

void Preferences::setDefaults() {
	m_GeneralPageGeneral->keepSizeRatio->setChecked(true);
	m_GeneralPageGeneral->loop->setChecked(false);
	m_GeneralPageGeneral->seekTime->setValue(10);

	m_GeneralPageOutput->videoDriver->setCurrentRow(0);
	m_GeneralPageOutput->audioDriver->setCurrentRow(0);

	m_OPPagePostproc->postProcessing->setChecked(false);
	m_OPPagePostproc->disablePPauto->setChecked(true);

	m_OPPagePostproc->defaultPreset->setChecked(true);

	m_OPPagePostproc->LinBlendDeinterlacer->setChecked(false);
	m_OPPagePostproc->LinIntDeinterlacer->setChecked(false);
	m_OPPagePostproc->CubicIntDeinterlacer->setChecked(false);
	m_OPPagePostproc->MedianDeinterlacer->setChecked(false);
	m_OPPagePostproc->FfmpegDeinterlacer->setChecked(false);

}

#include "moc_pref.cpp"
