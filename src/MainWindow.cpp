// SPDX-License-Identifier: GPL-2.0-only
#include "MainWindow.h"

#include <chrono>
#include <unordered_set>
#include <unordered_map>
#include <set>
#include <QtCore/qmimedata.h>
#include <QtCore/qprocess.h>
#include <QtCore/qsettings.h>
#include <QtCore/qstandardpaths.h>
#include <QtCore/qtimer.h>
#include <QtGui/qdesktopservices.h>
#include <QtGui/qdrag.h>
#include <QtGui/qevent.h>
#include <QtGui/qimagereader.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qmenubar.h>
#include <QtWidgets/qscrollbar.h>
#include <QtWidgets/qshortcut.h>
#include <QtWidgets/qfiledialog.h>
#include <QtWidgets/qheaderview.h>
#include <QtWidgets/qstyleditemdelegate.h>
#include <QtWidgets/qtoolbutton.h>
#include <QtWidgets/qtooltip.h>
#include "ImageViewWindow.h"
#include "project_version.h"
#include "resource_helpers.h"
#include "SettingsEditorWindow.h"

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

    double getRelativeScrollPos(QScrollBar* scroll_bar)
    {
        int64_t maximum = scroll_bar->maximum();
        int64_t minimum = scroll_bar->minimum();
        return double(scroll_bar->value()) / double(maximum - minimum);
    }

    void setRelativeScrollPos(QScrollBar* scroll_bar, double scroll_pos)
    {
        int64_t maximum = scroll_bar->maximum();
        int64_t minimum = scroll_bar->minimum();
        scroll_bar->setValue(int(scroll_pos * double(maximum - minimum)));
    }

    /**
    * A special QItemDelegate that can elide each line of a multi-lined text independently.
    */
    class MultiLinesElidedItemDelegate : public QStyledItemDelegate
    {
    public:
        MultiLinesElidedItemDelegate(QObject* parent);

        bool helpEvent(QHelpEvent* event, QAbstractItemView* view,
            const QStyleOptionViewItem& option, const QModelIndex& index) override;

    protected:
        void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override;

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

    _artist_button = new QRadioButton(AudioLibraryView::getDisplayModeFriendlyName(AudioLibraryView::DisplayMode::ARTISTS), this);
    _album_button = new QRadioButton(AudioLibraryView::getDisplayModeFriendlyName(AudioLibraryView::DisplayMode::ALBUMS), this);
    _track_button = new QRadioButton(AudioLibraryView::getDisplayModeFriendlyName(AudioLibraryView::DisplayMode::TRACKS), this);
    _year_button = new QRadioButton(AudioLibraryView::getDisplayModeFriendlyName(AudioLibraryView::DisplayMode::YEARS), this);
    _genre_button = new QRadioButton(AudioLibraryView::getDisplayModeFriendlyName(AudioLibraryView::DisplayMode::GENRES), this);

    _filter_box = new QLineEdit(this);
    _filter_box->setPlaceholderText(tr("Filter..."));
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
        return std::make_unique<AudioLibraryViewAllYears>();

    // either use a full view or a search

    QString filter_text = _filter_box->text();

    if (_artist_button->isChecked())
        return std::make_unique<AudioLibraryViewAllArtists>(filter_text);
    if (_album_button->isChecked())
        return std::make_unique<AudioLibraryViewAllAlbums>(filter_text);
    if (_track_button->isChecked())
        return std::make_unique<AudioLibraryViewAllTracks>(filter_text);
    if (_genre_button->isChecked())
        return std::make_unique<AudioLibraryViewAllGenres>(filter_text);

    // default to artist view if nothing is selected

    return std::make_unique<AudioLibraryViewAllArtists>(QString());
}

void ViewSelector::triggerDefaultView()
{
    _artist_button->click();
}

void ViewSelector::setButtonCheckedFromId(const QString& id)
{
    std::vector<std::pair<QRadioButton*, QString>> buttons_and_ids = {
        std::make_pair(_artist_button, AudioLibraryViewAllArtists::getBaseId()),
        std::make_pair(_album_button, AudioLibraryViewAllAlbums::getBaseId()),
        std::make_pair(_track_button, AudioLibraryViewAllTracks::getBaseId()),
        std::make_pair(_year_button, AudioLibraryViewAllYears::getBaseId()),
        std::make_pair(_genre_button, AudioLibraryViewAllGenres::getBaseId()),
    };

    for (const auto& button_and_id : buttons_and_ids)
    {
        if (id.startsWith(button_and_id.second))
        {
            button_and_id.first->setChecked(true);
            return;
        }
    }

    for (const auto& button_and_id : buttons_and_ids)
        button_and_id.first->setAutoExclusive(false);

    for (const auto& button_and_id : buttons_and_ids)
        button_and_id.first->setChecked(false);

    for (const auto& button_and_id : buttons_and_ids)
        button_and_id.first->setAutoExclusive(true);
}

void ViewSelector::radioButtonSelected()
{
    // filtering years makes no sense

    _filter_box->setEnabled(sender() != _year_button);

    emit selectionChanged();
}

//=============================================================================

void History::addItem(std::unique_ptr<AudioLibraryView> view, bool is_top_level_view, ViewRestoreData* restore_data_for_previous_view)
{
    // save the restore data

    if (restore_data_for_previous_view && !_items.empty())
    {
        _items[_current_item].restore_data = std::make_unique<ViewRestoreData>(*restore_data_for_previous_view);
    }

    // if current item is not the last one, destroy all further items

    if(_current_item < _items.size())
        _items.erase(_items.begin() + _current_item + 1, _items.end());

    // create new item

    Item item;
    item.view = std::move(view);
    item.is_top_level_view = is_top_level_view;

    _items.push_back(std::move(item));

    // prune the history if it becomes too long

    while (_items.size() > 1000)
    {
        auto second_top_level_item = std::find_if(_items.begin() + 1, _items.end(), [](const Item& i){
            return i.is_top_level_view;
        });

        _items.erase(_items.begin(), second_top_level_item);
    }

    _current_item = _items.size() - 1;
}

std::vector<const History::Item*> History::getCurrentItems() const
{
    // find top level view

    size_t index_of_top_level_view = 0;

    for (size_t i = _current_item; i != size_t(-1); --i)
    {
        if (_items[i].is_top_level_view)
        {
            index_of_top_level_view = i;
            break;
        }
    }

    // create result

    std::vector<const Item*> result;

    for (size_t i = index_of_top_level_view; i <= _current_item; ++i)
    {
        result.push_back(&_items[i]);
    }

    return result;
}

bool History::canGoBack() const
{
    return _current_item > 0 && _current_item < _items.size();
}

bool History::canGoForward() const
{
    return _current_item + 1 < _items.size();
}

void History::back()
{
    if(canGoBack())
        --_current_item;
}

void History::forward()
{
    if(canGoForward())
        ++_current_item;
}

//=============================================================================

class ContainingFolderOpener
{
public:
    ContainingFolderOpener(const QString& filepath);
    ContainingFolderOpener(const ContainingFolderOpener& other) = default;
    ContainingFolderOpener& operator=(const ContainingFolderOpener& other) = default;

    bool isSupported() const;
    void operator()() const;

private:
    QString _filepath;
};

ContainingFolderOpener::ContainingFolderOpener(const QString& filepath)
    : _filepath(filepath)
{
}

bool ContainingFolderOpener::isSupported() const
{
#ifdef _WIN32
    return true;
#else
    return false;
#endif
}

void ContainingFolderOpener::operator()() const
{
#ifdef _WIN32
    // the explorer expects backslashes
    QString filepath = _filepath;
    filepath.replace('/', '\\');

    QProcess::startDetached("explorer /select," + filepath);
#endif
}

//=============================================================================

MainWindow::MainWindow(Settings& settings)
    : _settings(settings)
    , _audio_files_loader(_library)
{
    setWindowTitle(APPLICATION_NAME);

    auto menubar = new QMenuBar(this);
    auto filemenu = menubar->addMenu(tr("&File"));
    addMenuAction(*filemenu, tr("Preferences..."), this, &MainWindow::onEditPreferences, QKeySequence::Preferences);
    filemenu->addSeparator();
    addMenuAction(*filemenu, tr("Exit"), this, &MainWindow::close, QKeySequence::Quit);

    auto viewmenu = menubar->addMenu(tr("&View"));
    _history_back_action = addMenuAction(*viewmenu, tr("Previous view"), this, &MainWindow::onHistoryBack, QKeySequence::Back);
    _history_forward_action = addMenuAction(*viewmenu, tr("Next view"), this, &MainWindow::onHistoryForward, QKeySequence::Forward);
    addMenuAction(*viewmenu, tr("Find..."), this, &MainWindow::onShowFindWidget, QKeySequence::Find);
    addMenuAction(*viewmenu, tr("Badly tagged albums"), this, &MainWindow::onShowDuplicateAlbums);
    addMenuAction(*viewmenu, tr("Reload all files"), this, &MainWindow::scanAudioDirs, QKeySequence::Refresh);

    auto toolarea = new QWidget(this);

    QToolButton* view_selector_popup_button = new QToolButton(toolarea);
    view_selector_popup_button->setToolTip(tr("Select view"));

    view_selector_popup_button->setIcon(iconFromResource(res::VIEW_MENU_SVG()));
    view_selector_popup_button->setIconSize(QSize(24, 24));

    connect(&_view_selector, &ViewSelector::selectionChanged, this, [this]() {
        setBreadCrumb(_view_selector.getSelectedView());
    });

    connect(view_selector_popup_button, &QToolButton::clicked, this, [this, view_selector_popup_button](){
        QPoint pos = view_selector_popup_button->mapToGlobal(view_selector_popup_button->rect().bottomLeft());

        _view_selector.move(pos);
        _view_selector.show();
    });

    _display_mode_tabs = new QTabBar(toolarea);
    _display_mode_tabs->setAutoHide(true);

    _model = new AudioLibraryModel(this);

    _list = new QListView(this);
    _list->setModel(_model->getModel());
    _list->setViewMode(QListView::IconMode);
    _list->setResizeMode(QListView::Adjust);
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
    _table->setModel(_model->getModel());
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

    _details = new DetailsPane(this);

    _details_splitter = new QSplitter(Qt::Horizontal);
    _details_splitter->addWidget(_view_stack);
    _details_splitter->addWidget(_details);
    _details_splitter->setChildrenCollapsible(false);

    _view_type_tabs = new QTabBar(toolarea);
    addViewTypeTab(_list, tr("Icons"), "icons");
    addViewTypeTab(_table, tr("Table"), "table");

    QToolButton* details_toggle_button = new QToolButton(toolarea);
    details_toggle_button->setText(tr("Details"));
    details_toggle_button->setToolTip(tr("Show details pane"));

    _status_bar = new QStatusBar(this);
    _status_bar->setSizeGripEnabled(false);

    QHBoxLayout* breadcrumb_layout_wrapper = new QHBoxLayout();
    _breadcrumb_layout = new QHBoxLayout();
    breadcrumb_layout_wrapper->addWidget(view_selector_popup_button);
    breadcrumb_layout_wrapper->addLayout(_breadcrumb_layout);
    breadcrumb_layout_wrapper->addStretch();
    breadcrumb_layout_wrapper->addWidget(_display_mode_tabs);
    breadcrumb_layout_wrapper->addWidget(_view_type_tabs);
    breadcrumb_layout_wrapper->addWidget(details_toggle_button);

    QVBoxLayout* tool_vbox = new QVBoxLayout(toolarea);
    tool_vbox->addLayout(breadcrumb_layout_wrapper);

    QVBoxLayout* vbox = new QVBoxLayout(this);
    vbox->setMargin(0);
    vbox->setSpacing(0);
    vbox->setMenuBar(menubar);
    vbox->addWidget(toolarea);
    vbox->addWidget(_details_splitter, 1);
    vbox->addWidget(_status_bar);

    connect(&_audio_files_loader, &AudioFilesLoader::libraryCacheLoading, this, &MainWindow::onLibraryCacheLoading);
    connect(&_audio_files_loader, &AudioFilesLoader::libraryLoadProgressed, this, &MainWindow::onLibraryLoadProgressed);
    connect(&_audio_files_loader, &AudioFilesLoader::libraryLoadFinished, this, &MainWindow::onLibraryLoadFinished);
    connect(&_audio_files_loader, &AudioFilesLoader::coverLoadFinished, this, &MainWindow::onCoverLoadFinished);
    connect(_display_mode_tabs, &QTabBar::tabBarClicked, this, &MainWindow::onDisplayModeSelected);
    connect(_view_type_tabs, &QTabBar::tabBarClicked, this, &MainWindow::onViewTypeSelected);
    connect(_list, &QAbstractItemView::doubleClicked, this, &MainWindow::onItemDoubleClicked);
    connect(_table, &QAbstractItemView::doubleClicked, this, &MainWindow::onItemDoubleClicked);
    connect(_table->horizontalHeader(), &QHeaderView::sectionClicked, this, &MainWindow::onTableHeaderSectionClicked);
    connect(_table->horizontalHeader(), &QHeaderView::customContextMenuRequested, this, &MainWindow::onTableHeaderContextMenu);
    connect(details_toggle_button, &QToolButton::clicked, this, &MainWindow::onToggleDetails);
    connect(_list->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::onModelSelectionChanged);
    connect(_table->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::onModelSelectionChanged);

    _view_selector.triggerDefaultView();

    restoreSettingsOnStart();

    show();

    // must be done after the main window is visible
    restoreDetailsSizeOnStart();

    if (_settings.audio_dir_paths.getValue().isEmpty())
    {
        FirstStartDialog dlg(this, _settings);
        dlg.exec();
    }

    scanAudioDirs();
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
                        auto insert_result = rows.insert(selected_index.row());
                        if (insert_result.second)
                        {
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
                        drag.exec(Qt::CopyAction);
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
        case QEvent::Paint:
            if (view->model()->rowCount() == 0)
            {
                QPainter p(view->viewport());
                p.drawText(QRect(QPoint(0, 0), view->viewport()->size()), Qt::AlignCenter, tr("This view is empty"));
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
        scanAudioDirs();
    }
}

void MainWindow::onShowFindWidget()
{
    if (!_find_widget)
    {
        _find_widget = new QWidget(this, Qt::Window);

        _find_widget_line_edit = new QLineEdit(_find_widget);
        _find_widget_line_edit->setPlaceholderText(tr("Find..."));
        _find_widget_line_edit->setClearButtonEnabled(true);
        connect(_find_widget_line_edit, &QLineEdit::returnPressed, this, &MainWindow::onFindNext);

        QPushButton* search_button = new QPushButton(_find_widget);
        search_button->setText(tr("Find Next"));
        connect(search_button, &QPushButton::clicked, this, &MainWindow::onFindNext);

        QShortcut* shortcut_escape = new QShortcut(Qt::Key_Escape, _find_widget);
        connect(shortcut_escape, &QShortcut::activated, _find_widget, &QWidget::close);

        QHBoxLayout* box = new QHBoxLayout(_find_widget);
        box->addWidget(_find_widget_line_edit);
        box->addWidget(search_button);
    }

    _find_widget_line_edit->selectAll();
    _find_widget_line_edit->setFocus();
    _find_widget->show();
    _find_widget->activateWindow();
}

void MainWindow::onFindNext()
{
    if (QAbstractItemView* view = qobject_cast<QAbstractItemView*>(_view_stack->currentWidget()))
    {
        int start_row = 0;

        QModelIndexList selected_indexes = view->selectionModel()->selectedIndexes();

        for (const QModelIndex& index : selected_indexes)
        {
            start_row = std::max(start_row, index.row());
        }

        if (!selected_indexes.isEmpty())
            ++start_row;

        QString search_text = _find_widget_line_edit->text();

        for (int i = 0, n = view->model()->rowCount(); i < n; ++i)
        {
            int row = (start_row + i) % n;

            QModelIndex index = view->model()->index(row, 0);

            QString item_text = index.data().toString();

            if (item_text.contains(search_text, Qt::CaseInsensitive))
            {
                setCurrentSelectedIndex(index);

                view->scrollTo(index);

                return;
            }
        }
    }
}

void MainWindow::onLibraryCacheLoading()
{
    size_t num_tracks = 0;

    {
        ThreadSafeAudioLibrary::LibraryAccessor acc(_library);

        num_tracks += acc.getLibrary().getNumberOfTracks();
    }

    const QString message = tr("Loading cache: %1 files", nullptr, int(num_tracks));

    _status_bar->showMessage(message.arg(num_tracks));

    updateCurrentViewIfOlderThan(1000);
}

void MainWindow::onLibraryLoadProgressed(int files_loaded, int files_in_cache)
{
    int num_tracks = files_in_cache + files_loaded;

    const QString message = tr("%1 files loaded", nullptr, num_tracks);

    _status_bar->showMessage(message.arg(num_tracks));

    updateCurrentViewIfOlderThan(1000);
}

void MainWindow::onLibraryLoadFinished(int files_loaded, int files_in_cache, float duration_sec)
{
    int num_tracks = files_in_cache + files_loaded;

    const QString message = tr("%1 files loaded in %2s", nullptr, num_tracks);

    _status_bar->showMessage(message.arg(num_tracks).arg(duration_sec, 0, 'f', 1));

    updateCurrentView();
}

void MainWindow::onCoverLoadFinished()
{
    updateCurrentView();
}

void MainWindow::onShowDuplicateAlbums()
{
    setBreadCrumb(std::make_unique<AudioLibraryViewDuplicateAlbums>());
}

void MainWindow::onBreadCrumbClicked()
{
    restoreBreadCrumb(sender());
}

void MainWindow::onHistoryBack()
{
    if (_history.canGoBack())
    {
        _history.back();

        updateAfterHistoryChange();
    }
}

void MainWindow::onHistoryForward()
{
    if (_history.canGoForward())
    {
        _history.forward();

        updateAfterHistoryChange();
    }
}

void MainWindow::onDisplayModeChanged(AudioLibraryView::DisplayMode display_mode)
{
    auto modes = getCurrentView()->getSupportedModes();

    // if there is a choice, remember selected display mode for later

    if (modes.size() > 1)
    {
        auto found = std::find_if(_selected_display_modes.begin(), _selected_display_modes.end(), [modes](const std::pair<std::vector<AudioLibraryView::DisplayMode>, AudioLibraryView::DisplayMode> & i) {
            return i.first == modes;
            });

        if (found != _selected_display_modes.end())
        {
            found->second = display_mode;
        }
        else
        {
            _selected_display_modes.emplace_back(modes, display_mode);
        }
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

    if (const AudioLibraryView* view = _model->getViewForIndex(index))
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
        QAction* action = menu.addAction(tr("Hide"));
        connect(action, &QAction::triggered, [this, clicked_index](){
            _table->setColumnHidden(clicked_index, true);

            _hidden_columns.insert(static_cast<AudioLibraryView::Column>(clicked_index));
        });

        menu.addSeparator();
    }

    // get and sort columns

    std::vector<std::pair<AudioLibraryView::Column, QString>> columns_and_names;

    for (AudioLibraryView::Column column : AudioLibraryView::getColumnsForDisplayMode(*_current_display_mode))
    {
        QString name = AudioLibraryView::getColumnFriendlyName(column, *_current_display_mode);
        columns_and_names.emplace_back(column, name);
    }

    std::sort(columns_and_names.begin(), columns_and_names.end(), [](const std::pair<AudioLibraryView::Column, QString>& a, const std::pair<AudioLibraryView::Column, QString>& b){
        return a.second < b.second;
    });

    // add columns to menu

    for (const auto& column_and_name : columns_and_names)
    {
        AudioLibraryView::Column column = column_and_name.first;
        QString name = column_and_name.second;

        bool column_hidden = _table->isColumnHidden(column);

        QAction* action = menu.addAction(name);
        action->setCheckable(true);
        action->setChecked(!column_hidden);
        connect(action, &QAction::triggered, [this, column, column_hidden, pos]() {
            _table->setColumnHidden(column, !column_hidden);

            if (column_hidden)
            {
                int column_visual_index = _table->horizontalHeader()->visualIndex(column);
                int clicked_visual_index = _table->horizontalHeader()->visualIndexAt(pos.x());

                if (column_visual_index != -1 && clicked_visual_index != -1)
                {
                    _table->horizontalHeader()->moveSection(column_visual_index, clicked_visual_index);
                }

                _hidden_columns.erase(column);
            }
            else
                _hidden_columns.insert(column);
        });
    }

    QPoint global_pos = _table->horizontalHeader()->mapToGlobal(pos);

    if (!menu.actions().isEmpty())
        menu.exec(global_pos);
}

void MainWindow::onToggleDetails()
{
    _details->setVisible(!_details->isVisibleTo(_details->parentWidget()));
}

void MainWindow::onModelSelectionChanged()
{
    QAbstractItemView* current_view = static_cast<QAbstractItemView*>(_view_stack->currentWidget());

    _details->setSelection(_model->getModel(), current_view, *_current_display_mode);
}

void MainWindow::saveLibrary()
{
    QString cache_dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QString filepath = cache_dir + "/AudioLibrary";

    _library.saveToCache(cache_dir, filepath);
}

void MainWindow::scanAudioDirs()
{
    QString cache_dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QString filepath = cache_dir + "/AudioLibrary";

    _audio_files_loader.startLoading(filepath, _settings.audio_dir_paths.getValue());
}

const AudioLibraryView* MainWindow::getCurrentView() const
{
    return _history.getCurrentItems().back()->view.get();
}

void MainWindow::updateCurrentView()
{
    const AudioLibraryView* current_view = getCurrentView();

    auto view_settings = saveViewSettings();

    QString current_view_id = current_view->getId();

    auto supported_modes = current_view->getSupportedModes();

    AudioLibraryView::DisplayMode current_display_mode = supported_modes.front();

    // restore user-selected display mode

    if (supported_modes.size() > 1)
    {
        auto found_selected_display_mode = std::find_if(_selected_display_modes.begin(), _selected_display_modes.end(), [supported_modes](const std::pair<std::vector<AudioLibraryView::DisplayMode>, AudioLibraryView::DisplayMode>& i) {
            return i.first == supported_modes;
        });

        if (found_selected_display_mode != _selected_display_modes.end())
        {
            current_display_mode = found_selected_display_mode->second;
        }
    }

    const bool same_view = _current_view_id == current_view_id;
    const bool same_display_mode = _current_display_mode && current_display_mode == *_current_display_mode;

    const bool incremental = same_view && same_display_mode;

    _current_view_id = current_view_id;
    if (!_current_display_mode)
        _current_display_mode = std::make_unique<AudioLibraryView::DisplayMode>(current_display_mode);
    else
        *_current_display_mode = current_display_mode;

    // create tabs for supported display modes

    while (_display_mode_tabs->count() > 0)
        _display_mode_tabs->removeTab(0);
    _display_mode_tab_indexes.clear();

    for (AudioLibraryView::DisplayMode display_mode : supported_modes)
    {
        int index = _display_mode_tabs->addTab(AudioLibraryView::getDisplayModeFriendlyName(display_mode));
        _display_mode_tab_indexes.emplace_back(index, display_mode);

        if (current_display_mode == display_mode)
        {
            _display_mode_tabs->setCurrentIndex(index);
        }
    }

    // hide unused columns

    std::vector<AudioLibraryView::Column> available_columns;
    available_columns = AudioLibraryView::getColumnsForDisplayMode(current_display_mode);

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
        AudioLibraryModel::IncrementalUpdateScope update_scope(*_model);

        ThreadSafeAudioLibrary::LibraryAccessor acc(_library);

        current_view->createItems(acc.getLibrary(), current_display_mode, _model);
    }
    else
    {
        AudioLibraryModel* model = new AudioLibraryModel(this);

        QStringList model_headers;
        for (const auto& column : AudioLibraryView::columnToStringMapping())
        {
            model_headers.push_back(AudioLibraryView::getColumnFriendlyName(column.first, *_current_display_mode));
        }
        model->setHorizontalHeaderLabels(model_headers);

        {
            ThreadSafeAudioLibrary::LibraryAccessor acc(_library);

            current_view->createItems(acc.getLibrary(), current_display_mode, model);
        }

        _model->deleteLater();
        _model = model;
        _list->setModel(model->getModel());
        _table->setModel(model->getModel());

        connect(_list->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::onModelSelectionChanged);
        connect(_table->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::onModelSelectionChanged);
    }

    const int old_sort_section = _table->horizontalHeader()->sortIndicatorSection();
    const Qt::SortOrder old_sort_order = _table->horizontalHeader()->sortIndicatorOrder();

    restoreViewSettings(view_settings.get());

    const int new_sort_section = _table->horizontalHeader()->sortIndicatorSection();
    const Qt::SortOrder new_sort_order = _table->horizontalHeader()->sortIndicatorOrder();

    if (old_sort_section == new_sort_section &&
        old_sort_order   == new_sort_order)
    {
        // restoreViewSettings only sorts the model if the sort section or order has changed
        // but at this point, the items are not ordered in any way
        // so sorting is needed even if the sort section and order have not changed

        _model->getModel()->sort(new_sort_section, new_sort_order);
    }

    onModelSelectionChanged();

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

    QModelIndex scroll_to_index = _list->currentIndex();
    if (!scroll_to_index.isValid())
        scroll_to_index = _list->model()->index(0, 0);

    auto found = std::find(_icon_size_steps.begin(), _icon_size_steps.end(), current_size);

    if (found != _icon_size_steps.end())
    {
        auto new_size_it = _icon_size_steps.end();

        if (direction > 0 && found + 1 != _icon_size_steps.end())
        {
            new_size_it = found + 1;
        }
        else if (direction < 0 && found != _icon_size_steps.begin())
        {
            new_size_it = found - 1;
        }

        if (new_size_it != _icon_size_steps.end())
        {
            _list->setIconSize(QSize(*new_size_it, *new_size_it));
            _list->scrollTo(scroll_to_index);
        }
    }
}

void MainWindow::addViewTypeTab(QWidget* view, const QString& friendly_name, const QString& internal_name)
{
    int index = _view_type_tabs->addTab(friendly_name);
    _view_type_tabs->setTabData(index, internal_name);
    _view_type_map.emplace_back(internal_name, view);
}

void MainWindow::getFilepathsFromIndex(const QModelIndex& index, std::vector<QString>& filepaths)
{
    if (!index.isValid())
        return;

    if (const AudioLibraryView* view = _model->getViewForIndex(index))
    {
        ThreadSafeAudioLibrary::LibraryAccessor acc(_library);

        std::vector<const AudioLibraryTrack*> tracks;
        view->resolveToTracks(acc.getLibrary(), tracks);

        for (const AudioLibraryTrack* track : tracks)
            filepaths.push_back(track->_filepath);

        return;
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

    if (const AudioLibraryView* view = _model->getViewForIndex(index))
    {
        ThreadSafeAudioLibrary::LibraryAccessor acc(_library);

        std::vector<const AudioLibraryTrack*> tracks;
        view->resolveToTracks(acc.getLibrary(), tracks);

        std::sort(tracks.begin(), tracks.end(), [](const AudioLibraryTrack* a, const AudioLibraryTrack* b) {
            if (a->_album->getKey().getArtist() != b->_album->getKey().getArtist())
                return a->_album->getKey().getArtist() < b->_album->getKey().getArtist();

            if (a->_album->getKey().getYear() != b->_album->getKey().getYear())
                return a->_album->getKey().getYear() < b->_album->getKey().getYear();

            if (a->_track_number != b->_track_number)
                return a->_track_number < b->_track_number;

            return a->_title < b->_title;
        });

        for (const AudioLibraryTrack* track : tracks)
            callback(track->_filepath);

        return;
    }

    QString path = _model->getFilepathFromIndex(index);
    if (!path.isEmpty())
    {
        callback(path);
    }
}

void MainWindow::updateAfterHistoryChange()
{
    _history_back_action->setEnabled(_history.canGoBack());
    _history_forward_action->setEnabled(_history.canGoForward());

    _view_selector.setButtonCheckedFromId(_history.getCurrentItems().front()->view->getId());

    _breadcrumb_buttons.clear();

    std::vector<const History::Item*> current_history_items = _history.getCurrentItems();

    for (const History::Item* item : current_history_items)
    {
        QPushButton* button = new QPushButton(item->view->getDisplayName(), this);
        connect(button, &QPushButton::clicked, this, &MainWindow::onBreadCrumbClicked);
        _breadcrumb_buttons.push_back(std::unique_ptr<QObject, LateDeleter>(button));

        _breadcrumb_layout->addWidget(button);
    }

    std::shared_ptr<ViewRestoreData> restore_data;
    if (current_history_items.back()->restore_data)
        restore_data.reset(new ViewRestoreData(*current_history_items.back()->restore_data));

    updateCurrentView();

    if (restore_data)
    {
        // restore uses a timer because the list view is updating asynchronously

        QTimer::singleShot(1, this, [this, restore_data]() {
            restoreViewSettings(restore_data.get());
            });
    }
}

void MainWindow::setBreadCrumb(std::unique_ptr<AudioLibraryView> view)
{
    _history.addItem(view->clone(), true, nullptr);

    // Reset scroll position.
    // This prevents the scroll position from being all over the place when going from e.g. artists to genres.

    _list->verticalScrollBar()->setValue(0);
    _table->verticalScrollBar()->setValue(0);

    updateAfterHistoryChange();
}

void MainWindow::addBreadCrumb(std::unique_ptr<AudioLibraryView> view)
{
    _history.addItem(view->clone(), false, saveViewSettings().get());

    // Reset scroll position.
    // This prevents the scroll position from being all over the place when going from e.g. artists to genres.

    _list->verticalScrollBar()->setValue(0);
    _table->verticalScrollBar()->setValue(0);

    updateAfterHistoryChange();
}

void MainWindow::restoreBreadCrumb(QObject* object)
{
    if (_breadcrumb_buttons.back().get() == object)
        return; // nothing to do

    // remove all breadcrumbs behind the given one

    auto found = std::find_if(_breadcrumb_buttons.begin(), _breadcrumb_buttons.end(), [object](const std::unique_ptr<QObject, LateDeleter>& i){
        return i.get() == object;
    });

    for(auto it = found + 1, end = _breadcrumb_buttons.end(); it != end; ++it)
        _history.back();

    updateAfterHistoryChange();
}

bool MainWindow::findBreadcrumbId(const QString& id) const
{
    for (const History::Item* item : _history.getCurrentItems())
    {
        if (item->view->getId() == id)
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
            rows.insert(selected_index.row());
        }

        QMenu menu;

        if (!getVlcPath().isEmpty())
        {
            QList<QPersistentModelIndex> selected_row_indexes;
            for (int row : rows)
                selected_row_indexes.push_back(view->model()->index(row, AudioLibraryView::ZERO));

            QAction* add_to_vlc_action = menu.addAction(tr("Add to VLC Playlist"));

            connect(add_to_vlc_action, &QAction::triggered, this, [this, selected_row_indexes]() {
                startVlc(selected_row_indexes, true);
            });

            QAction* play_with_vlc_action = menu.addAction(tr("Play with VLC"));

            connect(play_with_vlc_action, &QAction::triggered, this, [this, selected_row_indexes]() {
                startVlc(selected_row_indexes, false);
            });
        }

        if (rows.size() == 1)
        {
            const QVariant artist_variant         = mouse_index.sibling(mouse_index.row(), AudioLibraryView::ARTIST).data();
            const QVariant album_variant          = mouse_index.sibling(mouse_index.row(), AudioLibraryView::ALBUM).data();
            const QVariant year_variant           = mouse_index.sibling(mouse_index.row(), AudioLibraryView::YEAR).data();
            const QVariant genre_variant          = mouse_index.sibling(mouse_index.row(), AudioLibraryView::GENRE).data();
            const QVariant cover_checksum_variant = mouse_index.sibling(mouse_index.row(), AudioLibraryView::COVER_CHECKSUM).data();

            const QString artist = artist_variant.toString();

            // show artist

            if (artist_variant.isValid() && !artist.isEmpty())
            {
                auto artist_view = std::make_shared<AudioLibraryViewArtist>(artist);

                if (!findBreadcrumbId(artist_view->getId()))
                {
                    QAction* artist_action = menu.addAction(tr("More from artist \"%1\"...").arg(artist));

                    auto slot = [this, artist_view]() {
                        addBreadCrumb(artist_view->clone());
                    };

                    connect(artist_action, &QAction::triggered, this, slot);
                }
            }

            // only for tracks: show album

            QString filepath = _model->getFilepathFromIndex(mouse_index);

            if (!filepath.isEmpty())
            {
                {
                    bool year_ok = false;
                    int year = year_variant.toInt(&year_ok);
                    if (!year_ok)
                        year = 0;

                    bool cover_checksum_ok = false;
                    uint cover_checksum = cover_checksum_variant.toUInt(&cover_checksum_ok);
                    if (!cover_checksum_ok)
                        cover_checksum = 0;

                    AudioLibraryAlbumKey key(artist, album_variant.toString(), genre_variant.toString(), year, cover_checksum);

                    auto album_view = std::make_shared<AudioLibraryViewAlbum>(key);

                    if (!findBreadcrumbId(album_view->getId()))
                    {
                        QAction* artist_action = menu.addAction(tr("Show album \"%1\"").arg(key.getAlbum()));

                        auto slot = [this, album_view]() {
                            addBreadCrumb(album_view->clone());
                        };

                        connect(artist_action, &QAction::triggered, this, slot);
                    }
                }

                ContainingFolderOpener containing_folder_opener(filepath);

                if (containing_folder_opener.isSupported())
                {
                    QAction* action = menu.addAction(tr("Open containing folder"));

                    connect(action, &QAction::triggered, this, containing_folder_opener);
                }
            }

            const QVariant icon_variant = mouse_index.sibling(mouse_index.row(), AudioLibraryView::ZERO).data(Qt::DecorationRole);

            if(icon_variant.isValid())
            {
                QIcon icon = icon_variant.value<QIcon>();

                if (!_model->isDefaultIcon(icon) &&
                    !icon.availableSizes().empty())
                {
                    QPixmap pixmap = icon.pixmap(icon.availableSizes().front());

                    QAction* action = menu.addAction(tr("View coverart"));

                    auto slot = [this, pixmap]() {
                        ImageViewWindow* image_view = new ImageViewWindow(_settings);
                        image_view->setPixmap(pixmap);
                        image_view->show();
                    };

                    connect(action, &QAction::triggered, this, slot);
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
    QSettings vlc_registry("HKEY_LOCAL_MACHINE\\SOFTWARE\\VideoLAN\\VLC", QSettings::NativeFormat);
    return vlc_registry.value("Default").toString();
#else
    return QString();
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

void MainWindow::setCurrentSelectedIndex(const QModelIndex& index)
{
    QItemSelection selection;

    const QModelIndex end = index.sibling(index.row(), _model->getModel()->columnCount() - 1);
    selection.push_back(QItemSelectionRange(index, end));

    if (QAbstractItemView* view = qobject_cast<QAbstractItemView*>(_view_stack->currentWidget()))
    {
        view->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
        view->setCurrentIndex(index);
    }
}

std::unique_ptr<ViewRestoreData> MainWindow::saveViewSettings() const
{
    auto restore_data = std::make_unique<ViewRestoreData>();

    restore_data->_list_scroll_pos = getRelativeScrollPos(_list->verticalScrollBar());
    restore_data->_table_scroll_pos = getRelativeScrollPos(_table->verticalScrollBar());

    restore_data->_table_sort_indicator_section = _table->horizontalHeader()->sortIndicatorSection();
    restore_data->_table_sort_indicator_order = _table->horizontalHeader()->sortIndicatorOrder();

    // save selection

    QModelIndexList selected_indexes = qobject_cast<QAbstractItemView*>(_view_stack->currentWidget())->selectionModel()->selectedIndexes();

    QModelIndex first_index;
    bool multiple_rows_selected = false;

    for (const QModelIndex& index : selected_indexes)
    {
        if (index.column() != AudioLibraryView::ZERO)
            continue;

        if (first_index.isValid())
            multiple_rows_selected = true;
        else
            first_index = index;
    }

    if (first_index.isValid() && !multiple_rows_selected)
    {
        const QString id = _model->getItemId(first_index);
        if (!id.isEmpty())
            restore_data->_selected_item = id;
    }

    return restore_data;
}

void MainWindow::restoreViewSettings(ViewRestoreData* restore_data)
{
    setRelativeScrollPos(_list->verticalScrollBar(), restore_data->_list_scroll_pos);
    setRelativeScrollPos(_table->verticalScrollBar(), restore_data->_table_scroll_pos);

    int sort_indicator_section = restore_data->_table_sort_indicator_section;
    Qt::SortOrder sort_indicator_order = restore_data->_table_sort_indicator_order;

    if (sort_indicator_section < 0 ||
        _table->isColumnHidden(sort_indicator_section))
    {
        // don't sort by invisible columns
        // default to the zero column which is always present

        sort_indicator_section = AudioLibraryView::Column::ZERO;
        sort_indicator_order = Qt::AscendingOrder;
    }

    _table->sortByColumn(sort_indicator_section, sort_indicator_order);

    // restore selection

    if (!restore_data->_selected_item.isEmpty())
    {
        const QModelIndex index = _model->getIndexForId(restore_data->_selected_item);
        setCurrentSelectedIndex(index);
    }
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
                logical_and_visual_indexes.emplace_back(column.first, visual_index);
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

    // details

    const QList<int> details_splitter_sizes = _details_splitter->sizes();
    if (details_splitter_sizes.size() >= 2)
    {
        _settings.details_width.setValue(details_splitter_sizes[1]);
    }
}

void MainWindow::restoreDetailsSizeOnStart()
{
    const int details_width = _settings.details_width.getValue();

    const bool details_visible = details_width > 0;

    _details->setVisible(details_visible);

    if (details_visible)
    {
        QList<int> sizes = _details_splitter->sizes();

        // redistribute sizes between view and details

        int size_view = sizes[0] + sizes[1] - details_width;

        sizes[0] = size_view;
        sizes[1] = details_width;

        _details_splitter->setSizes(sizes);
    }
}