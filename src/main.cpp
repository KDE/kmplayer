/***************************************************************************
  main.cpp  -  description
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
#include <unistd.h>

#include "config-kmplayer.h"
#include <kcmdlineargs.h>
#include <k4aboutdata.h>
#include <klocale.h>
#include <kdemacros.h>
#include <kapplication.h>

#include <QPointer>
#include <qfileinfo.h>

#include "kmplayer.h"


extern "C" KDE_EXPORT int kdemain (int argc, char *argv[]) {
    setsid ();

    K4AboutData aboutData ("kmplayer", 0, ki18n("KMPlayer"),
            KMPLAYER_VERSION_STRING,
            ki18n ("Media player."),
            K4AboutData::License_GPL,
            ki18n ("(c) 2002-2009, Koos Vriezen"),
            KLocalizedString(),
            I18N_NOOP ("http://kmplayer.kde.org"));
    aboutData.addAuthor(ki18n("Koos Vriezen"), ki18n("Maintainer"),"koos.vriezen@gmail.com");
    KCmdLineArgs::init (argc, argv, &aboutData);
    KCmdLineOptions options;
    options.add ("+[File]", ki18n ("file to open"));
    KCmdLineArgs::addCmdLineOptions (options);

    KMPlayer::Ids::init();

    KApplication app;
    QPointer <KMPlayerApp> kmplayer;

    if (app.isSessionRestored ()) {
        RESTORE (KMPlayerApp);
    } else {
        kmplayer = new KMPlayerApp ();
        kmplayer->show();

        KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

        KUrl url;
        if (args->count () == 1)
            url = args->url (0);
        if (args->count () > 1)
            for (int i = 0; i < args->count (); i++) {
                KUrl url = args->url (i);
                if (url.url ().indexOf ("://") < 0)
                    url = KUrl (QFileInfo (url.url ()).absoluteFilePath ());
                if (url.isValid ())
                    kmplayer->addUrl (url);
            }
        kmplayer->openDocumentFile (url);
        args->clear ();
    }
    int retvalue = app.exec ();

    delete kmplayer;

    KMPlayer::Ids::reset();

    return retvalue;
}
