// SPDX-License-Identifier: GPL-2.0-only
#include "MainWindow.h"

#include <chrono>
#include <unordered_set>
#include <unordered_map>
#include <set>
#include <mutex>
#include <QtCore/qmimedata.h>
#include <QtCore/qsavefile.h>
#include <QtCore/qstandardpaths.h>
#include <QtCore/qtimer.h>
#include <QtGui/qdesktopservices.h>
#include <QtGui/qdrag.h>
#include <QtGui/qevent.h>
#include <QtGui/qimagereader.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qmenubar.h>
#include <QtWidgets/qscrollbar.h>
#include <QtWidgets/qshortcut.h>
#include <QtWidgets/qfiledialog.h>
#include <QtWidgets/qheaderview.h>
#include "project_version.h"
#include "SettingsEditorWindow.h"
#include "AdvancedSearchDialog.h"

namespace
{
    template<class RECEIVER, class FUNC>
    QAction* addMenuAction(QMenu& menu, const QString& text, RECEIVER rec, FUNC func, const QKeySequence& shortcut = 0)
    {
        QAction* action = new QAction(&menu);
        action->setText(text);
        action->setShortcut(shortcut);
        QObject::connect(action, &QAction::triggered, rec, func);
        menu.addAction(action);
        return action;
    }

    template<class T>
    QPushButton* createViewCreatorButton(MainWindow* parent)
    {
        std::unique_ptr<T> dummy(new T());

        QPushButton* button = new QPushButton(dummy->getDisplayName(), parent);
        QObject::connect(button, &QPushButton::clicked, parent, [parent](){
            parent->setBreadCrumb(new T());
        });

        return button;
    }

    template<class FUNC>
    void forEachFileInDirectory(const QString& dirpath, FUNC func)
    {
        std::vector<QString> queue;
        queue.push_back(dirpath);

        while (!queue.empty())
        {
            QString current_dir = queue.front();
            queue.erase(queue.begin());

            QDir dir(current_dir);

            QStringList subdirs = dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
            for (const QString& subdir : subdirs)
                queue.push_back(current_dir + "/" + subdir);

            QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
            for (const QFileInfo& file : files)
            {
                if (!func(file))
                    return;
            }
        }
    }
}

//=============================================================================

MainWindow::MainWindow(Settings& settings)
    : _settings(settings)
    , _abort_flag(false)
{
    setWindowTitle(APPLICATION_NAME);

    auto menubar = new QMenuBar(this);
    auto filemenu = menubar->addMenu("&File");
    addMenuAction(*filemenu, "Preferences...", this, &MainWindow::onEditPreferences);
    filemenu->addSeparator();
    addMenuAction(*filemenu, "Exit", this, &MainWindow::close, Qt::ALT + Qt::Key_F4);

    auto viewmenu = menubar->addMenu("View");
    addMenuAction(*viewmenu, "Previous view", this, &MainWindow::onBreadCrumpReverse, Qt::Key_Backspace);
    addMenuAction(*viewmenu, "Search...", this, &MainWindow::onOpenAdvancedSearch);
    addMenuAction(*viewmenu, "Badly tagged albums", this, &MainWindow::onShowDuplicateAlbums);
    addMenuAction(*viewmenu, "Reload all files", this, &MainWindow::scanAudioDirs, Qt::Key_F5);

    QPushButton* artists_button = createViewCreatorButton<AudioLibraryViewAllArtists>(this);
    QPushButton* albums_button = createViewCreatorButton<AudioLibraryViewAllAlbums>(this);
    QPushButton* tracks_button = createViewCreatorButton<AudioLibraryViewAllTracks>(this);
    QPushButton* years_button = createViewCreatorButton<AudioLibraryViewAllYears>(this);
    QPushButton* genres_button = createViewCreatorButton<AudioLibraryViewAllGenres>(this);

    _search_field = new QLineEdit(this);
    _search_field->setPlaceholderText("Search");

    _display_mode_tabs = new QTabBar(this);
    _display_mode_tabs->setAutoHide(true);

    _model = new QStandardItemModel(this);

    QStringList model_headers;
    for (const auto& column : AudioLibraryView::columnToStringMapping())
    {
        model_headers.push_back(AudioLibraryView::getColumnFriendlyName(column.first));
    }
    _model->setHorizontalHeaderLabels(model_headers);

    _icon_size_steps = { 64, 96, 128, 192, 256 };

    int default_icon_size = 128;

    for (int s : _icon_size_steps)
    {
        if (_settings.main_window_icon_size.getValue() == s)
            default_icon_size = s;
    }

    _list = new QListView(this);
    _list->setModel(_model);
    _list->setViewMode(QListView::IconMode);
    _list->setWordWrap(true);
    _list->setIconSize(QSize(default_icon_size, default_icon_size));
    _list->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    _list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    _list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _list->setDragEnabled(false);
    _list->viewport()->installEventFilter(this);
    _list->setSelectionMode(QAbstractItemView::ExtendedSelection);

    _table = new QTableView(this);
    _table->setSortingEnabled(true);
    _table->setModel(_model);
    _table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    _table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    _table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _table->viewport()->installEventFilter(this);
    _table->setSelectionBehavior(QAbstractItemView::SelectRows);
    _table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    _table->horizontalHeader()->setSectionsMovable(true);

    // restore table column widths

    const QHash<QString, QVariant> column_widths = _settings.audio_library_view_column_widths.getValue();
    for (const auto& column : AudioLibraryView::columnToStringMapping())
    {
        auto found = column_widths.find(column.second);
        if (found != column_widths.end())
        {
            bool int_ok;
            int column_width = found.value().toInt(&int_ok);
            if (int_ok)
                _table->setColumnWidth(column.first, column_width);
        }
    }

    _view_stack = new QStackedWidget(this);
    _view_stack->addWidget(_list);
    _view_stack->addWidget(_table);
    _view_stack->setCurrentWidget(_list);

    _view_type_tabs = new QTabBar(this);
    addViewTypeTab(_list, "Icons", "icons");
    addViewTypeTab(_table, "Table", "table");

    for (const auto& type : _view_type_map)
        if (type.first == _settings.main_window_view_type.getValue())
        {
            _view_stack->setCurrentWidget(type.second);
            for (int i = 0, end = _view_type_tabs->count(); i < end; ++i)
            {
                QVariant var = _view_type_tabs->tabData(i);
                if (var.toString() == type.first)
                {
                    _view_type_tabs->setCurrentIndex(i);
                    break;
                }
            }
            break;
        }

    _progress_label = new QLabel(this);

    QVBoxLayout* vbox = new QVBoxLayout(this);
    vbox->setMenuBar(menubar);
    QHBoxLayout* view_buttons_layout = new QHBoxLayout();
    view_buttons_layout->addWidget(artists_button);
    view_buttons_layout->addWidget(albums_button);
    view_buttons_layout->addWidget(tracks_button);
    view_buttons_layout->addWidget(years_button);
    view_buttons_layout->addWidget(genres_button);
    view_buttons_layout->addStretch(1);
    view_buttons_layout->addWidget(_search_field);
    vbox->addLayout(view_buttons_layout);
    _breadcrumb_layout = new QHBoxLayout();
    QHBoxLayout* breadcrumb_layout_wrapper = new QHBoxLayout();
    breadcrumb_layout_wrapper->addLayout(_breadcrumb_layout);
    breadcrumb_layout_wrapper->addStretch();
    breadcrumb_layout_wrapper->addWidget(_display_mode_tabs);
    breadcrumb_layout_wrapper->addWidget(_view_type_tabs);
    vbox->addLayout(breadcrumb_layout_wrapper);
    vbox->addWidget(_view_stack);
    vbox->addWidget(_progress_label);

    connect(this, &MainWindow::libraryLoadProgressed, this, &MainWindow::onLibraryLoadProgressed);
    connect(this, &MainWindow::libraryLoadFinished, this, &MainWindow::onLibraryLoadFinished);
    connect(_search_field, &QLineEdit::returnPressed, this, &MainWindow::onSearchEnterPressed);
    connect(_display_mode_tabs, &QTabBar::tabBarClicked, this, &MainWindow::onDisplayModeSelected);
    connect(_view_type_tabs, &QTabBar::tabBarClicked, this, &MainWindow::onViewTypeSelected);
    connect(_list, &QListView::doubleClicked, this, &MainWindow::onItemDoubleClicked);
    connect(_table, &QListView::doubleClicked, this, &MainWindow::onItemDoubleClicked);

    setBreadCrumb(new AudioLibraryViewAllArtists());

    _settings.main_window_geometry.restore(this);

    show();

    loadLibrary();
    scanAudioDirs();

    QTimer::singleShot(1, this, [=](){
        updateCurrentView();
    });
}

MainWindow::~MainWindow()
{
    abortScanAudioDirs();
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    QHash<QString, QVariant> column_widths;
    for (const auto& column : AudioLibraryView::columnToStringMapping())
    {
        if (!_table->isColumnHidden(column.first))
        {
            column_widths[column.second] = _table->columnWidth(column.first);
        }
    }
    _settings.audio_library_view_column_widths.setValue(column_widths);

    for(const auto& i : _view_type_map)
        if (i.second == _view_stack->currentWidget())
        {
            _settings.main_window_view_type.setValue(i.first);
            break;
        }

    _settings.main_window_icon_size.setValue(_list->iconSize().width());

    _settings.main_window_geometry.save(this);
    saveLibrary();

    QFrame::closeEvent(e);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _list->viewport() && event->type() == QEvent::Wheel)
    {
        QWheelEvent* we = static_cast<QWheelEvent*>(event);
        if (we->modifiers().testFlag(Qt::ControlModifier))
        {
            advanceIconSize(we->angleDelta().y() > 0 ? 1 : -1);

            event->accept();
            return true;
        }
    }

    QAbstractItemView* view = nullptr;
    if (watched == _list->viewport())
        view = _list;
    else if (watched == _table->viewport())
        view = _table;

    if (view)
    {
        switch (event->type())
        {
        case QEvent::MouseButtonPress:
        {
            QMouseEvent* me = static_cast<QMouseEvent*>(event);
            QModelIndex index = view->indexAt(me->pos());
            if (index.isValid())
            {
                _is_dragging = true;
                _drag_start_pos = me->pos();
                _dragged_indexes.clear();

                QModelIndexList selected_indexes = view->selectionModel()->selectedIndexes();
                if (selected_indexes.indexOf(index) < 0)
                {
                    _dragged_indexes.push_back(index);
                }
                else
                {
                    std::unordered_set<int> rows;

                    for (const QModelIndex& index : selected_indexes)
                    {
                        if (rows.find(index.row()) == rows.end())
                        {
                            rows.insert(index.row());
                            _dragged_indexes.push_back(index);
                        }
                    }
                }
            }

            break;
        }
        case QEvent::MouseButtonRelease:
            _is_dragging = false;
            _drag_start_pos = QPoint();
            _dragged_indexes.clear();
            break;
        case QEvent::MouseMove:
            if (_is_dragging)
            {
                QMouseEvent* me = static_cast<QMouseEvent*>(event);
                if ((me->pos() - _drag_start_pos).manhattanLength() >= QApplication::startDragDistance())
                {
                    QList<QUrl> urls;
                    for (const QModelIndex& index : _dragged_indexes)
                    {
                        forEachFilepathAtIndex(index, [&urls](const QString& filepath){
                            urls.push_back(QUrl::fromLocalFile(filepath));
                        });
                    }

                    QMimeData* data = new QMimeData();
                    data->setUrls(urls);

                    QDrag drag(this);
                    drag.setMimeData(data);

                    if (!urls.empty())
                    {
                        drag.exec();
                        _is_dragging = false;
                        _drag_start_pos = QPoint();
                        _dragged_indexes.clear();
                    }
                }

                return true;
            }
            break;
        default:
            break;
        }
    }

    return QFrame::eventFilter(watched, event);
}

void MainWindow::onEditPreferences()
{
    QStringList old_audio_dir_paths = _settings.audio_dir_paths.getValue();

    SettingsEditorDialog(this, _settings).exec();

    if (_settings.audio_dir_paths.getValue() != old_audio_dir_paths)
    {
        _library.cleanupTracksOutsideTheseDirectories(_settings.audio_dir_paths.getValue());
        scanAudioDirs();
    }
}

void MainWindow::onOpenAdvancedSearch()
{
    if (_advanced_search_dialog)
        return;

    AdvancedSearchDialog * advanced_search_dialog = new AdvancedSearchDialog(this);
    _advanced_search_dialog = advanced_search_dialog;
    _advanced_search_dialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(advanced_search_dialog, &AdvancedSearchDialog::searchRequested, this, [=](AudioLibraryViewAdvancedSearch::Query query) {
        setBreadCrumb(new AudioLibraryViewAdvancedSearch(query));
    });
}

void MainWindow::onLibraryLoadProgressed(int files_loaded, int files_skipped)
{
    _progress_label->setText(QString("Scan audio dirs: %1 files loaded, %2 files skipped").arg(files_loaded).arg(files_skipped));

    auto now = std::chrono::steady_clock::now();
    auto time_span = now - _last_view_update_time;
    if (std::chrono::duration_cast<std::chrono::milliseconds>(time_span).count() > 5000)
    {
        updateCurrentView();
    }
}

void MainWindow::onLibraryLoadFinished(int files_loaded, int files_skipped, float duration_sec)
{
    _progress_label->setText(QString("Scan audio dirs: %1 files loaded, %2 files skipped in %3 seconds").arg(files_loaded).arg(files_skipped).arg(duration_sec, 0, 'f', 1));

    updateCurrentView();
}

void MainWindow::onShowDuplicateAlbums()
{
    setBreadCrumb(new AudioLibraryViewDuplicateAlbums());
}

void MainWindow::onSearchEnterPressed()
{
    if (_search_field->text().isEmpty())
        return;

    setBreadCrumb(new AudioLibraryViewSimpleSearch(_search_field->text()));
}

void MainWindow::onBreadCrumbClicked()
{
    restoreBreadCrumb(sender());
}

void MainWindow::onBreadCrumpReverse()
{
    if (_breadcrumb_views.size() > 1)
    {
        QObject* second_last = _breadcrumb_buttons[_breadcrumb_buttons.size() - 2].get();

        restoreBreadCrumb(second_last);
    }
}

void MainWindow::onDisplayModeChanged(AudioLibraryView::DisplayMode display_mode)
{
    auto modes = getCurrentView()->getSupportedModes();

    auto found = std::find_if(_selected_display_modes.begin(), _selected_display_modes.end(), [modes](const std::pair<std::vector<AudioLibraryView::DisplayMode>, AudioLibraryView::DisplayMode>& i) {
        return i.first == modes;
    });

    if (found != _selected_display_modes.end())
    {
        found->second = display_mode;
    }
    else
    {
        _selected_display_modes.push_back(std::make_pair(modes, display_mode));
    }

    updateCurrentView();
}

void MainWindow::onDisplayModeSelected(int index)
{
    for (const auto& i : _display_mode_tab_indexes)
    {
        if (i.first == index)
        {
            onDisplayModeChanged(i.second);
            break;
        }
    }
}

void MainWindow::onViewTypeSelected(int index)
{
    QVariant var = _view_type_tabs->tabData(index);
    QString internal_name = var.toString();

    for (const auto& i : _view_type_map)
        if (i.first == internal_name)
        {
            _view_stack->setCurrentWidget(i.second);
            break;
        }
}

void MainWindow::onItemDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid())
        return;

    QStandardItem* item = _model->item(index.row(), AudioLibraryView::ZERO);
    if (!item)
        return;

    auto found = _views_for_items.find(item);
    if (found != _views_for_items.end())
    {
        addBreadCrumb(found->second.release());
        return;
    }

    if (QStandardItem* path_item = _model->item(index.row(), AudioLibraryView::PATH))
    {
        if (QDesktopServices::openUrl(QUrl::fromLocalFile(path_item->text())))
            return;
    }
}

void MainWindow::saveLibrary()
{
    QString cache_dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QString filepath = cache_dir + "/AudioLibrary";

    {
        QDir dir(cache_dir);
        if (!dir.mkpath(cache_dir))
            return;
    }

    QSaveFile file(filepath);
    if (!file.open(QIODevice::WriteOnly))
        return;

    {
        QDataStream stream(&file);
        _library.save(stream);
    }

    file.commit();
}

void MainWindow::loadLibrary()
{
    QString cache_dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QString filepath = cache_dir + "/AudioLibrary";

    {
        QFile file(filepath);
        if (!file.open(QIODevice::ReadOnly))
            return;

        {
            QDataStream stream(&file);
            _library.load(stream);
        }
    }

    _library.cleanupTracksOutsideTheseDirectories(_settings.audio_dir_paths.getValue());
}

void MainWindow::scanAudioDirs()
{
    abortScanAudioDirs();
    _library_load_thread = std::thread([=]() {
        scanAudioDirsThreadFunc(_settings.audio_dir_paths.getValue());
    });
}

void MainWindow::abortScanAudioDirs()
{
    if (_library_load_thread.joinable())
    {
        _abort_flag = true;
        _library_load_thread.join();
        _abort_flag = false;
    }
}

void MainWindow::scanAudioDirsThreadFunc(QStringList audio_dir_paths)
{
    int files_loaded = 0;
    int files_skipped = 0;
    auto start_time = std::chrono::system_clock::now();

    for (const QString& dirpath : audio_dir_paths)
    {
        forEachFileInDirectory(dirpath, [=, &files_loaded, &files_skipped](const QFileInfo& file) {
            if (_abort_flag)
                return false; // stop iteration

            QString filepath = file.filePath();
            QDateTime last_modified = file.lastModified();

            {
                std::lock_guard<SpinLock> lock(_library_spin_lock);

                if (AudioLibraryTrack* track = _library.findTrack(filepath))
                    if (track->_last_modified == last_modified)
                    {
                        ++files_skipped;
                        libraryLoadProgressed(files_loaded, files_skipped);
                        return true; // nothing to do
                    }
            }

            TrackInfo track_info;
            if (readTrackInfo(filepath, track_info))
            {
                std::lock_guard<SpinLock> lock(_library_spin_lock);

                _library.addTrack(filepath, last_modified, track_info);
            }

            ++files_loaded;
            libraryLoadProgressed(files_loaded, files_skipped);
            return true;
        });
    }

    auto end_time = std::chrono::system_clock::now();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    libraryLoadFinished(files_loaded, files_skipped, float(millis.count()) / 1000.0);
}

const AudioLibraryView* MainWindow::getCurrentView() const
{
    return _breadcrumb_views.back().get();
}

void MainWindow::updateCurrentView()
{
    _views_for_items.clear();
    _model->removeRows(0, _model->rowCount());

    auto supported_modes = _breadcrumb_views.back()->getSupportedModes();

    AudioLibraryView::DisplayMode current_display_mode;
    bool is_current_display_mode_valid = false;

    // restore user-selected display mode

    if (supported_modes.size() == 1)
    {
        current_display_mode = supported_modes.front();
        is_current_display_mode_valid = true;
    }
    else if (supported_modes.size() > 1)
    {
        auto found_selected_display_mode = std::find_if(_selected_display_modes.begin(), _selected_display_modes.end(), [=](const std::pair<std::vector<AudioLibraryView::DisplayMode>, AudioLibraryView::DisplayMode>& i) {
            return i.first == supported_modes;
        });

        if (found_selected_display_mode != _selected_display_modes.end())
        {
            current_display_mode = found_selected_display_mode->second;
            is_current_display_mode_valid = true;
        }
        else
        {
            current_display_mode = supported_modes.front();
            is_current_display_mode_valid = true;
        }
    }

    // create tabs for supported display modes

    while (_display_mode_tabs->count() > 0)
        _display_mode_tabs->removeTab(0);
    _display_mode_tab_indexes.clear();

    for (AudioLibraryView::DisplayMode display_mode : supported_modes)
    {
        int index = _display_mode_tabs->addTab(AudioLibraryView::getDisplayModeFriendlyName(display_mode));
        _display_mode_tab_indexes.push_back(std::make_pair(index, display_mode));

        if (is_current_display_mode_valid && current_display_mode == display_mode)
        {
            _display_mode_tabs->setCurrentIndex(index);
        }
    }

    // hide unused columns

    std::vector<AudioLibraryView::Column> visible_columns;
    if (is_current_display_mode_valid)
        visible_columns = AudioLibraryView::getColumnsForDisplayMode(current_display_mode);

    visible_columns.push_back(AudioLibraryView::ZERO);

    for (const auto& column : AudioLibraryView::columnToStringMapping())
    {
        auto it = std::find(visible_columns.begin(), visible_columns.end(), column.first);

        _table->setColumnHidden(column.first, it == visible_columns.end());
    }

    // craete items

    if (!_breadcrumb_views.empty())
    {
        std::lock_guard<SpinLock> lock(_library_spin_lock);

        _breadcrumb_views.back()->createItems(_library, is_current_display_mode_valid ? &current_display_mode : nullptr, _model, _views_for_items);
    }

    _model->sort(0);

    _last_view_update_time = std::chrono::steady_clock::now();
    _is_last_view_update_time_valid = true;
}

void MainWindow::advanceIconSize(int direction)
{
    int current_size = _list->iconSize().width();

    auto found = std::find(_icon_size_steps.begin(), _icon_size_steps.end(), current_size);

    if (found != _icon_size_steps.end())
    {
        if (direction > 0 && found + 1 != _icon_size_steps.end())
        {
            int new_size = *(found + 1);

            _list->setIconSize(QSize(new_size, new_size));
        }
        else if (direction < 0 && found != _icon_size_steps.begin())
        {
            int new_size = *(found - 1);

            _list->setIconSize(QSize(new_size, new_size));
        }
    }
}

void MainWindow::addViewTypeTab(QWidget* view, const QString& friendly_name, const QString& internal_name)
{
    int index = _view_type_tabs->addTab(friendly_name);
    _view_type_tabs->setTabData(index, internal_name);
    _view_type_map.push_back(std::make_pair(internal_name, view));
}

void MainWindow::getFilepathsFromIndex(const QModelIndex& index, std::vector<QString>& filepaths)
{
    if (!index.isValid())
        return;

    if (QStandardItem* item = _model->itemFromIndex(index))
    {
        auto view_found = _views_for_items.find(item);
        if (view_found != _views_for_items.end())
        {
            std::vector<const AudioLibraryTrack*> tracks;
            view_found->second->resolveToTracks(_library, tracks);

            for (const AudioLibraryTrack* track : tracks)
                filepaths.push_back(track->_filepath);

            return;
        }
    }

    if (QStandardItem* path_item = _model->item(index.row(), AudioLibraryView::PATH))
    {
        filepaths.push_back(path_item->text());
    }
}

void MainWindow::forEachFilepathAtIndex(const QModelIndex& index, std::function<void(const QString&)> callback) const
{
    if (!index.isValid())
        return;

    if (QStandardItem* item = _model->itemFromIndex(index))
    {
        auto view_found = _views_for_items.find(item);
        if (view_found != _views_for_items.end())
        {
            std::vector<const AudioLibraryTrack*> tracks;
            view_found->second->resolveToTracks(_library, tracks);

            std::sort(tracks.begin(), tracks.end(), [](const AudioLibraryTrack* a, const AudioLibraryTrack* b){
                return std::tie(a->_album->_key._artist, a->_album->_key._year, a->_track_number, a->_title) <
                    std::tie(b->_album->_key._artist, b->_album->_key._year, b->_track_number, b->_title);
            });

            for (const AudioLibraryTrack* track : tracks)
                callback(track->_filepath);

            return;
        }
    }

    if (QStandardItem* path_item = _model->item(index.row(), AudioLibraryView::PATH))
    {
        callback(path_item->text());
    }
}

void MainWindow::setBreadCrumb(AudioLibraryView* view)
{
    clearBreadCrumbs();
    addBreadCrumb(view);
}

void MainWindow::addBreadCrumb(AudioLibraryView* view)
{
    bool first_breadcrump = _breadcrumb_views.empty();

    QScrollBar* visible_scrollbar = nullptr;

    if (_list == _view_stack->currentWidget())
        visible_scrollbar = _list->verticalScrollBar();
    else if (_table == _view_stack->currentWidget())
        visible_scrollbar = _table->verticalScrollBar();

    double scroll_pos = double(visible_scrollbar->value()) / double(visible_scrollbar->maximum() - visible_scrollbar->minimum());

    QPushButton* button = new QPushButton(view->getDisplayName(), this);
    connect(button, &QPushButton::clicked, this, &MainWindow::onBreadCrumbClicked);
    _breadcrumb_layout->addWidget(button);
    _breadcrumb_buttons.push_back(std::unique_ptr<QObject, LateDeleter>(button));
    _breadcrumb_views.push_back(std::unique_ptr<AudioLibraryView>(view));
    // only save the scrollpos if there was a view active before this one
    if (!first_breadcrump)
        _breadcrump_saved_scrollpos.push_back(scroll_pos);

    updateCurrentView();
}

void MainWindow::clearBreadCrumbs()
{
    _breadcrumb_buttons.clear();
    _breadcrumb_views.clear();
    _breadcrump_saved_scrollpos.clear();
}

void MainWindow::restoreBreadCrumb(QObject* object)
{
    // remove all breadcrumbs behind the given one

    auto found = std::find_if(_breadcrumb_buttons.begin(), _breadcrumb_buttons.end(), [=](const std::unique_ptr<QObject, LateDeleter>& i){
        return i.get() == object;
    });
    size_t index = std::distance(_breadcrumb_buttons.begin(), found);
    if (index >= _breadcrump_saved_scrollpos.size())
        return;

    double scroll_pos = _breadcrump_saved_scrollpos[index];

    _breadcrumb_buttons.erase(_breadcrumb_buttons.begin() + index + 1, _breadcrumb_buttons.end());
    _breadcrumb_views.resize(_breadcrumb_buttons.size());
    _breadcrump_saved_scrollpos.resize(_breadcrumb_buttons.size() - 1);

    updateCurrentView();

    QScrollBar* visible_scrollbar = nullptr;

    if (_list == _view_stack->currentWidget())
        visible_scrollbar = _list->verticalScrollBar();
    else if (_table == _view_stack->currentWidget())
        visible_scrollbar = _table->verticalScrollBar();

    QTimer::singleShot(1, this, [=]() {
        visible_scrollbar->setValue(int(scroll_pos * double(visible_scrollbar->maximum() - visible_scrollbar->minimum())));
    });
}