// SPDX-License-Identifier: GPL-2.0-only

#include "ImageViewWindow.h"

#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qscrollbar.h>

const double ZOOM_IN_STEP = 1.25;
const double ZOOM_OUT_STEP = 0.8;
const double MAX_ZOOM = 16.0;
const int DEFAULT_MAX_SIZE = 512;

ImageView::ImageView(QWidget* parent)
    : QFrame(parent)
{
}

void ImageView::setPixmap(const QPixmap& pixmap)
{
    _pixmap = pixmap;
    _snap_to_borders = true;
    _scale_factor = 1;
    _offset = QPointF(0, 0);
}

QSize ImageView::sizeHint() const
{
    QSize result(DEFAULT_MAX_SIZE, DEFAULT_MAX_SIZE);

    if (!_pixmap.isNull())
    {
        result.rwidth() = qMin(_pixmap.width(), DEFAULT_MAX_SIZE);
        result.rheight() = qMin(_pixmap.height(), DEFAULT_MAX_SIZE);
    }

    return result;
}

void ImageView::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);

    QRectF source_rect (0, 0, _pixmap.width(), _pixmap.height());

    QRectF target_rect(
        imagePointToViewportPoint(QPointF(0, 0)),
        imagePointToViewportPoint(QPointF(_pixmap.width(), _pixmap.height())));

    p.drawPixmap(target_rect, _pixmap, source_rect);
}

void ImageView::mouseMoveEvent(QMouseEvent* event)
{
    if (_mouse_press_pos_valid)
    {
        QPoint drag_distance = event->pos() - _mouse_press_pos;

        setOffset(_mouse_press_offset - drag_distance);
        update();
    }
}

void ImageView::mousePressEvent(QMouseEvent* event)
{
    _mouse_press_pos_valid = true;
    _mouse_press_pos = event->pos();
    _mouse_press_offset = _offset;
}

void ImageView::mouseReleaseEvent(QMouseEvent* /*event*/)
{
    _mouse_press_pos_valid = false;
}

void ImageView::wheelEvent(QWheelEvent* event)
{
    // remember image point under the mouse

    const QPoint viewport_mouse_pos = mapFromGlobal(event->globalPos());
    QPointF image_mouse_pos = viewportPointToImagePoint(viewport_mouse_pos);

    image_mouse_pos.rx() = qBound(0.0, image_mouse_pos.rx(), double(_pixmap.width()));
    image_mouse_pos.ry() = qBound(0.0, image_mouse_pos.ry(), double(_pixmap.height()));

    // adjust scale factor

    if (event->angleDelta().y() > 0)
    {
        if (_snap_to_borders)
        {
            // enter scaled mode
            // init _scale_factor and _offset

            double tx;
            double ty;
            getImageToViewportTransformation(_scale_factor, tx, ty);
            _offset = QPointF(-tx, -ty);
            _snap_to_borders = false;
        }

        if (_scale_factor * ZOOM_IN_STEP > MAX_ZOOM)
            return;
        
        _scale_factor *= ZOOM_IN_STEP;
    }
    else if (event->angleDelta().y() < 0 && !_snap_to_borders)
    {
        _scale_factor *= ZOOM_OUT_STEP;

        double min_scale_factor = getMinScaleFactor();
        if (_scale_factor < min_scale_factor)
        {
            // enter snap mode

            _scale_factor = min_scale_factor;
            _snap_to_borders = true;
        }
    }

    // move the scrollbars so the same image point is under the mouse again

    const QPointF viewport_pos2 = imagePointToViewportPoint(image_mouse_pos);

    setOffset(_offset + viewport_pos2 - viewport_mouse_pos);

    update();
}

void ImageView::setOffset(const QPointF& offset)
{
    const QSizeF image_size = QSizeF(_pixmap.size()) * _scale_factor;
    const QSizeF viewport_size = size();

    _offset = offset;
    _offset.rx() = qBound(0.0, _offset.x(), image_size.width() - viewport_size.width());
    _offset.ry() = qBound(0.0, _offset.y(), image_size.height() - viewport_size.height());
}

QPointF ImageView::imagePointToViewportPoint(const QPointF& image_point) const
{
    double scaling;
    double tx;
    double ty;
    getImageToViewportTransformation(scaling, tx, ty);

    return image_point * scaling + QPointF(tx, ty);
}

QPointF ImageView::viewportPointToImagePoint(const QPoint& viewport_point) const
{
    double scaling;
    double tx;
    double ty;
    getImageToViewportTransformation(scaling, tx, ty);

    return (viewport_point - QPointF(tx, ty)) / scaling;
}

void ImageView::getImageToViewportTransformation(double& scaling, double& tx, double& ty) const
{
    const QSizeF image_size = _pixmap.size();
    const QSizeF viewport_size = size();

    if (_snap_to_borders)
    {
        scaling = getMinScaleFactor();
    }
    else
    {
        scaling = _scale_factor;
    }

    const QSizeF scaled_image_size = _pixmap.size() * scaling;

    if (scaled_image_size.width() > viewport_size.width())
    {
        tx = -_offset.x();
    }
    else
    {
        tx = viewport_size.width() / 2.0 - scaled_image_size.width() / 2.0;
    }

    if (scaled_image_size.height() > viewport_size.height())
    {
        ty = -_offset.y();
    }
    else
    {
        ty = viewport_size.height() / 2.0 - scaled_image_size.height() / 2.0;
    }
}

double ImageView::getMinScaleFactor() const
{
    const QSizeF image_size = _pixmap.size();
    const QSizeF viewport_size = size();

    if (image_size.width() > viewport_size.width() ||
        image_size.height() > viewport_size.height())
    {
        // fit image into the viewport

        const double width_ratio = image_size.width() / viewport_size.width();
        const double height_ratio = image_size.height() / viewport_size.height();

        return 1.0 / qMax(width_ratio, height_ratio);
    }
    else
    {
        // original size

        return 1;
    }
}

//=============================================================================

ImageViewWindow::ImageViewWindow(Settings& settings)
    : _settings(settings)
{
    _scroll_area = new ImageView(this);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setMargin(0);
    layout->addWidget(_scroll_area);

    setAttribute(Qt::WA_DeleteOnClose);

    _settings.coverart_window_geometry.restore(this);
}

void ImageViewWindow::setPixmap(const QPixmap& pixmap)
{
    _scroll_area->setPixmap(pixmap);
}

void ImageViewWindow::closeEvent(QCloseEvent* event)
{
    _settings.coverart_window_geometry.save(this);

    QFrame::closeEvent(event);
}