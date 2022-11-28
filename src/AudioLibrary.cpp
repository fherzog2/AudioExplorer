// SPDX-License-Identifier: GPL-2.0-only
#include "AudioLibrary.h"
#include <cassert>

QDataStream& operator<<(QDataStream& s, const AudioLibraryAlbumKey& key)
{
    s << key.getArtist();
    s << key.getAlbum();
    s << key.getGenre();
    s << qint32(key.getYear());
    s << key.getCoverChecksum();

    return s;
}

QDataStream& operator>>(QDataStream& s, AudioLibraryAlbumKey& key)
{
    QString artist;
    QString album;
    QString genre;
    qint32 year;
    quint16 cover_checksum;

    s >> artist;
    s >> album;
    s >> genre;
    s >> year;
    s >> cover_checksum;

    key = AudioLibraryAlbumKey(artist, album, genre, year, cover_checksum);

    return s;
}

//=============================================================================

AudioLibraryAlbumKey::AudioLibraryAlbumKey(QString artist, QString album, QString genre, int year, quint16 cover_checksum)
    : _artist(artist)
    , _album(album)
    , _genre(genre)
    , _year(year)
    , _cover_checksum(cover_checksum)
{
}

AudioLibraryAlbumKey::AudioLibraryAlbumKey(const TrackInfo& info)
    : _artist(!info.album_artist.isEmpty() ? info.album_artist : info.artist)
    , _album(info.album)
    , _genre(info.genre)
    , _year(info.year)
    , _cover_checksum(qChecksum(info.cover))
{
}

QString AudioLibraryAlbumKey::toString() const
{
    QLatin1Char sep(',');

    return _artist + sep +
        QString::number(_year) + sep +
        _album + sep +
        _genre + sep +
        QString::number(_cover_checksum);
}

bool AudioLibraryAlbumKey::operator<(const AudioLibraryAlbumKey& other) const
{
    auto tie = [](const AudioLibraryAlbumKey& key) {
        return std::tie(
            key._artist,
            key._year,
            key._album,
            key._genre,
            key._cover_checksum);
    };

    return tie(*this) < tie(other);
}

bool AudioLibraryAlbumKey::operator==(const AudioLibraryAlbumKey& other) const
{
    auto tie = [](const AudioLibraryAlbumKey& key) {
        return std::tie(
            key._artist,
            key._year,
            key._album,
            key._genre,
            key._cover_checksum);
    };

    return tie(*this) == tie(other);
}

bool AudioLibraryAlbumKey::operator!=(const AudioLibraryAlbumKey& other) const
{
    return !operator==(other);
}

//=============================================================================

AudioLibraryAlbum::AudioLibraryAlbum(const AudioLibraryAlbumKey& key, const QByteArray& cover, const QSize& cover_size)
    : _key(key)
    , _cover(cover)
    , _cover_size(cover_size)
{
    _cover_type = getCoverTypeInternal();
}

void AudioLibraryAlbum::addTrack(const AudioLibraryTrack* track)
{
    _tracks.push_back(track);

    // reset the uuid because data has been modified
    _uuid = QUuid::createUuid();
}

void AudioLibraryAlbum::removeTrack(const AudioLibraryTrack* track)
{
    auto new_end = std::remove(_tracks.begin(), _tracks.end(), track);
    _tracks.erase(new_end, _tracks.end());

    // reset the uuid because data has been modified
    _uuid = QUuid::createUuid();
}

template<class ARRAY>
bool compareSignature(const ARRAY& signature, const QByteArray& bytes)
{
    const size_t signature_size = std::distance(std::begin(signature), std::end(signature));

    return bytes.size() >= static_cast<int>(signature_size) &&
        memcmp(bytes.constData(), signature, signature_size) == 0;
}

QString AudioLibraryAlbum::getCoverTypeInternal() const
{
    const uint8_t JPG_SIGNATURE[] = { 0xff, 0xd8 };
    const uint8_t PNG_SIGNATURE[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
    const uint8_t BMP_SIGNATURE[] = { 0x42, 0x4d };

    if (compareSignature(JPG_SIGNATURE, _cover))
        return "jpg";

    if (compareSignature(PNG_SIGNATURE, _cover))
        return "png";

    if (compareSignature(BMP_SIGNATURE, _cover))
        return "bmp";

    if (!_cover.isEmpty())
    {
        return "unknown signature: " + QString::fromLatin1(_cover.left(32).toHex());
    }

    return QString();
}

//=============================================================================

AudioLibraryTrack::AudioLibraryTrack(AudioLibraryAlbum* album,
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
    int samplerate_hz)
    : _album(album)
    , _artist(artist)
    , _album_artist(album_artist)
    , _filepath(filepath)
    , _last_modified(last_modified)
    , _file_size(file_size)
    , _title(title)
    , _track_number(track_number)
    , _disc_number(disc_number)
    , _comment(comment)
    , _tag_types(tag_types)
    , _length_milliseconds(length_milliseconds)
    , _channels(channels)
    , _bitrate_kbs(bitrate_kbs)
    , _samplerate_hz(samplerate_hz)
{
}

bool AudioLibraryTrack::operator==(const AudioLibraryTrack& other) const
{
    auto tie = [](const AudioLibraryTrack& t) {
        return std::tie(
            t._album->getKey(),
            t._album->getCover(),
            t._artist,
            t._album_artist,
            t._filepath,
            t._last_modified,
            t._file_size,
            t._title,
            t._track_number,
            t._disc_number,
            t._comment,
            t._tag_types,
            t._length_milliseconds,
            t._channels,
            t._bitrate_kbs,
            t._samplerate_hz);
    };

    return tie(*this) == tie(other);
}

bool AudioLibraryTrack::operator!=(const AudioLibraryTrack& other) const
{
    return !operator==(other);
}

//=============================================================================

const AudioLibraryTrack* AudioLibrary::findTrack(const QString& filepath) const
{
    auto it = _filepath_to_track_map.find(filepath);
    if(it != _filepath_to_track_map.end())
        return it->second.get();

    return nullptr;
}

void AudioLibrary::addTrack(const QString& filepath, const QDateTime& last_modified, qint64 file_size, const TrackInfo& track_info)
{
    {
        auto it = _filepath_to_track_map.find(filepath);
        if (it != _filepath_to_track_map.end())
            removeTrack(it->second.get()); // clean up old stuff
    }

    AudioLibraryAlbum* album = addAlbum(AudioLibraryAlbumKey(track_info),
                                        track_info.cover);

    addTrack(album,
        filepath,
        last_modified,
        file_size,
        track_info.artist,
        track_info.album_artist,
        track_info.title,
        track_info.track_number,
        track_info.disc_number,
        track_info.comment,
        track_info.tag_types,
        track_info.length_milliseconds,
        track_info.channels,
        track_info.bitrate_kbs,
        track_info.samplerate_hz);

    _is_modified = true;
}

void AudioLibrary::removeTrack(AudioLibraryTrack* track)
{
    {
        track->getAlbum()->removeTrack(track);

        if(track->getAlbum()->getTracks().empty())
        {
            _album_map.erase(track->getAlbum()->getKey());
            track->setAlbumPtr(nullptr);
        }

        _filepath_to_track_map.erase(track->getFilepath());

        _is_modified = true;
    }
}

void AudioLibrary::removeTracksWithInvalidPaths()
{
    std::vector<AudioLibraryTrack*> tracks_to_remove;

    for(const auto& filepath_and_track : _filepath_to_track_map)
    {
        if(!QFileInfo::exists(filepath_and_track.first))
        {
            tracks_to_remove.push_back(filepath_and_track.second.get());
        }
    }

    for(AudioLibraryTrack* track : tracks_to_remove)
    {
        removeTrack(track);
    }
}

std::vector<const AudioLibraryAlbum*> AudioLibrary::getAlbums() const
{
    std::vector<const AudioLibraryAlbum*> result;

    for (const auto& album : _album_map)
        result.push_back(album.second.get());

    return result;
}

const AudioLibraryAlbum* AudioLibrary::getAlbum(const AudioLibraryAlbumKey& key) const
{
    auto it = _album_map.find(key);
    if (it != _album_map.end())
        return it->second.get();

    return nullptr;
}

AudioLibraryAlbum* AudioLibrary::getAlbum(const AudioLibraryAlbumKey& key)
{
    auto it = _album_map.find(key);
    if (it != _album_map.end())
        return it->second.get();

    return nullptr;
}

size_t AudioLibrary::getNumberOfTracks() const
{
    return _filepath_to_track_map.size();
}

bool AudioLibrary::isModified() const
{
    return _is_modified;
}

void AudioLibrary::removeTracksExcept(const std::unordered_set<QString>& loaded_audio_files)
{
    for (auto it = _filepath_to_track_map.begin(), end = _filepath_to_track_map.end(); it != end;)
    {
        if (loaded_audio_files.find(it->first) == loaded_audio_files.end())
        {
            // track is not one of the loaded files, must be outdated

            AudioLibraryTrack* track = it->second.get();
            ++it;
            removeTrack(track);
        }
        else
        {
            ++it;
        }
    }
}

void AudioLibrary::save(QDataStream& s) const
{
    s << qint32(7); // version

    s << quint64(_album_map.size());

    for (const auto& i : _album_map)
    {
        s << i.second->getKey();
        s << i.second->getCover();
        s << i.second->getCoverSize();

        s << quint64(i.second->getTracks().size());

        for(const AudioLibraryTrack* track : i.second->getTracks())
        {
            s << track->getFilepath();
            s << track->getLastModified();
            s << track->getFileSize();
            s << track->getArtist();
            s << track->getAlbumArtist();
            s << track->getTitle();
            s << qint32(track->getTrackNumber());
            s << qint32(track->getDiscNumber());
            s << track->getComment();
            s << track->getTagTypes();
            s << qint32(track->getLengthMs());
            s << qint32(track->getChannels());
            s << qint32(track->getBitrateKbs());
            s << qint32(track->getSampleRateHz());
        }
    }
}

void AudioLibrary::load(QDataStream& s)
{
    Loader loader;

    loader.init(*this, s);
    while (loader.hasNextAlbum())
        loader.loadNextAlbum(*this);
}

void AudioLibrary::Loader::init(AudioLibrary& library, QDataStream& s)
{
    _s = &s;

    library._album_map.clear();
    library._filepath_to_track_map.clear();
    library._is_modified = false;

    // for simplicity's sake, don't try to migrate old cache versions

    qint32 version;
    s >> version;
    if (version != 7)
        return;

    s >> _num_albums;
}

bool AudioLibrary::Loader::hasNextAlbum() const
{
    return _albums_loaded < _num_albums;
}

void AudioLibrary::Loader::loadNextAlbum(AudioLibrary& library)
{
    AudioLibraryAlbumKey key;
    QByteArray cover;
    QSize cover_size;

    *_s >> key;
    *_s >> cover;
    *_s >> cover_size;

    AudioLibraryAlbum* album = library.addAlbum(key, cover, cover_size);

    quint64 num_tracks;
    *_s >> num_tracks;

    for (quint64 ti = 0; ti < num_tracks; ++ti)
    {
        QString filepath;
        QDateTime last_modified;
        qint64 file_size;
        QString artist;
        QString album_artist;
        QString title;
        qint32 track_number;
        qint32 disc_number;
        QString comment;
        QString tag_types;
        qint32 length_milliseconds;
        qint32 channels;
        qint32 bitrate_kbs;
        qint32 samplerate_hz;

        *_s >> filepath;
        *_s >> last_modified;
        *_s >> file_size;
        *_s >> artist;
        *_s >> album_artist;
        *_s >> title;
        *_s >> track_number;
        *_s >> disc_number;
        *_s >> comment;
        *_s >> tag_types;
        *_s >> length_milliseconds;
        *_s >> channels;
        *_s >> bitrate_kbs;
        *_s >> samplerate_hz;

        library.addTrack(album, filepath, last_modified, file_size, artist, album_artist, title, track_number, disc_number, comment, tag_types, length_milliseconds, channels, bitrate_kbs, samplerate_hz);
    }

    ++_albums_loaded;
}

AudioLibraryAlbum* AudioLibrary::addAlbum(const AudioLibraryAlbumKey& album_key, const QByteArray& cover)
{
    auto it = _album_map.find(album_key);
    if (it == _album_map.end())
    {
        QPixmap cover_pixmap;
        cover_pixmap.loadFromData(cover);

        it = _album_map.insert(make_pair(album_key, std::make_unique<AudioLibraryAlbum>(album_key, cover, cover_pixmap.size()))).first;
    }

    return it->second.get();
}

AudioLibraryAlbum* AudioLibrary::addAlbum(const AudioLibraryAlbumKey& album_key, const QByteArray& cover, const QSize& cover_size)
{
    auto it = _album_map.find(album_key);
    if (it == _album_map.end())
    {
        it = _album_map.insert(make_pair(album_key, std::make_unique<AudioLibraryAlbum>(album_key, cover, cover_size))).first;
    }

    return it->second.get();
}

AudioLibraryTrack* AudioLibrary::addTrack(AudioLibraryAlbum* album,
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
    int samplerate_hz)
{
    auto it = _filepath_to_track_map.find(filepath);
    if (it != _filepath_to_track_map.end())
    {
        // file already added, shouldn't happen
        assert(false);
    }

    it = _filepath_to_track_map.insert(make_pair(filepath, std::make_unique<AudioLibraryTrack>(
        album,
        filepath,
        last_modified,
        file_size,
        artist,
        album_artist,
        title,
        track_number,
        disc_number,
        comment,
        tag_types,
        length_milliseconds,
        channels,
        bitrate_kbs,
        samplerate_hz))).first;
    album->addTrack(it->second.get());

    return it->second.get();
}