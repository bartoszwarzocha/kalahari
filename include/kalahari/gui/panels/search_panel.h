/// @file search_panel.h
/// @brief Search panel placeholder (Qt6)
///
/// This file defines the SearchPanel class, a placeholder for the
/// search functionality. Will be enhanced in Phase 1.

#pragma once

#include <QWidget>

class QLineEdit;
class QListWidget;

namespace kalahari {
namespace gui {

/// @brief Search panel (placeholder)
///
/// Displays a QLineEdit for search input and QListWidget for results.
/// This is a placeholder - full implementation comes in Phase 1.
class SearchPanel : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit SearchPanel(QWidget* parent = nullptr);

    /// @brief Destructor
    ~SearchPanel() override = default;

private:
    QLineEdit* m_searchEdit;
    QListWidget* m_resultsWidget;
};

} // namespace gui
} // namespace kalahari
