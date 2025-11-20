/// @file editor_panel.cpp
/// @brief Editor panel implementation

#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/core/logger.h"
#include <QPlainTextEdit>
#include <QVBoxLayout>

namespace kalahari {
namespace gui {

EditorPanel::EditorPanel(QWidget* parent)
    : QWidget(parent)
    , m_textEdit(nullptr)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("EditorPanel constructor called");

    // Create layout
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create text edit widget
    m_textEdit = new QPlainTextEdit(this);
    m_textEdit->setPlaceholderText(tr("Editor Panel (placeholder - full implementation in Phase 1)"));
    layout->addWidget(m_textEdit);

    setLayout(layout);

    logger.debug("EditorPanel initialized");
}

} // namespace gui
} // namespace kalahari
