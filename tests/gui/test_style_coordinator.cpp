/// @file test_style_coordinator.cpp
/// @brief The paragraph styles of the book changed from Format > Text Style: a new style, a
///        style of the type changed and reset, a deleted style whose paragraphs go to its base
///        in the open and the saved texts, the undo of the last change, and the commands

#include <catch2/catch_test_macros.hpp>
#include "document_window.h"
#include "kalahari/core/book_project.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/paragraph_styles.h"
#include "kalahari/gui/book_styles.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/style_coordinator.h"
#include "kalahari/gui/style_draft.h"

#include <QAction>
#include <QStringList>

#include <functional>

using namespace kalahari;
using gui::StyleCoordinator;
using gui::StyleDraft;
using test::OpenNovel;
using test::Window;

namespace {

/// A text of paragraphs in styles: (style, text); an empty style is Normal
QString kmlOf(const QList<std::pair<QString, QString>>& paragraphs) {
    QString kml = QStringLiteral("<kml>");
    for (const auto& [style, text] : paragraphs) {
        kml += style.isEmpty() ? QStringLiteral("<p>")
                               : QStringLiteral("<p style=\"%1\">").arg(style);
        kml += text + QStringLiteral("</p>");
    }
    return kml + QStringLiteral("</kml>");
}

/// The styles of the paragraphs of an editor, in their order
QStringList stylesOf(editor::BookEditor& text) {
    QStringList styles;
    const int count = text.textDocument()->blockCount();
    for (int i = 0; i < count; ++i) {
        text.setCursorPosition({i, 0});
        styles.append(text.currentParagraphStyle());
    }
    return styles;
}

/// The coordinator of the styles over the tabs of a window, as the main window has it
struct Styles {
    QStringList notices;
    StyleCoordinator coordinator;

    explicit Styles(Window& window)
        : coordinator(
              window.tabs,
              [this](gui::EditorPanel*, const QString& message, const QString&, const QString&,
                     std::function<void()>) { notices.append(message); },
              &window.statusBar, &window.window) {}
};

/// Memory, a style of the writer's own based on Quote, kept in the open book
QString addMemory(StyleCoordinator& styles) {
    StyleDraft draft = StyleDraft::newStyle(styles.styles(), QStringLiteral("quote"),
                                            {{QStringLiteral("fontSize"), 11}},
                                            QStringLiteral("Memory"));
    return styles.saveDraft(draft);
}

}  // namespace

TEST_CASE("Style coordinator: a new style of the book, kept and undone", "[gui][styles]") {
    OpenNovel book(QStringLiteral("Styles"));
    Window window;
    Styles styles(window);
    int changed = 0;
    QObject::connect(&styles.coordinator, &StyleCoordinator::stylesChanged,
                     [&changed]() { ++changed; });

    CHECK(styles.coordinator.whyNotChange().isEmpty());
    CHECK(styles.coordinator.typeName() == QStringLiteral("Novel"));
    CHECK_FALSE(styles.coordinator.canUndo());

    // It gets an id of its name, and the book keeps it in its file
    const QString id = addMemory(styles.coordinator);
    CHECK(id == QStringLiteral("memory"));
    CHECK(changed == 1);
    const editor::ParagraphStyle* memory = styles.coordinator.styles().find(id);
    REQUIRE(memory != nullptr);
    CHECK(memory->name == QStringLiteral("Memory"));
    CHECK(memory->origin == editor::StyleOrigin::Own);
    CHECK(memory->baseStyle == QStringLiteral("quote"));
    CHECK(memory->properties.value(QStringLiteral("italic")).toBool());  // Quote's
    CHECK(book.pm.paragraphStyles().size() == 1);
    CHECK(gui::bookParagraphStyles().contains(id));

    // A second one of the same name gets another id
    StyleDraft other = StyleDraft::newStyle(styles.coordinator.styles(), QStringLiteral("normal"),
                                            {}, QStringLiteral("Memory"));
    CHECK(other.problem() == QStringLiteral("The book has a style \"Memory\" already."));
    CHECK(styles.coordinator.saveDraft(other).isEmpty());

    // Undo takes it away
    REQUIRE(styles.coordinator.canUndo());
    CHECK(styles.coordinator.undoText() == QStringLiteral("adding the style \"Memory\""));
    CHECK(styles.coordinator.undo());
    CHECK_FALSE(styles.coordinator.styles().contains(id));
    CHECK(book.pm.paragraphStyles().isEmpty());
    CHECK_FALSE(styles.coordinator.canUndo());
    CHECK(changed == 2);
}

TEST_CASE("Style coordinator: a style of the type changed in the book and reset",
          "[gui][styles]") {
    OpenNovel book(QStringLiteral("Styles"));
    Window window;
    Styles styles(window);

    // A style of the type cannot be deleted; as the type has it, it cannot be reset
    CHECK_FALSE(styles.coordinator.whyNotDelete(QStringLiteral("quote")).isEmpty());
    CHECK_FALSE(styles.coordinator.whyNotDelete(QStringLiteral("normal")).isEmpty());
    CHECK(styles.coordinator.whyNotReset(QStringLiteral("quote")) ==
          QStringLiteral("\"Quote\" is as the type \"Novel\" has it"));
    CHECK(styles.coordinator.removeStyle(QStringLiteral("quote")) == -1);

    StyleDraft quote = StyleDraft::existing(styles.coordinator.styles(), gui::bookTypeStyles(),
                                            QStringLiteral("quote"));
    quote.setValue(QStringLiteral("italic"), false);
    CHECK(styles.coordinator.saveDraft(quote) == QStringLiteral("quote"));
    CHECK(styles.coordinator.styles().find(QStringLiteral("quote"))->origin ==
          editor::StyleOrigin::Changed);
    CHECK_FALSE(
        styles.coordinator.styles().find(QStringLiteral("quote"))->properties.value(
            QStringLiteral("italic")).toBool());
    CHECK(styles.coordinator.undoText() == QStringLiteral("changing the style \"Quote\""));

    // Reset: as the type has it, with a notice and Undo under the text in front; without a
    // text, the status bar says it
    CHECK(styles.coordinator.whyNotReset(QStringLiteral("quote")).isEmpty());
    CHECK(styles.coordinator.resetStyle(QStringLiteral("quote")));
    CHECK(styles.coordinator.styles().find(QStringLiteral("quote"))->origin ==
          editor::StyleOrigin::Type);
    CHECK(book.pm.paragraphStyles().isEmpty());
    CHECK(styles.notices.isEmpty());
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("The style \"Quote\" is as the type \"Novel\" has it again."));
    REQUIRE(styles.coordinator.canUndo());
    CHECK(styles.coordinator.undo());
    CHECK(styles.coordinator.styles().find(QStringLiteral("quote"))->origin ==
          editor::StyleOrigin::Changed);

    // A style of the writer's own has nothing to reset to
    const QString memory = addMemory(styles.coordinator);
    CHECK_FALSE(styles.coordinator.whyNotReset(memory).isEmpty());
    CHECK_FALSE(styles.coordinator.resetStyle(memory));
}

TEST_CASE("Style coordinator: the paragraphs of a deleted style go to its base",
          "[gui][styles]") {
    OpenNovel book(QStringLiteral("Styles"));
    Window window;
    Styles styles(window);
    const QString memory = addMemory(styles.coordinator);
    REQUIRE(memory == QStringLiteral("memory"));

    // A text open with two paragraphs of it, a saved one with one
    const QString open = book.chapterId;
    const QString saved = book.pm.addElement(book.pm.chapterKindFor(core::BookPlace::Main),
                                             QStringLiteral("The Calm"), core::BookPlace::Main);
    REQUIRE_FALSE(saved.isEmpty());
    const QString savedText =
        kmlOf({{QString(), QStringLiteral("Morning.")}, {memory, QStringLiteral("The river.")}});
    book.pm.setChapterContent(saved, savedText);
    REQUIRE(book.pm.saveChapterContent(saved));
    window.navigator.onElementSelected(open, QStringLiteral("The Storm"));
    editor::BookEditor* text = window.editor(window.tabs->currentIndex())->getBookEditor();
    text->replaceWithKml(kmlOf({{memory, QStringLiteral("One.")},
                                {QString(), QStringLiteral("Two.")},
                                {memory, QStringLiteral("Three.")}}));
    REQUIRE(stylesOf(*text) == QStringList{memory, QStringLiteral("normal"), memory});

    const QHash<QString, StyleCoordinator::Usage> used = styles.coordinator.usage();
    CHECK(used.value(memory) == StyleCoordinator::Usage{3, 2});
    CHECK(styles.coordinator.whyNotDelete(memory).isEmpty());

    // Deleted: its paragraphs are Quote's, in the open text and in the saved one
    CHECK(styles.coordinator.removeStyle(memory) == 3);
    CHECK_FALSE(styles.coordinator.styles().contains(memory));
    CHECK(stylesOf(*text) ==
          QStringList{QStringLiteral("quote"), QStringLiteral("normal"), QStringLiteral("quote")});
    CHECK(book.pm.chapterText(saved).contains(QStringLiteral("style=\"quote\"")));
    CHECK_FALSE(book.pm.chapterText(saved).contains(QStringLiteral("memory")));
    CHECK(styles.coordinator.usage().value(QStringLiteral("quote")) ==
          StyleCoordinator::Usage{3, 2});
    CHECK(styles.coordinator.undoText() == QStringLiteral("deleting the style \"Memory\""));

    // Undo: the style, its paragraphs in both texts
    REQUIRE(styles.coordinator.canUndo());
    CHECK(styles.coordinator.undo());
    CHECK(styles.coordinator.styles().contains(memory));
    CHECK(stylesOf(*text) == QStringList{memory, QStringLiteral("normal"), memory});
    CHECK(book.pm.chapterText(saved) == savedText);

    // Deleted again, and the open text written in: the deletion is final
    CHECK(styles.coordinator.removeStyle(memory) == 3);
    REQUIRE(styles.coordinator.canUndo());
    text->setCursorPosition({1, 0});
    text->insertText(QStringLiteral("x"));
    CHECK_FALSE(styles.coordinator.canUndo());
    CHECK_FALSE(styles.coordinator.undo());
    CHECK_FALSE(styles.coordinator.styles().contains(memory));
}

TEST_CASE("Style coordinator: Delete Style asks when paragraphs use the style",
          "[gui][styles]") {
    OpenNovel book(QStringLiteral("Styles"));
    Window window;
    Styles styles(window);
    const QString memory = addMemory(styles.coordinator);
    window.navigator.onElementSelected(book.chapterId, QStringLiteral("The Storm"));
    editor::BookEditor* text = window.editor(window.tabs->currentIndex())->getBookEditor();
    text->replaceWithKml(kmlOf({{memory, QStringLiteral("One.")}}));

    // The question says what happens to the paragraph; the notice under the text says it was
    // done, with Undo
    QStringList said;
    CHECK(test::accepting([&]() { CHECK(styles.coordinator.deleteStyle(memory)); }, &said) == 1);
    REQUIRE(said.size() == 1);
    CHECK(said.first().contains(QStringLiteral("Delete the Style \"Memory\"?")));
    // (The English words of a number are those of the source: "paragraph(s)")
    CHECK(said.first().contains(QStringLiteral("1 paragraph(s) in 1 text(s) use it.")));
    CHECK(said.first().contains(QStringLiteral("It goes to the style \"Quote\"")));
    CHECK(styles.notices == QStringList{QStringLiteral(
                                "The style \"Memory\" is deleted: 1 paragraph(s) now have the "
                                "style \"Quote\".")});
    CHECK_FALSE(styles.coordinator.styles().contains(memory));

    // A style no paragraph uses goes without asking
    const QString again = addMemory(styles.coordinator);
    styles.notices.clear();
    CHECK(test::accepting([&]() { CHECK(styles.coordinator.deleteStyle(again)); }) == 0);
    CHECK(styles.notices == QStringList{QStringLiteral("The style \"Memory\" is deleted.")});
}

TEST_CASE("Style coordinator: the commands follow the book and the text in front",
          "[gui][styles]") {
    gui::registerAllIcons();
    gui::registerAllCommands(gui::CommandCallbacks{});
    gui::CommandRegistry& registry = gui::CommandRegistry::getInstance();
    const auto action = [&registry](const char* id) {
        QAction* found = registry.getAction(QString::fromLatin1(id));
        REQUIRE(found != nullptr);
        return found;
    };
    for (const char* id :
         {"format.style.new", "format.style.modify", "format.style.delete",
          "format.style.manage"}) {
        INFO(id);
        CHECK(action(id)->icon().isNull() == false);
    }

    Window window;
    Styles styles(window);
    styles.coordinator.connectCommands();
    CHECK_FALSE(action("format.style.new")->isEnabled());
    CHECK_FALSE(action("format.style.manage")->isEnabled());
    {
        OpenNovel book(QStringLiteral("Styles"));
        CHECK(action("format.style.new")->isEnabled());
        CHECK(action("format.style.manage")->isEnabled());
        CHECK_FALSE(action("format.style.modify")->isEnabled());  // no text in front
        CHECK(registry.getCommand("format.style.modify")->disabledReason() ==
              QStringLiteral("No text of the book is in front"));

        // A text in front: its style at the cursor can change; Normal cannot be deleted
        window.navigator.onElementSelected(book.chapterId, QStringLiteral("The Storm"));
        editor::BookEditor* text = window.editor(window.tabs->currentIndex())->getBookEditor();
        CHECK(action("format.style.modify")->isEnabled());
        CHECK_FALSE(action("format.style.delete")->isEnabled());

        // In a paragraph of a style of the writer's own it can
        const QString memory = addMemory(styles.coordinator);
        text->replaceWithKml(kmlOf({{QString(), QStringLiteral("One.")},
                                    {memory, QStringLiteral("Two.")}}));
        text->setCursorPosition({1, 0});
        CHECK(styles.coordinator.styleAtCursor() == memory);
        CHECK(action("format.style.delete")->isEnabled());
        text->setCursorPosition({0, 0});
        CHECK_FALSE(action("format.style.delete")->isEnabled());
        window.tabs->clear();
    }
    CHECK_FALSE(action("format.style.new")->isEnabled());
}
