// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "AudioLibraryView.h"

class AudioLibraryModelImpl;
class AudioLibraryGroupUuidCache;

class AudioLibraryModel : public QObject
{
public:
    AudioLibraryModel(QObject* parent, AudioLibraryGroupUuidCache& group_uuids);

    class IncrementalUpdateScope
    {
    public:
        IncrementalUpdateScope(AudioLibraryModel& model);
        ~IncrementalUpdateScope();

    private:
        AudioLibraryModel& _model;
    };

    void addGroupItem(const QString& name, const AudioLibraryAlbum* showcase_album, int number_of_albums, int number_of_tracks, const std::function<std::unique_ptr<AudioLibraryView>()>& view_factory);
    void addAlbumItem(const AudioLibraryAlbum* album);
    void addTrackItem(const AudioLibraryTrack* track);

    QAbstractItemModel* getModel();
    const QAbstractItemModel* getModel() const;
    void setHorizontalHeaderLabels(const QStringList& labels);
    QUuid getItemId(const QModelIndex& index) const;
    QModelIndex getIndexForId(const QUuid& id) const;
    const AudioLibraryView* getViewForIndex(const QModelIndex& index) const;
    QString getFilepathFromIndex(const QModelIndex& index) const;

    bool isDefaultIcon(const QIcon& icon) const;

    void updateDecoration(const QModelIndex& index);

private:
    void addItemInternal(const QUuid& id,
        const std::function<void(int row)>& item_factory,
        const std::function<std::unique_ptr<AudioLibraryView>()>& view_factory);
    void removeId(const QUuid& id);
    void onUpdateStarted();
    void onUpdateFinished();
    void setDateTimeColumn(int row, AudioLibraryView::Column column, const QDateTime& date);
    void setLengthColumn(int row, int length_milliseconds);
    void setAlbumColumns(int row, const AudioLibraryAlbum* album);

    AudioLibraryModelImpl* _item_model;

    std::unordered_set<QUuid> _requested_ids;
    AudioLibraryGroupUuidCache& _group_uuids;
};

class AudioLibraryGroupUuidCache
{
public:
    AudioLibraryGroupUuidCache();
    ~AudioLibraryGroupUuidCache();

    QUuid getUuidForGroup(const QString& name, const AudioLibraryAlbum* showcase_album, int number_of_albums, int number_of_tracks);

private:
    class Private;
    std::unique_ptr<Private> _p;
};