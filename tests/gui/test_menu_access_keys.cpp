/// @file test_menu_access_keys.cpp
/// @brief The access keys of the menus, in English and in Polish
///
/// On Windows and Linux Alt with the underlined letter of a menu's name opens the menu, and
/// in an open menu the letter of an item chooses it. Every item of every menu of the program
/// has one: a letter or a digit of the Latin alphabet (Polish letters are typed with AltGr),
/// which no other item of the same menu has, in English and in the Polish translation the
/// program ships (embedded under :/i18n, as in the program). So do the context menus: of the
/// text (with the words for a misspelled word and for a grammar issue), of the Navigator, of
/// Qt's fields and of the panels' titles. A line that only tells something has none.

#include <catch2/catch_test_macros.hpp>
#include "../editor/editor_test_utils.h"
#include "context_menu_reader.h"
#include "kalahari/core/book.h"
#include "kalahari/core/book_element.h"
#include "kalahari/core/document.h"
#include "kalahari/core/part.h"
#include "kalahari/core/recent_books_manager.h"
#include "kalahari/editor/annotation.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/grammar_check_service.h"
#include "kalahari/editor/spell_check_service.h"
#include "kalahari/gui/annotations_coordinator.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/diagnostic_menus.h"
#include "kalahari/gui/dock_coordinator.h"
#include "kalahari/gui/menu_builder.h"
#include "kalahari/gui/panels/annotation_card.h"
#include "kalahari/gui/panels/annotation_entry.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/recent_books_menu.h"
#include "kalahari/gui/shortcut_rules.h"
#include "kalahari/gui/toolbar_manager.h"
#include "kalahari/gui/utils/access_keys.h"
#include "kalahari/gui/widget_keys.h"

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QDir>
#include <QDockWidget>
#include <QHash>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QNetworkProxy>
#include <QPlainTextEdit>
#include <QSet>
#include <QSpinBox>
#include <QString>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextEdit>
#include <QTranslator>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <memory>
#include <optional>

using namespace kalahari;
using namespace kalahari::gui;

namespace {

/// The menus in Polish while it lives: the translation the program ships
class Polish {
public:
    Polish() {
        m_loaded = m_translator.load(QStringLiteral(":/i18n/kalahari_pl.qm"));
        if (m_loaded) {
            QCoreApplication::installTranslator(&m_translator);
        }
    }
    ~Polish() { QCoreApplication::removeTranslator(&m_translator); }
    Polish(const Polish&) = delete;
    Polish& operator=(const Polish&) = delete;

    [[nodiscard]] bool loaded() const { return m_loaded; }

private:
    QTranslator m_translator;
    bool m_loaded = false;
};

/// Keeps the recent books of the test run as they were
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

/// The access keys a text marks: an '&' before a character ("&&" is an ampersand)
int accessKeyMarks(const QString& text) {
    int marks = 0;
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (text.at(i) != QLatin1Char('&')) {
            continue;
        }
        if (i + 1 < text.size() && text.at(i + 1) == QLatin1Char('&')) {
            ++i;
        } else {
            ++marks;
        }
    }
    return marks;
}

/// What is wrong with the access keys of the items of a menu, and of its submenus: an item
/// without one or with more than one, a key that is no Latin letter or digit, a key another
/// item of the menu has. The items @p keyless (their menu texts) have none, with a key or
/// without: the lines that only tell something, and the words of a dictionary or of a grammar
/// server where the menu shows them as they are
void collectProblems(const QList<test::MenuItem>& items, const QString& where,
                     const QStringList& keyless, QStringList& problems) {
    QStringList keylessLabels;
    for (const QString& text : keyless) {
        keylessLabels << withoutAccessKey(text);
    }
    QHash<QChar, QString> owners;
    for (const test::MenuItem& item : items) {
        if (item.separator) {
            continue;
        }
        const QString name = where + QStringLiteral(" › ") + withoutAccessKey(item.text);
        const QChar key = accessKeyOf(item.text);
        if (keylessLabels.contains(withoutAccessKey(item.text.section(QLatin1Char('\t'), 0, 0)))) {
            if (accessKeyMarks(item.text) != 0) {
                problems << name + QStringLiteral(": an access key it should not have (") +
                                item.text + QLatin1Char(')');
            }
        } else if (accessKeyMarks(item.text) != 1 || key.isNull()) {
            problems << name + QStringLiteral(": not one access key (") + item.text +
                            QLatin1Char(')');
        } else if (key.unicode() >= 0x80 || !key.isLetterOrNumber()) {
            problems << name + QStringLiteral(": not a Latin letter or digit (") + item.text +
                            QLatin1Char(')');
        } else if (owners.contains(key)) {
            problems << name + QStringLiteral(": ") + key + QStringLiteral(" also in ") +
                            owners.value(key);
        } else {
            owners.insert(key, withoutAccessKey(item.text));
        }
        collectProblems(item.items, name, keyless, problems);
    }
}

/// The items of a menu and of its submenus
int itemCount(const QList<test::MenuItem>& items) {
    int count = 0;
    for (const test::MenuItem& item : items) {
        if (!item.separator) {
            count += 1 + itemCount(item.items);
        }
    }
    return count;
}

/// The texts of the items of a menu
QStringList textsOf(const QMenu& menu) {
    QStringList texts;
    for (const QAction* item : menu.actions()) {
        if (!item->isSeparator()) {
            texts << item->text();
        }
    }
    return texts;
}

/// The labels of the items of a menu: their texts without the keys after a tab
QStringList labelsOf(const QList<test::MenuItem>& items) {
    QStringList labels;
    for (const test::MenuItem& item : items) {
        if (!item.separator) {
            labels << item.text.section(QLatin1Char('\t'), 0, 0);
        }
    }
    return labels;
}

/// A file of a recent book, by its name
QString bookPath(const QString& name) {
    return QDir::temp().absoluteFilePath(name + QStringLiteral(".klh"));
}

/// The main window's menus, as MainWindow makes them, with every item they can have: the
/// recent books, a user's toolbar and the menus of the diagnostic and the dev modes
class Menus {
public:
    Menus() {
        auto& recent = core::RecentBooksManager::getInstance();
        for (int i = 0; i < 9; ++i) {
            recent.addRecentFile(bookPath(QStringLiteral("Book %1").arg(i)));
        }
        recent.addRecentFile(bookPath(QStringLiteral("Tom & Jerry")));

        registerAllCommands(CommandCallbacks{});
        CommandRegistry& registry = CommandRegistry::getInstance();
        m_builder.buildMenuBar(registry, &m_window);
        QMenu* fileMenu = m_builder.getMenu("FILE");
        auto* recentBooks = new RecentBooksMenu(fileMenu);
        recentBooks->insertInto(fileMenu);

        m_toolbars = std::make_unique<ToolbarManager>(&m_window);
        m_toolbars->createToolbars(registry);
        m_toolbars->createViewMenuActions(m_builder.getMenu("VIEW"));
        m_userToolbar = m_toolbars->createUserToolbar(QStringLiteral("Tom & Jerry"));

        const auto noTool = [](DiagnosticTool) {};
        fillDiagnosticsMenu(m_window.menuBar()->addMenu(diagnosticsMenuTitle()), true, &m_window,
                            noTool);
        fillDevToolsMenu(m_window.menuBar()->addMenu(devToolsMenuTitle()), &m_window, noTool);
    }

    [[nodiscard]] QMenuBar* menuBar() { return m_window.menuBar(); }
    [[nodiscard]] ToolbarManager& toolbars() { return *m_toolbars; }
    [[nodiscard]] const QString& userToolbar() const { return m_userToolbar; }

    /// View > Toolbars
    [[nodiscard]] QMenu* toolbarsMenu() const {
        for (const QAction* item : m_builder.getMenu("VIEW")->actions()) {
            if (item->menu() != nullptr &&
                item->text() == MenuBuilder::menuText("Toolbars")) {
                return item->menu();
            }
        }
        return nullptr;
    }

private:
    QMainWindow m_window;
    MenuBuilder m_builder;
    std::unique_ptr<ToolbarManager> m_toolbars;
    QString m_userToolbar;
};

/// Check every menu of the program in the language installed now
void checkEveryMenu() {
    RecentBooksGuard guard;
    Menus menus;

    QStringList problems;
    const QList<test::MenuItem> menuBar = test::menuItems(menus.menuBar()->actions());
    collectProblems(menuBar, QStringLiteral("Menu bar"), {}, problems);
    CHECK(itemCount(menuBar) > 150);

    // Alt with a letter opens a menu: Settings > Keyboard Shortcuts keeps those for them
    for (const QAction* menu : menus.menuBar()->actions()) {
        INFO(menu->text().toStdString());
        CHECK(accessKeyOf(menu->text()).isLetter());
    }

    // The context menu of the toolbars
    QMenu toolbarMenu;
    menus.toolbars().fillContextMenu(&toolbarMenu);
    const QList<test::MenuItem> toolbarItems = test::menuItems(toolbarMenu.actions());
    collectProblems(toolbarItems, QStringLiteral("Toolbars' context menu"), {}, problems);
    CHECK(itemCount(toolbarItems) > 10);

    // The menu of the kinds of annotations, and the menus of the cards of each kind
    QMenu addMenu;
    AnnotationsCoordinator::fillAddMenu(&addMenu);
    const QList<test::MenuItem> addItems = test::menuItems(addMenu.actions());
    collectProblems(addItems, QStringLiteral("Add Annotation"), {}, problems);
    CHECK(itemCount(addItems) == 3);
    for (const editor::AnnotationKind kind : {editor::AnnotationKind::Comment,
                                              editor::AnnotationKind::Todo,
                                              editor::AnnotationKind::Note}) {
        for (const bool done : {false, true}) {
            AnnotationEntry entry;
            entry.annotation.id = QStringLiteral("a");
            entry.annotation.kind = kind;
            entry.annotation.done = done;
            entry.annotation.text = QStringLiteral("Text");
            AnnotationCard card;
            card.setEntry(entry);
            QMenu cardMenu;
            card.fillMenu(&cardMenu);
            const QList<test::MenuItem> cardItems = test::menuItems(cardMenu.actions());
            collectProblems(cardItems, QStringLiteral("Annotation card"), {}, problems);
            CHECK(itemCount(cardItems) >= 2);
        }
    }

    for (const QString& problem : problems) {
        FAIL_CHECK(problem.toStdString());
    }
}

// -----------------------------------------------------------------------------
// Context menus
// -----------------------------------------------------------------------------

/// The context of the book editor's words in the translations
constexpr const char* BOOK_EDITOR = "kalahari::editor::BookEditor";

/// The menus as Windows and Linux show them while it lives: with the filter the program gives
/// the application there (WidgetKeys). It gives the items of Qt's fields the access keys of
/// Windows, and an item of a menu's own without one a free one as the menu shows
class ShownMenus {
public:
    ShownMenus() { QCoreApplication::instance()->installEventFilter(&m_keys); }
    ~ShownMenus() { QCoreApplication::instance()->removeEventFilter(&m_keys); }
    ShownMenus(const ShownMenus&) = delete;
    ShownMenus& operator=(const ShownMenus&) = delete;

private:
    WidgetKeys m_keys{ShortcutPlatform::Windows};
};

/// The context menu a widget opens for an event at a place of it, read as it is about to
/// show; the menu belongs to @p owner, the parent Qt or the program gives it
QList<test::MenuItem> contextMenu(QWidget* widget, QWidget* owner, const QPoint& place,
                                  QContextMenuEvent::Reason reason = QContextMenuEvent::Mouse) {
    INFO("The context menu of a " << widget->metaObject()->className());
    test::ContextMenuReader reader(owner);
    QContextMenuEvent event(reason, place, widget->mapToGlobal(place));
    QCoreApplication::sendEvent(widget, &event);
    // A menu Qt opens with popup() is open until the events run
    CHECK(test::waitUntil([] { return QApplication::activePopupWidget() == nullptr; }));
    REQUIRE(reader.menusRead() == 1);
    return reader.items();
}

/// The context menu of a field as the menu key opens it
QList<test::MenuItem> fieldMenu(QWidget* field, QWidget* owner) {
    return contextMenu(field, owner, QPoint(5, 5), QContextMenuEvent::Keyboard);
}

/// A sentence with a misspelled word ("chaptre", 4 to 11) and a grammar issue ("cta", 24 to 27)
QString sentence() {
    return QStringLiteral("The chaptre one and the cta sat.");
}

/// The place of a character of the editor's paragraph, for a click
QPoint placeOf(editor::BookEditor& editor, int offset) {
    editor.setCursorPosition({0, offset});
    return editor.inputMethodQuery(Qt::ImCursorRectangle).toRect().center();
}

/// Talks to localhost directly, whatever proxy the environment sets (asking for the current
/// one would start libproxy, which leaks a few bytes the leak sanitizer reports)
struct NoProxy {
    NoProxy() { QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy); }
    ~NoProxy() { QNetworkProxy::setApplicationProxy(QNetworkProxy()); }
    NoProxy(const NoProxy&) = delete;
    NoProxy& operator=(const NoProxy&) = delete;
};

/// A LanguageTool server on localhost that finds one issue in the sentence: "cta", with two
/// words to put in its place and an explanation besides its short message
class FakeLanguageTool {
public:
    FakeLanguageTool() {
        REQUIRE(m_server.listen(QHostAddress::LocalHost));
        QObject::connect(&m_server, &QTcpServer::newConnection, [this]() {
            while (QTcpSocket* socket = m_server.nextPendingConnection()) {
                auto request = std::make_shared<QByteArray>();
                QObject::connect(socket, &QTcpSocket::readyRead, socket, [socket, request]() {
                    *request += socket->readAll();
                    if (cameWhole(*request)) {
                        answer(socket);
                    }
                });
                QObject::connect(socket, &QTcpSocket::disconnected, socket,
                                 &QObject::deleteLater);
            }
        });
    }
    FakeLanguageTool(const FakeLanguageTool&) = delete;
    FakeLanguageTool& operator=(const FakeLanguageTool&) = delete;

    [[nodiscard]] QString url() const {
        return QStringLiteral("http://127.0.0.1:%1").arg(m_server.serverPort());
    }

private:
    /// Whether a request came whole: its headers and the form of the length they give
    static bool cameWhole(const QByteArray& request) {
        const qsizetype headerEnd = request.indexOf("\r\n\r\n");
        if (headerEnd < 0) {
            return false;
        }
        const QByteArray headers = request.left(headerEnd).toLower();
        const QByteArray lengthKey = "content-length:";
        const qsizetype keyAt = headers.indexOf(lengthKey);
        if (keyAt < 0) {
            return true;
        }
        const qsizetype valueAt = keyAt + lengthKey.size();
        const qsizetype lineEnd = headers.indexOf("\r\n", valueAt);
        const int length =
            headers.mid(valueAt, lineEnd < 0 ? -1 : lineEnd - valueAt).trimmed().toInt();
        return request.size() >= headerEnd + 4 + length;
    }

    static void answer(QTcpSocket* socket) {
        const QByteArray json =
            R"({"matches":[{"message":"The letters seem to be in the wrong order.",)"
            R"("shortMessage":"Typo","offset":24,"length":3,)"
            R"("replacements":[{"value":"cat"},{"value":"act"}],)"
            R"("context":{"text":"The chaptre one and the cta sat.","offset":24,"length":3},)"
            R"("rule":{"id":"TEST_RULE","category":{"id":"GRAMMAR","name":"Grammar"}}}]})";
        socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " +
                      QByteArray::number(json.size()) + "\r\nConnection: close\r\n\r\n" + json);
        socket->disconnectFromHost();
    }

    QTcpServer m_server;
};

/// The context menus of a book's text: the usual one, without a selection and with one, in
/// both color modes; that of a misspelled word, with the words of the dictionary and without
/// a dictionary; that of a grammar issue. @p shown: as Windows and Linux show them, where the
/// words have keys too
void checkTextMenus(bool shown, QStringList& problems) {
    auto& registry = CommandRegistry::getInstance();
    editor::BookEditor editor;
    test::resizeWidget(editor, QSize(600, 400));
    editor.fromKml(test::kmlOf({sentence()}));
    // The commands the program gives the text's menu (AnnotationsCoordinator)
    editor.setContextMenuActions({registry.getAction(QStringLiteral("insert.comment")),
                                  registry.getAction(QStringLiteral("insert.todo")),
                                  registry.getAction(QStringLiteral("insert.note"))});

    for (const editor::EditorColorMode mode :
         {editor::EditorColorMode::Light, editor::EditorColorMode::Dark}) {
        editor.setEditorColorMode(mode);
        editor.clearSelection();
        const QPoint inText = placeOf(editor, 1);
        QList<test::MenuItem> items = contextMenu(&editor, &editor, inText);
        collectProblems(items, QStringLiteral("Text"), {}, problems);
        CHECK(itemCount(items) == 6);  // Paste, Select All, the three annotations, the color mode

        // A click in the selection keeps it
        editor.selectAll();
        items = contextMenu(&editor, &editor, inText);
        collectProblems(items, QStringLiteral("Selected text"), {}, problems);
        CHECK(itemCount(items) == 8);  // also Cut and Copy
    }
    editor.clearSelection();

    // The words for a misspelled one, from the dictionary, on top of the usual menu; the
    // editor checks its text as it shows
    editor::SpellCheckService spelling;
    REQUIRE(spelling.loadDictionary(QStringLiteral("en_US")));
    const auto checked = [](const editor::BookEditor& checkedEditor) {
        return test::waitUntil([&checkedEditor] { return !checkedEditor.isSpellCheckPending(); },
                               test::DICTIONARY_WAIT_MS);
    };
    editor.setSpellCheckService(&spelling);
    editor.show();
    REQUIRE(checked(editor));
    const QStringList words = spelling.suggestions(QStringLiteral("chaptre"), 5);
    REQUIRE(words.contains(QStringLiteral("chapter")));
    QStringList wordTexts;
    for (const QString& word : words) {
        wordTexts << literalMenuText(word);
    }
    QList<test::MenuItem> items = contextMenu(&editor, &editor, placeOf(editor, 6));
    collectProblems(items, QStringLiteral("Misspelled word"), shown ? QStringList() : wordTexts,
                    problems);
    // and Ignore All, Add to Dictionary, then the usual menu
    CHECK(itemCount(items) == words.size() + 2 + 6);

    // A word the dictionary has nothing like: the menu says there are none
    editor::BookEditor unknownWord;
    test::resizeWidget(unknownWord, QSize(600, 400));
    unknownWord.fromKml(test::kmlOf({QStringLiteral("The qxzvwk one.")}));
    unknownWord.setSpellCheckService(&spelling);
    unknownWord.show();
    REQUIRE(checked(unknownWord));
    REQUIRE(spelling.suggestions(QStringLiteral("qxzvwk"), 5).isEmpty());
    const QString none = QCoreApplication::translate(BOOK_EDITOR, "(No suggestions)");
    items = contextMenu(&unknownWord, &unknownWord, placeOf(unknownWord, 6));
    collectProblems(items, QStringLiteral("Misspelled word without suggestions"), {none},
                    problems);
    CHECK(labelsOf(items).contains(none));

    // Spelling goes before grammar: the issue below is a misspelling too
    editor.setSpellCheckService(nullptr);

    // The words for a grammar issue, from the writer's LanguageTool server; the lines that
    // tell what is wrong have no keys
    const NoProxy noProxy;
    FakeLanguageTool server;
    editor::GrammarCheckService grammar;
    grammar.setDebounceMs(100);
    grammar.setApiEndpoint(server.url());
    editor.setGrammarCheckService(&grammar);  // checks the text, as in the program
    REQUIRE(test::waitUntil([&grammar] { return !grammar.errorsForParagraph(0).isEmpty(); },
                            10000));
    QStringList told = {QStringLiteral("Typo"),
                        QCoreApplication::translate(BOOK_EDITOR, "Error: \"%1\"")
                            .arg(QStringLiteral("cta"))};
    if (!shown) {
        told << QStringLiteral("cat") << QStringLiteral("act");
    }
    items = contextMenu(&editor, &editor, placeOf(editor, 25));
    collectProblems(items, QStringLiteral("Grammar issue"), told, problems);
    CHECK(itemCount(items) == 6);  // and Explanation, Ignore this rule
}

/// The context menus of the Navigator, for every kind of its items
void checkNavigatorMenus(QStringList& problems) {
    core::Document document("Book", "Author", "en");
    core::Book& book = document.getBook();
    book.addFrontMatter(std::make_shared<core::BookElement>("title_page", "fm-001", "Title"));
    auto part = std::make_shared<core::Part>("part-001", "Part One");
    part->addChapter(std::make_shared<core::BookElement>("chapter", "ch-001", "Chapter 1"));
    part->addChapter(std::make_shared<core::BookElement>("chapter", "ch-002", "Chapter 2"));
    book.addPart(part);
    book.addBackMatter(std::make_shared<core::BookElement>("epilogue", "bm-001", "Epilogue"));

    NavigatorPanel panel;
    panel.setAttribute(Qt::WA_DontShowOnScreen);
    panel.resize(400, 900);
    panel.show();
    panel.loadDocument(document);
    panel.addStandaloneFile(QDir::temp().absoluteFilePath(QStringLiteral("notes.txt")));
    auto* tree = panel.findChild<QTreeWidget*>();
    REQUIRE(tree != nullptr);
    tree->expandAll();

    QSet<QString> kinds;
    for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it) {
        QTreeWidgetItem* item = *it;
        tree->scrollToItem(item);
        test::ContextMenuReader reader(&panel);
        emit tree->customContextMenuRequested(tree->visualItemRect(item).center());
        if (reader.menusRead() == 1) {
            kinds.insert(item->data(0, Qt::UserRole + 1).toString());
            collectProblems(reader.items(), QStringLiteral("Navigator › ") + item->text(0), {},
                            problems);
        }
    }
    CHECK(kinds == QSet<QString>{QStringLiteral("document"), QStringLiteral("section_frontmatter"),
                                 QStringLiteral("title_page"), QStringLiteral("section_body"),
                                 QStringLiteral("part"), QStringLiteral("chapter"),
                                 QStringLiteral("section_backmatter"), QStringLiteral("epilogue"),
                                 QStringLiteral("other_files"),
                                 QStringLiteral("standalone_file")});
}

/// A main window with the program's panels and toolbars, and a toolbar the user made (the
/// commands registered)
struct WindowPanels {
    QMainWindow window;
    DockCoordinator* docks = nullptr;  // owned by the window, as in MainWindow
    std::unique_ptr<ToolbarManager> toolbars;

    WindowPanels() {
        window.setAttribute(Qt::WA_DontShowOnScreen);
        docks = new DockCoordinator(&window, &window);
        docks->createDocks();
        toolbars = std::make_unique<ToolbarManager>(&window);
        toolbars->createToolbars(CommandRegistry::getInstance());
        toolbars->createUserToolbar(QStringLiteral("Tom & Jerry"));
        window.resize(1200, 800);
        window.show();
        QCoreApplication::processEvents();
    }
};

/// The menu of the menu bar and of the panels' titles: Qt's, as MainWindow gives it the keys
void checkPanelsMenu(QStringList& problems) {
    WindowPanels panels;
    const std::unique_ptr<QMenu> qtMenu(panels.window.createPopupMenu());
    REQUIRE(qtMenu != nullptr);
    const std::unique_ptr<QMenu> menu(menuWithAccessKeys(*qtMenu, &panels.window));
    const QList<test::MenuItem> items = test::menuItems(menu->actions());
    collectProblems(items, QStringLiteral("Panels and toolbars"), {}, problems);
    CHECK(itemCount(items) == itemCount(test::menuItems(qtMenu->actions())));
    CHECK(itemCount(items) > 10);
}

/// The context menus of Qt's fields as the menu key opens them: editable and read-only lines
/// and texts, a number, and the link of a label
void checkFieldMenus(QStringList& problems) {
    QLineEdit line(QStringLiteral("Some words"));
    line.setSelection(0, 4);
    collectProblems(fieldMenu(&line, &line), QStringLiteral("Line"), {}, problems);

    QLineEdit emptyLine;
    collectProblems(fieldMenu(&emptyLine, &emptyLine), QStringLiteral("Empty line"), {},
                    problems);

    QLineEdit readOnlyLine(QStringLiteral("Some words"));
    readOnlyLine.setReadOnly(true);
    collectProblems(fieldMenu(&readOnlyLine, &readOnlyLine), QStringLiteral("Read-only line"),
                    {}, problems);

    QSpinBox number;
    auto* numberLine = number.findChild<QLineEdit*>();
    REQUIRE(numberLine != nullptr);
    collectProblems(fieldMenu(&number, numberLine), QStringLiteral("Number"), {}, problems);

    // Qt gives the menu of a text to its viewport
    QTextEdit text(QStringLiteral("Some words"));
    collectProblems(fieldMenu(&text, text.viewport()), QStringLiteral("Text"), {}, problems);

    QPlainTextEdit plainText(QStringLiteral("Some words"));
    collectProblems(fieldMenu(&plainText, plainText.viewport()), QStringLiteral("Plain text"), {},
                    problems);

    QPlainTextEdit readOnlyText(QStringLiteral("Some words"));
    readOnlyText.setReadOnly(true);
    collectProblems(fieldMenu(&readOnlyText, readOnlyText.viewport()),
                    QStringLiteral("Read-only text"), {}, problems);

    // A label has its menu over a link
    QLabel label(QStringLiteral("<a href=\"https://example.com\">The page of the book</a>"));
    label.setTextInteractionFlags(Qt::TextBrowserInteraction);
    label.resize(300, 40);
    collectProblems(contextMenu(&label, &label, QPoint(10, 20)), QStringLiteral("Link"), {},
                    problems);
}

/// Check the context menus in the language installed now: as the program makes them (each
/// item of its own with its key; the words of a dictionary and of a grammar server as they
/// are) or, @p shown, as Windows and Linux show them (every item with a key: those words too,
/// and the items of Qt's fields)
void checkContextMenus(bool shown) {
    std::optional<ShownMenus> filter;
    if (shown) {
        filter.emplace();
    }
    registerAllCommands(CommandCallbacks{});

    QStringList problems;
    checkTextMenus(shown, problems);
    checkNavigatorMenus(problems);
    checkPanelsMenu(problems);
    if (shown) {
        checkFieldMenus(problems);
    }
    for (const QString& problem : problems) {
        FAIL_CHECK(problem.toStdString());
    }
}

/// The labels of the context menus of a line and of a number, as the menu key opens them
QStringList lineLabels() {
    QLineEdit line(QStringLiteral("Some words"));
    return labelsOf(fieldMenu(&line, &line));
}

QStringList numberLabels() {
    QSpinBox number;
    return labelsOf(fieldMenu(&number, number.findChild<QLineEdit*>()));
}

}  // namespace

TEST_CASE("Menu access keys: one in every item, unique in its menu, in English",
          "[gui][menus][keys]") {
    checkEveryMenu();
}

TEST_CASE("Menu access keys: one in every item, unique in its menu, in Polish",
          "[gui][menus][keys]") {
    const Polish polish;
    REQUIRE(polish.loaded());
    CHECK(MenuBuilder::menuText("FILE") == QStringLiteral("&Plik"));
    checkEveryMenu();
}

TEST_CASE("Menu access keys: one in every item of the context menus, in English",
          "[gui][menus][keys]") {
    SECTION("As the program makes them") {
        checkContextMenus(false);
    }
    SECTION("As Windows and Linux show them") {
        checkContextMenus(true);
    }
}

TEST_CASE("Menu access keys: one in every item of the context menus, in Polish",
          "[gui][menus][keys]") {
    const Polish polish;
    REQUIRE(polish.loaded());
    SECTION("As the program makes them") {
        checkContextMenus(false);
    }
    SECTION("As Windows and Linux show them") {
        checkContextMenus(true);
    }
}

TEST_CASE("Menu access keys: the fields have those of Windows", "[gui][menus][keys]") {
    const ShownMenus shown;

    SECTION("In English, also where Qt's words have none or share one") {
        CHECK(lineLabels() == QStringList{QStringLiteral("&Undo"), QStringLiteral("&Redo"),
                                          QStringLiteral("Cu&t"), QStringLiteral("&Copy"),
                                          QStringLiteral("&Paste"), QStringLiteral("&Delete"),
                                          QStringLiteral("Select &All")});
        CHECK(numberLabels() ==
              QStringList{QStringLiteral("&Undo"), QStringLiteral("&Redo"),
                          QStringLiteral("Cu&t"), QStringLiteral("&Copy"),
                          QStringLiteral("&Paste"), QStringLiteral("&Delete"),
                          QStringLiteral("Select &All"), QStringLiteral("&Step up"),
                          QStringLiteral("Step d&own")});
    }

    SECTION("In Polish") {
        const Polish polish;
        REQUIRE(polish.loaded());
        CHECK(lineLabels() == QStringList{QStringLiteral("&Cofnij"), QStringLiteral("&Ponów"),
                                          QStringLiteral("Wy&tnij"), QStringLiteral("&Kopiuj"),
                                          QStringLiteral("Wk&lej"), QStringLiteral("&Usuń"),
                                          QStringLiteral("Zaznacz &wszystko")});
        CHECK(numberLabels() ==
              QStringList{QStringLiteral("&Cofnij"), QStringLiteral("&Ponów"),
                          QStringLiteral("Wy&tnij"), QStringLiteral("&Kopiuj"),
                          QStringLiteral("Wk&lej"), QStringLiteral("&Usuń"),
                          QStringLiteral("Zaznacz &wszystko"), QStringLiteral("Zwięk&sz"),
                          QStringLiteral("Z&mniejsz")});
    }
}

TEST_CASE("Menu access keys: the menu of the panels' titles names them with keys",
          "[gui][menus][keys]") {
    registerAllCommands(CommandCallbacks{});
    WindowPanels panels;
    const std::unique_ptr<QMenu> qtMenu(panels.window.createPopupMenu());
    REQUIRE(qtMenu != nullptr);
    const std::unique_ptr<QMenu> menu(menuWithAccessKeys(*qtMenu, &panels.window));

    // Qt's items, as they are, with keys; the panels and the toolbars keep their names
    const QList<QAction*> qtItems = qtMenu->actions();
    const QList<QAction*> items = menu->actions();
    REQUIRE(items.size() == qtItems.size());
    for (qsizetype i = 0; i < items.size(); ++i) {
        CAPTURE(qtItems.at(i)->text().toStdString());
        CHECK(items.at(i)->isSeparator() == qtItems.at(i)->isSeparator());
        if (!items.at(i)->isSeparator()) {
            CHECK(withoutAccessKey(items.at(i)->text()) == qtItems.at(i)->text());
            CHECK(items.at(i)->isCheckable() == qtItems.at(i)->isCheckable());
            CHECK(items.at(i)->isChecked() == qtItems.at(i)->isChecked());
            CHECK(items.at(i)->isEnabled() == qtItems.at(i)->isEnabled());
        }
    }

    // An item hides its panel and shows it again
    QDockWidget* navigator = panels.docks->navigatorDock();
    QAction* navigatorItem = nullptr;
    for (QAction* item : items) {
        if (withoutAccessKey(item->text()) == navigator->windowTitle()) {
            navigatorItem = item;
        }
    }
    REQUIRE(navigatorItem != nullptr);
    const bool hidden = navigator->isHidden();
    navigatorItem->trigger();
    CHECK(navigator->isHidden() != hidden);
    navigatorItem->trigger();
    CHECK(navigator->isHidden() == hidden);
}

TEST_CASE("Menu access keys: lists and toolbar buttons show the names without them",
          "[gui][menus][keys]") {
    registerAllCommands(CommandCallbacks{});
    const Command* saveAs = CommandRegistry::getInstance().getCommand("file.saveAs");
    REQUIRE(saveAs != nullptr);
    CHECK(saveAs->actionText() == QStringLiteral("Save &As..."));
    CHECK(saveAs->label == "Save As...");
    CHECK(saveAs->tooltip == "Save As...");

    // An ampersand of the name stays one
    const Command* replace = CommandRegistry::getInstance().getCommand("edit.findReplace");
    REQUIRE(replace != nullptr);
    CHECK(replace->actionText() == QStringLiteral("Find && Rep&lace..."));
    CHECK(replace->label == "Find & Replace...");

    CHECK(MenuBuilder::menuText("FORMAT") == QStringLiteral("F&ormat"));
    CHECK(MenuBuilder::menuTitle("FORMAT") == QStringLiteral("Format"));
}

TEST_CASE("Menu access keys: a name the user gave gets a key no other item has",
          "[gui][menus][keys]") {
    RecentBooksGuard guard;
    Menus menus;
    QMenu* toolbarsMenu = menus.toolbarsMenu();
    REQUIRE(toolbarsMenu != nullptr);

    // "Tom & Jerry": T is the Tools Toolbar's, so J; the ampersand is shown as it is
    CHECK(textsOf(*toolbarsMenu).contains(QStringLiteral("Tom && &Jerry")));

    // Renamed, it gets the first free letter of the new name, not its own old one
    REQUIRE(menus.toolbars().renameUserToolbar(menus.userToolbar(), QStringLiteral("Notes")));
    CHECK(textsOf(*toolbarsMenu).contains(QStringLiteral("&Notes")));
    REQUIRE(menus.toolbars().renameUserToolbar(menus.userToolbar(), QStringLiteral("Journal")));
    CHECK(textsOf(*toolbarsMenu).contains(QStringLiteral("&Journal")));

    // The recent books: numbered, a name with an ampersand shown as it is
    QMenu* fileMenu = nullptr;
    for (const QAction* menu : menus.menuBar()->actions()) {
        if (menu->text() == MenuBuilder::menuText("FILE")) {
            fileMenu = menu->menu();
        }
    }
    REQUIRE(fileMenu != nullptr);
    QMenu* recentBooks = nullptr;
    for (const QAction* item : fileMenu->actions()) {
        if (item->text() == MenuBuilder::menuText("Recent Books")) {
            recentBooks = item->menu();
        }
    }
    REQUIRE(recentBooks != nullptr);
    CHECK(textsOf(*recentBooks).first() == QStringLiteral("&1. Tom && Jerry"));
    CHECK(textsOf(*recentBooks).contains(QStringLiteral("1&0. Book 0")));

    // None: a line that only tells so, disabled and without a key
    core::RecentBooksManager::getInstance().clearRecentFiles();
    REQUIRE(recentBooks->actions().size() == 1);
    const QAction* none = recentBooks->actions().constFirst();
    CHECK(none->text() == QStringLiteral("No Recent Files"));
    CHECK_FALSE(none->isEnabled());
}

TEST_CASE("Menu access keys: the names of the user's toolbars share the free keys",
          "[gui][menus][keys]") {
    registerAllCommands(CommandCallbacks{});
    QMainWindow window;
    ToolbarManager toolbars(&window);
    toolbars.createToolbars(CommandRegistry::getInstance());

    // "Big X-ray" comes first and wants X, the only free key of "Boxes": it takes another one
    toolbars.createUserToolbar(QStringLiteral("Big X-ray"));
    toolbars.createUserToolbar(QStringLiteral("Boxes"));

    // View > Toolbars, made with them: R is free there
    {
        QMenu viewMenu;
        toolbars.createViewMenuActions(&viewMenu);
        REQUIRE(viewMenu.actions().size() == 1);
        const QMenu* toolbarsMenu = viewMenu.actions().constFirst()->menu();
        REQUIRE(toolbarsMenu != nullptr);
        CHECK(textsOf(*toolbarsMenu).contains(QStringLiteral("Big X-&ray")));
        CHECK(textsOf(*toolbarsMenu).contains(QStringLiteral("Bo&xes")));
    }

    // The toolbars' context menu: R is Reset to Default's there
    QMenu contextMenu;
    toolbars.fillContextMenu(&contextMenu);
    CHECK(textsOf(contextMenu).contains(QStringLiteral("Bi&g X-ray")));
    CHECK(textsOf(contextMenu).contains(QStringLiteral("Bo&xes")));

    // With View > Toolbars gone, a new toolbar does without an item there
    CHECK_FALSE(toolbars.createUserToolbar(QStringLiteral("Later")).isEmpty());
}

TEST_CASE("Menu access keys: the words a menu shows as they are get free ones",
          "[gui][menus][keys]") {
    QObject window;  // the owner of the window's commands
    QMenu menu;
    menu.addAction(QStringLiteral("&Copy"));
    QAction* word = menu.addAction(QStringLiteral("chapter"));
    QAction* told = menu.addAction(QStringLiteral("Typo"));
    told->setEnabled(false);
    auto* command = new QAction(QStringLiteral("Shared"), &window);
    menu.addAction(command);
    QAction* withKeys = menu.addAction(QStringLiteral("R&&D\tCtrl+R"));
    QMenu* more = menu.addMenu(QStringLiteral("More"));
    QMenu other(QStringLiteral("Other"));
    menu.addMenu(&other);

    giveFreeAccessKeys(&menu);
    CHECK(word->text() == QStringLiteral("c&hapter"));        // C is Copy's
    CHECK(told->text() == QStringLiteral("Typo"));            // a line that only tells
    CHECK(command->text() == QStringLiteral("Shared"));       // a command of the window
    CHECK(withKeys->text() == QStringLiteral("&R&&D\tCtrl+R"));  // the keys stay
    CHECK(more->menuAction()->text() == QStringLiteral("&More"));  // its own submenu
    CHECK(other.menuAction()->text() == QStringLiteral("Other"));  // a menu of the window

    // Shown again, the menu keeps its texts
    const QStringList texts = textsOf(menu);
    giveFreeAccessKeys(&menu);
    CHECK(textsOf(menu) == texts);
}

TEST_CASE("Menu access keys: the texts of menus", "[gui][menus][keys]") {
    CHECK(withoutAccessKey(QStringLiteral("Save &As...")) == QStringLiteral("Save As..."));
    CHECK(withoutAccessKey(QStringLiteral("Find && Rep&lace...")) ==
          QStringLiteral("Find & Replace..."));
    CHECK(withoutAccessKey(QStringLiteral("&&")) == QStringLiteral("&"));
    CHECK(withoutAccessKey(QStringLiteral("Plain")) == QStringLiteral("Plain"));
    CHECK(withoutAccessKey(QStringLiteral("End &")) == QStringLiteral("End "));

    CHECK(literalMenuText(QStringLiteral("Tom & Jerry")) == QStringLiteral("Tom && Jerry"));
    CHECK(withoutAccessKey(literalMenuText(QStringLiteral("A && B &"))) ==
          QStringLiteral("A && B &"));

    CHECK(accessKeyOf(QStringLiteral("Save &As...")) == QLatin1Char('A'));
    CHECK(accessKeyOf(QStringLiteral("Find && Rep&lace...")) == QLatin1Char('L'));
    CHECK(accessKeyOf(QStringLiteral("1&0. Book")) == QLatin1Char('0'));
    CHECK(accessKeyOf(QStringLiteral("Tom && Jerry")).isNull());
    CHECK(accessKeyOf(QStringLiteral("Plain")).isNull());
    CHECK(accessKeyOf(QStringLiteral("End &")).isNull());

    QMenu menu;
    menu.addAction(QStringLiteral("&Open"));
    QAction* save = menu.addAction(QStringLiteral("&Save"));
    menu.addSeparator();
    menu.addAction(QStringLiteral("Plain"));
    CHECK(accessKeysIn(&menu) == QSet<QChar>{QLatin1Char('O'), QLatin1Char('S')});
    CHECK(accessKeysIn(&menu, save) == QSet<QChar>{QLatin1Char('O')});
}

TEST_CASE("Menu access keys: a free key for a name the user gave", "[gui][menus][keys]") {
    // The first letter of a word that is free, then any free letter or digit
    QSet<QChar> used = {QLatin1Char('T')};
    CHECK(withFreeAccessKey(QStringLiteral("Tom & Jerry"), used) ==
          QStringLiteral("Tom && &Jerry"));
    CHECK(used.contains(QLatin1Char('J')));

    used = {QLatin1Char('T'), QLatin1Char('J')};
    CHECK(withFreeAccessKey(QStringLiteral("Tom & Jerry"), used) ==
          QStringLiteral("T&om && Jerry"));

    // Polish letters are typed with AltGr: never an access key
    used = {};
    CHECK(withFreeAccessKey(QStringLiteral("Łódź 2"), used) == QStringLiteral("Łódź &2"));
    used = {QLatin1Char('2')};
    CHECK(withFreeAccessKey(QStringLiteral("Łódź 2"), used) == QStringLiteral("Łó&dź 2"));

    // Every letter taken: no key rather than one another item has
    used = {QLatin1Char('A'), QLatin1Char('B')};
    CHECK(withFreeAccessKey(QStringLiteral("Ab & ba"), used) == QStringLiteral("Ab && ba"));
    CHECK(used.size() == 2);
}

TEST_CASE("Menu access keys: free keys for names that share letters", "[gui][menus][keys]") {
    // Each keeps the first free one it wants where the names after it have free ones too
    QSet<QChar> used = {QLatin1Char('C')};
    CHECK(withFreeAccessKeys({QStringLiteral("Ab"), QStringLiteral("Ad"), QStringLiteral("cat")},
                             used) ==
          QStringList{QStringLiteral("&Ab"), QStringLiteral("A&d"), QStringLiteral("ca&t")});
    CHECK(used == QSet<QChar>{QLatin1Char('A'), QLatin1Char('C'), QLatin1Char('D'),
                              QLatin1Char('T')});

    // A name whose letters are all taken gets one a name before it leaves for another of its
    // own: Ab leaves A for A and takes B, which Bc leaves for C
    used = {};
    CHECK(withFreeAccessKeys({QStringLiteral("Ab"), QStringLiteral("Bc"), QStringLiteral("A")},
                             used) ==
          QStringList{QStringLiteral("A&b"), QStringLiteral("B&c"), QStringLiteral("&A")});

    // None left: no key rather than one another item has
    used = {QLatin1Char('X')};
    CHECK(withFreeAccessKeys({QStringLiteral("X"), QStringLiteral("Y")}, used) ==
          QStringList{QStringLiteral("X"), QStringLiteral("&Y")});
}
