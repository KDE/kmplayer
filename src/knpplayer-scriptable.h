/* This file is part of the KDE project
 *
 * Copyright (C) 2010 Koos Vriezen <koos.vriezen@gmail.com>
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

#ifndef _KMPLAYER_KNPPLAYER_SE_H_
#define _KMPLAYER_KNPPLAYER_SE_H_

#include <QDBusVariant>

#include "kmplayerprocess.h"

namespace KMPlayer {


class KMPLAYER_NO_EXPORT NpPlayer : public Process {
    Q_OBJECT
public:
    NpPlayer (QObject *, KMPlayer::ProcessInfo*, Settings *);
    ~NpPlayer ();

    static const char *name;
    static const char *supports [];
    static IProcess *create (PartBase *, ProcessUser *);

    virtual void init ();
    virtual bool deMediafiedPlay ();
    virtual void initProcess ();

    using Process::running;
    QDBusVariant call(const QDBusVariant &object, const QString &function, const QVariantList &arguments) KMPLAYER_NO_MBR_EXPORT;
    QDBusVariant get(const QDBusVariant &object, const QString &field) KMPLAYER_NO_MBR_EXPORT;
    QDBusVariant root() KMPLAYER_NO_MBR_EXPORT;
    void acquire (const QDBusVariant &obj) KMPLAYER_NO_MBR_EXPORT;
    void release (const QDBusVariant &obj) KMPLAYER_NO_MBR_EXPORT;
    void running (const QString &srv) KMPLAYER_NO_MBR_EXPORT;
    void plugged () KMPLAYER_NO_MBR_EXPORT;
    void request_stream (const QString &path, const QString &url, const QString &target, const QByteArray &post) KMPLAYER_NO_MBR_EXPORT;
    QString evaluate (const QString &script, bool store) KMPLAYER_NO_MBR_EXPORT;
    void dimension (int w, int h) KMPLAYER_NO_MBR_EXPORT;

    void destroyStream (uint32_t sid);

    KDE_NO_EXPORT const QString & destination () const { return service; }
    KDE_NO_EXPORT const QString & interface () const { return iface; }
    KDE_NO_EXPORT QString objectPath () const { return path; }
    virtual void stop ();
    virtual void quit ();
    bool ready ();
signals:
    void evaluate (const QString &scr, QVariant &result);
    void loaded ();
    void objectCall (const QVariant &obj, const QString &func,
            const QVariantList &args, QVariant &result);
    void objectGet (const QVariant &obj, const QString &func,
            QVariant &result);
    void hostRoot (QVariant &result);
    void acquireObject (const QVariant &object);
    void releaseObject (const QVariant &object);
public slots:
    void requestGet (const QVariant&, const QString&, QVariantList&, bool*);
    void requestCall (const QVariant&, const QString&, const QVariantList&, QVariant&, bool *);
private slots:
    void processOutput ();
    void processStopped (int, QProcess::ExitStatus);
    void wroteStdin (qint64);
    void streamStateChanged ();
    void streamRedirected (uint32_t, const KUrl &);
protected:
    virtual void terminateJobs ();
private:
    void sendFinish (uint32_t sid, uint32_t total, NpStream::Reason because);
    void processStreams ();
    QString service;
    QString iface;
    QString path;
    QString filter;
    typedef QMap <uint32_t, NpStream *> StreamMap;
    StreamMap streams;
    QString remote_service;
    QString m_base_url;
    QByteArray send_buf;
    bool write_in_progress;
    bool in_process_stream;
};

} // namespace

#endif
