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

#ifndef KRSQUEEZEDTEXTLABEL_H
#define KRSQUEEZEDTEXTLABEL_H

// QtGui
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QResizeEvent>

#include <KWidgetsAddons/KSqueezedTextLabel>

class QMouseEvent;
class QDragEnterEvent;
class QPaintEvent;

/**
This class overloads KSqueezedTextLabel and simply adds a clicked signal,
so that users will be able to click the label and switch focus between panels.

NEW: a special setText() method allows to choose which part of the string should
     be displayed (example: make sure that search results won't be cut out)
*/
class KrSqueezedTextLabel : public KSqueezedTextLabel
{
    Q_OBJECT
public:
    explicit KrSqueezedTextLabel(QWidget *parent = nullptr);
    ~KrSqueezedTextLabel() override;

public slots:
    void setText(const QString &text, int index = -1, int length = -1);

signals:
    void clicked(QMouseEvent *); /**< emitted when someone clicks on the label */

protected:
    void resizeEvent(QResizeEvent *) Q_DECL_OVERRIDE {
        squeezeTextToLabel(_index, _length);
    }
    void mousePressEvent(QMouseEvent *e) Q_DECL_OVERRIDE;
    void paintEvent(QPaintEvent * e) Q_DECL_OVERRIDE;
    void squeezeTextToLabel(int index = -1, int length = -1);

    QString fullText;

private:
    int _index, _length;
};

#endif
