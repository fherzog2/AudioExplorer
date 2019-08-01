// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <QtCore/qhash.h>
#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>
#include <QtWidgets/qwidget.h>
#include <map>

class SettingsAdapter
{
public:
    SettingsAdapter();

    void setValue(const QString& key, const QVariant& value);

    template<class T>
    bool getValue(const QString& key, T& value) const
    {
        auto found = _data.find(key);
        if (found == _data.end())
            return false;

        value = found->second.value<T>();
        return true;
    }

private:
    std::map<QString, QVariant> _data;
};

class SettingsItemBase
{
public:
    SettingsItemBase(SettingsAdapter& settings, const QString& key);

    SettingsAdapter& getSettings() const;
    const QString& getKey() const;

private:
    SettingsAdapter& _settings;
    QString _key;
};

template<class T>
class SettingsItem : public SettingsItemBase
{
public:
    SettingsItem(SettingsAdapter& settings, const QString& key, const T& default_value)
        : SettingsItemBase(settings, key)
        , _default_value(default_value)
    {
    }

    void setValue(const T& value)
    {
        getSettings().setValue(getKey(), value);
    }

    bool getValue(T& value) const
    {
        return getSettings().getValue(getKey(), value);
    }

    T getValue() const
    {
        T value;
        if (getValue(value))
            return value;

        return _default_value;
    }

private:
    T _default_value;
};

class SettingsItemWindowGeometry : public SettingsItemBase
{
public:
    SettingsItemWindowGeometry(SettingsAdapter& settings, const QString& key);

    void save(QWidget* w);
    void restore(QWidget* w) const;
};

class Settings
{
public:
    Settings();

private:
    SettingsAdapter _settings;

public:
    SettingsItemWindowGeometry main_window_geometry;
    SettingsItemWindowGeometry settings_window_geometry;
    SettingsItemWindowGeometry coverart_window_geometry;
    SettingsItem<QStringList> audio_dir_paths;
    SettingsItem<QHash<QString, QVariant>> audio_library_view_column_widths;
    SettingsItem<QHash<QString, QVariant>> audio_library_view_visual_indexes;
    SettingsItem<QStringList> audio_library_view_hidden_columns;
    SettingsItem<QString> main_window_view_type;
    SettingsItem<int> main_window_icon_size;
    SettingsItem<int> details_width;
};