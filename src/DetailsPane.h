// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <QtWidgets/qabstractitemview.h>
#include <QtGui/qstandarditemmodel.h>
#include <QtWidgets/qframe.h>
#include <QtWidgets/qgridlayout.h>
#include <QtWidgets/QLabel>
#include "AudioLibraryView.h"

class PictureBox;
class ElidedLabel;

class DetailsPane : public QFrame
{
public:
    DetailsPane(QWidget* parent);

    void setSelection(const QAbstractItemModel* model, const QModelIndex& current, AudioLibraryView::DisplayMode display_mode);

    QSize minimumSizeHint() const override;

protected:
    virtual void paintEvent(QPaintEvent* e) override;

private:
    QGridLayout* _data_grid = nullptr;
    std::vector<std::pair<QLabel*, ElidedLabel*>> _data_labels;

    PictureBox* _picture_box = nullptr;

    QString _empty_text;
};