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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <qlayout.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qcheckbox.h>
#include <qtable.h>
#include <qstringlist.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <qgroupbox.h>
#include <qwhatsthis.h>
#include <qtabwidget.h>
#include <qslider.h>
#include <qbuttongroup.h>
#include <qspinbox.h>
#include <qmessagebox.h>
#include <qmap.h>

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

#include "pref.h"
#include "kmplayerpartbase.h"
#include "kmplayerprocess.h"
#include "kmplayerconfig.h"


using namespace KMPlayer;

KDE_NO_CDTOR_EXPORT Preferences::Preferences(PartBase * player, Settings * settings)
: KDialogBase (IconList, i18n ("Preferences"),
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
    m_GeneralPageGeneral = new PrefGeneralPageGeneral (tab, settings);
    tab->insertTab (m_GeneralPageGeneral, i18n("General"));
    m_GeneralPageOutput = new PrefGeneralPageOutput
        (tab, settings->audiodrivers, settings->videodrivers);
    tab->insertTab (m_GeneralPageOutput, i18n("Output"));
    entries.insert (i18n("General Options"), tab);

    frame = addPage (i18n ("Source"), QString::null, KGlobal::iconLoader()->loadIcon (QString ("source"), KIcon::NoGroup, 32));
    vlay = new QVBoxLayout (frame, marginHint(), spacingHint());
    tab = new QTabWidget (frame);
    vlay->addWidget (tab);
    m_SourcePageURL = new PrefSourcePageURL (tab);
    tab->insertTab (m_SourcePageURL, i18n ("URL"));
    entries.insert (i18n("Source"), tab);

    frame = addPage (i18n ("Recording"), QString::null, KGlobal::iconLoader()->loadIcon (QString ("video"), KIcon::NoGroup, 32));
    vlay = new QVBoxLayout (frame, marginHint(), spacingHint());
    tab = new QTabWidget (frame);
    vlay->addWidget (tab);

    m_MEncoderPage = new PrefMEncoderPage (tab, player);
    tab->insertTab (m_MEncoderPage, i18n ("MEncoder"));
    recorders = m_MEncoderPage;

    m_FFMpegPage = new PrefFFMpegPage (tab, player);
    tab->insertTab (m_FFMpegPage, i18n ("FFMpeg"));
    m_MEncoderPage->next = m_FFMpegPage;

    m_MPlayerDumpstreamPage = new PrefMPlayerDumpstreamPage (tab, player);
    // tab->insertTab (m_MPlayerDumpstreamPage, i18n ("MPlayer -dumpstream"));
    m_FFMpegPage->next = m_MPlayerDumpstreamPage;

    m_RecordPage = new PrefRecordPage (tab, player, recorders);
    tab->insertTab (m_RecordPage, i18n ("General"), 0);
    tab->setCurrentPage (0);
    entries.insert (i18n("Recording"), tab);

    frame = addPage (i18n ("Output Plugins"), QString::null, KGlobal::iconLoader()->loadIcon (QString ("image"), KIcon::NoGroup, 32));
    vlay = new QVBoxLayout(frame, marginHint(), spacingHint());
    tab = new QTabWidget (frame);
    vlay->addWidget (tab);
    m_OPPagePostproc = new PrefOPPagePostProc (tab);
    tab->insertTab (m_OPPagePostproc, i18n ("Postprocessing"));
    entries.insert (i18n("Postprocessing"), tab);

    for (PreferencesPage * p = settings->pagelist; p; p = p->next)
        addPrefPage (p);
        

    connect (this, SIGNAL (defaultClicked ()), SLOT (confirmDefaults ()));
}

KDE_NO_EXPORT void Preferences::setPage (const char * name) {
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

KDE_NO_EXPORT void Preferences::addPrefPage (PreferencesPage * page) {
    QString item, subitem, icon;
    QFrame * frame;
    QTabWidget * tab;
    QVBoxLayout *vlay;
    page->prefLocation (item, icon, subitem);
    if (item.isEmpty ())
        return;
    QMap<QString, QTabWidget *>::iterator en_it = entries.find (item);
    if (en_it == entries.end ()) {
        frame = addPage (item, QString::null, KGlobal::iconLoader()->loadIcon ((icon), KIcon::NoGroup, 32));
        vlay = new QVBoxLayout (frame, marginHint(), spacingHint());
        tab = new QTabWidget (frame);
        vlay->addWidget (tab);
        entries.insert (item, tab);
    } else
        tab = en_it.data ();
    frame = page->prefPage (tab);
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

KDE_NO_CDTOR_EXPORT PrefGeneralPageGeneral::PrefGeneralPageGeneral(QWidget *parent, Settings * settings)
: QFrame (parent, "GeneralPage"), colors (settings->colors)
{
	QVBoxLayout *layout = new QVBoxLayout(this, 5, 2);

        QGroupBox *windowbox =new QGroupBox(3,Qt::Vertical,i18n("Window"),this);
	keepSizeRatio = new QCheckBox (i18n("Keep size ratio"), windowbox, 0);
	QWhatsThis::add(keepSizeRatio, i18n("When checked, movie will keep its aspect ratio\nwhen window is resized"));
	dockSysTray = new QCheckBox (i18n("Dock in system tray"), windowbox, 0);
	QWhatsThis::add (dockSysTray, i18n ("When checked, an icon of KMPlayer will be added to the system tray.\nWhen clicked it will hide KMPlayer's main window and removing KMPlayer's taskbar button."));
        sizesChoice = new QButtonGroup (2, Qt::Vertical, windowbox);
        new QRadioButton (i18n("Remember window size on exit"), sizesChoice);
        new QRadioButton (i18n("Always start with fixed size"), sizesChoice);
        QGroupBox *playbox =new QGroupBox(3, Qt::Vertical,i18n("Playing"),this);
	loop = new QCheckBox (i18n("Loop"), playbox);
	QWhatsThis::add(loop, i18n("Makes current movie loop"));
	framedrop = new QCheckBox (i18n ("Allow framedrops"), playbox);
	QWhatsThis::add (framedrop, i18n ("Allow dropping frames for better audio and video synchronization"));
	adjustvolume = new QCheckBox(i18n("Auto set volume on start"), playbox);
	QWhatsThis::add (adjustvolume, i18n ("When a new source is selected, the volume will be set according the volume control"));
        QGroupBox *buttonbox =new QGroupBox(3, Qt::Vertical,i18n("Control Panel"),this);
	showRecordButton = new QCheckBox(i18n("Show record button"), buttonbox);
	QWhatsThis::add (showRecordButton, i18n ("Add a record button to the control buttons"));
	showBroadcastButton = new QCheckBox (i18n ("Show broadcast button"), buttonbox);
	QWhatsThis::add (showBroadcastButton, i18n ("Add a broadcast button to the control buttons"));
	//autoHideSlider = new QCheckBox (i18n("Auto hide position slider"), this, 0);

	QWidget *seekingWidget = new QWidget (buttonbox);
	QHBoxLayout *seekingWidgetLayout = new QHBoxLayout (seekingWidget);
	seekingWidgetLayout->addWidget(new QLabel(i18n("Forward/backward seek time:"),seekingWidget));
	seekingWidgetLayout->addItem(new QSpacerItem(0,0,QSizePolicy::Minimum, QSizePolicy::Minimum));
	seekTime = new QSpinBox(1, 600, 1, seekingWidget);
	seekingWidgetLayout->addWidget(seekTime);
        QGroupBox *colorbox=new QGroupBox(2,Qt::Horizontal,i18n("Colors"),this);
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
	seekingWidgetLayout->addItem(new QSpacerItem(0,0,QSizePolicy::Minimum, QSizePolicy::Minimum));
	layout->addWidget (windowbox);
	layout->addWidget (playbox);
	layout->addWidget (buttonbox);
	//layout->addWidget(autoHideSlider);
	layout->addWidget (colorbox);
        layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

KDE_NO_EXPORT void PrefGeneralPageGeneral::colorItemChanged (int c) {
    if (c < int (ColorSetting::last_target))
        colorbutton->setColor (colors[c].newcolor);
}

KDE_NO_EXPORT void PrefGeneralPageGeneral::colorCanged (const QColor & c) {
    if (colorscombo->currentItem () < int (ColorSetting::last_target))
        colors[colorscombo->currentItem ()].newcolor = c;
}

KDE_NO_CDTOR_EXPORT PrefSourcePageURL::PrefSourcePageURL (QWidget *parent)
: QFrame (parent, "URLPage")
{
    QVBoxLayout *layout = new QVBoxLayout (this, 5, 5);
    QHBoxLayout * urllayout = new QHBoxLayout ();
    QHBoxLayout * sub_urllayout = new QHBoxLayout ();
    QLabel *urlLabel = new QLabel (i18n ("Location:"), this, 0);
    urllist = new KComboBox (true, this);
    urllist->setMaxCount (20);
    urllist->setDuplicatesEnabled (false); // not that it helps much :(
    url = new KURLRequester (urllist, this);
    //url->setShowLocalProtocol (true);
    url->setSizePolicy (QSizePolicy (QSizePolicy::Expanding, QSizePolicy::Preferred));
    QLabel *sub_urlLabel = new QLabel (i18n ("Sub title:"), this, 0);
    sub_urllist = new KComboBox (true, this);
    sub_urllist->setMaxCount (20);
    sub_urllist->setDuplicatesEnabled (false); // not that it helps much :(
    sub_url = new KURLRequester (sub_urllist, this);
    sub_url->setSizePolicy (QSizePolicy (QSizePolicy::Expanding, QSizePolicy::Preferred));
    backend = new QListBox (this);
    allowhref = new QCheckBox (i18n ("Enable 'Click to Play' support"), this);
    layout->addWidget (allowhref);
    urllayout->addWidget (urlLabel);
    urllayout->addWidget (url);
    layout->addLayout (urllayout);
    sub_urllayout->addWidget (sub_urlLabel);
    sub_urllayout->addWidget (sub_url);
    layout->addLayout (sub_urllayout);
    layout->addItem (new QSpacerItem (0, 10, QSizePolicy::Minimum, QSizePolicy::Minimum));
    QGridLayout * gridlayout = new QGridLayout (2, 2);
    QLabel *backendLabel = new QLabel (i18n ("Use movie player:"), this, 0);
    //QWhatsThis::add (allowhref, i18n ("Explain this in a few lines"));
    gridlayout->addWidget (backendLabel, 0, 0);
    gridlayout->addWidget (backend, 1, 0);
    gridlayout->addMultiCell (new QSpacerItem (0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum), 0, 1, 1, 1);
    layout->addLayout (gridlayout);
    layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
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

KDE_NO_CDTOR_EXPORT PrefRecordPage::PrefRecordPage (QWidget *parent, PartBase * player, RecorderPage * rl) : QFrame (parent, "RecordPage"), m_player (player), m_recorders (rl) {
    QVBoxLayout *layout = new QVBoxLayout (this, 5, 5);
    QHBoxLayout * urllayout = new QHBoxLayout ();
    QLabel *urlLabel = new QLabel (i18n ("Output file:"), this);
    url = new KURLRequester ("", this);
    url->setShowLocalProtocol (true);
    urllayout->addWidget (urlLabel);
    urllayout->addWidget (url);
    recordButton = new QPushButton (i18n ("Start &Recording"), this);
    connect (recordButton, SIGNAL (clicked ()), this, SLOT (slotRecord ()));
    QHBoxLayout *buttonlayout = new QHBoxLayout;
    buttonlayout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Minimum));
    buttonlayout->addWidget (recordButton);
    source = new QLabel (i18n ("Current source: ") + m_player->source ()->prettyName (), this);
    recorder = new QButtonGroup (3 /*m_recorders.size ()*/, Qt::Vertical, i18n ("Recorder"), this);
    for (RecorderPage * p = m_recorders; p; p = p->next)
        new QRadioButton (p->name (), recorder);
    if (m_player->source ())
        sourceChanged (0L, m_player->source ());
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
    connect (m_player, SIGNAL (sourceChanged(KMPlayer::Source*,KMPlayer::Source*)), this, SLOT (sourceChanged(KMPlayer::Source*,KMPlayer::Source*)));
    connect (replay, SIGNAL (clicked (int)), this, SLOT (replayClicked (int)));
}

KDE_NO_EXPORT void PrefRecordPage::recordingStarted () {
    recordButton->setText (i18n ("Stop Recording"));
    url->setEnabled (false);
    topLevelWidget ()->hide ();
}

KDE_NO_EXPORT void PrefRecordPage::recordingFinished () {
    recordButton->setText (i18n ("Start Recording"));
    url->setEnabled (true);
}

KDE_NO_EXPORT void PrefRecordPage::sourceChanged (Source * olds, Source * nws) {
    int id = 0;
    int nr_recs = 0;
    if (olds) {
        disconnect(nws,SIGNAL(startRecording()),this, SLOT(recordingStarted()));
        disconnect(nws,SIGNAL(stopRecording()),this, SLOT(recordingFinished()));
    }
    if (nws) {
        for (RecorderPage * p = m_recorders; p; p = p->next, ++id) {
            QButton * radio = recorder->find (id);
            bool b = m_player->recorders () [p->recorderName ()]->supports (nws->name ());
            radio->setEnabled (b);
            if (b) nr_recs++;
        }
        source->setText (i18n ("Current Source: ") + nws->prettyName ());
        connect (nws, SIGNAL(startRecording()), this, SLOT(recordingStarted()));
        connect (nws, SIGNAL(stopRecording()), this, SLOT(recordingFinished()));
    }
    recordButton->setEnabled (nr_recs > 0);
}

KDE_NO_EXPORT void PrefRecordPage::replayClicked (int id) {
    replaytime->setEnabled (id == Settings::ReplayAfter);
}

KDE_NO_EXPORT void PrefRecordPage::slotRecord () {
    if (!url->lineEdit()->text().isEmpty()) {
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
            if (id-- == 0)
                p->record ();
    }
}

KDE_NO_CDTOR_EXPORT RecorderPage::RecorderPage (QWidget *parent, PartBase * player)
 : QFrame (parent), next (0L), m_player (player) {}

KDE_NO_EXPORT void RecorderPage::record () {
    Process * proc = m_player->recorders () [recorderName ()];
    m_player->setRecorder (recorderName ());
    if (!proc->playing ()) {
        dynamic_cast <Recorder *> (proc)->setURL (KURL (m_player->settings ()->recordfile));
        proc->play (m_player->source (), m_player->source ()->current ());
    } else
        proc->stop ();
}

KDE_NO_CDTOR_EXPORT PrefMEncoderPage::PrefMEncoderPage (QWidget *parent, PartBase * player) : RecorderPage (parent, player) {
    QVBoxLayout *layout = new QVBoxLayout (this, 5, 5);
    format = new QButtonGroup (3, Qt::Vertical, i18n ("Format"), this);
    new QRadioButton (i18n ("Same as source"), format);
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

KDE_NO_EXPORT void PrefMEncoderPage::formatClicked (int id) {
    arguments->setEnabled (!!id);
}

KDE_NO_EXPORT void PrefMEncoderPage::record () {
#if KDE_IS_VERSION(3,1,90)
    m_player->settings ()->recordcopy = !format->selectedId ();
#else
    m_player->settings ()->recordcopy = !format->id (format->selected ());
#endif
    RecorderPage::record ();
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
    QVBoxLayout *layout = new QVBoxLayout (this, 5, 5);
    QGridLayout *gridlayout = new QGridLayout (1, 2, 2);
    QLabel *argLabel = new QLabel (i18n("FFMpeg arguments:"), this);
    arguments = new QLineEdit ("", this);
    gridlayout->addWidget (argLabel, 0, 0);
    gridlayout->addWidget (arguments, 0, 1);
    layout->addLayout (gridlayout);
    layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

KDE_NO_EXPORT void PrefFFMpegPage::record () {
    static_cast <FFMpeg *> (m_player->recorders () ["ffmpeg"])->setArguments (arguments->text ());
    RecorderPage::record ();
}

KDE_NO_EXPORT QString PrefFFMpegPage::name () {
    return i18n ("&FFMpeg");
}

KDE_NO_CDTOR_EXPORT PrefGeneralPageOutput::PrefGeneralPageOutput(QWidget *parent, OutputDriver * ad, OutputDriver * vd)
 : QFrame (parent) {
    QGridLayout *layout = new QGridLayout (this, 2, 2, 5);

    videoDriver = new QListBox (this);
    for (int i = 0; vd[i].driver; i++)
        videoDriver->insertItem (vd[i].description, i);
    QWhatsThis::add(videoDriver, i18n("Sets video driver. Recommended is XVideo, or, if it is not supported, X11, which is slower."));
    layout->addWidget (new QLabel (i18n ("Video driver:"), this), 0, 0);
    layout->addWidget (videoDriver, 1, 0);

    audioDriver = new QListBox (this);
    for (int i = 0; ad[i].driver; i++)
        audioDriver->insertItem (ad[i].description, i);
    layout->addWidget (new QLabel (i18n ("Audio driver:"), this), 0, 1);
    layout->addWidget (audioDriver, 1, 1);
    layout->addItem (new QSpacerItem (0, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

KDE_NO_CDTOR_EXPORT PrefOPPageGeneral::PrefOPPageGeneral(QWidget *parent)
: QFrame(parent)
{
    QVBoxLayout *layout = new QVBoxLayout (this, 5);
    layout->setAutoAdd (true);
}

KDE_NO_CDTOR_EXPORT PrefOPPagePostProc::PrefOPPagePostProc(QWidget *parent) : QFrame(parent)
{
    QVBoxLayout *tabLayout = new QVBoxLayout (this, 5);
    postProcessing = new QCheckBox (i18n ("Enable use of postprocessing filters"), this);
    postProcessing->setEnabled( TRUE );
    disablePPauto = new QCheckBox (i18n ("Disable use of postprocessing when watching TV/DVD"), this);

    tabLayout->addWidget( postProcessing );
    tabLayout->addWidget( disablePPauto );
    tabLayout->addItem ( new QSpacerItem( 5, 5, QSizePolicy::Minimum, QSizePolicy::Minimum ) );

    PostprocessingOptions = new QTabWidget( this, "PostprocessingOptions" );
    PostprocessingOptions->setEnabled (true);
    PostprocessingOptions->setAutoMask (false);
    PostprocessingOptions->setTabPosition( QTabWidget::Top );
    PostprocessingOptions->setTabShape( QTabWidget::Rounded );
    PostprocessingOptions->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)1, (QSizePolicy::SizeType)1, PostprocessingOptions->sizePolicy().hasHeightForWidth() ) );

    QWidget *presetSelectionWidget = new QWidget( PostprocessingOptions, "presetSelectionWidget" );
    QGridLayout *presetSelectionWidgetLayout = new QGridLayout( presetSelectionWidget, 1, 1, 1);

    QButtonGroup *presetSelection = new QButtonGroup(3, Qt::Vertical, presetSelectionWidget);
    presetSelection->setInsideSpacing(KDialog::spacingHint());

    defaultPreset = new QRadioButton (i18n ("Default"), presetSelection);
    defaultPreset->setChecked( TRUE );
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

    QGroupBox *customFilters = new QGroupBox(0, Qt::Vertical, customFiltersWidget, "customFilters" );
    customFilters->setSizePolicy(QSizePolicy((QSizePolicy::SizeType)1, (QSizePolicy::SizeType)2));
    customFilters->setFlat(false);
    customFilters->setEnabled( FALSE );
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

    QWhatsThis::add( defaultPreset, i18n( "Enable mplayer's default postprocessing filters" ) );
    QWhatsThis::add( customPreset, i18n( "Enable custom postprocessing filters (See: Custom preset -tab)" ) );
    QWhatsThis::add( fastPreset, i18n( "Enable mplayer's fast postprocessing filters" ) );
    PostprocessingOptions->changeTab( presetSelectionWidget, i18n( "General" ) );
    customFilters->setTitle( QString::null );
    QWhatsThis::add( HzDeblockAQuality, i18n( "Filter is used if there's enough CPU" ) );
    QWhatsThis::add( VtDeblockAQuality, i18n( "Filter is used if there's enough CPU" ) );
    QWhatsThis::add( DeringAQuality, i18n( "Filter is used if there's enough CPU" ) );
    //QWhatsThis::add( TmpNoiseSlider, i18n( "Strength of the noise reducer" ) );
    QWhatsThis::add( AutolevelsFullrange, i18n( "Stretches luminance to full range (0..255)" ) );
    PostprocessingOptions->changeTab( customFiltersWidget, i18n( "Custom Preset" ) );
    deinterlacingGroup->setTitle( QString::null );
    PostprocessingOptions->changeTab( deintSelectionWidget, i18n( "Deinterlacing" ) );
    PostprocessingOptions->adjustSize();
}

KDE_NO_EXPORT void Preferences::confirmDefaults() {
	// TODO: Switch to KMessageBox
	switch( QMessageBox::warning( this, i18n("Reset Settings?"),
        i18n("You are about to have all your settings overwritten with defaults.\nPlease confirm.\n"),
        i18n("&OK"), i18n("&Cancel"), QString::null, 0, 1 ) ){
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
