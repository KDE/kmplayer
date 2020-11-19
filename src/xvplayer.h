/*
    This file is part of the KMPlayer application
    SPDX-FileCopyrightText: 2004 Koos Vriezen <koos.vriezen@xs4all.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef _K_XV_PLAYER_H_
#define _K_XV_PLAYER_H_

#include <qapplication.h>
#include <qstring.h>
#include <qsessionmanager.h>

class KXVideoPlayer : public QApplication {
    Q_OBJECT
public:
    KXVideoPlayer (int argc, char ** argv);
    ~KXVideoPlayer ();

    void init ();
    void finished ();
    void saturation (int val);
    void hue (int val);
    void contrast (int val);
    void brightness (int val);
    void volume (int val);
    void frequency (int val);
    //void seek (int val);
    //bool event (QEvent * e);
public slots:
    void play ();
    void stop ();
    //void pause ();
    //void updatePosition ();
    //void postFinished ();
protected:
    void saveState (QSessionManager & sm);
    void timerEvent (QTimerEvent *);
private:
    int mute_timer;
};

#endif //_K_XV_PLAYER_H_
