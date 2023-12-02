// SPDX-License-Identifier: GPL-2.0-only
#include "gtest/gtest.h"

#include <QtWidgets/qapplication.h>

#include <ThreadSafeAudioLibrary.h>
#include "tools.h"

TEST(AudioExplorer, ThreadSafeAudioLibrary)
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