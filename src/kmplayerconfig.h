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

class KMPlayerConfig : public QObject {
    Q_OBJECT
public:
    KMPlayerConfig (KMPlayer *, KConfig * part);
    ~KMPlayerConfig ();
    KMPlayerPreferences *configDialog() const { return configdialog; }
    int contrast;
    int brightness;
    int hue;
    int saturation;
    bool usearts : 1;
    bool sizeratio : 1;
    bool showconsole : 1;
    bool loop : 1;
    bool framedrop : 1;
    bool showbuttons : 1;
    bool showcnfbutton : 1;
    bool showrecordbutton : 1;
    bool showbroadcastbutton : 1;
    bool autoplayafterrecording : 1;
    bool showposslider : 1;
    bool autohidebuttons : 1;
    bool autohideslider : 1;
    bool alwaysbuildindex : 1;
    bool playdvd : 1;
    bool playvcd : 1;
// postproc thingies
    bool postprocessing : 1;
    bool disableppauto : 1;
    bool pp_default : 1;	// -vop pp=de
    bool pp_fast : 1;	// -vop pp=fa
    bool pp_custom : 1;	// coming up

    bool pp_custom_hz : 1; 		// horizontal deblocking
    bool pp_custom_hz_aq : 1;	//  - autoquality
    bool pp_custom_hz_ch : 1;	//  - chrominance

    bool pp_custom_vt : 1;          // vertical deblocking
    bool pp_custom_vt_aq : 1;       //  - autoquality
    bool pp_custom_vt_ch : 1;       //  - chrominance

    bool pp_custom_dr : 1;          // dering filter
    bool pp_custom_dr_aq : 1;       //  - autoquality
    bool pp_custom_dr_ch : 1;       //  - chrominance

    bool pp_custom_al : 1;	// pp=al
    bool pp_custom_al_f : 1;//  - fullrange

    bool pp_custom_tn : 1;  // pp=tn
    int pp_custom_tn_s : 1; //  - noise reducer strength (1 <= x <= 3)

    bool pp_lin_blend_int : 1;	// linear blend deinterlacer
    bool pp_lin_int : 1;		// - interpolating -
    bool pp_cub_int : 1;		// cubic - -
    bool pp_med_int : 1;		// median interlacer
    bool pp_ffmpeg_int : 1;		// ffmpeg interlacer
// end of postproc
// TV stuff
    QString tvdriver;
    QPtrList <TVDevice> tvdevices;
// end of TV stuff
    QString bindaddress;
    int ffserverport;
    int maxclients;
    int maxbandwidth;
    QString feedfile;
    int feedfilesize;
    FFServerSetting * ffserversettings;
    int ffserversetting;
    QStringList ffserveracl;
    int seektime;
    int cachesize;
    int videodriver;
    int audiodriver;
    QString dvddevice;
    QString vcddevice;
    QString additionalarguments;
    QString mencoderarguments;
    QString sizepattern;
    QString cachepattern;
    QString positionpattern;
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
