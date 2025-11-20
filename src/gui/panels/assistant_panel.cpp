/// @file assistant_panel.cpp
/// @brief Assistant panel implementation

#include "kalahari/gui/panels/assistant_panel.h"
#include "kalahari/core/logger.h"
#include <QLabel>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {

AssistantPanel::AssistantPanel(QWidget* parent)
    : QWidget(parent)
    , m_placeholderLabel(nullptr)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("AssistantPanel constructor called");

    // Create layout
    QVBoxLayout* layout = new QVBoxLayout(this);

    // Create placeholder label
    m_placeholderLabel = new QLabel(tr("Assistant Panel\n🦁\n(Placeholder - full implementation in Phase 2+)"), this);
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_placeholderLabel);

    setLayout(layout);

    logger.debug("AssistantPanel initialized");
}

} // namespace gui
} // namespace kalahari
