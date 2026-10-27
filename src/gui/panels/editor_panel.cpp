/// @file editor_panel.cpp
/// @brief Editor panel implementation with BookEditor (OpenSpec #00042 Phase 7.1)

#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/annotation_colors.h"
#include "kalahari/gui/read_only_book.h"
#include "kalahari/gui/spelling_coordinator.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/kind_words.h"
#include "kalahari/core/text_statistics.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/annotation.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/clipboard_handler.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/editor/paragraph_styles.h"
#include "kalahari/editor/statistics_collector.h"
#include <QEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace kalahari {
namespace gui {

namespace {

/// @brief The smallest and the largest size of the annotations' marks (1 = 100%)
constexpr double MIN_MARK_SCALE = 0.5;
constexpr double MAX_MARK_SCALE = 3.0;

}  // namespace

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
    connect(m_bookEditor, &editor::BookEditor::modifiedChanged, this,
            &EditorPanel::modifiedChanged);

    // The text's context menu cuts, copies, pastes and selects all with the commands of the
    // Edit menu: their icons and keys, enabled as there
    auto& registry = CommandRegistry::getInstance();
    m_bookEditor->setContextMenuEditActions(
        {registry.getAction(QStringLiteral("edit.cut")),
         registry.getAction(QStringLiteral("edit.copy")),
         registry.getAction(QStringLiteral("edit.paste")),
         registry.getAction(QStringLiteral("edit.selectAll"))});

    // Autocorrect puts in the quotation marks of the text's language, asked when they are
    // typed: the book's language may change, and a chapter is told from a file outside the
    // book only after its tab is shown
    m_bookEditor->setTextLanguage([this]() { return textLanguage(); });

    // F7 in the menu of an issue goes on to the next one, as in the text
    m_bookEditor->setNextIssueAction(registry.getAction(QStringLiteral("tools.nextMisspelling")));

    // A chapter of a book open read-only: its context menu says why the fixes are greyed out
    m_bookEditor->setReadOnlyReason(readOnlyReason());

    // The light or dark paper chosen in the editor's context menu is the setting of all
    // editors, kept between sessions
    connect(m_bookEditor, &editor::BookEditor::editorColorModeChanged, this,
            [](editor::EditorColorMode mode) {
                core::SettingsManager::getInstance().set<bool>(
                    "editor.darkMode", mode == editor::EditorColorMode::Dark);
            });

    setLayout(layout);

    // A screen's display scaling or resolution changed while the program runs: the pages
    // keep their size on paper (every screen is watched, as the panel can move to any)
    const auto followScreen = [this](QScreen* screen) {
        connect(screen, &QScreen::physicalDotsPerInchChanged, this,
                &EditorPanel::applyPaperScale);
        connect(screen, &QScreen::logicalDotsPerInchChanged, this,
                &EditorPanel::applyPaperScale);
    };
    const QList<QScreen*> screens = QGuiApplication::screens();
    for (QScreen* screen : screens) {
        followScreen(screen);
    }
    connect(qGuiApp, &QGuiApplication::screenAdded, this, followScreen);

    // A changed editor setting reaches the editor, wherever it was changed: the Settings
    // dialog, the View menu or the context menu of another editor
    m_settingsListener = core::SettingsManager::getInstance().subscribe(
        [this](const std::string& key) {
            if (key.rfind("editor.", 0) == 0) {
                scheduleSettings();
            }
        });

    // The theme gives the annotations' marks and the waves their colors
    auto& themes = core::ThemeManager::getInstance();
    connect(&themes, &core::ThemeManager::themeChanged, this, [this]() { scheduleSettings(); });
    connect(&themes, &core::ThemeManager::editorColorsChanged, this,
            [this]() { scheduleSettings(); });

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
    m_bookEditor->setParagraphStyles(paragraphStyles());
    m_bookEditor->fromKml(kml);
    logger.debug("EditorPanel::setText - BookEditor::fromKml() complete");
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
    m_bookEditor->setParagraphStyles(paragraphStyles());
    const bool complete = m_bookEditor->fromKml(content);
    logger.debug("EditorPanel::setContent - BookEditor::fromKml() complete");
    return complete;
}

void EditorPanel::warnDamagedChapter(QWidget* parent, const QString& name,
                                     const core::KindWords* kind) {
    if (!kind) {
        dialogs::MessageDialog::warning(
            parent, tr("Damaged File"),
            tr("The file \"%1\" is damaged, so only its text before the damaged place is "
               "shown.\n\nSaving the file keeps only this text. Close it without saving to "
               "leave the file as it is.")
                .arg(name));
        return;
    }
    // "The story ... is damaged": the kind of the element in the program's language
    const QHash<QString, core::KindWords> nouns{{QStringLiteral("kind"), *kind}};
    //: In Polish: {kind:m=Uszkodzony|f=Uszkodzona|n=Uszkodzone|p=Uszkodzone} {kind}
    const QString title = core::fillWords(tr("Damaged {Kind}"), nouns);
    //: In Polish: {Kind} „%1” {kind:m=jest uszkodzony|f=jest uszkodzona|n=jest
    //: uszkodzone|p=są uszkodzone}, dlatego widać tylko tekst sprzed uszkodzonego miejsca.
    //: Zapisanie {kind:genitive} zachowa tylko ten tekst. Aby pozostawić plik bez zmian,
    //: zamknij {kind:accusative} bez zapisywania.
    const QString text = core::fillWords(
        tr("The {kind} \"%1\" {kind:s=is|p=are} damaged, so only {kind:s=its|p=their} text "
           "before the damaged place is shown.\n\nSaving the {kind} keeps only this text. "
           "Close {kind:s=it|p=them} without saving to leave the file as it is."),
        nouns);
    dialogs::MessageDialog::warning(parent, title, text.arg(name));
}

QString EditorPanel::getContent() const {
    // Phase 11: Use BookEditor::toKml() - QTextDocument-based architecture
    if (m_bookEditor) {
        return m_bookEditor->toKml();
    }
    return QString();
}

bool EditorPanel::isModified() const {
    return m_bookEditor && m_bookEditor->isModified();
}

void EditorPanel::setUnmodified() {
    if (m_bookEditor) {
        m_bookEditor->setUnmodified();
    }
}

editor::ParagraphStyleSet EditorPanel::paragraphStyles() const {
    const auto& projects = core::ProjectManager::getInstance();
    const core::BookProject* book = projects.project();
    // A chapter of the book, which NavigatorCoordinator tags with its element
    const bool chapter = book != nullptr && !property("elementId").toString().isEmpty();
    const QString base = QString::fromLatin1(core::ProjectManager::BASE_PACKAGE);
    QList<core::PackageParagraphStyle> styles;
    if (chapter && book->type.has_value()) {
        styles = projects.bookTypes().paragraphStyles(book->type->id);
    }
    // A type the program does not have gives none: the styles every book has
    if (styles.isEmpty()) {
        styles = projects.bookTypes().paragraphStyles(base);
    }
    return editor::ParagraphStyleSet::fromPackage(styles, programLanguage());
}

QString EditorPanel::textLanguage() const {
    // A chapter of the book, which NavigatorCoordinator tags with its element, or a file
    // outside the book
    const bool chapter = !property("elementId").toString().isEmpty();
    return chapter ? SpellingCoordinator::wantedLanguage() : SpellingCoordinator::defaultLanguage();
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

    // The paper (light or dark, independent from the application's theme) and the colors of
    // both papers: the theme's (the editor's own where the theme has none)
    appearance.colorMode = settings.get<bool>("editor.darkMode")
        ? editor::EditorColorMode::Dark
        : editor::EditorColorMode::Light;
    const auto& themes = core::ThemeManager::getInstance();
    const editor::EditorColors defaults;
    const editor::PaperColorKeys& light = editor::LIGHT_PAPER_COLOR_KEYS;
    const editor::PaperColorKeys& dark = editor::DARK_PAPER_COLOR_KEYS;
    auto& papers = appearance.colors.continuous;
    papers.backgroundLight = themes.editorColor(light.paper, defaults.continuous.backgroundLight);
    papers.textLight = themes.editorColor(light.text, defaults.continuous.textLight);
    papers.deskLight = themes.editorColor(light.desk, defaults.continuous.deskLight);
    papers.backgroundDark = themes.editorColor(dark.paper, defaults.continuous.backgroundDark);
    papers.textDark = themes.editorColor(dark.text, defaults.continuous.textDark);
    papers.deskDark = themes.editorColor(dark.desk, defaults.continuous.deskDark);
    appearance.colors.focus.inactiveLight =
        themes.editorColor(light.focusDimmed, defaults.focus.inactiveLight);
    appearance.colors.focus.inactiveDark =
        themes.editorColor(dark.focusDimmed, defaults.focus.inactiveDark);
    appearance.colors.formattingMarks.light =
        themes.editorColor(light.formattingMarks, defaults.formattingMarks.light);
    appearance.colors.formattingMarks.dark =
        themes.editorColor(dark.formattingMarks, defaults.formattingMarks.dark);
    // Under the text on both papers: translucent, so the text stays readable
    appearance.colors.selection =
        editor::underTextColor(themes.editorColor(editor::SELECTION_COLOR_KEY, defaults.selection),
                               editor::SELECTION_OPACITY);
    appearance.colors.searchMatch = editor::underTextColor(
        themes.editorColor(editor::SEARCH_MATCH_COLOR_KEY, defaults.searchMatch),
        editor::SEARCH_MATCH_OPACITY);
    appearance.colors.currentSearchMatch = editor::underTextColor(
        themes.editorColor(editor::CURRENT_SEARCH_MATCH_COLOR_KEY, defaults.currentSearchMatch),
        editor::CURRENT_SEARCH_MATCH_OPACITY);

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

    // The formatting marks (View > Show Formatting Marks) and how the scene breaks look
    appearance.showFormattingMarks = settings.get<bool>("editor.showFormattingMarks");
    appearance.sceneBreakLook = editor::sceneBreakLookFromName(
        QString::fromStdString(settings.get<std::string>("editor.sceneBreak.style")));

    // Text frame border
    appearance.textFrameBorder.show = settings.get<bool>("editor.textFrameBorder.show");
    appearance.textFrameBorder.color = color("editor.textFrameBorder.color");
    appearance.textFrameBorder.width = settings.get<int>("editor.textFrameBorder.width");

    // The annotations' marks (at the size of the setting) and the waves under the misspelled
    // words and the grammar issues: the theme's colors, on each paper as "Optimize colors in
    // themes" shows them
    const bool optimize = themes.optimizesEditorColors();
    const auto onPaper = [&themes, optimize](const std::string& key, const QColor& paper) {
        return shownColor(themes.editorColor(key, QColor()), paper, optimize);
    };
    const auto markColors = [&onPaper](const QColor& paper) {
        editor::EditorColors::AnnotationColors colors;
        colors.comment = onPaper(editor::annotationColorKey(editor::AnnotationKind::Comment), paper);
        colors.todo = onPaper(editor::annotationColorKey(editor::AnnotationKind::Todo), paper);
        colors.note = onPaper(editor::annotationColorKey(editor::AnnotationKind::Note), paper);
        return colors;
    };
    const auto waveColors = [&onPaper](const QColor& paper) {
        editor::EditorColors::IssueColors colors;
        colors.spelling = onPaper(editor::issueColorKey(editor::TextIssueKind::Spelling), paper);
        colors.grammar = onPaper(editor::issueColorKey(editor::TextIssueKind::Grammar), paper);
        return colors;
    };
    const QColor lightPaper = appearance.colors.continuous.backgroundLight;
    const QColor darkPaper = appearance.colors.continuous.backgroundDark;
    appearance.colors.annotationsLight = markColors(lightPaper);
    appearance.colors.annotationsDark = markColors(darkPaper);
    appearance.colors.issuesLight = waveColors(lightPaper);
    appearance.colors.issuesDark = waveColors(darkPaper);
    appearance.annotationMarkScale =
        std::clamp(settings.get<int>("editor.annotationMarkSize") / 100.0, MIN_MARK_SCALE,
                   MAX_MARK_SCALE);

    m_bookEditor->setAppearance(appearance);

    // Autocorrect: what is changed in the text as it is typed; nothing while it is off as a
    // whole (Tools > Auto-correct as You Type), each replacement keeping its choice
    const bool autoCorrecting = settings.get<bool>("editor.autoCorrect.enabled");
    const auto replaces = [&settings, autoCorrecting](const char* key) {
        return autoCorrecting && settings.get<bool>(key);
    };
    editor::AutoCorrectOptions autoCorrect;
    autoCorrect.quotes = replaces("editor.autoCorrect.quotes");
    autoCorrect.apostrophe = replaces("editor.autoCorrect.apostrophe");
    autoCorrect.dialogueDash = replaces("editor.autoCorrect.dialogueDash");
    autoCorrect.dash = settings.get<std::string>("editor.autoCorrect.dialogueDashType") == "enDash"
                           ? editor::DialogueDash::En
                           : editor::DialogueDash::Em;
    autoCorrect.ellipsis = replaces("editor.autoCorrect.ellipsis");
    m_bookEditor->setAutoCorrect(autoCorrect);

    // How text from other programs is pasted (Settings > Editor > General)
    m_bookEditor->setPasteMode(editor::ClipboardHandler::pasteModeFromId(
        QString::fromStdString(settings.get<std::string>(editor::PASTE_MODE_KEY))));

    // What the counts take for a word (a setting of the whole program)
    m_bookEditor->setWordCountRules(core::wordCountRules());
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
    // Shown, the tab is the one in front: the words written in it count to the session
    if (event->type() == QEvent::Show) {
        trackStatistics();
    }
    return QWidget::event(event);
}

void EditorPanel::setStatisticsCollector(editor::StatisticsCollector* collector) {
    m_statisticsCollector = collector;
    trackStatistics();
}

void EditorPanel::trackStatistics() {
    // The collector follows the chapter in front; a tab behind the others keeps it on the
    // chapter it follows until it is shown
    if (m_statisticsCollector && m_bookEditor && isVisible()) {
        m_statisticsCollector->setBookEditor(m_bookEditor);
        core::Logger::getInstance().debug("EditorPanel: StatisticsCollector follows this editor");
    }
}

} // namespace gui
} // namespace kalahari
