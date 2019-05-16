// SPDX-License-Identifier: GPL-2.0-only
#include "MainWindow.h"

#include <chrono>
#include <unordered_set>
#include <unordered_map>
#include <set>
#include <QtCore/qmimedata.h>
#include <QtCore/qprocess.h>
#include <QtCore/qsavefile.h>
#include <QtCore/qsettings.h>
#include <QtCore/qstandardpaths.h>
#include <QtCore/qtimer.h>
#include <QtGui/qdesktopservices.h>
#include <QtGui/qdrag.h>
#include <QtGui/qevent.h>
#include <QtGui/qimagereader.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qmenubar.h>
#include <QtWidgets/qscrollbar.h>
#include <QtWidgets/qshortcut.h>
#include <QtWidgets/qfiledialog.h>
#include <QtWidgets/qheaderview.h>
#include <QtWidgets/qstyleditemdelegate.h>
#include <QtWidgets/qtoolbutton.h>
#include <QtWidgets/qtooltip.h>
#include "project_version.h"
#include "resource_helpers.h"
#include "SettingsEditorWindow.h"
#include "AdvancedSearchDialog.h"

namespace
{
    template<class T, class V>
    class SetValueOnDestroy
    {
    public:
        SetValueOnDestroy(T& variable, V value)
            : _variable(variable)
            , _value(value)
        {}

        ~SetValueOnDestroy()
        {
            _variable = _value;
        }

        SetValueOnDestroy(const SetValueOnDestroy& other) = delete;
        SetValueOnDestroy& operator=(const SetValueOnDestroy& other) = delete;
        SetValueOnDestroy(SetValueOnDestroy&& other) = delete;
        SetValueOnDestroy& operator=(SetValueOnDestroy&& other) = delete;

    private:
        T& _variable;
        V _value;
    };

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

    double getRelativeScrollPos(QScrollBar* scroll_bar)
    {
        return double(scroll_bar->value()) / double(scroll_bar->maximum() - scroll_bar->minimum());
    }

    void setRelativeScrollPos(QScrollBar* scroll_bar, double scroll_pos)
    {
        scroll_bar->setValue(int(scroll_pos * double(scroll_bar->maximum() - scroll_bar->minimum())));
    }

    /**
    * A special QItemDelegate that can elide each line of a multi-lined text independently.
    */
    class MultiLinesElidedItemDelegate : public QStyledItemDelegate
    {
    public:
        MultiLinesElidedItemDelegate(QObject* parent);

        virtual bool helpEvent(QHelpEvent* event, QAbstractItemView* view,
            const QStyleOptionViewItem& option, const QModelIndex& index) override;

    protected:
        virtual void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override;

    private:
        static QString getElidedLines(const QString& text, const QFontMetrics& font_metrics, int width);
    };

    MultiLinesElidedItemDelegate::MultiLinesElidedItemDelegate(QObject* parent)
        : QStyledItemDelegate(parent)
    {}

    bool MultiLinesElidedItemDelegate::helpEvent(QHelpEvent* event, QAbstractItemView* view,
        const QStyleOptionViewItem& option, const QModelIndex& index)
    {
        if (event->type() == QEvent::ToolTip)
        {
            // show tooltip for elided text

            QString text = index.data(Qt::DisplayRole).toString();
            text.replace('\n', QChar::LineSeparator);

            if (!text.isEmpty())
            {
                const QString elided_text = getElidedLines(text, option.fontMetrics, option.decorationSize.width());
                if (elided_text != text)
                {
                    QToolTip::showText(event->globalPos(), text, view);
                    return true;
                }
            }

            QToolTip::hideText();
            return true;
        }

        return QStyledItemDelegate::helpEvent(event, view, option, index);
    }

    void MultiLinesElidedItemDelegate::initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const
    {
        QStyledItemDelegate::initStyleOption(option, index);

        // reserve a square area for the decoration icon
        // otherwise the layout may be uneven, depending on the icons

        if (const QListView* list = qobject_cast<const QListView*>(option->widget))
        {
            option->decorationSize = list->iconSize();
        }

        // bake elided text into the option
        // using the size of the pixmap as width

        QVariant multiline_var = index.data(AudioLibraryView::MULTILINE_DISPLAY_ROLE);

        if (multiline_var.isValid())
            option->text = multiline_var.toString();

        option->text = getElidedLines(option->text, option->fontMetrics, option->decorationSize.width());
    }

    QString MultiLinesElidedItemDelegate::getElidedLines(const QString& text, const QFontMetrics& font_metrics, int width)
    {
        QStringList lines = text.split(QChar::LineSeparator);

        for (QString& line : lines)
            line = font_metrics.elidedText(line, Qt::ElideRight, width);

        return lines.join(QChar::LineSeparator);
    }
}

//=============================================================================

ViewSelector::ViewSelector()
    : QFrame()
{
    setWindowFlag(Qt::Popup);

    setFrameShape(QFrame::Box);

    _artist_button = new QRadioButton("Artists", this);
    _album_button = new QRadioButton("Albums", this);
    _track_button = new QRadioButton("Tracks", this);
    _year_button = new QRadioButton("Years", this);
    _genre_button = new QRadioButton("Genres", this);

    _filter_box = new QLineEdit(this);
    _filter_box->setPlaceholderText("Filter...");
    _filter_box->setClearButtonEnabled(true);

    auto layout = new QVBoxLayout(this);
    layout->addWidget(_artist_button);
    layout->addWidget(_album_button);
    layout->addWidget(_track_button);
    layout->addWidget(_year_button);
    layout->addWidget(_genre_button);
    layout->addWidget(_filter_box);

    connect(_artist_button, &QRadioButton::clicked, this, &ViewSelector::radioButtonSelected);
    connect(_album_button, &QRadioButton::clicked, this, &ViewSelector::radioButtonSelected);
    connect(_track_button, &QRadioButton::clicked, this, &ViewSelector::radioButtonSelected);
    connect(_year_button, &QRadioButton::clicked, this, &ViewSelector::radioButtonSelected);
    connect(_genre_button, &QRadioButton::clicked, this, &ViewSelector::radioButtonSelected);

    connect(_filter_box, &QLineEdit::textChanged, this, &ViewSelector::selectionChanged);

    _artist_button->setChecked(true);
}

std::unique_ptr<AudioLibraryView> ViewSelector::getSelectedView() const
{
    // no filter box for years

    if (_year_button->isChecked())
        return std::unique_ptr<AudioLibraryView>(new AudioLibraryViewAllYears());

    // either use a full view or a search

    QString filter_text = _filter_box->text();

    if (filter_text.isEmpty())
    {
        if (_artist_button->isChecked())
            return std::unique_ptr<AudioLibraryView>(new AudioLibraryViewAllArtists());
        if (_album_button->isChecked())
            return std::unique_ptr<AudioLibraryView>(new AudioLibraryViewAllAlbums());
        if (_track_button->isChecked())
            return std::unique_ptr<AudioLibraryView>(new AudioLibraryViewAllTracks());
        if (_genre_button->isChecked())
            return std::unique_ptr<AudioLibraryView>(new AudioLibraryViewAllGenres());
    }
    else
    {
        AudioLibraryViewAdvancedSearch::Query query;

        if (_artist_button->isChecked())
            query.artist = filter_text;
        if (_album_button->isChecked())
            query.album = filter_text;
        if (_track_button->isChecked())
            query.title = filter_text;
        if (_genre_button->isChecked())
            query.genre = filter_text;

        return std::unique_ptr<AudioLibraryView>(new AudioLibraryViewAdvancedSearch(query));
    }

    // default to artist view if nothing is selected

    return std::unique_ptr<AudioLibraryView>(new AudioLibraryViewAllArtists());
}

void ViewSelector::radioButtonSelected()
{
    // filtering years makes no sense

    _filter_box->setEnabled(sender() != _year_button);

    emit selectionChanged();
}

//=============================================================================

ThreadSafeLibrary::LibraryAccessor::LibraryAccessor(ThreadSafeLibrary& data)
    : _lock(data._library_spin_lock)
    , _library(data._library)
{
}

const AudioLibrary& ThreadSafeLibrary::LibraryAccessor::getLibrary() const
{
    return _library;
}

AudioLibrary& ThreadSafeLibrary::LibraryAccessor::getLibraryForUpdate() const
{
    return _library;
}

//=============================================================================

MainWindow::MainWindow(Settings& settings)
    : _settings(settings)
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

    auto toolarea = new QWidget(this);

    QToolButton* view_selector_popup_button = new QToolButton(toolarea);
    view_selector_popup_button->setToolTip("Select view");

    view_selector_popup_button->setIcon(iconFromResource(res::VIEW_MENU_SVG()));
    view_selector_popup_button->setIconSize(QSize(24, 24));

    connect(&_view_selector, &ViewSelector::selectionChanged, this, [=]() {
        setBreadCrumb(_view_selector.getSelectedView().release());
    });

    connect(view_selector_popup_button, &QToolButton::clicked, this, [=](){
        QPoint pos = view_selector_popup_button->mapToGlobal(view_selector_popup_button->rect().bottomLeft());

        _view_selector.move(pos);
        _view_selector.show();
    });

    _display_mode_tabs = new QTabBar(toolarea);
    _display_mode_tabs->setAutoHide(true);

    _model = new AudioLibraryModel(this);

    _list = new QListView(this);
    _list->setModel(_model);
    _list->setViewMode(QListView::IconMode);
    _list->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    _list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    _list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _list->setDragEnabled(false);
    _list->viewport()->installEventFilter(this);
    _list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    _list->setTextElideMode(Qt::ElideNone);
    _list->setItemDelegate(new MultiLinesElidedItemDelegate(this));

    _table = new QTableView(this);
    _table->setSortingEnabled(true);
    _table->setWordWrap(false);
    _table->sortByColumn(AudioLibraryView::ZERO, Qt::AscendingOrder);
    _table->setModel(_model);
    _table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    _table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    _table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _table->viewport()->installEventFilter(this);
    _table->setSelectionBehavior(QAbstractItemView::SelectRows);
    _table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    _table->horizontalHeader()->setSectionsMovable(true);
    _table->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);

    _view_stack = new QStackedWidget(this);
    _view_stack->addWidget(_list);
    _view_stack->addWidget(_table);
    _view_stack->setCurrentWidget(_list);

    _view_type_tabs = new QTabBar(toolarea);
    addViewTypeTab(_list, "Icons", "icons");
    addViewTypeTab(_table, "Table", "table");

    _status_bar = new QStatusBar(this);
    _status_bar->setSizeGripEnabled(false);

    QHBoxLayout* breadcrumb_layout_wrapper = new QHBoxLayout();
    _breadcrumb_layout = new QHBoxLayout();
    breadcrumb_layout_wrapper->addWidget(view_selector_popup_button);
    breadcrumb_layout_wrapper->addLayout(_breadcrumb_layout);
    breadcrumb_layout_wrapper->addStretch();
    breadcrumb_layout_wrapper->addWidget(_display_mode_tabs);
    breadcrumb_layout_wrapper->addWidget(_view_type_tabs);

    QVBoxLayout* tool_vbox = new QVBoxLayout(toolarea);
    tool_vbox->addLayout(breadcrumb_layout_wrapper);

    QVBoxLayout* vbox = new QVBoxLayout(this);
    vbox->setMargin(0);
    vbox->setSpacing(0);
    vbox->setMenuBar(menubar);
    vbox->addWidget(toolarea);
    vbox->addWidget(_view_stack);
    vbox->addWidget(_status_bar);

    connect(&_library, &ThreadSafeLibrary::libraryCacheLoading, this, &MainWindow::onLibraryCacheLoading);
    connect(&_library, &ThreadSafeLibrary::libraryLoadProgressed, this, &MainWindow::onLibraryLoadProgressed);
    connect(&_library, &ThreadSafeLibrary::libraryLoadFinished, this, &MainWindow::onLibraryLoadFinished);
    connect(&_library, &ThreadSafeLibrary::coverLoadFinished, this, &MainWindow::onCoverLoadFinished);
    connect(_display_mode_tabs, &QTabBar::tabBarClicked, this, &MainWindow::onDisplayModeSelected);
    connect(_view_type_tabs, &QTabBar::tabBarClicked, this, &MainWindow::onViewTypeSelected);
    connect(_list, &QAbstractItemView::doubleClicked, this, &MainWindow::onItemDoubleClicked);
    connect(_table, &QAbstractItemView::doubleClicked, this, &MainWindow::onItemDoubleClicked);
    connect(_table->horizontalHeader(), &QHeaderView::sectionClicked, this, &MainWindow::onTableHeaderSectionClicked);
    connect(_table->horizontalHeader(), &QHeaderView::customContextMenuRequested, this, &MainWindow::onTableHeaderContextMenu);

    setBreadCrumb(new AudioLibraryViewAllArtists());

    restoreSettingsOnStart();

    show();

    if (_settings.audio_dir_paths.getValue().isEmpty())
    {
        FirstStartDialog dlg(this, _settings);
        dlg.exec();
    }

    scanAudioDirs();
}

MainWindow::~MainWindow()
{
    abortScanAudioDirs();
    abortLoadingCovers();
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    hide();

    saveSettingsOnExit();

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
            QModelIndex mouse_index = view->indexAt(me->pos());
            if (mouse_index.isValid())
            {
                _is_dragging = true;
                _drag_start_pos = me->pos();
                _dragged_indexes.clear();

                QModelIndexList selected_indexes = view->selectionModel()->selectedIndexes();
                if (selected_indexes.indexOf(mouse_index) < 0)
                {
                    _dragged_indexes.push_back(mouse_index);
                }
                else
                {
                    std::unordered_set<int> rows;

                    for (const QModelIndex& selected_index : selected_indexes)
                    {
                        if (rows.find(selected_index.row()) == rows.end())
                        {
                            rows.insert(selected_index.row());
                            _dragged_indexes.push_back(selected_index);
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

                    QMimeData* mime_data = new QMimeData();
                    mime_data->setUrls(urls);

                    QDrag drag(this);
                    drag.setMimeData(mime_data);

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
        case QEvent::ContextMenu:
            contextMenuEventForView(view, static_cast<QContextMenuEvent*>(event));
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

void MainWindow::onLibraryCacheLoading()
{
    size_t num_tracks = 0;

    {
        ThreadSafeLibrary::LibraryAccessor acc(_library);

        num_tracks += acc.getLibrary().getNumberOfTracks();
    }

    _status_bar->showMessage(QString("Loading cache: %1 files").arg(num_tracks));

    updateCurrentViewIfOlderThan(1000);
}

void MainWindow::onLibraryLoadProgressed(int files_loaded, int files_in_cache)
{
    _status_bar->showMessage(QString("%1 files in cache, %2 new files loaded").arg(files_in_cache).arg(files_loaded));

    updateCurrentViewIfOlderThan(1000);
}

void MainWindow::onLibraryLoadFinished(int files_loaded, int files_in_cache, float duration_sec)
{
    _status_bar->showMessage(QString("%1 files in cache, %2 new files loaded, needed %3 seconds").arg(files_in_cache).arg(files_loaded).arg(duration_sec, 0, 'f', 1));

    updateCurrentView();
}

void MainWindow::onCoverLoadFinished()
{
    updateCurrentView();
}

void MainWindow::onShowDuplicateAlbums()
{
    setBreadCrumb(new AudioLibraryViewDuplicateAlbums());
}

void MainWindow::onBreadCrumbClicked()
{
    restoreBreadCrumb(sender());
}

void MainWindow::onBreadCrumpReverse()
{
    if (_breadcrumbs.size() > 1)
    {
        QObject* second_last = _breadcrumbs[_breadcrumbs.size() - 2]._button.get();

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
            QAbstractItemView* previous_view = qobject_cast<QAbstractItemView*>(_view_stack->currentWidget());

            _view_stack->setCurrentWidget(i.second);

            QAbstractItemView* current_view = qobject_cast<QAbstractItemView*>(_view_stack->currentWidget());

            if (previous_view && current_view && previous_view != current_view)
            {
                double relative_scroll_pos = getRelativeScrollPos(previous_view->verticalScrollBar());
                setRelativeScrollPos(current_view->verticalScrollBar(), relative_scroll_pos);
            }

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

    if (const AudioLibraryView* view = _model->getViewForItem(item))
    {
        addBreadCrumb(view->clone());
        return;
    }

    QString path = _model->getFilepathFromIndex(index);
    if (!path.isEmpty() &&
        QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
    {
        return;
    }
}

void MainWindow::onTableHeaderSectionClicked()
{
    // keep the selection visible while re-sorting the table

    QModelIndexList selection = _table->selectionModel()->selectedIndexes();

    QModelIndex first_selected;

    for (const QModelIndex& index : selection)
    {
        if (index.column() == AudioLibraryView::ZERO)
        {
            if (!first_selected.isValid() ||
                index.row() < first_selected.row())
            {
                first_selected = index;
            }
        }
    }

    if (first_selected.isValid())
    {
        _table->scrollTo(first_selected);
    }
}

void MainWindow::onTableHeaderContextMenu(const QPoint& pos)
{
    if (!_current_display_mode)
        return;

    QMenu menu;

    int clicked_index = _table->horizontalHeader()->logicalIndexAt(pos.x());
    if (clicked_index != -1 && clicked_index != AudioLibraryView::ZERO)
    {
        QAction* action = menu.addAction("Hide");
        connect(action, &QAction::triggered, [=](){
            _table->setColumnHidden(clicked_index, true);

            _hidden_columns.insert(static_cast<AudioLibraryView::Column>(clicked_index));
        });

        menu.addSeparator();
    }

    for (AudioLibraryView::Column column : AudioLibraryView::getColumnsForDisplayMode(*_current_display_mode))
    {
        QString name = AudioLibraryView::getColumnFriendlyName(column);

        bool column_hidden = _table->isColumnHidden(column);

        QAction* action = menu.addAction(name);
        action->setCheckable(true);
        action->setChecked(!column_hidden);
        connect(action, &QAction::triggered, [=]() {
            _table->setColumnHidden(column, !column_hidden);

            if (column_hidden)
                _hidden_columns.erase(column);
            else
                _hidden_columns.insert(column);
        });
    }

    QPoint global_pos = _table->horizontalHeader()->mapToGlobal(pos);

    if (!menu.actions().isEmpty())
        menu.exec(global_pos);
}

void MainWindow::saveLibrary()
{
    if (!_library._library_cache_loaded)
        return; // don't save back a partially loaded library

    {
        ThreadSafeLibrary::LibraryAccessor acc(_library);

        if (!acc.getLibrary().isModified())
            return; // no need to save if the library has not changed
    }

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

        ThreadSafeLibrary::LibraryAccessor acc(_library);

        acc.getLibrary().save(stream);
    }

    file.commit();
}

void MainWindow::loadLibrary(QStringList audio_dir_paths, ThreadSafeLibrary& library)
{
    QString cache_dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QString filepath = cache_dir + "/AudioLibrary";

    {
        QFile file(filepath);
        if (!file.open(QIODevice::ReadOnly))
            return;

        {
            QDataStream stream(&file);

            AudioLibrary::Loader loader;

            {
                ThreadSafeLibrary::LibraryAccessor acc(library);

                loader.init(acc.getLibraryForUpdate(), stream);
            }

            int album_counter = 0;

            while (loader.hasNextAlbum())
            {
                {
                    ThreadSafeLibrary::LibraryAccessor acc(library);

                    loader.loadNextAlbum();
                }

                ++album_counter;

                if((album_counter % 10) == 0)
                    library.libraryCacheLoading();
            }
        }
    }
}

void MainWindow::scanAudioDirs()
{
    abortScanAudioDirs();
    abortLoadingCovers();

    _library._is_libary_loading = true;

    _library_load_thread = std::thread([=]() {
        scanAudioDirsThreadFunc(_settings.audio_dir_paths.getValue(), _library);
    });

    startLoadingCovers();
}

void MainWindow::abortScanAudioDirs()
{
    _library._abort_loading = true;
    SetValueOnDestroy<std::atomic_bool, bool> reset_abort_flag(_library._abort_loading, false);

    if (_library_load_thread.joinable())
        _library_load_thread.join();
}

void MainWindow::scanAudioDirsThreadFunc(QStringList audio_dir_paths, ThreadSafeLibrary& library)
{
    SetValueOnDestroy<std::atomic_bool, bool> reset_loading_flag(library._is_libary_loading, false);

    int files_loaded = 0;
    int files_in_cache = 0;
    auto start_time = std::chrono::system_clock::now();

    if(!library._library_cache_loaded)
    {
        loadLibrary(audio_dir_paths, library);
        library._library_cache_loaded = true;
        library.libraryCacheLoading();
    }

    std::unordered_set<QString> visited_audio_files;

    for (const QString& dirpath : audio_dir_paths)
    {
        forEachFileInDirectory(dirpath, [&library, &files_loaded, &files_in_cache, &visited_audio_files](const QFileInfo& file) {
            if (library._abort_loading)
                return false; // stop iteration

            QString filepath = file.filePath();
            QDateTime last_modified = file.lastModified();

            {
                ThreadSafeLibrary::LibraryAccessor acc(library);

                if (AudioLibraryTrack* track = acc.getLibrary().findTrack(filepath))
                    if (track->_last_modified == last_modified)
                    {
                        ++files_in_cache;
                        visited_audio_files.insert(filepath);
                        library.libraryLoadProgressed(files_loaded, files_in_cache);
                        return true; // nothing to do
                    }
            }

            TrackInfo track_info;
            if (readTrackInfo(filepath, track_info))
            {
                {
                    ThreadSafeLibrary::LibraryAccessor acc(library);

                    acc.getLibraryForUpdate().addTrack(filepath, last_modified, track_info);
                }

                ++files_loaded;
                visited_audio_files.insert(filepath);
                library.libraryLoadProgressed(files_loaded, files_in_cache);
            }
            return true;
        });
    }

    if (!library._abort_loading)
    {
        ThreadSafeLibrary::LibraryAccessor acc(library);

        acc.getLibraryForUpdate().removeTracksExcept(visited_audio_files);
    }

    auto end_time = std::chrono::system_clock::now();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    library.libraryLoadFinished(files_loaded, files_in_cache, float(millis.count()) / 1000.0);
}

const AudioLibraryView* MainWindow::getCurrentView() const
{
    return _breadcrumbs.back()._view.get();
}

void MainWindow::updateCurrentView()
{
    auto view_settings = saveViewSettings();

    QString current_view_id = _breadcrumbs.back()._view->getId();

    auto supported_modes = _breadcrumbs.back()._view->getSupportedModes();

    std::unique_ptr<AudioLibraryView::DisplayMode> current_display_mode;

    // restore user-selected display mode

    if (supported_modes.size() == 1)
    {
        current_display_mode.reset(new AudioLibraryView::DisplayMode(supported_modes.front()));
    }
    else if (supported_modes.size() > 1)
    {
        auto found_selected_display_mode = std::find_if(_selected_display_modes.begin(), _selected_display_modes.end(), [=](const std::pair<std::vector<AudioLibraryView::DisplayMode>, AudioLibraryView::DisplayMode>& i) {
            return i.first == supported_modes;
        });

        if (found_selected_display_mode != _selected_display_modes.end())
        {
            current_display_mode.reset(new AudioLibraryView::DisplayMode(found_selected_display_mode->second));
        }
        else
        {
            current_display_mode.reset(new AudioLibraryView::DisplayMode(supported_modes.front()));
        }
    }

    const bool same_view = _current_view_id == current_view_id;
    const bool same_display_mode = (current_display_mode && _current_display_mode && *current_display_mode == *_current_display_mode) ||
        (!current_display_mode || !_current_display_mode);

    const bool incremental = same_view && same_display_mode;

    _current_view_id = current_view_id;
    if (current_display_mode)
        _current_display_mode.reset(new AudioLibraryView::DisplayMode(*current_display_mode));
    else
        _current_display_mode.reset();

    // create tabs for supported display modes

    while (_display_mode_tabs->count() > 0)
        _display_mode_tabs->removeTab(0);
    _display_mode_tab_indexes.clear();

    for (AudioLibraryView::DisplayMode display_mode : supported_modes)
    {
        int index = _display_mode_tabs->addTab(AudioLibraryView::getDisplayModeFriendlyName(display_mode));
        _display_mode_tab_indexes.push_back(std::make_pair(index, display_mode));

        if (current_display_mode && *current_display_mode == display_mode)
        {
            _display_mode_tabs->setCurrentIndex(index);
        }
    }

    // hide unused columns

    std::vector<AudioLibraryView::Column> available_columns;
    if (current_display_mode)
        available_columns = AudioLibraryView::getColumnsForDisplayMode(*current_display_mode);

    available_columns.push_back(AudioLibraryView::ZERO);

    for (const auto& column : AudioLibraryView::columnToStringMapping())
    {
        bool is_available = std::find(available_columns.begin(), available_columns.end(), column.first) != available_columns.end();
        bool is_hidden = _hidden_columns.find(column.first) != _hidden_columns.end();

        _table->setColumnHidden(column.first, !is_available || is_hidden);
    }

    // create items

    if (incremental)
    {
        if (!_breadcrumbs.empty())
        {
            AudioLibraryModel::IncrementalUpdateScope update_scope(*_model);

            ThreadSafeLibrary::LibraryAccessor acc(_library);

            _breadcrumbs.back()._view->createItems(acc.getLibrary(), current_display_mode.get(), _model);
        }
    }
    else
    {
        AudioLibraryModel* model = new AudioLibraryModel(this);

        QStringList model_headers;
        for (const auto& column : AudioLibraryView::columnToStringMapping())
        {
            model_headers.push_back(AudioLibraryView::getColumnFriendlyName(column.first));
        }
        model->setHorizontalHeaderLabels(model_headers);

        if (!_breadcrumbs.empty())
        {
            ThreadSafeLibrary::LibraryAccessor acc(_library);

            _breadcrumbs.back()._view->createItems(acc.getLibrary(), current_display_mode.get(), model);
        }

        _model->deleteLater();
        _model = model;
        _list->setModel(model);
        _table->setModel(model);
    }

    restoreViewSettings(view_settings.get());

    _last_view_update_time = std::chrono::steady_clock::now();
    _is_last_view_update_time_valid = true;
}

void MainWindow::updateCurrentViewIfOlderThan(int msecs)
{
    if (!_is_last_view_update_time_valid)
    {
        updateCurrentView();
        return;
    }

    auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - _last_view_update_time).count() > msecs)
    {
        updateCurrentView();
        return;
    }
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
        if (const AudioLibraryView* view = _model->getViewForItem(item))
        {
            ThreadSafeLibrary::LibraryAccessor acc(_library);

            std::vector<const AudioLibraryTrack*> tracks;
            view->resolveToTracks(acc.getLibrary(), tracks);

            for (const AudioLibraryTrack* track : tracks)
                filepaths.push_back(track->_filepath);

            return;
        }
    }

    QString path = _model->getFilepathFromIndex(index);
    if (!path.isEmpty())
    {
        filepaths.push_back(path);
    }
}

void MainWindow::forEachFilepathAtIndex(const QModelIndex& index, std::function<void(const QString&)> callback)
{
    if (!index.isValid())
        return;

    if (QStandardItem* item = _model->itemFromIndex(index))
    {
        if (const AudioLibraryView* view = _model->getViewForItem(item))
        {
            ThreadSafeLibrary::LibraryAccessor acc(_library);

            std::vector<const AudioLibraryTrack*> tracks;
            view->resolveToTracks(acc.getLibrary(), tracks);

            std::sort(tracks.begin(), tracks.end(), [](const AudioLibraryTrack* a, const AudioLibraryTrack* b) {
                return std::tie(a->_album->_key._artist, a->_album->_key._year, a->_track_number, a->_title) <
                    std::tie(b->_album->_key._artist, b->_album->_key._year, b->_track_number, b->_title);
            });

            for (const AudioLibraryTrack* track : tracks)
                callback(track->_filepath);

            return;
        }
    }

    QString path = _model->getFilepathFromIndex(index);
    if (!path.isEmpty())
    {
        callback(path);
    }
}

void MainWindow::startLoadingCovers()
{
    _cover_load_thread = std::thread([=](){
        loadCoversThreadFunc(_library);
    });
}

void MainWindow::abortLoadingCovers()
{
    _library._abort_cover_loading = true;
    SetValueOnDestroy<std::atomic_bool, bool> reset_abort_flag(_library._abort_cover_loading, false);

    if (_cover_load_thread.joinable())
        _cover_load_thread.join();
}

void MainWindow::loadCoversThreadFunc(ThreadSafeLibrary& library)
{
    while (true)
    {
        // get cover bytes from library

        std::map<AudioLibraryAlbumKey, std::pair<QByteArray, QPixmap>> covers_to_load;
        bool enough_bytes_for_now = false;

        {
            ThreadSafeLibrary::LibraryAccessor acc(library);
            for (const AudioLibraryAlbum* album : acc.getLibrary().getAlbums())
            {
                if (library._abort_loading)
                    return;

                if (!album->_cover.isEmpty() && !album->isCoverPixmapSet())
                {
                    covers_to_load[album->_key].first = album->_cover;
                }

                if (covers_to_load.size() >= 100)
                {
                    enough_bytes_for_now = true;
                    break;
                }
            }
        }

        if (covers_to_load.empty() && !enough_bytes_for_now && !library._is_libary_loading)
        {
            library.coverLoadFinished();
            return; // ok, finished
        }

        // decode covers

        for (auto& cover : covers_to_load)
        {
            if (library._abort_loading)
                return;

            cover.second.second.loadFromData(cover.second.first);
        }

        // write decoded covers to library

        {
            ThreadSafeLibrary::LibraryAccessor acc(library);

            for (auto& cover : covers_to_load)
            {
                if (library._abort_loading)
                    return;

                acc.getLibraryForUpdate().getAlbum(cover.first)->setCoverPixmap(cover.second.second);
            }
        }
    }
}

void MainWindow::setBreadCrumb(AudioLibraryView* view)
{
    clearBreadCrumbs();
    addBreadCrumb(view);
}

void MainWindow::addBreadCrumb(AudioLibraryView* view)
{
    if (!_breadcrumbs.empty())
    {
        // if this isn't the first breadcrumb, create restore information so the view looks the same when going back

        _breadcrumbs.back()._restore_data = saveViewSettings();
    }

    QPushButton* button = new QPushButton(view->getDisplayName(), this);
    connect(button, &QPushButton::clicked, this, &MainWindow::onBreadCrumbClicked);

    _breadcrumb_layout->addWidget(button);

    Breadcrumb breadcrumb;
    breadcrumb._button.reset(button);
    breadcrumb._view.reset(view);
    _breadcrumbs.push_back(std::move(breadcrumb));

    updateCurrentView();
}

void MainWindow::clearBreadCrumbs()
{
    _breadcrumbs.clear();
}

void MainWindow::restoreBreadCrumb(QObject* object)
{
    // remove all breadcrumbs behind the given one

    auto found = std::find_if(_breadcrumbs.begin(), _breadcrumbs.end(), [=](const Breadcrumb& i){
        return i._button.get() == object;
    });

    // the last breadcrumb is the current view, nothing to restore

    if (found != _breadcrumbs.end() && found == _breadcrumbs.end() - 1)
        return;

    // delete breadcrumbs

    _breadcrumbs.erase(found + 1, _breadcrumbs.end());

    // restore view, take away restore data from current breadcrumb

    std::shared_ptr<ViewRestoreData> restore_data(_breadcrumbs.back()._restore_data.release());

    updateCurrentView();

    // restore uses a timer because the list view is updating asynchronously

    QTimer::singleShot(1, this, [=]() {
        restoreViewSettings(restore_data.get());
    });
}

bool MainWindow::findBreadcrumbId(const QString& id) const
{
    for (const Breadcrumb& breadcrumb : _breadcrumbs)
    {
        if (breadcrumb._view->getId() == id)
        {
            return true;
        }
    }

    return false;
}

void MainWindow::contextMenuEventForView(QAbstractItemView* view, QContextMenuEvent* event)
{
    QModelIndex mouse_index = view->indexAt(event->pos());
    if (mouse_index.isValid())
    {
        QModelIndexList selected_indexes = view->selectionModel()->selectedIndexes();
        std::unordered_set<int> rows;

        for (const QModelIndex& selected_index : selected_indexes)
        {
            if (rows.find(selected_index.row()) == rows.end())
            {
                rows.insert(selected_index.row());
            }
        }

        QMenu menu;

        if (!getVlcPath().isEmpty())
        {
            QList<QPersistentModelIndex> selected_row_indexes;
            for (int row : rows)
                selected_row_indexes.push_back(view->model()->index(row, AudioLibraryView::ZERO));

            QAction* add_to_vlc_action = menu.addAction("Add to VLC Playlist");

            connect(add_to_vlc_action, &QAction::triggered, this, [=]() {
                startVlc(selected_row_indexes, true);
            });

            QAction* play_with_vlc_action = menu.addAction("Play with VLC");

            connect(play_with_vlc_action, &QAction::triggered, this, [=]() {
                startVlc(selected_row_indexes, false);
            });
        }

        if (rows.size() == 1)
        {
            const QStandardItem* artist_item = _model->item(mouse_index.row(), AudioLibraryView::ARTIST);
            const QStandardItem* album_item = _model->item(mouse_index.row(), AudioLibraryView::ALBUM);
            const QStandardItem* year_item = _model->item(mouse_index.row(), AudioLibraryView::YEAR);
            const QStandardItem* genre_item = _model->item(mouse_index.row(), AudioLibraryView::GENRE);
            const QStandardItem* cover_checksum_item = _model->item(mouse_index.row(), AudioLibraryView::COVER_CHECKSUM);

            // show artist

            if (artist_item && !artist_item->text().isEmpty())
            {
                std::shared_ptr<AudioLibraryView> artist_view(new AudioLibraryViewArtist(artist_item->text()));

                if (!findBreadcrumbId(artist_view->getId()))
                {
                    QAction* artist_action = menu.addAction(QString("More from artist \"%1\"...").arg(artist_item->text()));

                    auto slot = [=]() {
                        addBreadCrumb(artist_view->clone());
                    };

                    connect(artist_action, &QAction::triggered, this, slot);
                }
            }

            // only for tracks: show album

            if (_model->item(mouse_index.row(), AudioLibraryView::PATH) &&
                artist_item && album_item && year_item && genre_item && cover_checksum_item)
            {
                AudioLibraryAlbumKey key;
                key._artist = artist_item->text();

                key._album = album_item->text();

                bool year_ok = false;
                key._year = year_item->text().toInt(&year_ok);

                key._genre = genre_item->text();

                bool cover_checksum_ok = false;
                key._cover_checksum = cover_checksum_item->text().toUInt(&cover_checksum_ok);

                if (year_ok && cover_checksum_ok)
                {
                    std::shared_ptr<AudioLibraryView> album_view(new AudioLibraryViewAlbum(key));

                    if (!findBreadcrumbId(album_view->getId()))
                    {
                        QAction* artist_action = menu.addAction(QString("Show album \"%1\"").arg(album_item->text()));

                        auto slot = [=](){
                            addBreadCrumb(album_view->clone());
                        };

                        connect(artist_action, &QAction::triggered, this, slot);
                    }
                }
            }
        }

        if(!menu.actions().isEmpty())
            menu.exec(event->globalPos());
    }
}

QString MainWindow::getVlcPath()
{
#if WIN32
    QSettings vlc_registry("HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\VideoLAN\\VLC", QSettings::NativeFormat);
    return vlc_registry.value("Default").toString();
#else
    return QString::null;
#endif
}

void MainWindow::startVlc(const QList<QPersistentModelIndex>& indexes, bool only_add_to_playlist)
{
    std::vector<QString> filepaths;

    for (const QPersistentModelIndex& index : indexes)
        if (index.isValid())
            getFilepathsFromIndex(index, filepaths);

    QStringList arguments;
    arguments << "--started-from-file";

    if(only_add_to_playlist)
        arguments << "--playlist-enqueue";

    for (QString filepath : filepaths)
    {
#if WIN32
        // VLC has problems with slashes in filepaths
        filepath.replace("/", "\\");
#endif

        arguments += filepath;
    }

    QProcess::startDetached(getVlcPath(), arguments);
}

std::unique_ptr<MainWindow::ViewRestoreData> MainWindow::saveViewSettings() const
{
    std::unique_ptr<MainWindow::ViewRestoreData> restore_data(new ViewRestoreData());

    restore_data->_list_scroll_pos = getRelativeScrollPos(_list->verticalScrollBar());
    restore_data->_table_scroll_pos = getRelativeScrollPos(_table->verticalScrollBar());

    restore_data->_table_sort_indicator_section = _table->horizontalHeader()->sortIndicatorSection();
    restore_data->_table_sort_indicator_order = _table->horizontalHeader()->sortIndicatorOrder();

    // save selection

    QModelIndexList selected_indexes = qobject_cast<QAbstractItemView*>(_view_stack->currentWidget())->selectionModel()->selectedIndexes();
    for (const QModelIndex& index : selected_indexes)
    {
        if (index.column() != AudioLibraryView::ZERO)
            continue;

        QString id = _model->getItemId(_model->itemFromIndex(index));
        if (!id.isEmpty())
            restore_data->_selected_items.push_back(id);
    }

    return restore_data;
}

void MainWindow::restoreViewSettings(ViewRestoreData* restore_data)
{
    setRelativeScrollPos(_list->verticalScrollBar(), restore_data->_list_scroll_pos);
    setRelativeScrollPos(_table->verticalScrollBar(), restore_data->_table_scroll_pos);

    if (restore_data->_table_sort_indicator_section >= 0)
        _table->sortByColumn(restore_data->_table_sort_indicator_section, restore_data->_table_sort_indicator_order);

    // restore selection

    std::unordered_map<QString, QModelIndex> id_to_index_map;

    for (int i = 0, end = _model->rowCount(); i < end; ++i)
    {
        QModelIndex index = _model->index(i, AudioLibraryView::ZERO);
        if (QStandardItem* item = _model->itemFromIndex(index))
        {
            QString id = _model->getItemId(item);
            if (!id.isEmpty())
            {
                id_to_index_map[id] = index;
            }
        }
    }

    QItemSelection selection;

    for (const QString& id : restore_data->_selected_items)
    {
        auto item_it = id_to_index_map.find(id);
        if (item_it != id_to_index_map.end())
        {
            QModelIndex end = _model->index(item_it->second.row(), _model->columnCount() - 1);
            selection.push_back(QItemSelectionRange(item_it->second, end));
        }
    }

    qobject_cast<QAbstractItemView*>(_view_stack->currentWidget())->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
}

void MainWindow::restoreSettingsOnStart()
{
    // table column widths

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

    // table visual indexes

    const QHash<QString, QVariant> visual_indexes = _settings.audio_library_view_visual_indexes.getValue();

    std::vector<std::pair<int, int>> logical_and_visual_indexes;

    for (const auto& column : AudioLibraryView::columnToStringMapping())
    {
        auto found = visual_indexes.find(column.second);
        if (found != visual_indexes.end())
        {
            bool int_ok;
            int visual_index = found.value().toInt(&int_ok);
            if (int_ok && visual_index >= 0 && visual_index < _table->horizontalHeader()->count())
            {
                logical_and_visual_indexes.push_back(std::make_pair(column.first, visual_index));
            }
        }
    }

    for (const auto& logical_and_visual_index : logical_and_visual_indexes)
    {
        _table->horizontalHeader()->moveSection(_table->horizontalHeader()->visualIndex(logical_and_visual_index.first), logical_and_visual_index.second);
    }

    // table hidden columns

    const QStringList hidden_columns = _settings.audio_library_view_hidden_columns.getValue();
    for (const QString& column_id : hidden_columns)
    {
        if (std::unique_ptr<AudioLibraryView::Column> column = AudioLibraryView::getColumnFromId(column_id))
            _hidden_columns.insert(*column);
    }

    // current view

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

    // icon size

    _icon_size_steps = { 64, 96, 128, 192, 256 };

    int default_icon_size = 128;

    for (int s : _icon_size_steps)
    {
        if (_settings.main_window_icon_size.getValue() == s)
            default_icon_size = s;
    }
    _list->setIconSize(QSize(default_icon_size, default_icon_size));

    // window geometry

    _settings.main_window_geometry.restore(this);
}

void MainWindow::saveSettingsOnExit()
{
    // table column widths

    QHash<QString, QVariant> column_widths;
    for (const auto& column : AudioLibraryView::columnToStringMapping())
    {
        if (!_table->isColumnHidden(column.first))
        {
            column_widths[column.second] = _table->columnWidth(column.first);
        }
    }
    _settings.audio_library_view_column_widths.setValue(column_widths);

    // table visual indexes

    QHash<QString, QVariant> visual_indexes;

    for (const auto& column : AudioLibraryView::columnToStringMapping())
    {
        visual_indexes[column.second] = _table->horizontalHeader()->visualIndex(column.first);
    }

    _settings.audio_library_view_visual_indexes.setValue(visual_indexes);

    // table hidden columns

    QStringList hidden_columns;
    for (AudioLibraryView::Column column : _hidden_columns)
    {
        hidden_columns.push_back(AudioLibraryView::getColumnId(column));
    }
    _settings.audio_library_view_hidden_columns.setValue(hidden_columns);

    // current view

    for (const auto& i : _view_type_map)
        if (i.second == _view_stack->currentWidget())
        {
            _settings.main_window_view_type.setValue(i.first);
            break;
        }

    // icon size

    _settings.main_window_icon_size.setValue(_list->iconSize().width());

    // window geometry

    _settings.main_window_geometry.save(this);
}