/* This file is part of the KDE project
 *
 * Copyright (C) 2003 Joonas Koivunen <rzei@mbnet.fi>
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
#include <qcheckbox.h>
#include <qtextedit.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <qradiobutton.h>
#include <qslider.h>
#include <qspinbox.h>


class KMPlayerPrefGeneralPageGeneral; 	// general, general
class KMPlayerPrefGeneralPageDVD;	// general, dvd
class KMPlayerPrefGeneralPageVCD;	// general, vcd
class KMPlayerPrefGeneralPageOutput;	// general, output
class KMPlayerPrefGeneralPageAdvanced;	// general, advanced, pattern matches etc.
class KMPlayerPrefOPPageGeneral;	// OP = outputplugins, general
class KMPlayerPrefOPPagePostProc;	// outputplugins, postproc

class KMPlayerPreferences : public KDialogBase
{
    Q_OBJECT
public:
    KMPlayerPreferences(QWidget *parent);
    ~KMPlayerPreferences();
    
    KMPlayerPrefGeneralPageGeneral 	*m_GeneralPageGeneral;
    KMPlayerPrefGeneralPageDVD 		*m_GeneralPageDVD;
    KMPlayerPrefGeneralPageVCD 		*m_GeneralPageVCD;
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
    QCheckBox *autoHideControlButtons;
    QCheckBox *showPositionSlider;
    QCheckBox *autoHideSlider;
    QCheckBox *alwaysBuildIndex;
    
    QSpinBox *seekTime;
        
};

class KMPlayerPrefGeneralPageDVD : public QFrame
{
    Q_OBJECT
public:
    KMPlayerPrefGeneralPageDVD(QWidget *parent = 0);
    ~KMPlayerPrefGeneralPageDVD() {}
    
    QCheckBox *autoPlayDVD;
    QLineEdit *dvdDevicePath;
          
};

class KMPlayerPrefGeneralPageVCD : public QFrame
{
    Q_OBJECT
public:
    KMPlayerPrefGeneralPageVCD(QWidget *parent = 0);
    ~KMPlayerPrefGeneralPageVCD() {}
    QLineEdit *vcdDevicePath;
    QCheckBox *autoPlayVCD;
    
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
