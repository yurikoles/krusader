/*****************************************************************************
 * Copyright (C) 2006 Shie Erlich <erlich@users.sourceforge.net>             *
 * Copyright (C) 2006 Rafi Yanai <yanai@users.sourceforge.net>               *
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

#include "useractionpage.h"

// QtWidgets
#include <QFileDialog>
#include <QSplitter>
#include <QLayout>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
// QtGui
#include <QClipboard>
// QtXml
#include <QDomDocumentType>

#include <KI18n/KLocalizedString>
#include <KWidgetsAddons/KStandardGuiItem>
#include <KCompletion/KLineEdit>
#include <KWidgetsAddons/KMessageBox>

#include "actionproperty.h"
#include "useractionlistview.h"
#include "../UserAction/useraction.h"
#include "../UserAction/kraction.h"
#include "../krusader.h"
#include "../krglobal.h"
#include "../icon.h"


//This is the filter in the QFileDialog of Import/Export:
static const char* FILE_FILTER = I18N_NOOP("*.xml|XML files\n*|All files");


UserActionPage::UserActionPage(QWidget* parent)
        : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);   // 0px margin, 6px item-spacing

    // ======== pseudo-toolbar start ========
    auto* toolbarLayout = new QHBoxLayout; // neither margin nor spacing for the toolbar with autoRaise
    toolbarLayout->setSpacing(0);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);

    newButton = new QToolButton(this);
    newButton->setIcon(Icon("document-new"));
    newButton->setAutoRaise(true);
    newButton->setToolTip(i18n("Create new useraction"));

    importButton = new QToolButton(this);
    importButton->setIcon(Icon("document-import"));
    importButton->setAutoRaise(true);
    importButton->setToolTip(i18n("Import useractions"));

    exportButton = new QToolButton(this);
    exportButton->setIcon(Icon("document-export"));
    exportButton->setAutoRaise(true);
    exportButton->setToolTip(i18n("Export useractions"));

    copyButton = new QToolButton(this);
    copyButton->setIcon(Icon("edit-copy"));
    copyButton->setAutoRaise(true);
    copyButton->setToolTip(i18n("Copy useractions to clipboard"));

    pasteButton = new QToolButton(this);
    pasteButton->setIcon(Icon("edit-paste"));
    pasteButton->setAutoRaise(true);
    pasteButton->setToolTip(i18n("Paste useractions from clipboard"));

    removeButton = new QToolButton(this);
    removeButton->setIcon(Icon("edit-delete"));
    removeButton->setAutoRaise(true);
    removeButton->setToolTip(i18n("Delete selected useractions"));

    toolbarLayout->addWidget(newButton);
    toolbarLayout->addWidget(importButton);
    toolbarLayout->addWidget(exportButton);
    toolbarLayout->addWidget(copyButton);
    toolbarLayout->addWidget(pasteButton);
    toolbarLayout->addSpacing(6);   // 6 pixel nothing
    toolbarLayout->addWidget(removeButton);
    toolbarLayout->addStretch(1000);   // some very large stretch-factor
    // ======== pseudo-toolbar end ========
    /* This seems obsolete now!
       // Display some help
       KMessageBox::information( this, // parent
         i18n( "When you apply changes to an action, the modifications "
          "become available in the current session immediately.\n"
          "When closing ActionMan, you will be asked to save the changes permanently."
         ),
        QString(), // caption
        "show UserAction help" //dontShowAgainName for the config
       );
    */
    layout->addLayout(toolbarLayout);
    auto *split = new QSplitter(this);
    layout->addWidget(split, 1000);   // again a very large stretch-factor to fix the height of the toolbar

    actionTree = new UserActionListView(split);
    actionTree->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    actionProperties = new ActionProperty(split);
    actionProperties->setEnabled(false);   // if there are any actions in the list, the first is displayed and this widget is enabled

    connect(actionTree, &UserActionListView::currentItemChanged, this, &UserActionPage::slotChangeCurrent);
    connect(newButton, &QToolButton::clicked, this, &UserActionPage::slotNewAction);
    connect(removeButton, &QToolButton::clicked, this, &UserActionPage::slotRemoveAction);
    connect(importButton, &QToolButton::clicked, this, &UserActionPage::slotImport);
    connect(exportButton, &QToolButton::clicked, this, &UserActionPage::slotExport);
    connect(copyButton, &QToolButton::clicked, this, &UserActionPage::slotToClip);
    connect(pasteButton, &QToolButton::clicked, this, &UserActionPage::slotFromClip);

    // forwards the changed signal of the properties
    connect(actionProperties, SIGNAL(changed()), SIGNAL(changed()));

    actionTree->setFirstActionCurrent();
    actionTree->setFocus();
}

UserActionPage::~UserActionPage()
= default;

bool UserActionPage::continueInSpiteOfChanges()
{
    if (! actionProperties->isModified())
        return true;

    int answer = KMessageBox::questionYesNoCancel(this,
                 i18n("The current action has been modified. Do you want to apply these changes?")
                                                 );
    if (answer == KMessageBox::Cancel) {
        disconnect(actionTree, &UserActionListView::currentItemChanged, this, &UserActionPage::slotChangeCurrent);
        actionTree->setCurrentAction(actionProperties->action());
        connect(actionTree, &UserActionListView::currentItemChanged, this, &UserActionPage::slotChangeCurrent);
        return false;
    }
    if (answer == KMessageBox::Yes) {
        if (! actionProperties->validProperties()) {
            disconnect(actionTree, &UserActionListView::currentItemChanged, this, &UserActionPage::slotChangeCurrent);
            actionTree->setCurrentAction(actionProperties->action());
            connect(actionTree, &UserActionListView::currentItemChanged, this, &UserActionPage::slotChangeCurrent);
            return false;
        }
        slotUpdateAction();
    } // if Yes
    return true;
}

void UserActionPage::slotChangeCurrent()
{
    if (! continueInSpiteOfChanges())
        return;

    KrAction* action = actionTree->currentAction();
    if (action) {
        actionProperties->setEnabled(true);
        // the distinct name is used as ID it is not allowed to change it afterwards because it is may referenced anywhere else
        actionProperties->leDistinctName->setEnabled(false);
        actionProperties->updateGUI(action);
    } else {
        // If the current item in the tree is no action (i.e. a category), disable the properties
        actionProperties->clear();
        actionProperties->setEnabled(false);
    }
    emit applied(); // to disable the apply-button
}


void UserActionPage::slotUpdateAction()
{
    // check that we have a command line, title and a name
    if (! actionProperties->validProperties())
        return;

    if (actionProperties->leDistinctName->isEnabled()) {
        // := new entry
        KrAction* action = new KrAction(krApp->actionCollection(), actionProperties->leDistinctName->text());
        krUserAction->addKrAction(action);
        actionProperties->updateAction(action);
        UserActionListViewItem* item = actionTree->insertAction(action);
        actionTree->setCurrentItem(item);
    } else { // := edit an existing
        actionProperties->updateAction();
        actionTree->update(actionProperties->action());   // update the listviewitem as well...
    }
    apply();
}


void UserActionPage::slotNewAction()
{
    if (continueInSpiteOfChanges()) {
        actionTree->clearSelection();  // else the user may think that he is overwriting the selected action
        actionProperties->clear();
        actionProperties->setEnabled(true);   // it may be disabled because the tree has the focus on a category
        actionProperties->leDistinctName->setEnabled(true);
        actionProperties->leDistinctName->setFocus();
    }
}

void UserActionPage::slotRemoveAction()
{
    if (! dynamic_cast<UserActionListViewItem*>(actionTree->currentItem()))
        return;

    int messageDelete = KMessageBox::warningContinueCancel(this,   //parent
                        i18n("Are you sure that you want to remove all selected actions?"), //text
                        i18n("Remove Selected Actions?"),  //caption
                        KStandardGuiItem::remove(), //Label for the continue-button
                        KStandardGuiItem::cancel(),
                        "Confirm Remove UserAction", //dontAskAgainName (for the config-file)
                        KMessageBox::Dangerous | KMessageBox::Notify);

    if (messageDelete != KMessageBox::Continue)
        return;

    actionProperties->clear();
    actionProperties->setEnabled(false);

    actionTree->removeSelectedActions();

    apply();
}

void UserActionPage::slotImport()
{
    QString filename = QFileDialog::getOpenFileName(this, QString(), QString(), i18n(FILE_FILTER));
    if (filename.isEmpty())
        return;

    UserAction::KrActionList newActions;
    krUserAction->readFromFile(filename, UserAction::renameDoublicated, &newActions);

    QListIterator<KrAction *> it(newActions);
    while (it.hasNext())
        actionTree->insertAction(it.next());

    if (newActions.count() > 0) {
        apply();
    }
}

void UserActionPage::slotExport()
{
    if (! dynamic_cast<UserActionListViewItem*>(actionTree->currentItem()))
        return;

    QString filename = QFileDialog::getSaveFileName(this, QString(), QString(), i18n(FILE_FILTER));
    if (filename.isEmpty())
        return;

    QDomDocument doc = QDomDocument(ACTION_DOCTYPE);
    QFile file(filename);
    int answer = 0;
    if (file.open(QIODevice::ReadOnly)) {    // getting here, means the file already exists an can be read
        if (doc.setContent(&file))    // getting here means the file exists and already contains an UserAction-XML-tree
            answer = KMessageBox::warningYesNoCancel(this,  //parent
                     i18n("This file already contains some useractions.\nDo you want to overwrite it or should it be merged with the selected actions?"), //text
                     i18n("Overwrite or Merge?"), //caption
                     KStandardGuiItem::overwrite(), //label for Yes-Button
                     KGuiItem(i18n("Merge")) //label for No-Button
                                                    );
        file.close();
    }
    if (answer == 0 && file.exists())
        answer = KMessageBox::warningContinueCancel(this,  //parent
                 i18n("This file already exists. Do you want to overwrite it?"), //text
                 i18n("Overwrite Existing File?"), //caption
                 KStandardGuiItem::overwrite() //label for Continue-Button
                                                   );

    if (answer == KMessageBox::Cancel)
        return;

    if (answer == KMessageBox::No)   // that means the merge-button
        doc = actionTree->dumpSelectedActions(&doc);   // merge
    else // Yes or Continue means overwrite
        doc = actionTree->dumpSelectedActions();

    bool success = UserAction::writeToFile(doc, filename);
    if (! success)
        KMessageBox::error(this,
                           i18n("Cannot open %1 for writing.\nNothing exported.", filename),
                           i18n("Export Failed")
                          );
}

void UserActionPage::slotToClip()
{
    if (! dynamic_cast<UserActionListViewItem*>(actionTree->currentItem()))
        return;

    QDomDocument doc = actionTree->dumpSelectedActions();
    QApplication::clipboard()->setText(doc.toString());
}

void UserActionPage::slotFromClip()
{
    QDomDocument doc(ACTION_DOCTYPE);
    if (doc.setContent(QApplication::clipboard()->text())) {
        QDomElement root = doc.documentElement();
        UserAction::KrActionList newActions;
        krUserAction->readFromElement(root, UserAction::renameDoublicated, &newActions);

        QListIterator<KrAction *> it(newActions);
        while (it.hasNext())
            actionTree->insertAction(it.next());

        if (newActions.count() > 0) {
            apply();
        }
    } // if ( doc.setContent )
}

bool UserActionPage::readyToQuit()
{
    // Check if the current UserAction has changed
    if (! continueInSpiteOfChanges())
        return false;

    krUserAction->writeActionFile();
    return true;
}

void UserActionPage::apply()
{
    krUserAction->writeActionFile();
    emit applied();
}

void UserActionPage::applyChanges()
{
    slotUpdateAction();
}

