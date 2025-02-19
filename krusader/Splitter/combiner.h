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

#ifndef COMBINER_H
#define COMBINER_H

// QtCore
#include <QString>
#include <QUrl>
// QtWidgets
#include <QProgressDialog>

#include <KIO/Job>

#include "crc32.h"

class Combiner : public QProgressDialog
{
    Q_OBJECT

public:
    Combiner(QWidget* parent,  QUrl baseURLIn, QUrl destinationURLIn, bool unixNamingIn = false);
    ~Combiner() override;

    void combine();

private slots:
    void statDest();
    void statDestResult(KJob* job);
    void combineSplitFileDataReceived(KIO::Job *, const QByteArray &byteArray);
    void combineSplitFileFinished(KJob *job);
    void combineDataReceived(KIO::Job *, const QByteArray &);
    void combineReceiveFinished(KJob *);
    void combineDataSend(KIO::Job *, QByteArray &);
    void combineSendFinished(KJob *);
    void combineWritePercent(KJob *, unsigned long);

private:
    void openNextFile();
    void combineAbortJobs();


    QUrl            splURL;
    QUrl            readURL;
    QUrl            writeURL;

    QUrl            baseURL;
    QUrl            destinationURL;
    CRC32          *crcContext;
    QByteArray      transferArray;

    QString         splitFile;
    QString         error;

    bool            hasValidSplitFile;
    QString         expectedFileName;
    KIO::filesize_t expectedSize;
    QString         expectedCrcSum;

    int             fileCounter;
    bool            firstFileIs000;
    int             permissions;
    KIO::filesize_t receivedSize;

    KIO::Job         *statJob;
    KIO::TransferJob *combineReadJob;
    KIO::TransferJob *combineWriteJob;

    bool            unixNaming;
};

#endif /* __COMBINER_H__ */
