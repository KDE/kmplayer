/*
    This file is part of the KMPlayer application
    SPDX-FileCopyrightText: 2004 Koos Vriezen <koos.vriezen@xs4all.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <cmath>
#include <unistd.h>

#include <QLayout>
#include <QLabel>
#include <QMap>
#include <QTimer>
#include <QPushButton>
#include <Q3ButtonGroup>
#include <QCheckBox>
#include <QTable>
#include <QStringList>
#include <QComboBox>
#include <QLineEdit>
#include <QGroupBox>
#include <QWhatsThis>
#include <QTabWidget>
#include <QRadioButton>
#include <QMessageBox>
#include <QSocket>
#include <QEventLoop>

#include <KLocalizedString>
#include <KMessageBox>
#include <KLineEdit>
#include <KUrlRequester>
#include <KComboBox>
#include <kstatusbar.h>
#include <kprocess.h>
#include <KConfig>
#include <kaction.h>
#include <KIconLoader>
#include <klistview.h>
#include <kinputdialog.h>

#include "kmplayerapp_log.h"
#include "kmplayer_backend_stub.h"
#include "kmplayer_callback.h"
#include "kmplayerpartbase.h"
#include "kmplayercontrolpanel.h"
#include "kmplayerconfig.h"
#include "playlistview.h"
#include "viewarea.h"
#include "kmplayervdr.h"
#include "kmplayer.h"

using namespace KMPlayer;

static const char * strVDR = "VDR";
static const char * strVDRPort = "Port";
static const char * strXVPort = "XV Port";
static const char * strXVEncoding = "XV Encoding";
static const char * strXVScale = "XV Scale";

KMPlayerPrefSourcePageVDR::KMPlayerPrefSourcePageVDR (QWidget * parent, KMPlayer::PartBase * player)
 : QFrame (parent), m_player (player) {
    //KUrlRequester * v4ldevice;
    QVBoxLayout *layout = new QVBoxLayout (this, 5, 2);
    QGridLayout *gridlayout = new QGridLayout (1, 2);
    xv_port = new KListView (this);
    xv_port->addColumn (QString());
    xv_port->header()->hide ();
    xv_port->setTreeStepSize (15);
    //xv_port->setRootIsDecorated (true);
    //xv_port->setSorting (-1);
    Q3ListViewItem * vitem = new Q3ListViewItem (xv_port, i18n ("XVideo port"));
    vitem->setOpen (true);
    QWhatsThis::add (xv_port, i18n ("Port base of the X Video extension.\nIf left to default (0), the first available port will be used. However if you have multiple XVideo instances, you might have to provide the port to use here.\nSee the output from 'xvinfo' for more information"));
    QLabel * label = new QLabel (i18n ("Communication port:"), this);
    gridlayout->addWidget (label, 0, 0);
    tcp_port = new QLineEdit ("", this);
    QWhatsThis::add (tcp_port, i18n ("Communication port with VDR. Default is port 2001.\nIf you use another port, with the '-p' option of 'vdr', you must set it here too."));
    gridlayout->addWidget (tcp_port, 0, 1);
    layout->addWidget (xv_port);
    layout->addLayout (gridlayout);
    scale = new Q3ButtonGroup (2, Qt::Vertical, i18n ("Scale"), this);
    new QRadioButton (i18n ("4:3"), scale);
    new QRadioButton (i18n ("16:9"), scale);
    QWhatsThis::add (scale, i18n ("Aspects to use when viewing VDR"));
    scale->setButton (0);
    layout->addWidget (scale);
    layout->addItem (new QSpacerItem (5, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

KMPlayerPrefSourcePageVDR::~KMPlayerPrefSourcePageVDR () {}

void KMPlayerPrefSourcePageVDR::showEvent (QShowEvent *) {
    XvProcessInfo *pi = static_cast<XvProcessInfo *>(m_player->mediaManager()->
            processInfos()["xvideo"]);
    if (!pi->config_doc)
        pi->getConfigData ();
}
//-----------------------------------------------------------------------------

static const char * cmd_chan_query = "CHAN\n";
static const char * cmd_list_channels = "LSTC\n";
static const char * cmd_volume_query = "VOLU\n";

class VDRCommand {
public:
    VDRCommand (const char * c, VDRCommand * n=0L)
        : command (strdup (c)), next (n) {}
    ~VDRCommand () { free (command); }
    char * command;
    VDRCommand * next;
};

//-----------------------------------------------------------------------------

KMPlayerVDRSource::KMPlayerVDRSource (KMPlayerApp * app)
 : KMPlayer::Source (QString ("VDR"), app->player (), "vdrsource"),
   m_app (app),
   m_configpage (0),
   m_socket (new QSocket (this)), 
   commands (0L),
   channel_timer (0),
   timeout_timer (0),
   finish_timer (0),
   tcp_port (0),
   m_stored_volume (0) {
    memset (m_actions, 0, sizeof (KAction *) * int (act_last));
    m_player->settings ()->addPage (this);
    connect (m_socket, SIGNAL (connectionClosed()), this, SLOT(disconnected()));
    connect (m_socket, SIGNAL (connected ()), this, SLOT (connected ()));
    connect (m_socket, SIGNAL (readyRead ()), this, SLOT (readyRead ()));
    connect (m_socket, SIGNAL (error (int)), this, SLOT (socketError (int)));
}

KMPlayerVDRSource::~KMPlayerVDRSource () {}

void KMPlayerVDRSource::waitForConnectionClose () {
    if (timeout_timer) {
        finish_timer = startTimer (500);
        kdDebug () << "VDR connection not yet closed" << endl;
        QApplication::eventLoop ()->enterLoop ();
        kdDebug () << "VDR connection:" << (m_socket->state () == QSocket::Connected) << endl;
        timeout_timer = 0;
    }
}

bool KMPlayerVDRSource::hasLength () {
    return false;
}

bool KMPlayerVDRSource::isSeekable () {
    return true;
}

QString KMPlayerVDRSource::prettyName () {
    return i18n ("VDR");
}

void KMPlayerVDRSource::activate () {
    last_channel = 0;
    connect (this, SIGNAL (startPlaying ()), this, SLOT (processStarted()));
    connect (this, SIGNAL (stopPlaying ()), this, SLOT (processStopped ()));
    KMPlayer::ControlPanel * panel = m_app->view()->controlPanel ();
    panel->button (KMPlayer::ControlPanel::button_red)->show ();
    panel->button (KMPlayer::ControlPanel::button_green)->show ();
    panel->button (KMPlayer::ControlPanel::button_yellow)->show ();
    panel->button (KMPlayer::ControlPanel::button_blue)->show ();
    panel->button (KMPlayer::ControlPanel::button_pause)->hide ();
    panel->button (KMPlayer::ControlPanel::button_record)->hide ();
    connect (panel->volumeBar (), SIGNAL (volumeChanged (int)), this, SLOT (volumeChanged (int)));
    connect (panel->button (KMPlayer::ControlPanel::button_red), SIGNAL (clicked ()), this, SLOT (keyRed ()));
    connect (panel->button (KMPlayer::ControlPanel::button_green), SIGNAL (clicked ()), this, SLOT (keyGreen ()));
    connect (panel->button (KMPlayer::ControlPanel::button_yellow), SIGNAL (clicked ()), this, SLOT (keyYellow ()));
    connect (panel->button (KMPlayer::ControlPanel::button_blue), SIGNAL (clicked ()), this, SLOT (keyBlue ()));
    setAspect (m_document, scale ? 16.0/9 : 1.33);
    if (!m_url.scheme().compare ("kmplayer"))
        m_request_jump = QUrl::fromPercentEncoding(m_url.path().latin1()).mid (1);
    setUrl (QString ("vdr://localhost:%1").arg (tcp_port));
    QTimer::singleShot (0, m_player, SLOT (play ()));
}

void KMPlayerVDRSource::deactivate () {
    disconnect (m_socket, SIGNAL (error (int)), this, SLOT (socketError (int)));
    if (m_player->view ()) {
        disconnect (this, SIGNAL(startPlaying()), this, SLOT(processStarted()));
        disconnect (this, SIGNAL (stopPlaying()), this, SLOT(processStopped()));
        KMPlayer::ControlPanel * panel = m_app->view()->controlPanel ();
        disconnect (panel->volumeBar (), SIGNAL (volumeChanged (int)), this, SLOT (volumeChanged (int)));
        disconnect (panel->button (KMPlayer::ControlPanel::button_red), SIGNAL (clicked ()), this, SLOT (keyRed ()));
        disconnect (panel->button (KMPlayer::ControlPanel::button_green), SIGNAL (clicked ()), this, SLOT (keyGreen ()));
        disconnect (panel->button (KMPlayer::ControlPanel::button_yellow), SIGNAL (clicked ()), this, SLOT (keyYellow ()));
        disconnect (panel->button (KMPlayer::ControlPanel::button_blue), SIGNAL (clicked ()), this, SLOT (keyBlue ()));
    }
    processStopped ();
    m_request_jump.truncate (0);
}

void KMPlayerVDRSource::processStopped () {
    if (m_socket->state () == QSocket::Connected) {
        queueCommand (QString ("VOLU %1\n").arg (m_stored_volume).ascii ());
        queueCommand ("QUIT\n");
    }
}

void KMPlayerVDRSource::processStarted () {
    m_socket->connectToHost ("127.0.0.1", tcp_port);
    commands = new VDRCommand ("connect", commands);
}

#define DEF_ACT(i,text,pix,scut,slot,name) \
    m_actions [i] = new KAction (text, QString (pix), KShortcut (scut), this, slot, m_app->actionCollection (), name); \
    m_fullscreen_actions [i] = new KAction (text, KShortcut (scut), this, slot, m_app->view ()->viewArea ()->actionCollection (), name)

void KMPlayerVDRSource::connected () {
    queueCommand (cmd_list_channels);
    queueCommand (cmd_volume_query);
    killTimer (channel_timer);
    channel_timer = startTimer (3000);
    KAction * action = m_app->actionCollection ()->action ("vdr_connect");
    action->setIcon (QString ("network-disconnect"));
    action->setText (i18n ("Dis&connect"));
    DEF_ACT (act_up, i18n ("VDR Key Up"), "go-up", , SLOT (keyUp ()), "vdr_key_up");
    DEF_ACT (act_down, i18n ("VDR Key Down"), "go-down", , SLOT (keyDown ()), "vdr_key_down");
    DEF_ACT (act_back, i18n ("VDR Key Back"), "go-previous", , SLOT (keyBack ()), "vdr_key_back");
    DEF_ACT (act_ok, i18n ("VDR Key Ok"), "dialog-ok", , SLOT (keyOk ()), "vdr_key_ok");
    DEF_ACT (act_setup, i18n ("VDR Key Setup"), "configure", , SLOT (keySetup ()), "vdr_key_setup");
    DEF_ACT (act_channels, i18n ("VDR Key Channels"), "view-media-playlist", , SLOT (keyChannels ()), "vdr_key_channels");
    DEF_ACT (act_menu, i18n ("VDR Key Menu"), "show-menu", , SLOT (keyMenu ()), "vdr_key_menu");
    DEF_ACT (act_red, i18n ("VDR Key Red"), "red", , SLOT (keyRed ()), "vdr_key_red");
    DEF_ACT (act_green, i18n ("VDR Key Green"), "green", , SLOT (keyGreen ()), "vdr_key_green");
    DEF_ACT (act_yellow, i18n ("VDR Key Yellow"), "yellow", , SLOT (keyYellow ()), "vdr_key_yellow");
    DEF_ACT (act_blue, i18n ("VDR Key Blue"), "blue", , SLOT (keyBlue ()), "vdr_key_blue");
    DEF_ACT (act_custom, "VDR Custom Command", "system-run", , SLOT (customCmd ()), "vdr_key_custom");
    m_app->initMenu (); // update menu and toolbar
    DEF_ACT (act_0, i18n ("VDR Key 0"), "0", Qt::Key_0, SLOT (key0 ()), "vdr_key_0");
    DEF_ACT (act_1, i18n ("VDR Key 1"), "1", Qt::Key_1, SLOT (key1 ()), "vdr_key_1");
    DEF_ACT (act_2, i18n ("VDR Key 2"), "2", Qt::Key_2, SLOT (key2 ()), "vdr_key_2");
    DEF_ACT (act_3, i18n ("VDR Key 3"), "3", Qt::Key_3, SLOT (key3 ()), "vdr_key_3");
    DEF_ACT (act_4, i18n ("VDR Key 4"), "4", Qt::Key_4, SLOT (key4 ()), "vdr_key_4");
    DEF_ACT (act_5, i18n ("VDR Key 5"), "5", Qt::Key_5, SLOT (key5 ()), "vdr_key_5");
    DEF_ACT (act_6, i18n ("VDR Key 6"), "6", Qt::Key_6, SLOT (key6 ()), "vdr_key_6");
    DEF_ACT (act_7, i18n ("VDR Key 7"), "7", Qt::Key_7, SLOT (key7 ()), "vdr_key_7");
    DEF_ACT (act_8, i18n ("VDR Key 8"), "8", Qt::Key_8, SLOT (key8 ()), "vdr_key_8");
    DEF_ACT (act_9, i18n ("VDR Key 9"), "9", Qt::Key_9, SLOT (key9 ()), "vdr_key_9");
    //KMPlayer::ViewLayer * layer = m_app->view ()->viewArea ();
    for (int i = 0; i < int (act_last); ++i)
        // somehow, the configured shortcuts only show up after createGUI() call
        m_fullscreen_actions [i]->setShortcut (m_actions [i]->shortcut ());
    //    m_fullscreen_actions[i]->plug (layer);
}

#undef DEF_ACT

void KMPlayerVDRSource::disconnected () {
    kdDebug() << "disconnected " << commands << endl;
    if (finish_timer) {
        deleteCommands ();
        return;
    }
    setUrl (QString ("vdr://localhost:%1").arg (tcp_port));
    if (channel_timer && m_player->source () == this)
        m_player->stop ();
    deleteCommands ();
    KAction * action = m_app->actionCollection ()->action ("vdr_connect");
    action->setIcon (QString ("network-connect"));
    action->setText (i18n ("&Connect"));
    m_app->guiFactory ()->removeClient (m_app);// crash w/ m_actions[i]->unplugAll (); in for loop below
    for (int i = 0; i < int (act_last); ++i)
        if (m_player->view () && m_actions[i]) {
            m_fullscreen_actions[i]->unplug (m_app->view()->viewArea());
            delete m_actions[i];
            delete m_fullscreen_actions[i];
        }
    m_app->initMenu ();
}

void KMPlayerVDRSource::toggleConnected () {
    if (m_socket->state () == QSocket::Connected) {
        queueCommand ("QUIT\n");
        killTimer (channel_timer);
        channel_timer = 0;
    } else {
        m_socket->connectToHost ("127.0.0.1", tcp_port);
        commands = new VDRCommand ("connect", commands);
    }
}

void KMPlayerVDRSource::volumeChanged (int val) {
    queueCommand (QString ("VOLU %1\n").arg (int (sqrt (255 * 255 * val / 100))).ascii ());
}

static struct ReadBuf {
    char * buf;
    int length;
    ReadBuf () : buf (0L), length (0) {}
    ~ReadBuf () {
        clear ();
    }
    void operator += (const char * s) {
        int l = strlen (s);
        char * b = new char [length + l + 1];
        if (length)
            strcpy (b, buf);
        strcpy (b + length, s);
        length += l;
        delete buf;
        buf = b;
    }
    QCString mid (int p) {
        return QCString (buf + p);
    }
    QCString left (int p) {
        return QCString (buf, p);
    }
    QCString getReadLine ();
    void clear () {
        delete [] buf;
        buf = 0;
        length = 0;
    }
} readbuf;

QCString ReadBuf::getReadLine () {
    QCString out;
    if (!length)
        return out;
    int p = strcspn (buf, "\r\n");
    if (p < length) {
        int skip = strspn (buf + p, "\r\n");
        out = left (p+1);
        int nl = length - p - skip;
        memmove (buf, buf + p + skip, nl + 1);
        length = nl;
    }
    return out;
}

void KMPlayerVDRSource::readyRead () {
    KMPlayer::View * v = finish_timer ? 0L : static_cast <KMPlayer::View *> (m_player->view ());
    long nr = m_socket->bytesAvailable();
    char * data = new char [nr + 1];
    m_socket->readBlock (data, nr);
    data [nr] = 0;
    readbuf += data;
    QCString line = readbuf.getReadLine ();
    if (commands) {
        bool cmd_done = false;
        while (!line.isEmpty ()) {
            bool toconsole = true;
            cmd_done = (line.length () > 3 && line[3] == ' '); // from svdrpsend.pl
            // kdDebug () << "readyRead " << cmd_done << " " << commands->command << endl;
            if (!strcmp (commands->command, cmd_list_channels) && m_document) {
                int p = line.find (';');
                int q = line.find (':');
                if (q > 0 && (p < 0 || q < p))
                    p = q;
                if (p > 0)
                    line.truncate (p);
                QString channel_name = line.mid (4);
                m_document->appendChild (new KMPlayer::GenericMrl (m_document, QString ("kmplayer://vdrsource/%1").arg(channel_name), channel_name));
                if (cmd_done) {
                    m_player->updateTree ();
                    if (!m_request_jump.isEmpty ()) {
                        jump (m_request_jump);
                        m_request_jump.truncate (0);
                    }
                }
                toconsole = false;
            } else if (!strcmp (commands->command, cmd_chan_query)) {
                if (v && line.length () > 4) {
                    QString ch = line.mid (4);
                    setTitle (ch);
                    KMPlayer::PlayListItem * lvi = static_cast <KMPlayer::PlayListItem *> (v->playList ()->findItem (ch, 0));
                    if (lvi && lvi->node != m_last_channel) {
                        KMPlayer::PlayListItem * si = static_cast <KMPlayer::PlayListItem *> (v->playList ()->selectedItem ());
                        bool jump_selection = (si && (si->node == m_document || si->node == m_last_channel));
                        if (m_last_channel)
                            m_last_channel->setState (KMPlayer::Node::state_finished);
                        m_last_channel = lvi->node;
                        if (m_last_channel)
                            m_last_channel->setState (KMPlayer::Node::state_began);
                        if (jump_selection) {
                            v->playList ()->setSelected (lvi, true);
                            v->playList ()->ensureItemVisible (lvi);
                        }
                        v->playList ()->triggerUpdate ();
                    }
                    //v->playList ()->selectItem (ch);
                    int c = strtol(ch.ascii(), 0L, 10);
                    if (c != last_channel) {
                        last_channel = c;
                        m_app->statusBar ()->changeItem (QString::number (c),
                                                         id_status_timer);
                    }
                }
            } else if (cmd_done && !strcmp(commands->command,cmd_volume_query)){
                int pos = line.lastIndexOf (QChar (' '));
                if (pos > 0) {
                    QString vol = line.mid (pos + 1);
                    if (!vol.compare ("mute"))
                        m_stored_volume = 0;
                    else
                        m_stored_volume = vol.toInt ();
                    if (m_stored_volume == 0)
                        volumeChanged (m_app->view ()->controlPanel ()->volumeBar ()->value ());
                }
            }
            if (v && toconsole)
                v->addText (QString (line), true);
            line = readbuf.getReadLine ();
        }
        if (cmd_done) {
            VDRCommand * c = commands->next;
            delete commands;
            commands = c;
            if (commands)
                sendCommand ();
            else {
                killTimer (timeout_timer);
                timeout_timer = 0;
            }
        }
    }
    delete [] data;
}

void KMPlayerVDRSource::socketError (int code) {
    if (code == QSocket::ErrHostNotFound) {
        KMessageBox::error (m_configpage, i18n ("Host not found"), i18n ("Error"));
    } else if (code == QSocket::ErrConnectionRefused) {
        KMessageBox::error (m_configpage, i18n ("Connection refused"), i18n ("Error"));
    }
}

void KMPlayerVDRSource::queueCommand (const char * cmd) {
    if (m_player->source () != this)
        return;
    if (!commands) {
        readbuf.clear ();
        commands = new VDRCommand (cmd);
        if (m_socket->state () == QSocket::Connected) {
            sendCommand ();
        } else {
            m_socket->connectToHost ("127.0.0.1", tcp_port);
            commands = new VDRCommand ("connect", commands);
        }
    } else {
        VDRCommand * c = commands;
        for (int i = 0; i < 10; ++i, c = c->next)
            if (!c->next) {
                c->next = new VDRCommand (cmd);
                break;
            }
    }
}

void KMPlayerVDRSource::queueCommand (const char * cmd, int t) {
    queueCommand (cmd);
    killTimer (channel_timer);
    channel_timer = startTimer (t);
}

void KMPlayerVDRSource::sendCommand () {
    //kdDebug () << "sendCommand " << commands->command << endl;
    m_socket->writeBlock (commands->command, strlen(commands->command));
    m_socket->flush ();
    killTimer (timeout_timer);
    timeout_timer = startTimer (30000);
}

void KMPlayerVDRSource::customCmd () {
    QString cmd = KInputDialog::getText (i18n ("Custom VDR command"), i18n ("You can pass commands to VDR.\nEnter 'HELP' to see a list of available commands.\nYou can see VDR response in the console window.\n\nVDR Command:"), QString(), 0, m_player->view ());
    if (!cmd.isEmpty ())
        queueCommand (QString (cmd + QChar ('\n')).local8Bit ());
}

void KMPlayerVDRSource::timerEvent (QTimerEvent * e) {
    if (e->timerId () == timeout_timer || e->timerId () == finish_timer) {
        deleteCommands ();
    } else if (e->timerId () == channel_timer) {
        queueCommand (cmd_chan_query);
        killTimer (channel_timer);
        channel_timer = startTimer (30000);
    }
}

void KMPlayerVDRSource::deleteCommands () {
    killTimer (timeout_timer);
    timeout_timer = 0;
    killTimer (channel_timer);
    channel_timer = 0;
    for (VDRCommand * c = commands; c; c = commands) {
        commands = commands->next;
        delete c;
    }
    readbuf.clear ();
    if (finish_timer) {
        killTimer (finish_timer);
        QApplication::eventLoop ()->exitLoop ();
    }
}

void KMPlayerVDRSource::play (KMPlayer::Mrl *mrl) {
    if (!mrl || !mrl->isPlayable ()) return;
    jump (mrl->pretty_name);
}

void KMPlayerVDRSource::jump (const QString & channel) {
    QCString c ("CHAN ");
    QCString ch = channel.local8Bit ();
    int p = ch.find (' ');
    if (p > 0)
        c += ch.left (p);
    else
        c += ch; // hope for the best ..
    c += '\n';
    queueCommand (c);
}

void KMPlayerVDRSource::forward () {
    queueCommand ("CHAN +\n", 1000);
}

void KMPlayerVDRSource::backward () {
    queueCommand ("CHAN -\n", 1000);
}

void KMPlayerVDRSource::keyUp () {
    queueCommand ("HITK UP\n", 1000);
}

void KMPlayerVDRSource::keyDown () {
    queueCommand ("HITK DOWN\n", 1000);
}

void KMPlayerVDRSource::keyBack () {
    queueCommand ("HITK BACK\n");
}

void KMPlayerVDRSource::keyOk () {
    queueCommand ("HITK OK\n");
}

void KMPlayerVDRSource::keySetup () {
    queueCommand ("HITK SETUP\n");
}

void KMPlayerVDRSource::keyChannels () {
    queueCommand ("HITK CHANNELS\n");
}

void KMPlayerVDRSource::keyMenu () {
    queueCommand ("HITK MENU\n");
}

void KMPlayerVDRSource::key0 () {
    queueCommand ("HITK 0\n", 2000);
}

void KMPlayerVDRSource::key1 () {
    queueCommand ("HITK 1\n", 2000);
}

void KMPlayerVDRSource::key2 () {
    queueCommand ("HITK 2\n", 2000);
}

void KMPlayerVDRSource::key3 () {
    queueCommand ("HITK 3\n", 2000);
}

void KMPlayerVDRSource::key4 () {
    queueCommand ("HITK 4\n", 2000);
}

void KMPlayerVDRSource::key5 () {
    queueCommand ("HITK 5\n", 2000);
}

void KMPlayerVDRSource::key6 () {
    queueCommand ("HITK 6\n", 2000);
}

void KMPlayerVDRSource::key7 () {
    queueCommand ("HITK 7\n", 2000);
}

void KMPlayerVDRSource::key8 () {
    queueCommand ("HITK 8\n", 2000);
}

void KMPlayerVDRSource::key9 () {
    queueCommand ("HITK 9\n", 2000);
}

void KMPlayerVDRSource::keyRed () {
    queueCommand ("HITK RED\n");
}

void KMPlayerVDRSource::keyGreen () {
    queueCommand ("HITK GREEN\n");
}

void KMPlayerVDRSource::keyYellow () {
    queueCommand ("HITK YELLOW\n");
}

void KMPlayerVDRSource::keyBlue () {
    queueCommand ("HITK BLUE\n");
}

void KMPlayerVDRSource::write (KConfig * m_config) {
    m_config->setGroup (strVDR);
    m_config->writeEntry (strVDRPort, tcp_port);
    m_config->writeEntry (strXVPort, m_xvport);
    m_config->writeEntry (strXVEncoding, m_xvencoding);
    m_config->writeEntry (strXVScale, scale);
}

void KMPlayerVDRSource::read (KConfig * m_config) {
    m_config->setGroup (strVDR);
    tcp_port = m_config->readNumEntry (strVDRPort, 2001);
    m_xvport = m_config->readNumEntry (strXVPort, 0);
    m_xvencoding = m_config->readNumEntry (strXVEncoding, 0);
    scale = m_config->readNumEntry (strXVScale, 0);
}

struct XVTreeItem : public Q3ListViewItem {
    XVTreeItem (Q3ListViewItem *parent, const QString & t, int p, int e)
        : Q3ListViewItem (parent, t), port (p), encoding (e) {}
    int port;
    int encoding;
};

void KMPlayerVDRSource::sync (bool fromUI) {
    XvProcessInfo *pi = static_cast<XvProcessInfo *>(m_player->mediaManager()->
            processInfos()["xvideo"]);
    if (fromUI) {
        tcp_port = m_configpage->tcp_port->text ().toInt ();
        scale = m_configpage->scale->id (m_configpage->scale->selected ());
        setAspect (m_document, scale ? 16.0/9 : 1.25);
        XVTreeItem * vitem = dynamic_cast <XVTreeItem *> (m_configpage->xv_port->selectedItem ());
        if (vitem) {
            m_xvport = vitem->port;
            m_xvencoding = vitem->encoding;
        }
    } else {
        m_configpage->tcp_port->setText (QString::number (tcp_port));
        m_configpage->scale->setButton (scale);
        Q3ListViewItem * vitem = m_configpage->xv_port->firstChild ();
        NodePtr configdoc = pi->config_doc;
        if (configdoc && configdoc->firstChild ()) {
            for (Q3ListViewItem *i=vitem->firstChild(); i; i=vitem->firstChild())
                delete i;
            NodePtr node = configdoc->firstChild ();
            for (node = node->firstChild (); node; node = node->nextSibling()) {
                if (!node->isElementNode ())
                    continue; // some text sneaked in ?
                Element * elm = convertNode <Element> (node);
                if (elm->getAttribute (KMPlayer::StringPool::attr_type) !=
                        QString ("tree"))
                    continue;
                for (NodePtr n = elm->firstChild (); n; n = n->nextSibling ()) {
                    if (!n->isElementNode () || strcmp (n->nodeName (), "Port"))
                        continue;
                    Element * e = convertNode <Element> (n);
                    QString portatt = e->getAttribute (
                            KMPlayer::StringPool::attr_value);
                    int port;
                    Q3ListViewItem *pi = new Q3ListViewItem (vitem, i18n ("Port ") + portatt);
                    port = portatt.toInt ();
                    for (NodePtr in=e->firstChild(); in; in=in->nextSibling()) {
                        if (!in->isElementNode () ||
                                strcmp (in->nodeName (), "Input"))
                            continue;
                        Element * i = convertNode <Element> (in);
                        QString inp = i->getAttribute (
                                KMPlayer::StringPool::attr_name);
                        int enc = i->getAttribute (
                                KMPlayer::StringPool::attr_value).toInt ();
                        Q3ListViewItem * ii = new XVTreeItem(pi, inp, port, enc);
                        if (m_xvport == port && enc == m_xvencoding) {
                            ii->setSelected (true);
                            m_configpage->xv_port->ensureItemVisible (ii);
                        }
                    }
                }
            }
        } else { // wait for showEvent
            connect(pi, SIGNAL(configReceived()), this, SLOT(configReceived()));
        }
    }
}

void KMPlayerVDRSource::configReceived () {
    XvProcessInfo *pi = static_cast<XvProcessInfo *>(m_player->mediaManager()->
            processInfos()["xvideo"]);
    disconnect (pi, SIGNAL (configReceived()), this, SLOT (configReceived()));
    sync (false);
}

void KMPlayerVDRSource::prefLocation (QString & item, QString & icon, QString & tab) {
    item = i18n ("Source");
    icon = QString ("source");
    tab = i18n ("VDR");
}

QFrame * KMPlayerVDRSource::prefPage (QWidget * parent) {
    if (!m_configpage)
        m_configpage = new KMPlayerPrefSourcePageVDR (parent, m_player);
    return m_configpage;
}

void KMPlayerVDRSource::stateElementChanged (KMPlayer::Node *, KMPlayer::Node::State, KMPlayer::Node::State) {
}

//-----------------------------------------------------------------------------

static const char * xv_supported [] = {
    "tvsource", "vdrsource", 0L
};

XvProcessInfo::XvProcessInfo (MediaManager *mgr)
 : CallbackProcessInfo ("xvideo", i18n ("X&Video"), xv_supported, mgr, NULL) {}

IProcess *XvProcessInfo::create (PartBase *part, AudioVideoMedia *media) {
    XVideo *x = new XVideo (part, this, part->settings ());
    x->setSource (part->source ());
    x->media_object = media;
    part->processCreated (x);
    return x;
}

bool XvProcessInfo::startBackend () {
    if (m_process && m_process->isRunning ())
        return true;
    Settings *cfg = manager->player ()->settings();

    initProcess ();

    QString cmd  = QString ("kxvplayer -cb %1").arg (dcopName ());
    if (have_config == config_unknown || have_config == config_probe)
        cmd += QString (" -c");
    //if (m_source) {
    //    int xv_port = m_source->xvPort ();
    //    int xv_encoding = m_source->xvEncoding ();
    //    int freq = m_source->frequency ();
    //    cmd += QString (" -port %1 -enc %2 -norm \"%3\"").arg (xv_port).arg (xv_encoding).arg (m_source->videoNorm ());
    //    if (freq > 0)
    //        cmd += QString (" -freq %1").arg (freq);
    //}
    fprintf (stderr, "%s\n", cmd.latin1 ());
    *m_process << cmd;
    m_process->start (KProcess::NotifyOnExit, KProcess::All);

    return m_process->isRunning ();
}

XVideo::XVideo (QObject *p, ProcessInfo *pi, Settings *s)
 : KMPlayer::CallbackProcess (p, pi, s, "xvideo") {
    //m_player->settings ()->addPage (m_configpage);
}

XVideo::~XVideo () {}

bool XVideo::ready () {
    if (running () || !media_object || !media_object->viewer) {
        setState (IProcess::Ready);
        return true;
    }
    return static_cast <XvProcessInfo *> (process_info)->startBackend ();
}

#include "kmplayervdr.moc"

#include "moc_kmplayervdr.cpp"
