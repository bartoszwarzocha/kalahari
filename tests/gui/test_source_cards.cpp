// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_source_cards.cpp
/// @brief The sources on the cards of the materials: the bibliographic description, its entry
/// in the bibliography, the bookmarks, the quotes from a source and where a quote comes from;
/// the list and the node of the sources

#include "document_window.h"

#include "kalahari/core/material_kinds.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/source_description.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/material_list.h"
#include "kalahari/gui/panels/material_list_panel.h"
#include "kalahari/gui/panels/material_page.h"
#include "kalahari/gui/panels/source_boxes.h"

#include <catch2/catch_test_macros.hpp>

#include <QAbstractItemDelegate>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QToolButton>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>

#include "test_support/window_parts.h"

namespace core = kalahari::core;
namespace gui = kalahari::gui;
namespace test = kalahari::test;
using core::SourceDescription;

namespace {

/// A material of base kind @p kindId in the Workshop of the open book
QString addOf(core::ProjectManager& pm, const char* kindId, const QString& title,
              const QString& fileName = QString(), const QByteArray& data = QByteArray()) {
    core::NewMaterial material;
    material.title = title;
    material.fileName = fileName;
    material.data = data;
    const QString id = pm.addMaterial(
        material, pm.bookTypes().findKind(QStringLiteral("kalahari.base"),
                                          QString::fromLatin1(kindId)));
    REQUIRE_FALSE(id.isEmpty());
    return id;
}

/// @p key pressed in @p widget
void press(QWidget* widget, Qt::Key key) {
    QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
    QCoreApplication::sendEvent(widget, &event);
}

/// The open editor of the cell at @p row, @p column of @p table
QWidget* cellEditor(const QTableWidget* table, int row, int column) {
    QWidget* editor = table->indexWidget(table->model()->index(row, column));
    REQUIRE(editor != nullptr);
    return editor;
}

/// Type @p text in the open line of a cell and leave it as @p hint says
void finishCell(QTableWidget* table, QWidget* editor, const QString& text,
                QAbstractItemDelegate::EndEditHint hint) {
    auto* line = qobject_cast<QLineEdit*>(editor);
    REQUIRE(line != nullptr);
    line->setText(text);
    QAbstractItemDelegate* delegate = table->itemDelegate();
    emit delegate->commitData(editor);
    emit delegate->closeEditor(editor, hint);
}

/// Type @p text in field @p edit and leave it
void typeField(QLineEdit* edit, const QString& text) {
    edit->setText(text);
    emit edit->editingFinished();
}

/// The item of @p tree with id @p id
QTreeWidgetItem* itemOf(QTreeWidget* tree, const QString& id) {
    for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it) {
        if ((*it)->data(0, Qt::UserRole).toString() == id) {
            return *it;
        }
    }
    return nullptr;
}

/// The description of the book of the examples
SourceDescription atlas() {
    SourceDescription source;
    source.type = QString::fromLatin1(SourceDescription::BOOK);
    source.people << core::SourcePerson{
        .name = QStringLiteral("Anna Wiśniewska"), .role = {}, .extra = {}};
    source.setValue(QString::fromLatin1(SourceDescription::TITLE),
                    QStringLiteral("Atlas pustyń świata"));
    source.setValue(QString::fromLatin1(SourceDescription::YEAR), QStringLiteral("2011"));
    return source;
}

}  // namespace

TEST_CASE("Source card: the description of a publication, its fields by kind, its people and "
          "its entry in the bibliography",
          "[gui][materials][sources]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Dunes"));
    auto& pm = novel.pm;
    const QString atlasId = addOf(pm, core::MaterialKinds::PUBLICATION, QStringLiteral("Atlas"));

    gui::MaterialPage page(kalahari::test::book(), nullptr, true);
    QStringList changed;
    QObject::connect(&page, &gui::MaterialPage::changed,
                     [&](const QString& id) { changed << id; });
    page.showMaterial(atlasId);
    gui::SourceDescriptionBox* box = page.sourceBox();
    REQUIRE(box != nullptr);
    CHECK_FALSE(box->isHidden());
    REQUIRE(page.entryBox() != nullptr);
    CHECK_FALSE(page.entryBox()->isHidden());
    // Not a source yet: no quotes from it, no bookmarks without a PDF, no source of a quote
    REQUIRE(page.quotesBox() != nullptr);
    CHECK(page.quotesBox()->isHidden());
    CHECK(page.bookmarksBox()->isHidden());
    CHECK(page.quoteBox()->isHidden());
    CHECK(pm.findElement(atlasId)->source.isEmpty());

    // A publication is a book at first, with the fields of a book; its title is that of the
    // material until the writer types one
    CHECK(box->kindBox()->currentData().toString() == QStringLiteral("book"));
    CHECK(box->isFieldShown(QStringLiteral("isbn")));
    CHECK_FALSE(box->isFieldShown(QStringLiteral("container")));
    CHECK(box->field(QStringLiteral("title"))->placeholderText() == QStringLiteral("Atlas"));
    CHECK_FALSE(page.entryBox()->copyButton()->isEnabled());

    // A field saved when it loses the keys makes it a source
    typeField(box->field(QStringLiteral("title")), QStringLiteral("Atlas pustyń świata"));
    typeField(box->field(QStringLiteral("year")), QStringLiteral(" 2011 "));
    const core::ProjectElement* atlas = pm.findElement(atlasId);
    CHECK(atlas->source.value(SourceDescription::TITLE) == QStringLiteral("Atlas pustyń świata"));
    CHECK(atlas->source.value(SourceDescription::YEAR) == QStringLiteral("2011"));
    CHECK(atlas->source.type == QStringLiteral("book"));
    CHECK(changed.contains(atlasId));
    page.refresh();
    CHECK_FALSE(page.quotesBox()->isHidden());

    // "+" adds a person with the keys in the name; one left without a name goes
    CHECK(box->peopleTable()->isHidden());
    box->addPersonButton()->click();
    QTableWidget* people = box->peopleTable();
    REQUIRE(people->rowCount() == 1);
    finishCell(people, cellEditor(people, 0, 0), QStringLiteral("Anna Wiśniewska"),
               QAbstractItemDelegate::SubmitModelCache);
    REQUIRE(pm.findElement(atlasId)->source.people.size() == 1);
    CHECK(pm.findElement(atlasId)->source.people.first().name == QStringLiteral("Anna Wiśniewska"));
    CHECK(people->item(0, 1)->text() == QStringLiteral("author"));
    press(people, Qt::Key_Insert);
    REQUIRE(people->rowCount() == 2);
    finishCell(people, cellEditor(people, 1, 0), QString(), QAbstractItemDelegate::NoHint);
    CHECK(people->rowCount() == 1);
    CHECK(pm.findElement(atlasId)->source.people.size() == 1);

    // The role of a person is chosen in its cell
    people->setCurrentCell(0, 1);
    people->editItem(people->item(0, 1));
    auto* role = qobject_cast<QComboBox*>(cellEditor(people, 0, 1));
    REQUIRE(role != nullptr);
    role->setCurrentIndex(role->findData(QStringLiteral("editor")));
    emit people->itemDelegate()->commitData(role);
    emit people->itemDelegate()->closeEditor(role, QAbstractItemDelegate::SubmitModelCache);
    CHECK(pm.findElement(atlasId)->source.people.first().role == QStringLiteral("editor"));
    CHECK(people->item(0, 1)->text() == QStringLiteral("editor"));

    // The entry in the style of the book, PN-ISO 690 at first; another style only shows it
    gui::BibliographyEntryBox* entry = page.entryBox();
    CHECK(entry->style() == QStringLiteral("pn-iso-690"));
    CHECK(entry->entryText().contains(QStringLiteral("Atlas pustyń świata")));
    CHECK(entry->entryLabel()->text().contains(QStringLiteral("<i>")));
    entry->styleBox()->setCurrentIndex(entry->styleBox()->findData(QStringLiteral("apa")));
    emit entry->styleBox()->activated(entry->styleBox()->currentIndex());
    CHECK(entry->style() == QStringLiteral("apa"));
    CHECK(pm.bibliographyStyle() == QStringLiteral("pn-iso-690"));
    entry->copyButton()->click();
    CHECK(QGuiApplication::clipboard()->text() == entry->entryText());
    REQUIRE(QGuiApplication::clipboard()->mimeData() != nullptr);
    CHECK(QGuiApplication::clipboard()->mimeData()->hasHtml());

    // Whether the bibliography has it
    entry->inclusionBox()->setCurrentIndex(
        entry->inclusionBox()->findData(QStringLiteral("never")));
    emit entry->inclusionBox()->activated(entry->inclusionBox()->currentIndex());
    CHECK(pm.findElement(atlasId)->source.bibliography == QStringLiteral("never"));
    // ...which the description keeps when a field changes
    typeField(box->field(QStringLiteral("publisher")), QStringLiteral("Wydawnictwo Piasek"));
    CHECK(pm.findElement(atlasId)->source.bibliography == QStringLiteral("never"));
    CHECK(pm.findElement(atlasId)->source.value(SourceDescription::PUBLISHER) ==
          QStringLiteral("Wydawnictwo Piasek"));

    // Another kind has its own fields and names; the values of the others stay
    box->kindBox()->setCurrentIndex(box->kindBox()->findData(QStringLiteral("article")));
    emit box->kindBox()->activated(box->kindBox()->currentIndex());
    CHECK(pm.findElement(atlasId)->source.type == QStringLiteral("article"));
    CHECK(box->isFieldShown(QStringLiteral("container")));
    CHECK(box->fieldLabel(QStringLiteral("container"))->text() == QStringLiteral("Journal:"));
    CHECK(box->fieldLabel(QStringLiteral("title"))->text() == QStringLiteral("Article title:"));
    CHECK_FALSE(box->isFieldShown(QStringLiteral("isbn")));
    CHECK(pm.findElement(atlasId)->source.value(SourceDescription::PUBLISHER) ==
          QStringLiteral("Wydawnictwo Piasek"));
}

TEST_CASE("Source card: a description pasted from BibTeX, and a description emptied is no "
          "source",
          "[gui][materials][sources]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Dunes"));
    auto& pm = novel.pm;
    const QString id = addOf(pm, core::MaterialKinds::PUBLICATION, QStringLiteral("Wells"));

    gui::MaterialPage page(kalahari::test::book(), nullptr, true);
    page.showMaterial(id);
    gui::SourceDescriptionBox* box = page.sourceBox();
    QGuiApplication::clipboard()->setText(QStringLiteral(
        "@article{z2019, author = {Zielińska, Marta}, title = {Studnie na szlakach solnych},\n"
        "  journal = {Kwartalnik Pustynny}, year = {2019}, pages = {41--58}}"));
    box->pasteButton()->click();
    const SourceDescription& pasted = pm.findElement(id)->source;
    CHECK(pasted.type == QStringLiteral("article"));
    CHECK(pasted.value(SourceDescription::CONTAINER) == QStringLiteral("Kwartalnik Pustynny"));
    REQUIRE(pasted.people.size() == 1);
    CHECK(pasted.people.first().name == QStringLiteral("Zielińska, Marta"));
    CHECK(box->kindBox()->currentData().toString() == QStringLiteral("article"));
    CHECK(box->field(QStringLiteral("container"))->text() == QStringLiteral("Kwartalnik Pustynny"));
    REQUIRE(box->peopleTable()->rowCount() == 1);

    // Text that is no record says so and changes nothing
    QGuiApplication::clipboard()->setText(QStringLiteral("Just a sentence."));
    QStringList said;
    using Answer = gui::dialogs::MessageDialog::Answer;
    CHECK(test::answering([&]() { box->pasteButton()->click(); }, Answer::Accept, said) == 1);
    CHECK(pm.findElement(id)->source.type == QStringLiteral("article"));

    // Without people and fields the material is no source, whatever its kind
    box->peopleTable()->setCurrentCell(0, 0);
    CHECK(test::answering([&]() { box->removePersonButton()->click(); }, Answer::Accept, said) ==
          1);
    for (const QString& field : SourceDescription::allFields()) {
        if (!box->field(field)->text().isEmpty()) {
            typeField(box->field(field), QString());
        }
    }
    CHECK(pm.findElement(id)->source.isEmpty());
}

TEST_CASE("Source card: the bookmarks of a PDF, saved at once", "[gui][materials][sources]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Dunes"));
    auto& pm = novel.pm;
    const QString id = addOf(pm, core::MaterialKinds::PUBLICATION, QStringLiteral("Atlas"),
                             QStringLiteral("atlas.pdf"), QByteArray("%PDF-1.4\n%%EOF\n"));
    REQUIRE(pm.setLastPage(id, 45));

    gui::MaterialPage page(kalahari::test::book(), nullptr, true);
    page.showMaterial(id);
    gui::BookmarksBox* box = page.bookmarksBox();
    CHECK_FALSE(box->isHidden());
    CHECK(box->table()->isHidden());
    box->addButton()->click();
    QTableWidget* table = box->table();
    REQUIRE(table->rowCount() == 1);
    REQUIRE(pm.findElement(id)->bookmarks.size() == 1);
    CHECK(pm.findElement(id)->bookmarks.first().page == 45);
    table->item(0, 1)->setText(QStringLiteral("The caravans"));
    CHECK(pm.findElement(id)->bookmarks.first().note == QStringLiteral("The caravans"));
    table->item(0, 0)->setData(Qt::EditRole, 210);
    CHECK(pm.findElement(id)->bookmarks.first().page == 210);

    // A bookmark with a note goes only when the writer agrees
    table->setCurrentCell(0, 0);
    QStringList said;
    using Answer = gui::dialogs::MessageDialog::Answer;
    CHECK(test::answering([&]() { press(table, Qt::Key_Delete); }, Answer::Cancel, said) == 1);
    CHECK(pm.findElement(id)->bookmarks.size() == 1);
    CHECK(test::answering([&]() { box->removeButton()->click(); }, Answer::Accept, said) == 1);
    CHECK(pm.findElement(id)->bookmarks.isEmpty());
}

TEST_CASE("Quote card: where a quote comes from, its reference, and the quotes of a source",
          "[gui][materials][sources]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Dunes"));
    auto& pm = novel.pm;
    const QString atlasId = addOf(pm, core::MaterialKinds::PUBLICATION, QStringLiteral("Atlas"));
    REQUIRE(pm.setSource(atlasId, atlas()));
    const QString quoteId = addOf(pm, core::MaterialKinds::QUOTE, QStringLiteral("Caravan"));
    REQUIRE(pm.setDescription(quoteId, QStringLiteral("A caravan goes as fast as its slowest "
                                                      "camel.")));

    gui::MaterialPage card(kalahari::test::book(), nullptr, true);
    QStringList opened;
    QObject::connect(&card, &gui::MaterialPage::requestOpenCard,
                     [&](const QString& id) { opened << id; });
    card.showMaterial(quoteId);
    CHECK(card.descriptionGroup()->title() == QStringLiteral("Quote"));
    CHECK(card.sourceBox()->isHidden());
    gui::QuoteSourceBox* box = card.quoteBox();
    CHECK_FALSE(box->isHidden());

    // No source: the author and the work
    CHECK(box->sourceBox()->currentData().toString().isEmpty());
    CHECK(box->authorEdit()->isVisibleTo(&card));
    CHECK_FALSE(box->openSourceButton()->isEnabled());
    typeField(box->authorEdit(), QStringLiteral("Tau"));
    typeField(box->workEdit(), QStringLiteral("A talk in Ghanzi"));
    CHECK(pm.findElement(quoteId)->quote.author == QStringLiteral("Tau"));
    CHECK(box->referenceLabel()->text() == QStringLiteral("Tau, A talk in Ghanzi"));

    // A source of the book and its page; the reference in the form of the Settings
    const int at = box->sourceBox()->findData(atlasId);
    REQUIRE(at > 0);
    CHECK(box->sourceBox()->itemText(at) ==
          QStringLiteral("Atlas pustyń świata – Wiśniewska, 2011"));
    box->sourceBox()->setCurrentIndex(at);
    emit box->sourceBox()->activated(at);
    typeField(box->pageEdit(), QStringLiteral("45"));
    CHECK(pm.findElement(quoteId)->quote.source == atlasId);
    CHECK(pm.findElement(quoteId)->quote.page == QStringLiteral("45"));
    CHECK_FALSE(box->authorEdit()->isVisibleTo(&card));
    CHECK(box->fromSourceLabel()->text().contains(QStringLiteral("Anna Wiśniewska")));
    CHECK(box->referenceLabel()->text() == QStringLiteral("(Wiśniewska 2011, p. 45)"));
    box->openSourceButton()->click();
    CHECK(opened == QStringList{atlasId});

    // The source lists its quotes; a double-click opens one, Insert asks for a new one
    gui::MaterialPage source(kalahari::test::book(), nullptr, true);
    QStringList newQuotes;
    QStringList openedQuotes;
    QObject::connect(&source, &gui::MaterialPage::requestNewQuote,
                     [&](const QString& id) { newQuotes << id; });
    QObject::connect(&source, &gui::MaterialPage::requestOpenCard,
                     [&](const QString& id) { openedQuotes << id; });
    source.showMaterial(atlasId);
    gui::SourceQuotesBox* quotes = source.quotesBox();
    CHECK_FALSE(quotes->isHidden());
    REQUIRE(quotes->table()->rowCount() == 1);
    CHECK(quotes->table()->item(0, 0)->text() == QStringLiteral("45"));
    CHECK(quotes->table()->item(0, 1)->text().startsWith(QStringLiteral("A caravan goes")));
    quotes->table()->setCurrentCell(0, 0);
    press(quotes->table(), Qt::Key_Return);
    CHECK(openedQuotes == QStringList{quoteId});
    press(quotes->table(), Qt::Key_Insert);
    CHECK(newQuotes == QStringList{atlasId});

    // The short card shows the entry in the style of the book, for a source only
    gui::MaterialPage brief(kalahari::test::book());
    brief.showMaterial(atlasId);
    CHECK(brief.sourceBox() == nullptr);
    CHECK_FALSE(brief.entryBox()->isHidden());
    CHECK(brief.entryBox()->styleBox()->isHidden());
    CHECK(brief.entryBox()->title().endsWith(QStringLiteral("PN-ISO 690")));
    brief.showMaterial(quoteId);
    CHECK(brief.entryBox()->isHidden());
    CHECK_FALSE(brief.quoteBox()->isHidden());
}

TEST_CASE("Sources: the list of the materials with a description and its node in the Navigator",
          "[gui][materials][sources]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Dunes"));
    auto& pm = novel.pm;
    const QString atlasId = addOf(pm, core::MaterialKinds::PUBLICATION, QStringLiteral("Atlas"));
    addOf(pm, core::MaterialKinds::PUBLICATION, QStringLiteral("Not described"));
    const QString pageId = addOf(pm, core::MaterialKinds::WEB_PAGE, QStringLiteral("Kalahari"));
    REQUIRE(pm.setSource(atlasId, atlas()));
    SourceDescription page;
    page.type = QString::fromLatin1(SourceDescription::WEB_PAGE);
    page.people << core::SourcePerson{
        .name = QStringLiteral("Nature Atlas"),
        .role = QString::fromLatin1(SourceDescription::INSTITUTION),
        .extra = {}};
    REQUIRE(pm.setSource(pageId, page));

    const QString sources = QString::fromLatin1(gui::MaterialList::SOURCES);
    CHECK(gui::MaterialList::nameOf(sources) == QStringLiteral("Sources"));
    CHECK(gui::MaterialList::countOf(pm, sources) == 2);
    const QList<gui::MaterialRow> rows = gui::MaterialList::rowsOf(pm, sources);
    REQUIRE(rows.size() == 2);
    CHECK(rows.at(0).id == atlasId);
    CHECK(rows.at(0).texts.value(QStringLiteral("sourceAuthors")) ==
          QStringLiteral("Wiśniewska Anna"));
    CHECK(rows.at(0).texts.value(QStringLiteral("sourceYear")) == QStringLiteral("2011"));
    CHECK(rows.at(0).texts.value(QStringLiteral("sourceKind")) == QStringLiteral("Book"));
    CHECK(rows.at(0).texts.value(QStringLiteral("sourcePlaces")) == QStringLiteral("–"));
    CHECK(rows.at(0).copy.contains(QStringLiteral("Atlas pustyń świata")));
    CHECK(rows.at(1).texts.value(QStringLiteral("sourceKind")) == QStringLiteral("Web page"));
    CHECK(gui::MaterialList::firstSortOf(sources) == QStringLiteral("sourceAuthors"));
    QStringList columns;
    for (const gui::MaterialColumn& column : gui::MaterialList::columnsOf(pm, sources)) {
        columns << column.id;
    }
    CHECK(columns.mid(0, 4) == QStringList{QStringLiteral("type"), QStringLiteral("sourceKind"),
                                           QStringLiteral("sourceAuthors"),
                                           QStringLiteral("title")});
    // Add of the sources offers the kinds that can be a source, a publication first
    const QStringList kinds = gui::MaterialList::addKindsOf(pm, sources);
    REQUIRE_FALSE(kinds.isEmpty());
    CHECK(kinds.first() == QStringLiteral("kalahari.base:publication"));
    CHECK_FALSE(kinds.contains(QStringLiteral("kalahari.base:quote")));

    // First in Materials, with its number, which follows the descriptions
    const test::Window window;
    QTreeWidget* tree = window.navigatorPanel->tree();
    QTreeWidgetItem* node = itemOf(tree, QStringLiteral("workshop:sources"));
    REQUIRE(node != nullptr);
    REQUIRE(node->parent() != nullptr);
    CHECK(node->parent()->indexOfChild(node) == 0);
    CHECK(node->font(0).italic());
    CHECK(node->data(0, Qt::AccessibleTextRole).toString() == QStringLiteral("Sources (2)"));
    CHECK(gui::NavigatorPanel::materialListOf(node) == sources);

    // Copy Entries copies the entries of all the sources shown, in the style of the book
    gui::MaterialListPanel panel(pm, sources);
    panel.refresh();
    REQUIRE(panel.entriesButton() != nullptr);
    CHECK_FALSE(panel.entriesButton()->isHidden());
    CHECK(panel.entriesButton()->text() == QStringLiteral("Copy Entries (PN-ISO 690)"));
    REQUIRE(panel.entriesButton()->isEnabled());
    QGuiApplication::clipboard()->clear();
    panel.entriesButton()->click();
    const QStringList copied = QGuiApplication::clipboard()->text().split(QLatin1Char('\n'));
    REQUIRE(copied.size() == 2);
    // By the authors, as the list sorts at first: Nature Atlas, Wiśniewska
    CHECK(copied.at(0).contains(QStringLiteral("Kalahari")));
    CHECK(copied.at(1).startsWith(QStringLiteral("WIŚNIEWSKA, Anna")));
    const QMimeData* data = QGuiApplication::clipboard()->mimeData();
    REQUIRE(data != nullptr);
    CHECK(data->html().contains(QStringLiteral("<i>Atlas pustyń świata")));
    // The other lists have no Copy Entries
    gui::MaterialListPanel quotes(pm, QStringLiteral("kalahari.base:quote"));
    quotes.refresh();
    CHECK(quotes.entriesButton()->isHidden());

    // A description emptied: the number follows by itself
    REQUIRE(pm.setSource(pageId, SourceDescription{}));
    CHECK(node->data(0, Qt::AccessibleTextRole).toString() == QStringLiteral("Sources (1)"));
}

TEST_CASE("Sources: a new quote from a source opens on its card at the page shown last",
          "[gui][materials][sources]") {
    gui::registerAllIcons();
    test::OpenNovel novel(QStringLiteral("Dunes"));
    auto& pm = novel.pm;
    const QString atlasId = addOf(pm, core::MaterialKinds::PUBLICATION, QStringLiteral("Atlas"));
    REQUIRE(pm.setSource(atlasId, atlas()));
    REQUIRE(pm.setLastPage(atlasId, 12));

    test::Window window;
    window.navigator.onRequestNewQuote(atlasId);
    const QList<const core::ProjectElement*> quotes = pm.quotesOf(atlasId);
    REQUIRE(quotes.size() == 1);
    CHECK(quotes.first()->quote.page == QStringLiteral("12"));
    CHECK(quotes.first()->title == QStringLiteral("Quote from: Atlas pustyń świata"));
    auto* card = qobject_cast<gui::MaterialCardPanel*>(window.tabs->currentWidget());
    REQUIRE(card != nullptr);
    CHECK(card->elementId() == quotes.first()->id);
}
