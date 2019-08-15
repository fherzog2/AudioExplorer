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

class MainWindow : public QFrame
{
    Q_OBJECT

public:
    MainWindow(Settings& settings);
    ~MainWindow();

    virtual bool eventFilter(QObject* watched, QEvent* event) override;
    void setBreadCrumb(AudioLibraryView* view);

protected:
    virtual void closeEvent(QCloseEvent* e) override;

private:
    void onEditPreferences();
    void onShowFindWidget();
    void onFindNext();
    void onLibraryCacheLoading();
    void onLibraryLoadProgressed(int files_loaded, int files_in_cache);
    void onLibraryLoadFinished(int files_loaded, int files_in_cache, float duration_sec);
    void onCoverLoadFinished();
    void onShowDuplicateAlbums();
    void onBreadCrumbClicked();
    void onBreadCrumpReverse();
    void onDisplayModeChanged(AudioLibraryView::DisplayMode display_mode);
    void onDisplayModeSelected(int index);
    void onViewTypeSelected(int index);
    void onItemDoubleClicked(const QModelIndex &index);
    void onTableHeaderSectionClicked();
    void onTableHeaderContextMenu(const QPoint& pos);
    void onToggleDetails();
    void onModelSelectionChanged();

    void saveLibrary();
    void scanAudioDirs();
    const AudioLibraryView* getCurrentView() const;
    void updateCurrentView();
    void updateCurrentViewIfOlderThan(int msecs);
    void advanceIconSize(int direction);
    void addViewTypeTab(QWidget* view, const QString& friendly_name, const QString& internal_name);
    void getFilepathsFromIndex(const QModelIndex& index, std::vector<QString>& filepaths);
    void forEachFilepathAtIndex(const QModelIndex& index, std::function<void(const QString&)> callback);

    void addBreadCrumb(AudioLibraryView* view);
    void clearBreadCrumbs();
    void restoreBreadCrumb(QObject* object);
    bool findBreadcrumbId(const QString& id) const;

    void contextMenuEventForView(QAbstractItemView* view, QContextMenuEvent* event);
    static QString getVlcPath();
    void startVlc(const QList<QPersistentModelIndex>& indexes, bool only_add_to_playlist);

    void setCurrentSelectedIndex(const QModelIndex& index);

    Settings& _settings;

    AudioLibraryModel* _model = nullptr;

    ViewSelector _view_selector;

    std::vector<std::pair<QString, QWidget*>> _view_type_map;
    QTabBar* _view_type_tabs = nullptr;

    QListView* _list = nullptr;
    QTableView* _table = nullptr;
    QStackedWidget* _view_stack = nullptr;

    ThreadSafeAudioLibrary _library;
    AudioFilesLoader _audio_files_loader;

    std::chrono::steady_clock::time_point _last_view_update_time;
    bool _is_last_view_update_time_valid = false;

    QPointer<QWidget> _advanced_search_dialog = nullptr;

    struct ViewRestoreData
    {
        double _list_scroll_pos = 0;
        double _table_scroll_pos = 0;

        int _table_sort_indicator_section = 0;
        Qt::SortOrder _table_sort_indicator_order = Qt::AscendingOrder;

        QString _selected_item;
    };

    std::unique_ptr<ViewRestoreData> saveViewSettings() const;
    void restoreViewSettings(ViewRestoreData* restore_data);

    void restoreSettingsOnStart();
    void saveSettingsOnExit();

    void restoreDetailsSizeOnStart();

    struct Breadcrumb
    {
        std::unique_ptr<QObject, LateDeleter> _button;
        std::unique_ptr<AudioLibraryView> _view;
        std::unique_ptr<ViewRestoreData> _restore_data;
    };

    QHBoxLayout* _breadcrumb_layout = nullptr;
    std::vector<Breadcrumb> _breadcrumbs;

    QTabBar* _display_mode_tabs = nullptr;
    std::vector<std::pair<int, AudioLibraryView::DisplayMode>> _display_mode_tab_indexes;

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
};