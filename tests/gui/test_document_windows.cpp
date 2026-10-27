/// @file test_document_windows.cpp
/// @brief The tabs of the texts in windows of their own: a text goes to one and back, the book
/// window still finds it (Save All, the Navigator), closing the window returns its texts or
/// closes them, the menu of a tab and the Window menu move them

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/settings_manager.h>
#include <kalahari/gui/command_registry.h>
#include <kalahari/gui/document_windows.h>
#include <kalahari/gui/panels/editor_panel.h>

#include "context_menu_reader.h"
#include "document_window.h"

#include <QAbstractButton>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QMainWindow>
#include <QMenuBar>
#include <QTabBar>
#include <QTabWidget>
#include <QToolBar>

#include <cstdio>
#include <memory>
#include <string>

using namespace kalahari;
using gui::DetachedWindow;
using gui::DocumentWindows;
using gui::EditorPanel;
using test::accepting;
using test::OpenNovel;
using test::Window;

namespace {

/// The windows closed meanwhile are deleted
void deleteClosed() {
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

/// Whether @p object was deleted, from now on
std::shared_ptr<bool> deletion(const QObject* object) {
    auto deleted = std::make_shared<bool>(false);
    QObject::connect(object, &QObject::destroyed, [deleted]() { *deleted = true; });
    return deleted;
}

/// A setting of the windows set for the test, and back as it was after it
class WindowSetting {
public:
    WindowSetting(const char* key, const std::string& value)
        : m_key(key), m_saved(settings().get<std::string>(key, "")) {
        settings().set<std::string>(m_key, value);
    }
    ~WindowSetting() {
        try {
            settings().set<std::string>(m_key, m_saved);
        } catch (...) {
            // A destructor may not throw: the later tests then find the setting of this one
            std::fputs("WindowSetting: the setting of the test stays\n", stderr);
        }
    }
    WindowSetting(const WindowSetting&) = delete;
    WindowSetting& operator=(const WindowSetting&) = delete;

private:
    static core::SettingsManager& settings() { return core::SettingsManager::getInstance(); }

    std::string m_key;
    std::string m_saved;
};

/// The pages DocumentWindows::frontChanged() names, in order
struct FrontPages {
    QList<QWidget*> pages;

    explicit FrontPages(DocumentWindows& owner) {
        QObject::connect(&owner, &DocumentWindows::frontChanged, &owner,
                         [this](QWidget* page) { pages.append(page); });
    }
};

/// A book window with the texts @p count in its tabs, as the main window has them
struct BookWindow {
    QMainWindow window;
    QTabWidget* tabs = new QTabWidget(&window);
    QList<EditorPanel*> texts;

    explicit BookWindow(int count) {
        window.setWindowTitle(QStringLiteral("Rain - Kalahari"));
        window.setCentralWidget(tabs);
        tabs->setTabsClosable(true);
        for (int i = 0; i < count; ++i) {
            auto* text = new EditorPanel();
            tabs->addTab(text, QStringLiteral("Chapter %1").arg(i + 1));
            texts.append(text);
        }
        // A tab closes as in the main window, from the book window or a window of its own
        const auto close = [](QWidget* page) {
            DocumentWindows::removePage(page);
            page->deleteLater();
        };
        QObject::connect(tabs, &QTabWidget::tabCloseRequested, &window,
                         [this, close](int index) { close(tabs->widget(index)); });
        QObject::connect(&DocumentWindows::of(*tabs), &DocumentWindows::closeRequested, &window,
                         close);
    }

    [[nodiscard]] DocumentWindows& owner() const { return DocumentWindows::of(*tabs); }
};

/// How tabMenu() chooses its item
enum class Choose {
    Key,   ///< With its access key (as with the mouse on macOS, which has none)
    Mouse  ///< As with the mouse
};

/// The menu of the tab at @p index of @p tabs, opened with the menu key: its items, the item
/// @p item chosen as @p choose says
QList<test::MenuItem> tabMenu(QTabWidget& tabs, int index, const QString& item = QString(),
                              Choose choose = Choose::Key) {
    QTabBar* bar = tabs.tabBar();
    bar->setCurrentIndex(index);
    test::ContextMenuReader reader(bar);
#ifdef Q_OS_MACOS
    choose = Choose::Mouse;  // macOS has no access keys
#endif
    if (!item.isEmpty() && choose == Choose::Mouse) {
        reader.chooseItem(item);
    } else if (!item.isEmpty()) {
        reader.chooseWith(item.at(item.indexOf(QLatin1Char('&')) + 1));
    }
    QContextMenuEvent event(QContextMenuEvent::Keyboard, bar->tabRect(index).center(),
                            bar->mapToGlobal(bar->tabRect(index).center()));
    QCoreApplication::sendEvent(bar, &event);
    REQUIRE(reader.menusRead() == 1);
    return reader.items();
}

/// The commands of the Window menu, registered for @p owner until the test ends
class WindowCommands {
public:
    explicit WindowCommands(DocumentWindows& owner) {
        CHECK(DocumentWindows::registerCommands(gui::CommandRegistry::getInstance(),
                                                [&owner]() { return &owner; }) == 5);
    }
    ~WindowCommands() {
        for (const char* id : IDS) {
            gui::CommandRegistry::getInstance().unregisterCommand(id);
        }
    }
    WindowCommands(const WindowCommands&) = delete;
    WindowCommands& operator=(const WindowCommands&) = delete;

    /// Whether the command @p id is enabled now
    [[nodiscard]] static bool enabled(const char* id) {
        return gui::CommandRegistry::getInstance().canExecute(id);
    }

    /// Run the command @p id
    static void run(const char* id) {
        REQUIRE(gui::CommandRegistry::getInstance().executeCommand(id) ==
                gui::CommandExecutionResult::Success);
    }

    static constexpr const char* MOVE = "window.moveTabToNewWindow";
    static constexpr const char* RETURN = "window.returnTab";
    static constexpr const char* NEXT = "window.next";
    static constexpr const char* PREVIOUS = "window.previous";
    static constexpr const char* SETTINGS = "window.settings";

private:
    static constexpr const char* IDS[] = {MOVE, RETURN, NEXT, PREVIOUS, SETTINGS};
};

}  // namespace

TEST_CASE("Windows: a text in a window of its own is found, saved and taken back",
          "[gui][windows]") {
    OpenNovel novel(QStringLiteral("Rain"));
    Window window;
    window.window.setWindowTitle(QStringLiteral("Rain - Kalahari"));
    window.writeChapter(novel.chapterId, QStringLiteral("The storm came at night."));
    EditorPanel* chapter = window.editor(0);
    DocumentWindows& texts = DocumentWindows::of(*window.tabs);
    DetachedWindow* own = texts.detach(chapter);
    REQUIRE(own != nullptr);
    const auto ownDeleted = deletion(own);
    CHECK(own->parent() == &window.window);
    CHECK(own->isVisible());
    CHECK(window.tabs->count() == 0);
    CHECK(own->tabs().count() == 1);
    CHECK(texts.windows() == QList<DetachedWindow*>{own});
    CHECK(texts.pages() == QList<QWidget*>{chapter});
    CHECK(texts.isDetached(chapter));
    CHECK(texts.frontPage() == chapter);
    CHECK(texts.frontTabs() == &own->tabs());
    // The text and the book window in its title, without the mark of changes
    CHECK(DocumentWindows::tabText(chapter) == QStringLiteral("*The Storm"));
    CHECK(own->windowTitle() == QStringLiteral("The Storm - Rain - Kalahari"));

    // The Navigator brings it to the front instead of opening it again
    window.navigator.onElementSelected(novel.chapterId, QStringLiteral("The Storm"));
    CHECK(window.tabs->count() == 0);
    CHECK(own->tabs().count() == 1);

    // Save All saves it, and its tab no longer shows changes
    CHECK(window.documents.unsavedDocumentNames().size() == 1);
    CHECK(accepting([&] { window.documents.onSaveAll(); }) == 0);
    CHECK(novel.chapterFile().contains("The storm came at night."));
    CHECK_FALSE(window.navigator.isChapterDirty(novel.chapterId));
    CHECK(DocumentWindows::tabText(chapter) == QStringLiteral("The Storm"));

    // Back in the book window, whose tabs it had; its window closes with its last tab
    texts.returnToBook(chapter);
    CHECK(window.tabs->count() == 1);
    CHECK(window.tabs->widget(0) == chapter);
    CHECK_FALSE(texts.isDetached(chapter));
    CHECK(texts.frontTabs() == window.tabs);
    CHECK(texts.frontPage() == chapter);
    deleteClosed();
    CHECK(*ownDeleted);
    CHECK(texts.windows().isEmpty());
}

TEST_CASE("Windows: texts move between the windows, the one in front followed",
          "[gui][windows]") {
    BookWindow book(3);
    DocumentWindows& owner = book.owner();
    const FrontPages front(owner);
    EditorPanel* first = book.texts.at(0);
    EditorPanel* second = book.texts.at(1);
    EditorPanel* third = book.texts.at(2);

    // Only a text goes to a window of its own
    QWidget other;
    CHECK(owner.detach(&other) == nullptr);
    CHECK_FALSE(DocumentWindows::canDetach(&other));

    DetachedWindow* own = owner.detach(second);
    REQUIRE(own != nullptr);
    REQUIRE_FALSE(front.pages.isEmpty());
    CHECK(front.pages.last() == second);
    CHECK(owner.pages() == QList<QWidget*>{first, third, second});

    // Alone there, it stays; another text joins it, in front
    CHECK(owner.detach(second) == own);
    owner.moveTo(first, own->tabs(), 0);
    CHECK(own->tabs().count() == 2);
    CHECK(own->tabs().widget(0) == first);
    CHECK(owner.frontPage() == first);
    CHECK(own->windowTitle() == QStringLiteral("Chapter 1 - Rain - Kalahari"));
    DocumentWindows::setTabText(first, QStringLiteral("*Chapter 1"));
    CHECK(own->windowTitle() == QStringLiteral("Chapter 1 - Rain - Kalahari"));

    // A text in front of the book window
    owner.showPage(third);
    CHECK(owner.frontTabs() == book.tabs);
    CHECK(front.pages.last() == third);

    // A second window of its own
    DetachedWindow* another = owner.detach(first);
    REQUIRE(another != nullptr);
    const auto anotherDeleted = deletion(another);
    CHECK(another != own);
    CHECK(owner.windows() == QList<DetachedWindow*>{own, another});
    CHECK(owner.pages() == QList<QWidget*>{third, second, first});

    // A tab closed in a window of its own closes as in the book window; its last tab gone,
    // the window closes
    emit another->tabs().tabCloseRequested(0);
    deleteClosed();
    CHECK(*anotherDeleted);
    CHECK(owner.pages() == QList<QWidget*>{third, second});
    CHECK(owner.windows() == QList<DetachedWindow*>{own});
}

TEST_CASE("Windows: closing a window of its own returns its texts, or closes them",
          "[gui][windows]") {
    SECTION("Its tabs return to the book window (default)") {
        const WindowSetting closing(DocumentWindows::CLOSING_KEY, "return");
        BookWindow book(3);
        DocumentWindows& owner = book.owner();
        DetachedWindow* own = owner.detach(book.texts.at(0));
        REQUIRE(own != nullptr);
        const auto ownDeleted = deletion(own);
        owner.moveTo(book.texts.at(2), own->tabs());
        CHECK(book.tabs->count() == 1);

        CHECK(own->close());
        deleteClosed();
        CHECK(*ownDeleted);
        CHECK(book.tabs->count() == 3);
        CHECK(owner.pages().size() == 3);
        // The text in front of the window is in front of the book window
        CHECK(owner.frontPage() == book.texts.at(2));
    }

    SECTION("Its texts close") {
        const WindowSetting closing(DocumentWindows::CLOSING_KEY, "close");
        BookWindow book(2);
        DocumentWindows& owner = book.owner();
        const auto secondDeleted = deletion(book.texts.at(1));
        DetachedWindow* own = owner.detach(book.texts.at(1));
        REQUIRE(own != nullptr);
        const auto ownDeleted = deletion(own);

        CHECK(own->close());
        deleteClosed();
        CHECK(*ownDeleted);
        CHECK(*secondDeleted);
        CHECK(book.tabs->count() == 1);
        CHECK(owner.pages() == QList<QWidget*>{book.texts.at(0)});
    }

    SECTION("A text that stays keeps its window open") {
        const WindowSetting closing(DocumentWindows::CLOSING_KEY, "close");
        QMainWindow window;
        auto* tabs = new QTabWidget(&window);
        window.setCentralWidget(tabs);
        auto* text = new EditorPanel();
        tabs->addTab(text, QStringLiteral("Chapter 1"));
        tabs->addTab(new EditorPanel(), QStringLiteral("Chapter 2"));
        DocumentWindows& owner = DocumentWindows::of(*tabs);
        // Its changes are kept: the tab does not close
        int asked = 0;
        QObject::connect(&owner, &DocumentWindows::closeRequested, &owner,
                         [&asked](QWidget*) { ++asked; });
        DetachedWindow* own = owner.detach(text);
        REQUIRE(own != nullptr);
        const auto ownDeleted = deletion(own);

        CHECK_FALSE(own->close());
        CHECK(asked == 1);
        deleteClosed();
        REQUIRE_FALSE(*ownDeleted);
        CHECK(own->isOpen());
        CHECK(owner.windows() == QList<DetachedWindow*>{own});
        CHECK(owner.pages().size() == 2);
    }
}

TEST_CASE("Windows: the look of a window of its own follows the settings", "[gui][windows]") {
    const WindowSetting look(DocumentWindows::LOOK_KEY, "full");
    BookWindow book(2);
    book.window.menuBar()->addMenu(QStringLiteral("&File"));
    book.window.menuBar()->addMenu(QStringLiteral("&Edit"));
    auto* standard = book.window.addToolBar(QStringLiteral("Standard"));
    standard->setObjectName(QStringLiteral("standard"));
    standard->addAction(QStringLiteral("Save"));
    auto* format = book.window.addToolBar(QStringLiteral("Format"));
    format->setObjectName(QStringLiteral("format"));
    format->addAction(QStringLiteral("Bold"));
    auto* hidden = book.window.addToolBar(QStringLiteral("Book"));
    hidden->setObjectName(QStringLiteral("book"));
    hidden->hide();

    // It stays open till the end
    DetachedWindow* own = book.owner().detach(book.texts.at(0));
    REQUIRE(own != nullptr);
    const auto toolbars = [own]() {
        QStringList names;
        for (const QToolBar* toolbar : own->findChildren<QToolBar*>()) {
            if (toolbar->parent() == own && !toolbar->isHidden()) {
                names << toolbar->objectName();
            }
        }
        names.sort();
        return names;
    };
    // The menus and the toolbars shown in the book window, with their commands
    CHECK(own->menuBar()->actions() == book.window.menuBar()->actions());
    CHECK(toolbars() == QStringList{QStringLiteral("format"), QStringLiteral("standard")});
    CHECK(own->createPopupMenu() == nullptr);

    // Writing only: the Format toolbar
    core::SettingsManager::getInstance().set<std::string>(DocumentWindows::LOOK_KEY, "writing");
    CHECK(DocumentWindows::look() == DocumentWindows::Look::Writing);
    QCoreApplication::processEvents();
    deleteClosed();
    CHECK(own->menuBar()->isHidden());
    CHECK(toolbars() == QStringList{QStringLiteral("format")});
}

TEST_CASE("Windows: the menu of a tab and the Window menu move the texts", "[gui][windows]") {
    BookWindow book(2);
    DocumentWindows& owner = book.owner();
    const WindowCommands commands(owner);
    EditorPanel* first = book.texts.at(0);

    // In the book window: to a window of its own, not back
    const QList<test::MenuItem> items = tabMenu(*book.tabs, 0);
    REQUIRE(items.size() == 4);
    CHECK(items.at(0).text == DocumentWindows::tr("&Move to New Window"));
    CHECK(items.at(0).enabled);
    CHECK(items.at(1).text == DocumentWindows::tr("&Return to Book Window"));
    CHECK_FALSE(items.at(1).enabled);
    CHECK(items.at(2).separator);
    CHECK(items.at(3).text == DocumentWindows::tr("&Close Tab"));
    owner.showPage(first);
    CHECK(WindowCommands::enabled(WindowCommands::MOVE));
    CHECK_FALSE(WindowCommands::enabled(WindowCommands::RETURN));

    // Its access key moves it
    tabMenu(*book.tabs, 0, DocumentWindows::tr("&Move to New Window"));
    REQUIRE(owner.isDetached(first));
    auto* own = qobject_cast<DetachedWindow*>(first->window());
    REQUIRE(own != nullptr);
    const auto ownDeleted = deletion(own);

    // Alone in its window: back, not to another one
    const QList<test::MenuItem> detached = tabMenu(own->tabs(), 0);
    CHECK_FALSE(detached.at(0).enabled);
    CHECK(detached.at(1).enabled);
    owner.showPage(first);
    CHECK_FALSE(WindowCommands::enabled(WindowCommands::MOVE));
    CHECK(WindowCommands::enabled(WindowCommands::RETURN));

    // Window > Return Tab to Book Window, then Move Tab to New Window
    WindowCommands::run(WindowCommands::RETURN);
    CHECK_FALSE(owner.isDetached(first));
    deleteClosed();
    CHECK(*ownDeleted);
    owner.showPage(first);
    WindowCommands::run(WindowCommands::MOVE);
    CHECK(owner.isDetached(first));

    // Window Settings... and the gear of the window open Settings > General
    int settings = 0;
    QObject::connect(&owner, &DocumentWindows::settingsRequested, &owner,
                     [&settings]() { ++settings; });
    WindowCommands::run(WindowCommands::SETTINGS);
    CHECK(settings == 1);
    auto* window = qobject_cast<DetachedWindow*>(first->window());
    REQUIRE(window != nullptr);
    auto* gear = qobject_cast<QAbstractButton*>(window->tabs().cornerWidget(Qt::TopRightCorner));
    REQUIRE(gear != nullptr);
    CHECK_FALSE(gear->toolTip().isEmpty());
    gear->click();
    CHECK(settings == 2);

    // The menu of a tab closes it, as its cross; chosen with the mouse
    tabMenu(window->tabs(), 0, DocumentWindows::tr("&Close Tab"), Choose::Mouse);
    deleteClosed();
    CHECK(owner.pages() == QList<QWidget*>{book.texts.at(1)});
    CHECK(owner.windows().isEmpty());
}

TEST_CASE("Windows: the Window menu lists the windows of the book", "[gui][windows]") {
    BookWindow book(2);
    DocumentWindows& owner = book.owner();
    const WindowCommands commands(owner);
    QMenu menu;
    menu.addAction(gui::CommandRegistry::getInstance().getAction(
        QString::fromLatin1(WindowCommands::MOVE)));
    menu.addSeparator();
    menu.addAction(gui::CommandRegistry::getInstance().getAction(
        QString::fromLatin1(WindowCommands::SETTINGS)));
    owner.listWindowsIn(&menu);
    REQUIRE(owner.detach(book.texts.at(1)) != nullptr);

    const auto titles = [&menu]() {
        emit menu.aboutToShow();
        QStringList texts;
        for (const QAction* action : menu.actions()) {
            texts << (action->isSeparator() ? QStringLiteral("-") : action->text());
        }
        return texts;
    };
    const QStringList expected{DocumentWindows::tr("&Move Tab to New Window"),
                               QStringLiteral("-"),
                               QStringLiteral("&1 Rain"),
                               QStringLiteral("&2 Chapter 2 - Rain"),
                               QStringLiteral("-"),
                               DocumentWindows::tr("Window &Settings...")};
    CHECK(titles() == expected);
    // Opened again, it lists them once
    CHECK(titles() == expected);
}
