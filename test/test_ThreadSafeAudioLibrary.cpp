// SPDX-License-Identifier: GPL-2.0-only
#include "gtest/gtest.h"

#include <ranges>
#include <QtWidgets/qapplication.h>

#include <ThreadSafeAudioLibrary.h>
#include "tools.h"

TEST(AudioExplorer, ThreadSafeAudioLibrary_LoadTestFiles)
{
    int argc = 1;
    char* argv = const_cast<char*>("");
    QApplication app(argc, &argv);

    ThreadSafeAudioLibrary library;
    library.setCacheLocation(QString());

    AudioFilesLoader audio_files_loader(library);
    audio_files_loader.startLoading({ "test_data" });

    // wait until the thread is finished

    while (audio_files_loader.isLoading())
        ;

    // check result

    ThreadSafeAudioLibrary::LibraryAccessor acc(library);

    ASSERT_EQ(acc.getLibrary().getAlbums().size(), 1);
}

TEST(AudioExplorer, ThreadSafeAudioLibrary_SimultaneousReadWrite)
{
    int argc = 0;
    char** argv = {};
    QApplication app(argc, argv);

    ThreadSafeAudioLibrary library;

    constexpr int MAX_NUMBER_OF_ALBUMS = 100;
    constexpr int TRACKS_PER_ALBUM = 10;

    auto write_thread = std::jthread([&library, MAX_NUMBER_OF_ALBUMS, TRACKS_PER_ALBUM] {
        for (const int album_number : std::views::iota(0, MAX_NUMBER_OF_ALBUMS))
        {
            for(const int track_number : std::views::iota(0, TRACKS_PER_ALBUM))
            {
                const auto filepath = QString("album %1 track %2").arg(album_number).arg(track_number);
                TrackInfo info;
                info.album = QString("album %1").arg(album_number);

                ThreadSafeAudioLibrary::LibraryAccessor acc(library);
                acc.getLibraryForUpdate().addTrack(filepath, QDateTime(), 1000, info);
            }
        }
    });

    int read_counter = 0;

    while (true)
    {
        ThreadSafeAudioLibrary::LibraryAccessor acc(library);
        ++read_counter;
        if (acc.getLibrary().getNumberOfTracks() >= MAX_NUMBER_OF_ALBUMS * TRACKS_PER_ALBUM)
        {
            break;
        }
    }

    qDebug() << "read_counter" << read_counter;

    write_thread = {};

    {
        ThreadSafeAudioLibrary::LibraryAccessor acc(library);
        ASSERT_EQ(acc.getLibrary().getNumberOfTracks(), MAX_NUMBER_OF_ALBUMS * TRACKS_PER_ALBUM);
    }
}