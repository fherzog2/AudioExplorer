// SPDX-License-Identifier: GPL-2.0-only
#include "resource_helpers.h"

#include <QtGui/qiconengine.h>
#include <QtGui/qpainter.h>
#include <QtSvg/qsvgrenderer.h>

class SVGIconEngine : public QIconEngine
{
public:
    SVGIconEngine(const QByteArray& icon_buffer);

    virtual void paint(QPainter* painter, const QRect& rect, QIcon::Mode mode, QIcon::State state) override;
    virtual QIconEngine* clone() const override;
    virtual QPixmap pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state) override;

private:
    QByteArray _data;
};

SVGIconEngine::SVGIconEngine(const QByteArray& icon_buffer)
    : _data(icon_buffer)
{
}

void SVGIconEngine::paint(QPainter* painter, const QRect& rect, QIcon::Mode /*mode*/, QIcon::State /*state*/)
{
    QSvgRenderer renderer(_data);
    renderer.render(painter, rect);
}

QIconEngine* SVGIconEngine::clone() const
{
    return new SVGIconEngine(*this);
}

QPixmap SVGIconEngine::pixmap(const QSize& size, QIcon::Mode mode, QIcon::State state)
{
    // draw the SVG on a transparent background

    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);
    {
        QPainter painter(&pixmap);
        QRect r(QPoint(0, 0), size);
        paint(&painter, r, mode, state);
    }
    return pixmap;
}

//=============================================================================

QIcon iconFromResource(const res::data& resource)
{
    return QIcon(new SVGIconEngine(
        QByteArray(resource.ptr, static_cast<int>(resource.size) /* ok to convert to int, icons are small */)));
}