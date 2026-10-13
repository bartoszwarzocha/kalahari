/// @file dashboard_layouts.cpp
/// @brief The layouts of the Dashboard, which fit it to a narrow or a low panel

#include "kalahari/gui/panels/dashboard_layouts.h"

#include <QLayoutItem>
#include <QRect>
#include <QWidget>

#include <algorithm>
#include <vector>

namespace kalahari {
namespace gui {

namespace {

/// @brief The height an item needs when it is @p width wide
int heightAt(const QLayoutItem* item, int width) {
    return item->hasHeightForWidth() ? item->heightForWidth(width) : item->sizeHint().height();
}

/// @brief Both margins of a layout together
QSize marginsOf(const QLayout& layout) {
    const QMargins margins = layout.contentsMargins();
    return {margins.left() + margins.right(), margins.top() + margins.bottom()};
}

/// @brief The item at @p index of a layout's items, or nullptr
QLayoutItem* itemOf(const QList<QLayoutItem*>& items, int index) {
    return index >= 0 && index < items.size() ? items.at(index) : nullptr;
}

/// @brief Take the item at @p index out of a layout's items
QLayoutItem* takeItem(QLayout& layout, QList<QLayoutItem*>& items, int index) {
    if (index < 0 || index >= items.size()) {
        return nullptr;
    }
    layout.invalidate();
    return items.takeAt(index);
}

/// @brief Delete the items a layout holds
void deleteItems(QList<QLayoutItem*>& items) {
    qDeleteAll(items);
    items.clear();
}

} // namespace

// ============================================================================
// DashboardContentLayout
// ============================================================================

DashboardContentLayout::DashboardContentLayout(QWidget* parent) : QLayout(parent) {
    setContentsMargins(0, 0, 0, 0);
}

DashboardContentLayout::~DashboardContentLayout() {
    deleteItems(m_items);
}

void DashboardContentLayout::addItem(QLayoutItem* item) {
    m_items.append(item);
    invalidate();
}

int DashboardContentLayout::count() const {
    return static_cast<int>(m_items.size());
}

QLayoutItem* DashboardContentLayout::itemAt(int index) const {
    return itemOf(m_items, index);
}

QLayoutItem* DashboardContentLayout::takeAt(int index) {
    return takeItem(*this, m_items, index);
}

Qt::Orientations DashboardContentLayout::expandingDirections() const {
    return Qt::Horizontal | Qt::Vertical;
}

bool DashboardContentLayout::hasHeightForWidth() const {
    return !m_items.isEmpty() && m_items.first()->hasHeightForWidth();
}

int DashboardContentLayout::heightForWidth(int width) const {
    if (m_items.isEmpty()) {
        return marginsOf(*this).height();
    }
    const int inner = width - marginsOf(*this).width();
    return heightAt(m_items.first(), contentWidth(inner)) + marginsOf(*this).height();
}

QSize DashboardContentLayout::sizeHint() const {
    if (m_items.isEmpty()) {
        return marginsOf(*this);
    }
    return m_items.first()->sizeHint() + QSize(2 * SIDE_MARGIN, 0) + marginsOf(*this);
}

QSize DashboardContentLayout::minimumSize() const {
    if (m_items.isEmpty()) {
        return marginsOf(*this);
    }
    return m_items.first()->minimumSize() + QSize(2 * SIDE_MARGIN, 0) + marginsOf(*this);
}

void DashboardContentLayout::setGeometry(const QRect& rect) {
    QLayout::setGeometry(rect);
    if (m_items.isEmpty()) {
        return;
    }
    const QRect area = rect.marginsRemoved(contentsMargins());
    const int width = contentWidth(area.width());
    m_items.first()->setGeometry(
        QRect(area.x() + (area.width() - width) / 2, area.y(), width, area.height()));
}

int DashboardContentLayout::contentWidth(int width) const {
    if (m_items.isEmpty()) {
        return 0;
    }
    const QLayoutItem* content = m_items.first();
    const int room = width - 2 * SIDE_MARGIN;
    const int share = static_cast<int>(width * CONTENT_SHARE);
    const int liked = std::max(share, content->sizeHint().width());
    return std::max(std::min(liked, room), content->minimumSize().width());
}

// ============================================================================
// DashboardHeaderLayout
// ============================================================================

DashboardHeaderLayout::DashboardHeaderLayout(QWidget* parent) : QLayout(parent) {
    setContentsMargins(0, 0, 0, 0);
}

DashboardHeaderLayout::~DashboardHeaderLayout() {
    deleteItems(m_items);
}

void DashboardHeaderLayout::setLargestLogo(int size) {
    size = std::clamp(size, LOGO_MIN_SIZE, LOGO_SIZE);
    if (size != m_largestLogo) {
        m_largestLogo = size;
        invalidate();
    }
}

int DashboardHeaderLayout::largestLogo() const {
    return m_largestLogo;
}

void DashboardHeaderLayout::addItem(QLayoutItem* item) {
    m_items.append(item);
    invalidate();
}

int DashboardHeaderLayout::count() const {
    return static_cast<int>(m_items.size());
}

QLayoutItem* DashboardHeaderLayout::itemAt(int index) const {
    return itemOf(m_items, index);
}

QLayoutItem* DashboardHeaderLayout::takeAt(int index) {
    return takeItem(*this, m_items, index);
}

Qt::Orientations DashboardHeaderLayout::expandingDirections() const {
    return {};
}

bool DashboardHeaderLayout::hasHeightForWidth() const {
    return true;
}

int DashboardHeaderLayout::heightForWidth(int width) const {
    return arrange(QRect(0, 0, width, 0), false);
}

QSize DashboardHeaderLayout::sizeHint() const {
    if (m_items.size() != 3) {
        return marginsOf(*this);
    }
    const int titleWidth = m_items.at(1)->sizeHint().width();
    const int taglineWidth = m_items.at(2)->sizeHint().width();
    const int textHeight = heightAt(m_items.at(1), titleWidth) + TITLE_SPACING +
                           heightAt(m_items.at(2), taglineWidth);
    return QSize(m_largestLogo + LOGO_SPACING + std::max(titleWidth, taglineWidth),
                 std::max(m_largestLogo, textHeight)) +
           marginsOf(*this);
}

QSize DashboardHeaderLayout::minimumSize() const {
    if (m_items.size() != 3) {
        return marginsOf(*this);
    }
    const int width =
        std::max(m_items.at(1)->minimumSize().width(), m_items.at(2)->minimumSize().width());
    return QSize(width, LOGO_MIN_SIZE) + marginsOf(*this);
}

void DashboardHeaderLayout::setGeometry(const QRect& rect) {
    QLayout::setGeometry(rect);
    arrange(rect, true);
}

int DashboardHeaderLayout::arrange(const QRect& rect, bool place) const {
    if (m_items.size() != 3) {
        return marginsOf(*this).height();
    }
    QLayoutItem* logo = m_items.at(0);
    QLayoutItem* title = m_items.at(1);
    QLayoutItem* tagline = m_items.at(2);
    const QRect area = rect.marginsRemoved(contentsMargins());
    const int width = area.width();
    const int titleWidth = title->sizeHint().width();
    const int taglineWidth = tagline->sizeHint().width();
    const int textWidth = std::max(titleWidth, taglineWidth);

    int height = 0;
    if (width >= LOGO_MIN_SIZE + LOGO_SPACING + textWidth) {
        // Beside the texts, each in its line: the logo takes the room they leave
        const int logoSize = std::min(m_largestLogo, width - LOGO_SPACING - textWidth);
        const int titleHeight = heightAt(title, titleWidth);
        const int taglineHeight = heightAt(tagline, taglineWidth);
        const int textHeight = titleHeight + TITLE_SPACING + taglineHeight;
        height = std::max(logoSize, textHeight);
        if (place) {
            const int left = area.x() + (width - logoSize - LOGO_SPACING - textWidth) / 2;
            logo->setGeometry(QRect(left, area.y() + (height - logoSize) / 2, logoSize, logoSize));
            const int textLeft = left + logoSize + LOGO_SPACING;
            const int textTop = area.y() + (height - textHeight) / 2;
            title->setGeometry(QRect(textLeft, textTop, titleWidth, titleHeight));
            tagline->setGeometry(QRect(textLeft, textTop + titleHeight + TITLE_SPACING,
                                       taglineWidth, taglineHeight));
        }
    } else {
        // Above the texts, centered, which wrap where their line does not fit
        const int logoSize = std::max(std::min(LOGO_MIN_SIZE, width), 0);
        if (place) {
            logo->setGeometry(
                QRect(area.x() + (width - logoSize) / 2, area.y(), logoSize, logoSize));
        }
        int top = area.y() + logoSize + LOGO_SPACING_ABOVE;
        const auto placeText = [&](QLayoutItem* text, int lineWidth) {
            const int itemWidth = std::max(std::min(width, lineWidth), text->minimumSize().width());
            const int itemHeight = heightAt(text, itemWidth);
            if (place) {
                text->setGeometry(
                    QRect(area.x() + (width - itemWidth) / 2, top, itemWidth, itemHeight));
            }
            top += itemHeight;
        };
        placeText(title, titleWidth);
        top += TITLE_SPACING;
        placeText(tagline, taglineWidth);
        height = top - area.y();
    }
    return height + marginsOf(*this).height();
}

// ============================================================================
// DashboardHintsLayout
// ============================================================================

namespace {

/// @brief A hint of a shortcut: its keys and its command, with their sizes in one line
struct Hint {
    QLayoutItem* keys;
    QLayoutItem* command;
    QSize keysSize;
    QSize commandSize;
};

/// @brief The hints of a layout's items, in pairs, without the hidden ones
std::vector<Hint> hintsOf(const QList<QLayoutItem*>& items) {
    std::vector<Hint> hints;
    for (int index = 0; index + 1 < items.size(); index += 2) {
        QLayoutItem* keys = items.at(index);
        QLayoutItem* command = items.at(index + 1);
        if (keys->isEmpty() && command->isEmpty()) {
            continue;
        }
        hints.push_back({keys, command, keys->sizeHint(), command->sizeHint()});
    }
    return hints;
}

/// @brief The width of all the hints in one row
int rowWidthOf(const std::vector<Hint>& hints) {
    if (hints.empty()) {
        return 0;
    }
    int width = DashboardHintsLayout::HINT_SPACING * (static_cast<int>(hints.size()) - 1);
    for (const Hint& hint : hints) {
        width += hint.keysSize.width() + DashboardHintsLayout::KEYS_SPACING +
                 hint.commandSize.width();
    }
    return width;
}

/// @brief The height of a row with the keys and the command of a hint
int rowHeightOf(const Hint& hint) {
    return std::max(hint.keysSize.height(), hint.commandSize.height());
}

} // namespace

DashboardHintsLayout::DashboardHintsLayout(QWidget* parent) : QLayout(parent) {
    setContentsMargins(0, 0, 0, 0);
}

DashboardHintsLayout::~DashboardHintsLayout() {
    deleteItems(m_items);
}

void DashboardHintsLayout::addHint(QWidget* keys, QWidget* command) {
    addWidget(keys);
    addWidget(command);
}

void DashboardHintsLayout::addItem(QLayoutItem* item) {
    m_items.append(item);
    invalidate();
}

int DashboardHintsLayout::count() const {
    return static_cast<int>(m_items.size());
}

QLayoutItem* DashboardHintsLayout::itemAt(int index) const {
    return itemOf(m_items, index);
}

QLayoutItem* DashboardHintsLayout::takeAt(int index) {
    return takeItem(*this, m_items, index);
}

Qt::Orientations DashboardHintsLayout::expandingDirections() const {
    return {};
}

bool DashboardHintsLayout::hasHeightForWidth() const {
    return true;
}

int DashboardHintsLayout::heightForWidth(int width) const {
    return arrange(QRect(0, 0, width, 0), false);
}

QSize DashboardHintsLayout::sizeHint() const {
    const std::vector<Hint> hints = hintsOf(m_items);
    int height = 0;
    for (const Hint& hint : hints) {
        height = std::max(height, rowHeightOf(hint));
    }
    return QSize(rowWidthOf(hints), height) + marginsOf(*this);
}

QSize DashboardHintsLayout::minimumSize() const {
    int width = 0;
    int height = 0;
    for (const Hint& hint : hintsOf(m_items)) {
        width = std::max({width, hint.keysSize.width(), hint.command->minimumSize().width()});
        height = std::max(height, rowHeightOf(hint));
    }
    return QSize(width, height) + marginsOf(*this);
}

void DashboardHintsLayout::setGeometry(const QRect& rect) {
    QLayout::setGeometry(rect);
    arrange(rect, true);
}

int DashboardHintsLayout::arrange(const QRect& rect, bool place) const {
    const std::vector<Hint> hints = hintsOf(m_items);
    const QRect area = rect.marginsRemoved(contentsMargins());
    const auto put = [place](QLayoutItem* item, int left, int top, QSize size) {
        if (place) {
            item->setGeometry(QRect(QPoint(left, top), size));
        }
    };
    int keysColumn = 0;
    int commandsColumn = 0;
    for (const Hint& hint : hints) {
        keysColumn = std::max(keysColumn, hint.keysSize.width());
        commandsColumn = std::max(commandsColumn, hint.commandSize.width());
    }
    const int columnsWidth = keysColumn + KEYS_SPACING + commandsColumn;

    int height = 0;
    if (rowWidthOf(hints) <= area.width()) {
        // All in one row, in the middle
        for (const Hint& hint : hints) {
            height = std::max(height, rowHeightOf(hint));
        }
        int left = area.x() + (area.width() - rowWidthOf(hints)) / 2;
        for (const Hint& hint : hints) {
            put(hint.keys, left, area.y() + (height - hint.keysSize.height()) / 2,
                hint.keysSize);
            left += hint.keysSize.width() + KEYS_SPACING;
            put(hint.command, left, area.y() + (height - hint.commandSize.height()) / 2,
                hint.commandSize);
            left += hint.commandSize.width() + HINT_SPACING;
        }
    } else if (columnsWidth <= area.width()) {
        // One under another, in the middle: the keys to the right of their column, the
        // commands to the left of theirs
        const int left = area.x() + (area.width() - columnsWidth) / 2;
        int top = area.y();
        for (const Hint& hint : hints) {
            const int rowHeight = rowHeightOf(hint);
            put(hint.keys, left + keysColumn - hint.keysSize.width(),
                top + (rowHeight - hint.keysSize.height()) / 2, hint.keysSize);
            put(hint.command, left + keysColumn + KEYS_SPACING,
                top + (rowHeight - hint.commandSize.height()) / 2, hint.commandSize);
            top += rowHeight + ROW_SPACING;
        }
        height = top - area.y() - ROW_SPACING;
    } else {
        // Each command under its keys, in the middle; a command longer than the line wraps
        int top = area.y();
        for (const Hint& hint : hints) {
            put(hint.keys, area.x() + (area.width() - hint.keysSize.width()) / 2, top,
                hint.keysSize);
            top += hint.keysSize.height();
            const int commandWidth = std::max(std::min(hint.commandSize.width(), area.width()),
                                              hint.command->minimumSize().width());
            const QSize commandSize(commandWidth, heightAt(hint.command, commandWidth));
            put(hint.command, area.x() + (area.width() - commandWidth) / 2, top, commandSize);
            top += commandSize.height() + STACK_SPACING;
        }
        height = top - area.y() - STACK_SPACING;
    }
    return height + marginsOf(*this).height();
}

// ============================================================================
// DashboardCardLayout
// ============================================================================

DashboardCardLayout::DashboardCardLayout(QWidget* parent) : QLayout(parent) {
    setContentsMargins(MARGIN, MARGIN, MARGIN, MARGIN);
}

DashboardCardLayout::~DashboardCardLayout() {
    deleteItems(m_items);
}

void DashboardCardLayout::addItem(QLayoutItem* item) {
    m_items.append(item);
    invalidate();
}

int DashboardCardLayout::count() const {
    return static_cast<int>(m_items.size());
}

QLayoutItem* DashboardCardLayout::itemAt(int index) const {
    return itemOf(m_items, index);
}

QLayoutItem* DashboardCardLayout::takeAt(int index) {
    return takeItem(*this, m_items, index);
}

Qt::Orientations DashboardCardLayout::expandingDirections() const {
    return Qt::Horizontal;
}

bool DashboardCardLayout::hasHeightForWidth() const {
    return true;
}

int DashboardCardLayout::heightForWidth(int width) const {
    return arrange(QRect(0, 0, width, 0), false);
}

QSize DashboardCardLayout::sizeHint() const {
    if (m_items.size() != 2) {
        return marginsOf(*this);
    }
    const QSize icon = m_items.at(0)->sizeHint();
    const int textWidth = m_items.at(1)->sizeHint().width();
    return QSize(icon.width() + SPACING + textWidth,
                 std::max(icon.height(), heightAt(m_items.at(1), textWidth))) +
           marginsOf(*this);
}

QSize DashboardCardLayout::minimumSize() const {
    if (m_items.size() != 2) {
        return marginsOf(*this);
    }
    const QSize icon = m_items.at(0)->minimumSize();
    const QSize text = m_items.at(1)->minimumSize();
    return QSize(std::max(icon.width(), text.width()), std::max(icon.height(), text.height())) +
           marginsOf(*this);
}

void DashboardCardLayout::setGeometry(const QRect& rect) {
    QLayout::setGeometry(rect);
    arrange(rect, true);
}

int DashboardCardLayout::arrange(const QRect& rect, bool place) const {
    if (m_items.size() != 2) {
        return marginsOf(*this).height();
    }
    QLayoutItem* icon = m_items.at(0);
    QLayoutItem* text = m_items.at(1);
    const QRect area = rect.marginsRemoved(contentsMargins());
    const QSize iconSize = icon->sizeHint();
    const int besideWidth = area.width() - iconSize.width() - SPACING;
    const int neededBeside = std::max(text->minimumSize().width(),
                                      std::min(text->sizeHint().width(), TEXT_ROOM_BESIDE));

    int height = 0;
    if (besideWidth >= neededBeside) {
        // Beside the texts, centered on them
        const int textHeight = heightAt(text, besideWidth);
        height = std::max(iconSize.height(), textHeight);
        if (place) {
            icon->setGeometry(
                QRect(QPoint(area.x(), area.y() + (height - iconSize.height()) / 2), iconSize));
            text->setGeometry(QRect(area.x() + iconSize.width() + SPACING,
                                    area.y() + (height - textHeight) / 2, besideWidth,
                                    textHeight));
        }
    } else {
        // Above the texts, which take the whole width
        const int textWidth = std::max(area.width(), text->minimumSize().width());
        const int textHeight = heightAt(text, textWidth);
        height = iconSize.height() + SPACING + textHeight;
        if (place) {
            icon->setGeometry(QRect(area.topLeft(), iconSize));
            text->setGeometry(
                QRect(area.x(), area.y() + iconSize.height() + SPACING, textWidth, textHeight));
        }
    }
    return height + marginsOf(*this).height();
}

} // namespace gui
} // namespace kalahari
