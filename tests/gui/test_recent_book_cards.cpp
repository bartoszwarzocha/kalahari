/// @file test_recent_book_cards.cpp
/// @brief The cards of the recent books on the Dashboard show the data of each book

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/recent_books_manager.h>
#include <kalahari/gui/panels/dashboard_panel.h>

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QTemporaryDir>

#include <algorithm>
#include <cstdio>

using namespace kalahari;

namespace {

/// Keeps the recent books of the test run as they were, with the books of a test in the list
class RecentBooks {
public:
    explicit RecentBooks(const QStringList& paths)
        : m_saved(core::RecentBooksManager::getInstance().getRecentFiles()) {
        auto& recent = core::RecentBooksManager::getInstance();
        recent.clearRecentFiles();
        // The book added last is the first in the list
        for (auto it = paths.crbegin(); it != paths.crend(); ++it) {
            recent.addRecentFile(*it);
        }
    }
    ~RecentBooks() {
        try {
            auto& recent = core::RecentBooksManager::getInstance();
            recent.clearRecentFiles();
            for (auto it = m_saved.crbegin(); it != m_saved.crend(); ++it) {
                recent.addRecentFile(*it);
            }
        } catch (...) {
            // A destructor may not throw: the later tests then find the books of this one
            std::fputs("RecentBooks: the recent books of the test run are lost\n", stderr);
        }
    }
    RecentBooks(const RecentBooks&) = delete;
    RecentBooks& operator=(const RecentBooks&) = delete;

private:
    QStringList m_saved;
};

/// A project of format 2 with one book, @p title by @p author, of the type @p typeId (none: a
/// user project)
core::BookProject project(const QString& typeId, const QString& title, const QString& author) {
    core::BookProject project;
    project.id = QStringLiteral("project-") + typeId;
    if (!typeId.isEmpty()) {
        core::ProjectType type;
        type.id = typeId;
        type.version = QStringLiteral("1.0");
        project.type = type;
    }
    core::ProjectBook book;
    book.id = QStringLiteral("book");
    book.folder = QString::fromLatin1(core::BookProject::BOOK_FOLDER);
    book.title = title;
    book.author = author;
    project.books.append(book);
    return project;
}

/// Write @p json to the file @p path
void writeJson(const QString& path, const QByteArray& json) {
    QFile file(path);
    REQUIRE(file.open(QIODevice::WriteOnly));
    REQUIRE(file.write(json) == json.size());
}

/// What the card of a book shows: its title, the line under it and the icon
struct Card {
    QString title;
    QString description;
    QString iconId;
};

/// The card of the book @p path on @p panel
Card cardOf(const gui::DashboardPanel& panel, const QString& path) {
    const QList<QWidget*> cards = panel.findChildren<QWidget*>(QStringLiteral("recentFileCard"));
    const auto card = std::find_if(cards.cbegin(), cards.cend(), [&path](const QWidget* widget) {
        return widget->property("filePath").toString() == path;
    });
    REQUIRE(card != cards.cend());
    const auto* title = (*card)->findChild<QLabel*>(QStringLiteral("cardTitle"));
    const auto* description = (*card)->findChild<QLabel*>(QStringLiteral("cardDescription"));
    const auto* icon = (*card)->findChild<QLabel*>(QStringLiteral("cardBookIcon"));
    REQUIRE(title != nullptr);
    REQUIRE(description != nullptr);
    REQUIRE(icon != nullptr);
    return {title->text(), description->text(), icon->property("iconId").toString()};
}

/// Whether @p panel has a label with @p text
bool hasLabel(const gui::DashboardPanel& panel, const QString& text) {
    const QList<QLabel*> labels = panel.findChildren<QLabel*>();
    return std::any_of(labels.cbegin(), labels.cend(),
                       [&text](const QLabel* label) { return label->text() == text; });
}

} // namespace

TEST_CASE("Dashboard: the card of a recent book shows its title, author and type",
          "[gui][dashboard]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QDir root(dir.path());

    // Books of format 2: of a type, a user project, and of a type that is not installed
    const QString stories = root.filePath(QStringLiteral("Stories.klh"));
    REQUIRE(project(QStringLiteral("kalahari.short_stories"), QStringLiteral("Night Trains"),
                    QStringLiteral("Anna Nowak"))
                .save(stories));
    const QString own = root.filePath(QStringLiteral("Own.klh"));
    REQUIRE(project(QString(), QStringLiteral("Letters"), QString()).save(own));
    const QString saga = root.filePath(QStringLiteral("Saga.klh"));
    REQUIRE(project(QStringLiteral("acme.saga"), QString(), QStringLiteral("Jan Kowalski"))
                .save(saga));

    // A file of the earlier format, with the data of the book in "document"
    const QString earlier = root.filePath(QStringLiteral("Earlier.klh"));
    QJsonObject document;
    document.insert(QStringLiteral("title"), QStringLiteral("Old Times"));
    document.insert(QStringLiteral("author"), QStringLiteral("Ewa Lis"));
    document.insert(QStringLiteral("genre"), QStringLiteral("poetry"));
    QJsonObject json;
    json.insert(QStringLiteral("document"), document);
    writeJson(earlier, QJsonDocument(json).toJson());

    // A file that is not a book
    const QString broken = root.filePath(QStringLiteral("Broken.klh"));
    writeJson(broken, QByteArray("{ not a book"));

    const RecentBooks recent({stories, own, saga, earlier, broken});
    const gui::DashboardPanel panel;
    CHECK(hasLabel(panel, QStringLiteral("Recent Books")));

    // The name and the icon of the type come from its package
    Card card = cardOf(panel, stories);
    CHECK(card.title == QStringLiteral("Night Trains"));
    CHECK(card.description == QStringLiteral("Anna Nowak | Short Story Collection"));
    CHECK(card.iconId == QStringLiteral("template.shortStories"));

    // A user project has the icon it has in the New Book window
    card = cardOf(panel, own);
    CHECK(card.title == QStringLiteral("Letters"));
    CHECK(card.description == QStringLiteral("Unknown Author | User Project"));
    CHECK(card.iconId == QStringLiteral("template.empty"));

    // Without a title the card has the name of the file
    card = cardOf(panel, saga);
    CHECK(card.title == QStringLiteral("Saga"));
    CHECK(card.description == QStringLiteral("Jan Kowalski | acme.saga (not installed)"));
    CHECK(card.iconId == QStringLiteral("template.novel"));

    card = cardOf(panel, earlier);
    CHECK(card.title == QStringLiteral("Old Times"));
    CHECK(card.description == QStringLiteral("Ewa Lis | Poetry"));
    CHECK(card.iconId == QStringLiteral("template.poetry"));

    card = cardOf(panel, broken);
    CHECK(card.title == QStringLiteral("Broken"));
    CHECK(card.description == QStringLiteral("Unknown Author | Novel"));
    CHECK(card.iconId == QStringLiteral("template.novel"));
}

TEST_CASE("Dashboard: without recent books the list says so", "[gui][dashboard]") {
    const RecentBooks recent({});
    const gui::DashboardPanel panel;
    CHECK(hasLabel(panel, QStringLiteral("Recent Books")));
    CHECK(hasLabel(panel, QStringLiteral("No recent books.")));
}
