// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <QtWidgets/qabstractscrollarea.h>
#include <QtWidgets/qframe.h>
#include "Settings.h"

class ImageView : public QFrame
{
public:
    ImageView(QWidget* parent);

    void setPixmap(const QPixmap& pixmap);
    virtual QSize sizeHint() const override;

protected:
    virtual void paintEvent(QPaintEvent* event) override;
    virtual void resizeEvent(QResizeEvent* event) override;
    virtual void mouseMoveEvent(QMouseEvent* event) override;
    virtual void mousePressEvent(QMouseEvent* event) override;
    virtual void mouseReleaseEvent(QMouseEvent* event) override;
    virtual void wheelEvent(QWheelEvent* e) override;

private:
    void setOffset(const QPointF& offset);
    QPointF imagePointToViewportPoint(const QPointF& image_point) const;
    QPointF viewportPointToImagePoint(const QPoint& viewport_point) const;
    void tryEnableSnapToBordersMode();
    void getImageToViewportTransformation(double& scaling, double& tx, double& ty) const;

    QPixmap _pixmap;
    bool _snap_to_borders = true;
    double _scale_factor = 1;
    QPointF _offset;

    bool _mouse_press_pos_valid = false;
    QPoint _mouse_press_pos;
    QPointF _mouse_press_offset;
};

class ImageViewWindow : public QFrame
{
public:
    ImageViewWindow(Settings& settings);

    void setPixmap(const QPixmap& pixmap);

protected:
    virtual void closeEvent(QCloseEvent* event) override;

private:
    ImageView* _scroll_area = nullptr;
    Settings& _settings;
};