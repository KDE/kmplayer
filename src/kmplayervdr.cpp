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

#include <klocale.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <klineedit.h>
#include <kurlrequester.h>
#include <kcombobox.h>
#include <kprocess.h>
#include <kconfig.h>

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

KDE_NO_CDTOR_EXPORT KMPlayerVDRSource::KMPlayerVDRSource (KMPlayerApp * app)
 : KMPlayerSource (i18n ("VDR"), app->player (), "vdrsource"),
   m_app (app),
   m_configpage (0),
   tcp_port (0),
   xv_port (0) {
    m_player->settings ()->addPage (this);
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
    m_player->setProcess ("xvideo");
    static_cast<XVideo *>(m_player->players () ["xvideo"])->setPort (xv_port);
    QTimer::singleShot (0, m_player, SLOT (play ()));
}

KDE_NO_EXPORT void KMPlayerVDRSource::deactivate () {
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
    QString cmd  = QString ("rootv -port %1 -id %2 ").arg (xv_port).arg (view()->viewer()->embeddedWinId ());
    printf ("%s\n", cmd.latin1 ());
    *m_process << cmd;
    m_process->start (KProcess::NotifyOnExit, KProcess::All);
    if (m_process->isRunning ())
        QTimer::singleShot (0, this, SLOT (emitStarted ()));
    return m_process->isRunning ();
}

KDE_NO_EXPORT bool XVideo::stop () {
    if (!playing ()) return true;
    return KMPlayerProcess::stop ();
}

KDE_NO_EXPORT void XVideo::processStopped (KProcess *) {
    QTimer::singleShot (0, this, SLOT (emitFinished ()));
}
