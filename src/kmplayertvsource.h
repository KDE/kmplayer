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
#include <vector>

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


class TVChannel {
public:
    TVChannel (const QString & n, int f);
    QString name;
    int frequency;
};

typedef std::list <TVChannel *> TVChannelList;

class TVInput {
public:
    TVInput (const QString & n, int id);
    KDE_NO_CDTOR_EXPORT ~TVInput () { clear (); }
    void clear ();
    QString name;
    int id;
    bool hastuner;
    QString norm;
    TVChannelList channels;
};

typedef std::list <TVInput *> TVInputList;

class TVDevice {
public:
    TVDevice (const QString & d, const QSize & size);
    KDE_NO_CDTOR_EXPORT ~TVDevice () { clear (); }
    void clear ();
    QString device;
    QString audiodevice;
    QString name;
    QSize minsize;
    QSize maxsize;
    QSize size;
    bool noplayback;
    TVInputList inputs;
};

inline bool operator == (const TVDevice * device, const QString & devstr) {
    return devstr == device->device;
}

inline bool operator == (const QString & devstr, const TVDevice * device) {
    return devstr == device->device;
}

inline bool operator != (const TVDevice * device, const QString & devstr) {
    return ! (devstr == device);
}

inline bool operator != (const QString & devstr, const TVDevice * device) {
    return ! (devstr == device);
}

typedef std::list <TVDevice *> TVDeviceList;


class KMPlayerPrefSourcePageTVDevice : public QFrame
{
    Q_OBJECT
public:
    KMPlayerPrefSourcePageTVDevice (QWidget *parent, TVDevice * dev);
    KDE_NO_CDTOR_EXPORT ~KMPlayerPrefSourcePageTVDevice () {}

    QLineEdit * name;
    KURLRequester * audiodevice;
    QLineEdit * sizewidth;
    QLineEdit * sizeheight;
    QCheckBox * noplayback;
    TVDevice * device;
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
    friend class TVDevicePageAdder;
public:
    KMPlayerPrefSourcePageTV (QWidget *parent);
    KDE_NO_CDTOR_EXPORT ~KMPlayerPrefSourcePageTV () {}

    QLineEdit * driver;
    KURLRequester * device;
    QTabWidget * tab;
    TVDeviceScannerSource * scanner;
    void setTVDevices (TVDeviceList *);
    void updateTVDevices ();
private slots:
    void slotScan ();
    void slotScanFinished (TVDevice * device);
    void slotDeviceDeleted (KMPlayerPrefSourcePageTVDevice *);
private:
    TVDeviceList * m_devices;
    TVDeviceList deleteddevices;
    TVDeviceList addeddevices;
    typedef std::list <KMPlayerPrefSourcePageTVDevice *> TVDevicePageList;
    TVDevicePageList m_devicepages;
};


class TVDeviceScannerSource : public KMPlayerSource {
    Q_OBJECT
public:
    TVDeviceScannerSource (KMPlayer * player);
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
    TVDevice * m_tvdevice;
    KMPlayerSource * m_source;
    QString m_driver;
    QRegExp m_nameRegExp;
    QRegExp m_sizesRegExp;
    QRegExp m_inputRegExp;
};

class KMPlayerTVSource : public KMPlayerMenuSource, public KMPlayerPreferencesPage {
    Q_OBJECT
public:
    struct TVSource {
        QSize size;
        QString command;
        QString videodevice;
        QString audiodevice;
        QString title;
        QString norm;
        int frequency;
        bool noplayback;
    };
    KMPlayerTVSource (KMPlayerApp * app, QPopupMenu * m);
    virtual ~KMPlayerTVSource ();
    virtual QString filterOptions ();
    virtual bool hasLength ();
    virtual bool isSeekable ();
    void buildMenu ();
    KDE_NO_EXPORT TVSource * tvsource () const { return m_tvsource; }
    virtual QString prettyName ();
    virtual void write (KConfig *);
    virtual void read (KConfig *);
    virtual void sync (bool);
    virtual void prefLocation (QString & item, QString & icon, QString & tab);
    virtual QFrame * prefPage (QWidget * parent);
public slots:
    virtual void activate ();
    virtual void deactivate ();
    virtual void forward ();
    virtual void backward ();

    void menuClicked (int id);
private:
    void buildArguments ();
    typedef QMap <int, TVSource *> CommandMap;
    TVSource * m_tvsource;
    CommandMap commands;
    QPopupMenu * m_channelmenu;
    QString tvdriver;
    TVDeviceList tvdevices;
    KMPlayerPrefSourcePageTV * m_configpage;
    int m_current_id;
};

#endif //_KMPLAYER_TV_SOURCE_H_
