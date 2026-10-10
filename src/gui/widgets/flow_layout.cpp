/// @file flow_layout.cpp
/// @brief A layout that puts its items side by side and goes on in the next row when a row is
/// full

#include "kalahari/gui/widgets/flow_layout.h"

#include <QLayoutItem>
#include <QStyle>
#include <QtAlgorithms>
#include <QWidget>

namespace kalahari {
namespace gui {

namespace {

/// @brief An item in its row, with its width there
struct RowItem {
    QLayoutItem* item = nullptr;
    int width = 0;
};

/// @brief The height of an item in its row
int itemHeight(const QLayoutItem* item)
{
    return item->sizeHint().expandedTo(item->minimumSize()).boundedTo(item->maximumSize()).height();
}

} // anonymous namespace

FlowLayout::FlowLayout(QWidget* parent)
    : QLayout(parent)
{
}

FlowLayout::~FlowLayout()
{
    qDeleteAll(m_items);
}

void FlowLayout::addItem(QLayoutItem* item)
{
    m_items.append(item);
    invalidate();
}

int FlowLayout::count() const
{
    return static_cast<int>(m_items.size());
}

QLayoutItem* FlowLayout::itemAt(int index) const
{
    return index >= 0 && index < m_items.size() ? m_items.at(index) : nullptr;
}

QLayoutItem* FlowLayout::takeAt(int index)
{
    if (index < 0 || index >= m_items.size()) {
        return nullptr;
    }
    QLayoutItem* item = m_items.takeAt(index);
    invalidate();
    return item;
}

Qt::Orientations FlowLayout::expandingDirections() const
{
    return {};
}

bool FlowLayout::hasHeightForWidth() const
{
    return true;
}

int FlowLayout::heightForWidth(int width) const
{
    return arrange(QRect(0, 0, width, 0), false);
}

QSize FlowLayout::sizeHint() const
{
    QSize size(0, 0);
    int items = 0;
    for (const QLayoutItem* item : m_items) {
        if (item->isEmpty()) {
            continue;
        }
        size.rwidth() += item->sizeHint().expandedTo(item->minimumSize()).width();
        size.setHeight(qMax(size.height(), itemHeight(item)));
        ++items;
    }
    size.rwidth() += qMax(0, items - 1) * gap();
    const QMargins margins = contentsMargins();
    return size + QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
}

QSize FlowLayout::minimumSize() const
{
    QSize size(0, 0);
    for (const QLayoutItem* item : m_items) {
        if (!item->isEmpty()) {
            size = size.expandedTo(item->minimumSize());
        }
    }
    const QMargins margins = contentsMargins();
    return size + QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
}

void FlowLayout::setGeometry(const QRect& rect)
{
    QLayout::setGeometry(rect);
    arrange(rect, true);
}

int FlowLayout::arrange(const QRect& rect, bool place) const
{
    const QMargins margins = contentsMargins();
    const QRect area = rect.marginsRemoved(margins);
    const int space = gap();

    // The rows: each takes the items that fit in it at their preferred widths, and at least
    // one item, however narrow the layout is
    QList<QList<RowItem>> rows;
    int taken = 0;  // width the items of the last row take, with the spaces between them
    for (QLayoutItem* item : m_items) {
        if (item->isEmpty()) {
            continue;
        }
        const int minimum = item->minimumSize().width();
        const int width =
            qMax(minimum, qMin(item->sizeHint().expandedTo(item->minimumSize()).width(),
                               area.width()));
        if (rows.isEmpty() || taken + space + width > area.width()) {
            rows.append(QList<RowItem>());
            taken = width;
        } else {
            taken += space + width;
        }
        rows.last().append(RowItem{item, width});
    }

    const QWidget* widget = parentWidget();
    const Qt::LayoutDirection direction =
        widget != nullptr ? widget->layoutDirection() : Qt::LeftToRight;
    int y = area.top();
    for (const QList<RowItem>& row : rows) {
        int rowHeight = 0;
        int rowWidth = -space;
        int expanding = 0;
        for (const RowItem& placed : row) {
            rowHeight = qMax(rowHeight, itemHeight(placed.item));
            rowWidth += space + placed.width;
            if (placed.item->expandingDirections() & Qt::Horizontal) {
                ++expanding;
            }
        }
        if (place) {
            // The room left in the row goes to the items that expand, in equal shares, as far
            // as their maximum widths allow; the rest stays empty at the end of the row
            int left = qMax(0, area.width() - rowWidth);
            int x = area.left();
            for (const RowItem& placed : row) {
                int width = placed.width;
                if (expanding > 0 && (placed.item->expandingDirections() & Qt::Horizontal)) {
                    const int share = left / expanding;
                    width = qMax(width, qMin(width + share, placed.item->maximumSize().width()));
                    left -= width - placed.width;
                    --expanding;
                }
                const int height = itemHeight(placed.item);
                const QRect itemRect(x, y + (rowHeight - height) / 2, width, height);
                placed.item->setGeometry(QStyle::visualRect(direction, area, itemRect));
                x += width + space;
            }
        }
        y += rowHeight + space;
    }
    if (!rows.isEmpty()) {
        y -= space;
    }
    return y - area.top() + margins.top() + margins.bottom();
}

int FlowLayout::gap() const
{
    return qMax(0, spacing());
}

} // namespace gui
} // namespace kalahari
