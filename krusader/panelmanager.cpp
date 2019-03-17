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

#include "panelmanager.h"

#include "defaults.h"
#include "icon.h"
#include "tabactions.h"
#include "krusaderview.h"
#include "krmainwindow.h"
#include "Panel/listpanel.h"
#include "Panel/panelfunc.h"
#include "Panel/PanelView/krviewfactory.h"

#include <assert.h>

// QtGui
#include <QImage>
// QtWidgets
#include <QStackedWidget>
#include <QToolButton>
#include <QGridLayout>

#include <KConfigCore/KConfig>
#include <KI18n/KLocalizedString>


PanelManager::PanelManager(QWidget *parent, KrMainWindow* mainWindow, bool left) :
        QWidget(parent),
        _otherManager(0),
        _actions(mainWindow->tabActions()),
        _layout(0),
        _left(left),
        _currentPanel(0)
{
    _layout = new QGridLayout(this);
    _layout->setContentsMargins(0, 0, 0, 0);
    _layout->setSpacing(0);
    _stack = new QStackedWidget(this);

    // new tab button
    _newTab = new QToolButton(this);
    _newTab->setAutoRaise(true);
    _newTab->setText(i18n("Open a new tab in home"));
    _newTab->setToolTip(i18n("Open a new tab in home"));
    _newTab->setIcon(Icon("tab-new"));
    _newTab->adjustSize();
    connect(_newTab, SIGNAL(clicked()), this, SLOT(slotNewTab()));

    // tab-bar
    _tabbar = new PanelTabBar(this, _actions);
    connect(_tabbar, SIGNAL(currentChanged(int)), this, SLOT(slotCurrentTabChanged(int)));
    connect(_tabbar, SIGNAL(tabCloseRequested(int)), this, SLOT(slotCloseTab(int)));
    connect(_tabbar, SIGNAL(closeCurrentTab()), this, SLOT(slotCloseTab()));
    connect(_tabbar, SIGNAL(newTab(QUrl)), this, SLOT(slotNewTab(QUrl)));
    connect(_tabbar, SIGNAL(draggingTab(QMouseEvent*)), this, SLOT(slotDraggingTab(QMouseEvent*)));
    connect(_tabbar, SIGNAL(draggingTabFinished(QMouseEvent*)), this, SLOT(slotDraggingTabFinished(QMouseEvent*)));

    QHBoxLayout *tabbarLayout = new QHBoxLayout;
    tabbarLayout->setSpacing(0);
    tabbarLayout->setContentsMargins(0, 0, 0, 0);

    tabbarLayout->addWidget(_tabbar);
    tabbarLayout->addWidget(_newTab);

    _layout->addWidget(_stack, 0, 0);
    _layout->addLayout(tabbarLayout, 1, 0);

    updateTabbarPos();

    setLayout(_layout);

    addPanel(true);

    tabsCountChanged();
}

void PanelManager::tabsCountChanged()
{
    const KConfigGroup cfg(krConfig, "Look&Feel");
    const bool showTabbar = _tabbar->count() > 1 || cfg.readEntry("Show Tab Bar On Single Tab", true);
    const bool showButtons = showTabbar && cfg.readEntry("Show Tab Buttons", true);

    _tabbar->setVisible(showTabbar);
    _newTab->setVisible(showButtons);

    // disable close button if only 1 tab is left
    _tabbar->setTabsClosable(showButtons && _tabbar->count() > 1);

    _actions->refreshActions();
}

void PanelManager::activate()
{
    assert(sender() == (currentPanel()->gui));
    emit setActiveManager(this);
    _actions->refreshActions();
}

void PanelManager::slotCurrentTabChanged(int index)
{
    ListPanel *panel = _tabbar->getPanel(index);

    if (!panel || panel == _currentPanel)
        return;

    ListPanel *previousPanel = _currentPanel;
    _currentPanel = panel;

    _stack->setCurrentWidget(_currentPanel);

    if (previousPanel) {
        previousPanel->slotFocusOnMe(false); // FIXME - necessary ?
    }
    _currentPanel->slotFocusOnMe(this == ACTIVE_MNG);

    emit pathChanged(panel);

    if (otherManager()) {
        otherManager()->currentPanel()->otherPanelChanged();
    }

    // go back to pinned url if tab is pinned and switched back to active
    if (panel->isPinned()) {
        panel->func->openUrl(panel->pinnedUrl());
    }
}

ListPanel* PanelManager::createPanel(KConfigGroup cfg)
{
    ListPanel * p = new ListPanel(_stack, this, cfg);
    connectPanel(p);
    return p;
}

void PanelManager::connectPanel(ListPanel *p)
{
    connect(p, &ListPanel::activate, this, &PanelManager::activate);
    connect(p, &ListPanel::pathChanged, this, [=]() { pathChanged(p); });
    connect(p, &ListPanel::pathChanged, this, [=]() { _tabbar->updateTab(p); });
}

void PanelManager::disconnectPanel(ListPanel *p)
{
    disconnect(p, &ListPanel::activate, this, nullptr);
    disconnect(p, &ListPanel::pathChanged, this, nullptr);
}

ListPanel* PanelManager::addPanel(bool setCurrent, KConfigGroup cfg, KrPanel *nextTo)
{
    // create the panel and add it into the widgetstack
    ListPanel * p = createPanel(cfg);
    _stack->addWidget(p);

    // now, create the corresponding tab
    int index = _tabbar->addPanel(p, setCurrent, nextTo);
    tabsCountChanged();

    if (setCurrent)
        slotCurrentTabChanged(index);

    return p;
}

void PanelManager::saveSettings(KConfigGroup config, bool saveHistory)
{
    config.writeEntry("ActiveTab", activeTab());

    KConfigGroup grpTabs(&config, "Tabs");
    foreach(const QString &grpTab, grpTabs.groupList())
        grpTabs.deleteGroup(grpTab);

    for(int i = 0; i < _tabbar->count(); i++) {
        ListPanel *panel = _tabbar->getPanel(i);
        KConfigGroup grpTab(&grpTabs, "Tab" + QString::number(i));
        panel->saveSettings(grpTab, saveHistory);
    }
}

void PanelManager::loadSettings(KConfigGroup config)
{
    KConfigGroup grpTabs(&config, "Tabs");
    int numTabsOld = _tabbar->count();
    int numTabsNew = grpTabs.groupList().count();

    for(int i = 0;  i < numTabsNew; i++) {
        KConfigGroup grpTab(&grpTabs, "Tab" + QString::number(i));
        // TODO workaround for bug 371453. Remove this when bug is fixed
        if (grpTab.keyList().isEmpty())
            continue;

        ListPanel *panel = i < numTabsOld ? _tabbar->getPanel(i) : addPanel(false, grpTab);
        panel->restoreSettings(grpTab);
        _tabbar->updateTab(panel);
    }

    for(int i = numTabsOld - 1; i >= numTabsNew && i > 0; i--)
        slotCloseTab(i);

    setActiveTab(config.readEntry("ActiveTab", 0));

    // this is needed so that all tab labels get updated
    layoutTabs();
}

void PanelManager::layoutTabs()
{
    // delayed url refreshes may be pending -
    // delay the layout too so it happens after them
    QTimer::singleShot(0, _tabbar, SLOT(layoutTabs()));
}

KrPanel *PanelManager::currentPanel() const {
    return _currentPanel;
}

void PanelManager::moveTabToOtherSide()
{
    if (tabCount() < 2)
        return;

    ListPanel *p;
    _tabbar->removeCurrentPanel(p);
    _stack->removeWidget(p);
    disconnectPanel(p);

    p->reparent(_otherManager->_stack, _otherManager);
    _otherManager->connectPanel(p);
    _otherManager->_stack->addWidget(p);
    _otherManager->_tabbar->addPanel(p, true);

    _otherManager->tabsCountChanged();
    tabsCountChanged();

    p->slotFocusOnMe();
}

void PanelManager::slotNewTab(const QUrl &url, bool setCurrent, KrPanel *nextTo)
{
    ListPanel *p = addPanel(setCurrent, KConfigGroup(), nextTo);
    if(nextTo && nextTo->gui) {
        // We duplicate tab settings by writing original settings to a temporary
        // group and making the new tab read settings from it. Duplicating
        // settings directly would add too much complexity.
        QString grpName = "PanelManager_" + QString::number(qApp->applicationPid());
        krConfig->deleteGroup(grpName); // make sure the group is empty
        KConfigGroup cfg(krConfig, grpName);

        nextTo->gui->saveSettings(cfg, true);
        // reset undesired duplicated settings
        cfg.writeEntry("Properties", 0);
        p->restoreSettings(cfg);
        krConfig->deleteGroup(grpName);
    }
    p->start(url);
}

void PanelManager::slotNewTab()
{
    slotNewTab(QUrl::fromLocalFile(QDir::home().absolutePath()));
    _currentPanel->slotFocusOnMe();
}

void PanelManager::slotCloseTab()
{
    slotCloseTab(_tabbar->currentIndex());
}

void PanelManager::slotCloseTab(int index)
{
    if (_tabbar->count() <= 1)    /* if this is the last tab don't close it */
        return;

    ListPanel *oldp;

    _tabbar->removePanel(index, oldp); //this automatically changes the current panel

    _stack->removeWidget(oldp);
    deletePanel(oldp);
    tabsCountChanged();
}

void PanelManager::updateTabbarPos()
{
    KConfigGroup group(krConfig, "Look&Feel");
    if(group.readEntry("Tab Bar Position", "bottom") == "top") {
        _layout->addWidget(_stack, 2, 0);
        _tabbar->setShape(QTabBar::RoundedNorth);
    } else {
        _layout->addWidget(_stack, 0, 0);
        _tabbar->setShape(QTabBar::RoundedSouth);
    }
}

int PanelManager::activeTab()
{
    return _tabbar->currentIndex();
}

void PanelManager::setActiveTab(int index)
{
    _tabbar->setCurrentIndex(index);
}

void PanelManager::slotRecreatePanels()
{
    updateTabbarPos();

    for (int i = 0; i != _tabbar->count(); i++) {
        QString grpName = "PanelManager_" + QString::number(qApp->applicationPid());
        krConfig->deleteGroup(grpName); // make sure the group is empty
        KConfigGroup cfg(krConfig, grpName);

        ListPanel *oldPanel = _tabbar->getPanel(i);
        oldPanel->saveSettings(cfg, true);
        disconnect(oldPanel);

        ListPanel *newPanel = createPanel(cfg);
        _stack->insertWidget(i, newPanel);
        _tabbar->changePanel(i, newPanel);

        if (_currentPanel == oldPanel) {
            _currentPanel = newPanel;
            _stack->setCurrentWidget(_currentPanel);
        }

        _stack->removeWidget(oldPanel);
        deletePanel(oldPanel);

        newPanel->restoreSettings(cfg);

        _tabbar->updateTab(newPanel);

        krConfig->deleteGroup(grpName);
    }
    tabsCountChanged();
    _currentPanel->slotFocusOnMe(this == ACTIVE_MNG);
    emit pathChanged(_currentPanel);
}

void PanelManager::slotNextTab()
{
    int currTab = _tabbar->currentIndex();
    int nextInd = (currTab == _tabbar->count() - 1 ? 0 : currTab + 1);
    _tabbar->setCurrentIndex(nextInd);
}


void PanelManager::slotPreviousTab()
{
    int currTab = _tabbar->currentIndex();
    int nextInd = (currTab == 0 ? _tabbar->count() - 1 : currTab - 1);
    _tabbar->setCurrentIndex(nextInd);
}

void PanelManager::reloadConfig()
{
    for (int i = 0; i < _tabbar->count(); i++) {
        ListPanel *panel = _tabbar->getPanel(i);
        if (panel) {
            panel->func->refresh();
        }
    }
}

void PanelManager::deletePanel(ListPanel * p)
{
    disconnect(p);
    delete p;
}

void PanelManager::slotCloseInactiveTabs()
{
    int i = 0;
    while (i < _tabbar->count()) {
        if (i == activeTab())
            i++;
        else
            slotCloseTab(i);
    }
}

void PanelManager::slotCloseDuplicatedTabs()
{
    int i = 0;
    while (i < _tabbar->count() - 1) {
        ListPanel * panel1 = _tabbar->getPanel(i);
        if (panel1 != 0) {
            for (int j = i + 1; j < _tabbar->count(); j++) {
                ListPanel * panel2 = _tabbar->getPanel(j);
                if (panel2 != 0 && panel1->virtualPath().matches(panel2->virtualPath(), QUrl::StripTrailingSlash)) {
                    if (j == activeTab()) {
                        slotCloseTab(i);
                        i--;
                        break;
                    } else {
                        slotCloseTab(j);
                        j--;
                    }
                }
            }
        }
        i++;
    }
}

int PanelManager::findTab(QUrl url)
{
    url.setPath(QDir::cleanPath(url.path()));
    for(int i = 0; i < _tabbar->count(); i++) {
        if(_tabbar->getPanel(i)) {
            QUrl panelUrl = _tabbar->getPanel(i)->virtualPath();
            panelUrl.setPath(QDir::cleanPath(panelUrl.path()));
            if(panelUrl.matches(url, QUrl::StripTrailingSlash))
                return i;
        }
    }
    return -1;
}

void PanelManager::slotLockTab()
{
    ListPanel *panel = _currentPanel;
    panel->gui->setTabState(panel->gui->isLocked() ? ListPanel::TabState::DEFAULT : ListPanel::TabState::LOCKED);
    _actions->refreshActions();
    _tabbar->updateTab(panel);
}

void PanelManager::slotPinTab()
{
    ListPanel *panel = _currentPanel;
    panel->gui->setTabState(panel->gui->isPinned() ? ListPanel::TabState::DEFAULT : ListPanel::TabState::PINNED);
    if (panel->gui->isPinned()) {
        QUrl virtualPath = panel->virtualPath();
        panel->setPinnedUrl(virtualPath);
    }
    _actions->refreshActions();
    _tabbar->updateTab(panel);
}

void PanelManager::newTabs(const QStringList& urls) {
    for(int i = 0; i < urls.count(); i++)
        slotNewTab(QUrl::fromUserInput(urls[i], QString(), QUrl::AssumeLocalFile));
}
