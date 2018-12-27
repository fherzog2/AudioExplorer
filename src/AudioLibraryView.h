// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <QtGui/qstandarditemmodel.h>
#include "AudioLibrary.h"

class AudioLibraryView
{
public:
    virtual ~AudioLibraryView();

    enum Column
    {
        ZERO = 0,
        ARTIST = 1,
        ALBUM = 2,
        YEAR = 3,
        GENRE = 4,
        COVER_CHECKSUM = 5,
        TITLE = 6,
        TRACK_NUMBER = 7,
        DISC_NUMBER = 8,
        ALBUM_ARTIST = 9,
        COMMENT = 10,
        PATH = 11,
        TAG_TYPES = 12,
    };
    static QString getColumnFriendlyName(Column column);
    static std::vector<std::pair<Column, QString>> columnToStringMapping();

    enum class DisplayMode
    {
        ALBUMS,
        TRACKS,
    };
    static QString getDisplayModeFriendlyName(DisplayMode mode);
    static std::vector<std::pair<DisplayMode, QString>> displayModeToStringMapping();
    static std::vector<Column> getColumnsForDisplayMode(DisplayMode mode);

    virtual AudioLibraryView* clone() const = 0;
    virtual QString getDisplayName() const = 0;
    virtual std::vector<DisplayMode> getSupportedModes() const = 0;
    virtual void createItems(const AudioLibrary& library,
        const DisplayMode* display_mode,
        QStandardItemModel* model,
        std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items) const = 0;
    virtual void resolveToTracks(const AudioLibrary& library, std::vector<const AudioLibraryTrack*>& tracks) const;
    virtual QString getId() const;
};

class AudioLibraryViewAllArtists : public AudioLibraryView
{
public:
    virtual AudioLibraryView* clone() const override;
    virtual QString getDisplayName() const override;
    virtual std::vector<DisplayMode> getSupportedModes() const override;
    virtual void createItems(const AudioLibrary& library,
        const DisplayMode* display_mode,
        QStandardItemModel* model,
        std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items) const override;
};

class AudioLibraryViewAllAlbums : public AudioLibraryView
{
public:
    virtual AudioLibraryView* clone() const override;
    virtual QString getDisplayName() const override;
    virtual std::vector<DisplayMode> getSupportedModes() const override;
    virtual void createItems(const AudioLibrary& library,
        const DisplayMode* display_mode,
        QStandardItemModel* model,
        std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items) const override;
};

class AudioLibraryViewAllTracks : public AudioLibraryView
{
public:
    virtual AudioLibraryView* clone() const override;
    virtual QString getDisplayName() const override;
    virtual std::vector<DisplayMode> getSupportedModes() const override;
    virtual void createItems(const AudioLibrary& library,
        const DisplayMode* display_mode,
        QStandardItemModel* model,
        std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items) const override;
};

class AudioLibraryViewAllYears : public AudioLibraryView
{
public:
    virtual AudioLibraryView* clone() const override;
    virtual QString getDisplayName() const override;
    virtual std::vector<DisplayMode> getSupportedModes() const override;
    virtual void createItems(const AudioLibrary& library,
        const DisplayMode* display_mode,
        QStandardItemModel* model,
        std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items) const override;
};

class AudioLibraryViewAllGenres : public AudioLibraryView
{
public:
    virtual AudioLibraryView* clone() const override;
    virtual QString getDisplayName() const override;
    virtual std::vector<DisplayMode> getSupportedModes() const override;
    virtual void createItems(const AudioLibrary& library,
        const DisplayMode* display_mode,
        QStandardItemModel* model,
        std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items) const override;
};

class AudioLibraryViewArtist : public AudioLibraryView
{
public:
    AudioLibraryViewArtist(const QString& artist);

    virtual AudioLibraryView* clone() const override;
    virtual QString getDisplayName() const override;
    virtual std::vector<DisplayMode> getSupportedModes() const override;
    virtual void createItems(const AudioLibrary& library,
        const DisplayMode* display_mode,
        QStandardItemModel* model,
        std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items) const override;
    virtual void resolveToTracks(const AudioLibrary& library, std::vector<const AudioLibraryTrack*>& tracks) const override;
    virtual QString getId() const override;

private:
    QString _artist;
};

class AudioLibraryViewAlbum : public AudioLibraryView
{
public:
    AudioLibraryViewAlbum(const AudioLibraryAlbumKey& key);

    virtual AudioLibraryView* clone() const override;
    virtual QString getDisplayName() const override;
    virtual std::vector<DisplayMode> getSupportedModes() const override;
    virtual void createItems(const AudioLibrary& library,
        const DisplayMode* display_mode,
        QStandardItemModel* model,
        std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items) const override;
    virtual void resolveToTracks(const AudioLibrary& library, std::vector<const AudioLibraryTrack*>& tracks) const override;
    virtual QString getId() const override;

private:
    AudioLibraryAlbumKey _key;
};

class AudioLibraryViewYear : public AudioLibraryView
{
public:
    AudioLibraryViewYear(int year);

    virtual AudioLibraryView* clone() const override;
    virtual QString getDisplayName() const override;
    virtual std::vector<DisplayMode> getSupportedModes() const override;
    virtual void createItems(const AudioLibrary& library,
        const DisplayMode* display_mode,
        QStandardItemModel* model,
        std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items) const override;
    virtual void resolveToTracks(const AudioLibrary& library, std::vector<const AudioLibraryTrack*>& tracks) const override;
    virtual QString getId() const override;

private:
    int _year;
};

class AudioLibraryViewGenre : public AudioLibraryView
{
public:
    AudioLibraryViewGenre(const QString& genre);

    virtual AudioLibraryView* clone() const override;
    virtual QString getDisplayName() const override;
    virtual std::vector<DisplayMode> getSupportedModes() const override;
    virtual void createItems(const AudioLibrary& library,
        const DisplayMode* display_mode,
        QStandardItemModel* model,
        std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items) const override;
    virtual void resolveToTracks(const AudioLibrary& library, std::vector<const AudioLibraryTrack*>& tracks) const override;
    virtual QString getId() const override;

private:
    QString _genre;
};

class AudioLibraryViewSimpleSearch : public AudioLibraryView
{
public:
    AudioLibraryViewSimpleSearch(const QString& search_text);

    virtual AudioLibraryView* clone() const override;
    virtual QString getDisplayName() const override;
    virtual std::vector<DisplayMode> getSupportedModes() const override;
    virtual void createItems(const AudioLibrary& library,
        const DisplayMode* display_mode,
        QStandardItemModel* model,
        std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items) const override;

private:
    static bool match(const QString& input, const QVector<QStringRef>& strings_to_match);

    QString _search_text;
};

class AudioLibraryViewAdvancedSearch : public AudioLibraryView
{
public:
    struct Query
    {
        QString artist;
        QString album;
        QString genre;
        QString title;
        QString comment;

        Qt::CaseSensitivity case_sensitive = Qt::CaseInsensitive;
        bool use_regex = false;
    };

    AudioLibraryViewAdvancedSearch(const Query& query);

    virtual AudioLibraryView* clone() const override;
    virtual QString getDisplayName() const override;
    virtual std::vector<DisplayMode> getSupportedModes() const override;
    virtual void createItems(const AudioLibrary& library,
        const DisplayMode* display_mode,
        QStandardItemModel* model,
        std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items) const override;

private:
    Query _query;
};

class AudioLibraryViewDuplicateAlbums : public AudioLibraryView
{
public:
    virtual AudioLibraryView* clone() const override;
    virtual QString getDisplayName() const override;
    virtual std::vector<DisplayMode> getSupportedModes() const override;
    virtual void createItems(const AudioLibrary& library,
        const DisplayMode* display_mode,
        QStandardItemModel* model,
        std::unordered_map<QStandardItem*, std::unique_ptr<AudioLibraryView>>& views_for_items) const override;
};