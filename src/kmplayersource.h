/* This file is part of the KDE project
 *
 * Copyright (C) 2003 Koos Vriezen <koos.vriezen@xs4all.nl>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef KMPLAYERSOURCE_H
#define KMPLAYERSOURCE_H

#include <qobject.h>
#include <qstring.h>
#include <kurl.h>

class KMPlayer;

class KMPlayerSource : public QObject {
    Q_OBJECT
public:
    KMPlayerSource (KMPlayer * player);
    virtual ~KMPlayerSource ();
    virtual void init ();
    virtual bool processOutput (const QString & line);

    bool identified () const { return m_identified; }
    virtual bool hasLength ();
    virtual bool isSeekable ();

    int width () const { return m_width; }
    int height () const { return m_height; }
    /* length () returns length in deci-seconds */
    int length () const { return m_length; }
    /* position () returns position in deci-seconds */
    int position () const { return m_position; }
    float aspect () const { return m_aspect > 0.01 ? m_aspect : (m_height > 0 ? (1.0*m_width)/m_height: 0.0); }
    const KURL & url () const { return m_url; }
    const QString & options () const { return m_options; }
    const QString & pipeCmd () const { return m_pipecmd; }
    const QString & recordCmd () const { return m_recordcmd; }
    virtual QString filterOptions ();

    void setWidth (int w) { m_width = w; }
    void setHeight (int h) { m_height = h; }
    void setAspect (float a) { m_aspect = a; }
    /* setLength (len) set length in deci-seconds */
    void setLength (int len) { m_length = len; }
    /* setPosition (pos) set position in deci-seconds */
    void setPosition (int pos) { m_position = pos; }
    virtual void setIdentified (bool b = true);
    virtual QString ffmpegCommand ();
public slots:
    virtual void activate () = 0;
    virtual void deactivate () = 0;
protected:
    KMPlayer * m_player;
    QString m_recordcmd;
    QString m_ffmpegCommand;
    bool m_identified;
    KURL m_url;
    QString m_options;
    QString m_pipecmd;
private:
    int m_width;
    int m_height;
    float m_aspect;
    int m_length;
    int m_position;
};

#endif
