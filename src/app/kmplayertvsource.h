/*
    This file is part of the KMPlayer application
    SPDX-FileCopyrightText: 2003 Koos Vriezen <koos.vriezen@xs4all.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef _KMPLAYER_TV_SOURCE_H_
#define _KMPLAYER_TV_SOURCE_H_

#include <QPointer>
#include <QString>
#include <QFrame>

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


class TVDevicePage : public QFrame
{
    Q_OBJECT
public:
    TVDevicePage (QWidget *parent, KMPlayer::NodePtr dev);
    ~TVDevicePage () override {}

    QLineEdit * name;
    KUrlRequester * audiodevice;
    QLineEdit * sizewidth;
    QLineEdit * sizeheight;
    QCheckBox * noplayback;
    QTabWidget * inputsTab;
    KMPlayer::NodePtrW device_doc;
Q_SIGNALS:
    void deleted (TVDevicePage *);
private Q_SLOTS:
    void slotDelete ();
};

class KMPlayerPrefSourcePageTV : public QFrame
{
    Q_OBJECT
public:
    KMPlayerPrefSourcePageTV (QWidget *parent, KMPlayerTVSource *);
    ~KMPlayerPrefSourcePageTV () override {}
    QLineEdit * driver;
    KUrlRequester * device;
    QPushButton * scan;
    QTabWidget * notebook;
protected:
    void showEvent (QShowEvent *) override;
    KMPlayerTVSource * m_tvsource;
};

class TVNode : public KMPlayer::GenericMrl
{
public:
    TVNode (KMPlayer::NodePtr &d, const QString &s, const char * t, short id, const QString &n=QString ());
    void setNodeName (const QString &) override;
};

/*
 * Element for channels
 */
class TVChannel : public TVNode
{
public:
    TVChannel (KMPlayer::NodePtr & d, const QString & n, double f);
    TVChannel (KMPlayer::NodePtr & d);
    ~TVChannel () override {}
    void closed () override;
};

/*
 * Element for inputs
 */
class TVInput : public TVNode
{
public:
    TVInput (KMPlayer::NodePtr & d, const QString & n, int id);
    TVInput (KMPlayer::NodePtr & d);
    ~TVInput () override {}
    KMPlayer::Node *childFromTag (const QString &) override;
    void setNodeName (const QString &) override;
    void closed () override;
};

/*
 * Element for TV devices
 */
class TVDevice : public TVNode
{
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

class TVDocument : public FileDocument
{
    KMPlayerTVSource * m_source;
public:
    TVDocument (KMPlayerTVSource *);
    KMPlayer::Node *childFromTag (const QString &) override;
    void defer () override;
    const char * nodeName () const override { return "tvdevices"; }
    void message (KMPlayer::MessageType msg, void *content=nullptr) override;
};


/*
 * Source form scanning TV devices
 */
class TVDeviceScannerSource : public KMPlayer::Source, KMPlayer::ProcessUser
{
    Q_OBJECT
public:
    TVDeviceScannerSource (KMPlayerTVSource * src);
    ~TVDeviceScannerSource () override {};
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
public Q_SLOTS:
    void scanningFinished ();
Q_SIGNALS:
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
class KMPlayerTVSource : public KMPlayer::Source, public KMPlayer::PreferencesPage
{
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
public Q_SLOTS:
    void menuClicked (int id);
private Q_SLOTS:
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
