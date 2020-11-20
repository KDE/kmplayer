/*
    SPDX-FileCopyrightText: 2003 Koos Vriezen <koos.vriezen@gmail.com>

    SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef _KXINEPLAYER_H_
#define _KXINEPLAYER_H_

#include <qapplication.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qsessionmanager.h>

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
public Q_SLOTS:
    void play (int repeat_count);
    void stop ();
    void pause ();
    void updatePosition ();
    void postFinished ();
protected:
    void saveState (QSessionManager & sm);
};

#endif //_KXINEPLAYER_H_
