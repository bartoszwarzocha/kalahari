/// @file tool_button_columns.h
/// @brief Tool buttons stacked in a column that wraps into more columns when low

#pragma once

#include <QList>
#include <QWidget>

class QAction;
class QToolButton;

namespace kalahari {
namespace gui {

/// @brief Icon buttons for the side of a panel, in place of a vertical QToolBar
///
/// The buttons stand one under another. When the panel is too low for all of them
/// (a small screen, a high scaling), they wrap into a second column instead of hiding
/// behind QToolBar's extension arrow, so every button stays visible. The buttons take
/// keyboard focus (Tab) like the panel's other controls.
class ToolButtonColumns : public QWidget {
    Q_OBJECT

public:
    /// @brief Create an empty column
    /// @param iconSize Edge of the buttons' icons in pixels
    /// @param parent Parent widget
    explicit ToolButtonColumns(int iconSize, QWidget* parent = nullptr);

    /// @brief Add a button for an action (its icon, tooltip and state follow the action)
    QToolButton* addAction(QAction* action);

    /// @brief The buttons, in the order they were added
    const QList<QToolButton*>& buttons() const { return m_buttons; }

    /// @brief One column with every button
    QSize sizeHint() const override;

    /// @brief As many columns as the current height needs, as low as one button
    QSize minimumSizeHint() const override;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    /// @brief Size of the largest button
    QSize cellSize() const;

    /// @brief Columns needed to show every button at a height
    int columnsFor(int height) const;

    /// @brief Place the buttons for the current height
    void arrange();

    int m_iconSize;
    int m_columns = 1;
    QList<QToolButton*> m_buttons;
};

} // namespace gui
} // namespace kalahari
