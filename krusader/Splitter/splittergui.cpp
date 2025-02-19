/*****************************************************************************
 * Copyright (C) 2003 Csaba Karai <krusader@users.sourceforge.net>           *
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

#include "splittergui.h"

#include "../FileSystem/filesystem.h"

// QtCore
#include <QDebug>
// QtGui
#include <QValidator>
#include <QKeyEvent>
// QtWidgets
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QLayout>
#include <QLabel>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QDialogButtonBox>

#include <KConfigCore/KConfigGroup>
#include <KConfigCore/KSharedConfig>
#include <KI18n/KLocalizedString>
#include <KWidgetsAddons/KMessageBox>
#include <utility>

struct SplitterGUI::PredefinedDevice
{
    PredefinedDevice(QString name, KIO::filesize_t capacity) :
        name(std::move(name)), capacity(capacity) {}
    PredefinedDevice(const PredefinedDevice &other) :
        name(other.name), capacity(other.capacity) {}
    PredefinedDevice &operator=(const PredefinedDevice &other);

    QString name;
    KIO::filesize_t capacity;
};


const QList<SplitterGUI::PredefinedDevice> &SplitterGUI::predefinedDevices()
{
    static QList<PredefinedDevice> list;
    if(!list.count()) {
        list << PredefinedDevice(i18n("1.44 MB (3.5\")"), 1457664);
        list << PredefinedDevice(i18n("1.2 MB (5.25\")"), 1213952);
        list << PredefinedDevice(i18n("720 kB (3.5\")"), 730112);
        list << PredefinedDevice(i18n("360 kB (5.25\")"), 362496);
        list << PredefinedDevice(i18n("100 MB (ZIP)"), 100431872);
        list << PredefinedDevice(i18n("250 MB (ZIP)"), 250331136);
        list << PredefinedDevice(i18n("650 MB (CD-R)"), 650*0x100000);
        list << PredefinedDevice(i18n("700 MB (CD-R)"), 700*0x100000);
    }
    return list;
};

SplitterGUI::SplitterGUI(QWidget* parent,  const QUrl& fileURL, const QUrl& defaultDir) :
        QDialog(parent),
        userDefinedSize(0x100000), lastSelectedDevice(-1),
        division(1)
{
    setModal(true);

    auto *grid = new QGridLayout(this);
    grid->setSpacing(6);
    grid->setContentsMargins(11, 11, 11, 11);

    QLabel *splitterLabel = new QLabel(this);
    splitterLabel->setText(i18n("Split the file %1 to folder:", fileURL.toDisplayString(QUrl::PreferLocalFile)));
    splitterLabel->setMinimumWidth(400);
    grid->addWidget(splitterLabel, 0 , 0);

    urlReq = new KUrlRequester(this);
    urlReq->setUrl(defaultDir);
    urlReq->setMode(KFile::Directory);
    grid->addWidget(urlReq, 1 , 0);

    QWidget *splitSizeLine = new QWidget(this);
    auto * splitSizeLineLayout = new QHBoxLayout;
    splitSizeLineLayout->setContentsMargins(0, 0, 0, 0);
    splitSizeLine->setLayout(splitSizeLineLayout);

    deviceCombo = new QComboBox(splitSizeLine);
    for (int i = 0; i != predefinedDevices().count(); i++)
        deviceCombo->addItem(predefinedDevices()[i].name);
    deviceCombo->addItem(i18n("User Defined"));
    splitSizeLineLayout->addWidget(deviceCombo);

    QLabel *spacer = new QLabel(splitSizeLine);
    spacer->setText(" ");
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    splitSizeLineLayout->addWidget(spacer);

    QLabel *bytesPerFile = new QLabel(splitSizeLine);
    bytesPerFile->setText(i18n("Max file size:"));
    splitSizeLineLayout->addWidget(bytesPerFile);

    spinBox = new QDoubleSpinBox(splitSizeLine);
    spinBox->setMaximum(9999999999.0);
    spinBox->setMinimumWidth(85);
    spinBox->setEnabled(false);
    splitSizeLineLayout->addWidget(spinBox);

    sizeCombo = new QComboBox(splitSizeLine);
    sizeCombo->addItem(i18n("Byte"));
    sizeCombo->addItem(i18n("kByte"));
    sizeCombo->addItem(i18n("MByte"));
    sizeCombo->addItem(i18n("GByte"));
    splitSizeLineLayout->addWidget(sizeCombo);

    grid->addWidget(splitSizeLine, 2 , 0);

    overwriteCb = new QCheckBox(i18n("Overwrite files without confirmation"), this);
    grid->addWidget(overwriteCb, 3, 0);

    QFrame *separator = new QFrame(this);
    separator->setFrameStyle(QFrame::HLine | QFrame::Sunken);
    separator->setFixedHeight(separator->sizeHint().height());

    grid->addWidget(separator, 4 , 0);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Ok)->setText(i18n("&Split"));

    grid->addWidget(buttonBox, 5 , 0);

    setWindowTitle(i18n("Krusader::Splitter"));


    KConfigGroup cfg(KSharedConfig::openConfig(), QStringLiteral("Splitter"));
    overwriteCb->setChecked(cfg.readEntry("OverWriteFiles", false));

    connect(sizeCombo, QOverload<int>::of(&QComboBox::activated), this, &SplitterGUI::sizeComboActivated);
    connect(deviceCombo, QOverload<int>::of(&QComboBox::activated), this, &SplitterGUI::predefinedComboActivated);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SplitterGUI::reject);
    connect(buttonBox , &QDialogButtonBox::accepted, this, &SplitterGUI::splitPressed);

    predefinedComboActivated(0);
}

SplitterGUI::~SplitterGUI()
{
    KConfigGroup cfg(KSharedConfig::openConfig(), QStringLiteral("Splitter"));
    cfg.writeEntry("OverWriteFiles", overwriteCb->isChecked());
}

KIO::filesize_t SplitterGUI::getSplitSize()
{
    if(deviceCombo->currentIndex() < predefinedDevices().count()) // predefined size selected
        return predefinedDevices()[deviceCombo->currentIndex()].capacity;
    // user defined size selected
    return spinBox->value() * division;
}

bool SplitterGUI::overWriteFiles()
{
    return overwriteCb->isChecked();
}

void SplitterGUI::sizeComboActivated(int item)
{
    KIO::filesize_t prevDivision = division;
    switch (item) {
    case 0:
        division = 1;            /* byte */
        break;
    case 1:
        division = 0x400;        /* kbyte */
        break;
    case 2:
        division = 0x100000;     /* Mbyte */
        break;
    case 3:
        division = 0x40000000;   /* Gbyte */
        break;
    }
    double value;
    if(deviceCombo->currentIndex() < predefinedDevices().count()) // predefined size selected
        value = (double)predefinedDevices()[deviceCombo->currentIndex()].capacity / division;
    else { // use defined size selected
        value = (double)(spinBox->value() * prevDivision) / division;
        if(value < 1)
            value = 1;
    }
    spinBox->setValue(value);
}

void SplitterGUI::predefinedComboActivated(int item)
{
    if(item == lastSelectedDevice)
        return;

    KIO::filesize_t capacity = userDefinedSize;

    if (lastSelectedDevice == predefinedDevices().count()) // user defined was selected previously
        userDefinedSize = spinBox->value() * division; // remember user defined size

    if(item < predefinedDevices().count()) { // predefined size selected
        capacity = predefinedDevices()[item].capacity;
        spinBox->setEnabled(false);
    } else // user defined size selected
        spinBox->setEnabled(true);

    //qDebug() << capacity;

    if (capacity >= 0x40000000) {          /* Gbyte */
        sizeCombo->setCurrentIndex(3);
        division = 0x40000000;
    } else if (capacity >= 0x100000) {      /* Mbyte */
        sizeCombo->setCurrentIndex(2);
        division = 0x100000;
    } else if (capacity >= 0x400) {         /* kbyte */
        sizeCombo->setCurrentIndex(1);
        division = 0x400;
    } else {
        sizeCombo->setCurrentIndex(0);       /* byte */
        division = 1;
    }

    spinBox->setValue((double)capacity / division);

    lastSelectedDevice = item;
}

void SplitterGUI::splitPressed()
{
    if (!urlReq->url().isValid()) {
        KMessageBox::error(this, i18n("The folder path URL is malformed."));
        return;
    }
    if(getSplitSize() > 0)
        emit accept();
}

void SplitterGUI::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
    case Qt::Key_Enter :
    case Qt::Key_Return :
        emit splitPressed();
        return;
    default:
        QDialog::keyPressEvent(e);
    }
}

