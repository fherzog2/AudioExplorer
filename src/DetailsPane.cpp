// SPDX-License-Identifier: GPL-2.0-only
#include "DetailsPane.h"

#include <QtWidgets/qlabel.h>
#include <QtWidgets/qscrollarea.h>
#include <QtGui/qevent.h>
#include <QtGui/qpainter.h>

/**
* Shows an icon in a square area. The icon is expected to contain a pixmap.
* The height of this widget can never be larger than the icon itself.
* The widget can also shrink if needed.
*/
class PictureBox : public QFrame
{
public:
    PictureBox(QWidget* parent);

    void setIcon(const QIcon& icon);
    const QIcon& icon() const;

    virtual QSize minimumSizeHint() const override;
    virtual QSize sizeHint() const override;
    virtual bool hasHeightForWidth() const override;
    virtual int heightForWidth(int width) const override;

protected:
    virtual void paintEvent(QPaintEvent* e) override;

private:
    QIcon _icon;
};

PictureBox::PictureBox(QWidget* parent)
    : QFrame(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
}

void PictureBox::setIcon(const QIcon& icon)
{
    _icon = icon;

    // update layout and repaint

    updateGeometry();
    update();
}

const QIcon& PictureBox::icon() const
{
    return _icon;
}

QSize PictureBox::minimumSizeHint() const
{
    QList<QSize> sizes = _icon.availableSizes();
    if (!sizes.empty())
        return QSize(1, 1);

    return QSize(0, 0);
}

QSize PictureBox::sizeHint() const
{
    QList<QSize> sizes = _icon.availableSizes();
    if (!sizes.empty())
        return sizes.front();

    return QSize(0, 0);
}

bool PictureBox::hasHeightForWidth() const
{
    return true;
}

int PictureBox::heightForWidth(int width) const
{
    int size_steps[] = { 256, 128, 64, 32 };

    for (int size_step : size_steps)
        if (size_step < width)
            return size_step;

    return width;
}

void PictureBox::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    _icon.paint(&p, rect());
}

/**
* A label which can shrink and only shows an elided portion of the text.
*/
class ElidedLabel : public QFrame
{
public:
    ElidedLabel(const QString& text, QWidget* parent);

    void setText(const QString& text);
    const QString& text() const;

    virtual QSize minimumSizeHint() const override;

protected:
    virtual bool event(QEvent* e) override;
    virtual void paintEvent(QPaintEvent* e) override;

private:
    QString _text;
};

ElidedLabel::ElidedLabel(const QString& text, QWidget* parent)
    : QFrame(parent)
    , _text(text)
{
    setToolTip(text);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void ElidedLabel::setText(const QString& text)
{
    _text = text;
}

const QString& ElidedLabel::text() const
{
    return _text;
}

QSize ElidedLabel::minimumSizeHint() const
{
    int width_em = fontMetrics().width('M');

    QSize sh = QFrame::minimumSizeHint();
    sh.setWidth(3 * width_em);

    return sh;
}

bool ElidedLabel::event(QEvent* e)
{
    if (e->type() == QEvent::ToolTip)
    {
        // only show the tooltip if the text is elided

        QString elided = fontMetrics().elidedText(_text, Qt::ElideRight, width());
        if (elided == _text)
        {
            return true;
        }
    }

    return QFrame::event(e);
}

void ElidedLabel::paintEvent(QPaintEvent* e)
{
    QFrame::paintEvent(e);

    QString elided = fontMetrics().elidedText(_text, Qt::ElideRight, width());

    QPainter p(this);
    p.drawText(rect(), Qt::AlignLeft, elided);
}

//=============================================================================

DetailsPane::DetailsPane(QWidget* parent)
    : QFrame(parent)
{
    _picture_box = new PictureBox(this);

    QWidget* data_grid_widget = new QWidget();

    QScrollArea* data_grid_scroll_area = new QScrollArea(this);
    data_grid_scroll_area->setWidget(data_grid_widget);
    data_grid_scroll_area->setFrameShape(QFrame::NoFrame);
    data_grid_scroll_area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    data_grid_scroll_area->setWidgetResizable(true);

    _data_grid = new QGridLayout();
    _data_grid->setColumnStretch(1, 1);

    QVBoxLayout* data_grid_padding_layout = new QVBoxLayout(data_grid_widget);
    data_grid_padding_layout->addLayout(_data_grid);
    data_grid_padding_layout->addStretch(1);

    QVBoxLayout* vbox = new QVBoxLayout(this);
    vbox->setMargin(0);
    vbox->addWidget(_picture_box);
    vbox->addWidget(data_grid_scroll_area, 1);

    setFrameShape(QFrame::StyledPanel);
}

void DetailsPane::setSelection(const QAbstractItemModel* model, const QAbstractItemView* view, AudioLibraryView::DisplayMode display_mode)
{
    for (QWidget* w : _data_labels)
        w->deleteLater();
    _data_labels.clear();

    QIcon picture;

    QModelIndex current = view->currentIndex();
    if (current.isValid())
    {
        int view_row = current.row();

        picture = qvariant_cast<QIcon>(model->data(model->index(view_row, AudioLibraryView::ZERO), Qt::DecorationRole));

        // create detail rows for each view column

        int next_row = 0;

        auto columns = AudioLibraryView::getColumnsForDisplayMode(display_mode);

        if(AudioLibraryView::isGroupDisplayMode(display_mode))
            columns.insert(columns.begin(), AudioLibraryView::ZERO);

        for (AudioLibraryView::Column view_column : columns)
        {
            int logical_index = static_cast<int>(view_column);

            QLabel* description = new QLabel(model->headerData(logical_index, Qt::Horizontal, Qt::DisplayRole).toString(), this);
            QWidget* value = new ElidedLabel(model->data(model->index(view_row, logical_index), Qt::DisplayRole).toString(), this);

            _data_grid->addWidget(description, next_row, 0);
            _data_grid->addWidget(value, next_row, 1);

            ++next_row;

            _data_labels.push_back(description);
            _data_labels.push_back(value);
        }
    }

    _picture_box->setIcon(picture);

    if (!current.isValid())
        _empty_text = "Nothing selected";
    else
        _empty_text.clear();

    // update the "Nothing selected" text
    update();
}

void DetailsPane::paintEvent(QPaintEvent* e)
{
    QFrame::paintEvent(e);

    if (_data_labels.empty() && _picture_box->icon().isNull())
    {
        QPainter p(this);
        p.drawText(rect(), Qt::AlignCenter, _empty_text);
    }
}