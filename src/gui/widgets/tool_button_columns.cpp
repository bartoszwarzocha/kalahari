/// @file tool_button_columns.cpp
/// @brief Implementation of ToolButtonColumns

#include "kalahari/gui/widgets/tool_button_columns.h"

#include <QResizeEvent>
#include <QStyle>
#include <QToolButton>

#include <algorithm>

namespace kalahari {
namespace gui {

ToolButtonColumns::ToolButtonColumns(int iconSize, QWidget* parent)
    : QWidget(parent)
    , m_iconSize(iconSize)
{
    // The width follows the columns; the height is whatever the panel has
    setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Ignored);
}

QToolButton* ToolButtonColumns::addAction(QAction* action)
{
    auto* button = new QToolButton(this);
    button->setDefaultAction(action);
    button->setAutoRaise(true);
    button->setIconSize(QSize(m_iconSize, m_iconSize));
    button->setToolButtonStyle(Qt::ToolButtonIconOnly);
    button->setFocusPolicy(Qt::TabFocus);
    m_buttons.append(button);
    arrange();
    updateGeometry();
    return button;
}

QSize ToolButtonColumns::cellSize() const
{
    QSize cell(0, 0);
    for (const QToolButton* button : m_buttons) {
        cell = cell.expandedTo(button->sizeHint());
    }
    return cell;
}

int ToolButtonColumns::columnsFor(int height) const
{
    const int cellHeight = cellSize().height();
    if (m_buttons.isEmpty() || cellHeight <= 0) {
        return 1;
    }
    const int rows = std::max(1, height / cellHeight);
    return static_cast<int>((m_buttons.size() + rows - 1) / rows);
}

QSize ToolButtonColumns::sizeHint() const
{
    const QSize cell = cellSize();
    return {cell.width() * m_columns, cell.height() * static_cast<int>(m_buttons.size())};
}

QSize ToolButtonColumns::minimumSizeHint() const
{
    const QSize cell = cellSize();
    return {cell.width() * m_columns, cell.height()};
}

void ToolButtonColumns::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    arrange();
}

void ToolButtonColumns::arrange()
{
    const QSize cell = cellSize();
    const int columns = columnsFor(height());
    const int rows = cell.height() > 0 ? std::max(1, height() / cell.height()) : 1;
    for (int i = 0; i < m_buttons.size(); ++i) {
        const int column = static_cast<int>(i) / rows;
        const int row = static_cast<int>(i) % rows;
        m_buttons[i]->setGeometry(QRect(QPoint(column * cell.width(), row * cell.height()), cell));
    }
    if (columns != m_columns) {
        // A new width for the panel's layout; the height stays, so this settles at once
        m_columns = columns;
        updateGeometry();
    }
}

} // namespace gui
} // namespace kalahari
