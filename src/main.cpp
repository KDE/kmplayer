/*
    SPDX-FileCopyrightText: 2002 Koos Vriezen

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <unistd.h>

#include "config-kmplayer.h"
#include <KAboutData>
#include <KLocalizedString>

#include <QCommandLineParser>
#include <QApplication>
#include <QPointer>
#include <QFileInfo>

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
    KLocalizedString::setApplicationDomain("kmplayer");

    KAboutData aboutData(QStringLiteral("kmplayer"),
            i18n("KMPlayer"),
            QStringLiteral(KMPLAYER_VERSION_STRING),
            i18n("Media player"), KAboutLicense::GPL,
            i18n("(c) 2002-2016, Koos Vriezen"), QString(), QStringLiteral("http://kmplayer.kde.org"));
    aboutData.addAuthor(i18n("Koos Vriezen"), i18n("Maintainer"), QStringLiteral("koos.vriezen@gmail.com"));
    aboutData.setProductName(QByteArray("kmplayer"));
    aboutData.setOrganizationDomain(QByteArray("kde.org"));
    aboutData.setDesktopFileName(QStringLiteral("org.kde.kmplayer"));
    KAboutData::setApplicationData(aboutData);

    QApplication::setWindowIcon(QIcon::fromTheme(QLatin1String("kmplayer")));

    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);
    parser.addPositionalArgument(QStringLiteral("File"), i18n("file to open"), i18n("+[File]"));
    parser.process(app);

    aboutData.processCommandLine(&parser);

    KMPlayer::Ids::init();

    QPointer <KMPlayerApp> kmplayer;

    if (app.isSessionRestored ()) {
        kRestoreMainWindows<KMPlayerApp>();
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
