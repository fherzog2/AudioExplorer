// SPDX-License-Identifier: GPL-2.0-only
#include "AudioLibraryView.h"

#include <unordered_set>
#include <QtCore/qcollator.h>

namespace {

    class CollatedItem : public QStandardItem
    {
    public:
        CollatedItem(const QString& text, const QCollator& collator);
        CollatedItem(const QIcon& icon, const QString& text, const QCollator& collator);

        bool operator<(const QStandardItem& other) const;

        const QCollator& collator() const { return _collator; }

    private:
        QCollator _collator;
    };

    CollatedItem::CollatedItem(const QString& text, const QCollator& collator)
        : QStandardItem(text)
        , _collator(collator)
    {
    }

    CollatedItem::CollatedItem(const QIcon& icon, const QString& text, const QCollator& collator)
        : QStandardItem(icon, text)
        , _collator(collator)
    {
    }

    bool CollatedItem::operator<(const QStandardItem& other) const
    {
        return _collator.compare(text(), other.text()) < 0;
    }

    //=========================================================================

    class AlbumModelItem : public CollatedItem
    {
    public:
        AlbumModelItem(const QIcon& icon, const QString& text, const AudioLibraryAlbum* album, const QCollator& collator);

        bool operator<(const QStandardItem& other) const;
    };

    AlbumModelItem::AlbumModelItem(const QIcon& icon, const QString& text, const AudioLibraryAlbum* album, const QCollator& collator)
        : CollatedItem(icon, text, collator)
    {
        QString sort_key = QString("%1 %2 %3")
            .arg(album->_key._artist)
            .arg(album->_key._year)
            .arg(album->_key._album);
        setData(sort_key);
    }

    bool AlbumModelItem::operator<(const QStandardItem& other) const
    {
        QVariant var_sort_key1 = data();
        QVariant var_sort_key2 = other.data();

        if (var_sort_key1.type() == QVariant::String && var_sort_key2.type() == QVariant::String)
        {
            return collator().compare(var_sort_key1.toString(), var_sort_key2.toString()) < 0;
        }

        return QStandardItem::operator<(other);
    }

    //=========================================================================

    class TrackModelItem : public CollatedItem
    {
    public:
        TrackModelItem(const QIcon& icon, const QString& text, const AudioLibraryTrack* track, const QCollator& collator);

        bool operator<(const QStandardItem& other) const;
    };

    TrackModelItem::TrackModelItem(const QIcon& icon, const QString& text, const AudioLibraryTrack* track, const QCollator& collator)
        : CollatedItem(icon, text, collator)
    {
        QString sort_key = QString("%1 %2 %3 %4 %5")
            .arg(track->_album->_key._artist)
            .arg(track->_album->_key._year)
            .arg(track->_album->_key._album)
            .arg(track->_disc_number)
            .arg(track->_track_number);
        setData(sort_key);
    }

    bool TrackModelItem::operator<(const QStandardItem& other) const
    {
        QVariant var_sort_key1 = data();
        QVariant var_sort_key2 = other.data();

        if (var_sort_key1.type() == QVariant::String && var_sort_key2.type() == QVariant::String)
        {
            return collator().compare(var_sort_key1.toString(), var_sort_key2.toString()) < 0;
        }

        return QStandardItem::operator<(other);
    }

    //=========================================================================

    class CoverLoader
    {
    public:
        CoverLoader();

        const QPixmap& getCoverPixmap(const AudioLibraryAlbum* album) const;

    private:
        QPixmap _default_pixmap;
    };

    CoverLoader::CoverLoader()
    {
        _default_pixmap = QPixmap(256, 256);
        _default_pixmap.fill(Qt::transparent);
    }

    const QPixmap& CoverLoader::getCoverPixmap(const AudioLibraryAlbum* album) const
    {
        if (!album->getCoverPixmap().isNull())
            return album->getCoverPixmap();

        return _default_pixmap;
    }

    void setAdditionalColumn(QStandardItemModel* model, int row, AudioLibraryView::Column column, const QString& text, const QCollator& collator)
    {
        model->setItem(row, static_cast<int>(column), new CollatedItem(text, collator));
    }

    void createAlbumColumns(const AudioLibraryAlbum* album, QStandardItemModel* model, int row, const QCollator& collator)
    {
        setAdditionalColumn(model, row, AudioLibraryView::ARTIST, album->_key._artist, collator);
        setAdditionalColumn(model, row, AudioLibraryView::ALBUM, album->_key._album, collator);
        setAdditionalColumn(model, row, AudioLibraryView::YEAR, QString("%1").arg(album->_key._year), collator);
        setAdditionalColumn(model, row, AudioLibraryView::GENRE, album->_key._genre, collator);
        setAdditionalColumn(model, row, AudioLibraryView::COVER_CHECKSUM, QString("%3").arg(album->_key._cover_checksum), collator);
    }

    void createAlbumRow(const AudioLibraryAlbum* album,
        QStandardItemModel* model,
        std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items,
        const QCollator& collator,
        const CoverLoader& cover_loader)
    {
        QPixmap pixmap = cover_loader.getCoverPixmap(album);

        int row = model->rowCount();
        QStandardItem* item = new AlbumModelItem(pixmap, album->_key._artist + " - " + album->_key._album, album, collator);
        model->setItem(row, item);
        createAlbumColumns(album, model, row, collator);
        views_for_items[item].reset(new AudioLibraryViewAlbum(album->_key));
    }

    void createTrackRow(const AudioLibraryTrack* track,
        QStandardItemModel* model,
        const QPixmap& cover,
        const QCollator& collator)
    {
        int row = model->rowCount();
        QStandardItem* item = new TrackModelItem(cover, track->_artist + " - " + track->_title, track, collator);
        model->setItem(row, item);
        setAdditionalColumn(model, row, AudioLibraryView::ARTIST, track->_artist, collator);
        setAdditionalColumn(model, row, AudioLibraryView::ALBUM, track->_album->_key._album, collator);
        setAdditionalColumn(model, row, AudioLibraryView::YEAR, QString("%1").arg(track->_album->_key._year), collator);
        setAdditionalColumn(model, row, AudioLibraryView::GENRE, track->_album->_key._genre, collator);
        setAdditionalColumn(model, row, AudioLibraryView::COVER_CHECKSUM, QString("%3").arg(track->_album->_key._cover_checksum), collator);
        setAdditionalColumn(model, row, AudioLibraryView::TITLE, track->_title, collator);
        setAdditionalColumn(model, row, AudioLibraryView::TRACK_NUMBER, QString("%1").arg(track->_track_number), collator);
        setAdditionalColumn(model, row, AudioLibraryView::DISC_NUMBER, QString("%1").arg(track->_disc_number), collator);
        setAdditionalColumn(model, row, AudioLibraryView::ALBUM_ARTIST, track->_album_artist, collator);
        setAdditionalColumn(model, row, AudioLibraryView::COMMENT, track->_comment, collator);
        setAdditionalColumn(model, row, AudioLibraryView::PATH, track->_filepath, collator);
        setAdditionalColumn(model, row, AudioLibraryView::TAG_TYPES, track->_tag_types, collator);
    }

    void createAlbumOrTrackRow(const AudioLibraryAlbum* album,
        const AudioLibraryView::DisplayMode* display_mode,
        QStandardItemModel* model,
        std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items,
        const QCollator& collator,
        const CoverLoader& cover_loader)
    {
        switch (*display_mode)
        {
        case AudioLibraryView::DisplayMode::ALBUMS:
            createAlbumRow(album, model, views_for_items, collator, cover_loader);
            break;
        case AudioLibraryView::DisplayMode::TRACKS:
        {
            QPixmap pixmap = cover_loader.getCoverPixmap(album);
            for (const AudioLibraryTrack* track : album->_tracks)
                createTrackRow(track, model, pixmap, collator);
        }
        break;
        }
    }

    template<class GROUPED_TYPE, class SUB_VIEW_TYPE, class GROUPED_TYPE_GETTER>
    void createGroupViewItems(const AudioLibrary& library,
        QStandardItemModel* model,
        std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items,
        GROUPED_TYPE_GETTER grouped_type_getter)
    {
        QCollator collator;
        collator.setNumericMode(true);

        CoverLoader cover_loader;

        std::unordered_set<GROUPED_TYPE> displayed_groups;

        for (const AudioLibraryAlbum* album : library.getAlbums())
        {
            GROUPED_TYPE group = grouped_type_getter(album);

            if (displayed_groups.find(group) != displayed_groups.end())
                continue; // already displayed

            displayed_groups.insert(group);

            QPixmap pixmap = cover_loader.getCoverPixmap(album);

            QStandardItem* item = new CollatedItem(pixmap, QString("%1").arg(group), collator);
            model->setItem(model->rowCount(), item);
            views_for_items[item].reset(new SUB_VIEW_TYPE(group));
        }
    }

    int getYear(const AudioLibraryAlbum* album)
    {
        return album->_key._year;
    }

    QString getGenre(const AudioLibraryAlbum* album)
    {
        return album->_key._genre;
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
        };
    case DisplayMode::TRACKS:
        return
        {
            AudioLibraryView::ARTIST,
            AudioLibraryView::ALBUM,
            AudioLibraryView::YEAR,
            AudioLibraryView::GENRE,
            AudioLibraryView::COVER_CHECKSUM,
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
    QStandardItemModel* model,
    std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items) const
{
    QCollator collator;
    collator.setNumericMode(true);

    CoverLoader cover_loader;

    std::unordered_set<QString> displayed_artists;

    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        for (const AudioLibraryTrack* track : album->_tracks)
        {
            {
                QString artist = track->_artist;

                if (displayed_artists.find(artist) != displayed_artists.end())
                    continue; // already displayed

                displayed_artists.insert(artist);

                QStandardItem* item = new CollatedItem(cover_loader.getCoverPixmap(album), QString("%1").arg(artist), collator);
                model->setItem(model->rowCount(), item);
                views_for_items[item].reset(new AudioLibraryViewArtist(artist));
            }

            if (!track->_album_artist.isEmpty())
            {
                QString artist = track->_album_artist;

                if (displayed_artists.find(artist) != displayed_artists.end())
                    continue; // already displayed

                displayed_artists.insert(artist);

                QStandardItem* item = new CollatedItem(cover_loader.getCoverPixmap(album), QString("%1").arg(artist), collator);
                model->setItem(model->rowCount(), item);
                views_for_items[item].reset(new AudioLibraryViewArtist(artist));
            }
        }
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
    QStandardItemModel* model,
    std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items) const
{
    QCollator collator;
    collator.setNumericMode(true);

    CoverLoader cover_loader;

    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        createAlbumRow(album, model, views_for_items, collator, cover_loader);
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
    QStandardItemModel* model,
    std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& /*views_for_items*/) const
{
    QCollator collator;
    collator.setNumericMode(true);

    CoverLoader cover_loader;

    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        QPixmap pixmap = cover_loader.getCoverPixmap(album);

        for (const AudioLibraryTrack* track : album->_tracks)
        {
            createTrackRow(track, model, pixmap, collator);
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
    QStandardItemModel* model,
    std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items) const
{
    createGroupViewItems<int, AudioLibraryViewYear>(library, model, views_for_items, getYear);
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
    QStandardItemModel* model,
    std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items) const
{
    createGroupViewItems<QString, AudioLibraryViewGenre>(library, model, views_for_items, getGenre);
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
    QStandardItemModel* model,
    std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items) const
{
    QCollator collator;
    collator.setNumericMode(true);

    CoverLoader cover_loader;

    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        for (const AudioLibraryTrack* track : album->_tracks)
        {
            bool album_row_created = false;

            if (track->_artist == _artist ||
                track->_album_artist == _artist)
            {
                switch (*display_mode)
                {
                case DisplayMode::ALBUMS:
                    createAlbumRow(album, model, views_for_items, collator, cover_loader);
                    album_row_created = true;
                    break;
                case DisplayMode::TRACKS:
                    createTrackRow(track, model, cover_loader.getCoverPixmap(album), collator);
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
        if (album->_key._artist == _artist)
        {
            tracks.insert(tracks.end(), album->_tracks.begin(), album->_tracks.end());
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
    QStandardItemModel* model,
    std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& /*views_for_items*/) const
{
    QCollator collator;
    collator.setNumericMode(true);

    CoverLoader cover_loader;

    if (const AudioLibraryAlbum* album = library.getAlbum(_key))
    {
        QPixmap pixmap = cover_loader.getCoverPixmap(album);

        for (const AudioLibraryTrack* track : album->_tracks)
        {
            createTrackRow(track, model, pixmap, collator);
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
    QStandardItemModel* model,
    std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items) const
{
    QCollator collator;
    collator.setNumericMode(true);

    CoverLoader cover_loader;

    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        if (album->_key._year == _year)
        {
            createAlbumOrTrackRow(album, display_mode, model, views_for_items, collator, cover_loader);
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
    QStandardItemModel* model,
    std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items) const
{
    QCollator collator;
    collator.setNumericMode(true);

    CoverLoader cover_loader;

    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        if (album->_key._genre == _genre)
        {
            createAlbumOrTrackRow(album, display_mode, model, views_for_items, collator, cover_loader);
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
    QStandardItemModel* model,
    std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items) const
{
    QCollator collator;
    collator.setNumericMode(true);

    CoverLoader cover_loader;

    auto strings_to_match = _search_text.splitRef(" ");

    for (const AudioLibraryAlbum* album : library.getAlbums())
    {
        switch (*display_mode)
        {
        case AudioLibraryView::DisplayMode::ALBUMS:
            if (match(album->_key._artist, strings_to_match) ||
                match(album->_key._album, strings_to_match))
            {
                createAlbumRow(album, model, views_for_items, collator, cover_loader);
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
                    if (pixmap.isNull())
                        pixmap = cover_loader.getCoverPixmap(album);

                    createTrackRow(track, model, pixmap, collator);
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
    QStandardItemModel* model,
    std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items) const
{
    QCollator collator;
    collator.setNumericMode(true);

    CoverLoader cover_loader;

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

                if (pixmap.isNull())
                    pixmap = cover_loader.getCoverPixmap(album);

                createTrackRow(track, model, pixmap, collator);
            }
        }
        else
            createAlbumRow(album, model, views_for_items, collator, cover_loader);
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
    QStandardItemModel* model,
    std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items) const
{
    QCollator collator;
    collator.setNumericMode(true);

    CoverLoader cover_loader;

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
        createAlbumRow(album, model, views_for_items, collator, cover_loader);
    }
}