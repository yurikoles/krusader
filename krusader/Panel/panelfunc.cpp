/*****************************************************************************
 * Copyright (C) 2000 Shie Erlich <krusader@users.sourceforge.net>           *
 * Copyright (C) 2000 Rafi Yanai <krusader@users.sourceforge.net>            *
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

#include "panelfunc.h"

// QtCore
#include <QEventLoop>
#include <QList>
#include <QMimeData>
#include <QDir>
#include <QTemporaryFile>
#include <QUrl>
// QtGui
#include <QClipboard>
#include <QDrag>
// QtWidgets
#include <QApplication>
#include <QInputDialog>

#include <KArchive/KTar>
#include <KConfigCore/KDesktopFile>
#include <KCoreAddons/KJobTrackerInterface>
#include <KCoreAddons/KProcess>
#include <KCoreAddons/KShell>
#include <KCoreAddons/KUrlMimeData>
#include <KI18n/KLocalizedString>
#include <KIO/DesktopExecParser>
#include <KIO/JobUiDelegate>
#include <KIOCore/KProtocolInfo>
#include <KIOWidgets/KOpenWithDialog>
#include <KIOWidgets/KPropertiesDialog>
#include <KIOWidgets/KRun>
#include <KService/KMimeTypeTrader>
#include <KWidgetsAddons/KCursor>
#include <KWidgetsAddons/KMessageBox>
#include <KWidgetsAddons/KToggleAction>

#include "dirhistoryqueue.h"
#include "krcalcspacedialog.h"
#include "krerrordisplay.h"
#include "krsearchbar.h"
#include "listpanel.h"
#include "listpanelactions.h"
#include "PanelView/krview.h"
#include "PanelView/krviewitem.h"
#include "../krglobal.h"
#include "../krslots.h"
#include "../kractions.h"
#include "../defaults.h"
#include "../abstractpanelmanager.h"
#include "../krservices.h"
#include "../Archive/krarchandler.h"
#include "../Archive/packjob.h"
#include "../FileSystem/fileitem.h"
#include "../FileSystem/virtualfilesystem.h"
#include "../FileSystem/krpermhandler.h"
#include "../FileSystem/filesystemprovider.h"
#include "../FileSystem/sizecalculator.h"
#include "../Dialogs/packgui.h"
#include "../Dialogs/krdialogs.h"
#include "../Dialogs/krpleasewait.h"
#include "../Dialogs/krspwidgets.h"
#include "../Dialogs/checksumdlg.h"
#include "../KViewer/krviewer.h"
#include "../MountMan/kmountman.h"


QPointer<ListPanelFunc> ListPanelFunc::copyToClipboardOrigin;

ListPanelFunc::ListPanelFunc(ListPanel *parent) : QObject(parent),
        panel(parent), fileSystemP(nullptr), urlManuallyEntered(false),
        _isPaused(true), _refreshAfterPaused(true), _quickSizeCalculator(nullptr)
{
    history = new DirHistoryQueue(panel);
    delayTimer.setSingleShot(true);
    connect(&delayTimer, &QTimer::timeout, this, &ListPanelFunc::doRefresh);
}

ListPanelFunc::~ListPanelFunc()
{
    if (fileSystemP) {
        fileSystemP->deleteLater();
    }
    delete history;
    if (_quickSizeCalculator)
        _quickSizeCalculator->deleteLater();
}

bool ListPanelFunc::isSyncing(const QUrl &url)
{
    if(otherFunc()->otherFunc() == this &&
       panel->otherPanel()->gui->syncBrowseButton->isChecked() &&
       !otherFunc()->syncURL.isEmpty() &&
       otherFunc()->syncURL == url)
        return true;

    return false;
}

void ListPanelFunc::openFileNameInternal(const QString &name, bool externallyExecutable)
{
    if (name == "..") {
        dirUp();
        return;
    }

    FileItem *fileitem = files()->getFileItem(name);
    if (fileitem == nullptr)
        return;

    QUrl url = files()->getUrl(name);

    if (fileitem->isDir()) {
        panel->view->setNameToMakeCurrent(QString());
        openUrl(url);
        return;
    }

    QString mime = fileitem->getMime();

    QUrl arcPath = browsableArchivePath(name);
    if (!arcPath.isEmpty()) {
        bool browseAsDirectory = !externallyExecutable
                || (KConfigGroup(krConfig, "Archives").readEntry("ArchivesAsDirectories", _ArchivesAsDirectories)
                && (KRarcHandler::arcSupported(mime) || KrServices::isoSupported(mime)));
        if (browseAsDirectory) {
            openUrl(arcPath);
            return;
        }
    }

    if (externallyExecutable) {
        if (KRun::isExecutableFile(url, mime)) {
            runCommand(KShell::quoteArg(url.path()));
            return;
        }

        KService::Ptr service = KMimeTypeTrader::self()->preferredService(mime);
        if(service) {
            runService(*service, QList<QUrl>() << url);
            return;
        }

        displayOpenWithDialog(QList<QUrl>() << url);
    }
}

QUrl ListPanelFunc::cleanPath(const QUrl &urlIn)
{
    QUrl url = urlIn;
    url.setPath(QDir::cleanPath(url.path()));
    if (!url.isValid() || url.isRelative()) {
        if (url.url() == "~")
            url = QUrl::fromLocalFile(QDir::homePath());
        else if (!url.url().startsWith('/')) {
            // possible relative URL - translate to full URL
            url = files()->currentDirectory();
            url.setPath(url.path() + '/' + urlIn.path());
        }
    }
    url.setPath(QDir::cleanPath(url.path()));
    return url;
}

void ListPanelFunc::openUrl(const QUrl &url, const QString& nameToMakeCurrent,
                            bool manuallyEntered)
{
    qDebug() << "URL=" << url.toDisplayString() << "; name to current=" << nameToMakeCurrent;
    if (panel->syncBrowseButton->isChecked()) {
        //do sync-browse stuff....
        if(syncURL.isEmpty())
            syncURL = panel->otherPanel()->virtualPath();

        QString relative = QDir(panel->virtualPath().path() + '/').relativeFilePath(url.path());
        syncURL.setPath(QDir::cleanPath(syncURL.path() + '/' + relative));
        panel->otherPanel()->gui->setTabState(ListPanel::TabState::DEFAULT);
        otherFunc()->openUrlInternal(syncURL, nameToMakeCurrent, false, false);
    }
    openUrlInternal(url, nameToMakeCurrent, false, manuallyEntered);
}

void ListPanelFunc::immediateOpenUrl(const QUrl &url)
{
    openUrlInternal(url, QString(), true, false);
}

void ListPanelFunc::openUrlInternal(const QUrl &url, const QString& nameToMakeCurrent,
                                    bool immediately, bool manuallyEntered)
{
    const QUrl cleanUrl = cleanPath(url);

    if (panel->isLocked() &&
            !files()->currentDirectory().matches(cleanUrl, QUrl::StripTrailingSlash)) {
        panel->_manager->newTab(url);
        urlManuallyEntered = false;
        return;
    }

    urlManuallyEntered = manuallyEntered;

    const QString currentItem = history->currentUrl().path() == cleanUrl.path()
                                ? history->currentItem()
                                : nameToMakeCurrent;

    history->add(cleanUrl, currentItem);

    if(immediately)
        doRefresh();
    else
        refresh();
}

void ListPanelFunc::refresh()
{
    panel->cancelProgress();
    delayTimer.start(0); // to avoid qApp->processEvents() deadlock situation
}

void ListPanelFunc::doRefresh()
{
    delayTimer.stop();

    if (_isPaused) {
        _refreshAfterPaused = true;
        // simulate refresh
        panel->slotStartUpdate(true);
        return;
    } else {
        _refreshAfterPaused = false;
    }

    const QUrl url = history->currentUrl();

    if(!url.isValid()) {
        panel->slotStartUpdate(true);  // refresh the panel
        urlManuallyEntered = false;
        return;
    }

    panel->cancelProgress();

    // if we are not refreshing to current URL
    const bool isEqualUrl = files()->currentDirectory().matches(url, QUrl::StripTrailingSlash);

    if (!isEqualUrl) {
        panel->setCursor(Qt::WaitCursor);
        panel->view->clearSavedSelection();
    }

    if (panel->fileSystemError) {
        panel->fileSystemError->hide();
    }

    panel->setNavigatorUrl(url);

    // may get a new filesystem for this url
    FileSystem *fileSystem = FileSystemProvider::instance().getFilesystem(url, files());
    fileSystem->setParentWindow(krMainWindow);
    connect(fileSystem, &FileSystem::aboutToOpenDir, &krMtMan, &KMountMan::autoMount, Qt::DirectConnection);
    if (fileSystem != fileSystemP) {
        panel->view->setFiles(nullptr);

        // disconnect older signals
        disconnect(fileSystemP, nullptr, panel, nullptr);

        fileSystemP->deleteLater();
        fileSystemP = fileSystem; // v != 0 so this is safe
    } else {
        if (fileSystemP->isRefreshing()) {
            // TODO remove busy waiting here
            delayTimer.start(100); /* if filesystem is busy try refreshing later */
            return;
        }
    }
    // (re)connect filesystem signals
    disconnect(files(), nullptr, panel, nullptr);
    connect(files(), &DirListerInterface::scanDone, panel, &ListPanel::slotStartUpdate);
    connect(files(), &FileSystem::fileSystemInfoChanged, panel, &ListPanel::updateFilesystemStats);
    connect(files(), &FileSystem::refreshJobStarted, panel, &ListPanel::slotRefreshJobStarted);
    connect(files(), &FileSystem::error, panel, &ListPanel::slotFilesystemError);

    panel->view->setFiles(files());

    if (!isEqualUrl || !panel->view->getCurrentKrViewItem()) {
        // set current item after refresh from history, if there is none yet
        panel->view->setNameToMakeCurrent(history->currentItem());
    }

    // workaround for detecting panel deletion while filesystem is refreshing
    QPointer<ListPanel> panelSave = panel;
    // NOTE: this is blocking. Returns false on error or interruption (cancel requested or panel
    // was deleted)
    const bool scanned = fileSystemP->refresh(url);
    if (scanned) {
        // update the history and address bar, as the actual url might differ from the one requested
        history->setCurrentUrl(fileSystemP->currentDirectory());
        panel->setNavigatorUrl(fileSystemP->currentDirectory());
    } else if (!panelSave) {
        return;
    }

    panel->view->setNameToMakeCurrent(QString());

    panel->setCursor(Qt::ArrowCursor);

    // on local file system change the working directory
    if (files()->isLocal())
        QDir::setCurrent(KrServices::urlToLocalPath(files()->currentDirectory()));

    // see if the open url operation failed, and if so,
    // put the attempted url in the navigator bar and let the user change it
    if (!scanned) {
        if(isSyncing(url))
            panel->otherPanel()->gui->syncBrowseButton->setChecked(false);
        else if(urlManuallyEntered) {
            panel->setNavigatorUrl(url);
            if(panel == ACTIVE_PANEL)
                panel->editLocation();
        }
    }

    if(otherFunc()->otherFunc() == this)  // not true if our tab is not active
        otherFunc()->syncURL = QUrl();

    urlManuallyEntered = false;

    refreshActions();
}

void ListPanelFunc::setPaused(bool paused) {
    if (paused == _isPaused)
        return;
    _isPaused = paused;

    // TODO: disable refresh() in local file system when paused

    if (!_isPaused && _refreshAfterPaused)
        refresh();
}

void ListPanelFunc::redirectLink()
{
    if (!files()->isLocal()) {
        KMessageBox::sorry(krMainWindow, i18n("You can edit links only on local file systems"));
        return;
    }

    FileItem *fileitem = files()->getFileItem(panel->getCurrentName());
    if (!fileitem)
        return;

    QString file = fileitem->getUrl().path();
    QString currentLink = fileitem->getSymDest();
    if (currentLink.isEmpty()) {
        KMessageBox::sorry(krMainWindow, i18n("The current file is not a link, so it cannot be redirected."));
        return;
    }

    // ask the user for a new destination
    bool ok = false;
    QString newLink =
        QInputDialog::getText(krMainWindow, i18n("Link Redirection"), i18n("Please enter the new link destination:"), QLineEdit::Normal, currentLink, &ok);

    // if the user canceled - quit
    if (!ok || newLink == currentLink)
        return;
    // delete the current link
    if (unlink(file.toLocal8Bit()) == -1) {
        KMessageBox::sorry(krMainWindow, i18n("Cannot remove old link: %1", file));
        return;
    }
    // try to create a new symlink
    if (symlink(newLink.toLocal8Bit(), file.toLocal8Bit()) == -1) {
        KMessageBox:: /* --=={ Patch by Heiner <h.eichmann@gmx.de> }==-- */sorry(krMainWindow, i18n("Failed to create a new link: %1", file));
        return;
    }
}

void ListPanelFunc::krlink(bool sym)
{
    if (!files()->isLocal()) {
        KMessageBox::sorry(krMainWindow, i18n("You can create links only on local file systems"));
        return;
    }

    QString name = panel->getCurrentName();

    // ask the new link name..
    bool ok = false;
    QString linkName =
        QInputDialog::getText(krMainWindow, i18n("New Link"),
                              i18n("Create a new link to: %1", name), QLineEdit::Normal, name, &ok);

    // if the user canceled - quit
    if (!ok || linkName == name)
        return;

    // if the name is already taken - quit
    if (files()->getFileItem(linkName) != nullptr) {
        KMessageBox::sorry(krMainWindow, i18n("A folder or a file with this name already exists."));
        return;
    }

    // make link name and target absolute path
    if (linkName.left(1) != "/")
        linkName = files()->currentDirectory().path() + '/' + linkName;
    name = files()->getUrl(name).path();

    if (sym) {
        if (symlink(name.toLocal8Bit(), linkName.toLocal8Bit()) == -1)
            KMessageBox::sorry(krMainWindow,
                               i18n("Failed to create a new symlink '%1' to: '%2'", linkName, name));
    } else {
        if (link(name.toLocal8Bit(), linkName.toLocal8Bit()) == -1)
            KMessageBox::sorry(krMainWindow,
                               i18n("Failed to create a new link '%1' to '%2'", linkName, name));
    }
}

void ListPanelFunc::view()
{
    panel->searchBar->hideBarIfSearching();

    QString fileName = panel->getCurrentName();
    if (fileName.isNull())
        return;

    // if we're trying to view a directory, just exit
    FileItem *fileitem = files()->getFileItem(fileName);
    if (!fileitem || fileitem->isDir())
        return;
    if (!fileitem->isReadable()) {
        KMessageBox::sorry(nullptr, i18n("No permissions to view this file."));
        return;
    }
    // call KViewer.
    KrViewer::view(files()->getUrl(fileName));
    // nothing more to it!
}

void ListPanelFunc::viewDlg()
{
    // ask the user for a url to view
    QUrl dest = KChooseDir::getFile(i18n("Enter a URL to view:"), QUrl(panel->getCurrentName()), panel->virtualPath());
    if (dest.isEmpty())
        return ;   // the user canceled

    KrViewer::view(dest);   // view the file
}

void ListPanelFunc::terminal()
{
    SLOTS->runTerminal(panel->lastLocalPath());
}

void ListPanelFunc::editFile(const QUrl &newFilePath)
{
    panel->searchBar->hideBarIfSearching();

    QUrl editPath;
    if (!newFilePath.isEmpty()) {
        editPath = newFilePath;
    } else {
        const QString name = panel->getCurrentName();
        if (name.isNull())
            return;
        editPath = files()->getUrl(name);
    }

    const KFileItem fileToEdit = KFileItem(newFilePath);

    if (fileToEdit.isDir()) {
        KMessageBox::sorry(krMainWindow, i18n("You cannot edit a folder"));
        return;
    }

    if (!fileToEdit.isReadable()) {
        KMessageBox::sorry(nullptr, i18n("No permissions to edit this file."));
        return;
    }

    KrViewer::edit(editPath);
}

void ListPanelFunc::editNewFile()
{
    // ask the user for the filename to edit
    const QUrl filePath = KChooseDir::getFile(i18n("Enter the filename to edit:"),
                                       QUrl(panel->getCurrentName()), panel->virtualPath());
    if(filePath.isEmpty())
        return ;   // the user canceled

    if (filePath.isLocalFile()) {
        // if the file exists, edit it instead of creating a new one
        QFile file(filePath.toLocalFile());
        if (file.exists()) {
            editFile();
            return;
        } else {
            // simply create a local file
            // also because KIO::CopyJob::setDefaultPermissions does not work
            file.open(QIODevice::NewOnly);
            file.close();
            slotFileCreated(nullptr, filePath);
            return;
        }
    }

    auto *tempFile = new QTemporaryFile;
    tempFile->setAutoRemove(false); // done below
    tempFile->open(); // create file

    KIO::CopyJob *job = KIO::copy(QUrl::fromLocalFile(tempFile->fileName()), filePath);
    job->setUiDelegate(nullptr);
    job->setDefaultPermissions(true);
    connect(job, &KIO::CopyJob::result, this, [=](KJob *job) { slotFileCreated(job, filePath); });
    connect(job, &KIO::CopyJob::result, tempFile, &QTemporaryFile::deleteLater);
}

void ListPanelFunc::slotFileCreated(KJob *job, const QUrl filePath)
{
    if (!job || (!job->error() || job->error() == KIO::ERR_FILE_ALREADY_EXIST)) {
        KrViewer::edit(filePath);

        if (KIO::upUrl(filePath).matches(panel->virtualPath(), QUrl::StripTrailingSlash)) {
            refresh();
        }
        if (KIO::upUrl(filePath).matches(panel->otherPanel()->virtualPath(), QUrl::StripTrailingSlash)) {
            otherFunc()->refresh();
        }
    } else {
        KMessageBox::sorry(krMainWindow, job->errorString());
    }
}

void ListPanelFunc::copyFiles(bool enqueue, bool move)
{
    panel->searchBar->hideBarIfSearching();

    const QStringList fileNames = panel->getSelectedNames();
    if (fileNames.isEmpty())
        return ;  // safety

    QUrl destination = panel->otherPanel()->virtualPath();

    bool fullDestPath = false;
    if (fileNames.count() == 1 && otherFunc()->files()->type() != FileSystem::FS_VIRTUAL) {
        FileItem *item = files()->getFileItem(fileNames[0]);
        if (item && !item->isDir()) {
            fullDestPath = true;
            // add original filename to destination
            destination.setPath(QDir(destination.path()).filePath(item->getUrl().fileName()));
        }
    }
    if (!fullDestPath) {
        destination = FileSystem::ensureTrailingSlash(destination);
    }

    const KConfigGroup group(krConfig, "Advanced");
    const bool showDialog = move ? group.readEntry("Confirm Move", _ConfirmMove) :
                                   group.readEntry("Confirm Copy", _ConfirmCopy);

    if (showDialog) {
        QString operationText;
        if (move) {
            operationText = fileNames.count() == 1
                                ? i18n("Move %1 to:", fileNames.first())
                                : i18np("Move %1 file to:", "Move %1 files to:", fileNames.count());
        } else {
            operationText = fileNames.count() == 1
                                ? i18n("Copy %1 to:", fileNames.first())
                                : i18np("Copy %1 file to:", "Copy %1 files to:", fileNames.count());
        }

        // ask the user for the copy/move dest
        const KChooseDir::ChooseResult result =
            KChooseDir::getCopyDir(operationText, destination, panel->virtualPath());
        destination = result.url;
        if (destination.isEmpty())
            return ; // the user canceled

        enqueue = result.enqueue;
    }

    const JobMan::StartMode startMode =
        enqueue && krJobMan->isQueueModeEnabled() ? JobMan::Delay :
        !enqueue && !krJobMan->isQueueModeEnabled() ? JobMan::Start : JobMan::Enqueue;

    const QList<QUrl> fileUrls = files()->getUrls(fileNames);

    if (move) {
        // after the delete return the cursor to the first unmarked file above the current item
        panel->prepareToDelete();
    }

    // make sure the user does not overwrite multiple files by mistake
    if (fileNames.count() > 1) {
        destination = FileSystem::ensureTrailingSlash(destination);
    }

    const KIO::CopyJob::CopyMode mode = move ? KIO::CopyJob::Move : KIO::CopyJob::Copy;
    FileSystemProvider::instance().startCopyFiles(fileUrls, destination, mode, true, startMode);

    if(KConfigGroup(krConfig, "Look&Feel").readEntry("UnselectBeforeOperation", _UnselectBeforeOperation)) {
        panel->view->saveSelection();
        panel->view->unselectAll();
    }
}

// called from SLOTS to begin the renaming process
void ListPanelFunc::rename()
{
    panel->searchBar->hideBarIfSearching();
    panel->view->renameCurrentItem();
}

// called by signal itemRenamed() from the view to complete the renaming process
void ListPanelFunc::rename(const QString &oldname, const QString &newname)
{
    if (oldname == newname)
        return ; // do nothing

    // set current after rename
    panel->view->setNameToMakeCurrent(newname);

    // as always - the filesystem do the job
    files()->rename(oldname, newname);
}

void ListPanelFunc::mkdir()
{
    // ask the new dir name..
    // suggested name is the complete name for the directories
    // while filenames are suggested without their extension
    QString suggestedName = panel->getCurrentName();
    if (!suggestedName.isEmpty() && !files()->getFileItem(suggestedName)->isDir())
        suggestedName = QFileInfo(suggestedName).completeBaseName();

    const QString dirName = QInputDialog::getText(krMainWindow, i18n("New folder"), i18n("Folder's name:"), QLineEdit::Normal, suggestedName);

    const QString firstName = dirName.section('/', 0, 0, QString::SectionIncludeLeadingSep);

    // if the user canceled or the name was composed of slashes - quit
    if (!dirName.startsWith('/') && firstName.isEmpty()) {
        return;
    }

    // notify user about existing folder if only single directory was given
    if (!dirName.contains('/') && files()->getFileItem(firstName)) {
        // focus the existing directory
        panel->view->setCurrentItem(firstName);
        // show error message
        KMessageBox::sorry(krMainWindow, i18n("A folder or a file with this name already exists."));
        return;
    }

    // focus new directory when next refresh happens
    panel->view->setNameToMakeCurrent(firstName);

    // create new directory (along with underlying directories if necessary)
    files()->mkDir(dirName);
}

void ListPanelFunc::defaultOrAlternativeDeleteFiles(bool invert)
{
    const bool trash = KConfigGroup(krConfig, "General").readEntry("Move To Trash", _MoveToTrash);
    deleteFiles(trash != invert);
}

void ListPanelFunc::deleteFiles(bool moveToTrash)
{
    panel->searchBar->hideBarIfSearching();

    const bool isVFS = files()->type() == FileSystem::FS_VIRTUAL;
    if (isVFS && files()->isRoot()) {
        // only virtual deletion possible
        removeVirtualFiles();
        return;
    }

    // first get the selected file names list
    const QStringList fileNames = panel->getSelectedNames();
    if (fileNames.isEmpty())
        return;

    // move to trash: only if possible
    moveToTrash = moveToTrash && files()->canMoveToTrash(fileNames);

    // now ask the user if he/she is sure:

    const QList<QUrl> confirmedUrls = confirmDeletion(
        files()->getUrls(fileNames), moveToTrash, isVFS, false);

    if (confirmedUrls.isEmpty())
        return; // nothing to delete

    // after the delete return the cursor to the first unmarked
    // file above the current item;
    panel->prepareToDelete();

    // let the filesystem do the job...
    files()->deleteFiles(confirmedUrls, moveToTrash);
}

QList<QUrl> ListPanelFunc::confirmDeletion(const QList<QUrl> &urls, bool moveToTrash,
                                           bool virtualFS, bool showPath)
{
    QStringList files;
    for (const QUrl& fileUrl : urls) {
        files.append(showPath ? fileUrl.toDisplayString(QUrl::PreferLocalFile) : fileUrl.fileName());
    }

    const KConfigGroup advancedGroup(krConfig, "Advanced");
    if (advancedGroup.readEntry("Confirm Delete", _ConfirmDelete)) {
        QString s; // text
        KGuiItem b; // continue button

        if (moveToTrash) {
            s = i18np("Do you really want to move this item to the trash?",
                      "Do you really want to move these %1 items to the trash?", files.count());
            b = KGuiItem(i18n("&Trash"));
        } else if (virtualFS) {
            s = i18np("<qt>Do you really want to delete this item <b>physically</b> (not just "
                      "removing it from the virtual items)?</qt>",
                      "<qt>Do you really want to delete these %1 items <b>physically</b> (not just "
                      "removing them from the virtual items)?</qt>",
                      files.count());
            b = KStandardGuiItem::del();
        } else {
            s = i18np("Do you really want to delete this item?",
                      "Do you really want to delete these %1 items?", files.count());
            b = KStandardGuiItem::del();
        }

        // show message
        // note: i'm using continue and not yes/no because the yes/no has cancel as default button
        if (KMessageBox::warningContinueCancelList(krMainWindow, s, files, i18n("Warning"),
                                                   b) != KMessageBox::Continue) {
            return QList<QUrl>();
        }
    }

    // we want to warn the user about non empty dir
    const bool emptyDirVerify = advancedGroup.readEntry("Confirm Unempty Dir", _ConfirmUnemptyDir);

    QList<QUrl> toDelete;
    if (emptyDirVerify) {
        QSet<QUrl> confirmedFiles = urls.toSet();
        for (const QUrl& fileUrl : urls) {
            if (!fileUrl.isLocalFile()) {
                continue; // TODO only local fs supported
            }

            const QString filePath = fileUrl.toLocalFile();
            QFileInfo fileInfo(filePath);
            if (fileInfo.isDir() && !fileInfo.isSymLink()) {
                // read local dir...
                const QDir dir(filePath);
                if (!dir.entryList(QDir::AllEntries | QDir::System | QDir::Hidden |
                                   QDir::NoDotAndDotDot).isEmpty()) {

                    // ...is not empty, ask user
                    const QString fileString = showPath ? filePath : fileUrl.fileName();
                    const KMessageBox::ButtonCode result = KMessageBox::warningYesNoCancel(
                        krMainWindow,
                        i18n("<qt><p>Folder <b>%1</b> is not empty.</p>", fileString) +
                            (moveToTrash ? i18n("<p>Skip this one or trash all?</p></qt>") :
                                           i18n("<p>Skip this one or delete all?</p></qt>")),
                        QString(), KGuiItem(i18n("&Skip")),
                        KGuiItem(moveToTrash ? i18n("&Trash All") : i18n("&Delete All")));
                    if (result == KMessageBox::Yes) {
                        confirmedFiles.remove(fileUrl); // skip this dir
                    } else if (result == KMessageBox::No) {
                        break; // accept all remaining
                    } else {
                        return QList<QUrl>(); // cancel
                    }
                }
            }
        }
        toDelete = confirmedFiles.toList();
    } else {
        toDelete = urls;
    }

    return toDelete;
}

void ListPanelFunc::removeVirtualFiles()
{
    if (files()->type() != FileSystem::FS_VIRTUAL) {
        qWarning() << "filesystem not virtual";
        return;
    }

    const QStringList fileNames = panel->getSelectedNames();
    if (fileNames.isEmpty())
        return;

    const QString text =
        i18np("Do you really want to delete this virtual item (physical files stay untouched)?",
              "Do you really want to delete these %1 virtual items (physical files stay "
              "untouched)?",
              fileNames.count());
    if (KMessageBox::warningContinueCancelList(krMainWindow, text, fileNames, i18n("Warning"),
                                               KStandardGuiItem::remove()) != KMessageBox::Continue)
        return;

    auto *fileSystem = dynamic_cast<VirtualFileSystem*>(files());
    fileSystem->remove(fileNames);
}

void ListPanelFunc::goInside(const QString& name)
{
    openFileNameInternal(name, false);
}

void ListPanelFunc::runCommand(const QString& cmd)
{
    qDebug() << "command=" << cmd;
    const QString workdir = panel->virtualPath().isLocalFile() ?
            panel->virtualPath().path() : QDir::homePath();
    if(!KRun::runCommand(cmd, krMainWindow, workdir))
        KMessageBox::error(nullptr, i18n("Could not start %1", cmd));
}

void ListPanelFunc::runService(const KService &service, const QList<QUrl>& urls)
{
    qDebug() << "service name=" << service.name();
    KIO::DesktopExecParser parser(service, urls);
    QStringList args = parser.resultingArguments();
    if (!args.isEmpty())
        runCommand(KShell::joinArgs(args));
    else
        KMessageBox::error(nullptr, i18n("%1 cannot open %2", service.name(), KrServices::toStringList(urls).join(", ")));
}

void ListPanelFunc::displayOpenWithDialog(const QList<QUrl>& urls)
{
    // NOTE: we are not using KRun::displayOpenWithDialog() because we want the commands working
    // directory to be the panel directory
    KOpenWithDialog dialog(urls, panel);
    dialog.hideRunInTerminal();
    if (dialog.exec()) {
        KService::Ptr service = dialog.service();
        if(!service)
            service = KService::Ptr(new KService(dialog.text(), dialog.text(), QString()));
        runService(*service, urls);
    }
}

QUrl ListPanelFunc::browsableArchivePath(const QString &filename)
{
    FileItem *fileitem = files()->getFileItem(filename);
    QUrl url = files()->getUrl(filename);
    QString mime = fileitem->getMime();

    if(url.isLocalFile()) {
        QString protocol = KrServices::registeredProtocol(mime);
        if(!protocol.isEmpty()) {
            url.setScheme(protocol);
            return url;
        }
    }
    return QUrl();
}

// this is done when you double click on a file
void ListPanelFunc::execute(const QString& name)
{
    openFileNameInternal(name, true);
}

void ListPanelFunc::pack()
{
    const QStringList fileNames = panel->getSelectedNames();
    if (fileNames.isEmpty())
        return ;  // safety

    if (fileNames.count() == 0)
        return ; // nothing to pack

    // choose the default name
    QString defaultName = panel->virtualPath().fileName();
    if (defaultName.isEmpty())
        defaultName = "pack";
    if (fileNames.count() == 1)
        defaultName = fileNames.first();
    // ask the user for archive name and packer
    new PackGUI(defaultName, panel->otherPanel()->virtualPath().toDisplayString(QUrl::PreferLocalFile | QUrl::StripTrailingSlash),
                fileNames.count(), fileNames.first());
    if (PackGUI::type.isEmpty()) {
        return ; // the user canceled
    }

    // check for partial URLs
    if (!PackGUI::destination.contains(":/") && !PackGUI::destination.startsWith('/')) {
        PackGUI::destination = panel->virtualPath().toDisplayString() + '/' + PackGUI::destination;
    }

    QString destDir = PackGUI::destination;
    if (!destDir.endsWith('/'))
        destDir += '/';

    bool packToOtherPanel = (destDir == FileSystem::ensureTrailingSlash(panel->otherPanel()->virtualPath()).toDisplayString(QUrl::PreferLocalFile));

    QUrl destURL = QUrl::fromUserInput(destDir + PackGUI::filename + '.' + PackGUI::type, QString(), QUrl::AssumeLocalFile);
    if (destURL.isLocalFile() && QFile::exists(destURL.path())) {
        QString msg = i18n("<qt><p>The archive <b>%1.%2</b> already exists. Do you want to overwrite it?</p><p>All data in the previous archive will be lost.</p></qt>", PackGUI::filename, PackGUI::type);
        if (PackGUI::type == "zip") {
            msg = i18n("<qt><p>The archive <b>%1.%2</b> already exists. Do you want to overwrite it?</p><p>Zip will replace identically named entries in the zip archive or add entries for new names.</p></qt>", PackGUI::filename, PackGUI::type);
        }
        if (KMessageBox::warningContinueCancel(krMainWindow, msg, QString(), KStandardGuiItem::overwrite())
                == KMessageBox::Cancel)
            return ; // stop operation
    } else if (destURL.scheme() == QStringLiteral("virt")) {
        KMessageBox::error(krMainWindow, i18n("Cannot pack files onto a virtual destination."));
        return;
    }

    PackJob * job = PackJob::createPacker(files()->currentDirectory(), destURL, fileNames, PackGUI::type, PackGUI::extraProps);
    job->setUiDelegate(new KIO::JobUiDelegate());
    KIO::getJobTracker()->registerJob(job);
    job->uiDelegate()->setAutoErrorHandlingEnabled(true);

    if (packToOtherPanel)
        connect(job, &PackJob::result, panel->otherPanel()->func, &ListPanelFunc::refresh);

}

void ListPanelFunc::testArchive()
{
    const QStringList fileNames = panel->getSelectedNames();
    if (fileNames.isEmpty())
        return ;  // safety

    TestArchiveJob * job = TestArchiveJob::testArchives(files()->currentDirectory(), fileNames);
    job->setUiDelegate(new KIO::JobUiDelegate());
    KIO::getJobTracker()->registerJob(job);
    job->uiDelegate()->setAutoErrorHandlingEnabled(true);
}

void ListPanelFunc::unpack()
{
    const QStringList fileNames = panel->getSelectedNames();
    if (fileNames.isEmpty())
        return ;  // safety

    QString s;
    if (fileNames.count() == 1)
        s = i18n("Unpack %1 to:", fileNames[0]);
    else
        s = i18np("Unpack %1 file to:", "Unpack %1 files to:", fileNames.count());

    // ask the user for the copy dest
    QUrl dest = KChooseDir::getDir(s, panel->otherPanel()->virtualPath(), panel->virtualPath());
    if (dest.isEmpty()) return ;   // the user canceled

    bool packToOtherPanel = (dest.matches(panel->otherPanel()->virtualPath(), QUrl::StripTrailingSlash));

    UnpackJob * job = UnpackJob::createUnpacker(files()->currentDirectory(), dest, fileNames);
    job->setUiDelegate(new KIO::JobUiDelegate());
    KIO::getJobTracker()->registerJob(job);
    job->uiDelegate()->setAutoErrorHandlingEnabled(true);

    if (packToOtherPanel)
        connect(job, &UnpackJob::result, panel->otherPanel()->func, &ListPanelFunc::refresh);

}

void ListPanelFunc::createChecksum()
{
    if (!panel->func->files()->isLocal())
        return; // only local, non-virtual files are supported

    const KrViewItemList items = panel->view->getSelectedKrViewItems();

    QStringList fileNames;
    for (KrViewItem *item : items) {
        FileItem *file = panel->func->getFileItem(item);
        fileNames.append(file->getUrl().fileName());
    }

    if (fileNames.isEmpty())
        return; // nothing selected and no valid current file

    Checksum::startCreationWizard(panel->virtualPath().toLocalFile(), fileNames);
}

void ListPanelFunc::matchChecksum()
{
    if (!panel->func->files()->isLocal())
        return; // only local, non-virtual files are supported

    FileItem *currentItem = files()->getFileItem(panel->getCurrentName());
    const QString checksumFilePath = currentItem ? currentItem->getUrl().toLocalFile() : QString();

    Checksum::startVerifyWizard(panel->virtualPath().toLocalFile(), checksumFilePath);
}

void ListPanelFunc::calcSpace()
{
    QStringList fileNames;
    panel->view->getSelectedItems(&fileNames);
    if (fileNames.isEmpty()) {
        // current file is ".." dummy file
        panel->view->selectAllIncludingDirs();
        panel->view->getSelectedItems(&fileNames);
        panel->view->unselectAll();
    }

    SizeCalculator *sizeCalculator = createAndConnectSizeCalculator(files()->getUrls(fileNames));
    KrCalcSpaceDialog::showDialog(panel, sizeCalculator);
}

void ListPanelFunc::quickCalcSpace()
{
    const QString currentName = panel->getCurrentName();
    if (currentName.isEmpty()) {
        // current file is ".." dummy, do a verbose calcSpace
        calcSpace();
        return;
    }

    if (!_quickSizeCalculator) {
        _quickSizeCalculator = createAndConnectSizeCalculator(QList<QUrl>());
        panel->connectQuickSizeCalculator(_quickSizeCalculator);
    }

    _quickSizeCalculator->add(files()->getUrl(currentName));
}


SizeCalculator *ListPanelFunc::createAndConnectSizeCalculator(const QList<QUrl> &urls)
{
    auto *sizeCalculator = new SizeCalculator(urls);
    connect(sizeCalculator, &SizeCalculator::calculated, this, &ListPanelFunc::slotSizeCalculated);
    connect(sizeCalculator, &SizeCalculator::finished, panel, &ListPanel::slotUpdateTotals);
    connect(this, &ListPanelFunc::destroyed, sizeCalculator, &SizeCalculator::deleteLater);
    return sizeCalculator;
}

void ListPanelFunc::slotSizeCalculated(const QUrl &url, KIO::filesize_t size)
{
    KrViewItem *item = panel->view->findItemByUrl(url);
    if (!item)
        return;

    item->setSize(size);
    item->redraw();
}

void ListPanelFunc::FTPDisconnect()
{
    // you can disconnect only if connected!
    if (files()->isRemote()) {
        panel->_actions->actFTPDisconnect->setEnabled(false);
        panel->view->setNameToMakeCurrent(QString());
        openUrl(QUrl::fromLocalFile(panel->lastLocalPath()));
    }
}

void ListPanelFunc::newFTPconnection()
{
    QUrl url = KRSpWidgets::newFTP();
    // if the user canceled - quit
    if (url.isEmpty())
        return;

    panel->_actions->actFTPDisconnect->setEnabled(true);

    qDebug() << "URL=" << url.toDisplayString();

    openUrl(url);
}

void ListPanelFunc::properties()
{
    const QStringList names = panel->getSelectedNames();
    if (names.isEmpty()) {
        return ;  // no names...
    }

    KFileItemList fileItems;

    for (const QString& name : names) {
        FileItem *fileitem = files()->getFileItem(name);
        if (!fileitem) {
            continue;
        }

        fileItems.push_back(KFileItem(fileitem->getEntry(), files()->getUrl(name)));
    }

    if (fileItems.isEmpty())
        return;

    // Show the properties dialog
    auto *dialog = new KPropertiesDialog(fileItems, krMainWindow);
    connect(dialog, &KPropertiesDialog::applied, this, &ListPanelFunc::refresh);
    dialog->show();
}

void ListPanelFunc::refreshActions()
{
    panel->updateButtons();

    if(ACTIVE_PANEL != panel)
        return;

    QString protocol = files()->currentDirectory().scheme();
    krRemoteEncoding->setEnabled(protocol == "ftp" || protocol == "sftp" || protocol == "fish" || protocol == "krarc");
    //krMultiRename->setEnabled( fileSystemType == FileSystem::FS_NORMAL );  // batch rename
    //krProperties ->setEnabled( fileSystemType == FileSystem::FS_NORMAL || fileSystemType == FileSystem::FS_FTP ); // file properties

    /*
      krUnpack->setEnabled(true);                            // unpack archive
      krTest->setEnabled(true);                              // test archive
      krSelect->setEnabled(true);                            // select a group by filter
      krSelectAll->setEnabled(true);                         // select all files
      krUnselect->setEnabled(true);                          // unselect by filter
      krUnselectAll->setEnabled( true);                      // remove all selections
      krInvert->setEnabled(true);                            // invert the selection
      krFTPConnect->setEnabled(true);                        // connect to an ftp
      krFTPNew->setEnabled(true);                            // create a new connection
      krAllFiles->setEnabled(true);                          // show all files in list
      krCustomFiles->setEnabled(true);                       // show a custom set of files
      krRoot->setEnabled(true);                              // go all the way up
          krExecFiles->setEnabled(true);                         // show only executables
    */

    panel->_actions->setViewActions[panel->panelType]->setChecked(true);
    panel->_actions->actFTPDisconnect->setEnabled(files()->isRemote()); // allow disconnecting a network session
    panel->_actions->actCreateChecksum->setEnabled(files()->isLocal());
    panel->_actions->actDirUp->setEnabled(!files()->isRoot());
    panel->_actions->actRoot->setEnabled(!panel->virtualPath().matches(QUrl::fromLocalFile(ROOT_DIR),
                                                                       QUrl::StripTrailingSlash));
    panel->_actions->actHome->setEnabled(!atHome());
    panel->_actions->actHistoryBackward->setEnabled(history->canGoBack());
    panel->_actions->actHistoryForward->setEnabled(history->canGoForward());
    panel->view->op()->emitRefreshActions();
}

FileSystem* ListPanelFunc::files()
{
    if (!fileSystemP)
        fileSystemP = FileSystemProvider::instance().getFilesystem(QUrl::fromLocalFile(ROOT_DIR));
    return fileSystemP;
}

QUrl ListPanelFunc::virtualDirectory()
{
    return _isPaused ? history->currentUrl() : files()->currentDirectory();
}

FileItem *ListPanelFunc::getFileItem(const QString &name)
{
    return files()->getFileItem(name);
}

FileItem *ListPanelFunc::getFileItem(KrViewItem *item)
{
    return files()->getFileItem(item->name());
}

void ListPanelFunc::clipboardChanged(QClipboard::Mode mode)
{
    if (mode == QClipboard::Clipboard && this == copyToClipboardOrigin) {
        disconnect(QApplication::clipboard(), nullptr, this, nullptr);
        copyToClipboardOrigin = nullptr;
    }
}

void ListPanelFunc::copyToClipboard(bool move)
{
    const QStringList fileNames = panel->getSelectedNames();
    if (fileNames.isEmpty())
        return ;  // safety

    QList<QUrl> fileUrls = files()->getUrls(fileNames);
    auto *mimeData = new QMimeData;
    mimeData->setData("application/x-kde-cutselection", move ? "1" : "0");
    mimeData->setUrls(fileUrls);

    if (copyToClipboardOrigin)
        disconnect(QApplication::clipboard(), nullptr, copyToClipboardOrigin, nullptr);
    copyToClipboardOrigin = this;

    QApplication::clipboard()->setMimeData(mimeData, QClipboard::Clipboard);

    connect(QApplication::clipboard(), &QClipboard::changed, this, &ListPanelFunc::clipboardChanged);
}

void ListPanelFunc::pasteFromClipboard()
{
    QClipboard * cb = QApplication::clipboard();

    ListPanelFunc *origin = nullptr;

    if (copyToClipboardOrigin) {
        disconnect(QApplication::clipboard(), nullptr, copyToClipboardOrigin, nullptr);
        origin = copyToClipboardOrigin;
        copyToClipboardOrigin = nullptr;
    }

    bool move = false;
    const QMimeData *data = cb->mimeData();
    if (data->hasFormat("application/x-kde-cutselection")) {
        QByteArray a = data->data("application/x-kde-cutselection");
        if (!a.isEmpty())
            move = (a.at(0) == '1'); // true if 1
    }

    QList<QUrl> urls = data->urls();
    if (urls.isEmpty())
        return;

    if(origin && KConfigGroup(krConfig, "Look&Feel").readEntry("UnselectBeforeOperation", _UnselectBeforeOperation)) {
        origin->panel->view->saveSelection();
        for(KrViewItem *item = origin->panel->view->getFirst(); item != nullptr; item = origin->panel->view->getNext(item)) {
            if (urls.contains(item->getFileItem()->getUrl()))
                item->setSelected(false);
        }
    }

    files()->addFiles(urls, move ? KIO::CopyJob::Move : KIO::CopyJob::Copy);
}

ListPanelFunc* ListPanelFunc::otherFunc()
{
    return panel->otherPanel()->func;
}

void ListPanelFunc::historyGotoPos(int pos)
{
    if(history->gotoPos(pos))
        refresh();
}

void ListPanelFunc::historyBackward()
{
    if(history->goBack())
        refresh();
}

void ListPanelFunc::historyForward()
{
    if(history->goForward())
        refresh();
}

void ListPanelFunc::dirUp()
{
    openUrl(KIO::upUrl(files()->currentDirectory()), files()->currentDirectory().fileName());
}

void ListPanelFunc::home()
{
    openUrl(QUrl::fromLocalFile(QDir::homePath()));
}

void ListPanelFunc::root()
{
    openUrl(QUrl::fromLocalFile(ROOT_DIR));
}

void ListPanelFunc::cdToOtherPanel()
{
    openUrl(panel->otherPanel()->virtualPath());
}

void ListPanelFunc::syncOtherPanel()
{
    otherFunc()->openUrl(panel->virtualPath());
}

bool ListPanelFunc::atHome()
{
    return QUrl::fromLocalFile(QDir::homePath()).matches(panel->virtualPath(), QUrl::StripTrailingSlash);
}
