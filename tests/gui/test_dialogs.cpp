/// @file test_dialogs.cpp
/// @brief The program's own dialogs: their common base and the Navigator's dialogs

#include <catch2/catch_test_macros.hpp>
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/dialogs/new_element_dialog.h"
#include "kalahari/gui/dialogs/new_item_dialog.h"
#include "kalahari/gui/dialogs/rename_element_dialog.h"
#include "kalahari/gui/section_words.h"

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
#include <QListWidget>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include <memory>
#include <string>
#include <vector>

using namespace kalahari::gui::dialogs;
using kalahari::core::BookPlace;
using kalahari::core::ElementPlace;
using kalahari::core::KindRef;
using kalahari::core::ProjectElement;
using kalahari::gui::SectionWords;

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
/// the item it is in: "Part One > Chapter 1"; "Main Section > Part One" for the start of the
/// main section; "Part Two >" for the end of Part Two
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

/// @brief Place @p index of the main section: in group @p groupId, or in the section itself
ElementPlace mainAt(const QString& groupId, qsizetype index) {
    return {BookPlace::Main, groupId, index};
}

/// @brief Show @p elements as the main section of a book named with the first set of names,
/// the dialog opened on group @p groupId or on the section
void setMain(NewElementDialog& dialog, const QList<ProjectElement>& elements,
             const QString& groupId = QString()) {
    dialog.setSection(elements, QStringLiteral("Main Section"),
                      SectionWords::forPart(nullptr, BookPlace::Main), builtInTypes(), groupId);
}

/// @brief The epilogue of a novel, @p id
ProjectElement epilogue(const QString& id) {
    ProjectElement made;
    made.id = id;
    made.kind = {QStringLiteral("kalahari.novel"), QStringLiteral("epilogue")};
    made.title = QStringLiteral("Epilogue");
    return made;
}

/// @brief The button next to the list of the book with tooltip @p toolTip
QPushButton* moveButton(const QDialog& dialog, const QString& toolTip) {
    for (QPushButton* button : dialog.findChildren<QPushButton*>()) {
        if (button->toolTip() == toolTip) {
            return button;
        }
    }
    return nullptr;
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

    CHECK(showsText(dialog,
                    QStringLiteral("The chapter is added as the last one in the main section.")));
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

TEST_CASE("Navigator: sentences name the parts of the book after its set of names",
          "[gui][dialogs]") {
    // Without a book: the first set
    const SectionWords first = SectionWords::forPart(nullptr, BookPlace::Main);
    CHECK(first.inPart == QStringLiteral("in the main section"));
    CHECK(first.atStart == QStringLiteral("at the start of the main section"));
    CHECK(first.atEnd == QStringLiteral("at the end of the main section"));
    CHECK(SectionWords::capitalized(first.atEnd) ==
          QStringLiteral("At the end of the main section"));

    kalahari::core::ProjectBook book;
    book.sectionSet = QStringLiteral("matter");
    CHECK(SectionWords::forPart(&book, BookPlace::Main).atEnd ==
          QStringLiteral("at the end of the body"));
    CHECK(SectionWords::forPart(&book, BookPlace::Front).inPart ==
          QStringLiteral("in the front matter"));

    // The writer's own names are quoted
    book.sectionSet = QString::fromLatin1(kalahari::core::ProjectBook::CUSTOM_SECTIONS);
    book.sectionNames = {QStringLiteral("Opening"), QStringLiteral("Story"),
                         QStringLiteral("Notes")};
    CHECK(SectionWords::forPart(&book, BookPlace::Main).atStart ==
          QStringLiteral("at the start of the section \"Story\""));

    // A book without sections: the place in the book
    book.partsLayer = false;
    CHECK(SectionWords::forPart(&book, BookPlace::Front).atEnd ==
          QStringLiteral("before the content of the book"));
    CHECK(SectionWords::forPart(&book, BookPlace::Back).atStart ==
          QStringLiteral("after the content of the book"));
    CHECK(SectionWords::forPart(&book, BookPlace::Main).inPart ==
          QStringLiteral("in the content of the book"));
}

TEST_CASE("Navigator: the writer chooses where the prologue goes", "[gui][dialogs]") {
    const KindRef prologue = novelKind(QStringLiteral("prologue"));
    const KindRef chapter = baseKind(QStringLiteral("chapter"));
    REQUIRE(prologue);
    REQUIRE(chapter);
    NewElementDialog dialog(NewElementKind::Chapter,
                            {{prologue, QStringLiteral("Prologue"), mainAt(QString(), 0)},
                             {chapter, QStringLiteral("Chapter 5"), mainAt(QString(), 3)}});
    setMain(dialog, novelBody());
    CHECK(showsText(dialog, QStringLiteral("Choose where the new prologue goes in the book.")));

    // At the start of the main section, as the first option says
    QRadioButton* start = option(dialog, QStringLiteral("At the start of the main section"));
    QRadioButton* firstPart = option(dialog, QStringLiteral("First in the part \"Part One\""));
    QRadioButton* elsewhere = option(dialog, QStringLiteral("Elsewhere: show the place in the list"));
    REQUIRE(start != nullptr);
    REQUIRE(firstPart != nullptr);
    REQUIRE(elsewhere != nullptr);
    CHECK(start->isVisibleTo(&dialog));
    CHECK(start->isChecked());
    CHECK(dialog.place() == mainAt(QString(), 0));
    CHECK(shownPlace(dialog, QStringLiteral("Prologue")) == "Main Section > Part One");

    // First in the first part, where it still opens the book
    firstPart->click();
    CHECK(dialog.place() == mainAt(QStringLiteral("part1"), 0));
    CHECK(shownPlace(dialog, QStringLiteral("Prologue")) == "Part One > Chapter 1");
    CHECK_FALSE(showsText(dialog, QStringLiteral("will not be the first element")));

    // Moved down with the button, and with the key in the list: any place in a part, between
    // the parts and at the end; after a chapter the dialog says that it does not open the book
    auto* list = dialog.findChild<QTreeWidget*>();
    REQUIRE(list != nullptr);
    QPushButton* up = moveButton(dialog, QStringLiteral("Move the element up"));
    QPushButton* down = moveButton(dialog, QStringLiteral("Move the element down"));
    REQUIRE(down != nullptr);
    REQUIRE(up != nullptr);
    down->click();
    CHECK(dialog.place() == mainAt(QStringLiteral("part1"), 1));
    CHECK(elsewhere->isChecked());
    CHECK(showsText(dialog, QStringLiteral("\"Prologue\" will not be the first element in the "
                                           "main section.")));
    press(list, Qt::Key_Down);
    press(list, Qt::Key_Down);
    CHECK(dialog.place() == mainAt(QString(), 1));
    CHECK(shownPlace(dialog, QStringLiteral("Prologue")) == "Main Section > Chapter 3");
    for (int i = 0; i < 10; ++i) {
        down->click();
    }
    CHECK(dialog.place() == mainAt(QString(), 3));
    CHECK(shownPlace(dialog, QStringLiteral("Prologue")) == "Main Section >");
    CHECK_FALSE(down->isEnabled());
    CHECK(up->isEnabled());

    // Clicking an element puts the new one before it; clicking the section, first in it
    emit list->itemClicked(listItem(dialog, QStringLiteral("Chapter 4")), 0);
    CHECK(dialog.place() == mainAt(QStringLiteral("part2"), 0));
    emit list->itemClicked(listItem(dialog, QStringLiteral("Main Section")), 0);
    CHECK(dialog.place() == mainAt(QString(), 0));
    CHECK(start->isChecked());
    CHECK_FALSE(up->isEnabled());
    CHECK_FALSE(showsText(dialog, QStringLiteral("will not be the first element")));

    // The title the writer types shows in the list
    auto* title = dialog.findChild<QLineEdit*>();
    REQUIRE(title != nullptr);
    title->setText(QStringLiteral("Before It All"));
    CHECK(shownPlace(dialog, QStringLiteral("Before It All")) == "Main Section > Part One");

    // A chapter takes the place of its kind: the dialog does not ask
    auto* kinds = dialog.findChild<QComboBox*>();
    REQUIRE(kinds != nullptr);
    kinds->setCurrentIndex(1);
    CHECK(dialog.place() == mainAt(QString(), 3));
    CHECK_FALSE(list->isVisibleTo(&dialog));
    CHECK(showsText(dialog,
                    QStringLiteral("The chapter is added as the last one in the main section.")));

    // Back to the prologue: the start of the main section again
    kinds->setCurrentIndex(0);
    CHECK(list->isVisibleTo(&dialog));
    CHECK(dialog.place() == mainAt(QString(), 0));
}

TEST_CASE("Navigator: the epilogue starts at the end of the part the dialog was opened on",
          "[gui][dialogs]") {
    const KindRef epilogueKind = novelKind(QStringLiteral("epilogue"));
    REQUIRE(epilogueKind);
    NewElementDialog dialog(
        NewElementKind::Chapter,
        {{epilogueKind, QStringLiteral("Epilogue"), mainAt(QStringLiteral("part1"), 2)}}, 0,
        QStringLiteral("Part One"));
    setMain(dialog, novelBody(), QStringLiteral("part1"));

    // There it does not end the book, and the dialog says so
    CHECK(dialog.place() == mainAt(QStringLiteral("part1"), 2));
    CHECK(shownPlace(dialog, QStringLiteral("Epilogue")) == "Part One >");
    CHECK(showsText(dialog, QStringLiteral(
                                "\"Epilogue\" will not be the last element in the main section.")));
    QRadioButton* end = option(dialog, QStringLiteral("At the end of the main section"));
    QRadioButton* lastPart = option(dialog, QStringLiteral("Last in the part \"Part Two\""));
    REQUIRE(end != nullptr);
    REQUIRE(lastPart != nullptr);
    CHECK(option(dialog, QStringLiteral("Elsewhere: show the place in the list"))->isChecked());

    lastPart->click();
    CHECK(dialog.place() == mainAt(QStringLiteral("part2"), 1));
    CHECK_FALSE(showsText(dialog, QStringLiteral("will not be the last element")));
    end->click();
    CHECK(dialog.place() == mainAt(QString(), 3));

    SECTION("Without parts, the end of the main section or a place in the list") {
        setMain(dialog, {element(QStringLiteral("c1"), QStringLiteral("Chapter 1"))});
        CHECK(dialog.place() == mainAt(QString(), 1));
        QRadioButton* endNow = option(dialog, QStringLiteral("At the end of the main section"));
        REQUIRE(endNow != nullptr);
        CHECK(endNow->isChecked());
        CHECK(option(dialog, QStringLiteral("Last in the part \"Part Two\"")) == nullptr);
    }
}

TEST_CASE("Navigator: without the part of the book the dialog only says where the element goes",
          "[gui][dialogs]") {
    const KindRef prologue = novelKind(QStringLiteral("prologue"));
    REQUIRE(prologue);
    NewElementDialog dialog(NewElementKind::Chapter,
                            {{prologue, QStringLiteral("Prologue"), mainAt(QString(), 0)}});

    CHECK(dialog.place() == mainAt(QString(), 0));
    CHECK_FALSE(dialog.findChild<QTreeWidget*>()->isVisibleTo(&dialog));
    CHECK(showsText(dialog,
                    QStringLiteral("The prologue is added as the first one in the main section.")));
}

TEST_CASE("Navigator: in a book without sections the list has no row of its part",
          "[gui][dialogs]") {
    const KindRef prologue = novelKind(QStringLiteral("prologue"));
    REQUIRE(prologue);
    kalahari::core::ProjectBook book;
    book.partsLayer = false;
    NewElementDialog dialog(NewElementKind::Chapter,
                            {{prologue, QStringLiteral("Prologue"), mainAt(QString(), 0)}});
    dialog.setSection(novelBody(), QString(), SectionWords::forPart(&book, BookPlace::Main),
                      builtInTypes());

    QRadioButton* start =
        option(dialog, QStringLiteral("At the start of the content of the book"));
    REQUIRE(start != nullptr);
    CHECK(start->isChecked());
    CHECK(shownPlace(dialog, QStringLiteral("Prologue")) == "> Part One");
}

TEST_CASE("Navigator: a new chapter goes before the epilogue that ends the last part",
          "[gui][dialogs]") {
    const KindRef chapter = baseKind(QStringLiteral("chapter"));
    REQUIRE(chapter);
    const QList<ProjectElement> body = {
        element(QStringLiteral("part1"), QStringLiteral("Part One"),
                {element(QStringLiteral("c1"), QStringLiteral("Chapter 1")),
                 epilogue(QStringLiteral("epi"))})};

    SECTION("Opened on the main section: before the epilogue, or after the part") {
        NewElementDialog dialog(
            NewElementKind::Chapter,
            {{chapter, QStringLiteral("Chapter 2"), mainAt(QStringLiteral("part1"), 1)}});
        setMain(dialog, body);

        // Before the epilogue, at the end of the part, as the first option says
        CHECK(showsText(dialog, QStringLiteral("\"Epilogue\" is the last element in the main "
                                               "section, so the new chapter goes before it, at "
                                               "the end of the part \"Part One\". You can choose "
                                               "another place.")));
        QRadioButton* inPart =
            option(dialog,
                   QStringLiteral("Before \"Epilogue\", at the end of the part \"Part One\""));
        QRadioButton* afterPart =
            option(dialog, QStringLiteral("At the end of the main section, after \"Part One\""));
        REQUIRE(inPart != nullptr);
        REQUIRE(afterPart != nullptr);
        CHECK(inPart->isChecked());
        CHECK(dialog.place() == mainAt(QStringLiteral("part1"), 1));
        CHECK(shownPlace(dialog, QStringLiteral("Chapter 2")) == "Part One > Epilogue");
        CHECK_FALSE(showsText(dialog, QStringLiteral("will no longer be the last element")));

        // After the part the epilogue no longer ends the book, and the dialog says so
        afterPart->click();
        CHECK(dialog.place() == mainAt(QString(), 1));
        CHECK(shownPlace(dialog, QStringLiteral("Chapter 2")) == "Main Section >");
        CHECK(showsText(dialog, QStringLiteral("\"Epilogue\" will no longer be the last element "
                                               "in the main section.")));
    }

    SECTION("Opened on the part: before the epilogue, without asking") {
        NewElementDialog dialog(
            NewElementKind::Chapter,
            {{chapter, QStringLiteral("Chapter 2"), mainAt(QStringLiteral("part1"), 1)}}, 0,
            QStringLiteral("Part One"));
        setMain(dialog, body, QStringLiteral("part1"));

        CHECK(dialog.place() == mainAt(QStringLiteral("part1"), 1));
        CHECK_FALSE(dialog.findChild<QTreeWidget*>()->isVisibleTo(&dialog));
        CHECK(showsText(dialog, QStringLiteral("The chapter is added at the end of the part "
                                               "\"Part One\", before \"Epilogue\".")));
    }
}

TEST_CASE("Navigator: the dialog warns when the new element makes the epilogue stop ending "
          "the book",
          "[gui][dialogs]") {
    const KindRef chapter = baseKind(QStringLiteral("chapter"));
    REQUIRE(chapter);
    const QList<ProjectElement> body = {
        element(QStringLiteral("part1"), QStringLiteral("Part One"),
                {element(QStringLiteral("c1"), QStringLiteral("Chapter 1")),
                 epilogue(QStringLiteral("epi"))}),
        element(QStringLiteral("part2"), QStringLiteral("Part Two"))};
    NewElementDialog dialog(
        NewElementKind::Chapter,
        {{chapter, QStringLiteral("Chapter 2"), mainAt(QStringLiteral("part2"), 0)}}, 0,
        QStringLiteral("Part Two"));
    setMain(dialog, body, QStringLiteral("part2"));

    // The place the writer asked for, with what it changes
    QRadioButton* inPart = option(dialog, QStringLiteral("Last in the part \"Part Two\""));
    REQUIRE(inPart != nullptr);
    CHECK(inPart->isChecked());
    CHECK(dialog.place() == mainAt(QStringLiteral("part2"), 0));
    CHECK(showsText(dialog, QStringLiteral("Choose where the new chapter goes in the book.")));
    CHECK(showsText(dialog, QStringLiteral("\"Epilogue\" will no longer be the last element in "
                                           "the main section.")));

    // Before the epilogue it stays the last one
    auto* list = dialog.findChild<QTreeWidget*>();
    REQUIRE(list != nullptr);
    emit list->itemClicked(listItem(dialog, QStringLiteral("Epilogue")), 0);
    CHECK(dialog.place() == mainAt(QStringLiteral("part1"), 1));
    CHECK_FALSE(showsText(dialog, QStringLiteral("will no longer be the last element")));
}

TEST_CASE("Navigator: a new part takes the epilogue that ends the book", "[gui][dialogs]") {
    const KindRef part = baseKind(QStringLiteral("part"));
    REQUIRE(part);
    const QList<ProjectElement> body = {
        element(QStringLiteral("part1"), QStringLiteral("Part One"),
                {element(QStringLiteral("c1"), QStringLiteral("Chapter 1")),
                 epilogue(QStringLiteral("epi"))})};
    NewElementDialog dialog(NewElementKind::Part,
                            {{part, QStringLiteral("Part Two"), mainAt(QString(), 1)}});
    setMain(dialog, body);
    // The main texts of a novel, which the dialog names
    dialog.setMainKind(baseKind(QStringLiteral("chapter")));
    CHECK(showsText(dialog, QStringLiteral(
                                "The part is added at the end of the main section, after \"Part "
                                "One\".")));

    // By default the epilogue goes to the end of the new part, where it stays the last element
    auto* take = dialog.findChild<QCheckBox*>();
    REQUIRE(take != nullptr);
    CHECK(take->isVisibleTo(&dialog));
    CHECK(take->text() == QStringLiteral("Move \"Epilogue\" to the end of the new part"));
    CHECK(take->isChecked());
    CHECK(dialog.takeInside() == QStringList{QStringLiteral("epi")});
    CHECK(dialog.place() == mainAt(QString(), 1));
    CHECK(showsText(dialog, QStringLiteral("\"Epilogue\" is now the last element in the main "
                                           "section. At the end of the new part it stays the "
                                           "last one.")));

    // The list only shows the book as it will be
    CHECK(shownPlace(dialog, QStringLiteral("Part Two")) == "Main Section >");
    CHECK(listItem(dialog, QStringLiteral("Epilogue"))->parent() ==
          listItem(dialog, QStringLiteral("Part Two")));
    CHECK(listItem(dialog, QStringLiteral("Part One"))->childCount() == 1);
    CHECK(showsText(dialog, QStringLiteral("The list only shows how the book will look.")));
    CHECK_FALSE(moveButton(dialog, QStringLiteral("Move the element up"))->isVisibleTo(&dialog));
    CHECK_FALSE(option(dialog, QStringLiteral("Elsewhere: show the place in the list"))
                    ->isVisibleTo(&dialog));

    // Left where it is, the chapters of the new part go after it
    take->setChecked(false);
    CHECK(dialog.takeInside().isEmpty());
    CHECK(showsText(dialog, QStringLiteral("\"Epilogue\" stays where it is, and the chapters "
                                           "added to the new part go after it.")));
    CHECK(listItem(dialog, QStringLiteral("Epilogue"))->parent() ==
          listItem(dialog, QStringLiteral("Part One")));
    CHECK(listItem(dialog, QStringLiteral("Part Two"))->childCount() == 0);

    SECTION("Without an epilogue at the end of the parts there is nothing to take") {
        NewElementDialog plain(NewElementKind::Part,
                               {{part, QStringLiteral("Part Three"), mainAt(QString(), 3)}});
        setMain(plain, novelBody());
        CHECK_FALSE(plain.findChild<QCheckBox*>()->isVisibleTo(&plain));
        CHECK(plain.takeInside().isEmpty());
        CHECK(showsText(plain, QStringLiteral("The part is added at the end of the main section, "
                                              "after \"Part Two\".")));
        CHECK(shownPlace(plain, QStringLiteral("Part Three")) == "Main Section >");
    }
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

namespace {

/// @brief The New Book window with the built-in book types, and its fields of sections
struct NewBook {
    NewBook() {
        kalahari::core::ProjectManager::getInstance().loadBookTypes(
            {QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
        dialog = std::make_unique<NewItemDialog>(NewItemMode::Project);
        sections = dialog->findChild<QComboBox*>(QStringLiteral("newBookSectionsCombo"));
        namesRow = dialog->findChild<QWidget*>(QStringLiteral("newBookSectionNames"));
        REQUIRE(sections != nullptr);
        REQUIRE(namesRow != nullptr);
        names = namesRow->findChildren<QLineEdit*>();
        REQUIRE(names.size() == 3);
        for (QComboBox* combo : dialog->findChildren<QComboBox*>()) {
            if (combo->findData(QStringLiteral("pl")) >= 0) {
                language = combo;
            }
        }
        REQUIRE(language != nullptr);
    }

    /// Choose the template @p id ("template.novel")
    void choose(const char* id) const {
        auto* templates = dialog->findChild<QListWidget*>();
        REQUIRE(templates != nullptr);
        for (int i = 0; i < templates->count(); ++i) {
            if (templates->item(i)->data(Qt::UserRole).toString() == QLatin1String(id)) {
                templates->setCurrentRow(i);
                return;
            }
        }
        FAIL("No template " << id);
    }

    /// Choose the book's language @p code ("pl")
    void speak(const char* code) const {
        language->setCurrentIndex(language->findData(QString::fromLatin1(code)));
    }

    /// Choose the item of the Sections field whose value is @p item
    void chooseSections(const char* item) const {
        const int index = sections->findData(QString::fromLatin1(item));
        REQUIRE(index >= 0);
        sections->setCurrentIndex(index);
    }

    /// The texts of the items of the Sections field
    std::string items() const {
        QStringList texts;
        for (int i = 0; i < sections->count(); ++i) {
            texts << sections->itemText(i);
        }
        return texts.join(QStringLiteral(" | ")).toStdString();
    }

    /// The fields of own names: their texts, or their placeholders
    std::string namesOf(bool placeholders = false) const {
        QStringList texts;
        for (const QLineEdit* name : names) {
            texts << (placeholders ? name->placeholderText() : name->text());
        }
        return texts.join(QStringLiteral(", ")).toStdString();
    }

    /// Fill in a title and a new folder, and create the book
    NewItemResult create(const QTemporaryDir& dir) const {
        for (QLineEdit* field : dialog->findChildren<QLineEdit*>()) {
            if (field->placeholderText() == QStringLiteral("Enter book title...")) {
                field->setText(QStringLiteral("Sections"));
            } else if (field->placeholderText() == QStringLiteral("Select book folder...")) {
                field->setText(dir.path());
            }
        }
        for (QPushButton* button : dialog->findChildren<QPushButton*>()) {
            if (button->text() == QStringLiteral("Create Book")) {
                REQUIRE(button->isEnabled());
                button->click();
            }
        }
        REQUIRE(dialog->QDialog::result() == QDialog::Accepted);
        return dialog->result();
    }

    std::unique_ptr<NewItemDialog> dialog;
    QComboBox* sections = nullptr;
    QComboBox* language = nullptr;
    QWidget* namesRow = nullptr;
    QList<QLineEdit*> names;
};

} // namespace

TEST_CASE("New Book: the sections of the book, in its language", "[gui][dialogs]") {
    using kalahari::core::BookSections;
    auto& settings = kalahari::core::SettingsManager::getInstance();
    settings.resetToDefaults();  // each section starts without the choice the last one saved
    const QString custom = QString::fromLatin1(kalahari::core::ProjectBook::CUSTOM_SECTIONS);
    QTemporaryDir dir;

    NewBook book;
    book.speak("en");
    book.choose("template.novel");

    // The sets of names in the language of the book, own names and no sections
    CHECK(book.items() ==
          "Front Section \u00B7 Main Section \u00B7 Back Section | Front Matter \u00B7 Body "
          "\u00B7 Back Matter | Opening Fragment \u00B7 Main Fragment \u00B7 Closing Fragment | "
          "Opening \u00B7 Development \u00B7 Closing | Custom Names | No Sections");
    book.speak("pl");
    CHECK(book.items() ==
          "Sekcja początkowa \u00B7 Sekcja główna \u00B7 Sekcja końcowa | Strony początkowe "
          "\u00B7 Tekst główny \u00B7 Strony końcowe | Fragment początkowy \u00B7 Fragment "
          "główny \u00B7 Fragment końcowy | Otwarcie \u00B7 Rozwinięcie \u00B7 Zamknięcie | "
          "Custom Names | No Sections");

    // A novel shows the first set, and no row of own names
    CHECK(book.sections->currentData().toString() == QStringLiteral("sections"));
    CHECK_FALSE(book.namesRow->isVisibleTo(book.dialog.get()));

    // A user project and a screenplay start without sections; the novel keeps the choice
    book.chooseSections("matter");
    book.choose("template.empty");
    CHECK(book.sections->currentData().toString() == QStringLiteral("none"));
    book.choose("template.screenplay");
    CHECK(book.sections->currentData().toString() == QStringLiteral("none"));
    book.choose("template.shortStories");
    CHECK(book.sections->currentData().toString() == QStringLiteral("matter"));

    SECTION("Own names start as the names of the set chosen before") {
        book.chooseSections("custom");
        CHECK(book.namesRow->isVisibleTo(book.dialog.get()));
        CHECK(book.namesOf() == "Strony początkowe, Tekst główny, Strony końcowe");
        CHECK(book.namesOf(true) == "Sekcja początkowa, Sekcja główna, Sekcja końcowa");

        // In the language of the book, until the writer types in them
        book.speak("en");
        CHECK(book.namesOf() == "Front Matter, Body, Back Matter");
        CHECK(book.namesOf(true) == "Front Section, Main Section, Back Section");
        type(book.names.at(1), QStringLiteral("x"));
        book.names.at(1)->setText(QStringLiteral(" Story "));
        book.names.at(2)->clear();
        book.speak("pl");
        CHECK(book.namesOf() == "Front Matter,  Story , ");

        // The book gets them without the spaces at their ends, and the next book starts with
        // them
        const NewItemResult result = book.create(dir);
        REQUIRE(result.sections.has_value());
        CHECK(*result.sections ==
              BookSections{true, custom,
                           {QStringLiteral("Front Matter"), QStringLiteral("Story"), QString()}});
        CHECK(settings.get<std::string>("project.sectionSet") == "custom");
        CHECK(settings.get<std::vector<std::string>>("project.sectionNames") ==
              std::vector<std::string>{"Front Matter", "Story", ""});

        NewBook next;
        next.choose("template.novel");
        CHECK(next.sections->currentData().toString() == custom);
        CHECK(next.namesOf() == "Front Matter, Story, ");
        next.speak("en");
        CHECK(next.namesOf() == "Front Matter, Story, ");  // the writer's names stay
        next.choose("template.empty");
        CHECK(next.sections->currentData().toString() == QStringLiteral("none"));
    }

    SECTION("A book without sections keeps the names of the first set") {
        book.choose("template.novel");
        book.chooseSections("none");
        const NewItemResult result = book.create(dir);
        REQUIRE(result.sections.has_value());
        CHECK(*result.sections == BookSections{false, QString(), {}});
        CHECK(settings.get<std::string>("project.sectionSet") == "none");

        NewBook next;
        next.choose("template.poetry");
        CHECK(next.sections->currentData().toString() == QStringLiteral("none"));
    }

    SECTION("The choice for a type without sections is not remembered") {
        book.choose("template.screenplay");
        book.chooseSections("arc");
        const NewItemResult result = book.create(dir);
        REQUIRE(result.sections.has_value());
        CHECK(*result.sections == BookSections{true, QStringLiteral("arc"), {}});
        CHECK(settings.get<std::string>("project.sectionSet") == "sections");

        NewBook next;
        next.choose("template.novel");
        CHECK(next.sections->currentData().toString() == QStringLiteral("sections"));
    }
}
