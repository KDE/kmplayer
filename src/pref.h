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

#include "kmplayer_def.h"

#include <kdialogbase.h>
#include <qframe.h>
#include <qmap.h>

class QTabWidget;
class QTable;
class QGroupBox;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QRadioButton;
class QSlider;
class QSpinBox;
class QColor;
class QButtonGroup;
class KHistoryCombo;
class KComboBox;
class KURLRequester;
class KColorButton;

namespace KMPlayer {
    
class PrefGeneralPageGeneral; 	// general, general
class PrefSourcePageURL;        // source, url
class PrefRecordPage;           // recording
class RecorderPage;                     // base recorder
class PrefMEncoderPage;         // mencoder
class PrefMPlayerDumpstreamPage; // mplayer -dumpstream
class PrefFFMpegPage;           // ffmpeg
class PrefXinePage;             // xine url:record
class PrefGeneralPageLooks; 	// general, looks
class PrefGeneralPageOutput;	// general, output
class PrefOPPageGeneral;	// OP = outputplugins, general
class PrefOPPagePostProc;	// outputplugins, postproc
class PartBase;
class Source;
class Settings;
class PreferencesPage;
class OutputDriver;
class ColorSetting;
class FontSetting;

class KMPLAYER_NO_EXPORT Preferences : public KDialogBase
{
    Q_OBJECT
public:

    Preferences(PartBase *, Settings *);
    ~Preferences();

    PrefGeneralPageGeneral 	*m_GeneralPageGeneral;
    PrefSourcePageURL 		*m_SourcePageURL;
    PrefRecordPage 		*m_RecordPage;
    PrefMEncoderPage            *m_MEncoderPage;
    PrefMPlayerDumpstreamPage   *m_MPlayerDumpstreamPage;
#ifdef HAVE_XINE
    PrefXinePage                *m_XinePage;
#endif
    PrefFFMpegPage              *m_FFMpegPage;
    PrefGeneralPageLooks 	*m_GeneralPageLooks;
    PrefGeneralPageOutput 	*m_GeneralPageOutput;
    PrefOPPageGeneral 		*m_OPPageGeneral;
    PrefOPPagePostProc		*m_OPPagePostproc;
    void setDefaults();
    void setPage (const char *);
    void addPrefPage (PreferencesPage *);
    void removePrefPage (PreferencesPage *);

    RecorderPage * recorders;
    QMap<QString, QTabWidget *> entries;
public slots:
    void confirmDefaults();
};

class KMPLAYER_NO_EXPORT PrefGeneralPageGeneral : public QFrame
{
    Q_OBJECT
public:
    PrefGeneralPageGeneral(QWidget *parent, Settings *);
    ~PrefGeneralPageGeneral() {}

    QCheckBox *keepSizeRatio;
    QCheckBox * autoResize;
    QButtonGroup *sizesChoice;
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

class KMPLAYER_NO_EXPORT PrefGeneralPageLooks : public QFrame {
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

class KMPLAYER_NO_EXPORT PrefSourcePageURL : public QFrame
{
    Q_OBJECT
public:
    PrefSourcePageURL (QWidget *parent);
    ~PrefSourcePageURL () {}

    KURLRequester * url;
    //KHistoryCombo * url;
    KComboBox * urllist;
    KURLRequester * sub_url;
    KComboBox * sub_urllist;
    QListBox * backend;
    QCheckBox * allowhref;
    QLineEdit * prefBitRate;
    QLineEdit * maxBitRate;
    bool changed;
private slots:
    void slotBrowse ();
    void slotTextChanged (const QString &);
};


class KMPLAYER_NO_EXPORT PrefRecordPage : public QFrame
{
    Q_OBJECT
public:
    PrefRecordPage (QWidget *parent, PartBase *, RecorderPage *, int len);
    ~PrefRecordPage () {}

    KURLRequester * url;
    QButtonGroup * recorder;
    QButtonGroup * replay;
    QLineEdit * replaytime;
    QLabel * source;
public slots:
    void replayClicked (int id);
    void recorderClicked (int id);
private slots:
    void slotRecord ();
    void slotNotPlaying ();
    void sourceChanged (KMPlayer::Source *, KMPlayer::Source *);
    void recordingStarted ();
    void recordingFinished ();
private:
    PartBase * m_player;
    RecorderPage * m_recorders;
    QPushButton * recordButton;
    int m_recorders_length;
};

class KMPLAYER_NO_EXPORT RecorderPage : public QFrame
{
    Q_OBJECT
public:
    RecorderPage (QWidget *parent, PartBase *);
    virtual ~RecorderPage () {};
    virtual void record ();
    virtual QString name () = 0;
    virtual const char * recorderName () = 0;
    RecorderPage * next;
protected:
    PartBase * m_player;
};

class KMPLAYER_NO_EXPORT PrefMEncoderPage : public RecorderPage 
{
    Q_OBJECT
public:
    PrefMEncoderPage (QWidget *parent, PartBase *);
    ~PrefMEncoderPage () {}

    void record ();
    QString name ();
    const char * recorderName () { return "mencoder"; }

    QLineEdit * arguments;
    QButtonGroup * format;
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

#ifdef HAVE_XINE
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

    void record ();
    QString name ();
    const char * recorderName () { return "ffmpeg"; }

    QLineEdit * arguments;
    QButtonGroup * format;
private:
};


class KMPLAYER_NO_EXPORT PrefGeneralPageOutput : public QFrame
{
    Q_OBJECT
public:
    PrefGeneralPageOutput (QWidget *parent, OutputDriver * ad, OutputDriver * vd);
    ~PrefGeneralPageOutput() {}

    QListBox *videoDriver;
    QListBox *audioDriver;
};

class KMPLAYER_NO_EXPORT PrefOPPageGeneral : public QFrame
{
    Q_OBJECT
public:
    PrefOPPageGeneral(QWidget *parent = 0);
    ~PrefOPPageGeneral() {}
};

class KMPLAYER_NO_EXPORT PrefOPPagePostProc : public QFrame
{
    Q_OBJECT
public:
    PrefOPPagePostProc(QWidget *parent = 0);
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
