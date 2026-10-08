/// @file length_spin_box.h
/// @brief A spin box for a length, shown in the unit the user chose
///
/// Each length setting is stored in its own unit (pixels or millimeters), so the files
/// do not change with the user's choice; the field shows the value in the unit set in
/// Settings > General > Units (ui.lengthUnit). One inch is 25.4 mm, 72 points and
/// 96 pixels (the CSS convention, also the size of a pixel at 100% zoom on Windows).

#pragma once

#include <QDoubleSpinBox>
#include <QString>

namespace kalahari {
namespace gui {

/// @brief Units a length can be shown in
enum class LengthUnit { Pixels, Millimeters, Centimeters, Inches, Points };

/// @brief The unit stored in ui.lengthUnit ("px", "mm", "cm", "in", "pt"); millimeters
///        for an unknown name
[[nodiscard]] LengthUnit lengthUnitFromName(const QString& name);

/// @brief The name of a unit as stored in ui.lengthUnit
[[nodiscard]] QString lengthUnitName(LengthUnit unit);

/// @brief Convert a length from one unit to another
[[nodiscard]] double convertLength(double value, LengthUnit from, LengthUnit to);

/// @brief The unit chosen in the settings (ui.lengthUnit)
[[nodiscard]] LengthUnit currentLengthUnit();

/// @brief A spin box for a length stored in one unit and shown in another
///
/// The stored value changes only when the user edits the field, so switching the shown
/// unit back and forth does not round it.
class LengthSpinBox : public QDoubleSpinBox {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param storedUnit Unit of the setting
    /// @param minimum Smallest value, in the stored unit
    /// @param maximum Largest value, in the stored unit
    /// @param parent Parent widget
    LengthSpinBox(LengthUnit storedUnit, double minimum, double maximum, QWidget* parent = nullptr);

    /// @brief Show the value in another unit (the stored value stays)
    void setDisplayUnit(LengthUnit unit);

    /// @brief The unit the value is shown in
    [[nodiscard]] LengthUnit displayUnit() const { return m_displayUnit; }

    /// @brief The unit of the setting
    [[nodiscard]] LengthUnit storedUnit() const { return m_storedUnit; }

    /// @brief The value in the stored unit
    [[nodiscard]] double storedValue() const { return m_storedValue; }

    /// @brief Set the value, in the stored unit
    void setStoredValue(double value);

private:
    /// @brief Show m_storedValue in the display unit
    void showStoredValue();

    LengthUnit m_storedUnit;
    LengthUnit m_displayUnit;
    double m_minimum;
    double m_maximum;
    double m_storedValue;
    bool m_updating = false;  ///< The value is being set by the program, not the user
};

} // namespace gui
} // namespace kalahari
