// SPDX-License-Identifier: GPL-2.0-only
#include "AudioLibraryModel.h"

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
        QVariant var_sort_key1 = data(AudioLibraryView::SORT_ROLE);
        QVariant var_sort_key2 = other.data(AudioLibraryView::SORT_ROLE);

        if (var_sort_key1.type() == QVariant::String && var_sort_key2.type() == QVariant::String)
        {
            return collator().compare(var_sort_key1.toString(), var_sort_key2.toString()) < 0;
        }

        return _collator.compare(text(), other.text()) < 0;
    }

    //=========================================================================

    class AlbumModelItem : public CollatedItem
    {
    public:
        AlbumModelItem(const QIcon& icon, const QString& text, const AudioLibraryAlbum* album, const QCollator& collator);
    };

    AlbumModelItem::AlbumModelItem(const QIcon& icon, const QString& text, const AudioLibraryAlbum* album, const QCollator& collator)
        : CollatedItem(icon, text, collator)
    {
        QLatin1Char sep(' ');

        QString sort_key = album->_key._artist + sep +
            QString::number(album->_key._year) + sep +
            album->_key._album;

        setData(sort_key, AudioLibraryView::SORT_ROLE);
    }

    //=========================================================================

    class TrackModelItem : public CollatedItem
    {
    public:
        TrackModelItem(const QIcon& icon, const QString& text, const AudioLibraryTrack* track, const QCollator& collator);
    };

    TrackModelItem::TrackModelItem(const QIcon& icon, const QString& text, const AudioLibraryTrack* track, const QCollator& collator)
        : CollatedItem(icon, text, collator)
    {
        QLatin1Char sep(' ');

        QString sort_key = track->_album->_key._artist + sep +
            QString::number(track->_album->_key._year) + sep +
            track->_album->_key._album + sep +
            QString::number(track->_disc_number) + sep +
            QString::number(track->_track_number);

        setData(sort_key, AudioLibraryView::SORT_ROLE);
    }

} // nameless namespace

AudioLibraryModel::AudioLibraryModel(QObject* parent)
    : QStandardItemModel(parent)
{
    QPixmap default_pixmap = QPixmap(256, 256);
    default_pixmap.fill(Qt::transparent);

    _default_icon = default_pixmap;

    _numeric_collator.setNumericMode(true);
}

void AudioLibraryModel::addItemInternal(const QString& id, const QIcon& icon,
    std::function<QStandardItem*(int row)> item_factory,
    std::function<AudioLibraryView*()> view_factory)
{
    _requested_ids.insert(id);

    if (QStandardItem* existing_item = getItemForId(id))
    {
        if (existing_item->icon().cacheKey() == _default_icon.cacheKey() &&
            !icon.isNull())
        {
            existing_item->setIcon(icon);
        }

        // already exists, nothing to do
        return;
    }

    int row = rowCount();

    QStandardItem* item = item_factory(row);

    _id_to_item_map[id] = item;
    item->setData(id, AudioLibraryView::ID_ROLE);

    if(AudioLibraryView* view = view_factory())
        _item_to_view_map[item].reset(view);
}

void AudioLibraryModel::addItem(const QString& id, const QString& name, const QIcon& icon, int number_of_albums, int number_of_tracks, std::function<AudioLibraryView*()> view_factory)
{
    auto item_factory = [=](int row) {
        QStandardItem* item = new CollatedItem(icon.isNull() ? _default_icon : icon, name, _numeric_collator);
        setItem(row, item);

        setAdditionalColumn(row, AudioLibraryView::NUMBER_OF_ALBUMS, QString::number(number_of_albums));
        setAdditionalColumn(row, AudioLibraryView::NUMBER_OF_TRACKS, QString::number(number_of_tracks));

        return item;
    };

    addItemInternal(id, icon, item_factory, view_factory);
}

void AudioLibraryModel::addAlbumItem(const AudioLibraryAlbum* album)
{
    QString id = album->getId();
    QIcon icon = album->getCoverPixmap().isNull() ? _default_icon : album->getCoverPixmap();

    auto item_factory = [=](int row) {
        QStandardItem* item = new AlbumModelItem(icon, album->_key._artist + " - " + album->_key._album, album, _numeric_collator);
        item->setData(album->_key._artist + QChar(QChar::LineSeparator) + album->_key._album, AudioLibraryView::MULTILINE_DISPLAY_ROLE);
        setItem(row, item);

        setAlbumColumns(row, album);
        setAdditionalColumn(row, AudioLibraryView::ARTIST, album->_key._artist);
        setAdditionalColumn(row, AudioLibraryView::NUMBER_OF_TRACKS, QString::number(album->_tracks.size()));

        int length_milliseconds = 0;
        for (const AudioLibraryTrack* track : album->_tracks)
            length_milliseconds += track->_length_milliseconds;

        setLengthColumn(row, length_milliseconds);

        return item;
    };

    addItemInternal(id, icon, item_factory, [=](){
        return new AudioLibraryViewAlbum(album->_key);
    });
}

void AudioLibraryModel::addTrackItem(const AudioLibraryTrack* track)
{
    QString id = track->getId();
    QIcon icon = track->_album->getCoverPixmap().isNull() ? _default_icon : track->_album->getCoverPixmap();

    auto item_factory = [=](int row) {
        QStandardItem* item = new TrackModelItem(icon, track->_artist + " - " + track->_title, track, _numeric_collator);
        item->setData(track->_artist + QChar(QChar::LineSeparator) + track->_title, AudioLibraryView::MULTILINE_DISPLAY_ROLE);
        setItem(row, item);

        setAlbumColumns(row, track->_album);
        setAdditionalColumn(row, AudioLibraryView::ARTIST, track->_artist);
        setAdditionalColumn(row, AudioLibraryView::TITLE, track->_title);
        if(track->_track_number != 0)
            setAdditionalColumn(row, AudioLibraryView::TRACK_NUMBER, QString::number(track->_track_number));
        if(track->_disc_number != 0)
            setAdditionalColumn(row, AudioLibraryView::DISC_NUMBER, QString::number(track->_disc_number));
        setAdditionalColumn(row, AudioLibraryView::ALBUM_ARTIST, track->_album_artist);
        setAdditionalColumn(row, AudioLibraryView::COMMENT, track->_comment);
        setAdditionalColumn(row, AudioLibraryView::PATH, track->_filepath);
        setDateTimeColumn(row, AudioLibraryView::DATE_MODIFIED, track->_last_modified);
        setAdditionalColumn(row, AudioLibraryView::TAG_TYPES, track->_tag_types);
        setLengthColumn(row, track->_length_milliseconds);
        setAdditionalColumn(row, AudioLibraryView::CHANNELS, QString::number(track->_channels));
        setAdditionalColumn(row, AudioLibraryView::BITRATE_KBS, QString::number(track->_bitrate_kbs) + QLatin1String(" kbit/s"));
        setAdditionalColumn(row, AudioLibraryView::SAMPLERATE_HZ, QString::number(track->_samplerate_hz) + QLatin1String(" Hz"));

        return item;
    };

    // no view for track items

    addItemInternal(id, icon, item_factory, []() {
        return nullptr;
    });
}

QString AudioLibraryModel::getItemId(QStandardItem* item) const
{
    return item->data(AudioLibraryView::ID_ROLE).toString();
}

QStandardItem* AudioLibraryModel::getItemForId(const QString& id) const
{
    auto it = _id_to_item_map.find(id);
    if (it != _id_to_item_map.end())
        return it->second;

    return nullptr;
}

const AudioLibraryView* AudioLibraryModel::getViewForItem(QStandardItem* item) const
{
    auto it = _item_to_view_map.find(item);
    if (it != _item_to_view_map.end())
        return it->second.get();

    return nullptr;
}

QString AudioLibraryModel::getFilepathFromIndex(const QModelIndex& index) const
{
    if (QStandardItem* path_item = item(index.row(), AudioLibraryView::PATH))
    {
        return path_item->text();
    }

    return QString::null;
}

void AudioLibraryModel::removeId(const QString& id)
{
    auto it = _id_to_item_map.find(id);
    if (it == _id_to_item_map.end())
        return;

    _item_to_view_map.erase(it->second);
    removeRow(it->second->index().row());

    _id_to_item_map.erase(it);
}

AudioLibraryModel::IncrementalUpdateScope::IncrementalUpdateScope(AudioLibraryModel& model)
    : _model(model)
{
    _model.onUpdateStarted();
}

AudioLibraryModel::IncrementalUpdateScope::~IncrementalUpdateScope()
{
    _model.onUpdateFinished();
}

void AudioLibraryModel::onUpdateStarted()
{
    _requested_ids.clear();
}

void AudioLibraryModel::onUpdateFinished()
{
    // remove all IDs that were not requested

    QStringList ids_to_remove;

    for (const auto& id_and_item : _id_to_item_map)
        if (_requested_ids.find(id_and_item.first) == _requested_ids.end())
            ids_to_remove.push_back(id_and_item.first);

    for (const QString& id : ids_to_remove)
        removeId(id);
}

QStandardItem* AudioLibraryModel::setAdditionalColumn(int row, AudioLibraryView::Column column, const QString& text)
{
    QStandardItem* item = new CollatedItem(text, _numeric_collator);
    setItem(row, static_cast<int>(column), item);
    return item;
}

void AudioLibraryModel::setDateTimeColumn(int row, AudioLibraryView::Column column, const QDateTime& date)
{
    QStandardItem* item = setAdditionalColumn(row, column, date.toString(Qt::DefaultLocaleShortDate));

    item->setData(date.toString(Qt::ISODate), AudioLibraryView::SORT_ROLE);
}

void AudioLibraryModel::setLengthColumn(int row, int length_milliseconds)
{
    int length_seconds = length_milliseconds / 1000;

    QString formatted_length = length_seconds < 3600 ?
        QDateTime::fromTime_t(length_seconds).toUTC().toString("mm:ss") :
        QDateTime::fromTime_t(length_seconds).toUTC().toString("hh:mm:ss");

    QStandardItem* length_item = setAdditionalColumn(row, AudioLibraryView::LENGTH_SECONDS, formatted_length);

    length_item->setData(QString::number(length_seconds), AudioLibraryView::SORT_ROLE);
}

void AudioLibraryModel::setAlbumColumns(int row, const AudioLibraryAlbum* album)
{
    setAdditionalColumn(row, AudioLibraryView::ALBUM, album->_key._album);

    if (album->_key._year != 0)
        setAdditionalColumn(row, AudioLibraryView::YEAR, QString::number(album->_key._year));

    setAdditionalColumn(row, AudioLibraryView::GENRE, album->_key._genre);

    if (!album->_cover.isEmpty())
    {
        setAdditionalColumn(row, AudioLibraryView::COVER_CHECKSUM, QString::number(album->_key._cover_checksum));

#if QT_VERSION >= QT_VERSION_CHECK(5,10,0)
        QString data_size = QLocale().formattedDataSize(album->_cover.size());
#else
        QString data_size = QString("%1 Bytes").arg(album->_cover.size());
#endif

        QStandardItem* datasize_item = setAdditionalColumn(row, AudioLibraryView::COVER_DATASIZE, data_size);
        datasize_item->setData(QString::number(album->_cover.size()), AudioLibraryView::SORT_ROLE);
    }

    setAdditionalColumn(row, AudioLibraryView::COVER_TYPE, album->getCoverType());

    if (!album->getCoverPixmap().isNull())
    {
        setAdditionalColumn(row, AudioLibraryView::COVER_WIDTH, QString::number(album->getCoverPixmap().width()));
        setAdditionalColumn(row, AudioLibraryView::COVER_HEIGHT, QString::number(album->getCoverPixmap().height()));
    }
}