// SPDX-License-Identifier: GPL-2.0-only
#include "AudioLibraryModel.h"

#include <array>
#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qcollator.h>
#include <QtCore/qtimer.h>

class AudioLibraryModelImpl : public QAbstractTableModel
{
public:
    AudioLibraryModelImpl(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    void setDataInternal(int row, AudioLibraryView::Column column, const QVariant& data, int role = Qt::DisplayRole);
    void setDecoration(int row, const AudioLibraryAlbum* album);
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    /**
    * States of the decoration pixmap
    */
    enum class LoadState
    {
        NotLoaded, //!< the decoration has not been requested yet
        Requested, //!< the decoration has been requested and will be decoded soon
        Done       //!< the decoration has been decoded
    };

    struct Decoration
    {
        QByteArray bytes;
        LoadState load_state = LoadState::NotLoaded;
        QPixmap pixmap;
        QVariant variant;

        bool load();
    };

    struct Row
    {
        std::array<QVariant, AudioLibraryView::NUMBER_OF_COLUMNS> display_role_data;
        std::array<QString, AudioLibraryView::NUMBER_OF_COLUMNS> sort_role_data;

        /**
        * decoration data can be shared between rows, because multiple tracks can have the same album cover
        * this keeps the memory consumption and decoding effort low
        */
        std::shared_ptr<Decoration> decoration;
        mutable LoadState decoration_load_state = LoadState::NotLoaded;

        QVariant multiline_display_role;
        QVariant id;
        std::unique_ptr<AudioLibraryView> view;

        int index = -1;
    };

    Row* createRow(const QUuid& id);
    void removeRow(const QUuid& id);
    Row* findRowForId(const QUuid& id) const;
    QModelIndex findIndexForId(const QUuid& id) const;
    const AudioLibraryView* getViewForIndex(const QModelIndex& index) const;

    void setHorizontalHeaderLabels(const QStringList& labels);

    std::vector<QUuid> getAllIds() const;

    const QIcon& getDefaultIcon() const;

    void updateDecoration(const QModelIndex& index);

private:
    void updateRowIndexes();

    void loadRequestedDecorations();

    std::vector<std::unique_ptr<Row>> _rows;
    std::unordered_map<QUuid, Row*> _id_to_row_map;
    std::unordered_map<QUuid, std::shared_ptr<Decoration>> _decorations_for_album_ids;
    mutable std::vector< std::shared_ptr<Decoration>> _requested_decorations;
    QStringList _header_labels;
    QIcon _default_icon;
};

AudioLibraryModelImpl::AudioLibraryModelImpl(QObject* parent)
    : QAbstractTableModel(parent)
{
    QPixmap default_pixmap = QPixmap(256, 256);
    default_pixmap.fill(Qt::transparent);

    _default_icon = default_pixmap;

    auto load_requested_decorations_timer = new QTimer(this);

    connect(load_requested_decorations_timer, &QTimer::timeout,
        this, &AudioLibraryModelImpl::loadRequestedDecorations);
    load_requested_decorations_timer->setSingleShot(false);
    load_requested_decorations_timer->start(100);
}

int AudioLibraryModelImpl::rowCount(const QModelIndex& /*parent*/) const
{
    return static_cast<int>(_rows.size());
}

int AudioLibraryModelImpl::columnCount(const QModelIndex& /*parent*/) const
{
    return AudioLibraryView::NUMBER_OF_COLUMNS;
}

void AudioLibraryModelImpl::setDataInternal(int row, AudioLibraryView::Column column, const QVariant& data, int role)
{
    if (row >= 0 &&
        row < static_cast<int>(_rows.size()))
    {
        Row* row_data = _rows[row].get();

        if (role == Qt::DisplayRole)
        {
            if (column >= 0 &&
                column < AudioLibraryView::NUMBER_OF_COLUMNS)
            {
                row_data->display_role_data[column] = data;
                row_data->sort_role_data[column] = data.toString();
            }
        }
        else if (role == AudioLibraryView::MULTILINE_DISPLAY_ROLE && column == AudioLibraryView::ZERO)
        {
            row_data->multiline_display_role = data;
        }
        else if (role == AudioLibraryView::ID_ROLE)
        {
            row_data->id = data;
        }
        else if (role == AudioLibraryView::SORT_ROLE)
        {
            if (column >= 0 &&
                column < AudioLibraryView::NUMBER_OF_COLUMNS)
            {
                row_data->sort_role_data[column] = data.toString();
            }
        }
    }
}

void AudioLibraryModelImpl::setDecoration(int row, const AudioLibraryAlbum* album)
{
    if (row >= 0 &&
        row < static_cast<int>(_rows.size()))
    {
        Row* row_data = _rows[row].get();

        auto it = _decorations_for_album_ids.find(album->getUuid());
        if (it == _decorations_for_album_ids.end())
        {
            auto decoration = std::make_shared<Decoration>();
            decoration->bytes = album->getCover();
            decoration->variant = _default_icon;
            it = _decorations_for_album_ids.emplace(std::make_pair(album->getUuid(), decoration)).first;
        }

        row_data->decoration = it->second;
    }
}

QVariant AudioLibraryModelImpl::data(const QModelIndex& index, int role) const
{
    int row = index.row();
    int column = index.column();

    if (row >= 0 &&
        row < static_cast<int>(_rows.size()))
    {
        const Row* row_data = _rows[row].get();

        if (role == Qt::DisplayRole)
        {
            if (column >= 0 &&
                column < AudioLibraryView::NUMBER_OF_COLUMNS)
            {
                return row_data->display_role_data[column];
            }
        }
        else if (role == Qt::DecorationRole && column == AudioLibraryView::ZERO)
        {
            if (row_data->decoration->load_state == LoadState::NotLoaded)
            {
                row_data->decoration->load_state = LoadState::Requested;
                _requested_decorations.push_back(row_data->decoration);
            }

            if (row_data->decoration_load_state == LoadState::NotLoaded)
            {
                row_data->decoration_load_state = LoadState::Requested;
            }

            return row_data->decoration->variant;
        }
        else if (role == AudioLibraryView::MULTILINE_DISPLAY_ROLE && column == AudioLibraryView::ZERO)
        {
            return row_data->multiline_display_role;
        }
        else if (role == AudioLibraryView::ID_ROLE)
        {
            return row_data->id;
        }
        else if (role == AudioLibraryView::SORT_ROLE)
        {
            if (column >= 0 &&
                column < AudioLibraryView::NUMBER_OF_COLUMNS)
            {
                return row_data->sort_role_data[column];
            }
        }
    }

    return QVariant();
}

QVariant AudioLibraryModelImpl::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal &&
        role == Qt::DisplayRole &&
        section >= 0 &&
        section < _header_labels.size())
    {
        return _header_labels[section];
    }

    return QVariant();
}

void AudioLibraryModelImpl::sort(int column, Qt::SortOrder order)
{
    if (column < 0 ||
        column >= AudioLibraryView::NUMBER_OF_COLUMNS ||
        rowCount() == 0)
        return;

    QList<QPersistentModelIndex> parents;
    layoutAboutToBeChanged(parents, QAbstractItemModel::VerticalSortHint);

    // sort

    QCollator numeric_collator;
    numeric_collator.setNumericMode(true);

    if (order == Qt::AscendingOrder)
    {
        auto less_than = [column, numeric_collator](const std::unique_ptr<Row>& a, const std::unique_ptr<Row>& b){
            return numeric_collator.compare(a->sort_role_data[column], b->sort_role_data[column]) < 0;
        };

        std::ranges::stable_sort(_rows, less_than);
    }
    else
    {
        auto greater_than = [column, numeric_collator](const std::unique_ptr<Row>& a, const std::unique_ptr<Row>& b) {
            return numeric_collator.compare(a->sort_role_data[column], b->sort_role_data[column]) > 0;
        };

        std::ranges::stable_sort(_rows, greater_than);
    }

    // update persistent indexes

    std::vector<int> old_to_new_index;
    old_to_new_index.resize(_rows.size());

    for (size_t i = 0; i < _rows.size(); ++i)
        old_to_new_index[_rows[i]->index] = static_cast<int>(i);

    const QModelIndexList old_persistent_indexes = persistentIndexList();
    QModelIndexList new_persistent_indexes;

    for (const QModelIndex& index : old_persistent_indexes)
    {
        int new_row = old_to_new_index[index.row()];
        new_persistent_indexes.push_back(createIndex(new_row, index.column()));
    }

    changePersistentIndexList(old_persistent_indexes, new_persistent_indexes);

    updateRowIndexes();

    // done

    layoutChanged(parents, QAbstractItemModel::VerticalSortHint);
}

AudioLibraryModelImpl::Row* AudioLibraryModelImpl::createRow(const QUuid& id)
{
    if (_rows.size() + 1 > INT_MAX)
        return nullptr; // QModelIndex uses int, so we can't have more than INT_MAX rows

    auto row = std::make_unique<Row>();
    row->id = id;
    row->index = static_cast<int>(_rows.size());

    beginInsertRows(QModelIndex(), row->index, row->index);

    Row* result = row.get();
    _id_to_row_map[id] = result;
    _rows.push_back(std::move(row));

    endInsertRows();

    return result;
}

void AudioLibraryModelImpl::removeRow(const QUuid& id)
{
    auto found = _id_to_row_map.find(id);
    if (found != _id_to_row_map.end())
    {
        beginRemoveRows(QModelIndex(), found->second->index, found->second->index);

        _rows.erase(_rows.begin() + found->second->index);

        updateRowIndexes();

        _id_to_row_map.erase(found);

        endRemoveRows();
    }
}

AudioLibraryModelImpl::Row* AudioLibraryModelImpl::findRowForId(const QUuid& id) const
{
    auto found = _id_to_row_map.find(id);
    if (found != _id_to_row_map.end())
        return found->second;

    return nullptr;
}

QModelIndex AudioLibraryModelImpl::findIndexForId(const QUuid& id) const
{
    if (const Row* row = findRowForId(id))
    {
        return createIndex(row->index, 0);
    }

    return QModelIndex();
}

const AudioLibraryView* AudioLibraryModelImpl::getViewForIndex(const QModelIndex& index) const
{
    return _rows[index.row()]->view.get();
}

void AudioLibraryModelImpl::setHorizontalHeaderLabels(const QStringList& labels)
{
    _header_labels = labels;
}

std::vector<QUuid> AudioLibraryModelImpl::getAllIds() const
{
    std::vector<QUuid> ids;

    for (const auto& i : _id_to_row_map)
    {
        ids.push_back(i.first);
    }

    return ids;
}

const QIcon& AudioLibraryModelImpl::getDefaultIcon() const
{
    return _default_icon;
}

void AudioLibraryModelImpl::updateDecoration(const QModelIndex& index)
{
    int row = index.row();

    if (row >= 0 &&
        row < static_cast<int>(_rows.size()))
    {
        Row* row_data = _rows[row].get();

        if (row_data->decoration->load())
        {
            const QModelIndex i = this->index(row, AudioLibraryView::ZERO);
            dataChanged(i, i);
        }
    }
}

void AudioLibraryModelImpl::updateRowIndexes()
{
    for (size_t i = 0; i < _rows.size(); ++i)
        _rows[i]->index = static_cast<int>(i);
}

void AudioLibraryModelImpl::loadRequestedDecorations()
{
    // first, load requested decorations until we either run out of time or out of work
    // the most recently requested decoration should always be loaded first

    auto start_time = std::chrono::steady_clock::now();

    for (auto it = _requested_decorations.rbegin(), end = _requested_decorations.rend(); it != end; ++it)
    {
        const auto& decoration = *it;

        auto current_time = std::chrono::steady_clock::now();

        if (std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count() > 50)
        {
            // to avoid stalling, only a few decorations are loaded with each call of this function
            break;
        }

        decoration->load();
    }

    // notify the rows if their decorations have been loaded
    // this part is inexpensive, so no timing is used here

    for (const auto& row : _rows)
    {
        if (row->decoration_load_state == LoadState::Requested && row->decoration->load_state == LoadState::Done)
        {
            row->decoration_load_state = LoadState::Done;

            const QModelIndex i = index(row->index, AudioLibraryView::ZERO);
            dataChanged(i, i);
        }
    }
}

//=============================================================================

bool AudioLibraryModelImpl::Decoration::load()
{
    if (load_state == LoadState::Requested)
    {
        const bool ok = pixmap.loadFromData(bytes);
        if (ok)
            variant = QIcon(pixmap);

        load_state = LoadState::Done;

        return ok;
    }

    return false;
}

//=============================================================================

AudioLibraryModel::AudioLibraryModel(QObject* parent, AudioLibraryGroupUuidCache& group_uuids)
    : QObject(parent)
    , _group_uuids(group_uuids)
{
    _item_model = new AudioLibraryModelImpl(this);
}

void AudioLibraryModel::addItemInternal(const QUuid& id,
    const std::function<void(int row)>& item_factory,
    const std::function<std::unique_ptr<AudioLibraryView>()>& view_factory)
{
    _requested_ids.insert(id);

    if (_item_model->findRowForId(id))
    {
        // already exists, nothing to do
        return;
    }
    AudioLibraryModelImpl::Row* row = _item_model->createRow(id);
    if (!row)
        return;

    item_factory(row->index);

    row->view = view_factory();

    _item_model->dataChanged(_item_model->index(row->index, 0), _item_model->index(row->index, AudioLibraryView::NUMBER_OF_COLUMNS - 1));
}

void AudioLibraryModel::addGroupItem(const QString& name, const AudioLibraryAlbum* showcase_album, int number_of_albums, int number_of_tracks, const std::function<std::unique_ptr<AudioLibraryView>()>& view_factory)
{
    const QUuid id = _group_uuids.getUuidForGroup(name, showcase_album, number_of_albums, number_of_tracks);

    auto item_factory = [this, name, showcase_album, number_of_albums, number_of_tracks](int row) {

        _item_model->setDataInternal(row, AudioLibraryView::ZERO, name);
        _item_model->setDecoration(row, showcase_album);

        _item_model->setDataInternal(row, AudioLibraryView::NUMBER_OF_ALBUMS, QString::number(number_of_albums));
        _item_model->setDataInternal(row, AudioLibraryView::NUMBER_OF_TRACKS, QString::number(number_of_tracks));
    };

    addItemInternal(id, item_factory, view_factory);
}

void AudioLibraryModel::addAlbumItem(const AudioLibraryAlbum* album)
{
    const QUuid id = album->getUuid();

    auto item_factory = [this, album](int row) {

        QLatin1Char sep(' ');

        QString sort_key = album->getKey().getArtist() + sep +
            QString::number(album->getKey().getYear()) + sep +
            album->getKey().getAlbum();

        _item_model->setDataInternal(row, AudioLibraryView::ZERO, album->getKey().getArtist() + " - " + album->getKey().getAlbum());
        _item_model->setDecoration(row, album);
        _item_model->setDataInternal(row, AudioLibraryView::ZERO, album->getKey().getArtist() + QChar(QChar::LineSeparator) + album->getKey().getAlbum(), AudioLibraryView::MULTILINE_DISPLAY_ROLE);
        _item_model->setDataInternal(row, AudioLibraryView::ZERO, sort_key, AudioLibraryView::SORT_ROLE);

        setAlbumColumns(row, album);
        _item_model->setDataInternal(row, AudioLibraryView::ARTIST, album->getKey().getArtist());
        _item_model->setDataInternal(row, AudioLibraryView::NUMBER_OF_TRACKS, QString::number(album->getTracks().size()));

        int length_milliseconds = 0;
        for (const AudioLibraryTrack* track : album->getTracks())
            length_milliseconds += track->getLengthMs();

        setLengthColumn(row, length_milliseconds);
    };

    addItemInternal(id, item_factory, [album](){
        return std::make_unique<AudioLibraryViewAlbum>(album->getKey());
    });
}

void AudioLibraryModel::addTrackItem(const AudioLibraryTrack* track)
{
    const QUuid id = track->getUuid();

    auto item_factory = [this, track](int row) {

        QLatin1Char sep(' ');

        QString sort_key = track->getAlbum()->getKey().getArtist() + sep +
            QString::number(track->getAlbum()->getKey().getYear()) + sep +
            track->getAlbum()->getKey().getAlbum() + sep +
            QString::number(track->getDiscNumber()) + sep +
            QString::number(track->getTrackNumber());

        _item_model->setDataInternal(row, AudioLibraryView::ZERO, track->getArtist() + " - " + track->getTitle());
        _item_model->setDecoration(row, track->getAlbum());
        _item_model->setDataInternal(row, AudioLibraryView::ZERO, track->getArtist() + QChar(QChar::LineSeparator) + track->getTitle(), AudioLibraryView::MULTILINE_DISPLAY_ROLE);
        _item_model->setDataInternal(row, AudioLibraryView::ZERO, sort_key, AudioLibraryView::SORT_ROLE);

        setAlbumColumns(row, track->getAlbum());
        _item_model->setDataInternal(row, AudioLibraryView::ARTIST, track->getArtist());
        _item_model->setDataInternal(row, AudioLibraryView::TITLE, track->getTitle());
        if(track->getTrackNumber() != 0)
            _item_model->setDataInternal(row, AudioLibraryView::TRACK_NUMBER, QString::number(track->getTrackNumber()));
        if(track->getDiscNumber() != 0)
            _item_model->setDataInternal(row, AudioLibraryView::DISC_NUMBER, QString::number(track->getDiscNumber()));
        _item_model->setDataInternal(row, AudioLibraryView::ALBUM_ARTIST, track->getAlbumArtist());
        _item_model->setDataInternal(row, AudioLibraryView::COMMENT, track->getComment());
        _item_model->setDataInternal(row, AudioLibraryView::PATH, track->getFilepath());
        setDateTimeColumn(row, AudioLibraryView::DATE_MODIFIED, track->getLastModified());
        QString file_size = QLocale().formattedDataSize(track->getFileSize());
        _item_model->setDataInternal(row, AudioLibraryView::FILE_SIZE, file_size);
        _item_model->setDataInternal(row, AudioLibraryView::FILE_SIZE, QString::number(track->getFileSize()), AudioLibraryView::SORT_ROLE);
        _item_model->setDataInternal(row, AudioLibraryView::TAG_TYPES, track->getTagTypes());
        setLengthColumn(row, track->getLengthMs());
        _item_model->setDataInternal(row, AudioLibraryView::CHANNELS, QString::number(track->getChannels()));
        _item_model->setDataInternal(row, AudioLibraryView::BITRATE_KBS, QString::number(track->getBitrateKbs()) + QLatin1String(" kbit/s"));
        _item_model->setDataInternal(row, AudioLibraryView::SAMPLERATE_HZ, QString::number(track->getSampleRateHz()) + QLatin1String(" Hz"));
    };

    // no view for track items

    addItemInternal(id, item_factory, []() {
        return nullptr;
    });
}

QAbstractItemModel* AudioLibraryModel::getModel()
{
    return _item_model;
}

const QAbstractItemModel* AudioLibraryModel::getModel() const
{
    return _item_model;
}

void AudioLibraryModel::setHorizontalHeaderLabels(const QStringList& labels)
{
    _item_model->setHorizontalHeaderLabels(labels);
}

QUuid AudioLibraryModel::getItemId(const QModelIndex& index) const
{
    QModelIndex zero_column = index.sibling(index.row(), AudioLibraryView::ZERO);
    return zero_column.data(AudioLibraryView::ID_ROLE).toUuid();
}

QModelIndex AudioLibraryModel::getIndexForId(const QUuid& id) const
{
    return _item_model->findIndexForId(id);
}

const AudioLibraryView* AudioLibraryModel::getViewForIndex(const QModelIndex& index) const
{
    return _item_model->getViewForIndex(index);
}

QString AudioLibraryModel::getFilepathFromIndex(const QModelIndex& index) const
{
    QModelIndex path_column = index.sibling(index.row(), AudioLibraryView::PATH);
    return path_column.data().toString();
}

bool AudioLibraryModel::isDefaultIcon(const QIcon& icon) const
{
    return icon.cacheKey() == _item_model->getDefaultIcon().cacheKey();
}

void AudioLibraryModel::updateDecoration(const QModelIndex& index)
{
    _item_model->updateDecoration(index);
}

void AudioLibraryModel::removeId(const QUuid& id)
{
    _item_model->removeRow(id);
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

    std::vector<QUuid> ids_to_remove;

    for (const QUuid& id : _item_model->getAllIds())
        if (_requested_ids.find(id) == _requested_ids.end())
            ids_to_remove.push_back(id);

    for (const QUuid& id : ids_to_remove)
        removeId(id);
}

void AudioLibraryModel::setDateTimeColumn(int row, AudioLibraryView::Column column, const QDateTime& date)
{
    _item_model->setDataInternal(row, column, QLocale::system().toString(date, QLocale::ShortFormat));
    _item_model->setDataInternal(row, column, date.toString(Qt::ISODate), AudioLibraryView::SORT_ROLE);
}

void AudioLibraryModel::setLengthColumn(int row, int length_milliseconds)
{
    int length_seconds = length_milliseconds / 1000;

    QTime length_time = QTime(0, 0).addMSecs(length_milliseconds);

    QString formatted_length = length_time.toString(length_seconds < 3600 ? "mm:ss" : "hh:mm:ss");

    _item_model->setDataInternal(row, AudioLibraryView::LENGTH_SECONDS, formatted_length);
    _item_model->setDataInternal(row, AudioLibraryView::LENGTH_SECONDS, QString::number(length_seconds), AudioLibraryView::SORT_ROLE);
}

void AudioLibraryModel::setAlbumColumns(int row, const AudioLibraryAlbum* album)
{
    _item_model->setDataInternal(row, AudioLibraryView::ALBUM, album->getKey().getAlbum());

    if (album->getKey().getYear() != 0)
        _item_model->setDataInternal(row, AudioLibraryView::YEAR, QString::number(album->getKey().getYear()));

    _item_model->setDataInternal(row, AudioLibraryView::GENRE, album->getKey().getGenre());

    if (!album->getCover().isEmpty())
    {
        _item_model->setDataInternal(row, AudioLibraryView::COVER_CHECKSUM, QString::number(album->getKey().getCoverChecksum()));

        QString data_size = QLocale().formattedDataSize(album->getCover().size());

        _item_model->setDataInternal(row, AudioLibraryView::COVER_DATASIZE, data_size);
        _item_model->setDataInternal(row, AudioLibraryView::COVER_DATASIZE, QString::number(album->getCover().size()), AudioLibraryView::SORT_ROLE);
    }

    _item_model->setDataInternal(row, AudioLibraryView::COVER_TYPE, album->getCoverType());

    _item_model->setDataInternal(row, AudioLibraryView::COVER_WIDTH, QString::number(album->getCoverSize().width()));
    _item_model->setDataInternal(row, AudioLibraryView::COVER_HEIGHT, QString::number(album->getCoverSize().height()));
}

//=============================================================================

class AudioLibraryGroupUuidCache::Private
{
public:
    struct GroupData
    {
        QString name;
        QUuid showcase_album_uuid;
        int num_albums = 0;
        int num_tracks = 0;

        bool operator<(const GroupData& other) const
        {
            auto tie = [](const GroupData& g) {
                return std::tie(
                    g.name,
                    g.showcase_album_uuid,
                    g.num_albums,
                    g.num_tracks);
            };

            return tie(*this) < tie(other);
        }
    };

    std::map<GroupData, QUuid> _group_uuids;
};

AudioLibraryGroupUuidCache::AudioLibraryGroupUuidCache()
    : _p(new Private())
{
}

AudioLibraryGroupUuidCache::~AudioLibraryGroupUuidCache() = default;

/**
* Assigns a persistent uuid for a group with the given parameters.
*/
QUuid AudioLibraryGroupUuidCache::getUuidForGroup(const QString& name, const AudioLibraryAlbum* showcase_album, int number_of_albums, int number_of_tracks)
{
    // the uuid is only inserted if the group does not already exist in the map
    auto it = _p->_group_uuids.insert({
        Private::GroupData{name, showcase_album->getUuid(), number_of_albums, number_of_tracks},
        QUuid::createUuid() });

    return it.first->second;
}