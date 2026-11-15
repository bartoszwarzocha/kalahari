// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_book_windows.cpp
/// @brief The book windows of the program, each with its own book and commands: Window > New
/// Window, a second book in a new window or in place of the open one (the question Opening a
/// Book), the same book never twice, File > Exit for every window with one question, the books
/// of the last work at startup, each book in the layout of its window, and what the windows
/// share (the settings, the keys, the grammar server, the writer's own words)

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_package.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/recent_books_manager.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/editor/spell_check_service.h>
#include <kalahari/gui/book_window_layouts.h>
#include <kalahari/gui/command.h>
#include <kalahari/gui/command_registry.h>
#include <kalahari/gui/dialogs/kalahari_dialog.h>
#include <kalahari/gui/dialogs/message_dialog.h>
#include <kalahari/gui/dialogs/open_book_dialog.h>
#include <kalahari/gui/document_coordinator.h>
#include <kalahari/gui/document_windows.h>
#include <kalahari/gui/grammar_coordinator.h>
#include <kalahari/gui/main_window.h>
#include <kalahari/gui/settings_coordinator.h>
#include <kalahari/gui/special_character_coordinator.h>
#include <kalahari/gui/spelling_coordinator.h>
#include <kalahari/gui/widgets/central_tabs.h>
#include <kalahari/gui/widgets/panel_tab_bar.h>
#include <kalahari/gui/widgets/standalone_info_bar.h>

#include "document_window.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QDockWidget>
#include <QElapsedTimer>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QTabBar>
#include <QTemporaryDir>
#include <QTimer>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace core = kalahari::core;
namespace gui = kalahari::gui;
namespace test = kalahari::test;
using gui::MainWindow;
using gui::dialogs::MessageDialog;
using gui::dialogs::OpenBookDialog;

namespace {

/// The longest a test waits for what it waits for
constexpr int WAIT_MS = 10000;

/// The events are processed this long at a time while a test waits
constexpr int EVENTS_MS = 20;

/// How often the question Opening a Book is looked for while a command runs
constexpr int QUESTION_LOOK_MS = 10;

/// Process the events until @p done, at most @p timeoutMs
bool waitFor(const std::function<bool()>& done, int timeoutMs = WAIT_MS) {
    QElapsedTimer timer;
    timer.start();
    while (!done()) {
        if (timer.elapsed() > timeoutMs) {
            return false;
        }
        QApplication::processEvents(QEventLoop::AllEvents, EVENTS_MS);
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
            (void)std::fputs("Setting: the setting of the test stays\n", stderr);
        }
    }
    Setting(const Setting&) = delete;
    Setting& operator=(const Setting&) = delete;
    Setting(Setting&&) = delete;
    Setting& operator=(Setting&&) = delete;

private:
    std::string m_key;
    T m_saved;
};

/// A new novel titled @p title in @p dir, closed; its .klh file
QString novel(const QTemporaryDir& dir, const QString& title) {
    core::ProjectManager::loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    core::ProjectManager book;
    REQUIRE(book.createProject(dir.path(), title, QStringLiteral("Author"), QStringLiteral("en"),
                               true, QStringLiteral("kalahari.novel")));
    const QString manifest = book.getManifestPath();
    book.closeProject(false);
    REQUIRE_FALSE(manifest.isEmpty());
    return manifest;
}

/// The settings of a test of the layouts kept for the books: none kept, back as they were after
/// it
class KeptLayouts {
public:
    KeptLayouts() = default;

private:
    Setting<nlohmann::json> m_kept{gui::BOOK_LAYOUTS_KEY, nlohmann::json::array()};
};

/// The books and the settings of a test of the book windows: two novels, Island and Desert, more
/// when the test asks, no layout kept for a book, no saved layout of the program (a window
/// without a layout of its book shows its panels), a book opened while another one is open goes
/// where @p where says ("new": a new window, "ask": the question Opening a Book); the settings
/// (the books of the last work too) and the recent books back as they were after the test
class TestBooks {
public:
    explicit TestBooks(const char* where = "new")
        : m_where(gui::DocumentCoordinator::OTHER_BOOK_KEY, where),
          m_island(novel(m_dir, QStringLiteral("Island"))),
          m_desert(novel(m_dir, QStringLiteral("Desert"))) {}

    /// The first novel
    [[nodiscard]] const QString& island() const { return m_island; }
    /// The second novel
    [[nodiscard]] const QString& desert() const { return m_desert; }
    /// Another new novel titled @p title
    [[nodiscard]] QString another(const QString& title) const { return novel(m_dir, title); }
    /// The path @p relative in the folder of the books
    [[nodiscard]] QString pathOf(const QString& relative) const { return m_dir.filePath(relative); }

private:
    test::RecentBooks m_recent;
    KeptLayouts m_kept;
    Setting<std::string> m_state{"window.state", std::string()};
    Setting<std::string> m_geometry{"window.geometry", std::string()};
    Setting<std::vector<std::string>> m_openBooks{core::RecentBooksManager::OPEN_BOOKS_KEY, {}};
    Setting<std::string> m_lastBook{"startup.lastBook", std::string()};
    Setting<std::string> m_where;
    QTemporaryDir m_dir;
    QString m_island;
    QString m_desert;
};

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
    Windows(Windows&&) = delete;
    Windows& operator=(Windows&&) = delete;

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

/// Open @p manifest in @p window; the window that opened it
MainWindow* opened(MainWindow* window, const QString& manifest) {
    open(window, manifest);
    return waitForBook(manifest);
}

/// Island open in @p first, then Desert in a new window; that window
MainWindow* withTwoBooks(MainWindow* first, const TestBooks& books) {
    REQUIRE(opened(first, books.island()) == first);
    MainWindow* const second = opened(first, books.desert());
    REQUIRE(second != nullptr);
    REQUIRE(second != first);
    return second;
}

/// Window > New Window in @p window, the only window of the test; the new window
MainWindow* newWindowByMenu(const MainWindow& window) {
    REQUIRE(window.commands().executeCommand("window.new") == gui::CommandExecutionResult::Success);
    REQUIRE(MainWindow::windows().size() == 2);
    return MainWindow::windows().at(1);
}

/// A second window of the program after the first window of @p windows
MainWindow* secondWindow(const Windows& windows) {
    MainWindow* const second = windows.first()->openNewWindow();
    REQUIRE(MainWindow::windows().size() == 2);
    return second;
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

/// The info bar of @p window says that the book is open, once it does
bool saysAlreadyOpen(const MainWindow& window) {
    return waitFor([&window]() {
        return infoBarText(window).contains(QStringLiteral("is already open"));
    });
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

/// The button of @p question that answers @p choice
QAbstractButton* buttonFor(const OpenBookDialog& question, OpenBookDialog::Choice choice) {
    switch (choice) {
    case OpenBookDialog::Choice::NewWindow:
        return question.acceptButton();
    case OpenBookDialog::Choice::ThisWindow:
        return question.thisWindowButton();
    case OpenBookDialog::Choice::Cancel:
        break;
    }
    return question.cancelButton();
}

/// Answer the question Opening a Book with @p choice, remembering it or not, while @p command
/// runs; the other windows with the button that does their job
/// @return The questions Opening a Book shown
int answeringOpenBook(const std::function<void()>& command, OpenBookDialog::Choice choice,
                      bool remember = false) {
    int asked = 0;
    QTimer timer;
    timer.setInterval(QUESTION_LOOK_MS);
    QObject::connect(&timer, &QTimer::timeout, [&]() {
        QWidget* modal = QApplication::activeModalWidget();
        if (modal == nullptr || modal->property("handled").toBool()) {
            return;
        }
        modal->setProperty("handled", true);
        if (auto* question = qobject_cast<OpenBookDialog*>(modal)) {
            ++asked;
            question->rememberBox()->setChecked(remember);
            QMetaObject::invokeMethod(buttonFor(*question, choice), "click", Qt::QueuedConnection);
        } else if (auto* own = qobject_cast<gui::dialogs::KalahariDialog*>(modal)) {
            QMetaObject::invokeMethod(own->acceptButton(), "click", Qt::QueuedConnection);
        }
    });
    timer.start();
    command();
    return asked;
}

/// Open @p manifest in @p window, answering the question Opening a Book with @p choice
/// (remembering it or not); unless the answer is Cancel, until the book is open
/// @return The questions Opening a Book shown
int openAnswering(MainWindow* window, const QString& manifest, OpenBookDialog::Choice choice,
                  bool remember = false) {
    return answeringOpenBook(
        [window, &manifest, choice]() {
            open(window, manifest);
            if (choice == OpenBookDialog::Choice::Cancel) {
                QApplication::processEvents();
            } else {
                waitForBook(manifest);
            }
        },
        choice, remember);
}

/// What another book opened in a window goes to, as the settings say now
std::string otherBookPlace() {
    return core::SettingsManager::getInstance().get<std::string>(
        gui::DocumentCoordinator::OTHER_BOOK_KEY);
}

/// The books kept for the next start (RecentBooksManager::OPEN_BOOKS_KEY)
QStringList keptOpenBooks() {
    QStringList books;
    const auto kept = core::SettingsManager::getInstance().get<std::vector<std::string>>(
        core::RecentBooksManager::OPEN_BOOKS_KEY);
    for (const std::string& book : kept) {
        books << QString::fromStdString(book);
    }
    return books;
}

/// File > Exit in @p asking, with unsaved books, not saving them; every window closes
void closeAllNotSaving(MainWindow* asking) {
    QStringList said;
    bool closed = false;
    REQUIRE(test::answering([&]() { closed = MainWindow::closeAllWindows(asking); },
                            MessageDialog::Answer::Alternative, said) == 1);
    REQUIRE(closed);
    REQUIRE(waitFor([]() { return MainWindow::windows().isEmpty(); }));
}

/// The tab bars of the docked panels of @p window (Qt keeps them as its direct children)
QList<QTabBar*> panelTabBarsOf(const MainWindow& window) {
    return window.findChildren<QTabBar*>(Qt::FindDirectChildrenOnly);
}

/// The tab bars of the docked panels of @p window, once one of them is no longer used (hidden,
/// without tabs)
QList<QTabBar*> tabBarsWithUnused(const MainWindow& window) {
    REQUIRE(waitFor([&window]() {
        const QList<QTabBar*> bars = panelTabBarsOf(window);
        return std::ranges::any_of(bars, [](const QTabBar* bar) {
            return bar->isHidden() && bar->count() == 0;
        });
    }));
    return panelTabBarsOf(window);
}

/// The tab bars deleted, and those of them deleted while their window was still a main window
struct TabBarsGone {
    int gone = 0;
    int beforeLayout = 0;
};

/// Count in @p counts the tab bars @p bars of @p window as they are deleted
void countGone(MainWindow* window, const QList<QTabBar*>& bars, TabBarsGone& counts) {
    for (QTabBar* const bar : bars) {
        QObject::connect(bar, &QObject::destroyed, bar, [window, &counts]() {
            ++counts.gone;
            if (qobject_cast<QMainWindow*>(static_cast<QObject*>(window)) != nullptr) {
                ++counts.beforeLayout;
            }
        });
    }
}

/// The rows of the tabs in the middle of @p window
gui::PanelTabBar::Rows centralRowsOf(const MainWindow& window) {
    const auto* tabs = window.findChild<gui::CentralTabs*>();
    REQUIRE(tabs != nullptr);
    return tabs->rows();
}

/// The grammar of @p window
const gui::GrammarCoordinator& grammarOf(const MainWindow& window) {
    const auto* grammar = window.findChild<gui::GrammarCoordinator*>();
    REQUIRE(grammar != nullptr);
    return *grammar;
}

/// The spelling of @p window
gui::SpellingCoordinator& spellingOf(const MainWindow& window) {
    auto* spelling = window.findChild<gui::SpellingCoordinator*>();
    REQUIRE(spelling != nullptr);
    return *spelling;
}

/// The books of the last work: Island and Desert, Desert opened last, both recent books
void lastWork(const TestBooks& books) {
    auto& recentBooks = core::RecentBooksManager::getInstance();
    recentBooks.addRecentFile(books.island());
    recentBooks.addRecentFile(books.desert());
    core::SettingsManager::getInstance().set<std::string>("startup.lastBook",
                                                          books.desert().toStdString());
    core::RecentBooksManager::setOpenBooks({books.island(), books.desert()});
}

/// The first window of the program at startup, with the file of the command line @p startupFile
/// (none: the books of the last work, as the settings say); shown, deleted with the windows of
/// the test
MainWindow* startingWindow(const QString& startupFile = QString()) {
    auto* first = new MainWindow;
    first->setFirstWindow(true);
    first->setStartupFile(startupFile);
    first->show();
    return first;
}

}  // namespace

TEST_CASE("Book windows: the first window is the only one", "[gui][windows][book-windows]") {
    const Windows windows;
    MainWindow* first = windows.first();
    CHECK(MainWindow::windows() == QList<MainWindow*>{first});
    CHECK(MainWindow::windowOf(first) == first);
}

TEST_CASE("Book windows: Window > New Window opens another window with the Dashboard",
          "[gui][windows][book-windows]") {
    const Windows windows;
    MainWindow* const second = newWindowByMenu(*windows.first());
    CHECK(second->isVisible());
    CHECK(second->testAttribute(Qt::WA_DeleteOnClose));
    CHECK_FALSE(second->book().isProjectOpen());
    CHECK(MainWindow::frontWindow() == second);
}

TEST_CASE("Book windows: each window has its own book and commands",
          "[gui][windows][book-windows]") {
    const Windows windows;
    MainWindow* const first = windows.first();
    MainWindow* const second = newWindowByMenu(*first);
    CHECK(&second->book() != &first->book());
    CHECK(&second->commands() != &first->commands());
    CHECK(gui::CommandRegistry::registries().contains(&first->commands()));
    CHECK(gui::CommandRegistry::registries().contains(&second->commands()));
}

TEST_CASE("Book windows: the Window menu of each window lists the windows of the program",
          "[gui][windows][book-windows]") {
    const Windows windows;
    MainWindow* const second = newWindowByMenu(*windows.first());
    const QStringList menu = windowMenu(*windows.first());
    CHECK(menu.first() == gui::DocumentWindows::tr("Ne&w Window"));
    CHECK(menu.contains(QStringLiteral("&1 Kalahari")));
    CHECK(menu.contains(QStringLiteral("&2 Kalahari")));
    CHECK(windowMenu(*second).filter(QStringLiteral(" Kalahari")).size() == 2);
}

TEST_CASE("Book windows: a window closed is gone, the first one stays",
          "[gui][windows][book-windows]") {
    const Windows windows;
    newWindowByMenu(*windows.first())->close();
    CHECK(waitFor([]() { return MainWindow::windows().size() == 1; }));
    CHECK(MainWindow::windows() == QList<MainWindow*>{windows.first()});
}

TEST_CASE("Book windows: the tab bars of the docked panels go after the parts of the window",
          "[gui][windows][book-windows]") {
    // Qt keeps the tab bars of the docked panels: a window that takes the layout of another
    // one has one no longer used, which Qt 6.8 and later deletes with the layout of the
    // window. Deleted with the parts of the window, it was deleted twice.
    const Windows windows;
    MainWindow* const second = secondWindow(windows);
    const QList<QTabBar*> bars = tabBarsWithUnused(*second);
    REQUIRE_FALSE(bars.isEmpty());

    // Each one goes once the window is no longer a main window: with its layout or after it
    TabBarsGone counts;
    countGone(second, bars, counts);
    delete second;
    CHECK(counts.gone == bars.size());
    CHECK(counts.beforeLayout == 0);
}

TEST_CASE("Book windows: another book opens in a new window, the first one keeps its book",
          "[gui][windows][book-windows]") {
    const TestBooks books;
    const Windows windows;
    MainWindow* first = windows.first();
    MainWindow* const second = withTwoBooks(first, books);
    CHECK(MainWindow::windows().size() == 2);
    CHECK(first->book().isProjectOpen());
    CHECK(QFileInfo(first->book().getManifestPath()) == QFileInfo(books.island()));
    CHECK(second->windowTitle() == QStringLiteral("Desert - Kalahari"));
}

TEST_CASE("Book windows: the Window menu names the books of the windows",
          "[gui][windows][book-windows]") {
    const TestBooks books;
    const Windows windows;
    const QStringList menu = windowMenu(*withTwoBooks(windows.first(), books));
    CHECK(menu.contains(QStringLiteral("&1 Island")));
    CHECK(menu.contains(QStringLiteral("&2 Desert")));
}

TEST_CASE("Book windows: a book open in another window comes to the front with it, saying so",
          "[gui][windows][book-windows]") {
    const TestBooks books;
    const Windows windows;
    MainWindow* first = windows.first();
    MainWindow* const second = withTwoBooks(first, books);
    open(second, books.island());
    REQUIRE(saysAlreadyOpen(*first));
    CHECK(infoBarText(*first).contains(
        QStringLiteral("The book \"Island\" is already open – in this window.")));
    CHECK(MainWindow::windows().size() == 2);
    CHECK(QFileInfo(second->book().getManifestPath()) == QFileInfo(books.desert()));
    CHECK(MainWindow::frontWindow() == first);
}

TEST_CASE("Book windows: the book of a window, chosen again there, opens nothing",
          "[gui][windows][book-windows]") {
    const TestBooks books;
    const Windows windows;
    MainWindow* first = windows.first();
    REQUIRE(opened(first, books.island()) == first);
    open(first, books.island());
    QApplication::processEvents();
    CHECK(MainWindow::windows().size() == 1);
    CHECK(QFileInfo(first->book().getManifestPath()) == QFileInfo(books.island()));
}

TEST_CASE("Book windows: the book closes, so does what the info bar said of it",
          "[gui][windows][book-windows]") {
    const TestBooks books;
    const Windows windows;
    MainWindow* const first = windows.first();
    open(withTwoBooks(first, books), books.island());
    REQUIRE(saysAlreadyOpen(*first));
    first->book().closeProject(false);
    CHECK_FALSE(infoBarText(*first).contains(QStringLiteral("is already open")));
}

TEST_CASE("Book windows: the question Opening a Book, its texts", "[gui][windows][book-windows]") {
    const OpenBookDialog question(QStringLiteral("Island"), QStringLiteral("Desert"));
    CHECK(question.windowTitle() == OpenBookDialog::tr("Opening a Book"));
    CHECK(test::textsOf(question).contains(QStringLiteral(
        "This window has the book \"Island\" open. Where do you want to open the book "
        "\"Desert\"?")));
    CHECK(question.acceptButton()->text() == OpenBookDialog::tr("In a &New Window"));
    CHECK(question.acceptButton()->isDefault());
    CHECK(question.thisWindowButton()->text() == OpenBookDialog::tr("In &This Window"));
}

TEST_CASE("Book windows: the question Opening a Book, three answers and no Apply",
          "[gui][windows][book-windows]") {
    const OpenBookDialog question(QStringLiteral("Island"), QStringLiteral("Desert"));
    CHECK_FALSE(question.thisWindowButton()->isHidden());
    CHECK_FALSE(question.cancelButton()->isHidden());
    CHECK(question.applyButton()->isHidden());
    CHECK_FALSE(question.rememberBox()->isChecked());
    CHECK(question.choice() == OpenBookDialog::Choice::Cancel);
}

TEST_CASE("Book windows: the question Opening a Book, Cancel: nothing opens",
          "[gui][windows][book-windows]") {
    const TestBooks books("ask");
    const Windows windows;
    MainWindow* first = windows.first();
    REQUIRE(opened(first, books.island()) == first);
    CHECK(openAnswering(first, books.desert(), OpenBookDialog::Choice::Cancel) == 1);
    CHECK(MainWindow::windows().size() == 1);
    CHECK(QFileInfo(first->book().getManifestPath()) == QFileInfo(books.island()));
    CHECK(otherBookPlace() == "ask");
}

TEST_CASE("Book windows: the question Opening a Book, In a New Window, remembered",
          "[gui][windows][book-windows]") {
    const TestBooks books("ask");
    const Windows windows;
    MainWindow* first = windows.first();
    REQUIRE(opened(first, books.island()) == first);
    CHECK(openAnswering(first, books.desert(), OpenBookDialog::Choice::NewWindow, true) == 1);
    CHECK(MainWindow::windowOfBook(books.desert()) != nullptr);
    CHECK(MainWindow::windowOfBook(books.desert()) != first);
    CHECK(otherBookPlace() == "new");
}

TEST_CASE("Book windows: the question Opening a Book, an answer remembered is not asked again",
          "[gui][windows][book-windows]") {
    const TestBooks books("ask");
    const Windows windows;
    MainWindow* first = windows.first();
    REQUIRE(opened(first, books.island()) == first);
    REQUIRE(openAnswering(first, books.desert(), OpenBookDialog::Choice::NewWindow, true) == 1);
    CHECK(openAnswering(first, books.another(QStringLiteral("Forest")),
                        OpenBookDialog::Choice::NewWindow) == 0);
    CHECK(MainWindow::windows().size() == 3);
}

TEST_CASE("Book windows: the question Opening a Book, In This Window: the open book closes",
          "[gui][windows][book-windows]") {
    const TestBooks books("ask");
    const Windows windows;
    MainWindow* first = windows.first();
    REQUIRE(opened(first, books.island()) == first);
    CHECK(openAnswering(first, books.desert(), OpenBookDialog::Choice::ThisWindow) == 1);
    CHECK(MainWindow::windowOfBook(books.desert()) == first);
    CHECK(MainWindow::windowOfBook(books.island()) == nullptr);
    CHECK(otherBookPlace() == "ask");
}

TEST_CASE("Book windows: File > Exit, Cancel: every window stays", "[gui][windows][book-windows]") {
    const TestBooks books;
    const Windows windows;
    MainWindow* first = windows.first();
    MainWindow* second = withTwoBooks(first, books);
    first->book().setDirty(true);
    second->book().setDirty(true);
    QStringList said;
    bool closed = true;
    CHECK(test::answering([&]() { closed = MainWindow::closeAllWindows(second); },
                          MessageDialog::Answer::Cancel, said) == 1);
    CHECK_FALSE(closed);
    CHECK(MainWindow::windows().size() == 2);
    CHECK(second->isVisible());
}

TEST_CASE("Book windows: File > Exit asks one question that names every unsaved book",
          "[gui][windows][book-windows]") {
    const TestBooks books;
    const Windows windows;
    MainWindow* const first = windows.first();
    MainWindow* const second = withTwoBooks(first, books);
    first->book().setDirty(true);
    second->book().setDirty(true);
    QStringList said;
    CHECK(test::answering([second]() { (void)MainWindow::closeAllWindows(second); },
                          MessageDialog::Answer::Alternative, said) == 1);
    REQUIRE(said.size() == 1);
    CHECK(said.first().contains(QStringLiteral("Island")));
    CHECK(said.first().contains(QStringLiteral("Desert")));
}

TEST_CASE("Book windows: File > Exit keeps the books of every window for the next start",
          "[gui][windows][book-windows]") {
    const TestBooks books;
    const Windows windows;
    MainWindow* const second = withTwoBooks(windows.first(), books);
    windows.first()->book().setDirty(true);
    second->book().setDirty(true);
    // The book of the window that asked goes last: it is in front at the next start
    closeAllNotSaving(second);
    const QStringList open = keptOpenBooks();
    REQUIRE(open.size() == 2);
    CHECK(QFileInfo(open.at(0)) == QFileInfo(books.island()));
    CHECK(QFileInfo(open.at(1)) == QFileInfo(books.desert()));
}

TEST_CASE("Book windows: a window closed while another one is open is not the end of the work",
          "[gui][windows][book-windows]") {
    const TestBooks books;
    core::RecentBooksManager::setOpenBooks({books.island(), books.desert()});
    const Windows windows;
    MainWindow* first = windows.first();
    withTwoBooks(first, books);
    first->close();
    REQUIRE(waitFor([]() { return MainWindow::windows().size() == 1; }));
    CHECK(keptOpenBooks().size() == 2);
}

TEST_CASE("Book windows: the last window closed keeps its book for the next start",
          "[gui][windows][book-windows]") {
    const TestBooks books;
    core::RecentBooksManager::setOpenBooks({books.island(), books.desert()});
    const Windows windows;
    MainWindow* first = windows.first();
    MainWindow* second = withTwoBooks(first, books);
    first->close();
    REQUIRE(waitFor([]() { return MainWindow::windows().size() == 1; }));
    second->close();
    REQUIRE(waitFor([]() { return MainWindow::windows().isEmpty(); }));
    CHECK(keptOpenBooks() == QStringList{QFileInfo(books.desert()).absoluteFilePath()});
}

TEST_CASE("Book windows: at startup all the books of the last work open, each in its window",
          "[gui][windows][book-windows]") {
    const TestBooks books;
    lastWork(books);
    const Setting<bool> autoLoad("startup.autoLoadLastProject", true);
    const Setting<std::string> all(core::RecentBooksManager::STARTUP_BOOKS_KEY, "all");
    CHECK(core::RecentBooksManager::getInstance().booksToOpenAtStart() ==
          QStringList{books.island(), books.desert()});
    MainWindow* const first = startingWindow();
    const Windows windows;  // deletes the windows; its own first one opens nothing
    CHECK(waitForBook(books.island()) == first);
    MainWindow* const second = waitForBook(books.desert());
    CHECK(second != nullptr);
    CHECK(second != first);
    CHECK(MainWindow::windows().size() == 3);
}

TEST_CASE("Book windows: at startup only the book of the last work opened last",
          "[gui][windows][book-windows]") {
    const TestBooks books;
    lastWork(books);
    const Setting<bool> autoLoad("startup.autoLoadLastProject", true);
    const Setting<std::string> last(core::RecentBooksManager::STARTUP_BOOKS_KEY, "last");
    CHECK(core::RecentBooksManager::getInstance().booksToOpenAtStart() ==
          QStringList{books.desert()});
    MainWindow* const first = startingWindow();
    const Windows windows;
    CHECK(waitForBook(books.desert()) == first);
    QApplication::processEvents();
    CHECK(MainWindow::windowOfBook(books.island()) == nullptr);
    CHECK(MainWindow::windows().size() == 2);
}

TEST_CASE("Book windows: a book of the last work gone since does not open at startup",
          "[gui][windows][book-windows]") {
    const TestBooks books;
    lastWork(books);
    const Setting<std::string> all(core::RecentBooksManager::STARTUP_BOOKS_KEY, "all");
    const auto& recentBooks = core::RecentBooksManager::getInstance();
    core::RecentBooksManager::setOpenBooks(
        {books.pathOf(QStringLiteral("Gone/Gone.klh")), books.desert()});
    CHECK(recentBooks.booksToOpenAtStart() == QStringList{books.desert()});
    core::RecentBooksManager::setOpenBooks({});
    CHECK(recentBooks.booksToOpenAtStart() == QStringList{books.desert()});
}

TEST_CASE("Book windows: the settings applied in one window show in every window",
          "[gui][windows][book-windows]") {
    const Windows windows;
    MainWindow* const second = secondWindow(windows);
    auto* firstSettings = windows.first()->findChild<gui::SettingsCoordinator*>();
    REQUIRE(firstSettings != nullptr);
    CHECK(gui::SettingsCoordinator::coordinators().size() == 2);
    const Setting<std::string> rows(gui::CentralTabs::ROWS_KEY, gui::CentralTabs::ROWS_WRAP);
    firstSettings->onApplySettings({QString::fromLatin1(gui::CentralTabs::ROWS_KEY)});
    CHECK(centralRowsOf(*windows.first()) == gui::PanelTabBar::Rows::Wrap);
    CHECK(centralRowsOf(*second) == gui::PanelTabBar::Rows::Wrap);
}

TEST_CASE("Book windows: one grammar server for every window", "[gui][windows][book-windows]") {
    const Windows windows;
    const gui::GrammarCoordinator& secondGrammar = grammarOf(*secondWindow(windows));
    const gui::GrammarCoordinator& firstGrammar = grammarOf(*windows.first());
    CHECK(firstGrammar.server() == secondGrammar.server());
    CHECK(firstGrammar.setup() == secondGrammar.setup());
}

TEST_CASE("Book windows: a word added to the dictionary in one window is known in the other",
          "[gui][windows][book-windows]") {
    const Windows windows;
    const gui::SpellingCoordinator& second = spellingOf(*secondWindow(windows));
    const gui::SpellingCoordinator& first = spellingOf(*windows.first());
    const QString word = QStringLiteral("Kalaharix");
    REQUIRE_FALSE(second.service()->isInUserDictionary(word));
    first.service()->addToUserDictionary(word);
    CHECK(second.service()->isInUserDictionary(word));
    first.service()->removeFromUserDictionary(word);
    CHECK_FALSE(second.service()->isInUserDictionary(word));
}

TEST_CASE("Book windows: keys given to a character in one window work in the other",
          "[gui][windows][book-windows]") {
    const Windows windows;
    MainWindow* const second = secondWindow(windows);
    auto* characters = windows.first()->findChild<gui::SpecialCharacterCoordinator*>();
    REQUIRE(characters != nullptr);
    const gui::KeyboardShortcut keys(Qt::Key_F7, Qt::ControlModifier);
    characters->setCharacterKeys(U'→', keys);
    const QAction* action = second->commands().getAction(std::string("insert.character.2192"));
    REQUIRE(action != nullptr);
    CHECK(action->shortcut() == keys.toQKeySequence());
    CHECK(second->actions().contains(action));
    characters->setCharacterKeys(U'→', gui::KeyboardShortcut());
}

namespace {

/// The panel of the books, whose place in the layout of a window the tests follow
constexpr const char* NAVIGATOR = "NavigatorDock";

/// The Navigator of @p window, which every window has
QDockWidget* navigatorOf(const MainWindow& window) {
    auto* dock = window.findChild<QDockWidget*>(QString::fromLatin1(NAVIGATOR));
    REQUIRE(dock != nullptr);
    return dock;
}

/// The layout of @p window now
gui::WindowLayout layoutOf(MainWindow& window) {
    return gui::WindowLayout{.geometry = window.saveGeometry(), .state = window.saveState()};
}

/// Keep for the book @p manifest the layout of a window with the Navigator shown or not
void keepLayoutWithNavigator(const QString& manifest, bool shown) {
    MainWindow arranged;
    arranged.show();
    navigatorOf(arranged)->setVisible(shown);
    gui::keepBookWindowLayout(manifest, layoutOf(arranged));
}

/// Open the first text of the book of @p window in it
void openFirstText(MainWindow& window) {
    const core::ProjectManager& book = window.book();
    const core::BookProject* project = book.project();
    REQUIRE(project != nullptr);
    for (const core::ProjectElement* const element : project->readingOrder()) {
        if (book.formOf(*element) == core::ElementForm::Text) {
            window.openChapter(element->id, element->title);
            return;
        }
    }
    FAIL("The book has no text");
}

/// A book of the tests numbered @p number in @p dir (no book is made: only its path counts)
QString numberedBook(const QTemporaryDir& dir, std::size_t number) {
    return dir.filePath(QStringLiteral("Book %1/Book.klh").arg(number));
}

/// A layout of the tests numbered @p number, not a real one
gui::WindowLayout numberedLayout(std::size_t number) {
    const QByteArray bytes = QByteArray::number(static_cast<qulonglong>(number));
    return gui::WindowLayout{.geometry = bytes, .state = bytes + bytes};
}

/// The geometry kept for the book @p manifest; empty when it has no layout
QByteArray keptGeometry(const QString& manifest) {
    return gui::bookWindowLayout(manifest).value_or(gui::WindowLayout{}).geometry;
}

/// The state kept for the book @p manifest; empty when it has no layout
QByteArray keptState(const QString& manifest) {
    return gui::bookWindowLayout(manifest).value_or(gui::WindowLayout{}).state;
}

/// The layouts kept for the books now
nlohmann::json keptLayouts() {
    return core::SettingsManager::getInstance().get<nlohmann::json>(gui::BOOK_LAYOUTS_KEY);
}

}  // namespace

TEST_CASE("Book windows: the layout kept for a book, none for another one",
          "[gui][windows][book-windows]") {
    const KeptLayouts kept;
    const QTemporaryDir dir;
    gui::keepBookWindowLayout(numberedBook(dir, 1), numberedLayout(1));
    CHECK(keptGeometry(numberedBook(dir, 1)) == numberedLayout(1).geometry);
    CHECK(keptState(numberedBook(dir, 1)) == numberedLayout(1).state);
    CHECK_FALSE(gui::bookWindowLayout(numberedBook(dir, 2)).has_value());
    CHECK_FALSE(gui::bookWindowLayout(QString()).has_value());
}

TEST_CASE("Book windows: a book kept again has its new layout, once, first",
          "[gui][windows][book-windows]") {
    const KeptLayouts kept;
    const QTemporaryDir dir;
    gui::keepBookWindowLayout(numberedBook(dir, 1), numberedLayout(1));
    gui::keepBookWindowLayout(numberedBook(dir, 2), numberedLayout(2));
    gui::keepBookWindowLayout(numberedBook(dir, 1), numberedLayout(3));
    const nlohmann::json layouts = keptLayouts();
    CHECK(layouts.size() == 2);
    CHECK(QString::fromStdString(layouts.front().value("book", std::string())) ==
          QFileInfo(numberedBook(dir, 1)).absoluteFilePath());
    CHECK(keptGeometry(numberedBook(dir, 1)) == numberedLayout(3).geometry);
    CHECK(gui::bookWindowLayout(numberedBook(dir, 2)).has_value());
}

TEST_CASE("Book windows: the books whose windows closed the longest ago lose their layouts",
          "[gui][windows][book-windows]") {
    const KeptLayouts kept;
    const QTemporaryDir dir;
    for (std::size_t number = 0; number <= gui::MAX_BOOK_LAYOUTS; ++number) {
        gui::keepBookWindowLayout(numberedBook(dir, number), numberedLayout(number));
    }
    CHECK(keptLayouts().size() == gui::MAX_BOOK_LAYOUTS);
    CHECK_FALSE(gui::bookWindowLayout(numberedBook(dir, 0)).has_value());
    CHECK(gui::bookWindowLayout(numberedBook(dir, 1)).has_value());
    CHECK(gui::bookWindowLayout(numberedBook(dir, gui::MAX_BOOK_LAYOUTS)).has_value());
}

TEST_CASE("Book windows: layouts changed by hand, not an array", "[gui][windows][book-windows]") {
    const KeptLayouts kept;
    const QTemporaryDir dir;
    core::SettingsManager::getInstance().set<nlohmann::json>(gui::BOOK_LAYOUTS_KEY,
                                                             nlohmann::json::object());
    CHECK_FALSE(gui::bookWindowLayout(numberedBook(dir, 1)).has_value());
    gui::keepBookWindowLayout(numberedBook(dir, 1), numberedLayout(1));
    CHECK(keptLayouts().size() == 1);
}

TEST_CASE("Book windows: layouts changed by hand, what is not a layout of a book is left out",
          "[gui][windows][book-windows]") {
    const KeptLayouts kept;
    const QTemporaryDir dir;
    const QString book = numberedBook(dir, 1);
    core::SettingsManager::getInstance().set<nlohmann::json>(
        gui::BOOK_LAYOUTS_KEY,
        nlohmann::json::array({"Book", {{"book", 2}}, {{"geometry", "AA=="}},
                               {{"book", book.toStdString()}, {"geometry", 2}}}));
    // The layout of the book without its parts: empty ones
    CHECK(gui::bookWindowLayout(book).has_value());
    CHECK(keptGeometry(book).isEmpty());
    CHECK(keptState(book).isEmpty());
    gui::keepBookWindowLayout(numberedBook(dir, 2), numberedLayout(2));
    CHECK(keptLayouts().size() == 2);
    CHECK(gui::bookWindowLayout(book).has_value());
}

TEST_CASE("Book windows: a book keeps the layout of its window as the window closes",
          "[gui][windows][book-windows]") {
    const TestBooks books;
    const Windows windows;
    MainWindow* first = windows.first();
    open(first, books.island());
    REQUIRE(waitForBook(books.island()) == first);
    navigatorOf(*first)->hide();
    const gui::WindowLayout closing = layoutOf(*first);
    // A window without a book keeps nothing as it closes
    first->openNewWindow()->close();
    first->close();
    REQUIRE(waitFor([]() { return MainWindow::windows().isEmpty(); }));
    CHECK(keptState(books.island()) == closing.state);
    CHECK(keptGeometry(books.island()) == closing.geometry);
    CHECK(keptLayouts().size() == 1);
}

TEST_CASE("Book windows: a new window of a book takes the layout the book kept",
          "[gui][windows][book-windows]") {
    const TestBooks books;
    keepLayoutWithNavigator(books.island(), false);
    const Windows windows;
    MainWindow* first = windows.first();
    MainWindow* const island = first->openBookInNewWindow(books.island());
    REQUIRE(waitForBook(books.island()) == island);
    CHECK(navigatorOf(*island)->isHidden());
    // A book without a layout of its own takes the one of the window it came from
    MainWindow* const desert = first->openBookInNewWindow(books.desert());
    REQUIRE(waitForBook(books.desert()) == desert);
    CHECK_FALSE(navigatorOf(*desert)->isHidden());
}

TEST_CASE("Book windows: a book keeps the layout of its window as it leaves the window",
          "[gui][windows][book-windows]") {
    const TestBooks books;
    const Windows windows;
    MainWindow* first = windows.first();
    open(first, books.island());
    REQUIRE(waitForBook(books.island()) == first);
    navigatorOf(*first)->hide();
    first->book().closeProject(false);
    navigatorOf(*first)->show();
    MainWindow* const again = first->openBookInNewWindow(books.island());
    REQUIRE(waitForBook(books.island()) == again);
    CHECK(navigatorOf(*again)->isHidden());
    CHECK_FALSE(navigatorOf(*first)->isHidden());
}

TEST_CASE("Book windows: no layout is kept from Distraction-Free", "[gui][windows][book-windows]") {
    const TestBooks books;
    const Windows windows;
    MainWindow* first = windows.first();
    open(first, books.island());
    REQUIRE(waitForBook(books.island()) == first);
    openFirstText(*first);
    QAction* distractionFree =
        first->commands().getAction(std::string("view.mode.distraction-free"));
    REQUIRE(waitFor([distractionFree]() { return distractionFree->isEnabled(); }));
    distractionFree->trigger();
    REQUIRE(distractionFree->isChecked());
    first->book().closeProject(false);
    CHECK_FALSE(gui::bookWindowLayout(books.island()).has_value());
}

TEST_CASE("Book windows: at startup each book of the last work takes the layout it kept",
          "[gui][windows][book-windows]") {
    const TestBooks books;
    keepLayoutWithNavigator(books.island(), false);
    keepLayoutWithNavigator(books.desert(), true);
    core::RecentBooksManager::setOpenBooks({books.island(), books.desert()});
    const Setting<bool> autoLoad("startup.autoLoadLastProject", true);
    const Setting<std::string> all(core::RecentBooksManager::STARTUP_BOOKS_KEY, "all");
    MainWindow* const first = startingWindow();
    const Windows windows;  // deletes the windows; its own first one opens nothing
    REQUIRE(waitForBook(books.island()) == first);
    CHECK(navigatorOf(*first)->isHidden());
    MainWindow* const desert = waitForBook(books.desert());
    REQUIRE(desert != nullptr);
    CHECK_FALSE(navigatorOf(*desert)->isHidden());
}

TEST_CASE("Book windows: at startup the book of the command line takes the layout it kept",
          "[gui][windows][book-windows]") {
    const TestBooks books;
    keepLayoutWithNavigator(books.island(), false);
    MainWindow* const first = startingWindow(books.island());
    const Windows windows;
    REQUIRE(waitForBook(books.island()) == first);
    CHECK(navigatorOf(*first)->isHidden());
}
