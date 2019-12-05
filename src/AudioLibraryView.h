// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <QtGui/qstandarditemmodel.h>
#include "AudioLibrary.h"

class AudioLibraryModel;

class AudioLibraryView
{
public:
    virtual ~AudioLibraryView() = default;

    enum Column
    {
        ZERO = 0,
        NUMBER_OF_ALBUMS,
        ARTIST,
        ALBUM,
        YEAR,
        GENRE,
        COVER_CHECKSUM,
        COVER_TYPE,
        COVER_WIDTH,
        COVER_HEIGHT,
        COVER_DATASIZE,
        NUMBER_OF_TRACKS,
        TITLE,
        TRACK_NUMBER,
        DISC_NUMBER,
        ALBUM_ARTIST,
        COMMENT,
        PATH,
        DATE_MODIFIED,
        TAG_TYPES,
        LENGTH_SECONDS,
        CHANNELS,
        BITRATE_KBS,
        SAMPLERATE_HZ,

        NUMBER_OF_COLUMNS, // helper value to create fixed-size arrays
    };

    enum class DisplayMode
    {
        ARTISTS,
        ALBUMS,
        TRACKS,
        YEARS,
        GENRES,
    };

    static QString getColumnFriendlyName(Column column, DisplayMode mode);
    static std::vector<std::pair<Column, QString>> columnToStringMapping();
    static QString getColumnId(Column column);
    static std::unique_ptr<Column> getColumnFromId(const QString& column_id);

    static QString getDisplayModeFriendlyName(DisplayMode mode);
    static std::vector<std::pair<DisplayMode, QString>> displayModeToStringMapping();
    static std::vector<Column> getColumnsForDisplayMode(DisplayMode mode);
    static bool isGroupDisplayMode(DisplayMode mode);

    static const int SORT_ROLE = Qt::UserRole + 1;
    static const int MULTILINE_DISPLAY_ROLE = Qt::UserRole + 2;
    static const int ID_ROLE = Qt::UserRole + 3;

    virtual std::unique_ptr<AudioLibraryView> clone() const = 0;
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

    virtual std::unique_ptr<AudioLibraryView> clone() const override;
    virtual QString getDisplayName() const override;
    virtual std::vector<DisplayMode> getSupportedModes() const override;
    virtual void createItems(const AudioLibrary& library,
        DisplayMode display_mode,
        AudioLibraryModel* model) const override;
    virtual QString getId() const override;

    static QString getBaseId();

private:
    QString _filter;
};

class AudioLibraryViewAllAlbums : public AudioLibraryView
{
public:
    AudioLibraryViewAllAlbums(QString filter);

    virtual std::unique_ptr<AudioLibraryView> clone() const override;
    virtual QString getDisplayName() const override;
    virtual std::vector<DisplayMode> getSupportedModes() const override;
    virtual void createItems(const AudioLibrary& library,
        DisplayMode display_mode,
        AudioLibraryModel* model) const override;
    virtual QString getId() const override;

    static QString getBaseId();

private:
    QString _filter;
};

class AudioLibraryViewAllTracks : public AudioLibraryView
{
public:
    AudioLibraryViewAllTracks(QString filter);

    virtual std::unique_ptr<AudioLibraryView> clone() const override;
    virtual QString getDisplayName() const override;
    virtual std::vector<DisplayMode> getSupportedModes() const override;
    virtual void createItems(const AudioLibrary& library,
        DisplayMode display_mode,
        AudioLibraryModel* model) const override;
    virtual QString getId() const override;

    static QString getBaseId();

private:
    QString _filter;
};

class AudioLibraryViewAllYears : public AudioLibraryView
{
public:
    virtual std::unique_ptr<AudioLibraryView> clone() const override;
    virtual QString getDisplayName() const override;
    virtual std::vector<DisplayMode> getSupportedModes() const override;
    virtual void createItems(const AudioLibrary& library,
        DisplayMode display_mode,
        AudioLibraryModel* model) const override;
    virtual QString getId() const override;

    static QString getBaseId();
};

class AudioLibraryViewAllGenres : public AudioLibraryView
{
public:
    AudioLibraryViewAllGenres(const QString& filter);

    virtual std::unique_ptr<AudioLibraryView> clone() const override;
    virtual QString getDisplayName() const override;
    virtual std::vector<DisplayMode> getSupportedModes() const override;
    virtual void createItems(const AudioLibrary& library,
        DisplayMode display_mode,
        AudioLibraryModel* model) const override;
    virtual QString getId() const override;

    static QString getBaseId();

private:
    QString _filter;
};

class AudioLibraryViewArtist : public AudioLibraryView
{
public:
    AudioLibraryViewArtist(const QString& artist);

    virtual std::unique_ptr<AudioLibraryView> clone() const override;
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

    virtual std::unique_ptr<AudioLibraryView> clone() const override;
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

    virtual std::unique_ptr<AudioLibraryView> clone() const override;
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

    virtual std::unique_ptr<AudioLibraryView> clone() const override;
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
    virtual std::unique_ptr<AudioLibraryView> clone() const override;
    virtual QString getDisplayName() const override;
    virtual std::vector<DisplayMode> getSupportedModes() const override;
    virtual void createItems(const AudioLibrary& library,
        DisplayMode display_mode,
        AudioLibraryModel* model) const override;
    virtual QString getId() const override;
};