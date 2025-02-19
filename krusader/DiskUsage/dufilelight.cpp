/*****************************************************************************
 * Copyright (C) 2004 Csaba Karai <krusader@users.sourceforge.net>           *
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

#include "dufilelight.h"
#include "radialMap/radialMap.h"

// QtGui
#include <QMouseEvent>
#include <QPixmap>
// QtWidgets
#include <QInputDialog>
#include <QMenu>

#include <KI18n/KLocalizedString>

#define SCHEME_POPUP_ID    6730

DUFilelight::DUFilelight(DiskUsage *usage)
        : RadialMap::Widget(usage), diskUsage(usage), currentDir(nullptr), refreshNeeded(true)
{
//     setFocusPolicy(Qt::StrongFocus);

    connect(diskUsage, &DiskUsage::enteringDirectory, this, &DUFilelight::slotDirChanged);
    connect(diskUsage, &DiskUsage::clearing, this, &DUFilelight::clear);
    connect(diskUsage, &DiskUsage::changed, this, &DUFilelight::slotChanged);
    connect(diskUsage, &DiskUsage::deleted, this, &DUFilelight::slotChanged);
    connect(diskUsage, &DiskUsage::changeFinished, this, &DUFilelight::slotRefresh);
    connect(diskUsage, &DiskUsage::deleteFinished, this, &DUFilelight::slotRefresh);
    connect(diskUsage, &DiskUsage::currentChanged, this, &DUFilelight::slotAboutToShow);
}

void DUFilelight::slotDirChanged(Directory *dir)
{
    if (diskUsage->currentWidget() != this)
        return;

    if (currentDir != dir) {
        currentDir = dir;

        invalidate(false);
        create(dir);
        refreshNeeded = false;
    }
}

void DUFilelight::clear()
{
    invalidate(false);
    currentDir = nullptr;
}

File * DUFilelight::getCurrentFile()
{
    const RadialMap::Segment * focus = focusSegment();

    if (!focus || focus->isFake() || focus->file() == currentDir)
        return nullptr;

    return (File *)focus->file();
}

void DUFilelight::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        File * file = nullptr;

        const RadialMap::Segment * focus = focusSegment();

        if (focus && !focus->isFake() && focus->file() != currentDir)
            file = (File *)focus->file();

        QMenu filelightPopup;
        filelightPopup.addAction(i18n("Zoom In"),  this, SLOT(zoomIn()), Qt::Key_Plus);
        filelightPopup.addAction(i18n("Zoom Out"), this, SLOT(zoomOut()), Qt::Key_Minus);

        QMenu schemePopup;
        schemePopup.addAction(i18n("Rainbow"),       this, SLOT(schemeRainbow()));
        schemePopup.addAction(i18n("High Contrast"), this, SLOT(schemeHighContrast()));
        schemePopup.addAction(i18n("KDE"),           this, SLOT(schemeKDE()));

        filelightPopup.addMenu(&schemePopup);
        schemePopup.setTitle(i18n("Scheme"));

        filelightPopup.addAction(i18n("Increase contrast"), this, SLOT(increaseContrast()));
        filelightPopup.addAction(i18n("Decrease contrast"), this, SLOT(decreaseContrast()));

        QAction * act = filelightPopup.addAction(i18n("Use anti-aliasing"), this, SLOT(changeAntiAlias()));
        act->setCheckable(true);
        act->setChecked(Filelight::Config::antiAliasFactor > 1);

        act = filelightPopup.addAction(i18n("Show small files"), this, SLOT(showSmallFiles()));
        act->setCheckable(true);
        act->setChecked(Filelight::Config::showSmallFiles);

        act = filelightPopup.addAction(i18n("Vary label font sizes"), this, SLOT(varyLabelFontSizes()));
        act->setCheckable(true);
        act->setChecked(Filelight::Config::varyLabelFontSizes);

        filelightPopup.addAction(i18n("Minimum font size"), this, SLOT(minFontSize()));

        diskUsage->rightClickMenu(event->globalPos(), file, &filelightPopup, i18n("Filelight"));
        return;
    } else if (event->button() == Qt::LeftButton) {
        const RadialMap::Segment * focus = focusSegment();

        if (focus && !focus->isFake() && focus->file() == currentDir) {
            diskUsage->dirUp();
            return;
        } else if (focus && !focus->isFake() && focus->file()->isDir()) {
            diskUsage->changeDirectory((Directory *)focus->file());
            return;
        }
    }

    RadialMap::Widget::mousePressEvent(event);
}

void DUFilelight::setScheme(Filelight::MapScheme scheme)
{
    Filelight::Config::scheme = scheme;
    Filelight::Config::write();
    slotRefresh();
}

void DUFilelight::increaseContrast()
{
    if ((Filelight::Config::contrast += 10) > 100)
        Filelight::Config::contrast = 100;

    Filelight::Config::write();
    slotRefresh();
}

void DUFilelight::decreaseContrast()
{
    if ((Filelight::Config::contrast -= 10) > 100)
        Filelight::Config::contrast = 0;

    Filelight::Config::write();
    slotRefresh();
}

void DUFilelight::changeAntiAlias()
{
    Filelight::Config::antiAliasFactor = 1 + (Filelight::Config::antiAliasFactor == 1);
    Filelight::Config::write();
    slotRefresh();
}

void DUFilelight::showSmallFiles()
{
    Filelight::Config::showSmallFiles = !Filelight::Config::showSmallFiles;
    Filelight::Config::write();
    slotRefresh();
}

void DUFilelight::varyLabelFontSizes()
{
    Filelight::Config::varyLabelFontSizes = !Filelight::Config::varyLabelFontSizes;
    Filelight::Config::write();
    slotRefresh();
}

void DUFilelight::minFontSize()
{
    bool ok = false;

    int result = QInputDialog::getInt(this, i18n("Krusader::Filelight"), i18n("Minimum font size"),
                                      (int)Filelight::Config::minFontPitch, 1, 100, 1, &ok);

    if (ok) {
        Filelight::Config::minFontPitch = (uint)result;

        Filelight::Config::write();
        slotRefresh();
    }
}

void DUFilelight::slotAboutToShow(int ndx)
{
    QWidget *widget = diskUsage->widget(ndx);
    if (widget == this && (diskUsage->getCurrentDir() != currentDir || refreshNeeded)) {
        refreshNeeded = false;
        if ((currentDir = diskUsage->getCurrentDir()) != nullptr) {
            invalidate(false);
            create(currentDir);
        }
    }
}

void DUFilelight::slotRefresh()
{
    if (diskUsage->currentWidget() != this)
        return;

    refreshNeeded = false;
    if (currentDir && currentDir == diskUsage->getCurrentDir()) {
        invalidate(false);
        create(currentDir);
    }
}

void DUFilelight::slotChanged(File *)
{
    if (!refreshNeeded)
        refreshNeeded = true;
}

