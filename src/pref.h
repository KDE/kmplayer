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

#define VDRIVER_XV_INDEX 0
#define VDRIVER_XV "XV"
#define VDRIVER_X11_INDEX 1
#define VDRIVER_X11 "X11Shm"
#define VDRIVER_XVIDIX_INDEX 2
#define VDRIVER_XVIDIX "XVidix" // this is wrong.. they like it to be in lowercase
#ifndef _KMPlayerPREF_H_
#define _KMPlayerPREF_H_

#include <list>
#include <vector>

#include <kdialogbase.h>
#include <qframe.h>
#include <qptrlist.h>
#include <qregexp.h>
#include <qcheckbox.h>
#include <qtextedit.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <qradiobutton.h>
#include <qslider.h>
#include <qspinbox.h>
#include <qstringlist.h>
#include <kurlrequester.h>

#include "kmplayersource.h"

class KMPlayerPrefGeneralPageGeneral; 	// general, general
class KMPlayerPrefSourcePageURL;        // source, url
class KMPlayerPrefGeneralPageDVD;	// general, dvd
class KMPlayerPrefGeneralPageVCD;	// general, vcd
class KMPlayerPrefRecordPage;           // recording
class RecorderPage;                     // base recorder
class KMPlayerPrefMEncoderPage;         // mencoder
class KMPlayerPrefFFMpegPage;           // ffmpeg
class KMPlayerPrefBroadcastPage;        // broadcast
class KMPlayerPrefBroadcastFormatPage;  // broadcast format
class KMPlayerPrefGeneralPageOutput;	// general, output
class KMPlayerPrefGeneralPageAdvanced;	// general, advanced, pattern matches etc.
class KMPlayerPrefOPPageGeneral;	// OP = outputplugins, general
class KMPlayerPrefOPPagePostProc;	// outputplugins, postproc
class KMPlayer;
class QTabWidget;
class QTable;
class QGroupBox;
class KHistoryCombo;
class KComboBox;

typedef std::list<RecorderPage*> RecorderList;

class MPlayerAudioDriver {
public:
    const char * audiodriver;
    const QString description;
};

template <class T>
void Deleter (T * t) {
    delete t;
}

class FFServerSetting {
public:
    FFServerSetting () {}
    FFServerSetting (int i, const QString & n, const QString & f, const QString & ac, int abr, int asr, const QString & vc, int vbr, int q, int fr, int gs, int w, int h);
    FFServerSetting (const QStringList & sl) { *this = sl; }
    int index;
    QString name;
    QString format;
    QString audiocodec;
    QString audiobitrate;
    QString audiosamplerate;
    QString videocodec;
    QString videobitrate;
    QString quality;
    QString framerate;
    QString gopsize;
    QString width;
    QString height;
    QStringList acl;
    FFServerSetting & operator = (const QStringList &);
    FFServerSetting & operator = (const FFServerSetting & fs);
    const QStringList list ();
    QString & ffconfig (QString & buf);
};

typedef std::vector <FFServerSetting *> FFServerSettingList;

struct PrefSubEntry {
    PrefSubEntry (const QString &, QFrame *, KMPlayerSource *); 
    QString name;
    QFrame * frame;
    KMPlayerSource * source;
};

typedef std::list <PrefSubEntry *> TabList;

struct PrefEntry {
    PrefEntry (const QString &, const QString &, QFrame *, QTabWidget *);
    QString name;
    QString icon;
    TabList tabs;
    QFrame * frame;
    QTabWidget * tab;
};

typedef std::list <PrefEntry *> PrefEntryList;


class KMPlayerPreferences : public KDialogBase
{
    Q_OBJECT
public:

    KMPlayerPreferences(KMPlayer *, MPlayerAudioDriver * ad, FFServerSettingList &);
    ~KMPlayerPreferences();

    KMPlayerPrefGeneralPageGeneral 	*m_GeneralPageGeneral;
    KMPlayerPrefSourcePageURL 		*m_SourcePageURL;
    KMPlayerPrefGeneralPageDVD 		*m_GeneralPageDVD;
    KMPlayerPrefGeneralPageVCD 		*m_GeneralPageVCD;
    KMPlayerPrefRecordPage 		*m_RecordPage;
    KMPlayerPrefMEncoderPage            *m_MEncoderPage;
    KMPlayerPrefFFMpegPage              *m_FFMpegPage;
    KMPlayerPrefBroadcastPage 		*m_BroadcastPage;
    KMPlayerPrefBroadcastFormatPage 	*m_BroadcastFormatPage;
    KMPlayerPrefGeneralPageOutput 	*m_GeneralPageOutput;
    KMPlayerPrefGeneralPageAdvanced	*m_GeneralPageAdvanced;
    KMPlayerPrefOPPageGeneral 		*m_OPPageGeneral;
    KMPlayerPrefOPPagePostProc		*m_OPPagePostproc;
    void setDefaults();
    void setPage (const char *);

    RecorderList recorders;
    PrefEntryList entries;
public slots:
    void confirmDefaults();
};

class KMPlayerPrefGeneralPageGeneral : public QFrame
{
    Q_OBJECT
public:
    KMPlayerPrefGeneralPageGeneral(QWidget *parent = 0);
    ~KMPlayerPrefGeneralPageGeneral() {}

    QCheckBox *keepSizeRatio;
    QCheckBox *showConsoleOutput;
    QCheckBox *loop;
    QCheckBox *showControlButtons;
    QCheckBox *showRecordButton;
    QCheckBox *showBroadcastButton;
    QCheckBox *autoHideControlButtons;
    QCheckBox *showPositionSlider;
    QCheckBox *autoHideSlider;
    QCheckBox *alwaysBuildIndex;
    QCheckBox *framedrop;

    QSpinBox *seekTime;

};

class KMPlayerPrefSourcePageURL : public QFrame
{
    Q_OBJECT
public:
    KMPlayerPrefSourcePageURL (QWidget *parent);
    ~KMPlayerPrefSourcePageURL () {}

    KURLRequester * url;
    //KHistoryCombo * url;
    KComboBox * urllist;
    KURLRequester * sub_url;
    KComboBox * sub_urllist;
    QComboBox * backend;
    QCheckBox * allowhref;
    bool changed;
private slots:
    void slotBrowse ();
    void slotTextChanged (const QString &);
};

class KMPlayerPrefGeneralPageDVD : public QFrame
{
    Q_OBJECT
public:
    KMPlayerPrefGeneralPageDVD(QWidget *parent = 0);
    ~KMPlayerPrefGeneralPageDVD() {}

    QCheckBox *autoPlayDVD;
    KURLRequester *dvdDevicePath;

};

class KMPlayerPrefGeneralPageVCD : public QFrame
{
    Q_OBJECT
public:
    KMPlayerPrefGeneralPageVCD(QWidget *parent = 0);
    ~KMPlayerPrefGeneralPageVCD() {}
    KURLRequester *vcdDevicePath;
    QCheckBox *autoPlayVCD;

};


class KMPlayerPrefRecordPage : public QFrame
{
    Q_OBJECT
public:
    KMPlayerPrefRecordPage (QWidget *parent, KMPlayer *, RecorderList &);
    ~KMPlayerPrefRecordPage () {}

    KURLRequester * url;
    QButtonGroup * recorder;
    QButtonGroup * replay;
    QLineEdit * replaytime;
    QLabel * source;
public slots:
    void replayClicked (int id);
private slots:
    void slotRecord ();
    void sourceChanged (KMPlayerSource *);
    void recordingStarted ();
    void recordingFinished ();
private:
    KMPlayer * m_player;
    RecorderList & m_recorders;
    QPushButton * recordButton;
};

class RecorderPage : public QFrame
{
    Q_OBJECT
public:
    RecorderPage (QWidget *parent, KMPlayer *);
    virtual ~RecorderPage () {};
    virtual void record () = 0;
    virtual QString name () = 0;
    virtual bool sourceSupported (KMPlayerSource *) = 0;
protected:
    KMPlayer * m_player;
};

class KMPlayerPrefMEncoderPage : public RecorderPage 
{
    Q_OBJECT
public:
    KMPlayerPrefMEncoderPage (QWidget *parent, KMPlayer *);
    ~KMPlayerPrefMEncoderPage () {}

    void record ();
    QString name ();
    bool sourceSupported (KMPlayerSource *);

    QLineEdit * arguments;
    QButtonGroup * format;
public slots:
    void formatClicked (int id);
private:
};

class KMPlayerPrefFFMpegPage : public RecorderPage
{
    Q_OBJECT
public:
    KMPlayerPrefFFMpegPage (QWidget *parent, KMPlayer *);
    ~KMPlayerPrefFFMpegPage () {}

    void record ();
    QString name ();
    bool sourceSupported (KMPlayerSource *);

    QLineEdit * arguments;
    QButtonGroup * format;
private:
};

class KMPlayerPrefBroadcastPage : public QFrame
{
    Q_OBJECT
public:
    KMPlayerPrefBroadcastPage (QWidget *parent);
    ~KMPlayerPrefBroadcastPage () {}

    QLineEdit * bindaddress;
    QLineEdit * port;
    QLineEdit * maxclients;
    QLineEdit * maxbandwidth;
    QLineEdit * feedfile;
    QLineEdit * feedfilesize;
};

class KMPlayerPrefBroadcastFormatPage : public QFrame
{
    Q_OBJECT
public:
    KMPlayerPrefBroadcastFormatPage (QWidget *parent, FFServerSettingList &);
    ~KMPlayerPrefBroadcastFormatPage () {}

    QListBox * profilelist;
    QComboBox * format;
    QLineEdit * audiocodec;
    QLineEdit * audiobitrate;
    QLineEdit * audiosamplerate;
    QLineEdit * videocodec;
    QLineEdit * videobitrate;
    QLineEdit * quality;
    QLineEdit * framerate;
    QLineEdit * gopsize;
    QLineEdit * moviewidth;
    QLineEdit * movieheight;
    QLineEdit * profile;
    void setSettings (const FFServerSetting &);
    void getSettings (FFServerSetting &);
private slots:
    void slotIndexChanged (int index);
    void slotItemHighlighted (int index);
    void slotTextChanged (const QString &);
    void slotLoad ();
    void slotSave ();
    void slotDelete ();
private:
    QTable * accesslist;
    QPushButton * load;
    QPushButton * save;
    QPushButton * del;
    FFServerSettingList & profiles;
};

class KMPlayerPrefGeneralPageOutput : public QFrame
{
    Q_OBJECT
public:
    KMPlayerPrefGeneralPageOutput(QWidget *parent,  MPlayerAudioDriver * ad);
    ~KMPlayerPrefGeneralPageOutput() {}

    QComboBox *videoDriver;
    QComboBox *audioDriver;
};

class KMPlayerPrefOPPageGeneral : public QFrame
{
    Q_OBJECT
public:
    KMPlayerPrefOPPageGeneral(QWidget *parent = 0);
    ~KMPlayerPrefOPPageGeneral() {}
};

class KMPlayerPrefOPPagePostProc : public QFrame
{
    Q_OBJECT
public:
    KMPlayerPrefOPPagePostProc(QWidget *parent = 0);
    ~KMPlayerPrefOPPagePostProc() {}

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

class KMPlayerPrefGeneralPageAdvanced : public QFrame
{
    Q_OBJECT
public:
    KMPlayerPrefGeneralPageAdvanced(QWidget *parent = 0);
    ~KMPlayerPrefGeneralPageAdvanced() {}
    QLineEdit *dvdLangPattern;
    QLineEdit *dvdTitlePattern;
    QLineEdit *dvdSubPattern;
    QLineEdit *dvdChapPattern;
    QLineEdit *vcdTrackPattern;
    QLineEdit *sizePattern;
    QLineEdit *cachePattern;
    QLineEdit *startPattern;
    QLineEdit *indexPattern;
    QLineEdit *referenceURLPattern;
    QLineEdit *referencePattern;
    QLineEdit *additionalArguments;
    QSpinBox *cacheSize;
};

#endif // _KMPlayerPREF_H_
