// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <QtCore/qpointer.h>
#include <QtGui/qstandarditemmodel.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qframe.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qlistview.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qradiobutton.h>
#include <QtWidgets/qsplitter.h>
#include <QtWidgets/qstatusbar.h>
#include <QtWidgets/qtableview.h>
#include <QtWidgets/qstackedwidget.h>
#include <QtWidgets/QToolBar>

#include "Settings.h"
#include "AudioLibrary.h"
#include "AudioLibraryView.h"
#include "AudioLibraryModel.h"
#include "DetailsPane.h"
#include "ThreadSafeAudioLibrary.h"

/**
* For managing a QObject with std::unique_ptr
*/
struct LateDeleter
{
    void operator()(QObject* object) const
    {
        if(object)
            object->deleteLater();
    }
};

class ViewSelector : public QFrame
{
    Q_OBJECT

public:
    ViewSelector();

    std::unique_ptr<AudioLibraryView> getSelectedView() const;
    void triggerDefaultView();
    void setButtonCheckedFromId(const QString& id);

signals:
    void selectionChanged();

private:
    void radioButtonSelected();

    QRadioButton* _artist_button = nullptr;
    QRadioButton* _album_button = nullptr;
    QRadioButton* _track_button = nullptr;
    QRadioButton* _year_button = nullptr;
    QRadioButton* _genre_button = nullptr;

    QLineEdit* _filter_box = nullptr;
};

struct ViewRestoreData
{
    double _list_scroll_pos = 0;
    double _table_scroll_pos = 0;

    int _table_sort_indicator_section = 0;
    Qt::SortOrder _table_sort_indicator_order = Qt::AscendingOrder;

    QUuid _selected_item;
};

class History
{
public:
    struct Item
    {
        Item() = default;
        Item(Item&& other) = default;
        Item& operator=(Item&& other) = default;
        ~Item() = default;

        std::unique_ptr<AudioLibraryView> view;
        bool is_top_level_view = false;
        std::unique_ptr<ViewRestoreData> restore_data; //!< maybe null
    };

    void addItem(std::unique_ptr<AudioLibraryView> view, bool is_top_level_view, ViewRestoreData* restore_data_for_previous_view);
    std::vector<const Item*> getCurrentItems() const;
    bool canGoBack() const;
    bool canGoForward() const;
    void back();
    void forward();

private:
    std::vector<Item> _items;
    size_t _current_item = 0;
};

class MainWindow : public QFrame
{
    Q_OBJECT

public:
    MainWindow(Settings& settings, ThreadSafeAudioLibrary& library, AudioFilesLoader& audio_files_loader);

    virtual bool eventFilter(QObject* watched, QEvent* event) override;
    void setBreadCrumb(std::unique_ptr<AudioLibraryView> view);

Q_SIGNALS:
    void checkLanguageChanged();

protected:
    virtual void closeEvent(QCloseEvent* e) override;

private:
    void onEditPreferences();
    void onShowFindWidget();
    void onFindNext();
    void onLibraryCacheLoading();
    void onLibraryLoadProgressed(int files_loaded, int files_in_cache);
    void onLibraryLoadFinished(int files_loaded, int files_in_cache, float duration_sec);
    void onShowDuplicateAlbums();
    void onBreadCrumbClicked();
    void onHistoryBack();
    void onHistoryForward();
    void onDisplayModeChanged(AudioLibraryView::DisplayMode display_mode);
    void onViewTypeSelected(QWidget* view);
    void onItemDoubleClicked(const QModelIndex &index);
    void onTableHeaderSectionClicked();
    void onTableHeaderContextMenu(const QPoint& pos);
    void onModelCurrentChanged(const QModelIndex& current, const QModelIndex& previous);
    void onModelDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles);

    void saveLibrary();
    void scanAudioDirs();
    void selectRandomItem();
    const AudioLibraryView* getCurrentView() const;
    void updateCurrentView();
    void updateCurrentViewIfOlderThan(int msecs);
    void advanceIconSize(int direction);
    void addViewTypeAction(QWidget* view, const QString& friendly_name, const QString& internal_name);
    void getFilepathsFromIndex(const QModelIndex& index, std::vector<QString>& filepaths);
    void forEachFilepathAtIndex(const QModelIndex& index, std::function<void(const QString&)> callback);

    void updateAfterHistoryChange();
    void addBreadCrumb(std::unique_ptr<AudioLibraryView> view);
    void restoreBreadCrumb(QObject* object);
    bool findBreadcrumbId(const QString& id) const;

    void contextMenuEventForView(QAbstractItemView* view, QContextMenuEvent* event);
    static QString getVlcPath();
    void startVlc(const QList<QPersistentModelIndex>& indexes, bool only_add_to_playlist);

    void setCurrentSelectedIndex(const QModelIndex& index);

    Settings& _settings;

    AudioLibraryGroupUuidCache _group_uuids;
    AudioLibraryModel* _model = nullptr;

    ViewSelector _view_selector;

    std::vector<std::pair<QAction*, QString>> _view_type_actions;

    QToolBar* _toolbar = nullptr;

    QListView* _list = nullptr;
    QTableView* _table = nullptr;
    QStackedWidget* _view_stack = nullptr;

    ThreadSafeAudioLibrary& _library;
    AudioFilesLoader& _audio_files_loader;

    std::chrono::steady_clock::time_point _last_view_update_time;
    bool _is_last_view_update_time_valid = false;

    QPointer<QWidget> _advanced_search_dialog = nullptr;

    std::unique_ptr<ViewRestoreData> saveViewSettings() const;
    void restoreViewSettings(ViewRestoreData* restore_data);

    void restoreSettingsOnStart();
    void saveSettingsOnExit();

    void restoreDetailsSizeOnStart();

    History _history;
    QAction* _history_back_action = nullptr;
    QAction* _history_forward_action = nullptr;

    QHBoxLayout* _breadcrumb_layout = nullptr;
    std::vector<std::unique_ptr<QObject, LateDeleter>> _breadcrumb_buttons;

    std::vector<std::pair<QAction*, AudioLibraryView::DisplayMode>> _display_mode_actions;
    QAction* _separator_display_modes_view_types = nullptr;

    QString _current_view_id;
    std::unique_ptr<AudioLibraryView::DisplayMode> _current_display_mode;

    // for each combination of display modes, remember the users choice
    std::vector<std::pair<std::vector<AudioLibraryView::DisplayMode>, AudioLibraryView::DisplayMode>> _selected_display_modes;

    std::unordered_set<AudioLibraryView::Column> _hidden_columns;

    QStatusBar* _status_bar = nullptr;

    bool _is_dragging = false;
    QPoint _drag_start_pos;
    std::vector<QModelIndex> _dragged_indexes;

    std::vector<int> _icon_size_steps;

    QWidget* _find_widget = nullptr;
    QLineEdit* _find_widget_line_edit = nullptr;

    QSplitter* _details_splitter = nullptr;
    DetailsPane* _details = nullptr;
    QAction* _details_action = nullptr;
};