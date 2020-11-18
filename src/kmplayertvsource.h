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

#include <QPointer>
#include <qstring.h>
#include <qframe.h>

#include "kmplayer.h"
#include "kmplayerconfig.h"
#include "mediaobject.h"
#include "kmplayer_lists.h"

const short id_node_tv_document = 40;
const short id_node_tv_device = 41;
const short id_node_tv_input = 42;
const short id_node_tv_channel = 43;

class KMPlayerPrefSourcePageTV;         // source, TV
class TVDeviceScannerSource;
class KMPlayerTVSource;
class KUrlRequester;
class KMPlayerApp;
class QTabWidget;
class QLineEdit;
class QCheckBox;
class QPushButton;


class KMPLAYER_NO_EXPORT TVDevicePage : public QFrame {
    Q_OBJECT
public:
    TVDevicePage (QWidget *parent, KMPlayer::NodePtr dev);
    KDE_NO_CDTOR_EXPORT ~TVDevicePage () override {}

    QLineEdit * name;
    KUrlRequester * audiodevice;
    QLineEdit * sizewidth;
    QLineEdit * sizeheight;
    QCheckBox * noplayback;
    QTabWidget * inputsTab;
    KMPlayer::NodePtrW device_doc;
signals:
    void deleted (TVDevicePage *);
private slots:
    void slotDelete ();
};

class KMPLAYER_NO_EXPORT KMPlayerPrefSourcePageTV : public QFrame {
    Q_OBJECT
public:
    KMPlayerPrefSourcePageTV (QWidget *parent, KMPlayerTVSource *);
    KDE_NO_CDTOR_EXPORT ~KMPlayerPrefSourcePageTV () override {}
    QLineEdit * driver;
    KUrlRequester * device;
    QPushButton * scan;
    QTabWidget * notebook;
protected:
    void showEvent (QShowEvent *) override;
    KMPlayerTVSource * m_tvsource;
};

class KMPLAYER_NO_EXPORT TVNode : public KMPlayer::GenericMrl {
public:
    TVNode (KMPlayer::NodePtr &d, const QString &s, const char * t, short id, const QString &n=QString ());
    void setNodeName (const QString &) override;
};

/*
 * Element for channels
 */
class KMPLAYER_NO_EXPORT TVChannel : public TVNode {
public:
    TVChannel (KMPlayer::NodePtr & d, const QString & n, double f);
    TVChannel (KMPlayer::NodePtr & d);
    KDE_NO_CDTOR_EXPORT ~TVChannel () override {}
    void closed () override;
};

/*
 * Element for inputs
 */
class KMPLAYER_NO_EXPORT TVInput : public TVNode {
public:
    TVInput (KMPlayer::NodePtr & d, const QString & n, int id);
    TVInput (KMPlayer::NodePtr & d);
    KDE_NO_CDTOR_EXPORT ~TVInput () override {}
    KMPlayer::Node *childFromTag (const QString &) override;
    void setNodeName (const QString &) override;
    void closed () override;
};

/*
 * Element for TV devices
 */
class KMPLAYER_NO_EXPORT TVDevice : public TVNode {
public:
    TVDevice (KMPlayer::NodePtr & d, const QString & s);
    TVDevice (KMPlayer::NodePtr & d);
    ~TVDevice () override;
    KMPlayer::Node *childFromTag (const QString &) override;
    void closed () override;
    void message (KMPlayer::MessageType msg, void *content=nullptr) override;
    void *role (KMPlayer::RoleType msg, void *content=nullptr) override;
    void setNodeName (const QString &) override;
    void updateNodeName ();
    void updateDevicePage ();
    bool zombie;
    QPointer <TVDevicePage> device_page;
};

class KMPLAYER_NO_EXPORT TVDocument : public FileDocument {
    KMPlayerTVSource * m_source;
public:
    TVDocument (KMPlayerTVSource *);
    KMPlayer::Node *childFromTag (const QString &) override;
    void defer () override;
    KDE_NO_EXPORT const char * nodeName () const override { return "tvdevices"; }
    void message (KMPlayer::MessageType msg, void *content=nullptr) override;
};


/*
 * Source form scanning TV devices
 */
class KMPLAYER_NO_EXPORT TVDeviceScannerSource
                             : public KMPlayer::Source, KMPlayer::ProcessUser {
    Q_OBJECT
public:
    TVDeviceScannerSource (KMPlayerTVSource * src);
    KDE_NO_CDTOR_EXPORT ~TVDeviceScannerSource () override {};
    void init () override;
    bool processOutput (const QString & line) override;
    QString filterOptions () override;
    bool hasLength () override;
    bool isSeekable () override;
    virtual bool scan (const QString & device, const QString & driver);

    void starting (KMPlayer::IProcess *) override {}
    void stateChange (KMPlayer::IProcess *, KMPlayer::IProcess::State, KMPlayer::IProcess::State) override;
    void processDestroyed (KMPlayer::IProcess *p) override;
    KMPlayer::IViewer *viewer () override;
    KMPlayer::Mrl *getMrl () override;

    void activate () override;
    void deactivate () override;
    void play (KMPlayer::Mrl *) override;
public slots:
    void scanningFinished ();
signals:
    void scanFinished (TVDevice * tvdevice);
private:
    KMPlayerTVSource * m_tvsource;
    TVDevice * m_tvdevice;
    KMPlayer::IProcess *m_process;
    KMPlayer::IViewer *m_viewer;
    KMPlayer::Source * m_old_source;
    QString m_driver;
    QString m_caps;
    QRegExp m_nameRegExp;
    QRegExp m_sizesRegExp;
    QRegExp m_inputRegExp;
    QRegExp m_inputRegExpV4l2;
};

/*
 * Source form TV devices, also implementing preference page for it
 */
class KMPLAYER_NO_EXPORT KMPlayerTVSource : public KMPlayer::Source, public KMPlayer::PreferencesPage {
    Q_OBJECT
public:
    KMPlayerTVSource(KMPlayerApp* app);
    ~KMPlayerTVSource () override;
    QString filterOptions () override;
    bool hasLength () override;
    bool isSeekable () override;
    KMPlayer::NodePtr root () override;
    QString prettyName () override;
    void write (KSharedConfigPtr) override;
    void read (KSharedConfigPtr) override;
    void sync (bool) override;
    void prefLocation (QString & item, QString & icon, QString & tab) override;
    QFrame * prefPage (QWidget * parent) override;
    void readXML ();
    void setCurrent (KMPlayer::Mrl *) override;
    void activate () override;
    void deactivate () override;
    void play (KMPlayer::Mrl *) override;
public slots:
    void menuClicked (int id);
private slots:
    void slotScan ();
    void slotScanFinished (TVDevice * device);
    void slotDeviceDeleted (TVDevicePage *);
private:
    void addTVDevicePage (TVDevice * dev, bool show=false);
    KMPlayer::NodePtrW m_cur_tvdevice;
    KMPlayer::NodePtrW m_cur_tvinput;
    KMPlayerApp* m_app;
    QMenu * m_channelmenu;
    QString tvdriver;
    KMPlayerPrefSourcePageTV * m_configpage;
    TVDeviceScannerSource * scanner;
    int tree_id;
    bool config_read; // whether tv.xml is read
};

#endif //_KMPLAYER_TV_SOURCE_H_
