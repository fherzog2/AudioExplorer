// SPDX-License-Identifier: GPL-2.0-only
#include "SettingsEditorWindow.h"

#include <set>
#include <QtGui/qstandarditemmodel.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qmenu.h>
#include <QtWidgets/qdialogbuttonbox.h>
#include <QtWidgets/qfiledialog.h>
#include <QtWidgets/qlistview.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qshortcut.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qgroupbox.h>

class SettingsWidgetDirPaths : public AbstractSettingsWidget
{
public:
    SettingsWidgetDirPaths(QWidget* parent, SettingsItem<QStringList>& item);

    QWidget* getWidget() const override;
    void applyChanges() const override;

private:
    void deleteSelectedRows() const;

    SettingsItem<QStringList>& _item;
    QFrame* _container = nullptr;
    QListView* _list = nullptr;
    QStandardItemModel* _model = nullptr;
};

SettingsWidgetDirPaths::SettingsWidgetDirPaths(QWidget* parent, SettingsItem<QStringList>& item)
    : _item(item)
{
    _container = new QFrame(parent);

    QPushButton* add_button = new QPushButton(QObject::tr("Add audio directory..."), _container);
    QObject::connect(add_button, &QPushButton::clicked, [this](){
        QString path = QFileDialog::getExistingDirectory();
        if(!path.isEmpty())
            _model->setItem(_model->rowCount(), new QStandardItem(path));
    });

    _model = new QStandardItemModel();

    for (const QString& path : item.getValue())
    {
        _model->setItem(_model->rowCount(), new QStandardItem(path));
    }

    _list = new QListView(_container);
    _list->setModel(_model);
    _list->setContextMenuPolicy(Qt::CustomContextMenu);
    _list->setSelectionMode(QListView::ExtendedSelection);
    _list->setEditTriggers(QAbstractItemView::NoEditTriggers);
    QObject::connect(_list, &QWidget::customContextMenuRequested, [this](const QPoint& pos){
        QModelIndex index = _list->indexAt(pos);
        if (!index.isValid())
            return;

        QMenu menu;
        QAction* delete_action = menu.addAction(QObject::tr("Delete"));
        QObject::connect(delete_action, &QAction::triggered, [this](){
            deleteSelectedRows();
        });
        menu.exec(_list->mapToGlobal(pos));
    });

    auto delete_shortcut = new QShortcut(Qt::Key_Delete, _list);
    delete_shortcut->setContext(Qt::WidgetShortcut);
    QObject::connect(delete_shortcut, &QShortcut::activated, [this](){
        deleteSelectedRows();
    });

    QVBoxLayout* vbox = new QVBoxLayout(_container);
    vbox->addWidget(add_button);
    vbox->addWidget(_list);
}

QWidget* SettingsWidgetDirPaths::getWidget() const
{
    return _container;
}

void SettingsWidgetDirPaths::applyChanges() const
{
    QStringList paths;

    for (int i = 0, n = _model->rowCount(); i < n; ++i)
        paths.push_back(_model->item(i)->text());

    _item.setValue(paths);
}

void SettingsWidgetDirPaths::deleteSelectedRows() const
{
    // highest rows first for safe deletion
    std::set<int, std::greater<int>> rows;

    for (const QModelIndex& selected : _list->selectionModel()->selectedIndexes())
        rows.insert(selected.row());

    for (int row : rows)
        _model->removeRow(row);
}

//=============================================================================

class LanguageSelect : public AbstractSettingsWidget
{
public:
    LanguageSelect(QWidget* parent, SettingsItem<QString>& item);

    QWidget* getWidget() const override;
    void applyChanges() const override;

private:
    SettingsItem<QString>& _item;
    QComboBox* _combobox = nullptr;
    QGroupBox* _container = nullptr;
};

LanguageSelect::LanguageSelect(QWidget* parent, SettingsItem<QString>& item)
    : _item(item)
{
    _container = new QGroupBox(QObject::tr("Language"), parent);

    _combobox = new QComboBox();
    _combobox->addItem(QObject::tr("From operating system"), "");
    _combobox->addItem("English (en)", "en");
    _combobox->addItem("Deutsch (de)", "de");

    auto layout = new QVBoxLayout(_container);
    layout->addWidget(_combobox);

    for (int i = 0, n = _combobox->count(); i < n; ++i)
    {
        if (_combobox->itemData(i).toString() == item.getValue())
        {
            _combobox->setCurrentIndex(i);
            break;
        }
    }
}

QWidget* LanguageSelect::getWidget() const
{
    return _container;
}

void LanguageSelect::applyChanges() const
{
    _item.setValue(_combobox->currentData().toString());
}

//=============================================================================

FirstStartDialog::FirstStartDialog(QWidget* parent, Settings& settings)
    : QDialog(parent)
{
    auto layout = new QVBoxLayout(this);

    setWindowTitle(tr("Initialization"));

    _audio_dir_paths_widget.reset(new SettingsWidgetDirPaths(this, settings.audio_dir_paths));
    layout->addWidget(_audio_dir_paths_widget->getWidget());

    auto buttonbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    layout->addWidget(buttonbox);
}

void FirstStartDialog::accept()
{
    _audio_dir_paths_widget->applyChanges();

    QDialog::accept();
}

//=============================================================================

SettingsEditorDialog::SettingsEditorDialog(QWidget* parent, Settings& settings)
    : QDialog(parent)
    , _settings(settings)
{
    auto layout = new QVBoxLayout(this);

    setWindowTitle(tr("Preferences"));

    _widgets.push_back(std::make_unique<LanguageSelect>(this, settings.language));
    _widgets.push_back(std::unique_ptr<AbstractSettingsWidget>(new SettingsWidgetDirPaths(this, settings.audio_dir_paths)));

    for(const auto& w : _widgets)
        layout->addWidget(w->getWidget());

    auto buttonbox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    layout->addWidget(buttonbox);

    _settings.settings_window_geometry.restore(this);
}

void SettingsEditorDialog::accept()
{
    for (const auto& widget : _widgets)
        widget->applyChanges();

    QDialog::accept();
}

void SettingsEditorDialog::closeEvent(QCloseEvent* e)
{
    _settings.settings_window_geometry.save(this);

    QDialog::closeEvent(e);
}