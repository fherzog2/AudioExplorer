// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <QtCore/qstring.h>
#include <QtCore/qdatetime.h>

struct TrackInfo
{
    QString artist;
    QString album_artist;
    QString album;
    int year = 0;
    QString genre;
    QByteArray cover;
    int disc_number = 0;

    QString title;
    int track_number = 0;
    QString comment;

    QString tag_types;

    int length_seconds = 0;
    int channels = 0;
    int bitrate_kbs = 0;
    int samplerate_hz = 0;
};

bool readTrackInfo(const QString& filepath, TrackInfo& info);