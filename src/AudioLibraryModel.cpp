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
        setData(sort_key, AudioLibraryView::SORT_ROLE);
    }

    bool AlbumModelItem::operator<(const QStandardItem& other) const
    {
        QVariant var_sort_key1 = data(AudioLibraryView::SORT_ROLE);
        QVariant var_sort_key2 = other.data(AudioLibraryView::SORT_ROLE);

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
        setData(sort_key, AudioLibraryView::SORT_ROLE);
    }

    bool TrackModelItem::operator<(const QStandardItem& other) const
    {
        QVariant var_sort_key1 = data(AudioLibraryView::SORT_ROLE);
        QVariant var_sort_key2 = other.data(AudioLibraryView::SORT_ROLE);

        if (var_sort_key1.type() == QVariant::String && var_sort_key2.type() == QVariant::String)
        {
            return collator().compare(var_sort_key1.toString(), var_sort_key2.toString()) < 0;
        }

        return QStandardItem::operator<(other);
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

void AudioLibraryModel::addItem(const QString& id, const QString& name, const QIcon& icon, std::function<AudioLibraryView*()> view_factory)
{
    auto item_factory = [=](int row) {
        QStandardItem* item = new CollatedItem(icon.isNull() ? _default_icon : icon, name, _numeric_collator);
        setItem(row, item);
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

        setAdditionalColumn(row, AudioLibraryView::ARTIST, album->_key._artist);
        setAdditionalColumn(row, AudioLibraryView::ALBUM, album->_key._album);
        setAdditionalColumn(row, AudioLibraryView::YEAR, QString("%1").arg(album->_key._year));
        setAdditionalColumn(row, AudioLibraryView::GENRE, album->_key._genre);
        setAdditionalColumn(row, AudioLibraryView::COVER_CHECKSUM, QString("%1").arg(album->_key._cover_checksum));
        setAdditionalColumn(row, AudioLibraryView::COVER_TYPE, getCoverType(album));

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

        setAdditionalColumn(row, AudioLibraryView::ARTIST, track->_artist);
        setAdditionalColumn(row, AudioLibraryView::ALBUM, track->_album->_key._album);
        setAdditionalColumn(row, AudioLibraryView::YEAR, QString("%1").arg(track->_album->_key._year));
        setAdditionalColumn(row, AudioLibraryView::GENRE, track->_album->_key._genre);
        setAdditionalColumn(row, AudioLibraryView::COVER_CHECKSUM, QString("%1").arg(track->_album->_key._cover_checksum));
        setAdditionalColumn(row, AudioLibraryView::COVER_TYPE, getCoverType(track->_album));
        setAdditionalColumn(row, AudioLibraryView::TITLE, track->_title);
        setAdditionalColumn(row, AudioLibraryView::TRACK_NUMBER, QString("%1").arg(track->_track_number));
        setAdditionalColumn(row, AudioLibraryView::DISC_NUMBER, QString("%1").arg(track->_disc_number));
        setAdditionalColumn(row, AudioLibraryView::ALBUM_ARTIST, track->_album_artist);
        setAdditionalColumn(row, AudioLibraryView::COMMENT, track->_comment);
        setAdditionalColumn(row, AudioLibraryView::PATH, track->_filepath);
        setAdditionalColumn(row, AudioLibraryView::TAG_TYPES, track->_tag_types);

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

void AudioLibraryModel::setAdditionalColumn(int row, AudioLibraryView::Column column, const QString& text)
{
    setItem(row, static_cast<int>(column), new CollatedItem(text, _numeric_collator));
}

template<class ARRAY>
bool compareSignature(const ARRAY& signature, const QByteArray& bytes)
{
    const size_t signature_size = std::distance(std::begin(signature), std::end(signature));

    return bytes.size() >= signature_size &&
        memcmp(bytes.constData(), signature, signature_size) == 0;
}

QString AudioLibraryModel::getCoverType(const AudioLibraryAlbum* album)
{
    const uint8_t JPG_SIGNATURE[] = { 0xff, 0xd8 };
    const uint8_t PNG_SIGNATURE[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
    const uint8_t BMP_SIGNATURE[] = { 0x42, 0x4d };

    if (compareSignature(JPG_SIGNATURE, album->_cover))
        return "jpg";

    if (compareSignature(PNG_SIGNATURE, album->_cover))
        return "png";

    if (compareSignature(BMP_SIGNATURE, album->_cover))
        return "bmp";

    if (!album->_cover.isEmpty())
    {
        return "unknown signature: " + QString::fromLatin1(album->_cover.left(32).toHex());
    }

    return QString::null;
}