/*
    SPDX-FileCopyrightText: 2003 Joonas Koivunen <rzei@mbnet.fi>
    SPDX-FileCopyrightText: 2003 Koos Vriezen <koos.vriezen@xs4all.nl>

    SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _KMPlayerPREF_H_
#define _KMPlayerPREF_H_

#include "config-kmplayer.h"

#include <KPageDialog>
#include <QMap>

#include "kmplayerplaylist.h"

class QTabWidget;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QRadioButton;
class QSlider;
class QSpinBox;
class QColor;
class QButtonGroup;
class QListWidget;
class KHistoryCombo;
class KComboBox;
class KUrlRequester;
class KColorButton;
class KPageWidgetItem;

namespace KMPlayer {

class PrefGeneralPageGeneral;   // general, general
class PrefSourcePageURL;        // source, url
class PrefRecordPage;           // recording
class RecorderPage;                     // base recorder
class PrefMEncoderPage;         // mencoder
class PrefMPlayerDumpstreamPage; // mplayer -dumpstream
class PrefFFMpegPage;           // ffmpeg
class PrefXinePage;             // xine url:record
class PrefGeneralPageLooks;     // general, looks
class PrefGeneralPageOutput;	// general, output
class PrefOPPagePostProc;	// outputplugins, postproc
class PartBase;
class Source;
class Settings;
class PreferencesPage;
class OutputDriver;
class ColorSetting;
class FontSetting;

class Preferences : public KPageDialog
{
    Q_OBJECT
public:

    Preferences(PartBase *, Settings *);
    ~Preferences() override;

    PrefGeneralPageGeneral      *m_GeneralPageGeneral;
    PrefSourcePageURL           *m_SourcePageURL;
    PrefRecordPage              *m_RecordPage;
    PrefMEncoderPage            *m_MEncoderPage;
    PrefMPlayerDumpstreamPage   *m_MPlayerDumpstreamPage;
#ifdef KMPLAYER_WITH_XINE
    PrefXinePage                *m_XinePage;
#warning foo
#endif
    PrefFFMpegPage              *m_FFMpegPage;
    PrefGeneralPageLooks        *m_GeneralPageLooks;
    PrefGeneralPageOutput       *m_GeneralPageOutput;
    PrefOPPagePostProc		*m_OPPagePostproc;
    void setDefaults();
    void setPage (const char *);
    void addPrefPage (PreferencesPage *);
    void removePrefPage (PreferencesPage *);

    RecorderPage * recorders;
    QMap<QString, QTabWidget *> entries;
public Q_SLOTS:
    void confirmDefaults();
private:
    KPageWidgetItem *m_record_item;
    KPageWidgetItem *m_url_item;
};

class PrefGeneralPageGeneral : public QWidget
{
    Q_OBJECT
public:
    PrefGeneralPageGeneral(QWidget *parent, Settings *);
    ~PrefGeneralPageGeneral() override {}

    QCheckBox *keepSizeRatio;
    QCheckBox * autoResize;
    QButtonGroup* sizesChoice;
    QCheckBox *dockSysTray;
    QCheckBox *loop;
    QCheckBox *showConfigButton;
    QCheckBox *showPlaylistButton;
    QCheckBox *showRecordButton;
    QCheckBox *showBroadcastButton;
    QCheckBox *framedrop;
    QCheckBox *adjustvolume;
    QCheckBox *adjustcolors;

    QSpinBox *seekTime;
};

class PrefGeneralPageLooks : public QWidget {
    Q_OBJECT
public:
    PrefGeneralPageLooks (QWidget *parent, Settings *);
    ~PrefGeneralPageLooks () override {}
    QComboBox *colorscombo;
    KColorButton *colorbutton;
    QComboBox *fontscombo;
    QPushButton *fontbutton;
public Q_SLOTS:
    void colorItemChanged (int);
    void colorCanged (const QColor &);
    void fontItemChanged (int);
    void fontClicked ();
private:
    ColorSetting * colors;
    FontSetting * fonts;
};

class PrefSourcePageURL : public QWidget
{
    Q_OBJECT
public:
    PrefSourcePageURL (QWidget *parent);
    ~PrefSourcePageURL () override {}

    KUrlRequester * url;
    //KHistoryCombo * url;
    KComboBox * urllist;
    KUrlRequester * sub_url;
    KComboBox * sub_urllist;
    QListWidget* backend;
    QCheckBox * clicktoplay;
    QCheckBox * grabhref;
    QLineEdit * prefBitRate;
    QLineEdit * maxBitRate;
    bool changed;
private Q_SLOTS:
    void slotBrowse ();
    void slotTextChanged (const QString &);
};


class PrefRecordPage : public QWidget
{
    Q_OBJECT
public:
    PrefRecordPage (QWidget *parent, PartBase *, RecorderPage *, int len);
    ~PrefRecordPage () override;

    KUrlRequester * url;
    QButtonGroup* recorder;
    QButtonGroup* replay;
    QSpinBox* replaytime;
    QLabel * source;
protected:
    void showEvent (QShowEvent *) override;
public Q_SLOTS:
    void replayClicked (int id);
    void recorderClicked (int id);
private Q_SLOTS:
    void slotRecord ();
    void recording (bool);
private:
    PartBase * m_player;
    RecorderPage *m_recorders;
    QPushButton * recordButton;
    QString source_url;
    int m_recorders_length;
};

class RecorderPage : public QWidget
{
    Q_OBJECT
public:
    RecorderPage (QWidget *parent, PartBase *);
    ~RecorderPage () override {}
    virtual void startRecording () {}
    virtual QString name () = 0;
    virtual const char * recorderName () = 0;
    RecorderPage * next;
protected:
    PartBase *m_player;
};

class PrefMEncoderPage : public RecorderPage
{
    Q_OBJECT
public:
    PrefMEncoderPage (QWidget *parent, PartBase *);
    ~PrefMEncoderPage () override {}

    void startRecording () override;
    QString name () override;
    const char * recorderName () override { return "mencoder"; }

    QLineEdit * arguments;
    QButtonGroup* format;
public Q_SLOTS:
    void formatClicked (int id);
private:
};

class PrefMPlayerDumpstreamPage : public RecorderPage {
public:
    PrefMPlayerDumpstreamPage (QWidget *parent, PartBase *);
    ~PrefMPlayerDumpstreamPage () override {}

    QString name () override;
    const char * recorderName () override { return "mplayerdumpstream"; }
};

#ifdef KMPLAYER_WITH_XINE
class PrefXinePage : public RecorderPage {
public:
    PrefXinePage (QWidget *parent, PartBase *);
    ~PrefXinePage () {}

    QString name ();
    const char * recorderName () { return "xine"; }
};
#endif

class PrefFFMpegPage : public RecorderPage
{
    Q_OBJECT
public:
    PrefFFMpegPage (QWidget *parent, PartBase *);
    ~PrefFFMpegPage () override {}

    void startRecording () override;
    QString name () override;
    const char * recorderName () override { return "ffmpeg"; }

    QLineEdit * arguments;
    QButtonGroup* format;
private:
};


class PrefGeneralPageOutput : public QWidget
{
    Q_OBJECT
public:
    PrefGeneralPageOutput (QWidget *parent, OutputDriver * ad, OutputDriver * vd);
    ~PrefGeneralPageOutput() override {}

    QListWidget* videoDriver;
    QListWidget* audioDriver;
};

class PrefOPPagePostProc : public QWidget
{
    Q_OBJECT
public:
    PrefOPPagePostProc(QWidget *parent = nullptr);
    ~PrefOPPagePostProc() override {}

    QCheckBox* postProcessing;
    QCheckBox* disablePPauto;
    QTabWidget* PostprocessingOptions;

    QRadioButton* defaultPreset;
    QRadioButton* customPreset;
    QRadioButton* fastPreset;

    QCheckBox* HzDeblockFilter;
    QCheckBox* VtDeblockFilter;
    QCheckBox* DeringFilter;
    QCheckBox* HzDeblockAQuality;
    QCheckBox* VtDeblockAQuality;
    QCheckBox* DeringAQuality;

    QCheckBox* AutolevelsFilter;
    QCheckBox* AutolevelsFullrange;
    QCheckBox* HzDeblockCFiltering;
    QCheckBox* VtDeblockCFiltering;
    QCheckBox* DeringCFiltering;
    QCheckBox* TmpNoiseFilter;
    QSlider* TmpNoiseSlider;

    QCheckBox* LinBlendDeinterlacer;
    QCheckBox* CubicIntDeinterlacer;
    QCheckBox* LinIntDeinterlacer;
    QCheckBox* MedianDeinterlacer;
    QCheckBox* FfmpegDeinterlacer;
};

} // namespace

#endif // _KMPlayerPREF_H_
