/// @file main_window.cpp
/// @brief Main application window implementation

#include "kalahari/gui/main_window.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/dialogs/about_dialog.h"
#include "kalahari/gui/dialogs/add_to_project_dialog.h"
#include "kalahari/gui/dialogs/icon_downloader_dialog.h"
#include "kalahari/gui/dialogs/new_item_dialog.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/menu_builder.h"
#include "kalahari/gui/toolbar_builder.h"
#include "kalahari/gui/panels/dashboard_panel.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/panels/search_panel.h"
#include "kalahari/gui/panels/assistant_panel.h"
#include "kalahari/gui/panels/log_panel.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/log_panel_sink.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/icon_registry.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/core/theme.h"
#include "kalahari/core/document.h"
#include "kalahari/core/document_archive.h"
#include "kalahari/core/book.h"
#include "kalahari/core/book_element.h"
#include "kalahari/core/part.h"
#include "kalahari/core/recent_books_manager.h"
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QApplication>
#include <QSettings>
#include <QDockWidget>
#include <QCloseEvent>
#include <QShowEvent>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QDir>
#include <QTextEdit>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolButton>
#include <QStyle>
#include <QProgressDialog>

namespace kalahari {
namespace gui {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_fileMenu(nullptr)
    , m_editMenu(nullptr)
    , m_viewMenu(nullptr)
    , m_toolbarManager(nullptr)
    , m_menuBuilder(nullptr)
    , m_viewNavigatorAction(nullptr)
    , m_viewPropertiesAction(nullptr)
    , m_viewLogAction(nullptr)
    , m_viewSearchAction(nullptr)
    , m_viewAssistantAction(nullptr)
    , m_navigatorDock(nullptr)
    , m_propertiesDock(nullptr)
    , m_logDock(nullptr)
    , m_searchDock(nullptr)
    , m_assistantDock(nullptr)
    , m_centralTabs(nullptr)
    , m_dashboardPanel(nullptr)
    , m_editorPanel(nullptr)
    , m_navigatorPanel(nullptr)
    , m_propertiesPanel(nullptr)
    , m_searchPanel(nullptr)
    , m_assistantPanel(nullptr)
    , m_logPanel(nullptr)
    , m_firstShow(true)
    , m_diagnosticMode(false)
    , m_diagnosticMenu(nullptr)
    , m_devMode(false)
    , m_devToolsMenu(nullptr)
    , m_currentDocument(std::nullopt)
    , m_currentFilePath("")
    , m_isDirty(false)
    , m_currentElementId()
    , m_standaloneInfoBar(nullptr)
    , m_centralWrapper(nullptr)
{
    auto& logger = core::Logger::getInstance();
    logger.info("MainWindow constructor called");

    // Set window properties
    setWindowTitle("Kalahari Writer's IDE");
    resize(1280, 720);

    // Create UI components (Command Registry pattern)
    registerCommands();    // Register all commands with CommandRegistry
    createMenus();         // Build menus from CommandRegistry
    createToolbars();      // Build toolbars from CommandRegistry
    createStatusBar();
    createDocks();

    // NOTE (Task #00015): EditorPanel textChanged signal connected when tab created
    // No m_editorPanel at startup - Dashboard is default first tab

    // Connect ThemeManager to MainWindow (Task #00023)
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged,
            this, &MainWindow::onThemeChanged);
    logger.debug("MainWindow: Connected ThemeManager::themeChanged signal to MainWindow");

    // Note: ThemeManager->IconRegistry connection is handled internally by ArtProvider
    // ArtProvider::initialize() connects ThemeManager::themeChanged to IconRegistry::onThemeChanged
    // and synchronizes colors from the current theme

    // Connect ProjectManager signals (OpenSpec #00033 Phase D)
    auto& pm = core::ProjectManager::getInstance();
    connect(&pm, &core::ProjectManager::projectOpened,
            this, &MainWindow::onProjectOpened);
    connect(&pm, &core::ProjectManager::projectClosed,
            this, &MainWindow::onProjectClosed);
    logger.debug("MainWindow: Connected ProjectManager signals");

    logger.info("MainWindow initialized successfully");
}

void MainWindow::registerCommands() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Registering commands with CommandRegistry");

    CommandRegistry& registry = CommandRegistry::getInstance();
    int count = 0;

    #include "register_commands.hpp"

    // =========================================================================
    // ICON REGISTRY - Register default SVG icons (Task #00021)
    // =========================================================================
    auto& iconRegistry = core::IconRegistry::getInstance();

    // -------------------------------------------------------------------------
    // FILE MENU ICONS (Material Design twotone style)
    // -------------------------------------------------------------------------
    iconRegistry.registerIcon("file.new", "resources/icons/twotone/note_add.svg", "New File");
    iconRegistry.registerIcon("file.new.project", "resources/icons/twotone/create_new_folder.svg", "New Book");
    iconRegistry.registerIcon("file.open", "resources/icons/twotone/folder_open.svg", "Open File");
    iconRegistry.registerIcon("file.close", "resources/icons/twotone/close.svg", "Close File");
    iconRegistry.registerIcon("file.save", "resources/icons/twotone/save.svg", "Save");
    iconRegistry.registerIcon("file.saveAs", "resources/icons/twotone/save_as.svg", "Save As");
    iconRegistry.registerIcon("file.saveAll", "resources/icons/twotone/layers.svg", "Save All");
    iconRegistry.registerIcon("file.exit", "resources/icons/twotone/logout.svg", "Exit");
    iconRegistry.registerIcon("file.open.file", "resources/icons/twotone/description.svg", "Open File");
    iconRegistry.registerIcon("file.import.docx", "resources/icons/twotone/upload_file.svg", "Import DOCX");
    iconRegistry.registerIcon("file.import.pdf", "resources/icons/twotone/picture_as_pdf.svg", "Import PDF");
    iconRegistry.registerIcon("file.import.text", "resources/icons/twotone/text_snippet.svg", "Import Text");
    iconRegistry.registerIcon("file.import.scrivener", "resources/icons/twotone/integration_instructions.svg", "Import Scrivener");
    iconRegistry.registerIcon("file.export.docx", "resources/icons/twotone/download.svg", "Export DOCX");
    iconRegistry.registerIcon("file.export.pdf", "resources/icons/twotone/picture_as_pdf.svg", "Export PDF");
    iconRegistry.registerIcon("file.export.markdown", "resources/icons/twotone/code.svg", "Export Markdown");
    iconRegistry.registerIcon("file.export.epub", "resources/icons/twotone/auto_stories.svg", "Export EPUB");
    iconRegistry.registerIcon("file.export.mobi", "resources/icons/twotone/smartphone.svg", "Export MOBI");
    iconRegistry.registerIcon("file.export.latex", "resources/icons/twotone/functions.svg", "Export LaTeX");

    // -------------------------------------------------------------------------
    // EDIT MENU ICONS
    // -------------------------------------------------------------------------
    iconRegistry.registerIcon("edit.undo", "resources/icons/twotone/undo.svg", "Undo");
    iconRegistry.registerIcon("edit.redo", "resources/icons/twotone/redo.svg", "Redo");
    iconRegistry.registerIcon("edit.cut", "resources/icons/twotone/content_cut.svg", "Cut");
    iconRegistry.registerIcon("edit.copy", "resources/icons/twotone/content_copy.svg", "Copy");
    iconRegistry.registerIcon("edit.paste", "resources/icons/twotone/content_paste.svg", "Paste");
    iconRegistry.registerIcon("edit.pasteSpecial", "resources/icons/twotone/content_paste_go.svg", "Paste Special");
    iconRegistry.registerIcon("edit.delete", "resources/icons/twotone/delete.svg", "Delete");
    iconRegistry.registerIcon("edit.selectAll", "resources/icons/twotone/select_all.svg", "Select All");
    iconRegistry.registerIcon("edit.selectWord", "resources/icons/twotone/highlight_alt.svg", "Select Word");
    iconRegistry.registerIcon("edit.selectParagraph", "resources/icons/twotone/segment.svg", "Select Paragraph");
    iconRegistry.registerIcon("edit.find", "resources/icons/twotone/search.svg", "Find");
    iconRegistry.registerIcon("edit.findNext", "resources/icons/twotone/navigate_next.svg", "Find Next");
    iconRegistry.registerIcon("edit.findPrevious", "resources/icons/twotone/navigate_before.svg", "Find Previous");
    iconRegistry.registerIcon("edit.findReplace", "resources/icons/twotone/find_replace.svg", "Find & Replace");
    iconRegistry.registerIcon("edit.preferences", "resources/icons/twotone/settings.svg", "Preferences");
    iconRegistry.registerIcon("edit.settings", "resources/icons/twotone/settings.svg", "Settings");

    // -------------------------------------------------------------------------
    // BOOK MENU ICONS
    // -------------------------------------------------------------------------
    iconRegistry.registerIcon("book.newChapter", "resources/icons/twotone/article.svg", "New Chapter");
    iconRegistry.registerIcon("book.newScene", "resources/icons/twotone/theaters.svg", "New Scene");
    iconRegistry.registerIcon("book.newCharacter", "resources/icons/twotone/person.svg", "New Character");
    iconRegistry.registerIcon("book.newLocation", "resources/icons/twotone/place.svg", "New Location");
    iconRegistry.registerIcon("book.newItem", "resources/icons/twotone/add_box.svg", "New Item");
    iconRegistry.registerIcon("book.newMindMap", "resources/icons/twotone/account_tree.svg", "New Mind Map");
    iconRegistry.registerIcon("book.newTimeline", "resources/icons/twotone/timeline.svg", "New Timeline");
    iconRegistry.registerIcon("book.chapterBreak", "resources/icons/twotone/horizontal_rule.svg", "Chapter Break");
    iconRegistry.registerIcon("book.sceneBreak", "resources/icons/twotone/more_horiz.svg", "Scene Break");
    iconRegistry.registerIcon("book.properties", "resources/icons/twotone/tune.svg", "Book Properties");

    // -------------------------------------------------------------------------
    // INSERT MENU ICONS
    // -------------------------------------------------------------------------
    iconRegistry.registerIcon("insert.image", "resources/icons/twotone/add_photo_alternate.svg", "Insert Image");
    iconRegistry.registerIcon("insert.table", "resources/icons/twotone/table_chart.svg", "Insert Table");
    iconRegistry.registerIcon("insert.link", "resources/icons/twotone/link.svg", "Insert Link");
    iconRegistry.registerIcon("insert.footnote", "resources/icons/twotone/short_text.svg", "Insert Footnote");
    iconRegistry.registerIcon("insert.endnote", "resources/icons/twotone/notes.svg", "Insert Endnote");
    iconRegistry.registerIcon("insert.comment", "resources/icons/twotone/add_comment.svg", "Insert Comment");
    iconRegistry.registerIcon("insert.annotation", "resources/icons/twotone/edit_note.svg", "Insert Annotation");
    iconRegistry.registerIcon("insert.specialChar", "resources/icons/twotone/emoji_symbols.svg", "Insert Special Character");
    iconRegistry.registerIcon("insert.dateTime", "resources/icons/twotone/schedule.svg", "Insert Date/Time");
    iconRegistry.registerIcon("insert.field", "resources/icons/twotone/data_object.svg", "Insert Field");

    // -------------------------------------------------------------------------
    // FORMAT MENU ICONS
    // -------------------------------------------------------------------------
    iconRegistry.registerIcon("format.font", "resources/icons/twotone/text_format.svg", "Font");
    iconRegistry.registerIcon("format.style.heading1", "resources/icons/twotone/looks_one.svg", "Heading 1");
    iconRegistry.registerIcon("format.style.heading2", "resources/icons/twotone/looks_two.svg", "Heading 2");
    iconRegistry.registerIcon("format.style.heading3", "resources/icons/twotone/looks_3.svg", "Heading 3");
    iconRegistry.registerIcon("format.style.body", "resources/icons/twotone/subject.svg", "Body Text");
    iconRegistry.registerIcon("format.style.quote", "resources/icons/twotone/format_quote.svg", "Block Quote");
    iconRegistry.registerIcon("format.style.code", "resources/icons/twotone/code.svg", "Code Block");
    iconRegistry.registerIcon("format.style.manage", "resources/icons/twotone/style.svg", "Manage Styles");
    iconRegistry.registerIcon("format.bold", "resources/icons/twotone/format_bold.svg", "Bold");
    iconRegistry.registerIcon("format.italic", "resources/icons/twotone/format_italic.svg", "Italic");
    iconRegistry.registerIcon("format.underline", "resources/icons/twotone/format_underlined.svg", "Underline");
    iconRegistry.registerIcon("format.strikethrough", "resources/icons/twotone/strikethrough_s.svg", "Strikethrough");
    iconRegistry.registerIcon("format.alignLeft", "resources/icons/twotone/format_align_left.svg", "Align Left");
    iconRegistry.registerIcon("format.alignCenter", "resources/icons/twotone/format_align_center.svg", "Align Center");
    iconRegistry.registerIcon("format.alignRight", "resources/icons/twotone/format_align_right.svg", "Align Right");
    iconRegistry.registerIcon("format.justify", "resources/icons/twotone/format_align_justify.svg", "Justify");
    iconRegistry.registerIcon("format.increaseIndent", "resources/icons/twotone/format_indent_increase.svg", "Increase Indent");
    iconRegistry.registerIcon("format.decreaseIndent", "resources/icons/twotone/format_indent_decrease.svg", "Decrease Indent");
    iconRegistry.registerIcon("format.bullets", "resources/icons/twotone/format_list_bulleted.svg", "Bullets");
    iconRegistry.registerIcon("format.numbering", "resources/icons/twotone/format_list_numbered.svg", "Numbering");
    iconRegistry.registerIcon("format.color", "resources/icons/twotone/format_color_text.svg", "Text Color");
    iconRegistry.registerIcon("format.clearFormatting", "resources/icons/twotone/format_clear.svg", "Clear Formatting");
    iconRegistry.registerIcon("format.paragraph", "resources/icons/twotone/notes.svg", "Paragraph");

    // -------------------------------------------------------------------------
    // TOOLS MENU ICONS
    // -------------------------------------------------------------------------
    iconRegistry.registerIcon("tools.stats.full", "resources/icons/twotone/analytics.svg", "Full Statistics");
    iconRegistry.registerIcon("tools.stats.wordCount", "resources/icons/twotone/label.svg", "Word Count");
    iconRegistry.registerIcon("tools.spellcheck", "resources/icons/twotone/spellcheck.svg", "Spellcheck");
    iconRegistry.registerIcon("tools.grammar", "resources/icons/twotone/grading.svg", "Grammar Check");
    iconRegistry.registerIcon("tools.focus.normal", "resources/icons/twotone/visibility.svg", "Normal Mode");
    iconRegistry.registerIcon("tools.focus.focused", "resources/icons/twotone/center_focus_strong.svg", "Focus Mode");
    iconRegistry.registerIcon("tools.focus.distractionFree", "resources/icons/twotone/fullscreen.svg", "Distraction Free");
    iconRegistry.registerIcon("tools.backupNow", "resources/icons/twotone/backup.svg", "Backup Now");
    iconRegistry.registerIcon("tools.autoSaveSettings", "resources/icons/twotone/sync.svg", "Auto-Save Settings");
    iconRegistry.registerIcon("tools.versionHistory", "resources/icons/twotone/history.svg", "Version History");
    iconRegistry.registerIcon("tools.plugins.manager", "resources/icons/twotone/extension.svg", "Plugin Manager");
    iconRegistry.registerIcon("tools.plugins.marketplace", "resources/icons/twotone/storefront.svg", "Marketplace");
    iconRegistry.registerIcon("tools.plugins.updates", "resources/icons/twotone/system_update.svg", "Plugin Updates");
    iconRegistry.registerIcon("tools.plugins.reload", "resources/icons/twotone/refresh.svg", "Reload Plugins");
    iconRegistry.registerIcon("tools.challenges", "resources/icons/twotone/emoji_events.svg", "Writing Challenges");
    iconRegistry.registerIcon("tools.writingGoals", "resources/icons/twotone/track_changes.svg", "Writing Goals");
    iconRegistry.registerIcon("tools.cloudSync", "resources/icons/twotone/cloud_sync.svg", "Cloud Sync");
    iconRegistry.registerIcon("tools.collaboration", "resources/icons/twotone/groups.svg", "Collaboration");
    iconRegistry.registerIcon("tools.readability", "resources/icons/twotone/auto_stories.svg", "Readability Analysis");

    // -------------------------------------------------------------------------
    // ASSISTANT MENU ICONS
    // -------------------------------------------------------------------------
    iconRegistry.registerIcon("assistant", "resources/icons/twotone/pets.svg", "AI Assistant");
    iconRegistry.registerIcon("assistant.ask", "resources/icons/twotone/psychology.svg", "Ask Assistant");
    iconRegistry.registerIcon("assistant.switch", "resources/icons/twotone/swap_horiz.svg", "Switch Assistant");
    iconRegistry.registerIcon("assistant.action.style", "resources/icons/twotone/brush.svg", "Style Suggestions");
    iconRegistry.registerIcon("assistant.action.plot", "resources/icons/twotone/auto_graph.svg", "Plot Analysis");
    iconRegistry.registerIcon("assistant.action.research", "resources/icons/twotone/science.svg", "Research Help");
    iconRegistry.registerIcon("assistant.action.speedDraft", "resources/icons/twotone/flash_on.svg", "Speed Draft");
    iconRegistry.registerIcon("assistant.action.grammar", "resources/icons/twotone/fact_check.svg", "Grammar Assistant");
    iconRegistry.registerIcon("assistant.settings", "resources/icons/twotone/settings.svg", "Assistant Settings");

    // -------------------------------------------------------------------------
    // VIEW MENU ICONS
    // -------------------------------------------------------------------------
    iconRegistry.registerIcon("view.navigator", "resources/icons/twotone/import_contacts.svg", "Navigator");
    iconRegistry.registerIcon("view.search", "resources/icons/twotone/search.svg", "Search Panel");
    iconRegistry.registerIcon("view.properties", "resources/icons/twotone/tune.svg", "Properties");
    iconRegistry.registerIcon("view.assistant", "resources/icons/twotone/pets.svg", "Assistant");
    iconRegistry.registerIcon("view.log", "resources/icons/twotone/description.svg", "Log");
    iconRegistry.registerIcon("view.perspectives.writer", "resources/icons/twotone/edit.svg", "Writer Perspective");
    iconRegistry.registerIcon("view.perspectives.editor", "resources/icons/twotone/rate_review.svg", "Editor Perspective");
    iconRegistry.registerIcon("view.perspectives.researcher", "resources/icons/twotone/science.svg", "Researcher Perspective");
    iconRegistry.registerIcon("view.perspectives.planner", "resources/icons/twotone/event_note.svg", "Planner Perspective");
    iconRegistry.registerIcon("view.perspectives.manage", "resources/icons/twotone/dashboard.svg", "Manage Perspectives");
    iconRegistry.registerIcon("view.toolbars.standard", "resources/icons/twotone/view_headline.svg", "Standard Toolbar");
    iconRegistry.registerIcon("view.toolbars.format", "resources/icons/twotone/text_format.svg", "Format Toolbar");
    iconRegistry.registerIcon("view.toolbars.book", "resources/icons/twotone/menu_book.svg", "Book Toolbar");
    iconRegistry.registerIcon("view.toolbars.quickAccess", "resources/icons/twotone/bolt.svg", "Quick Access");
    iconRegistry.registerIcon("view.toolbars.customize", "resources/icons/twotone/build.svg", "Customize Toolbars");
    iconRegistry.registerIcon("view.showStatusBar", "resources/icons/twotone/horizontal_split.svg", "Status Bar");
    iconRegistry.registerIcon("view.showStatsBar", "resources/icons/twotone/insert_chart.svg", "Stats Bar");
    iconRegistry.registerIcon("view.showFormattingMarks", "resources/icons/twotone/notes.svg", "Formatting Marks");
    iconRegistry.registerIcon("view.zoomIn", "resources/icons/twotone/zoom_in.svg", "Zoom In");
    iconRegistry.registerIcon("view.zoomOut", "resources/icons/twotone/zoom_out.svg", "Zoom Out");
    iconRegistry.registerIcon("view.resetZoom", "resources/icons/twotone/fit_screen.svg", "Reset Zoom");
    iconRegistry.registerIcon("view.resetLayout", "resources/icons/twotone/dashboard.svg", "Reset Layout");
    iconRegistry.registerIcon("view.fullScreen", "resources/icons/twotone/fullscreen.svg", "Full Screen");

    // -------------------------------------------------------------------------
    // LOG PANEL TOOLBAR ICONS (OpenSpec #00024)
    // -------------------------------------------------------------------------
    iconRegistry.registerIcon("log.options", "resources/icons/twotone/settings.svg", "Log Options");
    iconRegistry.registerIcon("log.openFolder", "resources/icons/twotone/folder_open.svg", "Open Log Folder");
    iconRegistry.registerIcon("log.copy", "resources/icons/twotone/content_copy.svg", "Copy Log");
    iconRegistry.registerIcon("log.clear", "resources/icons/twotone/delete.svg", "Clear Log");

    // -------------------------------------------------------------------------
    // HELP MENU ICONS
    // -------------------------------------------------------------------------
    iconRegistry.registerIcon("help.help", "resources/icons/twotone/help.svg", "Help");
    iconRegistry.registerIcon("help.about", "resources/icons/twotone/info.svg", "About");
    iconRegistry.registerIcon("help.tutorial", "resources/icons/twotone/school.svg", "Tutorial");
    iconRegistry.registerIcon("help.shortcuts", "resources/icons/twotone/keyboard.svg", "Keyboard Shortcuts");
    iconRegistry.registerIcon("help.tipsTricks", "resources/icons/twotone/tips_and_updates.svg", "Tips & Tricks");
    iconRegistry.registerIcon("help.whatsNew", "resources/icons/twotone/new_releases.svg", "What's New");
    iconRegistry.registerIcon("help.reportBug", "resources/icons/twotone/bug_report.svg", "Report Bug");
    iconRegistry.registerIcon("help.suggestFeature", "resources/icons/twotone/feedback.svg", "Suggest Feature");
    iconRegistry.registerIcon("help.communityForum", "resources/icons/twotone/forum.svg", "Community Forum");
    iconRegistry.registerIcon("help.checkUpdates", "resources/icons/twotone/update.svg", "Check for Updates");

    // -------------------------------------------------------------------------
    // COMMON/UTILITY ICONS (for UI elements, dialogs, etc.)
    // -------------------------------------------------------------------------
    iconRegistry.registerIcon("common.check", "resources/icons/twotone/check.svg", "Check");
    iconRegistry.registerIcon("common.checkCircle", "resources/icons/twotone/check_circle.svg", "Check Circle");
    iconRegistry.registerIcon("common.cancel", "resources/icons/twotone/cancel.svg", "Cancel");
    iconRegistry.registerIcon("common.error", "resources/icons/twotone/error.svg", "Error");
    iconRegistry.registerIcon("common.warning", "resources/icons/twotone/warning.svg", "Warning");
    iconRegistry.registerIcon("common.notifications", "resources/icons/twotone/notifications.svg", "Notifications");
    iconRegistry.registerIcon("common.home", "resources/icons/twotone/home.svg", "Home");
    iconRegistry.registerIcon("common.menu", "resources/icons/twotone/menu.svg", "Menu");
    iconRegistry.registerIcon("common.moreVert", "resources/icons/twotone/more_vert.svg", "More Options");
    iconRegistry.registerIcon("common.moreHoriz", "resources/icons/twotone/more_horiz.svg", "More Options Horizontal");
    iconRegistry.registerIcon("common.expandMore", "resources/icons/twotone/expand_more.svg", "Expand More");
    iconRegistry.registerIcon("common.expandLess", "resources/icons/twotone/expand_less.svg", "Expand Less");
    iconRegistry.registerIcon("common.chevronRight", "resources/icons/twotone/chevron_right.svg", "Chevron Right");
    iconRegistry.registerIcon("common.chevronLeft", "resources/icons/twotone/chevron_left.svg", "Chevron Left");
    iconRegistry.registerIcon("common.arrowBack", "resources/icons/twotone/arrow_back.svg", "Arrow Back");
    iconRegistry.registerIcon("common.arrowForward", "resources/icons/twotone/arrow_forward.svg", "Arrow Forward");
    iconRegistry.registerIcon("common.arrowUpward", "resources/icons/twotone/arrow_upward.svg", "Arrow Up");
    iconRegistry.registerIcon("common.arrowDownward", "resources/icons/twotone/arrow_downward.svg", "Arrow Down");
    iconRegistry.registerIcon("common.delete", "resources/icons/twotone/delete.svg", "Delete");
    // Aliases for dialog buttons (navigation.up, navigation.down, action.delete)
    iconRegistry.registerIcon("navigation.up", "resources/icons/twotone/arrow_upward.svg", "Move Up");
    iconRegistry.registerIcon("navigation.down", "resources/icons/twotone/arrow_downward.svg", "Move Down");
    iconRegistry.registerIcon("action.delete", "resources/icons/twotone/delete.svg", "Delete");
    iconRegistry.registerIcon("common.firstPage", "resources/icons/twotone/first_page.svg", "First Page");
    iconRegistry.registerIcon("common.lastPage", "resources/icons/twotone/last_page.svg", "Last Page");
    iconRegistry.registerIcon("common.sort", "resources/icons/twotone/sort.svg", "Sort");
    iconRegistry.registerIcon("common.filterList", "resources/icons/twotone/filter_list.svg", "Filter");
    iconRegistry.registerIcon("common.lock", "resources/icons/twotone/lock.svg", "Lock");
    iconRegistry.registerIcon("common.lockOpen", "resources/icons/twotone/lock_open.svg", "Unlock");
    iconRegistry.registerIcon("common.print", "resources/icons/twotone/print.svg", "Print");
    iconRegistry.registerIcon("common.share", "resources/icons/twotone/share.svg", "Share");
    iconRegistry.registerIcon("common.attachFile", "resources/icons/twotone/attach_file.svg", "Attach File");
    iconRegistry.registerIcon("common.filePresent", "resources/icons/twotone/file_present.svg", "File Present");
    iconRegistry.registerIcon("common.folder", "resources/icons/twotone/folder.svg", "Folder");
    iconRegistry.registerIcon("common.createNewFolder", "resources/icons/twotone/create_new_folder.svg", "Create Folder");
    iconRegistry.registerIcon("common.driveFileMove", "resources/icons/twotone/drive_file_move.svg", "Move File");
    iconRegistry.registerIcon("common.fileCopy", "resources/icons/twotone/file_copy.svg", "Copy File");
    // Dock title bar button icons (OpenSpec #00032)
    iconRegistry.registerIcon("dock.float", "resources/icons/twotone/fullscreen_exit.svg", "Float");
    iconRegistry.registerIcon("dock.close", "resources/icons/twotone/close.svg", "Close");

    // -------------------------------------------------------------------------
    // NEW ITEM DIALOG ICONS (OpenSpec #00033)
    // -------------------------------------------------------------------------
    iconRegistry.registerIcon("template.novel", "resources/icons/twotone/auto_stories.svg", "Novel");
    iconRegistry.registerIcon("template.shortStories", "resources/icons/twotone/book.svg", "Short Stories");
    iconRegistry.registerIcon("template.nonfiction", "resources/icons/twotone/menu_book.svg", "Non-fiction");
    iconRegistry.registerIcon("template.screenplay", "resources/icons/twotone/theaters.svg", "Screenplay");
    iconRegistry.registerIcon("template.poetry", "resources/icons/twotone/edit_note.svg", "Poetry");
    iconRegistry.registerIcon("template.empty", "resources/icons/twotone/folder_open.svg", "Empty Project");
    iconRegistry.registerIcon("template.chapter", "resources/icons/twotone/article.svg", "Chapter");
    iconRegistry.registerIcon("template.mindmap", "resources/icons/twotone/account_tree.svg", "Mind Map");
    iconRegistry.registerIcon("template.timeline", "resources/icons/twotone/timeline.svg", "Timeline");
    iconRegistry.registerIcon("template.note", "resources/icons/twotone/sticky_note_2.svg", "Note");
    iconRegistry.registerIcon("template.character", "resources/icons/twotone/person.svg", "Character");
    iconRegistry.registerIcon("template.location", "resources/icons/twotone/place.svg", "Location");

    logger.debug("Registered {} icons with IconRegistry", 177);

    // =========================================================================
    // FILE MENU
    // =========================================================================

    REG_CMD_TOOL_ICON("file.new", "New File", "FILE/New File", 10, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::New),
                      IconSet(),
                      [this]() { onNewDocument(); });

    // OpenSpec #00033: New Book command (Ctrl+Shift+N)
    REG_CMD_TOOL_ICON("file.new.project", "New Book...", "FILE/New Book...", 15, false, 0,
                      KeyboardShortcut(Qt::Key_N, Qt::ControlModifier | Qt::ShiftModifier),
                      IconSet(),
                      [this]() { onNewProject(); });

    REG_CMD_TOOL_ICON("file.open", "Open Book...", "FILE/Open Book...", 20, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Open),
                      IconSet(),
                      [this]() { onOpenDocument(); });

    // OpenSpec #00033 Phase F: Open standalone file (Ctrl+Shift+O)
    REG_CMD_TOOL_ICON("file.open.file", "Open File...", "FILE/Open/Open File...", 35, false, 0,
                      KeyboardShortcut(Qt::Key_O, Qt::ControlModifier | Qt::ShiftModifier),
                      IconSet(),
                      [this]() { onOpenStandaloneFile(); });

    // Recent Books - dynamic submenu (registered separately)

    // OpenSpec #00030: Added Ctrl+W shortcut for Close Book
    REG_CMD_TOOL_ICON("file.close", "Close Book", "FILE/Close Book", 40, true, 1,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Close),
                      IconSet(),
                      []() {});

    REG_CMD_TOOL_ICON("file.save", "Save", "FILE/Save", 50, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Save),
                      IconSet(),
                      [this]() { onSaveDocument(); });

    REG_CMD_TOOL_ICON("file.saveAs", "Save As...", "FILE/Save As...", 60, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::SaveAs),
                      IconSet(),
                      [this]() { onSaveAsDocument(); });

    REG_CMD("file.saveAll", "Save All", "FILE/Save All", 70, true, 1);

    // Import submenu
    REG_CMD("file.import.docx", "DOCX Document...", "FILE/Import/DOCX Document...", 80, false, 1);
    REG_CMD("file.import.pdf", "PDF Reference...", "FILE/Import/PDF Reference...", 90, false, 2);
    REG_CMD("file.import.text", "Plain Text...", "FILE/Import/Plain Text...", 100, false, 1);
    REG_CMD("file.import.scrivener", "Scrivener Project...", "FILE/Import/Scrivener Project...", 110, false, 2);

    // Import Archive (priority 75 in Import submenu)
    REG_CMD_TOOL_ICON("file.import.archive", "Project Archive...", "FILE/Import/Project Archive...", 75, false, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      [this]() { onImportArchive(); });

    // Export submenu
    REG_CMD("file.export.docx", "DOCX", "FILE/Export/DOCX", 120, false, 1);
    REG_CMD("file.export.pdf", "PDF", "FILE/Export/PDF", 130, false, 1);
    REG_CMD("file.export.markdown", "Markdown", "FILE/Export/Markdown", 140, true, 1);
    REG_CMD("file.export.epub", "EPUB", "FILE/Export/EPUB", 150, false, 2);
    REG_CMD("file.export.mobi", "MOBI", "FILE/Export/MOBI", 160, false, 2);
    REG_CMD("file.export.icml", "InDesign ICML", "FILE/Export/InDesign ICML", 170, false, 3);
    REG_CMD("file.export.latex", "LaTeX", "FILE/Export/LaTeX", 180, false, 3);
    REG_CMD("file.export.settings", "Export Settings...", "FILE/Export/Export Settings...", 190, true, 2);

    // Export Archive (priority 195 = end of Export submenu)
    REG_CMD_TOOL_ICON("file.export.archive", "Project Archive...", "FILE/Export/Project Archive...", 195, true, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      [this]() { onExportArchive(); });

    REG_CMD_TOOL_ICON("file.exit", "Exit", "FILE/Exit", 200, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Quit),
                      IconSet(),
                      [this]() { onExit(); });

    // =========================================================================
    // EDIT MENU
    // =========================================================================

    REG_CMD_TOOL_ICON("edit.undo", "Undo", "EDIT/Undo", 10, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Undo),
                      IconSet(),
                      [this]() { onUndo(); });

    REG_CMD_TOOL_ICON("edit.redo", "Redo", "EDIT/Redo", 20, true, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Redo),
                      IconSet(),
                      [this]() { onRedo(); });

    REG_CMD_TOOL_ICON("edit.cut", "Cut", "EDIT/Cut", 30, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Cut),
                      IconSet(),
                      [this]() { onCut(); });

    REG_CMD_TOOL_ICON("edit.copy", "Copy", "EDIT/Copy", 40, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Copy),
                      IconSet(),
                      [this]() { onCopy(); });

    REG_CMD_TOOL_ICON("edit.paste", "Paste", "EDIT/Paste", 50, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::Paste),
                      IconSet(),
                      [this]() { onPaste(); });

    REG_CMD("edit.pasteSpecial", "Paste Special...", "EDIT/Paste Special...", 60, false, 1);
    REG_CMD("edit.delete", "Delete", "EDIT/Delete", 70, true, 1);

    REG_CMD_TOOL_ICON("edit.selectAll", "Select All", "EDIT/Select All", 80, false, 0,
                      KeyboardShortcut::fromQKeySequence(QKeySequence::SelectAll),
                      IconSet(),
                      [this]() { onSelectAll(); });

    REG_CMD("edit.selectWord", "Select Word", "EDIT/Select Word", 90, false, 1);
    REG_CMD("edit.selectParagraph", "Select Paragraph", "EDIT/Select Paragraph", 100, true, 1);

    // OpenSpec #00030: Added keyboard shortcuts for Find operations
    REG_CMD_KEY("edit.find", "Find...", "EDIT/Find...", 110, false, 1,
                KeyboardShortcut::fromQKeySequence(QKeySequence::Find));
    REG_CMD_KEY("edit.findNext", "Find Next", "EDIT/Find Next", 120, false, 1,
                KeyboardShortcut::fromQKeySequence(QKeySequence::FindNext));
    REG_CMD_KEY("edit.findPrevious", "Find Previous", "EDIT/Find Previous", 130, false, 1,
                KeyboardShortcut::fromQKeySequence(QKeySequence::FindPrevious));
    REG_CMD_KEY("edit.findReplace", "Find & Replace...", "EDIT/Find & Replace...", 140, false, 1,
                KeyboardShortcut::fromQKeySequence(QKeySequence::Replace));
    REG_CMD("edit.findInBook", "Find in Book...", "EDIT/Find in Book...", 150, true, 1);

    REG_CMD_CB("edit.preferences", "Preferences...", "EDIT/Preferences...", 160, false, 0,
               [this]() { onSettings(); });

    // =========================================================================
    // BOOK MENU
    // =========================================================================

    REG_CMD_TOOL_ICON("book.newChapter", "New Chapter...", "BOOK/New Chapter...", 10, false, 1,
                      KeyboardShortcut(),
                      IconSet(),
                      []() {});

    REG_CMD("book.newScene", "New Scene...", "BOOK/New Scene...", 20, true, 1);

    REG_CMD_TOOL_ICON("book.newCharacter", "New Character...", "BOOK/New Character...", 30, false, 1,
                      KeyboardShortcut(),
                      IconSet(),
                      []() {});

    REG_CMD_TOOL_ICON("book.newLocation", "New Location...", "BOOK/New Location...", 40, false, 1,
                      KeyboardShortcut(),
                      IconSet(),
                      []() {});

    REG_CMD("book.newItem", "New Item...", "BOOK/New Item...", 50, true, 1);

    REG_CMD("book.newMindMap", "New Mind Map...", "BOOK/New Mind Map...", 60, false, 1);
    REG_CMD("book.newTimeline", "New Timeline...", "BOOK/New Timeline...", 70, true, 1);

    REG_CMD("book.chapterBreak", "Chapter Break", "BOOK/Chapter Break", 80, false, 1);
    REG_CMD("book.sceneBreak", "Scene Break", "BOOK/Scene Break", 90, true, 1);

    REG_CMD_TOOL_ICON("book.properties", "Book Properties...", "BOOK/Book Properties...", 100, false, 1,
                      KeyboardShortcut(),
                      IconSet(),
                      []() {});

    // =========================================================================
    // INSERT MENU
    // =========================================================================

    REG_CMD("insert.image", "Image...", "INSERT/Image...", 10, false, 1);
    REG_CMD("insert.table", "Table...", "INSERT/Table...", 20, false, 1);
    REG_CMD("insert.link", "Link...", "INSERT/Link...", 30, true, 1);

    REG_CMD("insert.footnote", "Footnote", "INSERT/Footnote", 40, false, 1);
    REG_CMD("insert.endnote", "Endnote", "INSERT/Endnote", 50, false, 1);
    REG_CMD("insert.comment", "Comment", "INSERT/Comment", 60, false, 1);
    REG_CMD("insert.annotation", "Annotation", "INSERT/Annotation", 70, true, 1);

    REG_CMD("insert.specialChar", "Special Character...", "INSERT/Special Character...", 80, false, 1);
    REG_CMD("insert.dateTime", "Date & Time", "INSERT/Date & Time", 90, false, 1);
    REG_CMD("insert.field", "Field...", "INSERT/Field...", 100, false, 1);

    // =========================================================================
    // FORMAT MENU
    // =========================================================================

    REG_CMD("format.font", "Font...", "FORMAT/Font...", 10, false, 1);
    REG_CMD("format.paragraph", "Paragraph...", "FORMAT/Paragraph...", 20, true, 1);

    // Text Style submenu
    REG_CMD("format.style.heading1", "Heading 1", "FORMAT/Text Style/Heading 1", 30, false, 1);
    REG_CMD("format.style.heading2", "Heading 2", "FORMAT/Text Style/Heading 2", 40, false, 1);
    REG_CMD("format.style.heading3", "Heading 3", "FORMAT/Text Style/Heading 3", 50, false, 1);
    REG_CMD("format.style.body", "Body Text", "FORMAT/Text Style/Body Text", 60, false, 1);
    REG_CMD("format.style.quote", "Quote", "FORMAT/Text Style/Quote", 70, false, 1);
    REG_CMD("format.style.code", "Code", "FORMAT/Text Style/Code", 80, true, 1);
    REG_CMD("format.style.manage", "Manage Styles...", "FORMAT/Text Style/Manage Styles...", 90, false, 1);

    // OpenSpec #00030: Added standard formatting shortcuts (Ctrl+B/I/U)
    REG_CMD_KEY("format.bold", "Bold", "FORMAT/Bold", 100, false, 1,
                KeyboardShortcut::fromQKeySequence(QKeySequence::Bold));
    REG_CMD_KEY("format.italic", "Italic", "FORMAT/Italic", 110, false, 1,
                KeyboardShortcut::fromQKeySequence(QKeySequence::Italic));
    REG_CMD_KEY("format.underline", "Underline", "FORMAT/Underline", 120, false, 1,
                KeyboardShortcut::fromQKeySequence(QKeySequence::Underline));
    REG_CMD("format.strikethrough", "Strikethrough", "FORMAT/Strikethrough", 130, true, 1);

    REG_CMD("format.alignLeft", "Align Left", "FORMAT/Align Left", 140, false, 1);
    REG_CMD("format.alignCenter", "Align Center", "FORMAT/Align Center", 150, false, 1);
    REG_CMD("format.alignRight", "Align Right", "FORMAT/Align Right", 160, false, 1);
    REG_CMD("format.justify", "Justify", "FORMAT/Justify", 170, true, 1);

    REG_CMD("format.increaseIndent", "Increase Indent", "FORMAT/Increase Indent", 180, false, 1);
    REG_CMD("format.decreaseIndent", "Decrease Indent", "FORMAT/Decrease Indent", 190, true, 1);

    REG_CMD("format.bullets", "Bullets", "FORMAT/Bullets", 200, false, 1);
    REG_CMD("format.numbering", "Numbering", "FORMAT/Numbering", 210, true, 1);

    REG_CMD("format.color", "Color", "FORMAT/Color", 220, true, 1);

    REG_CMD("format.clearFormatting", "Clear Formatting", "FORMAT/Clear Formatting", 230, false, 1);

    // =========================================================================
    // TOOLS MENU
    // =========================================================================

    // Statistics submenu (analysis tools only, panels in VIEW)
    REG_CMD("tools.stats.full", "Full Statistics...", "TOOLS/Statistics/Full Statistics...", 10, false, 2);

    REG_CMD_TOOL_ICON("tools.stats.wordCount", "Word Count", "TOOLS/Statistics/Word Count", 20, true, 0,
                      KeyboardShortcut(),
                      IconSet(),
                      []() {});

    REG_CMD_TOOL_ICON("tools.spellcheck", "Spellchecker", "TOOLS/Spellchecker", 40, false, 2,
                      KeyboardShortcut(),
                      IconSet(),
                      []() {});

    REG_CMD("tools.grammar", "Grammar Check", "TOOLS/Grammar Check", 50, false, 2);
    REG_CMD("tools.readability", "Readability Score", "TOOLS/Readability Score", 60, true, 2);

    // Focus Mode submenu
    REG_CMD_TOOL_ICON("tools.focus.normal", "Normal", "TOOLS/Focus Mode/Normal", 70, false, 1,
                      KeyboardShortcut(),
                      IconSet(),
                      []() {});

    REG_CMD("tools.focus.focused", "Focused", "TOOLS/Focus Mode/Focused", 80, false, 1);
    REG_CMD("tools.focus.distractionFree", "Distraction-Free", "TOOLS/Focus Mode/Distraction-Free", 90, false, 1);

    REG_CMD("tools.backupNow", "Backup Now", "TOOLS/Backup Now", 100, false, 2);
    REG_CMD("tools.autoSaveSettings", "Auto-Save Settings...", "TOOLS/Auto-Save Settings...", 110, false, 1);
    REG_CMD("tools.versionHistory", "Version History...", "TOOLS/Version History...", 120, true, 2);

    // Plugins submenu
    REG_CMD("tools.plugins.manager", "Plugin Manager...", "TOOLS/Plugins/Plugin Manager...", 130, false, 2);
    REG_CMD("tools.plugins.marketplace", "Browse Marketplace...", "TOOLS/Plugins/Browse Marketplace...", 140, false, 2);
    REG_CMD("tools.plugins.updates", "Check for Updates...", "TOOLS/Plugins/Check for Updates...", 150, true, 2);
    REG_CMD("tools.plugins.reload", "Reload Plugins", "TOOLS/Plugins/Reload Plugins", 160, false, 2);

    REG_CMD("tools.challenges", "Challenges & Badges...", "TOOLS/Challenges & Badges...", 170, false, 2);
    REG_CMD("tools.writingGoals", "Writing Goals & Deadlines...", "TOOLS/Writing Goals & Deadlines...", 180, true, 2);

    REG_CMD("tools.cloudSync", "Cloud Sync...", "TOOLS/Cloud Sync...", 190, false, 3);
    REG_CMD("tools.collaboration", "Collaboration...", "TOOLS/Collaboration...", 200, false, 3);

    // =========================================================================
    // ASSISTANT MENU
    // =========================================================================

    REG_CMD("assistant.ask", "Ask Assistant...", "ASSISTANT/Ask Assistant...", 10, true, 2);

    REG_CMD("assistant.switch", "Switch Assistant...", "ASSISTANT/Switch Assistant...", 20, true, 2);

    // Assistant Actions submenu
    REG_CMD("assistant.action.grammar", "Check Grammar", "ASSISTANT/Assistant Actions/Check Grammar", 30, false, 2);
    REG_CMD("assistant.action.style", "Improve Style", "ASSISTANT/Assistant Actions/Improve Style", 40, false, 2);
    REG_CMD("assistant.action.plot", "Analyze Plot", "ASSISTANT/Assistant Actions/Analyze Plot", 50, false, 2);
    REG_CMD("assistant.action.research", "Research Topic...", "ASSISTANT/Assistant Actions/Research Topic...", 60, false, 2);
    REG_CMD("assistant.action.speedDraft", "Speed Draft Mode", "ASSISTANT/Assistant Actions/Speed Draft Mode", 70, false, 2);

    REG_CMD("assistant.settings", "Assistant Settings...", "ASSISTANT/Assistant Settings...", 80, false, 2);

    // =========================================================================
    // VIEW MENU
    // =========================================================================

    // Panel toggle commands - registered here for CommandRegistry/Toolbar system
    // Execute callbacks are set later in createDocks() after dock widgets exist
    REG_CMD_KEY("view.navigator", "Navigator", "VIEW/Panels/Navigator", 10, false, 0,
                KeyboardShortcut(Qt::Key_F2, Qt::NoModifier));

    REG_CMD_KEY("view.properties", "Properties", "VIEW/Panels/Properties", 20, false, 0,
                KeyboardShortcut(Qt::Key_F3, Qt::NoModifier));

    REG_CMD_KEY("view.log", "Log", "VIEW/Panels/Log", 30, false, 0,
                KeyboardShortcut(Qt::Key_F4, Qt::NoModifier));

    REG_CMD_KEY("view.search", "Search", "VIEW/Panels/Search", 40, false, 0,
                KeyboardShortcut(Qt::Key_F5, Qt::NoModifier));

    REG_CMD_KEY("view.assistant", "Assistant", "VIEW/Panels/Assistant", 50, false, 0,
                KeyboardShortcut(Qt::Key_F6, Qt::NoModifier));

    // Perspectives submenu
    REG_CMD("view.perspectives.writer", "Writer", "VIEW/Perspectives/Writer", 70, false, 1);
    REG_CMD("view.perspectives.editor", "Editor", "VIEW/Perspectives/Editor", 80, false, 1);
    REG_CMD("view.perspectives.researcher", "Researcher", "VIEW/Perspectives/Researcher", 90, false, 1);
    REG_CMD("view.perspectives.planner", "Planner", "VIEW/Perspectives/Planner", 100, true, 1);
    REG_CMD("view.perspectives.save", "Save Current Perspective...", "VIEW/Perspectives/Save Current Perspective...", 110, false, 1);
    REG_CMD("view.perspectives.manage", "Manage Perspectives...", "VIEW/Perspectives/Manage Perspectives...", 120, false, 1);

    // OpenSpec #00030: VIEW/Toolbars submenu is created DYNAMICALLY
    // by ToolbarManager::createViewMenuActions() - no static commands here.
    // Toolbar toggle actions + "Toolbar Manager..." are all dynamic.

    REG_CMD("view.showStatusBar", "Show Status Bar", "VIEW/Show Status Bar", 180, false, 0);
    REG_CMD("view.showStatsBar", "Show Statistics Bar", "VIEW/Show Statistics Bar", 190, false, 1);
    REG_CMD("view.showFormattingMarks", "Show Formatting Marks", "VIEW/Show Formatting Marks", 210, true, 1);

    REG_CMD("view.zoomIn", "Zoom In", "VIEW/Zoom In", 220, false, 1);
    REG_CMD("view.zoomOut", "Zoom Out", "VIEW/Zoom Out", 230, false, 1);
    REG_CMD("view.resetZoom", "Reset Zoom", "VIEW/Reset Zoom", 240, true, 1);

    // OpenSpec #00030: F11 for Full Screen (standard)
    REG_CMD_KEY("view.fullScreen", "Full Screen", "VIEW/Full Screen", 250, true, 1,
                KeyboardShortcut::fromQKeySequence(QKeySequence::FullScreen));

    REG_CMD_CB("view.resetLayout", "Reset Layout", "VIEW/Reset Layout", 260, false, 0,
               [this]() { resetLayout(); });

    // =========================================================================
    // HELP MENU
    // =========================================================================

    // OpenSpec #00030: F1 for Help (standard)
    REG_CMD_KEY("help.manual", "Kalahari Help", "HELP/Kalahari Help", 10, false, 2,
                KeyboardShortcut::fromQKeySequence(QKeySequence::HelpContents));
    REG_CMD("help.tutorial", "Getting Started Tutorial", "HELP/Getting Started Tutorial", 20, true, 2);

    REG_CMD("help.shortcuts", "Keyboard Shortcuts", "HELP/Keyboard Shortcuts", 30, false, 1);
    REG_CMD("help.tipsTricks", "Tips & Tricks", "HELP/Tips & Tricks", 40, false, 2);
    REG_CMD("help.whatsNew", "What's New", "HELP/What's New", 50, true, 1);

    REG_CMD("help.reportBug", "Report a Bug...", "HELP/Report a Bug...", 60, false, 1);
    REG_CMD("help.suggestFeature", "Suggest a Feature...", "HELP/Suggest a Feature...", 70, false, 1);
    REG_CMD("help.communityForum", "Community Forum", "HELP/Community Forum", 80, true, 2);

    REG_CMD("help.checkUpdates", "Check for Updates...", "HELP/Check for Updates...", 90, true, 2);

    REG_CMD_CB("help.about", "About Kalahari", "HELP/About Kalahari", 100, false, 0,
               [this]() { onAbout(); });

    logger.debug("Commands registered successfully ({} commands)", count);
}

void MainWindow::createMenus() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating menus from CommandRegistry");

    // Build menu bar from CommandRegistry using MenuBuilder
    // Task #00025: Store MenuBuilder for icon refresh on theme change
    m_menuBuilder = new MenuBuilder();
    CommandRegistry& registry = CommandRegistry::getInstance();
    m_menuBuilder->buildMenuBar(registry, this);

    // Store FILE menu pointer for Recent Books integration
    m_fileMenu = m_menuBuilder->getMenu("FILE");

    // OpenSpec #00030: Add Recent Books submenu to FILE menu
    if (m_fileMenu) {
        auto& recentBooks = core::RecentBooksManager::getInstance();
        recentBooks.createRecentBooksMenu(m_fileMenu);

        // Connect signal to open recent file
        connect(&recentBooks, &core::RecentBooksManager::recentFileClicked,
                this, &MainWindow::onOpenRecentFile);

        logger.debug("Recent Books submenu added to FILE menu");
    }

    // Note: View menu will be populated by createDocks() with panel toggles
    // This is intentional - View menu is created empty by MenuBuilder
    // and filled with QDockWidget toggleViewAction() later

    logger.debug("Menus created successfully from CommandRegistry");
}

void MainWindow::createToolbars() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating toolbars from CommandRegistry using ToolbarManager");

    // Task #00019: Create ToolbarManager and build 5 toolbars
    m_toolbarManager = new ToolbarManager(this);
    CommandRegistry& registry = CommandRegistry::getInstance();
    m_toolbarManager->createToolbars(registry);

    // OpenSpec #00026: Connect ArtProvider::resourcesChanged() to update toolbar icon sizes
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged,
            this, [this]() {
                if (m_toolbarManager) {
                    m_toolbarManager->updateIconSizes();
                }
            });

    // OpenSpec #00032: Connect ArtProvider::resourcesChanged() to refresh dock title bar icons
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged,
            this, &MainWindow::refreshDockIcons);

    logger.debug("5 toolbars created successfully (File, Edit, Book, View, Tools)");
}

void MainWindow::createStatusBar() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating status bar");

    statusBar()->showMessage(tr("Ready"), 3000);  // Show for 3 seconds

    logger.debug("Status bar created successfully");
}

// Slots implementation
void MainWindow::onNewDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: New Document");

    // Task #00015: Check for unsaved changes in current editor tab
    EditorPanel* currentEditor = getCurrentEditor();
    if (currentEditor && m_isDirty) {
        auto reply = QMessageBox::question(
            this,
            tr("Unsaved Changes"),
            tr("Do you want to save changes to the current document?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
        );

        if (reply == QMessageBox::Save) {
            onSaveDocument();
            if (m_isDirty) return;  // Save was cancelled or failed
        } else if (reply == QMessageBox::Cancel) {
            return;
        }
        // Discard → continue
    }

    // Task #00015: Create new EditorPanel tab (on-demand)
    EditorPanel* newEditor = new EditorPanel(this);
    int tabIndex = m_centralTabs->addTab(newEditor, tr("Untitled"));
    m_centralTabs->setCurrentIndex(tabIndex);

    // Connect textChanged signal for dirty tracking
    connect(newEditor->getTextEdit(), &QTextEdit::textChanged,
            this, [this]() {
                if (!m_currentDocument.has_value()) return;
                setDirty(true);
            });

    // Create new document
    m_currentDocument = core::Document("Untitled", "User", "en");
    m_currentFilePath = "";
    newEditor->setText("");
    setDirty(false);

    // Update navigator panel
    m_navigatorPanel->loadDocument(m_currentDocument.value());

    logger.info("New document created in new tab");
    statusBar()->showMessage(tr("New document created"), 2000);
}

void MainWindow::onNewProject() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: New Project (OpenSpec #00033)");

    // Show NewItemDialog in Project mode
    dialogs::NewItemDialog dialog(dialogs::NewItemMode::Project, this);
    if (dialog.exec() == QDialog::Accepted) {
        auto result = dialog.result();

        // Create project folder path
        QString projectPath = result.location;
        if (result.createSubfolder) {
            projectPath = QDir(result.location).filePath(result.title);
        }

        // Use ProjectManager to create project
        auto& pm = core::ProjectManager::getInstance();
        if (pm.createProject(projectPath, result.title, result.author, result.language, false)) {
            // Project created successfully - update UI
            updateWindowTitle();

            // Update navigator if document available
            if (pm.getDocument()) {
                m_navigatorPanel->loadDocument(*pm.getDocument());
            }

            logger.info("Project created: {} at {}", result.title.toStdString(), projectPath.toStdString());
            statusBar()->showMessage(tr("Project created: %1").arg(result.title), 3000);
        } else {
            logger.error("Failed to create project: {}", result.title.toStdString());
            QMessageBox::critical(
                this,
                tr("Project Creation Failed"),
                tr("Could not create project '%1'.\n\nCheck that the location is writable and try again.")
                    .arg(result.title)
            );
        }
    }
}

void MainWindow::onOpenDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Open Document");

    // OpenSpec #00033 Phase D: Use ProjectManager for opening projects
    QString filename = QFileDialog::getOpenFileName(
        this,
        tr("Open Book"),
        QString(),
        tr("Kalahari Books (*.klh)")
    );

    if (filename.isEmpty()) {
        logger.info("Open cancelled by user");
        return;
    }

    // Use ProjectManager to open the project
    auto& pm = core::ProjectManager::getInstance();
    if (!pm.openProject(filename)) {
        QMessageBox::critical(
            this,
            tr("Open Error"),
            tr("Failed to open book: %1").arg(filename)
        );
        return;
    }
    // ProjectManager emits projectOpened, which triggers onProjectOpened()

    // OpenSpec #00030: Add to recent files list
    core::RecentBooksManager::getInstance().addRecentFile(filename);
}

void MainWindow::onOpenRecentFile(const QString& filePath) {
    auto& logger = core::Logger::getInstance();
    logger.info("Opening recent file: {}", filePath.toStdString());

    // Check if file still exists
    if (!QFileInfo::exists(filePath)) {
        QMessageBox::warning(
            this,
            tr("File Not Found"),
            tr("The file '%1' no longer exists.").arg(filePath)
        );

        // Remove from recent files
        core::RecentBooksManager::getInstance().removeRecentFile(filePath);
        return;
    }

    // Check for unsaved changes in current editor tab
    EditorPanel* currentEditor = getCurrentEditor();
    if (currentEditor && m_isDirty) {
        auto reply = QMessageBox::question(
            this,
            tr("Unsaved Changes"),
            tr("Do you want to save changes to the current document?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
        );

        if (reply == QMessageBox::Save) {
            onSaveDocument();
            if (m_isDirty) return;
        } else if (reply == QMessageBox::Cancel) {
            return;
        }
    }

    // Check if file is a .klh manifest - try ProjectManager first
    if (filePath.endsWith(".klh", Qt::CaseInsensitive)) {
        auto& pm = core::ProjectManager::getInstance();
        if (pm.openProject(filePath)) {
            // Successfully opened as project
            logger.info("Opened .klh file as project: {}", filePath.toStdString());
            return;
        }
        // If failed, fall through to try old archive format
        logger.debug("Failed to open .klh as project, trying old archive format");
    }

    // Load document (old archive format)
    std::filesystem::path filepath = filePath.toStdString();
    auto loaded = core::DocumentArchive::load(filepath);

    if (!loaded.has_value()) {
        QMessageBox::critical(
            this,
            tr("Open Error"),
            tr("Failed to open document: %1").arg(filePath)
        );
        logger.error("Failed to load recent file: {}", filepath.string());

        // Remove from recent files
        core::RecentBooksManager::getInstance().removeRecentFile(filePath);
        return;
    }

    // Create new EditorPanel tab
    EditorPanel* newEditor = new EditorPanel(this);
    QString docTitle = QString::fromStdString(loaded.value().getTitle());
    int tabIndex = m_centralTabs->addTab(newEditor, docTitle);
    m_centralTabs->setCurrentIndex(tabIndex);

    // Connect textChanged signal for dirty tracking
    connect(newEditor->getTextEdit(), &QTextEdit::textChanged,
            this, [this]() {
                if (!m_currentDocument.has_value()) return;
                setDirty(true);
            });

    // Update state
    m_currentDocument = std::move(loaded.value());
    m_currentFilePath = filepath;

    // Extract text and load into editor
    QString content = getPhase0Content(m_currentDocument.value());
    newEditor->setText(content);
    setDirty(false);

    // Update navigator panel
    m_navigatorPanel->loadDocument(m_currentDocument.value());

    // Move to top of recent files
    core::RecentBooksManager::getInstance().addRecentFile(filePath);

    logger.info("Recent file loaded: {}", filepath.string());
    statusBar()->showMessage(tr("Document opened: %1").arg(filePath), 2000);
}

void MainWindow::onSaveDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Save Document");

    // Task #00015: Get current editor (returns nullptr if Dashboard is active)
    EditorPanel* editor = getCurrentEditor();
    if (!editor) {
        logger.debug("No editor tab active - cannot save");
        statusBar()->showMessage(tr("No document to save"), 2000);
        return;
    }

    // If no current file, delegate to Save As
    if (m_currentFilePath.empty()) {
        onSaveAsDocument();
        return;
    }

    // Ensure we have a document
    if (!m_currentDocument.has_value()) {
        m_currentDocument = core::Document("Untitled", "User", "en");
    }

    // Get text from current editor and update document
    QString text = editor->getText();
    setPhase0Content(m_currentDocument.value(), text);

    // Save to file
    bool saved = core::DocumentArchive::save(m_currentDocument.value(), m_currentFilePath);

    if (!saved) {
        QMessageBox::critical(
            this,
            tr("Save Error"),
            tr("Failed to save document: %1").arg(QString::fromStdString(m_currentFilePath.string()))
        );
        logger.error("Failed to save document: {}", m_currentFilePath.string());
        return;
    }

    setDirty(false);
    logger.info("Document saved: {}", m_currentFilePath.string());
    statusBar()->showMessage(tr("Document saved"), 2000);
}

void MainWindow::onSaveAsDocument() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Save As Document");

    // Task #00015: Get current editor (returns nullptr if Dashboard is active)
    EditorPanel* editor = getCurrentEditor();
    if (!editor) {
        logger.debug("No editor tab active - cannot save");
        statusBar()->showMessage(tr("No document to save"), 2000);
        return;
    }

    // Show save file dialog
    QString filename = QFileDialog::getSaveFileName(
        this,
        tr("Save Document As"),
        QString(),
        tr("Kalahari Files (*.klh)")
    );

    if (filename.isEmpty()) {
        logger.info("Save As cancelled by user");
        return;
    }

    // Ensure .klh extension
    if (!filename.endsWith(".klh", Qt::CaseInsensitive)) {
        filename += ".klh";
    }

    // Ensure we have a document
    if (!m_currentDocument.has_value()) {
        m_currentDocument = core::Document("Untitled", "User", "en");
    }

    // Get text from current editor and update document
    QString text = editor->getText();
    setPhase0Content(m_currentDocument.value(), text);

    // Update document title from filename
    std::filesystem::path filepath = filename.toStdString();
    std::string title = filepath.stem().string();
    m_currentDocument->setTitle(title);

    // Save to file
    bool saved = core::DocumentArchive::save(m_currentDocument.value(), filepath);

    if (!saved) {
        QMessageBox::critical(
            this,
            tr("Save Error"),
            tr("Failed to save document: %1").arg(filename)
        );
        logger.error("Failed to save document: {}", filepath.string());
        return;
    }

    // Success - update state
    m_currentFilePath = filepath;
    setDirty(false);
    logger.info("Document saved as: {}", filepath.string());
    statusBar()->showMessage(tr("Document saved as: %1").arg(filename), 2000);
}

void MainWindow::onSaveAll() {
    // OpenSpec #00033 Phase E: Save all dirty chapters
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Save All");

    auto& pm = core::ProjectManager::getInstance();
    if (!pm.isProjectOpen()) {
        logger.debug("No project open - Save All does nothing");
        statusBar()->showMessage(tr("No project open"), 2000);
        return;
    }

    // First, update content cache for all dirty chapters from open tabs
    for (int i = 0; i < m_centralTabs->count(); ++i) {
        EditorPanel* editor = qobject_cast<EditorPanel*>(m_centralTabs->widget(i));
        if (!editor) continue;

        QString elemId = editor->property("elementId").toString();
        if (elemId.isEmpty()) continue;

        if (m_dirtyChapters.value(elemId, false)) {
            core::BookElement* element = pm.findElement(elemId);
            if (element) {
                element->setContent(editor->getContent());
                logger.debug("Updated content cache for: {}", elemId.toStdString());
            }
        }
    }

    // Save all dirty elements via ProjectManager
    bool success = pm.saveAllDirty();

    if (success) {
        // Clear dirty flags and update tab titles
        for (int i = 0; i < m_centralTabs->count(); ++i) {
            EditorPanel* editor = qobject_cast<EditorPanel*>(m_centralTabs->widget(i));
            if (!editor) continue;

            QString elemId = editor->property("elementId").toString();
            if (!elemId.isEmpty() && m_dirtyChapters.value(elemId, false)) {
                // Clear dirty flag
                m_dirtyChapters[elemId] = false;

                // Remove asterisk from tab title
                QString tabText = m_centralTabs->tabText(i);
                if (tabText.startsWith("*")) {
                    m_centralTabs->setTabText(i, tabText.mid(1));
                }
            }
        }

        pm.setDirty(false);
        logger.info("All chapters saved successfully");
        statusBar()->showMessage(tr("All changes saved"), 2000);
    } else {
        logger.error("Failed to save some chapters");
        statusBar()->showMessage(tr("Error saving some chapters"), 3000);
        QMessageBox::warning(this, tr("Save Warning"),
            tr("Some chapters could not be saved. Check the log for details."));
    }
}

void MainWindow::onExit() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Exit");
    QApplication::quit();
}

void MainWindow::onUndo() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Undo");

    m_editorPanel->getTextEdit()->undo();

    statusBar()->showMessage(tr("Undo performed"), 2000);
}

void MainWindow::onRedo() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Redo");

    m_editorPanel->getTextEdit()->redo();

    statusBar()->showMessage(tr("Redo performed"), 2000);
}

void MainWindow::onCut() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Cut");

    m_editorPanel->getTextEdit()->cut();

    statusBar()->showMessage(tr("Cut to clipboard"), 2000);
}

void MainWindow::onCopy() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Copy");

    m_editorPanel->getTextEdit()->copy();

    statusBar()->showMessage(tr("Copied to clipboard"), 2000);
}

void MainWindow::onPaste() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Paste");

    m_editorPanel->getTextEdit()->paste();

    statusBar()->showMessage(tr("Pasted from clipboard"), 2000);
}

void MainWindow::onSelectAll() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Select All");

    m_editorPanel->getTextEdit()->selectAll();

    statusBar()->showMessage(tr("All text selected"), 2000);
}

void MainWindow::onSettings() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Settings");

    // Collect current settings to pass to dialog
    SettingsData currentSettings = collectCurrentSettings();

    // Create dialog with current settings
    SettingsDialog dialog(this, currentSettings);

    // Connect settings applied signal - MainWindow reacts (e.g., diagnostic mode)
    connect(&dialog, &SettingsDialog::settingsApplied,
            this, [this](const SettingsData& settings) {
                onApplySettings(settings, false);
            });

    int result = dialog.exec();

    if (result == QDialog::Accepted) {
        logger.info("Settings dialog: OK clicked");
        statusBar()->showMessage(tr("Settings applied"), 2000);
    } else {
        logger.info("Settings dialog: Cancel clicked (changes discarded)");
        statusBar()->showMessage(tr("Settings changes discarded"), 2000);
    }
}

void MainWindow::onAbout() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: About Kalahari");

    dialogs::AboutDialog dialog(this);
    dialog.exec();

    logger.info("About dialog closed");
}

void MainWindow::onAboutQt() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: About Qt");

    QMessageBox::aboutQt(this, tr("About Qt"));

    logger.info("About Qt dialog displayed");
}

// Dock management

/// @brief Setup custom title bar widget with icon for QDockWidget (Task #00028)
/// @param dock The dock widget to customize
/// @param iconId Icon command ID (e.g., "view.navigator")
/// @param title Translated title text
/// @note Creates horizontal layout with icon label + title label + float/close buttons
/// @note Since setTitleBarWidget() replaces Qt's default title bar, we add our own buttons
/// @note Stores icon label in m_dockIconLabels for theme refresh
void MainWindow::setupDockTitleBar(QDockWidget* dock, const QString& iconId, const QString& title) {
    auto& artProvider = core::ArtProvider::getInstance();

    // Create custom title bar widget
    QWidget* titleBar = new QWidget(dock);
    QHBoxLayout* layout = new QHBoxLayout(titleBar);
    layout->setContentsMargins(6, 2, 6, 2);
    layout->setSpacing(4);

    // Icon label (16x16 for title bar)
    QLabel* iconLabel = new QLabel(titleBar);
    QIcon icon = artProvider.getIcon(iconId);
    iconLabel->setPixmap(icon.pixmap(16, 16));
    iconLabel->setFixedSize(16, 16);
    iconLabel->setProperty("iconId", iconId);  // Store icon ID for theme refresh (Task #00028)
    layout->addWidget(iconLabel);

    // Store reference for theme refresh (Task #00028)
    m_dockIconLabels.append(iconLabel);

    // Title label
    QLabel* titleLabel = new QLabel(title, titleBar);
    titleLabel->setStyleSheet("font-weight: bold;");
    layout->addWidget(titleLabel);

    // Stretch to push buttons to the right
    layout->addStretch();

    // Float button (if dock is floatable) - OpenSpec #00032: Use themed icons
    QDockWidget::DockWidgetFeatures features = dock->features();
    if (features & QDockWidget::DockWidgetFloatable) {
        QToolButton* floatButton = new QToolButton(titleBar);
        floatButton->setAutoRaise(true);
        floatButton->setIcon(artProvider.getIcon("dock.float"));
        floatButton->setToolTip(QObject::tr("Float"));
        floatButton->setFixedSize(16, 16);
        floatButton->setProperty("iconId", "dock.float");  // Store for theme refresh
        QObject::connect(floatButton, &QToolButton::clicked, dock, [dock]() {
            dock->setFloating(!dock->isFloating());
        });
        layout->addWidget(floatButton);
        m_dockToolButtons.append(floatButton);
    }

    // Close button (if dock is closable) - OpenSpec #00032: Use themed icons
    if (features & QDockWidget::DockWidgetClosable) {
        QToolButton* closeButton = new QToolButton(titleBar);
        closeButton->setAutoRaise(true);
        closeButton->setIcon(artProvider.getIcon("dock.close"));
        closeButton->setToolTip(QObject::tr("Close"));
        closeButton->setFixedSize(16, 16);
        closeButton->setProperty("iconId", "dock.close");  // Store for theme refresh
        QObject::connect(closeButton, &QToolButton::clicked, dock, &QDockWidget::close);
        layout->addWidget(closeButton);
        m_dockToolButtons.append(closeButton);
    }

    dock->setTitleBarWidget(titleBar);
}

/// @brief Refresh all dock title bar icons (Task #00028, OpenSpec #00032)
/// @note Called when theme changes to update icon colors
void MainWindow::refreshDockIcons() {
    auto& artProvider = core::ArtProvider::getInstance();
    auto& logger = core::Logger::getInstance();

    int refreshedCount = 0;

    // Refresh icon labels (panel icons)
    for (QLabel* label : m_dockIconLabels) {
        if (label) {
            QString iconId = label->property("iconId").toString();
            if (!iconId.isEmpty()) {
                label->setPixmap(artProvider.getIcon(iconId).pixmap(16, 16));
                ++refreshedCount;
            }
        }
    }

    // Refresh tool buttons (float/close buttons) - OpenSpec #00032
    for (QToolButton* button : m_dockToolButtons) {
        if (button) {
            QString iconId = button->property("iconId").toString();
            if (!iconId.isEmpty()) {
                button->setIcon(artProvider.getIcon(iconId));
                ++refreshedCount;
            }
        }
    }

    logger.debug("MainWindow: Refreshed {} dock title bar icons", refreshedCount);
}

void MainWindow::createDocks() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating dock widgets");

    // OpenSpec #00033 Phase F: Create wrapper widget for info bar + tabs
    m_centralWrapper = new QWidget(this);
    QVBoxLayout* centralLayout = new QVBoxLayout(m_centralWrapper);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);

    // Create central tabbed workspace (Task #00015)
    m_centralTabs = new QTabWidget(m_centralWrapper);
    m_centralTabs->setTabsClosable(true);       // All tabs can be closed
    m_centralTabs->setMovable(true);            // Tabs can be reordered
    m_centralTabs->setDocumentMode(true);       // Better look on macOS/Windows
    centralLayout->addWidget(m_centralTabs, 1); // Stretch factor 1 to fill space

    // OpenSpec #00033 Phase F: Create standalone info bar at BOTTOM (hidden by default)
    m_standaloneInfoBar = new StandaloneInfoBar(m_centralWrapper);
    m_standaloneInfoBar->hide();
    centralLayout->addWidget(m_standaloneInfoBar);

    // Connect info bar signals
    connect(m_standaloneInfoBar, &StandaloneInfoBar::addToProjectClicked,
            this, &MainWindow::onAddToProject);
    connect(m_standaloneInfoBar, &StandaloneInfoBar::dismissed,
            m_standaloneInfoBar, &QWidget::hide);

    setCentralWidget(m_centralWrapper);

    // Add Dashboard as first tab (default at startup, closable)
    m_dashboardPanel = new DashboardPanel(this);
    int dashboardIndex = m_centralTabs->addTab(m_dashboardPanel, tr("Dashboard"));
    m_centralTabs->setCurrentIndex(dashboardIndex);

    // Connect tab close signal (Task #00015)
    connect(m_centralTabs, &QTabWidget::tabCloseRequested, this, [this](int index) {
        auto& logger = core::Logger::getInstance();
        QWidget* widget = m_centralTabs->widget(index);
        EditorPanel* editor = qobject_cast<EditorPanel*>(widget);

        // Check for unsaved changes if EditorPanel
        if (editor && m_isDirty) {
            // TODO (Phase 1): Prompt user to save changes
            // For now: just close
        }

        // Remove tab and delete widget
        m_centralTabs->removeTab(index);
        widget->deleteLater();

        logger.debug("Tab closed at index {}", index);
    });

    // NOTE (Task #00015): EditorPanel tabs created ON-DEMAND via File > New/Open
    // No m_editorPanel at startup! Use getCurrentEditor() to access active editor.

    // Navigator dock (left)
    m_navigatorPanel = new NavigatorPanel(this);
    m_navigatorDock = new QDockWidget(tr("Navigator"), this);
    m_navigatorDock->setWidget(m_navigatorPanel);
    m_navigatorDock->setObjectName("NavigatorDock");  // Required for saveState!
    setupDockTitleBar(m_navigatorDock, "view.navigator", tr("Navigator"));
    addDockWidget(Qt::LeftDockWidgetArea, m_navigatorDock);

    // Connect Navigator element selection signal (Task #00015, OpenSpec #00033)
    connect(m_navigatorPanel, &NavigatorPanel::elementSelected,
            this, &MainWindow::onNavigatorElementSelected);

    // Properties dock (right)
    m_propertiesPanel = new PropertiesPanel(this);
    m_propertiesDock = new QDockWidget(tr("Properties"), this);
    m_propertiesDock->setWidget(m_propertiesPanel);
    m_propertiesDock->setObjectName("PropertiesDock");
    setupDockTitleBar(m_propertiesDock, "view.properties", tr("Properties"));
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);

    // Log dock (bottom)
    // Pass diagnosticMode to control log level filtering (INFO+ vs ALL)
    m_logPanel = new LogPanel(this, m_diagnosticMode);
    m_logDock = new QDockWidget(tr("Log"), this);
    m_logDock->setWidget(m_logPanel);
    m_logDock->setObjectName("LogDock");
    setupDockTitleBar(m_logDock, "view.log", tr("Log"));
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);

    // Register LogPanel's sink with spdlog logger (OpenSpec #00024)
    // This enables real-time log display in the panel
    core::Logger::getInstance().addSink(m_logPanel->getSink());

    // Connect LogPanel's Options button to open Settings Dialog
    connect(m_logPanel, &LogPanel::openSettingsRequested, this, &MainWindow::onSettings);

    // Hide log panel by default in normal mode, show in diagnostic mode
    if (!m_diagnosticMode) {
        m_logDock->hide();
    }

    // Search dock (right, tabbed with Properties)
    m_searchPanel = new SearchPanel(this);
    m_searchDock = new QDockWidget(tr("Search"), this);
    m_searchDock->setWidget(m_searchPanel);
    m_searchDock->setObjectName("SearchDock");
    setupDockTitleBar(m_searchDock, "view.search", tr("Search"));
    addDockWidget(Qt::RightDockWidgetArea, m_searchDock);
    tabifyDockWidget(m_propertiesDock, m_searchDock);

    // Assistant dock (right, tabbed with Properties/Search)
    m_assistantPanel = new AssistantPanel(this);
    m_assistantDock = new QDockWidget(tr("Assistant"), this);
    m_assistantDock->setWidget(m_assistantPanel);
    m_assistantDock->setObjectName("AssistantDock");
    setupDockTitleBar(m_assistantDock, "view.assistant", tr("Assistant"));
    addDockWidget(Qt::RightDockWidgetArea, m_assistantDock);
    tabifyDockWidget(m_searchDock, m_assistantDock);

    // Raise Properties tab (default visible)
    m_propertiesDock->raise();

    // Get VIEW menu from MenuBuilder (i18n-safe - uses technical name, not translated text)
    if (m_menuBuilder) {
        m_viewMenu = m_menuBuilder->getMenu("VIEW");
        if (m_viewMenu) {
            logger.debug("Found VIEW menu via MenuBuilder::getMenu()");
        }
    }

    if (!m_viewMenu) {
        logger.warn("VIEW menu not found in MenuBuilder! Creating fallback menu.");
        m_viewMenu = menuBar()->addMenu(tr("&View"));
    }

    // Connect panel toggle commands to dock widgets (commands registered in registerCommands)
    auto& registry = CommandRegistry::getInstance();
    auto& artProvider = core::ArtProvider::getInstance();

    // Helper lambda to setup two-way binding between command and dock widget
    auto connectPanelCommand = [&registry](const std::string& cmdId, QDockWidget* dock) {
        Command* cmd = registry.getCommand(cmdId);
        if (cmd) {
            // Set execute callback to toggle dock visibility
            cmd->execute = [dock]() {
                dock->setVisible(!dock->isVisible());
            };
            // Set isChecked callback for checkable state
            cmd->isChecked = [dock]() {
                return dock->isVisible();
            };
        }
    };

    connectPanelCommand("view.navigator", m_navigatorDock);
    connectPanelCommand("view.properties", m_propertiesDock);
    connectPanelCommand("view.log", m_logDock);
    connectPanelCommand("view.search", m_searchDock);
    connectPanelCommand("view.assistant", m_assistantDock);

    // Create Panels submenu with actions from CommandRegistry
    QMenu* panelsSubmenu = m_viewMenu->addMenu(tr("Panels"));
    logger.debug("Created VIEW/Panels submenu for dock toggles");

    // Helper lambda to create panel toggle action with two-way binding
    auto createPanelAction = [&](const std::string& cmdId, QDockWidget* dock) -> QAction* {
        Command* cmd = registry.getCommand(cmdId);
        if (!cmd) {
            logger.warn("Command not found: {}", cmdId);
            return nullptr;
        }

        QAction* action = artProvider.createAction(
            QString::fromStdString(cmdId),
            QString::fromStdString(cmd->label),
            panelsSubmenu,
            core::IconContext::Menu
        );
        action->setCheckable(true);
        action->setChecked(dock->isVisible());
        action->setShortcut(cmd->shortcut.toQKeySequence());

        // Two-way binding: action -> dock
        QObject::connect(action, &QAction::triggered, [dock](bool) {
            dock->setVisible(!dock->isVisible());
        });
        // Two-way binding: dock -> action
        QObject::connect(dock, &QDockWidget::visibilityChanged, [action](bool visible) {
            action->blockSignals(true);
            action->setChecked(visible);
            action->blockSignals(false);
        });

        panelsSubmenu->addAction(action);
        return action;
    };

    // OpenSpec #00030: Panel shortcuts F2-F6 (set in registerCommands)
    m_viewNavigatorAction = createPanelAction("view.navigator", m_navigatorDock);
    m_viewPropertiesAction = createPanelAction("view.properties", m_propertiesDock);
    m_viewLogAction = createPanelAction("view.log", m_logDock);
    m_viewSearchAction = createPanelAction("view.search", m_searchDock);
    m_viewAssistantAction = createPanelAction("view.assistant", m_assistantDock);

    // NOTE: Reset Layout action is registered in CommandRegistry (view.resetLayout)
    // and built by MenuBuilder - no need to add manually here

    // Task #00019: Add toolbar toggle actions to View menu
    if (m_toolbarManager) {
        m_toolbarManager->createViewMenuActions(m_viewMenu);
        logger.debug("Toolbar toggle actions added to VIEW menu");
    }

    logger.debug("Dock widgets created successfully");
}

void MainWindow::resetLayout() {
    auto& logger = core::Logger::getInstance();
    logger.info("Resetting dock layout to default");

    // Remove all docks
    removeDockWidget(m_navigatorDock);
    removeDockWidget(m_propertiesDock);
    removeDockWidget(m_logDock);
    removeDockWidget(m_searchDock);
    removeDockWidget(m_assistantDock);

    // Re-add in default layout
    addDockWidget(Qt::LeftDockWidgetArea, m_navigatorDock);
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
    addDockWidget(Qt::RightDockWidgetArea, m_searchDock);
    addDockWidget(Qt::RightDockWidgetArea, m_assistantDock);

    // Tab right-side docks
    tabifyDockWidget(m_propertiesDock, m_searchDock);
    tabifyDockWidget(m_searchDock, m_assistantDock);

    // Raise Properties tab
    m_propertiesDock->raise();

    // Show all docks
    m_navigatorDock->show();
    m_propertiesDock->show();
    m_logDock->show();
    m_searchDock->show();
    m_assistantDock->show();

    statusBar()->showMessage(tr("Layout reset to default"), 2000);
}

// Perspective save/restore
void MainWindow::closeEvent(QCloseEvent* event) {
    auto& logger = core::Logger::getInstance();
    logger.debug("MainWindow::closeEvent triggered");

    // Check for unsaved changes (Task #00008)
    if (m_isDirty) {
        QString filename = m_currentFilePath.empty()
            ? "Untitled"
            : QString::fromStdString(m_currentFilePath.filename().string());

        auto reply = QMessageBox::question(
            this,
            tr("Unsaved Changes"),
            tr("Do you want to save changes to %1?").arg(filename),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save
        );

        if (reply == QMessageBox::Save) {
            onSaveDocument();
            if (m_isDirty) {
                // Save was cancelled or failed
                event->ignore();
                return;
            }
        } else if (reply == QMessageBox::Cancel) {
            event->ignore();
            return;
        }
        // Discard → continue with close
    }

    // Save perspective (existing code)
    logger.debug("Saving window perspective");

    QSettings settings("Bartosz W. Warzocha & Kalahari Team", "Kalahari");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());

    // Task #00019: Save toolbar state (visibility)
    if (m_toolbarManager) {
        m_toolbarManager->saveState();
    }

    logger.debug("Window perspective saved");

    event->accept();
}

// Helper methods (Task #00008)
void MainWindow::setDirty(bool dirty) {
    m_isDirty = dirty;
    updateWindowTitle();
}

void MainWindow::updateWindowTitle() {
    QString title = "Kalahari - ";

    if (!m_currentFilePath.empty()) {
        QString filename = QString::fromStdString(m_currentFilePath.filename().string());
        title += filename;
    } else {
        title += "Untitled";
    }

    if (m_isDirty) {
        title = "Kalahari - *" + title.mid(11);  // Insert "*" after "Kalahari - "
    }

    setWindowTitle(title);
}

QString MainWindow::getPhase0Content(const core::Document& doc) const {
    const auto& book = doc.getBook();
    const auto& body = book.getBody();

    if (body.empty()) return "";

    const auto& firstPart = body[0];
    const auto& chapters = firstPart->getChapters();

    if (chapters.empty()) return "";

    const auto& firstChapter = chapters[0];
    auto content = firstChapter->getMetadata("_phase0_content");

    return content.has_value() ? QString::fromStdString(content.value()) : "";
}

void MainWindow::setPhase0Content(core::Document& doc, const QString& text) {
    auto& book = doc.getBook();
    auto& body = book.getBody();

    // Create Part if doesn't exist
    if (body.empty()) {
        auto part = std::make_shared<core::Part>("part-001", "Content");
        body.push_back(part);
    }

    auto& firstPart = body[0];
    const auto& chapters = firstPart->getChapters();

    // Create Chapter if doesn't exist
    if (chapters.empty()) {
        auto chapter = std::make_shared<core::BookElement>(
            "chapter", "ch-001", "Chapter 1", ""
        );
        firstPart->addChapter(chapter);  // Use addChapter() instead of push_back()
    }

    // Store text in metadata (chapters vector is const, but elements are mutable)
    const auto& chaptersList = firstPart->getChapters();
    chaptersList[0]->setMetadata("_phase0_content", text.toStdString());
    chaptersList[0]->touch();  // Update modified timestamp
    doc.touch();
}

EditorPanel* MainWindow::getCurrentEditor() {
    // Task #00015: Get active EditorPanel tab (or nullptr)
    if (!m_centralTabs) {
        return nullptr;
    }

    QWidget* currentWidget = m_centralTabs->currentWidget();
    return qobject_cast<EditorPanel*>(currentWidget);  // nullptr if not EditorPanel
}

// =============================================================================
// Project Manager Slots (OpenSpec #00033 Phase D)
// =============================================================================

void MainWindow::onProjectOpened(const QString& projectPath) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();
    const core::Document* doc = pm.getDocument();

    if (!doc) {
        logger.error("onProjectOpened: Document is null");
        return;
    }

    // Update Navigator panel with document structure
    m_navigatorPanel->loadDocument(*doc);

    // Update window title with book title
    setWindowTitle(QString::fromStdString(doc->getTitle()) + " - Kalahari");

    // Log and status bar
    logger.info("Project opened: {}", projectPath.toStdString());
    statusBar()->showMessage(tr("Book opened: %1").arg(projectPath), 3000);
}

void MainWindow::onProjectClosed() {
    auto& logger = core::Logger::getInstance();

    // Clear Navigator panel
    m_navigatorPanel->clearDocument();

    // Reset window title
    setWindowTitle("Kalahari");

    // Phase E: Clear chapter editing state
    m_dirtyChapters.clear();
    m_currentElementId.clear();

    // Log and status bar
    logger.info("Project closed");
    statusBar()->showMessage(tr("Book closed"), 2000);
}

void MainWindow::onNavigatorElementSelected(const QString& elementId, const QString& elementTitle) {
    // Task #00015, OpenSpec #00033 Phase D+E: Handle Navigator element selection
    auto& logger = core::Logger::getInstance();
    logger.info("Navigator element selected: {} (id={})",
                elementTitle.toStdString(), elementId.toStdString());

    // Check if project is loaded via ProjectManager
    auto& pm = core::ProjectManager::getInstance();
    if (!pm.isProjectOpen()) {
        logger.debug("No project loaded - ignoring Navigator selection");
        statusBar()->showMessage(tr("No project loaded"), 2000);
        return;
    }

    // Phase E: Save current chapter if dirty before switching
    if (!m_currentElementId.isEmpty() && m_dirtyChapters.value(m_currentElementId, false)) {
        EditorPanel* currentEditor = getCurrentEditor();
        if (currentEditor) {
            // Update element content cache
            core::BookElement* element = pm.findElement(m_currentElementId);
            if (element) {
                element->setContent(currentEditor->getContent());
                logger.debug("Cached content for element: {}", m_currentElementId.toStdString());
            }
        }
    }

    // Load chapter content from file via ProjectManager
    QString content = pm.loadChapterContent(elementId);

    if (content.isEmpty()) {
        logger.warn("Failed to load content for element: {}", elementId.toStdString());
        // Still create tab but with empty content
    }

    // Create new editor tab with chapter icon
    EditorPanel* newEditor = new EditorPanel(this);
    QIcon chapterIcon = core::ArtProvider::getInstance().getIcon("template.chapter");
    int tabIndex = m_centralTabs->addTab(newEditor, chapterIcon, elementTitle);
    m_centralTabs->setCurrentIndex(tabIndex);

    // Store element ID for save operations
    newEditor->setProperty("elementId", elementId);
    m_currentElementId = elementId;

    // Phase E: Connect textChanged signal for per-chapter dirty tracking
    connect(newEditor->getTextEdit(), &QTextEdit::textChanged,
            this, [this, elementId, elementTitle, newEditor]() {
                auto& pm = core::ProjectManager::getInstance();
                if (pm.isProjectOpen()) {
                    // Mark chapter as dirty
                    if (!m_dirtyChapters.value(elementId, false)) {
                        m_dirtyChapters[elementId] = true;
                        pm.setDirty(true);

                        // Update tab title with asterisk
                        int currentIdx = m_centralTabs->indexOf(newEditor);
                        if (currentIdx >= 0) {
                            QString tabText = m_centralTabs->tabText(currentIdx);
                            if (!tabText.startsWith("*")) {
                                m_centralTabs->setTabText(currentIdx, "*" + tabText);
                            }
                        }
                    }
                }
            });

    // Set content using new setContent method (Phase E)
    newEditor->setContent(content);

    // Update PropertiesPanel to show chapter properties
    m_propertiesPanel->showChapterProperties(elementId);

    logger.info("Opened chapter: {} ({})", elementTitle.toStdString(), elementId.toStdString());
    statusBar()->showMessage(tr("Opened: %1").arg(elementTitle), 2000);
}

// =============================================================================
// Diagnostic Mode (Task #00018)
// =============================================================================

void MainWindow::enableDiagnosticMode() {
    auto& logger = core::Logger::getInstance();
    logger.info("Enabling diagnostic mode");

    m_diagnosticMode = true;
    createDiagnosticMenu();

    // Set Logger level to TRACE to enable ALL log messages (OpenSpec #00024)
    // This must be done BEFORE setDiagnosticMode on LogPanel, otherwise
    // debug/trace messages won't reach the sink
    logger.setLevel(spdlog::level::trace);

    // Show log panel and enable all log levels (OpenSpec #00024)
    if (m_logPanel) {
        m_logPanel->setDiagnosticMode(true);
    }
    if (m_logDock) {
        m_logDock->show();
    }

    statusBar()->showMessage(tr("Diagnostic mode enabled"), 3000);
}

void MainWindow::disableDiagnosticMode() {
    auto& logger = core::Logger::getInstance();
    logger.info("Disabling diagnostic mode");

    removeDiagnosticMenu();
    m_diagnosticMode = false;

    // Only restore log level and hide panel if dev mode is also disabled
    if (!m_devMode) {
        // Restore Logger level based on build type (OpenSpec #00024)
#ifdef NDEBUG
        logger.setLevel(spdlog::level::info);   // Release: info and above
#else
        logger.setLevel(spdlog::level::debug);  // Debug: debug and above
#endif

        // Hide log panel and filter to INFO+ only (OpenSpec #00024)
        if (m_logPanel) {
            m_logPanel->setDiagnosticMode(false);
        }
        if (m_logDock) {
            m_logDock->hide();
        }
    }

    statusBar()->showMessage(tr("Diagnostic mode disabled"), 3000);
}

void MainWindow::onDiagModeChanged(bool enabled) {
    if (enabled) {
        enableDiagnosticMode();
    } else {
        disableDiagnosticMode();
    }
}

void MainWindow::createDiagnosticMenu() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating Diagnostics menu");

    // Don't create if already exists
    if (m_diagnosticMenu) {
        logger.warn("Diagnostics menu already exists");
        return;
    }

    // Create menu (inserted between Help and existing menus)
    m_diagnosticMenu = menuBar()->addMenu(tr("&Diagnostics"));

    // === CATEGORY 1: System Information ===
    QAction* sysInfoAction = m_diagnosticMenu->addAction(tr("System Information"));
    connect(sysInfoAction, &QAction::triggered, this, &MainWindow::onDiagSystemInfo);

    QAction* qtEnvAction = m_diagnosticMenu->addAction(tr("Qt Environment"));
    connect(qtEnvAction, &QAction::triggered, this, &MainWindow::onDiagQtEnvironment);

    QAction* fsCheckAction = m_diagnosticMenu->addAction(tr("File System Check"));
    connect(fsCheckAction, &QAction::triggered, this, &MainWindow::onDiagFileSystemCheck);

    m_diagnosticMenu->addSeparator();

    // === CATEGORY 2: Application State ===
    QAction* settingsDumpAction = m_diagnosticMenu->addAction(tr("Settings Dump"));
    connect(settingsDumpAction, &QAction::triggered, this, &MainWindow::onDiagSettingsDump);

    QAction* memStatsAction = m_diagnosticMenu->addAction(tr("Memory Statistics"));
    connect(memStatsAction, &QAction::triggered, this, &MainWindow::onDiagMemoryStats);

    QAction* openDocsAction = m_diagnosticMenu->addAction(tr("Open Documents Statistics"));
    connect(openDocsAction, &QAction::triggered, this, &MainWindow::onDiagOpenDocsStats);

    m_diagnosticMenu->addSeparator();

    // === CATEGORY 3: Core Systems ===
    QAction* loggerTestAction = m_diagnosticMenu->addAction(tr("Logger Test"));
    connect(loggerTestAction, &QAction::triggered, this, &MainWindow::onDiagLoggerTest);

    QAction* eventBusTestAction = m_diagnosticMenu->addAction(tr("Event Bus Test"));
    connect(eventBusTestAction, &QAction::triggered, this, &MainWindow::onDiagEventBusTest);

    QAction* pluginCheckAction = m_diagnosticMenu->addAction(tr("Plugin Manager Check"));
    connect(pluginCheckAction, &QAction::triggered, this, &MainWindow::onDiagPluginCheck);

    QAction* cmdRegistryDumpAction = m_diagnosticMenu->addAction(tr("Command Registry Dump"));
    connect(cmdRegistryDumpAction, &QAction::triggered, this, &MainWindow::onDiagCommandRegistryDump);

    m_diagnosticMenu->addSeparator();

    // === CATEGORY 4: Python Environment ===
    QAction* pyEnvAction = m_diagnosticMenu->addAction(tr("Python Environment"));
    connect(pyEnvAction, &QAction::triggered, this, &MainWindow::onDiagPythonEnvironment);

    QAction* pyImportAction = m_diagnosticMenu->addAction(tr("Python Import Test"));
    connect(pyImportAction, &QAction::triggered, this, &MainWindow::onDiagPythonImportTest);

    QAction* pyMemoryAction = m_diagnosticMenu->addAction(tr("Python Memory Test"));
    connect(pyMemoryAction, &QAction::triggered, this, &MainWindow::onDiagPythonMemoryTest);

    QAction* pyInterpAction = m_diagnosticMenu->addAction(tr("Embedded Interpreter Status"));
    connect(pyInterpAction, &QAction::triggered, this, &MainWindow::onDiagEmbeddedInterpreterStatus);

    m_diagnosticMenu->addSeparator();

    // === CATEGORY 5: Performance ===
    QAction* perfBenchAction = m_diagnosticMenu->addAction(tr("Performance Benchmark"));
    connect(perfBenchAction, &QAction::triggered, this, &MainWindow::onDiagPerformanceBenchmark);

    QAction* renderStatsAction = m_diagnosticMenu->addAction(tr("Render Statistics"));
    connect(renderStatsAction, &QAction::triggered, this, &MainWindow::onDiagRenderStats);

    m_diagnosticMenu->addSeparator();

    // === CATEGORY 6: Quick Actions ===
    QAction* clearLogAction = m_diagnosticMenu->addAction(tr("Clear Log"));
    connect(clearLogAction, &QAction::triggered, this, &MainWindow::onDiagClearLog);

#ifdef _DEBUG
    m_diagnosticMenu->addSeparator();
    QAction* forceCrashAction = m_diagnosticMenu->addAction(tr("Force Crash (Debug Only)"));
    connect(forceCrashAction, &QAction::triggered, this, &MainWindow::onDiagForceCrash);

    QAction* memLeakAction = m_diagnosticMenu->addAction(tr("Memory Leak Test (Debug Only)"));
    connect(memLeakAction, &QAction::triggered, this, &MainWindow::onDiagMemoryLeakTest);
#endif

    logger.debug("Diagnostics menu created successfully with 18 tools");
}

void MainWindow::removeDiagnosticMenu() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Removing Diagnostics menu");

    if (!m_diagnosticMenu) {
        logger.debug("Diagnostics menu doesn't exist, nothing to remove");
        return;
    }

    // Remove from menu bar
    menuBar()->removeAction(m_diagnosticMenu->menuAction());

    // Delete menu
    delete m_diagnosticMenu;
    m_diagnosticMenu = nullptr;

    logger.debug("Diagnostics menu removed successfully");
}

// =============================================================================
// Diagnostic Tool Implementations (Task #00018)
// =============================================================================

void MainWindow::onDiagSystemInfo() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: System Information ===");
    logger.info("OS: {}", QSysInfo::prettyProductName().toStdString());
    logger.info("Kernel: {}", QSysInfo::kernelType().toStdString() + " " + QSysInfo::kernelVersion().toStdString());
    logger.info("CPU Architecture: {}", QSysInfo::currentCpuArchitecture().toStdString());
    logger.info("Build ABI: {}", QSysInfo::buildAbi().toStdString());
    statusBar()->showMessage(tr("System information logged"), 2000);
}

void MainWindow::onDiagQtEnvironment() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Qt Environment ===");
    logger.info("Qt Version: {}", qVersion());
    logger.info("Qt Build Mode: {}",
#ifdef QT_DEBUG
        "Debug"
#else
        "Release"
#endif
    );
    logger.info("Application: {} {}",
        QCoreApplication::applicationName().toStdString(),
        QCoreApplication::applicationVersion().toStdString());
    logger.info("Organization: {}", QCoreApplication::organizationName().toStdString());
    statusBar()->showMessage(tr("Qt environment logged"), 2000);
}

void MainWindow::onDiagFileSystemCheck() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: File System Check ===");
    logger.info("Working Directory: {}", QDir::currentPath().toStdString());
    logger.info("Application Path: {}", QCoreApplication::applicationDirPath().toStdString());
    logger.info("Temp Path: {}", QDir::tempPath().toStdString());
    logger.info("Home Path: {}", QDir::homePath().toStdString());
    statusBar()->showMessage(tr("File system check logged"), 2000);
}

void MainWindow::onDiagSettingsDump() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Settings Dump ===");
    auto& settings = core::SettingsManager::getInstance();
    logger.info("Theme: {}", settings.getTheme());
    logger.info("Language: {}", settings.getLanguage());
    logger.info("Editor Font: {}", settings.get<std::string>("editor.fontFamily", "N/A"));
    logger.info("Editor Font Size: {}", settings.get<int>("editor.fontSize", 0));
    logger.info("Tab Size: {}", settings.get<int>("editor.tabSize", 0));
    logger.info("Line Numbers: {}", settings.get<bool>("editor.lineNumbers", false));
    logger.info("Word Wrap: {}", settings.get<bool>("editor.wordWrap", false));
    statusBar()->showMessage(tr("Settings dumped to log"), 2000);
}

void MainWindow::onDiagMemoryStats() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Memory Statistics ===");
    logger.info("NOTE: Detailed memory stats require platform-specific code");
    logger.info("Document loaded: {}", m_currentDocument.has_value());
    if (m_currentDocument.has_value()) {
        logger.info("Document dirty: {}", m_isDirty);
        logger.info("Document path: {}", m_currentFilePath.string());
    }
    statusBar()->showMessage(tr("Memory statistics logged"), 2000);
}

void MainWindow::onDiagOpenDocsStats() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Open Documents Statistics ===");
    logger.info("Document loaded: {}", m_currentDocument.has_value());
    if (m_currentDocument.has_value()) {
        logger.info("Dirty flag: {}", m_isDirty);
        logger.info("File path: {}", m_currentFilePath.string());
    }
    logger.info("Open editor tabs: {}", m_centralTabs ? m_centralTabs->count() : 0);
    statusBar()->showMessage(tr("Document statistics logged"), 2000);
}

void MainWindow::onDiagLoggerTest() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Logger Test ===");
    logger.debug("DEBUG level message");
    logger.info("INFO level message");
    logger.warn("WARN level message");
    logger.error("ERROR level message");
    logger.critical("CRITICAL level message");
    logger.info("Logger test complete - check Log Panel for all levels");
    statusBar()->showMessage(tr("Logger test complete"), 2000);
}

void MainWindow::onDiagEventBusTest() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Event Bus Test ===");
    logger.info("NOTE: Event Bus implementation pending (Phase 1)");
    logger.info("Event Bus test will be implemented when Event Bus is ready");
    statusBar()->showMessage(tr("Event Bus test logged"), 2000);
}

void MainWindow::onDiagPluginCheck() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Plugin Manager Check ===");
    logger.info("NOTE: Plugin Manager implementation pending (Phase 2)");
    logger.info("Plugin check will be implemented when Plugin Manager is ready");
    statusBar()->showMessage(tr("Plugin check logged"), 2000);
}

void MainWindow::onDiagCommandRegistryDump() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Command Registry Dump ===");
    logger.info("NOTE: CommandRegistry::dump() not yet implemented");
    logger.info("Command Registry diagnostic will be enhanced in future");
    statusBar()->showMessage(tr("Command Registry dump logged"), 2000);
}

void MainWindow::onDiagPythonEnvironment() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Python Environment ===");
    logger.info("NOTE: Python environment check requires pybind11 integration");
    logger.info("This diagnostic will be implemented in Phase 2 (Plugin System)");
    statusBar()->showMessage(tr("Python environment check logged"), 2000);
}

void MainWindow::onDiagPythonImportTest() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Python Import Test ===");
    logger.info("NOTE: Python import test requires embedded interpreter");
    logger.info("This diagnostic will be implemented in Phase 2 (Plugin System)");
    statusBar()->showMessage(tr("Python import test logged"), 2000);
}

void MainWindow::onDiagPythonMemoryTest() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Python Memory Test ===");
    logger.info("NOTE: Python memory test requires embedded interpreter");
    logger.info("This diagnostic will be implemented in Phase 2 (Plugin System)");
    statusBar()->showMessage(tr("Python memory test logged"), 2000);
}

void MainWindow::onDiagEmbeddedInterpreterStatus() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Embedded Interpreter Status ===");
    logger.info("NOTE: Embedded interpreter check requires pybind11 integration");
    logger.info("This diagnostic will be implemented in Phase 2 (Plugin System)");
    statusBar()->showMessage(tr("Interpreter status logged"), 2000);
}

void MainWindow::onDiagPerformanceBenchmark() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Performance Benchmark ===");
    logger.info("NOTE: Performance benchmark requires implementation");
    logger.info("This will test editor performance, rendering, file I/O, etc.");
    statusBar()->showMessage(tr("Performance benchmark logged"), 2000);
}

void MainWindow::onDiagRenderStats() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Render Statistics ===");
    logger.info("NOTE: Render statistics require Qt rendering metrics");
    logger.info("This diagnostic will show FPS, paint events, update regions, etc.");
    statusBar()->showMessage(tr("Render statistics logged"), 2000);
}

void MainWindow::onDiagClearLog() {
    auto& logger = core::Logger::getInstance();
    logger.info("=== DIAGNOSTIC: Clear Log ===");

    // Clear log panel
    if (m_logPanel) {
        m_logPanel->clear();
        logger.info("Log Panel cleared");
    }

    statusBar()->showMessage(tr("Log cleared"), 2000);
}

#ifdef _DEBUG
void MainWindow::onDiagForceCrash() {
    auto& logger = core::Logger::getInstance();
    logger.critical("=== DIAGNOSTIC: Force Crash (Debug Only) ===");
    logger.critical("User requested application crash - SIMULATING CRITICAL ERROR");

    // Show confirmation
    QMessageBox::StandardButton reply = QMessageBox::critical(this,
        tr("Force Crash"),
        tr("This will IMMEDIATELY crash the application!\n\n"
           "All unsaved work will be LOST.\n\n"
           "Are you sure you want to continue?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        logger.critical("Crashing application NOW!");
        std::abort();  // Immediate crash
    } else {
        logger.info("Crash cancelled by user");
        statusBar()->showMessage(tr("Crash cancelled"), 2000);
    }
}

void MainWindow::onDiagMemoryLeakTest() {
    auto& logger = core::Logger::getInstance();
    logger.warn("=== DIAGNOSTIC: Memory Leak Test (Debug Only) ===");

    // Intentionally leak memory for testing
    const size_t leakSize = 1024 * 1024;  // 1 MB
    char* leak = new char[leakSize];
    (void)leak;  // Suppress unused variable warning

    logger.warn("Leaked {} bytes of memory (intentional)", leakSize);
    logger.info("Use Valgrind/AddressSanitizer to detect this leak");
    statusBar()->showMessage(tr("Memory leak created (1 MB)"), 3000);
}
#endif

// =============================================================================
// Dev Tools Mode (Task #00020)
// =============================================================================

void MainWindow::enableDevMode() {
    auto& logger = core::Logger::getInstance();
    logger.info("Enabling dev mode");

    m_devMode = true;
    createDevToolsMenu();

    // Dev mode also enables full logging like diagnostic mode (OpenSpec #00024)
    logger.setLevel(spdlog::level::trace);

    // Show log panel and enable all log levels
    if (m_logPanel) {
        m_logPanel->setDiagnosticMode(true);
    }
    if (m_logDock) {
        m_logDock->show();
    }

    statusBar()->showMessage(tr("Dev mode enabled"), 3000);
}

void MainWindow::disableDevMode() {
    auto& logger = core::Logger::getInstance();
    logger.info("Disabling dev mode");

    m_devMode = false;
    removeDevToolsMenu();

    // Only restore log level and hide panel if diagnostic mode is also disabled
    if (!m_diagnosticMode) {
        // Restore Logger level based on build type (OpenSpec #00024)
#ifdef NDEBUG
        logger.setLevel(spdlog::level::info);   // Release: info and above
#else
        logger.setLevel(spdlog::level::debug);  // Debug: debug and above
#endif

        // Hide log panel and filter to INFO+ only
        if (m_logPanel) {
            m_logPanel->setDiagnosticMode(false);
        }
        if (m_logDock) {
            m_logDock->hide();
        }
    }

    statusBar()->showMessage(tr("Dev mode disabled"), 3000);
}

void MainWindow::createDevToolsMenu() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating Dev Tools menu");

    // Don't create if already exists
    if (m_devToolsMenu) {
        logger.warn("Dev Tools menu already exists");
        return;
    }

    // Create menu (inserted before Help menu)
    m_devToolsMenu = menuBar()->addMenu(tr("&Dev Tools"));

    // === Icon Downloader ===
    QAction* iconDownloaderAction = m_devToolsMenu->addAction(tr("Icon Downloader"));
    iconDownloaderAction->setToolTip(tr("Download Material Design icons for the project"));
    connect(iconDownloaderAction, &QAction::triggered, this, &MainWindow::onDevToolsIconDownloader);

    logger.info("Dev Tools menu created");
}

void MainWindow::removeDevToolsMenu() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Removing Dev Tools menu");

    if (!m_devToolsMenu) {
        logger.debug("Dev Tools menu doesn't exist, nothing to remove");
        return;
    }

    // Remove from menu bar
    menuBar()->removeAction(m_devToolsMenu->menuAction());

    // Delete menu
    delete m_devToolsMenu;
    m_devToolsMenu = nullptr;

    logger.info("Dev Tools menu removed");
}

// =============================================================================
// Dev Tools Implementations (Task #00020)
// =============================================================================

void MainWindow::onDevToolsIconDownloader() {
    auto& logger = core::Logger::getInstance();
    logger.info("Opening Icon Downloader dialog");

    IconDownloaderDialog dialog(this);
    dialog.exec();

    logger.info("Icon Downloader dialog closed");
}

void MainWindow::onThemeChanged(const core::Theme& theme) {
    auto& logger = core::Logger::getInstance();
    logger.info("MainWindow: Theme changed to '{}'", theme.name);

    // Note: IconRegistry::setThemeColors() is called automatically by ArtProvider
    // when ThemeManager::themeChanged is emitted - no need to call it here

    // OpenSpec #00026: Manual icon refresh REMOVED for menu/toolbar actions
    // Icons now auto-refresh via ArtProvider::resourcesChanged() signal
    // which is connected to all actions created by ArtProvider::createAction()

    // Task #00028: Refresh dock panel title bar icons
    // These are QLabel pixmaps, not QActions, so they need manual refresh
    refreshDockIcons();
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);

    if (m_firstShow) {
        auto& logger = core::Logger::getInstance();
        logger.debug("Restoring window perspective");

        QSettings settings("Bartosz W. Warzocha & Kalahari Team", "Kalahari");
        restoreGeometry(settings.value("geometry").toByteArray());
        restoreState(settings.value("windowState").toByteArray());

        // Task #00019: Restore toolbar state (visibility)
        if (m_toolbarManager) {
            m_toolbarManager->restoreState();
        }

        // OpenSpec #00024: LogDock visibility is INDEPENDENT of saveState/restoreState
        // It depends ONLY on diagnostic mode, not on saved window state
        // Use setVisible() instead of show()/hide() for more reliable behavior
        if (m_logDock) {
            bool shouldShow = m_diagnosticMode || m_devMode;
            m_logDock->setVisible(shouldShow);
            logger.info("LogDock: {} (diagnosticMode={}, devMode={})",
                       shouldShow ? "shown" : "hidden", m_diagnosticMode, m_devMode);
        }

        logger.debug("Window perspective restored");

        m_firstShow = false;
    }
}

// =============================================================================
// Settings Management (SettingsData architecture)
// =============================================================================

SettingsData MainWindow::collectCurrentSettings() const {
    auto& logger = core::Logger::getInstance();
    logger.debug("MainWindow: Collecting current settings");

    auto& settings = core::SettingsManager::getInstance();
    auto& iconRegistry = core::IconRegistry::getInstance();

    SettingsData settingsData;

    // Appearance/General
    settingsData.language = QString::fromStdString(settings.getLanguage());
    settingsData.uiFontSize = settings.get<int>("appearance.uiFontSize", 12);

    // Appearance/Theme
    settingsData.theme = QString::fromStdString(settings.getTheme());
    // Get colors from ArtProvider (which uses IconRegistry's current colors)
    // This ensures we get the user's custom colors, not theme defaults
    auto& artProvider = core::ArtProvider::getInstance();
    settingsData.primaryColor = artProvider.getPrimaryColor();
    settingsData.secondaryColor = artProvider.getSecondaryColor();

    // Appearance/Icons
    settingsData.iconTheme = QString::fromStdString(settings.get<std::string>("appearance.iconTheme", "twotone"));
    const auto& sizes = iconRegistry.getSizes();
    settingsData.iconSizes[core::IconContext::Toolbar] = sizes.toolbar;
    settingsData.iconSizes[core::IconContext::Menu] = sizes.menu;
    settingsData.iconSizes[core::IconContext::TreeView] = sizes.treeView;
    settingsData.iconSizes[core::IconContext::TabBar] = sizes.tabBar;
    settingsData.iconSizes[core::IconContext::Button] = sizes.button;
    settingsData.iconSizes[core::IconContext::StatusBar] = sizes.statusBar;
    settingsData.iconSizes[core::IconContext::ComboBox] = sizes.comboBox;

    // Editor/General
    settingsData.editorFontFamily = QString::fromStdString(settings.get<std::string>("editor.fontFamily", "Consolas"));
    settingsData.editorFontSize = settings.get<int>("editor.fontSize", 12);
    settingsData.tabSize = settings.get<int>("editor.tabSize", 4);
    settingsData.showLineNumbers = settings.get<bool>("editor.lineNumbers", true);
    settingsData.wordWrap = settings.get<bool>("editor.wordWrap", false);

    // Advanced/General
    settingsData.diagnosticMode = m_diagnosticMode;

    // Advanced/Log
    settingsData.logBufferSize = settings.get<int>("log.bufferSize", 500);

    // UI Colors (Task #00028)
    // Load per-theme colors with theme-appropriate defaults
    std::string themeName = settingsData.theme.toStdString();
    bool isDark = (themeName == "Dark");

    // Define UI color defaults (from theme.cpp)
    std::string defToolTipBase = isDark ? "#3c3c3c" : "#ffffdc";
    std::string defToolTipText = isDark ? "#e0e0e0" : "#000000";
    std::string defPlaceholderText = isDark ? "#808080" : "#a0a0a0";
    std::string defBrightText = "#ffffff";

    if (settings.hasCustomUiColorsForTheme(themeName)) {
        settingsData.tooltipBackgroundColor = QColor(QString::fromStdString(
            settings.getUiColorForTheme(themeName, "toolTipBase", defToolTipBase)));
        settingsData.tooltipTextColor = QColor(QString::fromStdString(
            settings.getUiColorForTheme(themeName, "toolTipText", defToolTipText)));
        settingsData.placeholderTextColor = QColor(QString::fromStdString(
            settings.getUiColorForTheme(themeName, "placeholderText", defPlaceholderText)));
        settingsData.brightTextColor = QColor(QString::fromStdString(
            settings.getUiColorForTheme(themeName, "brightText", defBrightText)));
    } else {
        settingsData.tooltipBackgroundColor = QColor(QString::fromStdString(defToolTipBase));
        settingsData.tooltipTextColor = QColor(QString::fromStdString(defToolTipText));
        settingsData.placeholderTextColor = QColor(QString::fromStdString(defPlaceholderText));
        settingsData.brightTextColor = QColor(QString::fromStdString(defBrightText));
    }

    // Log Panel Colors (Task #00027)
    // Define theme defaults
    std::string defTrace = isDark ? "#FF66FF" : "#CC00CC";
    std::string defDebug = isDark ? "#FF66FF" : "#CC00CC";
    std::string defInfo = isDark ? "#FFFFFF" : "#000000";
    std::string defWarning = isDark ? "#FFA500" : "#FF8C00";
    std::string defError = isDark ? "#FF4444" : "#CC0000";
    std::string defCritical = isDark ? "#FF4444" : "#CC0000";
    std::string defBackground = isDark ? "#252525" : "#F5F5F5";

    // Check for corrupted log color settings (all #000000 due to previous bug)
    bool useStoredLogColors = false;
    if (settings.hasCustomLogColorsForTheme(themeName)) {
        std::string storedTrace = settings.getLogColorForTheme(themeName, "trace", defTrace);
        std::string storedWarning = settings.getLogColorForTheme(themeName, "warning", defWarning);
        std::string storedError = settings.getLogColorForTheme(themeName, "error", defError);

        // If trace, warning, AND error are all #000000, data is corrupted
        bool corrupted = (storedTrace == "#000000" && storedWarning == "#000000" && storedError == "#000000");

        if (corrupted) {
            logger.warn("MainWindow: Detected corrupted log colors for theme '{}', clearing", themeName);
            settings.clearCustomLogColorsForTheme(themeName);
            useStoredLogColors = false;
        } else {
            useStoredLogColors = true;
        }
    }

    if (useStoredLogColors) {
        settingsData.logTraceColor = QColor(QString::fromStdString(
            settings.getLogColorForTheme(themeName, "trace", defTrace)));
        settingsData.logDebugColor = QColor(QString::fromStdString(
            settings.getLogColorForTheme(themeName, "debug", defDebug)));
        settingsData.logInfoColor = QColor(QString::fromStdString(
            settings.getLogColorForTheme(themeName, "info", defInfo)));
        settingsData.logWarningColor = QColor(QString::fromStdString(
            settings.getLogColorForTheme(themeName, "warning", defWarning)));
        settingsData.logErrorColor = QColor(QString::fromStdString(
            settings.getLogColorForTheme(themeName, "error", defError)));
        settingsData.logCriticalColor = QColor(QString::fromStdString(
            settings.getLogColorForTheme(themeName, "critical", defCritical)));
        settingsData.logBackgroundColor = QColor(QString::fromStdString(
            settings.getLogColorForTheme(themeName, "background", defBackground)));
    } else {
        settingsData.logTraceColor = QColor(QString::fromStdString(defTrace));
        settingsData.logDebugColor = QColor(QString::fromStdString(defDebug));
        settingsData.logInfoColor = QColor(QString::fromStdString(defInfo));
        settingsData.logWarningColor = QColor(QString::fromStdString(defWarning));
        settingsData.logErrorColor = QColor(QString::fromStdString(defError));
        settingsData.logCriticalColor = QColor(QString::fromStdString(defCritical));
        settingsData.logBackgroundColor = QColor(QString::fromStdString(defBackground));
    }

    // Palette Colors (Task #00028)
    // Define palette color defaults (from theme.cpp)
    std::string defWindow = isDark ? "#2d2d2d" : "#f0f0f0";
    std::string defWindowText = isDark ? "#e0e0e0" : "#000000";
    std::string defBase = isDark ? "#252525" : "#ffffff";
    std::string defAlternateBase = isDark ? "#323232" : "#f5f5f5";
    std::string defTextPalette = isDark ? "#e0e0e0" : "#000000";
    std::string defButton = isDark ? "#404040" : "#e0e0e0";
    std::string defButtonText = isDark ? "#e0e0e0" : "#000000";
    std::string defHighlight = "#0078d4";
    std::string defHighlightedText = "#ffffff";
    std::string defLight = isDark ? "#505050" : "#ffffff";
    std::string defMidlight = isDark ? "#404040" : "#e0e0e0";
    std::string defMid = isDark ? "#303030" : "#a0a0a0";
    std::string defDark = isDark ? "#202020" : "#606060";
    std::string defShadow = "#000000";
    std::string defLink = isDark ? "#5eb3f0" : "#0078d4";
    std::string defLinkVisited = isDark ? "#b48ade" : "#551a8b";

    if (settings.hasCustomPaletteColorsForTheme(themeName)) {
        settingsData.paletteWindowColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "window", defWindow)));
        settingsData.paletteWindowTextColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "windowText", defWindowText)));
        settingsData.paletteBaseColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "base", defBase)));
        settingsData.paletteAlternateBaseColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "alternateBase", defAlternateBase)));
        settingsData.paletteTextColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "text", defTextPalette)));
        settingsData.paletteButtonColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "button", defButton)));
        settingsData.paletteButtonTextColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "buttonText", defButtonText)));
        settingsData.paletteHighlightColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "highlight", defHighlight)));
        settingsData.paletteHighlightedTextColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "highlightedText", defHighlightedText)));
        settingsData.paletteLightColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "light", defLight)));
        settingsData.paletteMidlightColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "midlight", defMidlight)));
        settingsData.paletteMidColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "mid", defMid)));
        settingsData.paletteDarkColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "dark", defDark)));
        settingsData.paletteShadowColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "shadow", defShadow)));
        settingsData.paletteLinkColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "link", defLink)));
        settingsData.paletteLinkVisitedColor = QColor(QString::fromStdString(
            settings.getPaletteColorForTheme(themeName, "linkVisited", defLinkVisited)));
    } else {
        settingsData.paletteWindowColor = QColor(QString::fromStdString(defWindow));
        settingsData.paletteWindowTextColor = QColor(QString::fromStdString(defWindowText));
        settingsData.paletteBaseColor = QColor(QString::fromStdString(defBase));
        settingsData.paletteAlternateBaseColor = QColor(QString::fromStdString(defAlternateBase));
        settingsData.paletteTextColor = QColor(QString::fromStdString(defTextPalette));
        settingsData.paletteButtonColor = QColor(QString::fromStdString(defButton));
        settingsData.paletteButtonTextColor = QColor(QString::fromStdString(defButtonText));
        settingsData.paletteHighlightColor = QColor(QString::fromStdString(defHighlight));
        settingsData.paletteHighlightedTextColor = QColor(QString::fromStdString(defHighlightedText));
        settingsData.paletteLightColor = QColor(QString::fromStdString(defLight));
        settingsData.paletteMidlightColor = QColor(QString::fromStdString(defMidlight));
        settingsData.paletteMidColor = QColor(QString::fromStdString(defMid));
        settingsData.paletteDarkColor = QColor(QString::fromStdString(defDark));
        settingsData.paletteShadowColor = QColor(QString::fromStdString(defShadow));
        settingsData.paletteLinkColor = QColor(QString::fromStdString(defLink));
        settingsData.paletteLinkVisitedColor = QColor(QString::fromStdString(defLinkVisited));
    }

    logger.debug("MainWindow: Settings collected");
    return settingsData;
}

void MainWindow::onApplySettings(const SettingsData& settings, bool /*fromOkButton*/) {
    auto& logger = core::Logger::getInstance();
    logger.info("MainWindow: Reacting to settings applied");

    // Handle diagnostic mode change
    if (settings.diagnosticMode != m_diagnosticMode) {
        if (settings.diagnosticMode) {
            enableDiagnosticMode();
        } else {
            disableDiagnosticMode();
        }
    }

    // Handle log buffer size change
    if (m_logPanel && static_cast<int>(m_logPanel->getMaxBufferSize()) != settings.logBufferSize) {
        m_logPanel->setMaxBufferSize(static_cast<size_t>(settings.logBufferSize));
        logger.info("MainWindow: Log buffer size updated to {}", settings.logBufferSize);
    }

    // Apply log panel color changes (Task #00027)
    if (m_logPanel) {
        m_logPanel->applyThemeColors();
        logger.info("MainWindow: Log panel colors updated");
    }

    logger.info("MainWindow: Settings reaction complete");
}

// =============================================================================
// Standalone File Support (OpenSpec #00033 Phase F)
// =============================================================================

void MainWindow::onOpenStandaloneFile() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Open Standalone File");

    // Show file dialog with supported file types
    QString filename = QFileDialog::getOpenFileName(
        this,
        tr("Open File"),
        QString(),
        tr("Kalahari Files (*.rtf *.kmap *.ktl);;Rich Text Format (*.rtf);;Mind Maps (*.kmap);;Timelines (*.ktl);;All Files (*.*)")
    );

    if (filename.isEmpty()) {
        logger.info("Open standalone file cancelled by user");
        return;
    }

    openStandaloneFile(filename);
}

void MainWindow::openStandaloneFile(const QString& path) {
    auto& logger = core::Logger::getInstance();
    logger.info("Opening standalone file: {}", path.toStdString());

    // Check if file exists
    QFileInfo fileInfo(path);
    if (!fileInfo.exists()) {
        QMessageBox::warning(
            this,
            tr("File Not Found"),
            tr("The file '%1' does not exist.").arg(path)
        );
        logger.error("Standalone file not found: {}", path.toStdString());
        return;
    }

    // Read file content
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(
            this,
            tr("Open Error"),
            tr("Failed to open file: %1\n\n%2").arg(path, file.errorString())
        );
        logger.error("Failed to open standalone file: {} ({})",
                     path.toStdString(), file.errorString().toStdString());
        return;
    }

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    // Create new editor tab with icon based on file extension
    EditorPanel* newEditor = new EditorPanel(this);
    QString tabTitle = fileInfo.fileName();
    QString suffix = fileInfo.suffix().toLower();
    QString iconId;
    if (suffix == "rtf") {
        iconId = "template.chapter";
    } else if (suffix == "kmap") {
        iconId = "book.newMindMap";
    } else if (suffix == "ktl") {
        iconId = "book.newTimeline";
    } else {
        iconId = "common.file";
    }
    QIcon tabIcon = core::ArtProvider::getInstance().getIcon(iconId);
    int tabIndex = m_centralTabs->addTab(newEditor, tabIcon, tabTitle);
    m_centralTabs->setCurrentIndex(tabIndex);

    // Store file path for this tab
    newEditor->setProperty("standaloneFilePath", path);
    newEditor->setProperty("isStandaloneFile", true);

    // Set content
    newEditor->setContent(content);

    // Add to standalone files list
    if (!m_standaloneFilePaths.contains(path)) {
        m_standaloneFilePaths.append(path);
    }

    // Add to Navigator "Other Files" section
    m_navigatorPanel->addStandaloneFile(path);

    // Show info bar for standalone files with context-aware message
    m_standaloneInfoBar->setFilePath(path);
    if (core::ProjectManager::getInstance().isProjectOpen()) {
        m_standaloneInfoBar->setMessage(tr("This file is not part of the current project."));
    } else {
        m_standaloneInfoBar->setMessage(tr("This file is not part of a project. Limited features available."));
    }
    m_standaloneInfoBar->show();

    // Connect textChanged signal for dirty tracking
    connect(newEditor->getTextEdit(), &QTextEdit::textChanged,
            this, [this, path, newEditor]() {
                // Mark tab as dirty
                int currentIdx = m_centralTabs->indexOf(newEditor);
                if (currentIdx >= 0) {
                    QString tabText = m_centralTabs->tabText(currentIdx);
                    if (!tabText.startsWith("*")) {
                        m_centralTabs->setTabText(currentIdx, "*" + tabText);
                    }
                }
            });

    logger.info("Standalone file opened: {}", path.toStdString());
    statusBar()->showMessage(tr("Opened: %1").arg(tabTitle), 2000);
}

void MainWindow::onAddToProject() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Add to Project");

    auto& pm = core::ProjectManager::getInstance();

    // Check if a project is open
    if (!pm.isProjectOpen()) {
        QMessageBox::information(
            this,
            tr("No Project Open"),
            tr("Please open or create a book project first.\n\n"
               "Use File > New Book... or File > Open Book... to start.")
        );
        logger.info("Add to Project: No project open");
        return;
    }

    // Get current standalone file path from info bar
    QString filePath = m_standaloneInfoBar->filePath();
    if (filePath.isEmpty()) {
        logger.warn("Add to Project: No file path in info bar");
        return;
    }

    // Show AddToProjectDialog for copy/move options
    dialogs::AddToProjectDialog dialog(filePath, this);
    if (dialog.exec() == QDialog::Accepted) {
        auto result = dialog.result();

        // Add file to project using ProjectManager
        QString elementId = pm.addChapterToSection(
            result.targetSection,
            result.targetPart,
            result.newTitle,
            filePath,
            result.copyFile
        );

        if (!elementId.isEmpty()) {
            // Success - hide info bar and remove from standalone files
            m_standaloneInfoBar->hide();
            m_navigatorPanel->removeStandaloneFile(filePath);

            // Remove standalone file tab if exists
            for (int i = 0; i < m_centralTabs->count(); ++i) {
                EditorPanel* editor = qobject_cast<EditorPanel*>(m_centralTabs->widget(i));
                if (editor && editor->property("standaloneFilePath").toString() == filePath) {
                    m_centralTabs->removeTab(i);
                    editor->deleteLater();
                    break;
                }
            }

            // Remove from standalone files list
            m_standaloneFilePaths.removeAll(filePath);

            statusBar()->showMessage(
                tr("File added to project: %1").arg(result.newTitle), 3000);
            logger.info("Add to Project: Successfully added {} as {}",
                       filePath.toStdString(), elementId.toStdString());
        } else {
            QMessageBox::warning(this, tr("Error"),
                tr("Failed to add file to project. Check logs for details."));
            logger.error("Add to Project: Failed to add file");
        }
    } else {
        logger.info("Add to Project: User cancelled");
    }
}

void MainWindow::onExportArchive() {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) {
        QMessageBox::information(this, tr("No Project Open"),
            tr("Please open a project first before exporting."));
        return;
    }

    // Get project title for default filename
    QString defaultName = pm.getDocument() ?
        QString::fromStdString(pm.getDocument()->getTitle()) : "project";

    QString outputPath = QFileDialog::getSaveFileName(
        this,
        tr("Export Project Archive"),
        QDir::homePath() + "/" + defaultName + ".klh.zip",
        tr("Kalahari Archive (*.klh.zip)")
    );

    if (outputPath.isEmpty()) return;

    // Create progress dialog
    QProgressDialog progress(tr("Exporting project archive..."), tr("Cancel"), 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(500);

    bool success = pm.exportArchive(outputPath, [&progress](int percent) {
        progress.setValue(percent);
        QApplication::processEvents();
    });

    if (success) {
        QMessageBox::information(this, tr("Export Complete"),
            tr("Project exported successfully to:\n%1").arg(outputPath));
        logger.info("Project exported to: {}", outputPath.toStdString());
    } else {
        QMessageBox::warning(this, tr("Export Failed"),
            tr("Failed to export project archive."));
        logger.error("Failed to export project archive");
    }
}

void MainWindow::onImportArchive() {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    // Select archive to import
    QString archivePath = QFileDialog::getOpenFileName(
        this,
        tr("Import Project Archive"),
        QDir::homePath(),
        tr("Kalahari Archive (*.klh.zip)")
    );

    if (archivePath.isEmpty()) return;

    // Select target directory
    QString targetDir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Destination Folder"),
        QDir::homePath()
    );

    if (targetDir.isEmpty()) return;

    // Check if project folder would already exist
    QFileInfo archiveInfo(archivePath);
    QString projectName = archiveInfo.completeBaseName();
    if (projectName.endsWith(".klh", Qt::CaseInsensitive)) {
        projectName.chop(4);
    }
    QString extractDir = targetDir + "/" + projectName;

    if (QDir(extractDir).exists()) {
        auto reply = QMessageBox::question(this, tr("Folder Exists"),
            tr("A folder named '%1' already exists in the destination.\n"
               "Do you want to choose a different location?").arg(projectName),
            QMessageBox::Yes | QMessageBox::Cancel);
        if (reply == QMessageBox::Yes) {
            onImportArchive();  // Retry
        }
        return;
    }

    // Create progress dialog
    QProgressDialog progress(tr("Importing project archive..."), tr("Cancel"), 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(500);

    bool success = pm.importArchive(archivePath, targetDir, [&progress](int percent) {
        progress.setValue(percent);
        QApplication::processEvents();
    });

    if (success) {
        QMessageBox::information(this, tr("Import Complete"),
            tr("Project imported and opened successfully."));
        logger.info("Project imported from: {}", archivePath.toStdString());
    } else {
        QMessageBox::warning(this, tr("Import Failed"),
            tr("Failed to import project archive."));
        logger.error("Failed to import project archive");
    }
}

} // namespace gui
} // namespace kalahari
