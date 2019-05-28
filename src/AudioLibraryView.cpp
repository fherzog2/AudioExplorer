// SPDX-License-Identifier: GPL-2.0-only
#include "AudioLibraryView.h"
#include "AudioLibraryModel.h"

#include <unordered_set>

namespace {

    void createAlbumOrTrackRow(const AudioLibraryAlbum* album,
        AudioLibraryView::DisplayMode display_mode,
        AudioLibraryModel* model)
    {
        switch (display_mode)
        {
        case AudioLibraryView::DisplayMode::ALBUMS:
            model->addAlbumItem(album);
            break;
        case AudioLibraryView::DisplayMode::TRACKS:
        {
            for (const AudioLibraryTrack* track : album->_tracks)
                model->addTrackItem(track);
        }
        break;
        }
    }

    struct AudioLibraryGroupData
    {
        const AudioLibraryAlbum* showcase_album = nullptr;
        int num_albums = 0;
        int num_tracks = 0;

        QString getId() const
        {
            QLatin1Char sep(',');

            return showcase_album->_key._album + sep +
                QString::number(showcase_album->_key._cover_checksum) + sep +
                QString::number(num_albums) + sep +
                QString::number(num_tracks) + QLatin1Char(')');
        }
    };

    template<class GROUPED_TYPE>
    void addAlbumToGroup(GROUPED_TYPE group, const AudioLibraryAlbum* album,
        std::unordered_map<GROUPED_TYPE, AudioLibraryGroupData>& displayed_groups)
    {
        AudioLibraryGroupData& group_data = displayed_groups[group];

        if (!group_data.showcase_album ||
            group_data.showcase_album->getCoverPixmap().isNull())
        {
            group_data.showcase_album = album;
        }

        ++group_data.num_albums;
        group_data.num_tracks += static_cast<int>(album->_tracks.size());
    }

    //=============================================================================

    /**
    * Implements a filter which only lets through text that contains all the specified words.
    * Word can also be forbidden by adding an exclamation mark in front (negative filter).
    */
    class FilterHandler
    {
    public:
        FilterHandler(const QString& filter);

        bool checkText(const QString& text) const;
        QString formatFilterString(const QString& view_name) const;

    private:
        std::vector<QString> _words;
        std::vector<QString> _forbidden_words;
    };

    FilterHandler::FilterHandler(const QString& filter)
    {
        for (const QString& word : filter.split(' ', QString::SkipEmptyParts))
        {
            if (word.startsWith('!'))
            {
                if (word.length() > 1)
                {
                    _forbidden_words.push_back(word.right(word.length() - 1));
                }
            }
            else
            {
                _words.push_back(word);
            }
        }
    }

    bool FilterHandler::checkText(const QString& text) const
    {
        for (const QString& word : _forbidden_words)
            if (text.contains(word, Qt::CaseInsensitive))
                return false;

        for (const QString& word : _words)
            if (!text.contains(word, Qt::CaseInsensitive))
                return false;

        return true;
    }

    QString FilterHandler::formatFilterString(const QString& view_name) const
    {
        if (_words.empty() && _forbidden_words.empty())
            return view_name;

        QStringList result;

        for (const QString& word : _words)
            result.push_back('\"' + word + '\"');

        for (const QString& word : _forbidden_words)
            result.push_back("not \"" + word + '\"');

        return QString("%1 (%2)").arg(view_name, result.join(", "));
    }

}

//=============================================================================

AudioLibraryView::~AudioLibraryView()
{
}

QString AudioLibraryView::getColumnFriendlyName(Column column)
{
    switch (column)
    {
    case ZERO:
        return "";
    case NUMBER_OF_ALBUMS:
        return "Number of albums";
    case ARTIST:
        return "Artist";
    case ALBUM:
        return "Album";
    case YEAR:
        return "Year";
    case GENRE:
        return "Genre";
    case COVER_CHECKSUM:
        return "Cover checksum";
    case COVER_TYPE:
        return "Cover type";
    case COVER_WIDTH:
        return "Cover width";
    case COVER_HEIGHT:
        return "Cover height";
    case COVER_DATASIZE:
        return "Cover size";
    case NUMBER_OF_TRACKS:
        return "Number of tracks";
    case TITLE:
        return "Title";
    case TRACK_NUMBER:
        return "Track number";
    case DISC_NUMBER:
        return "Disc number";
    case ALBUM_ARTIST:
        return "Album Artist";
    case COMMENT:
        return "Comment";
    case PATH:
        return "Path";
    case TAG_TYPES:
        return "Tag types";
    case LENGTH_SECONDS:
        return "Length";
    case CHANNELS:
        return "Channels";
    case BITRATE_KBS:
        return "Bit Rate";
    case SAMPLERATE_HZ:
        return "Sample Rate";
    }

    return QString();
}

std::vector<std::pair<AudioLibraryView::Column, QString>> AudioLibraryView::columnToStringMapping()
{
    std::vector<std::pair<Column, QString>> result;
    result.push_back(std::make_pair(ZERO, "zero_column"));
    result.push_back(std::make_pair(NUMBER_OF_ALBUMS, "number_of_albums"));
    result.push_back(std::make_pair(ARTIST, "artist"));
    result.push_back(std::make_pair(ALBUM, "album"));
    result.push_back(std::make_pair(YEAR, "year"));
    result.push_back(std::make_pair(GENRE, "genre"));
    result.push_back(std::make_pair(COVER_CHECKSUM, "cover_checksum"));
    result.push_back(std::make_pair(COVER_TYPE, "cover_type"));
    result.push_back(std::make_pair(COVER_WIDTH, "cover_width"));
    result.push_back(std::make_pair(COVER_HEIGHT, "cover_height"));
    result.push_back(std::make_pair(COVER_DATASIZE, "cover_datasize"));
    result.push_back(std::make_pair(NUMBER_OF_TRACKS, "number_of_tracks"));
    result.push_back(std::make_pair(TITLE, "title"));
    result.push_back(std::make_pair(TRACK_NUMBER, "track_number"));
    result.push_back(std::make_pair(DISC_NUMBER, "disc_number"));
    result.push_back(std::make_pair(ALBUM_ARTIST, "album_artist"));
    result.push_back(std::make_pair(COMMENT, "comment"));
    result.push_back(std::make_pair(PATH, "path"));
    result.push_back(std::make_pair(TAG_TYPES, "tag_types"));
    result.push_back(std::make_pair(LENGTH_SECONDS, "length"));
    result.push_back(std::make_pair(CHANNELS, "channels"));
    result.push_back(std::make_pair(BITRATE_KBS, "bit_rate"));
    result.push_back(std::make_pair(SAMPLERATE_HZ, "sample_rate"));
    return result;
}

QString AudioLibraryView::getColumnId(Column column)
{
    for (const auto& i : columnToStringMapping())
        if (i.first == column)
            return i.second;

    // should never happen as long as columnToStringMapping() is complete

    return QString::null;
}

std::unique_ptr<AudioLibraryView::Column> AudioLibraryView::getColumnFromId(const QString& column_id)
{
    for (const auto& i : columnToStringMapping())
        if (i.second == column_id)
            return std::unique_ptr<Column>(new Column(i.first));

    return std::unique_ptr<Column>();
}

QString AudioLibraryView::getDisplayModeFriendlyName(DisplayMode mode)
{
    switch (mode)
    {
    case DisplayMode::ARTISTS:
        return "Artists";
    case DisplayMode::ALBUMS:
        return "Albums";
    case DisplayMode::TRACKS:
        return "Tracks";
    case DisplayMode::YEARS:
        return "Years";
    case DisplayMode::GENRES:
        return "Genres";
    }

    return QString();
}

std::vector<std::pair<AudioLibraryView::DisplayMode, QString>> AudioLibraryView::displayModeToStringMapping()
{
    std::vector<std::pair<DisplayMode, QString>> result;
    result.push_back(std::make_pair(DisplayMode::ARTISTS, "artists"));
    result.push_back(std::make_pair(DisplayMode::ALBUMS, "albums"));
    result.push_back(std::make_pair(DisplayMode::TRACKS, "tracks"));
    result.push_back(std::make_pair(DisplayMode::YEARS, "years"));
    result.push_back(std::make_pair(DisplayMode::GENRES, "genres"));
    return result;
}

std::vector<AudioLibraryView::Column> AudioLibraryView::getColumnsForDisplayMode(DisplayMode mode)
{
    switch (mode)
    {
    case DisplayMode::ARTISTS:
    case DisplayMode::YEARS:
    case DisplayMode::GENRES:
        return
        {
            AudioLibraryView::NUMBER_OF_ALBUMS,
            AudioLibraryView::NUMBER_OF_TRACKS,
        };
    case DisplayMode::ALBUMS:
        return
        {
            AudioLibraryView::ARTIST,
            AudioLibraryView::ALBUM,
            AudioLibraryView::YEAR,
            AudioLibraryView::GENRE,
            AudioLibraryView::COVER_CHECKSUM,
            AudioLibraryView::COVER_TYPE,
            AudioLibraryView::COVER_WIDTH,
            AudioLibraryView::COVER_HEIGHT,
            AudioLibraryView::COVER_DATASIZE,
            AudioLibraryView::NUMBER_OF_TRACKS,
            AudioLibraryView::LENGTH_SECONDS,
        };
    case DisplayMode::TRACKS:
        return
        {
            AudioLibraryView::ARTIST,
            AudioLibraryView::ALBUM,
            AudioLibraryView::YEAR,
            AudioLibraryView::GENRE,
            AudioLibraryView::COVER_CHECKSUM,
            AudioLibraryView::COVER_TYPE,
            AudioLibraryView::COVER_WIDTH,
            AudioLibraryView::COVER_HEIGHT,
            AudioLibraryView::COVER_DATASIZE,
            AudioLibraryView::TITLE,
            AudioLibraryView::TRACK_NUMBER,
            AudioLibraryView::DISC_NUMBER,
            AudioLibraryView::ALBUM_ARTIST,
            AudioLibraryView::COMMENT,
            AudioLibraryView::PATH,
            AudioLibraryView::TAG_TYPES,
            AudioLibraryView::LENGTH_SECONDS,
            AudioLibraryView::CHANNELS,
            AudioLibraryView::BITRATE_KBS,
            AudioLibraryView::SAMPLERATE_HZ,
        };
    }

    return std::vector<AudioLibraryView::Column>();
}

void AudioLibraryView::resolveToTracks(const AudioLibrary& /*library*/, std::vector<const AudioLibraryTrack*>& /*tracks*/) const
{
    throw std::runtime_error("not implemented");
}

//=============================================================================

AudioLibraryViewAllArtists::AudioLibraryViewAllArtists(QString filter)
    : _filter(filter)
{}

AudioLibraryView* AudioLibraryViewAllArtists::clone() const
{
    return new AudioLibraryViewAllArtists(*this);
}

QString AudioLibraryViewAllArtists::getDisplayName() const
{
    return FilterHandler(_filter).formatFilterString("Artists");
}

std::vector<AudioLibraryView::DisplayMode> AudioLibraryViewAllArtists::getSupportedModes() const
{
    return{ DisplayMode::ARTISTS };
}

void AudioLibraryViewAllArtists::createItems(const AudioLibrary& library,
    DisplayMode /*display_mode*/,
    AudioLibraryModel* model) const
{
    FilterHandler filter_handler(_filter);

    std::unordered_map<QString, AudioLibraryGroupData> displayed_groups;

    struct AlbumAdded
    {
        bool added_for_artist = false;
        bool added_for_album_artist = false;
    };

    // only add each album once for artist and album_artist

    std::unordered_map<const AudioLibraryAlbum*, AlbumAdded> albums_added;

    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        for (const AudioLibraryTrack* track : album->_tracks)
        {
            // always add an item for artist, even if this field is empty

            if (filter_handler.checkText(track->_artist) &&
                !albums_added[album].added_for_artist)
            {
                addAlbumToGroup(track->_artist, album, displayed_groups);
                albums_added[album].added_for_artist = true;
            }

            // if the album has an album artist, add an extra item for this field

            if (!track->_album_artist.isEmpty() &&
                filter_handler.checkText(track->_album_artist) &&
                !albums_added[album].added_for_album_artist)
            {
                addAlbumToGroup(track->_album_artist, album, displayed_groups);
                albums_added[album].added_for_album_artist = true;
            }
        }
    }

    for (const auto& group : displayed_groups)
    {
        QString id = QLatin1String("artist(") +
            group.first + QLatin1Char(',') +
            group.second.getId() + QLatin1Char(')');

        model->addItem(id, group.first, group.second.showcase_album->getCoverPixmap(), group.second.num_albums, group.second.num_tracks, [=](){
            return new AudioLibraryViewArtist(group.first);
        });
    }
}

QString AudioLibraryViewAllArtists::getId() const
{
    return QString("AudioLibraryViewAllArtists, %1").arg(_filter);
}

//=============================================================================

AudioLibraryViewAllAlbums::AudioLibraryViewAllAlbums(QString filter)
    : _filter(filter)
{}

AudioLibraryView* AudioLibraryViewAllAlbums::clone() const
{
    return new AudioLibraryViewAllAlbums(*this);
}

QString AudioLibraryViewAllAlbums::getDisplayName() const
{
    return FilterHandler(_filter).formatFilterString("Albums");
}

std::vector<AudioLibraryView::DisplayMode> AudioLibraryViewAllAlbums::getSupportedModes() const
{
    return{ DisplayMode::ALBUMS };
}

void AudioLibraryViewAllAlbums::createItems(const AudioLibrary& library,
    DisplayMode /*display_mode*/,
    AudioLibraryModel* model) const
{
    FilterHandler filter_handler(_filter);

    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        if(filter_handler.checkText(album->_key._album))
            model->addAlbumItem(album);
    }
}

QString AudioLibraryViewAllAlbums::getId() const
{
    return QString("AudioLibraryViewAllAlbums, %1").arg(_filter);
}

//=============================================================================

AudioLibraryViewAllTracks::AudioLibraryViewAllTracks(QString filter)
    : _filter(filter)
{}

AudioLibraryView* AudioLibraryViewAllTracks::clone() const
{
    return new AudioLibraryViewAllTracks(*this);
}

QString AudioLibraryViewAllTracks::getDisplayName() const
{
    return FilterHandler(_filter).formatFilterString("Tracks");
}

std::vector<AudioLibraryView::DisplayMode> AudioLibraryViewAllTracks::getSupportedModes() const
{
    return{ DisplayMode::TRACKS };
}

void AudioLibraryViewAllTracks::createItems(const AudioLibrary& library,
    DisplayMode /*display_mode*/,
    AudioLibraryModel* model) const
{
    FilterHandler filter_handler(_filter);

    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        for (const AudioLibraryTrack* track : album->_tracks)
        {
            if(filter_handler.checkText(track->_title))
                model->addTrackItem(track);
        }
    }
}

QString AudioLibraryViewAllTracks::getId() const
{
    return QString("AudioLibraryViewAllTracks, %1").arg(_filter);
}

//=============================================================================

AudioLibraryView* AudioLibraryViewAllYears::clone() const
{
    return new AudioLibraryViewAllYears(*this);
}

QString AudioLibraryViewAllYears::getDisplayName() const
{
    return "Years";
}

std::vector<AudioLibraryView::DisplayMode> AudioLibraryViewAllYears::getSupportedModes() const
{
    return{ DisplayMode::YEARS };
}

void AudioLibraryViewAllYears::createItems(const AudioLibrary& library,
    DisplayMode /*display_mode*/,
    AudioLibraryModel* model) const
{
    std::unordered_map<int, AudioLibraryGroupData> displayed_groups;

    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        addAlbumToGroup(album->_key._year, album, displayed_groups);
    }

    for (const auto& group : displayed_groups)
    {
        QString id = QLatin1String("year(") +
            QString::number(group.first) + QLatin1Char(',') +
            group.second.getId() + QLatin1Char(')');

        model->addItem(id, QString::number(group.first), group.second.showcase_album->getCoverPixmap(), group.second.num_albums, group.second.num_tracks, [=]() {
            return new AudioLibraryViewYear(group.first);
        });
    }
}

QString AudioLibraryViewAllYears::getId() const
{
    return QLatin1String("AudioLibraryViewAllYears");
}

//=============================================================================

AudioLibraryViewAllGenres::AudioLibraryViewAllGenres(const QString& filter)
    : _filter(filter)
{
}

AudioLibraryView* AudioLibraryViewAllGenres::clone() const
{
    return new AudioLibraryViewAllGenres(*this);
}

QString AudioLibraryViewAllGenres::getDisplayName() const
{
    return FilterHandler(_filter).formatFilterString("Genres");
}

std::vector<AudioLibraryView::DisplayMode> AudioLibraryViewAllGenres::getSupportedModes() const
{
    if(_filter.isEmpty())
        return { DisplayMode::GENRES };
    else
        return { DisplayMode::GENRES, DisplayMode::ALBUMS };
}

void AudioLibraryViewAllGenres::createItems(const AudioLibrary& library,
    DisplayMode display_mode,
    AudioLibraryModel* model) const
{
    FilterHandler filter_handler(_filter);

    if(display_mode == DisplayMode::GENRES)
    {
        std::unordered_map<QString, AudioLibraryGroupData> displayed_groups;

        for (const AudioLibraryAlbum* album : library.getAlbums())
        {
            if (filter_handler.checkText(album->_key._genre))
            {
                addAlbumToGroup(album->_key._genre, album, displayed_groups);
            }
        }

        for (const auto& group : displayed_groups)
        {
            QString id = QLatin1String("genre(") +
                group.first + QLatin1Char(',') +
                group.second.getId() + QLatin1Char(')');

            model->addItem(id, group.first, group.second.showcase_album->getCoverPixmap(), group.second.num_albums, group.second.num_tracks, [=]() {
                return new AudioLibraryViewGenre(group.first);
            });
        }
    }
    else if (display_mode == DisplayMode::ALBUMS)
    {
        for (const AudioLibraryAlbum* album : library.getAlbums())
        {
            if (filter_handler.checkText(album->_key._genre))
            {
                model->addAlbumItem(album);
            }
        }
    }
}

QString AudioLibraryViewAllGenres::getId() const
{
    return QString("AudioLibraryViewAllGenres, %1").arg(_filter);
}

//=============================================================================

AudioLibraryViewArtist::AudioLibraryViewArtist(const QString& artist)
    : _artist(artist)
{
}

AudioLibraryView* AudioLibraryViewArtist::clone() const
{
    return new AudioLibraryViewArtist(*this);
}

QString AudioLibraryViewArtist::getDisplayName() const
{
    return _artist;
}

std::vector<AudioLibraryView::DisplayMode> AudioLibraryViewArtist::getSupportedModes() const
{
    return{ DisplayMode::ALBUMS, DisplayMode::TRACKS };
}

void AudioLibraryViewArtist::createItems(const AudioLibrary& library,
    DisplayMode display_mode,
    AudioLibraryModel* model) const
{
    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        for (const AudioLibraryTrack* track : album->_tracks)
        {
            bool album_row_created = false;

            if (track->_artist == _artist ||
                (!track->_album_artist.isEmpty() && track->_album_artist == _artist))
            {
                switch (display_mode)
                {
                case DisplayMode::ALBUMS:
                    model->addAlbumItem(album);
                    album_row_created = true;
                    break;
                case DisplayMode::TRACKS:
                    model->addTrackItem(track);
                    break;
                }
            }

            if (album_row_created)
                break;
        }
    }
}

void AudioLibraryViewArtist::resolveToTracks(const AudioLibrary& library, std::vector<const AudioLibraryTrack*>& tracks) const
{
    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        for (const AudioLibraryTrack* track : album->_tracks)
        {
            if (track->_artist == _artist ||
                (!track->_album_artist.isEmpty() && track->_album_artist == _artist))
            {
                tracks.push_back(track);
            }
        }
    }
}

QString AudioLibraryViewArtist::getId() const
{
    return QString("AudioLibraryViewArtist,%1").arg(_artist);
}

//=============================================================================

AudioLibraryViewAlbum::AudioLibraryViewAlbum(const AudioLibraryAlbumKey& key)
    : _key(key)
{
}

AudioLibraryView* AudioLibraryViewAlbum::clone() const
{
    return new AudioLibraryViewAlbum(*this);
}

QString AudioLibraryViewAlbum::getDisplayName() const
{
    return _key._album;
}

std::vector<AudioLibraryView::DisplayMode> AudioLibraryViewAlbum::getSupportedModes() const
{
    return{ DisplayMode::TRACKS };
}

void AudioLibraryViewAlbum::createItems(const AudioLibrary& library,
    DisplayMode /*display_mode*/,
    AudioLibraryModel* model) const
{
    if (const AudioLibraryAlbum* album = library.getAlbum(_key))
    {
        for (const AudioLibraryTrack* track : album->_tracks)
        {
            model->addTrackItem(track);
        }
    }
}

void AudioLibraryViewAlbum::resolveToTracks(const AudioLibrary& library, std::vector<const AudioLibraryTrack*>& tracks) const
{
    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        if (album->_key == _key)
        {
            tracks.insert(tracks.end(), album->_tracks.begin(), album->_tracks.end());
        }
    }
}

QString AudioLibraryViewAlbum::getId() const
{
    return QLatin1String("AudioLibraryViewAlbum,") + _key.toString();
}

//=============================================================================

AudioLibraryViewYear::AudioLibraryViewYear(int year)
    : _year(year)
{
}

AudioLibraryView* AudioLibraryViewYear::clone() const
{
    return new AudioLibraryViewYear(*this);
}

QString AudioLibraryViewYear::getDisplayName() const
{
    return QString::number(_year);
}

std::vector<AudioLibraryView::DisplayMode> AudioLibraryViewYear::getSupportedModes() const
{
    return{ DisplayMode::ALBUMS, DisplayMode::TRACKS };
}

void AudioLibraryViewYear::createItems(const AudioLibrary& library,
    DisplayMode display_mode,
    AudioLibraryModel* model) const
{
    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        if (album->_key._year == _year)
        {
            createAlbumOrTrackRow(album, display_mode, model);
        }
    }
}

void AudioLibraryViewYear::resolveToTracks(const AudioLibrary& library, std::vector<const AudioLibraryTrack*>& tracks) const
{
    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        if (album->_key._year == _year)
        {
            tracks.insert(tracks.end(), album->_tracks.begin(), album->_tracks.end());
        }
    }
}

QString AudioLibraryViewYear::getId() const
{
    return QString("AudioLibraryViewYear,%1").arg(_year);
}

//=============================================================================

AudioLibraryViewGenre::AudioLibraryViewGenre(const QString& genre)
    : _genre(genre)
{
}

AudioLibraryView* AudioLibraryViewGenre::clone() const
{
    return new AudioLibraryViewGenre(*this);
}

QString AudioLibraryViewGenre::getDisplayName() const
{
    return _genre;
}

std::vector<AudioLibraryView::DisplayMode> AudioLibraryViewGenre::getSupportedModes() const
{
    return{ DisplayMode::ALBUMS, DisplayMode::TRACKS };
}

void AudioLibraryViewGenre::createItems(const AudioLibrary& library,
    DisplayMode display_mode,
    AudioLibraryModel* model) const
{
    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        if (album->_key._genre == _genre)
        {
            createAlbumOrTrackRow(album, display_mode, model);
        }
    }
}

void AudioLibraryViewGenre::resolveToTracks(const AudioLibrary& library, std::vector<const AudioLibraryTrack*>& tracks) const
{
    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        if (album->_key._genre == _genre)
        {
            tracks.insert(tracks.end(), album->_tracks.begin(), album->_tracks.end());
        }
    }
}

QString AudioLibraryViewGenre::getId() const
{
    return QString("AudioLibraryViewGenre,%1").arg(_genre);
}

//=============================================================================

AudioLibraryView* AudioLibraryViewDuplicateAlbums::clone() const
{
    return new AudioLibraryViewDuplicateAlbums(*this);
}

QString AudioLibraryViewDuplicateAlbums::getDisplayName() const
{
    return "Badly tagged albums";
}

std::vector<AudioLibraryView::DisplayMode> AudioLibraryViewDuplicateAlbums::getSupportedModes() const
{
    return{ DisplayMode::ALBUMS };
}

namespace {
    struct DuplicateAlbumKey
    {
        DuplicateAlbumKey(const AudioLibraryAlbum* album)
            : _artist(album->_key._artist)
            , _album(album->_key._album)
        {}

        bool operator<(const DuplicateAlbumKey& other) const
        {
            return std::tie(_artist, _album) <
                std::tie(other._artist, other._album);
        }

        QString _artist;
        QString _album;
    };
}

void AudioLibraryViewDuplicateAlbums::createItems(const AudioLibrary& library,
    DisplayMode /*display_mode*/,
    AudioLibraryModel* model) const
{
    std::map<DuplicateAlbumKey, const AudioLibraryAlbum*> first_album_occurence;
    std::unordered_set<const AudioLibraryAlbum*> duplicates;

    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        auto first_occurence = first_album_occurence.insert(std::make_pair(DuplicateAlbumKey(album), album));

        // an album of that name was already inserted
        if (!first_occurence.second)
        {
            duplicates.insert(first_occurence.first->second);
            duplicates.insert(album);
        }
    }

    for (const AudioLibraryAlbum* album : duplicates)
    {
        model->addAlbumItem(album);
    }
}

QString AudioLibraryViewDuplicateAlbums::getId() const
{
    return "AudioLibraryViewDuplicateAlbums";
}