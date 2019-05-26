// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <QtWidgets/qabstractitemview.h>
#include <QtGui/qstandarditemmodel.h>
#include <QtWidgets/qframe.h>
#include <QtWidgets/qgridlayout.h>
#include "AudioLibraryView.h"

class PictureBox;

class DetailsPane : public QFrame
{
public:
    DetailsPane(QWidget* parent);

    void setSelection(const QStandardItemModel* model, const QAbstractItemView* view, AudioLibraryView::DisplayMode display_mode);

protected:
    virtual void paintEvent(QPaintEvent* e) override;

private:
    QGridLayout* _data_grid = nullptr;
    std::vector<QWidget*> _data_labels;

    PictureBox* _picture_box = nullptr;

    QString _empty_text;
};