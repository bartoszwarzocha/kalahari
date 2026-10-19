/// @file test_paragraph_style_menu.cpp
/// @brief Format > Text Style: the styles every book has, with their keys, and the other
///        styles of the text in front after Code, one checked at a time

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/paragraph_styles.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/menu_builder.h"
#include "kalahari/gui/paragraph_style_menu.h"
#include "kalahari/gui/utils/access_keys.h"

#include <QAction>
#include <QActionGroup>
#include <QMainWindow>
#include <QMenu>
#include <QSet>
#include <QStringList>

#include <string>
#include <vector>

using namespace kalahari;
using namespace kalahari::gui;

namespace {

/// A style of a package, named in English
core::PackageParagraphStyle packageStyle(const QString& id, const QString& name) {
    core::PackageParagraphStyle style;
    style.id = id;
    style.name.values.insert(QStringLiteral("en"), name);
    return style;
}

/// The styles of a book: those every book has, and two of its own
editor::ParagraphStyleSet bookStyles() {
    return editor::ParagraphStyleSet::fromPackage(
        {packageStyle(QStringLiteral("normal"), QStringLiteral("Normal")),
         packageStyle(QStringLiteral("heading_1"), QStringLiteral("Heading 1")),
         packageStyle(QStringLiteral("heading_2"), QStringLiteral("Heading 2")),
         packageStyle(QStringLiteral("heading_3"), QStringLiteral("Heading 3")),
         packageStyle(QStringLiteral("quote"), QStringLiteral("Quote")),
         packageStyle(QStringLiteral("dialogue"), QStringLiteral("Dialogue")),
         packageStyle(QStringLiteral("code"), QStringLiteral("Code")),
         packageStyle(QStringLiteral("caption"), QStringLiteral("Caption")),
         packageStyle(QStringLiteral("letter"), QStringLiteral("Letter"))},
        QStringLiteral("en"));
}

/// The items of a menu as it shows them, without the access keys ("-" for a line)
QStringList itemsOf(const QMenu& menu) {
    QStringList items;
    for (const QAction* action : menu.actions()) {
        items.append(action->isSeparator() ? QStringLiteral("-")
                                           : withoutAccessKey(action->text()));
    }
    return items;
}

}  // namespace

TEST_CASE("Text Style menu: the styles every book has, with their keys",
          "[gui][styles][shortcuts]") {
    QStringList chosen;
    CommandCallbacks callbacks;
    callbacks.onParagraphStyle = [&chosen](const QString& styleId) { chosen.append(styleId); };
    registerAllCommands(callbacks);
    CommandRegistry& registry = CommandRegistry::getInstance();

    // Each fixed command puts the paragraphs in its style
    for (const StyleCommand& command : styleCommands()) {
        INFO(command.commandId);
        QAction* action = registry.getAction(QString::fromLatin1(command.commandId));
        REQUIRE(action != nullptr);
        action->trigger();
        REQUIRE_FALSE(chosen.isEmpty());
        CHECK(chosen.constLast() == QLatin1String(command.styleId));
    }
    CHECK(chosen.size() == static_cast<qsizetype>(styleCommands().size()));
    CHECK(QLatin1String(styleCommands().at(3).styleId) == QLatin1String(editor::NORMAL_STYLE_ID));

    // "Body Text" is now the style Normal
    CHECK(registry.getAction(QStringLiteral("format.style.body"))->text() ==
          QStringLiteral("&Normal"));

    // The headings and Normal have Alt with a digit (Option+Cmd on macOS)
#ifdef Q_OS_MACOS
    const Qt::KeyboardModifiers styleKeys = Qt::ControlModifier | Qt::AltModifier;
#else
    const Qt::KeyboardModifiers styleKeys = Qt::AltModifier;
#endif
    const auto keysOf = [&registry](const char* id) {
        return registry.getAction(QString::fromLatin1(id))->shortcut();
    };
    CHECK(keysOf("format.style.heading1") == QKeySequence(QKeyCombination(styleKeys, Qt::Key_1)));
    CHECK(keysOf("format.style.heading2") == QKeySequence(QKeyCombination(styleKeys, Qt::Key_2)));
    CHECK(keysOf("format.style.heading3") == QKeySequence(QKeyCombination(styleKeys, Qt::Key_3)));
    CHECK(keysOf("format.style.body") == QKeySequence(QKeyCombination(styleKeys, Qt::Key_0)));
    CHECK(keysOf("format.style.quote").isEmpty());

    // Manage Styles... comes later: greyed out
    CHECK_FALSE(registry.getAction(QStringLiteral("format.style.manage"))->isEnabled());
}

TEST_CASE("Text Style menu: the keys of the new commands", "[gui][shortcuts]") {
    registerAllCommands(CommandCallbacks{});
    CommandRegistry& registry = CommandRegistry::getInstance();
    const auto shortcutsOf = [&registry](const char* id) {
        return registry.getAction(QString::fromLatin1(id))->shortcuts();
    };

    // Ctrl+Enter, with the Enter of the keypad too; written Ctrl+Enter
    const QList<QKeySequence> pageBreak = shortcutsOf("insert.pageBreak");
    REQUIRE(pageBreak.size() == 2);
    CHECK(pageBreak.at(0) == QKeySequence(QKeyCombination(Qt::ControlModifier, Qt::Key_Return)));
    CHECK(pageBreak.at(1) == QKeySequence(QKeyCombination(Qt::ControlModifier, Qt::Key_Enter)));
    CHECK(registry.getCommand("insert.pageBreak")->shortcut.toString() ==
          QStringLiteral("Ctrl+Enter"));

    // Ctrl+* as the shortcuts page records it, Delete for Edit > Delete
    CHECK(shortcutsOf("view.showFormattingMarks") ==
          QList<QKeySequence>{QKeySequence(QKeyCombination(Qt::ControlModifier, Qt::Key_Asterisk))});
    CHECK(registry.getCommand("view.showFormattingMarks")->shortcut.toString() ==
          QStringLiteral("Ctrl+*"));
    CHECK(shortcutsOf("edit.delete") == QList<QKeySequence>{QKeySequence(Qt::Key_Delete)});

    // The other commands have one shortcut each
    CHECK(shortcutsOf("file.save").size() == 1);
}

TEST_CASE("Text Style menu: the other styles of the text come after Code", "[gui][styles]") {
    CommandCallbacks callbacks;
    callbacks.onParagraphStyle = [](const QString&) {};
    registerAllCommands(callbacks);
    CommandRegistry& registry = CommandRegistry::getInstance();

    // The window's menus, as MainWindow makes them
    QMainWindow window;
    MenuBuilder builder;
    builder.buildMenuBar(registry, &window);
    QMenu* menu = nullptr;
    for (QObject* object :
         registry.getAction(QStringLiteral("format.style.heading1"))->associatedObjects()) {
        if (auto* found = qobject_cast<QMenu*>(object)) {
            menu = found;
        }
    }
    REQUIRE(menu != nullptr);
    const QStringList fixed = {QStringLiteral("Heading 1"), QStringLiteral("Heading 2"),
                               QStringLiteral("Heading 3"), QStringLiteral("Normal"),
                               QStringLiteral("Quote"),     QStringLiteral("Dialogue"),
                               QStringLiteral("Code"),      QStringLiteral("-"),
                               QStringLiteral("Manage Styles...")};
    CHECK(itemsOf(*menu) == fixed);

    editor::BookEditor book;
    test::resizeWidget(book, QSize(600, 400));
    book.setParagraphStyles(bookStyles());
    book.fromKml(QStringLiteral("<kml><p>Text</p><p style=\"caption\">Picture</p><hr/></kml>"));
    editor::BookEditor* inFront = nullptr;
    ParagraphStyleMenu styles(menu, [&inFront]() { return inFront; }, nullptr);

    // No text in front: the fixed styles only
    styles.rebuild();
    CHECK(itemsOf(*menu) == fixed);

    // The text's other styles, after Code, each time the menu opens; each with a free key
    inFront = &book;
    book.setCursorPosition({1, 2});
    emit menu->aboutToShow();
    emit menu->aboutToShow();
    QStringList withBookStyles = fixed;
    withBookStyles.insert(7, QStringLiteral("Letter"));
    withBookStyles.insert(7, QStringLiteral("Caption"));
    CHECK(itemsOf(*menu) == withBookStyles);
    QSet<QChar> keys;
    for (const QAction* action : menu->actions()) {
        if (!action->isSeparator()) {
            INFO(action->text().toStdString());
            const QChar key = accessKeyOf(action->text());
            CHECK_FALSE(key.isNull());
            CHECK_FALSE(keys.contains(key));
            keys.insert(key);
        }
    }

    // The style of the paragraph at the cursor is checked, as a choice of one
    const QList<QAction*>& bookItems = styles.bookStyleActions();
    REQUIRE(bookItems.size() == 2);
    CHECK(bookItems.at(0)->isChecked());
    CHECK_FALSE(bookItems.at(1)->isChecked());
    CHECK(bookItems.at(0)->actionGroup() != nullptr);
    CHECK(bookItems.at(0)->actionGroup() ==
          registry.getAction(QStringLiteral("format.style.body"))->actionGroup());
    CHECK(bookItems.at(0)->actionGroup()->isExclusive());

    // Choosing one puts the paragraph in it
    book.setCursorPosition({0, 1});
    emit menu->aboutToShow();
    CHECK_FALSE(styles.bookStyleActions().at(0)->isChecked());
    styles.bookStyleActions().at(1)->trigger();
    CHECK(book.currentParagraphStyle() == QStringLiteral("letter"));
    CHECK(book.toKml() == QStringLiteral("<kml><p style=\"letter\">Text</p>"
                                         "<p style=\"caption\">Picture</p><hr/></kml>"));

    // On a line no style is checked
    book.setCursorPosition({2, 0});
    emit menu->aboutToShow();
    for (const QAction* action : styles.bookStyleActions()) {
        CHECK_FALSE(action->isChecked());
    }

    // A chapter of a book open read-only: greyed out, saying why
    book.setReadOnly(true);
    emit menu->aboutToShow();
    for (const QAction* action : styles.bookStyleActions()) {
        CHECK_FALSE(action->isEnabled());
        CHECK_FALSE(action->toolTip().isEmpty());
    }
}
