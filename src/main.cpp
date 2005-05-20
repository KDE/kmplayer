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

#include <config.h>
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>
#include <kdemacros.h>
#include <dcopclient.h>

#include <qguardedptr.h>
#include <qfileinfo.h>

#include "kmplayer.h"

static const char description[] = I18N_NOOP("KMPlayer");


static KCmdLineOptions options[] =
{
    { "+[File]", I18N_NOOP("file to open"), 0 },
    KCmdLineLastOption
    // INSERT YOUR COMMANDLINE OPTIONS HERE
};

extern "C" {

    KDE_EXPORT int kdemain (int argc, char *argv[])
    {
        setsid ();

        KAboutData aboutData ("kmplayer", I18N_NOOP ("KMPlayer"),
                VERSION, description, KAboutData::License_GPL,
                "(c) 2002-2005, Koos Vriezen", 0, 0, "");
        aboutData.addAuthor( "Koos Vriezen",0, "");
        KCmdLineArgs::init (argc, argv, &aboutData);
        KCmdLineArgs::addCmdLineOptions (options); // Add our own options.

        KApplication app;
        QGuardedPtr <KMPlayerApp> kmplayer;

        if (app.isRestored ()) {
            RESTORE (KMPlayerApp);
        } else {
            kmplayer = new KMPlayerApp ();
            kmplayer->show();

            KCmdLineArgs *args = KCmdLineArgs::parsedArgs();

            KURL url;
            if (args->count () == 1)
                url = args->url (0);
            else
                for (int i = 0; i < args->count (); i++) {
                    KURL url = args->url (i);
                    if (url.url ().find ("://") < 0)
                        url = KURL (QFileInfo (url.url ()).absFilePath ());
                    if (url.isValid ())
                        kmplayer->addURL (url);
                }
            kmplayer->openDocumentFile (url);
            args->clear ();
        }
        app.dcopClient()->registerAs("kmplayer");
        int retvalue = app.exec ();

        delete kmplayer;

        return retvalue;
    }
}
