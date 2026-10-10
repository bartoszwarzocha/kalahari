/// @file test_navigator_panel.cpp
/// @brief The Navigator's tree of a book project and the places elements are dragged to

#include <catch2/catch_test_macros.hpp>
#include <kalahari/gui/panels/navigator_panel.h>
#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_registry.h>

#include <QTreeWidget>
#include <QTreeWidgetItem>

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

/// @brief Element @p id of kind @p kind ("kalahari.base:chapter"), titled as its id
core::ProjectElement element(const QString& id, const QString& kind, const QString& file = {},
                             const QString& status = {}) {
    core::ProjectElement result;
    result.id = id;
    result.kind = *core::KindReference::parse(kind);
    result.title = id;
    result.file = file;
    result.status = status;
    return result;
}

/// @brief A novel with @p front, @p main and @p back elements
core::BookProject novel(const QList<core::ProjectElement>& front,
                        const QList<core::ProjectElement>& main,
                        const QList<core::ProjectElement>& back = {}) {
    core::ProjectBook book;
    book.title = QStringLiteral("Test Book");
    book.frontElements = front;
    book.mainElements = main;
    book.backElements = back;

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

QString typeOf(const QTreeWidgetItem* item) {
    return item->data(0, Qt::UserRole + 1).toString();
}

} // namespace

TEST_CASE("Navigator: the elements of a book in its three sections", "[gui][navigator]") {
    core::ProjectElement part = element("Part One", "kalahari.base:part");
    part.elements.append(
        element("Chapter 1", "kalahari.base:chapter", "book/chapter_001.kchapter", "draft"));
    const core::BookProject project = novel(
        {element("Motto", "kalahari.base:motto", "book/motto_001.kchapter", "final"),
         element("Contents", "kalahari.base:toc", "book/toc.json")},
        {element("Prologue", "kalahari.base:chapter", "book/chapter_002.kchapter", "revision"),
         part},
        {element("Afterword", "kalahari.base:afterword", "book/afterword_001.kchapter")});

    gui::NavigatorPanel panel;
    auto* tree = panel.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    panel.loadProject(project, registry());

    // The book, named by its title, with its sections
    REQUIRE(tree->topLevelItemCount() == 1);
    const QTreeWidgetItem* root = tree->topLevelItem(0);
    CHECK(root->text(0) == QStringLiteral("Test Book"));
    CHECK(typeOf(root) == QStringLiteral("document"));
    REQUIRE(root->childCount() == 3);
    CHECK(typeOf(root->child(0)) == QStringLiteral("section_frontmatter"));
    CHECK(typeOf(root->child(1)) == QStringLiteral("section_body"));
    CHECK(typeOf(root->child(2)) == QStringLiteral("section_backmatter"));

    // A chapter can be in the body itself or in a part; text elements show their status
    // unless it is final, a chapter without one is a draft
    const QTreeWidgetItem* body = root->child(1);
    REQUIRE(body->childCount() == 2);
    CHECK(body->child(0)->text(0) == QStringLiteral("Prologue [Revision]"));
    CHECK(typeOf(body->child(0)) == QStringLiteral("text_element"));
    CHECK(body->child(1)->text(0) == QStringLiteral("Part One"));
    CHECK(typeOf(body->child(1)) == QStringLiteral("group_element"));
    REQUIRE(body->child(1)->childCount() == 1);
    CHECK(body->child(1)->child(0)->text(0) == QStringLiteral("Chapter 1 [Draft]"));
    CHECK(body->child(1)->isExpanded());
    CHECK(findItem(tree, "Afterword")->text(0) == QStringLiteral("Afterword [Draft]"));

    // Elements of the front matter
    CHECK(findItem(tree, "Motto")->text(0) == QStringLiteral("Motto"));
    CHECK(typeOf(findItem(tree, "Contents")) == QStringLiteral("window_element"));

    // Elements are dragged; sections and parts take them
    CHECK(findItem(tree, "Prologue")->flags().testFlag(Qt::ItemIsDragEnabled));
    CHECK_FALSE(findItem(tree, "Prologue")->flags().testFlag(Qt::ItemIsDropEnabled));
    CHECK(findItem(tree, "Part One")->flags().testFlag(Qt::ItemIsDragEnabled));
    CHECK(findItem(tree, "Part One")->flags().testFlag(Qt::ItemIsDropEnabled));
    CHECK_FALSE(body->flags().testFlag(Qt::ItemIsDragEnabled));
    CHECK(body->flags().testFlag(Qt::ItemIsDropEnabled));
    CHECK_FALSE(root->flags().testFlag(Qt::ItemIsDropEnabled));

    // A chapter with unsaved changes keeps its mark when the tree is loaded again
    panel.setElementModified(QStringLiteral("Chapter 1"), true);
    panel.loadProject(project, registry());
    CHECK(findItem(tree, "Chapter 1")->text(0) == QStringLiteral("*Chapter 1 [Draft]"));
}

TEST_CASE("Navigator: the sections have the names the book gives them", "[gui][navigator]") {
    core::BookProject project = novel({}, {element("Chapter 1", "kalahari.base:chapter")},
                                      {element("Afterword", "kalahari.base:afterword")});
    gui::NavigatorPanel panel;
    auto* tree = panel.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    const auto names = [tree]() {
        const QTreeWidgetItem* root = tree->topLevelItem(0);
        QStringList list;
        for (int i = 0; i < root->childCount(); ++i) {
            list << root->child(i)->text(0);
        }
        return list.join(QStringLiteral(", ")).toStdString();
    };
    core::ProjectBook& book = project.books[0];

    // The first set, in the language of the book
    panel.loadProject(project, registry());
    CHECK(names() == "Front Section, Main Section, Back Section");
    book.language = QStringLiteral("pl");
    panel.loadProject(project, registry());
    CHECK(names() == "Sekcja początkowa, Sekcja główna, Sekcja końcowa");

    // Another set, and the writer's own names
    book.setSections({true, QStringLiteral("matter"), {}});
    panel.loadProject(project, registry());
    CHECK(names() == "Strony początkowe, Tekst główny, Strony końcowe");
    const QString custom = QString::fromLatin1(core::ProjectBook::CUSTOM_SECTIONS);
    book.setSections({true, custom,
                      {QStringLiteral("Wstęp"), QStringLiteral("Opowieść"),
                       QStringLiteral("Dodatki")}});
    panel.loadProject(project, registry());
    CHECK(names() == "Wstęp, Opowieść, Dodatki");

    // A renamed section stays expanded or collapsed
    findSection(tree, QStringLiteral("section_backmatter"))->setExpanded(true);
    findSection(tree, QStringLiteral("section_body"))->setExpanded(false);
    const QStringList expanded = panel.expandedItemIds();
    book.setSections({true, custom,
                      {QStringLiteral("Wstęp"), QStringLiteral("Historia"),
                       QStringLiteral("Aneksy")}});
    panel.loadProject(project, registry());
    panel.setExpandedItemIds(expanded);
    CHECK(names() == "Wstęp, Historia, Aneksy");
    CHECK(findSection(tree, QStringLiteral("section_backmatter"))->isExpanded());
    CHECK_FALSE(findSection(tree, QStringLiteral("section_body"))->isExpanded());
}

TEST_CASE("Navigator: a book without sections shows its elements one after another",
          "[gui][navigator]") {
    core::ProjectElement part = element("Part One", "kalahari.base:part");
    part.elements.append(element("Chapter 2", "kalahari.base:chapter"));
    core::BookProject project =
        novel({element("Title", "kalahari.base:title_page"),
               element("Dedication", "kalahari.base:dedication")},
              {element("Chapter 1", "kalahari.base:chapter"), part},
              {element("Afterword", "kalahari.base:afterword")});
    project.books[0].partsLayer = false;

    gui::NavigatorPanel panel;
    auto* tree = panel.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    panel.loadProject(project, registry());

    // No items of sections: the elements of the front, main and back section in reading order
    REQUIRE(tree->topLevelItemCount() == 1);
    const QTreeWidgetItem* root = tree->topLevelItem(0);
    QStringList ids;
    for (int i = 0; i < root->childCount(); ++i) {
        ids << root->child(i)->data(0, Qt::UserRole).toString();
    }
    CHECK(ids.join(QStringLiteral(", ")).toStdString() ==
          "Title, Dedication, Chapter 1, Part One, Afterword");
    CHECK(findSection(tree, QStringLiteral("section_body")) == nullptr);
    CHECK(findItem(tree, "Chapter 2")->parent() == findItem(tree, "Part One"));

    // The book's item takes the drops of its elements; an element is dragged only within its
    // section, and its new index is in the list of the section
    CHECK(root->flags().testFlag(Qt::ItemIsDropEnabled));
    const auto drop = [tree](const char* dragged, const char* target, bool below) {
        return gui::NavigatorPanel::dropIndexOf(findItem(tree, QString::fromLatin1(dragged)),
                                                findItem(tree, QString::fromLatin1(target)),
                                                below);
    };
    CHECK(drop("Part One", "Chapter 1", false) == 0);
    CHECK(drop("Chapter 1", "Part One", true) == 1);
    CHECK(drop("Dedication", "Title", false) == 0);
    CHECK(drop("Chapter 1", "Part One", false) == -1);   // where it is
    CHECK(drop("Chapter 1", "Dedication", true) == -1);  // another section
    CHECK(drop("Afterword", "Part One", true) == -1);
    CHECK(drop("Chapter 2", "Chapter 1", false) == -1);  // another list

    // With sections, the elements are in the items of their sections
    project.books[0].partsLayer = true;
    panel.loadProject(project, registry());
    CHECK(findItem(tree, "Chapter 1")->parent() ==
          findSection(tree, QStringLiteral("section_body")));
    CHECK(findItem(tree, "Title")->parent() ==
          findSection(tree, QStringLiteral("section_frontmatter")));
    CHECK_FALSE(tree->topLevelItem(0)->flags().testFlag(Qt::ItemIsDropEnabled));
    CHECK(drop("Part One", "Chapter 1", false) == 0);
}

TEST_CASE("Navigator: elements show the icons of their kinds", "[gui][navigator]") {
    const core::ProjectElement motto =
        element("Motto", "kalahari.base:motto", "book/motto_001.kchapter");
    const core::ProjectElement chapter =
        element("Chapter 1", "kalahari.base:chapter", "book/chapter_001.kchapter");
    const core::ProjectElement part = element("Part One", "kalahari.base:part");

    CHECK(gui::NavigatorPanel::iconIdOf(registry(), motto) == QStringLiteral("format.style.quote"));
    CHECK(gui::NavigatorPanel::iconIdOf(registry(), chapter) == QStringLiteral("template.chapter"));
    CHECK(gui::NavigatorPanel::iconIdOf(registry(), part) == QStringLiteral("structure.part"));
}

TEST_CASE("Navigator: elements of a package that is not installed stay in the tree",
          "[gui][navigator]") {
    // A package of thrillers that this installation does not have
    core::ProjectElement arc = element("Arc", "acme.thriller:arc");
    arc.elements.append(
        element("Scene", "acme.thriller:scene", "book/scene_001.kchapter", "final"));
    const core::BookProject project =
        novel({}, {arc, element("Board", "acme.thriller:board", "book/board.json")});

    gui::NavigatorPanel panel;
    auto* tree = panel.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    panel.loadProject(project, registry());

    // Their form comes from their files: a chapter file is a text, no file a group
    REQUIRE(findItem(tree, "Arc") != nullptr);
    REQUIRE(findItem(tree, "Scene") != nullptr);
    REQUIRE(findItem(tree, "Board") != nullptr);
    CHECK(typeOf(findItem(tree, "Arc")) == QStringLiteral("group_element"));
    CHECK(findItem(tree, "Scene")->parent() == findItem(tree, "Arc"));
    CHECK(typeOf(findItem(tree, "Scene")) == QStringLiteral("text_element"));
    CHECK(typeOf(findItem(tree, "Board")) == QStringLiteral("window_element"));
    CHECK(findSection(tree, "section_body")->childCount() == 2);

    CHECK(gui::NavigatorPanel::iconIdOf(registry(), arc) == QStringLiteral("structure.part"));
    CHECK(gui::NavigatorPanel::iconIdOf(registry(), arc.elements.first()) ==
          QStringLiteral("template.chapter"));
    CHECK(gui::NavigatorPanel::iconIdOf(registry(), project.books.first().mainElements.last()) ==
          QStringLiteral("common.file"));
}

TEST_CASE("Navigator: the place in its list an element is dragged to", "[gui][navigator]") {
    // A list of four elements: A B C D
    CHECK(gui::NavigatorPanel::dropIndex(0, 2, true) == 2);    // A below C: B C A D
    CHECK(gui::NavigatorPanel::dropIndex(0, 2, false) == 1);   // A above C: B A C D
    CHECK(gui::NavigatorPanel::dropIndex(3, 0, false) == 0);   // D above A: D A B C
    CHECK(gui::NavigatorPanel::dropIndex(3, 1, true) == 2);    // D below B: A B D C
    CHECK(gui::NavigatorPanel::dropIndex(0, 3, true) == 3);    // A below D: B C D A

    // Next to itself, an element stays where it is
    CHECK(gui::NavigatorPanel::dropIndex(1, 2, false) == -1);  // B above C
    CHECK(gui::NavigatorPanel::dropIndex(1, 0, true) == -1);   // B below A
}
