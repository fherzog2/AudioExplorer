// SPDX-License-Identifier: GPL-2.0-only
#include "AudioLibrary.h"
#include <assert.h>

QDataStream& operator<<(QDataStream& s, const AudioLibraryAlbumKey& key)
{
    s << key._artist;
    s << key._album;
    s << key._genre;
    s << qint32(key._year);
    s << key._cover_checksum;

    return s;
}

QDataStream& operator>>(QDataStream& s, AudioLibraryAlbumKey& key)
{
    s >> key._artist;
    s >> key._album;
    s >> key._genre;

    qint32 year;
    s >> year;
    key._year = year;

    s >> key._cover_checksum;

    return s;
}

QDataStream& operator<<(QDataStream& s, const AudioLibraryTrack& track)
{
    s << track._filepath;
    s << track._last_modified;
    s << track._title;
    s << qint32(track._track_number);
    s << track._comment;

    return s;
}

QDataStream& operator >> (QDataStream& s, AudioLibraryTrack& track)
{
    s >> track._filepath;
    s >> track._last_modified;
    s >> track._title;

    qint32 track_number;
    s >> track_number;
    track._track_number = track_number;

    s >> track._comment;

    return s;
}

//=============================================================================

const QPixmap& AudioLibraryAlbum::getCoverPixmap()
{
    if (!_tried_loading_cover)
    {
        _cover_pixmap.loadFromData(_cover);
        _tried_loading_cover = true;
    }

    return _cover_pixmap;
}

//=============================================================================

AudioLibraryTrack* AudioLibrary::findTrack(const QString& filepath) const
{
    auto it = _filepath_to_track_map.find(filepath);
    if(it != _filepath_to_track_map.end())
        return it->second.get();

    return nullptr;
}

void AudioLibrary::addTrack(const QFileInfo& file_info)
{
    QString filepath = file_info.filePath();
    QDateTime last_modified = file_info.lastModified();

    if(AudioLibraryTrack* track = findTrack(filepath))
        if(track->_last_modified == last_modified)
            return; // nothing to do

    TrackInfo track_info;
    if (readTrackInfo(filepath, track_info))
    {
        addTrack(filepath, last_modified, track_info);
    }
}

void AudioLibrary::addTrack(const QString& filepath, const QDateTime& last_modified, const TrackInfo& track_info)
{
    if(AudioLibraryTrack* track = findTrack(filepath))
        removeTrack(track); // clean up old stuff

    AudioLibraryAlbum* album = addAlbum(AudioLibraryAlbumKey(track_info),
                                        track_info.cover);

    AudioLibraryTrack* track = addTrack(album,
                                        filepath,
                                        last_modified,
                                        track_info.artist,
                                        track_info.album_artist,
                                        track_info.title,
                                        track_info.track_number,
                                        track_info.comment,
                                        track_info.tag_types);
}

void AudioLibrary::removeTrack(AudioLibraryTrack* track)
{
    {
        auto& tracks = track->_album->_tracks;

        auto new_end = std::remove(tracks.begin(), tracks.end(), track);
        tracks.erase(new_end, tracks.end());

        if(tracks.empty())
        {
            _album_map.erase(track->_album->_key);
            track->_album = nullptr;
        }

        _filepath_to_track_map.erase(track->_filepath);
    }
}

void AudioLibrary::removeAlbum(AudioLibraryAlbum* album)
{
    for(AudioLibraryTrack* track : album->_tracks)
    {
        _filepath_to_track_map.erase(track->_filepath);
    }

    _album_map.erase(album->_key);
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

template<class CONDITION>
std::vector<AudioLibraryAlbum*> getAlbumsIf(const std::map<AudioLibraryAlbumKey, std::unique_ptr<AudioLibraryAlbum>>& album_map,
                                            CONDITION condition)
{
    std::vector<AudioLibraryAlbum*> result;

    for(const auto& album : album_map)
    {
        if(condition(album.second.get()))
        {
            result.push_back(album.second.get());
        }
    }

    return result;
}

std::vector<AudioLibraryAlbum*> AudioLibrary::getAlbums() const
{
    return getAlbumsIf(_album_map, [](AudioLibraryAlbum* album){
        return true;
    });
}

void AudioLibrary::forEachAlbum(const std::function<void(AudioLibraryAlbum* album)>& func) const
{
    for (const auto& album : _album_map)
        func(album.second.get());
}

AudioLibraryAlbum* AudioLibrary::getAlbum(const AudioLibraryAlbumKey& key) const
{
    auto it = _album_map.find(key);
    if (it != _album_map.end())
        return it->second.get();

    return nullptr;
}

void AudioLibrary::cleanupTracksOutsideTheseDirectories(const QStringList& paths)
{
    for (auto it = _filepath_to_track_map.begin(), end = _filepath_to_track_map.end(); it != end;)
    {
        bool is_path_outside = true;
        for (const QString& path : paths)
        {
            if (it->first.startsWith(path))
            {
                is_path_outside = false;
                break;
            }
        }

        if (is_path_outside || !QFileInfo::exists(it->first))
        {
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

void AudioLibrary::save(QDataStream& s)
{
    s << qint32(2); // version

    s << quint64(_album_map.size());

    for (const auto& i : _album_map)
    {
        s << i.second->_key;
        s << i.second->_cover;

        s << quint64(i.second->_tracks.size());

        for(const AudioLibraryTrack* track : i.second->_tracks)
        {
            s << track->_filepath;
            s << track->_last_modified;
            s << track->_artist;
            s << track->_album_artist;
            s << track->_title;
            s << qint32(track->_track_number);
            s << track->_comment;
            s << track->_tag_types;
        }
    }
}

void AudioLibrary::load(QDataStream& s)
{
    _album_map.clear();
    _filepath_to_track_map.clear();

    qint32 version;
    s >> version;
    if (version != 2)
        return;

    quint64 num_albums;
    s >> num_albums;

    for (quint64 i = 0; i < num_albums; ++i)
    {
        AudioLibraryAlbumKey key;
        QByteArray cover;

        s >> key;
        s >> cover;

        AudioLibraryAlbum* album = addAlbum(key, cover);

        quint64 num_tracks;
        s >> num_tracks;

        for(quint64 ti = 0; ti < num_tracks; ++ti)
        {
            QString filepath;
            QDateTime last_modified;
            QString artist;
            QString album_artist;
            QString title;
            qint32 track_number;
            QString comment;
            QString tag_types;

            s >> filepath;
            s >> last_modified;
            s >> artist;
            s >> album_artist;
            s >> title;
            s >> track_number;
            s >> comment;
            s >> tag_types;

            addTrack(album, filepath, last_modified, artist, album_artist, title, track_number, comment, tag_types);
        }
    }
}

AudioLibraryAlbum* AudioLibrary::addAlbum(const AudioLibraryAlbumKey& album_key, const QByteArray& cover)
{
    auto it = _album_map.find(album_key);
    if (it == _album_map.end())
    {
        it = _album_map.insert(make_pair(album_key, std::unique_ptr<AudioLibraryAlbum>(new AudioLibraryAlbum(album_key, cover)))).first;
    }

    return it->second.get();
}

AudioLibraryTrack* AudioLibrary::addTrack(AudioLibraryAlbum* album,
    const QString& filepath,
    const QDateTime& last_modified,
    const QString& artist,
    const QString& album_artist,
    const QString& title,
    int track_number,
    const QString& comment,
    const QString& tag_types)
{
    auto it = _filepath_to_track_map.find(filepath);
    if (it != _filepath_to_track_map.end())
    {
        // file already added, shouldn't happen
        assert(false);
    }

    it = _filepath_to_track_map.insert(make_pair(filepath, std::unique_ptr<AudioLibraryTrack>(new AudioLibraryTrack(album,
        filepath, last_modified, artist, album_artist, title, track_number, comment, tag_types)))).first;
    album->_tracks.push_back(it->second.get());

    return it->second.get();
}