/// @file editor_panel.cpp
/// @brief Editor panel implementation with BookEditor (OpenSpec #00042 Phase 7.1)

#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/book_styles.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/font_dialog.h"
#include "kalahari/gui/dialogs/hyperlink_dialog.h"
#include "kalahari/gui/dialogs/image_properties_dialog.h"
#include "kalahari/gui/dialogs/insert_table_dialog.h"
#include "kalahari/gui/dialogs/list_dialogs.h"
#include "kalahari/gui/dialogs/paragraph_dialog.h"
#include "kalahari/gui/dialogs/table_dialogs.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/annotation_colors.h"
#include "kalahari/gui/quick_insert_items.h"
#include "kalahari/gui/read_only_book.h"
#include "kalahari/gui/spelling_coordinator.h"
#include "kalahari/gui/utils/language_names.h"
#include "kalahari/gui/utils/program_folders.h"
#include "kalahari/gui/utils/caps_lock.h"
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
#include "kalahari/editor/quick_insert_controller.h"
#include "kalahari/editor/statistics_collector.h"
#include "kalahari/editor/text_images.h"
#include "kalahari/editor/text_link.h"
#include "kalahari/editor/word_repetitions.h"
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QSaveFile>
#include <QGuiApplication>
#include <QMainWindow>
#include <QScreen>
#include <QStatusBar>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <cmath>
#include <nlohmann/json.hpp>
#include <string>

namespace kalahari {
namespace gui {

namespace {

/// @brief The smallest and the largest size of the annotations' marks (1 = 100%)
constexpr double MIN_MARK_SCALE = 0.5;
constexpr double MAX_MARK_SCALE = 3.0;

/// The size of the text of the cards under the paragraphs, in percent of the chapter's text
constexpr int MIN_CARD_FONT = 60;
constexpr int MAX_CARD_FONT = 130;

/// How much of the kind's color the cards have, in percent of the usual
constexpr int MIN_CARD_TINT = 50;
constexpr int MAX_CARD_TINT = 200;

/// @brief The languages with lists of abbreviations of their own in the settings
/// (editor.autoCorrect.abbreviations.pl); another language has the English one
constexpr std::array<const char*, 2> ABBREVIATION_LANGUAGES = {"pl", "en"};

/// @brief The key of the abbreviations of a language
std::string abbreviationsKey(const QString& language) {
    return "editor.autoCorrect.abbreviations." + language.toStdString();
}

/// @brief The texts of a list of the settings
QStringList textsOf(const nlohmann::json& list) {
    QStringList texts;
    if (list.is_array()) {
        for (const nlohmann::json& item : list) {
            if (item.is_string()) {
                texts << QString::fromStdString(item.get<std::string>());
            }
        }
    }
    return texts;
}

/// @brief How long a message of autocorrect stays in the status bar (ms)
constexpr int AUTO_CORRECT_MESSAGE_MS = 5000;

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

    // A style of the book added, changed or removed: the text shows it at once
    connect(&core::ProjectManager::getInstance(), &core::ProjectManager::paragraphStylesChanged,
            this, [this]() { m_bookEditor->setParagraphStyles(paragraphStyles()); });

    // The text's context menu cuts, copies, pastes and selects all with the commands of the
    // Edit menu: their icons and keys, enabled as there
    auto& registry = CommandRegistry::getInstance();
    m_bookEditor->setContextMenuEditActions(
        {registry.getAction(QStringLiteral("edit.cut")),
         registry.getAction(QStringLiteral("edit.copy")),
         registry.getAction(QStringLiteral("edit.paste")),
         registry.getAction(QStringLiteral("edit.selectAll")),
         registry.getAction(QStringLiteral("insert.link"))});
    // The context menu of a cell: the commands of the Table menu, enabled as there
    const auto actions = [&registry](std::initializer_list<const char*> ids) {
        QList<QAction*> found;
        for (const char* id : ids) {
            found << (id != nullptr ? registry.getAction(QString::fromLatin1(id)) : nullptr);
        }
        return found;
    };
    m_bookEditor->setContextMenuTableActions(
        {actions({"table.insertRowsAbove", "table.insertRowsBelow", "table.insertColumnsLeft",
                  "table.insertColumnsRight"}),
         actions({"table.deleteRows", "table.deleteColumns", "table.deleteTable"}),
         actions({"table.selectCell", "table.selectRow", "table.selectColumn", "table.selectTable"}),
         actions({"table.mergeCells", "table.splitCells", nullptr, "table.distributeRows",
                  "table.distributeColumns", nullptr, "table.sort", "table.properties"})});
    // The context menu of an image: the commands of the Image menu, enabled as there
    m_bookEditor->setContextMenuImageActions(
        {actions({"image.properties", "image.addCaption", "image.altText"}),
         actions({"image.rotateRight", "image.rotateLeft", "image.flipHorizontal",
                  "image.flipVertical", nullptr, "image.rotateOptions"}),
         actions({"image.filterNone", "image.filterGrayscale", "image.filterSepia",
                  "image.filterBlackWhite", "image.filterWash", nullptr, "image.colorOptions"}),
         actions({"image.crop", "image.originalSize", nullptr, "image.change", "image.saveAs"})});

    // Autocorrect puts in the quotation marks of the text's language, asked when they are
    // typed: the book's language may change, and a chapter is told from a file outside the
    // book only after its tab is shown
    m_bookEditor->setTextLanguage([this]() { return textLanguage(); });

    // Quick insert: the cards of the book, the fragments and the characters at the cursor;
    // the editor owns it, Insert > Quick Insert... finds it there
    m_quickInsert = new editor::QuickInsertController(
        m_bookEditor, [](QChar trigger) { return quickInsertItems(trigger); }, m_bookEditor);

    // A change of autocorrect undone right after it: as in Word, the exceptions keep the
    // abbreviation or the word, unless the writer turned that off
    connect(m_bookEditor, &editor::BookEditor::autoCorrectionUndone, this,
            &EditorPanel::learnAutoCorrectException);

    // A word typed with Caps Lock on by mistake: the key goes off, where the system lets the
    // program do it, or the writer is told
    connect(m_bookEditor, &editor::BookEditor::capsLockCorrected, this, [this]() {
        showStatusMessage(turnCapsLockOff()
                              ? tr("Caps Lock was on: the word typed with it is corrected, "
                                   "and Caps Lock is off now.")
                              : tr("Caps Lock is on: the word typed with it is corrected. "
                                   "Turn Caps Lock off."));
    });

    // The windows of the lists, asked for by the context menu of a list item
    connect(m_bookEditor, &editor::BookEditor::bulletsAndNumberingAsked, this,
            &EditorPanel::showBulletsAndNumbering);
    connect(m_bookEditor, &editor::BookEditor::numberingValueAsked, this,
            &EditorPanel::showNumberingValue);

    // The window of a link, and its address opened, asked for by its context menu or a
    // Ctrl+click
    connect(m_bookEditor, &editor::BookEditor::linkDialogAsked, this,
            &EditorPanel::showHyperlinkDialog);
    connect(m_bookEditor, &editor::BookEditor::linkOpenAsked, this, &EditorPanel::openLink);

    // The files of an image, asked for by its context menu: another picture, a copy saved
    connect(m_bookEditor, &editor::BookEditor::imageFileAsked, this,
            &EditorPanel::showChangeImageDialog);
    connect(m_bookEditor, &editor::BookEditor::imageSaveAsked, this,
            &EditorPanel::showSaveImageDialog);
    // Its look, asked for by a double click or its context menu
    connect(m_bookEditor, &editor::BookEditor::imagePropertiesAsked, this,
            [this](int position, int page) { showImagePropertiesDialog(position, page); });

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
    // A chapter of the book, which NavigatorCoordinator tags with its element, has the styles
    // of the book; any other text those every book has
    const bool chapter = core::ProjectManager::getInstance().project() != nullptr &&
                         !property("elementId").toString().isEmpty();
    return chapter ? bookParagraphStyles()
                   : editor::ParagraphStyleSet::fromPackage(baseStyles(), programLanguage());
}

void EditorPanel::learnAutoCorrectException(editor::LearnedException list,
                                            const QString& exception) {
    auto& settings = core::SettingsManager::getInstance();
    if (exception.isEmpty() || !settings.get<bool>("editor.autoCorrect.learnExceptions")) {
        return;
    }
    std::string key;
    QString message;
    switch (list) {
        case editor::LearnedException::Abbreviation: {
            // The list of the text's language, the one autocorrect looked at
            const QString language = editor::abbreviationsLanguage(
                textLanguage(), m_bookEditor->autoCorrect().languageAbbreviations);
            if (language.isEmpty()) {
                return;
            }
            key = abbreviationsKey(language);
            //: %1 is an abbreviation with its period, as "godz."
            message = tr("After “%1” auto-correct no longer starts a sentence with a capital: "
                         "it is among the exceptions now.")
                          .arg(exception);
            break;
        }
        case editor::LearnedException::TwoCapitals:
            key = "editor.autoCorrect.twoCapitalsExceptions";
            //: %1 is a word with two capitals at its start, as "PCs"
            message = tr("Auto-correct no longer changes “%1”: it is among the exceptions now.")
                          .arg(exception);
            break;
        case editor::LearnedException::None:
            return;
    }
    nlohmann::json words = settings.get<nlohmann::json>(key);
    if (!words.is_array()) {
        words = nlohmann::json::array();
    }
    const Qt::CaseSensitivity sensitivity = list == editor::LearnedException::Abbreviation
                                                ? Qt::CaseInsensitive
                                                : Qt::CaseSensitive;
    if (textsOf(words).contains(exception, sensitivity)) {
        return;
    }
    words.push_back(exception.toStdString());
    settings.set<nlohmann::json>(key, words);
    showStatusMessage(message);
}

void EditorPanel::showStatusMessage(const QString& message) {
    // The status bar of the main window; a panel outside one (a test) shows nothing
    if (auto* mainWindow = qobject_cast<QMainWindow*>(window())) {
        mainWindow->statusBar()->showMessage(message, AUTO_CORRECT_MESSAGE_MS);
    }
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
    appearance.typography.tabStops =
        settings.get<std::string>("editor.tabStops") == editor::TAB_STOPS_SPACES
            ? editor::TabStops::Spaces
            : editor::TabStops::Indent;
    appearance.typography.tabSpaces = settings.get<int>("editor.tabSize");

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
    appearance.colors.links.light = themes.editorColor(light.link, defaults.links.light);
    appearance.colors.links.dark = themes.editorColor(dark.link, defaults.links.dark);
    auto& tables = appearance.colors.tables;
    tables.linesLight = themes.editorColor(light.tableLines, defaults.tables.linesLight);
    tables.linesDark = themes.editorColor(dark.tableLines, defaults.tables.linesDark);
    tables.headerLight = themes.editorColor(light.tableHeader, defaults.tables.headerLight);
    tables.headerDark = themes.editorColor(dark.tableHeader, defaults.tables.headerDark);
    tables.gridlinesLight = themes.editorColor(light.tableGridlines, defaults.tables.gridlinesLight);
    tables.gridlinesDark = themes.editorColor(dark.tableGridlines, defaults.tables.gridlinesDark);
    auto& images = appearance.colors.images;
    images.cropMarksLight =
        themes.editorColor(light.imageCropMarks, defaults.images.cropMarksLight);
    images.cropMarksDark = themes.editorColor(dark.imageCropMarks, defaults.images.cropMarksDark);
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
    // Mirror margins: the inner one at the binding (the chapter tells whether its first page
    // is a left one: setFirstPageLeft())
    appearance.pageMargins.mirrored = settings.get<bool>("editor.margins.mirrorEnabled");
    appearance.pageMargins.inner = settings.get<double>("editor.margins.pageInner");
    appearance.pageMargins.outer = settings.get<double>("editor.margins.pageOuter");

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

    // The numbers on the left margin (View > Margin Numbering)
    editor::MarginNumbers& numbers = appearance.marginNumbers;
    numbers.numbering = editor::marginNumberingFromName(
        QString::fromStdString(settings.get<std::string>("editor.marginNumbers.mode")));
    numbers.interval = settings.get<int>("editor.marginNumbers.interval");
    numbers.restartEachPage = settings.get<bool>("editor.marginNumbers.restartEachPage");
    numbers.fontFamily =
        QString::fromStdString(settings.get<std::string>("editor.marginNumbers.font"));
    numbers.pointSize = settings.get<double>("editor.marginNumbers.size");
    numbers.color = color("editor.marginNumbers.color");
    appearance.sceneBreakLook = editor::sceneBreakLookFromName(
        QString::fromStdString(settings.get<std::string>("editor.sceneBreak.style")));

    // Text frame border
    appearance.textFrameBorder.show = settings.get<bool>("editor.textFrameBorder.show");
    appearance.textFrameBorder.color = color("editor.textFrameBorder.color");
    appearance.textFrameBorder.width = settings.get<int>("editor.textFrameBorder.width");

    // The annotations' marks (at the size of the setting), the waves under the misspelled
    // words and the grammar issues, and the marks of the repeated words: the theme's colors,
    // on each paper as "Optimize colors in themes" shows them
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
        colors.repetition = onPaper(editor::REPETITION_COLOR_KEY, paper);
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

    // The cards of the annotations under their paragraphs
    const auto text = [&settings](const std::string& key) { return settings.get<std::string>(key); };
    editor::AnnotationCardSettings& cards = appearance.annotationCards;
    cards.show = settings.get<bool>("editor.annotationCards.show");
    cards.comments = settings.get<bool>("editor.annotationCards.comments");
    cards.todos = settings.get<bool>("editor.annotationCards.todos");
    cards.notes = settings.get<bool>("editor.annotationCards.notes");
    cards.compact = text("editor.annotationCards.look") == "compact";
    cards.collapsedLine = text("editor.annotationCards.collapsedLook") == "line";
    cards.startCollapsed = text("editor.annotationCards.afterOpening") == "collapsed";
    cards.longLines = std::max(0, settings.get<int>("editor.annotationCards.longLines"));
    cards.fontPercent = std::clamp(settings.get<int>("editor.annotationCards.fontSize"),
                                   MIN_CARD_FONT, MAX_CARD_FONT);
    cards.tintPercent = std::clamp(settings.get<int>("editor.annotationCards.tint"),
                                   MIN_CARD_TINT, MAX_CARD_TINT);
    cards.highlightFragment = settings.get<bool>("editor.annotationCards.highlightFragment");
    cards.inDistractionFree = text("editor.annotationCards.distractionFree") != "hide";

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
    autoCorrect.dashBetweenWords = replaces("editor.autoCorrect.dashBetweenWords");
    autoCorrect.wordDash =
        settings.get<std::string>("editor.autoCorrect.dashBetweenWordsType") == "emDash"
            ? editor::DialogueDash::Em
            : editor::DialogueDash::En;
    autoCorrect.dialogueDashInDialogue = settings.get<bool>("editor.autoCorrect.dashInDialogue");
    autoCorrect.doubleHyphen = replaces("editor.autoCorrect.doubleHyphen");
    if (replaces("editor.autoCorrect.replacementTable")) {
        // The pairs turned on; a pair without "enabled" is on
        const auto pairs = settings.get<nlohmann::json>("editor.autoCorrect.replacements");
        if (pairs.is_array()) {
            for (const nlohmann::json& pair : pairs) {
                if (!pair.is_object() || !pair.value("enabled", true) ||
                    !pair.value("from", nlohmann::json()).is_string() ||
                    !pair.value("to", nlohmann::json()).is_string()) {
                    continue;
                }
                const QString from =
                    QString::fromStdString(pair.at("from").get<std::string>()).trimmed();
                if (!from.isEmpty()) {
                    autoCorrect.replacements.push_back(
                        {from, QString::fromStdString(pair.at("to").get<std::string>())});
                }
            }
        }
    }
    autoCorrect.sentenceCapitals = replaces("editor.autoCorrect.sentenceCapitals");
    for (const char* language : ABBREVIATION_LANGUAGES) {
        const QString code = QString::fromLatin1(language);
        autoCorrect.languageAbbreviations[code] =
            textsOf(settings.get<nlohmann::json>(abbreviationsKey(code)));
    }
    autoCorrect.twoInitialCapitals = replaces("editor.autoCorrect.twoInitialCapitals");
    autoCorrect.twoCapitalsExceptions =
        textsOf(settings.get<nlohmann::json>("editor.autoCorrect.twoCapitalsExceptions"));
    autoCorrect.capsLock = replaces("editor.autoCorrect.capsLock");
    autoCorrect.sceneBreak = replaces("editor.autoCorrect.sceneBreak");
    autoCorrect.horizontalRule = replaces("editor.autoCorrect.horizontalRule");
    autoCorrect.bulletedLists = replaces("editor.autoCorrect.bulletedLists");
    autoCorrect.numberedLists = replaces("editor.autoCorrect.numberedLists");
    autoCorrect.links = replaces("editor.autoCorrect.links");
    m_bookEditor->setAutoCorrect(autoCorrect);

    // The characters that open quick insert (Settings > Editor > Quick Insert)
    m_quickInsert->setTriggers(quickInsertTriggers());

    // How text from other programs is pasted (Settings > Editor > General)
    m_bookEditor->setPasteMode(editor::ClipboardHandler::pasteModeFromId(
        QString::fromStdString(settings.get<std::string>(editor::PASTE_MODE_KEY))));

    // What the counts take for a word (a setting of the whole program)
    m_bookEditor->setWordCountRules(core::wordCountRules());
    applyPaperScale();

    core::Logger::getInstance().debug("EditorPanel: editor settings applied");
}

void EditorPanel::showBulletsAndNumbering() {
    if (m_bookEditor->isReadOnly()) {
        return;
    }
    const std::optional<editor::ListItem> item = m_bookEditor->currentListItem();
    dialogs::BulletsNumberingDialog dialog(
        item ? item->definition : editor::ListDefinition::numbered(), item ? item->level : 0,
        window());
    if (dialog.exec() == QDialog::Accepted) {
        m_bookEditor->applyListDefinition(dialog.definition());
    }
}

void EditorPanel::showFontDialog() {
    if (m_bookEditor->isReadOnly()) {
        return;
    }
    dialogs::FontDialog dialog(m_bookEditor->shownCharacterFormat(), m_bookEditor->selectedText(),
                               window());
    if (dialog.exec() == QDialog::Accepted) {
        const editor::CharacterFormat changes = dialog.changes();
        if (changes != editor::CharacterFormat{}) {
            m_bookEditor->applyCharacterFormat(changes);
        }
    }
}

void EditorPanel::showParagraphDialog() {
    if (m_bookEditor->isReadOnly()) {
        return;
    }
    dialogs::ParagraphDialog dialog(m_bookEditor->shownParagraphFormat(),
                                    m_bookEditor->currentAlignment(),
                                    m_bookEditor->currentListItem().has_value(), window());
    if (dialog.exec() == QDialog::Accepted) {
        const editor::ParagraphFormat changes = dialog.changes();
        const std::optional<Qt::Alignment> alignment = dialog.alignmentChange();
        if (!changes.isEmpty() || alignment) {
            m_bookEditor->applyParagraphFormat(changes, alignment);
        }
    }
}

void EditorPanel::showHyperlinkDialog() {
    if (m_bookEditor->isReadOnly()) {
        return;
    }
    const editor::LinkTarget target = m_bookEditor->linkTarget();
    dialogs::HyperlinkDialog dialog(window());
    dialog.setLink(target.text, target.address, target.existing, target.textEditable);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    if (dialog.removeAsked()) {
        m_bookEditor->removeLink(target);
    } else {
        m_bookEditor->setLink(target, dialog.text(), dialog.address());
    }
}

void EditorPanel::showInsertTableDialog() {
    if (m_bookEditor->isReadOnly() || m_bookEditor->isInTable()) {
        return;
    }
    dialogs::InsertTableDialog dialog(window());
    if (dialog.exec() == QDialog::Accepted) {
        m_bookEditor->insertTable(dialog.columns(), dialog.rows(), dialog.headerRow());
    }
}

namespace {

/// The picture chosen in the system window of Insert > Image... or Change Image...; empty:
/// none. The window starts in the folder of the picture chosen last.
QString chooseImageFile(QWidget* parent, const QString& title) {
    using Operation = ProgramFolders::Operation;
    const QString path = QFileDialog::getOpenFileName(
        parent, title, ProgramFolders::windowFolder(Operation::InsertImage),
        //: The pictures of the window choosing an image; In Polish: Obrazy
        editor::imageFileFilter(EditorPanel::tr("Images")) + QStringLiteral(";;") +
            EditorPanel::tr("All Files (*)"));
    if (!path.isEmpty()) {
        ProgramFolders::remember(Operation::InsertImage, QFileInfo(path).absolutePath());
    }
    return path;
}

/// Tell the writer that a chosen file could not be the picture of an image
void warnImageNotInserted(QWidget* parent, const QString& path) {
    dialogs::MessageDialog::warning(
        parent,
        //: In Polish: Nie można wstawić obrazu
        EditorPanel::tr("Image Not Inserted"),
        //: In Polish: Plik „%1” nie jest obrazem, który Kalahari potrafi pokazać (PNG, JPEG,
        //: GIF, BMP, SVG), albo nie udało się go skopiować do folderu obrazów książki.
        EditorPanel::tr("The file \"%1\" is not a picture Kalahari can show (PNG, JPEG, GIF, "
                        "BMP, SVG), or it could not be copied into the book's folder of images.")
            .arg(QFileInfo(path).fileName()));
}

}  // namespace

void EditorPanel::setImagesFolder(const QString& folder) {
    m_bookEditor->setImagesFolder(folder);
}

void EditorPanel::moveImagesFolder(const QString& folder) {
    m_bookEditor->moveImagesFolder(folder);
}

void EditorPanel::showInsertImageDialog() {
    if (!m_bookEditor->canInsertImage()) {
        return;
    }
    //: The system window of Insert > Image... In Polish: Wstaw obraz
    const QString path = chooseImageFile(window(), tr("Insert Image"));
    if (!path.isEmpty() && !m_bookEditor->insertImageFile(path)) {
        warnImageNotInserted(window(), path);
    }
}

void EditorPanel::showChangeImageDialog(int position) {
    if (!m_bookEditor->canInsertImage() || m_bookEditor->imageFilePath(position).isEmpty()) {
        return;
    }
    //: The system window of Change Image... In Polish: Zmień obraz
    const QString path = chooseImageFile(window(), tr("Change Image"));
    if (!path.isEmpty() && !m_bookEditor->changeImageFile(position, path)) {
        warnImageNotInserted(window(), path);
    }
}

void EditorPanel::showSaveImageDialog(int position) {
    using Operation = ProgramFolders::Operation;
    const QString source = m_bookEditor->imageFilePath(position);
    const QFileInfo file(source);
    if (!file.isFile()) {
        return;
    }
    //: The files of the window saving an image, %1 its extension (PNG, JPG...). In Polish:
    //: Obraz %1
    const QString filter = tr("%1 Image (*.%2)").arg(file.suffix().toUpper(), file.suffix());
    QString target = QFileDialog::getSaveFileName(
        window(),
        //: The system window of Save as Image... In Polish: Zapisz jako obraz
        tr("Save as Image"),
        QDir(ProgramFolders::windowFolder(Operation::SaveImage)).filePath(file.fileName()),
        filter);
    if (target.isEmpty()) {
        return;
    }
    if (QFileInfo(target).suffix().isEmpty() && !file.suffix().isEmpty()) {
        target += QLatin1Char('.') + file.suffix();
    }
    ProgramFolders::remember(Operation::SaveImage, QFileInfo(target).absolutePath());
    if (QFileInfo(target).absoluteFilePath() == file.absoluteFilePath()) {
        return;  // the file itself
    }
    // The system window asked before replacing a file; the file it replaces stays as it was
    // until the copy is whole
    QFile from(source);
    QSaveFile to(target);
    bool saved = from.open(QIODevice::ReadOnly) && to.open(QIODevice::WriteOnly);
    while (saved && !from.atEnd()) {
        const QByteArray part = from.read(1 << 20);
        saved = !part.isEmpty() && to.write(part) == part.size();
    }
    saved = saved && to.commit();
    if (!saved) {
        dialogs::MessageDialog::warning(
            window(),
            //: In Polish: Nie zapisano obrazu
            tr("Image Not Saved"),
            //: In Polish: Nie udało się zapisać obrazu w pliku „%1”.
            tr("The image could not be saved as \"%1\".").arg(QDir::toNativeSeparators(target)));
    }
}

void EditorPanel::showImagePropertiesDialog(int position, int page) {
    const std::optional<editor::ImagePropertiesData> properties =
        m_bookEditor->imagePropertiesData(position);
    if (m_bookEditor->isReadOnly() || !properties) {
        return;
    }
    // An automatic border has the color of the text, on the editor's paper
    const editor::EditorAppearance& appearance = m_bookEditor->appearance();
    dialogs::ImagePropertiesDialog dialog(
        *properties, appearance.colors.textColor(appearance.colorMode), window());
    // Another picture needs the folder of images: a book saved
    if (m_bookEditor->canInsertImage()) {
        dialog.setFileChooser([&dialog]() {
            //: The system window of Change Image... In Polish: Zmień obraz
            return chooseImageFile(&dialog, tr("Change Image"));
        });
    }
    dialog.setImageSaver([this, position]() { showSaveImageDialog(position); });
    dialog.setCropInTextAvailable(true);
    if (page > 0 && page < dialogs::ImagePropertiesDialog::PageCount) {
        dialog.showPage(static_cast<dialogs::ImagePropertiesDialog::Page>(page));
    }
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const editor::ImagePropertiesData chosen = dialog.data();
    if (!m_bookEditor->setImagePropertiesData(chosen) && !chosen.newFile.isEmpty()) {
        warnImageNotInserted(window(), chosen.newFile);
        return;
    }
    // Crop in the Text: the marks on the image, with the changes made
    if (dialog.cropInTextAsked()) {
        m_bookEditor->setFocus();
        m_bookEditor->startImageCrop(position);
    }
}

void EditorPanel::showSplitCellsDialog() {
    if (m_bookEditor->isReadOnly() || !m_bookEditor->canSplitTableCells()) {
        return;
    }
    const bool cellsSelected = m_bookEditor->selectedCellRange().has_value();
    dialogs::SplitCellsDialog dialog(
        cellsSelected,
        [this](bool mergeFirst) { return m_bookEditor->splitCellParagraphs(mergeFirst); },
        window());
    if (dialog.exec() == QDialog::Accepted) {
        m_bookEditor->splitTableCells(dialog.columns(), dialog.rows(), dialog.mergeFirst(),
                                      dialog.distribute());
    }
}

void EditorPanel::showSortDialog() {
    const std::optional<editor::BookEditor::SortSubject> subject = m_bookEditor->sortSubject();
    if (m_bookEditor->isReadOnly() || !subject) {
        return;
    }
    dialogs::SortDialog dialog(subject->tableRows, subject->columns, subject->header,
                               utils::languageName(textLanguage()), window());
    if (dialog.exec() == QDialog::Accepted) {
        m_bookEditor->sortSelection(dialog.options());
    }
}

void EditorPanel::showTextToTableDialog() {
    const QStringList paragraphs = m_bookEditor->paragraphsForTable();
    if (m_bookEditor->isReadOnly() || paragraphs.isEmpty()) {
        return;
    }
    dialogs::TextToTableDialog dialog(paragraphs, window());
    if (dialog.exec() == QDialog::Accepted) {
        m_bookEditor->convertTextToTable(dialog.separator(), dialog.columns(), dialog.headerRow());
    }
}

void EditorPanel::showTableToTextDialog() {
    const std::vector<QStringList> rows = m_bookEditor->tableTexts();
    if (m_bookEditor->isReadOnly() || rows.empty()) {
        return;
    }
    dialogs::TableToTextDialog dialog(rows, window());
    if (dialog.exec() == QDialog::Accepted) {
        m_bookEditor->convertTableToText(dialog.separator());
    }
}

void EditorPanel::showTablePropertiesDialog() {
    const std::optional<editor::TablePropertiesData> properties =
        m_bookEditor->tablePropertiesData();
    if (m_bookEditor->isReadOnly() || !properties) {
        return;
    }
    // The lines without a color of their own have the editor's, on its paper
    const editor::EditorAppearance& appearance = m_bookEditor->appearance();
    const QColor lines = appearance.colorMode == editor::EditorColorMode::Light
                             ? appearance.colors.tables.linesLight
                             : appearance.colors.tables.linesDark;
    dialogs::TablePropertiesDialog dialog(*properties, lines, window());
    if (dialog.exec() == QDialog::Accepted) {
        m_bookEditor->setTablePropertiesData(dialog.data());
    }
}

void EditorPanel::openLink(const QString& address) {
    // Only a page, a file server or a new e-mail: another address (file:, a script) could run
    // a program on the computer
    if (!editor::isOpenableLinkAddress(address)) {
        showStatusMessage(
            tr("Kalahari opens only the addresses of web pages and e-mail addresses: %1")
                .arg(address));
        return;
    }
    if (!QDesktopServices::openUrl(QUrl(address.trimmed()))) {
        showStatusMessage(tr("The address could not be opened: %1").arg(address));
    }
}

void EditorPanel::showNumberingValue() {
    const std::optional<editor::ListItem> item = m_bookEditor->currentListItem();
    if (!item || m_bookEditor->isReadOnly()) {
        return;
    }
    const int number = m_bookEditor->currentListNumber();
    dialogs::NumberingValueDialog dialog(item->levelFormat(),
                                         number >= 0 ? number : item->levelFormat().start,
                                         window());
    if (dialog.exec() == QDialog::Accepted) {
        m_bookEditor->setNumberingValue(dialog.value());
    }
}

void EditorPanel::setFirstPageLeft(bool left) {
    // The editor's appearance keeps it: applySettings() starts from it
    editor::EditorAppearance appearance = m_bookEditor->appearance();
    if (appearance.pageMargins.firstPageLeft == left) {
        return;
    }
    appearance.pageMargins.firstPageLeft = left;
    m_bookEditor->setAppearance(appearance);
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
