/// @file log_panel.cpp
/// @brief Log panel implementation

#include "kalahari/gui/panels/log_panel.h"
#include "kalahari/core/logger.h"
#include <QPlainTextEdit>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {

LogPanel::LogPanel(QWidget* parent)
    : QWidget(parent)
    , m_logEdit(nullptr)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("LogPanel constructor called");

    // Create layout
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create log text edit
    m_logEdit = new QPlainTextEdit(this);
    m_logEdit->setReadOnly(true);
    m_logEdit->setPlaceholderText(tr("Log Panel (placeholder - colored output in Phase 1)"));
    m_logEdit->appendPlainText(tr("[INFO] Log panel initialized"));
    m_logEdit->appendPlainText(tr("[INFO] Full log integration comes in Phase 1"));
    layout->addWidget(m_logEdit);

    setLayout(layout);

    logger.debug("LogPanel initialized");
}

} // namespace gui
} // namespace kalahari
