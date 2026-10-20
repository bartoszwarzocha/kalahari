/// @file test_paragraph_panel.cpp
/// @brief The Paragraph and Styles panel: the band of the paragraph at the cursor, the
///        buttons of its formatting and the palette of the styles of the book

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/paragraph_styles.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/panels/paragraph_panel.h"
#include "kalahari/gui/read_only_book.h"

#include <QAction>
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QToolBar>
#include <QToolButton>

using namespace kalahari;

namespace {

/// A style of a package, named in English, with its properties
core::PackageParagraphStyle packageStyle(const QString& id, const QString& name,
                                         const QVariantMap& properties = {}) {
    core::PackageParagraphStyle style;
    style.id = id;
    style.name.values.insert(QStringLiteral("en"), name);
    style.properties = properties;
    return style;
}

/// The KML of the tests: a heading, two paragraphs and a horizontal line
const QString KML = QStringLiteral("<kml><h1>Title</h1><p>Text</p><p>More</p><hr/></kml>");

/// An editor and the panel that follows it, side by side in a window
struct Window {
    QWidget window;
    editor::BookEditor* text = nullptr;
    gui::ParagraphPanel* panel = nullptr;

    Window() {
        gui::registerAllCommands(gui::CommandCallbacks{});
        auto* layout = new QHBoxLayout(&window);
        text = new editor::BookEditor(&window);
        text->setParagraphStyles(editor::ParagraphStyleSet::fromPackage(
            {packageStyle(QStringLiteral("normal"), QStringLiteral("Normal"),
                          {{QStringLiteral("fontSize"), 12}}),
             packageStyle(QStringLiteral("heading_1"), QStringLiteral("Heading 1"),
                          {{QStringLiteral("fontSize"), 24}, {QStringLiteral("bold"), true}}),
             packageStyle(QStringLiteral("quote"), QStringLiteral("Quote"),
                          {{QStringLiteral("italic"), true}})},
            QStringLiteral("en")));
        text->fromKml(KML);
        text->setCursorPosition({0, 1});
        layout->addWidget(text, 1);
        panel = new gui::ParagraphPanel(&window);
        layout->addWidget(panel);
        panel->setEditor(text);
        window.resize(900, 600);
    }

    [[nodiscard]] QListWidget* list() const {
        auto* found = panel->findChild<QListWidget*>(QStringLiteral("paragraphStyleList"));
        REQUIRE(found != nullptr);
        return found;
    }

    [[nodiscard]] QString label(const char* name) const {
        const auto* found = panel->findChild<QLabel*>(QLatin1String(name));
        REQUIRE(found != nullptr);
        return found->text();
    }

    [[nodiscard]] bool shows(const char* name) const {
        const auto* found = panel->findChild<QWidget*>(QLatin1String(name));
        REQUIRE(found != nullptr);
        return found->isVisibleTo(panel);
    }

    /// The id of the style selected in the palette; empty without one
    [[nodiscard]] QString selectedStyle() const {
        const QList<QListWidgetItem*> selected = list()->selectedItems();
        return selected.isEmpty() ? QString() : selected.first()->data(Qt::UserRole).toString();
    }

    /// The item of style @p id
    [[nodiscard]] QListWidgetItem* item(const QString& id) const {
        for (int i = 0; i < list()->count(); ++i) {
            if (list()->item(i)->data(Qt::UserRole).toString() == id) {
                return list()->item(i);
            }
        }
        FAIL("no item of the style");
        return nullptr;
    }
};

}  // anonymous namespace

TEST_CASE("Paragraph and Styles: the band names the paragraph at the cursor or the selection",
          "[gui][styles]") {
    Window window;
    CHECK(window.label("paragraphTitle") == QStringLiteral("Current Paragraph"));
    CHECK(window.label("paragraphPlace") == QStringLiteral("paragraph 1 of 4"));

    window.text->setCursorPosition({2, 0});
    window.panel->refresh();
    CHECK(window.label("paragraphPlace") == QStringLiteral("paragraph 3 of 4"));

    window.text->setSelection({{0, 1}, {2, 2}});
    window.panel->refresh();
    CHECK(window.label("paragraphTitle") == QStringLiteral("Selected Paragraphs"));
    CHECK(window.label("paragraphPlace") == QStringLiteral("3 of 4"));

    SECTION("it follows the cursor by itself, after the cursor stops, while it shows") {
        window.window.show();
        window.text->clearSelection();
        window.text->setCursorPosition({1, 0});
        test::waitUntil([&window] {
            return window.label("paragraphPlace") == QStringLiteral("paragraph 2 of 4");
        });
        CHECK(window.label("paragraphTitle") == QStringLiteral("Current Paragraph"));
        CHECK(window.label("paragraphPlace") == QStringLiteral("paragraph 2 of 4"));
    }
}

TEST_CASE("Paragraph and Styles: the buttons are the commands of the Format menu",
          "[gui][styles]") {
    Window window;
    gui::CommandRegistry& registry = gui::CommandRegistry::getInstance();
    const auto actionsOf = [&window](const char* name) {
        const auto* bar = window.panel->findChild<QToolBar*>(QLatin1String(name));
        REQUIRE(bar != nullptr);
        return bar->actions();
    };

    // Bold, italic and underline together, a line, then Clear Formatting
    const QList<QAction*> format = actionsOf("paragraphFormatBar");
    REQUIRE(format.size() == 5);
    CHECK(format.at(0) == registry.getAction(std::string("format.bold")));
    CHECK(format.at(1) == registry.getAction(std::string("format.italic")));
    CHECK(format.at(2) == registry.getAction(std::string("format.underline")));
    CHECK(format.at(3)->isSeparator());
    CHECK(format.at(4) == registry.getAction(std::string("format.clearFormatting")));

    const QList<QAction*> align = actionsOf("paragraphAlignBar");
    REQUIRE(align.size() == 4);
    CHECK(align.at(0) == registry.getAction(std::string("format.alignLeft")));
    CHECK(align.at(1) == registry.getAction(std::string("format.alignCenter")));
    CHECK(align.at(2) == registry.getAction(std::string("format.alignRight")));
    CHECK(align.at(3) == registry.getAction(std::string("format.justify")));
}

TEST_CASE("Paragraph and Styles: the palette shows the styles of the text, the current selected",
          "[gui][styles]") {
    Window window;
    QListWidget* list = window.list();

    QStringList names;
    for (int i = 0; i < list->count(); ++i) {
        names << list->item(i)->text();
    }
    CHECK(names == QStringList{QStringLiteral("Normal"), QStringLiteral("Heading 1"),
                               QStringLiteral("Quote")});
    CHECK(window.selectedStyle() == QStringLiteral("heading_1"));
    CHECK(list->isEnabled());
    CHECK_FALSE(window.shows("paragraphKind"));

    // Another paragraph: its style
    window.text->setCursorPosition({1, 0});
    window.panel->refresh();
    CHECK(window.selectedStyle() == QStringLiteral("normal"));

    // Paragraphs of two styles: none; of one: that one
    window.text->setSelection({{0, 1}, {1, 2}});
    window.panel->refresh();
    CHECK(window.selectedStyle().isEmpty());
    window.text->setSelection({{1, 1}, {2, 2}});
    window.panel->refresh();
    CHECK(window.selectedStyle() == QStringLiteral("normal"));

    // On a line: its kind, no style, and the palette greyed out
    window.text->clearSelection();
    window.text->setCursorPosition({3, 0});
    window.panel->refresh();
    CHECK(window.shows("paragraphKind"));
    CHECK(window.label("paragraphKind") == QStringLiteral("Horizontal Line"));
    CHECK(window.selectedStyle().isEmpty());
    CHECK_FALSE(list->isEnabled());
}

TEST_CASE("Paragraph and Styles: a click on a style gives it to the paragraphs, and the keys to "
          "the text",
          "[gui][styles]") {
    Window window;
    window.window.show();
    window.window.activateWindow();
    test::waitUntil([&window] { return QApplication::activeWindow() == &window.window; });
    window.text->setSelection({{1, 1}, {2, 2}});
    window.panel->refresh();
    window.list()->setFocus();

    emit window.list()->itemClicked(window.item(QStringLiteral("quote")));

    CHECK(window.text->toKml() ==
          QStringLiteral("<kml><h1>Title</h1><p style=\"quote\">Text</p><p style=\"quote\">More"
                         "</p><hr/></kml>"));
    CHECK(window.selectedStyle() == QStringLiteral("quote"));
    CHECK(window.window.focusWidget() == window.text);

    // One step of the undo history
    window.text->undo();
    CHECK(window.text->toKml() == KML);
}

TEST_CASE("Paragraph and Styles: the palette shows the styles by their names or in their look",
          "[gui][styles]") {
    Window window;
    const bool preview = window.panel->showsPreview();
    struct Back {
        gui::ParagraphPanel* panel;
        bool preview;
        ~Back() { panel->setShowsPreview(preview); }
    } back{window.panel, preview};

    auto* names = window.panel->findChild<QToolButton*>(QStringLiteral("styleNamesButton"));
    auto* look = window.panel->findChild<QToolButton*>(QStringLiteral("stylePreviewButton"));
    REQUIRE(names != nullptr);
    REQUIRE(look != nullptr);

    look->click();
    CHECK(window.panel->showsPreview());
    const QFont normal = window.item(QStringLiteral("normal"))->font();
    const QFont heading = window.item(QStringLiteral("heading_1"))->font();
    const QFont quote = window.item(QStringLiteral("quote"))->font();
    CHECK(heading.pointSizeF() > normal.pointSizeF());
    CHECK(heading.bold());
    CHECK_FALSE(normal.bold());
    CHECK(quote.italic());

    names->click();
    CHECK_FALSE(window.panel->showsPreview());
    CHECK(window.item(QStringLiteral("heading_1"))->font() == window.list()->font());
}

TEST_CASE("Paragraph and Styles: without a text, and with a read-only one", "[gui][styles]") {
    Window window;

    SECTION("without a text in front it says why it shows nothing") {
        window.panel->setEditor(nullptr);
        CHECK(window.shows("paragraphEmptyLabel"));
        CHECK(window.label("paragraphPlace").isEmpty());
        CHECK(window.list()->count() == 0);
        CHECK_FALSE(window.list()->isEnabled());
    }

    SECTION("a text closed while it is followed: it shows nothing, as without a text") {
        delete window.text;
        window.text = nullptr;
        CHECK(window.panel->editor() == nullptr);
        CHECK(window.shows("paragraphEmptyLabel"));
        CHECK(window.list()->count() == 0);
    }

    SECTION("a chapter of a book open read-only takes no style, and the palette says why") {
        window.text->setReadOnly(true);
        window.panel->refresh();
        CHECK_FALSE(window.shows("paragraphEmptyLabel"));
        CHECK_FALSE(window.list()->isEnabled());
        CHECK(window.list()->toolTip() == gui::readOnlyReason());
    }
}

TEST_CASE("Paragraph and Styles: in the Properties panel it folds to its band", "[gui][styles]") {
    Window window;
    auto* fold = window.panel->findChild<QToolButton*>(QStringLiteral("paragraphFoldButton"));
    REQUIRE(fold != nullptr);
    CHECK_FALSE(fold->isVisibleTo(window.panel));

    window.panel->setFoldable(true);
    CHECK(fold->isVisibleTo(window.panel));
    QList<bool> folded;
    QObject::connect(window.panel, &gui::ParagraphPanel::foldedChanged,
                     [&folded](bool value) { folded.append(value); });

    fold->click();
    CHECK(window.panel->isFolded());
    CHECK(folded == QList<bool>{true});
    CHECK(window.shows("paragraphBand"));
    CHECK_FALSE(window.shows("paragraphStyleList"));
    CHECK_FALSE(window.shows("paragraphFormatBar"));

    fold->click();
    CHECK_FALSE(window.panel->isFolded());
    CHECK(window.shows("paragraphStyleList"));
}
