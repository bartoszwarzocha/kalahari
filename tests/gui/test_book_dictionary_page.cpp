// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_book_dictionary_page.cpp
/// @brief The book's dictionary on its tab: its lists, counts, search and keys, in a book open
///        read-only too; the commands and windows that change it (BookDictionaryCoordinator),
///        with the spelling dictionary of the book's language; Settings > Editor > Spelling;
///        and the page in the book window (Book > Book Dictionary..., Find, closing the book)

#include <catch2/catch_test_macros.hpp>

#include "kalahari_tests/editor/editor_test_utils.h"
#include "kalahari/core/book_dictionary.h"
#include "kalahari/core/name_declension.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/spell_check_service.h"
#include "kalahari/gui/book_dictionary_coordinator.h"
#include "kalahari/gui/command.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/gui/main_window.h"
#include "kalahari/gui/panels/book_dictionary_panel.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/read_only_book.h"
#include "kalahari/gui/settings/settings_pages.h"
#include "kalahari/gui/settings_coordinator.h"
#include "kalahari/gui/settings_dialog.h"
#include "kalahari/gui/spelling_coordinator.h"
#include "kalahari/gui/utils/language_names.h"
#include "kalahari/gui/widgets/central_tabs.h"
#include "kalahari_tests/test_support/window_parts.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShortcut>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>

#include <nlohmann/json_fwd.hpp>

#include <array>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace core = kalahari::core;
namespace editor = kalahari::editor;
namespace gui = kalahari::gui;
using core::DictionaryName;
using core::NameDeclension;
using core::NameKind;
using core::SpellingRule;
using gui::BookDictionaryCoordinator;
using gui::BookDictionaryPanel;
using gui::dialogs::KalahariDialog;
using kalahari::test::DICTIONARY_WAIT_MS;
using kalahari::test::waitUntil;
using Part = gui::BookDictionaryPanel::Part;

namespace {

/// The folder of the book types that come with the program
QString builtInTypes() {
    return QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes");
}

core::SettingsManager& settings() {
    return core::SettingsManager::getInstance();
}

/// Gives back the settings of the spelling and the book's dictionary as they were before
class KeptSettings {
public:
    KeptSettings() {
        for (const char* key : KEYS) {
            m_values[key] = settings().get<nlohmann::json>(key, nlohmann::json());
        }
        settings().set<bool>("editor.spellCheck.enabled", true);
        settings().set<std::string>("editor.spellCheck.language", "");
    }
    ~KeptSettings() {
        try {
            for (const auto& [key, value] : m_values) {
                if (value.is_null()) {
                    settings().removeKey(key);
                } else {
                    settings().set<nlohmann::json>(key, value);
                }
            }
        } catch (...) {
            // Left as the test made them; the next tests set what they need
            static_cast<void>(std::fputs("The settings of the book's dictionary could not be "
                                         "given back\n",
                                         stderr));
        }
    }
    KeptSettings(const KeptSettings&) = delete;
    KeptSettings& operator=(const KeptSettings&) = delete;
    KeptSettings(KeptSettings&&) = delete;
    KeptSettings& operator=(KeptSettings&&) = delete;

private:
    static constexpr std::array<const char*, 4> KEYS = {
        "editor.spellCheck.enabled", "editor.spellCheck.language", "bookDictionary.markRules",
        "bookDictionary.panelAddsTo"};
    std::map<std::string, nlohmann::json> m_values;
};

/// The book of the tests, "Rain", open in a new folder until the test ends
class OpenBook {
public:
    explicit OpenBook(const QString& language = QStringLiteral("pl")) {
        REQUIRE(m_folder.isValid());
        REQUIRE(book().closeProject(false));
        REQUIRE(book().createProject(m_folder.path(), QStringLiteral("Rain"),
                                     QStringLiteral("Author"), language, true));
    }
    ~OpenBook() {
        try {
            book().closeProject(false);
        } catch (...) {
            // A destructor may not throw: the later tests close the book themselves
            static_cast<void>(
                std::fputs("OpenBook: the book of the test stays open\n", stderr));
        }
    }
    OpenBook(const OpenBook&) = delete;
    OpenBook& operator=(const OpenBook&) = delete;
    OpenBook(OpenBook&&) = delete;
    OpenBook& operator=(OpenBook&&) = delete;

    static core::ProjectManager& book() { return kalahari::test::book(); }

private:
    QTemporaryDir m_folder;
};

/// A rule of the book with its forms as given
SpellingRule rule(const QString& right, const QStringList& wrong, bool allForms,
                  const QStringList& forms = {}) {
    SpellingRule made;
    made.right = right;
    made.wrong = wrong;
    made.allForms = allForms;
    for (qsizetype i = 0; i + 1 < forms.size(); i += 2) {
        made.forms.insert(forms.at(i), forms.at(i + 1));
    }
    return made;
}

/// The dictionary of the tests: the names Anna Wolska (also "Ania"), Księga Popiołów and
/// Eldrion, which does not change; the rules Smok, okej, e-mail and półelf; the words Kirran
/// and Vael
void fillSample(core::ProjectManager& book) {
    const NameDeclension polish(QStringLiteral("pl"));
    DictionaryName anna =
        core::makeName(QStringLiteral("Anna Wolska"), NameKind::Character, polish);
    core::NameVariant ania;
    ania.text = QStringLiteral("Ania");
    core::refreshForms(ania, NameKind::Character, polish);
    anna.variants.append(ania);
    DictionaryName eldrion = core::makeName(QStringLiteral("Eldrion"), NameKind::Character, polish);
    eldrion.variants.front().models = {NameDeclension::unchanged()};
    REQUIRE(book.changeBookDictionary([&](core::BookDictionary& dictionary) {
        dictionary.putName(anna);
        dictionary.putName(
            core::makeName(QStringLiteral("Księga Popiołów"), NameKind::Item, polish));
        dictionary.putName(eldrion);
        dictionary.putRule(rule(QStringLiteral("Smok"), {QStringLiteral("smok")}, true,
                                {QStringLiteral("smok"), QStringLiteral("Smok"),
                                 QStringLiteral("smoka"), QStringLiteral("Smoka")}));
        SpellingRule okej = rule(QStringLiteral("okej"), {QStringLiteral("okey")}, false);
        okej.note = QStringLiteral("As the publisher writes it.");
        dictionary.putRule(okej);
        dictionary.putRule(rule(QStringLiteral("e-mail"), {QStringLiteral("email")}, true,
                                {QStringLiteral("email"), QStringLiteral("e-mail")}));
        SpellingRule halfElf = rule(QStringLiteral("półelf"), {QStringLiteral("pół-elf")}, true,
                                    {QStringLiteral("pół-elf"), QStringLiteral("półelf"),
                                     QStringLiteral("pół-elfa"), QStringLiteral("półelfa")});
        halfElf.pattern = QStringLiteral("elf");
        dictionary.putRule(halfElf);
        dictionary.addWord(QStringLiteral("Kirran"));
        dictionary.addWord(QStringLiteral("Vael"));
    }));
}

/// The id of the name or the rule with the text
QString idOf(const core::ProjectManager& book, const QString& text) {
    const core::BookDictionary& dictionary = book.bookDictionary();
    if (const DictionaryName* name = dictionary.findNameWithText(text)) {
        return name->id;
    }
    for (const SpellingRule& each : dictionary.rules()) {
        if (each.right == text) {
            return each.id;
        }
    }
    return {};
}

/// The texts of the rows of a list, the columns apart by " | "
QStringList rows(const QTreeWidget* list) {
    QStringList texts;
    for (int i = 0; i < list->topLevelItemCount(); ++i) {
        QStringList columns;
        for (int column = 0; column < list->columnCount(); ++column) {
            columns.append(list->topLevelItem(i)->text(column));
        }
        texts.append(columns.join(QStringLiteral(" | ")));
    }
    return texts;
}

/// The first column of the rows of a list
QStringList firstColumn(const QTreeWidget* list) {
    QStringList texts;
    for (int i = 0; i < list->topLevelItemCount(); ++i) {
        texts.append(list->topLevelItem(i)->text(0));
    }
    return texts;
}

/// The child of @p parent with the object name @p name
template <typename Child>
Child* child(const QWidget& parent, const char* name) {
    auto* found = parent.findChild<Child*>(QLatin1String(name));
    REQUIRE(found != nullptr);
    return found;
}

/// The texts of the labels of a window, one per line
QString textsOf(const QDialog& dialog) {
    QStringList texts;
    for (const QLabel* label : dialog.findChildren<QLabel*>()) {
        if (!label->text().isEmpty()) {
            texts.append(label->text());
        }
    }
    return texts.join(QLatin1Char('\n'));
}

/// Press a key in @p widget
void press(QWidget* widget, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QCoreApplication::sendEvent(widget, &event);
}

/// The shortcut of the page with the keys
QShortcut* shortcut(const QWidget& page, QKeyCombination keys) {
    QShortcut* found = nullptr;
    for (QShortcut* each : page.findChildren<QShortcut*>()) {
        if (each->key() == QKeySequence(keys)) {
            found = each;
            break;
        }
    }
    REQUIRE(found != nullptr);
    return found;
}

/// Write @p text in place of the text of @p field, as the writer does
void type(QLineEdit* field, const QString& text) {
    field->setText(text);
    emit field->textEdited(text);
}

/// What a test does with a window shown
using Answer = std::function<void(QDialog&)>;

/// Run @p command, answering the windows it shows with @p answers in turn; a window beyond
/// them is closed with Cancel, so that the test never waits for it
/// @return The number of windows the command showed
int answering(const std::function<void()>& command, QList<Answer> answers = {}) {
    int shown = 0;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&shown, &answers]() {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (dialog == nullptr || !dialog->isVisible()) {
            return;
        }
        ++shown;
        if (answers.isEmpty()) {
            dialog->reject();
        } else {
            answers.takeFirst()(*dialog);
        }
    });
    timer.start();
    command();
    return shown;
}

/// The answer that presses the button of the action of a window: Add, Save, Remove
void pressAction(QDialog& dialog) {
    if (const auto* own = qobject_cast<KalahariDialog*>(&dialog)) {
        own->acceptButton()->click();
    } else {
        dialog.accept();
    }
}

/// Copy folder @p from, with the folders in it, to @p to
void copyFolder(const QString& from, const QString& to) {
    const QDir source(from);
    QDirIterator it(from, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString file = source.relativeFilePath(it.next());
        const QString target = QDir(to).filePath(file);
        REQUIRE(QDir().mkpath(QFileInfo(target).absolutePath()));
        REQUIRE(QFile::copy(source.filePath(file), target));
    }
}

/// A novel with a word in its dictionary, made where the program had the novel and open where
/// it does not have it: read-only, until the test ends
struct ReadOnlyNovel {
    QTemporaryDir booksDir;
    QTemporaryDir typesDir;
    core::ProjectManager& pm = kalahari::test::book();

    ReadOnlyNovel() {
        REQUIRE(booksDir.isValid());
        REQUIRE(typesDir.isValid());
        REQUIRE(pm.closeProject(false));
        core::ProjectManager::loadBookTypes({builtInTypes()});
        REQUIRE(pm.createProject(booksDir.path(), QStringLiteral("Rain"),
                                 QStringLiteral("Author"), QStringLiteral("en"), false,
                                 QStringLiteral("kalahari.novel")));
        const QString manifest = pm.getManifestPath();
        REQUIRE(pm.changeBookDictionary([](core::BookDictionary& dictionary) {
            dictionary.addWord(QStringLiteral("Kirran"));
            dictionary.putRule(rule(QStringLiteral("Smok"), {QStringLiteral("smok")}, false));
        }));
        REQUIRE(pm.closeProject(false));

        // Another computer
        const QString types = typesDir.filePath(QStringLiteral("booktypes"));
        copyFolder(builtInTypes(), types);
        for (const char* id : {"kalahari.novel", "kalahari.short_stories"}) {
            REQUIRE(QDir(QDir(types).filePath(QLatin1String(id))).removeRecursively());
        }
        core::ProjectManager::loadBookTypes({types});
        REQUIRE(pm.openProject(manifest));
        REQUIRE(pm.isReadOnly());
    }
    ~ReadOnlyNovel() {
        try {
            pm.closeProject(false);
            core::ProjectManager::loadBookTypes({builtInTypes()});
        } catch (...) {
            // A destructor may not throw: the later tests then find this book or its types
            static_cast<void>(
                std::fputs("ReadOnlyNovel: the book or the book types of the test stay\n", stderr));
        }
    }
    ReadOnlyNovel(const ReadOnlyNovel&) = delete;
    ReadOnlyNovel& operator=(const ReadOnlyNovel&) = delete;
    ReadOnlyNovel(ReadOnlyNovel&&) = delete;
    ReadOnlyNovel& operator=(ReadOnlyNovel&&) = delete;
};

/// The tests that give the keys to a part of a window need an active window; the offscreen
/// platform has one without a window on screen
bool canActivateWindows() {
    return QGuiApplication::platformName() == QStringLiteral("offscreen");
}

}  // namespace

// =============================================================================
// The page
// =============================================================================

TEST_CASE("Book dictionary page: its lists, counts and search", "[gui][dictionary][page]") {
    const OpenBook open;
    fillSample(OpenBook::book());
    BookDictionaryPanel panel(OpenBook::book());
    panel.resize(800, 600);
    panel.show();

    // The counts on the tabs, the book and the language of its forms
    CHECK(panel.tabs()->tabText(0) == QStringLiteral("Names (3)"));
    CHECK(panel.tabs()->tabText(1) == QStringLiteral("Spelling Rules (4)"));
    CHECK(panel.tabs()->tabText(2) == QStringLiteral("Book Words (2)"));
    CHECK(child<QLabel>(panel, "bookDictionarySubtitle")
              ->text()
              .startsWith(QStringLiteral("Rain · forms: ")));
    CHECK(panel.part() == Part::Names);

    // The names in order, with their kinds, forms and other names
    const QStringList names = rows(panel.list(Part::Names));
    REQUIRE(names.size() == 3);
    CHECK(names.at(0).startsWith(QStringLiteral("Anna Wolska | Character | Anny Wolskiej, ")));
    CHECK(names.at(0).endsWith(QStringLiteral(" | Anna, Wolska, Ania")));
    CHECK(names.at(1) == QStringLiteral("Eldrion | Character | does not change | "));
    CHECK(names.at(2).startsWith(QStringLiteral("Księga Popiołów | Item | ")));

    // The rules, with what they cover (in the order of the language of the program)
    QStringList rules = rows(panel.list(Part::Rules));
    rules.sort(Qt::CaseInsensitive);
    CHECK(rules ==
          QStringList{QStringLiteral("e-mail | email | does not change | "),
                      QStringLiteral("okej | okey | no | As the publisher writes it."),
                      QStringLiteral("półelf | pół-elf | yes (like %1) | ")
                          .arg(QLocale().quoteString(QStringLiteral("elf"))),
                      QStringLiteral("Smok | smok | yes | ")});
    CHECK(firstColumn(panel.list(Part::Words)) ==
          QStringList{QStringLiteral("Kirran"), QStringLiteral("Vael")});

    // The search finds the names in their forms and other names too, the rules in their
    // forms and notes
    panel.searchField()->setText(QStringLiteral("annę"));
    CHECK(firstColumn(panel.list(Part::Names)) == QStringList{QStringLiteral("Anna Wolska")});
    panel.searchField()->setText(QStringLiteral("ania"));
    CHECK(firstColumn(panel.list(Part::Names)) == QStringList{QStringLiteral("Anna Wolska")});
    panel.searchField()->setText(QStringLiteral("smoka"));
    CHECK(firstColumn(panel.list(Part::Rules)) == QStringList{QStringLiteral("Smok")});
    CHECK(panel.list(Part::Names)->topLevelItemCount() == 0);
    panel.searchField()->setText(QStringLiteral("publisher"));
    CHECK(firstColumn(panel.list(Part::Rules)) == QStringList{QStringLiteral("okej")});
    panel.searchField()->setText(QStringLiteral("półelfa"));
    CHECK(firstColumn(panel.list(Part::Rules)) == QStringList{QStringLiteral("półelf")});
    panel.searchField()->setText(QStringLiteral(" kir "));
    CHECK(firstColumn(panel.list(Part::Words)) == QStringList{QStringLiteral("Kirran")});
    CHECK(panel.tabs()->tabText(2) == QStringLiteral("Book Words (2)"));  // all of them
    panel.focusSearch();
    CHECK(panel.searchField()->selectedText() == QStringLiteral(" kir "));
    panel.searchField()->clear();

    // The rows chosen stay chosen when the dictionary changes
    panel.showPart(Part::Rules);
    REQUIRE(panel.selectId(idOf(OpenBook::book(), QStringLiteral("Smok"))));
    CHECK_FALSE(panel.selectId(QStringLiteral("missing")));
    REQUIRE(OpenBook::book().changeBookDictionary([](core::BookDictionary& dictionary) {
        dictionary.putRule(rule(QStringLiteral("Bóg"), {QStringLiteral("bóg")}, false));
    }));
    panel.refresh();
    CHECK(panel.tabs()->tabText(1) == QStringLiteral("Spelling Rules (5)"));
    CHECK(panel.selectedIds() == QStringList{idOf(OpenBook::book(), QStringLiteral("Smok"))});
}

TEST_CASE("Book dictionary page: its buttons and keys", "[gui][dictionary][page]") {
    const OpenBook open;
    fillSample(OpenBook::book());
    BookDictionaryPanel panel(OpenBook::book());
    panel.resize(800, 600);
    panel.show();
    auto* add = child<QPushButton>(panel, "bookDictionaryAdd");
    auto* edit = child<QPushButton>(panel, "bookDictionaryEdit");
    auto* remove = child<QPushButton>(panel, "bookDictionaryRemove");
    auto* move = child<QPushButton>(panel, "bookDictionaryMove");
    const auto* note = child<QLabel>(panel, "bookDictionaryNote");

    QList<Part> added;
    QStringList edited;
    QList<QStringList> removed;
    QStringList words;
    QList<QStringList> moved;
    int settingsAsked = 0;
    QObject::connect(&panel, &BookDictionaryPanel::addRequested,
                     [&added](Part part) { added.append(part); });
    QObject::connect(&panel, &BookDictionaryPanel::editRequested,
                     [&edited](Part, const QString& id) { edited.append(id); });
    QObject::connect(&panel, &BookDictionaryPanel::removeRequested,
                     [&removed](Part, const QStringList& ids) { removed.append(ids); });
    QObject::connect(&panel, &BookDictionaryPanel::addWordRequested,
                     [&words](const QString& word) { words.append(word); });
    QObject::connect(&panel, &BookDictionaryPanel::moveToUserWordsRequested,
                     [&moved](const QStringList& chosen) { moved.append(chosen); });
    QObject::connect(&panel, &BookDictionaryPanel::settingsRequested,
                     [&settingsAsked]() { ++settingsAsked; });

    SECTION("the names: Add Name..., Edit..., Remove, with the buttons and the keys") {
        CHECK(add->text() == QStringLiteral("Add Name..."));
        CHECK(add->isVisibleTo(&panel));
        CHECK_FALSE(move->isVisibleTo(&panel));
        CHECK(note->text().startsWith(QStringLiteral("Names written in the dictionary")));
        const QString anna = idOf(OpenBook::book(), QStringLiteral("Anna Wolska"));
        CHECK(panel.selectedIds() == QStringList{anna});  // the first row
        CHECK(edit->isEnabled());

        add->click();
        edit->click();
        remove->click();
        QTreeWidget* names = panel.list(Part::Names);
        press(names, Qt::Key_Insert);
        press(names, Qt::Key_Return);
        press(names, Qt::Key_Delete);
        press(names, Qt::Key_Delete, Qt::ShiftModifier);  // not the key of Remove
        CHECK(added == QList<Part>{Part::Names, Part::Names});
        CHECK(edited == QStringList{anna, anna});
        CHECK(removed == QList<QStringList>{{anna}, {anna}});

        child<QToolButton>(panel, "bookDictionarySettings")->click();
        CHECK(settingsAsked == 1);
    }

    SECTION("the rules: Add Rule...; Ctrl+Tab and Ctrl+Shift+Tab go through the tabs") {
        emit shortcut(panel, QKeyCombination(Qt::ControlModifier, Qt::Key_Tab))->activated();
        CHECK(panel.part() == Part::Rules);
        CHECK(add->text() == QStringLiteral("Add Rule..."));
        CHECK(note->text().startsWith(QStringLiteral("The spelling check marks a departure")));
        add->click();
        CHECK(added == QList<Part>{Part::Rules});
        emit shortcut(panel, QKeyCombination(Qt::ControlModifier, Qt::Key_Tab))->activated();
        emit shortcut(panel, QKeyCombination(Qt::ControlModifier, Qt::Key_Tab))->activated();
        CHECK(panel.part() == Part::Names);
        emit shortcut(panel, QKeyCombination(Qt::ControlModifier | Qt::ShiftModifier, Qt::Key_Tab))
            ->activated();
        CHECK(panel.part() == Part::Words);
        emit shortcut(panel, QKeyCombination(Qt::ControlModifier, Qt::Key_Backtab))->activated();
        CHECK(panel.part() == Part::Rules);
    }

    SECTION("the words: the field of a new one, Remove and Move to Your Words") {
        panel.showPart(Part::Words);
        CHECK_FALSE(add->isVisibleTo(&panel));
        CHECK_FALSE(edit->isVisibleTo(&panel));
        CHECK(move->isVisibleTo(&panel));
        CHECK(note->text().startsWith(QStringLiteral("Words right in this book only")));

        // A new word: Enter in its field
        QLineEdit* field = panel.newWordField();
        press(field, Qt::Key_Return);  // empty: nothing
        field->setText(QStringLiteral("  Zorblat "));
        press(field, Qt::Key_Return);
        CHECK(words == QStringList{QStringLiteral("Zorblat")});
        CHECK(field->text().isEmpty());

        // Insert goes to the field; Enter does not edit a word
        QTreeWidget* list = panel.list(Part::Words);
        press(list, Qt::Key_Insert);
        press(list, Qt::Key_Return);
        CHECK(added.isEmpty());
        CHECK(edited.isEmpty());

        // Both words chosen
        list->topLevelItem(0)->setSelected(true);
        list->topLevelItem(1)->setSelected(true);
        const QStringList both{QStringLiteral("Kirran"), QStringLiteral("Vael")};
        CHECK(panel.selectedIds() == both);
        move->click();
        remove->click();
        CHECK(moved == QList<QStringList>{both});
        CHECK(removed == QList<QStringList>{both});

        // Nothing chosen: nothing to do
        list->setCurrentItem(nullptr);
        list->clearSelection();
        CHECK(panel.selectedIds().isEmpty());
        CHECK_FALSE(move->isEnabled());
        press(list, Qt::Key_Delete);
        CHECK(removed.size() == 1);
        panel.focusList();
    }
}

TEST_CASE("Book dictionary page: a book open read-only", "[gui][dictionary][page][read-only]") {
    const ReadOnlyNovel novel;
    BookDictionaryPanel panel(novel.pm);
    panel.resize(800, 600);
    panel.show();
    auto* add = child<QPushButton>(panel, "bookDictionaryAdd");
    CHECK(panel.tabs()->tabText(2) == QStringLiteral("Book Words (1)"));
    CHECK_FALSE(add->isEnabled());
    CHECK(add->toolTip() == gui::readOnlyReason());
    CHECK_FALSE(child<QPushButton>(panel, "bookDictionaryRemove")->isEnabled());
    CHECK_FALSE(panel.newWordField()->isEnabled());

    // The keys change nothing either
    int asked = 0;
    const auto count = [&asked]() { ++asked; };
    QObject::connect(&panel, &BookDictionaryPanel::addRequested, count);
    QObject::connect(&panel, &BookDictionaryPanel::editRequested, count);
    QObject::connect(&panel, &BookDictionaryPanel::removeRequested, count);
    QObject::connect(&panel, &BookDictionaryPanel::addWordRequested, count);
    QObject::connect(&panel, &BookDictionaryPanel::moveToUserWordsRequested, count);
    panel.showPart(Part::Rules);
    press(panel.list(Part::Rules), Qt::Key_Insert);
    press(panel.list(Part::Rules), Qt::Key_Return);
    press(panel.list(Part::Rules), Qt::Key_Delete);
    panel.showPart(Part::Words);
    child<QPushButton>(panel, "bookDictionaryMove")->click();
    panel.newWordField()->setText(QStringLiteral("Vael"));
    press(panel.newWordField(), Qt::Key_Return);
    CHECK(asked == 0);

    // Nor does the coordinator; the status bar says why
    QStatusBar statusBar;
    QTabWidget tabs;
    BookDictionaryCoordinator coordinator(novel.pm, kalahari::test::commands(), &tabs, nullptr,
                                          &statusBar);
    CHECK_FALSE(coordinator.addWord(QStringLiteral("Vael")));
    CHECK(statusBar.currentMessage() ==
          QStringLiteral("The book is open read-only: its dictionary cannot be changed"));
    CHECK_FALSE(novel.pm.bookDictionary().hasWord(QStringLiteral("Vael")));
}

// =============================================================================
// The commands and the windows
// =============================================================================

TEST_CASE("Book dictionary page: a series by its name, with the language of the current volume",
          "[gui][dictionary][page][series]") {
    const KeptSettings kept;
    const OpenBook open;
    core::ProjectManager& book = OpenBook::book();
    core::NewBook request;
    request.title = QStringLiteral("Dunes");
    request.language = QStringLiteral("en");
    const QString dunes = book.addBook(request);
    REQUIRE_FALSE(dunes.isEmpty());
    REQUIRE(book.setSeriesName(QStringLiteral("Desert")));
    QStatusBar statusBar;
    QTabWidget tabs;
    BookDictionaryCoordinator coordinator(book, kalahari::test::commands(), &tabs, nullptr,
                                          &statusBar);
    const BookDictionaryPanel* panel = coordinator.openDictionary(false);
    REQUIRE(panel != nullptr);
    const auto* subtitle = child<QLabel>(*panel, "bookDictionarySubtitle");
    const auto expected = [](const char* language) {
        return QStringLiteral("Desert · forms: %1 (the language of the book)")
            .arg(gui::utils::languageName(QString::fromLatin1(language)));
    };
    CHECK(subtitle->text() == expected("pl"));
    // Another current volume: the language of its forms
    REQUIRE(book.setCurrentBook(dunes));
    CHECK(subtitle->text() == expected("en"));
}

TEST_CASE("Book dictionary: its page on a tab, and Book > Book Dictionary...",
          "[gui][dictionary][coordinator]") {
    REQUIRE(OpenBook::book().closeProject(false));
    QStatusBar statusBar;
    QTabWidget tabs;
    tabs.resize(800, 600);
    tabs.show();
    gui::registerAllCommands(kalahari::test::commands(), gui::CommandCallbacks{});
    gui::Command* command =
        kalahari::test::commands().getCommand(BookDictionaryCoordinator::COMMAND_ID);
    REQUIRE(command != nullptr);
    {
        BookDictionaryCoordinator coordinator(OpenBook::book(), kalahari::test::commands(), &tabs,
                                              nullptr, &statusBar);
        coordinator.connectCommands();
        REQUIRE(command->execute);

        // Without a book: no page
        CHECK(coordinator.openDictionary() == nullptr);
        CHECK(coordinator.panel() == nullptr);
        CHECK(coordinator.currentPanel() == nullptr);
        CHECK_FALSE(coordinator.addName());
        CHECK_FALSE(coordinator.editName(QStringLiteral("n1")));
        CHECK_FALSE(coordinator.editCardName(QStringLiteral("c1"), QStringLiteral("Anna"),
                                             NameKind::Character));
        CHECK_FALSE(coordinator.addRule());
        CHECK_FALSE(coordinator.editRule(QStringLiteral("r1")));
        CHECK_FALSE(coordinator.removeEntries(Part::Words, {QStringLiteral("Kirran")}));
        CHECK_FALSE(coordinator.addWord(QStringLiteral("Kirran")));
        CHECK_FALSE(coordinator.moveToUserWords({QStringLiteral("Kirran")}));

        // The page on its tab, one for the book; it follows the dictionary
        const OpenBook open;
        command->execute();
        const BookDictionaryPanel* panel = coordinator.panel();
        REQUIRE(panel != nullptr);
        CHECK(tabs.count() == 1);
        CHECK(tabs.tabText(0) == QStringLiteral("Book Dictionary"));
        CHECK(panel->property("tabIconId").toString() == QStringLiteral("book.dictionary"));
        CHECK(coordinator.currentPanel() == panel);
        CHECK(coordinator.openDictionary(false) == panel);
        CHECK(tabs.count() == 1);
        CHECK(coordinator.addWord(QStringLiteral(" Kirran ")));
        CHECK_FALSE(coordinator.addWord(QStringLiteral("  ")));
        CHECK(firstColumn(panel->list(Part::Words)) == QStringList{QStringLiteral("Kirran")});

        // The gear of the page: Settings > Editor > Spelling
        int settingsAsked = 0;
        QObject::connect(&coordinator, &BookDictionaryCoordinator::settingsRequested,
                         [&settingsAsked]() { ++settingsAsked; });
        child<QToolButton>(*panel, "bookDictionarySettings")->click();
        CHECK(settingsAsked == 1);

        // Another page in front
        auto* other = new QWidget();
        tabs.setCurrentIndex(tabs.addTab(other, QStringLiteral("Chapter")));
        gui::DocumentWindows::of(tabs).showPage(other);
        CHECK(coordinator.currentPanel() == nullptr);
        CHECK(coordinator.openDictionary() == panel);
        CHECK(coordinator.currentPanel() == panel);

        // Without the tabs of a window: no page
        BookDictionaryCoordinator without(OpenBook::book(), kalahari::test::commands(), nullptr,
                                          nullptr, nullptr);
        CHECK(without.openDictionary() == nullptr);
        CHECK(without.panel() == nullptr);
        CHECK(without.currentPanel() == nullptr);
        CHECK(without.addWord(QStringLiteral("Vael")));
        CHECK_FALSE(without.moveToUserWords({QStringLiteral("Vael")}));  // no spelling
    }
    CHECK_FALSE(command->execute);
}

TEST_CASE("Book dictionary: the windows of the names", "[gui][dictionary][coordinator]") {
    const OpenBook open;
    fillSample(OpenBook::book());
    QStatusBar statusBar;
    QTabWidget tabs;
    tabs.resize(800, 600);
    tabs.show();
    BookDictionaryCoordinator coordinator(OpenBook::book(), kalahari::test::commands(), &tabs,
                                          nullptr, &statusBar);
    const core::BookDictionary& dictionary = OpenBook::book().bookDictionary();
    BookDictionaryPanel* panel = coordinator.openDictionary();
    REQUIRE(panel != nullptr);

    SECTION("Add Name... with a word: the window with its forms; a name known: its own") {
        bool added = false;
        QString acceptText;
        QString formsShown;
        CHECK(answering([&]() { added = coordinator.addName(QStringLiteral("Jan Nowak")); },
                        {[&](QDialog& dialog) {
                            const auto& window = static_cast<KalahariDialog&>(dialog);
                            acceptText = window.acceptButton()->text();
                            formsShown = child<QLineEdit>(window, "nameText")->text();
                            pressAction(dialog);
                        }}) == 1);
        CHECK(added);
        CHECK(acceptText == QStringLiteral("Add"));
        CHECK(formsShown == QStringLiteral("Jan Nowak"));
        const DictionaryName* jan = dictionary.findNameWithText(QStringLiteral("Jan Nowak"));
        REQUIRE(jan != nullptr);
        CHECK(jan->variants.size() == 3);
        CHECK(panel->tabs()->tabText(0) == QStringLiteral("Names (4)"));

        CHECK(answering([&]() { added = coordinator.addName(QStringLiteral(" Anna  Wolska")); },
                        {[&acceptText](QDialog& dialog) {
                            const auto& window = static_cast<KalahariDialog&>(dialog);
                            acceptText = window.acceptButton()->text();
                            dialog.reject();
                        }}) == 1);
        CHECK_FALSE(added);
        CHECK(acceptText == QStringLiteral("Save"));
        CHECK(answering([&]() { added = coordinator.addName(); }) == 1);
        CHECK_FALSE(added);
    }

    SECTION("Edit... of a name, and from the page") {
        const QString anna = idOf(OpenBook::book(), QStringLiteral("Anna Wolska"));
        bool changed = false;
        CHECK(answering([&]() { changed = coordinator.editName(anna); },
                        {[](QDialog& dialog) {
                            child<QCheckBox>(dialog, "nameQuickInsert")->setChecked(false);
                            pressAction(dialog);
                        }}) == 1);
        CHECK(changed);
        CHECK_FALSE(dictionary.findName(anna)->quickInsert);
        CHECK_FALSE(coordinator.editName(QStringLiteral("missing")));

        // From the page: Insert and Enter
        panel->showPart(Part::Names);
        CHECK(answering([panel]() { emit panel->addRequested(Part::Names); }) == 1);
        CHECK(answering([panel, &anna]() { emit panel->editRequested(Part::Names, anna); }) == 1);
        CHECK(dictionary.names().size() == 3);
    }

    SECTION("a card of the Workshop: its name, one of the same name, or a new one") {
        const QString anna = idOf(OpenBook::book(), QStringLiteral("Anna Wolska"));
        const auto accept = [](QDialog& dialog) { pressAction(dialog); };
        CHECK(answering(
                  [&]() {
                      CHECK(coordinator.editCardName(QStringLiteral("card-1"),
                                                     QStringLiteral("Anna Wolska"),
                                                     NameKind::Character));
                  },
                  {accept}) == 1);
        CHECK(dictionary.findName(anna)->card == QStringLiteral("card-1"));
        CHECK(dictionary.names().size() == 3);

        // A name with another card is not taken over
        CHECK(answering(
                  [&]() {
                      CHECK(coordinator.editCardName(QStringLiteral("card-2"),
                                                     QStringLiteral("Anna Wolska"),
                                                     NameKind::Character));
                  },
                  {accept}) == 1);
        CHECK(dictionary.findNameOfCard(QStringLiteral("card-2")) != nullptr);
        CHECK(dictionary.findNameOfCard(QStringLiteral("card-2"))->id != anna);

        // The card's own name, whatever its title now
        QString shown;
        CHECK(answering(
                  [&]() {
                      CHECK_FALSE(coordinator.editCardName(QStringLiteral("card-1"),
                                                           QStringLiteral("Anna Nowak"),
                                                           NameKind::Character));
                  },
                  {[&shown](QDialog& dialog) {
                      shown = child<QLineEdit>(dialog, "nameText")->text();
                      dialog.reject();
                  }}) == 1);
        CHECK(shown == QStringLiteral("Anna Wolska"));

        // A place
        CHECK(answering(
                  [&]() {
                      CHECK(coordinator.editCardName(QStringLiteral("card-3"),
                                                     QStringLiteral("Czarny Las"),
                                                     NameKind::Place));
                  },
                  {accept}) == 1);
        const DictionaryName* forest = dictionary.findNameOfCard(QStringLiteral("card-3"));
        REQUIRE(forest != nullptr);
        CHECK(forest->kind == NameKind::Place);
        CHECK(forest->text() == QStringLiteral("Czarny Las"));
    }
}

TEST_CASE("Book dictionary: the windows of the rules, and Remove",
          "[gui][dictionary][coordinator]") {
    const OpenBook open;
    fillSample(OpenBook::book());
    QStatusBar statusBar;
    QTabWidget tabs;
    tabs.resize(800, 600);
    tabs.show();
    BookDictionaryCoordinator coordinator(OpenBook::book(), kalahari::test::commands(), &tabs,
                                          nullptr, &statusBar);
    const core::BookDictionary& dictionary = OpenBook::book().bookDictionary();
    BookDictionaryPanel* panel = coordinator.openDictionary();
    REQUIRE(panel != nullptr);
    const QString smok = idOf(OpenBook::book(), QStringLiteral("Smok"));

    SECTION("Add Rule... and Edit... of a rule") {
        bool done = false;
        CHECK(answering([&]() { done = coordinator.addRule(); },
                        {[](QDialog& dialog) {
                            type(child<QLineEdit>(dialog, "ruleRight"), QStringLiteral("Elfka"));
                            pressAction(dialog);
                        }}) == 1);
        CHECK(done);
        const QString elfka = idOf(OpenBook::book(), QStringLiteral("Elfka"));
        REQUIRE_FALSE(elfka.isEmpty());
        CHECK(dictionary.findRule(elfka)->wrong == QStringList{QStringLiteral("elfka")});

        CHECK(answering([&]() { done = coordinator.editRule(smok); },
                        {[](QDialog& dialog) {
                            child<QLineEdit>(dialog, "ruleNote")
                                ->setText(QStringLiteral("A people of the north."));
                            pressAction(dialog);
                        }}) == 1);
        CHECK(done);
        CHECK(dictionary.findRule(smok)->note == QStringLiteral("A people of the north."));
        CHECK_FALSE(coordinator.editRule(QStringLiteral("missing")));

        // From the page
        CHECK(answering([panel]() { emit panel->addRequested(Part::Rules); }) == 1);
        CHECK(answering([panel, &smok]() { emit panel->editRequested(Part::Rules, smok); }) == 1);
        CHECK(dictionary.rules().size() == 5);
    }

    SECTION("Remove asks first: names, rules, a word, words") {
        const QString anna = idOf(OpenBook::book(), QStringLiteral("Anna Wolska"));
        QString asked;
        const auto confirm = [&asked](QDialog& dialog) {
            asked = textsOf(dialog);
            pressAction(dialog);
        };
        CHECK(answering([&]() { CHECK(coordinator.removeEntries(Part::Names, {anna})); },
                        {confirm}) == 1);
        CHECK(asked.contains(QStringLiteral("Remove %1 from the book's dictionary? Its forms will "
                                            "no longer be right in the book.")
                                 .arg(QLocale().quoteString(QStringLiteral("Anna Wolska")))));
        CHECK(dictionary.findName(anna) == nullptr);

        // Cancel: it stays
        CHECK(answering([&]() { CHECK_FALSE(coordinator.removeEntries(Part::Rules, {smok})); }) ==
              1);
        CHECK(dictionary.findRule(smok) != nullptr);
        CHECK(answering([&]() { emit panel->removeRequested(Part::Rules, {smok}); }, {confirm}) ==
              1);
        CHECK(asked.contains(QStringLiteral("Remove the rule %1 from the book's dictionary?")
                                   .arg(QLocale().quoteString(QStringLiteral("Smok")))));
        CHECK(dictionary.findRule(smok) == nullptr);

        CHECK(answering(
                  [&]() {
                      CHECK(coordinator.removeEntries(Part::Words, {QStringLiteral("Kirran")}));
                  },
                  {confirm}) == 1);
        CHECK(asked.contains(QStringLiteral("Remove the word %1 from the book's dictionary?")
                                 .arg(QLocale().quoteString(QStringLiteral("Kirran")))));
        REQUIRE(coordinator.addWord(QStringLiteral("Kirran")));
        CHECK(answering(
                  [&]() {
                      CHECK(coordinator.removeEntries(
                          Part::Words, {QStringLiteral("Kirran"), QStringLiteral("Vael")}));
                  },
                  {confirm}) == 1);
        CHECK(asked.contains(
            QStringLiteral("Remove the chosen words from the book's dictionary (2)?")));
        CHECK(dictionary.words().isEmpty());

        // Nothing to remove: no question
        CHECK(answering([&]() {
                  CHECK_FALSE(coordinator.removeEntries(Part::Names, {QStringLiteral("missing")}));
                  CHECK_FALSE(coordinator.removeEntries(Part::Rules, {QStringLiteral("missing")}));
                  CHECK_FALSE(coordinator.removeEntries(Part::Words, {}));
              }) == 0);
    }

    SECTION("a word of the page; Move to Your Words needs the spelling") {
        emit panel->addWordRequested(QStringLiteral("Zorblat"));
        CHECK(dictionary.hasWord(QStringLiteral("Zorblat")));
        emit panel->moveToUserWordsRequested({QStringLiteral("Zorblat")});
        CHECK(dictionary.hasWord(QStringLiteral("Zorblat")));
        CHECK(statusBar.currentMessage() == QStringLiteral("Your words cannot be changed now"));
        CHECK_FALSE(coordinator.moveToUserWords({}));
    }
}

TEST_CASE("Book dictionary: the spelling dictionary of the book's language gives the forms",
          "[gui][dictionary][coordinator][spelling]") {
    const KeptSettings kept;
    const OpenBook open;
    QStatusBar statusBar;
    QTabWidget tabs;
    tabs.resize(800, 600);
    tabs.show();
    gui::SpellingCoordinator spelling(OpenBook::book(), kalahari::test::commands(), &tabs,
                                      &statusBar);
    editor::SpellCheckService* service = spelling.service();
    REQUIRE(service != nullptr);
    REQUIRE(waitUntil(
        [service]() {
            return service->currentDictionary() == QStringLiteral("pl_PL") &&
                   !service->isLoading();
        },
        DICTIONARY_WAIT_MS));
    BookDictionaryCoordinator coordinator(OpenBook::book(), kalahari::test::commands(), &tabs,
                                          &spelling, &statusBar);
    const core::BookDictionary& dictionary = OpenBook::book().bookDictionary();

    // The window of a name says the dictionary knows its words
    QString hint;
    CHECK(answering([&]() { coordinator.addName(QStringLiteral("Anna")); },
                    {[&hint](QDialog& dialog) {
                        hint = child<QLabel>(dialog, "nameHint")->text();
                        dialog.reject();
                    }}) == 1);
    CHECK(hint.startsWith(QStringLiteral("The spelling dictionary knows the words of the name")));

    // The window of a rule shows the forms it covers, and the rule has them
    QString forms;
    CHECK(answering([&]() { CHECK(coordinator.addRule()); },
                    {[&forms](QDialog& dialog) {
                        type(child<QLineEdit>(dialog, "ruleRight"), QStringLiteral("Smok"));
                        forms = child<QLabel>(dialog, "ruleForms")->text();
                        pressAction(dialog);
                    }}) == 1);
    CHECK(forms.startsWith(QStringLiteral("Forms from the spelling dictionary: ")));
    REQUIRE(dictionary.rules().size() == 1);
    CHECK(dictionary.rules().front().forms.value(QStringLiteral("smoka")) ==
          QStringLiteral("Smoka"));

    // A word of the book moved to the writer's words
    const QString word = QStringLiteral("Kalaharix");
    REQUIRE(coordinator.addWord(word));
    CHECK(coordinator.moveToUserWords({word}));
    CHECK_FALSE(dictionary.hasWord(word));
    CHECK(service->isInUserDictionary(word));
    service->removeFromUserDictionary(word);
}

// =============================================================================
// Settings and the book window
// =============================================================================

TEST_CASE("Book dictionary: Settings > Editor > Spelling", "[gui][dictionary][settings]") {
    const KeptSettings kept;
    settings().set<bool>("bookDictionary.markRules", true);
    settings().set<std::string>("bookDictionary.panelAddsTo", "book");

    SECTION("without a book: the dictionary of a book cannot be opened") {
        gui::EditorSpellingPage page(nullptr, false);
        page.load();
        const auto* openButton = child<QPushButton>(page, "openBookDictionary");
        CHECK_FALSE(openButton->isEnabled());
        CHECK(openButton->toolTip() ==
              QStringLiteral("Open a book first: each book has its own dictionary"));
        CHECK(child<QCheckBox>(page, "bookDictionaryMarkRules")->isChecked());
        CHECK(child<QComboBox>(page, "bookDictionaryPanelAddsTo")->currentData().toString() ==
              QStringLiteral("book"));
    }

    SECTION("the settings, applied") {
        gui::EditorSpellingPage page(nullptr, true);
        page.load();
        child<QCheckBox>(page, "bookDictionaryMarkRules")->setChecked(false);
        auto* addsTo = child<QComboBox>(page, "bookDictionaryPanelAddsTo");
        addsTo->setCurrentIndex(addsTo->findData(QStringLiteral("user")));
        page.apply();
        CHECK_FALSE(settings().get<bool>("bookDictionary.markRules", true));
        CHECK(settings().get<std::string>("bookDictionary.panelAddsTo", "") == "user");
    }

    SECTION("Open Book Dictionary closes the settings, applied") {
        const OpenBook open;
        gui::SettingsDialog dialog(OpenBook::book(), kalahari::test::commands(), nullptr);
        REQUIRE(dialog.openPage(QString::fromLatin1(gui::SettingsDialog::SPELLING_PAGE)));
        CHECK_FALSE(dialog.isBookDictionaryAsked());
        auto* openButton = child<QPushButton>(dialog, "openBookDictionary");
        CHECK(openButton->isEnabled());
        openButton->click();
        CHECK(dialog.result() == QDialog::Accepted);
        CHECK(dialog.isBookDictionaryAsked());
    }
}

TEST_CASE("Book dictionary: in the book window", "[gui][dictionary][window]") {
    const KeptSettings kept;
    const QTemporaryDir folder;
    REQUIRE(folder.isValid());
    core::ProjectManager::loadBookTypes({builtInTypes()});
    auto window = std::make_unique<gui::MainWindow>();
    window->show();
    REQUIRE(window->book().createProject(folder.path(), QStringLiteral("Rain"),
                                         QStringLiteral("Author"), QStringLiteral("pl"), true));
    auto* tabs = window->findChild<gui::CentralTabs*>();
    REQUIRE(tabs != nullptr);
    const auto front = [tabs]() {
        return qobject_cast<BookDictionaryPanel*>(gui::DocumentWindows::of(*tabs).frontPage());
    };

    // Book > Book Dictionary...
    gui::Command* command = window->commands().getCommand(BookDictionaryCoordinator::COMMAND_ID);
    REQUIRE(command != nullptr);
    REQUIRE(command->execute);
    command->execute();
    BookDictionaryPanel* panel = front();
    REQUIRE(panel != nullptr);

    // Find goes to its search
    gui::Command* find = window->commands().getCommand("edit.find");
    REQUIRE(find != nullptr);
    REQUIRE(find->isEnabled);
    CHECK(find->isEnabled());
    if (canActivateWindows()) {
        window->activateWindow();
        REQUIRE(waitUntil([&window]() { return window->isActiveWindow(); }));
        find->execute();
        CHECK(panel->searchField()->hasFocus());
    }

    // The Navigator: the page again
    gui::DocumentWindows::removePage(panel);
    panel->deleteLater();
    auto* navigator = window->findChild<gui::NavigatorPanel*>();
    REQUIRE(navigator != nullptr);
    emit navigator->requestOpenBookDictionary(true);
    REQUIRE(front() != nullptr);

    // Settings > Editor > Spelling from the gear of the page: Open Book Dictionary
    BookDictionaryPanel* shown = front();
    gui::DocumentWindows::removePage(shown);
    shown->deleteLater();
    const Answer openDictionary = [](QDialog& dialog) {
        child<QPushButton>(dialog, "openBookDictionary")->click();
    };
    CHECK(answering(
              [&window]() {
                  window->findChild<gui::SettingsCoordinator*>()->openSettingsPage(
                      QString::fromLatin1(gui::SettingsDialog::SPELLING_PAGE));
              },
              {openDictionary}) == 1);
    REQUIRE(front() != nullptr);

    // The page closes with the book
    REQUIRE(window->book().closeProject(false));
    CHECK(waitUntil([&window]() { return window->findChild<BookDictionaryPanel*>() == nullptr; }));
    CHECK_FALSE(find->isEnabled());
}
