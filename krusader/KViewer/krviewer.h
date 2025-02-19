/*****************************************************************************
 * Copyright (C) 2002 Shie Erlich <erlich@users.sourceforge.net>             *
 * Copyright (C) 2002 Rafi Yanai <yanai@users.sourceforge.net>               *
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

#ifndef KRVIEWER_H
#define KRVIEWER_H

// QtCore
#include <QEvent>
#include <QList>
#include <QPointer>
#include <QUrl>
// QtGui
#include <QKeyEvent>
#include <QFocusEvent>
// QtWidgets
#include <QAction>
#include <QMenu>
#include <QTabWidget>

#include <KParts/MainWindow>
#include <KParts/PartManager>
#include <KParts/BrowserExtension>

#include "../krglobal.h"

class PanelViewerBase;

class KrViewer : public KParts::MainWindow
{
    Q_OBJECT
public:
    virtual ~KrViewer();

    enum Mode {Generic, Text, Hex, Lister, Default};

    static void view(QUrl url, QWidget * parent = krMainWindow);
    static void view(QUrl url, Mode mode, bool new_window, QWidget * parent = krMainWindow);
    static void edit(QUrl url, QWidget * parent);
    static void edit(const QUrl& url, Mode mode = Text, int new_window = -1, QWidget * parent = krMainWindow);
    static void configureDeps();

    virtual bool eventFilter(QObject * watched, QEvent * e) Q_DECL_OVERRIDE;

public slots:
    void createGUI(KParts::Part*);
    void configureShortcuts();

    void viewGeneric();
    void viewText();
    void viewHex();
    void viewLister();
    void editText();

    void print();
    void copy();

    void tabChanged(int index);
    void tabURLChanged(PanelViewerBase * pvb, const QUrl &url);
    void tabCloseRequest(int index, bool force = false);
    void tabCloseRequest();

    void nextTab();
    void prevTab();
    void detachTab();

    void checkModified();

protected:
    virtual bool queryClose() Q_DECL_OVERRIDE;
    virtual void changeEvent(QEvent *e) Q_DECL_OVERRIDE;
    virtual void resizeEvent(QResizeEvent *e) Q_DECL_OVERRIDE;
    virtual void focusInEvent(QFocusEvent *) Q_DECL_OVERRIDE {
        if (viewers.removeAll(this)) viewers.prepend(this);
    } // move to first


private slots:
    void openUrlFinished(PanelViewerBase *pvb, bool success);

private:
    explicit KrViewer(QWidget *parent = 0);
    void addTab(PanelViewerBase* pvb);
    void updateActions();
    void refreshTab(PanelViewerBase* pvb);
    void viewInternal(QUrl url, Mode mode, QWidget * parent = krMainWindow);
    void editInternal(QUrl url, Mode mode, QWidget * parent = krMainWindow);
    void addPart(KParts::ReadOnlyPart *part);
    void removePart(KParts::ReadOnlyPart *part);
    bool isPartAdded(KParts::Part* part);

    static KrViewer* getViewer(bool new_window);
    static QString makeTabText(PanelViewerBase *pvb);
    static QString makeTabToolTip(PanelViewerBase *pvb);
    static QIcon makeTabIcon(PanelViewerBase *pvb);

    KParts::PartManager manager;
    QMenu* viewerMenu;
    QTabWidget tabBar;
    QPointer<QWidget> returnFocusTo;

    QAction *detachAction;
    QAction *printAction;
    QAction *copyAction;
    QAction *quitAction;

    QAction *configKeysAction;
    QAction *tabCloseAction;
    QAction *tabNextAction;
    QAction *tabPrevAction;

    static QList<KrViewer *> viewers; // the first viewer is the active one
    QList<int>    reservedKeys;   // the reserved key sequences
    QList<QAction *> reservedKeyActions; // the IDs of the reserved keys

    int sizeX;
    int sizeY;
};

class Invoker : public QObject
{
    Q_OBJECT

public:
    Invoker(QObject *recv, const char * slot) {
        connect(this, SIGNAL(invokeSignal()), recv, slot);
    }

    void invoke() {
        emit invokeSignal();
    }

signals:
    void invokeSignal();
};

#endif
