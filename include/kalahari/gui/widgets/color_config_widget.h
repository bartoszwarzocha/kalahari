/// @file color_config_widget.h
/// @brief Reusable color configuration widget for settings panels

#pragma once

#include <QWidget>
#include <QColor>

class QPushButton;
class QLabel;

namespace kalahari {
namespace gui {

/// @brief A reusable widget for color configuration in settings
///
/// Displays a horizontal layout with:
/// - Label (color name)
/// - Color button (shows current color as background, clickable)
/// - Hex code label (e.g., "#FF0000")
///
/// Usage:
/// @code
/// ColorConfigWidget* colorWidget = new ColorConfigWidget(tr("Primary Color"), this);
/// colorWidget->setColor(QColor("#333333"));
/// connect(colorWidget, &ColorConfigWidget::colorChanged, this, &MyClass::onColorChanged);
/// @endcode
class ColorConfigWidget : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param label The display label for the color (e.g., "Primary", "Background")
    /// @param parent Parent widget
    explicit ColorConfigWidget(const QString& label, QWidget* parent = nullptr);

    /// @brief Destructor
    ~ColorConfigWidget() override = default;

    /// @brief Set the current color
    /// @param color The color to display
    void setColor(const QColor& color);

    /// @brief Get the current color
    /// @return The currently selected color
    QColor color() const;

signals:
    /// @brief Emitted when the color changes via user interaction
    /// @param color The new color selected by the user
    void colorChanged(const QColor& color);

private slots:
    /// @brief Handle color button click - opens QColorDialog
    void onColorButtonClicked();

private:
    /// @brief Update the button and hex label to reflect current color
    void updateColorDisplay();

    QLabel* m_label;            ///< Label showing color name
    QPushButton* m_colorButton; ///< Button showing color preview
    QLabel* m_hexLabel;         ///< Label showing hex code (e.g., "#FF0000")
    QColor m_color;             ///< Current color value
};

} // namespace gui
} // namespace kalahari
