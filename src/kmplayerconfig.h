/*
    SPDX-FileCopyrightText: 2002-2003 Koos Vriezen <koos.vriezen@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef _KMPLAYERCONFIG_H_
#define _KMPLAYERCONFIG_H_

#include "config-kmplayer.h"

#include <qobject.h>
#include <QColor>
#include <QFont>
#include <qstringlist.h>
#include <qmap.h>

#include <ksharedconfig.h>

#include "kmplayercommon_export.h"

class KSharedConfig;
class QFrame;

namespace KMPlayer {

class PartBase;
class Preferences;
class View;

class OutputDriver
{
public:
    const char * driver;
    const QString description;
};

class ColorSetting
{
public:
    QString title;
    QString option;
    QColor color;
    QColor newcolor;
    enum Target {
        playlist_background = 0, playlist_foreground, playlist_active,
        console_background, console_foreground,
        video_background, area_background,
        infowindow_background, infowindow_foreground,
        last_target
    } target;
};

class FontSetting
{
public:
    QString title;
    QString option; // for ini file
    QFont font;
    QFont newfont;
    enum Target {
        playlist, infowindow, last_target
    } target;
};

template <class T>
struct Deleter {
    void operator ()(T * t) {
        delete t;
    }
};

/*
 * Base class for all dynamic preferance pages
 */
class KMPLAYERCOMMON_EXPORT PreferencesPage
{
public:
    virtual ~PreferencesPage () {}
    virtual void write (KSharedConfigPtr) = 0;
    virtual void read (KSharedConfigPtr) = 0;
    virtual void sync (bool fromUI) = 0;
    virtual void prefLocation (QString & item, QString & icon, QString & tab) = 0;
    virtual QFrame * prefPage (QWidget * parent) = 0;
    PreferencesPage * next;
};

/*
 * Class for storing all actual settings and reading/writing them
 */
class KMPLAYERCOMMON_EXPORT Settings : public QObject
{
    Q_OBJECT
public:
    Settings (PartBase *, KSharedConfigPtr part);
    ~Settings () override;
    bool createDialog () KMPLAYERCOMMON_NO_EXPORT;
    void show (const char * pagename = nullptr);
    void addPage (PreferencesPage *);
    void removePage (PreferencesPage *);
    void applyColorSetting (bool only_changed_ones) KMPLAYERCOMMON_NO_EXPORT;
    Preferences *configDialog() const { return configdialog; }
    View * defaultView ();
    KSharedConfigPtr kconfig () { return m_config; }

    QStringList urllist;
    QStringList sub_urllist;
    int volume;
    int contrast;
    int brightness;
    int hue;
    int saturation;
    int prefbitrate;
    int maxbitrate;
    bool usearts : 1;
    bool no_intro : 1;
    bool sizeratio : 1;
    bool remembersize : 1;
    bool autoresize : 1;
    bool docksystray : 1;
    bool loop : 1;
    bool framedrop : 1;
    bool autoadjustvolume : 1;
    bool autoadjustcolors : 1;
    bool showcnfbutton : 1;
    bool showplaylistbutton : 1;
    bool showrecordbutton : 1;
    bool showbroadcastbutton : 1;
    bool autohideslider : 1;
    bool clicktoplay : 1;
    bool grabhref : 1;
// postproc thingies
    bool postprocessing : 1;
    bool disableppauto : 1;
    bool pp_default : 1;	// -vf pp=de
    bool pp_fast : 1;	// -vf pp=fa
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
    ColorSetting colors [ColorSetting::last_target];
    FontSetting fonts [FontSetting::last_target];
    QString dvddevice;
    QString vcddevice;
    QMap <QString, QString> backends;
    PreferencesPage * pagelist;
Q_SIGNALS:
    void configChanged ();
public Q_SLOTS:
    void readConfig () KMPLAYERCOMMON_NO_EXPORT;
    void writeConfig ();
private Q_SLOTS:
    void okPressed () KMPLAYERCOMMON_NO_EXPORT;
    void getHelp () KMPLAYERCOMMON_NO_EXPORT;
private:
    Preferences * configdialog;
    KSharedConfigPtr m_config;
    PartBase * m_player;
};

} // namespace

#endif //_KMPLAYERCONFIG_H_
