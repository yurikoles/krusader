/*****************************************************************************
 * Copyright (C) 2006 Jonas Bähr <jonas.baehr@web.de>                        *
 * Copyright (C) 2006-2018 Krusader Krew [https://krusader.org]              *
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

#ifndef USERACTIONLISTVIEW_H
#define USERACTIONLISTVIEW_H

#include "../GUI/krtreewidget.h"

class KrAction;
class QString;
class UserActionListViewItem;
class QDomDocument;

class UserActionListView : public KrTreeWidget
{
    Q_OBJECT

public:
    explicit UserActionListView(QWidget* parent = nullptr);
    ~UserActionListView() override;
    QSize sizeHint() const Q_DECL_OVERRIDE;

    void update();
    void update(KrAction* action);
    UserActionListViewItem* insertAction(KrAction* action);

    KrAction* currentAction() const;
    void setCurrentAction(const KrAction*);

    QDomDocument dumpSelectedActions(QDomDocument* mergeDoc = nullptr) const;

    void removeSelectedActions();

    /**
     * makes the first action in the list current
     */
    void setFirstActionCurrent();

    /**
     * makes @e item current and ensures its visibility
     */
protected slots:
    void slotCurrentItemChanged(QTreeWidgetItem*);

protected:
    QTreeWidgetItem* findCategoryItem(const QString& category);
    UserActionListViewItem* findActionItem(const KrAction* action);
};

class UserActionListViewItem : public QTreeWidgetItem
{
public:
    UserActionListViewItem(QTreeWidget* view, KrAction* action);
    UserActionListViewItem(QTreeWidgetItem* item, KrAction* action);
    ~UserActionListViewItem() override;

    void setAction(KrAction* action);
    KrAction* action() const;
    void update();

    /**
     * This reimplements qt's compare-function in order to have categories on the top of the list
     */
    bool operator<(const QTreeWidgetItem &other) const Q_DECL_OVERRIDE;

private:
    KrAction* _action;
};

#endif
