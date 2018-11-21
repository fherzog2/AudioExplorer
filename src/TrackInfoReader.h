// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <QtCore/qstring.h>
#include <QtCore/qdatetime.h>

struct TrackInfo
{
    QString artist;
    QString album_artist;
    QString album;
    int year;
    QString genre;
    QByteArray cover;

    QString title;
    int track_number;
    QString comment;

    QString tag_types;
};

bool readTrackInfo(const QString& filepath, TrackInfo& info);