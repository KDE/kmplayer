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
 * the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef KMPLAYERSOURCE_H
#define KMPLAYERSOURCE_H

#include <qobject.h>
#include <qstring.h>
#include <kurl.h>

#include "kmplayerplaylist.h"
#include "kmplayerprocess.h"

class KConfig;
class QWidget;
class QFrame;
class QListViewItem;

namespace KMPlayer {
    
class PartBase;

/**
 * Class for a certain media, like URL, DVD, TV etc
 */
class KMPLAYER_EXPORT Source : public QObject, public PlayListNotify {
    Q_OBJECT
public:
    Source (const QString & name, PartBase * player, const char * src);
    virtual ~Source ();
    virtual void init ();
    virtual bool processOutput (const QString & line);

    bool identified () const { return m_identified; }
    virtual bool hasLength ();
    virtual bool isSeekable ();

    KDE_NO_EXPORT int width () const { return m_width; }
    KDE_NO_EXPORT int height () const { return m_height; }
    virtual void dimensions (int & w, int & h) { w = m_width; h = m_height; }
    /* length () returns length in deci-seconds */
    KDE_NO_EXPORT int length () const { return m_length; }
    /* position () returns position in deci-seconds */
    KDE_NO_EXPORT int position () const { return m_position; }
    KDE_NO_EXPORT float aspect () const { return m_aspect; }
    KDE_NO_EXPORT const KURL & url () const { return m_url; }
    KDE_NO_EXPORT const KURL & subUrl () const { return m_sub_url; }
    PartBase * player () { return m_player; }
    void reset ();
    QString currentMrl ();
    QString mime () const;
    KDE_NO_EXPORT const QString & audioDevice () const { return m_audiodevice; }
    KDE_NO_EXPORT const QString & videoDevice () const { return m_videodevice; }
    KDE_NO_EXPORT const QString & videoNorm () const { return m_videonorm; }
    KDE_NO_EXPORT int frequency () const { return m_frequency; }
    KDE_NO_EXPORT int xvPort () const { return m_xvport; }
    KDE_NO_EXPORT int xvEncoding () const { return m_xvencoding; }
    KDE_NO_EXPORT const QString & pipeCmd () const { return m_pipecmd; }
    KDE_NO_EXPORT const QString & options () const { return m_options; }
    KDE_NO_EXPORT const QString & recordCmd () const { return m_recordcmd; }
    KDE_NO_EXPORT NodePtr current () const { return m_current; }
    KDE_NO_EXPORT NodePtr document () const { return m_document; }
    virtual QString filterOptions ();

    void setURL (const KURL & url);
    void insertURL (NodePtr mrl, const QString & url);
    KDE_NO_EXPORT void setSubURL (const KURL & url) { m_sub_url = url; }
    void setLanguages (const QStringList & alang, const QStringList & slang);
    void setMime (const QString & m);
    KDE_NO_EXPORT void setWidth (int w) { m_width = w; }
    KDE_NO_EXPORT void setHeight (int h) { m_height = h; }
    virtual void setDimensions (NodePtr, int w, int h);
    virtual void setAspect (NodePtr, float a);
    /* setLength (len) set length in deci-seconds */
    void setLength (NodePtr, int len);
    /* setPosition (pos) set position in deci-seconds */
    void setPosition (int pos);
    virtual void setIdentified (bool b = true);
    // backend process state changed
    virtual void stateChange (Process *, Process::State os, Process::State ns);
    KDE_NO_EXPORT void setAutoPlay (bool b) { m_auto_play = b; }
    void setTitle (const QString & title);
    void setLoading (int percentage);
    bool setCurrent (NodePtr mrl);

    virtual QString prettyName ();
signals:
    void startPlaying ();
    void stopPlaying ();
    void startRecording ();
    void stopRecording ();
    /**
     * Signal for notifying this source is at the end of play items
     */
    void endOfPlayItems ();
    void dimensionsChanged ();
    void titleChanged (const QString & title);
public slots:
    virtual void activate () = 0;
    virtual void deactivate () = 0;
    virtual void forward ();
    virtual void backward ();
    virtual void play ();
    /**
     * Continuing playing where current is now
     * May call play process if a video needs to play or 
     * emit endOfPlayItems when done
     */
    virtual void playCurrent ();
    virtual void jump (NodePtr e);
    void setAudioLang (int);
    void setSubtitle (int);
protected:
    /**
     * PlayListNotify implementation
     */
    bool requestPlayURL (NodePtr mrl);
    bool resolveURL (NodePtr mrl);
    void stateElementChanged (NodePtr element);
    void setEventDispatcher (NodePtr element);
    void setInfoMessage (const QString & msg);
    void repaintRect (int x, int y, int w, int h);
    void moveRect (int x, int y, int w, int h, int x1, int y1);
    void avWidgetSizes (int x, int y, int w, int h, unsigned int * bg_color);
    void bitRates (int & prefered, int & maximal);

    NodePtr m_document;
    NodePtrW m_current;
    NodePtrW m_back_request;
    QString m_name;
    PartBase * m_player;
    QString m_recordcmd;
    bool m_identified;
    bool m_auto_play;
    KURL m_url;
    KURL m_sub_url;
    QString m_audiodevice;
    QString m_videodevice;
    QString m_videonorm;
    int m_frequency;
    int m_xvport;
    int m_xvencoding;
    QString m_pipecmd;
    QString m_options;
private:
    int m_width;
    int m_height;
    float m_aspect;
    int m_length;
    int m_position;
};

} // namespace

#endif
