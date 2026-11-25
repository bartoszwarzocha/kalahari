// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file assistant_panel.h
/// @brief Assistant panel placeholder (Qt6)
///
/// This file defines the AssistantPanel class, a placeholder for the
/// AI assistant panel. Will be enhanced in Phase 2+.

#ifndef KALAHARI_GUI_PANELS_ASSISTANT_PANEL_H
#define KALAHARI_GUI_PANELS_ASSISTANT_PANEL_H

#include <QWidget>

class QLabel;

namespace kalahari::gui {

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
    QLabel* m_placeholderLabel = nullptr;
};

} // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_ASSISTANT_PANEL_H
