/// @file test_book_windows.cpp
/// @brief The book windows of the program, each with its own book and commands: Window > New
/// Window, a second book in a new window or in place of the open one (the question Opening a
/// Book), the same book never twice, File > Exit for every window with one question, the books
/// of the last work at startup, and what the windows share (the settings, the keys, the grammar
/// server, the writer's own words)

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/recent_books_manager.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/editor/spell_check_service.h>
#include <kalahari/gui/command_registry.h>
#include <kalahari/gui/dialogs/message_dialog.h>
#include <kalahari/gui/dialogs/open_book_dialog.h>
#include <kalahari/gui/document_coordinator.h>
#include <kalahari/gui/document_windows.h>
#include <kalahari/gui/grammar_coordinator.h>
#include <kalahari/gui/main_window.h>
#include <kalahari/gui/menu_builder.h>
#include <kalahari/gui/settings_coordinator.h>
#include <kalahari/gui/shortcut_settings.h>
#include <kalahari/gui/special_character_coordinator.h>
#include <kalahari/gui/spelling_coordinator.h>
#include <kalahari/gui/widgets/central_tabs.h>
#include <kalahari/gui/widgets/standalone_info_bar.h>

#include "document_window.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTimer>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>

using namespace kalahari;
using gui::MainWindow;
using gui::dialogs::MessageDialog;
using gui::dialogs::OpenBookDialog;

namespace {

/// Process the events until @p done, at most @p ms
bool waitFor(const std::function<bool()>& done, int ms = 10000) {
    QElapsedTimer timer;
    timer.start();
    while (!done()) {
        if (timer.elapsed() > ms) {
            return false;
        }
        QApplication::processEvents(QEventLoop::AllEvents, 20);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    return true;
}

/// A setting of the test, and back as it was after it
template <typename T>
class Setting {
public:
    Setting(const char* key, const T& value)
        : m_key(key), m_saved(core::SettingsManager::getInstance().get<T>(key, T())) {
        core::SettingsManager::getInstance().set<T>(m_key, value);
    }
    ~Setting() {
        try {
            core::SettingsManager::getInstance().set<T>(m_key, m_saved);
        } catch (...) {
            // A destructor may not throw: the later tests then find the setting of this one
            std::fputs("Setting: the setting of the test stays\n", stderr);
        }
    }
    Setting(const Setting&) = delete;
    Setting& operator=(const Setting&) = delete;

private:
    std::string m_key;
    T m_saved;
};

/// A new novel titled @p title in @p dir, closed; its .klh file
QString novel(const QTemporaryDir& dir, const QString& title) {
    core::ProjectManager::loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    core::ProjectManager pm;
    REQUIRE(pm.createProject(dir.path(), title, QStringLiteral("Author"), QStringLiteral("en"),
                             true, QStringLiteral("kalahari.novel")));
    const QString manifest = pm.getManifestPath();
    pm.closeProject(false);
    REQUIRE_FALSE(manifest.isEmpty());
    return manifest;
}

/// The book windows of a test: a first window, then the ones made meanwhile, closed without
/// questions and deleted when the test ends
class Windows {
public:
    Windows() : m_first(new MainWindow) {
        m_first->setAttribute(Qt::WA_DeleteOnClose);
        m_first->show();
    }
    ~Windows() {
        for (MainWindow* window : MainWindow::windows()) {
            window->book().closeProject(false);
            delete window;
        }
    }
    Windows(const Windows&) = delete;
    Windows& operator=(const Windows&) = delete;

    /// The first window, until a test closes it
    [[nodiscard]] MainWindow* first() const { return m_first; }

private:
    MainWindow* m_first;
};

/// Open @p manifest in @p window, as a later start of the program asks
void open(MainWindow* window, const QString& manifest) {
    window->onLaterStart(manifest);
}

/// The window with the book @p manifest, once it opened it
MainWindow* waitForBook(const QString& manifest) {
    MainWindow* found = nullptr;
    waitFor([&]() {
        found = MainWindow::windowOfBook(manifest);
        return found != nullptr;
    });
    return found;
}

/// The texts of the info bar of @p window, if it is shown
QString infoBarText(const MainWindow& window) {
    QStringList texts;
    for (const auto* bar : window.findChildren<gui::StandaloneInfoBar*>()) {
        if (bar->isVisible()) {
            for (const QLabel* label : bar->findChildren<QLabel*>()) {
                texts << label->text();
            }
        }
    }
    return texts.join(QLatin1Char(' '));
}

/// The texts of the Window menu of @p window as it opens
QStringList windowMenu(const MainWindow& window) {
    QMenu* menu = nullptr;
    for (QAction* action : window.menuBar()->actions()) {
        if (action->menu() != nullptr && action->text() == QObject::tr("&Window")) {
            menu = action->menu();
        }
    }
    REQUIRE(menu != nullptr);
    emit menu->aboutToShow();
    QStringList texts;
    for (const QAction* action : menu->actions()) {
        texts << (action->isSeparator() ? QStringLiteral("-") : action->text());
    }
    return texts;
}

/// Answer the question Opening a Book with @p choice, remembering it or not, while @p command
/// runs; the other windows with the button that does their job
/// @return The questions Opening a Book shown
int answeringOpenBook(const std::function<void()>& command, OpenBookDialog::Choice choice,
                      bool remember = false) {
    int asked = 0;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        QWidget* modal = QApplication::activeModalWidget();
        if (modal == nullptr || modal->property("handled").toBool()) {
            return;
        }
        modal->setProperty("handled", true);
        if (auto* question = qobject_cast<OpenBookDialog*>(modal)) {
            ++asked;
            question->rememberBox()->setChecked(remember);
            QAbstractButton* button = choice == OpenBookDialog::Choice::NewWindow
                                          ? static_cast<QAbstractButton*>(question->acceptButton())
                                      : choice == OpenBookDialog::Choice::ThisWindow
                                          ? question->thisWindowButton()
                                          : question->cancelButton();
            QMetaObject::invokeMethod(button, "click", Qt::QueuedConnection);
        } else if (auto* own = qobject_cast<gui::dialogs::KalahariDialog*>(modal)) {
            QMetaObject::invokeMethod(own->acceptButton(), "click", Qt::QueuedConnection);
        }
    });
    timer.start();
    command();
    return asked;
}

}  // namespace

TEST_CASE("Book windows: each has its own book and commands; Window > New Window",
          "[gui][windows][book-windows]") {
    const Windows windows;
    MainWindow* first = windows.first();
    REQUIRE(MainWindow::windows() == QList<MainWindow*>{first});
    CHECK(MainWindow::windowOf(first) == first);

    // Window > New Window: another window with the Dashboard and commands of its own
    REQUIRE(first->commands().executeCommand("window.new") ==
            gui::CommandExecutionResult::Success);
    REQUIRE(MainWindow::windows().size() == 2);
    MainWindow* second = MainWindow::windows().at(1);
    CHECK(second->isVisible());
    CHECK(second->testAttribute(Qt::WA_DeleteOnClose));
    CHECK_FALSE(second->book().isProjectOpen());
    CHECK(&second->book() != &first->book());
    CHECK(&second->commands() != &first->commands());
    CHECK(gui::CommandRegistry::registries().contains(&first->commands()));
    CHECK(gui::CommandRegistry::registries().contains(&second->commands()));
    CHECK(MainWindow::frontWindow() == second);

    // The Window menu of each one lists the windows of the program
    const QStringList menu = windowMenu(*first);
    CHECK(menu.first() == gui::DocumentWindows::tr("Ne&w Window"));
    CHECK(menu.contains(QStringLiteral("&1 Kalahari")));
    CHECK(menu.contains(QStringLiteral("&2 Kalahari")));
    CHECK(windowMenu(*second).filter(QStringLiteral(" Kalahari")).size() == 2);

    // Closed, it is gone; the first one stays
    second->close();
    CHECK(waitFor([]() { return MainWindow::windows().size() == 1; }));
    CHECK(MainWindow::windows() == QList<MainWindow*>{first});
}

TEST_CASE("Book windows: the tab bars of the docked panels go after the parts of the window",
          "[gui][windows][book-windows]") {
    // Qt keeps the tab bars of the docked panels: a window that takes the layout of another
    // one has one no longer used, which Qt 6.8 and later deletes with the layout of the
    // window. Deleted with the parts of the window, it was deleted twice.
    const Windows windows;
    MainWindow* second = windows.first()->openNewWindow();
    REQUIRE(MainWindow::windows().size() == 2);
    CHECK(waitFor([second]() {
        const auto bars = second->findChildren<QTabBar*>(Qt::FindDirectChildrenOnly);
        return std::any_of(bars.cbegin(), bars.cend(), [](const QTabBar* bar) {
            return bar->isHidden() && bar->count() == 0;
        });
    }));
    const QList<QTabBar*> bars = second->findChildren<QTabBar*>(Qt::FindDirectChildrenOnly);
    REQUIRE_FALSE(bars.isEmpty());

    // Each one goes once the window is no longer a main window: with its layout or after it
    int gone = 0;
    int beforeLayout = 0;
    for (QTabBar* bar : bars) {
        QObject::connect(bar, &QObject::destroyed, bar, [second, &gone, &beforeLayout]() {
            ++gone;
            if (qobject_cast<QMainWindow*>(static_cast<QObject*>(second)) != nullptr) {
                ++beforeLayout;
            }
        });
    }
    delete second;
    CHECK(gone == bars.size());
    CHECK(beforeLayout == 0);
}

TEST_CASE("Book windows: a second book in a new window, the same book never twice",
          "[gui][windows][book-windows]") {
    const test::RecentBooks recent;
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString island = novel(dir, QStringLiteral("Island"));
    const QString desert = novel(dir, QStringLiteral("Desert"));
    const Setting<std::string> where(gui::DocumentCoordinator::OTHER_BOOK_KEY, "new");

    const Windows windows;
    MainWindow* first = windows.first();
    open(first, island);
    REQUIRE(waitForBook(island) == first);

    // Another book: a new window has it, the first one keeps its book
    open(first, desert);
    MainWindow* second = waitForBook(desert);
    REQUIRE(second != nullptr);
    CHECK(second != first);
    CHECK(MainWindow::windows().size() == 2);
    CHECK(first->book().isProjectOpen());
    CHECK(QFileInfo(first->book().getManifestPath()) == QFileInfo(island));
    CHECK(second->windowTitle() == QStringLiteral("Desert - Kalahari"));
    const QStringList menu = windowMenu(*second);
    CHECK(menu.contains(QStringLiteral("&1 Island")));
    CHECK(menu.contains(QStringLiteral("&2 Desert")));

    // The book open in the first window, chosen in the second one: the first one comes to the
    // front saying so, and nothing else opens
    open(second, island);
    REQUIRE(waitFor([&]() {
        return infoBarText(*first).contains(QStringLiteral("is already open"));
    }));
    CHECK(infoBarText(*first).contains(
        QStringLiteral("The book \"Island\" is already open – in this window.")));
    CHECK(MainWindow::windows().size() == 2);
    CHECK(QFileInfo(second->book().getManifestPath()) == QFileInfo(desert));
    CHECK(MainWindow::frontWindow() == first);

    // The book of its own window, chosen again there: nothing happens
    open(first, island);
    QApplication::processEvents();
    CHECK(MainWindow::windows().size() == 2);
    CHECK(QFileInfo(first->book().getManifestPath()) == QFileInfo(island));

    // Its book closes: so does what the info bar said of it
    first->book().closeProject(false);
    CHECK_FALSE(infoBarText(*first).contains(QStringLiteral("is already open")));
}

TEST_CASE("Book windows: the question Opening a Book", "[gui][windows][book-windows]") {
    const test::RecentBooks recent;
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString island = novel(dir, QStringLiteral("Island"));
    const QString desert = novel(dir, QStringLiteral("Desert"));
    const QString forest = novel(dir, QStringLiteral("Forest"));
    const Setting<std::string> where(gui::DocumentCoordinator::OTHER_BOOK_KEY, "ask");
    auto& settings = core::SettingsManager::getInstance();

    const Windows windows;
    MainWindow* first = windows.first();
    open(first, island);
    REQUIRE(waitForBook(island) == first);

    SECTION("The question itself") {
        const OpenBookDialog question(QStringLiteral("Island"), QStringLiteral("Desert"), first);
        CHECK(question.windowTitle() == OpenBookDialog::tr("Opening a Book"));
        CHECK(test::textsOf(question).contains(QStringLiteral(
            "This window has the book \"Island\" open. Where do you want to open the book "
            "\"Desert\"?")));
        CHECK(question.acceptButton()->text() == OpenBookDialog::tr("In a &New Window"));
        CHECK(question.acceptButton()->isDefault());
        CHECK(question.thisWindowButton()->text() == OpenBookDialog::tr("In &This Window"));
        // Three answers: no Apply
        CHECK_FALSE(question.thisWindowButton()->isHidden());
        CHECK_FALSE(question.cancelButton()->isHidden());
        CHECK(question.applyButton()->isHidden());
        CHECK_FALSE(question.rememberBox()->isChecked());
        CHECK(question.choice() == OpenBookDialog::Choice::Cancel);
    }

    SECTION("Cancel: nothing opens") {
        CHECK(answeringOpenBook(
                  [&]() {
                      open(first, desert);
                      QApplication::processEvents();
                  },
                  OpenBookDialog::Choice::Cancel) == 1);
        CHECK(MainWindow::windows().size() == 1);
        CHECK(QFileInfo(first->book().getManifestPath()) == QFileInfo(island));
        CHECK(settings.get<std::string>(gui::DocumentCoordinator::OTHER_BOOK_KEY) == "ask");
    }

    SECTION("In a new window, remembered: the next book goes there without the question") {
        CHECK(answeringOpenBook(
                  [&]() {
                      open(first, desert);
                      waitForBook(desert);
                  },
                  OpenBookDialog::Choice::NewWindow, true) == 1);
        MainWindow* second = MainWindow::windowOfBook(desert);
        REQUIRE(second != nullptr);
        CHECK(second != first);
        CHECK(settings.get<std::string>(gui::DocumentCoordinator::OTHER_BOOK_KEY) == "new");

        CHECK(answeringOpenBook(
                  [&]() {
                      open(first, forest);
                      waitForBook(forest);
                  },
                  OpenBookDialog::Choice::Cancel) == 0);
        CHECK(MainWindow::windows().size() == 3);
    }

    SECTION("In this window: the open book closes, without asking again") {
        CHECK(answeringOpenBook(
                  [&]() {
                      open(first, desert);
                      waitForBook(desert);
                  },
                  OpenBookDialog::Choice::ThisWindow) == 1);
        CHECK(MainWindow::windows().size() == 1);
        CHECK(MainWindow::windowOfBook(desert) == first);
        CHECK(MainWindow::windowOfBook(island) == nullptr);
        CHECK(settings.get<std::string>(gui::DocumentCoordinator::OTHER_BOOK_KEY) == "ask");
    }
}

TEST_CASE("Book windows: File > Exit closes every window with one question",
          "[gui][windows][book-windows]") {
    const test::RecentBooks recent;
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString island = novel(dir, QStringLiteral("Island"));
    const QString desert = novel(dir, QStringLiteral("Desert"));
    const Setting<std::string> where(gui::DocumentCoordinator::OTHER_BOOK_KEY, "new");

    const Windows windows;
    MainWindow* first = windows.first();
    open(first, island);
    REQUIRE(waitForBook(island) == first);
    open(first, desert);
    MainWindow* second = waitForBook(desert);
    REQUIRE(second != nullptr);
    // Both books have unsaved changes
    first->book().setDirty(true);
    second->book().setDirty(true);

    QStringList said;
    SECTION("Cancel: every window stays") {
        CHECK(test::answering([&]() { CHECK_FALSE(MainWindow::closeAllWindows(second)); },
                              MessageDialog::Answer::Cancel, said) == 1);
        CHECK(MainWindow::windows().size() == 2);
        CHECK(first->isVisible());
        CHECK(second->isVisible());
    }

    SECTION("Don't Save: one question names both books, every window closes") {
        core::RecentBooksManager::getInstance().setOpenBooks({});
        CHECK(test::answering([&]() { CHECK(MainWindow::closeAllWindows(second)); },
                              MessageDialog::Answer::Alternative, said) == 1);
        REQUIRE(said.size() == 1);
        CHECK(said.first().contains(QStringLiteral("Island")));
        CHECK(said.first().contains(QStringLiteral("Desert")));
        CHECK(waitFor([]() { return MainWindow::windows().isEmpty(); }));

        // The books of the next start, the one of the window that asked last
        const auto open = core::SettingsManager::getInstance().get<std::vector<std::string>>(
            core::RecentBooksManager::OPEN_BOOKS_KEY);
        REQUIRE(open.size() == 2);
        CHECK(QFileInfo(QString::fromStdString(open.at(0))) == QFileInfo(island));
        CHECK(QFileInfo(QString::fromStdString(open.at(1))) == QFileInfo(desert));
    }
}

TEST_CASE("Book windows: the last window closed keeps its book for the next start",
          "[gui][windows][book-windows]") {
    const test::RecentBooks recent;
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString island = novel(dir, QStringLiteral("Island"));
    const QString desert = novel(dir, QStringLiteral("Desert"));
    const Setting<std::string> where(gui::DocumentCoordinator::OTHER_BOOK_KEY, "new");
    core::RecentBooksManager::getInstance().setOpenBooks({island, desert});

    const Windows windows;
    MainWindow* first = windows.first();
    open(first, island);
    REQUIRE(waitForBook(island) == first);
    open(first, desert);
    MainWindow* second = waitForBook(desert);
    REQUIRE(second != nullptr);

    // A window closed while another one is open is not the end of the work
    first->close();
    REQUIRE(waitFor([]() { return MainWindow::windows().size() == 1; }));
    auto& settings = core::SettingsManager::getInstance();
    CHECK(settings.get<std::vector<std::string>>(core::RecentBooksManager::OPEN_BOOKS_KEY).size() ==
          2);
    // The last one is
    second->close();
    REQUIRE(waitFor([]() { return MainWindow::windows().isEmpty(); }));
    const auto open = settings.get<std::vector<std::string>>(core::RecentBooksManager::OPEN_BOOKS_KEY);
    REQUIRE(open.size() == 1);
    CHECK(QFileInfo(QString::fromStdString(open.front())) == QFileInfo(desert));
}

TEST_CASE("Book windows: the books of the last work open at startup, each in its window",
          "[gui][windows][book-windows]") {
    const test::RecentBooks recent;
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QString island = novel(dir, QStringLiteral("Island"));
    const QString desert = novel(dir, QStringLiteral("Desert"));
    auto& recentBooks = core::RecentBooksManager::getInstance();
    recentBooks.addRecentFile(island);
    recentBooks.addRecentFile(desert);
    core::SettingsManager::getInstance().set<std::string>("startup.lastBook",
                                                          desert.toStdString());
    recentBooks.setOpenBooks({island, desert});
    const Setting<bool> autoLoad("startup.autoLoadLastProject", true);

    SECTION("All, each in its own window") {
        const Setting<std::string> books(core::RecentBooksManager::STARTUP_BOOKS_KEY, "all");
        CHECK(recentBooks.booksToOpenAtStart() == QStringList{island, desert});
        auto* first = new MainWindow;
        first->setFirstWindow(true);
        first->show();
        const Windows windows;  // deletes the windows; its own first one opens nothing
        CHECK(waitForBook(island) == first);
        MainWindow* second = waitForBook(desert);
        REQUIRE(second != nullptr);
        CHECK(second != first);
        CHECK(MainWindow::windows().size() == 3);
    }

    SECTION("Only the last one") {
        const Setting<std::string> books(core::RecentBooksManager::STARTUP_BOOKS_KEY, "last");
        CHECK(recentBooks.booksToOpenAtStart() == QStringList{desert});
        auto* first = new MainWindow;
        first->setFirstWindow(true);
        first->show();
        const Windows windows;
        CHECK(waitForBook(desert) == first);
        QApplication::processEvents();
        CHECK(MainWindow::windowOfBook(island) == nullptr);
        CHECK(MainWindow::windows().size() == 2);
    }

    SECTION("A book of the last work gone since is left out") {
        const Setting<std::string> books(core::RecentBooksManager::STARTUP_BOOKS_KEY, "all");
        recentBooks.setOpenBooks({dir.filePath(QStringLiteral("Gone/Gone.klh")), desert});
        CHECK(recentBooks.booksToOpenAtStart() == QStringList{desert});
        recentBooks.setOpenBooks({});
        CHECK(recentBooks.booksToOpenAtStart() == QStringList{desert});
    }
}

TEST_CASE("Book windows: what the windows share", "[gui][windows][book-windows]") {
    const Windows windows;
    MainWindow* first = windows.first();
    MainWindow* second = first->openNewWindow();
    REQUIRE(MainWindow::windows().size() == 2);

    SECTION("The settings applied in one window show in every window") {
        auto* firstSettings = first->findChild<gui::SettingsCoordinator*>();
        REQUIRE(firstSettings != nullptr);
        CHECK(gui::SettingsCoordinator::coordinators().size() == 2);
        const Setting<std::string> rows(gui::CentralTabs::ROWS_KEY, gui::CentralTabs::ROWS_WRAP);
        firstSettings->onApplySettings({QString::fromLatin1(gui::CentralTabs::ROWS_KEY)});
        for (MainWindow* window : {first, second}) {
            auto* tabs = window->findChild<gui::CentralTabs*>();
            REQUIRE(tabs != nullptr);
            CHECK(tabs->rows() == gui::PanelTabBar::Rows::Wrap);
        }
    }

    SECTION("One grammar server for every window") {
        auto* firstGrammar = first->findChild<gui::GrammarCoordinator*>();
        auto* secondGrammar = second->findChild<gui::GrammarCoordinator*>();
        REQUIRE(firstGrammar != nullptr);
        REQUIRE(secondGrammar != nullptr);
        CHECK(firstGrammar->server() == secondGrammar->server());
        CHECK(firstGrammar->setup() == secondGrammar->setup());
    }

    SECTION("A word added to the dictionary in one window is known in the other") {
        auto* firstSpelling = first->findChild<gui::SpellingCoordinator*>();
        auto* secondSpelling = second->findChild<gui::SpellingCoordinator*>();
        REQUIRE(firstSpelling != nullptr);
        REQUIRE(secondSpelling != nullptr);
        const QString word = QStringLiteral("Kalaharix");
        REQUIRE_FALSE(secondSpelling->service()->isInUserDictionary(word));
        firstSpelling->service()->addToUserDictionary(word);
        CHECK(secondSpelling->service()->isInUserDictionary(word));
        firstSpelling->service()->removeFromUserDictionary(word);
        CHECK_FALSE(secondSpelling->service()->isInUserDictionary(word));
    }

    SECTION("Keys given to a character in one window work in the other") {
        auto* characters = first->findChild<gui::SpecialCharacterCoordinator*>();
        REQUIRE(characters != nullptr);
        const gui::KeyboardShortcut keys(Qt::Key_F7, Qt::ControlModifier);
        characters->setCharacterKeys(U'→', keys);
        const QAction* action = second->commands().getAction(std::string("insert.character.2192"));
        REQUIRE(action != nullptr);
        CHECK(action->shortcut() == keys.toQKeySequence());
        CHECK(second->actions().contains(action));
        characters->setCharacterKeys(U'→', gui::KeyboardShortcut());
    }
}
