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

class KXinePlayerPrivate;

class KXinePlayer : public QApplication {
    Q_OBJECT
public:
    KXinePlayer (int argc, char ** argv);
    ~KXinePlayer ();
    void setURL (const QString & url);
    void finished ();
public slots:
    void play ();
    void stop ();
private:
    KXinePlayerPrivate *d;
};

#endif //_KXINEPLAYER_H_
