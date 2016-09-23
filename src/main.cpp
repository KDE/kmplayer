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
#include <KAboutData>
#include <KLocalizedString>
#if KCrash_VERSION >= QT_VERSION_CHECK(5, 15, 0)
 #include <KCrash>
#endif

#include <QCommandLineParser>
#include <QApplication>
#include <QPointer>
#include <qfileinfo.h>

#include "kmplayer.h"

static QUrl makeUrl(const QString& link)
{
    QFileInfo info(link);
    if (info.exists())
        return QUrl::fromLocalFile(info.absoluteFilePath());
    return QUrl::fromUserInput(link);
}

extern "C" Q_DECL_EXPORT int kdemain(int argc, char **argv)
{
    setsid ();

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("kwrite"));
#if KCrash_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    KCrash::initialize();
#endif
    KLocalizedString::setApplicationDomain("kmplayer");

    KAboutData aboutData(QStringLiteral("kmplayer"),
            i18n("KMPlayer"),
            QStringLiteral(KMPLAYER_VERSION_STRING),
            i18n("Media player"), KAboutLicense::GPL,
            i18n("(c) 2002-2016, Koos Vriezen"), QString(), QStringLiteral("http://kmplayer.kde.org"));
    aboutData.addAuthor(i18n("Koos Vriezen"), i18n("Maintainer"), QStringLiteral("koos.vriezen@gmail.com"));
    aboutData.setProductName(QByteArray("kmplayer"));
    KAboutData::setApplicationData(aboutData);

    app.setApplicationName(aboutData.componentName());
    app.setApplicationDisplayName(aboutData.displayName());
    app.setOrganizationDomain(aboutData.organizationDomain());
    app.setApplicationVersion(aboutData.version());
    QApplication::setWindowIcon(QIcon::fromTheme(QLatin1String("kmplayer")));

    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);
    parser.setApplicationDescription(aboutData.shortDescription());
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("File"), i18n("file to open"), i18n("+[File]"));
    parser.process(app);

    aboutData.processCommandLine(&parser);

    KMPlayer::Ids::init();

    QPointer <KMPlayerApp> kmplayer;

    if (app.isSessionRestored ()) {
        RESTORE (KMPlayerApp);
    } else {
        kmplayer = new KMPlayerApp ();
        kmplayer->show();

        QUrl url;
        QStringList args = parser.positionalArguments();
        if (args.size() == 1)
            url = makeUrl(args[0]);
        if (args.size() > 1) {
            for (int i = 0; i < args.size(); i++) {
                QUrl url1 = makeUrl(args[i]);
                if (url1.isValid())
                    kmplayer->addUrl(url1);
            }
        }
        kmplayer->openDocumentFile (url);
    }
    int retvalue = app.exec ();

    delete kmplayer;

    KMPlayer::Ids::reset();

    return retvalue;
}
