/**
 * Copyright (C) 2002-2003 by Koos Vriezen <koos ! vriezen ? xs4all ! nl>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 **/

#ifndef _KMPLAYERCONFIG_H_
#define _KMPLAYERCONFIG_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <qobject.h>
#include <qstringlist.h>
#include <qmap.h>

#include <kurl.h>

class KConfig;

namespace KMPlayer {
    
class PartBase;
class Preferences;

class OutputDriver {
public:
    const char * driver;
    const QString description;
};

template <class T>
void Deleter (T * t) {
    delete t;
}

class KMPLAYER_EXPORT PreferencesPage {
public:
    virtual ~PreferencesPage () {}
    virtual void write (KConfig *) = 0;
    virtual void read (KConfig *) = 0;
    virtual void sync (bool fromUI) = 0;
    virtual void prefLocation (QString & item, QString & icon, QString & tab) = 0;
    virtual QFrame * prefPage (QWidget * parent) = 0;
    PreferencesPage * next;
};

class KMPLAYER_EXPORT Settings : public QObject {
    Q_OBJECT
public:
    Settings (PartBase *, KConfig * part);
    ~Settings ();
    bool createDialog ();
    void show (const char * pagename = 0L);
    void addPage (PreferencesPage *);
    void removePage (PreferencesPage *);
    Preferences *configDialog() const { return configdialog; }

    QStringList urllist;
    QStringList sub_urllist;
    int contrast;
    int brightness;
    int hue;
    int saturation;
    bool usearts : 1;
    bool sizeratio : 1;
    bool loop : 1;
    bool framedrop : 1;
    bool showcnfbutton : 1;
    bool showrecordbutton : 1;
    bool showbroadcastbutton : 1;
    bool autohideslider : 1;
    bool mplayerpost090 : 1;
    bool allowhref : 1;
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
    // recording
    bool recordcopy : 1;
    enum Recorder { MEncoder = 0, FFMpeg, MPlayerDumpstream };
    Recorder recorder;
    enum ReplayOption { ReplayNo = 0, ReplayFinished, ReplayAfter };
    ReplayOption replayoption;
    int replaytime;
    QString mencoderarguments;
    QString ffmpegarguments;
    QString recordfile;
    int seektime;
    int videodriver;
    int audiodriver;
    OutputDriver * audiodrivers;
    OutputDriver * videodrivers;
    QString dvddevice;
    QString vcddevice;
    QMap <QString, QString> backends;
    PreferencesPage * pagelist;
signals:
    void configChanged ();
public slots:
    void readConfig ();
    void writeConfig ();
private slots:
    void okPressed ();
    void getHelp ();
private:
    Preferences * configdialog;
    KConfig * m_config;
    PartBase * m_player;
};

} // namespace

#endif //_KMPLAYERCONFIG_H_
