/// @file test_navigator_expansion.cpp
/// @brief Navigator tree expansion state survives closing and reopening a book; the
///        elements' status shows in the program's language

#include <catch2/catch_test_macros.hpp>
#include <kalahari/gui/panels/navigator_panel.h>
#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_registry.h>

#include <QCoreApplication>
#include <QTranslator>
#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <utility>

using namespace kalahari;

namespace {

/// @brief Packages of the resources folder
const core::BookTypeRegistry& registry() {
    static const core::BookTypeRegistry packages = [] {
        core::BookTypeRegistry loaded;
        loaded.load({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
        return loaded;
    }();
    return packages;
}

core::ProjectElement element(const QString& id, const QString& kind, const QString& file = {}) {
    core::ProjectElement result;
    result.id = id;
    result.kind = core::KindReference{QStringLiteral("kalahari.base"), kind};
    result.title = id;
    result.file = file;
    result.status = file.isEmpty() ? QString() : QStringLiteral("draft");
    return result;
}

/// @brief A book with a title page and two parts of one chapter each
core::BookProject makeProject() {
    core::ProjectBook book;
    book.title = QStringLiteral("Test Book");
    book.frontElements.append(
        element("fm-001", QStringLiteral("title_page"), QStringLiteral("book/title_page_001.kchapter")));
    for (const auto& [partId, chapterId] :
         {std::pair{"part-001", "ch-001"}, std::pair{"part-002", "ch-002"}}) {
        core::ProjectElement part = element(partId, QStringLiteral("part"));
        part.elements.append(element(chapterId, QStringLiteral("chapter"),
                                     QStringLiteral("book/%1.kchapter").arg(chapterId)));
        book.mainElements.append(part);
    }

    core::BookProject project;
    project.type = core::ProjectType{QStringLiteral("kalahari.novel"), QStringLiteral("1.0"), {}};
    project.books.append(book);
    return project;
}

QTreeWidgetItem* findItem(QTreeWidget* tree, const QString& id) {
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        if ((*it)->data(0, Qt::UserRole).toString() == id) {
            return *it;
        }
    }
    return nullptr;
}

QTreeWidgetItem* findSection(QTreeWidget* tree, const QString& type) {
    for (QTreeWidgetItemIterator it(tree); *it; ++it) {
        if ((*it)->data(0, Qt::UserRole + 1).toString() == type) {
            return *it;
        }
    }
    return nullptr;
}

} // namespace

TEST_CASE("Navigator restores collapsed items expanded by default", "[gui][navigator]") {
    const core::BookProject project = makeProject();
    const QString projectId = "test_navigator_expansion_project";

    gui::NavigatorPanel panel;
    auto* tree = panel.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    panel.loadProject(project, registry());

    // Parts are expanded by default, front matter collapsed
    REQUIRE(findItem(tree, "part-001")->isExpanded());
    REQUIRE_FALSE(findSection(tree, "section_frontmatter")->isExpanded());

    findItem(tree, "part-001")->setExpanded(false);
    findSection(tree, "section_frontmatter")->setExpanded(true);
    panel.saveExpansionState(projectId);

    // Reopening the book rebuilds the tree with defaults, then restores the state
    panel.loadProject(project, registry());
    panel.restoreExpansionState(projectId);

    CHECK_FALSE(findItem(tree, "part-001")->isExpanded());
    CHECK(findItem(tree, "part-002")->isExpanded());
    CHECK(findSection(tree, "section_frontmatter")->isExpanded());
    CHECK(findSection(tree, "section_body")->isExpanded());
}

TEST_CASE("Navigator restores a fully collapsed tree", "[gui][navigator]") {
    const core::BookProject project = makeProject();
    const QString projectId = "test_navigator_expansion_collapsed";

    gui::NavigatorPanel panel;
    auto* tree = panel.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    panel.loadProject(project, registry());

    tree->collapseAll();
    panel.saveExpansionState(projectId);

    panel.loadProject(project, registry());
    panel.restoreExpansionState(projectId);

    CHECK(panel.expandedItemIds().isEmpty());
}

TEST_CASE("Navigator keeps defaults when no state was saved", "[gui][navigator]") {
    gui::NavigatorPanel panel;
    auto* tree = panel.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    panel.loadProject(makeProject(), registry());

    panel.restoreExpansionState("test_navigator_expansion_never_saved");

    CHECK(findItem(tree, "part-001")->isExpanded());
    CHECK_FALSE(findSection(tree, "section_frontmatter")->isExpanded());
}

namespace {

// Stands in for the Polish translation of the status names
class StatusTranslator : public QTranslator {
public:
    QString translate(const char* context, const char* sourceText, const char* /*disambiguation*/,
                      int /*n*/) const override {
        if (QString::fromLatin1(context) != QLatin1String("kalahari::gui::NavigatorPanel")) {
            return {};
        }
        const QString source = QString::fromLatin1(sourceText);
        if (source == QLatin1String("Draft")) {
            return QStringLiteral("Szkic");
        }
        if (source == QLatin1String("Revision")) {
            return QStringLiteral("Poprawki");
        }
        return {};
    }
    bool isEmpty() const override { return false; }
};

} // namespace

TEST_CASE("Navigator shows the elements' status in the program's language", "[gui][navigator]") {
    // Regression: the tree showed the stored code, e.g. "Chapter 1 [Draft]" in the Polish
    // program, while the "Set Status" menu named the same status "Szkic"
    core::ProjectElement part = element(QStringLiteral("part-001"), QStringLiteral("part"));
    int number = 0;
    for (const auto& [id, status] : {std::pair{"ch-001", "draft"}, std::pair{"ch-002", "revision"},
                                     std::pair{"ch-003", "final"}}) {
        core::ProjectElement chapter =
            element(QString::fromLatin1(id), QStringLiteral("chapter"),
                    QStringLiteral("book/%1.kchapter").arg(QString::fromLatin1(id)));
        chapter.title = QStringLiteral("Chapter %1").arg(++number);
        chapter.status = QString::fromLatin1(status);
        part.elements.append(chapter);
    }
    core::BookProject project = makeProject();
    project.books.first().mainElements = {part};

    StatusTranslator translator;
    QCoreApplication::installTranslator(&translator);
    struct RemoveTranslator {
        QTranslator* translator;
        ~RemoveTranslator() { QCoreApplication::removeTranslator(translator); }
    } removeTranslator{&translator};

    gui::NavigatorPanel panel;
    auto* tree = panel.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    panel.loadProject(project, registry());

    CHECK(findItem(tree, "ch-001")->text(0) == QStringLiteral("Chapter 1 [Szkic]"));
    CHECK(findItem(tree, "ch-002")->text(0) == QStringLiteral("Chapter 2 [Poprawki]"));
    CHECK(findItem(tree, "ch-003")->text(0) == QStringLiteral("Chapter 3"));
}
