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
   the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#ifndef _K_GST_PLAYER_H_
#define _K_GST_PLAYER_H_

#include <qapplication.h>
#include <qstring.h>

struct GstSizeEvent : public QEvent {
    GstSizeEvent (int l, int w, int h);
    int length;
    int width;
    int height;
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
public slots:
    void play ();
    void stop ();
    void pause ();
    void updatePosition ();
    //void postFinished ();
};

#endif //_K_GST_PLAYER_H_
