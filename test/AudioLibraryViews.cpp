// SPDX-License-Identifier: GPL-2.0-only
#include <QtGui/qstandarditemmodel.h>
#include <QtWidgets/qapplication.h>

#include <AudioLibrary.h>
#include <AudioLibraryModel.h>
#include "tools.h"

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
        library.addTrack(QString("Artist 1 - %1").arg(i), QDateTime(), 0, createTrackInfo("Artist 1", QString(), "Album", 2000, "Genre 1", QByteArray(), QString::number(i), i));

    for (int i = 1; i <= 10; ++i)
        library.addTrack(QString("Artist 2 - %1").arg(i), QDateTime(), 0, createTrackInfo("Artist 2", "Artist 2", "Album", 2001, "Genre 2", QByteArray(), QString::number(i), i));

    library.addTrack("Sampler - 1", QDateTime(), 0, createTrackInfo("Artist 1", "Various Artists", "Sampler", 2001, "Genre 3", QByteArray(), "1", 1));
    library.addTrack("Sampler - 2", QDateTime(), 0, createTrackInfo("Artist 3", "Various Artists", "Sampler", 2001, "Genre 3", QByteArray(), "2", 2));

    AudioLibraryGroupUuidCache group_uuids;
    AudioLibraryModel model(nullptr, group_uuids);

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

int test_AudioLibraryViews(int argc, char** const argv)
{
    QApplication app(argc, argv);

    QString source_test_data_dir = app.arguments()[1];
    QString binary_dir = app.arguments()[2];

    return testAudioLibraryViewAllArtists() ? 0 : 1;
}