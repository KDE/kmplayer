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

#ifndef _K_XV_PLAYER_H_
#define _K_XV_PLAYER_H_

#include <qapplication.h>
#include <qstring.h>

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
};

#endif //_K_XV_PLAYER_H_
