// SPDX-License-Identifier: GPL-2.0-only
#include "AudioLibraryView.h"
#include "AudioLibraryModel.h"

#include <unordered_set>
#include <stdexcept>

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
            for (const AudioLibraryTrack* track : album->getTracks())
                model->addTrackItem(track);
        }
        break;
        case AudioLibraryView::DisplayMode::ARTISTS:
        case AudioLibraryView::DisplayMode::YEARS:
        case AudioLibraryView::DisplayMode::GENRES:
            break;
        }
    }

    struct AudioLibraryArtistGroupData
    {
        const AudioLibraryAlbum* showcase_album = nullptr;
        std::unordered_set<const AudioLibraryAlbum*> albums;
        int num_tracks = 0;
    };

    struct AudioLibraryGroupData
    {
        const AudioLibraryAlbum* showcase_album = nullptr;
        int num_albums = 0;
        int num_tracks = 0;
    };

    void addTrackToArtistGroup(const QString& artist, const AudioLibraryTrack* track,
        std::unordered_map<QString, AudioLibraryArtistGroupData>& displayed_groups)
    {
        AudioLibraryArtistGroupData& group_data = displayed_groups[artist];

        if (!group_data.showcase_album ||
            group_data.showcase_album->getCover().isEmpty())
        {
            group_data.showcase_album = track->getAlbum();
        }

        group_data.albums.insert(track->getAlbum());
        ++group_data.num_tracks;
    }

    template<class GROUPED_TYPE>
    void addAlbumToGroup(GROUPED_TYPE group, const AudioLibraryAlbum* album,
        std::unordered_map<GROUPED_TYPE, AudioLibraryGroupData>& displayed_groups)
    {
        AudioLibraryGroupData& group_data = displayed_groups[group];

        if (!group_data.showcase_album ||
            group_data.showcase_album->getCover().isEmpty())
        {
            group_data.showcase_album = album;
        }

        ++group_data.num_albums;
        group_data.num_tracks += static_cast<int>(album->getTracks().size());
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
        for (const QString& word : filter.split(' ',
#if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
            Qt::SkipEmptyParts
#else
            QString::SkipEmptyParts
#endif
            ))
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
            result.push_back(QObject::tr("not \"%1\"").arg(word));

        return QString("%1 (%2)").arg(view_name, result.join(", "));
    }

}

//=============================================================================

QString AudioLibraryView::getColumnFriendlyName(Column column, DisplayMode mode)
{
    switch (column)
    {
    case ZERO:
        switch (mode)
        {
        case DisplayMode::ARTISTS:
            return getColumnFriendlyName(ARTIST, mode);
        case DisplayMode::YEARS:
            return getColumnFriendlyName(YEAR, mode);
        case DisplayMode::GENRES:
            return getColumnFriendlyName(GENRE, mode);
        case DisplayMode::ALBUMS:
        case DisplayMode::TRACKS:
            return QObject::tr("Name");
        }
        break;
    case NUMBER_OF_ALBUMS:
        return QObject::tr("Number of albums");
    case ARTIST:
        return QObject::tr("Artist");
    case ALBUM:
        return QObject::tr("Album");
    case YEAR:
        return QObject::tr("Year");
    case GENRE:
        return QObject::tr("Genre");
    case COVER_CHECKSUM:
        return QObject::tr("Cover checksum");
    case COVER_TYPE:
        return QObject::tr("Cover type");
    case COVER_WIDTH:
        return QObject::tr("Cover width");
    case COVER_HEIGHT:
        return QObject::tr("Cover height");
    case COVER_DATASIZE:
        return QObject::tr("Cover size");
    case NUMBER_OF_TRACKS:
        return QObject::tr("Number of tracks");
    case TITLE:
        return QObject::tr("Title");
    case TRACK_NUMBER:
        return QObject::tr("Track number");
    case DISC_NUMBER:
        return QObject::tr("Disc number");
    case ALBUM_ARTIST:
        return QObject::tr("Album Artist");
    case COMMENT:
        return QObject::tr("Comment");
    case PATH:
        return QObject::tr("Path");
    case DATE_MODIFIED:
        return QObject::tr("Date modified");
    case FILE_SIZE:
        return QObject::tr("File size");
    case TAG_TYPES:
        return QObject::tr("Tag types");
    case LENGTH_SECONDS:
        return QObject::tr("Length");
    case CHANNELS:
        return QObject::tr("Channels");
    case BITRATE_KBS:
        return QObject::tr("Bit Rate");
    case SAMPLERATE_HZ:
        return QObject::tr("Sample Rate");

    case NUMBER_OF_COLUMNS:
        break;
    }

    return QString();
}

std::vector<std::pair<AudioLibraryView::Column, QString>> AudioLibraryView::columnToStringMapping()
{
    std::vector<std::pair<Column, QString>> result;
    result.emplace_back(ZERO, "zero_column");
    result.emplace_back(NUMBER_OF_ALBUMS, "number_of_albums");
    result.emplace_back(ARTIST, "artist");
    result.emplace_back(ALBUM, "album");
    result.emplace_back(YEAR, "year");
    result.emplace_back(GENRE, "genre");
    result.emplace_back(COVER_CHECKSUM, "cover_checksum");
    result.emplace_back(COVER_TYPE, "cover_type");
    result.emplace_back(COVER_WIDTH, "cover_width");
    result.emplace_back(COVER_HEIGHT, "cover_height");
    result.emplace_back(COVER_DATASIZE, "cover_datasize");
    result.emplace_back(NUMBER_OF_TRACKS, "number_of_tracks");
    result.emplace_back(TITLE, "title");
    result.emplace_back(TRACK_NUMBER, "track_number");
    result.emplace_back(DISC_NUMBER, "disc_number");
    result.emplace_back(ALBUM_ARTIST, "album_artist");
    result.emplace_back(COMMENT, "comment");
    result.emplace_back(PATH, "path");
    result.emplace_back(DATE_MODIFIED, "date_modified");
    result.emplace_back(FILE_SIZE, "file_size");
    result.emplace_back(TAG_TYPES, "tag_types");
    result.emplace_back(LENGTH_SECONDS, "length");
    result.emplace_back(CHANNELS, "channels");
    result.emplace_back(BITRATE_KBS, "bit_rate");
    result.emplace_back(SAMPLERATE_HZ, "sample_rate");
    return result;
}

QString AudioLibraryView::getColumnId(Column column)
{
    for (const auto& i : columnToStringMapping())
        if (i.first == column)
            return i.second;

    // should never happen as long as columnToStringMapping() is complete

    return QString();
}

std::unique_ptr<AudioLibraryView::Column> AudioLibraryView::getColumnFromId(const QString& column_id)
{
    for (const auto& i : columnToStringMapping())
        if (i.second == column_id)
            return std::make_unique<Column>(i.first);

    return std::unique_ptr<Column>();
}

QString AudioLibraryView::getDisplayModeFriendlyName(DisplayMode mode)
{
    switch (mode)
    {
    case DisplayMode::ARTISTS:
        return QObject::tr("Artists");
    case DisplayMode::ALBUMS:
        return QObject::tr("Albums");
    case DisplayMode::TRACKS:
        return QObject::tr("Tracks");
    case DisplayMode::YEARS:
        return QObject::tr("Years");
    case DisplayMode::GENRES:
        return QObject::tr("Genres");
    }

    return QString();
}

std::vector<std::pair<AudioLibraryView::DisplayMode, QString>> AudioLibraryView::displayModeToStringMapping()
{
    std::vector<std::pair<DisplayMode, QString>> result;
    result.emplace_back(DisplayMode::ARTISTS, "artists");
    result.emplace_back(DisplayMode::ALBUMS, "albums");
    result.emplace_back(DisplayMode::TRACKS, "tracks");
    result.emplace_back(DisplayMode::YEARS, "years");
    result.emplace_back(DisplayMode::GENRES, "genres");
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
            AudioLibraryView::DATE_MODIFIED,
            AudioLibraryView::FILE_SIZE,
            AudioLibraryView::TAG_TYPES,
            AudioLibraryView::LENGTH_SECONDS,
            AudioLibraryView::CHANNELS,
            AudioLibraryView::BITRATE_KBS,
            AudioLibraryView::SAMPLERATE_HZ,
        };
    }

    return std::vector<AudioLibraryView::Column>();
}

/**
* Groups of albums, aggregated over e.g. artist, year, genre
*/
bool AudioLibraryView::isGroupDisplayMode(DisplayMode mode)
{
    switch (mode)
    {
    case DisplayMode::ARTISTS:
    case DisplayMode::YEARS:
    case DisplayMode::GENRES:
        return true;
    case DisplayMode::ALBUMS:
    case DisplayMode::TRACKS:
        break;
    }

    return false;
}

const ResolveToTracksIF* AudioLibraryView::getResolveToTracksIF() const
{
    return nullptr;
}

//=============================================================================

AudioLibraryViewAllArtists::AudioLibraryViewAllArtists(QString filter)
    : _filter(filter)
{}

std::unique_ptr<AudioLibraryView> AudioLibraryViewAllArtists::clone() const
{
    return std::make_unique<AudioLibraryViewAllArtists>(*this);
}

QString AudioLibraryViewAllArtists::getDisplayName() const
{
    return FilterHandler(_filter).formatFilterString(QObject::tr("Artists"));
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

    std::unordered_map<QString, AudioLibraryArtistGroupData> displayed_groups;

    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        for (const AudioLibraryTrack* track : album->getTracks())
        {
            // always add an item for artist, even if this field is empty

            if (filter_handler.checkText(track->getArtist()))
            {
                addTrackToArtistGroup(track->getArtist(), track, displayed_groups);
            }

            // if the album has an album artist, add an extra item for this field

            if (!track->getAlbumArtist().isEmpty() &&
                track->getArtist() != track->getAlbumArtist() &&
                filter_handler.checkText(track->getAlbumArtist()))
            {
                addTrackToArtistGroup(track->getAlbumArtist(), track, displayed_groups);
            }
        }
    }

    for (const auto& group : displayed_groups)
    {
        model->addGroupItem(group.first, group.second.showcase_album, static_cast<int>(group.second.albums.size()), group.second.num_tracks, [group](){
            return std::make_unique<AudioLibraryViewArtist>(group.first);
        });
    }
}

QString AudioLibraryViewAllArtists::getId() const
{
    return QString("%1, %2").arg(getBaseId()).arg(_filter);
}

QString AudioLibraryViewAllArtists::getBaseId()
{
    return "AudioLibraryViewAllArtists";
}

//=============================================================================

AudioLibraryViewAllAlbums::AudioLibraryViewAllAlbums(QString filter)
    : _filter(filter)
{}

std::unique_ptr<AudioLibraryView> AudioLibraryViewAllAlbums::clone() const
{
    return std::make_unique<AudioLibraryViewAllAlbums>(*this);
}

QString AudioLibraryViewAllAlbums::getDisplayName() const
{
    return FilterHandler(_filter).formatFilterString(QObject::tr("Albums"));
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
        if(filter_handler.checkText(album->getKey().getAlbum()))
            model->addAlbumItem(album);
    }
}

QString AudioLibraryViewAllAlbums::getId() const
{
    return QString("%1, %2").arg(getBaseId()).arg(_filter);
}

QString AudioLibraryViewAllAlbums::getBaseId()
{
    return "AudioLibraryViewAllAlbums";
}

//=============================================================================

AudioLibraryViewAllTracks::AudioLibraryViewAllTracks(QString filter)
    : _filter(filter)
{}

std::unique_ptr<AudioLibraryView> AudioLibraryViewAllTracks::clone() const
{
    return std::make_unique<AudioLibraryViewAllTracks>(*this);
}

QString AudioLibraryViewAllTracks::getDisplayName() const
{
    return FilterHandler(_filter).formatFilterString(QObject::tr("Tracks"));
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
        for (const AudioLibraryTrack* track : album->getTracks())
        {
            if(filter_handler.checkText(track->getTitle()))
                model->addTrackItem(track);
        }
    }
}

QString AudioLibraryViewAllTracks::getId() const
{
    return QString("%1, %2").arg(getBaseId()).arg(_filter);
}

QString AudioLibraryViewAllTracks::getBaseId()
{
    return "AudioLibraryViewAllTracks";
}

//=============================================================================

std::unique_ptr<AudioLibraryView> AudioLibraryViewAllYears::clone() const
{
    return std::make_unique<AudioLibraryViewAllYears>(*this);
}

QString AudioLibraryViewAllYears::getDisplayName() const
{
    return QObject::tr("Years");
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
        addAlbumToGroup(album->getKey().getYear(), album, displayed_groups);
    }

    for (const auto& group : displayed_groups)
    {
        model->addGroupItem(QString::number(group.first), group.second.showcase_album, group.second.num_albums, group.second.num_tracks, [group]() {
            return std::make_unique<AudioLibraryViewYear>(group.first);
        });
    }
}

QString AudioLibraryViewAllYears::getId() const
{
    return getBaseId();
}

QString AudioLibraryViewAllYears::getBaseId()
{
    return "AudioLibraryViewAllYears";
}

//=============================================================================

AudioLibraryViewAllGenres::AudioLibraryViewAllGenres(const QString& filter)
    : _filter(filter)
{
}

std::unique_ptr<AudioLibraryView> AudioLibraryViewAllGenres::clone() const
{
    return std::make_unique<AudioLibraryViewAllGenres>(*this);
}

QString AudioLibraryViewAllGenres::getDisplayName() const
{
    return FilterHandler(_filter).formatFilterString(QObject::tr("Genres"));
}

std::vector<AudioLibraryView::DisplayMode> AudioLibraryViewAllGenres::getSupportedModes() const
{
    if(_filter.isEmpty())
        return { DisplayMode::GENRES };
    else
        return { DisplayMode::GENRES, DisplayMode::ARTISTS, DisplayMode::ALBUMS };
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
            if (filter_handler.checkText(album->getKey().getGenre()))
            {
                addAlbumToGroup(album->getKey().getGenre(), album, displayed_groups);
            }
        }

        for (const auto& group : displayed_groups)
        {
            model->addGroupItem(group.first, group.second.showcase_album, group.second.num_albums, group.second.num_tracks, [group]() {
                return std::make_unique<AudioLibraryViewGenre>(group.first);
            });
        }
    }
    else if (display_mode == DisplayMode::ALBUMS)
    {
        for (const AudioLibraryAlbum* album : library.getAlbums())
        {
            if (filter_handler.checkText(album->getKey().getGenre()))
            {
                model->addAlbumItem(album);
            }
        }
    }
    else if (display_mode == DisplayMode::ARTISTS)
    {
        // collect all artists that have released at least one album of the genre

        std::unordered_map<QString, AudioLibraryGroupData> displayed_groups;

        for (const AudioLibraryAlbum* album : library.getAlbums())
        {
            if (filter_handler.checkText(album->getKey().getGenre()))
            {
                addAlbumToGroup(album->getKey().getArtist(), album, displayed_groups);
            }
        }

        for (const auto& group : displayed_groups)
        {
            model->addGroupItem(group.first, group.second.showcase_album, group.second.num_albums, group.second.num_tracks, [group]() {
                return std::make_unique<AudioLibraryViewArtist>(group.first);
            });
        }
    }
}

QString AudioLibraryViewAllGenres::getId() const
{
    return QString("%1, %2").arg(getBaseId()).arg(_filter);
}

QString AudioLibraryViewAllGenres::getBaseId()
{
    return "AudioLibraryViewAllGenres";
}

//=============================================================================

AudioLibraryViewArtist::AudioLibraryViewArtist(const QString& artist)
    : _artist(artist)
{
}

std::unique_ptr<AudioLibraryView> AudioLibraryViewArtist::clone() const
{
    return std::make_unique<AudioLibraryViewArtist>(*this);
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
        for (const AudioLibraryTrack* track : album->getTracks())
        {
            bool album_row_created = false;

            if (track->getArtist() == _artist ||
                (!track->getAlbumArtist().isEmpty() && track->getAlbumArtist() == _artist))
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
                case AudioLibraryView::DisplayMode::ARTISTS:
                case AudioLibraryView::DisplayMode::YEARS:
                case AudioLibraryView::DisplayMode::GENRES:
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
        for (const AudioLibraryTrack* track : album->getTracks())
        {
            if (track->getArtist() == _artist ||
                (!track->getAlbumArtist().isEmpty() && track->getAlbumArtist() == _artist))
            {
                tracks.push_back(track);
            }
        }
    }
}

const ResolveToTracksIF* AudioLibraryViewArtist::getResolveToTracksIF() const
{
    return this;
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

std::unique_ptr<AudioLibraryView> AudioLibraryViewAlbum::clone() const
{
    return std::make_unique<AudioLibraryViewAlbum>(*this);
}

QString AudioLibraryViewAlbum::getDisplayName() const
{
    return _key.getAlbum();
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
        for (const AudioLibraryTrack* track : album->getTracks())
        {
            model->addTrackItem(track);
        }
    }
}

void AudioLibraryViewAlbum::resolveToTracks(const AudioLibrary& library, std::vector<const AudioLibraryTrack*>& tracks) const
{
    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        if (album->getKey() == _key)
        {
            tracks.insert(tracks.end(), album->getTracks().begin(), album->getTracks().end());
        }
    }
}

const ResolveToTracksIF* AudioLibraryViewAlbum::getResolveToTracksIF() const
{
    return this;
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

std::unique_ptr<AudioLibraryView> AudioLibraryViewYear::clone() const
{
    return std::make_unique<AudioLibraryViewYear>(*this);
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
        if (album->getKey().getYear() == _year)
        {
            createAlbumOrTrackRow(album, display_mode, model);
        }
    }
}

void AudioLibraryViewYear::resolveToTracks(const AudioLibrary& library, std::vector<const AudioLibraryTrack*>& tracks) const
{
    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        if (album->getKey().getYear() == _year)
        {
            tracks.insert(tracks.end(), album->getTracks().begin(), album->getTracks().end());
        }
    }
}

const ResolveToTracksIF* AudioLibraryViewYear::getResolveToTracksIF() const
{
    return this;
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

std::unique_ptr<AudioLibraryView> AudioLibraryViewGenre::clone() const
{
    return std::make_unique<AudioLibraryViewGenre>(*this);
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
        if (album->getKey().getGenre() == _genre)
        {
            createAlbumOrTrackRow(album, display_mode, model);
        }
    }
}

void AudioLibraryViewGenre::resolveToTracks(const AudioLibrary& library, std::vector<const AudioLibraryTrack*>& tracks) const
{
    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        if (album->getKey().getGenre() == _genre)
        {
            tracks.insert(tracks.end(), album->getTracks().begin(), album->getTracks().end());
        }
    }
}

const ResolveToTracksIF* AudioLibraryViewGenre::getResolveToTracksIF() const
{
    return this;
}

QString AudioLibraryViewGenre::getId() const
{
    return QString("AudioLibraryViewGenre,%1").arg(_genre);
}

//=============================================================================

std::unique_ptr<AudioLibraryView> AudioLibraryViewDuplicateAlbums::clone() const
{
    return std::make_unique<AudioLibraryViewDuplicateAlbums>(*this);
}

QString AudioLibraryViewDuplicateAlbums::getDisplayName() const
{
    return QObject::tr("Badly tagged albums");
}

std::vector<AudioLibraryView::DisplayMode> AudioLibraryViewDuplicateAlbums::getSupportedModes() const
{
    return{ DisplayMode::ALBUMS };
}

namespace {
    struct DuplicateAlbumKey
    {
        DuplicateAlbumKey(const AudioLibraryAlbum* album)
            : _artist(album->getKey().getArtist())
            , _album(album->getKey().getAlbum())
        {}

        std::strong_ordering operator<=>(const DuplicateAlbumKey&) const = default;

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