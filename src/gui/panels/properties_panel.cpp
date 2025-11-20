/// @file properties_panel.cpp
/// @brief Properties panel implementation

#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/core/logger.h"
#include <QLabel>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {

PropertiesPanel::PropertiesPanel(QWidget* parent)
    : QWidget(parent)
    , m_placeholderLabel(nullptr)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("PropertiesPanel constructor called");

    // Create layout
    QVBoxLayout* layout = new QVBoxLayout(this);

    // Create placeholder label
    m_placeholderLabel = new QLabel(tr("Properties Panel\n\n(Placeholder - full implementation in Phase 1)"), this);
    m_placeholderLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_placeholderLabel);

    setLayout(layout);

    logger.debug("PropertiesPanel initialized");
}

} // namespace gui
} // namespace kalahari
