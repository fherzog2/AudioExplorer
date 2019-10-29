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

AbstractSettingsWidget::~AbstractSettingsWidget()
{
}

//=============================================================================

class SettingsWidgetDirPaths : public AbstractSettingsWidget
{
public:
    SettingsWidgetDirPaths(QWidget* parent, SettingsItem<QStringList>& item, const QString& button_text);

    virtual QWidget* getWidget() const override;
    virtual void applyChanges() const override;

private:
    void deleteSelectedRows() const;

    SettingsItem<QStringList>& _item;
    QFrame* _container = nullptr;
    QListView* _list = nullptr;
    QStandardItemModel* _model = nullptr;
};

SettingsWidgetDirPaths::SettingsWidgetDirPaths(QWidget* parent, SettingsItem<QStringList>& item, const QString& button_text)
    : _item(item)
{
    _container = new QFrame(parent);

    QPushButton* add_button = new QPushButton(button_text, _container);
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
        QAction* delete_action = menu.addAction("Delete");
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

FirstStartDialog::FirstStartDialog(QWidget* parent, Settings& settings)
    : QDialog(parent)
    , _settings(settings)
{
    auto layout = new QVBoxLayout(this);

    setWindowTitle("Initialization");

    _audio_dir_paths_widget.reset(new SettingsWidgetDirPaths(this, settings.audio_dir_paths, "Add audio directory..."));
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

    setWindowTitle("Preferences");

    _widgets.push_back(std::unique_ptr<AbstractSettingsWidget>(new SettingsWidgetDirPaths(this, settings.audio_dir_paths, "Add audio directory...")));
    layout->addWidget(_widgets.back()->getWidget());

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