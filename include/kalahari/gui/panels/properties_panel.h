/// @file properties_panel.h
/// @brief Properties panel placeholder (Qt6)
///
/// This file defines the PropertiesPanel class, a placeholder for the
/// properties editor. Will be enhanced in Phase 1.

#pragma once

#include <QWidget>

class QLabel;

namespace kalahari {
namespace gui {

/// @brief Properties panel (placeholder)
///
/// Displays a simple QLabel with placeholder text.
/// This is a placeholder - full implementation comes in Phase 1.
class PropertiesPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit PropertiesPanel(QWidget* parent = nullptr);

    /// @brief Destructor
    ~PropertiesPanel() override = default;

private:
    QLabel* m_placeholderLabel;
};

} // namespace gui
} // namespace kalahari
