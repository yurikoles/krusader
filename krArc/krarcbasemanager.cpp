#include "krarcbasemanager.h"

#include <KArchive/KTar>

KrArcBaseManager::AutoDetectParams KrArcBaseManager::autoDetectParams[] = {{"zip",  0, "PK\x03\x04"},
    {"rar",  0, "Rar!\x1a" },
    {"arj",  0, "\x60\xea" },
    {"rpm",  0, "\xed\xab\xee\xdb"},
    {"ace",  7, "**ACE**" },
    {"bzip2", 0, "\x42\x5a\x68\x39\x31" },
    {"gzip", 0, "\x1f\x8b"},
    {"deb",  0, "!<arch>\ndebian-binary   " },
    {"7z",   0, "7z\xbc\xaf\x27\x1c" }/*,
    {"xz",   0, "\xfd\x37\x7a\x58\x5a\x00"}*/
};

int KrArcBaseManager::autoDetectElems = sizeof(autoDetectParams) / sizeof(AutoDetectParams);

QString KrArcBaseManager::detectArchive(bool &encrypted, QString fileName, bool checkEncrypted, bool fast)
{
    encrypted = false;

    QFile arcFile(fileName);
    if (arcFile.open(QIODevice::ReadOnly)) {
        char buffer[ 1024 ];
        long sizeMax = arcFile.read(buffer, sizeof(buffer));
        arcFile.close();

        for (int i = 0; i < autoDetectElems; i++) {
            QByteArray detectionString = autoDetectParams[ i ].detectionString;
            int location = autoDetectParams[ i ].location;

            int endPtr = detectionString.length() + location;
            if (endPtr > sizeMax)
                continue;

            int j = 0;
            for (; j != detectionString.length(); j++) {
                if (detectionString[ j ] == '?')
                    continue;
                if (buffer[ location + j ] != detectionString[ j ])
                    break;
            }

            if (j == detectionString.length()) {
                QString type = autoDetectParams[ i ].type;
                if (type == "bzip2" || type == "gzip") {
                    if (fast) {
                        if (fileName.endsWith(QLatin1String(".tar.gz")))
                            type = "tgz";
                        else if (fileName.endsWith(QLatin1String(".tar.bz2")))
                            type = "tbz";
                    } else {
                        KTar tapeArchive(fileName);
                        if (tapeArchive.open(QIODevice::ReadOnly)) {
                            tapeArchive.close();
                            if (type == "gzip")
                                type = "tgz";
                            else if (type == "bzip2")
                                type = "tbz";
                        }
                    }
                } else if (type == "zip")
                    encrypted = (buffer[6] & 1);
                else if (type == "arj") {
                    if (sizeMax > 4) {
                        long headerSize = ((unsigned char *)buffer)[ 2 ] + 256 * ((unsigned char *)buffer)[ 3 ];
                        long fileHeader = headerSize + 10;
                        if (fileHeader + 9 < sizeMax && buffer[ fileHeader ] == (char)0x60 && buffer[ fileHeader + 1 ] == (char)0xea)
                            encrypted = (buffer[ fileHeader + 8 ] & 1);
                    }
                } else if (type == "rar") {
                    if (sizeMax > 13 && buffer[ 9 ] == (char)0x73) {
                        if (buffer[ 10 ] & 0x80) {  // the header is encrypted?
                            encrypted = true;
                        } else {
                            long offset = 7;
                            long mainHeaderSize = ((unsigned char *)buffer)[ offset+5 ] + 256 * ((unsigned char *)buffer)[ offset+6 ];
                            offset += mainHeaderSize;
                            while (offset + 10 < sizeMax) {
                                long headerSize = ((unsigned char *)buffer)[ offset+5 ] + 256 * ((unsigned char *)buffer)[ offset+6 ];
                                bool isDir = (buffer[ offset+7 ] == '\0') && (buffer[ offset+8 ] == '\0') &&
                                             (buffer[ offset+9 ] == '\0') && (buffer[ offset+10 ] == '\0');

                                if (buffer[ offset + 2 ] != (char)0x74)
                                    break;
                                if (!isDir) {
                                    encrypted = (buffer[ offset + 3 ] & 4) != 0;
                                    break;
                                }
                                offset += headerSize;
                            }
                        }
                    }
                } else if (type == "ace") {
                    long offset = 0;
                    long mainHeaderSize = ((unsigned char *)buffer)[ offset+2 ] + 256 * ((unsigned char *)buffer)[ offset+3 ] + 4;
                    offset += mainHeaderSize;
                    while (offset + 10 < sizeMax) {
                        long headerSize = ((unsigned char *)buffer)[ offset+2 ] + 256 * ((unsigned char *)buffer)[ offset+3 ] + 4;
                        bool isDir = (buffer[ offset+11 ] == '\0') && (buffer[ offset+12 ] == '\0') &&
                                     (buffer[ offset+13 ] == '\0') && (buffer[ offset+14 ] == '\0');

                        if (buffer[ offset + 4 ] != (char)0x01)
                            break;
                        if (!isDir) {
                            encrypted = (buffer[ offset + 6 ] & 64) != 0;
                            break;
                        }
                        offset += headerSize;
                    }
                } else if (type == "7z") {
                    // encryption check is expensive, check only if it's necessary
                    if (checkEncrypted)
                        checkIf7zIsEncrypted(encrypted, fileName);
                }
                return type;
            }
        }

        if (sizeMax >= 512) {
            /* checking if it's a tar file */
            unsigned checksum = 32 * 8;
            char chksum[ 9 ];
            for (int i = 0; i != 512; i++)
                checksum += ((unsigned char *)buffer)[ i ];
            for (int i = 148; i != 156; i++)
                checksum -= ((unsigned char *)buffer)[ i ];
            sprintf(chksum, "0%o", checksum);
            if (!memcmp(buffer + 148, chksum, strlen(chksum))) {
                int k = strlen(chksum);
                for (; k < 8; k++)
                    if (buffer[148+k] != 0 && buffer[148+k] != 32)
                        break;
                if (k == 8)
                    return "tar";
            }
        }
    }

    if (fileName.endsWith(QLatin1String(".tar.lzma")) ||
            fileName.endsWith(QLatin1String(".tlz"))) {
        return "tlz";
    }
    if (fileName.endsWith(QLatin1String(".lzma"))) {
        return "lzma";
    }

    if (fileName.endsWith(QLatin1String(".tar.xz")) ||
            fileName.endsWith(QLatin1String(".txz"))) {
        return "txz";
    }
    if (fileName.endsWith(QLatin1String(".xz"))) {
        return "xz";
    }

    return QString();
}
