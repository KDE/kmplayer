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


class KMPlayer;
class KConfig;
class ConfigDialog;

class KMPlayerConfig : public QObject {
    Q_OBJECT
public:
    KMPlayerConfig (KMPlayer *, KConfig * part);
    ~KMPlayerConfig ();
    bool usearts;
    bool sizeratio;
    bool showconsole;
    bool loop;
    bool showbuttons;
    bool showcnfbutton;
    bool showposslider;
    bool autohidebuttons;
    bool showdvdmenu;
    bool showvcdmenu;
    bool playdvd;
    bool playvcd;
// postproc thingies
    bool postprocessing;

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
    int seektime;
    int cachesize;
    QString videodriver;
    QString dvddevice;
    QString vcddevice;
    QString additionalarguments;
    QString sizepattern;
    QString cachepattern;
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
    void fileOpen ();
    void getHelp ();
private:
    ConfigDialog * configdialog;
    KConfig * m_config;
    KMPlayer * m_player;
};

#endif //_KMPLAYERCONFIG_H_
