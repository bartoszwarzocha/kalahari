/// @file test_navigator_expansion.cpp
/// @brief Navigator tree expansion state survives closing and reopening a book

#include <catch2/catch_test_macros.hpp>
#include <kalahari/gui/panels/navigator_panel.h>
#include <kalahari/core/document.h>
#include <kalahari/core/book.h>
#include <kalahari/core/part.h>
#include <kalahari/core/book_element.h>

#include <QTreeWidget>
#include <QTreeWidgetItem>

#include <memory>

using namespace kalahari;

namespace {

core::Document makeDocument() {
    core::Document doc("Test Book", "Author", "en");
    auto& book = doc.getBook();
    book.addFrontMatter(std::make_shared<core::BookElement>("title_page", "fm-001", "Title Page"));
    for (const auto& [partId, chapterId] : {std::pair{"part-001", "ch-001"}, std::pair{"part-002", "ch-002"}}) {
        auto part = std::make_shared<core::Part>(partId, partId);
        part->addChapter(std::make_shared<core::BookElement>("chapter", chapterId, chapterId));
        book.addPart(part);
    }
    return doc;
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
    const core::Document doc = makeDocument();
    const QString projectId = "test_navigator_expansion_project";

    gui::NavigatorPanel panel;
    auto* tree = panel.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    panel.loadDocument(doc);

    // Parts are expanded by default, front matter collapsed
    REQUIRE(findItem(tree, "part-001")->isExpanded());
    REQUIRE_FALSE(findSection(tree, "section_frontmatter")->isExpanded());

    findItem(tree, "part-001")->setExpanded(false);
    findSection(tree, "section_frontmatter")->setExpanded(true);
    panel.saveExpansionState(projectId);

    // Reopening the book rebuilds the tree with defaults, then restores the state
    panel.loadDocument(doc);
    panel.restoreExpansionState(projectId);

    CHECK_FALSE(findItem(tree, "part-001")->isExpanded());
    CHECK(findItem(tree, "part-002")->isExpanded());
    CHECK(findSection(tree, "section_frontmatter")->isExpanded());
    CHECK(findSection(tree, "section_body")->isExpanded());
}

TEST_CASE("Navigator restores a fully collapsed tree", "[gui][navigator]") {
    const core::Document doc = makeDocument();
    const QString projectId = "test_navigator_expansion_collapsed";

    gui::NavigatorPanel panel;
    auto* tree = panel.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    panel.loadDocument(doc);

    tree->collapseAll();
    panel.saveExpansionState(projectId);

    panel.loadDocument(doc);
    panel.restoreExpansionState(projectId);

    CHECK(panel.expandedItemIds().isEmpty());
}

TEST_CASE("Navigator keeps defaults when no state was saved", "[gui][navigator]") {
    gui::NavigatorPanel panel;
    auto* tree = panel.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    panel.loadDocument(makeDocument());

    panel.restoreExpansionState("test_navigator_expansion_never_saved");

    CHECK(findItem(tree, "part-001")->isExpanded());
    CHECK_FALSE(findSection(tree, "section_frontmatter")->isExpanded());
}
