// SPDX-License-Identifier: GPL-2.0-only
#include "AudioLibraryModel.h"

#include <array>
#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qcollator.h>

class AudioLibraryModelImpl : public QAbstractTableModel
{
public:
    AudioLibraryModelImpl(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    void setDataInternal(int row, AudioLibraryView::Column column, const QVariant& data, int role = Qt::DisplayRole);
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    struct Row
    {
        std::array<QVariant, AudioLibraryView::NUMBER_OF_COLUMNS> display_role_data;
        std::array<QString, AudioLibraryView::NUMBER_OF_COLUMNS> sort_role_data;

        QVariant decoration_role_data;
        QVariant multiline_display_role;
        QVariant id;
        std::unique_ptr<AudioLibraryView> view;

        int index = -1;

        QIcon icon() const { return qvariant_cast<QIcon>(decoration_role_data); }
    };

    Row* createRow(const QString& id);
    void removeRow(const QString& id);
    Row* findRowForId(const QString& id) const;
    QModelIndex findIndexForId(const QString& id) const;
    const AudioLibraryView* getViewForIndex(const QModelIndex& index) const;

    void setHorizontalHeaderLabels(const QStringList& labels);

    std::vector<QString> getAllIds() const;

private:
    void updateRowIndexes();

    std::vector<std::unique_ptr<Row>> _rows;
    std::unordered_map<QString, Row*> _id_to_row_map;
    QStringList _header_labels;
};

AudioLibraryModelImpl::AudioLibraryModelImpl(QObject* parent)
    : QAbstractTableModel(parent)
{
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
        else if (role == Qt::DecorationRole && column == AudioLibraryView::ZERO)
        {
            row_data->decoration_role_data = data;
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
            return row_data->decoration_role_data;
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

        std::stable_sort(_rows.begin(), _rows.end(), less_than);
    }
    else
    {
        auto greater_than = [column, numeric_collator](const std::unique_ptr<Row>& a, const std::unique_ptr<Row>& b) {
            return numeric_collator.compare(a->sort_role_data[column], b->sort_role_data[column]) > 0;
        };

        std::stable_sort(_rows.begin(), _rows.end(), greater_than);
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

AudioLibraryModelImpl::Row* AudioLibraryModelImpl::createRow(const QString& id)
{
    if (_rows.size() + 1 > INT_MAX)
        return nullptr; // QModelIndex uses int, so we can't have more than INT_MAX rows

    std::unique_ptr<Row> row(new Row());
    row->id = id;
    row->index = static_cast<int>(_rows.size());

    beginInsertRows(QModelIndex(), row->index, row->index);

    Row* result = row.get();
    _id_to_row_map[id] = result;
    _rows.push_back(std::move(row));

    endInsertRows();

    return result;
}

void AudioLibraryModelImpl::removeRow(const QString& id)
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

AudioLibraryModelImpl::Row* AudioLibraryModelImpl::findRowForId(const QString& id) const
{
    auto found = _id_to_row_map.find(id);
    if (found != _id_to_row_map.end())
        return found->second;

    return nullptr;
}

QModelIndex AudioLibraryModelImpl::findIndexForId(const QString& id) const
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

std::vector<QString> AudioLibraryModelImpl::getAllIds() const
{
    std::vector<QString> ids;

    for (const auto& i : _id_to_row_map)
    {
        ids.push_back(i.first);
    }

    return ids;
}

void AudioLibraryModelImpl::updateRowIndexes()
{
    for (size_t i = 0; i < _rows.size(); ++i)
        _rows[i]->index = static_cast<int>(i);
}

//=============================================================================

AudioLibraryModel::AudioLibraryModel(QObject* parent)
    : QObject(parent)
{
    _item_model = new AudioLibraryModelImpl(this);

    QPixmap default_pixmap = QPixmap(256, 256);
    default_pixmap.fill(Qt::transparent);

    _default_icon = default_pixmap;
}

void AudioLibraryModel::addItemInternal(const QString& id, const QIcon& icon,
    std::function<void(int row)> item_factory,
    std::function<AudioLibraryView*()> view_factory)
{
    _requested_ids.insert(id);

    if (AudioLibraryModelImpl::Row* existing_row = _item_model->findRowForId(id))
    {
        if (existing_row->icon().cacheKey() == _default_icon.cacheKey() &&
            !icon.isNull())
        {
            existing_row->decoration_role_data = icon;

            QModelIndex index = _item_model->index(existing_row->index, AudioLibraryView::ZERO);
            _item_model->dataChanged(index, index);
        }

        // already exists, nothing to do
        return;
    }

    AudioLibraryModelImpl::Row* row = _item_model->createRow(id);
    if (!row)
        return;

    item_factory(row->index);

    if(AudioLibraryView* view = view_factory())
        row->view.reset(view);

    _item_model->dataChanged(_item_model->index(row->index, 0), _item_model->index(row->index, AudioLibraryView::NUMBER_OF_COLUMNS - 1));
}

void AudioLibraryModel::addItem(const QString& id, const QString& name, const QIcon& icon, int number_of_albums, int number_of_tracks, std::function<AudioLibraryView*()> view_factory)
{
    auto item_factory = [this, name, icon, number_of_albums, number_of_tracks](int row) {

        _item_model->setDataInternal(row, AudioLibraryView::ZERO, name);
        _item_model->setDataInternal(row, AudioLibraryView::ZERO, icon.isNull() ? _default_icon : icon, Qt::DecorationRole);

        _item_model->setDataInternal(row, AudioLibraryView::NUMBER_OF_ALBUMS, QString::number(number_of_albums));
        _item_model->setDataInternal(row, AudioLibraryView::NUMBER_OF_TRACKS, QString::number(number_of_tracks));
    };

    addItemInternal(id, icon, item_factory, view_factory);
}

void AudioLibraryModel::addAlbumItem(const AudioLibraryAlbum* album)
{
    QString id = album->getId();
    QIcon icon = album->getCoverPixmap().isNull() ? _default_icon : album->getCoverPixmap();

    auto item_factory = [this, album, icon](int row) {

        QLatin1Char sep(' ');

        QString sort_key = album->getKey().getArtist() + sep +
            QString::number(album->getKey().getYear()) + sep +
            album->getKey().getAlbum();

        _item_model->setDataInternal(row, AudioLibraryView::ZERO, album->getKey().getArtist() + " - " + album->getKey().getAlbum());
        _item_model->setDataInternal(row, AudioLibraryView::ZERO, icon, Qt::DecorationRole);
        _item_model->setDataInternal(row, AudioLibraryView::ZERO, album->getKey().getArtist() + QChar(QChar::LineSeparator) + album->getKey().getAlbum(), AudioLibraryView::MULTILINE_DISPLAY_ROLE);
        _item_model->setDataInternal(row, AudioLibraryView::ZERO, sort_key, AudioLibraryView::SORT_ROLE);

        setAlbumColumns(row, album);
        _item_model->setDataInternal(row, AudioLibraryView::ARTIST, album->getKey().getArtist());
        _item_model->setDataInternal(row, AudioLibraryView::NUMBER_OF_TRACKS, QString::number(album->getTracks().size()));

        int length_milliseconds = 0;
        for (const AudioLibraryTrack* track : album->getTracks())
            length_milliseconds += track->_length_milliseconds;

        setLengthColumn(row, length_milliseconds);
    };

    addItemInternal(id, icon, item_factory, [album](){
        return new AudioLibraryViewAlbum(album->getKey());
    });
}

void AudioLibraryModel::addTrackItem(const AudioLibraryTrack* track)
{
    QString id = track->getId();
    QIcon icon = track->_album->getCoverPixmap().isNull() ? _default_icon : track->_album->getCoverPixmap();

    auto item_factory = [this, track, icon](int row) {

        QLatin1Char sep(' ');

        QString sort_key = track->_album->getKey().getArtist() + sep +
            QString::number(track->_album->getKey().getYear()) + sep +
            track->_album->getKey().getAlbum() + sep +
            QString::number(track->_disc_number) + sep +
            QString::number(track->_track_number);

        _item_model->setDataInternal(row, AudioLibraryView::ZERO, track->_artist + " - " + track->_title);
        _item_model->setDataInternal(row, AudioLibraryView::ZERO, icon, Qt::DecorationRole);
        _item_model->setDataInternal(row, AudioLibraryView::ZERO, track->_artist + QChar(QChar::LineSeparator) + track->_title, AudioLibraryView::MULTILINE_DISPLAY_ROLE);
        _item_model->setDataInternal(row, AudioLibraryView::ZERO, sort_key, AudioLibraryView::SORT_ROLE);

        setAlbumColumns(row, track->_album);
        _item_model->setDataInternal(row, AudioLibraryView::ARTIST, track->_artist);
        _item_model->setDataInternal(row, AudioLibraryView::TITLE, track->_title);
        if(track->_track_number != 0)
            _item_model->setDataInternal(row, AudioLibraryView::TRACK_NUMBER, QString::number(track->_track_number));
        if(track->_disc_number != 0)
            _item_model->setDataInternal(row, AudioLibraryView::DISC_NUMBER, QString::number(track->_disc_number));
        _item_model->setDataInternal(row, AudioLibraryView::ALBUM_ARTIST, track->_album_artist);
        _item_model->setDataInternal(row, AudioLibraryView::COMMENT, track->_comment);
        _item_model->setDataInternal(row, AudioLibraryView::PATH, track->_filepath);
        setDateTimeColumn(row, AudioLibraryView::DATE_MODIFIED, track->_last_modified);
        _item_model->setDataInternal(row, AudioLibraryView::TAG_TYPES, track->_tag_types);
        setLengthColumn(row, track->_length_milliseconds);
        _item_model->setDataInternal(row, AudioLibraryView::CHANNELS, QString::number(track->_channels));
        _item_model->setDataInternal(row, AudioLibraryView::BITRATE_KBS, QString::number(track->_bitrate_kbs) + QLatin1String(" kbit/s"));
        _item_model->setDataInternal(row, AudioLibraryView::SAMPLERATE_HZ, QString::number(track->_samplerate_hz) + QLatin1String(" Hz"));
    };

    // no view for track items

    addItemInternal(id, icon, item_factory, []() {
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

QString AudioLibraryModel::getItemId(const QModelIndex& index) const
{
    QModelIndex zero_column = index.sibling(index.row(), AudioLibraryView::ZERO);
    return zero_column.data(AudioLibraryView::ID_ROLE).toString();
}

QModelIndex AudioLibraryModel::getIndexForId(const QString& id) const
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
    return icon.cacheKey() == _default_icon.cacheKey();
}

void AudioLibraryModel::removeId(const QString& id)
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

    QStringList ids_to_remove;

    for (const QString& id : _item_model->getAllIds())
        if (_requested_ids.find(id) == _requested_ids.end())
            ids_to_remove.push_back(id);

    for (const QString& id : ids_to_remove)
        removeId(id);
}

void AudioLibraryModel::setDateTimeColumn(int row, AudioLibraryView::Column column, const QDateTime& date)
{
    _item_model->setDataInternal(row, column, date.toString(Qt::DefaultLocaleShortDate));
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

    if (!album->getCoverPixmap().isNull())
    {
        _item_model->setDataInternal(row, AudioLibraryView::COVER_WIDTH, QString::number(album->getCoverPixmap().width()));
        _item_model->setDataInternal(row, AudioLibraryView::COVER_HEIGHT, QString::number(album->getCoverPixmap().height()));
    }
}