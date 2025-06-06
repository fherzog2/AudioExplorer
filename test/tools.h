// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <AudioLibrary.h>
#include <TrackInfoReader.h>

inline void PrintTo(const QString& str, ::std::ostream* os)
{
    *os << qPrintable(str);
}

namespace {

    [[maybe_unused]] TrackInfo createTrackInfo(QString artist,
        QString album_artist,
        QString album,
        int year,
        QString genre,
        QByteArray cover,
        QString title,
        int track_number,
        int length_milliseconds = 0)
    {
        TrackInfo info;

        info.artist = artist;
        info.album_artist = album_artist;
        info.album = album;
        info.year = year;
        info.genre = genre;
        info.cover = cover;
        info.title = title;
        info.track_number = track_number;
        info.length_milliseconds = length_milliseconds;

        return info;
    }

    [[maybe_unused]] bool compareLibraries(const AudioLibrary& a, const AudioLibrary& b)
    {
        const auto albums_a = a.getAlbums();
        const auto albums_b = b.getAlbums();

        if (albums_a.size() != albums_b.size())
            return false;

        for (size_t i = 0, endi = albums_a.size(); i < endi; ++i)
        {
            const auto album_a = albums_a[i];
            const auto album_b = albums_b[i];

            if (album_a->getKey() != album_b->getKey())
                return false;

            if (album_a->getCover() != album_b->getCover())
                return false;

            if (album_a->getTracks().size() != album_b->getTracks().size())
                return false;

            auto tracks_a = album_a->getTracks();
            auto tracks_b = album_b->getTracks();

            auto compare_tracks = [](const AudioLibraryTrack* a, const AudioLibraryTrack* b) {
                return a->getFilepath() < b->getFilepath();
            };

            // the library itself does not sort the tracks

            std::ranges::sort(tracks_a, compare_tracks);
            std::ranges::sort(tracks_b, compare_tracks);

            for (size_t j = 0, endj = album_a->getTracks().size(); j < endj; ++j)
            {
                const auto track_a = tracks_a[j];
                const auto track_b = tracks_b[j];

                if (*track_a != *track_b)
                    return false;
            }
        }

        return true;
    }
}