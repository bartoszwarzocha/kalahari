/// @file test_recent_books_menu.cpp
/// @brief File > Recent Books submenu follows the recent books list

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/recent_books_manager.h>
#include <kalahari/gui/command.h>
#include <kalahari/gui/command_registry.h>
#include <kalahari/gui/recent_books_menu.h>

#include <QAction>
#include <QDir>
#include <QMenu>

using namespace kalahari;

namespace {

// Keeps the recent books of the test run as they were
class RecentBooksGuard {
public:
    RecentBooksGuard() : m_saved(core::RecentBooksManager::getInstance().getRecentFiles()) {
        core::RecentBooksManager::getInstance().clearRecentFiles();
    }
    ~RecentBooksGuard() {
        auto& recent = core::RecentBooksManager::getInstance();
        recent.clearRecentFiles();
        for (auto it = m_saved.crbegin(); it != m_saved.crend(); ++it) {
            recent.addRecentFile(*it);
        }
    }
    RecentBooksGuard(const RecentBooksGuard&) = delete;
    RecentBooksGuard& operator=(const RecentBooksGuard&) = delete;

private:
    QStringList m_saved;
};

QString bookPath(const QString& name) {
    return QDir::temp().absoluteFilePath(name + ".klh");
}

} // namespace

TEST_CASE("Recent Books menu: the books, numbered, and the list cleared", "[gui][recent]") {
    RecentBooksGuard guard;
    auto& recent = core::RecentBooksManager::getInstance();
    gui::RecentBooksMenu menu;

    SECTION("an empty list") {
        REQUIRE(menu.actions().size() == 1);
        CHECK_FALSE(menu.actions().first()->isEnabled());
    }

    SECTION("the menu follows the list, the latest book first") {
        recent.addRecentFile(bookPath("First"));
        recent.addRecentFile(bookPath("Second"));

        const QList<QAction*> actions = menu.actions();
        REQUIRE(actions.size() == 4);  // two books, a separator, Clear Recent Files
        CHECK(actions[0]->text() == QStringLiteral("&1. Second"));
        CHECK(actions[1]->text() == QStringLiteral("&2. First"));
        CHECK(actions[2]->isSeparator());

        QStringList chosen;
        QObject::connect(&menu, &gui::RecentBooksMenu::bookChosen,
                         [&chosen](const QString& path) { chosen.append(path); });
        actions[1]->trigger();
        CHECK(chosen == QStringList{bookPath("First")});

        actions[3]->trigger();
        CHECK(recent.getRecentFiles().isEmpty());
        CHECK(menu.actions().size() == 1);
    }
}

TEST_CASE("Recent Books menu: before Close Book whatever its text", "[gui][recent]") {
    // Regression: the submenu looked for "Close" in the items' text, so in the Polish
    // program ("Zamknij ksiazke") it went to the end of the File menu
    auto& registry = gui::CommandRegistry::getInstance();
    const bool registered = registry.isCommandRegistered("file.close");
    if (!registered) {
        gui::Command close;
        close.id = "file.close";
        close.label = "Zamknij ksiazke";
        close.execute = []() {};
        registry.registerCommand(close);
    }

    QMenu fileMenu;
    QAction* openBook = fileMenu.addAction(QStringLiteral("Otworz ksiazke..."));
    QAction* closeBook = registry.getAction(std::string("file.close"));
    REQUIRE(closeBook != nullptr);
    fileMenu.addAction(closeBook);
    QAction* exit = fileMenu.addAction(QStringLiteral("Zakoncz"));

    auto* recentBooks = new gui::RecentBooksMenu(&fileMenu);
    recentBooks->insertInto(&fileMenu);

    const QList<QAction*> actions = fileMenu.actions();
    REQUIRE(actions.size() == 4);
    CHECK(actions[0] == openBook);
    CHECK(actions[1] == recentBooks->menuAction());
    CHECK(actions[2] == closeBook);
    CHECK(actions[3] == exit);

    if (!registered) {
        registry.unregisterCommand("file.close");
    }
}
