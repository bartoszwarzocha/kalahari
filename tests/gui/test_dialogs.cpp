/// @file test_dialogs.cpp
/// @brief The program's own dialogs: their common base and the Navigator's dialogs

#include <catch2/catch_test_macros.hpp>
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/chapter_document.h"
#include "kalahari/core/kind_words.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/gui/dialogs/add_to_book_dialog.h"
#include "kalahari/gui/dialogs/delete_dialogs.h"
#include "kalahari/gui/dialogs/file_preview_dialog.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/dialogs/move_element_dialog.h"
#include "kalahari/gui/dialogs/new_element_dialog.h"
#include "kalahari/gui/dialogs/new_item_dialog.h"
#include "kalahari/gui/dialogs/rename_element_dialog.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/section_words.h"
#include "kalahari/gui/widgets/fitting_scroll_area.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTextBrowser>
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
using kalahari::gui::PickerSection;
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

/// @brief Element @p id of kind @p kindId of the base package, titled @p title
ProjectElement baseElement(const QString& kindId, const QString& id, const QString& title) {
    ProjectElement made;
    made.id = id;
    made.kind = {QStringLiteral("kalahari.base"), kindId};
    made.title = title;
    return made;
}

/// @brief The front, main and back section of a book with @p front, @p main and @p back
/// elements: with sections named with the first set of names, or of @p book
QList<PickerSection> bookSections(const QList<ProjectElement>& front,
                                  const QList<ProjectElement>& main,
                                  const QList<ProjectElement>& back,
                                  const kalahari::core::ProjectBook* book = nullptr) {
    const bool rows = !book || book->partsLayer;
    const auto section = [rows, book](BookPlace place, const char* name,
                                      const QList<ProjectElement>& elements) {
        return PickerSection{place, rows ? QString::fromLatin1(name) : QString(),
                             SectionWords::forPart(book, place), elements};
    };
    return {section(BookPlace::Front, "Front Section", front),
            section(BookPlace::Main, "Main Section", main),
            section(BookPlace::Back, "Back Section", back)};
}

/// @brief @p text with the name of the system's Trash for "{bin}"
QString withBin(const QString& text) {
    return kalahari::core::fillWords(text, {{QStringLiteral("bin"), trashWords()}});
}

/// @brief Element @p id of a collection of short stories: a story with file @p file, or a
/// division with @p elements
ProjectElement storiesElement(const QString& kindId, const QString& id, const QString& title,
                              const QString& file = QString(),
                              const QList<ProjectElement>& elements = {}) {
    ProjectElement made;
    made.id = id;
    made.kind = {QStringLiteral("kalahari.short_stories"), kindId};
    made.title = title;
    made.file = file;
    made.elements = elements;
    return made;
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
    CHECK(showsText(dialog, QStringLiteral("Click the element the prologue is to go before, or "
                                           "move it with the buttons")));
    CHECK(list->toolTip() == QStringLiteral("Click the element the prologue is to go before"));
    QPushButton* up = moveButton(dialog, QStringLiteral("Move the prologue up"));
    QPushButton* down = moveButton(dialog, QStringLiteral("Move the prologue down"));
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

TEST_CASE("Navigator: the dialog grows when it shows the list of places", "[gui][dialogs]") {
    // Regression: the window kept the height of a chapter, and the places of a prologue were
    // hidden under the scroll bar of its content
    const KindRef prologue = novelKind(QStringLiteral("prologue"));
    const KindRef chapter = baseKind(QStringLiteral("chapter"));
    REQUIRE(prologue);
    REQUIRE(chapter);
    NewElementDialog dialog(NewElementKind::Chapter,
                            {{chapter, QStringLiteral("Chapter 5"), mainAt(QString(), 3)},
                             {prologue, QStringLiteral("Prologue"), mainAt(QString(), 0)}});
    setMain(dialog, novelBody());
    dialog.show();
    QApplication::processEvents();
    auto* list = dialog.findChild<QTreeWidget*>();
    auto* kinds = dialog.findChild<QComboBox*>();
    auto* area = dialog.findChild<kalahari::gui::FittingScrollArea*>(
        QString(), Qt::FindDirectChildrenOnly);
    REQUIRE(list != nullptr);
    REQUIRE(kinds != nullptr);
    REQUIRE(area != nullptr);
    REQUIRE_FALSE(list->isVisible());
    const int before = dialog.height();

    // The prologue: the window grows for the list; where the screen has room for all of it,
    // the content does not scroll
    kinds->setCurrentIndex(1);
    QApplication::processEvents();
    REQUIRE(list->isVisible());
    CHECK(dialog.height() > before);
    if (dialog.sizeHint().height() < dialog.screen()->availableGeometry().height() - 100) {
        CHECK(dialog.height() == dialog.sizeHint().height());
        CHECK_FALSE(area->verticalScrollBar()->isVisible());
    }

    // A chapter again: the height of a chapter
    kinds->setCurrentIndex(0);
    QApplication::processEvents();
    CHECK(dialog.height() == before);
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
    CHECK_FALSE(moveButton(dialog, QStringLiteral("Move the part up"))->isVisibleTo(&dialog));
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

/// @brief The program in language @p code while it lives, as after loading its translation
class ProgramLanguage {
public:
    explicit ProgramLanguage(const QString& code)
        : m_before(kalahari::gui::programLanguage()) {
        kalahari::gui::setProgramLanguage(code);
    }
    ~ProgramLanguage() { kalahari::gui::setProgramLanguage(m_before); }
    ProgramLanguage(const ProgramLanguage&) = delete;
    ProgramLanguage& operator=(const ProgramLanguage&) = delete;

private:
    QString m_before;
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

TEST_CASE("New Book: the book is at first in the language of the program", "[gui][dialogs]") {
    // Regression: in the program in Polish the window proposed English, so the Sections field
    // named the sets in English
    auto& settings = kalahari::core::SettingsManager::getInstance();
    settings.resetToDefaults();

    SECTION("The language of the program, with the names of the sections in it") {
        const ProgramLanguage polish(QStringLiteral("pl"));
        NewBook book;
        book.choose("template.novel");
        CHECK(book.language->currentData().toString() == QStringLiteral("pl"));
        CHECK(book.sections->currentText().toStdString() ==
              "Sekcja początkowa \u00B7 Sekcja główna \u00B7 Sekcja końcowa");
        book.chooseSections("custom");
        CHECK(book.namesOf() == "Sekcja początkowa, Sekcja główna, Sekcja końcowa");
    }

    SECTION("The language of new books in the settings comes first") {
        const ProgramLanguage polish(QStringLiteral("pl"));
        settings.set<std::string>("project.defaultLanguage", "de");
        const NewBook book;
        CHECK(book.language->currentData().toString() == QStringLiteral("de"));
        settings.removeKey("project.defaultLanguage");
    }

    SECTION("English when the list has no language of the program") {
        const ProgramLanguage ukrainian(QStringLiteral("uk"));
        const NewBook book;
        CHECK(book.language->currentData().toString() == QStringLiteral("en"));
    }
}

TEST_CASE("New Book: a book of a type asks about the open book; a user project goes on to its "
          "kinds",
          "[gui][dialogs]") {
    QTemporaryDir dir;
    NewBook book;
    for (QLineEdit* field : book.dialog->findChildren<QLineEdit*>()) {
        if (field->placeholderText() == QStringLiteral("Enter book title...")) {
            field->setText(QStringLiteral("Second"));
        } else if (field->placeholderText() == QStringLiteral("Select book folder...")) {
            field->setText(dir.path());
        }
    }
    const auto* box = book.dialog->findChild<QDialogButtonBox*>();
    REQUIRE(box != nullptr);
    QPushButton* create = nullptr;
    for (QAbstractButton* button : box->buttons()) {
        if (box->buttonRole(button) == QDialogButtonBox::AcceptRole) {
            create = qobject_cast<QPushButton*>(button);
        }
    }
    REQUIRE(create != nullptr);
    QStringList asked;
    NewBookOpening answer = NewBookOpening::Cancel;
    book.dialog->setOpeningQuestion([&](QWidget* window, const QString& title) {
        CHECK(window == book.dialog.get());
        asked << title;
        return answer;
    });
    book.dialog->show();

    SECTION("A book of a type") {
        book.choose("template.novel");
        CHECK(create->text() == QStringLiteral("Create Book"));

        // Cancel of the question: the window stays, with the data
        create->click();
        CHECK(asked == QStringList{QStringLiteral("Second")});
        CHECK(book.dialog->isVisible());

        answer = NewBookOpening::DontOpen;
        create->click();
        CHECK(asked.size() == 2);
        CHECK_FALSE(book.dialog->isVisible());
        CHECK(book.dialog->QDialog::result() == QDialog::Accepted);
        CHECK_FALSE(book.dialog->result().open);
    }

    SECTION("A user project") {
        book.choose("template.empty");
        CHECK(create->text() == QStringLiteral("Next"));
        book.choose("template.poetry");
        CHECK(create->text() == QStringLiteral("Create Book"));
        book.choose("template.empty");

        // The question comes in the window of its kinds
        create->click();
        CHECK(asked.isEmpty());
        CHECK(book.dialog->QDialog::result() == QDialog::Accepted);
        CHECK(book.dialog->result().open);
        CHECK(NewItemDialog::isUserProject(book.dialog->result().templateId));
    }
}

TEST_CASE("Delete from Book: what goes, named after its kind, and where its files go",
          "[gui][dialogs]") {
    const QString folder = QDir::cleanPath(QDir::tempPath() + QStringLiteral("/Stories"));
    const auto pathOf = [&folder](const QString& file) {
        return QDir::cleanPath(QDir(folder).filePath(file));
    };
    DeleteFromBookDialog::Content content;
    content.element = storiesElement(
        QStringLiteral("section"), QStringLiteral("love"), QStringLiteral("Love"), QString(),
        {storiesElement(QStringLiteral("story"), QStringLiteral("s1"), QStringLiteral("The Letter"),
                        QStringLiteral("stories/story_001.kchapter")),
         storiesElement(QStringLiteral("story"), QStringLiteral("s2"), QStringLiteral("The Visit"),
                        QStringLiteral("stories/story_002.kchapter"))});
    content.files = {pathOf(QStringLiteral("stories/story_001.kchapter")),
                     pathOf(QStringLiteral("stories/story_002.kchapter"))};
    content.projectFolder = folder;

    SECTION("The files stay in the book's folder unless the writer deletes them") {
        DeleteFromBookDialog dialog(content, builtInTypes());
        CHECK(dialog.windowTitle() == QStringLiteral("Delete from Book"));
        CHECK(dialog.clipboardText() ==
              QStringLiteral("Delete from Book\n\n"
                             "Delete the division \"Love\" from the book?\n\n"
                             "Together with it, 2 stories will be deleted from the book.\n\n"
                             "Love\n"
                             "  The Letter – story_001.kchapter\n"
                             "  The Visit – story_002.kchapter\n\n"
                             "The files stay in the book's folder. You will find them in the "
                             "Navigator, under \"Unassigned Files\", where you can add them back "
                             "to the book or delete them.\n\n"
                             "Folder: ") +
                  QDir::toNativeSeparators(pathOf(QStringLiteral("stories"))));
        CHECK(dialog.list()->topLevelItem(0)->childCount() == 2);
        CHECK(dialog.deleteFilesBox()->isVisibleTo(&dialog));
        CHECK_FALSE(dialog.deleteFilesBox()->isChecked());
        CHECK_FALSE(dialog.deleteFiles());
        CHECK(dialog.deleteFilesBox()->text() ==
              withBin(QStringLiteral(
                  "&Also delete the files of the stories (2) – they go to the {bin}")));
        CHECK(dialog.folderField()->isVisibleTo(&dialog));
        CHECK(dialog.showInFolderButton()->isVisibleTo(&dialog));

        // Deleting needs Delete or its mnemonic: Cancel is the default
        CHECK(dialog.acceptButton()->text() == QStringLiteral("&Delete"));
        CHECK(dialog.cancelButton()->isDefault());
        CHECK_FALSE(dialog.acceptButton()->isDefault());

        // The files too: the window says where they go
        dialog.deleteFilesBox()->setChecked(true);
        CHECK(dialog.deleteFiles());
        CHECK(dialog.filesNote()->text() ==
              withBin(QStringLiteral("The files of the stories go to the system's {bin}. You "
                                     "can restore them from the {bin}, but not from the "
                                     "program.")));
        CHECK_FALSE(dialog.folderField()->isVisibleTo(&dialog));
    }

    SECTION("Checking the box moves nothing in the window") {
        DeleteFromBookDialog dialog(content, builtInTypes());
        dialog.show();
        QApplication::processEvents();
        const QSize size = dialog.size();
        const QRect box = dialog.deleteFilesBox()->geometry();
        const QRect list = dialog.list()->geometry();
        for (const bool checked : {true, false}) {
            dialog.deleteFilesBox()->setChecked(checked);
            QApplication::processEvents();
            CHECK(dialog.size() == size);
            CHECK(dialog.deleteFilesBox()->geometry() == box);
            CHECK(dialog.list()->geometry() == list);
        }

        // The end of the folder's path shows the folder of the book
        CHECK(dialog.folderField()->cursorPosition() == dialog.folderField()->text().size());
    }

    SECTION("From a folder without a Trash they are gone for good") {
        content.trash = false;
        DeleteFromBookDialog dialog(content, builtInTypes());
        CHECK(dialog.deleteFilesBox()->text() ==
              QStringLiteral(
                  "&Also delete the files of the stories (2) – they will be gone for good"));
        dialog.deleteFilesBox()->setChecked(true);
        CHECK(dialog.filesNote()->text() ==
              withBin(QStringLiteral("The files of the stories will be gone for good: files in "
                                     "this folder cannot go to the {bin}.")));
    }

    SECTION("Files that the Navigator does not list stay in their folders") {
        content.unassigned = false;
        DeleteFromBookDialog dialog(content, builtInTypes());
        CHECK(dialog.filesNote()->text() == QStringLiteral("The files stay in their folders."));
    }

    SECTION("A story") {
        // A copy: the assignment frees the list that holds the story
        const ProjectElement story = content.element.elements.first();
        content.element = story;
        content.files = {content.files.first()};
        DeleteFromBookDialog dialog(content, builtInTypes());
        CHECK(dialog.clipboardText().startsWith(
            QStringLiteral("Delete from Book\n\nDelete the story \"The Letter\" from the "
                           "book?\n\nThe Letter – story_001.kchapter\n\nThe file stays in "
                           "the book's folder. You will find it in the Navigator, under "
                           "\"Unassigned Files\", where you can add it back to the book or "
                           "delete it.")));
        CHECK(dialog.deleteFilesBox()->text() ==
              withBin(
                  QStringLiteral("&Also delete the file of the story – it goes to the {bin}")));
        dialog.deleteFilesBox()->setChecked(true);
        CHECK(dialog.filesNote()->text() ==
              withBin(QStringLiteral("The file of the story goes to the system's {bin}. You can "
                                     "restore it from the {bin}, but not from the program.")));
    }

    SECTION("A file of the book outside its text is named as a file, not as the file of an "
            "element") {
        ProjectElement notes;
        notes.id = QStringLiteral("f1");
        notes.title = QStringLiteral("Notes");
        notes.file = QStringLiteral("stories/file_001.kchapter");
        content.element = notes;
        content.files = {pathOf(notes.file)};
        DeleteFromBookDialog dialog(content, builtInTypes());
        CHECK(dialog.clipboardText().startsWith(
            QStringLiteral("Delete from Book\n\nDelete the file \"Notes\" from the book?\n\n"
                           "Notes – file_001.kchapter\n\nThe file stays in the book's folder. "
                           "You will find it in the Navigator, under \"Unassigned Files\"")));
        CHECK(dialog.deleteFilesBox()->text() ==
              withBin(QStringLiteral(
                  "&Also delete the file from the book's folder – it goes to the {bin}")));
        dialog.deleteFilesBox()->setChecked(true);
        CHECK(dialog.filesNote()->text() ==
              withBin(QStringLiteral("The file goes to the system's {bin}. You can restore it "
                                     "from the {bin}, but not from the program.")));

        content.trash = false;
        DeleteFromBookDialog withoutTrash(content, builtInTypes());
        CHECK(withoutTrash.deleteFilesBox()->text() ==
              QStringLiteral(
                  "&Also delete the file from the book's folder – it will be gone for good"));
        withoutTrash.deleteFilesBox()->setChecked(true);
        CHECK(withoutTrash.filesNote()->text() ==
              withBin(QStringLiteral("The file will be gone for good: files in this folder "
                                     "cannot go to the {bin}.")));
    }

    SECTION("Elements of different kinds are elements") {
        content.element.elements.append(storiesElement(QStringLiteral("contributors"),
                                                       QStringLiteral("notes"),
                                                       QStringLiteral("Notes"),
                                                       QStringLiteral("back/notes.kchapter")));
        content.files.append(pathOf(QStringLiteral("back/notes.kchapter")));
        DeleteFromBookDialog dialog(content, builtInTypes());
        CHECK(dialog.clipboardText().contains(
            QStringLiteral("Together with it, 3 elements will be deleted from the book.")));
        CHECK(dialog.deleteFilesBox()->text() ==
              withBin(QStringLiteral(
                  "&Also delete the files of the elements (3) – they go to the {bin}")));
    }

    SECTION("An empty division has no files") {
        content.element.elements.clear();
        content.files.clear();
        DeleteFromBookDialog dialog(content, builtInTypes());
        CHECK(dialog.clipboardText() ==
              QStringLiteral("Delete from Book\n\nDelete the division \"Love\" from the "
                             "book?\n\nThe division is empty.\n\nLove"));
        CHECK_FALSE(dialog.deleteFilesBox()->isVisibleTo(&dialog));
        CHECK_FALSE(dialog.filesNote()->isVisibleTo(&dialog));
        CHECK_FALSE(dialog.folderField()->isVisibleTo(&dialog));
        dialog.deleteFilesBox()->setChecked(true);
        CHECK_FALSE(dialog.deleteFiles());
    }
}

TEST_CASE("Delete Files: the files that no element has, and where they go", "[gui][dialogs]") {
    const QString folder = QDir::cleanPath(QDir::tempPath() + QStringLiteral("/Novel"));
    const QList<kalahari::core::UnassignedFile> files{
        {QStringLiteral("manuscript/chapter_002.kchapter"), QStringLiteral("Chapter 2")},
        {QStringLiteral("manuscript/draft.kchapter"), QString()}};

    SECTION("All of them, to the Trash") {
        DeleteFilesDialog dialog(files, folder, true);
        CHECK(dialog.windowTitle() == QStringLiteral("Delete Files"));
        CHECK(dialog.clipboardText() ==
              withBin(QStringLiteral(
                  "Delete Files\n\nDelete all unassigned files (2)?\n\n"
                  "Chapter 2 – chapter_002.kchapter\ndraft.kchapter\n\n"
                  "The files go to the system's {bin}. You can restore them from the {bin}, but "
                  "not from the program.")));
        CHECK(dialog.list()->topLevelItem(1)->toolTip(0) ==
              QDir::toNativeSeparators(folder + QStringLiteral("/manuscript/draft.kchapter")));
        CHECK(dialog.acceptButton()->text() == QStringLiteral("&Delete"));
        CHECK(dialog.cancelButton()->isDefault());
        CHECK_FALSE(dialog.acceptButton()->isDefault());
    }

    SECTION("One of them, for good from a folder without a Trash") {
        DeleteFilesDialog dialog({files.first()}, folder, false);
        CHECK(dialog.windowTitle() == QStringLiteral("Delete File"));
        CHECK(dialog.clipboardText() ==
              withBin(QStringLiteral(
                  "Delete File\n\nDelete the file \"chapter_002.kchapter\"?\n\n"
                  "Chapter 2 – chapter_002.kchapter\n\n"
                  "The file will be gone for good: files in this folder cannot go to the "
                  "{bin}.")));
    }
}

TEST_CASE("Preview: the text of a file that no element has, and what can be done with it",
          "[gui][dialogs]") {
    const QString folder = QDir::cleanPath(QDir::tempPath() + QStringLiteral("/Novel"));
    const QString path = folder + QStringLiteral("/manuscript/chapter_002.kchapter");
    kalahari::core::ChapterDocument chapter;
    chapter.setTitle(QStringLiteral("Chapter 2"));
    chapter.setStatus(QStringLiteral("revision"));
    chapter.setKml(
        QStringLiteral("<kml><p>The storm came at night.</p><p>Nobody slept.</p></kml>"));

    SECTION("Its title, its file, its status, its words and its text") {
        FilePreviewDialog dialog(chapter, path, folder);
        CHECK(dialog.windowTitle() == QStringLiteral("Preview"));
        CHECK(showsText(dialog, QStringLiteral("Chapter 2")));
        const QString file = QStringLiteral("manuscript/chapter_002.kchapter");
        CHECK(showsText(dialog, QDir::toNativeSeparators(file) +
                                    QStringLiteral(" · Status: Revision · Words: 7")));
        CHECK(dialog.text()->toPlainText() ==
              QStringLiteral("The storm came at night.\nNobody slept."));
        CHECK(dialog.text()->isReadOnly());

        // Close is the default; the other buttons lead on
        CHECK(dialog.acceptButton()->text() == QStringLiteral("&Add to Book..."));
        CHECK(dialog.deleteButton()->text() == QStringLiteral("&Delete File..."));
        CHECK(dialog.cancelButton()->text() == QStringLiteral("Close"));
        CHECK(dialog.cancelButton()->isDefault());
        CHECK_FALSE(dialog.acceptButton()->isDefault());
        CHECK(dialog.choice() == FilePreviewDialog::Choice::Close);
    }

    SECTION("Add to Book...") {
        FilePreviewDialog dialog(chapter, path, folder);
        dialog.acceptButton()->click();
        CHECK(dialog.result() == QDialog::Accepted);
        CHECK(dialog.choice() == FilePreviewDialog::Choice::AddToBook);
    }

    SECTION("Delete File...") {
        FilePreviewDialog dialog(chapter, path, folder);
        dialog.deleteButton()->click();
        CHECK(dialog.result() == QDialog::Accepted);
        CHECK(dialog.choice() == FilePreviewDialog::Choice::Delete);
    }

    SECTION("Close") {
        FilePreviewDialog dialog(chapter, path, folder);
        dialog.cancelButton()->click();
        CHECK(dialog.result() == QDialog::Rejected);
        CHECK(dialog.choice() == FilePreviewDialog::Choice::Close);
    }

    SECTION("A file without a title, a status of the elements and a text") {
        chapter.setTitle(QString());
        chapter.setStatus(QStringLiteral("unknown"));
        chapter.setKml(QStringLiteral("<kml></kml>"));
        FilePreviewDialog dialog(chapter, path, folder);
        CHECK(showsText(dialog, QStringLiteral("chapter_002.kchapter")));
        CHECK_FALSE(showsText(dialog, QStringLiteral("Status:")));
        CHECK(showsText(dialog, QStringLiteral("Words: 0")));
        CHECK(dialog.text()->toPlainText().isEmpty());
        CHECK(dialog.text()->placeholderText() == QStringLiteral("The file has no text."));
    }
}

TEST_CASE("Add to Book: the file, the kind, the title and the place of the new element",
          "[gui][dialogs]") {
    const KindRef chapter = baseKind(QStringLiteral("chapter"));
    const KindRef prologue = novelKind(QStringLiteral("prologue"));
    REQUIRE(chapter);
    REQUIRE(prologue);
    AddToBookDialog::Book book;
    book.choices = {{chapter, QString(), mainAt(QString(), 3)},
                    {prologue, QString(), mainAt(QString(), 0)}};
    book.sections = {{BookPlace::Main, QStringLiteral("Main Section"),
                      SectionWords::forPart(nullptr, BookPlace::Main), novelBody()}};

    SECTION("A chapter from outside the book: copied, with its title and its status") {
        AddToBookDialog dialog({QStringLiteral("/notes/storm.kchapter"),
                                QStringLiteral(" The Storm "), QStringLiteral("final"), false},
                               book, builtInTypes());
        CHECK(dialog.windowTitle() == QStringLiteral("Add to Book"));
        CHECK(showsText(dialog, QStringLiteral("Add the File to the Book")));
        CHECK(showsText(dialog, QStringLiteral("The file becomes an element of the book, with the "
                                               "title, the status and the notes saved in it.")));
        REQUIRE(dialog.copyButton() != nullptr);
        REQUIRE(dialog.moveButton() != nullptr);
        CHECK(dialog.copyButton()->isChecked());
        CHECK(dialog.copy());
        dialog.moveButton()->setChecked(true);
        CHECK_FALSE(dialog.copy());

        CHECK(dialog.kindBox()->count() == 2);
        CHECK(dialog.kindBox()->isEnabled());
        CHECK(dialog.kindBox()->currentText() == QStringLiteral("Chapter"));
        CHECK(dialog.kind().kind == chapter.kind);
        CHECK(dialog.titleEdit()->text() == QStringLiteral("The Storm"));
        CHECK(dialog.title() == QStringLiteral("The Storm"));
        CHECK(dialog.statusLabel()->text() == QStringLiteral("Final (from the file)"));
        CHECK(dialog.place() == mainAt(QString(), 3));
        CHECK(dialog.acceptButton()->text() == QStringLiteral("Add"));
        CHECK(dialog.acceptButton()->isEnabled());

        // Another kind takes its own place
        dialog.kindBox()->setCurrentIndex(1);
        CHECK(dialog.kind().kind == prologue.kind);
        CHECK(dialog.place() == mainAt(QString(), 0));

        // Without a title it cannot be accepted
        dialog.titleEdit()->setText(QStringLiteral("  "));
        CHECK_FALSE(dialog.acceptButton()->isEnabled());
    }

    SECTION("A text file: its name is the title, and the element is a draft") {
        AddToBookDialog dialog({QStringLiteral("/notes/storm notes.txt"), QString(), QString(),
                                false},
                               book, builtInTypes());
        CHECK(showsText(dialog, QStringLiteral("The text of the file becomes a new element of "
                                               "the book; its lines become paragraphs.")));
        CHECK(dialog.titleEdit()->text() == QStringLiteral("storm notes"));
        CHECK(dialog.statusLabel()->text() == QStringLiteral("Draft"));
    }

    SECTION("A file of the book goes back to it and stays where it is") {
        AddToBookDialog dialog({QStringLiteral("/book/manuscript/chapter_002.kchapter"),
                                QStringLiteral("Chapter 2"), QStringLiteral("revision"), true},
                               book, builtInTypes());
        CHECK(showsText(dialog, QStringLiteral("The file goes back to the book as an element, "
                                               "with the title, the status and the notes saved "
                                               "in it.")));
        CHECK(dialog.copyButton() == nullptr);
        CHECK(dialog.moveButton() == nullptr);
        CHECK(dialog.copy());
        CHECK(dialog.statusLabel()->text() == QStringLiteral("Revision (from the file)"));
    }

    SECTION("The kind chosen at the start; with one kind the list does not change") {
        book.current = 1;
        AddToBookDialog started({QStringLiteral("/notes/storm.kchapter"), QString(), QString(),
                                 false},
                                book, builtInTypes());
        CHECK(started.kindBox()->currentText() == QStringLiteral("Prologue"));

        book.choices.removeLast();
        AddToBookDialog dialog({QStringLiteral("/notes/storm.kchapter"), QString(), QString(),
                                false},
                               book, builtInTypes());
        CHECK(dialog.kindBox()->currentText() == QStringLiteral("Chapter"));
        CHECK_FALSE(dialog.kindBox()->isEnabled());
    }
}

TEST_CASE("Move: the writer chooses a new place for the epilogue", "[gui][dialogs]") {
    // The epilogue ends the main section of a novel, after "Part Two"
    MoveElementDialog::Book book;
    book.element = epilogue(QStringLiteral("epi"));
    book.from = mainAt(QString(), 3);
    book.sections =
        bookSections({baseElement(QStringLiteral("title_page"), QStringLiteral("title"),
                                  QStringLiteral("Title Page"))},
                     novelBody(),
                     {baseElement(QStringLiteral("afterword"), QStringLiteral("after"),
                                  QStringLiteral("Afterword"))});
    MoveElementDialog dialog(book, builtInTypes());
    CHECK(dialog.windowTitle() == QStringLiteral("Move"));
    CHECK(showsText(dialog, QStringLiteral("Move \"Epilogue\"")));
    CHECK(showsText(dialog, QStringLiteral("Choose the new place. The list shows the epilogue "
                                           "where it goes.")));
    CHECK(dialog.acceptButton()->text() == QStringLiteral("Move"));

    // Where it is now; the list shows only the main section, where an epilogue can be
    CHECK(showsText(dialog, QStringLiteral("Current place: at the end of the main section. "
                                           "Click the element the epilogue is to go before, "
                                           "or move it with the buttons")));
    CHECK(shownPlace(dialog, QStringLiteral("Epilogue")) == "Main Section >");
    CHECK(dialog.place() == mainAt(QString(), 3));
    CHECK(listItem(dialog, QStringLiteral("Main Section")) != nullptr);
    CHECK(listItem(dialog, QStringLiteral("Front Section")) == nullptr);
    CHECK(listItem(dialog, QStringLiteral("Title Page")) == nullptr);
    CHECK(listItem(dialog, QStringLiteral("Back Section")) == nullptr);
    CHECK(dialog.picker()->places() ==
          QList<ElementPlace>{mainAt(QString(), 0), mainAt(QStringLiteral("part1"), 0),
                              mainAt(QStringLiteral("part1"), 1),
                              mainAt(QStringLiteral("part1"), 2), mainAt(QString(), 1),
                              mainAt(QString(), 2), mainAt(QStringLiteral("part2"), 0),
                              mainAt(QStringLiteral("part2"), 1), mainAt(QString(), 3)});

    // The places of its kind as options; at the place it has, the dialog cannot be accepted
    QRadioButton* atEnd = option(dialog, QStringLiteral("At the end of the main section"));
    QRadioButton* inPart = option(dialog, QStringLiteral("Last in the part \"Part Two\""));
    QRadioButton* elsewhere =
        option(dialog, QStringLiteral("Elsewhere: show the place in the list"));
    REQUIRE(atEnd != nullptr);
    REQUIRE(inPart != nullptr);
    REQUIRE(elsewhere != nullptr);
    CHECK(atEnd->isChecked());
    CHECK_FALSE(dialog.acceptButton()->isEnabled());

    // Last in "Part Two": it still ends the book
    inPart->click();
    CHECK(dialog.place() == mainAt(QStringLiteral("part2"), 1));
    CHECK(shownPlace(dialog, QStringLiteral("Epilogue")) == "Part Two >");
    CHECK(dialog.acceptButton()->isEnabled());
    CHECK_FALSE(showsText(dialog, QStringLiteral("will not be the last element")));

    // Before "Chapter 3": the dialog says that it no longer ends the main section
    auto* list = dialog.findChild<QTreeWidget*>();
    REQUIRE(list != nullptr);
    emit list->itemClicked(listItem(dialog, QStringLiteral("Chapter 3")), 0);
    CHECK(dialog.place() == mainAt(QString(), 1));
    CHECK(elsewhere->isChecked());
    CHECK(showsText(dialog, QStringLiteral("\"Epilogue\" will not be the last element in the "
                                           "main section.")));

    // Up and Down in the list move it at once; back at its place, nothing to accept
    press(list, Qt::Key_Down);
    CHECK(dialog.place() == mainAt(QString(), 2));
    press(list, Qt::Key_Down);
    press(list, Qt::Key_Down);
    press(list, Qt::Key_Down);
    CHECK(dialog.place() == mainAt(QString(), 3));
    CHECK(atEnd->isChecked());
    CHECK_FALSE(dialog.acceptButton()->isEnabled());
    CHECK(moveButton(dialog, QStringLiteral("Move the epilogue up"))->isEnabled());
    CHECK_FALSE(moveButton(dialog, QStringLiteral("Move the epilogue down"))->isEnabled());
}

TEST_CASE("Move: what changes is said against the book as it is", "[gui][dialogs]") {
    // Chapter 3 between the parts, and an epilogue that ends the main section
    const ProjectElement chapter = element(QStringLiteral("c3"), QStringLiteral("Chapter 3"));
    QList<ProjectElement> body = novelBody();
    body.removeAt(1);
    body.append(epilogue(QStringLiteral("epi")));
    MoveElementDialog::Book book;
    book.element = chapter;
    book.from = mainAt(QString(), 1);
    book.sections = bookSections({}, body, {});
    MoveElementDialog dialog(book, builtInTypes());
    CHECK(showsText(dialog, QStringLiteral("Current place: in the main section, before \"Part "
                                           "Two\".")));
    CHECK(shownPlace(dialog, QStringLiteral("Chapter 3")) == "Main Section > Part Two");

    // A chapter has no options; after the epilogue, the epilogue no longer ends the section
    CHECK_FALSE(option(dialog, QStringLiteral("Elsewhere: show the place in the list"))
                    ->isVisibleTo(&dialog));
    CHECK_FALSE(showsText(dialog, QStringLiteral("will no longer be")));
    dialog.picker()->setPlace(mainAt(QString(), 3));
    CHECK(showsText(dialog, QStringLiteral("\"Epilogue\" will no longer be the last element in "
                                           "the main section.")));

    // A chapter that is already after the epilogue: where it is, nothing changes
    MoveElementDialog::Book after = book;
    after.from = mainAt(QString(), 3);
    MoveElementDialog already(after, builtInTypes());
    CHECK(already.place() == mainAt(QString(), 3));
    CHECK_FALSE(showsText(already, QStringLiteral("will no longer be")));
    CHECK(showsText(already, QStringLiteral("Current place: at the end of the main section.")));
}

TEST_CASE("Move: a part goes with its chapters, only where a part can be", "[gui][dialogs]") {
    QList<ProjectElement> body = novelBody();
    MoveElementDialog::Book book;
    book.element = body.takeLast();  // "Part Two" with "Chapter 4"
    book.from = mainAt(QString(), 2);
    book.sections = bookSections({}, body, {});
    MoveElementDialog dialog(book, builtInTypes());
    CHECK(showsText(dialog, QStringLiteral("Choose the new place. The list shows the part where "
                                           "it goes.")));
    CHECK(showsText(dialog, QStringLiteral("Current place: at the end of the main section.")));

    // In the main section only, not inside "Part One"; the chapter goes with it
    CHECK(dialog.picker()->places() ==
          QList<ElementPlace>{mainAt(QString(), 0), mainAt(QString(), 1), mainAt(QString(), 2)});
    CHECK(listItem(dialog, QStringLiteral("Chapter 4"))->parent() ==
          listItem(dialog, QStringLiteral("Part Two")));
    moveButton(dialog, QStringLiteral("Move the part up"))->click();
    CHECK(dialog.place() == mainAt(QString(), 1));
    CHECK(shownPlace(dialog, QStringLiteral("Part Two")) == "Main Section > Chapter 3");
    CHECK(dialog.acceptButton()->isEnabled());
}

TEST_CASE("Move: in a book without sections the list is one, and every press moves the element",
          "[gui][dialogs]") {
    // A table of contents at the end of the front part; the main part is empty
    kalahari::core::ProjectBook flat;
    flat.partsLayer = false;
    MoveElementDialog::Book book;
    book.element = baseElement(QStringLiteral("toc"), QStringLiteral("toc"),
                               QStringLiteral("Contents"));
    book.from = {BookPlace::Front, QString(), 1};
    book.sections = bookSections({baseElement(QStringLiteral("title_page"),
                                              QStringLiteral("title"), QStringLiteral("Title"))},
                                 {},
                                 {baseElement(QStringLiteral("afterword"),
                                              QStringLiteral("after"),
                                              QStringLiteral("Afterword"))},
                                 &flat);
    MoveElementDialog dialog(book, builtInTypes());
    CHECK(showsText(dialog, QStringLiteral("Current place: before the content of the book.")));
    auto* list = dialog.findChild<QTreeWidget*>();
    REQUIRE(list != nullptr);
    CHECK(list->topLevelItemCount() == 3);
    CHECK(shownPlace(dialog, QStringLiteral("Contents")) == "> Afterword");

    // The end of the front part and the start of the back part are one line of the list: the
    // keys pass over one of them
    press(list, Qt::Key_Down);
    CHECK(dialog.place() == ElementPlace{BookPlace::Back, QString(), 1});
    CHECK(shownPlace(dialog, QStringLiteral("Contents")) == ">");
    press(list, Qt::Key_Up);
    CHECK(dialog.place() == ElementPlace{BookPlace::Back, QString(), 0});
    press(list, Qt::Key_Up);
    CHECK(dialog.place() == ElementPlace{BookPlace::Front, QString(), 0});
    CHECK(shownPlace(dialog, QStringLiteral("Contents")) == "> Title");
    CHECK_FALSE(moveButton(dialog, QStringLiteral("Move the table of contents up"))->isEnabled());
    press(list, Qt::Key_Down);
    CHECK(dialog.place() == ElementPlace{BookPlace::Front, QString(), 1});
}

TEST_CASE("New Book: on a small screen the buttons stay in view and the details scroll",
          "[gui][dialogs]") {
    NewItemDialog dialog(NewItemMode::Project);
    dialog.show();
    QApplication::processEvents();
    const QRect available = dialog.screen()->availableGeometry();
    CHECK(dialog.width() <= available.width());
    CHECK(dialog.height() <= available.height());

    // A window lower than its templates and details, as on 1366x768 at 150%
    dialog.resize(dialog.width(), 360);
    QApplication::processEvents();
    QApplication::processEvents();
    REQUIRE(dialog.height() == 360);
    auto* area = dialog.findChild<kalahari::gui::FittingScrollArea*>();
    REQUIRE(area != nullptr);
    CHECK(area->verticalScrollBar()->isVisible());
    for (const QAbstractButton* button : {dialog.acceptButton(), dialog.cancelButton()}) {
        INFO(button->text().toStdString());
        CHECK(button->isVisible());
        CHECK(dialog.rect().contains(QRect(button->mapTo(&dialog, QPoint(0, 0)), button->size())));
    }

    // The last field of the details scrolls into view
    QLineEdit* location = nullptr;
    for (QLineEdit* field : dialog.findChildren<QLineEdit*>()) {
        if (field->placeholderText() == QStringLiteral("Select book folder...")) {
            location = field;
        }
    }
    REQUIRE(location != nullptr);
    area->ensureWidgetVisible(location);
    QApplication::processEvents();
    CHECK(area->viewport()->rect().contains(
        QRect(location->mapTo(area->viewport(), QPoint(0, 0)), location->size())));
}
