// SPDX-License-Identifier: GPL-2.0-only
#include "Settings.h"

#include <QtCore/qsettings.h>
#include <QtCore/qprocess.h>
#include <QtCore/qcoreapplication.h>
#include "project_version.h"

SettingsAdapter::SettingsAdapter()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::applicationName());
    for (const QString& key : settings.allKeys())
        _data[key] = settings.value(key);
}

void SettingsAdapter::setValue(const QString& key, const QVariant& value)
{
    _data[key] = value;

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QCoreApplication::applicationName());
    settings.setValue(key, value);
}

//=============================================================================

SettingsItemBase::SettingsItemBase(SettingsAdapter& settings, const QString& key)
    : _settings(settings)
    , _key(key)
{
}

SettingsAdapter& SettingsItemBase::getSettings() const
{
    return _settings;
}

const QString& SettingsItemBase::getKey() const
{
    return _key;
}

//=============================================================================

SettingsItemWindowGeometry::SettingsItemWindowGeometry(SettingsAdapter& settings, const QString& key)
    : SettingsItemBase(settings, key)
{
}

void SettingsItemWindowGeometry::save(QWidget* w)
{
    getSettings().setValue(getKey(), w->saveGeometry());
}

void SettingsItemWindowGeometry::restore(QWidget* w) const
{
    QByteArray geometry;
    if (getSettings().getValue<QByteArray>(getKey(), geometry))
        w->restoreGeometry(geometry);
}

//=============================================================================

Settings::Settings()
    : _settings()
    , main_window_geometry(_settings, "main_window_geometry")
    , settings_window_geometry(_settings, "settings_window_geometry")
    , audio_dir_paths(_settings, "audio_dir_paths", QStringList())
    , audio_library_view_column_widths(_settings, "audio_library_view_column_widths", QHash<QString, QVariant>())
    , audio_library_view_hidden_columns(_settings, "audio_library_view_hidden_columns", QStringList())
    , main_window_view_type(_settings, "main_window_view_type", QString())
    , main_window_icon_size(_settings, "main_window_icon_size", 0)
{
}