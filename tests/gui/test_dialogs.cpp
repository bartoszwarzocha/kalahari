/// @file test_dialogs.cpp
/// @brief The program's own dialogs: their common base and the Navigator's dialogs

#include <catch2/catch_test_macros.hpp>
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/dialogs/new_element_dialog.h"
#include "kalahari/gui/dialogs/new_item_dialog.h"
#include "kalahari/gui/dialogs/rename_element_dialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <optional>

using namespace kalahari::gui::dialogs;
using kalahari::core::KindRef;
using kalahari::core::ProjectElement;

namespace {

/// @brief The labels of a dialog that show a text, also while the dialog is hidden
bool showsText(const QDialog& dialog, const QString& text) {
    for (const QLabel* label : dialog.findChildren<QLabel*>()) {
        if (label->isVisibleTo(&dialog) && label->text().contains(text)) {
            return true;
        }
    }
    return false;
}

/// @brief Type a letter into a field, as the writer does
void type(QLineEdit* field, const QString& letter) {
    QKeyEvent press(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, letter);
    QApplication::sendEvent(field, &press);
}

/// @brief The book types of the resources folder
const kalahari::core::BookTypeRegistry& builtInTypes() {
    static const kalahari::core::BookTypeRegistry registry = [] {
        kalahari::core::BookTypeRegistry loaded;
        loaded.load({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
        return loaded;
    }();
    return registry;
}

/// @brief Kind @p id of the base package of the resources folder
KindRef baseKind(const QString& id) {
    return builtInTypes().findKind(QStringLiteral("kalahari.base"), id);
}

/// @brief Kind @p id of the novel package of the resources folder
KindRef novelKind(const QString& id) {
    return builtInTypes().findKind(QStringLiteral("kalahari.novel"), id);
}

/// @brief Element @p id of the body of a novel: a chapter, or a part with @p elements
ProjectElement element(const QString& id, const QString& title,
                       const QList<ProjectElement>& elements = {}) {
    ProjectElement made;
    made.id = id;
    made.kind = {QStringLiteral("kalahari.base"),
                 id.startsWith(QStringLiteral("part")) ? QStringLiteral("part")
                                                       : QStringLiteral("chapter")};
    made.title = title;
    made.elements = elements;
    return made;
}

/// @brief The body of a novel: "Part One" (Chapter 1, Chapter 2), "Chapter 3", "Part Two"
/// (Chapter 4)
QList<ProjectElement> novelBody() {
    return {element(QStringLiteral("part1"), QStringLiteral("Part One"),
                    {element(QStringLiteral("c1"), QStringLiteral("Chapter 1")),
                     element(QStringLiteral("c2"), QStringLiteral("Chapter 2"))}),
            element(QStringLiteral("c3"), QStringLiteral("Chapter 3")),
            element(QStringLiteral("part2"), QStringLiteral("Part Two"),
                    {element(QStringLiteral("c4"), QStringLiteral("Chapter 4"))})};
}

/// @brief The option of the place with text @p text
QRadioButton* option(const QDialog& dialog, const QString& text) {
    for (QRadioButton* button : dialog.findChildren<QRadioButton*>()) {
        if (button->text() == text) {
            return button;
        }
    }
    return nullptr;
}

/// @brief The item of the list of the body with text @p text
QTreeWidgetItem* listItem(const QDialog& dialog, const QString& text) {
    auto* list = dialog.findChild<QTreeWidget*>();
    REQUIRE(list != nullptr);
    for (QTreeWidgetItemIterator it(list); *it; ++it) {
        if ((*it)->text(0) == text) {
            return *it;
        }
    }
    return nullptr;
}

/// @brief The text of the item before which the new element @p title is in the list, and of
/// the item it is in: "Part One > Chapter 1"; "> Part One" for the start of the body;
/// "Part Two >" for the end of Part Two
std::string shownPlace(const QDialog& dialog, const QString& title) {
    QTreeWidgetItem* shown = listItem(dialog, title);
    REQUIRE(shown != nullptr);
    CHECK(shown->font(0).bold());
    CHECK(shown->isSelected());
    const QTreeWidgetItem* parent = shown->parent();
    const QTreeWidget* list = shown->treeWidget();
    const int index = parent ? parent->indexOfChild(shown) : list->indexOfTopLevelItem(shown);
    const int count = parent ? parent->childCount() : list->topLevelItemCount();
    const QString in = parent ? parent->text(0) : QString();
    const QString next = index + 1 < count
                             ? (parent ? parent->child(index + 1) : list->topLevelItem(index + 1))
                                   ->text(0)
                             : QString();
    return (in + QStringLiteral(" > ") + next).trimmed().toStdString();
}

/// @brief Press @p key in @p widget
void press(QWidget* widget, Qt::Key key) {
    QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
    QApplication::sendEvent(widget, &event);
}

} // anonymous namespace

TEST_CASE("Own dialogs: the heading and the buttons of the base", "[gui][dialogs]") {
    KalahariDialog dialog;
    dialog.setHeading(QStringLiteral("Add Chapter"), QStringLiteral("Where it goes"));
    dialog.setAcceptText(QStringLiteral("Add"));

    CHECK(dialog.windowTitle() == QStringLiteral("Add Chapter"));
    CHECK(showsText(dialog, QStringLiteral("Add Chapter")));
    CHECK(showsText(dialog, QStringLiteral("Where it goes")));
    CHECK(dialog.acceptButton()->text() == QStringLiteral("Add"));
    CHECK(dialog.acceptButton()->isDefault());
    CHECK(dialog.cancelButton()->isVisibleTo(&dialog));

    SECTION("Without a description the heading shows only the title") {
        dialog.setHeading(QStringLiteral("Rename"));
        CHECK_FALSE(showsText(dialog, QStringLiteral("Where it goes")));
    }

    SECTION("Apply is hidden until shown, and it keeps the dialog open") {
        CHECK_FALSE(dialog.applyButton()->isVisibleTo(&dialog));
        dialog.setApplyButtonVisible(true);
        CHECK(dialog.applyButton()->isVisibleTo(&dialog));

        int applied = 0;
        QObject::connect(&dialog, &KalahariDialog::applyClicked, [&applied]() { ++applied; });
        dialog.show();
        dialog.applyButton()->click();
        CHECK(applied == 1);
        CHECK(dialog.isVisible());

        dialog.acceptButton()->click();
        CHECK_FALSE(dialog.isVisible());
        CHECK(dialog.result() == QDialog::Accepted);
    }

    SECTION("Cancel closes it without accepting") {
        dialog.show();
        dialog.cancelButton()->click();
        CHECK_FALSE(dialog.isVisible());
        CHECK(dialog.result() == QDialog::Rejected);
    }
}

TEST_CASE("Own dialogs: a compact heading is lower", "[gui][dialogs]") {
    // The Settings window needs a heading that does not take room from its pages
    KalahariDialog regular;
    regular.setHeading(QStringLiteral("Settings"));
    KalahariDialog compact;
    compact.setHeading(QStringLiteral("Settings"));
    compact.setCompactHeading(true);

    const auto* regularHeading = regular.findChild<QFrame*>(QStringLiteral("kalahariDialogHeading"));
    const auto* compactHeading = compact.findChild<QFrame*>(QStringLiteral("kalahariDialogHeading"));
    REQUIRE(regularHeading != nullptr);
    REQUIRE(compactHeading != nullptr);
    CHECK(compactHeading->sizeHint().height() < regularHeading->sizeHint().height());
}

TEST_CASE("Own dialogs: a dialog opens as high as its content needs", "[gui][dialogs]") {
    // Regression: the height was measured for a narrower dialog, where the sentence in the
    // heading takes more lines, so the wider dialog had a gap above its buttons
    RenameElementDialog dialog(QStringLiteral("Chapter One: The Beginning"),
                               QStringLiteral("template.chapter"));
    dialog.adjustSize();  // what showing the dialog does

    CHECK(dialog.width() >= dialog.minimumWidth());
    CHECK(dialog.height() == dialog.layout()->totalHeightForWidth(dialog.width()));
}

TEST_CASE("Navigator: the new chapter dialog asks for a title", "[gui][dialogs]") {
    const KindRef chapter = baseKind(QStringLiteral("chapter"));
    REQUIRE(chapter);
    NewElementDialog dialog(NewElementKind::Chapter, {{chapter, QStringLiteral("Chapter 3")}}, 0,
                            QStringLiteral("Part One"));

    CHECK(dialog.kind().kind == chapter.kind);
    CHECK(dialog.findChild<QComboBox*>() == nullptr);  // one kind: nothing to choose
    CHECK(showsText(dialog, QStringLiteral("Part One")));  // the part it goes to

    auto* title = dialog.findChild<QLineEdit*>();
    REQUIRE(title != nullptr);
    CHECK(title->text() == QStringLiteral("Chapter 3"));
    CHECK(title->selectedText() == title->text());  // typing replaces it
    CHECK(dialog.acceptButton()->isEnabled());

    title->setText(QStringLiteral("   "));
    CHECK_FALSE(dialog.acceptButton()->isEnabled());

    title->setText(QStringLiteral("  The Storm "));
    CHECK(dialog.acceptButton()->isEnabled());
    CHECK(dialog.title() == QStringLiteral("The Storm"));
}

TEST_CASE("Navigator: a new chapter without a part goes to the body", "[gui][dialogs]") {
    const KindRef chapter = baseKind(QStringLiteral("chapter"));
    REQUIRE(chapter);
    NewElementDialog dialog(NewElementKind::Chapter, {{chapter, QStringLiteral("Chapter 1")}});

    CHECK(showsText(dialog, QStringLiteral("body of the book")));
    CHECK(dialog.title() == QStringLiteral("Chapter 1"));
}

TEST_CASE("Navigator: the new part dialog asks for a title", "[gui][dialogs]") {
    const KindRef part = baseKind(QStringLiteral("part"));
    REQUIRE(part);
    NewElementDialog dialog(NewElementKind::Part, {{part, QStringLiteral("Part 2")}});

    CHECK(dialog.kind().kind == part.kind);
    CHECK(dialog.findChild<QComboBox*>() == nullptr);
    CHECK(dialog.title() == QStringLiteral("Part 2"));
}

TEST_CASE("Navigator: a new element of more than one kind gets the title of its kind",
          "[gui][dialogs]") {
    const KindRef titlePage = baseKind(QStringLiteral("title_page"));
    const KindRef dedication = baseKind(QStringLiteral("dedication"));
    const KindRef preface = baseKind(QStringLiteral("preface"));
    REQUIRE(titlePage);
    REQUIRE(dedication);
    REQUIRE(preface);
    const QList<NewElementChoice> choices = {{titlePage, QStringLiteral("Title Page")},
                                             {dedication, QStringLiteral("Dedication")},
                                             {preface, QStringLiteral("Preface")}};

    SECTION("The writer chooses the kind") {
        NewElementDialog dialog(NewElementKind::FrontMatterItem, choices);
        auto* kinds = dialog.findChild<QComboBox*>();
        auto* title = dialog.findChild<QLineEdit*>();
        REQUIRE(kinds != nullptr);
        REQUIRE(title != nullptr);

        // Kinds are named in the language of the program
        const QString language =
            QString::fromStdString(kalahari::core::SettingsManager::getInstance().getLanguage());
        REQUIRE(kinds->count() == 3);
        CHECK(kinds->itemText(1) == dedication.kind->name.text(language));
        CHECK(dialog.kind().kind == titlePage.kind);
        CHECK(dialog.title() == QStringLiteral("Title Page"));

        kinds->setCurrentIndex(1);
        CHECK(dialog.kind().kind == dedication.kind);
        CHECK(dialog.title() == QStringLiteral("Dedication"));

        // A title the writer typed stays when the kind changes
        title->setText(QStringLiteral("For "));
        title->end(false);
        type(title, QStringLiteral("M"));
        REQUIRE(dialog.title() == QStringLiteral("For M"));
        kinds->setCurrentIndex(2);
        CHECK(dialog.kind().kind == preface.kind);
        CHECK(dialog.title() == QStringLiteral("For M"));
    }

    SECTION("The dialog starts with the given kind") {
        NewElementDialog dialog(NewElementKind::BackMatterItem, choices, 2);
        auto* kinds = dialog.findChild<QComboBox*>();
        REQUIRE(kinds != nullptr);
        CHECK(kinds->currentIndex() == 2);
        CHECK(dialog.kind().kind == preface.kind);
        CHECK(dialog.title() == QStringLiteral("Preface"));
    }
}

TEST_CASE("Navigator: the rename dialog starts with the current name", "[gui][dialogs]") {
    RenameElementDialog dialog(QStringLiteral("Chapter One"), QStringLiteral("template.chapter"));

    CHECK(showsText(dialog, QStringLiteral("Chapter One")));
    auto* name = dialog.findChild<QLineEdit*>();
    REQUIRE(name != nullptr);
    CHECK(name->text() == QStringLiteral("Chapter One"));
    CHECK(name->selectedText() == name->text());
    CHECK(dialog.acceptButton()->isEnabled());

    name->setText(QString());
    CHECK_FALSE(dialog.acceptButton()->isEnabled());

    name->setText(QStringLiteral(" The Beginning  "));
    CHECK(dialog.name() == QStringLiteral("The Beginning"));
}

TEST_CASE("Navigator: the writer chooses where the prologue goes", "[gui][dialogs]") {
    const KindRef prologue = novelKind(QStringLiteral("prologue"));
    const KindRef chapter = baseKind(QStringLiteral("chapter"));
    REQUIRE(prologue);
    REQUIRE(chapter);
    NewElementDialog dialog(NewElementKind::Chapter, {{prologue, QStringLiteral("Prologue")},
                                                      {chapter, QStringLiteral("Chapter 5")}});
    dialog.setBody(novelBody(), builtInTypes());
    CHECK(showsText(dialog, QStringLiteral("Choose where the new element goes in the book.")));

    // At the start of the body, as the first option says
    QRadioButton* start = option(dialog, QStringLiteral("At the start of the body of the book"));
    QRadioButton* firstPart = option(dialog, QStringLiteral("First in \"Part One\""));
    QRadioButton* elsewhere = option(dialog, QStringLiteral("Elsewhere: show the place in the list"));
    REQUIRE(start != nullptr);
    REQUIRE(firstPart != nullptr);
    REQUIRE(elsewhere != nullptr);
    CHECK(start->isVisibleTo(&dialog));
    CHECK(start->isChecked());
    CHECK(dialog.place() == NewElementPlace{QString(), 0});
    CHECK(shownPlace(dialog, QStringLiteral("Prologue")) == "> Part One");

    // First in the first part
    firstPart->click();
    CHECK(dialog.place() == NewElementPlace{QStringLiteral("part1"), 0});
    CHECK(shownPlace(dialog, QStringLiteral("Prologue")) == "Part One > Chapter 1");

    // Moved down with the button, and with the key in the list: any place in a part, between
    // the parts and at the end
    auto* list = dialog.findChild<QTreeWidget*>();
    REQUIRE(list != nullptr);
    QPushButton* down = nullptr;
    QPushButton* up = nullptr;
    for (QPushButton* button : dialog.findChildren<QPushButton*>()) {
        if (button->toolTip() == QStringLiteral("Move the new element down")) {
            down = button;
        } else if (button->toolTip() == QStringLiteral("Move the new element up")) {
            up = button;
        }
    }
    REQUIRE(down != nullptr);
    REQUIRE(up != nullptr);
    down->click();
    CHECK(dialog.place() == NewElementPlace{QStringLiteral("part1"), 1});
    CHECK(elsewhere->isChecked());
    press(list, Qt::Key_Down);
    press(list, Qt::Key_Down);
    CHECK(dialog.place() == NewElementPlace{QString(), 1});
    CHECK(shownPlace(dialog, QStringLiteral("Prologue")) == "> Chapter 3");
    for (int i = 0; i < 10; ++i) {
        down->click();
    }
    CHECK(dialog.place() == NewElementPlace{QString(), 3});
    CHECK(shownPlace(dialog, QStringLiteral("Prologue")) == ">");
    CHECK_FALSE(down->isEnabled());
    CHECK(up->isEnabled());

    // Clicking an element puts the new one before it
    emit list->itemClicked(listItem(dialog, QStringLiteral("Chapter 4")), 0);
    CHECK(dialog.place() == NewElementPlace{QStringLiteral("part2"), 0});
    emit list->itemClicked(listItem(dialog, QStringLiteral("Part One")), 0);
    CHECK(dialog.place() == NewElementPlace{QString(), 0});
    CHECK(start->isChecked());
    CHECK_FALSE(up->isEnabled());

    // The title the writer types shows in the list
    auto* title = dialog.findChild<QLineEdit*>();
    REQUIRE(title != nullptr);
    title->setText(QStringLiteral("Before It All"));
    CHECK(shownPlace(dialog, QStringLiteral("Before It All")) == "> Part One");

    // A chapter takes the place of its kind: the dialog does not ask
    auto* kinds = dialog.findChild<QComboBox*>();
    REQUIRE(kinds != nullptr);
    kinds->setCurrentIndex(1);
    CHECK_FALSE(dialog.place().has_value());
    CHECK_FALSE(list->isVisibleTo(&dialog));
    CHECK(showsText(dialog, QStringLiteral("body of the book")));

    // Back to the prologue: the start of the body again
    kinds->setCurrentIndex(0);
    CHECK(list->isVisibleTo(&dialog));
    CHECK(dialog.place() == NewElementPlace{QString(), 0});
}

TEST_CASE("Navigator: the epilogue starts at the end of the part the dialog was opened on",
          "[gui][dialogs]") {
    const KindRef epilogue = novelKind(QStringLiteral("epilogue"));
    REQUIRE(epilogue);
    NewElementDialog dialog(NewElementKind::Chapter, {{epilogue, QStringLiteral("Epilogue")}}, 0,
                            QStringLiteral("Part One"));
    dialog.setBody(novelBody(), builtInTypes(), QStringLiteral("part1"));

    CHECK(dialog.place() == NewElementPlace{QStringLiteral("part1"), 2});
    CHECK(shownPlace(dialog, QStringLiteral("Epilogue")) == "Part One >");
    QRadioButton* end = option(dialog, QStringLiteral("At the end of the body of the book"));
    QRadioButton* lastPart = option(dialog, QStringLiteral("Last in \"Part Two\""));
    REQUIRE(end != nullptr);
    REQUIRE(lastPart != nullptr);
    CHECK(option(dialog, QStringLiteral("Elsewhere: show the place in the list"))->isChecked());

    lastPart->click();
    CHECK(dialog.place() == NewElementPlace{QStringLiteral("part2"), 1});
    end->click();
    CHECK(dialog.place() == NewElementPlace{QString(), 3});

    SECTION("Without parts, the end of the body or a place in the list") {
        dialog.setBody({element(QStringLiteral("c1"), QStringLiteral("Chapter 1"))},
                       builtInTypes());
        CHECK(dialog.place() == NewElementPlace{QString(), 1});
        CHECK(end->isVisibleTo(&dialog));
        CHECK_FALSE(option(dialog, QStringLiteral("Last in \"Part Two\""))->isVisibleTo(&dialog));
    }
}

TEST_CASE("Navigator: without the body of the book the prologue opens it", "[gui][dialogs]") {
    // A book without the parts layer: the dialog does not ask for the place
    const KindRef prologue = novelKind(QStringLiteral("prologue"));
    REQUIRE(prologue);
    NewElementDialog dialog(NewElementKind::Chapter, {{prologue, QStringLiteral("Prologue")}});

    CHECK_FALSE(dialog.place().has_value());
    CHECK(showsText(dialog,
                    QStringLiteral("The element is added as the first one in the body of the book.")));
}

TEST_CASE("New Book: the dialog checks the folder of the book", "[gui][dialogs]") {
    // Regression: a folder with files was found only after the open book was closed
    QTemporaryDir dir;
    const QDir parent(dir.path());
    REQUIRE(parent.mkpath(QStringLiteral("Taken")));
    QFile notes(parent.filePath(QStringLiteral("Taken/notes.txt")));
    REQUIRE(notes.open(QIODevice::WriteOnly));
    notes.write("mine");
    notes.close();

    NewItemDialog dialog(NewItemMode::Project);
    QLineEdit* title = nullptr;
    QLineEdit* location = nullptr;
    for (QLineEdit* field : dialog.findChildren<QLineEdit*>()) {
        if (field->placeholderText() == QStringLiteral("Enter book title...")) {
            title = field;
        } else if (field->placeholderText() == QStringLiteral("Select book folder...")) {
            location = field;
        }
    }
    QPushButton* create = nullptr;
    for (QPushButton* button : dialog.findChildren<QPushButton*>()) {
        if (button->text() == QStringLiteral("Create Book")) {
            create = button;
        }
    }
    auto* subfolder = dialog.findChild<QCheckBox*>();
    const auto* folderLine = dialog.findChild<QLabel*>(QStringLiteral("newBookFolderLabel"));
    REQUIRE(title != nullptr);
    REQUIRE(location != nullptr);
    REQUIRE(create != nullptr);
    REQUIRE(subfolder != nullptr);
    REQUIRE(folderLine != nullptr);

    location->setText(dir.path());
    title->setText(QStringLiteral("New One"));
    CHECK(create->isEnabled());
    CHECK(folderLine->text() == QStringLiteral("The book will be created in the folder 'New One'."));

    title->setText(QStringLiteral("Taken"));
    CHECK_FALSE(create->isEnabled());
    CHECK(folderLine->text().startsWith(
        QStringLiteral("The folder 'Taken' is already in this location and is not empty.")));

    SECTION("Without a subfolder the chosen folder itself must be new or empty") {
        subfolder->setChecked(false);
        location->setText(parent.filePath(QStringLiteral("Taken")));
        CHECK_FALSE(create->isEnabled());
        CHECK(folderLine->text().startsWith(QStringLiteral("The folder 'Taken' is not empty.")));

        location->setText(parent.filePath(QStringLiteral("Empty")));
        CHECK(create->isEnabled());
        CHECK(folderLine->text() ==
              QStringLiteral("The book will be created in the folder 'Empty'."));
    }

    SECTION("Without a title there is no folder") {
        title->clear();
        CHECK_FALSE(create->isEnabled());
        CHECK(folderLine->text().isEmpty());
    }
}
