/* This file is part of the KMPlayer application
   Copyright (C) 2004 Koos Vriezen <koos.vriezen@xs4all.nl>

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

#include <qlayout.h>
#include <qlabel.h>
#include <qmap.h>
#include <qtimer.h>
#include <qpushbutton.h>
#include <qcheckbox.h>
#include <qtable.h>
#include <qstringlist.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <qgroupbox.h>
#include <qwhatsthis.h>
#include <qtabwidget.h>
#include <qbuttongroup.h>
#include <qmessagebox.h>
#include <qpopupmenu.h>
#include <qsocket.h>

#include <klocale.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <klineedit.h>
#include <kurlrequester.h>
#include <kcombobox.h>
#include <kprocess.h>
#include <kconfig.h>
#include <kaction.h>
#include <kiconloader.h>

#include "kmplayerpartbase.h"
#include "kmplayerconfig.h"
#include "kmplayervdr.h"
#include "kmplayer.h"

static const char * strVDR = "VDR";
static const char * strVDRPort = "Port";
static const char * strXVPort = "XV Port";

KDE_NO_CDTOR_EXPORT KMPlayerPrefSourcePageVDR::KMPlayerPrefSourcePageVDR (QWidget * parent)
 : QFrame (parent) {
    //KURLRequester * v4ldevice;
    QVBoxLayout *layout = new QVBoxLayout (this, 5, 2);
    QGridLayout *gridlayout = new QGridLayout (layout, 2, 2);
    QLabel * label = new QLabel (i18n ("XVideo port:"), this);
    gridlayout->addWidget (label, 0, 0);
    xv_port = new QLineEdit ("", this);
    gridlayout->addWidget (xv_port, 0, 1);
    label = new QLabel (i18n ("Communication port:"), this);
    gridlayout->addWidget (label, 1, 0);
    tcp_port = new QLineEdit ("", this);
    gridlayout->addWidget (tcp_port, 1, 1);
    layout->addLayout (gridlayout);
}

KDE_NO_CDTOR_EXPORT KMPlayerPrefSourcePageVDR::~KMPlayerPrefSourcePageVDR () {}

//-----------------------------------------------------------------------------

class VDRCommand {
public:
    KDE_NO_CDTOR_EXPORT VDRCommand (const char * c) : command (c), next (0L) {}
    KDE_NO_CDTOR_EXPORT ~VDRCommand () {}
    const char * command;
    VDRCommand * next;
};

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT KMPlayerVDRSource::KMPlayerVDRSource (KMPlayerApp * app, QPopupMenu * m)
 : KMPlayerMenuSource (i18n ("VDR"), app, m, "vdrsource"),
   m_configpage (0),
   m_socket (new QSocket (this)), 
   commands (0L),
   tcp_port (0),
   xv_port (0) {
    m_player->settings ()->addPage (this);
    act_up = new KAction (i18n ("VDR Key Up"), 0, 0, this, SLOT (keyUp ()), m_app->actionCollection (),"vdr_key_up");
    act_down = new KAction (i18n ("VDR Key Down"), 0, 0, this, SLOT (keyDown ()), m_app->actionCollection (),"vdr_key_down");
    act_back = new KAction (i18n ("VDR Key Back"), 0, 0, this, SLOT (keyBack ()), m_app->actionCollection (),"vdr_key_back");
    act_ok = new KAction (i18n ("VDR Key Ok"), 0, 0, this, SLOT (keyOk ()), m_app->actionCollection (),"vdr_key_ok");
    act_setup = new KAction (i18n ("VDR Key Setup"), 0, 0, this, SLOT (keySetup ()), m_app->actionCollection (),"vdr_key_setup");
    act_channels = new KAction (i18n ("VDR Key Channels"), 0, 0, this, SLOT (keyChannels ()), m_app->actionCollection (),"vdr_key_channels");
}

KDE_NO_CDTOR_EXPORT KMPlayerVDRSource::~KMPlayerVDRSource () {
}

KDE_NO_EXPORT bool KMPlayerVDRSource::hasLength () {
    return false;
}

KDE_NO_EXPORT bool KMPlayerVDRSource::isSeekable () {
    return false;
}

KDE_NO_EXPORT QString KMPlayerVDRSource::prettyName () {
    return i18n ("VDR");
}

KDE_NO_EXPORT void KMPlayerVDRSource::activate () {
    KMPlayerView * view = static_cast <KMPlayerView *> (m_player->view ());
    view->addFullscreenAction (i18n ("VDR Key Up"), act_up->shortcut (), this, SLOT (keyUp ()), "vdr_key_up");
    view->addFullscreenAction (i18n ("VDR Key Down"), act_down->shortcut (), this, SLOT (keyDown ()), "vdr_key_down");
    view->addFullscreenAction (i18n ("VDR Key Ok"), act_ok->shortcut (), this, SLOT (keyOk ()), "vdr_key_ok");
    view->addFullscreenAction (i18n ("VDR Key Back"), act_back->shortcut (), this, SLOT (keyBack ()), "vdr_key_back");
    view->addFullscreenAction (i18n ("VDR Key Setup"), act_setup->shortcut (), this, SLOT (keySetup ()), "vdr_key_setup");
    view->addFullscreenAction (i18n ("VDR Key Channels"), act_channels->shortcut (), this, SLOT (keyChannels ()), "vdr_key_channels");
    m_player->setProcess ("xvideo");
    static_cast<XVideo *>(m_player->players () ["xvideo"])->setPort (xv_port);
    m_menu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("up"), KIcon::Small, 0, true), i18n ("Up"), this, SLOT (keyUp ()), 0, -1, 1);
    m_menu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("down"), KIcon::Small, 0, true), i18n ("Down"), this, SLOT (keyUp ()), 0, -1, 2);
    m_menu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("back"), KIcon::Small, 0, true), i18n ("Back"), this, SLOT (keyBack ()), 0, -1, 3);
    m_menu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("ok"), KIcon::Small, 0, true), i18n ("Ok"), this, SLOT (keyOk ()), 0, -1, 4);
    m_menu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("player_playlist"), KIcon::Small, 0, true), i18n ("Channels"), this, SLOT (keyChannels ()), 0, -1, 5);
    m_menu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("configure"), KIcon::Small, 0, true), i18n ("Setup"), this, SLOT (keySetup ()), 0, -1, 6);
    m_socket->connectToHost ("localhost", tcp_port);
    connect (m_socket, SIGNAL (connected ()), this, SLOT (connected ()));
    connect (m_socket, SIGNAL (readyRead ()), this, SLOT (readyRead ()));
    connect (m_socket, SIGNAL (bytesWritten (int)), this, SLOT (dataWritten (int)));
    connect (m_socket, SIGNAL (error (int)), this, SLOT (socketError (int)));
}

KDE_NO_EXPORT void KMPlayerVDRSource::deactivate () {
    disconnect (m_socket, SIGNAL (connected ()), this, SLOT (connected ()));
    disconnect (m_socket, SIGNAL (readyRead ()), this, SLOT (readyRead ()));
    disconnect (m_socket, SIGNAL (bytesWritten (int)), this, SLOT (dataWritten (int)));
    disconnect (m_socket, SIGNAL (error (int)), this, SLOT (socketError (int)));
    if (m_socket->state () == QSocket::Connected)
        sendCommand ("QUIT\n");
    m_menu->removeItemAt (6);
    m_menu->removeItemAt (5);
    m_menu->removeItemAt (4);
    m_menu->removeItemAt (3);
    m_menu->removeItemAt (2);
    m_menu->removeItemAt (1);
}

KDE_NO_EXPORT void KMPlayerVDRSource::connected () {
    if (!m_player->players () ["xvideo"]->playing ())
        QTimer::singleShot (0, m_player, SLOT (play ()));
    else if (commands) {
        m_socket->writeBlock (commands->command, strlen (commands->command));
        m_socket->flush ();
    }
}

KDE_NO_EXPORT void KMPlayerVDRSource::dataWritten (int) {
    if (commands) {
        VDRCommand * c = commands->next;
        delete commands;
        commands = c;
    }
    if (commands) {
        m_socket->writeBlock (commands->command, strlen (commands->command));
        m_socket->flush ();
    }
}

KDE_NO_EXPORT void KMPlayerVDRSource::readyRead () {
    unsigned long nr = m_socket->bytesAvailable();
    char * data = new char [nr + 1];
    m_socket->readBlock (data, nr);
    data [nr] = 0;
    KMPlayerView * v = static_cast <KMPlayerView *> (m_player->view ());
    if (v)
        v->addText (QString::fromLocal8Bit (str, nr));
    delete [] data;
}

KDE_NO_EXPORT void KMPlayerVDRSource::socketError (int code) {
    if (code == QSocket::ErrHostNotFound) {
        KMessageBox::error (m_configpage, i18n ("Host not found"), i18n ("Error"));
    } else if (code == QSocket::ErrConnectionRefused) {
        KMessageBox::error (m_configpage, i18n ("Connection refused"), i18n ("Error"));
    }
}

KDE_NO_EXPORT void KMPlayerVDRSource::sendCommand (const char * cmd) {
    if (m_player->process ()->source () != this)
        return;
    if (!commands) {
        commands = new VDRCommand (cmd);
        if (m_socket->state () == QSocket::Connected) {
            m_socket->writeBlock (commands->command, strlen(commands->command));
            m_socket->flush ();
        } else
            m_socket->connectToHost ("localhost", tcp_port);
    } else {
        VDRCommand * c = commands;
        for (int i = 0; i < 10; ++i)
            if (!c->next) {
                c->next = new VDRCommand (cmd);
                break;
            }
    }
}
    
KDE_NO_EXPORT void KMPlayerVDRSource::keyUp () {
    sendCommand ("HITK UP\n");
}

KDE_NO_EXPORT void KMPlayerVDRSource::keyDown () {
    sendCommand ("HITK DOWN\n");
}

KDE_NO_EXPORT void KMPlayerVDRSource::keyBack () {
    sendCommand ("HITK BACK\n");
}

KDE_NO_EXPORT void KMPlayerVDRSource::keyOk () {
    sendCommand ("HITK OK\n");
}

KDE_NO_EXPORT void KMPlayerVDRSource::keySetup () {
    sendCommand ("HITK SETUP\n");
}

KDE_NO_EXPORT void KMPlayerVDRSource::keyChannels () {
    sendCommand ("HITK CHANNELS\n");
}

KDE_NO_EXPORT void KMPlayerVDRSource::write (KConfig * m_config) {
    m_config->setGroup (strVDR);
    m_config->writeEntry (strVDRPort, tcp_port);
    m_config->writeEntry (strXVPort, xv_port);
}

KDE_NO_EXPORT void KMPlayerVDRSource::read (KConfig * m_config) {
    m_config->setGroup (strVDR);
    tcp_port = m_config->readNumEntry (strVDRPort, 2001);
    xv_port = m_config->readNumEntry (strXVPort, 146);
}

KDE_NO_EXPORT void KMPlayerVDRSource::sync (bool fromUI) {
    if (fromUI) {
        tcp_port = m_configpage->tcp_port->text ().toInt ();
        xv_port = m_configpage->xv_port->text ().toInt ();
        static_cast<XVideo *>(m_player->players()["xvideo"])->setPort(xv_port);
    } else {
        m_configpage->tcp_port->setText (QString::number (tcp_port));
        m_configpage->xv_port->setText (QString::number (xv_port));
    }
}

KDE_NO_EXPORT void KMPlayerVDRSource::prefLocation (QString & item, QString & icon, QString & tab) {
    item = i18n ("Source");
    icon = QString ("source");
    tab = i18n ("VDR");
}

KDE_NO_EXPORT QFrame * KMPlayerVDRSource::prefPage (QWidget * parent) {
    if (!m_configpage)
        m_configpage = new KMPlayerPrefSourcePageVDR (parent);
    return m_configpage;
}
//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT XVideo::XVideo (KMPlayer * player)
 : KMPlayerProcess  (player, "xvideo"), xv_port (0) {}

KDE_NO_CDTOR_EXPORT XVideo::~XVideo () {}

KDE_NO_EXPORT bool XVideo::play () {
    delete m_process;
    m_process = new KProcess;
    m_process->setUseShell (true);
    connect (m_process, SIGNAL (processExited (KProcess *)),
            this, SLOT (processStopped (KProcess *)));
    connect (m_process, SIGNAL (receivedStdout (KProcess *, char *, int)),
            this, SLOT (processOutput (KProcess *, char *, int)));
    connect (m_process, SIGNAL (receivedStderr (KProcess *, char *, int)),
            this, SLOT (processOutput (KProcess *, char *, int)));
    QString cmd  = QString ("rootv -port %1 -id %2 ").arg (xv_port).arg (view()->viewer()->embeddedWinId ());
    printf ("%s\n", cmd.latin1 ());
    *m_process << cmd;
    m_process->start (KProcess::NotifyOnExit, KProcess::All);
    if (m_process->isRunning ())
        emit startedPlaying ();
    return m_process->isRunning ();
}

KDE_NO_EXPORT bool XVideo::stop () {
    if (!playing ()) return true;
    if (view ())
        view ()->viewer ()->sendKeyEvent ('q');
#if KDE_IS_VERSION(3, 1, 90)
    m_process->wait(2);
#else
    QTime t;
    t.start ();
    do {
        KProcessController::theKProcessController->waitForProcessExit (2);
    } while (t.elapsed () < 2000 && m_process->isRunning ());
#endif
    return KMPlayerProcess::stop ();
}

KDE_NO_EXPORT void XVideo::processStopped (KProcess *) {
    QTimer::singleShot (0, this, SLOT (emitFinished ()));
}

KDE_NO_EXPORT void XVideo::processOutput (KProcess *, char * str, int slen) {
    KMPlayerView * v = view ();
    if (v && slen > 0)
        v->addText (QString::fromLocal8Bit (str, slen));
}
