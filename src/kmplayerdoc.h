/***************************************************************************
                          kmplayerdoc.h  -  description
                             -------------------
    begin                : Sat Dec  7 16:14:51 CET 2002
    copyright            : (C) 2002 by Koos Vriezen
    email                : 
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KMPLAYERDOC_H
#define KMPLAYERDOC_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif 

#include <qobject.h>
#include <qstring.h>
#include <qlist.h>

#include <kurl.h>

class KMPlayerView;

class KMPlayerDoc : public QObject
{
  Q_OBJECT
  public:
    KMPlayerDoc(QWidget *parent, const char *name=0);
    ~KMPlayerDoc();

    void addView(KMPlayerView *view);
    void removeView(KMPlayerView *view);
    void setModified(bool _m=true){ modified=_m; };
    bool isModified(){ return modified; };
    void deleteContents();
    bool newDocument();
    void closeDocument();
    bool openDocument(const KURL& url, const char *format=0);
    const KURL& URL() const;
    void setURL(const KURL& url);
    int width () { return m_width; }
    int height () { return m_height; }
    void setWidth (int w) { m_width = w; }
    void setHeight (int h) { m_height = h; }
    float aspect () { return m_aspect; }
    void setAspect (float a) { m_aspect = a; }
  public slots:
    void slotUpdateAllViews(KMPlayerView *sender);
 	
  public:	
    static QList<KMPlayerView> *pViewList;	

  private:
    bool modified;
    KURL doc_url;
    int m_width;
    int m_height;
    float m_aspect;
};

#endif // KMPLAYERDOC_H
