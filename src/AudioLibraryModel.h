// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <QtCore/qcollator.h>
#include <QtGui/qstandarditemmodel.h>

#include "AudioLibraryView.h"

class AudioLibraryModel : public QStandardItemModel
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

    void addItem(const QString& id, const QString& name, const QIcon& icon, std::function<AudioLibraryView*()> view_factory);
    void addAlbumItem(const AudioLibraryAlbum* album);
    void addTrackItem(const AudioLibraryTrack* track);

    QString getItemId(QStandardItem* item) const;
    QStandardItem* getItemForId(const QString& id) const;
    const AudioLibraryView* getViewForItem(QStandardItem* item) const;
    QString getFilepathFromIndex(const QModelIndex& index) const;

private:
    void addItemInternal(const QString& id, const QIcon& icon,
        std::function<QStandardItem*(int row)> item_factory,
        std::function<AudioLibraryView*()> view_factory);
    void removeId(const QString& id);
    void onUpdateStarted();
    void onUpdateFinished();
    void setAdditionalColumn(int row, AudioLibraryView::Column column, const QString& text);

    std::unordered_map<QString, QStandardItem*> _id_to_item_map;
    std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>> _item_to_view_map;

    QIcon _default_icon;
    QCollator _numeric_collator;

    std::unordered_set<QString> _requested_ids;
};