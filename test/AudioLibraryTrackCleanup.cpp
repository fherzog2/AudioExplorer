// SPDX-License-Identifier: GPL-2.0-only
#include "gtest/gtest.h"

#include <QtCore/qcoreapplication.h>

#include <AudioLibrary.h>
#include "tools.h"

bool createEmptyFile(const QString& filepath)
{
    QFile file(filepath);
    return file.open(QIODevice::WriteOnly);
}

TEST(AudioExplorer, AudioLibraryTrackCleanup)
{
    int argc = 1;
    char* argv = const_cast<char*>("");
    QCoreApplication app(argc, &argv);

    QString dirpath1 = "test_AudioLibraryTrackCleanup1";
    QString dirpath2 = "test_AudioLibraryTrackCleanup2";

    QDir dir1(dirpath1);
    ASSERT_TRUE(dir1.removeRecursively());
    ASSERT_TRUE(QDir().mkpath(dirpath1));

    QDir dir2(dirpath2);
    ASSERT_TRUE(dir2.removeRecursively());
    ASSERT_TRUE(QDir().mkpath(dirpath2));

    QString filepath1 = dirpath1 + "/file1.txt";
    QString filepath2 = dirpath2 + "/file2.txt";
    QString filepath3 = dirpath2 + "/file3.txt";

    ASSERT_TRUE(createEmptyFile(filepath1));
    ASSERT_TRUE(createEmptyFile(filepath2));
    ASSERT_TRUE(createEmptyFile(filepath3));

    AudioLibrary library;
    library.addTrack(filepath1, QDateTime(), 0, TrackInfo());
    library.addTrack(filepath2, QDateTime(), 0, TrackInfo());
    library.addTrack(filepath3, QDateTime(), 0, TrackInfo());

    QString new_filepath3 = dirpath2 + "/new file3.txt";

    ASSERT_TRUE(QDir().rename(filepath3, new_filepath3));

    library.addTrack(new_filepath3, QDateTime(), 0, TrackInfo());

    std::unordered_set<QString> loaded_audio_files;
    loaded_audio_files.insert(filepath2);
    loaded_audio_files.insert(new_filepath3);

    library.removeTracksExcept(loaded_audio_files);

    // filepath1 is not inside valid_dirs, so the library must not contain it anymore
    // filepath3 does not exist anymore, so the library must not contain it anymore

    AudioLibrary library2;
    library2.addTrack(filepath2, QDateTime(), 0, TrackInfo());
    library2.addTrack(new_filepath3, QDateTime(), 0, TrackInfo());

    ASSERT_TRUE(compareLibraries(library, library2));
}