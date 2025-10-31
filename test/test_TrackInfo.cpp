// SPDX-License-Identifier: GPL-2.0-only
#include "gtest/gtest.h"

#include <QtCore/qcoreapplication.h>

#include <AudioLibrary.h>
#include "tools.h"

void readAndAssertTrackInfo(const QString& audio_filepath, const QString& cover_filepath)
{
    // test cover loading
    // other tags are loaded by taglib internally and don't need extra testing here

    TrackInfo info;
    ASSERT_TRUE(readTrackInfo(audio_filepath, info));

    QFile cover_file(cover_filepath);
    ASSERT_TRUE(cover_file.open(QIODevice::ReadOnly));

    const QByteArray original_cover = cover_file.readAll();
    EXPECT_TRUE(!original_cover.isEmpty());
    EXPECT_EQ(info.cover, original_cover);

    EXPECT_EQ(info.album_artist, "albumartist");
    EXPECT_EQ(info.disc_number, 1);

    EXPECT_EQ(info.length_milliseconds / 1000, 1);
}

TEST(AudioExplorer, TrackInfo)
{
    int argc = 1;
    char* argv = const_cast<char*>("");
    QCoreApplication app(argc, &argv);

    QString original_cover_filepath = "test_data/gradient.jpg";
    readAndAssertTrackInfo("test_data/noise.mp3", original_cover_filepath);
    readAndAssertTrackInfo("test_data/noise.ogg", original_cover_filepath);
    readAndAssertTrackInfo("test_data/noise.m4a", original_cover_filepath);
    readAndAssertTrackInfo("test_data/noise.wma", original_cover_filepath);
    readAndAssertTrackInfo("test_data/noise.ape", original_cover_filepath);
}