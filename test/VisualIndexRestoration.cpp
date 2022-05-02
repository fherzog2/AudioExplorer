// SPDX-License-Identifier: GPL-2.0-only
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
        RETURN_IF_FAILED(header.visualIndex(logical_and_visual_index.first) == logical_and_visual_index.second);
    }

    return true;
}

bool test_VisualIndexRestorationImp()
{
    RETURN_IF_FAILED(test_VisualIndexRestorationImpStep({ {0, 0},{ 1, 1 },{ 2, 2 },{ 3, 3 } }));

    RETURN_IF_FAILED(test_VisualIndexRestorationImpStep({ { 0, 3 },{ 1, 2 },{ 2, 1 },{ 3, 0 } }));

    RETURN_IF_FAILED(test_VisualIndexRestorationImpStep({ { 0, 0 },{ 1, 3 },{ 2, 1 },{ 3, 2 } }));

    RETURN_IF_FAILED(test_VisualIndexRestorationImpStep({ { 0, 3 },{ 1, 0 },{ 2, 1 },{ 3, 2 } }));

    RETURN_IF_FAILED(test_VisualIndexRestorationImpStep({ { 0, 2 },{ 1, 0 },{ 2, 3 },{ 3, 1 } }));

    RETURN_IF_FAILED(test_VisualIndexRestorationImpStep({ { 3, 0 },{ 2, 1 },{ 1, 2 },{ 0, 3 } }));

    return true;
}

int test_VisualIndexRestoration(int argc, char** const argv)
{
    QApplication app(argc, argv);

    return test_VisualIndexRestorationImp() ? 0 : 1;
}