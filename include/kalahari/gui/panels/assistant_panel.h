/// @file assistant_panel.h
/// @brief Assistant panel placeholder (Qt6)
///
/// This file defines the AssistantPanel class, a placeholder for the
/// AI assistant panel. Will be enhanced in Phase 2+.

#pragma once

#include <QWidget>

class QLabel;

namespace kalahari {
namespace gui {

/// @brief Assistant panel (placeholder)
///
/// Displays a simple QLabel with placeholder text.
/// This is a placeholder - full implementation comes in Phase 2+.
class AssistantPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit AssistantPanel(QWidget* parent = nullptr);

    /// @brief Destructor
    ~AssistantPanel() override = default;

private:
    QLabel* m_placeholderLabel;
};

} // namespace gui
} // namespace kalahari
