/// @file flow_layout.cpp
/// @brief A row of items that goes on in the next line where the panel is too narrow

#include "kalahari/gui/widgets/flow_layout.h"

#include <QLayoutItem>
#include <QRect>
#include <QWidget>

#include <algorithm>
#include <vector>

namespace kalahari {
namespace gui {

FlowLayout::FlowLayout(QWidget* parent, int spacing, int lineSpacing)
    : QLayout(parent), m_lineSpacing(lineSpacing) {
    setContentsMargins(0, 0, 0, 0);
    setSpacing(spacing);
}

FlowLayout::~FlowLayout() {
    qDeleteAll(m_items);
    m_items.clear();
}

void FlowLayout::addItem(QLayoutItem* item) {
    m_items.append(item);
    invalidate();
}

void FlowLayout::addLayout(QLayout* layout) {
    addChildLayout(layout);
    addItem(layout);
}

int FlowLayout::count() const {
    return static_cast<int>(m_items.size());
}

QLayoutItem* FlowLayout::itemAt(int index) const {
    return index >= 0 && index < m_items.size() ? m_items.at(index) : nullptr;
}

QLayoutItem* FlowLayout::takeAt(int index) {
    if (index < 0 || index >= m_items.size()) {
        return nullptr;
    }
    invalidate();
    return m_items.takeAt(index);
}

Qt::Orientations FlowLayout::expandingDirections() const {
    return {};
}

bool FlowLayout::hasHeightForWidth() const {
    return true;
}

int FlowLayout::heightForWidth(int width) const {
    return arrange(QRect(0, 0, width, 0), false);
}

QSize FlowLayout::sizeHint() const {
    int width = 0;
    int height = 0;
    int shown = 0;
    for (const QLayoutItem* item : m_items) {
        if (item->isEmpty()) {
            continue;
        }
        const QSize hint = item->sizeHint();
        width += hint.width() + (shown > 0 ? spacing() : 0);
        height = std::max(height, hint.height());
        ++shown;
    }
    const QMargins margins = contentsMargins();
    return {width + margins.left() + margins.right(), height + margins.top() + margins.bottom()};
}

QSize FlowLayout::minimumSize() const {
    QSize least;
    for (const QLayoutItem* item : m_items) {
        if (!item->isEmpty()) {
            least = least.expandedTo(item->minimumSize());
        }
    }
    // The height of one line: the lines a width needs come from heightForWidth()
    const QMargins margins = contentsMargins();
    return least + QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
}

void FlowLayout::setGeometry(const QRect& rect) {
    QLayout::setGeometry(rect);
    arrange(rect, true);
}

int FlowLayout::arrange(const QRect& rect, bool place) const {
    const QRect area = rect.marginsRemoved(contentsMargins());
    const int lineWidth = std::max(area.width(), 0);

    // The items in lines: each as wide as it likes, or as the line where that is less
    struct Placed {
        QLayoutItem* item;
        int width;
    };
    std::vector<std::vector<Placed>> lines(1);
    int used = 0;
    for (QLayoutItem* item : m_items) {
        if (item->isEmpty()) {
            continue;
        }
        const int width = std::max(std::min(item->sizeHint().width(), lineWidth),
                                   item->minimumSize().width());
        std::vector<Placed>& line = lines.back();
        if (!line.empty() && used + spacing() + width > lineWidth) {
            lines.emplace_back();
            used = 0;
        }
        used += (lines.back().empty() ? 0 : spacing()) + width;
        lines.back().push_back({item, width});
    }

    int y = area.top();
    bool first = true;
    for (std::vector<Placed>& line : lines) {
        if (line.empty()) {
            continue;
        }
        // The room the line leaves goes to the items that expand sideways
        int lineUsed = 0;
        int expanding = 0;
        for (const Placed& placed : line) {
            lineUsed += placed.width;
            if (placed.item->expandingDirections() & Qt::Horizontal) {
                ++expanding;
            }
        }
        lineUsed += spacing() * static_cast<int>(line.size() - 1);
        int room = std::max(lineWidth - lineUsed, 0);
        for (Placed& placed : line) {
            if (expanding > 0 && (placed.item->expandingDirections() & Qt::Horizontal)) {
                const int share = room / expanding;
                placed.width += share;
                room -= share;
                --expanding;
            }
        }

        int height = 0;
        for (const Placed& placed : line) {
            const int itemHeight = placed.item->hasHeightForWidth()
                                       ? placed.item->heightForWidth(placed.width)
                                       : placed.item->sizeHint().height();
            height = std::max(height, itemHeight);
        }
        if (!first) {
            y += m_lineSpacing;
        }
        first = false;
        if (place) {
            int x = area.left();
            for (const Placed& placed : line) {
                placed.item->setGeometry(QRect(x, y, placed.width, height));
                x += placed.width + spacing();
            }
        }
        y += height;
    }
    const QMargins margins = contentsMargins();
    return y - area.top() + margins.top() + margins.bottom();
}

} // namespace gui
} // namespace kalahari
