/// @file test_field_coordinator.cpp
/// @brief The fields of the open texts: what they show of the book, Insert > Date && Time...,
///        Insert > Field..., the date and the time at once, Edit Field... and their windows

#include <catch2/catch_test_macros.hpp>
#include "dialog_checks.h"
#include "document_window.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/text_fields.h"
#include "kalahari/gui/command_registrar.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/dialogs/field_dialogs.h"
#include "kalahari/gui/field_coordinator.h"
#include "kalahari/gui/settings/shortcuts_page.h"
#include "kalahari/gui/settings_dialog.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QFile>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QKeyEvent>
#include <QTabWidget>
#include <QTextDocument>
#include <QTimer>
#include <QToolButton>
#include <QTranslator>
#include <QTreeWidget>

#include <cstdio>
#include <functional>
#include <map>
#include <optional>

using namespace kalahari;
using editor::FieldType;
using editor::TextField;
using gui::CommandRegistry;
using gui::FieldCoordinator;
using gui::dialogs::DateTimeDialog;
using gui::dialogs::FieldDialog;
using test::checkKeys;
using test::checkSmallScreen;
using test::layOut;
using test::OpenNovel;
using test::Polish;
using test::SettingsOpener;
using test::Window;

namespace {

/// The fields over the tabs of a window, as the main window has them
struct Fields {
    editor::BookEditor* keys = nullptr;  ///< The text with the keys
    FieldCoordinator coordinator;

    explicit Fields(Window& window)
        : coordinator(window.tabs, [this]() { return keys; }, &window.window) {
        coordinator.connectCommands();
    }
};

/// The settings of the date as they were, until the test ends
class DateSettings {
public:
    DateSettings()
        : m_format(core::SettingsManager::getInstance().get<std::string>(
              FieldCoordinator::DATE_FORMAT_KEY, "long")),
          m_asField(core::SettingsManager::getInstance().get<bool>(
              FieldCoordinator::DATE_AS_FIELD_KEY, false)) {}
    ~DateSettings() {
        try {
            auto& settings = core::SettingsManager::getInstance();
            settings.set<std::string>(FieldCoordinator::DATE_FORMAT_KEY, m_format);
            settings.set<bool>(FieldCoordinator::DATE_AS_FIELD_KEY, m_asField);
        } catch (...) {
            // A destructor may not throw: the later tests then find the date's settings changed
            std::fputs("DateSettings: the settings of the date were not restored\n", stderr);
        }
    }
    DateSettings(const DateSettings&) = delete;
    DateSettings& operator=(const DateSettings&) = delete;

private:
    std::string m_format;
    bool m_asField;
};

/// Run @p command; the window of type @p Dialog it shows is answered by @p answer (which
/// chooses in it and accepts or rejects it)
/// @return Whether the window showed
template <typename Dialog>
bool answering(const std::function<void()>& command, const std::function<void(Dialog&)>& answer) {
    bool shown = false;
    QTimer timer;
    timer.setInterval(10);
    QObject::connect(&timer, &QTimer::timeout, [&shown, &answer]() {
        auto* dialog = qobject_cast<Dialog*>(test::shownDialog());
        if (dialog == nullptr || dialog->property("handled").toBool()) {
            return;
        }
        dialog->setProperty("handled", true);
        shown = true;
        answer(*dialog);
    });
    timer.start();
    command();
    return shown;
}

/// The row of a list whose item has @p data
int rowOf(const QListWidget& list, const QVariant& data) {
    int found = -1;
    for (int row = 0; row < list.count() && found < 0; ++row) {
        if (list.item(row)->data(Qt::UserRole) == data) {
            found = row;
        }
    }
    // A failed REQUIRE ends the test; nothing after it is unreachable for the compilers
    INFO("The row of " << data.toString().toStdString());
    REQUIRE(found >= 0);
    return found;
}

/// The one widget of a type in a window
template <typename Widget>
Widget* only(const QWidget& root) {
    const QList<Widget*> found = root.findChildren<Widget*>();
    REQUIRE(found.size() == 1);
    return found.first();
}

/// The list of the tab Insert > Field... shows
QListWidget* shownList(const QWidget& dialog) {
    const QWidget* page = only<QTabWidget>(dialog)->currentWidget();
    REQUIRE(page != nullptr);
    auto* list = page->findChild<QListWidget*>();
    REQUIRE(list != nullptr);
    return list;
}

/// A field chosen in Insert > Field...: its tab shown, its row current
void chooseField(const QWidget& dialog, FieldType type) {
    QTabWidget* tabs = only<QTabWidget>(dialog);
    bool found = false;
    for (int tab = 0; tab < tabs->count() && !found; ++tab) {
        auto* list = tabs->widget(tab)->findChild<QListWidget*>();
        for (int row = 0; list != nullptr && row < list->count() && !found; ++row) {
            if (list->item(row)->data(Qt::UserRole).toInt() == static_cast<int>(type)) {
                tabs->setCurrentIndex(tab);
                list->setCurrentRow(row);
                found = true;
            }
        }
    }
    INFO("The field of type " << static_cast<int>(type));
    REQUIRE(found);
}

/// The fields of an editor, in text order
QList<TextField> fieldsOf(const editor::BookEditor& text) {
    QList<TextField> fields;
    const int end = text.textDocument()->characterCount();
    for (int position = 0; position < end; ++position) {
        if (const std::optional<TextField> field = text.textFieldAt(position)) {
            fields.append(*field);
        }
    }
    return fields;
}

/// The text of an editor as it shows, each field with its text
QString shownText(const editor::BookEditor& text) {
    const QTextDocument& document = *text.textDocument();
    QString shown;
    for (int position = 0; position + 1 < document.characterCount(); ++position) {
        const QString field = text.fieldTextAt(position);
        if (!field.isEmpty()) {
            shown += field;
        } else if (const QChar character = document.characterAt(position);
                   character == QChar::ParagraphSeparator) {
            shown += QLatin1Char('\n');
        } else {
            shown += character;
        }
    }
    return shown;
}

}  // namespace

TEST_CASE("Fields: the open texts show the book's title, author, language and their titles",
          "[gui][fields]") {
    OpenNovel book(QStringLiteral("The Desert"));
    Window window;
    Fields fields(window);
    window.navigator.onElementSelected(book.chapterId, QStringLiteral("The Storm"));
    editor::BookEditor* text = window.editor(window.tabs->currentIndex())->getBookEditor();
    fields.coordinator.refresh();  // as the window does when the text comes to the front

    editor::FieldContext context = text->fieldContext();
    CHECK(context.bookTitle == QStringLiteral("The Desert"));
    CHECK(context.author == QStringLiteral("Author"));
    CHECK(context.textTitle == QStringLiteral("The Storm"));
    CHECK(context.language == QStringLiteral("en"));
    CHECK(context.now.isValid());
    CHECK(fields.coordinator.contextFor(book.chapterId).bookTitle == context.bookTitle);

    // The text renamed in the Navigator: its title follows
    REQUIRE(book.pm.renameElement(book.chapterId, QStringLiteral("The Rain")));
    CHECK(text->fieldContext().textTitle == QStringLiteral("The Rain"));

    // A file outside the book: no book's values, its name, the program's language
    const editor::FieldContext outside =
        fields.coordinator.contextFor(QString(), QStringLiteral("Notes"));
    CHECK(outside.bookTitle.isEmpty());
    CHECK(outside.author.isEmpty());
    CHECK(outside.textTitle == QStringLiteral("Notes"));
    CHECK_FALSE(outside.language.isEmpty());

    // The names in the window: the title and the words named after the kind of the text
    const auto names = FieldCoordinator::fieldNames(book.chapterId);
    CHECK(names.at(static_cast<size_t>(FieldType::TextTitle)) == QStringLiteral("Chapter Title"));
    CHECK(names.at(static_cast<size_t>(FieldType::Words)) ==
          QStringLiteral("Chapter Word Count"));
    CHECK(names.at(static_cast<size_t>(FieldType::BookTitle)) == QStringLiteral("Book Title"));
    CHECK(FieldCoordinator::fieldNames(QString()).at(static_cast<size_t>(FieldType::TextTitle)) ==
          QStringLiteral("Text Title"));
}

TEST_CASE("Fields: Insert > Field... inserts the field chosen, Edit Field... changes it",
          "[gui][fields]") {
    gui::registerAllCommands(gui::CommandCallbacks{});
    OpenNovel book(QStringLiteral("The Desert"));
    Window window;
    Fields fields(window);
    window.writeChapter(book.chapterId, QStringLiteral("Page "));
    editor::BookEditor* text = window.editor(window.tabs->currentIndex())->getBookEditor();
    fields.keys = text;
    text->setCursorPosition(editor::CursorPosition{0, 5});

    // The page number, in Roman numerals; the window shows how it looks where it goes
    QString preview;
    QString hint;
    CHECK(answering<FieldDialog>(
        [&fields]() { fields.coordinator.insertField(); },
        [&preview, &hint](FieldDialog& dialog) {
            // The fields in four tabs: the book, the text, the pages, the date
            QTabWidget* tabs = only<QTabWidget>(dialog);
            REQUIRE(tabs->count() == 4);
            CHECK(tabs->tabText(0) == QStringLiteral("&Book"));
            CHECK(tabs->tabText(3) == QStringLiteral("&Date"));
            int fieldCount = 0;
            for (int tab = 0; tab < tabs->count(); ++tab) {
                fieldCount += tabs->widget(tab)->findChild<QListWidget*>()->count();
            }
            CHECK(fieldCount == static_cast<int>(editor::FIELD_TYPES.size()));
            CHECK(tabs->currentIndex() == 0);
            CHECK(shownList(dialog)->currentRow() == 0);  // the book's title first
            CHECK(dialog.field().type == FieldType::BookTitle);
            tabs->setCurrentIndex(1);
            CHECK(shownList(dialog)->item(0)->text() == QStringLiteral("Chapter Title"));
            CHECK(dialog.field().type == FieldType::TextTitle);
            chooseField(dialog, FieldType::Page);
            auto* formats = only<QComboBox>(dialog);
            REQUIRE(formats->count() == 3);
            CHECK(formats->itemText(1) == QStringLiteral("i, ii, iii"));
            formats->setCurrentIndex(1);
            for (const QLabel* label : dialog.findChildren<QLabel*>()) {
                if (label->text().startsWith(QStringLiteral("The page number"))) {
                    hint = label->text();
                }
            }
            preview = dialog.field().format;
            CHECK(dialog.acceptButton()->text() == QStringLiteral("Insert"));
            dialog.acceptButton()->click();
        }));
    CHECK(preview == QStringLiteral("i"));
    CHECK(hint.contains(QStringLiteral("Page Layout view")));
    REQUIRE(fieldsOf(*text).size() == 1);
    CHECK(fieldsOf(*text).first() == TextField{FieldType::Page, QStringLiteral("i")});
    CHECK(shownText(*text) == QStringLiteral("Page #"));  // the Continuous view

    // Edit Field... (a double click, the context menu): the window opens on it, OK changes it
    const int position = 5;
    REQUIRE(text->textFieldAt(position).has_value());
    CHECK(answering<FieldDialog>(
        [&]() { CHECK(fields.coordinator.editField(*text, position)); },
        [](FieldDialog& dialog) {
            CHECK(only<QTabWidget>(dialog)->currentIndex() == 2);  // the pages
            CHECK(shownList(dialog)->currentRow() ==
                  rowOf(*shownList(dialog), static_cast<int>(FieldType::Page)));
            CHECK(only<QComboBox>(dialog)->currentIndex() == 1);
            CHECK(dialog.acceptButton()->text() == QStringLiteral("OK"));
            chooseField(dialog, FieldType::BookTitle);
            only<QComboBox>(dialog)->setCurrentIndex(1);  // in capitals
            dialog.acceptButton()->click();
        }));
    CHECK(fieldsOf(*text).first() == TextField{FieldType::BookTitle, QStringLiteral("upper")});
    CHECK(shownText(*text) == QStringLiteral("Page THE DESERT"));

    // The same field again: nothing changes; Cancel: nothing either
    CHECK(answering<FieldDialog>(
        [&]() { CHECK_FALSE(fields.coordinator.editField(*text, position)); },
        [](FieldDialog& dialog) { dialog.acceptButton()->click(); }));
    CHECK(answering<FieldDialog>(
        [&]() { CHECK_FALSE(fields.coordinator.editField(*text, position)); },
        [](FieldDialog& dialog) { dialog.reject(); }));

    // The double click on a field asks the coordinator
    CHECK(answering<FieldDialog>([&]() { emit text->fieldEditRequested(position); },
                                 [](FieldDialog& dialog) { dialog.reject(); }));

    // A text open read-only: neither Insert > Field... nor Edit Field...
    text->setReadOnly(true);
    CHECK_FALSE(FieldCoordinator::whyNoField(*text).isEmpty());
    CHECK_FALSE(answering<FieldDialog>([&]() { fields.coordinator.insertField(); },
                                       [](FieldDialog& dialog) { dialog.reject(); }));
    CHECK_FALSE(fields.coordinator.editField(*text, position));
    text->setReadOnly(false);

    // A plain text file keeps no fields
    text->setAnnotationsAllowed(false);
    CHECK(FieldCoordinator::whyNoField(*text) == QStringLiteral("A plain text file keeps no fields"));
    text->setAnnotationsAllowed(true);
    CHECK(FieldCoordinator::whyNoField(*text).isEmpty());
}

TEST_CASE("Fields: Insert > Date && Time... as text or as a field, and the date at once",
          "[gui][fields]") {
    gui::registerAllCommands(gui::CommandCallbacks{});
    const DateSettings kept;
    auto& settings = core::SettingsManager::getInstance();
    settings.set<std::string>(FieldCoordinator::DATE_FORMAT_KEY, "long");
    settings.set<bool>(FieldCoordinator::DATE_AS_FIELD_KEY, false);
    OpenNovel book(QStringLiteral("The Desert"));
    Window window;
    Fields fields(window);
    window.writeChapter(book.chapterId, QStringLiteral("x"));
    editor::BookEditor* text = window.editor(window.tabs->currentIndex())->getBookEditor();
    fields.keys = text;
    text->setCursorPosition(editor::CursorPosition{0, 1});

    // The keys, which no menu has, as the window says
    const gui::Command* date = CommandRegistry::getInstance().getCommand("insert.date");
    REQUIRE(date != nullptr);
    CHECK_FALSE(date->showInMenu);
    CHECK(date->menuPath.rfind("INSERT/", 0) == 0);
    CHECK(window.window.actions().contains(
        CommandRegistry::getInstance().getAction(std::string("insert.date"))));
    // Settings > Keyboard Shortcuts lists them under Insert, with their keys
    {
        gui::ShortcutsPage page;
        page.load();
        auto* list = page.findChild<QTreeWidget*>(QStringLiteral("shortcutsList"));
        REQUIRE(list != nullptr);
        QStringList inserts;
        for (int top = 0; top < list->topLevelItemCount(); ++top) {
            const QTreeWidgetItem* group = list->topLevelItem(top);
            if (group->text(0) != QStringLiteral("Insert")) {
                continue;
            }
            for (int row = 0; row < group->childCount(); ++row) {
                inserts << group->child(row)->text(0) + QLatin1Char('=') +
                               group->child(row)->text(1);
            }
        }
        const QString keys = date->shortcut.toQKeySequence().toString(QKeySequence::NativeText);
        INFO(inserts.join(QStringLiteral(" | ")).toStdString());
        CHECK(inserts.contains(QStringLiteral("Today's Date as Text=") + keys));
        CHECK(inserts.indexOf(QStringLiteral("Today's Date as Text=") + keys) >
              inserts.indexOf(QStringLiteral("Date & Time...=")));
    }
#ifndef Q_OS_MACOS
    CHECK(date->shortcut.toQKeySequence() == QKeySequence(QStringLiteral("Alt+Shift+D")));
    CHECK(CommandRegistry::getInstance().getCommand("insert.time")->shortcut.toQKeySequence() ==
          QKeySequence(QStringLiteral("Alt+Shift+T")));
#endif

    // The ISO date as text; the format is kept
    QString said;
    CHECK(answering<DateTimeDialog>(
        [&fields]() { fields.coordinator.insertDateTime(); },
        [&said](DateTimeDialog& dialog) {
            auto* list = only<QListWidget>(dialog);
            CHECK(list->count() == 6);
            CHECK(list->currentRow() == 0);  // the long date, as last
            list->setCurrentRow(rowOf(*list, QStringLiteral("iso")));
            CHECK_FALSE(only<QCheckBox>(dialog)->isChecked());
            CHECK(only<QCheckBox>(dialog)->isEnabled());
            said = test::textsOf(dialog);
            dialog.acceptButton()->click();
        }));
    const QString today = QDate::currentDate().toString(Qt::ISODate);
    CHECK(shownText(*text) == QLatin1Char('x') + today);
    CHECK(fieldsOf(*text).isEmpty());
    CHECK(settings.get<std::string>(FieldCoordinator::DATE_FORMAT_KEY, "") == "iso");
    CHECK(said.contains(date->shortcut.toQKeySequence().toString(QKeySequence::NativeText)));

    // Alt+Shift+D: the date in the format chosen last; Alt+Shift+T: the time
    text->insertText(QStringLiteral(" "));
    fields.coordinator.insertDate();
    CHECK(shownText(*text) == QLatin1Char('x') + today + QLatin1Char(' ') + today);
    settings.set<std::string>(FieldCoordinator::DATE_FORMAT_KEY, "time");
    CHECK(FieldCoordinator::dateFormatOfKeys() == QStringLiteral("long"));
    text->insertText(QStringLiteral(" "));
    fields.coordinator.insertTime();
    CHECK(shownText(*text).size() > (today.size() * 2) + 3);
    CHECK(fieldsOf(*text).isEmpty());

    // Update Automatically: a date field, kept for the next time
    text->selectAll();
    text->insertText(QString());
    CHECK(answering<DateTimeDialog>(
        [&fields]() { fields.coordinator.insertDateTime(); },
        [](DateTimeDialog& dialog) {
            auto* list = only<QListWidget>(dialog);
            CHECK(list->currentRow() == rowOf(*list, QStringLiteral("time")));
            list->setCurrentRow(rowOf(*list, QStringLiteral("iso")));
            only<QCheckBox>(dialog)->setChecked(true);
            dialog.acceptButton()->click();
        }));
    REQUIRE(fieldsOf(*text).size() == 1);
    CHECK(fieldsOf(*text).first() == TextField{FieldType::Date, QStringLiteral("iso")});
    CHECK(shownText(*text) == today);
    CHECK(settings.get<bool>(FieldCoordinator::DATE_AS_FIELD_KEY, false));

    // In a plain text file: Update Automatically greyed out, saying why, and the setting kept
    text->setAnnotationsAllowed(false);
    CHECK(answering<DateTimeDialog>(
        [&fields]() { fields.coordinator.insertDateTime(); },
        [](DateTimeDialog& dialog) {
            auto* box = only<QCheckBox>(dialog);
            CHECK_FALSE(box->isEnabled());
            CHECK_FALSE(box->isChecked());
            CHECK(box->toolTip() == QStringLiteral("A plain text file keeps no fields"));
            CHECK_FALSE(dialog.asField());
            dialog.acceptButton()->click();
        }));
    CHECK(fieldsOf(*text).size() == 1);  // the new date as text
    CHECK(settings.get<bool>(FieldCoordinator::DATE_AS_FIELD_KEY, false));
    text->setAnnotationsAllowed(true);

    // Read-only: nothing
    text->setReadOnly(true);
    const QString before = shownText(*text);
    CHECK_FALSE(answering<DateTimeDialog>([&fields]() { fields.coordinator.insertDateTime(); },
                                          [](DateTimeDialog& dialog) { dialog.reject(); }));
    fields.coordinator.insertDate();
    fields.coordinator.insertTime();
    CHECK(shownText(*text) == before);
}

TEST_CASE("Fields: the formats of the windows", "[gui][fields]") {
    const QDateTime when(QDate(2026, 10, 11), QTime(14, 35));
    CHECK(gui::dialogs::fieldFormatText(FieldType::Date, QStringLiteral("long"), when,
                                        QStringLiteral("en-US")) ==
          QStringLiteral("October 11, 2026"));
    CHECK(gui::dialogs::fieldFormatText(FieldType::Date, QStringLiteral("iso"), when,
                                        QStringLiteral("pl")) == QStringLiteral("2026-10-11"));
    CHECK(gui::dialogs::fieldFormatText(FieldType::Page, QStringLiteral("I"), when, QString()) ==
          QStringLiteral("I, II, III"));
    CHECK(gui::dialogs::fieldFormatText(FieldType::Words, QStringLiteral(""), when,
                                        QStringLiteral("en-US")) == QStringLiteral("12,345"));
    CHECK(gui::dialogs::fieldFormatText(FieldType::Words, QStringLiteral("plain"), when,
                                        QStringLiteral("en-US")) == QStringLiteral("12345"));
    CHECK(gui::dialogs::fieldFormatText(FieldType::Author, QStringLiteral("upper"), when,
                                        QString()) == QStringLiteral("IN CAPITALS"));

    // The window shows the field as the text will, on the background of the fields
    FieldDialog::Data data;
    data.field = TextField{FieldType::Words, QStringLiteral("plain")};
    data.now = when;
    data.language = QStringLiteral("en-US");
    data.shown = [](const TextField& field) {
        editor::FieldCounts counts;
        counts.words = 4321;
        editor::FieldContext context;
        context.language = QStringLiteral("en-US");
        return editor::textFieldText(field, context, counts);
    };
    data.background = QColor(QStringLiteral("#dddddd"));
    FieldDialog dialog(data);
    QLabel* preview = nullptr;
    for (QLabel* label : dialog.findChildren<QLabel*>()) {
        if (label->text() == QStringLiteral("4321")) {
            preview = label;
        }
    }
    REQUIRE(preview != nullptr);
    CHECK(preview->autoFillBackground());
    CHECK(preview->palette().color(QPalette::Window) == data.background);
    only<QComboBox>(dialog)->setCurrentIndex(0);
    CHECK(preview->text() == QStringLiteral("4,321"));
    CHECK(dialog.field() == TextField{FieldType::Words, QString()});
}

TEST_CASE("Fields: the windows in Polish, their keys, the gear, a small screen",
          "[gui][fields][dialog]") {
    const Polish polish;
    REQUIRE(polish.loaded());
    QString openedPage;
    const SettingsOpener opener(
        [&openedPage](QWidget*, const QString& page) { openedPage = page; });
    const QDateTime when(QDate(2026, 10, 11), QTime(14, 35));

    DateTimeDialog::Data dateData;
    dateData.now = when;
    dateData.language = QStringLiteral("pl");
    dateData.dateKeys = QStringLiteral("Alt+Shift+D");
    dateData.timeKeys = QStringLiteral("Alt+Shift+T");
    DateTimeDialog dateDialog(dateData);
    layOut(dateDialog);
    CHECK(dateDialog.windowTitle() == QStringLiteral("Data i godzina"));
    auto* dates = only<QListWidget>(dateDialog);
    CHECK(dates->item(0)->text() == QStringLiteral("11 października 2026"));
    CHECK(dates->item(2)->text() == QStringLiteral("niedziela, 11 października 2026"));
    CHECK(dates->item(5)->text() == QStringLiteral("11.10.2026, 14:35"));
    CHECK(QApplication::focusWidget() == dates);
    CHECK(test::textsOf(dateDialog).contains(QStringLiteral(
        "Alt+Shift+D i Alt+Shift+T wstawiają od razu datę i godzinę tekstem")));
    checkKeys(dateDialog);
    checkSmallScreen(dateDialog);

    FieldDialog::Data fieldData;
    fieldData.now = when;
    fieldData.language = QStringLiteral("pl");
    FieldDialog fieldDialog(fieldData);
    layOut(fieldDialog);
    CHECK(fieldDialog.windowTitle() == QStringLiteral("Pole"));
    QTabWidget* tabs = only<QTabWidget>(fieldDialog);
    REQUIRE(tabs->count() == 4);
    CHECK(tabs->tabText(0) == QStringLiteral("&Książka"));
    CHECK(tabs->tabText(1) == QStringLiteral("&Tekst"));
    CHECK(tabs->tabText(2) == QStringLiteral("&Strony"));
    CHECK(tabs->tabText(3) == QStringLiteral("&Data"));
    auto* list = shownList(fieldDialog);
    CHECK(QApplication::focusWidget() == list);
    CHECK(list->item(0)->text() == QStringLiteral("Tytuł książki"));
    CHECK(only<QComboBox>(fieldDialog)->itemText(1) == QStringLiteral("WIELKIMI LITERAMI"));
    checkKeys(fieldDialog);

    // Every field: the window keeps its size, small enough, nothing in it scrolls
    const QSize size = fieldDialog.sizeHint();
    for (int tab = 0; tab < tabs->count(); ++tab) {
        tabs->setCurrentIndex(tab);
        QListWidget* shown = shownList(fieldDialog);
        for (int row = 0; row < shown->count(); ++row) {
            shown->setCurrentRow(row);
            layOut(fieldDialog);
            INFO("tab " << tab << ", row " << row);
            CHECK(fieldDialog.sizeHint().height() <= size.height() + 2);
            checkSmallScreen(fieldDialog);
        }
    }

    // Ctrl+Tab shows the next tab, its list gets the keys
    tabs->setCurrentIndex(0);
    shownList(fieldDialog)->setFocus();
    QKeyEvent nextTab(QEvent::KeyPress, Qt::Key_Tab, Qt::ControlModifier);
    QCoreApplication::sendEvent(shownList(fieldDialog), &nextTab);
    CHECK(tabs->currentIndex() == 1);
    CHECK(QApplication::focusWidget() == shownList(fieldDialog));

    // The gear: Settings > Editor > General, where the gray background is turned off
    QToolButton* gear = nullptr;
    for (QToolButton* each : fieldDialog.findChildren<QToolButton*>()) {
        if (each->toolTip() == QStringLiteral("Ustawienia pól")) {
            gear = each;
        }
    }
    REQUIRE(gear != nullptr);
    gear->click();
    CHECK(openedPage == QString::fromLatin1(gui::SettingsDialog::FIELDS_PAGE));
}
