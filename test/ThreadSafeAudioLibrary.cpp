// SPDX-License-Identifier: GPL-2.0-only
#include <QtWidgets/qapplication.h>

#include <ThreadSafeAudioLibrary.h>
#include "tools.h"

bool test_ThreadSafeAudioLibraryImp(const QString& audio_files_dir)
{
    ThreadSafeAudioLibrary library;
    library.setCacheLocation(QString());

    AudioFilesLoader audio_files_loader(library);
    audio_files_loader.startLoading({ audio_files_dir });

    // wait until the thread is finished

    while (audio_files_loader.isLoading())
        ;

    // check result

    ThreadSafeAudioLibrary::LibraryAccessor acc(library);

    return acc.getLibrary().getAlbums().size() == 1;
}

int test_ThreadSafeAudioLibrary(int argc, char** const argv)
{
    QApplication app(argc, argv);

    const QString source_test_dir = app.arguments()[1];

    return test_ThreadSafeAudioLibraryImp(source_test_dir + "/data") ? 0 : 1;
}