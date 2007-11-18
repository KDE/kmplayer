/**
 * Copyright (C) 2003 by Koos Vriezen <koos.vriezen@gmail.com>
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
#include <qsessionmanager.h>

struct XineMovieParamEvent : public QEvent {
    XineMovieParamEvent (unsigned long wid, int l, int w, int h,
            const QStringList &al, const QStringList &sl, bool ff=false);
    int length;
    int width;
    int height;
    QStringList alang;
    QStringList slang;
    bool first_frame;
    unsigned long wid;
};

struct XineURLEvent : public QEvent {
    XineURLEvent (unsigned long wid, const QString & u);
    QString url;
    unsigned long wid;
};

struct XineTitleEvent : public QEvent {
    XineTitleEvent (unsigned long wid, const char *);
    QString title;
    unsigned long wid;
};

struct XineProgressEvent : public QEvent {
    XineProgressEvent (unsigned long wid, int p);
    int progress;
    unsigned long wid;
};

struct XineFinishedEvent : public QEvent {
    XineFinishedEvent (unsigned long wid);
    unsigned long wid;
};

class KXinePlayer : public QApplication {
    Q_OBJECT
public:
    KXinePlayer (int argc, char ** argv);
    ~KXinePlayer ();

    void init ();
    void finished (unsigned long);
    void saturation (unsigned long, int val);
    void hue (unsigned long, int val);
    void contrast (unsigned long, int val);
    void brightness (unsigned long, int val);
    void volume (unsigned long, int val);
    void seek (unsigned long, int val);
    bool event (QEvent * e);
    void setAudioLang (unsigned long, int, const QString &);
    void setSubtitle (unsigned long, int, const QString &);
public slots:
    void play (unsigned long, int repeat_count);
    void play ();
    void stop (unsigned long);
    void pause (unsigned long);
    void updatePosition ();
protected:
    void saveState (QSessionManager & sm);
};

#endif //_KXINEPLAYER_H_
