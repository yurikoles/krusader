/*****************************************************************************
 * Copyright (C) 2005 Csaba Karai <cskarai@freemail.hu>                      *
 * Copyright (C) 2005-2018 Krusader Krew [https://krusader.org]              *
 *                                                                           *
 * Based on KRemoteEncodingPlugin from Dawit Alemayehu <adawit@kde.org>      *
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

#include "krremoteencodingmenu.h"

// QtCore
#include <QDebug>
// QtWidgets
#include <QMenu>

#include <KXmlGui/KActionCollection>
#include <KCodecs/KCharsets>
#include <KConfigCore/KConfig>
#include <KConfigCore/KConfigGroup>
#include <KI18n/KLocalizedString>
#include <KIO/Scheduler>
#include <KIOCore/KProtocolManager>

#include "../krglobal.h"
#include "../icon.h"
#include "../Panel/krpanel.h"
#include "../Panel/panelfunc.h"

#define DATA_KEY    QString::fromLatin1("Charset")

KrRemoteEncodingMenu::KrRemoteEncodingMenu(const QString &text, const QString &iconName, KActionCollection *parent) :
        KActionMenu(Icon(iconName), text, parent), settingsLoaded(false)
{
    connect(menu(), &QMenu::aboutToShow, this, &KrRemoteEncodingMenu::slotAboutToShow);

    parent->addAction("changeremoteencoding", this);
}

void KrRemoteEncodingMenu::slotAboutToShow()
{
    if (!settingsLoaded)
        loadSettings();

    // uncheck everything
    QList<QAction *> acts = menu()->actions();
    foreach(QAction *act, acts)
    act->setChecked(false);

    QString charset = currentCharacterSet();
    if (!charset.isEmpty()) {
        int id = 1;
        QStringList::Iterator it;
        for (it = encodingNames.begin(); it != encodingNames.end(); ++it, ++id)
            if ((*it).indexOf(charset) != -1)
                break;

        bool found = false;

        foreach(QAction *act, acts) {
            if (act->data().canConvert<int> ()) {
                int idr = act->data().toInt();

                if (idr == id) {
                    act->setChecked(found = true);
                    break;
                }
            }
        }

        if (!found)
            qWarning() << Q_FUNC_INFO << "could not find entry for charset=" << charset;
    } else {
        foreach(QAction *act, acts) {
            if (act->data().canConvert<int> ()) {
                int idr = act->data().toInt();

                if (idr == -2) {
                    act->setChecked(true);
                    break;
                }
            }
        }
    }
}

QString KrRemoteEncodingMenu::currentCharacterSet()
{
    QUrl currentURL = ACTIVE_PANEL->virtualPath();
    return KProtocolManager::charsetFor(currentURL);
}

void KrRemoteEncodingMenu::loadSettings()
{
    settingsLoaded = true;
    encodingNames = KCharsets::charsets()->descriptiveEncodingNames();

    QMenu *qmenu = menu();
    disconnect(qmenu, &QMenu::triggered, this, &KrRemoteEncodingMenu::slotTriggered);
    connect(qmenu, &QMenu::triggered, this, &KrRemoteEncodingMenu::slotTriggered);
    qmenu->clear();

    QStringList::ConstIterator it;
    int count = 0;
    QAction *act;

    for (it = encodingNames.constBegin(); it != encodingNames.constEnd(); ++it) {
        act = qmenu->addAction(*it);
        act->setData(QVariant(++count));
        act->setCheckable(true);
    }
    qmenu->addSeparator();

    act = qmenu->addAction(i18n("Reload"));
    act->setCheckable(true);
    act->setData(QVariant(-1));

    act = qmenu->addAction(i18nc("Default encoding", "Default"));
    act->setCheckable(true);
    act->setData(QVariant(-2));
}

void KrRemoteEncodingMenu::slotTriggered(QAction * act)
{
    if (!act || !act->data().canConvert<int> ())
        return;

    int id = act->data().toInt();

    switch (id) {
    case -1:
        slotReload();
        return;
    case -2:
        chooseDefault();
        return;
    default:
        chooseEncoding(encodingNames[id - 1]);
    }
}

void KrRemoteEncodingMenu::chooseEncoding(QString encoding)
{
    QUrl currentURL = ACTIVE_PANEL->virtualPath();

    KConfig config(("kio_" + currentURL.scheme() + "rc").toLatin1());
    QString host = currentURL.host();

    QString charset = KCharsets::charsets()->encodingForName(encoding);

    KConfigGroup group(&config, host);
    group.writeEntry(DATA_KEY, charset);
    config.sync();

    // Update the io-slaves...
    updateKIOSlaves();
}

void KrRemoteEncodingMenu::slotReload()
{
    loadSettings();
}

void KrRemoteEncodingMenu::chooseDefault()
{
    QUrl currentURL = ACTIVE_PANEL->virtualPath();

    // We have no choice but delete all higher domain level
    // settings here since it affects what will be matched.
    KConfig config(("kio_" + currentURL.scheme() + "rc").toLatin1());

    QStringList partList = currentURL.host().split('.', QString::SkipEmptyParts);
    if (!partList.isEmpty()) {
        partList.erase(partList.begin());

        QStringList domains;
        // Remove the exact name match...
        domains << currentURL.host();

        while (partList.count()) {
            if (partList.count() == 2)
                if (partList[0].length() <= 2 && partList[1].length() == 2)
                    break;

            if (partList.count() == 1)
                break;

            domains << partList.join(".");
            partList.erase(partList.begin());
        }

        for (auto & domain : domains) {
            //qDebug() << "Domain to remove: " << *it;
            if (config.hasGroup(domain))
                config.deleteGroup(domain);
            else if (config.group("").hasKey(domain))
                config.group("").deleteEntry(domain);       //don't know what group name is supposed to be XXX
        }
    }
    config.sync();

    updateKIOSlaves();
}


void KrRemoteEncodingMenu::updateKIOSlaves()
{
    KIO::Scheduler::emitReparseSlaveConfiguration();

    // Reload the page with the new charset
    QTimer::singleShot(500, ACTIVE_FUNC, SLOT(refresh()));
}

