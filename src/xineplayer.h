/**
 * Copyright (C) 2003 by Koos Vriezen <koos ! vriezen () xs4all ! nl>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 **/

#ifndef _KXINEPLAYER_H_
#define _KXINEPLAYER_H_

#include <qapplication.h>
#include <qstring.h>

class KXinePlayerPrivate;

struct XineSizeEvent : public QEvent {
    XineSizeEvent (int l, int w, int h);
    int length;
    int width;
    int height;
};

struct XineURLEvent : public QEvent {
    XineURLEvent (const QString & u);
    QString url;
};

struct XineTitleEvent : public QEvent {
    XineTitleEvent (const char *);
    QString title;
};

struct XineProgressEvent : public QEvent {
    XineProgressEvent (int p);
    int progress;
};

class KXinePlayer : public QApplication {
    Q_OBJECT
public:
    KXinePlayer (int argc, char ** argv);
    ~KXinePlayer ();

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
    void postFinished ();
private:
    KXinePlayerPrivate *d;
};

#endif //_KXINEPLAYER_H_
