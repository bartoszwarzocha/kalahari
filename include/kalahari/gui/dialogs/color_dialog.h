/// @file color_dialog.h
/// @brief The program's own window for choosing a color

#pragma once

#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QColor>
#include <QString>
#include <QStringList>

#include <optional>

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QSpinBox;

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief A color to choose, in place of QColorDialog
///
/// The palette offers greys and nine hues in five shades; the recent colors are the
/// last ones chosen with Select, kept between sessions. The color can also be typed as
/// HEX or as red, green and blue. The preview shows the previous color (pressing it
/// brings it back) beside the new one. Everything works from the keyboard: the arrows
/// move in the palette and the recent colors, Alt with the underlined letter reaches each
/// part, Enter selects and Esc cancels.
class ColorDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief Most recent colors kept
    static constexpr int MAX_RECENT_COLORS = 10;

    /// @brief Create the dialog
    /// @param initial The color the setting has now, shown as the previous color
    /// @param description What the color is for (e.g. the setting's name); empty: none
    /// @param parent Parent widget
    ColorDialog(const QColor& initial, const QString& description = QString(),
                QWidget* parent = nullptr);

    /// @brief The color chosen so far
    QColor color() const { return m_color; }

    /// @brief Choose a color
    void setColor(const QColor& color);

    /// @brief The palette's colors, row by row (10 columns)
    static QList<QColor> paletteColors();

    /// @brief The recent colors, newest first, from the settings
    static QStringList recentColors();

    /// @brief Put a color at the front of the recent colors in the settings
    static void addRecentColor(const QColor& color);

    /// @brief Ask for a color in one call
    /// @param initial The color the setting has now
    /// @param parent Parent widget
    /// @param description What the color is for; empty: none
    /// @return The chosen color (also added to the recent colors), or nothing if cancelled
    static std::optional<QColor> getColor(const QColor& initial, QWidget* parent,
                                          const QString& description = QString());

    /// @brief Select: remember the color among the recent ones and close
    void accept() override;

    /// @brief The palette list (for tests)
    QListWidget* paletteList() const { return m_paletteList; }

    /// @brief The recent colors list (for tests)
    QListWidget* recentList() const { return m_recentList; }

    /// @brief The HEX field (for tests)
    QLineEdit* hexField() const { return m_hexEdit; }

    /// @brief The red, green and blue fields (for tests)
    QSpinBox* redField() const { return m_redSpin; }
    QSpinBox* greenField() const { return m_greenSpin; }
    QSpinBox* blueField() const { return m_blueSpin; }

    /// @brief The button with the previous color (for tests)
    QPushButton* previousButton() const { return m_previousButton; }

private:
    /// @brief A list of color swatches: arrows move, the current item is the color
    QListWidget* createSwatchList(const QString& accessibleName);

    /// @brief Add a swatch of a color to a list
    void addSwatch(QListWidget* list, const QColor& color);

    /// @brief The HEX field changed: a complete value chooses the color
    void onHexEdited(const QString& text);

    /// @brief A red, green or blue field changed
    void onRgbChanged();

    /// @brief Show the chosen color in the preview, the fields and the lists
    void updateViews();

    QColor m_initial;
    QColor m_color;
    bool m_updating = false;
    bool m_typingHex = false;     ///< The writer is typing in the HEX field: leave its text
    QListWidget* m_paletteList = nullptr;
    QListWidget* m_recentList = nullptr;
    QLabel* m_noRecentLabel = nullptr;
    QPushButton* m_previousButton = nullptr;
    QLabel* m_newLabel = nullptr;
    QLineEdit* m_hexEdit = nullptr;
    QSpinBox* m_redSpin = nullptr;
    QSpinBox* m_greenSpin = nullptr;
    QSpinBox* m_blueSpin = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
