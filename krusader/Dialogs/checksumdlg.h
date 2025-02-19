/*****************************************************************************
 * Copyright (C) 2005 Shie Erlich <erlich@users.sourceforge.net>             *
 * Copyright (C) 2007-2008 Csaba Karai <cskarai@freemail.hu>                 *
 * Copyright (C) 2008 Jonas Bähr <jonas.baehr@web.de>                        *
 * Copyright (C) 2005-2018 Krusader Krew [https://krusader.org]              *
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

#ifndef CHECKSUMDLG_H
#define CHECKSUMDLG_H

// QtCore
#include <QFutureWatcher>
#include <QString>
#include <QTemporaryFile>
// QtWidgets
#include <QCheckBox>
#include <QLabel>
#include <QWizard>

#include <KCompletion/KComboBox>
#include <KCoreAddons/KProcess>

class KrListWidget;
class KrTreeWidget;

/**
 * Perform checksum operations: Creation of checksums or verifying files with a checksum file.
 *
 * The dialogs are not modal. The used checksum tools only support local files which are expected to
 * be in one directory (specified by 'path').
 */
class Checksum
{
  public:
    static void startCreationWizard(const QString &path, const QStringList &fileNames);
    static void startVerifyWizard(const QString &path, const QString &checksumFile = QString());
};

namespace  CHECKSUM_ { // private namespace

/** Wrapper for KProcess to handle errors and output. */
class ChecksumProcess : public KProcess
{
Q_OBJECT
  public:
    ChecksumProcess(QObject *parent, const QString &path);
    ~ChecksumProcess() override;

    QStringList stdOutput() const { return m_outputLines; }
    QStringList errOutput() const { return m_errorLines; }

  signals:
    void resultReady();

  private slots:
    void slotError(QProcess::ProcessError error);
    void slotFinished(int, QProcess::ExitStatus exitStatus);

  private:
    QStringList m_outputLines;
    QStringList m_errorLines;
    QTemporaryFile m_tmpOutFile;
    QTemporaryFile m_tmpErrFile;
};

/** Base class for common code in creation and verify wizard. */
class ChecksumWizard : public QWizard
{
    Q_OBJECT
public:
    explicit ChecksumWizard(const QString &path);
    ~ChecksumWizard() override;

private slots:
    void slotCurrentIdChanged(int id);

protected:
    virtual QWizardPage *createIntroPage() = 0;
    virtual QWizardPage *createResultPage() = 0;

    virtual void onIntroPage() = 0;
    virtual void onProgressPage() = 0;
    virtual void onResultPage() = 0;

    QWizardPage *createProgressPage(const QString &title);

    bool checkExists(const QString& type);
    void runProcess(const QString &type, const QStringList &args);
    void addChecksumLine(KrTreeWidget *tree, const QString &line);

    const QString m_path;
    ChecksumProcess *m_process;

    QMap<QString, QString> m_checksumTools; // extension/typ-name -> binary name

    int m_introId, m_progressId, m_resultId;
};

class CreateWizard : public ChecksumWizard
{
Q_OBJECT
public:
    CreateWizard(const QString &path, const QStringList &_files);

public slots:
    void accept() Q_DECL_OVERRIDE;

private:
    QWizardPage *createIntroPage() Q_DECL_OVERRIDE;
    QWizardPage *createResultPage() Q_DECL_OVERRIDE;

    void onIntroPage() Q_DECL_OVERRIDE;
    void onProgressPage() Q_DECL_OVERRIDE;
    void onResultPage() Q_DECL_OVERRIDE;

    void createChecksums();
    bool savePerFile();
    bool saveChecksumFile(const QStringList &data, const QString &filename = QString());

    const QStringList m_fileNames;

    QFutureWatcher<QStringList> m_listFilesWatcher;

    QString m_suggestedFilePath;

    // intro page
    KComboBox *m_methodBox;
    // result page
    KrTreeWidget *m_hashesTreeWidget;
    QLabel *m_errorLabel;
    KrListWidget *m_errorListWidget;
    QCheckBox *m_onePerFileBox;
};

class VerifyWizard : public ChecksumWizard
{
Q_OBJECT
public:
  VerifyWizard(const QString &path, const QString &inputFile);

private slots:
    void slotChecksumPathChanged(const QString &path);

private:
    QWizardPage *createIntroPage() Q_DECL_OVERRIDE;
    QWizardPage *createResultPage() Q_DECL_OVERRIDE;

    void onIntroPage() Q_DECL_OVERRIDE;
    void onProgressPage() Q_DECL_OVERRIDE;
    void onResultPage() Q_DECL_OVERRIDE;

    bool isSupported(const QString &path);

    QString m_checksumFile;

    // intro page
    KrTreeWidget *m_hashesTreeWidget;
    // result page
    QLabel *m_outputLabel;
    KrListWidget *m_outputListWidget;
};

} // NAMESPACE CHECKSUM_

#endif // CHECKSUMDLG_H
