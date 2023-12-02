// SPDX-License-Identifier: GPL-2.0-only
#include "gtest/gtest.h"

#include <QtCore/qcoreapplication.h>
#include <QtCore/qbuffer.h>

#include <AudioLibrary.h>
#include "tools.h"

TEST(AudioExplorer, AudioLibrarySaveAndLoad)
{
    int argc = 1;
    char* argv = const_cast<char*>("");
    QCoreApplication app(argc, &argv);

    AudioLibrary lib;

    lib.addTrack("a", QDateTime(), 0, createTrackInfo("artist 1", QString(), "album 1", 2000, "genre 1", QByteArray(), "title 1", 1));
    lib.addTrack("b", QDateTime(), 0, createTrackInfo("artist 1", QString(), "album 1", 2000, "genre 1", QByteArray(), "title 2", 2));
    lib.addTrack("c", QDateTime(), 0, createTrackInfo("artist 1", QString(), "album 1", 2000, "genre 1", QByteArray(), "title 3", 3));

    lib.addTrack("d", QDateTime(), 0, createTrackInfo("artist 2", QString(), "album 2", 2000, "genre 1", QByteArray(), "title 1", 1));

    QByteArray bytes;

    {
        QBuffer buffer(&bytes);
        ASSERT_TRUE(buffer.open(QBuffer::WriteOnly));
        QDataStream s(&buffer);

        lib.save(s);
    }

    {
        AudioLibrary lib2;

        // load twice
        // first with an empty library
        // then with one that already contains data to throw away

        for (int i = 0; i < 2; ++i)
        {
            QBuffer buffer(&bytes);
            ASSERT_TRUE(buffer.open(QBuffer::ReadOnly));
            QDataStream s(&buffer);
            lib2.load(s);
            ASSERT_TRUE(compareLibraries(lib, lib2));
        }
    }
}