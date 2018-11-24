// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <atomic>
#include <thread>
#include <QtCore/qpointer.h>
#include <QtGui/qstandarditemmodel.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qframe.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qlistview.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qtableview.h>
#include <QtWidgets/qstackedwidget.h>

#include "Settings.h"
#include "AudioLibrary.h"
#include "AudioLibraryView.h"

class SpinLock
{
public:
    void lock()
    {
        while (locked.test_and_set(std::memory_order_acquire))
            ;
    }

    void unlock()
    {
        locked.clear(std::memory_order_release);
    }

private:
    std::atomic_flag locked = ATOMIC_FLAG_INIT;
};

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

class MainWindow : public QFrame
{
    Q_OBJECT

public:
    MainWindow(Settings& settings);
    ~MainWindow();

    virtual bool eventFilter(QObject* watched, QEvent* event) override;
    void setBreadCrumb(AudioLibraryView* view);

signals:
    void libraryCacheLoaded();
    void libraryLoadProgressed(int files_loaded, int files_skipped);
    void libraryLoadFinished(int files_loaded, int files_skipped, float duration_sec);

protected:
    virtual void closeEvent(QCloseEvent* e) override;

private:
    void onEditPreferences();
    void onOpenAdvancedSearch();
    void onLibraryCacheLoaded();
    void onLibraryLoadProgressed(int files_loaded, int files_skipped);
    void onLibraryLoadFinished(int files_loaded, int files_skipped, float duration_sec);
    void onShowDuplicateAlbums();
    void onSearchEnterPressed();
    void onBreadCrumbClicked();
    void onBreadCrumpReverse();
    void onDisplayModeChanged(AudioLibraryView::DisplayMode display_mode);
    void onDisplayModeSelected(int index);
    void onViewTypeSelected(int index);
    void onItemDoubleClicked(const QModelIndex &index);

    void saveLibrary();
    static void loadLibrary(const Settings& settings, AudioLibrary& library);
    void scanAudioDirs();
    void abortScanAudioDirs();
    void scanAudioDirsThreadFunc(QStringList audio_dir_paths);
    const AudioLibraryView* getCurrentView() const;
    void updateCurrentView();
    void advanceIconSize(int direction);
    void addViewTypeTab(QWidget* view, const QString& friendly_name, const QString& internal_name);
    void getFilepathsFromIndex(const QModelIndex& index, std::vector<QString>& filepaths);
    void forEachFilepathAtIndex(const QModelIndex& index, std::function<void(const QString&)> callback);

    void addBreadCrumb(AudioLibraryView* view);
    void clearBreadCrumbs();
    void restoreBreadCrumb(QObject* object);

    Settings& _settings;

    QStandardItemModel* _model = nullptr;

    std::vector<std::pair<QString, QWidget*>> _view_type_map;
    QTabBar* _view_type_tabs = nullptr;

    QListView* _list = nullptr;
    QTableView* _table = nullptr;
    QStackedWidget* _view_stack = nullptr;
    AudioLibrary _library;
    SpinLock _library_spin_lock;
    std::atomic_bool _abort_flag;
    std::thread _library_load_thread;
    std::chrono::steady_clock::time_point _last_view_update_time;
    bool _is_last_view_update_time_valid = false;
    std::atomic_bool _library_cache_loaded;

    QLineEdit* _search_field = nullptr;
    QPointer<QWidget> _advanced_search_dialog = nullptr;

    std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>> _views_for_items;

    QHBoxLayout* _breadcrumb_layout = nullptr;
    std::vector<std::unique_ptr<QObject, LateDeleter>> _breadcrumb_buttons;
    std::vector<std::unique_ptr<AudioLibraryView>> _breadcrumb_views;
    std::vector<double> _breadcrump_saved_scrollpos;

    QTabBar* _display_mode_tabs = nullptr;
    std::vector<std::pair<int, AudioLibraryView::DisplayMode>> _display_mode_tab_indexes;

    // for each combination of display modes, remember the users choice
    std::vector<std::pair<std::vector<AudioLibraryView::DisplayMode>, AudioLibraryView::DisplayMode>> _selected_display_modes;

    QLabel* _progress_label = nullptr;

    bool _is_dragging = false;
    QPoint _drag_start_pos;
    std::vector<QModelIndex> _dragged_indexes;

    std::vector<int> _icon_size_steps;
};