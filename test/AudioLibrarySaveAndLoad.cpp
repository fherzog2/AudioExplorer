// SPDX-License-Identifier: GPL-2.0-only
#include <QtCore/qcoreapplication.h>
#include <QtCore/qbuffer.h>

#include <AudioLibrary.h>
#include "tools.h"

bool test_AudioLibrarySaveAndLoadImp()
{
    AudioLibrary lib;

    lib.addTrack("a", QDateTime(), 0, createTrackInfo("artist 1", QString(), "album 1", 2000, "genre 1", QByteArray(), "title 1", 1));
    lib.addTrack("b", QDateTime(), 0, createTrackInfo("artist 1", QString(), "album 1", 2000, "genre 1", QByteArray(), "title 2", 2));
    lib.addTrack("c", QDateTime(), 0, createTrackInfo("artist 1", QString(), "album 1", 2000, "genre 1", QByteArray(), "title 3", 3));

    lib.addTrack("d", QDateTime(), 0, createTrackInfo("artist 2", QString(), "album 2", 2000, "genre 1", QByteArray(), "title 1", 1));

    QByteArray bytes;

    {
        QBuffer buffer(&bytes);
        RETURN_IF_FAILED(buffer.open(QBuffer::WriteOnly));
        QDataStream s(&buffer);

        lib.save(s);
    }

    {
        AudioLibrary lib2;

        // load twice
        // first with an empty library
        // then with one that already contains data to throw away

        for(int i = 0; i < 2; ++i)
        {
            QBuffer buffer(&bytes);
            RETURN_IF_FAILED(buffer.open(QBuffer::ReadOnly));
            QDataStream s(&buffer);
            lib2.load(s);
            RETURN_IF_FAILED(compareLibraries(lib, lib2));
        }
    }

    return true;
}

int test_AudioLibrarySaveAndLoad(int argc, char** const argv)
{
    QCoreApplication app(argc, argv);

    return test_AudioLibrarySaveAndLoadImp() ? 0 : 1;
}