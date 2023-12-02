// SPDX-License-Identifier: GPL-2.0-only
#include "gtest/gtest.h"

#include <QtGui/qstandarditemmodel.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qheaderview.h>

#include "tools.h"

bool test_VisualIndexRestorationImpStep(const std::vector<std::pair<int, int>>& logical_and_visual_indexes)
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
        EXPECT_EQ(header.visualIndex(logical_and_visual_index.first), logical_and_visual_index.second);
    }

    return true;
}

TEST(AudioExplorer, VisualIndexRestoration)
{
    int argc = 1;
    char* argv = const_cast<char*>("");
    QApplication app(argc, &argv);

    EXPECT_TRUE(test_VisualIndexRestorationImpStep({ {0, 0},{ 1, 1 },{ 2, 2 },{ 3, 3 } }));

    EXPECT_TRUE(test_VisualIndexRestorationImpStep({ { 0, 3 },{ 1, 2 },{ 2, 1 },{ 3, 0 } }));

    EXPECT_TRUE(test_VisualIndexRestorationImpStep({ { 0, 0 },{ 1, 3 },{ 2, 1 },{ 3, 2 } }));

    EXPECT_TRUE(test_VisualIndexRestorationImpStep({ { 0, 3 },{ 1, 0 },{ 2, 1 },{ 3, 2 } }));

    EXPECT_TRUE(test_VisualIndexRestorationImpStep({ { 0, 2 },{ 1, 0 },{ 2, 3 },{ 3, 1 } }));

    EXPECT_TRUE(test_VisualIndexRestorationImpStep({ { 3, 0 },{ 2, 1 },{ 1, 2 },{ 0, 3 } }));
}