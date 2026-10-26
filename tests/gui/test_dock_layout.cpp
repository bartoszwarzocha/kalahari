/// @file test_dock_layout.cpp
/// @brief The panels of the main window: the Annotations panel is on top of the panels on the
/// right by default, choosing an element in the Navigator does not cover it, the Paragraph and
/// Styles panel is the tab after Properties or the lower part of it, the button of a panel
/// shows it, brings it on top and closes it, the Spelling and Grammar panel opens as the tab
/// after Annotations, the gears of the panels ask for their settings, on
/// a small screen the panels on the right make room for the text, the tabs of the panels show
/// their icons above the titles, and Reset Layout docks the toolbars again

#include <catch2/catch_test_macros.hpp>
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/find_replace_bar.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dock_coordinator.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/navigator_coordinator.h"
#include "kalahari/gui/panel_tabs.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/panels/paragraph_panel.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/toolbar_manager.h"
#include "kalahari/gui/widgets/info_bar.h"
#include "kalahari/gui/widgets/panel_tab_bar.h"
#include "kalahari/gui/widgets/standalone_info_bar.h"

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QDockWidget>
#include <QKeySequence>
#include <QLabel>
#include <QLayout>
#include <QMainWindow>
#include <QMenuBar>
#include <QMetaObject>
#include <QPushButton>
#include <QRect>
#include <QScreen>
#include <QStatusBar>
#include <QStyle>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

using namespace kalahari;
using namespace kalahari::gui;

namespace {

/// The part of a small screen for windows: 1366×768 at 150% less the taskbar, 911×480
const QRect SMALL_SCREEN(0, 0, 911, 480);

/// The part of a large screen for windows: 1920×1080 less the taskbar
const QRect LARGE_SCREEN(0, 0, 1920, 1040);

/// The title bar and the borders of a window on a small screen, with a margin
constexpr int WINDOW_FRAME_HEIGHT = 46;

/// The setting that the bar about a small screen was shown
constexpr const char* NOTICE_SHOWN = "window.smallScreenNoticeShown";

/// Title of the tab on top of the tab group holding @p dock (empty without a group)
QString raisedTab(const QMainWindow& window, const QDockWidget* dock) {
    for (const QTabBar* tabBar : window.findChildren<QTabBar*>()) {
        for (int i = 0; i < tabBar->count(); ++i) {
            if (tabBar->tabText(i) == dock->windowTitle()) {
                return tabBar->tabText(tabBar->currentIndex());
            }
        }
    }
    return {};
}

/// The place of @p dock's tab in its tab group (-1 without a group)
int tabIndex(const QMainWindow& window, const QDockWidget* dock) {
    for (const QTabBar* tabBar : window.findChildren<QTabBar*>()) {
        for (int i = 0; i < tabBar->count(); ++i) {
            if (tabBar->tabText(i) == dock->windowTitle()) {
                return i;
            }
        }
    }
    return -1;
}

/// The icons of the buttons on the title bar of @p dock, in their order
std::vector<std::string> titleBarButtons(const QDockWidget* dock) {
    std::vector<std::string> icons;
    const auto* row =
        dock->titleBarWidget()->findChild<QWidget*>(QString::fromLatin1(DockCoordinator::TITLE_ROW));
    REQUIRE(row != nullptr);
    const QLayout* layout = row->layout();
    for (int i = 0; i < layout->count(); ++i) {
        if (const auto* button = qobject_cast<const QToolButton*>(layout->itemAt(i)->widget())) {
            icons.push_back(button->property("iconId").toString().toStdString());
        }
    }
    return icons;
}

/// The button with the icon @p iconId on the title bar of @p dock
QToolButton* titleBarButton(const QDockWidget* dock, const QString& iconId) {
    for (QToolButton* button : dock->titleBarWidget()->findChildren<QToolButton*>()) {
        if (button->property("iconId").toString() == iconId) {
            return button;
        }
    }
    return nullptr;
}

/// A book open for the test; closed again when the test ends
struct OpenBook {
    QTemporaryDir dir;

    OpenBook() {
        REQUIRE(dir.isValid());
        REQUIRE(core::ProjectManager::getInstance().createProject(dir.path(), "Panels",
                                                                  "Author", "en", true));
    }
    ~OpenBook() { core::ProjectManager::getInstance().closeProject(false); }

    OpenBook(const OpenBook&) = delete;
    OpenBook& operator=(const OpenBook&) = delete;
};

/// The panels of the main window, with the Navigator's requests handled as in MainWindow
struct Panels {
    // Not the window's own: Qt 6.9.1 crashes destroying a main window that has a status bar but
    // no toolbar once its panels were laid out again (QTBUG-137524). MainWindow has toolbars.
    QStatusBar statusBar;
    QMainWindow window;
    DockCoordinator* docks = nullptr;  // owned by the window, as in MainWindow

    Panels() {
        registerAllCommands(CommandCallbacks{});
        docks = new DockCoordinator(&window, &window);
        docks->createDocks();
        auto* navigator =
            new NavigatorCoordinator(docks->navigatorPanel(), docks->propertiesPanel(),
                                     docks->centralTabs(), &statusBar, &window);
        QObject::connect(docks, &DockCoordinator::navigatorRequestProperties, navigator,
                         &NavigatorCoordinator::onRequestProperties);
        QObject::connect(docks, &DockCoordinator::navigatorRequestSectionProperties, navigator,
                         &NavigatorCoordinator::onRequestSectionProperties);
        QObject::connect(docks, &DockCoordinator::navigatorRequestPartProperties, navigator,
                         &NavigatorCoordinator::onRequestPartProperties);
        window.resize(1200, 800);
        window.show();
        QApplication::processEvents();
    }

    /// Whether @p dock is the tab on top of its tab group
    [[nodiscard]] bool onTop(const QDockWidget* dock) const {
        return raisedTab(window, dock) == dock->windowTitle();
    }

    /// The panels on the right of the window
    [[nodiscard]] QList<QDockWidget*> rightDocks() const {
        return {docks->propertiesDock(), docks->paragraphDock(), docks->searchDock(),
                docks->assistantDock(), docks->annotationsDock()};
    }

    /// Whether the panels on the right are all open (in their tabs) or all closed
    [[nodiscard]] bool rightDocksOpen(bool open) const {
        for (const QDockWidget* dock : rightDocks()) {
            if (dock->toggleViewAction()->isChecked() != open) {
                return false;
            }
        }
        return true;
    }
};

}  // namespace

TEST_CASE("Panels: the Annotations panel is on top of the right panels by default",
          "[gui][panels]") {
    Panels panels;
    CHECK(panels.onTop(panels.docks->annotationsDock()));

    SECTION("Reset Layout puts it on top again") {
        panels.docks->propertiesDock()->raise();
        QApplication::processEvents();
        REQUIRE(panels.onTop(panels.docks->propertiesDock()));

        panels.docks->resetLayout(false, false);
        QApplication::processEvents();
        const QScreen* screen = panels.window.screen();
        REQUIRE(screen != nullptr);
        if (screen->availableGeometry().width() < DockCoordinator::SMALL_SCREEN_WIDTH) {
            // A small screen (the offscreen screen of the tests) has them hidden by default,
            // and the bar above the text brings them back
            CHECK(panels.rightDocksOpen(false));
            CHECK(panels.docks->layoutNoticeBar()->isVisible());
            panels.docks->showRightPanels();
            QApplication::processEvents();
        } else {
            CHECK_FALSE(panels.docks->layoutNoticeBar()->isVisible());
        }
        CHECK(panels.onTop(panels.docks->annotationsDock()));
    }
}

TEST_CASE("Panels: the Spelling and Grammar panel opens as the tab after the Annotations tab",
          "[gui][panels][spelling-panel]") {
    // The layout of a window saved before the panel existed, which does not know the panel
    QByteArray oldLayout;
    {
        Panels old;
        old.window.removeDockWidget(old.docks->spellingDock());
        oldLayout = old.window.saveState();
    }
    Panels panels;
    QDockWidget* spelling = panels.docks->spellingDock();
    QDockWidget* annotations = panels.docks->annotationsDock();

    SECTION("in the default layout") {
    }
    SECTION("in a layout saved before the panel existed") {
        REQUIRE(panels.window.restoreState(oldLayout));
        QApplication::processEvents();
    }

    // Closed until opened
    CHECK(spelling->isHidden());
    CHECK(panels.onTop(annotations));

    // View > Panels > Spelling and Grammar
    CommandRegistry::getInstance().executeCommand("view.spelling");
    QApplication::processEvents();
    CHECK(spelling->isVisible());
    CHECK(panels.onTop(spelling));
    REQUIRE(tabIndex(panels.window, annotations) >= 0);
    CHECK(tabIndex(panels.window, spelling) == tabIndex(panels.window, annotations) + 1);
}

TEST_CASE("Panels: the Paragraph and Styles panel is the tab after Properties",
          "[gui][panels][styles]") {
    Panels panels;
    QDockWidget* paragraph = panels.docks->paragraphDock();
    QDockWidget* properties = panels.docks->propertiesDock();
    REQUIRE(paragraph != nullptr);

    // Open, under the Annotations panel on top
    CHECK(paragraph->toggleViewAction()->isChecked());
    CHECK(panels.docks->viewParagraphAction()->isChecked());
    CHECK(panels.onTop(panels.docks->annotationsDock()));
    REQUIRE(tabIndex(panels.window, properties) >= 0);
    CHECK(tabIndex(panels.window, paragraph) == tabIndex(panels.window, properties) + 1);

    SECTION("Reset Layout puts it there again") {
        panels.window.addDockWidget(Qt::LeftDockWidgetArea, paragraph);
        panels.docks->resetLayout(false, false);
        panels.docks->showRightPanels();  // hidden by Reset Layout on a small screen
        QApplication::processEvents();
        CHECK(paragraph->toggleViewAction()->isChecked());
        CHECK(panels.window.dockWidgetArea(paragraph) == Qt::RightDockWidgetArea);
        CHECK(tabIndex(panels.window, paragraph) == tabIndex(panels.window, properties) + 1);
    }
}

TEST_CASE("Panels: a layout saved before the Paragraph and Styles panel shows it with the right "
          "panels",
          "[gui][panels][styles]") {
    QByteArray oldLayout;
    {
        Panels old;
        old.window.removeDockWidget(old.docks->paragraphDock());
        oldLayout = old.window.saveState();
    }
    Panels panels;
    REQUIRE(panels.window.restoreState(oldLayout));
    panels.docks->placeParagraphPanel(false);
    QApplication::processEvents();

    QDockWidget* paragraph = panels.docks->paragraphDock();
    CHECK(paragraph->toggleViewAction()->isChecked());
    CHECK(panels.window.dockWidgetArea(paragraph) == Qt::RightDockWidgetArea);
    CHECK(tabIndex(panels.window, paragraph) >= 0);
    CHECK(panels.onTop(panels.docks->annotationsDock()));

    // Its button brings it on top
    CommandRegistry::getInstance().executeCommand("view.paragraph");
    QApplication::processEvents();
    CHECK(panels.onTop(paragraph));
}

TEST_CASE("Panels: the button of a panel shows it, brings it on top, closes it",
          "[gui][panels][styles]") {
    Panels panels;
    CommandRegistry& registry = CommandRegistry::getInstance();
    QDockWidget* paragraph = panels.docks->paragraphDock();
    QAction* button = panels.docks->viewParagraphAction();
    const auto click = [&registry]() {
        registry.executeCommand("view.paragraph");
        QApplication::processEvents();
    };
    REQUIRE(panels.onTop(panels.docks->annotationsDock()));
    REQUIRE(button->isChecked());

    // Open under another tab: on top, the button still checked
    click();
    CHECK(panels.onTop(paragraph));
    CHECK(button->isChecked());

    // On top: closed, the button unchecked
    click();
    CHECK(paragraph->isHidden());
    CHECK_FALSE(button->isChecked());

    // Closed: open and on top
    click();
    CHECK(paragraph->isVisible());
    CHECK(panels.onTop(paragraph));
    CHECK(button->isChecked());
}

TEST_CASE("Panels: Paragraph and Styles at the bottom of the Properties panel",
          "[gui][panels][styles]") {
    auto& settings = core::SettingsManager::getInstance();
    struct PlaceBack {
        ~PlaceBack() {
            try {
                auto& settings = core::SettingsManager::getInstance();
                settings.set<std::string>(DockCoordinator::PARAGRAPH_PLACE_KEY,
                                          DockCoordinator::PARAGRAPH_IN_PANEL);
                settings.set<bool>("panels.paragraphFolded", false);
            } catch (...) {
                std::fputs("The place of the Paragraph and Styles panel could not be given "
                           "back\n",
                           stderr);
            }
        }
    } placeBack;
    Panels panels;
    CommandRegistry& registry = CommandRegistry::getInstance();
    QDockWidget* properties = panels.docks->propertiesDock();
    ParagraphPanel* lower = panels.docks->propertiesParagraphPanel();
    QAction* button = panels.docks->viewParagraphAction();
    const auto click = [&registry]() {
        registry.executeCommand("view.paragraph");
        QApplication::processEvents();
    };

    // Settings > Appearance > General: the panel goes there and shows in front
    settings.set<std::string>(DockCoordinator::PARAGRAPH_PLACE_KEY,
                              DockCoordinator::PARAGRAPH_IN_PROPERTIES);
    panels.docks->placeParagraphPanel(true);
    QApplication::processEvents();
    CHECK(panels.docks->paragraphDock()->isHidden());
    CHECK(lower->isVisible());
    CHECK_FALSE(lower->isFolded());
    CHECK(panels.onTop(properties));
    CHECK(button->isChecked());

    // The button folds the lower part to its band, and unfolds it
    click();
    CHECK(lower->isFolded());
    CHECK_FALSE(button->isChecked());
    CHECK(settings.get<bool>("panels.paragraphFolded", false));
    CHECK(panels.onTop(properties));
    click();
    CHECK_FALSE(lower->isFolded());
    CHECK(button->isChecked());

    // Under another tab: Properties on top, unfolded
    panels.docks->annotationsDock()->raise();
    QApplication::processEvents();
    REQUIRE_FALSE(panels.onTop(properties));
    click();
    CHECK(panels.onTop(properties));
    CHECK_FALSE(lower->isFolded());

    // Its own band folds it as well
    lower->setFolded(true);
    CHECK_FALSE(button->isChecked());

    // Back in a panel of its own: shown on top, the lower part gone
    settings.set<std::string>(DockCoordinator::PARAGRAPH_PLACE_KEY,
                              DockCoordinator::PARAGRAPH_IN_PANEL);
    panels.docks->placeParagraphPanel(true);
    QApplication::processEvents();
    CHECK_FALSE(lower->isVisible());
    CHECK(panels.onTop(panels.docks->paragraphDock()));
    CHECK(button->isChecked());
}

TEST_CASE("Panels: the Paragraph and Styles panel follows the text in front",
          "[gui][panels][styles]") {
    Panels panels;
    auto* chapter = new EditorPanel();
    panels.docks->centralTabs()->addTab(chapter, QStringLiteral("Chapter 1"));
    panels.docks->setParagraphEditor(chapter->getBookEditor());
    CHECK(panels.docks->paragraphPanel()->editor() == chapter->getBookEditor());
    CHECK(panels.docks->propertiesParagraphPanel()->editor() == chapter->getBookEditor());
    panels.docks->setParagraphEditor(nullptr);
    CHECK(panels.docks->paragraphPanel()->editor() == nullptr);
    CHECK(panels.docks->propertiesParagraphPanel()->editor() == nullptr);
}

TEST_CASE("Panels: Reset Layout closes the Spelling and Grammar panel",
          "[gui][panels][spelling-panel]") {
    Panels panels;
    QDockWidget* spelling = panels.docks->spellingDock();
    CommandRegistry::getInstance().executeCommand("view.spelling");
    QApplication::processEvents();
    REQUIRE(panels.onTop(spelling));

    panels.docks->resetLayout(false, false);
    panels.docks->showRightPanels();  // hidden by Reset Layout on a small screen
    QApplication::processEvents();
    CHECK(spelling->isHidden());
    CHECK(panels.onTop(panels.docks->annotationsDock()));

    CommandRegistry::getInstance().executeCommand("view.spelling");
    QApplication::processEvents();
    CHECK(panels.onTop(spelling));
    CHECK(tabIndex(panels.window, spelling) ==
          tabIndex(panels.window, panels.docks->annotationsDock()) + 1);
}

TEST_CASE("Panels: the gear on the title bar of a panel asks for the panel's settings",
          "[gui][panels][spelling-panel]") {
    Panels panels;
    QString asked;
    QObject::connect(panels.docks, &DockCoordinator::settingsPageRequested,
                     [&asked](const QString& page) { asked = page; });

    SECTION("The Spelling and Grammar panel: the gear, then Float and Close") {
        QDockWidget* spelling = panels.docks->spellingDock();
        CHECK(titleBarButtons(spelling) ==
              std::vector<std::string>{"dock.settings", "dock.float", "dock.close"});
        QToolButton* gear = titleBarButton(spelling, QStringLiteral("dock.settings"));
        REQUIRE(gear != nullptr);
        CHECK(gear->toolTip() == QStringLiteral("Spelling and Grammar Settings"));
        gear->click();
        CHECK(asked == QString::fromLatin1(SettingsDialog::SPELLING_PAGE));
    }

    SECTION("The Annotations panel: the gear, then Float and Close") {
        QDockWidget* annotations = panels.docks->annotationsDock();
        CHECK(titleBarButtons(annotations) ==
              std::vector<std::string>{"dock.settings", "dock.float", "dock.close"});
        QToolButton* gear = titleBarButton(annotations, QStringLiteral("dock.settings"));
        REQUIRE(gear != nullptr);
        CHECK(gear->toolTip() == QStringLiteral("Annotation Settings"));
        gear->click();
        CHECK(asked == QString::fromLatin1(SettingsDialog::ANNOTATIONS_PAGE));
    }
}

TEST_CASE("Panels: a small screen leaves the text room", "[gui][panels]") {
    Panels panels;
    QDockWidget* navigator = panels.docks->navigatorDock();
    REQUIRE(panels.rightDocksOpen(true));

    SECTION("A large screen keeps every panel") {
        CHECK_FALSE(panels.docks->fitToScreen(LARGE_SCREEN));
        QApplication::processEvents();
        CHECK(panels.rightDocksOpen(true));
    }

    SECTION("A small screen hides the panels on the right and narrows the Navigator") {
        REQUIRE(panels.docks->fitToScreen(SMALL_SCREEN));
        QApplication::processEvents();
        CHECK(panels.rightDocksOpen(false));
        CHECK(navigator->isVisible());
        // About a fifth of the screen, as far as the Navigator's minimum allows
        CAPTURE(navigator->width(), navigator->minimumSizeHint().width());
        CHECK(navigator->width() <=
              qMax(SMALL_SCREEN.width() / 4, navigator->minimumSizeHint().width()));
        CHECK(navigator->width() >= navigator->minimumSizeHint().width());
        // The bar is for the first start and for Reset Layout
        CHECK_FALSE(panels.docks->layoutNoticeBar()->isVisible());
    }
}

TEST_CASE("Panels: the bar about a small screen says how to show the panels again",
          "[gui][panels]") {
    Panels panels;
    REQUIRE(panels.docks->fitToScreen(SMALL_SCREEN));
    panels.docks->showLayoutNotice();
    QApplication::processEvents();
    InfoBar* bar = panels.docks->layoutNoticeBar();
    REQUIRE(bar->isVisible());
    CHECK(bar->message().contains(QStringLiteral("View > Panels")));

    SECTION("Its button shows the panels on the right, with the Annotations panel on top") {
        auto* button = bar->findChild<QPushButton*>();
        REQUIRE(button != nullptr);
        REQUIRE(button->isVisible());
        button->click();
        QApplication::processEvents();
        CHECK(panels.rightDocksOpen(true));
        CHECK(panels.onTop(panels.docks->annotationsDock()));
        CHECK_FALSE(bar->isVisible());
    }

    SECTION("A panel shown from the View menu ends it") {
        panels.docks->searchDock()->toggleViewAction()->trigger();
        QApplication::processEvents();
        CHECK(panels.docks->searchDock()->isVisible());
        CHECK_FALSE(bar->isVisible());
    }

    SECTION("Its close button ends it, and the panels stay hidden") {
        auto* close = bar->findChild<QToolButton*>();
        REQUIRE(close != nullptr);
        close->click();
        QApplication::processEvents();
        CHECK_FALSE(bar->isVisible());
        CHECK(panels.rightDocksOpen(false));
    }
}

TEST_CASE("Panels: the first layout on a small screen says why once", "[gui][panels]") {
    auto& settings = core::SettingsManager::getInstance();
    settings.set<bool>(NOTICE_SHOWN, false);  // a first start, whatever tests ran before

    SECTION("A large screen keeps every panel and says nothing") {
        Panels panels;
        panels.docks->fitFirstLayout(LARGE_SCREEN);
        QApplication::processEvents();
        CHECK(panels.rightDocksOpen(true));
        CHECK_FALSE(panels.docks->layoutNoticeBar()->isVisible());
        CHECK_FALSE(settings.get<bool>(NOTICE_SHOWN, false));
    }

    SECTION("A small screen: the bar the first time, not on the next start") {
        {
            Panels first;
            first.docks->fitFirstLayout(SMALL_SCREEN);
            QApplication::processEvents();
            CHECK(first.rightDocksOpen(false));
            CHECK(first.docks->layoutNoticeBar()->isVisible());
            CHECK(settings.get<bool>(NOTICE_SHOWN, false));
        }
        Panels next;  // a start without a saved layout again
        next.docks->fitFirstLayout(SMALL_SCREEN);
        QApplication::processEvents();
        CHECK(next.rightDocksOpen(false));
        CHECK_FALSE(next.docks->layoutNoticeBar()->isVisible());
    }

    settings.set<bool>(NOTICE_SHOWN, false);  // the default for the tests that follow
}

TEST_CASE("Panels: every panel and the text fit a small screen side by side", "[gui][panels]") {
    // Regression: the window took the width of the bar of a file outside the book, so it
    // reached beyond a small screen
    Panels panels;
    // The bars of the program's window: the menus, the toolbars and the status bar
    auto* menus = new QMenuBar(&panels.window);
    menus->setNativeMenuBar(false);
    for (const char* title : {"File", "Edit", "Book", "Insert", "Format", "Tools", "Assistant",
                              "View", "Help"}) {
        menus->addMenu(QString::fromLatin1(title));
    }
    panels.window.setMenuBar(menus);
    ToolbarManager toolbars(&panels.window);
    toolbars.createToolbars(CommandRegistry::getInstance());
    panels.window.statusBar()->showMessage(QStringLiteral("Words: 0"));

    panels.docks->logDock()->show();
    panels.docks->standaloneInfoBar()->show();
    panels.docks->showLayoutNotice();
    auto* chapter = new EditorPanel();
    panels.docks->centralTabs()->addTab(chapter, QStringLiteral("Chapter 1"));
    panels.docks->centralTabs()->setCurrentWidget(chapter);
    QApplication::processEvents();

    const QSize needed = panels.window.minimumSizeHint();
    CAPTURE(needed.width(), needed.height());
    CHECK(needed.width() <= SMALL_SCREEN.width());
    CHECK(needed.height() <= SMALL_SCREEN.height() - WINDOW_FRAME_HEIGHT);

    SECTION("The panels on the right one under another") {
        // Each panel keeps a few lines and scrolls, so the window is not higher than the
        // screen
        const QList<QDockWidget*> right = panels.rightDocks();
        for (qsizetype i = 1; i < right.size(); ++i) {
            panels.window.splitDockWidget(right.at(i - 1), right.at(i), Qt::Vertical);
        }
        for (QDockWidget* dock : right) {
            dock->show();
        }
        QApplication::processEvents();
        const QSize stacked = panels.window.minimumSizeHint();
        CAPTURE(stacked.width(), stacked.height());
        CHECK(stacked.width() <= SMALL_SCREEN.width());
        CHECK(stacked.height() <= SMALL_SCREEN.height() - WINDOW_FRAME_HEIGHT);
    }

    SECTION("The bar to find and replace fits the text") {
        panels.window.resize(SMALL_SCREEN.width(), SMALL_SCREEN.height() - WINDOW_FRAME_HEIGHT);
        chapter->getBookEditor()->showFindReplace();
        QApplication::processEvents();
        auto* findBar = chapter->findChild<editor::FindReplaceBar*>();
        REQUIRE(findBar != nullptr);
        REQUIRE(findBar->isVisible());
        CAPTURE(findBar->width(), findBar->minimumSizeHint().width(),
                chapter->getBookEditor()->width());
        CHECK(findBar->minimumSizeHint().width() <= findBar->width());
    }
}

TEST_CASE("Panels: choosing an element in the Navigator keeps the panel on top",
          "[gui][panels][navigator]") {
    // Regression: every element chosen in the Navigator brought the Properties panel to the
    // front, so the Annotations panel seemed gone once a chapter was opened from the Navigator
    OpenBook book;
    Panels panels;
    NavigatorPanel* navigator = panels.docks->navigatorPanel();
    const auto& projects = core::ProjectManager::getInstance();
    REQUIRE(projects.project() != nullptr);
    navigator->loadProject(*projects.project(), projects.bookTypes());
    auto* tree = navigator->findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    QTreeWidgetItem* root = tree->topLevelItem(0);
    REQUIRE(root != nullptr);
    REQUIRE(panels.onTop(panels.docks->annotationsDock()));

    SECTION("with the mouse") {
        emit tree->itemClicked(root, 0);
    }
    SECTION("with the keys") {
        tree->setCurrentItem(root);
    }
    QApplication::processEvents();

    CHECK(panels.onTop(panels.docks->annotationsDock()));
}

TEST_CASE("Panels: the Properties command of the Navigator brings the Properties panel forward",
          "[gui][panels][navigator]") {
    OpenBook book;
    Panels panels;
    REQUIRE(panels.onTop(panels.docks->annotationsDock()));

    REQUIRE(QMetaObject::invokeMethod(panels.docks->navigatorPanel(), "onContextMenuProperties",
                                      Qt::DirectConnection));
    QApplication::processEvents();

    CHECK(panels.onTop(panels.docks->propertiesDock()));
}

TEST_CASE("Panels: the panels on the right can be made narrow", "[gui][panels]") {
    // On a small screen (1366x768 at 150%) wide panels on the right left the editor little room
    // The program's style (Fusion): its tab bars scroll, the macOS style's are as wide as their tabs
    const QString systemStyle = QApplication::style()->name();
    QApplication::setStyle(QStringLiteral("Fusion"));
    struct StyleBack {
        QString name;
        ~StyleBack() { QApplication::setStyle(name); }
    } styleBack{systemStyle};
    registerAllIcons();  // the buttons of the Paragraph and Styles panel show their icons
    Panels panels;
    // The Paragraph and Styles panel with all its rows: it follows a text
    auto* chapter = new EditorPanel();
    panels.docks->centralTabs()->addTab(chapter, QStringLiteral("Chapter 1"));
    panels.docks->setParagraphEditor(chapter->getBookEditor());
    QDockWidget* annotations = panels.docks->annotationsDock();
    panels.window.resizeDocks({annotations}, {100}, Qt::Horizontal);
    QApplication::processEvents();

    // Each panel is narrow, its long title too; the dock adds its frame, wider in some styles
    // (macOS). The column is as narrow as its widest panel can be
    int widest = 0;
    for (const QDockWidget* dock : panels.rightDocks()) {
        INFO(dock->objectName().toStdString());
        CHECK(dock->widget()->minimumSizeHint().width() <= 200);
        CHECK(dock->titleBarWidget()->minimumSizeHint().width() <= 200);
        widest = std::max(widest, dock->minimumSizeHint().width());
    }
    CHECK(annotations->width() <= widest);
}

TEST_CASE("Panels: the tabs of the panels on the right show their icons above the titles, "
          "with the names and keys of the panels in the tooltips",
          "[gui][panels][paneltabs]") {
    registerAllIcons();
    Panels panels;
    QApplication::processEvents();
    QDockWidget* annotations = panels.docks->annotationsDock();
    REQUIRE(panels.onTop(annotations));
    REQUIRE(panels.docks->panelTabs() != nullptr);
    PanelTabBar* tabs = panels.docks->panelTabs()->tabBar(annotations);
    REQUIRE(tabs != nullptr);
    CHECK(tabs->isVisible());
    CHECK(tabs->look() == PanelTabBar::Look::Icons);
    CHECK(tabs->side() == PanelTabBar::Side::Above);
    CHECK(annotations->titleBarWidget()->layout()->indexOf(tabs) == 0);

    // A tab for every panel on the right that is open, in front the Annotations panel
    QStringList names;
    for (int i = 0; i < tabs->count(); ++i) {
        names.append(tabs->tab(i).name);
        CHECK_FALSE(tabs->tab(i).icon.isNull());
    }
    CAPTURE(names.join(QStringLiteral(", ")).toStdString());
    CHECK(tabs->count() >= 3);
    CHECK(tabs->tab(tabs->currentIndex()).name == annotations->windowTitle());
    const int index = static_cast<int>(names.indexOf(annotations->windowTitle()));
    CHECK(tabs->toolTipOf(index) ==
          QStringLiteral("%1 (%2)").arg(annotations->windowTitle(),
                                        QKeySequence(Qt::Key_F9).toString(QKeySequence::NativeText)));

    // The tab of another panel brings it forward
    const int properties = static_cast<int>(names.indexOf(panels.docks->propertiesDock()->windowTitle()));
    REQUIRE(properties >= 0);
    emit tabs->tabChosen(properties);
    QApplication::processEvents();
    CHECK(panels.onTop(panels.docks->propertiesDock()));
}

TEST_CASE("Reset Layout: the toolbars dock at the top again, in their rows, shown as by default, "
          "and the Panels toolbar by the right edge",
          "[gui][panels][toolbar]") {
    // Reset Layout put the panels back, and a floating toolbar stayed where it was
    Panels panels;
    panels.window.setCentralWidget(new QLabel(QStringLiteral("text")));
    ToolbarManager toolbars(&panels.window);
    toolbars.createToolbars(CommandRegistry::getInstance());
    const QString mine =
        toolbars.createUserToolbar(QStringLiteral("Mine"), {QStringLiteral("file.save")});
    QApplication::processEvents();

    const std::vector<std::string> row1 = {"quickActions", "edit", "format", "insert"};
    const std::vector<std::string> row2 = {"book", "styles", "views", "tools", "help", "file",
                                           mine.toStdString()};
    const auto toolbar = [&toolbars](const std::string& id) {
        QToolBar* found = toolbars.getToolbar(id);
        REQUIRE(found != nullptr);
        return found;
    };

    // Moved by the writer: Format and their own toolbar floating (the window flags Qt gives a
    // toolbar dragged off the window), their own closed, Edit by the left edge, Help closed
    // and File opened, Panels at the top
    for (QToolBar* floating : {toolbar("format"), toolbar(mine.toStdString())}) {
        floating->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
        floating->move(400, 300);
        floating->show();
    }
    toolbar(mine.toStdString())->hide();
    panels.window.addToolBar(Qt::LeftToolBarArea, toolbar("edit"));
    toolbar("help")->hide();
    toolbar("file")->show();
    panels.window.addToolBar(Qt::TopToolBarArea, toolbar("view"));
    QApplication::processEvents();
    REQUIRE(toolbar("format")->isFloating());
    REQUIRE(panels.window.toolBarArea(toolbar("edit")) == Qt::LeftToolBarArea);
    REQUIRE(toolbar("edit")->orientation() == Qt::Vertical);

    // View > Reset Layout
    toolbars.applyDefaultLayout();
    panels.docks->resetLayout(false, false);
    QApplication::processEvents();

    // All at the top of the window, in the window, lying, in two rows in their order
    for (const std::vector<std::string>* row : {&row1, &row2}) {
        int left = -1;
        for (const std::string& id : *row) {
            INFO(id);
            QToolBar* bar = toolbar(id);
            CHECK_FALSE(bar->isFloating());
            CHECK(panels.window.toolBarArea(bar) == Qt::TopToolBarArea);
            CHECK(bar->orientation() == Qt::Horizontal);
            // The second row begins with Book
            CHECK(panels.window.toolBarBreak(bar) == (id == "book"));
            if (!bar->isHidden()) {
                CHECK(bar->y() == toolbar(row->front())->y());
                CHECK(bar->x() > left);
                left = bar->x();
            }
        }
    }
    CHECK(toolbar("book")->y() > toolbar("quickActions")->y());

    // Shown as by default: all but File, and the writer's own toolbar
    for (const std::vector<std::string>* row : {&row1, &row2}) {
        for (const std::string& id : *row) {
            INFO(id);
            CHECK(toolbar(id)->isHidden() == (id == "file"));
        }
    }

    // Panels standing by the right edge of the window, shown
    CHECK(panels.window.toolBarArea(toolbar("view")) == Qt::RightToolBarArea);
    CHECK(toolbar("view")->orientation() == Qt::Vertical);
    CHECK_FALSE(toolbar("view")->isHidden());

    CHECK(toolbars.deleteUserToolbar(mine));
}
