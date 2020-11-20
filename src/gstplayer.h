/*
    This file is part of the KMPlayer application
    SPDX-FileCopyrightText: 2004 Koos Vriezen <koos.vriezen@xs4all.nl>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef _K_GST_PLAYER_H_
#define _K_GST_PLAYER_H_

#include <qapplication.h>
#include <qstring.h>
#include <qsessionmanager.h>

struct GstSizeEvent : public QEvent {
    GstSizeEvent (int l, int w, int h);
    int length;
    int width;
    int height;
};

struct GstProgressEvent : public QEvent {
    GstProgressEvent (int p);
    int progress;
};

class KGStreamerPlayer : public QApplication {
    Q_OBJECT
public:
    KGStreamerPlayer (int argc, char ** argv);
    ~KGStreamerPlayer ();

    void init ();
    void finished ();
    void saturation (int val);
    void hue (int val);
    void contrast (int val);
    void brightness (int val);
    void volume (int val);
    void seek (int val);
    bool event (QEvent * e);
public Q_SLOTS:
    void play (int repeat_count);
    void stop ();
    void pause ();
    void updatePosition ();
    //void postFinished ();
protected:
    void saveState (QSessionManager & sm);
};

#endif //_K_GST_PLAYER_H_
