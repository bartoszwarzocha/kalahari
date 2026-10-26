/// @file color_menu.h
/// @brief The submenus Format > Font Color and Highlight Color: the automatic color or none,
///        the palette of the color window, its recent colors and More Colors...

#pragma once

#include <QColor>
#include <QList>
#include <QMenu>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <functional>
#include <optional>
#include <utility>

class QAction;
class QFocusEvent;
class QKeyEvent;
class QLabel;
class QMouseEvent;
class QPaintEvent;
class QWidgetAction;

namespace kalahari {
namespace gui {

/// @brief Swatches of colors in rows, for a menu: the arrows move among them, Enter, Space
///        or a click chooses one
///
/// An arrow up from the first row or down from the last goes on to the menu, which goes to
/// the item above or below; Esc closes the menu. Each swatch tells its name and HEX in its
/// tooltip, also while the arrows move to it.
class ColorSwatches : public QWidget {
    Q_OBJECT

public:
    /// @brief Side of a swatch, and of its cell with the room around it
    static constexpr int SWATCH_SIZE = 16;
    static constexpr int CELL_SIZE = 20;

    /// @param columns The swatches in a row
    /// @param parent Parent widget
    explicit ColorSwatches(int columns, QWidget* parent = nullptr);

    /// @brief Show colors, with their names, row by row
    void setColors(const QList<QColor>& colors, const QStringList& names);

    /// @brief How many swatches there are
    [[nodiscard]] int count() const { return static_cast<int>(m_colors.size()); }

    /// @brief The color of a swatch; invalid outside the swatches
    [[nodiscard]] QColor colorAt(int index) const;

    /// @brief What a swatch tells: "Red (#CC0000)"
    [[nodiscard]] QString textAt(int index) const;

    /// @brief The swatch the arrows are at; -1 for none
    [[nodiscard]] int currentIndex() const { return m_current; }
    void setCurrentIndex(int index);

    /// @brief The swatch of a color: framed, as the color of the text; an invalid color: none
    void setMarkedColor(const QColor& color);

    /// @brief The swatch with a color; -1 when there is none
    [[nodiscard]] int indexOf(const QColor& color) const;

    /// @brief The swatches in a row
    [[nodiscard]] int columns() const { return m_columns; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    /// @brief The writer chose a swatch
    void colorChosen(const QColor& color);

    /// @brief The swatches got the keys (from the menu, with an arrow)
    void entered();

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    /// @brief The rows the swatches take
    [[nodiscard]] int rows() const;

    /// @brief Where a swatch is drawn
    [[nodiscard]] QRect swatchRect(int index) const;

    /// @brief The swatch at a point; -1 for none
    [[nodiscard]] int indexAt(const QPoint& point) const;

    /// @brief Tell the current swatch in its tooltip and to the screen readers
    void tellCurrent();

    int m_columns;
    QList<QColor> m_colors;
    QStringList m_names;
    QColor m_marked;
    int m_current = -1;
};

/// @brief A submenu of colors: of the text (Format > Font Color) or of its highlight
///        (Format > Highlight Color)
///
/// Automatic (No Color for the highlight) takes the color off, a swatch of the palette the
/// color window showed last or of its recent colors gives it, More Colors... opens the color
/// window. The swatch of the color of the text at the cursor is framed. The command, a toolbar
/// button, gives the color chosen last: at first red, or yellow for the highlight, as in Word.
class ColorMenu : public QMenu {
    Q_OBJECT

public:
    /// @brief What the color is for
    enum class Kind {
        Text,      ///< The color of the letters
        Highlight  ///< The color behind them
    };

    /// @brief The color the text at the cursor has: invalid for the automatic one, none or
    ///        one not the same in all of the selection
    using CurrentColor = std::function<QColor()>;

    /// @param kind What the color is for
    /// @param currentColor The color of the text at the cursor; none: no swatch is framed
    /// @param parent Parent widget
    explicit ColorMenu(Kind kind, CurrentColor currentColor = {}, QWidget* parent = nullptr);

    /// @brief What the color is for
    [[nodiscard]] Kind kind() const { return m_kind; }

    /// @brief The color the command gives: the one chosen last; invalid for Automatic or No
    ///        Color
    [[nodiscard]] QColor lastColor() const { return m_lastColor; }

    /// @brief Choose a color, as a swatch does: the menu and the menus it was opened from
    ///        close
    /// @param color The color; invalid for Automatic or No Color
    void choose(const QColor& color);

    /// @brief Show the palette and the recent colors as they are now (before the menu shows)
    void refresh();

    /// @brief The items (for tests)
    [[nodiscard]] QAction* noColorAction() const { return m_noColorAction; }
    [[nodiscard]] QAction* moreColorsAction() const { return m_moreColorsAction; }
    [[nodiscard]] QWidgetAction* recentAction() const { return m_recentAction; }
    [[nodiscard]] ColorSwatches* paletteSwatches() const { return m_palette; }
    [[nodiscard]] ColorSwatches* recentSwatches() const { return m_recent; }

    /// @brief Ask for a color in the color window (More Colors...); replaced in tests
    using ColorAsker = std::function<std::optional<QColor>(const QColor& initial)>;
    void setColorAsker(ColorAsker asker) { m_askColor = std::move(asker); }

signals:
    /// @brief The writer chose a color: invalid for Automatic or No Color
    void colorChosen(const QColor& color);

protected:
    /// @brief An arrow that leaves the swatches: they get the keys at the side it came from
    void keyPressEvent(QKeyEvent* event) override;

private:
    /// @brief The swatches got the keys: the swatch of the current color, else the first one
    ///        of the row the arrow came to
    void enterSwatches(ColorSwatches* swatches);

    Kind m_kind;
    CurrentColor m_currentColor;
    ColorAsker m_askColor;
    QColor m_lastColor;
    bool m_upward = false;  ///< The last arrow of the menu went up
    QAction* m_noColorAction = nullptr;
    QWidgetAction* m_paletteAction = nullptr;
    QWidgetAction* m_recentAction = nullptr;
    QAction* m_moreColorsAction = nullptr;
    ColorSwatches* m_palette = nullptr;
    ColorSwatches* m_recent = nullptr;
    QLabel* m_recentLabel = nullptr;
};

} // namespace gui
} // namespace kalahari
