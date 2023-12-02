// SPDX-License-Identifier: GPL-2.0-only
#include "gtest/gtest.h"

#include <QtGui/qstandarditemmodel.h>
#include <QtWidgets/qapplication.h>

#include <AudioLibrary.h>
#include <AudioLibraryModel.h>
#include "tools.h"

bool testAudioLibraryView(const AudioLibrary& library, const AudioLibraryView& view, AudioLibraryView::DisplayMode display_mode, QStringList& result_list)
{
    // create model

    AudioLibraryGroupUuidCache group_uuids;
    AudioLibraryModel model(nullptr, group_uuids);

    view.createItems(library, display_mode, &model);
    model.getModel()->sort(AudioLibraryView::ZERO);

    // serialize model

    QStringList row_str_list;

    const QAbstractItemModel* m = model.getModel();
    for (int row = 0; row < m->rowCount(); ++row)
    {
        QStringList row_str;

        for (int col = 0; col < m->columnCount(); ++col)
        {
            const QVariant v = m->data(m->index(row, col), Qt::DisplayRole);
            if (v.isValid())
                row_str << v.toString();
        }

        row_str_list << row_str.join(',');
    }

    const auto display_mode_mapping = AudioLibraryView::displayModeToStringMapping();
    const auto display_mode_str = std::find_if(display_mode_mapping.begin(), display_mode_mapping.end(), [display_mode](const auto& i) { return i.first == display_mode; });

    result_list << "===============================================================================";
    result_list << "    " + view.getId() + "|" + display_mode_str->second;
    result_list << "===============================================================================";
    result_list.append(row_str_list);

    return true;
}

bool checkAgainstReferenceDataFile(const QString& reference_data_filepath, const QString& data)
{
    const bool create_reference_data = !qEnvironmentVariable("AUDIO_LIBRARY_CREATE_REFERENCE_DATA").isEmpty();

    QFile reference_data_file(reference_data_filepath);
    if (create_reference_data)
    {
        EXPECT_TRUE(reference_data_file.open(QFile::WriteOnly));
        reference_data_file.write(data.toUtf8());
    }
    else
    {
        EXPECT_TRUE(reference_data_file.open(QFile::ReadOnly));
        const QByteArray reference_data_bytes = reference_data_file.readAll();
        const QString reference_data = QString::fromUtf8(reference_data_bytes);

        EXPECT_EQ(data, reference_data);
    }

    return true;
}

class AlbumCreator
{
public:
    AlbumCreator(AudioLibrary& library, QString artist, QString album_artist, QString album, int year, QString genre);

    void addTrack(QString title, int min, int sec);

private:
    AudioLibrary& _library;
    QString _artist;
    QString _album_artist;
    QString _album;
    int _year;
    QString _genre;

    int _track_number;
};

AlbumCreator::AlbumCreator(AudioLibrary& library, QString artist, QString album_artist, QString album, int year, QString genre)
    : _library(library)
    , _artist(artist)
    , _album_artist(album_artist)
    , _album(album)
    , _year(year)
    , _genre(genre)
    , _track_number(1)
{}

void AlbumCreator::addTrack(QString title, int min, int sec)
{
    const int length_milliseconds = (min * 60 + sec) * 1000;

    _library.addTrack(QString("%1 %2 %3").arg(_artist).arg(_year).arg(title), QDateTime(), 0,
        createTrackInfo(_artist, _album_artist, _album, _year, _genre, QByteArray(), title, _track_number, length_milliseconds));

    ++_track_number;
}

TEST(AudioExplorer, AudioLibraryViews)
{
    int argc = 1;
    char* argv = const_cast<char*>("");
    QApplication app(argc, &argv);

    // pin the locale, because the reference data depends on it

    QLocale l(QLocale::German, QLocale::Germany);
    QLocale::setDefault(l);

    // build library

    AudioLibrary library;

    {
        AlbumCreator creator(library, "Blind Guardian", "", "Somewhere Far Beyond", 1992, "Power Metal");

        creator.addTrack("Time What Is Time", 5, 46);
        creator.addTrack("Journey Through the Dark", 4, 48);
        creator.addTrack("Black Chamber", 0, 58);
        creator.addTrack("Theatre of Pain", 4, 17);
        creator.addTrack("The Quest for Tanelorn", 5, 57);
        creator.addTrack("Ashes to Ashes", 6, 00);
        creator.addTrack("The Bard's Song - In the Forest", 3, 10);
        creator.addTrack("The Bard's Song - The Hobbit", 3, 54);
        creator.addTrack("The Piper's Calling", 0, 59);
        creator.addTrack("Somewhere Far Beyond", 7, 30);
        creator.addTrack("Spread Your Wings (Queen cover)", 4, 15);
        creator.addTrack("Trial by Fire (Satan cover)", 3, 45);
        creator.addTrack("Theatre of Pain (Classic Version)", 4, 15);
    }

    {
        AlbumCreator creator(library, "Blind Guardian", "", "Imaginations from the Other Side", 1995, "Power Metal");

        creator.addTrack("Imaginations from the Other Side", 7, 19);
        creator.addTrack("I'm Alive", 5, 31);
        creator.addTrack("A Past and Future Secret", 3, 48);
        creator.addTrack("The Script for My Requiem", 6, 9);
        creator.addTrack("Mordred's Song", 5, 28);
        creator.addTrack("Born in a Mourning Hall", 5, 14);
        creator.addTrack("Bright Eyes", 5, 16);
        creator.addTrack("Another Holy War", 4, 32);
        creator.addTrack("And the Story Ends", 6, 0);
    }

    {
        AlbumCreator creator(library, "Kreator", "", "Pleasure to Kill", 1986, "Thrash Metal");

        creator.addTrack("Choir of the Damned", 0, 46);
        creator.addTrack("Ripping Corpse", 3, 36);
        creator.addTrack("Death Is Your Saviour", 3, 58);
        creator.addTrack("Pleasure to Kill", 4, 11);
        creator.addTrack("Riot of Violence", 4, 56);
        creator.addTrack("The Pestilence", 6, 58);
        creator.addTrack("Carrion", 4, 48);
        creator.addTrack("Command of the Blade", 3, 57);
        creator.addTrack("Under the Guillotine", 4, 38);
    }

    QStringList result_list;

    ASSERT_TRUE(testAudioLibraryView(library, AudioLibraryViewAllArtists(QString()), AudioLibraryView::DisplayMode::ARTISTS, result_list));
    ASSERT_TRUE(testAudioLibraryView(library, AudioLibraryViewAllArtists("Blind"), AudioLibraryView::DisplayMode::ARTISTS, result_list));
    ASSERT_TRUE(testAudioLibraryView(library, AudioLibraryViewAllAlbums(QString()), AudioLibraryView::DisplayMode::ALBUMS, result_list));
    ASSERT_TRUE(testAudioLibraryView(library, AudioLibraryViewAllAlbums("Other"), AudioLibraryView::DisplayMode::ALBUMS, result_list));
    ASSERT_TRUE(testAudioLibraryView(library, AudioLibraryViewAllTracks(QString()), AudioLibraryView::DisplayMode::TRACKS, result_list));
    ASSERT_TRUE(testAudioLibraryView(library, AudioLibraryViewAllTracks("the"), AudioLibraryView::DisplayMode::TRACKS, result_list));
    ASSERT_TRUE(testAudioLibraryView(library, AudioLibraryViewAllYears(), AudioLibraryView::DisplayMode::YEARS, result_list));
    ASSERT_TRUE(testAudioLibraryView(library, AudioLibraryViewAllGenres(QString()), AudioLibraryView::DisplayMode::GENRES, result_list));
    ASSERT_TRUE(testAudioLibraryView(library, AudioLibraryViewAllGenres("Power"), AudioLibraryView::DisplayMode::GENRES, result_list));
    ASSERT_TRUE(testAudioLibraryView(library, AudioLibraryViewAllGenres("Power"), AudioLibraryView::DisplayMode::ARTISTS, result_list));
    ASSERT_TRUE(testAudioLibraryView(library, AudioLibraryViewAllGenres("Power"), AudioLibraryView::DisplayMode::ALBUMS, result_list));

    ASSERT_TRUE(testAudioLibraryView(library, AudioLibraryViewArtist("Blind Guardian"), AudioLibraryView::DisplayMode::ALBUMS, result_list));
    ASSERT_TRUE(testAudioLibraryView(library, AudioLibraryViewArtist("Blind Guardian"), AudioLibraryView::DisplayMode::TRACKS, result_list));
    AudioLibraryAlbumKey key("Blind Guardian", "Imaginations from the Other Side", "Power Metal", 1995, 0);
    ASSERT_TRUE(testAudioLibraryView(library, AudioLibraryViewAlbum(key), AudioLibraryView::DisplayMode::TRACKS, result_list));
    ASSERT_TRUE(testAudioLibraryView(library, AudioLibraryViewYear(1995), AudioLibraryView::DisplayMode::ALBUMS, result_list));
    ASSERT_TRUE(testAudioLibraryView(library, AudioLibraryViewYear(1995), AudioLibraryView::DisplayMode::TRACKS, result_list));
    ASSERT_TRUE(testAudioLibraryView(library, AudioLibraryViewGenre("Power Metal"), AudioLibraryView::DisplayMode::ALBUMS, result_list));
    ASSERT_TRUE(testAudioLibraryView(library, AudioLibraryViewGenre("Power Metal"), AudioLibraryView::DisplayMode::TRACKS, result_list));

    ASSERT_TRUE(testAudioLibraryView(library, AudioLibraryViewDuplicateAlbums(), AudioLibraryView::DisplayMode::ALBUMS, result_list));

    ASSERT_TRUE(checkAgainstReferenceDataFile("test_data/AudioLibraryViews.txt", result_list.join('\n')));
}