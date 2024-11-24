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
#include <QtCore/QUuid>
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

namespace std
{
    template<> struct hash<QUuid>
    {
        std::size_t operator()(const QUuid& uuid) const
        {
            return qHash(uuid);
        }
    };
}

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

    bool operator==(const AudioLibraryAlbumKey&) const;
    std::strong_ordering operator<=>(const AudioLibraryAlbumKey&) const;

    QString toString() const;

private:
    QString _artist;
    int _year = 0;
    QString _album;
    QString _genre;
    quint16 _cover_checksum = 0;
};

class AudioLibraryAlbum
{
public:
    AudioLibraryAlbum(const AudioLibraryAlbumKey& key, const QByteArray& cover, const QSize& cover_size);

    const AudioLibraryAlbumKey& getKey() const { return _key; }
    const QByteArray& getCover() const { return _cover; }
    const QSize& getCoverSize() const { return _cover_size; }

    const QUuid& getUuid() const { return _uuid; }

    const QString& getCoverType() const { return _cover_type; }

    void addTrack(const AudioLibraryTrack* track);
    void removeTrack(const AudioLibraryTrack* track);
    const std::vector<const AudioLibraryTrack*>& getTracks() const { return _tracks; }

private:
    QString getCoverTypeInternal() const;

    AudioLibraryAlbumKey _key;
    QByteArray _cover;
    QSize _cover_size;

    std::vector<const AudioLibraryTrack*> _tracks;

    QUuid _uuid = QUuid::createUuid();

    QString _cover_type;
};

class AudioLibraryTrack
{
public:
    AudioLibraryTrack(AudioLibraryAlbum* album,
        const QString& filepath,
        const QDateTime& last_modified,
        qint64 file_size,
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
    qint64 getFileSize() const { return _file_size; }
    const QString& getTitle() const { return _title; }
    int getTrackNumber() const { return _track_number; }
    int getDiscNumber() const { return _disc_number; }
    const QString& getComment() const { return _comment; }
    const QString& getTagTypes() const { return _tag_types; }
    int getLengthMs() const { return _length_milliseconds; }
    int getChannels() const { return _channels; }
    int getBitrateKbs() const { return _bitrate_kbs; }
    int getSampleRateHz() const { return _samplerate_hz; }

    const QUuid& getUuid() const { return _uuid; }

    AudioLibraryAlbum* getAlbum() { return _album; }
    void setAlbumPtr(AudioLibraryAlbum* album) { _album = album; }

private:
    AudioLibraryAlbum* _album = nullptr;
    QString _artist;
    QString _album_artist;
    QString _filepath;
    QDateTime _last_modified;
    qint64 _file_size;
    QString _title;
    int _track_number;
    int _disc_number;
    QString _comment;
    QString _tag_types;
    int _length_milliseconds;
    int _channels;
    int _bitrate_kbs;
    int _samplerate_hz;

    const QUuid _uuid = QUuid::createUuid();
};

class AudioLibrary
{
public:
    const AudioLibraryTrack* findTrack(const QString& filepath) const;
    void addTrack(const QString& filepath, const QDateTime& last_modified, qint64 file_size, const TrackInfo& track_info);

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
    AudioLibraryAlbum* addAlbum(const AudioLibraryAlbumKey& album_key, const QByteArray& cover, const QSize& cover_size);
    AudioLibraryTrack* addTrack(AudioLibraryAlbum* album,
        const QString& filepath,
        const QDateTime& last_modified,
        qint64 file_size,
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