/*****************************************************************************
 * Copyright (C) 2003 Shie Erlich <erlich@users.sourceforge.net>             *
 * Copyright (C) 2003 Rafi Yanai <yanai@users.sourceforge.net>               *
 * Copyright (C) 2004-2018 Krusader Krew [https://krusader.org]              *
 *                                                                           *
 * This file is part of Krusader [https://krusader.org].                     *
 *                                                                           *
 * Krusader is free software: you can redistribute it and/or modify          *
 * it under the terms of the GNU General Public License as published by      *
 * the Free Software Foundation, either version 2 of the License, or         *
 * (at your option) any later version.                                       *
 *                                                                           *
 * Krusader is distributed in the hope that it will be useful,               *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                              *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with Krusader.  If not, see [http://www.gnu.org/licenses/].         *
 *****************************************************************************/

#include "filesystemprovider.h"

#ifdef HAVE_POSIX_ACL
#include <sys/acl.h>
#ifdef HAVE_NON_POSIX_ACL_EXTENSIONS
#include <acl/libacl.h>
#endif
#endif

// QtCore
#include <QDebug>
#include <QDir>

#include <KIOCore/KMountPoint>

#include "defaultfilesystem.h"
#include "fileitem.h"
#include "virtualfilesystem.h"
#include "../krservices.h"
#include "../JobMan/jobman.h"


FileSystemProvider::FileSystemProvider() : _defaultFileSystem(nullptr), _virtFileSystem(nullptr) {}

FileSystem *FileSystemProvider::getFilesystem(const QUrl &url, FileSystem *oldFilesystem)
{
    const FileSystem::FS_TYPE type = getFilesystemType(url);
    return oldFilesystem && oldFilesystem->type() == type ? oldFilesystem : createFilesystem(type);
}

void FileSystemProvider::startCopyFiles(const QList<QUrl> &urls, const QUrl &destination,
                                  KIO::CopyJob::CopyMode mode, bool showProgressInfo,
                                  JobMan::StartMode startMode)
{
    FileSystem *fs = getFilesystemInstance(destination);
    fs->copyFiles(urls, destination, mode, showProgressInfo, startMode);
}

void FileSystemProvider::startDropFiles(QDropEvent *event, const QUrl &destination)
{
    FileSystem *fs = getFilesystemInstance(destination);
    fs->dropFiles(destination, event);
}

void FileSystemProvider::startDeleteFiles(const QList<QUrl> &urls, bool moveToTrash)
{
    if (urls.isEmpty())
        return;

    // assume all URLs use the same filesystem
    FileSystem *fs = getFilesystemInstance(urls.first());
    fs->deleteFiles(urls, moveToTrash);
}

void FileSystemProvider::refreshFilesystems(const QUrl &directory, bool removed)
{
    qDebug() << "changed=" << directory.toDisplayString();

    QMutableListIterator<QPointer<FileSystem>> it(_fileSystems);
    while (it.hasNext()) {
        if (it.next().isNull()) {
            it.remove();
        }
    }

    QString mountPoint = "";
    if (directory.isLocalFile()) {
        KMountPoint::Ptr kMountPoint = KMountPoint::currentMountPoints().findByPath(directory.path());
        if (kMountPoint)
            mountPoint = kMountPoint->mountPoint();
    }

    for(const QPointer<FileSystem>& fileSystemPointer: _fileSystems) {
        FileSystem *fs = fileSystemPointer.data();
        // refresh all filesystems currently showing this directory
        // and always refresh filesystems showing a virtual directory; it can contain files from
        // various places, we don't know if they were (re)moved. Refreshing is also fast enough.
        const QUrl fileSystemDir = fs->currentDirectory();
        if ((!fs->hasAutoUpdate() && (fileSystemDir == FileSystem::cleanUrl(directory) ||
                                      (fs->type() == FileSystem::FS_VIRTUAL && !fs->isRoot())))
            // also refresh if a parent directory was (re)moved (not detected by file watcher)
            || (removed && directory.isParentOf(fileSystemDir))) {
            fs->refresh();
            // ..or refresh filesystem info if mount point is the same (for free space update)
        } else if (!mountPoint.isEmpty() && mountPoint == fs->mountPoint()) {
            fs->updateFilesystemInfo();
        }
    }
}

FileSystem *FileSystemProvider::getFilesystemInstance(const QUrl &directory)
{
    const FileSystem::FS_TYPE type = getFilesystemType(directory);
    switch (type) {
    case FileSystem::FS_VIRTUAL:
        if (!_virtFileSystem)
            _virtFileSystem = createFilesystem(type);
        return _virtFileSystem;
        break;
    default:
        if (!_defaultFileSystem)
            _defaultFileSystem = createFilesystem(type);
        return _defaultFileSystem;
    }
}

FileSystem *FileSystemProvider::createFilesystem(FileSystem::FS_TYPE type)
{
    FileSystem *newFilesystem;
    switch (type) {
    case (FileSystem::FS_VIRTUAL): newFilesystem = new VirtualFileSystem(); break;
    default: newFilesystem = new DefaultFileSystem();
    }

    QPointer<FileSystem> fileSystemPointer(newFilesystem);
    _fileSystems.append(fileSystemPointer);
    connect(newFilesystem, &FileSystem::fileSystemChanged, this, &FileSystemProvider::refreshFilesystems);
    return newFilesystem;
}

// ==== static ====

FileSystemProvider &FileSystemProvider::instance()
{
    static FileSystemProvider instance;
    return instance;
}

FileSystem::FS_TYPE FileSystemProvider::getFilesystemType(const QUrl &url)
{
    return url.scheme() == QStringLiteral("virt") ? FileSystem::FS_VIRTUAL : FileSystem::FS_DEFAULT;
}

void FileSystemProvider::getACL(FileItem *file, QString &acl, QString &defAcl)
{
    Q_UNUSED(file);
    acl.clear();
    defAcl.clear();
#ifdef HAVE_POSIX_ACL
    QString fileName = FileSystem::cleanUrl(file->getUrl()).path();
#ifdef HAVE_NON_POSIX_ACL_EXTENSIONS
    if (acl_extended_file(fileName)) {
#endif
        acl = getACL(fileName, ACL_TYPE_ACCESS);
        if (file->isDir())
            defAcl = getACL(fileName, ACL_TYPE_DEFAULT);
#ifdef HAVE_NON_POSIX_ACL_EXTENSIONS
    }
#endif
#endif
}

QString FileSystemProvider::getACL(const QString & path, int type)
{
    Q_UNUSED(path);
    Q_UNUSED(type);
#ifdef HAVE_POSIX_ACL
    acl_t acl = nullptr;
    // do we have an acl for the file, and/or a default acl for the dir, if it is one?
    if ((acl = acl_get_file(path.toLocal8Bit(), type)) != nullptr) {
        bool aclExtended = false;

#ifdef HAVE_NON_POSIX_ACL_EXTENSIONS
        aclExtended = acl_equiv_mode(acl, 0);
#else
        acl_entry_t entry;
        int ret = acl_get_entry(acl, ACL_FIRST_ENTRY, &entry);
        while (ret == 1) {
            acl_tag_t currentTag;
            acl_get_tag_type(entry, &currentTag);
            if (currentTag != ACL_USER_OBJ &&
                    currentTag != ACL_GROUP_OBJ &&
                    currentTag != ACL_OTHER) {
                aclExtended = true;
                break;
            }
            ret = acl_get_entry(acl, ACL_NEXT_ENTRY, &entry);
        }
#endif

        if (!aclExtended) {
            acl_free(acl);
            acl = nullptr;
        }
    }

    if (acl == nullptr)
        return QString();

    char *aclString = acl_to_text(acl, nullptr);
    QString ret = QString::fromLatin1(aclString);
    acl_free((void*)aclString);
    acl_free(acl);

    return ret;
#else
    return QString();
#endif
}
