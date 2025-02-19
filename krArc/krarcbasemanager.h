/*****************************************************************************
 * Copyright (C) 2003 Rafi Yanai <krusader@users.sf.net>                     *
 * Copyright (C) 2003 Shie Erlich <krusader@users.sf.net>                    *
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

#ifndef KRARCBASEMANAGER_H
#define KRARCBASEMANAGER_H

// QtCore
#include <QFile>

/*!
 * \brief An abstract base class for managing archives.
 */
class KrArcBaseManager
{
private:
    //! Information about a type of archive and the bytes that are used to detect it.
    struct AutoDetectParams {
        QString type;
        int location;
        QByteArray detectionString;
    };

    static AutoDetectParams autoDetectParams[]; //! Information used to detect if a file is an archive
    static int autoDetectElems; //!< The size of autoDetectParams[]

protected:
    //! The maximum length of a short QString that represents the type of a file
    static const int maxLenType;

    static bool checkStatus(const QString &, int);

public:
    KrArcBaseManager() {}
    QString detectArchive(bool &, const QString&, bool = true, bool = false);
    virtual void checkIf7zIsEncrypted(bool &, QString) = 0;
    static QString getShortTypeFromMime(const QString &);
    virtual ~KrArcBaseManager() {}
};

#endif // KRARCBASEMANAGER_H

