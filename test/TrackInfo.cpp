// SPDX-License-Identifier: GPL-2.0-only
#include <QtCore/qcoreapplication.h>

#include <AudioLibrary.h>
#include "tools.h"

bool readAndAssertTrackInfo(const QString& audio_filepath, const QString& cover_filepath)
{
    // test cover loading
    // other tags are loaded by taglib internally and don't need extra testing here

    TrackInfo info;
    RETURN_IF_FAILED(readTrackInfo(audio_filepath, info));

    QFile cover_file(cover_filepath);
    RETURN_IF_FAILED(cover_file.open(QIODevice::ReadOnly));

    const QByteArray original_cover = cover_file.readAll();
    RETURN_IF_FAILED(!original_cover.isEmpty());
    RETURN_IF_FAILED(info.cover == original_cover);

    RETURN_IF_FAILED(info.album_artist == "albumartist");
    RETURN_IF_FAILED(info.disc_number == 1);

    RETURN_IF_FAILED(info.length_milliseconds / 1000 == 1);

    return true;
}

bool test_TrackInfoImp(const QString& source_test_data_dir)
{
    QString original_cover_filepath = source_test_data_dir + "/gradient.jpg";
    RETURN_IF_FAILED(readAndAssertTrackInfo(source_test_data_dir + "/noise.mp3", original_cover_filepath));
    RETURN_IF_FAILED(readAndAssertTrackInfo(source_test_data_dir + "/noise.ogg", original_cover_filepath));
    RETURN_IF_FAILED(readAndAssertTrackInfo(source_test_data_dir + "/noise.m4a", original_cover_filepath));
    RETURN_IF_FAILED(readAndAssertTrackInfo(source_test_data_dir + "/noise.wma", original_cover_filepath));
    RETURN_IF_FAILED(readAndAssertTrackInfo(source_test_data_dir + "/noise.ape", original_cover_filepath));

    return true;
}

int test_TrackInfo(int argc, char** const argv)
{
    QCoreApplication app(argc, argv);

    QString source_test_data_dir = app.arguments()[1];
    QString binary_dir = app.arguments()[2];

    return test_TrackInfoImp(source_test_data_dir) ? 0 : 1;
}