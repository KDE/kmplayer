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
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 **/

#ifndef _KXINEPLAYER_H_
#define _KXINEPLAYER_H_

#include <qapplication.h>
#include <qstring.h>
#include <qstringlist.h>


struct XineMovieParamEvent : public QEvent {
    XineMovieParamEvent (int l, int w, int h, const QStringList & al, const QStringList & sl, bool ff=false);
    int length;
    int width;
    int height;
    QStringList alang;
    QStringList slang;
    bool first_frame;
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
    void setAudioLang (int, const QString &);
    void setSubtitle (int, const QString &);
public slots:
    void play ();
    void stop ();
    void pause ();
    void updatePosition ();
    void postFinished ();
};

#endif //_KXINEPLAYER_H_
