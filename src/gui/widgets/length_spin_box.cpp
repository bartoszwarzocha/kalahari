/// @file length_spin_box.cpp
/// @brief Implementation of LengthSpinBox

#include "kalahari/gui/widgets/length_spin_box.h"
#include "kalahari/core/settings_manager.h"

#include <string>

namespace kalahari {
namespace gui {

namespace {

/// Size of one unit in millimeters
double millimetersPer(LengthUnit unit) {
    switch (unit) {
    case LengthUnit::Pixels:      return 25.4 / 96.0;
    case LengthUnit::Millimeters: return 1.0;
    case LengthUnit::Centimeters: return 10.0;
    case LengthUnit::Inches:      return 25.4;
    case LengthUnit::Points:      return 25.4 / 72.0;
    }
    return 1.0;
}

int decimalsOf(LengthUnit unit) {
    switch (unit) {
    case LengthUnit::Pixels:      return 0;
    case LengthUnit::Millimeters: return 1;
    case LengthUnit::Points:      return 1;
    case LengthUnit::Centimeters: return 2;
    case LengthUnit::Inches:      return 2;
    }
    return 1;
}

double stepOf(LengthUnit unit) {
    switch (unit) {
    case LengthUnit::Centimeters: return 0.1;
    case LengthUnit::Inches:      return 0.05;
    default:                      return 1.0;
    }
}

QString suffixOf(LengthUnit unit) {
    switch (unit) {
    case LengthUnit::Pixels:      return LengthSpinBox::tr(" px");
    case LengthUnit::Millimeters: return LengthSpinBox::tr(" mm");
    case LengthUnit::Centimeters: return LengthSpinBox::tr(" cm");
    case LengthUnit::Inches:      return LengthSpinBox::tr(" in");
    case LengthUnit::Points:      return LengthSpinBox::tr(" pt");
    }
    return QString();
}

} // namespace

LengthUnit lengthUnitFromName(const QString& name) {
    if (name == QStringLiteral("px")) return LengthUnit::Pixels;
    if (name == QStringLiteral("cm")) return LengthUnit::Centimeters;
    if (name == QStringLiteral("in")) return LengthUnit::Inches;
    if (name == QStringLiteral("pt")) return LengthUnit::Points;
    return LengthUnit::Millimeters;
}

QString lengthUnitName(LengthUnit unit) {
    switch (unit) {
    case LengthUnit::Pixels:      return QStringLiteral("px");
    case LengthUnit::Millimeters: return QStringLiteral("mm");
    case LengthUnit::Centimeters: return QStringLiteral("cm");
    case LengthUnit::Inches:      return QStringLiteral("in");
    case LengthUnit::Points:      return QStringLiteral("pt");
    }
    return QStringLiteral("mm");
}

double convertLength(double value, LengthUnit from, LengthUnit to) {
    if (from == to) {
        return value;
    }
    return value * millimetersPer(from) / millimetersPer(to);
}

LengthUnit currentLengthUnit() {
    return lengthUnitFromName(QString::fromStdString(
        core::SettingsManager::getInstance().get<std::string>("ui.lengthUnit", "mm")));
}

LengthSpinBox::LengthSpinBox(LengthUnit storedUnit, double minimum, double maximum, QWidget* parent)
    : QDoubleSpinBox(parent)
    , m_storedUnit(storedUnit)
    , m_displayUnit(storedUnit)
    , m_minimum(minimum)
    , m_maximum(maximum)
    , m_storedValue(minimum)
{
    connect(this, &QDoubleSpinBox::valueChanged, this, [this](double shown) {
        if (!m_updating) {
            m_storedValue = convertLength(shown, m_displayUnit, m_storedUnit);
        }
    });
    setDisplayUnit(currentLengthUnit());
}

void LengthSpinBox::setDisplayUnit(LengthUnit unit) {
    m_displayUnit = unit;
    m_updating = true;
    setDecimals(decimalsOf(unit));
    setSingleStep(stepOf(unit));
    setSuffix(suffixOf(unit));
    setRange(convertLength(m_minimum, m_storedUnit, unit), convertLength(m_maximum, m_storedUnit, unit));
    m_updating = false;
    showStoredValue();
}

void LengthSpinBox::setStoredValue(double value) {
    m_storedValue = value;
    showStoredValue();
}

void LengthSpinBox::showStoredValue() {
    m_updating = true;
    setValue(convertLength(m_storedValue, m_storedUnit, m_displayUnit));
    m_updating = false;
}

} // namespace gui
} // namespace kalahari
