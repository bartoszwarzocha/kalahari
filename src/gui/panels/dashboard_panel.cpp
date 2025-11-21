/// @file dashboard_panel.cpp
/// @brief Dashboard panel implementation
///
/// @author Claude (AI Assistant)
/// @date 2025-11-21
/// @task #00015 - Central Tabbed Workspace

#include "kalahari/gui/panels/dashboard_panel.h"
#include "kalahari/core/logger.h"

namespace kalahari {
namespace gui {

DashboardPanel::DashboardPanel(QWidget* parent)
    : QWidget(parent)
    , m_welcomeLabel(nullptr)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("DashboardPanel constructor called");

    // Create main layout
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // Create welcome label with HTML styling
    m_welcomeLabel = new QLabel(this);
    m_welcomeLabel->setText(
        tr("<h1 style='color: #2c3e50;'>Welcome to Kalahari</h1>"
           "<p style='font-size: 14px; color: #7f8c8d;'>"
           "A Writer's IDE for book authors"
           "</p>"
           "<br>"
           "<p style='font-size: 12px;'>"
           "Create a new document or open an existing one to get started."
           "</p>"
           "<br>"
           "<p style='font-size: 11px; color: #95a5a6;'>"
           "<b>Quick Actions:</b><br>"
           "• File → New (Ctrl+N)<br>"
           "• File → Open (Ctrl+O)<br>"
           "• View → Panels (Ctrl+1-5)<br>"
           "</p>")
    );
    m_welcomeLabel->setAlignment(Qt::AlignCenter);
    m_welcomeLabel->setWordWrap(true);
    m_welcomeLabel->setTextFormat(Qt::RichText);

    // Add to layout with stretch (centered)
    mainLayout->addStretch();
    mainLayout->addWidget(m_welcomeLabel);
    mainLayout->addStretch();

    setLayout(mainLayout);

    logger.debug("DashboardPanel initialized");
}

} // namespace gui
} // namespace kalahari
