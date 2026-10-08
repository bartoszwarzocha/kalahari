/// @file editor_panel.cpp
/// @brief Editor panel implementation with BookEditor (OpenSpec #00042 Phase 7.1)

#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/clipboard_handler.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/editor/statistics_collector.h"
#include <QEvent>
#include <QMessageBox>
#include <QScreen>
#include <QVBoxLayout>
#include <cmath>

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

    // The light or dark paper chosen in the editor's context menu is the setting of all
    // editors, kept between sessions
    connect(m_bookEditor, &editor::BookEditor::editorColorModeChanged, this,
            [](editor::EditorColorMode mode) {
                core::SettingsManager::getInstance().set<bool>(
                    "editor.darkMode", mode == editor::EditorColorMode::Dark);
            });

    setLayout(layout);

    // Apply settings (font, appearance)
    applySettings();

    // Every chapter or file opens at the zoom of the settings (100%), or at the page's
    // width when the page is wider than the editor (a small screen); after that the zoom
    // changes only when asked for, not with the size of the window
    m_bookEditor->shrinkToPageWidthOnFirstShow();

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
    // This method populates QTextDocument, ViewportManager, EditorRenderPipeline
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

bool EditorPanel::setContent(const QString& content) {
    if (!m_bookEditor) {
        return true;
    }

    auto& logger = core::Logger::getInstance();
    logger.debug("EditorPanel::setContent called with {} chars", content.length());

    // Use BookEditor::fromKml() for Phase 11 architecture
    // This method populates QTextDocument, ViewportManager, EditorRenderPipeline
    const bool complete = m_bookEditor->fromKml(content);
    logger.debug("EditorPanel::setContent - BookEditor::fromKml() complete");

    // Reconnect statistics collector if needed
    if (m_statisticsCollector && m_bookEditor) {
        m_statisticsCollector->setBookEditor(m_bookEditor);
    }
    return complete;
}

void EditorPanel::warnDamagedChapter(QWidget* parent, const QString& chapterName) {
    QMessageBox::warning(
        parent,
        tr("Damaged Chapter"),
        tr("The chapter \"%1\" is damaged, so only its text before the damaged place is shown.\n\n"
           "Saving the chapter keeps only this text. Close it without saving to leave the file "
           "as it is.").arg(chapterName));
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
    std::string fontFamily = settings.get<std::string>("editor.fontFamily", editor::DEFAULT_TEXT_FONT_FAMILY);
    int fontSize = settings.get<int>("editor.fontSize", editor::DEFAULT_TEXT_FONT_SIZE);
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

    // Editor color mode (light/dark toggle - independent from app theme)
    bool darkMode = settings.get<bool>("editor.darkMode", true);
    appearance.colorMode = darkMode
        ? editor::EditorColorMode::Dark
        : editor::EditorColorMode::Light;
    logger.debug("Applied editor color mode: {}", darkMode ? "Dark" : "Light");

    // Editor colors - light mode
    std::string bgLight = settings.get<std::string>("editor.colors.backgroundLight", "#ffffff");
    std::string textLight = settings.get<std::string>("editor.colors.textLight", "#1e1e1e");
    std::string inactiveLight = settings.get<std::string>("editor.colors.inactiveLight", "#aaaaaa");
    appearance.colors.continuous.backgroundLight = QColor(QString::fromStdString(bgLight));
    appearance.colors.continuous.textLight = QColor(QString::fromStdString(textLight));
    appearance.colors.focus.inactiveLight = QColor(QString::fromStdString(inactiveLight));

    // Editor colors - dark mode
    std::string bgDark = settings.get<std::string>("editor.colors.backgroundDark", "#232328");
    std::string textDark = settings.get<std::string>("editor.colors.textDark", "#e0e0e0");
    std::string inactiveDark = settings.get<std::string>("editor.colors.inactiveDark", "#78787d");
    appearance.colors.continuous.backgroundDark = QColor(QString::fromStdString(bgDark));
    appearance.colors.continuous.textDark = QColor(QString::fromStdString(textDark));
    appearance.colors.focus.inactiveDark = QColor(QString::fromStdString(inactiveDark));

    logger.debug("Applied editor colors");

    // Cursor settings
    int cursorStyleInt = settings.get<int>("editor.cursor.style", 0);  // 0 = Line
    appearance.cursor.style = static_cast<editor::CursorStyle>(cursorStyleInt);
    appearance.cursor.useCustomColor = settings.get<bool>("editor.cursor.useCustomColor", false);
    std::string cursorColor = settings.get<std::string>("editor.cursor.customColor", "#ffffff");
    appearance.cursor.customColor = QColor(QString::fromStdString(cursorColor));
    appearance.cursor.blinking = settings.get<bool>("editor.cursor.blinking", true);
    appearance.cursor.blinkInterval = settings.get<int>("editor.cursor.blinkInterval", 500);
    appearance.cursor.lineWidth = settings.get<int>("editor.cursor.lineWidth", 2);
    logger.debug("Applied cursor settings: style={}, blinking={}, interval={}ms",
                 cursorStyleInt, appearance.cursor.blinking, appearance.cursor.blinkInterval);

    // Page margins (every view: the continuous views are an endless page)
    appearance.pageMargins.top = settings.get<double>("editor.margins.pageTop", 25.4);
    appearance.pageMargins.bottom = settings.get<double>("editor.margins.pageBottom", 25.4);
    appearance.pageMargins.left = settings.get<double>("editor.margins.pageLeft", 25.4);
    appearance.pageMargins.right = settings.get<double>("editor.margins.pageRight", 25.4);
    appearance.pageMargins.mirrorEnabled = settings.get<bool>("editor.margins.mirrorEnabled", false);
    appearance.pageMargins.inner = settings.get<double>("editor.margins.pageInner", 30.0);
    appearance.pageMargins.outer = settings.get<double>("editor.margins.pageOuter", 20.0);
    logger.debug("Applied page margins: T={} B={} L={} R={}",
        appearance.pageMargins.top, appearance.pageMargins.bottom,
        appearance.pageMargins.left, appearance.pageMargins.right);

    // Page format (Page Layout view)
    appearance.pageLayout.pageSize = editor::PageLayout::pageSizeFromId(
        QString::fromStdString(settings.get<std::string>("editor.page.size", "A4")));
    appearance.pageLayout.customWidth = settings.get<double>("editor.page.customWidth", 210.0);
    appearance.pageLayout.customHeight = settings.get<double>("editor.page.customHeight", 297.0);
    appearance.pageLayout.pageGap = settings.get<int>("editor.page.gap", 20);
    appearance.pageLayout.showPageNumbers = settings.get<bool>("editor.page.showNumbers", true);

    // Typewriter scrolling (View > Typewriter Scrolling; the height in the settings)
    appearance.typewriter.enabled = settings.get<bool>("editor.typewriter.enabled", false);
    appearance.typewriter.focusPosition = settings.get<double>("editor.typewriter.focusPosition", 0.5);
    appearance.typewriter.smoothScroll = settings.get<bool>("editor.typewriter.smoothScroll", true);

    // Focus (View > Focus)
    appearance.focusMode.enabled = settings.get<bool>("editor.focus.enabled", false);

    // Text frame border
    appearance.textFrameBorder.show = settings.get<bool>("editor.textFrameBorder.show", false);
    std::string borderColor = settings.get<std::string>("editor.textFrameBorder.color", "#b4b4b4");
    appearance.textFrameBorder.color = QColor(QString::fromStdString(borderColor));
    appearance.textFrameBorder.width = settings.get<int>("editor.textFrameBorder.width", 1);
    logger.debug("Applied text frame border: show={}", appearance.textFrameBorder.show);

    // Apply appearance
    m_bookEditor->setAppearance(appearance);
    applyPaperScale();

    logger.debug("EditorPanel settings applied to BookEditor");
}

void EditorPanel::applyPaperScale() {
    // Zoom 100% shows the pages at their size on paper, from the size of the screen
    const QScreen* panelScreen = screen();
    const double scale = editor::BookEditor::paperScaleOf(panelScreen);
    if (std::abs(scale - m_bookEditor->paperScale()) < 1e-6) {
        return;
    }
    m_bookEditor->setPaperScale(scale);
    core::Logger::getInstance().info(
        "EditorPanel: pages at their size on paper, scale {:.3f} (screen {:.1f} dpi, logical {:.1f})",
        scale, panelScreen ? panelScreen->physicalDotsPerInch() : 0.0,
        panelScreen ? panelScreen->logicalDotsPerInch() : 0.0);
}

bool EditorPanel::event(QEvent* event) {
    // Shown, or moved to another screen, the pages keep their size on paper
    if ((event->type() == QEvent::Show || event->type() == QEvent::ScreenChangeInternal) &&
        m_bookEditor) {
        applyPaperScale();
    }
    return QWidget::event(event);
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
