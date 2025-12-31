/// @file editor_panel.cpp
/// @brief Editor panel implementation with BookEditor (OpenSpec #00042 Phase 7.1)

#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/clipboard_handler.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/editor/statistics_collector.h"
#include <QVBoxLayout>

namespace kalahari {
namespace gui {

EditorPanel::EditorPanel(QWidget* parent)
    : QWidget(parent)
    , m_bookEditor(nullptr)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("EditorPanel constructor called");

    // Create layout
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create BookEditor widget (Phase 11: uses QTextDocument internally)
    m_bookEditor = new editor::BookEditor(this);
    layout->addWidget(m_bookEditor);

    // Connect BookEditor's contentChanged signal to our signal
    connect(m_bookEditor, &editor::BookEditor::contentChanged,
            this, &EditorPanel::contentChanged);

    setLayout(layout);

    // Apply settings (font, appearance)
    applySettings();

    logger.debug("EditorPanel initialized with BookEditor (new architecture)");
}

EditorPanel::~EditorPanel() {
    // Disconnect StatisticsCollector from editor
    if (m_statisticsCollector) {
        m_statisticsCollector->setBookEditor(nullptr);
    }
}

void EditorPanel::setText(const QString& text) {
    if (!m_bookEditor) {
        return;
    }

    auto& logger = core::Logger::getInstance();
    logger.debug("EditorPanel::setText called with {} chars", text.length());

    // Convert plain text to KML
    QString kml = editor::ClipboardHandler::textToKml(text);

    // Use BookEditor::fromKml() for Phase 11 architecture
    // This method populates QTextDocument, ViewportManager, RenderEngine
    m_bookEditor->fromKml(kml);
    logger.debug("EditorPanel::setText - BookEditor::fromKml() complete");

    // Reconnect statistics collector to editor (OpenSpec #00042 Task 7.7)
    if (m_statisticsCollector && m_bookEditor) {
        m_statisticsCollector->setBookEditor(m_bookEditor);
    }
}

QString EditorPanel::getText() const {
    // Use BookEditor's new API which reads from QTextDocument
    if (m_bookEditor) {
        return m_bookEditor->plainText();
    }
    return QString();
}

void EditorPanel::setContent(const QString& content) {
    if (!m_bookEditor) {
        return;
    }

    auto& logger = core::Logger::getInstance();
    logger.debug("EditorPanel::setContent called with {} chars", content.length());

    // Use BookEditor::fromKml() for Phase 11 architecture
    // This method populates QTextDocument, ViewportManager, RenderEngine
    m_bookEditor->fromKml(content);
    logger.debug("EditorPanel::setContent - BookEditor::fromKml() complete");

    // Reconnect statistics collector if needed
    if (m_statisticsCollector && m_bookEditor) {
        m_statisticsCollector->setBookEditor(m_bookEditor);
    }
}

QString EditorPanel::getContent() const {
    // Phase 11: Use BookEditor::toKml() - QTextDocument-based architecture
    if (m_bookEditor) {
        return m_bookEditor->toKml();
    }
    return QString();
}

void EditorPanel::applySettings() {
    if (!m_bookEditor) {
        return;
    }

    auto& logger = core::Logger::getInstance();
    auto& settings = core::SettingsManager::getInstance();

    // Get current appearance and modify
    editor::EditorAppearance appearance = m_bookEditor->appearance();

    // Font family and size (applied to typography.textFont)
    std::string fontFamily = settings.get<std::string>("editor.fontFamily", "Georgia");
    int fontSize = settings.get<int>("editor.fontSize", 14);
    appearance.typography.textFont = QFont(QString::fromStdString(fontFamily), fontSize);
    logger.debug("Applied font: {} {}pt", fontFamily, fontSize);

    // Line height (default 1.6 for readability)
    double lineHeight = settings.get<double>("editor.lineHeight", 1.6);
    appearance.typography.lineHeight = lineHeight;
    logger.debug("Applied line height: {}", lineHeight);

    // Paragraph spacing
    double paragraphSpacing = settings.get<double>("editor.paragraphSpacing", 12.0);
    appearance.typography.paragraphSpacing = paragraphSpacing;
    logger.debug("Applied paragraph spacing: {} px", paragraphSpacing);

    // First line indent
    bool firstLineIndent = settings.get<bool>("editor.firstLineIndent", true);
    appearance.typography.firstLineIndent = firstLineIndent;
    double indentSize = settings.get<double>("editor.indentSize", 24.0);
    appearance.typography.indentSize = indentSize;
    logger.debug("Applied first line indent: {} (size: {})", firstLineIndent, indentSize);

    // Apply appearance
    m_bookEditor->setAppearance(appearance);

    logger.debug("EditorPanel settings applied to BookEditor");
}

void EditorPanel::setStatisticsCollector(editor::StatisticsCollector* collector) {
    auto& logger = core::Logger::getInstance();

    // Disconnect from previous collector
    if (m_statisticsCollector) {
        m_statisticsCollector->setBookEditor(nullptr);
    }

    m_statisticsCollector = collector;

    // Connect to new collector
    if (m_statisticsCollector && m_bookEditor) {
        m_statisticsCollector->setBookEditor(m_bookEditor);
        logger.debug("EditorPanel: StatisticsCollector connected to BookEditor");
    }
}

} // namespace gui
} // namespace kalahari
