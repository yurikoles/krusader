/*****************************************************************************
 * Copyright (C) 2000 Shie Erlich <erlich@users.sourceforge.net>             *
 * Copyright (C) 2000 Rafi Yanai <yanai@users.sourceforge.net>               *
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

#ifndef KRDIALOGS_H
#define KRDIALOGS_H

// QtCore
#include <QDateTime>
#include <QUrl>
// QtWidgets
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
// QtGui
#include <QPixmap>

#include <KWidgetsAddons/KAnimatedButton>
#include <KIOWidgets/KFile>
#include <KIOWidgets/KUrlRequesterDialog>
#include <KWidgetsAddons/KDatePicker>

/** \class KChooseDir
 * Used for asking the user for a folder.
 * example:
 * \code
 * QUrl u = KChooseDir::getDir("target folder", "/suggested/path", ACTIVE_PANEL->virtualPath());
 * if (u.isEmpty()) {
 *   // user canceled (either by pressing cancel, or esc
 * } else {
 *   // do you thing here: you've got a safe url to use
 * }
 * \endcode
 */
class KChooseDir
{
public:
    struct ChooseResult {
        QUrl url;
        bool enqueue;
    };

    /**
     * \param text - description of the info requested from the user
     * \param url - a suggested url to appear in the box as a default choice
     * \param cwd - a path which is the current working directory (usually ACTIVE_PANEL->virtualPath()).
     *              this is used for completion of partial urls
     */
    static QUrl getFile(const QString &text, const QUrl &url, const QUrl &cwd);
    static QUrl getDir(const QString &text, const QUrl &url, const QUrl &cwd);
    static ChooseResult getCopyDir(const QString &text, const QUrl &url, const QUrl &cwd);


  private:
    static QUrl get(const QString &text, const QUrl &url, const QUrl &cwd, KFile::Modes mode);
};

class KUrlRequesterDlgForCopy : public QDialog
{
    Q_OBJECT
public:
    KUrlRequesterDlgForCopy(const QUrl& url, const QString& text, QWidget *parent,
                            bool modal = true);

    QUrl selectedURL() const;
    bool isQueued() { return queueStart; }

    KUrlRequester *urlRequester();

protected:
    void keyPressEvent(QKeyEvent *e) Q_DECL_OVERRIDE;

private slots:
    void slotQueueButtonClicked();
    void slotTextChanged(const QString &);

private:
    KUrlRequester *urlRequester_;
    QPushButton *okButton;
    bool queueStart = false;
};

class KRGetDate : public QDialog
{
    Q_OBJECT
public:
    explicit KRGetDate(QDate date = QDate::currentDate(), QWidget *parent = nullptr);
    QDate getDate();

private slots:
    void setDate(QDate);

private:
    KDatePicker *dateWidget;
    QDate chosenDate, originalDate;
};

#endif
