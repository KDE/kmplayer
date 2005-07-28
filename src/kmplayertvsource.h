/* This file is part of the KMPlayer application
   Copyright (C) 2003 Koos Vriezen <koos.vriezen@xs4all.nl>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef _KMPLAYER_TV_SOURCE_H_
#define _KMPLAYER_TV_SOURCE_H_

#include <list>

#include <qframe.h>
#include <qstring.h>

#include "kmplayerappsource.h"
#include "kmplayerconfig.h"


class KMPlayerPrefSourcePageTV;         // source, TV
class TVDeviceScannerSource;
class KMPlayerTVSource;
class KMPlayerApp;
class QTabWidget;
class QTable;
class QGroupBox;
class QLineEdit;
class QCheckBox;
class KHistoryCombo;
class KComboBox;
class KConfig;
class KURLRequester;


class TVNode : public KMPlayer::GenericMrl {
public:
    TVNode (KMPlayer::NodePtr &d, const QString &s, const char * t, short id, const QString &n=QString::null);
    KDE_NO_EXPORT const char * nodeName () const { return tag; }
    virtual void setNodeName (const QString &);
protected:
    const char * tag;
};

/*
 * Element for channels
 */
class TVChannel : public TVNode {
public:
    TVChannel (KMPlayer::NodePtr & d, const QString & n, int f);
    TVChannel (KMPlayer::NodePtr & d);
    KDE_NO_CDTOR_EXPORT ~TVChannel () {}
    void closed ();
};

/*
 * Element for inputs
 */
class TVInput : public TVNode {
public:
    TVInput (KMPlayer::NodePtr & d, const QString & n, int id);
    TVInput (KMPlayer::NodePtr & d);
    KDE_NO_CDTOR_EXPORT ~TVInput () {}
    KMPlayer::NodePtr childFromTag (const QString &);
    void closed ();
};

/*
 * Element for TV devices
 */
class TVDevice : public TVNode {
public:
    TVDevice (KMPlayer::NodePtr & d, const QString & d);
    TVDevice (KMPlayer::NodePtr & d);
    KDE_NO_CDTOR_EXPORT ~TVDevice () {}
    KMPlayer::NodePtr childFromTag (const QString &);
    void closed ();
    bool expose () const { return !zombie; }
    bool zombie;
};

class TVDocument : public KMPlayer::Document {
    KMPlayerTVSource * m_source;
public:
    TVDocument (KMPlayerTVSource *);
    KMPlayer::NodePtr childFromTag (const QString &);
    KDE_NO_EXPORT const char * nodeName () const { return "tvdevices"; }
};

class KMPlayerPrefSourcePageTVDevice : public QFrame {
    Q_OBJECT
public:
    KMPlayerPrefSourcePageTVDevice (QWidget *parent, KMPlayer::NodePtr dev);
    KDE_NO_CDTOR_EXPORT ~KMPlayerPrefSourcePageTVDevice () {}

    QLineEdit * name;
    KURLRequester * audiodevice;
    QLineEdit * sizewidth;
    QLineEdit * sizeheight;
    QCheckBox * noplayback;
    KMPlayer::NodePtrW device_doc;
    void updateTVDevice ();
signals:
    void deleted (KMPlayerPrefSourcePageTVDevice *);
private slots:
    void slotDelete ();
private:
    QTabWidget * inputsTab;
};

class KMPlayerPrefSourcePageTV : public QFrame
{
    Q_OBJECT
public:
    KMPlayerPrefSourcePageTV (QWidget *parent, KMPlayerTVSource *);
    KDE_NO_CDTOR_EXPORT ~KMPlayerPrefSourcePageTV () {}
    QLineEdit * driver;
    KURLRequester * device;
    QPushButton * scan;
    QTabWidget * tab;
protected:
    void showEvent (QShowEvent *);
    KMPlayerTVSource * m_tvsource;
};


/*
 * Source form scanning TV devices
 */
class TVDeviceScannerSource : public KMPlayer::Source {
    Q_OBJECT
public:
    TVDeviceScannerSource (KMPlayerTVSource * src);
    KDE_NO_CDTOR_EXPORT ~TVDeviceScannerSource () {};
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
    virtual void playURLDone ();
signals:
    void scanFinished (TVDevice * tvdevice);
private:
    KMPlayerTVSource * m_tvsource;
    TVDevice * m_tvdevice;
    KMPlayer::Source * m_old_source;
    QString m_driver;
    QRegExp m_nameRegExp;
    QRegExp m_sizesRegExp;
    QRegExp m_inputRegExp;
};

/*
 * Source form TV devices, also implementing preference page for it
 */
class KMPlayerTVSource : public KMPlayerMenuSource, public KMPlayer::PreferencesPage {
    Q_OBJECT
public:
    KMPlayerTVSource (KMPlayerApp * app, QPopupMenu * m);
    virtual ~KMPlayerTVSource ();
    virtual QString filterOptions ();
    virtual bool hasLength ();
    virtual bool isSeekable ();
    void buildMenu ();
    virtual QString prettyName ();
    virtual void write (KConfig *);
    virtual void read (KConfig *);
    virtual void sync (bool);
    virtual void prefLocation (QString & item, QString & icon, QString & tab);
    virtual QFrame * prefPage (QWidget * parent);
    void readXML ();
public slots:
    virtual void activate ();
    virtual void deactivate ();
    virtual void playCurrent ();
    void menuAboutToShow ();
    void menuClicked (int id);
private slots:
    void slotScan ();
    void slotScanFinished (TVDevice * device);
    void slotDeviceDeleted (KMPlayerPrefSourcePageTVDevice *);
private:
    void addTVDevicePage (TVDevice * dev, bool show=false);
    void buildArguments ();
    KMPlayer::NodePtrW m_cur_tvdevice;
    QPopupMenu * m_channelmenu;
    QString tvdriver;
    KMPlayerPrefSourcePageTV * m_configpage;
    TVDeviceScannerSource * scanner;
    typedef std::list <KMPlayerPrefSourcePageTVDevice *> TVDevicePageList;
    TVDevicePageList m_devicepages;
    bool config_read; // whether tv.xml is read
};

#endif //_KMPLAYER_TV_SOURCE_H_
