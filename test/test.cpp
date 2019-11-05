// SPDX-License-Identifier: GPL-2.0-only
#include <AudioLibrary.h>
#include <ThreadSafeAudioLibrary.h>
#include <AudioLibraryModel.h>

#include <QtCore/qbuffer.h>
#include <QtGui/qstandarditemmodel.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qheaderview.h>

// print an error if the expression fails
#define RETURN_IF_FAILED(expression)\
    if(!(expression))\
    {\
        fprintf(stderr, "%s (%d): error: %s\n", __FILE__, __LINE__, #expression);\
        return false;\
    }

// print the expression, whether it passes or fails
// on error, also invalidates a variable called "ok"
#define RESET_OK_IF_FAILED(expression)\
    if(!(expression))\
    {\
        fprintf(stderr, "%s (%d): error: %s\n", __FILE__, __LINE__, #expression);\
        ok = false;\
    }\
    else\
    {\
        fprintf(stderr, "%s (%d): ok: %s\n", __FILE__, __LINE__, #expression);\
    }

TrackInfo createTrackInfo(QString artist,
    QString album_artist,
    QString album,
    int year,
    QString genre,
    QByteArray cover,
    QString title,
    int track_number)
{
    TrackInfo info;

    info.artist = artist;
    info.album_artist = album_artist;
    info.album = album;
    info.year = year;
    info.genre = genre;
    info.cover = cover;
    info.title = title;
    info.track_number = track_number;

    return info;
}

bool compareLibraries(const AudioLibrary& a, const AudioLibrary& b)
{
    const auto albums_a = a.getAlbums();
    const auto albums_b = b.getAlbums();

    if (albums_a.size() != albums_b.size())
        return false;

    for (size_t i = 0, endi = albums_a.size(); i < endi; ++i)
    {
        const auto album_a = albums_a[i];
        const auto album_b = albums_b[i];

        if (album_a->getKey() != album_b->getKey())
            return false;

        if (album_a->getCover() != album_b->getCover())
            return false;

        if (album_a->getTracks().size() != album_b->getTracks().size())
            return false;

        auto tracks_a = album_a->getTracks();
        auto tracks_b = album_b->getTracks();

        auto compare_tracks = [](const AudioLibraryTrack* a, const AudioLibraryTrack* b) {
            return a->_filepath < b->_filepath;
        };

        // the library itself does not sort the tracks

        std::sort(tracks_a.begin(), tracks_a.end(), compare_tracks);
        std::sort(tracks_b.begin(), tracks_b.end(), compare_tracks);

        for (size_t j = 0, endj = album_a->getTracks().size(); j < endj; ++j)
        {
            const auto track_a = tracks_a[j];
            const auto track_b = tracks_b[j];

            if (*track_a != *track_b)
                return false;
        }
    }

    return true;
}

bool testLibrarySaveAndLoad()
{
    AudioLibrary lib;

    lib.addTrack("a", QDateTime(), createTrackInfo("artist 1", QString(), "album 1", 2000, "genre 1", QByteArray(), "title 1", 1));
    lib.addTrack("b", QDateTime(), createTrackInfo("artist 1", QString(), "album 1", 2000, "genre 1", QByteArray(), "title 2", 2));
    lib.addTrack("c", QDateTime(), createTrackInfo("artist 1", QString(), "album 1", 2000, "genre 1", QByteArray(), "title 3", 3));

    lib.addTrack("d", QDateTime(), createTrackInfo("artist 2", QString(), "album 2", 2000, "genre 1", QByteArray(), "title 1", 1));

    QByteArray bytes;

    {
        QBuffer buffer(&bytes);
        RETURN_IF_FAILED(buffer.open(QBuffer::WriteOnly));
        QDataStream s(&buffer);

        lib.save(s);
    }

    {
        AudioLibrary lib2;

        // load twice
        // first with an empty library
        // then with one that already contains data to throw away

        for(int i = 0; i < 2; ++i)
        {
            QBuffer buffer(&bytes);
            RETURN_IF_FAILED(buffer.open(QBuffer::ReadOnly));
            QDataStream s(&buffer);
            lib2.load(s);
            RETURN_IF_FAILED(compareLibraries(lib, lib2));
        }
    }

    return true;
}

bool testTrackInfoHeader(const QString& audio_filepath, const QString& cover_filepath)
{
    // test cover loading
    // other tags are loaded by taglib internally and don't need extra testing here

    TrackInfo info;
    RETURN_IF_FAILED(readTrackInfo(audio_filepath, info));

    QFile cover_file(cover_filepath);
    RETURN_IF_FAILED(cover_file.open(QIODevice::ReadOnly));

    const QByteArray original_cover = cover_file.readAll();
    RETURN_IF_FAILED(!original_cover.isEmpty());
    RETURN_IF_FAILED(info.cover == original_cover);

    RETURN_IF_FAILED(info.album_artist == "albumartist");
    RETURN_IF_FAILED(info.disc_number == 1);

    RETURN_IF_FAILED(info.length_milliseconds / 1000 == 1);

    return true;
}

bool createEmptyFile(const QString& filepath)
{
    QFile file(filepath);
    return file.open(QIODevice::WriteOnly);
}

bool testLibraryTrackCleanup(const QString binary_dir)
{
    RETURN_IF_FAILED(!binary_dir.isEmpty());

    QString dirpath1 = binary_dir + "/testLibraryTrackCleanup1";
    QString dirpath2 = binary_dir + "/testLibraryTrackCleanup2";

    QDir dir1(dirpath1);
    RETURN_IF_FAILED(dir1.removeRecursively());
    RETURN_IF_FAILED(QDir().mkpath(dirpath1));

    QDir dir2(dirpath2);
    RETURN_IF_FAILED(dir2.removeRecursively());
    RETURN_IF_FAILED(QDir().mkpath(dirpath2));

    QString filepath1 = dirpath1 + "/file1.txt";
    QString filepath2 = dirpath2 + "/file2.txt";
    QString filepath3 = dirpath2 + "/file3.txt";

    RETURN_IF_FAILED(createEmptyFile(filepath1));
    RETURN_IF_FAILED(createEmptyFile(filepath2));
    RETURN_IF_FAILED(createEmptyFile(filepath3));

    AudioLibrary library;
    library.addTrack(filepath1, QDateTime(), TrackInfo());
    library.addTrack(filepath2, QDateTime(), TrackInfo());
    library.addTrack(filepath3, QDateTime(), TrackInfo());

    QString new_filepath3 = dirpath2 + "/new file3.txt";

    RETURN_IF_FAILED(QDir().rename(filepath3, new_filepath3));

    library.addTrack(new_filepath3, QDateTime(), TrackInfo());

    std::unordered_set<QString> loaded_audio_files;
    loaded_audio_files.insert(filepath2);
    loaded_audio_files.insert(new_filepath3);

    library.removeTracksExcept(loaded_audio_files);

    // filepath1 is not inside valid_dirs, so the library must not contain it anymore
    // filepath3 does not exist anymore, so the library must not contain it anymore

    AudioLibrary library2;
    library2.addTrack(filepath2, QDateTime(), TrackInfo());
    library2.addTrack(new_filepath3, QDateTime(), TrackInfo());

    RETURN_IF_FAILED(compareLibraries(library, library2));

    return true;
}

bool testVisualIndexRestorationStep(const std::vector<std::pair<int, int>>& logical_and_visual_indexes)
{
    QStandardItemModel model;

    QHeaderView header(Qt::Horizontal);
    header.setModel(&model);

    // create the sections

    QStringList header_labels;

    for (const auto& logical_and_visual_index : logical_and_visual_indexes)
        header_labels.push_back(QString("%1").arg(logical_and_visual_index.first));

    model.setHorizontalHeaderLabels(header_labels);

    // apply visual indexes

    for (const auto& logical_and_visual_index : logical_and_visual_indexes)
    {
        header.moveSection(header.visualIndex(logical_and_visual_index.first), logical_and_visual_index.second);
    }

    // check

    for (const auto& logical_and_visual_index : logical_and_visual_indexes)
    {
        RETURN_IF_FAILED(header.visualIndex(logical_and_visual_index.first) == logical_and_visual_index.second);
    }

    return true;
}

bool testVisualIndexRestoration()
{
    RETURN_IF_FAILED(testVisualIndexRestorationStep({ {0, 0},{ 1, 1 },{ 2, 2 },{ 3, 3 } }));

    RETURN_IF_FAILED(testVisualIndexRestorationStep({ { 0, 3 },{ 1, 2 },{ 2, 1 },{ 3, 0 } }));

    RETURN_IF_FAILED(testVisualIndexRestorationStep({ { 0, 0 },{ 1, 3 },{ 2, 1 },{ 3, 2 } }));

    RETURN_IF_FAILED(testVisualIndexRestorationStep({ { 0, 3 },{ 1, 0 },{ 2, 1 },{ 3, 2 } }));

    RETURN_IF_FAILED(testVisualIndexRestorationStep({ { 0, 2 },{ 1, 0 },{ 2, 3 },{ 3, 1 } }));

    RETURN_IF_FAILED(testVisualIndexRestorationStep({ { 3, 0 },{ 2, 1 },{ 1, 2 },{ 0, 3 } }));

    return true;
}

bool testThreadSafeAudioLibrary(const QString& audio_files_dir)
{
    ThreadSafeAudioLibrary library;
    AudioFilesLoader audio_files_loader(library);

    audio_files_loader.startLoading(QString(), { audio_files_dir });

    // wait until the thread is finished

    while (audio_files_loader.isLoading())
        ;

    // check result

    ThreadSafeAudioLibrary::LibraryAccessor acc(library);

    return acc.getLibrary().getAlbums().size() == 1;
}

void printModel(const QAbstractItemModel& model)
{
    for (int row = 0; row < model.rowCount(); ++row)
    {
        for (int col = 0; col < model.columnCount(); ++col)
        {
            QString text = model.data(model.index(row, col), Qt::DisplayRole).toString();
            fprintf(stderr, "%s\t", qPrintable(text));
        }

        fprintf(stderr, "\n");
    }

    fprintf(stderr, "\n");
}

void addModelRow(QStandardItemModel& model, const std::vector<std::pair<AudioLibraryView::Column, QString>>& data)
{
    int row = model.rowCount();

    for (const auto& i : data)
        model.setItem(row, i.first, new QStandardItem(i.second));
}

#define CHECK_MODEL(expression, model1, model2) \
    if (!(expression)) \
    { \
        printModel(*(model1).getModel()); \
        printModel((model2)); \
        RETURN_IF_FAILED((expression)); \
    }

bool compareModels(const AudioLibraryModel& model1, const QStandardItemModel& model2)
{
    CHECK_MODEL(model1.getModel()->rowCount() == model2.rowCount(), model1, model2);

    for (int row = 0; row < model1.getModel()->rowCount(); ++row)
    {
        for (int col = 0; col < model1.getModel()->columnCount(); ++col)
        {
            QString text1 = model1.getModel()->data(model1.getModel()->index(row, col), Qt::DisplayRole).toString();
            QString text2 = model2.data(model2.index(row, col), Qt::DisplayRole).toString();

            CHECK_MODEL(text1 == text2, model1, model2);
        }
    }

    return true;
}

bool testAudioLibraryViewAllArtists()
{
    // build library

    AudioLibrary library;

    for (int i = 1; i <= 10; ++i)
        library.addTrack(QString("Artist 1 - %1").arg(i), QDateTime(), createTrackInfo("Artist 1", QString(), "Album", 2000, "Genre 1", QByteArray(), QString::number(i), i));

    for (int i = 1; i <= 10; ++i)
        library.addTrack(QString("Artist 2 - %1").arg(i), QDateTime(), createTrackInfo("Artist 2", "Artist 2", "Album", 2001, "Genre 2", QByteArray(), QString::number(i), i));

    library.addTrack("Sampler - 1", QDateTime(), createTrackInfo("Artist 1", "Various Artists", "Sampler", 2001, "Genre 3", QByteArray(), "1", 1));
    library.addTrack("Sampler - 2", QDateTime(), createTrackInfo("Artist 3", "Various Artists", "Sampler", 2001, "Genre 3", QByteArray(), "2", 2));

    AudioLibraryModel model(nullptr);

    // unfiltered view

    AudioLibraryViewAllArtists all_artists_view("");

    all_artists_view.createItems(library, AudioLibraryView::DisplayMode::ARTISTS, &model);
    model.getModel()->sort(AudioLibraryView::ZERO);

    QStandardItemModel expected_artists(nullptr);

    addModelRow(expected_artists, { {AudioLibraryView::ZERO, "Artist 1"}, {AudioLibraryView::NUMBER_OF_ALBUMS, "2"}, {AudioLibraryView::NUMBER_OF_TRACKS, "11"} });
    addModelRow(expected_artists, { {AudioLibraryView::ZERO, "Artist 2"}, {AudioLibraryView::NUMBER_OF_ALBUMS, "1"}, {AudioLibraryView::NUMBER_OF_TRACKS, "10"} });
    addModelRow(expected_artists, { {AudioLibraryView::ZERO, "Artist 3"}, {AudioLibraryView::NUMBER_OF_ALBUMS, "1"}, {AudioLibraryView::NUMBER_OF_TRACKS, "1"} });
    addModelRow(expected_artists, { {AudioLibraryView::ZERO, "Various Artists"}, {AudioLibraryView::NUMBER_OF_ALBUMS, "1"}, {AudioLibraryView::NUMBER_OF_TRACKS, "2"} });

    RETURN_IF_FAILED(compareModels(model, expected_artists));

    // filtered view

    all_artists_view = AudioLibraryViewAllArtists("1");

    {
        AudioLibraryModel::IncrementalUpdateScope update_scope(model);
        all_artists_view.createItems(library, AudioLibraryView::DisplayMode::ARTISTS, &model);
    }

    expected_artists.clear();
    addModelRow(expected_artists, { {AudioLibraryView::ZERO, "Artist 1"}, {AudioLibraryView::NUMBER_OF_ALBUMS, "2"}, {AudioLibraryView::NUMBER_OF_TRACKS, "11"} });

    RETURN_IF_FAILED(compareModels(model, expected_artists));

    return true;
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    QString source_test_data_dir = app.arguments()[1];
    QString binary_dir = app.arguments()[2];

    bool ok = true;

    RESET_OK_IF_FAILED(testLibrarySaveAndLoad());

    QString original_cover_filepath = source_test_data_dir + "/gradient.jpg";
    RESET_OK_IF_FAILED(testTrackInfoHeader(source_test_data_dir + "/noise.mp3", original_cover_filepath));
    RESET_OK_IF_FAILED(testTrackInfoHeader(source_test_data_dir + "/noise.ogg", original_cover_filepath));
    RESET_OK_IF_FAILED(testTrackInfoHeader(source_test_data_dir + "/noise.m4a", original_cover_filepath));
    RESET_OK_IF_FAILED(testTrackInfoHeader(source_test_data_dir + "/noise.wma", original_cover_filepath));
    RESET_OK_IF_FAILED(testTrackInfoHeader(source_test_data_dir + "/noise.ape", original_cover_filepath));

    RESET_OK_IF_FAILED(testLibraryTrackCleanup(binary_dir));

    RESET_OK_IF_FAILED(testVisualIndexRestoration());

    RESET_OK_IF_FAILED(testThreadSafeAudioLibrary(source_test_data_dir));

    RESET_OK_IF_FAILED(testAudioLibraryViewAllArtists());

    return ok ? 0 : 1;
}