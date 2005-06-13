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

#ifndef _KMPlayerPREF_H_
#define _KMPlayerPREF_H_

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
class PrefGeneralPageOutput;	// general, output
class PrefOPPageGeneral;	// OP = outputplugins, general
class PrefOPPagePostProc;	// outputplugins, postproc
class PartBase;
class Source;
class Settings;
class PreferencesPage;
class OutputDriver;
class ColorSetting;

class Preferences : public KDialogBase
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
    PrefFFMpegPage              *m_FFMpegPage;
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

class PrefGeneralPageGeneral : public QFrame
{
    Q_OBJECT
public:
    PrefGeneralPageGeneral(QWidget *parent, Settings *);
    ~PrefGeneralPageGeneral() {}

    QCheckBox *keepSizeRatio;
    QButtonGroup *sizesChoice;
    QCheckBox *dockSysTray;
    QCheckBox *loop;
    QCheckBox *showRecordButton;
    QCheckBox *showBroadcastButton;
    QCheckBox *framedrop;
    QCheckBox *adjustvolume;

    QSpinBox *seekTime;
    QComboBox *colorscombo;
    KColorButton *colorbutton;
public slots:
    void colorItemChanged (int);
    void colorCanged (const QColor &);
private:
    ColorSetting * colors;
};

class PrefSourcePageURL : public QFrame
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
    bool changed;
private slots:
    void slotBrowse ();
    void slotTextChanged (const QString &);
};


class PrefRecordPage : public QFrame
{
    Q_OBJECT
public:
    PrefRecordPage (QWidget *parent, PartBase *, RecorderPage *);
    ~PrefRecordPage () {}

    KURLRequester * url;
    QButtonGroup * recorder;
    QButtonGroup * replay;
    QLineEdit * replaytime;
    QLabel * source;
public slots:
    void replayClicked (int id);
private slots:
    void slotRecord ();
    void sourceChanged (KMPlayer::Source *);
    void recordingStarted ();
    void recordingFinished ();
private:
    PartBase * m_player;
    RecorderPage * m_recorders;
    QPushButton * recordButton;
};

class RecorderPage : public QFrame
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

class PrefMEncoderPage : public RecorderPage 
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

class PrefMPlayerDumpstreamPage : public RecorderPage 
{
    Q_OBJECT
public:
    PrefMPlayerDumpstreamPage (QWidget *parent, PartBase *);
    ~PrefMPlayerDumpstreamPage () {}

    QString name ();
    const char * recorderName () { return "mplayerdumpstream"; }

    QLineEdit * arguments;
    QButtonGroup * format;
private:
};

class PrefFFMpegPage : public RecorderPage
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


class PrefGeneralPageOutput : public QFrame
{
    Q_OBJECT
public:
    PrefGeneralPageOutput (QWidget *parent, OutputDriver * ad, OutputDriver * vd);
    ~PrefGeneralPageOutput() {}

    QListBox *videoDriver;
    QListBox *audioDriver;
};

class PrefOPPageGeneral : public QFrame
{
    Q_OBJECT
public:
    PrefOPPageGeneral(QWidget *parent = 0);
    ~PrefOPPageGeneral() {}
};

class PrefOPPagePostProc : public QFrame
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
