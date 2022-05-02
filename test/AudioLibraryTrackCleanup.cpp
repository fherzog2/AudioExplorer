// SPDX-License-Identifier: GPL-2.0-only
#include <QtCore/qcoreapplication.h>

#include <AudioLibrary.h>
#include "tools.h"

bool createEmptyFile(const QString& filepath)
{
    QFile file(filepath);
    return file.open(QIODevice::WriteOnly);
}

bool test_AudioLibraryTrackCleanupImp(const QString binary_dir)
{
    RETURN_IF_FAILED(!binary_dir.isEmpty());

    QString dirpath1 = binary_dir + "/test_AudioLibraryTrackCleanup1";
    QString dirpath2 = binary_dir + "/test_AudioLibraryTrackCleanup2";

    QDir dir1(dirpath1);
    RETURN_IF_FAILED(dir1.removeRecursively());
    RETURN_IF_FAILED(QDir().mkpath(dirpath1));

    QDir dir2(dirpath2);
    RETURN_IF_FAILED(dir2.removeRecursively());
    RETURN_IF_FAILED(QDir().mkpath(dirpath2));

    QString filepath1 = dirpath1 + "/file1.txt";
    QString filepath2 = dirpath2 + "/file2.txt";
    QString filepath3 = dirpath2 + "/file3.txt";

    RETURN_IF_FAILED(createEmptyFile(filepath1));
    RETURN_IF_FAILED(createEmptyFile(filepath2));
    RETURN_IF_FAILED(createEmptyFile(filepath3));

    AudioLibrary library;
    library.addTrack(filepath1, QDateTime(), 0, TrackInfo());
    library.addTrack(filepath2, QDateTime(), 0, TrackInfo());
    library.addTrack(filepath3, QDateTime(), 0, TrackInfo());

    QString new_filepath3 = dirpath2 + "/new file3.txt";

    RETURN_IF_FAILED(QDir().rename(filepath3, new_filepath3));

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

    RETURN_IF_FAILED(compareLibraries(library, library2));

    return true;
}

int test_AudioLibraryTrackCleanup(int argc, char** const argv)
{
    QCoreApplication app(argc, argv);

    QString source_test_data_dir = app.arguments()[1];
    QString binary_dir = app.arguments()[2];

    return test_AudioLibraryTrackCleanupImp(binary_dir) ? 0 : 1;
}