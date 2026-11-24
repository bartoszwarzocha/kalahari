// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file list_set_tile.cpp
/// @brief A tile of a set of list formats

#include "kalahari/gui/widgets/list_set_tile.h"

#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionFocusRect>

#include <algorithm>
#include <cstdlib>
#include <limits>

namespace kalahari::gui {

namespace {

/// @brief The levels a tile shows
constexpr int TILE_LEVELS = 3;

/// @brief The text of a set's name: "%1 (default)" for the default set of its kind
QString defaultName(const QString& name)
{
    //: A set of list formats that F12 or Shift+F12 give. In Polish: %1 (domyślny)
    return QCoreApplication::translate("ListSets", "%1 (default)").arg(name);
}

}  // namespace

QFont listPictureFont(const QFont& base)
{
    QFont font = base;
    font.setFamilies({QStringLiteral("Times New Roman"), QStringLiteral("Liberation Serif")});
    font.setStyleHint(QFont::Serif);
    return font;
}

editor::ListPath firstListNumbers(const editor::ListDefinition& definition, int level, int number)
{
    editor::ListPath path{};
    for (int above = 0; above < editor::LIST_LEVELS; ++above) {
        path[static_cast<size_t>(above)] = definition.levels[static_cast<size_t>(above)].start;
    }
    path[static_cast<size_t>(level)] = number;
    return path;
}

ListSetTile::ListSetTile(const ListSet& set, bool isDefault, QWidget* parent)
    : QAbstractButton(parent)
    , m_set(set)
    , m_default(isDefault)
{
    setCheckable(true);
    setFocusPolicy(Qt::StrongFocus);
    const QString name = isDefault ? defaultName(set.displayName()) : set.displayName();
    setAccessibleName(name);
    setToolTip(name);
}

QSize ListSetTile::sizeHint() const
{
    const int line = fontMetrics().height();
    return {qRound(line * 7.5), qRound(line * 5.4)};
}

ListSetTile* ListSetTile::nearest(const QList<ListSetTile*>& tiles, const ListSetTile* from,
                                  int key, const QWidget* area)
{
    const QPoint centre = from->mapTo(area, from->rect().center());
    const int row = from->height() / 2;
    ListSetTile* best = nullptr;
    int bestDistance = std::numeric_limits<int>::max();
    for (ListSetTile* other : tiles) {
        const QPoint at = other->mapTo(area, other->rect().center()) - centre;
        const bool sameRow = std::abs(at.y()) < row;
        const bool ahead = (key == Qt::Key_Right && at.x() > 0 && sameRow) ||
                           (key == Qt::Key_Left && at.x() < 0 && sameRow) ||
                           (key == Qt::Key_Down && at.y() >= row) ||
                           (key == Qt::Key_Up && at.y() <= -row);
        const int distance = std::abs(at.x()) + 4 * std::abs(at.y());
        if (other != from && ahead && distance < bestDistance) {
            best = other;
            bestDistance = distance;
        }
    }
    return best;
}

void ListSetTile::keyPressEvent(QKeyEvent* event)
{
    const int key = event->key();
    if ((key == Qt::Key_Left || key == Qt::Key_Right || key == Qt::Key_Up ||
         key == Qt::Key_Down) && m_move) {
        m_move(this, key);
        return;
    }
    if (m_clickOnEnter && (key == Qt::Key_Return || key == Qt::Key_Enter)) {
        click();
        return;
    }
    QAbstractButton::keyPressEvent(event);
}

void ListSetTile::contextMenuEvent(QContextMenuEvent* event)
{
    if (m_menu) {
        const QPoint at = event->reason() == QContextMenuEvent::Keyboard
                              ? mapToGlobal(rect().center())
                              : event->globalPos();
        m_menu(m_set.id, at);
    }
}

void ListSetTile::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    const QPalette& colors = palette();
    painter.fillRect(rect(), colors.base());
    if (isChecked()) {
        painter.setPen(QPen(colors.highlight().color(), 2));
        painter.drawRect(QRectF(rect()).adjusted(1, 1, -1, -1));
    } else {
        painter.setPen(colors.mid().color());
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }
    if (hasFocus()) {
        QStyleOptionFocusRect focus;
        focus.initFrom(this);
        focus.rect = rect().adjusted(4, 4, -4, -4);
        style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, &painter, this);
    }

    // The first item of each level, each level further in, with a bar for its text
    const QFont font = listPictureFont(this->font());
    painter.setFont(font);
    const QFontMetrics metrics(font);
    const int margin = height() / 10;
    const int step = metrics.horizontalAdvance(QStringLiteral("MM"));
    const int lineHeight = (height() - 2 * margin - metrics.height()) / TILE_LEVELS;
    for (int level = 0; level < TILE_LEVELS; ++level) {
        const int centre = margin + lineHeight * level + lineHeight / 2;
        const QString mark = editor::listMarkerText(
            m_set.definition, level,
            firstListNumbers(m_set.definition, level,
                             m_set.definition.levels[static_cast<size_t>(level)].start));
        const int left = margin + step * level;
        painter.setPen(colors.text().color());
        painter.drawText(QPoint(left, centre + metrics.ascent() / 2 - 1), mark);
        const int barLeft = left + metrics.horizontalAdvance(mark) + margin / 2;
        painter.fillRect(QRect(barLeft, centre - 1, std::max(0, width() - barLeft - margin), 2),
                         colors.mid());
    }

    // Its name, under the items: the writer's, or "default"
    QString caption = m_set.name.trimmed();
    if (m_default) {
        caption = caption.isEmpty()
                      //: Under the tile of the default set of its kind. In Polish: domyślny
                      ? QCoreApplication::translate("ListSets", "default")
                      : defaultName(caption);
    }
    painter.setFont(this->font());
    painter.setPen(colors.placeholderText().color());
    const QRect captionRect(margin, height() - margin - fontMetrics().height(),
                            width() - 2 * margin, fontMetrics().height());
    painter.drawText(captionRect, Qt::AlignLeft | Qt::AlignVCenter,
                     fontMetrics().elidedText(caption, Qt::ElideRight, captionRect.width()));
}

}  // namespace kalahari::gui
