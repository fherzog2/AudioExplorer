// SPDX-License-Identifier: GPL-2.0-only
#include "AudioLibraryView.h"
#include "AudioLibraryModel.h"

#include <unordered_set>

namespace {

    void createAlbumOrTrackRow(const AudioLibraryAlbum* album,
        const AudioLibraryView::DisplayMode* display_mode,
        AudioLibraryModel* model)
    {
        switch (*display_mode)
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

    template<class GROUPED_TYPE>
    void addAlbumToGroup(GROUPED_TYPE group, const AudioLibraryAlbum* album,
        std::unordered_map<GROUPED_TYPE, const AudioLibraryAlbum*>& displayed_groups)
    {
        auto it = displayed_groups.find(group);
        if (it != displayed_groups.end())
        {
            if (it->second->getCoverPixmap().isNull())
                it->second = album;
        }
        else
        {
            displayed_groups[group] = album;
        }
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
    }

    return QString();
}

std::vector<std::pair<AudioLibraryView::Column, QString>> AudioLibraryView::columnToStringMapping()
{
    std::vector<std::pair<Column, QString>> result;
    result.push_back(std::make_pair(ZERO, "zero_column"));
    result.push_back(std::make_pair(ARTIST, "artist"));
    result.push_back(std::make_pair(ALBUM, "album"));
    result.push_back(std::make_pair(YEAR, "year"));
    result.push_back(std::make_pair(GENRE, "genre"));
    result.push_back(std::make_pair(COVER_CHECKSUM, "cover_checksum"));
    result.push_back(std::make_pair(COVER_TYPE, "cover_type"));
    result.push_back(std::make_pair(TITLE, "title"));
    result.push_back(std::make_pair(TRACK_NUMBER, "track_number"));
    result.push_back(std::make_pair(DISC_NUMBER, "disc_number"));
    result.push_back(std::make_pair(ALBUM_ARTIST, "album_artist"));
    result.push_back(std::make_pair(COMMENT, "comment"));
    result.push_back(std::make_pair(PATH, "path"));
    result.push_back(std::make_pair(TAG_TYPES, "tag_types"));
    return result;
}

QString AudioLibraryView::getDisplayModeFriendlyName(DisplayMode mode)
{
    switch (mode)
    {
    case DisplayMode::ALBUMS:
        return "Albums";
    case DisplayMode::TRACKS:
        return "Tracks";
    }

    return QString();
}

std::vector<std::pair<AudioLibraryView::DisplayMode, QString>> AudioLibraryView::displayModeToStringMapping()
{
    std::vector<std::pair<DisplayMode, QString>> result;
    result.push_back(std::make_pair(DisplayMode::ALBUMS, "albums"));
    result.push_back(std::make_pair(DisplayMode::TRACKS, "tracks"));
    return result;
}

std::vector<AudioLibraryView::Column> AudioLibraryView::getColumnsForDisplayMode(DisplayMode mode)
{
    switch (mode)
    {
    case DisplayMode::ALBUMS:
        return
        {
            AudioLibraryView::ARTIST,
            AudioLibraryView::ALBUM,
            AudioLibraryView::YEAR,
            AudioLibraryView::GENRE,
            AudioLibraryView::COVER_CHECKSUM,
            AudioLibraryView::COVER_TYPE,
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
            AudioLibraryView::TITLE,
            AudioLibraryView::TRACK_NUMBER,
            AudioLibraryView::DISC_NUMBER,
            AudioLibraryView::ALBUM_ARTIST,
            AudioLibraryView::COMMENT,
            AudioLibraryView::PATH,
            AudioLibraryView::TAG_TYPES,
        };
    }

    return std::vector<AudioLibraryView::Column>();
}

void AudioLibraryView::resolveToTracks(const AudioLibrary& /*library*/, std::vector<const AudioLibraryTrack*>& /*tracks*/) const
{
    throw std::runtime_error("not implemented");
}

QString AudioLibraryView::getId() const
{
    throw std::runtime_error("not implemented");
}

//=============================================================================

AudioLibraryView* AudioLibraryViewAllArtists::clone() const
{
    return new AudioLibraryViewAllArtists(*this);
}

QString AudioLibraryViewAllArtists::getDisplayName() const
{
    return "Artists";
}

std::vector<AudioLibraryView::DisplayMode> AudioLibraryViewAllArtists::getSupportedModes() const
{
    return{};
}

void AudioLibraryViewAllArtists::createItems(const AudioLibrary& library,
    const DisplayMode* /*display_mode*/,
    AudioLibraryModel* model) const
{
    std::unordered_map<QString, const AudioLibraryAlbum*> displayed_groups;

    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        for (const AudioLibraryTrack* track : album->_tracks)
        {
            // always add an item for artist, even if this field is empty

            addAlbumToGroup(track->_artist, album, displayed_groups);

            // if the album has an album artist, add an extra item for this field

            if (!track->_album_artist.isEmpty())
            {
                addAlbumToGroup(track->_album_artist, album, displayed_groups);
            }
        }
    }

    for (const auto& group : displayed_groups)
    {
        QString id = QString("artist(%1,%2,%3)")
            .arg(group.first)
            .arg(group.second->_key._album)
            .arg(group.second->_key._cover_checksum);

        model->addItem(id, group.first, group.second->getCoverPixmap(), [=](){
            return new AudioLibraryViewArtist(group.first);
        });
    }
}

//=============================================================================

AudioLibraryView* AudioLibraryViewAllAlbums::clone() const
{
    return new AudioLibraryViewAllAlbums(*this);
}

QString AudioLibraryViewAllAlbums::getDisplayName() const
{
    return "Albums";
}

std::vector<AudioLibraryView::DisplayMode> AudioLibraryViewAllAlbums::getSupportedModes() const
{
    return{ DisplayMode::ALBUMS };
}

void AudioLibraryViewAllAlbums::createItems(const AudioLibrary& library,
    const DisplayMode* /*display_mode*/,
    AudioLibraryModel* model) const
{
    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        model->addAlbumItem(album);
    }
}

//=============================================================================

AudioLibraryView* AudioLibraryViewAllTracks::clone() const
{
    return new AudioLibraryViewAllTracks(*this);
}

QString AudioLibraryViewAllTracks::getDisplayName() const
{
    return "Tracks";
}

std::vector<AudioLibraryView::DisplayMode> AudioLibraryViewAllTracks::getSupportedModes() const
{
    return{ DisplayMode::TRACKS };
}

void AudioLibraryViewAllTracks::createItems(const AudioLibrary& library,
    const DisplayMode* /*display_mode*/,
    AudioLibraryModel* model) const
{
    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        for (const AudioLibraryTrack* track : album->_tracks)
        {
            model->addTrackItem(track);
        }
    }
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
    return{};
}

void AudioLibraryViewAllYears::createItems(const AudioLibrary& library,
    const DisplayMode* /*display_mode*/,
    AudioLibraryModel* model) const
{
    std::unordered_map<int, const AudioLibraryAlbum*> displayed_groups;

    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        addAlbumToGroup(album->_key._year, album, displayed_groups);
    }

    for (const auto& group : displayed_groups)
    {
        QString id = QString("year(%1,%2,%3)")
            .arg(group.first)
            .arg(group.second->_key._album)
            .arg(group.second->_key._cover_checksum);

        model->addItem(id, QString("%1").arg(group.first), group.second->getCoverPixmap(), [=]() {
            return new AudioLibraryViewYear(group.first);
        });
    }
}

//=============================================================================

AudioLibraryView* AudioLibraryViewAllGenres::clone() const
{
    return new AudioLibraryViewAllGenres(*this);
}

QString AudioLibraryViewAllGenres::getDisplayName() const
{
    return "Genres";
}

std::vector<AudioLibraryView::DisplayMode> AudioLibraryViewAllGenres::getSupportedModes() const
{
    return{};
}

void AudioLibraryViewAllGenres::createItems(const AudioLibrary& library,
    const DisplayMode* /*display_mode*/,
    AudioLibraryModel* model) const
{
    std::unordered_map<QString, const AudioLibraryAlbum*> displayed_groups;

    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        addAlbumToGroup(album->_key._genre, album, displayed_groups);
    }

    for (const auto& group : displayed_groups)
    {
        QString id = QString("genre(%1,%2,%3)")
            .arg(group.first)
            .arg(group.second->_key._album)
            .arg(group.second->_key._cover_checksum);

        model->addItem(id, group.first, group.second->getCoverPixmap(), [=]() {
            return new AudioLibraryViewGenre(group.first);
        });
    }
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
    const DisplayMode* display_mode,
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
                switch (*display_mode)
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
    return QString("view:artist=%1").arg(_artist);
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
    const DisplayMode* /*display_mode*/,
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
    return QString("view:album=%1,%2,%3,%4,%5")
        .arg(_key._artist)
        .arg(_key._album)
        .arg(_key._genre)
        .arg(_key._year)
        .arg(_key._cover_checksum);
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
    return QString("%1").arg(_year);
}

std::vector<AudioLibraryView::DisplayMode> AudioLibraryViewYear::getSupportedModes() const
{
    return{ DisplayMode::ALBUMS, DisplayMode::TRACKS };
}

void AudioLibraryViewYear::createItems(const AudioLibrary& library,
    const DisplayMode* display_mode,
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
    return QString("view:year=%1").arg(_year);
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
    const DisplayMode* display_mode,
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
    return QString("view:genre=%1").arg(_genre);
}

//=============================================================================

AudioLibraryViewSimpleSearch::AudioLibraryViewSimpleSearch(const QString& search_text)
    : _search_text(search_text)
{}

AudioLibraryView* AudioLibraryViewSimpleSearch::clone() const
{
    return new AudioLibraryViewSimpleSearch(*this);
}

QString AudioLibraryViewSimpleSearch::getDisplayName() const
{
    return QString("Search \"%1\"").arg(_search_text);
}

std::vector<AudioLibraryView::DisplayMode> AudioLibraryViewSimpleSearch::getSupportedModes() const
{
    return{ DisplayMode::ALBUMS, DisplayMode::TRACKS };
}

void AudioLibraryViewSimpleSearch::createItems(const AudioLibrary& library,
    const DisplayMode* display_mode,
    AudioLibraryModel* model) const
{
    auto strings_to_match = _search_text.splitRef(" ");

    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        switch (*display_mode)
        {
        case AudioLibraryView::DisplayMode::ALBUMS:
            if (match(album->_key._artist, strings_to_match) ||
                match(album->_key._album, strings_to_match))
            {
                model->addAlbumItem(album);
            }
            break;
        case AudioLibraryView::DisplayMode::TRACKS:
        {
            QPixmap pixmap;

            for (const AudioLibraryTrack* track : album->_tracks)
            {
                if (match(track->_artist, strings_to_match) ||
                    match(track->_album_artist, strings_to_match) ||
                    match(track->_title, strings_to_match))
                {
                    model->addTrackItem(track);
                }
            }
            break;
        }
        }
    }
}

bool AudioLibraryViewSimpleSearch::match(const QString& input, const QVector<QStringRef>& strings_to_match)
{
    for (const QStringRef& ref : strings_to_match)
        if (!input.contains(ref, Qt::CaseInsensitive))
            return false;

    return true;
}

//=============================================================================

AudioLibraryViewAdvancedSearch::AudioLibraryViewAdvancedSearch(const Query& query)
    : _query(query)
{
}

AudioLibraryView* AudioLibraryViewAdvancedSearch::clone() const
{
    return new AudioLibraryViewAdvancedSearch(*this);
}

QString AudioLibraryViewAdvancedSearch::getDisplayName() const
{
    QString text = "Search: ";

    if (!_query.artist.isEmpty())
        text += QString("Artist ~ \"%1\" ").arg(_query.artist);
    if (!_query.album.isEmpty())
        text += QString("Album ~ \"%1\" ").arg(_query.album);
    if (!_query.genre.isEmpty())
        text += QString("Genre ~ \"%1\" ").arg(_query.genre);
    if (!_query.title.isEmpty())
        text += QString("Title ~ \"%1\" ").arg(_query.title);
    if (!_query.comment.isEmpty())
        text += QString("Comment ~ \"%1\" ").arg(_query.comment);

    return text;
}

std::vector<AudioLibraryView::DisplayMode> AudioLibraryViewAdvancedSearch::getSupportedModes() const
{
    return{ DisplayMode::TRACKS };
}

class AdvancedSearchComparer
{
public:
    AdvancedSearchComparer(const QString& text, Qt::CaseSensitivity case_sensitivity, bool use_regex);

    bool check(const QString& text) const;

private:
    QString _text;
    Qt::CaseSensitivity _case_sensitivity;
    QRegExp _regex;
};

AdvancedSearchComparer::AdvancedSearchComparer(const QString& text, Qt::CaseSensitivity case_sensitivity, bool use_regex)
    : _case_sensitivity(case_sensitivity)
{
    if (use_regex)
        _regex = QRegExp(text, _case_sensitivity);
    else
        _text = text;
}

bool AdvancedSearchComparer::check(const QString& text) const
{
    if (!_text.isEmpty())
        return text.contains(_text, _case_sensitivity);

    return text.contains(_regex);
}

void AudioLibraryViewAdvancedSearch::createItems(const AudioLibrary& library,
    const DisplayMode* /*display_mode*/,
    AudioLibraryModel* model) const
{
    AdvancedSearchComparer compare_artist(_query.artist, _query.case_sensitive, _query.use_regex);
    AdvancedSearchComparer compare_album(_query.album, _query.case_sensitive, _query.use_regex);
    AdvancedSearchComparer compare_genre(_query.genre, _query.case_sensitive, _query.use_regex);
    AdvancedSearchComparer compare_title(_query.title, _query.case_sensitive, _query.use_regex);
    AdvancedSearchComparer compare_comment(_query.comment, _query.case_sensitive, _query.use_regex);

    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        if (!_query.artist.isEmpty() && !compare_artist.check(album->_key._artist))
            continue;
        if (!_query.album.isEmpty() && !compare_album.check(album->_key._album))
            continue;
        if (!_query.genre.isEmpty() && !compare_genre.check(album->_key._genre))
            continue;

        if (!_query.title.isEmpty() || !_query.comment.isEmpty())
        {
            QPixmap pixmap;

            for (const AudioLibraryTrack* track : album->_tracks)
            {
                if (!_query.title.isEmpty() && !compare_title.check(track->_title))
                    continue;
                if (!_query.comment.isEmpty() && !compare_comment.check(track->_comment))
                    continue;

                model->addTrackItem(track);
            }
        }
        else
            model->addAlbumItem(album);
    }
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
    const DisplayMode* /*display_mode*/,
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