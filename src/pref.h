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

#ifndef _KMPlayerPREF_H_
#define _KMPlayerPREF_H_

#include "config-kmplayer.h"

#include "kmplayer_def.h"

#include <kpagedialog.h>
#include <qmap.h>

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

class KMPLAYER_NO_EXPORT Preferences : public KPageDialog
{
    Q_OBJECT
public:

    Preferences(PartBase *, Settings *);
    ~Preferences();

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
public slots:
    void confirmDefaults();
private:
    KPageWidgetItem *m_record_item;
    KPageWidgetItem *m_url_item;
};

class KMPLAYER_NO_EXPORT PrefGeneralPageGeneral : public QWidget
{
    Q_OBJECT
public:
    PrefGeneralPageGeneral(QWidget *parent, Settings *);
    ~PrefGeneralPageGeneral() {}

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

class KMPLAYER_NO_EXPORT PrefGeneralPageLooks : public QWidget {
    Q_OBJECT
public:
    PrefGeneralPageLooks (QWidget *parent, Settings *);
    ~PrefGeneralPageLooks () {}
    QComboBox *colorscombo;
    KColorButton *colorbutton;
    QComboBox *fontscombo;
    QPushButton *fontbutton;
public slots:
    void colorItemChanged (int);
    void colorCanged (const QColor &);
    void fontItemChanged (int);
    void fontClicked ();
private:
    ColorSetting * colors;
    FontSetting * fonts;
};

class KMPLAYER_NO_EXPORT PrefSourcePageURL : public QWidget
{
    Q_OBJECT
public:
    PrefSourcePageURL (QWidget *parent);
    ~PrefSourcePageURL () {}

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
private slots:
    void slotBrowse ();
    void slotTextChanged (const QString &);
};


class KMPLAYER_NO_EXPORT PrefRecordPage : public QWidget
{
    Q_OBJECT
public:
    PrefRecordPage (QWidget *parent, PartBase *, RecorderPage *, int len);
    ~PrefRecordPage ();

    KUrlRequester * url;
    QButtonGroup* recorder;
    QButtonGroup* replay;
    QSpinBox* replaytime;
    QLabel * source;
protected:
    void showEvent (QShowEvent *);
public slots:
    void replayClicked (int id);
    void recorderClicked (int id);
private slots:
    void slotRecord ();
    void recording (bool);
private:
    PartBase * m_player;
    RecorderPage *m_recorders;
    QPushButton * recordButton;
    QString source_url;
    int m_recorders_length;
};

class KMPLAYER_NO_EXPORT RecorderPage : public QWidget
{
    Q_OBJECT
public:
    RecorderPage (QWidget *parent, PartBase *);
    virtual ~RecorderPage () {}
    virtual void startRecording () {}
    virtual QString name () = 0;
    virtual const char * recorderName () = 0;
    RecorderPage * next;
protected:
    PartBase *m_player;
};

class KMPLAYER_NO_EXPORT PrefMEncoderPage : public RecorderPage
{
    Q_OBJECT
public:
    PrefMEncoderPage (QWidget *parent, PartBase *);
    ~PrefMEncoderPage () {}

    virtual void startRecording ();
    QString name ();
    const char * recorderName () { return "mencoder"; }

    QLineEdit * arguments;
    QButtonGroup* format;
public slots:
    void formatClicked (int id);
private:
};

class KMPLAYER_NO_EXPORT PrefMPlayerDumpstreamPage : public RecorderPage {
public:
    PrefMPlayerDumpstreamPage (QWidget *parent, PartBase *);
    ~PrefMPlayerDumpstreamPage () {}

    QString name ();
    const char * recorderName () { return "mplayerdumpstream"; }
};

#ifdef KMPLAYER_WITH_XINE
class KMPLAYER_NO_EXPORT PrefXinePage : public RecorderPage {
public:
    PrefXinePage (QWidget *parent, PartBase *);
    ~PrefXinePage () {}

    QString name ();
    const char * recorderName () { return "xine"; }
};
#endif

class KMPLAYER_NO_EXPORT PrefFFMpegPage : public RecorderPage
{
    Q_OBJECT
public:
    PrefFFMpegPage (QWidget *parent, PartBase *);
    ~PrefFFMpegPage () {}

    virtual void startRecording ();
    QString name ();
    const char * recorderName () { return "ffmpeg"; }

    QLineEdit * arguments;
    QButtonGroup* format;
private:
};


class KMPLAYER_NO_EXPORT PrefGeneralPageOutput : public QWidget
{
    Q_OBJECT
public:
    PrefGeneralPageOutput (QWidget *parent, OutputDriver * ad, OutputDriver * vd);
    ~PrefGeneralPageOutput() {}

    QListWidget* videoDriver;
    QListWidget* audioDriver;
};

class KMPLAYER_NO_EXPORT PrefOPPagePostProc : public QWidget
{
    Q_OBJECT
public:
    PrefOPPagePostProc(QWidget *parent = nullptr);
    ~PrefOPPagePostProc() {}

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
