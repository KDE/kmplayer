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
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef _KMPLAYER_TV_SOURCE_H_
#define _KMPLAYER_TV_SOURCE_H_

#include <list>

#include <qframe.h>

#include "kmplayerappsource.h"
#include "kmplayerconfig.h"


class KMPlayerPrefSourcePageTV;         // source, TV
class TVDeviceScannerSource;
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


/*
 * Element for channels
 */
class TVChannel : public KMPlayer::GenericMrl {
public:
    TVChannel (KMPlayer::NodePtr & d, const QString & n, int f);
    KDE_NO_CDTOR_EXPORT ~TVChannel () {}
    KDE_NO_EXPORT const char * nodeName () const { return "tvchannel"; }
    QString name;
    int frequency;
};

/*
 * Element for inputs
 */
class TVInput : public KMPlayer::GenericMrl {
public:
    TVInput (KMPlayer::NodePtr & d, const QString & n, int id);
    KDE_NO_CDTOR_EXPORT ~TVInput () {}
    KDE_NO_EXPORT const char * nodeName () const { return "tvinput"; }
    QString name;
    int id;
    bool hastuner;
    QString norm;
};

/*
 * Element for TV devices
 */
class TVDevice : public KMPlayer::GenericMrl {
public:
    TVDevice (KMPlayer::NodePtr & d, const QString & d, const QSize & size);
    KDE_NO_CDTOR_EXPORT ~TVDevice () {}
    KDE_NO_EXPORT const char * nodeName () const { return "tvdevice"; }
    QString audiodevice;
    QSize minsize;
    QSize maxsize;
    QSize size;
    bool noplayback;
};

class KMPlayerPrefSourcePageTVDevice : public QFrame
{
    Q_OBJECT
public:
    KMPlayerPrefSourcePageTVDevice (QWidget *parent, KMPlayer::NodePtr dev);
    KDE_NO_CDTOR_EXPORT ~KMPlayerPrefSourcePageTVDevice () {}

    QLineEdit * name;
    KURLRequester * audiodevice;
    QLineEdit * sizewidth;
    QLineEdit * sizeheight;
    QCheckBox * noplayback;
    KMPlayer::NodePtr device_doc;
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
    KMPlayerPrefSourcePageTV (QWidget *parent);
    KDE_NO_CDTOR_EXPORT ~KMPlayerPrefSourcePageTV () {}
    QLineEdit * driver;
    KURLRequester * device;
    QPushButton * scan;
    QTabWidget * tab;
};


/*
 * Source form scanning TV devices
 */
class TVDeviceScannerSource : public KMPlayer::Source {
    Q_OBJECT
public:
    TVDeviceScannerSource (KMPlayer::NodePtr d, KMPlayer::PartBase * player);
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
    void finished ();
signals:
    void scanFinished (TVDevice * tvdevice);
private:
    KMPlayer::NodePtr m_doc;
    TVDevice * m_tvdevice;
    KMPlayer::Source * m_source;
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
public slots:
    virtual void activate ();
    virtual void deactivate ();
    virtual void playCurrent ();
    void menuClicked (int id);
private slots:
    void slotScan ();
    void slotScanFinished (TVDevice * device);
    void slotDeviceDeleted (KMPlayerPrefSourcePageTVDevice *);
private:
    void addTVDevicePage (TVDevice * dev, bool show=false);
    KMPlayer::NodePtr m_cur_tvdevice;
    KMPlayer::NodePtr deleteddevices;
    KMPlayer::NodePtr addeddevices;
    QPopupMenu * m_channelmenu;
    QString tvdriver;
    KMPlayerPrefSourcePageTV * m_configpage;
    TVDeviceScannerSource * scanner;
    typedef std::list <KMPlayerPrefSourcePageTVDevice *> TVDevicePageList;
    TVDevicePageList m_devicepages;
};

#endif //_KMPLAYER_TV_SOURCE_H_
