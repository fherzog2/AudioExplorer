// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <unordered_set>
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

#if QT_VERSION < QT_VERSION_CHECK(5,15,0)
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
#endif

class AudioLibraryAlbumKey
{
public:
    AudioLibraryAlbumKey() = default;
    AudioLibraryAlbumKey(QString _artist, QString _album, QString _genre, int _year, quint16 _cover_checksum);
    AudioLibraryAlbumKey(const TrackInfo& info);

    const QString& getArtist() const { return _artist; }
    const QString& getAlbum() const { return _album; }
    const QString& getGenre() const { return _genre; }
    int getYear() const { return _year; }
    quint16 getCoverChecksum() const { return _cover_checksum; }
    const QString& getId() const { return _id; }

    bool operator<(const AudioLibraryAlbumKey& other) const
    {
        return _id < other._id;
    }

    bool operator==(const AudioLibraryAlbumKey& other) const
    {
        return _id == other._id;
    }

    bool operator!=(const AudioLibraryAlbumKey& other) const
    {
        return !operator==(other);
    }

private:
    QString toString() const;

    QString _artist;
    QString _album;
    QString _genre;
    int _year = 0;
    quint16 _cover_checksum = 0;

    QString _id;
};

class AudioLibraryAlbum
{
public:
    AudioLibraryAlbum(const AudioLibraryAlbumKey& key, const QByteArray& cover);

    const AudioLibraryAlbumKey& getKey() const { return _key; }
    const QByteArray& getCover() const { return _cover; }

    const QString& getId() const { return _id; }

    const QString& getCoverType() const { return _cover_type; }

    const QPixmap& getCoverPixmap() const;
    void setCoverPixmap(const QPixmap& pixmap);
    bool isCoverPixmapSet() const;

    void addTrack(const AudioLibraryTrack* track);
    void removeTrack(const AudioLibraryTrack* track);
    const std::vector<const AudioLibraryTrack*>& getTracks() const { return _tracks; }

private:
    QString getCoverTypeInternal() const;

    AudioLibraryAlbumKey _key;
    QByteArray _cover;

    std::vector<const AudioLibraryTrack*> _tracks;

    QString _id;

    QString _cover_type;

    bool _is_cover_pixmap_set = false;
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
        int disc_number,
        const QString& comment,
        const QString& tag_types,
        int length_milliseconds,
        int channels,
        int bitrate_kbs,
        int samplerate_hz);

    bool operator==(const AudioLibraryTrack& other) const;
    bool operator!=(const AudioLibraryTrack& other) const;

    const AudioLibraryAlbum* getAlbum() const { return _album; }
    const QString& getArtist() const { return _artist; }
    const QString& getAlbumArtist() const { return _album_artist; }
    const QString& getFilepath() const { return _filepath; }
    const QDateTime& getLastModified() const { return _last_modified; }
    const QString& getTitle() const { return _title; }
    int getTrackNumber() const { return _track_number; }
    int getDiscNumber() const { return _disc_number; }
    const QString& getComment() const { return _comment; }
    const QString& getTagTypes() const { return _tag_types; }
    int getLengthMs() const { return _length_milliseconds; }
    int getChannels() const { return _channels; }
    int getBitrateKbs() const { return _bitrate_kbs; }
    int getSampleRateHz() const { return _samplerate_hz; }

    const QString& getId() const { return _id;  }

    AudioLibraryAlbum* getAlbum() { return _album; }
    void setAlbumPtr(AudioLibraryAlbum* album) { _album = album; }

private:
    AudioLibraryAlbum* _album = nullptr;
    QString _artist;
    QString _album_artist;
    QString _filepath;
    QDateTime _last_modified;
    QString _title;
    int _track_number;
    int _disc_number;
    QString _comment;
    QString _tag_types;
    int _length_milliseconds;
    int _channels;
    int _bitrate_kbs;
    int _samplerate_hz;

    QString _id;
};

class AudioLibrary
{
public:
    const AudioLibraryTrack* findTrack(const QString& filepath) const;
    void addTrack(const QString& filepath, const QDateTime& last_modified, const TrackInfo& track_info);

    void removeTrack(AudioLibraryTrack* track);
    void removeTracksWithInvalidPaths();

    std::vector<const AudioLibraryAlbum*> getAlbums() const;
    const AudioLibraryAlbum* getAlbum(const AudioLibraryAlbumKey& key) const;
    AudioLibraryAlbum* getAlbum(const AudioLibraryAlbumKey& key);
    size_t getNumberOfTracks() const;

    bool isModified() const;

    void removeTracksExcept(const std::unordered_set<QString>& loaded_audio_files);

    void save(QDataStream& s) const;
    void load(QDataStream& s);

    class Loader
    {
    public:
        void init(AudioLibrary& library, QDataStream& s);
        bool hasNextAlbum() const;
        void loadNextAlbum(AudioLibrary& library);

    private:
        QDataStream* _s = nullptr;
        quint64 _num_albums = 0;
        quint64 _albums_loaded = 0;
    };

private:
    AudioLibraryAlbum* addAlbum(const AudioLibraryAlbumKey& album_key, const QByteArray& cover);
    AudioLibraryTrack* addTrack(AudioLibraryAlbum* album,
        const QString& filepath,
        const QDateTime& last_modified,
        const QString& artist,
        const QString& album_artist,
        const QString& title,
        int track_number,
        int disc_number,
        const QString& comment,
        const QString& tag_types,
        int length_milliseconds,
        int channels,
        int bitrate_kbs,
        int samplerate_hz);

    std::map<AudioLibraryAlbumKey, std::unique_ptr<AudioLibraryAlbum>> _album_map;
    std::unordered_map<QString, std::unique_ptr<AudioLibraryTrack>> _filepath_to_track_map;
    bool _is_modified = false;
};