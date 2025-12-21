/// @file editor_panel.cpp
/// @brief Editor panel implementation with BookEditor (OpenSpec #00042 Phase 7.1)

#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/kml_document.h"
#include "kalahari/editor/kml_paragraph.h"
#include "kalahari/editor/kml_parser.h"
#include "kalahari/editor/clipboard_handler.h"
#include "kalahari/editor/editor_appearance.h"
#include <QVBoxLayout>

namespace kalahari {
namespace gui {

/// @brief Document observer that forwards content changes to EditorPanel
class EditorPanel::Observer : public editor::IDocumentObserver {
public:
    explicit Observer(EditorPanel* panel) : m_panel(panel) {}

    void onContentChanged() override {
        if (m_panel) {
            emit m_panel->contentChanged();
        }
    }

private:
    EditorPanel* m_panel;
};

EditorPanel::EditorPanel(QWidget* parent)
    : QWidget(parent)
    , m_bookEditor(nullptr)
    , m_document(nullptr)
    , m_observer(nullptr)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("EditorPanel constructor called");

    // Create observer first
    m_observer = std::make_unique<Observer>(this);

    // Create layout
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create BookEditor widget
    m_bookEditor = new editor::BookEditor(this);
    layout->addWidget(m_bookEditor);

    setLayout(layout);

    // Create empty document
    m_document = createEmptyDocument();

    // Set document on editor
    m_bookEditor->setDocument(m_document.get());

    // Apply settings (font, appearance)
    applySettings();

    // Setup document observer
    setupDocumentObserver();

    logger.debug("EditorPanel initialized with BookEditor");
}

EditorPanel::~EditorPanel() {
    // Remove observer before document is destroyed
    if (m_document && m_observer) {
        m_document->removeObserver(m_observer.get());
    }
}

void EditorPanel::setText(const QString& text) {
    if (!m_bookEditor || !m_observer) {
        return;
    }

    auto& logger = core::Logger::getInstance();
    logger.debug("EditorPanel::setText called with {} chars", text.length());

    // Remove observer from old document
    if (m_document) {
        m_document->removeObserver(m_observer.get());
    }

    // Convert plain text to KML and parse
    QString kml = editor::ClipboardHandler::textToKml(text);
    editor::KmlParser parser;
    auto result = parser.parseDocument(kml);

    if (result.success && result.result) {
        // Replace document
        m_document = std::move(result.result);
    } else {
        logger.warn("Failed to parse text as KML: {}", result.errorMessage.toStdString());
        // Fallback: create document with plain text paragraph
        m_document = createEmptyDocument();
        if (!m_document->isEmpty()) {
            auto* para = m_document->paragraph(0);
            if (para) {
                para->clearElements();
                para->insertText(0, text);
            }
        }
    }

    // Setup observer and update editor
    setupDocumentObserver();
    m_bookEditor->setDocument(m_document.get());
}

QString EditorPanel::getText() const {
    if (!m_document) {
        return QString();
    }

    return m_document->plainText();
}

void EditorPanel::setContent(const QString& content) {
    if (!m_bookEditor || !m_observer) {
        return;
    }

    auto& logger = core::Logger::getInstance();
    logger.debug("EditorPanel::setContent called with {} chars", content.length());

    // Remove observer from old document
    if (m_document) {
        m_document->removeObserver(m_observer.get());
    }

    // Convert HTML to KML
    QString kml = editor::ClipboardHandler::htmlToKml(content);

    // Parse KML
    editor::KmlParser parser;
    auto result = parser.parseDocument(kml);

    if (result.success && result.result) {
        // Replace document
        m_document = std::move(result.result);
        // Setup observer and update editor
        setupDocumentObserver();
        m_bookEditor->setDocument(m_document.get());
    } else {
        logger.warn("Failed to parse HTML->KML: {}", result.errorMessage.toStdString());
        // Fallback: treat as plain text
        setText(content);
    }
}

QString EditorPanel::getContent() const {
    if (!m_document) {
        return QString();
    }

    // Convert KML to HTML
    QString kml = m_document->toKml();
    return editor::ClipboardHandler::kmlToHtml(kml);
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

std::unique_ptr<editor::KmlDocument> EditorPanel::createEmptyDocument() {
    auto doc = std::make_unique<editor::KmlDocument>();
    // Add one empty paragraph
    auto para = std::make_unique<editor::KmlParagraph>();
    doc->addParagraph(std::move(para));
    return doc;
}

void EditorPanel::setupDocumentObserver() {
    if (m_document && m_observer) {
        m_document->addObserver(m_observer.get());
    }
}

} // namespace gui
} // namespace kalahari
