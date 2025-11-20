/// @file search_panel.cpp
/// @brief Search panel implementation

#include "kalahari/gui/panels/search_panel.h"
#include "kalahari/core/logger.h"
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {

SearchPanel::SearchPanel(QWidget* parent)
    : QWidget(parent)
    , m_searchEdit(nullptr)
    , m_resultsWidget(nullptr)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("SearchPanel constructor called");

    // Create layout
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(5, 5, 5, 5);

    // Create search input
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Search... (placeholder)"));
    layout->addWidget(m_searchEdit);

    // Create results list
    m_resultsWidget = new QListWidget(this);
    m_resultsWidget->addItem(tr("Search Panel (placeholder)"));
    m_resultsWidget->addItem(tr("Full implementation in Phase 1"));
    layout->addWidget(m_resultsWidget);

    setLayout(layout);

    logger.debug("SearchPanel initialized");
}

} // namespace gui
} // namespace kalahari
