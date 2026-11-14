/// @file main_window.cpp
/// @brief Main application window implementation

#include "kalahari/gui/main_window.h"
#include "kalahari/gui/diagnostic_controller.h"
#include "kalahari/gui/dock_coordinator.h"
#include "kalahari/gui/settings_coordinator.h"
#include "kalahari/gui/shortcut_settings.h"
#include "kalahari/gui/navigator_coordinator.h"
#include "kalahari/gui/document_coordinator.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/gui/annotations_coordinator.h"
#include "kalahari/gui/paragraph_marks_coordinator.h"
#include "kalahari/gui/material_list_coordinator.h"
#include "kalahari/gui/panels/material_list_panel.h"
#include "kalahari/gui/auto_correct_coordinator.h"
#include "kalahari/gui/special_character_coordinator.h"
#include "kalahari/gui/edit_coordinator.h"
#include "kalahari/gui/versions_coordinator.h"
#include "kalahari/gui/style_coordinator.h"
#include "kalahari/gui/book_export_coordinator.h"
#include "kalahari/gui/caption_coordinator.h"
#include "kalahari/gui/footnote_coordinator.h"
#include "kalahari/gui/book_search_coordinator.h"
#include "kalahari/gui/chapter_break_coordinator.h"
#include "kalahari/gui/image_filter_coordinator.h"
#include "kalahari/gui/paragraph_style_menu.h"
#include "kalahari/gui/toolbar_keys.h"
#include "kalahari/gui/spelling_coordinator.h"
#include "kalahari/gui/grammar_coordinator.h"
#include "kalahari/gui/spelling_panel_coordinator.h"
#include "kalahari/gui/repetitions_coordinator.h"
#include "kalahari/gui/writing_stats_coordinator.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/read_only_book.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/dialogs/about_dialog.h"
#include "kalahari/gui/dialogs/icon_downloader_dialog.h"
#include "kalahari/gui/dialogs/image_properties_dialog.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dialogs/new_item_dialog.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/project_database.h"
#include "kalahari/gui/menu_builder.h"
#include "kalahari/gui/recent_books_menu.h"
#include "kalahari/gui/panels/dashboard_panel.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/image_preview_panel.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/text_images.h"
#include "kalahari/editor/paragraph_styles.h"
#include "kalahari/editor/quick_insert_controller.h"
#include "kalahari/editor/view_modes.h"
#include "kalahari/editor/editor_types.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/panels/paragraph_panel.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/panels/log_panel.h"
#include "kalahari/gui/widgets/color_menu.h"
#include "kalahari/gui/widgets/standalone_info_bar.h"
#include "kalahari/gui/utils/access_keys.h"
#include "kalahari/gui/utils/distraction_free_layout.h"
#include "kalahari/gui/utils/document_commands.h"
#include "kalahari/gui/utils/layout_utils.h"
#include "kalahari/gui/utils/setting_toggle.h"
#include "kalahari/gui/utils/tab_focus.h"
#include "kalahari/gui/utils/chapter_cursor.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/log_panel_sink.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/core/icon_registry.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/core/theme.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/recent_books_manager.h"
#include "kalahari/gui/widgets/status_bar_statistics.h"
#include <QActionGroup>
#include <QLayout>
#include <QPointer>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QTabBar>
#include <QApplication>
#include <QSettings>
#include <QDockWidget>
#include <QCloseEvent>
#include <QShowEvent>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QTextEdit>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolButton>
#include <QScreen>
#include <QStyle>
#include <QTimer>
#include <QAbstractItemView>
#include <QFont>
#include <algorithm>
#include <map>
#include <memory>
#include <utility>

namespace kalahari {
namespace gui {

namespace {

/// How long the status bar keeps why the open book is read-only, or why a change was refused
constexpr int READ_ONLY_MESSAGE_MS = 5000;

/// The commands of the Book menu that add an element of a kind of the Workshop, with the id of
/// the kind in the base package
constexpr std::pair<const char*, const char*> WORKSHOP_COMMANDS[] = {
    {"book.newCharacter", "character"}, {"book.newLocation", "location"},
    {"book.newItem", "item"},           {"book.newMindMap", "mindmap"},
    {"book.newTimeline", "timeline"},
};

/// The kind @p kindId of the Workshop of the base package; none when the program has none
core::KindRef workshopKind(const QString& kindId) {
    return core::ProjectManager::installedBookTypes().findKind(
        QString::fromLatin1(core::ProjectManager::BASE_PACKAGE), kindId);
}

/// Why the objects of the Insert menu and tables are greyed out in a cell of a table
QString inTableReason() {
    //: Why Insert > Horizontal Line, Scene Break, Page Break and Table are greyed out with the
    //: cursor in a table. In Polish: Komórka tabeli może zawierać tylko tekst
    return MainWindow::tr("A cell of a table holds only text");
}

/// The commands of the Table menu that work in the table with the cursor
constexpr const char* TABLE_COMMANDS[] = {
    "table.insertRowsAbove", "table.insertRowsBelow", "table.insertColumnsLeft",
    "table.insertColumnsRight", "table.deleteRows",   "table.deleteColumns",
    "table.deleteTable",       "table.splitCells",    "table.distributeRows",
    "table.distributeColumns", "table.tableToText",   "table.properties"};

/// The commands of the Table menu that select a part of the table
constexpr const char* TABLE_SELECT_COMMANDS[] = {"table.selectCell", "table.selectRow",
                                                  "table.selectColumn", "table.selectTable"};

/// The commands of the Image menu that change the selected image or open its window
constexpr const char* IMAGE_COMMANDS[] = {
    "image.properties",      "image.addCaption",     "image.altText",
    "image.rotateRight",     "image.rotateLeft",     "image.flipHorizontal",
    "image.flipVertical",    "image.rotateOptions",  "image.filterNone",
    "image.filterGrayscale", "image.filterSepia",    "image.filterBlackWhite",
    "image.filterWash",      "image.filterAll",      "image.colorOptions"};

/// The commands of Image > Filter and their filters
constexpr std::pair<const char*, editor::ImageFilter> IMAGE_FILTER_COMMANDS[] = {
    {"image.filterNone", editor::ImageFilter::None},
    {"image.filterGrayscale", editor::ImageFilter::Grayscale},
    {"image.filterSepia", editor::ImageFilter::Sepia},
    {"image.filterBlackWhite", editor::ImageFilter::BlackWhite},
    {"image.filterWash", editor::ImageFilter::Wash}};

/// The selected image of a text, as it is
std::optional<editor::ImageProperties> selectedImageOf(const editor::BookEditor& text) {
    const std::optional<int> position = text.selectedImage();
    return position && text.textDocument() != nullptr
               ? editor::imageAt(*text.textDocument(), *position)
               : std::nullopt;
}

/// The book windows of the program, in the order they were made
QList<MainWindow*>& bookWindowList() {
    static QList<MainWindow*> windows;
    return windows;
}

/// The book window in front last (a window of its own of its tabs counts as it)
QPointer<MainWindow>& lastFrontWindow() {
    static QPointer<MainWindow> window;
    return window;
}

/// File > Exit is closing every window: they asked about their changes together
bool& closingAllWindows() {
    static bool closing = false;
    return closing;
}

/// How far a new window is from the window it came from, to the right and down
constexpr int NEW_WINDOW_OFFSET = 32;

/// The panel of a text, whose windows are its own: the one in front without one
EditorPanel* panelOf(const editor::BookEditor& text, EditorPanel* inFront) {
    for (QWidget* widget = text.parentWidget(); widget != nullptr;
         widget = widget->parentWidget()) {
        if (auto* panel = qobject_cast<EditorPanel*>(widget)) {
            return panel;
        }
    }
    return inFront;
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_book(std::make_unique<core::ProjectManager>())
    , m_commands(std::make_unique<CommandRegistry>())
    , m_fileMenu(nullptr)
    , m_editMenu(nullptr)
    , m_viewMenu(nullptr)
    , m_dockCoordinator(nullptr)
    , m_firstShow(true)
    , m_diagnosticController(nullptr)
    , m_settingsCoordinator(nullptr)
    , m_navigatorCoordinator(nullptr)
    , m_documentCoordinator(nullptr)
{
    auto& logger = core::Logger::getInstance();
    logger.info("MainWindow constructor called");

    // Set window properties: no book is open yet, so the title is the name of the program
    // alone, as after a book is closed (updateWindowTitle())
    setWindowTitle(QStringLiteral("Kalahari"));
    utils::resizeWithinScreen(this, QSize(1280, 720));

    // Create UI components (Command Registry pattern)
    registerCommands();    // Register all commands with CommandRegistry
    createMenus();         // Build menus from CommandRegistry
    createToolbars();      // Build toolbars from CommandRegistry
    createStatusBar();
    createDocks();

    // Create DiagnosticController after docks (needs LogPanel and LogDock from DockCoordinator)
    // OpenSpec #00038 - Diagnostic/dev mode extracted from MainWindow
    m_diagnosticController = new DiagnosticController(
        this,
        m_dockCoordinator->logPanel(),
        m_dockCoordinator->logDock(),
        statusBar(),
        this
    );
    logger.debug("MainWindow: DiagnosticController created");

    // Create SettingsCoordinator after docks (needs DockCoordinator for panel access)
    // OpenSpec #00038 Phase 5 - Settings management extracted from MainWindow
    m_settingsCoordinator =
        new SettingsCoordinator(*m_book, *m_commands, this, m_dockCoordinator, statusBar(), this);
    // Set diagnostic mode getter so SettingsCoordinator can check current state
    m_settingsCoordinator->setDiagnosticModeGetter([this]() { return isDiagnosticMode(); });
    // Connect diagnostic mode signals
    connect(m_settingsCoordinator, &SettingsCoordinator::enableDiagnosticModeRequested,
            this, &MainWindow::enableDiagnosticMode);
    connect(m_settingsCoordinator, &SettingsCoordinator::disableDiagnosticModeRequested,
            this, &MainWindow::disableDiagnosticMode);
    // The gear of a dialog opens the settings at the page of its settings, over the dialog,
    // with the Settings of the book window the dialog is over
    dialogs::KalahariDialog::setSettingsOpener([](QWidget* dialog, const QString& page) {
        MainWindow* window = windowOf(dialog);
        if (window == nullptr) {
            window = frontWindow();
        }
        if (window != nullptr && window->m_settingsCoordinator != nullptr) {
            window->m_settingsCoordinator->openSettingsPage(page, dialog);
        }
    });
    // Restarting for another language closes every window, asking once
    m_settingsCoordinator->setProgramCloser([this]() { return closeAllWindows(this); });
    logger.debug("MainWindow: SettingsCoordinator created");

    // Create NavigatorCoordinator after docks (needs DockCoordinator for panel/dock access)
    // OpenSpec #00038 Phase 6 - Navigator handlers extracted from MainWindow
    m_navigatorCoordinator = new NavigatorCoordinator(
        *m_book, *m_commands, m_dockCoordinator->navigatorPanel(),
        m_dockCoordinator->propertiesPanel(), m_dockCoordinator->centralTabs(), statusBar(), this);
    logger.debug("MainWindow: NavigatorCoordinator created");

    // Connect DockCoordinator navigator signals to NavigatorCoordinator
    // These connections must be made after NavigatorCoordinator is created
    connect(m_dockCoordinator, &DockCoordinator::navigatorRequestRename,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestRename);
    connect(m_dockCoordinator, &DockCoordinator::navigatorRequestDelete,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestDelete);
    connect(m_dockCoordinator, &DockCoordinator::navigatorRequestMove,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestMove);
    connect(m_dockCoordinator, &DockCoordinator::navigatorRequestProperties,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestProperties);
    connect(m_dockCoordinator, &DockCoordinator::navigatorRequestSectionProperties,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestSectionProperties);
    connect(m_dockCoordinator, &DockCoordinator::navigatorRequestPartProperties,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestPartProperties);
    connect(m_dockCoordinator, &DockCoordinator::elementMoved,
            m_navigatorCoordinator, &NavigatorCoordinator::onElementMoved);
    // Move...: any place of the book its kind can have, in the Move dialog or dragged there
    connect(m_dockCoordinator->navigatorPanel(), &NavigatorPanel::requestMoveTo,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestMoveTo);
    connect(m_dockCoordinator->navigatorPanel(), &NavigatorPanel::elementMovedTo,
            m_navigatorCoordinator, &NavigatorCoordinator::onElementMovedTo);
    // A chapter title changed in the Properties panel shows on its tab too
    connect(m_dockCoordinator, &DockCoordinator::chapterStatusChanged,
            m_navigatorCoordinator, &NavigatorCoordinator::refreshTabTitle);
    // ... and whether it starts on a left page, in the margins of its pages
    connect(m_dockCoordinator, &DockCoordinator::chapterFirstPageChanged,
            m_navigatorCoordinator, &NavigatorCoordinator::refreshFirstPage);
    // Add... on a node of the Navigator: the Add Element window with the kinds of the node
    connect(m_dockCoordinator->navigatorPanel(), &NavigatorPanel::requestAdd,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestAdd);
    // The nodes of the book and of the Workshop: their names, and the groups of the Workshop
    connect(m_dockCoordinator->navigatorPanel(), &NavigatorPanel::requestRenameNode,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestRenameNode);
    connect(m_dockCoordinator->navigatorPanel(), &NavigatorPanel::requestWorkshopGrouped,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestWorkshopGrouped);
    // A material of the Workshop opens as the settings say: on its preview tab or in the
    // program the system gives its file. Its card in the Properties panel opens it both ways
    // and opens the elements it is about; the page of an element opens its materials.
    connect(m_dockCoordinator->navigatorPanel(), &NavigatorPanel::requestOpenMaterial,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestOpenMaterial);
    connect(m_dockCoordinator->propertiesPanel(), &PropertiesPanel::requestOpenMaterial,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestOpenMaterial);
    connect(m_dockCoordinator->propertiesPanel(), &PropertiesPanel::requestPreview,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestPreview);
    connect(m_dockCoordinator->propertiesPanel(), &PropertiesPanel::requestOpenWithSystem,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestOpenWithSystem);
    connect(m_dockCoordinator->propertiesPanel(), &PropertiesPanel::requestOpenLink,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestOpenLink);
    // A material with an address: its page in the browser, the copy of its picture; a change
    // on one card of a material shows on the others
    connect(m_dockCoordinator->propertiesPanel(), &PropertiesPanel::requestOpenInBrowser,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestOpenInBrowser);
    connect(m_dockCoordinator->propertiesPanel(), &PropertiesPanel::requestSaveCopy,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestSaveCopy);
    connect(m_dockCoordinator->propertiesPanel(), &PropertiesPanel::materialChanged,
            m_navigatorCoordinator, &NavigatorCoordinator::onMaterialChanged);
    connect(m_dockCoordinator->propertiesPanel(), &PropertiesPanel::message, this,
            [this](const QString& text) { statusBar()->showMessage(text, 2000); });
    // The card of a material in the middle of the window; files, web addresses and pictures
    // dropped on the Navigator or pasted in it are new materials
    connect(m_dockCoordinator->navigatorPanel(), &NavigatorPanel::requestOpenMaterialCard,
            m_navigatorCoordinator,
            [this](const QString& elementId) {
                m_navigatorCoordinator->onRequestOpenMaterialCard(elementId);
            });
    connect(m_dockCoordinator->navigatorPanel(), &NavigatorPanel::requestAddMaterials,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestAddMaterials);
    // The sections of the book: shown or not, and their names
    connect(m_dockCoordinator->navigatorPanel(), &NavigatorPanel::requestShowSections,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestShowSections);
    connect(m_dockCoordinator->navigatorPanel(), &NavigatorPanel::requestRenameSection,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestRenameSection);
    // The files no element of the book has: shown, added back to the book or deleted
    connect(m_dockCoordinator->navigatorPanel(), &NavigatorPanel::requestPreviewUnassigned,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestPreviewUnassigned);
    connect(m_dockCoordinator->navigatorPanel(), &NavigatorPanel::requestAddUnassigned,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestAddUnassigned);
    connect(m_dockCoordinator->navigatorPanel(), &NavigatorPanel::requestDeleteUnassigned,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestDeleteUnassigned);
    connect(m_dockCoordinator->navigatorPanel(), &NavigatorPanel::requestDeleteAllUnassigned,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestDeleteAllUnassigned);
    connect(m_dockCoordinator->propertiesPanel(), &PropertiesPanel::requestSections,
            m_navigatorCoordinator, &NavigatorCoordinator::onRequestSections);
    // The Navigator shows the title of the book and the names of its sections in its language
    connect(m_dockCoordinator->propertiesPanel(), &PropertiesPanel::bookChanged,
            m_navigatorCoordinator, &NavigatorCoordinator::refreshNavigator);
    // Connect NavigatorCoordinator dirty state signal to NavigatorPanel (OpenSpec #00042 Phase 7.5)
    connect(m_navigatorCoordinator, &NavigatorCoordinator::chapterDirtyStateChanged,
            m_dockCoordinator->navigatorPanel(), &NavigatorPanel::setElementModified);
    // A chapter opens where its cursor was when it was closed last
    connect(m_navigatorCoordinator, &NavigatorCoordinator::elementOpened,
            this, &MainWindow::placeOpenedChapterCursor);
    logger.debug("MainWindow: Connected DockCoordinator navigator signals to NavigatorCoordinator");

    // Create DocumentCoordinator after NavigatorCoordinator (needs it for dirty chapter tracking)
    // OpenSpec #00038 Phase 7 - Document operations extracted from MainWindow
    m_documentCoordinator = new DocumentCoordinator(
        *m_book, *m_commands, this, m_dockCoordinator->centralTabs(),
        m_dockCoordinator->navigatorPanel(), m_dockCoordinator->propertiesPanel(),
        m_dockCoordinator->dashboardPanel(), m_navigatorCoordinator,
        m_dockCoordinator->standaloneInfoBar(), statusBar(), [this]() { updateWindowTitle(); },
        [this]() {
            // An annotation or a note being written is kept before the documents are saved
            // or closed
            if (m_annotationsCoordinator) {
                m_annotationsCoordinator->finishWriting();
            }
            if (m_footnoteCoordinator) {
                m_footnoteCoordinator->finishWriting();
            }
            return hasUnsavedChanges();
        },
        this);
    // Connect DocumentCoordinator signals
    connect(m_documentCoordinator, &DocumentCoordinator::windowTitleChanged,
            this, &MainWindow::setWindowTitle);
    // Save, Save As and Save All are greyed out while they have nothing to save, and say why;
    // the tabs and the open book update them (updateEditorActionStates(),
    // updateBookCommands(), a closed tab). Their keys, and those of the commands of the open
    // book, say it in the status bar then.
    {
        CommandRegistry& registry = commands();
        utils::bindSaveCommands(
            registry,
            [this]() {
                return m_documentCoordinator != nullptr ? m_documentCoordinator->whyNotSave()
                                                        : QString();
            },
            [this]() {
                return m_documentCoordinator != nullptr ? m_documentCoordinator->whyNotSaveAs()
                                                        : QString();
            },
            [this]() {
                return m_documentCoordinator != nullptr ? m_documentCoordinator->whyNotSaveAll()
                                                        : QString();
            });
        utils::explainGreyedKeys(registry, this, statusBar());
    }
    // A file outside the book opened from the Navigator gets the keys, as a chapter does
    // (connected after the coordinator, which opens it); a new file, not saved yet, is
    // listed by its key
    connect(m_dockCoordinator->navigatorPanel(), &NavigatorPanel::standaloneFileSelected, this,
            [this](const QString& path) {
                const EditorPanel* editor = getCurrentEditor();
                if (editor != nullptr
                    && (editor->property("standaloneFilePath").toString() == path
                        || editor->property("newFileKey").toString() == path)) {
                    focusCurrentText();
                }
            });

    // Distraction-Free writing hides the window's bars, panels and tabs and brings them
    // back (also with Esc); the editors and the commands follow it
    m_distractionFreeLayout = new utils::DistractionFreeLayout(this);
    connect(m_distractionFreeLayout, &utils::DistractionFreeLayout::activeChanged,
            this, &MainWindow::onDistractionFreeChanged);
    connect(&commands(), &CommandRegistry::shortcutsChanged,
            m_distractionFreeLayout, &utils::DistractionFreeLayout::updateShortcuts);

    logger.debug("MainWindow: DocumentCoordinator created");

    // The statistics bar above the tabs of the texts, and the record of writing of the book
    // that opens (after the documents, which make the collector of its editors)
    m_writingStatsCoordinator = new WritingStatsCoordinator(
        *m_book, *m_commands, m_dockCoordinator, m_documentCoordinator,
        [this](const QString& page) { m_settingsCoordinator->openSettingsPage(page); }, this);
    m_writingStatsCoordinator->connectCommands();
    // The time of writing and the days in a row after the reading time, and the summary of
    // the session over this window
    m_writingStatsCoordinator->attachStatusBar(statusBar());
    m_writingStatsCoordinator->setSummaryWindow(this);

    // The annotation commands and the Annotations panel; a chapter is opened as from the
    // Navigator
    m_annotationsCoordinator = new AnnotationsCoordinator(
        *m_book, *m_commands, m_dockCoordinator->annotationsPanel(),
        m_dockCoordinator->annotationsDock(), m_dockCoordinator->centralTabs(),
        [this](const QString& elementId) {
            if (const core::ProjectElement* element =
                    book().findElement(elementId)) {
                m_navigatorCoordinator->onElementSelected(elementId, element->title);
            }
        },
        statusBar(), this);
    m_annotationsCoordinator->connectCommands();
    // Card Appearance and Colors... in the menu of a card opens the Annotations page
    connect(m_annotationsCoordinator, &AnnotationsCoordinator::settingsPageRequested, this,
            [this](const QString& page) {
                if (m_settingsCoordinator) m_settingsCoordinator->openSettingsPage(page);
            });

    // The paperclips beside the paragraphs that materials are linked with: one chosen shows
    // its material in the Properties panel, brought to the front
    m_paragraphMarksCoordinator = new ParagraphMarksCoordinator(
        *m_book, m_dockCoordinator->centralTabs(),
        [this](const QString& materialId) {
            m_dockCoordinator->propertiesPanel()->showMaterialProperties(materialId);
            QDockWidget* dock = m_dockCoordinator->propertiesDock();
            dock->show();
            dock->raise();
            if (dock->isFloating()) {
                dock->activateWindow();
            }
        },
        this);

    // The lists of the kinds of materials on their tabs: a kind of the Materials clicked in
    // the Navigator, or Materials itself. A material chosen in a list shows in the Properties
    // panel, brought to the front; Alt+Enter gives it the keys.
    m_materialListCoordinator = new MaterialListCoordinator(
        *m_book, m_dockCoordinator->centralTabs(), m_navigatorCoordinator, statusBar(),
        [this](bool keys) {
            QDockWidget* dock = m_dockCoordinator->propertiesDock();
            dock->show();
            dock->raise();
            if (keys) {
                if (dock->isFloating()) {
                    dock->activateWindow();
                }
                m_dockCoordinator->propertiesPanel()->focusFirstField();
            }
        },
        this);
    connect(m_dockCoordinator->navigatorPanel(), &NavigatorPanel::requestOpenMaterialList,
            m_materialListCoordinator, &MaterialListCoordinator::openList);
    // Hide Kind in the menu of a kind of the Navigator; Kind Settings... there and the gear of
    // a list: the materials in Settings > Navigator
    connect(m_dockCoordinator->navigatorPanel(), &NavigatorPanel::requestHideKind,
            m_materialListCoordinator, &MaterialListCoordinator::hideKind);
    const auto openMaterialSettings = [this]() {
        if (m_settingsCoordinator) {
            m_settingsCoordinator->openSettingsPage(
                QString::fromLatin1(SettingsDialog::NAVIGATOR_PAGE));
        }
    };
    connect(m_dockCoordinator->navigatorPanel(), &NavigatorPanel::requestMaterialSettings, this,
            openMaterialSettings);
    connect(m_materialListCoordinator, &MaterialListCoordinator::settingsRequested, this,
            openMaterialSettings);
    // Save Version and Version History of the text in front; a text is opened as from the
    // Navigator, and the bar under it says that a version was restored
    m_versionsCoordinator = new VersionsCoordinator(
        *m_book, *m_commands, m_dockCoordinator->centralTabs(),
        [this](const QString& elementId) {
            if (const core::ProjectElement* element =
                    book().findElement(elementId)) {
                m_navigatorCoordinator->onElementSelected(elementId, element->title);
            }
        },
        [this](EditorPanel* editor, const QString& message, const QString& actionText,
               const QString& actionToolTip, std::function<void()> action) {
            m_documentCoordinator->showNotice(editor, message, actionText, actionToolTip,
                                              std::move(action));
        },
        statusBar(), this, this);
    m_versionsCoordinator->connectCommands();
    // Format > Text Style: new styles of the book, the one at the cursor changed or deleted,
    // Manage Styles...; the bar under the text says that a style was deleted, with Undo
    m_styleCoordinator = new StyleCoordinator(
        *m_book, *m_commands, m_dockCoordinator->centralTabs(),
        [this](EditorPanel* editor, const QString& message, const QString& actionText,
               const QString& actionToolTip, std::function<void()> action) {
            m_documentCoordinator->showNotice(editor, message, actionText, actionToolTip,
                                              std::move(action));
        },
        statusBar(), this, this);
    m_styleCoordinator->connectCommands();
    // Insert > Caption..., Cross-Reference... and List of Figures, and the numbers of the
    // captions of every open text in the book around it; Ctrl+click on a caption of another
    // text opens it, as from the Navigator
    m_captionCoordinator = new CaptionCoordinator(
        *m_book, *m_commands, m_dockCoordinator->centralTabs(),
        [this](const QString& elementId) {
            if (const core::ProjectElement* element =
                    book().findElement(elementId)) {
                m_navigatorCoordinator->onElementSelected(elementId, element->title);
            }
        },
        this, this);
    m_captionCoordinator->connectCommands();
    // A text opened, or the book's order of texts changed: other numbers
    connect(m_navigatorCoordinator, &NavigatorCoordinator::elementOpened, m_captionCoordinator,
            [this]() { m_captionCoordinator->refreshSoon(); });
    connect(m_navigatorCoordinator, &NavigatorCoordinator::refreshNavigatorRequested,
            m_captionCoordinator, [this]() { m_captionCoordinator->refreshSoon(); });
    // Image > Filter > Same Filter for All Images...; the bar under the text says how many
    // images changed, with Undo
    m_imageFilterCoordinator = new ImageFilterCoordinator(
        *m_book, m_dockCoordinator->centralTabs(),
        [this](EditorPanel* editor, const QString& message, const QString& actionText,
               const QString& actionToolTip, std::function<void()> action) {
            m_documentCoordinator->showNotice(editor, message, actionText, actionToolTip,
                                              std::move(action));
        },
        statusBar(), this, this);
    // The buttons and the menu of the palette, in both places of the Paragraph and Styles panel
    for (ParagraphPanel* palette : {m_dockCoordinator->paragraphPanel(),
                                    m_dockCoordinator->propertiesParagraphPanel()}) {
        if (palette != nullptr) {
            palette->setStyleCoordinator(m_styleCoordinator);
        }
    }
    // Version History... of a text in the Navigator, and the buttons of the group of the
    // versions in the Properties panel: the text opens, if it is not open
    connect(m_dockCoordinator->navigatorPanel(), &NavigatorPanel::requestVersionHistory,
            m_versionsCoordinator,
            [this](const QString& elementId) { m_versionsCoordinator->showHistory(elementId); });
    connect(m_dockCoordinator->propertiesPanel(), &PropertiesPanel::requestSaveVersion,
            m_versionsCoordinator,
            [this](const QString& elementId) { m_versionsCoordinator->saveVersion(elementId); });
    connect(m_dockCoordinator->propertiesPanel(), &PropertiesPanel::requestVersionHistory,
            m_versionsCoordinator,
            [this](const QString& elementId) { m_versionsCoordinator->showHistory(elementId); });
    // The gear of the group of the versions: their settings
    connect(m_dockCoordinator->propertiesPanel(), &PropertiesPanel::requestVersionSettings, this,
            [this]() {
                if (m_settingsCoordinator) {
                    m_settingsCoordinator->openSettingsPage(
                        QString::fromLatin1(SettingsDialog::VERSIONS_PAGE));
                }
            });

    // Undo, Redo, Cut, Copy, Paste and Select All work on the element with the keys; what
    // is done in the Annotations panel is undone in the text in front
    m_editCoordinator = new EditCoordinator(
        *m_commands,
        [this]() -> editor::BookEditor* {
            EditorPanel* panel = getCurrentEditor();
            return panel != nullptr ? panel->getBookEditor() : nullptr;
        },
        this);
    m_editCoordinator->connectCommands();

    // Select Word, Select Paragraph, the objects and the formatting characters of the Insert
    // menu and the pastes in each mode work in the book's text with the keys: in a field (the
    // find bar, the Navigator's filter) they are greyed out, and Ctrl+Enter stays with the
    // field. Over a chapter of a book open read-only the objects, the characters and the
    // pastes are greyed out, saying why
    m_editCoordinator->addTextCommands({"edit.selectWord", "edit.selectParagraph"},
                                       [](const editor::BookEditor&) { return true; });
    // The objects not in a cell of a table, which holds only text
    const std::vector<std::string> objectCommands = {
        "insert.horizontalLine", "insert.sceneBreak", "insert.pageBreak", "book.sceneBreak"};
    const std::vector<std::string> characterCommands = {
        "insert.specialChar", "insert.quickCharacter", "insert.noBreakSpace",
        "insert.nonBreakingHyphen", "insert.optionalHyphen", "insert.enDash", "insert.emDash",
        "insert.quickInsert"};
    m_editCoordinator->addTextCommands(objectCommands, [](const editor::BookEditor& text) {
        return !text.isReadOnly() && !text.isInTable();
    });
    m_editCoordinator->addTextCommands(
        characterCommands, [](const editor::BookEditor& text) { return !text.isReadOnly(); });
    std::vector<std::string> insertCommands = objectCommands;
    insertCommands.insert(insertCommands.end(), characterCommands.begin(),
                          characterCommands.end());
    // The pastes in each mode: with something on the clipboard, as Paste
    const std::vector<std::string> pasteCommands = {
        "edit.pasteSourceFormatting", "edit.pasteMergeFormatting", "edit.pasteTextOnly"};
    m_editCoordinator->addTextCommands(
        pasteCommands, [](const editor::BookEditor& text) { return text.canPaste(); });
    std::vector<std::string> changingCommands = insertCommands;
    changingCommands.insert(changingCommands.end(), pasteCommands.begin(), pasteCommands.end());
    for (const std::string& id : changingCommands) {
        if (Command* command = commands().getCommand(id)) {
            const bool object =
                std::find(objectCommands.begin(), objectCommands.end(), id) != objectCommands.end();
            command->disabledReason = [this, object]() {
                const editor::BookEditor* text = m_editCoordinator->textWithKeys();
                if (text != nullptr && text->isReadOnly()) {
                    return readOnlyReason();
                }
                return text != nullptr && object && text->isInTable() ? inTableReason()
                                                                       : QString();
            };
        }
    }
    addTableCommands();
    addImageCommands();

    // Insert > Footnote and Endnote: in the book's text with the keys, not in a read-only one
    // nor in a plain text file (it keeps no notes); the frame of a note's text
    // the numbers of the notes of every open text in the book around it, and Insert >
    // Footnote Layout...; a click on an endnote of another text at the end of the book
    // opens that text, as from the Navigator
    m_footnoteCoordinator = new FootnoteCoordinator(
        *m_book, *m_commands, m_dockCoordinator->centralTabs(),
        [this]() { return m_editCoordinator->textWithKeys(); },
        [this](const QString& elementId) {
            if (const core::ProjectElement* element =
                    book().findElement(elementId)) {
                m_navigatorCoordinator->onElementSelected(elementId, element->title);
            }
        },
        this, this);
    m_footnoteCoordinator->connectCommands();
    // A text opened, or the book's order of texts changed: other numbers
    connect(m_navigatorCoordinator, &NavigatorCoordinator::elementOpened, m_footnoteCoordinator,
            [this]() { m_footnoteCoordinator->refreshSoon(); });
    connect(m_navigatorCoordinator, &NavigatorCoordinator::refreshNavigatorRequested,
            m_footnoteCoordinator, [this]() { m_footnoteCoordinator->refreshSoon(); });
    const std::vector<std::string> noteCommands = {"insert.footnote", "insert.endnote"};
    m_editCoordinator->addTextCommands(noteCommands, [](const editor::BookEditor& text) {
        return !text.isReadOnly() && text.annotationsAllowed();
    });
    for (const std::string& id : noteCommands) {
        if (Command* command = commands().getCommand(id)) {
            command->disabledReason = [this]() {
                const editor::BookEditor* text = m_editCoordinator->textWithKeys();
                if (text != nullptr && text->isReadOnly()) {
                    return readOnlyReason();
                }
                //: Why Insert > Footnote and Endnote are greyed out in a .txt file. In Polish:
                //: Plik zwykłego tekstu nie przechowuje przypisów
                return text != nullptr && !text->annotationsAllowed()
                           ? tr("A plain text file keeps no notes")
                           : QString();
            };
        }
    }
    m_editCoordinator->updateStates();

    // File > Export > DOCX and Markdown: the book, some of its texts or the text in front, with
    // the numbers of the captions and notes of the book
    m_bookExportCoordinator = new BookExportCoordinator(
        book(), commands(), m_dockCoordinator->centralTabs(),
        [this]() { return getCurrentEditor(); },
        m_captionCoordinator, m_footnoteCoordinator, this, this);
    m_bookExportCoordinator->connectCommands();

    // Edit > Find in Book... and the Search panel: all the texts of the book; a match of a
    // text not open opens it, as from the Navigator, and the bar under the text says how many
    // matches Replace All in Book changed, with Undo
    m_bookSearchCoordinator = new BookSearchCoordinator(
        book(), commands(), m_dockCoordinator->centralTabs(), m_dockCoordinator->searchPanel(),
        [this]() {
            QDockWidget* dock = m_dockCoordinator->searchDock();
            dock->show();
            dock->raise();
            if (dock->isFloating()) {
                dock->activateWindow();
            }
        },
        [this]() { return m_editCoordinator->textWithKeys(); },
        [this](const QString& elementId) {
            if (const core::ProjectElement* element =
                    book().findElement(elementId)) {
                m_navigatorCoordinator->onElementSelected(elementId, element->title);
            }
        },
        [this](const QString& elementId) { return m_footnoteCoordinator->contextFor(elementId); },
        [this](EditorPanel* editor, const QString& message, const QString& actionText,
               const QString& actionToolTip, std::function<void()> action) {
            m_documentCoordinator->showNotice(editor, message, actionText, actionToolTip,
                                              std::move(action));
        },
        statusBar(), this, this);
    m_bookSearchCoordinator->connectCommands();
    // Texts added, moved or removed: the matches in the book's order
    connect(m_navigatorCoordinator, &NavigatorCoordinator::refreshNavigatorRequested,
            m_bookSearchCoordinator, [this]() { m_bookSearchCoordinator->searchAgain(); });

    // Book > Chapter Break: the text from the cursor to a new text right after the text with
    // the keys; the bar under the new text says where the text went, with Undo
    m_chapterBreakCoordinator = new ChapterBreakCoordinator(
        book(), commands(), m_dockCoordinator->centralTabs(), m_navigatorCoordinator,
        [this]() { return m_editCoordinator->textWithKeys(); },
        [this](EditorPanel* editor, const QString& message, const QString& actionText,
               const QString& actionToolTip, std::function<void()> action) {
            m_documentCoordinator->showNotice(editor, message, actionText, actionToolTip,
                                              std::move(action));
        },
        statusBar(), this, this);
    m_chapterBreakCoordinator->connectCommands();
    m_editCoordinator->addTextCommands(
        {ChapterBreakCoordinator::COMMAND}, [this](const editor::BookEditor& text) {
            return m_chapterBreakCoordinator->canBreak(text);
        });

    // Format > Text Style also shows the other styles of the text in front (the window owns
    // the object)
    if (QAction* heading = commands().getAction(
            QString::fromLatin1(styleCommands().front().commandId))) {
        for (QObject* object : heading->associatedObjects()) {
            if (auto* menu = qobject_cast<QMenu*>(object)) {
                new ParagraphStyleMenu(
                    *m_commands, menu,
                    [this]() -> editor::BookEditor* {
                        EditorPanel* panel = getCurrentEditor();
                        return panel != nullptr ? panel->getBookEditor() : nullptr;
                    },
                    this);
                break;
            }
        }
    }

    // Format > Font Color and Highlight Color: their submenus of colors, the swatch of the
    // color at the cursor framed (the window owns them)
    const auto shownColor = [this](bool highlight) {
        return [this, highlight]() {
            EditorPanel* panel = getCurrentEditor();
            if (panel == nullptr || panel->getBookEditor() == nullptr) {
                return QColor();
            }
            const editor::CharacterFormat shown = panel->getBookEditor()->shownCharacterFormat();
            return (highlight ? shown.highlight : shown.color).value_or(QColor());
        };
    };
    m_fontColorMenu = new ColorMenu(ColorMenu::Kind::Text, shownColor(false), this);
    m_highlightColorMenu = new ColorMenu(ColorMenu::Kind::Highlight, shownColor(true), this);
    for (ColorMenu* menu : {m_fontColorMenu, m_highlightColorMenu}) {
        const bool highlight = menu == m_highlightColorMenu;
        if (QAction* command = commands().getAction(
                highlight ? QStringLiteral("format.highlightColor")
                          : QStringLiteral("format.fontColor"))) {
            command->setMenu(menu);
        }
        connect(menu, &ColorMenu::colorChosen, this,
                [this, highlight](const QColor& color) { applyColor(color, highlight); });
    }

    // After a command given in a toolbar the keys go back where they were, in the text or a
    // field; a floating toolbar, a window of its own, takes them at a click (the window owns
    // the object)
    new ToolbarKeys(this, this);

    // Auto-correct as you type turned on and off as a whole (the window owns the object)
    auto* autoCorrect = new AutoCorrectCoordinator(*m_commands, statusBar(), this);
    autoCorrect->connectCommands();

    // Insert > Special Character...: the window inserts in the chapter in front; the
    // characters given keys there work as the formatting characters (the window owns the
    // object)
    m_specialCharacterCoordinator = new SpecialCharacterCoordinator(
        *m_commands, this, statusBar(),
        [this]() -> editor::BookEditor* {
            EditorPanel* panel = getCurrentEditor();
            return panel != nullptr ? panel->getBookEditor() : nullptr;
        },
        [this](const QString& characters) {
            if (editor::BookEditor* text = m_editCoordinator->textWithKeys()) {
                text->insertCharacters(characters);
            }
        },
        [this](const std::vector<std::string>& ids) {
            m_editCoordinator->addTextCommands(
                ids, [](const editor::BookEditor& text) { return !text.isReadOnly(); });
            for (const std::string& id : ids) {
                if (Command* command = commands().getCommand(id)) {
                    command->disabledReason = [this]() {
                        const editor::BookEditor* text = m_editCoordinator->textWithKeys();
                        return text != nullptr && text->isReadOnly() ? readOnlyReason()
                                                                     : QString();
                    };
                }
            }
            m_editCoordinator->updateStates();
        },
        this);
    m_specialCharacterCoordinator->connectCommands();

    // Spelling as you type: the dictionary of the editors follows the settings and the
    // language of the book, also as it is changed in the Properties panel
    m_spellingCoordinator = new SpellingCoordinator(
        *m_book, *m_commands, m_dockCoordinator->centralTabs(), statusBar(), this);
    m_spellingCoordinator->connectCommands();
    connect(m_dockCoordinator->propertiesPanel(), &PropertiesPanel::bookLanguageChanged,
            m_spellingCoordinator, &SpellingCoordinator::updateDictionary);
    m_settingsCoordinator->setSpellCheckService(m_spellingCoordinator->service());

    // Grammar as you type, in the spelling's language: on LanguageTool the program prepares
    // on this computer, or on a server in the internet the writer chose
    m_grammarCoordinator = new GrammarCoordinator(
        *m_book, *m_commands, m_dockCoordinator->centralTabs(), statusBar(), this);
    m_grammarCoordinator->connectCommands();
    connect(m_dockCoordinator->propertiesPanel(), &PropertiesPanel::bookLanguageChanged,
            m_grammarCoordinator, &GrammarCoordinator::updateChecking);
    m_settingsCoordinator->setGrammarCoordinator(m_grammarCoordinator);

    // The Spelling and Grammar panel: the issues of the text in front, where Next Spelling or
    // Grammar Issue shows the one it finds
    m_spellingPanelCoordinator = new SpellingPanelCoordinator(
        *m_book, *m_commands, m_dockCoordinator->spellingPanel(), m_dockCoordinator->spellingDock(),
        m_dockCoordinator->centralTabs(), m_spellingCoordinator, m_grammarCoordinator, this);

    // The Repetitions panel, the marks of the repeated words and Next Repetition; a chapter
    // is opened as from the Navigator
    m_repetitionsCoordinator = new RepetitionsCoordinator(
        *m_book, *m_commands, m_dockCoordinator->repetitionsPanel(),
        m_dockCoordinator->repetitionsDock(), m_dockCoordinator->centralTabs(),
        m_spellingCoordinator,
        [this](const QString& elementId) {
            if (const core::ProjectElement* element =
                    book().findElement(elementId)) {
                m_navigatorCoordinator->onElementSelected(elementId, element->title);
            }
        },
        statusBar(), this);
    m_repetitionsCoordinator->connectCommands();

    // NOTE (Task #00015): EditorPanel textChanged signal connected when tab created
    // No m_editorPanel at startup - Dashboard is default first tab

    // Connect ThemeManager to MainWindow (Task #00023)
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged,
            this, &MainWindow::onThemeChanged);
    logger.debug("MainWindow: Connected ThemeManager::themeChanged signal to MainWindow");

    // Note: the icon stack's theme handling is internal to ArtProvider.
    // ArtProvider::initialize() synchronizes colors from the current theme and connects
    // ThemeManager::themeChanged to ArtProvider::onThemeChanged, which drives IconRegistry
    // itself. Do not connect IconRegistry to themeChanged here -- see ArtProvider::onThemeChanged.

    // The books this window opens go to the top of the recent books
    core::RecentBooksManager::getInstance().follow(*m_book);

    // Connect ProjectManager signals (OpenSpec #00033 Phase D) to DocumentCoordinator
    auto& pm = book();
    connect(&pm, &core::ProjectManager::projectOpened,
            m_documentCoordinator, &DocumentCoordinator::onProjectOpened);
    connect(&pm, &core::ProjectManager::projectAboutToClose,
            m_documentCoordinator, &DocumentCoordinator::prepareForProjectClose);
    connect(&pm, &core::ProjectManager::projectClosed,
            m_documentCoordinator, &DocumentCoordinator::onProjectClosed);
    // The chapters still open when the book closes open there next time (its database is
    // still open then)
    connect(&pm, &core::ProjectManager::projectAboutToClose,
            this, &MainWindow::rememberChapterCursors);
    logger.debug("MainWindow: Connected ProjectManager signals to DocumentCoordinator");

    // The Book menu and the commands of the open book follow it
    connect(&pm, &core::ProjectManager::projectOpened, this, &MainWindow::updateBookCommands);
    connect(&pm, &core::ProjectManager::projectClosed, this, &MainWindow::updateBookCommands);
    updateBookCommands();

    // OpenSpec #00043: Create debounce timer for action state updates
    // This prevents expensive updates on every cursor movement
    m_actionStateDebounceTimer = new QTimer(this);
    m_actionStateDebounceTimer->setSingleShot(true);
    m_actionStateDebounceTimer->setInterval(50);  // 50ms debounce
    connect(m_actionStateDebounceTimer, &QTimer::timeout,
            this, &MainWindow::updateEditorActionStates);
    logger.debug("MainWindow: Action state debounce timer created (50ms)");

    // The other book windows: a book open in one of them is not opened again, and another
    // book can go to a new window
    m_documentCoordinator->setOtherWindows(
        {[this](const QString& manifest) {
             MainWindow* other = windowOfBook(manifest);
             if (other == nullptr || other == this) {
                 return false;
             }
             DocumentWindows::showWindow(other);
             other->m_documentCoordinator->showBookOpenHere();
             return true;
         },
         [this](const QString& manifest) { openBookInNewWindow(manifest); }});

    // One of the book windows of the program; the one in front is the one whose window, or
    // a window of its own of its tabs, has the keys
    bookWindowList().append(this);
    if (lastFrontWindow().isNull()) {
        lastFrontWindow() = this;
    }
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget* /*old*/, QWidget* now) {
        if (now != nullptr && windowOf(now) == this) {
            lastFrontWindow() = this;
        }
    });

    logger.info("MainWindow initialized successfully");
}

MainWindow::~MainWindow() {
    auto& logger = core::Logger::getInstance();
    logger.debug("MainWindow destructor - disconnecting all signals");

    // No longer one of the book windows of the program
    bookWindowList().removeAll(this);

    // The book still open closes as when the program ends, while everything that follows it is
    // there: the texts, the panels, the statistics in its database
    m_book->closeProject();

    // The windows of their own of the tabs go with the window, saying nothing meanwhile
    if (m_documentWindows) {
        m_documentWindows->blockSignals(true);
    }

    // CRITICAL: Disconnect from SINGLETON objects first.
    // These are NOT children of MainWindow, so findChildren() won't find them.
    // If we don't disconnect, signals from singletons may be delivered to
    // partially-destroyed MainWindow, causing ASSERT failure.
    disconnect(&core::ThemeManager::getInstance(), nullptr, this, nullptr);
    disconnect(&core::ArtProvider::getInstance(), nullptr, this, nullptr);
    disconnect(&book(), nullptr, this, nullptr);
    // The dialogs' gears open the settings of a book window: without one, none
    if (bookWindowList().isEmpty()) {
        dialogs::KalahariDialog::setSettingsOpener(nullptr);
    }

    // Disconnect from coordinators (they may have timers or async operations)
    if (m_documentCoordinator) {
        disconnect(m_documentCoordinator, nullptr, this, nullptr);
    }
    if (m_settingsCoordinator) {
        disconnect(m_settingsCoordinator, nullptr, this, nullptr);
    }
    if (m_dockCoordinator) {
        disconnect(m_dockCoordinator, nullptr, this, nullptr);
    }
    if (m_navigatorCoordinator) {
        disconnect(m_navigatorCoordinator, nullptr, this, nullptr);
    }
    if (m_annotationsCoordinator) {
        disconnect(m_annotationsCoordinator, nullptr, this, nullptr);
    }
    // The edit commands stop following the keys before the widgets go
    delete m_editCoordinator;
    m_editCoordinator = nullptr;
    // The Spelling and Grammar panel stops following the text and the checking before they go
    delete m_spellingPanelCoordinator;
    m_spellingPanelCoordinator = nullptr;
    // The Repetitions panel too: it follows the texts and marks them
    delete m_repetitionsCoordinator;
    m_repetitionsCoordinator = nullptr;
    if (m_spellingCoordinator) {
        disconnect(m_spellingCoordinator, nullptr, this, nullptr);
    }
    if (m_grammarCoordinator) {
        disconnect(m_grammarCoordinator, nullptr, this, nullptr);
    }

    // Block signals to prevent any delivery during child destruction
    blockSignals(true);

    // Disconnect all remaining children (BookEditors, panels, etc.)
    for (QObject* child : findChildren<QObject*>()) {
        disconnect(child, nullptr, this, nullptr);
    }

    // The parts of the window go while the book and the commands they use are still there (the
    // members go after this body, the children of a widget only after it is no longer a
    // MainWindow). Its layout stays with it, and so do the tab bars of the docked panels: Qt
    // keeps them, and from Qt 6.8 its layout deletes the ones it no longer uses itself
    const auto nextPart = [this]() -> QObject* {
        for (QObject* child : children()) {
            if (qobject_cast<QLayout*>(child) == nullptr &&
                qobject_cast<QTabBar*>(child) == nullptr) {
                return child;
            }
        }
        return nullptr;
    };
    // Deleting a part takes it out of the children, and may delete others with it
    while (QObject* part = nextPart()) {
        delete part;
    }

    logger.debug("MainWindow destructor complete");
}

void MainWindow::registerCommands() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Registering commands with CommandRegistry (via CommandRegistrar)");

    // =========================================================================
    // ICON REGISTRY - Register default SVG icons (OpenSpec #00038: extracted to IconRegistrar)
    // =========================================================================
    registerAllIcons();

    // =========================================================================
    // COMMAND REGISTRY - Register all commands (OpenSpec #00038: extracted to CommandRegistrar)
    // =========================================================================

    // Create callbacks struct with lambdas that call MainWindow methods or DocumentCoordinator
    // NOTE: Document operations delegated to DocumentCoordinator (OpenSpec #00038 Phase 7)
    // DocumentCoordinator is created later, so we capture 'this' and call via m_documentCoordinator
    CommandCallbacks callbacks;

    // File commands - delegate to DocumentCoordinator
    // A document opened gets the keys
    callbacks.onNewDocument = [this]() {
        openGivingKeys([this]() {
            if (m_documentCoordinator) m_documentCoordinator->onNewDocument();
        });
    };
    callbacks.onNewProject = [this]() { if (m_documentCoordinator) m_documentCoordinator->onNewProject(); };
    callbacks.onOpenDocument = [this]() { if (m_documentCoordinator) m_documentCoordinator->onOpenDocument(); };
    callbacks.onOpenStandaloneFile = [this]() {
        openGivingKeys([this]() {
            if (m_documentCoordinator) m_documentCoordinator->onOpenStandaloneFile();
        });
    };
    // An annotation or a note being written is saved with the document
    callbacks.onSaveDocument = [this]() {
        if (m_annotationsCoordinator) m_annotationsCoordinator->finishWriting();
        if (m_footnoteCoordinator) m_footnoteCoordinator->finishWriting();
        if (m_documentCoordinator) m_documentCoordinator->onSaveDocument();
    };
    callbacks.onSaveAsDocument = [this]() {
        if (m_annotationsCoordinator) m_annotationsCoordinator->finishWriting();
        if (m_footnoteCoordinator) m_footnoteCoordinator->finishWriting();
        if (m_documentCoordinator) m_documentCoordinator->onSaveAsDocument();
    };
    callbacks.onSaveAll = [this]() {
        if (m_annotationsCoordinator) m_annotationsCoordinator->finishWriting();
        if (m_footnoteCoordinator) m_footnoteCoordinator->finishWriting();
        if (m_documentCoordinator) m_documentCoordinator->onSaveAll();
    };
    callbacks.onCloseDocument = [this]() { if (m_documentCoordinator) m_documentCoordinator->onCloseDocument(); };
    callbacks.onImportArchive = [this]() { if (m_documentCoordinator) m_documentCoordinator->onImportArchive(); };
    callbacks.onExportArchive = [this]() { if (m_documentCoordinator) m_documentCoordinator->onExportArchive(); };
    callbacks.onExit = [this]() { onExit(); };

    // Edit commands
    callbacks.onUndo = [this]() { onUndo(); };
    callbacks.onRedo = [this]() { onRedo(); };
    callbacks.onCut = [this]() { onCut(); };
    callbacks.onCopy = [this]() { onCopy(); };
    callbacks.onPaste = [this]() { onPaste(); };
    callbacks.onDelete = [this]() {
        if (m_editCoordinator) {
            m_editCoordinator->execute(EditCoordinator::Edit::Delete);
        }
    };
    callbacks.onSelectAll = [this]() { onSelectAll(); };

    // Select Word, Select Paragraph, the objects of the Insert menu and the pastes in each
    // mode work in the book's text with the keys (EditCoordinator::addTextCommands())
    const auto inText = [this](const std::function<void(editor::BookEditor&)>& action) {
        return [this, action]() {
            if (editor::BookEditor* text =
                    m_editCoordinator ? m_editCoordinator->textWithKeys() : nullptr) {
                action(*text);
            }
        };
    };
    callbacks.onSelectWord = inText([](editor::BookEditor& text) { text.selectWord(); });
    callbacks.onSelectParagraph =
        inText([](editor::BookEditor& text) { text.selectParagraph(); });
    const auto insert = [&inText](editor::TextObject object) {
        return inText([object](editor::BookEditor& text) { text.insertTextObject(object); });
    };
    callbacks.onInsertHorizontalLine = insert(editor::TextObject::HorizontalRule);
    callbacks.onInsertSceneBreak = insert(editor::TextObject::SceneBreak);
    callbacks.onInsertPageBreak = insert(editor::TextObject::PageBreak);
    // The list of quick insert of the text (EditorPanel), with all its items
    callbacks.onQuickInsert = inText([](editor::BookEditor& text) {
        if (auto* quick = text.findChild<editor::QuickInsertController*>(
                QString(), Qt::FindDirectChildrenOnly)) {
            quick->openAtCursor();
        }
    });
    callbacks.onSpecialCharacter = [this]() {
        if (m_specialCharacterCoordinator != nullptr) {
            m_specialCharacterCoordinator->openWindow();
        }
    };
    callbacks.onQuickCharacter = [this]() {
        if (m_specialCharacterCoordinator != nullptr) {
            m_specialCharacterCoordinator->openPalette();
        }
    };
    callbacks.onInsertCharacters = [this](const QString& characters) {
        if (editor::BookEditor* text =
                m_editCoordinator ? m_editCoordinator->textWithKeys() : nullptr) {
            text->insertCharacters(characters);
        }
    };
    const auto pasteAs = [this, &inText](editor::PasteMode mode) {
        return inText([this, mode](editor::BookEditor& text) {
            text.pasteAs(mode);
            statusBar()->showMessage(tr("Pasted from clipboard"), 2000);
        });
    };
    callbacks.onPasteSourceFormatting = pasteAs(editor::PasteMode::Source);
    callbacks.onPasteMergeFormatting = pasteAs(editor::PasteMode::Merge);
    callbacks.onPasteTextOnly = pasteAs(editor::PasteMode::Text);
    callbacks.onFind = [this]() { onFind(); };
    callbacks.onFindNext = [this]() { onFindNext(); };
    callbacks.onFindPrevious = [this]() { onFindPrevious(); };
    callbacks.onFindReplace = [this]() { onFindReplace(); };
    callbacks.onSettings = [this]() { onSettings(); };

    // Book commands: Add... for what is in front, a main text of the book (a chapter, a
    // story...) placed in the window of adding it, and the book's properties
    callbacks.onAddElement = [this]() {
        if (m_navigatorCoordinator) m_navigatorCoordinator->onRequestAddHere();
    };
    callbacks.onNewChapter = [this]() {
        if (m_navigatorCoordinator) m_navigatorCoordinator->onRequestNewChapter();
    };
    // The kinds of the Workshop: the window with all of them, that one chosen
    callbacks.onNewWorkshopElement = [this](const QString& kindId) {
        if (!m_navigatorCoordinator) {
            return;
        }
        AddTarget target;
        target.scope = AddTarget::Scope::Workshop;
        target.kind = workshopKind(kindId);
        m_navigatorCoordinator->onRequestAdd(target);
    };
    callbacks.onBookProperties = [this]() { showBookProperties(); };

    // Format commands (OpenSpec #00042 Phase 7.2)
    callbacks.onFormatBold = [this]() { onFormatBold(); };
    callbacks.onFormatItalic = [this]() { onFormatItalic(); };
    callbacks.onFormatUnderline = [this]() { onFormatUnderline(); };
    callbacks.onFormatStrikethrough = [this]() { onFormatStrikethrough(); };
    callbacks.onClearFormatting = [this]() {
        EditorPanel* editor = getCurrentEditor();
        if (editor && editor->getBookEditor()) {
            editor->getBookEditor()->clearFormatting();
        }
        updateEditorActionStates();  // bold and the alignments are off now
    };

    // Format > Font... and Paragraph...: the windows of the editor in front
    callbacks.onFont = [this]() {
        if (EditorPanel* editor = getCurrentEditor()) {
            editor->showFontDialog();
        }
        updateEditorActionStates();
    };
    callbacks.onParagraph = [this]() {
        if (EditorPanel* editor = getCurrentEditor()) {
            editor->showParagraphDialog();
        }
        updateEditorActionStates();
    };
    // Insert > Link...: the window of the link at the cursor, of the editor in front
    callbacks.onInsertLink = [this]() {
        if (EditorPanel* editor = getCurrentEditor()) {
            editor->showHyperlinkDialog();
        }
    };
    // Insert > Image...: the system window choosing a picture, of the editor in front
    callbacks.onInsertImage = [this]() {
        if (EditorPanel* editor = getCurrentEditor()) {
            editor->showInsertImageDialog();
        }
    };
    // Insert > Table...: the window of the new table, of the editor in front
    callbacks.onInsertTable = [this]() {
        if (EditorPanel* editor = getCurrentEditor()) {
            editor->showInsertTableDialog();
        }
    };
    // The Table menu: in the table of the book's text with the keys, the windows of its panel
    callbacks.onTable = [this](TableCommand command) {
        editor::BookEditor* text = m_editCoordinator ? m_editCoordinator->textWithKeys() : nullptr;
        if (text == nullptr) {
            return;
        }
        onTableCommand(*text, command);
    };
    // The Image menu: on the selected image of the book's text with the keys
    callbacks.onImage = [this](ImageCommand command) {
        editor::BookEditor* text = m_editCoordinator ? m_editCoordinator->textWithKeys() : nullptr;
        if (text == nullptr) {
            return;
        }
        onImageCommand(*text, command);
    };

    // Alignment commands
    callbacks.onAlignLeft = [this]() { onAlignLeft(); };
    callbacks.onAlignCenter = [this]() { onAlignCenter(); };
    callbacks.onAlignRight = [this]() { onAlignRight(); };
    callbacks.onAlignJustify = [this]() { onAlignJustify(); };

    // The indents and the lists, as the alignments: the paragraphs of the cursor or the
    // selection in the document in front, the buttons checked as the paragraph at the cursor
    const auto inDocument = [this](const std::function<void(editor::BookEditor&)>& action) {
        return [this, action]() {
            EditorPanel* editor = getCurrentEditor();
            if (editor && editor->getBookEditor()) {
                action(*editor->getBookEditor());
            }
            updateEditorActionStates();
        };
    };
    callbacks.onIncreaseIndent =
        inDocument([](editor::BookEditor& text) { text.increaseIndent(); });
    callbacks.onDecreaseIndent =
        inDocument([](editor::BookEditor& text) { text.decreaseIndent(); });
    callbacks.onBullets = inDocument(
        [](editor::BookEditor& text) { text.toggleList(editor::ListKind::Bulleted); });
    callbacks.onNumbering = inDocument(
        [](editor::BookEditor& text) { text.toggleList(editor::ListKind::Numbered); });
    // The effects, the cases, the colors and Copy and Paste Formatting, as the indents
    callbacks.onSuperscript =
        inDocument([](editor::BookEditor& text) { text.toggleSuperscript(); });
    callbacks.onSubscript = inDocument([](editor::BookEditor& text) { text.toggleSubscript(); });
    callbacks.onSmallCaps = inDocument([](editor::BookEditor& text) { text.toggleSmallCaps(); });
    callbacks.onAllCaps = inDocument([](editor::BookEditor& text) { text.toggleAllCaps(); });
    callbacks.onChangeCase = [inDocument](editor::CaseChange change) {
        inDocument([change](editor::BookEditor& text) { text.changeCase(change); })();
    };
    callbacks.onFontColor = [this]() {
        if (m_fontColorMenu != nullptr) {
            applyColor(m_fontColorMenu->lastColor(), false);
        }
    };
    callbacks.onHighlightColor = [this]() {
        if (m_highlightColorMenu != nullptr) {
            applyColor(m_highlightColorMenu->lastColor(), true);
        }
    };
    callbacks.onCopyFormatting = inDocument([this](editor::BookEditor& text) {
        text.copyFormatting();
        statusBar()->showMessage(
            tr("Formatting copied: select text and choose Format > Paste Formatting"), 4000);
    });
    callbacks.onPasteFormatting = inDocument([this](editor::BookEditor& text) {
        if (!editor::BookEditor::hasCopiedFormatting()) {
            statusBar()->showMessage(
                tr("No formatting copied yet: choose Format > Copy Formatting first"), 4000);
            return;
        }
        text.pasteFormatting();
    });
    callbacks.onBulletsNumbering = [this]() {
        if (EditorPanel* editor = getCurrentEditor()) {
            editor->showBulletsAndNumbering();
        }
        updateEditorActionStates();
    };

    // Format > Text Style, as the alignments: the paragraphs of the cursor or the selection
    // in the document in front
    callbacks.onParagraphStyle = [this](const QString& styleId) {
        core::Logger::getInstance().info("Action triggered: Text Style {}", styleId.toStdString());
        EditorPanel* editor = getCurrentEditor();
        if (editor && editor->getBookEditor()) {
            editor->getBookEditor()->applyParagraphStyle(styleId);
        }
        // Choosing the style in use changes nothing, the style stays checked
        updateEditorActionStates();
    };

    // View Mode commands (OpenSpec #00042 Phase 7.3)
    callbacks.onViewModeContinuous = [this]() { onViewModeContinuous(); };
    callbacks.onViewModePage = [this]() { onViewModePage(); };
    callbacks.onTypewriterToggle = [this]() { onTypewriterToggle(); };
    callbacks.onFocusToggle = [this]() { onFocusToggle(); };
    callbacks.onDarkPaperToggle = [this]() { onDarkPaperToggle(); };
    callbacks.onTextFrameBorderToggle = [this]() { onTextFrameBorderToggle(); };
    callbacks.onAnnotationCardsToggle = [this]() { onAnnotationCardsToggle(); };
    callbacks.onDistractionFreeToggle = [this]() { onDistractionFreeToggle(); };
    callbacks.onFormattingMarksToggle = [this]() { onFormattingMarksToggle(); };
    callbacks.onMarginNumbering = [this](const QString& mode) { onMarginNumbering(mode); };
    callbacks.onMarginNumberingSettings = [this]() {
        m_settingsCoordinator->openSettingsPage(
            QString::fromLatin1(SettingsDialog::MARGIN_NUMBERING_PAGE));
    };

    // Zoom commands act on the editor in front, or on the preview of a material, which fits
    // its picture to the window for the width of the page and the whole page
    const auto onEditor = [this](void (editor::BookEditor::*action)(),
                                 void (ImagePreviewPanel::*previewAction)()) {
        return [this, action, previewAction]() {
            if (ImagePreviewPanel* preview = currentPreview()) {
                (preview->*previewAction)();
                return;
            }
            EditorPanel* editor = getCurrentEditor();
            if (editor && editor->getBookEditor()) {
                (editor->getBookEditor()->*action)();
            }
        };
    };
    callbacks.onZoomIn = onEditor(&editor::BookEditor::zoomIn, &ImagePreviewPanel::zoomIn);
    callbacks.onZoomOut = onEditor(&editor::BookEditor::zoomOut, &ImagePreviewPanel::zoomOut);
    callbacks.onZoomReset =
        onEditor(&editor::BookEditor::zoomReset, &ImagePreviewPanel::zoomReset);
    callbacks.onZoomPageWidth =
        onEditor(&editor::BookEditor::zoomToPageWidth, &ImagePreviewPanel::zoomToFit);
    callbacks.onZoomWholePage =
        onEditor(&editor::BookEditor::zoomToWholePage, &ImagePreviewPanel::zoomToFit);

    // View commands
    callbacks.onDashboard = [this]() {
        // Check if Dashboard tab already exists
        QTabWidget* centralTabs = m_dockCoordinator->centralTabs();
        for (int i = 0; i < centralTabs->count(); ++i) {
            if (qobject_cast<DashboardPanel*>(centralTabs->widget(i))) {
                // Dashboard exists - activate it, its window in front
                centralTabs->setCurrentIndex(i);
                if (m_documentWindows) {
                    m_documentWindows->showPage(centralTabs->widget(i));
                }
                core::Logger::getInstance().debug("Dashboard tab activated at index {}", i);
                return;
            }
        }

        // Dashboard doesn't exist - create new one
        DashboardPanel* dashboardPanel = new DashboardPanel(*m_commands, this);
        m_dockCoordinator->setDashboardPanel(dashboardPanel);
        auto& artProvider = core::ArtProvider::getInstance();
        QIcon dashboardIcon = artProvider.getIcon("view.dashboard");
        int dashboardIndex = centralTabs->addTab(dashboardPanel, dashboardIcon, tr("Dashboard"));
        dashboardPanel->setProperty("tabIconId", "view.dashboard");
        centralTabs->setCurrentIndex(dashboardIndex);

        // Reconnect Dashboard signals to DocumentCoordinator
        connect(dashboardPanel, &DashboardPanel::openRecentBookRequested,
                m_documentCoordinator, &DocumentCoordinator::onOpenRecentFile);

        core::Logger::getInstance().info("Dashboard tab created at index {}", dashboardIndex);
    };
    callbacks.onResetLayout = [this]() { resetLayout(); };

    // Tools commands
    callbacks.onToolbarManager = [this]() { m_toolbarManager->openToolbarManagerDialog(); };

    // Help commands
    callbacks.onKeyboardShortcuts = [this]() { m_settingsCoordinator->openKeyboardShortcuts(); };
    callbacks.onAbout = [this]() { onAbout(); };

    // Register all commands with the callbacks
    int count = registerAllCommands(commands(), callbacks);

    // The Window menu: the texts in windows of their own (DocumentWindows, made with the tabs)
    count += DocumentWindows::registerCommands(
        commands(), [this]() { return m_documentWindows; }, [this]() { openNewWindow(); });

    // The keys the user changed in Settings > Keyboard Shortcuts take the place of the
    // program's, in the menus and the toolbars built next
    commands().setCustomShortcuts(loadCustomShortcuts());

    // OpenSpec #00040: Setup fullscreen command callbacks (post-registration modification)
    CommandRegistry& registry = commands();
    if (auto* fsCmd = registry.getCommand("view.fullScreen")) {
        fsCmd->execute = [this]() { toggleFullScreen(); };
        fsCmd->isChecked = [this]() { return isFullScreen(); };
    }

    // Distraction-Free is for writing in the document in front: without one (the Dashboard,
    // no tab) it cannot be turned on, and it goes off (see createDocks())
    if (auto* dfCmd = registry.getCommand("view.mode.distraction-free")) {
        dfCmd->isChecked = [this]() { return isDistractionFree(); };
        dfCmd->isEnabled = [this]() {
            // A text in a window of its own is written there, not in the book window
            EditorPanel* front = m_dockCoordinator != nullptr ? getCurrentEditor() : nullptr;
            const bool detached = m_documentWindows && m_documentWindows->isDetached(front);
            return isDistractionFree() || (front != nullptr && !detached);
        };
    }

    // The Book menu works with a book open, and changes it: over a book open read-only it is
    // greyed out, its tooltips and the status bar saying why. New Chapter... also needs a kind
    // of text the body of the book can have (see updateBookCommands())
    const auto whyReadOnly = [this]() {
        const auto& projects = book();
        return projects.isProjectOpen() && projects.isReadOnly() ? readOnlyReason() : QString();
    };
    if (auto* newText = registry.getCommand("book.newChapter")) {
        newText->isEnabled = [this]() {
            const auto& projects = book();
            return projects.isProjectOpen() && !projects.isReadOnly()
                   && static_cast<bool>(projects.chapterKindFor(core::BookPlace::Main));
        };
        newText->disabledReason = whyReadOnly;
    }
    if (auto* properties = registry.getCommand("book.properties")) {
        properties->isEnabled = [this]() {
            const auto& projects = book();
            return projects.isProjectOpen() && !projects.isReadOnly();
        };
        properties->disabledReason = whyReadOnly;
    }
    // Add... adds to the book open, after the element in front or anywhere in it; it leads the
    // menu, in bold, as in the menus of the Navigator
    if (auto* add = registry.getCommand("book.addElement")) {
        add->isEnabled = [this]() {
            const auto& projects = book();
            return projects.isProjectOpen() && !projects.isReadOnly();
        };
        add->disabledReason = whyReadOnly;
        if (QAction* action = registry.getAction(std::string("book.addElement"))) {
            QFont bold = action->font();
            bold.setBold(true);
            action->setFont(bold);
        }
    }
    // The kinds of the Workshop wait for their editors, saying so: "The program does not have
    // an editor for the character yet."
    for (const auto& [commandId, kindId] : WORKSHOP_COMMANDS) {
        Command* command = registry.getCommand(commandId);
        if (command == nullptr) {
            continue;
        }
        const QString kindName = QString::fromLatin1(kindId);
        command->isEnabled = [this, kindName]() {
            const auto& projects = book();
            const core::KindRef kind = workshopKind(kindName);
            const QList<core::KindRef> offered =
                projects.isProjectOpen() ? projects.kindsFor(core::BookPlace::Workshop)
                                         : QList<core::KindRef>{};
            return kind && core::ProjectManager::canAdd(kind) && !projects.isReadOnly() &&
                   std::any_of(offered.cbegin(), offered.cend(), [&kind](const core::KindRef& other) {
                       return other.kind == kind.kind;
                   });
        };
        command->disabledReason = [kindName, whyReadOnly]() {
            const core::KindRef kind = workshopKind(kindName);
            if (kind && !core::ProjectManager::canAdd(kind)) {
                //: The tooltip of a greyed command of the Book menu that adds an element of a
                //: kind of the Workshop, e.g. New Character.... In Polish: Program nie ma
                //: jeszcze edytora {kind:genitive}.
                return core::fillWords(
                    tr("The program does not have an editor for the {kind} yet."),
                    {{QStringLiteral("kind"), wordsOf(kind)}});
            }
            return whyReadOnly();
        };
    }

    // Find, the formats, the alignments, the view mode and the zoom work on the document in
    // front: without one (the Dashboard, no tab) they are greyed out with their keys, as in
    // Word without a document; over a chapter of a book open read-only, those that change it.
    // The tab in front updates them (updateEditorActionStates())
    utils::bindToDocument(
        registry, [this]() { return hasDocumentInFront(); },
        [this]() {
            const EditorPanel* editor = m_dockCoordinator != nullptr ? getCurrentEditor() : nullptr;
            return editor != nullptr && editor->getBookEditor() != nullptr
                   && editor->getBookEditor()->isReadOnly();
        });
    // Insert > Table... also not in a cell of a table, which holds only text
    if (Command* table = registry.getCommand("insert.table")) {
        const auto inTable = [this]() {
            const EditorPanel* editor = m_dockCoordinator != nullptr ? getCurrentEditor() : nullptr;
            return editor != nullptr && editor->getBookEditor() != nullptr
                   && editor->getBookEditor()->isInTable();
        };
        table->isEnabled = [inTable, enabled = table->isEnabled]() {
            return (!enabled || enabled()) && !inTable();
        };
        table->disabledReason = [inTable, reason = table->disabledReason]() {
            const QString readOnly = reason ? reason() : QString();
            return readOnly.isEmpty() && inTable() ? inTableReason() : readOnly;
        };
    }

    // Insert > Image... also needs a folder for the images: a new file is saved first
    if (Command* image = registry.getCommand("insert.image")) {
        const auto withoutFolder = [this]() {
            const EditorPanel* editor = m_dockCoordinator != nullptr ? getCurrentEditor() : nullptr;
            return editor != nullptr && editor->getBookEditor() != nullptr &&
                   editor->getBookEditor()->imagesFolder().isEmpty();
        };
        image->isEnabled = [withoutFolder, enabled = image->isEnabled]() {
            return (!enabled || enabled()) && !withoutFolder();
        };
        image->disabledReason = [withoutFolder, reason = image->disabledReason]() {
            const QString readOnly = reason ? reason() : QString();
            //: Why Insert > Image... is greyed out in a new file. In Polish: Najpierw zapisz
            //: plik: jego obrazy są przechowywane w folderze obok niego
            return readOnly.isEmpty() && withoutFolder()
                       ? MainWindow::tr("Save the file first: its images are kept in a folder "
                                        "next to it")
                       : readOnly;
        };
    }

    // Find goes to the search of a list of materials too
    if (Command* command = registry.getCommand("edit.find")) {
        command->isEnabled = [this]() {
            return hasDocumentInFront() || (m_materialListCoordinator != nullptr &&
                                            m_materialListCoordinator->currentList() != nullptr);
        };
    }

    // The zoom works on the preview of a material too
    for (const char* id : {"view.zoomIn", "view.zoomOut", "view.resetZoom", "view.zoomPageWidth",
                           "view.zoomWholePage"}) {
        if (Command* command = registry.getCommand(id)) {
            command->isEnabled = [this]() {
                return hasDocumentInFront() ||
                       (m_dockCoordinator != nullptr && currentPreview() != nullptr);
            };
        }
    }

    // Close Book and the export of the book's archive work on the open book: without one
    // they are greyed out, as the Book menu (updateBookCommands())
    utils::bindToOpenBook(registry, [this]() {
        return book().isProjectOpen();
    });

    // The paper, the text frame border, Typewriter Scrolling and Focus are settings of every
    // editor: their toggles show them also before an editor opens, and follow the editor's
    // context menu and the Settings dialog
    utils::followSetting(registry.getAction(std::string("view.darkPaper")), "editor.darkMode");
    utils::followSetting(registry.getAction(std::string("view.textFrameBorder")),
                         "editor.textFrameBorder.show");
    utils::followSetting(registry.getAction(std::string("view.annotationCards")),
                         "editor.annotationCards.show");
    utils::followSetting(registry.getAction(std::string("view.typewriter")),
                         "editor.typewriter.enabled");
    utils::followSetting(registry.getAction(std::string("view.focus")), "editor.focus.enabled");
    utils::followSetting(registry.getAction(std::string("view.showFormattingMarks")),
                         "editor.showFormattingMarks");

    // So are the numbers on the margin: one choice of three, as in the Settings dialog
    auto* numbering = new QActionGroup(this);
    for (const auto& [id, mode] : {std::pair{"view.marginNumbers.none", editor::MARGIN_NUMBERING_NONE},
                                   std::pair{"view.marginNumbers.lines", editor::MARGIN_NUMBERING_LINES},
                                   std::pair{"view.marginNumbers.paragraphs",
                                             editor::MARGIN_NUMBERING_PARAGRAPHS}}) {
        QAction* action = registry.getAction(std::string(id));
        utils::followChoice(action, "editor.marginNumbers.mode", mode);
        if (action != nullptr) {
            numbering->addAction(action);
        }
    }

    logger.debug("Commands registered successfully ({} commands)", count);
}


void MainWindow::createMenus() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating menus from CommandRegistry");

    // Build menu bar from CommandRegistry using MenuBuilder
    // Task #00025: Store MenuBuilder for icon refresh on theme change
    m_menuBuilder = std::make_unique<MenuBuilder>();
    CommandRegistry& registry = commands();
    m_menuBuilder->buildMenuBar(registry, this);

    // Store FILE menu pointer for Recent Books integration
    m_fileMenu = m_menuBuilder->getMenu("FILE");

    // OpenSpec #00030: Add Recent Books submenu to FILE menu
    if (m_fileMenu) {
        auto* recentBooks = new RecentBooksMenu(*m_commands, m_fileMenu);
        recentBooks->insertInto(m_fileMenu);

        // Open the chosen book via DocumentCoordinator
        // Note: DocumentCoordinator is created later, but signal connections are deferred
        connect(recentBooks, &RecentBooksMenu::bookChosen,
                this, [this](const QString& path) {
                    if (m_documentCoordinator) m_documentCoordinator->onOpenRecentFile(path);
                });

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
    m_toolbarManager = std::make_unique<ToolbarManager>(*m_commands, this);
    CommandRegistry& registry = commands();
    m_toolbarManager->createToolbars(registry);

    // OpenSpec #00026: Connect ArtProvider::resourcesChanged() to update toolbar icon sizes
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged,
            this, [this]() {
                if (m_toolbarManager) {
                    m_toolbarManager->updateIconSizes();
                }
            });

    // OpenSpec #00032: Connect ArtProvider::resourcesChanged() to refresh dock title bar icons
    // Note: Connection moved to createDocks() after DockCoordinator is created

    logger.debug("5 toolbars created successfully (File, Edit, Book, View, Tools)");

    // Create exclusive action group for alignment buttons
    QActionGroup* alignGroup = new QActionGroup(this);
    alignGroup->setExclusive(true);

    if (QAction* leftAction = registry.getAction(QString("format.alignLeft"))) {
        alignGroup->addAction(leftAction);
    }
    if (QAction* centerAction = registry.getAction(QString("format.alignCenter"))) {
        alignGroup->addAction(centerAction);
    }
    if (QAction* rightAction = registry.getAction(QString("format.alignRight"))) {
        alignGroup->addAction(rightAction);
    }
    if (QAction* justifyAction = registry.getAction(QString("format.justify"))) {
        alignGroup->addAction(justifyAction);
    }

    logger.debug("Alignment action group created (exclusive)");
}

void MainWindow::createStatusBar() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating status bar");

    statusBar()->showMessage(tr("Ready"), 3000);  // Show for 3 seconds

    // A book open read-only: a lock left of the page, and why in its tooltip and, after a
    // click, in the status bar (updateReadOnlyStatus())
    m_readOnlyStatus = new QToolButton(this);
    m_readOnlyStatus->setObjectName(QStringLiteral("readOnlyStatus"));
    m_readOnlyStatus->setAutoRaise(true);
    m_readOnlyStatus->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    //: In the status bar while the open book is read-only. In Polish: Tylko do odczytu
    m_readOnlyStatus->setDefaultAction(core::ArtProvider::getInstance().createAction(
        QStringLiteral("common.lock"), tr("Read-only"), m_readOnlyStatus,
        core::IconContext::StatusBar));
    connect(m_readOnlyStatus->defaultAction(), &QAction::triggered, this, [this]() {
        statusBar()->showMessage(readOnlyBookMessage(book()), READ_ONLY_MESSAGE_MS);
    });
    m_readOnlyStatus->hide();
    statusBar()->addPermanentWidget(m_readOnlyStatus);

    // Page of the cursor (Page Layout view) and zoom of the editor in front
    m_pageLabel = new QLabel(this);
    m_pageLabel->setFrameStyle(QFrame::NoFrame);
    m_pageLabel->setMinimumWidth(110);
    m_pageLabel->hide();

    m_zoomLabel = new QLabel(this);
    m_zoomLabel->setFrameStyle(QFrame::NoFrame);
    m_zoomLabel->setMinimumWidth(60);
    m_zoomLabel->hide();

    // Add widgets to status bar (permanent = right side)
    statusBar()->addPermanentWidget(m_pageLabel);
    statusBar()->addPermanentWidget(m_zoomLabel);

    // Words, characters and reading time of the document in front, right of the zoom
    m_statusBarStatistics = new StatusBarStatistics(statusBar(), this);

    logger.debug("Status bar created successfully with statistics labels");
}

// =============================================================================
// Document Operations - MOVED to DocumentCoordinator (OpenSpec #00038 Phase 7)
// onNewDocument, onNewProject, onOpenDocument, onOpenRecentFile,
// onSaveDocument, onSaveAsDocument, onSaveAll are now in document_coordinator.cpp
// =============================================================================

void MainWindow::onExit() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Exit");
    // Every window closes, asking once about the changes of all of them
    if (closeAllWindows(this)) {
        QApplication::quit();
    }
}

// The edit commands work on the element with the keys (EditCoordinator): where they cannot,
// they do nothing

void MainWindow::onUndo() {
    core::Logger::getInstance().info("Action triggered: Undo");
    if (m_editCoordinator && m_editCoordinator->execute(EditCoordinator::Edit::Undo)) {
        statusBar()->showMessage(tr("Undo performed"), 2000);
    }
}

void MainWindow::onRedo() {
    core::Logger::getInstance().info("Action triggered: Redo");
    if (m_editCoordinator && m_editCoordinator->execute(EditCoordinator::Edit::Redo)) {
        statusBar()->showMessage(tr("Redo performed"), 2000);
    }
}

void MainWindow::onCut() {
    core::Logger::getInstance().info("Action triggered: Cut");
    if (m_editCoordinator && m_editCoordinator->execute(EditCoordinator::Edit::Cut)) {
        statusBar()->showMessage(tr("Cut to clipboard"), 2000);
    }
}

void MainWindow::onCopy() {
    core::Logger::getInstance().info("Action triggered: Copy");
    if (m_editCoordinator && m_editCoordinator->execute(EditCoordinator::Edit::Copy)) {
        statusBar()->showMessage(tr("Copied to clipboard"), 2000);
    }
}

void MainWindow::onPaste() {
    core::Logger::getInstance().info("Action triggered: Paste");
    if (m_editCoordinator && m_editCoordinator->execute(EditCoordinator::Edit::Paste)) {
        statusBar()->showMessage(tr("Pasted from clipboard"), 2000);
    }
}

void MainWindow::onSelectAll() {
    core::Logger::getInstance().info("Action triggered: Select All");
    if (!m_editCoordinator) {
        return;
    }
    // A list selects its items: there is no text to tell of
    QWidget* element = m_editCoordinator->focusedElement();
    if (m_editCoordinator->execute(EditCoordinator::Edit::SelectAll, element) &&
        qobject_cast<QAbstractItemView*>(element) == nullptr) {
        statusBar()->showMessage(tr("All text selected"), 2000);
    }
}

void MainWindow::onFind() {
    core::Logger::getInstance().info("Action triggered: Find");

    // A list of materials in front: its search
    if (MaterialListPanel* list = m_materialListCoordinator != nullptr
                                      ? m_materialListCoordinator->currentList()
                                      : nullptr) {
        list->focusSearch();
        return;
    }
    EditorPanel* editor = getCurrentEditor();
    if (editor && editor->getBookEditor()) {
        editor->getBookEditor()->showFind();
    }
}

void MainWindow::onFindNext() {
    core::Logger::getInstance().info("Action triggered: Find Next");

    EditorPanel* editor = getCurrentEditor();
    if (editor && editor->getBookEditor()) {
        editor->getBookEditor()->findNext();
    }
}

void MainWindow::onFindPrevious() {
    core::Logger::getInstance().info("Action triggered: Find Previous");

    EditorPanel* editor = getCurrentEditor();
    if (editor && editor->getBookEditor()) {
        editor->getBookEditor()->findPrevious();
    }
}

void MainWindow::onFindReplace() {
    core::Logger::getInstance().info("Action triggered: Find & Replace");

    EditorPanel* editor = getCurrentEditor();
    if (editor && editor->getBookEditor()) {
        editor->getBookEditor()->showFindReplace();
    }
}

// =============================================================================
// Format Actions (OpenSpec #00042 Phase 7.2)
// =============================================================================

void MainWindow::onFormatBold() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Format Bold");

    EditorPanel* editor = getCurrentEditor();
    if (editor && editor->getBookEditor()) {
        editor->getBookEditor()->toggleBold();
    }
}

void MainWindow::onFormatItalic() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Format Italic");

    EditorPanel* editor = getCurrentEditor();
    if (editor && editor->getBookEditor()) {
        editor->getBookEditor()->toggleItalic();
    }
}

void MainWindow::onFormatUnderline() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Format Underline");

    EditorPanel* editor = getCurrentEditor();
    if (editor && editor->getBookEditor()) {
        editor->getBookEditor()->toggleUnderline();
    }
}

void MainWindow::onFormatStrikethrough() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Format Strikethrough");

    EditorPanel* editor = getCurrentEditor();
    if (editor && editor->getBookEditor()) {
        editor->getBookEditor()->toggleStrikethrough();
    }
}

void MainWindow::applyColor(const QColor& color, bool highlight) {
    core::Logger::getInstance().info("Action triggered: {} {}",
                                     highlight ? "Highlight Color" : "Font Color",
                                     color.isValid() ? color.name().toStdString() : "none");
    EditorPanel* editor = getCurrentEditor();
    if (editor && editor->getBookEditor()) {
        editor::CharacterFormat changes;
        (highlight ? changes.highlight : changes.color) = color;
        editor->getBookEditor()->applyCharacterFormat(changes);
    }
    updateEditorActionStates();
}

void MainWindow::onAlignLeft() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Align Left");

    EditorPanel* editor = getCurrentEditor();
    if (editor && editor->getBookEditor()) {
        editor->getBookEditor()->setAlignLeft();
    }
}

void MainWindow::onAlignCenter() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Align Center");

    EditorPanel* editor = getCurrentEditor();
    if (editor && editor->getBookEditor()) {
        editor->getBookEditor()->setAlignCenter();
    }
}

void MainWindow::onAlignRight() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Align Right");

    EditorPanel* editor = getCurrentEditor();
    if (editor && editor->getBookEditor()) {
        editor->getBookEditor()->setAlignRight();
    }
}

void MainWindow::onAlignJustify() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: Justify");

    EditorPanel* editor = getCurrentEditor();
    if (editor && editor->getBookEditor()) {
        editor->getBookEditor()->setAlignJustify();
    }
}

// =============================================================================
// View Mode Actions (OpenSpec #00042 Phase 7.3)
// =============================================================================

void MainWindow::onViewModeContinuous() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: View Mode Continuous");

    EditorPanel* editor = getCurrentEditor();
    if (editor && editor->getBookEditor()) {
        editor->getBookEditor()->setViewMode(editor::ViewMode::Continuous);
        statusBar()->showMessage(tr("View mode: Continuous"), 2000);
    }
    // Choosing the mode in use unchecks its action, but the mode stays: check again
    updateEditorActionStates();
}

void MainWindow::onViewModePage() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: View Mode Page Layout");

    EditorPanel* editor = getCurrentEditor();
    if (editor && editor->getBookEditor()) {
        editor->getBookEditor()->setViewMode(editor::ViewMode::Page);
        statusBar()->showMessage(tr("View mode: Page Layout"), 2000);
    }
    // Choosing the mode in use unchecks its action, but the mode stays: check again
    updateEditorActionStates();
}

void MainWindow::onTypewriterToggle() {
    auto& settings = core::SettingsManager::getInstance();

    // A setting of all editors, kept between sessions: every editor follows it
    const bool enabled = !settings.get<bool>("editor.typewriter.enabled");
    settings.set<bool>("editor.typewriter.enabled", enabled);
    core::Logger::getInstance().info("Action triggered: Typewriter Scrolling {}",
                                     enabled ? "on" : "off");
    statusBar()->showMessage(enabled ? tr("Typewriter scrolling: on")
                                     : tr("Typewriter scrolling: off"), 2000);
}

void MainWindow::onFocusToggle() {
    auto& settings = core::SettingsManager::getInstance();

    // A setting of all editors, kept between sessions: every editor follows it, the view
    // mode stays
    const bool enabled = !settings.get<bool>("editor.focus.enabled");
    settings.set<bool>("editor.focus.enabled", enabled);
    core::Logger::getInstance().info("Action triggered: Focus {}", enabled ? "on" : "off");
    statusBar()->showMessage(enabled ? tr("Focus: on") : tr("Focus: off"), 2000);
}

void MainWindow::onDarkPaperToggle() {
    auto& settings = core::SettingsManager::getInstance();

    // A setting of all editors, kept between sessions, as in the editor's context menu:
    // every editor follows it
    const bool dark = !settings.get<bool>("editor.darkMode");
    settings.set<bool>("editor.darkMode", dark);
    core::Logger::getInstance().info("Action triggered: Dark Paper {}", dark ? "on" : "off");
    statusBar()->showMessage(dark ? tr("Dark paper: on") : tr("Dark paper: off"), 2000);
}

void MainWindow::onTextFrameBorderToggle() {
    auto& settings = core::SettingsManager::getInstance();

    // A setting of all editors, kept between sessions, as in the Settings dialog: every
    // editor follows it
    const bool show = !settings.get<bool>("editor.textFrameBorder.show");
    settings.set<bool>("editor.textFrameBorder.show", show);
    core::Logger::getInstance().info("Action triggered: Text Frame Border {}", show ? "on" : "off");
    statusBar()->showMessage(show ? tr("Text frame border: on") : tr("Text frame border: off"),
                             2000);
}

void MainWindow::onAnnotationCardsToggle() {
    auto& settings = core::SettingsManager::getInstance();

    // A setting of all editors, kept between sessions, as in the Settings dialog: every
    // editor follows it
    const bool show = !settings.get<bool>("editor.annotationCards.show");
    settings.set<bool>("editor.annotationCards.show", show);
    core::Logger::getInstance().info("Action triggered: Annotation Cards Under Paragraphs {}",
                                     show ? "on" : "off");
    statusBar()->showMessage(show ? tr("Annotation cards under paragraphs: shown")
                                  : tr("Annotation cards under paragraphs: hidden"),
                             2000);
}

void MainWindow::onFormattingMarksToggle() {
    auto& settings = core::SettingsManager::getInstance();

    // A setting of all editors, kept between sessions: every editor follows it
    const bool show = !settings.get<bool>("editor.showFormattingMarks");
    settings.set<bool>("editor.showFormattingMarks", show);
    core::Logger::getInstance().info("Action triggered: Show Formatting Marks {}",
                                     show ? "on" : "off");
    statusBar()->showMessage(show ? tr("Formatting marks: shown") : tr("Formatting marks: hidden"),
                             2000);
}

void MainWindow::onMarginNumbering(const QString& mode) {
    // A setting of all editors, kept between sessions, as in the Settings dialog: every
    // editor follows it. The numbering on chosen again (its toolbar button) turns it off.
    const std::string chosen = utils::chooseSetting("editor.marginNumbers.mode",
                                                    mode.toStdString(),
                                                    editor::MARGIN_NUMBERING_NONE);
    core::Logger::getInstance().info("Action triggered: Margin Numbering {}", chosen);

    QString message = tr("Margin numbering: off");
    switch (editor::marginNumberingFromName(QString::fromStdString(chosen))) {
        case editor::MarginNumbering::Lines: message = tr("Margin numbering: lines"); break;
        case editor::MarginNumbering::Paragraphs: message = tr("Margin numbering: paragraphs"); break;
        case editor::MarginNumbering::None: break;
    }
    statusBar()->showMessage(message, 2000);
}

bool MainWindow::isDistractionFree() const {
    return m_distractionFreeLayout != nullptr && m_distractionFreeLayout->isActive();
}

void MainWindow::onDistractionFreeToggle() {
    auto& logger = core::Logger::getInstance();
    const bool enabled = !isDistractionFree();
    logger.info("Action triggered: Distraction-Free {}", enabled ? "on" : "off");

    // The chapter tabs, and the button by them, hide with the window's bars and panels
    QTabWidget* centralTabs = m_dockCoordinator->centralTabs();
    QList<QWidget*> alsoHidden;
    if (centralTabs) {
        alsoHidden.append(centralTabs->tabBar());
        if (QWidget* corner = centralTabs->cornerWidget(Qt::TopRightCorner)) {
            alsoHidden.append(corner);
        }
    }
    m_distractionFreeLayout->setActive(enabled, alsoHidden);
}

void MainWindow::onDistractionFreeChanged(bool enabled) {
    // The info bar of a file outside the project comes back with the other bars
    m_documentCoordinator->setInfoBarAllowed(!enabled);

    // Every editor darkens the sides of its view and shows the word count; an editor
    // opened meanwhile gets it when it comes to the front
    for (QWidget* page : DocumentWindows::of(*m_dockCoordinator->centralTabs()).pages()) {
        auto* editor = qobject_cast<EditorPanel*>(page);
        if (editor && editor->getBookEditor()) {
            editor->getBookEditor()->setDistractionFree(enabled);
        }
    }
    if (EditorPanel* editor = getCurrentEditor(); editor && editor->getBookEditor()) {
        editor->getBookEditor()->setFocus();
    }


    auto& registry = commands();
    registry.updateActionState("view.mode.distraction-free");
    registry.updateActionState("view.fullScreen");
}

void MainWindow::addTableCommands() {
    // Each greyed out where it does nothing: over a chapter of a book open read-only those
    // that change the text, and outside a table those of a table
    using Text = const editor::BookEditor&;
    const auto commands = [](std::initializer_list<const char*> ids) {
        return std::vector<std::string>(ids.begin(), ids.end());
    };
    std::vector<std::string> tableCommands(std::begin(TABLE_COMMANDS), std::end(TABLE_COMMANDS));
    m_editCoordinator->addTextCommands(
        tableCommands, [](Text text) { return !text.isReadOnly() && text.isInTable(); });
    m_editCoordinator->addTextCommands(
        std::vector<std::string>(std::begin(TABLE_SELECT_COMMANDS), std::end(TABLE_SELECT_COMMANDS)),
        [](Text text) { return text.isInTable(); });
    m_editCoordinator->addTextCommands(commands({"table.insert"}), [](Text text) {
        return !text.isReadOnly() && !text.isInTable();
    });
    m_editCoordinator->addTextCommands(commands({"table.mergeCells"}), [](Text text) {
        return !text.isReadOnly() && text.canMergeTableCells();
    });
    m_editCoordinator->addTextCommands(commands({"table.headerRows"}), [](Text text) {
        return !text.isReadOnly() && text.canChangeTableHeader();
    });
    m_editCoordinator->addTextCommands(commands({"table.sort"}), [](Text text) {
        return !text.isReadOnly() && text.sortSubject().has_value();
    });
    m_editCoordinator->addTextCommands(commands({"table.textToTable"}), [](Text text) {
        return !text.isReadOnly() && text.canConvertTextToTable();
    });

    // Why
    CommandRegistry& registry = *m_commands;  // commands() is the list of ids here
    const auto explain = [this, &registry](const std::string& id,
                                           const std::function<QString(Text)>& why) {
        if (Command* command = registry.getCommand(id)) {
            command->disabledReason = [this, why]() {
                const editor::BookEditor* text = m_editCoordinator->textWithKeys();
                if (text == nullptr) {
                    return QString();
                }
                return text->isReadOnly() ? readOnlyReason() : why(*text);
            };
        }
    };
    //: Why the commands of the Table menu are greyed out outside a table. In Polish: Kursor
    //: nie jest w tabeli
    const QString notInTable = tr("The cursor is not in a table");
    for (const std::string& id : tableCommands) {
        explain(id, [notInTable](Text text) { return text.isInTable() ? QString() : notInTable; });
    }
    explain("table.insert", [](Text text) { return text.isInTable() ? inTableReason() : QString(); });
    explain("table.mergeCells", [notInTable](Text text) {
        //: Why Table > Merge Cells is greyed out. In Polish: Zaznacz komórki do scalenia
        return text.isInTable() ? tr("Select the cells to merge") : notInTable;
    });
    explain("table.headerRows", [notInTable](Text text) {
        //: Why Table > Repeat Header Rows is greyed out in a row under the body's first one
        return text.isInTable() ? tr("The header rows are the first rows of the table: put "
                                     "the cursor in them or in the row under them")
                                : notInTable;
    });
    explain("table.sort", [](Text text) { return text.sortRefusal(); });
    explain("table.textToTable", [](Text text) {
        return text.isInTable()
                   //: Why Table > Convert > Text to Table... is greyed out in a table
                   ? tr("The cursor is in a table: Table to Text... makes text of it")
                   //: Why Table > Convert > Text to Table... is greyed out for a selection
                   : tr("Only paragraphs of text become a table: the selection holds a table "
                        "or an object");
    });
}

void MainWindow::onTableCommand(editor::BookEditor& text, TableCommand command) {
    // The windows are those of the text's panel (the one in front, without one)
    EditorPanel* panel = panelOf(text, getCurrentEditor());
    using Part = editor::BookEditor::TablePart;
    switch (command) {
    case TableCommand::InsertTable:
        if (panel != nullptr) {
            panel->showInsertTableDialog();
        }
        return;
    case TableCommand::InsertRowsAbove:
        text.insertTableRows(false);
        return;
    case TableCommand::InsertRowsBelow:
        text.insertTableRows(true);
        return;
    case TableCommand::InsertColumnsLeft:
        text.insertTableColumns(false);
        return;
    case TableCommand::InsertColumnsRight:
        text.insertTableColumns(true);
        return;
    case TableCommand::DeleteRows:
        text.deleteTableRows();
        return;
    case TableCommand::DeleteColumns:
        text.deleteTableColumns();
        return;
    case TableCommand::DeleteTable:
        text.deleteTable();
        return;
    case TableCommand::SelectCell:
        text.selectTablePart(Part::Cell);
        return;
    case TableCommand::SelectRow:
        text.selectTablePart(Part::Row);
        return;
    case TableCommand::SelectColumn:
        text.selectTablePart(Part::Column);
        return;
    case TableCommand::SelectTable:
        text.selectTablePart(Part::Table);
        return;
    case TableCommand::MergeCells:
        text.mergeTableCells();
        return;
    case TableCommand::DistributeRows:
        text.distributeTableRows();
        return;
    case TableCommand::DistributeColumns:
        text.distributeTableColumns();
        return;
    case TableCommand::HeaderRows:
        text.setTableHeader(!text.isTableHeader());
        return;
    case TableCommand::SplitCells:
    case TableCommand::Sort:
    case TableCommand::TextToTable:
    case TableCommand::TableToText:
    case TableCommand::Properties:
        break;
    }
    if (panel == nullptr || panel->getBookEditor() != &text) {
        return;
    }
    switch (command) {
    case TableCommand::SplitCells:
        panel->showSplitCellsDialog();
        break;
    case TableCommand::Sort:
        panel->showSortDialog();
        break;
    case TableCommand::TextToTable:
        panel->showTextToTableDialog();
        break;
    case TableCommand::TableToText:
        panel->showTableToTextDialog();
        break;
    case TableCommand::Properties:
        panel->showTablePropertiesDialog();
        break;
    default:
        break;
    }
}

void MainWindow::addImageCommands() {
    // Each greyed out where it does nothing: without a selected image, over a chapter of a
    // book open read-only those that change it, without its file those that need it
    using Text = const editor::BookEditor&;
    const auto commands = [](std::initializer_list<const char*> ids) {
        return std::vector<std::string>(ids.begin(), ids.end());
    };
    const auto changeable = [](Text text) {
        return !text.isReadOnly() && text.selectedImage().has_value();
    };
    m_editCoordinator->addTextCommands(
        std::vector<std::string>(std::begin(IMAGE_COMMANDS), std::end(IMAGE_COMMANDS)), changeable);
    m_editCoordinator->addTextCommands(commands({"image.crop"}), [](Text text) {
        const std::optional<int> image = text.selectedImage();
        return image && text.canCropImage(*image);
    });
    m_editCoordinator->addTextCommands(commands({"image.originalSize"}), [changeable](Text text) {
        const std::optional<editor::ImageProperties> image = selectedImageOf(text);
        return changeable(text) && image && image->unit != editor::ImageWidthUnit::Natural;
    });
    m_editCoordinator->addTextCommands(commands({"image.change"}), [changeable](Text text) {
        return changeable(text) && text.canInsertImage();
    });
    m_editCoordinator->addTextCommands(commands({"image.saveAs"}), [](Text text) {
        const std::optional<int> image = text.selectedImage();
        return image && QFileInfo(text.imageFilePath(*image)).isFile();
    });

    // The filter of the selected image
    CommandRegistry& registry = *m_commands;  // commands() is the list of ids here
    for (const auto& [id, filter] : IMAGE_FILTER_COMMANDS) {
        if (Command* command = registry.getCommand(id)) {
            command->isChecked = [this, wanted = filter]() {
                const editor::BookEditor* text =
                    m_editCoordinator ? m_editCoordinator->textWithKeys() : nullptr;
                const std::optional<editor::ImageProperties> image =
                    text != nullptr ? selectedImageOf(*text) : std::nullopt;
                return image && image->filter == wanted;
            };
        }
    }

    // Why
    const auto explain = [this, &registry](const std::string& id,
                                           const std::function<QString(Text)>& why) {
        if (Command* command = registry.getCommand(id)) {
            command->disabledReason = [this, why]() {
                const editor::BookEditor* text = m_editCoordinator->textWithKeys();
                if (text == nullptr) {
                    return QString();
                }
                //: Why the commands of the Image menu are greyed out. In Polish: Zaznacz obraz
                //: w tekście
                return !text->selectedImage() ? tr("Select an image in the text") : why(*text);
            };
        }
    };
    const auto readOnly = [](Text text) {
        return text.isReadOnly() ? readOnlyReason() : QString();
    };
    //: Why Image > Crop and Save as Image... are greyed out for an image whose file is not
    //: in the book's folder of images. In Polish: Brak pliku obrazu w folderze książki
    QString missing = tr("The image's file is not in the book's folder");
    for (const char* id : IMAGE_COMMANDS) {
        explain(id, readOnly);
    }
    explain("image.crop", [readOnly, missing](Text text) {
        return text.isReadOnly() ? readOnly(text) : missing;
    });
    explain("image.originalSize", [readOnly](Text text) {
        //: Why Image > Original Size is greyed out. In Polish: Obraz ma już rozmiar swojego
        //: pliku
        return text.isReadOnly() ? readOnly(text)
                                 : tr("The image already has the size of its file");
    });
    explain("image.change", [readOnly](Text text) {
        return text.isReadOnly() ? readOnly(text)
                                 : tr("Save the file first: its images are kept in a folder "
                                      "next to it");
    });
    explain("image.saveAs", [missing](Text) { return missing; });
}

void MainWindow::onImageCommand(editor::BookEditor& text, ImageCommand command) {
    const std::optional<int> image = text.selectedImage();
    if (!image) {
        return;
    }
    // The windows are those of the text's panel (the one in front, without one)
    EditorPanel* panel = panelOf(text, getCurrentEditor());
    if (panel != nullptr && panel->getBookEditor() != &text) {
        panel = nullptr;
    }
    using Page = dialogs::ImagePropertiesDialog::Page;
    const auto showPage = [panel, image](Page page) {
        if (panel != nullptr) {
            panel->showImagePropertiesDialog(*image, page);
        }
    };
    const auto filter = [&text, image](editor::ImageFilter look) {
        text.setImageFilter(*image, look);
    };
    switch (command) {
    case ImageCommand::Properties:
        showPage(Page::ImagePage);
        break;
    case ImageCommand::AddCaption:
        m_captionCoordinator->editCaption(text, *image);
        break;
    case ImageCommand::AltText:
        showPage(Page::DescriptionPage);
        break;
    case ImageCommand::RotateRight:
        text.rotateImage(*image, 1);
        break;
    case ImageCommand::RotateLeft:
        text.rotateImage(*image, -1);
        break;
    case ImageCommand::FlipHorizontal:
        text.flipImage(*image, Qt::Horizontal);
        break;
    case ImageCommand::FlipVertical:
        text.flipImage(*image, Qt::Vertical);
        break;
    case ImageCommand::RotateOptions:
        showPage(Page::SizePage);
        break;
    case ImageCommand::FilterNone:
        filter(editor::ImageFilter::None);
        break;
    case ImageCommand::FilterGrayscale:
        filter(editor::ImageFilter::Grayscale);
        break;
    case ImageCommand::FilterSepia:
        filter(editor::ImageFilter::Sepia);
        break;
    case ImageCommand::FilterBlackWhite:
        filter(editor::ImageFilter::BlackWhite);
        break;
    case ImageCommand::FilterWash:
        filter(editor::ImageFilter::Wash);
        break;
    case ImageCommand::FilterAll:
        m_imageFilterCoordinator->applyToBook(text, *image);
        break;
    case ImageCommand::ColorOptions:
        showPage(Page::ColorsPage);
        break;
    case ImageCommand::Crop:
        // The marks in the text, which takes the keys: Enter crops, Esc leaves it
        text.setFocus();
        text.startImageCrop(*image);
        break;
    case ImageCommand::OriginalSize:
        text.setImageOriginalSize(*image);
        break;
    case ImageCommand::Change:
        if (panel != nullptr) {
            panel->showChangeImageDialog(*image);
        }
        break;
    case ImageCommand::SaveAs:
        if (panel != nullptr) {
            panel->showSaveImageDialog(*image);
        }
        break;
    }
    // A check of Image > Filter follows at once, as a menu shows it
    m_editCoordinator->updateStates();
}

void MainWindow::updateEditorActionStates() {
    // Undo, Redo and the clipboard follow the element with the keys
    if (m_editCoordinator) {
        m_editCoordinator->updateStates();
    }

    // The commands of the document follow the editor in front, checked as the text at the
    // cursor; without a document (the Dashboard, no tab) none is checked and they are greyed
    // out (utils::bindToDocument())
    EditorPanel* editor = getCurrentEditor();
    editor::BookEditor* bookEditor = editor != nullptr ? editor->getBookEditor() : nullptr;
    CommandRegistry& registry = commands();
    const auto setChecked = [&registry](const std::string& id, bool checked) {
        if (Command* command = registry.getCommand(id)) {
            command->isChecked = [checked]() { return checked; };
        }
    };

    // Update format action checked states
    setChecked("format.bold", bookEditor != nullptr && bookEditor->isBold());
    setChecked("format.italic", bookEditor != nullptr && bookEditor->isItalic());
    setChecked("format.underline", bookEditor != nullptr && bookEditor->isUnderline());
    setChecked("format.strikethrough", bookEditor != nullptr && bookEditor->isStrikethrough());
    setChecked("format.superscript", bookEditor != nullptr && bookEditor->isSuperscript());
    setChecked("format.subscript", bookEditor != nullptr && bookEditor->isSubscript());
    setChecked("format.smallCaps", bookEditor != nullptr && bookEditor->isSmallCaps());
    setChecked("format.allCaps", bookEditor != nullptr && bookEditor->isAllCaps());

    // Update view mode checked states
    const auto inMode = [bookEditor](editor::ViewMode mode) {
        return bookEditor != nullptr && bookEditor->viewMode() == mode;
    };
    setChecked("view.mode.continuous", inMode(editor::ViewMode::Continuous));
    setChecked("view.mode.page", inMode(editor::ViewMode::Page));

    // Update alignment action checked states
    const Qt::Alignment alignment = bookEditor != nullptr
                                        ? bookEditor->currentAlignment() & Qt::AlignHorizontal_Mask
                                        : Qt::Alignment();
    setChecked("table.headerRows", bookEditor != nullptr && bookEditor->isTableHeader());
    setChecked("format.alignLeft", bookEditor != nullptr && alignment == Qt::AlignLeft);
    setChecked("format.alignCenter", alignment == Qt::AlignHCenter);
    setChecked("format.alignRight", alignment == Qt::AlignRight);
    setChecked("format.justify", alignment == Qt::AlignJustify);

    // The kind of the list item at the cursor
    const std::optional<editor::ListKind> listKind =
        bookEditor != nullptr ? bookEditor->currentListKind() : std::nullopt;
    setChecked("format.bullets", listKind == editor::ListKind::Bulleted);
    setChecked("format.numbering", listKind == editor::ListKind::Numbered);

    // The style of the paragraph at the cursor; none on a line or a break
    const QString style = bookEditor != nullptr ? bookEditor->currentParagraphStyle() : QString();
    for (const StyleCommand& command : styleCommands()) {
        setChecked(command.commandId, !style.isEmpty() && style == QLatin1String(command.styleId));
    }

    utils::updateDocumentCommands(registry);
    // Save, Save As and Save All follow the tabs: a chapter, a file outside the book, none
    utils::updateSaveCommands(registry);
    // The export works on the book or on the text in front
    if (m_bookExportCoordinator != nullptr) {
        m_bookExportCoordinator->updateCommands();
    }

    // Sync the font family/size toolbar controls to the char format under the caret,
    // so they reflect the formatting at the cursor (updates on every cursor move /
    // selection change), not just the global default set on tab activation.
    if (bookEditor != nullptr && m_toolbarManager) {
        m_toolbarManager->syncFontWidgetsToEditor(editor);
    }
}

bool MainWindow::hasDocumentInFront() {
    if (m_dockCoordinator == nullptr) {
        return false;
    }
    EditorPanel* editor = getCurrentEditor();
    return editor != nullptr && editor->getBookEditor() != nullptr;
}

void MainWindow::onSettings() {
    // OpenSpec #00038 Phase 5: Delegate to SettingsCoordinator
    m_settingsCoordinator->openSettingsDialog();
}

void MainWindow::onAbout() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: About Kalahari");

    dialogs::AboutDialog dialog(this);
    dialog.exec();

    logger.info("About dialog closed");
}

// Dock management - OpenSpec #00038 Phase 4: Delegated to DockCoordinator

void MainWindow::createDocks() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Creating dock widgets via DockCoordinator");

    // Create DockCoordinator to manage all dock widgets and panels
    m_dockCoordinator = new DockCoordinator(*m_book, *m_commands, this, this);
    m_dockCoordinator->createDocks();

    // Connect DockCoordinator signals to MainWindow slots or DocumentCoordinator
    // Note: navigatorElementSelected is kept for MainWindow delegation to NavigatorCoordinator
    connect(m_dockCoordinator, &DockCoordinator::navigatorElementSelected,
            this, &MainWindow::onNavigatorElementSelected);
    // OpenSpec #00038 Phase 7: Document operations delegated to DocumentCoordinator
    // These connections are made BEFORE DocumentCoordinator is created, so we use lambdas
    connect(m_dockCoordinator, &DockCoordinator::openRecentBookRequested,
            this, [this](const QString& path) {
                if (m_documentCoordinator) m_documentCoordinator->onOpenRecentFile(path);
            });
    connect(m_dockCoordinator, &DockCoordinator::addToProjectRequested,
            this, [this]() {
                if (m_documentCoordinator) m_documentCoordinator->onAddToProject();
            });
    connect(m_dockCoordinator, &DockCoordinator::openSettingsRequested,
            this, &MainWindow::onSettings);
    // The gear on the title bar of a panel opens the settings at the panel's page
    connect(m_dockCoordinator, &DockCoordinator::settingsPageRequested, this,
            [this](const QString& page) {
                if (m_settingsCoordinator) m_settingsCoordinator->openSettingsPage(page);
            });

    // NOTE: Navigator context menu signals and drag/drop reorder signals are connected
    // in the constructor after NavigatorCoordinator is created (OpenSpec #00038 Phase 6)

    // Connect PropertiesPanel status change to Navigator refresh
    connect(m_dockCoordinator, &DockCoordinator::chapterStatusChanged,
            m_dockCoordinator->navigatorPanel(), &NavigatorPanel::refreshItem);

    // A document brought to the front with a click on its tab gets the keys
    QTabWidget* centralTabs = m_dockCoordinator->centralTabs();
    utils::focusPageOnTabClick(*centralTabs);

    // The texts of the tabs, also in the windows of their own the tabs are moved to
    m_documentWindows = &DocumentWindows::of(*centralTabs);
    if (m_menuBuilder) {
        m_documentWindows->listWindowsIn(m_menuBuilder->getMenu("WINDOW"));
    }
    connect(m_documentWindows, &DocumentWindows::settingsRequested, this, [this]() {
        if (m_settingsCoordinator) {
            m_settingsCoordinator->openSettingsPage(
                QString::fromLatin1(SettingsDialog::GENERAL_PAGE));
        }
    });

    // Close a tab, of the book window or of a window of its own (Task #00015)
    const auto closeTab = [this](QWidget* widget) {
        auto& logger = core::Logger::getInstance();
        if (widget == nullptr) {
            return;
        }
        EditorPanel* editor = qobject_cast<EditorPanel*>(widget);

        // An annotation or a note being written in this tab is kept first
        if (editor && m_annotationsCoordinator) {
            m_annotationsCoordinator->finishWriting(editor->getBookEditor());
        }
        if (editor && m_footnoteCoordinator) {
            m_footnoteCoordinator->finishWriting(editor->getBookEditor());
        }

        // Prompt to save if THIS tab has unsaved changes (Bug#2 fix).
        if (editor && m_documentCoordinator && m_documentCoordinator->isEditorDirty(editor)) {
            const auto reply = dialogs::MessageDialog::ask(
                this, tr("Unsaved Changes"),
                tr("This document has unsaved changes.\n\nDo you want to save before closing?"),
                tr("&Save"), tr("Do&n't Save"));

            if (reply == dialogs::MessageDialog::Answer::Cancel) {
                return;  // Do NOT close the tab.
            }

            if (reply == dialogs::MessageDialog::Answer::Accept) {
                // Close the tab only once its content is saved: a failed or cancelled
                // save keeps it open, with its changes
                if (!m_documentCoordinator->saveEditor(editor)) {
                    return;
                }
            } else {  // Don't Save
                m_documentCoordinator->discardEditorChanges(editor);
            }
        }

        // A chapter opens next time where its cursor is now
        rememberChapterCursor(editor);

        // Remove tab and delete widget
        // (the keys go on to the page in front, not to the font box of the toolbar)
        DocumentWindows::removePage(widget);
        widget->deleteLater();
        // Save All saves the files of all tabs: the last file outside a book leaves it
        // nothing, also from a tab behind the one in front
        utils::updateSaveCommands(commands());

        logger.debug("Tab closed");
    };
    connect(m_dockCoordinator, &DockCoordinator::tabCloseRequested, this,
            [centralTabs, closeTab](int index) { closeTab(centralTabs->widget(index)); });
    connect(m_documentWindows, &DocumentWindows::closeRequested, this, closeTab);

    // Connect tab change to navigator highlight (OpenSpec #00034 Phase C)
    // Also update action states when switching tabs (OpenSpec #00042 Phase 7.3)
    // Also connect properties panel to active editor (OpenSpec #00042 Task 7.4)
    // Also sync font toolbar widgets (OpenSpec #00042 Phase 7.2)
    connect(m_documentWindows, &DocumentWindows::frontChanged, this, [this](QWidget* page) {
        NavigatorPanel* navigatorPanel = m_dockCoordinator->navigatorPanel();
        PropertiesPanel* propertiesPanel = m_dockCoordinator->propertiesPanel();

        // The page, the zoom and the counts of the document in front in the status bar
        // (none over the Dashboard or without a tab)
        EditorPanel* front = getCurrentEditor();
        m_statusBarStatistics->setEditor(front ? front->getBookEditor() : nullptr);
        // The zoom of the preview of a material follows it
        if (ImagePreviewPanel* preview = currentPreview()) {
            connect(preview, &ImagePreviewPanel::zoomChanged, this,
                    &MainWindow::updatePageStatus, Qt::UniqueConnection);
        }
        updatePageStatus();

        // Distraction-Free needs a document in front: it goes off over the Dashboard or
        // without a tab, and its command waits for a document
        if (isDistractionFree()
            && (getCurrentEditor() == nullptr || m_documentWindows->isDetached(page))) {
            m_distractionFreeLayout->setActive(false);
        }
        commands().updateActionState("view.mode.distraction-free");

        if (page == nullptr) {
            navigatorPanel->clearHighlight();
            // OpenSpec #00042 Task 7.4: Disconnect from editor when no tab
            if (propertiesPanel) {
                propertiesPanel->setActiveEditor(nullptr);
            }
            m_dockCoordinator->setParagraphEditor(nullptr);
            // OpenSpec #00042 Phase 7.2: Sync font toolbar (disabled state)
            if (m_toolbarManager) {
                m_toolbarManager->syncFontWidgetsToEditor(nullptr);
            }
            // No document: its commands greyed out
            updateEditorActionStates();
            return;
        }

        EditorPanel* editor = qobject_cast<EditorPanel*>(page);
        if (editor) {
            QString elementId = editor->property("elementId").toString();
            if (!elementId.isEmpty()) {
                navigatorPanel->highlightElement(elementId);
            } else {
                navigatorPanel->clearHighlight();
            }

            // OpenSpec #00042 Phase 7.3: Connect BookEditor signals for action state updates
            if (editor::BookEditor* bookEditor = editor->getBookEditor()) {
                // Disconnect any previous connections to avoid duplicates
                disconnect(bookEditor, &editor::BookEditor::selectionChanged, this, nullptr);
                disconnect(bookEditor, &editor::BookEditor::cursorPositionChanged, this, nullptr);
                disconnect(bookEditor, &editor::BookEditor::viewModeChanged, this, nullptr);
                disconnect(bookEditor, &editor::BookEditor::currentPageChanged, this, nullptr);
                disconnect(bookEditor, &editor::BookEditor::totalPagesChanged, this, nullptr);
                disconnect(bookEditor, &editor::BookEditor::zoomChanged, this, nullptr);
                disconnect(bookEditor, &editor::BookEditor::contentChanged, this, nullptr);
                disconnect(bookEditor, &editor::BookEditor::changeRefused, this, nullptr);
                disconnect(bookEditor, &editor::BookEditor::searchColorsRequested, this, nullptr);

                // Connect to update action states when selection/cursor/viewMode changes
                connect(bookEditor, &editor::BookEditor::selectionChanged,
                        this, &MainWindow::updateEditorActionStates);
                // OpenSpec #00043: Use debounced updates for cursor changes
                connect(bookEditor, &editor::BookEditor::cursorPositionChanged,
                        this, [this](const editor::CursorPosition&) {
                    if (m_actionStateDebounceTimer) {
                        m_actionStateDebounceTimer->start();  // Restart timer on each move
                    }
                });
                // Also a change with the cursor where it was, such as one made in the
                // Annotations panel: Undo is to be enabled for it
                connect(bookEditor, &editor::BookEditor::contentChanged, this, [this]() {
                    if (m_actionStateDebounceTimer) {
                        m_actionStateDebounceTimer->start();
                    }
                });
                connect(bookEditor, &editor::BookEditor::viewModeChanged,
                        this, [this](editor::ViewMode) {
                    updateEditorActionStates();
                    updatePageStatus();
                });

                // A key or a command that would change a chapter of a book open read-only:
                // the status bar says why nothing happened
                connect(bookEditor, &editor::BookEditor::changeRefused, this, [this]() {
                    statusBar()->showMessage(readOnlyReason(), READ_ONLY_MESSAGE_MS);
                });

                // The gear of the find bar: the colors of the hits
                connect(bookEditor, &editor::BookEditor::searchColorsRequested, this, [this]() {
                    if (m_settingsCoordinator) {
                        m_settingsCoordinator->openSettingsPage(
                            QString::fromLatin1(SettingsDialog::EDITOR_COLORS_PAGE));
                    }
                });

                // Page of the cursor and zoom in the status bar
                connect(bookEditor, &editor::BookEditor::currentPageChanged,
                        this, [this](int) { updatePageStatus(); });
                connect(bookEditor, &editor::BookEditor::totalPagesChanged,
                        this, [this](int) { updatePageStatus(); });
                connect(bookEditor, &editor::BookEditor::zoomChanged,
                        this, [this](double) { updatePageStatus(); });

                // Also an editor opened during Distraction-Free writing
                bookEditor->setDistractionFree(isDistractionFree());
            }

            // OpenSpec #00042 Task 7.4: Connect properties panel to active editor
            if (propertiesPanel) {
                propertiesPanel->setActiveEditor(editor);
            }
            // The paragraph at its cursor in the Paragraph and Styles panel
            m_dockCoordinator->setParagraphEditor(editor->getBookEditor());

            // OpenSpec #00042 Phase 7.2: Sync font toolbar to current editor
            if (m_toolbarManager) {
                m_toolbarManager->syncFontWidgetsToEditor(editor);
            }

            // Update action states immediately for the new tab
            updateEditorActionStates();
        } else {
            navigatorPanel->clearHighlight();
            // OpenSpec #00042 Task 7.4: Disconnect from editor when switching to non-editor tab
            if (propertiesPanel) {
                propertiesPanel->setActiveEditor(nullptr);
            }
            m_dockCoordinator->setParagraphEditor(nullptr);
            // OpenSpec #00042 Phase 7.2: Sync font toolbar (disabled state)
            if (m_toolbarManager) {
                m_toolbarManager->syncFontWidgetsToEditor(nullptr);
            }
            // The Dashboard: the commands of the document greyed out
            updateEditorActionStates();
        }
    });

    // OpenSpec #00032: Connect ArtProvider::resourcesChanged() to refresh dock title bar icons
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged,
            m_dockCoordinator, &DockCoordinator::refreshDockIcons);

    // Central tab icons are plain QIcons (not managed QActions), so they must be
    // refreshed explicitly on theme/color change, same as dock icons above.
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged,
            m_dockCoordinator, &DockCoordinator::refreshTabIcons);

    // Get VIEW menu reference for toolbar actions
    if (m_menuBuilder) {
        m_viewMenu = m_menuBuilder->getMenu("VIEW");
    }
    if (!m_viewMenu) {
        m_viewMenu = menuBar()->addMenu(tr("&View"));
    }

    // Task #00019: Add toolbar toggle actions to View menu
    if (m_toolbarManager) {
        m_toolbarManager->createViewMenuActions(m_viewMenu);
        logger.debug("Toolbar toggle actions added to VIEW menu");

        // OpenSpec #00042 Phase 7.2: Connect font toolbar widgets to active editor
        m_toolbarManager->connectFontWidgets([this]() -> EditorPanel* {
            return getCurrentEditor();
        });
        // Initially sync font widgets (disabled, no editor)
        m_toolbarManager->syncFontWidgetsToEditor(nullptr);
        logger.debug("Font toolbar widgets connected to editor callback");
    }

    logger.debug("Dock widgets created successfully via DockCoordinator");
}

void MainWindow::resetLayout() {
    // The toolbars as well: a floating one, or one by another edge, docks at the top again
    if (m_toolbarManager) {
        m_toolbarManager->applyDefaultLayout();
    }
    m_dockCoordinator->resetLayout(isDiagnosticMode(), isDevMode());
    statusBar()->showMessage(tr("Layout reset to default"), 2000);
}

void MainWindow::showBookProperties() {
    if (m_dockCoordinator == nullptr || !book().isProjectOpen()) {
        return;
    }
    QDockWidget* dock = m_dockCoordinator->propertiesDock();
    dock->show();
    // In a tab group the panel would lie under the current tab; a floating one is a window
    dock->raise();
    if (dock->isFloating()) {
        dock->activateWindow();
    }
    m_dockCoordinator->propertiesPanel()->editProjectProperties();
}

void MainWindow::updateReadOnlyStatus() {
    if (m_readOnlyStatus == nullptr) {
        return;
    }
    const QString message = readOnlyBookMessage(book());
    m_readOnlyStatus->defaultAction()->setToolTip(message);
    m_readOnlyStatus->setVisible(!message.isEmpty());
}

void MainWindow::updateBookCommands() {
    auto& registry = commands();
    if (Command* newText = registry.getCommand("book.newChapter")) {
        // Named after the texts it adds: "New Story..." in a collection of short stories,
        // "New Chapter..." without a book
        const auto& projects = book();
        const core::KindRef kind = projects.isProjectOpen()
                                       ? projects.chapterKindFor(core::BookPlace::Main)
                                       : core::KindRef{};
        const QString menuText = newTextMenuText(kind);
        newText->menuText = menuText.toStdString();
        newText->label = withoutAccessKey(menuText).toStdString();
        newText->tooltip = newText->label;
        if (QAction* action = registry.getAction(std::string("book.newChapter"))) {
            action->setText(menuText);
        }
    }
    if (Command* textBreak = registry.getCommand(ChapterBreakCoordinator::COMMAND)) {
        // Named after the texts of the book too: "Story Break"
        const auto& projects = book();
        const core::KindRef kind = projects.isProjectOpen()
                                       ? projects.chapterKindFor(core::BookPlace::Main)
                                       : core::KindRef{};
        const QString menuText = textBreakMenuText(kind);
        textBreak->menuText = menuText.toStdString();
        textBreak->label = withoutAccessKey(menuText).toStdString();
        textBreak->tooltip = textBreak->label;
        if (QAction* action = registry.getAction(std::string(ChapterBreakCoordinator::COMMAND))) {
            action->setText(menuText);
        }
    }
    registry.updateActionState("book.addElement");
    registry.updateActionState("book.newChapter");
    for (const auto& [commandId, kindId] : WORKSHOP_COMMANDS) {
        registry.updateActionState(commandId);
    }
    registry.updateActionState("book.properties");
    utils::updateOpenBookCommands(registry);
    if (m_bookExportCoordinator != nullptr) {
        m_bookExportCoordinator->updateCommands();
    }
    // A book open read-only says so in the status bar, and in the title of the window
    updateReadOnlyStatus();
    // Save saves the open book also with no tab of it in front
    utils::updateSaveCommands(registry);
}

void MainWindow::setStartupFile(const QString& path) {
    m_startupFile = path;
}

void MainWindow::onLaterStart(const QString& path) {
    auto& logger = core::Logger::getInstance();
    logger.info("A later start of the program handed over: {}", path.toStdString());

    // The window comes to the front as it was, maximized or not
    if (isMinimized()) {
        setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
    }
    show();
    raise();
    activateWindow();

    // A window that asks something stays in front, and is answered first
    if (QWidget* modal = QApplication::activeModalWidget()) {
        modal->raise();
        modal->activateWindow();
        if (!path.isEmpty()) {
            logger.info("Not opened while a window asks something: {}", path.toStdString());
        }
        return;
    }
    if (!path.isEmpty() && m_documentCoordinator) {
        // As from the recent books: a question about the open book comes in its turn
        QTimer::singleShot(0, this, [this, path]() {
            if (m_documentCoordinator) m_documentCoordinator->onOpenRecentFile(path);
        });
    }
}

QMenu* MainWindow::createPopupMenu() {
    // Qt's menu names the panels and the toolbars after their titles, without access keys:
    // a copy gives them keys, and the panels and the toolbars keep their titles
    const std::unique_ptr<QMenu> qtMenu(QMainWindow::createPopupMenu());
    return qtMenu != nullptr ? menuWithAccessKeys(*qtMenu, this) : nullptr;
}

bool MainWindow::hasUnsavedChanges() const {
    // The book (the texts of its open elements and its data) and the files outside it whose
    // texts differ from their saved texts; the files are checked with or without a book
    return m_documentCoordinator && !m_documentCoordinator->unsavedDocumentNames().isEmpty();
}

// Perspective save/restore
void MainWindow::closeEvent(QCloseEvent* event) {
    auto& logger = core::Logger::getInstance();
    logger.debug("MainWindow::closeEvent triggered");

    // Single source of truth for all unsaved-changes prompts (content + structure
    // + standalone tabs). Fixes Bug#3 - previously divergent OR of competing flags.
    auto& pm = book();

    // An annotation or a note being written is kept first
    finishWriting();

    // File > Exit asked about the changes of every window together
    if (!closingAllWindows() && hasUnsavedChanges()) {
        const QStringList names = m_documentCoordinator
            ? m_documentCoordinator->unsavedDocumentNames() : QStringList();
        const QString filename = names.isEmpty() ? tr("Untitled") : names.join(QStringLiteral(", "));

        const auto reply = dialogs::MessageDialog::ask(
            this, tr("Unsaved Changes"), tr("Do you want to save changes to %1?").arg(filename),
            tr("&Save"), tr("Do&n't Save"));

        if (reply == dialogs::MessageDialog::Answer::Accept) {
            // Save the project and every editor tab; whatever could not be saved (failed
            // or cancelled) keeps the window open with its changes
            if (!m_documentCoordinator || !m_documentCoordinator->saveAllChanges()) {
                event->ignore();
                return;
            }
        } else if (reply == dialogs::MessageDialog::Answer::Cancel) {
            event->ignore();
            return;
        }
        // Don't Save -> continue with close
    }

    // The summary of the session of writing, when the settings say so
    if (m_writingStatsCoordinator) {
        m_writingStatsCoordinator->showSessionSummary();
    }

    // The last window ends the work: its book is the book of the last work, opened at the
    // next start (File > Exit keeps the books of every window)
    if (!closingAllWindows() && bookWindowList() == QList<MainWindow*>{this}) {
        core::RecentBooksManager::getInstance().setOpenBooks(
            pm.isProjectOpen() ? QStringList{pm.getManifestPath()} : QStringList());
    }

    // Phase F: Save Navigator expansion state before closing
    if (pm.isProjectOpen()) {
        // The open chapters open next time where their cursors are now
        rememberChapterCursors();

        QString projectPath = pm.getProjectPath();
        if (!projectPath.isEmpty()) {
            QFileInfo pathInfo(projectPath);
            QString projectId = pathInfo.absoluteFilePath()
                .replace("/", "_")
                .replace("\\", "_")
                .replace(":", "_")
                .replace(" ", "_");
            NavigatorPanel* navigator = m_dockCoordinator ? m_dockCoordinator->navigatorPanel() : nullptr;
            if (navigator) {
                navigator->saveExpansionState(projectId);
                logger.debug("Saved expansion state for project: {}", projectId.toStdString());
            }
        }
    }

    // The window's bars, panels and size from before Distraction-Free are the ones to keep
    if (isDistractionFree()) {
        m_distractionFreeLayout->setActive(false);
    }

    // Save perspective (existing code)
    logger.debug("Saving window perspective");

    auto& settings = core::SettingsManager::getInstance();
    settings.setBinary("window.geometry", saveGeometry());
    settings.setBinary("window.state", saveState());

    // Task #00019: Save toolbar state (visibility)
    if (m_toolbarManager) {
        m_toolbarManager->saveState();
    }

    // Save settings to disk (including Navigator expansion state)
    core::SettingsManager::getInstance().save();

    logger.debug("Window perspective saved");

    event->accept();
}

// Helper methods (Task #00008)
void MainWindow::updateWindowTitle() {
    // The open book, as when it is opened: "Rain - Kalahari"; without a book, the name of
    // the program alone, as when a book is closed
    const auto& pm = book();
    if (pm.isProjectOpen() && pm.book() && !pm.book()->title.isEmpty()) {
        QString title = pm.book()->title;
        if (pm.isReadOnly()) {
            //: After the title of a book open read-only, in the title bar of the window: "Rain
            //: [read-only] - Kalahari". In Polish: [tylko do odczytu]
            title += QLatin1Char(' ') + tr("[read-only]");
        }
        setWindowTitle(title + QStringLiteral(" - Kalahari"));
    } else {
        setWindowTitle(QStringLiteral("Kalahari"));
    }
}

void MainWindow::toggleFullScreen() {
    auto& logger = core::Logger::getInstance();

    // Distraction-Free fills the screen, so leaving the full screen leaves it too
    const bool leave = isFullScreen() || isDistractionFree();
    if (isDistractionFree()) {
        m_distractionFreeLayout->setActive(false);
    }

    if (leave) {
        // Exit fullscreen (still on if it was on before Distraction-Free)
        if (isFullScreen()) {
            logger.debug("MainWindow: Exiting fullscreen mode");
            showNormal();
            if (!m_savedGeometryBeforeFullscreen.isEmpty()) {
                restoreGeometry(m_savedGeometryBeforeFullscreen);
            }
        }
    } else {
        // Enter fullscreen
        logger.debug("MainWindow: Entering fullscreen mode");
        m_savedGeometryBeforeFullscreen = saveGeometry();
        showFullScreen();
    }

    // OpenSpec #00040: Update action checked state after fullscreen toggle
    // This synchronizes the QAction's checked state with actual window state
    commands().updateActionState("view.fullScreen");
}

EditorPanel* MainWindow::getCurrentEditor() {
    // Task #00015: Get active EditorPanel tab (or nullptr)
    QTabWidget* centralTabs = m_dockCoordinator->centralTabs();
    if (!centralTabs) {
        return nullptr;
    }

    // The page the keys went to last, also in a window of its own
    QWidget* currentWidget =
        m_documentWindows ? m_documentWindows->frontPage() : centralTabs->currentWidget();
    return qobject_cast<EditorPanel*>(currentWidget);  // nullptr if not EditorPanel
}

ImagePreviewPanel* MainWindow::currentPreview() {
    QTabWidget* centralTabs = m_dockCoordinator->centralTabs();
    return centralTabs != nullptr ? qobject_cast<ImagePreviewPanel*>(centralTabs->currentWidget())
                                  : nullptr;
}

// =============================================================================
// Project Manager Slots - MOVED to DocumentCoordinator (OpenSpec #00038 Phase 7)
// onProjectOpened, onProjectClosed are now in document_coordinator.cpp
// =============================================================================

void MainWindow::onNavigatorElementSelected(const QString& elementId, const QString& elementTitle) {
    // OpenSpec #00038 Phase 6: Delegate to NavigatorCoordinator
    if (m_navigatorCoordinator) {
        m_navigatorCoordinator->onElementSelected(elementId, elementTitle);
    }
    // The chapter opened, or brought to the front, gets the keys
    const EditorPanel* editor = getCurrentEditor();
    if (editor != nullptr && editor->property("elementId").toString() == elementId) {
        focusCurrentText();
    }
}

void MainWindow::openGivingKeys(const std::function<void()>& open) {
    const DocumentWindows& texts = DocumentWindows::of(*m_dockCoordinator->centralTabs());
    const QWidget* front = texts.frontPage();
    const qsizetype count = texts.pages().size();
    open();
    // A tab more, or another one in front: a document was opened (also after the window that
    // chose it, as the window for choosing a file), or brought to the front in its window
    if (texts.frontPage() != front || texts.pages().size() != count) {
        focusCurrentText();
    }
}

void MainWindow::focusCurrentText() {
    // In the window behind a window of the program, the text gets the keys when it comes back
    if (EditorPanel* editor = getCurrentEditor(); editor != nullptr && editor->getBookEditor()) {
        editor->getBookEditor()->setFocus(Qt::OtherFocusReason);
    }
}

void MainWindow::placeOpenedChapterCursor(const QString& elementId) {
    const auto opensAt = core::SettingsManager::getInstance().get<std::string>(
        utils::CHAPTER_OPENS_AT_KEY, utils::CHAPTER_OPENS_WHERE_LEFT);
    const core::ProjectDatabase* database = book().getDatabase();
    if (opensAt == utils::CHAPTER_OPENS_AT_START || database == nullptr || !database->isOpen()) {
        return;  // at its start, as a new editor has it
    }
    for (QWidget* page : DocumentWindows::of(*m_dockCoordinator->centralTabs()).pages()) {
        auto* editor = qobject_cast<EditorPanel*>(page);
        if (editor != nullptr && editor->getBookEditor() != nullptr
            && editor->property("elementId").toString() == elementId) {
            utils::restoreChapterCursor(*database, elementId, *editor->getBookEditor());
            return;
        }
    }
}

void MainWindow::rememberChapterCursor(const EditorPanel* editor) {
    core::ProjectDatabase* database = book().getDatabase();
    if (editor == nullptr || editor->getBookEditor() == nullptr || database == nullptr
        || !database->isOpen()) {
        return;
    }
    // A chapter of the book, not a file outside it
    const QString elementId = editor->property("elementId").toString();
    if (!elementId.isEmpty()) {
        utils::rememberChapterCursor(*database, elementId, *editor->getBookEditor());
    }
}

void MainWindow::rememberChapterCursors() {
    core::ProjectDatabase* database = book().getDatabase();
    QTabWidget* centralTabs = m_dockCoordinator ? m_dockCoordinator->centralTabs() : nullptr;
    if (database == nullptr || !database->isOpen() || centralTabs == nullptr) {
        return;
    }
    // One write to the book's database for all of them
    database->executeInTransaction([this, centralTabs]() {
        for (QWidget* page : DocumentWindows::of(*centralTabs).pages()) {
            rememberChapterCursor(qobject_cast<const EditorPanel*>(page));
        }
        return true;
    });
}

void MainWindow::openChapter(const QString& elementId, const QString& elementTitle) {
    // OpenSpec #00043: Public wrapper for CLI/benchmark access
    onNavigatorElementSelected(elementId, elementTitle);
}

// =============================================================================
// Diagnostic/Dev Mode (OpenSpec #00038 - Delegated to DiagnosticController)
// =============================================================================

void MainWindow::enableDiagnosticMode() {
    if (m_diagnosticController) {
        m_diagnosticController->enableDiagnosticMode();
    }
}

void MainWindow::disableDiagnosticMode() {
    if (m_diagnosticController) {
        m_diagnosticController->disableDiagnosticMode();
    }
}

bool MainWindow::isDiagnosticMode() const {
    return m_diagnosticController ? m_diagnosticController->isDiagnosticMode() : false;
}

void MainWindow::enableDevMode() {
    if (m_diagnosticController) {
        m_diagnosticController->enableDevMode();
    }
}

void MainWindow::disableDevMode() {
    if (m_diagnosticController) {
        m_diagnosticController->disableDevMode();
    }
}

bool MainWindow::isDevMode() const {
    return m_diagnosticController ? m_diagnosticController->isDevMode() : false;
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
    m_dockCoordinator->refreshDockIcons();
}

void MainWindow::showEvent(QShowEvent* event) {
    QMainWindow::showEvent(event);

    if (m_firstShow) {
        restoreLayout();
        openAtStart();
        m_firstShow = false;
    }
}

void MainWindow::restoreLayout() {
    auto& logger = core::Logger::getInstance();
    logger.debug("Restoring window perspective");

    // OpenSpec #00037: Check if toolbar config needs reset BEFORE restoring state
    // If reset needed, clear saved window state to prevent old toolbar layout from being restored
    bool toolbarResetNeeded = ToolbarManager::needsConfigReset();
    if (toolbarResetNeeded) {
        logger.info("Toolbar config reset needed, clearing saved window state");
        ToolbarManager::clearSavedWindowState();
    }

    // A new window takes the layout of the window it came from, a little lower and to the
    // right; the first one the saved layout
    auto& settings = core::SettingsManager::getInstance();
    restoreGeometry(m_layoutFrom ? m_layoutFrom->geometry : settings.getBinary("window.geometry"));
    if (m_layoutFrom && !isMaximized() && !isFullScreen()) {
        move(pos() + QPoint(NEW_WINDOW_OFFSET, NEW_WINDOW_OFFSET));
    }
    // A size saved on a larger screen, or at a smaller scaling, would leave the window
    // bigger than this screen
    if (!isMaximized() && !isFullScreen()) {
        utils::resizeWithinScreen(this, size());
    }

    // Only restore window state if we haven't cleared it for toolbar reset
    // IMPORTANT: Read windowState AFTER clearSavedWindowState() to get fresh value
    QByteArray windowState = m_layoutFrom ? m_layoutFrom->state : settings.getBinary("window.state");
    const bool savedLayout = !windowState.isEmpty() && !toolbarResetNeeded;
    if (savedLayout) {
        // Normal case: restore saved window state (includes toolbar positions)
        restoreState(windowState);
    } else if (const QScreen* screen = this->screen()) {
        // No saved layout (the first start, or the toolbars were reset): a small screen
        // gets a layout that leaves the text room
        m_dockCoordinator->fitFirstLayout(screen->availableGeometry());
    }
    // A layout saved before the Statistics panel
    m_dockCoordinator->placeStatisticsPanel(savedLayout);

    // Task #00019: Restore toolbar state (visibility)
    // This runs AFTER QMainWindow::restoreState() to ensure our visibility settings
    // take precedence over Qt's saved state
    if (m_toolbarManager) {
        m_toolbarManager->restoreState();
    }

    // A layout saved before the Paragraph and Styles panel, or with it in its other place
    m_dockCoordinator->placeParagraphPanel(false);

    // OpenSpec #00024: LogDock visibility is INDEPENDENT of saveState/restoreState
    // It depends ONLY on diagnostic mode, not on saved window state
    // Use setVisible() instead of show()/hide() for more reliable behavior
    QDockWidget* logDock = m_dockCoordinator->logDock();
    if (logDock) {
        bool shouldShow = isDiagnosticMode() || isDevMode();
        logDock->setVisible(shouldShow);
        logger.info("LogDock: {} (diagnosticMode={}, devMode={})",
                   shouldShow ? "shown" : "hidden", isDiagnosticMode(), isDevMode());
    }

    logger.debug("Window perspective restored");
}

void MainWindow::openAtStart() {
    if (!m_firstWindow) {
        return;
    }
    auto& logger = core::Logger::getInstance();

    // OpenSpec #00036: Auto-load last project if enabled
    // OpenSpec #00038 Phase 7: Use DocumentCoordinator for file operations
    auto& settingsManager = core::SettingsManager::getInstance();
    bool autoLoad = settingsManager.get<bool>("startup.autoLoadLastProject", false);
    if (!m_startupFile.isEmpty()) {
        // The file of the command line takes the place of the last book
        logger.info("Opening the file of the command line: {}", m_startupFile.toStdString());
        QTimer::singleShot(100, this, [this, file = m_startupFile]() {
            if (m_documentCoordinator) m_documentCoordinator->onOpenRecentFile(file);
        });
    } else if (autoLoad) {
        // The books of the last work, or the book opened last (not one created or imported
        // without opening since): the first one here, each other one in a window of its own,
        // so that the one in front last comes to the front
        const QStringList books = core::RecentBooksManager::getInstance().booksToOpenAtStart();
        if (!books.isEmpty()) {
            logger.info("Auto-loading the books of the last work: {}", books.size());
            // Use QTimer::singleShot to defer opening after window is fully shown
            QTimer::singleShot(100, this, [this, books]() {
                if (m_documentCoordinator == nullptr) {
                    return;
                }
                m_documentCoordinator->onOpenRecentFile(books.first());
                for (qsizetype i = 1; i < books.size(); ++i) {
                    openBookInNewWindow(books.at(i));
                }
            });
        }
    }
}

// =============================================================================
// The book windows of the program
// =============================================================================

QList<MainWindow*> MainWindow::windows() {
    return bookWindowList();
}

MainWindow* MainWindow::windowOf(const QWidget* widget) {
    const QList<MainWindow*>& windows = bookWindowList();
    for (const QWidget* each = widget; each != nullptr; each = each->parentWidget()) {
        for (MainWindow* window : windows) {
            if (window == each) {
                return window;
            }
        }
    }
    return nullptr;
}

MainWindow* MainWindow::windowOfBook(const QString& manifest) {
    const QFileInfo wanted(manifest);
    for (MainWindow* window : std::as_const(bookWindowList())) {
        const core::ProjectManager& book = window->book();
        if (book.isProjectOpen() && QFileInfo(book.getManifestPath()) == wanted) {
            return window;
        }
    }
    return nullptr;
}

MainWindow* MainWindow::frontWindow() {
    if (MainWindow* active = windowOf(QApplication::activeWindow())) {
        return active;
    }
    if (MainWindow* last = lastFrontWindow().data(); bookWindowList().contains(last)) {
        return last;
    }
    return bookWindowList().isEmpty() ? nullptr : bookWindowList().last();
}

MainWindow* MainWindow::openNewWindow() {
    auto& logger = core::Logger::getInstance();
    logger.info("Action triggered: New Window");

    auto* window = new MainWindow;
    window->setAttribute(Qt::WA_DeleteOnClose);
    // The modes of this window
    if (isDiagnosticMode()) {
        window->enableDiagnosticMode();
    }
    if (isDevMode()) {
        window->enableDevMode();
    }
    // The panels and the bars of this window, as they are outside Distraction-Free and full
    // screen; in them, the saved ones
    if (!isDistractionFree() && !isFullScreen()) {
        window->m_layoutFrom = Layout{saveGeometry(), saveState()};
    }
    window->show();
    window->raise();
    window->activateWindow();
    lastFrontWindow() = window;
    return window;
}

MainWindow* MainWindow::openBookInNewWindow(const QString& manifest) {
    MainWindow* window = openNewWindow();
    // Once it is shown, as from the recent books
    QTimer::singleShot(0, window, [window, manifest]() {
        if (window->m_documentCoordinator != nullptr) {
            window->m_documentCoordinator->onOpenRecentFile(manifest);
        }
    });
    return window;
}

bool MainWindow::closeAllWindows(MainWindow* asking) {
    auto& logger = core::Logger::getInstance();
    const QList<MainWindow*> windows = bookWindowList();

    // The annotations and the notes being written are kept, then one question names what
    // has unsaved changes in every window
    QStringList names;
    for (MainWindow* window : windows) {
        window->finishWriting();
        if (window->hasUnsavedChanges() && window->m_documentCoordinator != nullptr) {
            names << window->m_documentCoordinator->unsavedDocumentNames();
        }
    }
    names.removeDuplicates();
    if (!names.isEmpty()) {
        const auto reply = dialogs::MessageDialog::ask(
            asking, tr("Unsaved Changes"),
            tr("Do you want to save changes to %1?").arg(names.join(QStringLiteral(", "))),
            tr("&Save"), tr("Do&n't Save"));
        if (reply == dialogs::MessageDialog::Answer::Cancel) {
            return false;
        }
        if (reply == dialogs::MessageDialog::Answer::Accept) {
            // Whatever could not be saved (failed or cancelled) keeps every window open, the
            // window with it in front
            for (MainWindow* window : windows) {
                if (window->hasUnsavedChanges() &&
                    (window->m_documentCoordinator == nullptr ||
                     !window->m_documentCoordinator->saveAllChanges())) {
                    DocumentWindows::showWindow(window);
                    return false;
                }
            }
        }
    }

    // The books open now are the books of the next start, the one in front last
    QList<MainWindow*> order = windows;
    if (order.removeOne(asking)) {
        order.append(asking);
    }
    QStringList books;
    for (MainWindow* window : std::as_const(order)) {
        if (window->book().isProjectOpen()) {
            books << window->book().getManifestPath();
        }
    }
    core::RecentBooksManager::getInstance().setOpenBooks(books);
    logger.info("Closing every window of the program: {} books open", books.size());

    // The window that asked closes last, so its layout is the one kept
    closingAllWindows() = true;
    bool closed = true;
    for (MainWindow* window : std::as_const(order)) {
        closed = window->close() && closed;
    }
    closingAllWindows() = false;
    return closed;
}

void MainWindow::finishWriting() {
    if (m_annotationsCoordinator) {
        m_annotationsCoordinator->finishWriting();
    }
    if (m_footnoteCoordinator) {
        m_footnoteCoordinator->finishWriting();
    }
}

// =============================================================================
// Settings Management - NOTE: Moved to SettingsCoordinator (OpenSpec #00038 Phase 5)
// collectCurrentSettings() and onApplySettings() are now in settings_coordinator.cpp
// =============================================================================

// =============================================================================
// Standalone File Support - MOVED to DocumentCoordinator (OpenSpec #00038 Phase 7)
// onOpenStandaloneFile, openStandaloneFile, onAddToProject,
// onExportArchive, onImportArchive are now in document_coordinator.cpp
// =============================================================================

// =============================================================================
// Navigator Context Menu Handlers - Moved to NavigatorCoordinator (OpenSpec #00038 Phase 6)
// =============================================================================

// =============================================================================
// Editor Settings Application (OpenSpec #00042)
// =============================================================================

void MainWindow::updatePageStatus() {
    if (!m_pageLabel || !m_zoomLabel) {
        return;
    }

    // The zoom of the preview of a material
    if (ImagePreviewPanel* preview = currentPreview()) {
        m_pageLabel->hide();
        m_zoomLabel->setText(tr("%1%").arg(qRound(preview->zoom() * 100.0)));
        m_zoomLabel->show();
        return;
    }

    // "Page X of Y" in the Page Layout view; the zoom of any editor
    EditorPanel* editor = getCurrentEditor();
    editor::BookEditor* bookEditor = editor ? editor->getBookEditor() : nullptr;
    if (!bookEditor) {
        m_pageLabel->hide();
        m_zoomLabel->hide();
        return;
    }
    const int total = bookEditor->totalPages();
    m_pageLabel->setVisible(total > 0);
    if (total > 0) {
        m_pageLabel->setText(tr("Page %1 of %2").arg(bookEditor->currentPage()).arg(total));
    }
    m_zoomLabel->setText(tr("%1%").arg(qRound(bookEditor->zoomFactor() * 100.0)));
    m_zoomLabel->show();
}

} // namespace gui
} // namespace kalahari
