/// @file segmented_layout.cpp
/// @brief Joined buttons of a choice: in a row, or one under another on a narrow panel

#include "kalahari/gui/widgets/segmented_layout.h"

#include <QLayoutItem>
#include <QRect>
#include <QStyle>
#include <QVariant>
#include <QWidget>

#include <algorithm>

namespace kalahari {
namespace gui {

namespace {

/// @brief The property that tells a widget how the buttons are arranged
constexpr const char* STACKED_PROPERTY = "stacked";

}  // namespace

SegmentedLayout::SegmentedLayout(QWidget* parent) : QLayout(parent) {
    setContentsMargins(0, 0, 0, 0);
    setSpacing(0);
}

SegmentedLayout::~SegmentedLayout() {
    qDeleteAll(m_items);
    m_items.clear();
}

void SegmentedLayout::addItem(QLayoutItem* item) {
    m_items.append(item);
    if (QWidget* widget = item->widget()) {
        widget->setProperty(STACKED_PROPERTY, m_stacked);
        // One under another, a button is as wide as the widest (a tool button would keep
        // its own width)
        QSizePolicy policy = widget->sizePolicy();
        policy.setHorizontalPolicy(QSizePolicy::Preferred);
        widget->setSizePolicy(policy);
    }
    invalidate();
}

int SegmentedLayout::count() const {
    return static_cast<int>(m_items.size());
}

QLayoutItem* SegmentedLayout::itemAt(int index) const {
    return index >= 0 && index < m_items.size() ? m_items.at(index) : nullptr;
}

QLayoutItem* SegmentedLayout::takeAt(int index) {
    if (index < 0 || index >= m_items.size()) {
        return nullptr;
    }
    invalidate();
    return m_items.takeAt(index);
}

Qt::Orientations SegmentedLayout::expandingDirections() const {
    return {};
}

bool SegmentedLayout::hasHeightForWidth() const {
    return true;
}

int SegmentedLayout::rowWidth() const {
    int width = 0;
    int shown = 0;
    for (const QLayoutItem* item : m_items) {
        if (!item->isEmpty()) {
            width += item->sizeHint().width() + (shown > 0 ? spacing() : 0);
            ++shown;
        }
    }
    return width;
}

int SegmentedLayout::columnWidth() const {
    int width = 0;
    for (const QLayoutItem* item : m_items) {
        if (!item->isEmpty()) {
            width = std::max(width, item->sizeHint().width());
        }
    }
    return width;
}

int SegmentedLayout::buttonHeight() const {
    int height = 0;
    for (const QLayoutItem* item : m_items) {
        if (!item->isEmpty()) {
            height = std::max(height, item->sizeHint().height());
        }
    }
    return height;
}

int SegmentedLayout::heightForWidth(int width) const {
    const QMargins margins = contentsMargins();
    const int room = width - margins.left() - margins.right();
    int shown = 0;
    for (const QLayoutItem* item : m_items) {
        if (!item->isEmpty()) {
            ++shown;
        }
    }
    const int lines = room >= rowWidth() ? std::min(shown, 1) : shown;
    return lines * buttonHeight() + std::max(lines - 1, 0) * spacing() + margins.top() +
           margins.bottom();
}

QSize SegmentedLayout::sizeHint() const {
    const QMargins margins = contentsMargins();
    return {rowWidth() + margins.left() + margins.right(),
            buttonHeight() + margins.top() + margins.bottom()};
}

QSize SegmentedLayout::minimumSize() const {
    // One button high: the height one under another comes from heightForWidth()
    const QMargins margins = contentsMargins();
    return {columnWidth() + margins.left() + margins.right(),
            buttonHeight() + margins.top() + margins.bottom()};
}

void SegmentedLayout::setGeometry(const QRect& rect) {
    QLayout::setGeometry(rect);
    const QRect area = rect.marginsRemoved(contentsMargins());
    const bool stacked = area.width() < rowWidth();
    const int height = buttonHeight();
    int x = area.left();
    int y = area.top();
    for (QLayoutItem* item : m_items) {
        if (item->isEmpty()) {
            continue;
        }
        if (stacked) {
            item->setGeometry(QRect(area.left(), y, columnWidth(), height));
            y += height + spacing();
        } else {
            const int width = item->sizeHint().width();
            item->setGeometry(QRect(x, area.top(), width, height));
            x += width + spacing();
        }
    }
    setStacked(stacked);
}

void SegmentedLayout::setStacked(bool stacked) {
    if (stacked == m_stacked) {
        return;
    }
    m_stacked = stacked;
    for (QLayoutItem* item : m_items) {
        if (QWidget* widget = item->widget()) {
            widget->setProperty(STACKED_PROPERTY, stacked);
            widget->style()->unpolish(widget);
            widget->style()->polish(widget);
            widget->update();
        }
    }
}

} // namespace gui
} // namespace kalahari
