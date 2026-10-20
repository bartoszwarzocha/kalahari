/// @file test_recent_book_cards.cpp
/// @brief The cards of the recent books on the Dashboard show the data of each book

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/icon_registry.h>
#include <kalahari/core/package_card.h>
#include <kalahari/core/recent_books_manager.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/core/theme_manager.h>
#include <kalahari/gui/kind_names.h>
#include <kalahari/gui/panels/dashboard_panel.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QTemporaryDir>
#include <QTextDocumentFragment>
#include <QToolButton>
#include <QTranslator>

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
    QString description;   ///< The text of the line, without its markup
    QString iconId;
    bool typeGreyed;       ///< The type in the line and the icon are greyed out
    QString descriptionMarkup;  ///< The line as the label has it
    bool hasIcon;          ///< The icon label shows an icon
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
    const QString text = description->textFormat() == Qt::RichText
                             ? QTextDocumentFragment::fromHtml(description->text()).toPlainText()
                             : description->text();
    const bool greyed = description->property("typeMissing").toBool();
    REQUIRE(icon->property("typeMissing").toBool() == greyed);
    return {title->text(), text, icon->property("iconId").toString(), greyed,
            description->text(), !icon->pixmap().isNull()};
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
    CHECK(card.description == QStringLiteral("Jan Kowalski | acme.saga (read-only)"));
    CHECK(card.iconId == QStringLiteral("template.novel"));
    CHECK(card.typeGreyed);

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

TEST_CASE("Dashboard: a book whose type the program does not have is named after its card, "
          "greyed out, read-only",
          "[gui][dashboard]") {
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QDir root(dir.path());
    const QString saga = root.filePath(QStringLiteral("Saga/Saga.klh"));
    REQUIRE(root.mkpath(QStringLiteral("Saga")));
    REQUIRE(project(QStringLiteral("acme.saga"), QStringLiteral("Winter"),
                    QStringLiteral("Jan Kowalski"))
                .save(saga));

    // The book's card of its type, a package of another computer, with an icon the program
    // does not have
    core::PackageCard type;
    type.id = QStringLiteral("acme.saga");
    type.version = QStringLiteral("2.0");
    type.name.values = {{QStringLiteral("en"), QStringLiteral("Family Saga")},
                        {QStringLiteral("pl"), QStringLiteral("Saga rodzinna")}};
    type.icon = QStringLiteral("acme.saga.tree");
    const QDir card(core::PackageCards::folderOf(root.filePath(QStringLiteral("Saga")),
                                                 QStringLiteral("acme.saga")));
    REQUIRE(root.mkpath(card.path()));
    writeJson(card.filePath(QStringLiteral("card.json")), QJsonDocument(type.toJson()).toJson());
    for (const QString& theme : core::IconRegistry::iconThemes()) {
        REQUIRE(card.mkpath(QStringLiteral("icons/") + theme));
        REQUIRE(QFile::copy(
            QStringLiteral(KALAHARI_SOURCE_DIR "/resources/icons/%1/account_tree.svg").arg(theme),
            card.filePath(QStringLiteral("icons/%1/acme.saga.tree.svg").arg(theme))));
    }
    REQUIRE_FALSE(core::IconRegistry::getInstance().hasIcon(QStringLiteral("acme.saga.tree")));

    const RecentBooks recent({saga});
    const gui::DashboardPanel panel;
    const Card shown = cardOf(panel, saga);
    CHECK(shown.title == QStringLiteral("Winter"));
    CHECK(shown.description == QStringLiteral("Jan Kowalski | Family Saga (read-only)"));
    CHECK(shown.iconId == QStringLiteral("acme.saga.tree"));
    CHECK(shown.hasIcon);

    // The type and its icon are greyed out; the author is not
    CHECK(shown.typeGreyed);
    const QString grey =
        core::ThemeManager::getInstance().getCurrentTheme().palette.placeholderText.name();
    CHECK(shown.descriptionMarkup.startsWith(QStringLiteral("Jan Kowalski | <span")));
    CHECK(shown.descriptionMarkup.contains(grey));

    // In Polish, the name of the type is the Polish one on its card (test report of
    // 17 October, part 2: "tylko do odczytu", not "nie ma w programie")
    {
        QTranslator polish;
        REQUIRE(polish.load(QStringLiteral(":/i18n/kalahari_pl.qm")));
        QCoreApplication::installTranslator(&polish);
        gui::setProgramLanguage(QStringLiteral("pl"));
        const gui::DashboardPanel polishPanel;
        const Card polishCard = cardOf(polishPanel, saga);
        gui::setProgramLanguage(QStringLiteral("en"));
        QCoreApplication::removeTranslator(&polish);
        CHECK(polishCard.description ==
              QStringLiteral("Jan Kowalski | Saga rodzinna (tylko do odczytu)"));
    }

    // A book of a type the program has is not greyed out
    QFile::remove(saga);
    REQUIRE(project(QStringLiteral("kalahari.novel"), QStringLiteral("Winter"),
                    QStringLiteral("Jan Kowalski"))
                .save(saga));
    const RecentBooks again({saga});
    const gui::DashboardPanel novelPanel;
    const Card novel = cardOf(novelPanel, saga);
    CHECK(novel.description == QStringLiteral("Jan Kowalski | Novel"));
    CHECK_FALSE(novel.typeGreyed);
    CHECK(novel.descriptionMarkup == novel.description);
}

TEST_CASE("Dashboard: the X of a recent book's card takes the book off the list, its file stays",
          "[gui][dashboard]") {
    // Test report of 17 October, part 18
    QTemporaryDir dir;
    REQUIRE(dir.isValid());
    const QDir root(dir.path());
    const QString rain = root.filePath(QStringLiteral("Rain/Rain.klh"));
    const QString snow = root.filePath(QStringLiteral("Snow/Snow.klh"));
    REQUIRE(root.mkpath(QStringLiteral("Rain")));
    REQUIRE(root.mkpath(QStringLiteral("Snow")));
    REQUIRE(project(QStringLiteral("kalahari.novel"), QStringLiteral("Rain"),
                    QStringLiteral("Anna Nowak"))
                .save(rain));
    REQUIRE(project(QStringLiteral("kalahari.novel"), QStringLiteral("Snow"),
                    QStringLiteral("Anna Nowak"))
                .save(snow));

    // "Rain" is the book opened last, so the one opened at startup
    auto& settings = core::SettingsManager::getInstance();
    const std::string lastBook = settings.get<std::string>("startup.lastBook");
    settings.set<std::string>("startup.lastBook", rain.toStdString());
    const RecentBooks recent({rain, snow});
    auto& books = core::RecentBooksManager::getInstance();
    REQUIRE(books.lastOpenedBook() == rain);

    gui::DashboardPanel panel;
    int opened = 0;
    QObject::connect(&panel, &gui::DashboardPanel::openRecentBookRequested,
                     [&opened](const QString&) { ++opened; });
    const auto cardsShown = [&panel]() {
        QStringList paths;
        for (const QWidget* card :
             panel.findChildren<QWidget*>(QStringLiteral("recentFileCard"))) {
            paths << card->property("filePath").toString();
        }
        return paths;
    };
    REQUIRE(cardsShown() == QStringList({rain, snow}));

    // Each card has its X, with a tooltip that says the file stays
    QToolButton* remove = nullptr;
    for (QWidget* card : panel.findChildren<QWidget*>(QStringLiteral("recentFileCard"))) {
        auto* button = card->findChild<QToolButton*>(QStringLiteral("recentBookRemove"));
        REQUIRE(button != nullptr);
        CHECK(button->toolTip() ==
              QStringLiteral("Remove from the recent books (the file stays on the disk)"));
        if (card->property("filePath").toString() == rain) {
            remove = button;
        }
    }
    REQUIRE(remove != nullptr);

    // The book leaves the list and its card, its file stays, and nothing opens; the book opened
    // at startup is now the first of the list
    remove->click();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK(books.getRecentFiles() == QStringList{snow});
    CHECK(cardsShown() == QStringList{snow});
    CHECK(QFileInfo::exists(rain));
    CHECK(opened == 0);
    CHECK(books.lastOpenedBook() == snow);
    settings.set<std::string>("startup.lastBook", lastBook);
}
