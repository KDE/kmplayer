/* This file is part of the KDE project
 *
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

#define ADRIVER_DEFAULT_INDEX 0
#define ADRIVER_DEFAULT "Default"
#define ADRIVER_OSS_INDEX 1
#define ADRIVER_OSS "Oss"
#define ADRIVER_SDL_INDEX 2
#define ADRIVER_SDL "SDL"
#define ADRIVER_ALSA_INDEX 3
#define ADRIVER_ALSA "ALSA"
#define ADRIVER_ARTS_INDEX 4
#define ADRIVER_ARTS "Arts"

#define VDRIVER_XV_INDEX 0
#define VDRIVER_XV "XV"
#define VDRIVER_X11_INDEX 1
#define VDRIVER_X11 "X11Shm"
#define VDRIVER_XVIDIX_INDEX 2
#define VDRIVER_XVIDIX "XVidix" // this is wrong.. they like it to be in lowercase
#ifndef _KMPlayerPREF_H_
#define _KMPlayerPREF_H_

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
class KMPlayerPrefSourcePageTV;         // source, TV
class KMPlayerPrefBroadcastPage;        // broadcast
class KMPlayerPrefBroadcastACLPage;     // broadcast ACL
class KMPlayerPrefGeneralPageOutput;	// general, output
class KMPlayerPrefGeneralPageAdvanced;	// general, advanced, pattern matches etc.
class KMPlayerPrefOPPageGeneral;	// OP = outputplugins, general
class KMPlayerPrefOPPagePostProc;	// outputplugins, postproc
class QTabWidget;
class QTable;
class QGroupBox;


class TVChannel {
public:
    TVChannel (const QString & n, int f);
    QString name;
    int frequency;
};

class TVInput {
public:
    TVInput (const QString & n, int id);
    QString name;
    int id;
    bool hastuner;
    QString norm;
    QPtrList <TVChannel> channels;
};

class TVDevice {
public:
    TVDevice (const QString & d, const QSize & size);
    QString device;
    QString audiodevice;
    QString name;
    QSize minsize;
    QSize maxsize;
    QSize size;
    bool noplayback;
    QPtrList <TVInput> inputs;
};

class FFServerSetting {
public:
    FFServerSetting () {}
    FFServerSetting (int i, const QString & n, const QString & f, const QString & ac, int abr, int asr, const QString & vc, int vbr, int q, int fr, int gs, int w, int h);
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
    FFServerSetting & operator = (const QStringList &);
    FFServerSetting & operator = (const FFServerSetting & fs);
    const QStringList list ();
    QString & ffconfig (QString & buf);
};

class TVDeviceScannerSource : public KMPlayerSource {
    Q_OBJECT
public:
    TVDeviceScannerSource (KMPlayer * player);
    virtual void init ();
    virtual bool processOutput (const QString & line);
    virtual QString filterOptions ();
    virtual bool hasLength ();
    virtual bool isSeekable ();
    virtual bool scan (const QString & device, const QString & driver);
    public slots:
        virtual void activate ();
    virtual void deactivate ();
    virtual void play ();
    void finished ();
signals:
    void scanFinished (TVDevice * tvdevice);
private:
    TVDevice * m_tvdevice;
    KMPlayerSource * m_source;
    QString m_driver;
    QRegExp m_nameRegExp;
    QRegExp m_sizesRegExp;
    QRegExp m_inputRegExp;
};


class KMPlayerPreferences : public KDialogBase
{
    Q_OBJECT
public:
    KMPlayerPreferences(QWidget *parent, FFServerSetting * ffs);
    ~KMPlayerPreferences();

    KMPlayerPrefGeneralPageGeneral 	*m_GeneralPageGeneral;
    KMPlayerPrefSourcePageURL 		*m_SourcePageURL;
    KMPlayerPrefGeneralPageDVD 		*m_GeneralPageDVD;
    KMPlayerPrefGeneralPageVCD 		*m_GeneralPageVCD;
    KMPlayerPrefSourcePageTV 		*m_SourcePageTV;
    KMPlayerPrefBroadcastPage 		*m_BroadcastPage;
    KMPlayerPrefBroadcastACLPage 	*m_BroadcastACLPage;
    KMPlayerPrefGeneralPageOutput 	*m_GeneralPageOutput;
    KMPlayerPrefGeneralPageAdvanced	*m_GeneralPageAdvanced;
    KMPlayerPrefOPPageGeneral 		*m_OPPageGeneral;
    KMPlayerPrefOPPagePostProc		*m_OPPagePostproc;
    void setDefaults();
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

    QLineEdit *url;
private slots:
    void slotBrowse ();
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

class KMPlayerPrefSourcePageTVDevice : public QFrame
{
    Q_OBJECT
public:
    KMPlayerPrefSourcePageTVDevice (QWidget *parent, TVDevice * dev);
    ~KMPlayerPrefSourcePageTVDevice () {}

    QLineEdit * name;
    QLineEdit * audiodevice;
    QLineEdit * sizewidth;
    QLineEdit * sizeheight;
    QCheckBox * noplayback;
    TVDevice * device;
    void updateTVDevice ();
signals:
    void deleted (QFrame *);
private slots:
    void slotDelete ();
private:
    QTabWidget * inputsTab;
};

class KMPlayerPrefSourcePageTV : public QFrame
{
    Q_OBJECT
public:
    KMPlayerPrefSourcePageTV (QWidget *parent, KMPlayerPreferences * pref);
    ~KMPlayerPrefSourcePageTV () {}

    QLineEdit * driver;
    QLineEdit * device;
    TVDeviceScannerSource * scanner;
    void setTVDevices (QPtrList <TVDevice> * devs);
    void updateTVDevices ();
private slots:
    void slotScan ();
    void slotScanFinished (TVDevice * device);
    void slotDeviceDeleted (QFrame *);
private:
    void addPage (TVDevice *, bool show=false);
    TVDevice * findDevice (QPtrList <TVDevice> & list, const QString & device);
    QPtrList <TVDevice> * m_devices;
    QPtrList <TVDevice> deleteddevices;
    QPtrList <TVDevice> addeddevices;
    QPtrList <QFrame> m_devicepages;
    KMPlayerPreferences * m_preference;
};

class KMPlayerPrefBroadcastPage : public QFrame
{
    Q_OBJECT
public:
    KMPlayerPrefBroadcastPage (QWidget *parent, FFServerSetting * _ffs);
    ~KMPlayerPrefBroadcastPage () {}

    QLineEdit * bindaddress;
    QLineEdit * port;
    QLineEdit * maxclients;
    QLineEdit * maxbandwidth;
    QLineEdit * feedfile;
    QLineEdit * feedfilesize;
    QComboBox * optimize;
    QGroupBox * movieparams;
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
    FFServerSetting custom;
public slots:
    void slotIndexChanged (int index);
private:
    FFServerSetting * ffs;
};

class KMPlayerPrefBroadcastACLPage : public QFrame
{
    Q_OBJECT
public:
    KMPlayerPrefBroadcastACLPage (QWidget *parent);
    ~KMPlayerPrefBroadcastACLPage () {}
    QTable * accesslist;
};

class KMPlayerPrefGeneralPageOutput : public QFrame
{
    Q_OBJECT
public:
    KMPlayerPrefGeneralPageOutput(QWidget *parent = 0);
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
    QLineEdit *additionalArguments;
    QSpinBox *cacheSize;
};

#endif // _KMPlayerPREF_H_
