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

static const char * cmd_chan_query = "CHAN\n";
static const char * cmd_list_channels = "LSTC\n";

class VDRCommand {
public:
    KDE_NO_CDTOR_EXPORT VDRCommand (const char * c, bool b=false)
        : command (c), next (0L), waitForResponse (b) {}
    KDE_NO_CDTOR_EXPORT ~VDRCommand () {}
    const char * command;
    VDRCommand * next;
    bool waitForResponse;
};

//-----------------------------------------------------------------------------

KDE_NO_CDTOR_EXPORT KMPlayerVDRSource::KMPlayerVDRSource (KMPlayerApp * app, QPopupMenu * m)
 : KMPlayerMenuSource (i18n ("VDR"), app, m, "vdrsource"),
   m_configpage (0),
   m_socket (new QSocket (this)), 
   commands (0L),
   channel_timer (0),
   tcp_port (0),
   xv_port (0) {
    m_player->settings ()->addPage (this);
    act_up = new KAction (i18n ("VDR Key Up"), 0, 0, this, SLOT (keyUp ()), m_app->actionCollection (),"vdr_key_up");
    act_down = new KAction (i18n ("VDR Key Down"), 0, 0, this, SLOT (keyDown ()), m_app->actionCollection (),"vdr_key_down");
    act_back = new KAction (i18n ("VDR Key Back"), 0, 0, this, SLOT (keyBack ()), m_app->actionCollection (),"vdr_key_back");
    act_ok = new KAction (i18n ("VDR Key Ok"), 0, 0, this, SLOT (keyOk ()), m_app->actionCollection (),"vdr_key_ok");
    act_setup = new KAction (i18n ("VDR Key Setup"), 0, 0, this, SLOT (keySetup ()), m_app->actionCollection (),"vdr_key_setup");
    act_channels = new KAction (i18n ("VDR Key Channels"), 0, 0, this, SLOT (keyChannels ()), m_app->actionCollection (),"vdr_key_channels");
    act_menu = new KAction (i18n ("VDR Key Menu"), 0, 0, this, SLOT (keyMenu ()), m_app->actionCollection (),"vdr_key_menu");
    act_red = new KAction (i18n ("VDR Key Red"), 0, 0, this, SLOT (keyRed ()), m_app->actionCollection (),"vdr_key_red");
    act_green = new KAction (i18n ("VDR Key Green"), 0, 0, this, SLOT (keyGreen ()), m_app->actionCollection (),"vdr_key_green");
    act_yellow = new KAction (i18n ("VDR Key Yellow"), 0, 0, this, SLOT (keyYellow ()), m_app->actionCollection (),"vdr_key_yellow");
    act_blue = new KAction (i18n ("VDR Key Blue"), 0, 0, this, SLOT (keyBlue ()), m_app->actionCollection (),"vdr_key_blue");
}

KDE_NO_CDTOR_EXPORT KMPlayerVDRSource::~KMPlayerVDRSource () {
}

KDE_NO_EXPORT bool KMPlayerVDRSource::hasLength () {
    return false;
}

KDE_NO_EXPORT bool KMPlayerVDRSource::isSeekable () {
    return true;
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
    view->addFullscreenAction (i18n ("VDR Key Menu"), act_menu->shortcut (), this, SLOT (keyMenu ()), "vdr_key_menu");
    view->addFullscreenAction (i18n ("VDR Key 0"), KShortcut (Qt::Key_0), this, SLOT (key0 ()), "vdr_key_0");
    view->addFullscreenAction (i18n ("VDR Key 1"), KShortcut (Qt::Key_1), this, SLOT (key1 ()), "vdr_key_1");
    view->addFullscreenAction (i18n ("VDR Key 2"), KShortcut (Qt::Key_2), this, SLOT (key2 ()), "vdr_key_2");
    view->addFullscreenAction (i18n ("VDR Key 3"), KShortcut (Qt::Key_3), this, SLOT (key3 ()), "vdr_key_3");
    view->addFullscreenAction (i18n ("VDR Key 4"), KShortcut (Qt::Key_4), this, SLOT (key4 ()), "vdr_key_4");
    view->addFullscreenAction (i18n ("VDR Key 5"), KShortcut (Qt::Key_5), this, SLOT (key5 ()), "vdr_key_5");
    view->addFullscreenAction (i18n ("VDR Key 6"), KShortcut (Qt::Key_6), this, SLOT (key6 ()), "vdr_key_6");
    view->addFullscreenAction (i18n ("VDR Key 7"), KShortcut (Qt::Key_7), this, SLOT (key7 ()), "vdr_key_7");
    view->addFullscreenAction (i18n ("VDR Key 8"), KShortcut (Qt::Key_8), this, SLOT (key8 ()), "vdr_key_8");
    view->addFullscreenAction (i18n ("VDR Key 9"), KShortcut (Qt::Key_9), this, SLOT (key9 ()), "vdr_key_9");
    view->addFullscreenAction (i18n ("VDR Key Red"), act_red->shortcut (), this, SLOT (keyRed ()), "vdr_key_red");
    view->addFullscreenAction (i18n ("VDR Key Green"), act_green->shortcut (), this, SLOT (keyGreen ()), "vdr_key_green");
    view->addFullscreenAction (i18n ("VDR Key Yellow"), act_yellow->shortcut (), this, SLOT (keyYellow ()), "vdr_key_yellow");
    view->addFullscreenAction (i18n ("VDR Key Blue"), act_blue->shortcut (), this, SLOT (keyBlue ()), "vdr_key_blue");
    m_player->setProcess ("xvideo");
    static_cast<XVideo *>(m_player->players () ["xvideo"])->setPort (xv_port);
    m_menu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("up"), KIcon::Small, 0, true), i18n ("Up"), this, SLOT (keyUp ()), 0, -1, 1);
    m_menu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("down"), KIcon::Small, 0, true), i18n ("Down"), this, SLOT (keyUp ()), 0, -1, 2);
    m_menu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("back"), KIcon::Small, 0, true), i18n ("Back"), this, SLOT (keyBack ()), 0, -1, 3);
    m_menu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("ok"), KIcon::Small, 0, true), i18n ("Ok"), this, SLOT (keyOk ()), 0, -1, 4);
    m_menu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("player_playlist"), KIcon::Small, 0, true), i18n ("Channels"), this, SLOT (keyChannels ()), 0, -1, 5);
    m_menu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("configure"), KIcon::Small, 0, true), i18n ("Setup"), this, SLOT (keySetup ()), 0, -1, 6);
    m_menu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("showmenu"), KIcon::Small, 0, true), i18n ("Menu"), this, SLOT (keyMenu ()), 0, -1, 7);
    m_menu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("red"), KIcon::Small, 0, true), i18n ("Red"), this, SLOT (keyRed ()), 0, -1, 8);
    m_menu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("green"), KIcon::Small, 0, true), i18n ("Green"), this, SLOT (keyGreen ()), 0, -1, 9);
    m_menu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("yellow"), KIcon::Small, 0, true), i18n ("Yellow"), this, SLOT (keyYellow ()), 0, -1, 10);
    m_menu->insertItem (KGlobal::iconLoader ()->loadIconSet (QString ("blue"), KIcon::Small, 0, true), i18n ("Blue"), this, SLOT (keyBlue ()), 0, -1, 11);
    m_socket->connectToHost ("localhost", tcp_port);
    connect (m_socket, SIGNAL (connected ()), this, SLOT (connected ()));
    connect (m_socket, SIGNAL (readyRead ()), this, SLOT (readyRead ()));
    connect (m_socket, SIGNAL (bytesWritten (int)), this, SLOT (dataWritten (int)));
    connect (m_socket, SIGNAL (error (int)), this, SLOT (socketError (int)));
    channel_timer = startTimer (30000);
    m_document = (new Document (QString ("VDR")))->self ();
}

KDE_NO_EXPORT void KMPlayerVDRSource::deactivate () {
    disconnect (m_socket, SIGNAL (connected ()), this, SLOT (connected ()));
    disconnect (m_socket, SIGNAL (readyRead ()), this, SLOT (readyRead ()));
    disconnect (m_socket, SIGNAL (bytesWritten (int)), this, SLOT (dataWritten (int)));
    disconnect (m_socket, SIGNAL (error (int)), this, SLOT (socketError (int)));
    if (m_socket->state () == QSocket::Connected)
        sendCommand ("QUIT\n");
    m_menu->removeItemAt (11);
    m_menu->removeItemAt (10);
    m_menu->removeItemAt (9);
    m_menu->removeItemAt (8);
    m_menu->removeItemAt (7);
    m_menu->removeItemAt (6);
    m_menu->removeItemAt (5);
    m_menu->removeItemAt (4);
    m_menu->removeItemAt (3);
    m_menu->removeItemAt (2);
    m_menu->removeItemAt (1);
    killTimer (channel_timer);
    channel_timer = 0;
    if (m_document)
        m_document->document ()->dispose ();
    m_document = 0L;
}

KDE_NO_EXPORT void KMPlayerVDRSource::connected () {
    if (!m_player->players () ["xvideo"]->playing ()) {
        QTimer::singleShot (0, m_player, SLOT (play ()));
        //sendCommand (cmd_list_channels, true);
    } else if (commands) {
        m_socket->writeBlock (commands->command, strlen (commands->command));
        m_socket->flush ();
    }
}

KDE_NO_EXPORT void KMPlayerVDRSource::dataWritten (int) {
    if (commands) {
        if (commands->waitForResponse)
            return;
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
    kdDebug () << "readyRead " << nr << endl;
    if (commands && commands->waitForResponse) {
        if (!strcmp (commands->command, cmd_chan_query)) {
            data [strcspn (data, "\r\n")] = 0;
            m_player->changeTitle (QString (data));
        } /*else if (!strcmp (commands->command, cmd_list_channels)&&m_document) {
            char * d = data;
            int count = 0;
            do {
                int p = strcspn (d, "\r\n");
                d[p] = 0;
                if (p != 0)
                    m_document->appendChild ((new GenericURL (m_document, d))->self ());
                kdDebug () << d << endl;
                nr -= (p + 2);
                d += (p + 2);
                count++;
            } while (nr > 0 && count < 1000);
            m_player->updateTree (m_document, m_current);
        }*/
        VDRCommand * c = commands->next;
        delete commands;
        commands = c;
    } else {
        KMPlayerView * v = static_cast <KMPlayerView *> (m_player->view ());
        if (v)
            v->addText (QString::fromLocal8Bit (data, nr));
    }
    delete [] data;
}

KDE_NO_EXPORT void KMPlayerVDRSource::socketError (int code) {
    if (code == QSocket::ErrHostNotFound) {
        KMessageBox::error (m_configpage, i18n ("Host not found"), i18n ("Error"));
    } else if (code == QSocket::ErrConnectionRefused) {
        KMessageBox::error (m_configpage, i18n ("Connection refused"), i18n ("Error"));
    }
}

KDE_NO_EXPORT void KMPlayerVDRSource::sendCommand (const char * cmd, bool b) {
    if (m_player->process ()->source () != this)
        return;
    if (!commands) {
        commands = new VDRCommand (cmd, b);
        if (m_socket->state () == QSocket::Connected) {
            m_socket->writeBlock (commands->command, strlen(commands->command));
            m_socket->flush ();
        } else
            m_socket->connectToHost ("localhost", tcp_port);
    } else {
        VDRCommand * c = commands;
        for (int i = 0; i < 10; ++i)
            if (!c->next) {
                c->next = new VDRCommand (cmd, b);
                break;
            }
    }
}

KDE_NO_EXPORT void KMPlayerVDRSource::timerEvent (QTimerEvent *) {
    sendCommand (cmd_chan_query, true);
}

KDE_NO_EXPORT void KMPlayerVDRSource::jump (ElementPtr e) {
}
KDE_NO_EXPORT void KMPlayerVDRSource::forward () {
    sendCommand ("CHAN +\n");
}

KDE_NO_EXPORT void KMPlayerVDRSource::backward () {
    sendCommand ("CHAN -\n");
}

KDE_NO_EXPORT void KMPlayerVDRSource::keyUp () {
    //sendCommand ("HITK UP\n");
    KAction * action = ::qt_cast <KAction *> (sender ());
    kdDebug () << action << " " << action->name () << !strncmp ("vdr_key_", action->name (), 8) << endl;
    if (action && !strncmp ("vdr_key_", action->name (), 8))
        sendCommand (QString ("HITK %1\n").arg (action->name () + 8).ascii ());
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

KDE_NO_EXPORT void KMPlayerVDRSource::keyMenu () {
    sendCommand ("HITK MENU\n");
}

KDE_NO_EXPORT void KMPlayerVDRSource::key0 () {
    sendCommand ("HITK 0\n");
}

KDE_NO_EXPORT void KMPlayerVDRSource::key1 () {
    sendCommand ("HITK 1\n");
}

KDE_NO_EXPORT void KMPlayerVDRSource::key2 () {
    sendCommand ("HITK 2\n");
}

KDE_NO_EXPORT void KMPlayerVDRSource::key3 () {
    sendCommand ("HITK 3\n");
}

KDE_NO_EXPORT void KMPlayerVDRSource::key4 () {
    sendCommand ("HITK 4\n");
}

KDE_NO_EXPORT void KMPlayerVDRSource::key5 () {
    sendCommand ("HITK 5\n");
}

KDE_NO_EXPORT void KMPlayerVDRSource::key6 () {
    sendCommand ("HITK 6\n");
}

KDE_NO_EXPORT void KMPlayerVDRSource::key7 () {
    sendCommand ("HITK 7\n");
}

KDE_NO_EXPORT void KMPlayerVDRSource::key8 () {
    sendCommand ("HITK 8\n");
}

KDE_NO_EXPORT void KMPlayerVDRSource::key9 () {
    sendCommand ("HITK 9\n");
}

KDE_NO_EXPORT void KMPlayerVDRSource::keyRed () {
    sendCommand ("HITK RED\n");
}

KDE_NO_EXPORT void KMPlayerVDRSource::keyGreen () {
    sendCommand ("HITK GREEN\n");
}

KDE_NO_EXPORT void KMPlayerVDRSource::keyYellow () {
    sendCommand ("HITK YELLOW\n");
}

KDE_NO_EXPORT void KMPlayerVDRSource::keyBlue () {
    sendCommand ("HITK BLUE\n");
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

#include "kmplayervdr.moc"
