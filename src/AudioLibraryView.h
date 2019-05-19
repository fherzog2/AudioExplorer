// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <QtGui/qstandarditemmodel.h>
#include "AudioLibrary.h"

class AudioLibraryModel;

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
        COVER_TYPE = 6,
        TITLE = 7,
        TRACK_NUMBER = 8,
        DISC_NUMBER = 9,
        ALBUM_ARTIST = 10,
        COMMENT = 11,
        PATH = 12,
        TAG_TYPES = 13,
        LENGTH_SECONDS = 14,
        CHANNELS = 15,
        BITRATE_KBS = 16,
        SAMPLERATE_HZ = 17,
    };
    static QString getColumnFriendlyName(Column column);
    static std::vector<std::pair<Column, QString>> columnToStringMapping();
    static QString getColumnId(Column column);
    static std::unique_ptr<Column> getColumnFromId(const QString& column_id);

    enum class DisplayMode
    {
        ARTISTS,
        ALBUMS,
        TRACKS,
        YEARS,
        GENRES,
    };
    static QString getDisplayModeFriendlyName(DisplayMode mode);
    static std::vector<std::pair<DisplayMode, QString>> displayModeToStringMapping();
    static std::vector<Column> getColumnsForDisplayMode(DisplayMode mode);

    static const int SORT_ROLE = Qt::UserRole + 1;
    static const int MULTILINE_DISPLAY_ROLE = Qt::UserRole + 2;
    static const int ID_ROLE = Qt::UserRole + 3;

    virtual AudioLibraryView* clone() const = 0;
    virtual QString getDisplayName() const = 0;
    virtual std::vector<DisplayMode> getSupportedModes() const = 0;
    virtual void createItems(const AudioLibrary& library,
        DisplayMode display_mode,
        AudioLibraryModel* model) const = 0;
    virtual void resolveToTracks(const AudioLibrary& library, std::vector<const AudioLibraryTrack*>& tracks) const;
    virtual QString getId() const = 0;
};

class AudioLibraryViewAllArtists : public AudioLibraryView
{
public:
    AudioLibraryViewAllArtists(QString filter);

    virtual AudioLibraryView* clone() const override;
    virtual QString getDisplayName() const override;
    virtual std::vector<DisplayMode> getSupportedModes() const override;
    virtual void createItems(const AudioLibrary& library,
        DisplayMode display_mode,
        AudioLibraryModel* model) const override;
    virtual QString getId() const override;

private:
    QString _filter;
};

class AudioLibraryViewAllAlbums : public AudioLibraryView
{
public:
    AudioLibraryViewAllAlbums(QString filter);

    virtual AudioLibraryView* clone() const override;
    virtual QString getDisplayName() const override;
    virtual std::vector<DisplayMode> getSupportedModes() const override;
    virtual void createItems(const AudioLibrary& library,
        DisplayMode display_mode,
        AudioLibraryModel* model) const override;
    virtual QString getId() const override;

private:
    QString _filter;
};

class AudioLibraryViewAllTracks : public AudioLibraryView
{
public:
    AudioLibraryViewAllTracks(QString filter);

    virtual AudioLibraryView* clone() const override;
    virtual QString getDisplayName() const override;
    virtual std::vector<DisplayMode> getSupportedModes() const override;
    virtual void createItems(const AudioLibrary& library,
        DisplayMode display_mode,
        AudioLibraryModel* model) const override;
    virtual QString getId() const override;

private:
    QString _filter;
};

class AudioLibraryViewAllYears : public AudioLibraryView
{
public:
    virtual AudioLibraryView* clone() const override;
    virtual QString getDisplayName() const override;
    virtual std::vector<DisplayMode> getSupportedModes() const override;
    virtual void createItems(const AudioLibrary& library,
        DisplayMode display_mode,
        AudioLibraryModel* model) const override;
    virtual QString getId() const override;
};

class AudioLibraryViewAllGenres : public AudioLibraryView
{
public:
    AudioLibraryViewAllGenres(const QString& filter);

    virtual AudioLibraryView* clone() const override;
    virtual QString getDisplayName() const override;
    virtual std::vector<DisplayMode> getSupportedModes() const override;
    virtual void createItems(const AudioLibrary& library,
        DisplayMode display_mode,
        AudioLibraryModel* model) const override;
    virtual QString getId() const override;

private:
    QString _filter;
};

class AudioLibraryViewArtist : public AudioLibraryView
{
public:
    AudioLibraryViewArtist(const QString& artist);

    virtual AudioLibraryView* clone() const override;
    virtual QString getDisplayName() const override;
    virtual std::vector<DisplayMode> getSupportedModes() const override;
    virtual void createItems(const AudioLibrary& library,
        DisplayMode display_mode,
        AudioLibraryModel* model) const override;
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
        DisplayMode display_mode,
        AudioLibraryModel* model) const override;
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
        DisplayMode display_mode,
        AudioLibraryModel* model) const override;
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
        DisplayMode display_mode,
        AudioLibraryModel* model) const override;
    virtual void resolveToTracks(const AudioLibrary& library, std::vector<const AudioLibraryTrack*>& tracks) const override;
    virtual QString getId() const override;

private:
    QString _genre;
};

class AudioLibraryViewDuplicateAlbums : public AudioLibraryView
{
public:
    virtual AudioLibraryView* clone() const override;
    virtual QString getDisplayName() const override;
    virtual std::vector<DisplayMode> getSupportedModes() const override;
    virtual void createItems(const AudioLibrary& library,
        DisplayMode display_mode,
        AudioLibraryModel* model) const override;
    virtual QString getId() const override;
};