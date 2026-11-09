/// @file color_dialog.h
/// @brief The program's own window for choosing a color

#pragma once

#include "kalahari/gui/dialogs/color_palettes.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QColor>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSlider;
class QSpinBox;
class QToolButton;
class QWidget;

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief A color to choose, in place of QColorDialog
///
/// Six palettes (Basic, Kalahari, Okavango, Pastel, Ink and Paper, Savanna Dusk) offer ten
/// families of named colors in six shades; the window remembers the palette chosen last. The
/// name of the color is shown under the preview, for a color of no palette with the closest
/// one. Without knowing RGB or HEX the writer takes a lighter or darker shade of the color,
/// or changes its hue, intensity and lightness on sliders. The recent colors are the last
/// ones chosen with Select, My colors the ones the writer keeps; both are kept between
/// sessions. HEX and red, green and blue are under Numeric values. Everything works from the
/// keyboard: the arrows move in the swatches and on the sliders and choose the color, Alt
/// with the underlined letter reaches each part, Enter selects and Esc cancels.
class ColorDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief Most recent colors kept
    static constexpr int MAX_RECENT_COLORS = 10;

    /// @brief Most colors kept among My colors
    static constexpr int MAX_MY_COLORS = 10;

    /// @brief The shades of the color offered from light to dark
    static constexpr int SHADES = 9;

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

    /// @brief The recent colors, newest first, from the settings
    static QStringList recentColors();

    /// @brief Put a color at the front of the recent colors in the settings
    static void addRecentColor(const QColor& color);

    /// @brief My colors, in the order they were added, from the settings
    static QStringList myColors();

    /// @brief The palette the window showed last, which it shows next (the first one at
    ///        first)
    static ColorPalette lastPalette();

    /// @brief Ask for a color in one call
    /// @param initial The color the setting has now
    /// @param parent Parent widget
    /// @param description What the color is for; empty: none
    /// @return The chosen color (also added to the recent colors), or nothing if cancelled
    static std::optional<QColor> getColor(const QColor& initial, QWidget* parent,
                                          const QString& description = QString());

    /// @brief Select: remember the color among the recent ones and close
    void accept() override;

    /// @brief The palettes, in the order of the list of palettes
    const QList<ColorPalette>& palettes() const { return m_palettes; }

    /// @brief The list of palettes (for tests)
    QComboBox* paletteBox() const { return m_paletteBox; }

    /// @brief The swatches of the palette shown (for tests)
    QListWidget* paletteList() const { return m_paletteList; }

    /// @brief The recent colors list (for tests)
    QListWidget* recentList() const { return m_recentList; }

    /// @brief My colors with their buttons and their note (for tests)
    QListWidget* myColorsList() const { return m_myList; }
    QPushButton* addButton() const { return m_addButton; }
    QPushButton* removeButton() const { return m_removeButton; }
    QLabel* myColorsNote() const { return m_myColorsNote; }

    /// @brief The name of the color, and the closest named one for a color of no palette
    /// (for tests)
    QLabel* nameLabel() const { return m_nameLabel; }
    QLabel* closestLabel() const { return m_closestLabel; }

    /// @brief The shades of the color, from light to dark (for tests)
    QListWidget* shadesList() const { return m_shadesList; }

    /// @brief The hue, intensity and lightness sliders (for tests)
    QSlider* hueSlider() const { return m_hueSlider; }
    QSlider* intensitySlider() const { return m_intensitySlider; }
    QSlider* lightnessSlider() const { return m_lightnessSlider; }

    /// @brief The button that shows or hides the numeric values (for tests)
    QToolButton* valuesButton() const { return m_valuesButton; }

    /// @brief The HEX field (for tests)
    QLineEdit* hexField() const { return m_hexEdit; }

    /// @brief The red, green and blue fields (for tests)
    QSpinBox* redField() const { return m_redSpin; }
    QSpinBox* greenField() const { return m_greenSpin; }
    QSpinBox* blueField() const { return m_blueSpin; }

    /// @brief The button with the previous color (for tests)
    QPushButton* previousButton() const { return m_previousButton; }

private:
    /// @brief The left column: the palettes, the recent colors and My colors
    QLayout* createSwatches();

    /// @brief The right column: the preview with the name, the shades, the sliders and the
    /// numeric values
    QLayout* createAdjustments();

    /// @brief A list of @p columns color swatches in a row: arrows move, and the current
    /// item is the color
    QListWidget* createSwatchList(const QString& accessibleName, int columns);

    /// @brief Fill @p list with swatches of @p colors
    void fillSwatches(QListWidget* list, const QStringList& colors);

    /// @brief Show the palette @p index of the list of palettes, and remember it
    void showPalette(int index);

    /// @brief The writer chose a swatch of @p list
    void onSwatchChosen(QListWidget* list);

    /// @brief A slider moved: the color with its hue, intensity and lightness
    void onSliderMoved();

    /// @brief The HEX field changed: a complete value chooses the color
    void onHexEdited(const QString& text);

    /// @brief A red, green or blue field changed
    void onRgbChanged();

    /// @brief Add the color to My colors, which the settings keep
    void addMyColor();

    /// @brief Remove the selected color from My colors
    void removeMyColor();

    /// @brief Show or hide the numeric values, and remember it
    void showValues(bool shown);

    /// @brief The name of @p color in the palettes; for a color of no palette "Own color"
    QString nameOf(const QColor& color) const;

    /// @brief Show the chosen color in the preview, its name, the shades, the sliders, the
    /// fields and the lists
    void updateViews();

    QList<ColorPalette> m_palettes;
    QColor m_initial;
    QColor m_color;
    int m_hue = 0;          ///< Of the color, in degrees; a grey keeps the one it had
    int m_saturation = 0;   ///< Of the color, in percent
    int m_lightness = 0;    ///< Of the color, in percent
    bool m_updating = false;
    bool m_typingHex = false;     ///< The writer is typing in the HEX field: leave its text
    QComboBox* m_paletteBox = nullptr;
    QLabel* m_paletteDescription = nullptr;
    QListWidget* m_paletteList = nullptr;
    QListWidget* m_recentList = nullptr;
    QLabel* m_noRecentLabel = nullptr;
    QListWidget* m_myList = nullptr;
    QPushButton* m_addButton = nullptr;
    QPushButton* m_removeButton = nullptr;
    QLabel* m_myColorsNote = nullptr;
    QPushButton* m_previousButton = nullptr;
    QLabel* m_newLabel = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_closestLabel = nullptr;
    QListWidget* m_shadesList = nullptr;
    QSlider* m_hueSlider = nullptr;
    QSlider* m_intensitySlider = nullptr;
    QSlider* m_lightnessSlider = nullptr;
    QToolButton* m_valuesButton = nullptr;
    QWidget* m_valuesArea = nullptr;
    QLineEdit* m_hexEdit = nullptr;
    QSpinBox* m_redSpin = nullptr;
    QSpinBox* m_greenSpin = nullptr;
    QSpinBox* m_blueSpin = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
