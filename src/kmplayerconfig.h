/***************************************************************************
                          kmplayerconfig.h  -  description
                             -------------------
    begin                : 2002/12/30
    copyright            : (C) |2002| by Koos Vriezen
    email                : |EMAIL|
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef _KMPLAYERCONFIG_H_
#define _KMPLAYERCONFIG_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <qobject.h>
#include <qsize.h>

#include "pref.h"

class KMPlayer;
class KConfig;
class KMPlayerPreferences;
/*
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
    QPtrList <TVChannel> channels;
};

class TVDevice {
public:
    TVDevice (const QString & d, const QSize & size);
    QString device;
    QString name;
    QSize minsize;
    QSize maxsize;
    QSize size;
    QPtrList <TVInput> inputs;
};

class TVDeviceScanner : public QObject {
    Q_OBJECT
public:
    TVDeviceScanner () {}
    virtual ~TVDeviceScanner () {}
    virtual bool scan (const QString & device, const QString & driver) = 0;
signals:
    void scanFinished (TVDevice * tvdevice);
};
class TVDeviceScannerSource : public KMPlayerSource, public TVDeviceScanner {
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
private:
    TVDevice * m_tvdevice;
    KMPlayerSource * m_source;
    QString m_driver;
    QRegExp m_nameRegExp;
    QRegExp m_sizesRegExp;
    QRegExp m_inputRegExp
};
*/

class KMPlayerConfig : public QObject {
    Q_OBJECT
public:
    KMPlayerConfig (KMPlayer *, KConfig * part);
    ~KMPlayerConfig ();
    KMPlayerPreferences *configDialog() const { return configdialog; }
    bool usearts;
    bool sizeratio;
    bool showconsole;
    bool loop;
    bool showbuttons;
    bool showcnfbutton;
    bool showposslider;
    bool autohidebuttons;
    bool autohideslider;
    bool alwaysbuildindex;
    bool playdvd;
    bool playvcd;
// postproc thingies
    bool postprocessing;
    bool disableppauto;
    bool pp_default;	// -vop pp=de
    bool pp_fast;	// -vop pp=fa
    bool pp_custom;	// coming up

    bool pp_custom_hz; 		// horizontal deblocking
    bool pp_custom_hz_aq;	//  - autoquality
    bool pp_custom_hz_ch;	//  - chrominance

    bool pp_custom_vt;          // vertical deblocking
    bool pp_custom_vt_aq;       //  - autoquality
    bool pp_custom_vt_ch;       //  - chrominance

    bool pp_custom_dr;          // dering filter
    bool pp_custom_dr_aq;       //  - autoquality
    bool pp_custom_dr_ch;       //  - chrominance

    bool pp_custom_al;	// pp=al
    bool pp_custom_al_f;//  - fullrange

    bool pp_custom_tn;  // pp=tn
    int pp_custom_tn_s; //  - noise reducer strength (1 <= x <= 3)

    bool pp_lin_blend_int;	// linear blend deinterlacer
    bool pp_lin_int;		// - interpolating -
    bool pp_cub_int;		// cubic - -
    bool pp_med_int;		// median interlacer
    bool pp_ffmpeg_int;		// ffmpeg interlacer
// end of postproc
// TV stuff
    QString tvdriver;
    QPtrList <TVDevice> tvdevices;
// end of TV stuff
    int seektime;
    int cachesize;
    int videodriver;
    int audiodriver;
    QString dvddevice;
    QString vcddevice;
    QString additionalarguments;
    QString sizepattern;
    QString cachepattern;
    QString indexpattern;
    QString startpattern;
    QString langpattern;
    QString titlespattern;
    QString subtitlespattern;
    QString chapterspattern;
    QString trackspattern;
signals:
    void configChanged ();
public slots:
    void readConfig ();
    void writeConfig ();
    void show ();
private slots:
    void okPressed ();
    void getHelp ();
private:
    KMPlayerPreferences * configdialog;
    KConfig * m_config;
    KMPlayer * m_player;
};

#endif //_KMPLAYERCONFIG_H_
