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

    // A changed editor setting reaches the editor, wherever it was changed: the Settings
    // dialog, the View menu or the context menu of another editor
    m_settingsListener = core::SettingsManager::getInstance().subscribe(
        [this](const std::string& key) {
            if (key.rfind("editor.", 0) == 0) {
                scheduleSettings();
            }
        });

    // Apply settings (font, appearance)
    applySettings();

    // Every chapter or file opens at the zoom of the settings (100%), or at the page's
    // width when the page is wider than the editor (a small screen); after that the zoom
    // changes only when asked for, not with the size of the window
    m_bookEditor->shrinkToPageWidthOnFirstShow();

    logger.debug("EditorPanel initialized with BookEditor (new architecture)");
}

EditorPanel::~EditorPanel() {
    core::SettingsManager::getInstance().unsubscribe(m_settingsListener);

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

    // Every editor setting, read here only, with the defaults of the settings schema
    const auto& settings = core::SettingsManager::getInstance();
    const auto color = [&settings](const std::string& key) {
        return QColor(QString::fromStdString(settings.get<std::string>(key)));
    };

    editor::EditorAppearance appearance = m_bookEditor->appearance();

    // Text
    appearance.typography.textFont =
        QFont(QString::fromStdString(settings.get<std::string>("editor.fontFamily")),
              settings.get<int>("editor.fontSize"));
    appearance.typography.lineHeight = settings.get<double>("editor.lineHeight");
    appearance.typography.paragraphSpacing = settings.get<double>("editor.paragraphSpacing");
    appearance.typography.firstLineIndent = settings.get<bool>("editor.firstLineIndent");
    appearance.typography.indentSize = settings.get<double>("editor.indentSize");

    // The paper (light or dark, independent from the application's theme) and its colors
    appearance.colorMode = settings.get<bool>("editor.darkMode")
        ? editor::EditorColorMode::Dark
        : editor::EditorColorMode::Light;
    appearance.colors.continuous.backgroundLight = color("editor.colors.backgroundLight");
    appearance.colors.continuous.textLight = color("editor.colors.textLight");
    appearance.colors.focus.inactiveLight = color("editor.colors.inactiveLight");
    appearance.colors.continuous.backgroundDark = color("editor.colors.backgroundDark");
    appearance.colors.continuous.textDark = color("editor.colors.textDark");
    appearance.colors.focus.inactiveDark = color("editor.colors.inactiveDark");

    // Cursor
    appearance.cursor.style = static_cast<editor::CursorStyle>(settings.get<int>("editor.cursor.style"));
    appearance.cursor.useCustomColor = settings.get<bool>("editor.cursor.useCustomColor");
    appearance.cursor.customColor = color("editor.cursor.customColor");
    appearance.cursor.blinking = settings.get<bool>("editor.cursor.blinking");
    appearance.cursor.blinkInterval = settings.get<int>("editor.cursor.blinkInterval");
    appearance.cursor.lineWidth = settings.get<int>("editor.cursor.lineWidth");

    // Page margins (every view: the continuous views are an endless page)
    appearance.pageMargins.top = settings.get<double>("editor.margins.pageTop");
    appearance.pageMargins.bottom = settings.get<double>("editor.margins.pageBottom");
    appearance.pageMargins.left = settings.get<double>("editor.margins.pageLeft");
    appearance.pageMargins.right = settings.get<double>("editor.margins.pageRight");

    // Page format (Page Layout view)
    appearance.pageLayout.pageSize = editor::PageLayout::pageSizeFromId(
        QString::fromStdString(settings.get<std::string>("editor.page.size")));
    appearance.pageLayout.customWidth = settings.get<double>("editor.page.customWidth");
    appearance.pageLayout.customHeight = settings.get<double>("editor.page.customHeight");
    appearance.pageLayout.pageGap = settings.get<int>("editor.page.gap");
    appearance.pageLayout.showPageNumbers = settings.get<bool>("editor.page.showNumbers");

    // Typewriter scrolling (View > Typewriter Scrolling; the height in the settings)
    appearance.typewriter.enabled = settings.get<bool>("editor.typewriter.enabled");
    appearance.typewriter.focusPosition = settings.get<double>("editor.typewriter.focusPosition");
    appearance.typewriter.smoothScroll = settings.get<bool>("editor.typewriter.smoothScroll");

    // Focus (View > Focus)
    appearance.focusMode.enabled = settings.get<bool>("editor.focus.enabled");

    // Text frame border
    appearance.textFrameBorder.show = settings.get<bool>("editor.textFrameBorder.show");
    appearance.textFrameBorder.color = color("editor.textFrameBorder.color");
    appearance.textFrameBorder.width = settings.get<int>("editor.textFrameBorder.width");

    m_bookEditor->setAppearance(appearance);
    applyPaperScale();

    core::Logger::getInstance().debug("EditorPanel: editor settings applied");
}

void EditorPanel::scheduleSettings() {
    // Runs on the thread that changed the setting. The editor takes the settings on the
    // panel's thread, once for all the settings changed before the event loop runs (the
    // Settings dialog changes many at once).
    if (m_settingsPending.exchange(true)) {
        return;
    }
    QMetaObject::invokeMethod(this, "applyScheduledSettings", Qt::QueuedConnection);
}

void EditorPanel::applyScheduledSettings() {
    m_settingsPending = false;
    applySettings();
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
