/// @file editor_panel.cpp
/// @brief Editor panel implementation (Task #00007)

#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include <QPlainTextEdit>
#include <QVBoxLayout>
#include <QFontMetrics>

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
    layout->addWidget(m_textEdit);

    setLayout(layout);

    // Apply settings (font, tab size, word wrap)
    applySettings();

    logger.debug("EditorPanel initialized with settings");
}

void EditorPanel::setText(const QString& text) {
    if (m_textEdit) {
        m_textEdit->setPlainText(text);
    }
}

QString EditorPanel::getText() const {
    if (m_textEdit) {
        return m_textEdit->toPlainText();
    }
    return QString();
}

void EditorPanel::applySettings() {
    auto& logger = core::Logger::getInstance();
    auto& settings = core::SettingsManager::getInstance();

    // Font family and size
    std::string fontFamily = settings.get<std::string>("editor.fontFamily", "Consolas");
    int fontSize = settings.get<int>("editor.fontSize", 12);
    QFont font(QString::fromStdString(fontFamily), fontSize);
    m_textEdit->setFont(font);
    logger.debug("Applied font: {} {}pt", fontFamily, fontSize);

    // Tab size (convert spaces to pixels)
    int tabSize = settings.get<int>("editor.tabSize", 4);
    QFontMetrics metrics(font);
    int tabWidth = tabSize * metrics.horizontalAdvance(' ');
    m_textEdit->setTabStopDistance(tabWidth);
    logger.debug("Applied tab size: {} spaces ({} px)", tabSize, tabWidth);

    // Word wrap
    bool wordWrap = settings.get<bool>("editor.wordWrap", false);
    m_textEdit->setLineWrapMode(wordWrap ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
    logger.debug("Applied word wrap: {}", wordWrap);

    // Line numbers (feature not implemented yet - placeholder)
    bool lineNumbers = settings.get<bool>("editor.lineNumbers", true);
    logger.debug("Line numbers setting: {} (display not yet implemented)", lineNumbers);
}

} // namespace gui
} // namespace kalahari
