// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "AudioLibraryView.h"

class AudioLibraryModelImpl;

class AudioLibraryModel : public QObject
{
public:
    AudioLibraryModel(QObject* parent);

    class IncrementalUpdateScope
    {
    public:
        IncrementalUpdateScope(AudioLibraryModel& model);
        ~IncrementalUpdateScope();

    private:
        AudioLibraryModel& _model;
    };

    void addItem(const QString& id, const QString& name, const AudioLibraryAlbum* showcase_album, int number_of_albums, int number_of_tracks, const std::function<std::unique_ptr<AudioLibraryView>()>& view_factory);
    void addAlbumItem(const AudioLibraryAlbum* album);
    void addTrackItem(const AudioLibraryTrack* track);

    QAbstractItemModel* getModel();
    const QAbstractItemModel* getModel() const;
    void setHorizontalHeaderLabels(const QStringList& labels);
    QString getItemId(const QModelIndex& index) const;
    QModelIndex getIndexForId(const QString& id) const;
    const AudioLibraryView* getViewForIndex(const QModelIndex& index) const;
    QString getFilepathFromIndex(const QModelIndex& index) const;

    bool isDefaultIcon(const QIcon& icon) const;

    void updateDecoration(const QModelIndex& index);

private:
    void addItemInternal(const QString& id,
        const std::function<void(int row)>& item_factory,
        const std::function<std::unique_ptr<AudioLibraryView>()>& view_factory);
    void removeId(const QString& id);
    void onUpdateStarted();
    void onUpdateFinished();
    void setDateTimeColumn(int row, AudioLibraryView::Column column, const QDateTime& date);
    void setLengthColumn(int row, int length_milliseconds);
    void setAlbumColumns(int row, const AudioLibraryAlbum* album);

    AudioLibraryModelImpl* _item_model;

    std::unordered_set<QString> _requested_ids;
};