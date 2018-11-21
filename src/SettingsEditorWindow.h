// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include "Settings.h"

#include <memory>
#include <QtWidgets/qdialog.h>

class AbstractSettingsWidget
{
public:
    virtual ~AbstractSettingsWidget();

    virtual QWidget* getWidget() const = 0;
    virtual void applyChanges() const = 0;
};

class SettingsEditorDialog : public QDialog
{
public:
    SettingsEditorDialog(QWidget* parent, Settings& settings);

    virtual void accept() override;

protected:
    virtual void closeEvent(QCloseEvent* e) override;

private:
    Settings& _settings;
    std::vector<std::unique_ptr<AbstractSettingsWidget>> _widgets;
};