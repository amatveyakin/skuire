/***************************************************************************
                              kiojobwrapper.cpp
                             -------------------
    copyright            : (C) 2008+ by Csaba Karai
    email                : krusader@users.sourceforge.net
    web site             : http://krusader.sourceforge.net
 ---------------------------------------------------------------------------
  Description
 ***************************************************************************

  A

     db   dD d8888b. db    db .d8888.  .d8b.  d8888b. d88888b d8888b.
     88 ,8P' 88  `8D 88    88 88'  YP d8' `8b 88  `8D 88'     88  `8D
     88,8P   88oobY' 88    88 `8bo.   88ooo88 88   88 88ooooo 88oobY'
     88`8b   88`8b   88    88   `Y8b. 88~~~88 88   88 88~~~~~ 88`8b
     88 `88. 88 `88. 88b  d88 db   8D 88   88 88  .8D 88.     88 `88.
     YP   YD 88   YD ~Y8888P' `8888Y' YP   YP Y8888D' Y88888P 88   YD

                                                     S o u r c e    F i l e

 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "kiojobwrapper.h"

#include <cstdio>
#include <iostream>

#include <QtCore/QEvent>
#include <QtGui/QApplication>
#include <QtGui/QTextDocument>

#include <kurl.h>
#include <kio/global.h>
#include <kio/jobclasses.h>
#include <kio/directorysizejob.h>
#include <kio/jobuidelegate.h>
#include <kio/job.h>
#include <klocale.h>
#include <kdebug.h>

#include "virtualcopyjob.h"
#include "packjob.h"

#include "vfs/virtualaddjob.h"

class JobStartEvent : public QEvent
{
public:
    JobStartEvent(KIOJobWrapper * wrapperIn) : QEvent(QEvent::User),
            m_wrapper(wrapperIn) {}
    virtual ~JobStartEvent() {}

    KIOJobWrapper * wrapper() {
        return m_wrapper;
    }
private:
    QPointer<KIOJobWrapper> m_wrapper;
};

KrJobStarter * KrJobStarter::m_self = 0;

bool KrJobStarter::event(QEvent * e)
{
    if (e->type() == QEvent::User) {
        JobStartEvent *je = (JobStartEvent *)e;
        je->wrapper()->createJob();
        return true;
    }
    return QObject::event(e);
}

KIOJobWrapper::KIOJobWrapper(KIOJobWrapperType type, const KUrl &url) :
        m_vcopy_job(0), m_vadd_job(0),
        m_autoErrorHandling(false), m_started(false),
        m_suspended(false)
{
    moveToThread(QApplication::instance()->thread());
    m_type = type;
    m_url = url;
}

KIOJobWrapper::KIOJobWrapper(KIOJobWrapperType type, const KUrl &url, VirtualCopyJob* vcopy_job) :
        m_vcopy_job(vcopy_job), m_vadd_job(0),
        m_autoErrorHandling(false), m_started(false),
        m_suspended(false)
{
    moveToThread(QApplication::instance()->thread());
    m_type = type;
    m_url = url;
}

KIOJobWrapper::KIOJobWrapper(KIOJobWrapperType type, const KUrl &url, VFS::VirtualAddJob* vadd_job) :
        m_vcopy_job(0), m_vadd_job(vadd_job),
        m_autoErrorHandling(false), m_started(false),
        m_suspended(false)
{
    moveToThread(QApplication::instance()->thread());
    m_type = type;
    m_url = url;
}

KIOJobWrapper::KIOJobWrapper(KIOJobWrapperType type, const KUrl &url, const KUrl::List &list,
                             int pmode, bool showp) :
        m_vcopy_job(0), m_vadd_job(0),
        m_autoErrorHandling(false), m_started(false),
        m_suspended(false)
{
    moveToThread(QApplication::instance()->thread());
    m_type = type;
    m_url = url;
    m_urlList = list;
    m_pmode = pmode;
    m_showProgress = showp;
}

KIOJobWrapper::KIOJobWrapper(KIOJobWrapperType type, const KUrl &url, const KUrl &dest, const QStringList &names,
                             bool showp, const QString &atype, const QMap<QString, QString> &packProps) :
        m_urlList(), m_vcopy_job(0), m_vadd_job(0),
        m_autoErrorHandling(false), m_started(false),
        m_suspended(false)
{
    m_type = type;
    m_url = dest;
    m_archiveSourceBase = url;
    foreach(const QString &name , names) {
        KUrl srcUrl = url;
        srcUrl.addPath(name);
        m_urlList << srcUrl;
    }
    m_archiveFileNames = names;
    m_showProgress = showp;
    m_archiveType = atype;
    m_archiveProperties = packProps;
}

KIOJobWrapper::~KIOJobWrapper()
{
}

void KIOJobWrapper::createJob()
{
    KIO::Job * job = 0;
    switch (m_type) {
    case Stat:
        job = KIO::stat(m_url);
        break;
    case DirectorySize:
        job = KIO::directorySize(m_url);
        break;
    case VirtualMove:
    case VirtualCopy:
        QTimer::singleShot(0, m_vcopy_job, SLOT(slotStart()));
        job = m_vcopy_job;
        break;
    case VirtualAdd:
        QTimer::singleShot(0, m_vadd_job, SLOT(slotStart()));
        job = m_vadd_job;
        break;
    case Copy:
        job = PreservingCopyJob::createCopyJob((PreserveMode)m_pmode, m_urlList, m_url, KIO::CopyJob::Copy, false, m_showProgress);
        break;
    case Move:
        job = PreservingCopyJob::createCopyJob((PreserveMode)m_pmode, m_urlList, m_url, KIO::CopyJob::Move, false, m_showProgress);
        break;
    case Pack:
        job = PackJob::createPacker(m_archiveSourceBase, m_url, m_archiveFileNames, m_archiveType, m_archiveProperties);
        break;
    case Unpack:
        job = UnpackJob::createUnpacker(m_archiveSourceBase, m_url, m_archiveFileNames);
        break;
    default:
        fprintf(stderr, "Internal error: invalid job!\n");
        break;
    }
    if (job) {
        m_job = job;
        connect(job, SIGNAL(destroyed()), this, SLOT(deleteLater()));
        for (int i = 0; i != m_signals.count(); i++)
            if (!m_receivers[ i ].isNull())
                connect(job, m_signals[ i ], m_receivers[ i ], m_methods[ i ]);

        if (m_autoErrorHandling && job->ui())
            job->ui()->setAutoErrorHandlingEnabled(true);
        if (m_suspended)
            job->suspend();
    } else
        deleteLater();
}

KIOJobWrapper * KIOJobWrapper::stat(KUrl &url)
{
    return new KIOJobWrapper(Stat, url);
}

KIOJobWrapper * KIOJobWrapper::directorySize(KUrl &url)
{
    return new KIOJobWrapper(DirectorySize, url);
}

KIOJobWrapper * KIOJobWrapper::copy(int pmode, KUrl::List &list, KUrl &url, bool showProgress)
{
    return new KIOJobWrapper(Copy, url, list, pmode, showProgress);
}

KIOJobWrapper * KIOJobWrapper::move(int pmode, KUrl::List &list, KUrl &url, bool showProgress)
{
    return new KIOJobWrapper(Move, url, list, pmode, showProgress);
}

KIOJobWrapper * KIOJobWrapper::virtualAdd(KUrl::List urls, QString destDir)
{
    return new KIOJobWrapper(VirtualAdd, KUrl(),
                             new VFS::VirtualAddJob(urls, destDir));
}

KIOJobWrapper * KIOJobWrapper::virtualCopy(const KFileItemList &files, KUrl& dest,
        const KUrl& baseURL, int pmode, bool showProgressInfo)
{
    return new KIOJobWrapper(VirtualCopy, dest,
                             new VirtualCopyJob(files, dest, baseURL, (PreserveMode)pmode, KIO::CopyJob::Copy, showProgressInfo, false));
}

KIOJobWrapper * KIOJobWrapper::virtualMove(const KFileItemList &files, KUrl& dest,
        const KUrl& baseURL, int pmode, bool showProgressInfo)
{
    return new KIOJobWrapper(VirtualMove, dest,
                             new VirtualCopyJob(files, dest, baseURL, (PreserveMode)pmode, KIO::CopyJob::Move, showProgressInfo, false));
}

KIOJobWrapper * KIOJobWrapper::pack(const KUrl &srcUrl, const KUrl &destUrl, const QStringList & fileNames,
                                    const QString &type, const QMap<QString, QString> &packProps, bool showProgressInfo)
{
    return new KIOJobWrapper(Pack, srcUrl, destUrl, fileNames, showProgressInfo, type, packProps);
}

KIOJobWrapper * KIOJobWrapper::unpack(const KUrl &srcUrl, const KUrl &destUrl, const QStringList & fileNames,
                                      bool showProgressInfo)
{
    return new KIOJobWrapper(Unpack, srcUrl, destUrl, fileNames, showProgressInfo, QString(), QMap<QString, QString> ());
}

void KIOJobWrapper::start()
{
    m_started = true;
    KrJobStarter *self = KrJobStarter::self();
    QApplication::postEvent(self, new JobStartEvent(this));
}

void KIOJobWrapper::connectTo(const char * signal, const QObject * receiver, const char * method)
{
    m_signals.append(signal);
    m_receivers.append((QObject *)receiver);
    m_methods.append(method);
}

QStringList KIOJobWrapper::sourceItems() const
{
    switch (m_type) {
    case Stat:
    case DirectorySize:
        return QStringList();
    case Copy:
    case Move:
    case Pack:
    case Unpack: {
        QStringList srcItems;
        foreach (const KUrl& url, m_urlList)
            srcItems.append(url.pathOrUrl());
        return srcItems;
    }
    case VirtualCopy:
    case VirtualMove:
        return m_vcopy_job->filesToCopy();
    case VirtualAdd:
        return m_vadd_job->filesToCopy();
    }
    return QStringList();
}

QString KIOJobWrapper::typeStr() const
{
    switch (m_type) {
    case Stat:
        return i18nc("Job type", "Status");
    case DirectorySize:
        return i18nc("Job type", "Directory Size");
    case Copy:
    case VirtualCopy:
    case VirtualAdd:
        return i18nc("Job type", "Copy");
    case Move:
    case VirtualMove:
        return i18nc("Job type", "Move");
    case Pack:
        return i18nc("Job type", "Pack");
    case Unpack:
        return i18nc("Job type", "Unpack");
    default:
        return i18nc("Job type", "Unknown");
    }
}

void KIOJobWrapper::suspend()
{
    m_suspended = true;
    if (m_job)
        m_job->suspend();
}

void KIOJobWrapper::resume()
{
    m_suspended = false;
    if (m_job)
        m_job->resume();
}

void KIOJobWrapper::abort()
{
    if (m_job)
        m_job->kill();
}

QString KIOJobWrapper::description() const
{
    QStringList srcItems = sourceItems();
    if (srcItems.empty())
        return i18n("%1 of %2", typeStr(), Qt::escape(m_url.pathOrUrl()));
    else if (srcItems.size() == 1)
        return i18n("%1 %2 to %3", typeStr(), Qt::escape(srcItems[0]), Qt::escape(m_url.pathOrUrl()));
    else
        return i18np("%2 %1 item to %3", "%2  %1 items to %3", srcItems.size(), typeStr(), Qt::escape(m_url.pathOrUrl()));
}

QString KIOJobWrapper::toolTip() const
{
    QString tip = "<qt><div align=\"center\">";
    tip += "<h3>" + Qt::escape(typeStr()) + "</h3>";
    tip += "<table frame=\"box\" border=\"1\"><tbody>";
    tip += "<tr><td>" + Qt::escape(i18n("Target")) + "</td><td>" + Qt::escape(m_url.pathOrUrl()) + "</td></tr>";
    tip += "<tr><td>" + Qt::escape(i18n("Source")) + "</td><td>";
    foreach(const QString &srcItem, sourceItems()) {
        tip += "<li>" + Qt::escape(srcItem) + "</li>";
    }
    tip += "</td></tr>";
    tip += "</tbody></table>";
    tip += "</div></qt>";
    return tip;
}
