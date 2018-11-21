// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <vector>
#include <QtCore/qdatastream.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qdir.h>
#include <QtCore/qhashfunctions.h>
#include <QtCore/qstring.h>
#include <QtGui/qpixmap.h>
#include "TrackInfoReader.h"

class AudioLibraryTrack;
class AudioLibraryAlbum;

namespace std
{
    template<> struct hash<QString>
    {
        std::size_t operator()(const QString& s) const
        {
            return qHash(s);
        }
    };
}

class AudioLibraryAlbumKey
{
public:
    AudioLibraryAlbumKey() = default;
    AudioLibraryAlbumKey(const TrackInfo& info)
        : _artist(!info.album_artist.isEmpty() ? info.album_artist : info.artist)
        , _album(info.album)
        , _genre(info.genre)
        , _year(info.year)
        , _cover_checksum(qChecksum(info.cover.data(), info.cover.size()))
    {}

    std::tuple<QString, int, QString, QString, qint16> getMembersAsTuple() const
    {
        return std::tie(_artist, _year, _album, _genre, _cover_checksum);
    }

    bool operator<(const AudioLibraryAlbumKey& other) const
    {
        return getMembersAsTuple() < other.getMembersAsTuple();
    }

    bool operator==(const AudioLibraryAlbumKey& other) const
    {
        return getMembersAsTuple() == other.getMembersAsTuple();
    }

    bool operator!=(const AudioLibraryAlbumKey& other) const
    {
        return !operator==(other);
    }

    QString _artist;
    QString _album;
    QString _genre;
    int _year;
    quint16 _cover_checksum;
};

class AudioLibraryAlbum
{
public:
    AudioLibraryAlbum(const AudioLibraryAlbumKey& key, const QByteArray& cover)
        : _key(key)
        , _cover(cover)
    {}

    const QPixmap& getCoverPixmap();

    AudioLibraryAlbumKey _key;
    QByteArray _cover;

    std::vector<AudioLibraryTrack*> _tracks;

private:
    bool _tried_loading_cover = false;
    QPixmap _cover_pixmap;
};

class AudioLibraryTrack
{
public:
    AudioLibraryTrack(AudioLibraryAlbum* album,
        const QString& filepath,
        const QDateTime& last_modified,
        const QString& artist,
        const QString& album_artist,
        const QString& title,
        int track_number,
        const QString& comment,
        const QString& tag_types)
        : _album(album)
        , _filepath(filepath)
        , _last_modified(last_modified)
        , _artist(artist)
        , _album_artist(album_artist)
        , _title(title)
        , _track_number(track_number)
        , _comment(comment)
        , _tag_types(tag_types)
    {}

    std::tuple<AudioLibraryAlbumKey, QByteArray, QString, QString, QString, QDateTime, QString, int, QString, QString> getMembersAsTuple() const
    {
        return std::tie(_album->_key, _album->_cover, _artist, _album_artist, _filepath, _last_modified, _title, _track_number, _comment, _tag_types);
    }

    bool operator==(const AudioLibraryTrack& other) const
    {
        return getMembersAsTuple() == other.getMembersAsTuple();
    }

    bool operator!=(const AudioLibraryTrack& other) const
    {
        return !operator==(other);
    }

    AudioLibraryAlbum* _album = nullptr;
    QString _artist;
    QString _album_artist;
    QString _filepath;
    QDateTime _last_modified;
    QString _title;
    int _track_number;
    QString _comment;
    QString _tag_types;
};

class AudioLibrary
{
public:
    AudioLibraryTrack* findTrack(const QString& filepath) const;
    void addTrack(const QFileInfo& file_info);
    void addTrack(const QString& filepath, const QDateTime& last_modified, const TrackInfo& track_info);

    void removeTrack(AudioLibraryTrack* track);
    void removeAlbum(AudioLibraryAlbum* album);
    void removeTracksWithInvalidPaths();

    std::vector<AudioLibraryAlbum*> getAlbums() const;
    void forEachAlbum(const std::function<void(AudioLibraryAlbum* album)>& func) const;
    AudioLibraryAlbum* getAlbum(const AudioLibraryAlbumKey& key) const;

    void cleanupTracksOutsideTheseDirectories(const QStringList& paths);
    void save(QDataStream& s);
    void load(QDataStream& s);

private:
    AudioLibraryAlbum* addAlbum(const AudioLibraryAlbumKey& album_key, const QByteArray& cover);
    AudioLibraryTrack* addTrack(AudioLibraryAlbum* album,
        const QString& filepath,
        const QDateTime& last_modified,
        const QString& artist,
        const QString& album_artist,
        const QString& title,
        int track_number,
        const QString& comment,
        const QString& tag_types);

    std::map<AudioLibraryAlbumKey, std::unique_ptr<AudioLibraryAlbum>> _album_map;
    std::unordered_map<QString, std::unique_ptr<AudioLibraryTrack>> _filepath_to_track_map;
};