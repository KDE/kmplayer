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

#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <klocale.h>
#include <dcopclient.h>

#include <qguardedptr.h>

#include "kmplayer.h"

static const char *description =
I18N_NOOP("KMPlayer");
// INSERT A DESCRIPTION FOR YOUR APPLICATION HERE


static KCmdLineOptions options[] =
{
    { "+[File]", I18N_NOOP("file to open"), 0 },
    { 0, 0, 0 }
    // INSERT YOUR COMMANDLINE OPTIONS HERE
};

int main (int argc, char *argv[])
{
    setsid ();

    KAboutData aboutData ("kmplayer", I18N_NOOP ("KMPlayer"),
            VERSION, description, KAboutData::License_GPL,
            "(c) 2002, Koos Vriezen", 0, 0, "");
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

        if (args->count ()) {
            KURL url = args->url(args->count() - 1);
            if(!url.isMalformed())
                kmplayer->openDocumentFile (args->url (0));
            else
                kmplayer->openDocumentFile ();
        } else {
            kmplayer->openDocumentFile ();
        }
        args->clear ();
    }
    app.dcopClient()->attach();
    int retvalue = app.exec ();
    
    delete kmplayer;

    return retvalue;
}
