// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_list.cpp
/// @brief The lists of the cards of the characters, the locations and the items on their tabs

#include "kalahari/gui/card_list.h"

#include "kalahari/core/book_dictionary.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/card_names.h"
#include "kalahari/core/kind_words.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/card_parts.h"
#include "kalahari/gui/panels/navigator_panel.h"

#include <QLocale>

#include <algorithm>
#include <functional>
#include <optional>

namespace kalahari::gui {

namespace {

/// The kind of cards that list @p list names, of the packages of @p registry; none for a list
/// of another kind
core::KindRef cardKindOf(const core::BookTypeRegistry& registry, const QString& list) {
    const std::optional<core::KindReference> reference = core::KindReference::parse(list);
    const core::KindRef kind =
        reference ? registry.findKind(reference->packageId, reference->kindId) : core::KindRef{};
    return kind && kind.kind->isCard() ? kind : core::KindRef{};
}

/// The cards of kind @p kind of the Workshop of the book open in @p book, in its order
QList<const core::ProjectElement*> cardsOf(const core::ProjectManager& book,
                                           const core::KindRef& kind) {
    QList<const core::ProjectElement*> cards;
    const core::BookProject* project = book.project();
    if (project == nullptr || !kind) {
        return cards;
    }
    const QString reference = kind.reference();
    const std::function<void(const QList<core::ProjectElement>&)> collect =
        [&](const QList<core::ProjectElement>& elements) {
            for (const core::ProjectElement& element : elements) {
                if (book.kindOf(element).reference() == reference) {
                    cards << &element;
                }
                collect(element.elements);
            }
        };
    collect(project->workshop.elements);
    return cards;
}

/// The elements of the books of @p project in reading order: those a card can be in
QList<const core::ProjectElement*> bookElements(const core::BookProject& project) {
    QList<const core::ProjectElement*> elements;
    for (qsizetype book = 0; book < project.books.size(); ++book) {
        elements << project.readingOrder(book);
    }
    return elements;
}

/// The place of the value @p value of field @p field among its choices, by which the column
/// sorts: a card without a value after the others
int choiceOrder(const core::CardField& field, const QString& value) {
    const auto found =
        std::find_if(field.choices.cbegin(), field.choices.cend(),
                     [&value](const core::CardFieldChoice& choice) { return choice.id == value; });
    return static_cast<int>(std::distance(field.choices.cbegin(), found));
}

/// What a card shows under its name on its tile: its kind with the value of the field that
/// stands beside it ("Character · main")
QString subtitleOf(const core::ProjectManager& book, const core::ProjectElement& element) {
    const QString kind =
        core::fillWords(QStringLiteral("{Kind}"), {{QStringLiteral("kind"), wordsOf(book, element)}});
    const QString value = card::subtitleValue(book, element);
    //: The kind of a card on its tile in the list of cards, with the value of the field beside
    //: it: "Character · main". In Polish: %1 · %2
    return value.isEmpty() ? kind : CardList::tr("%1 · %2").arg(kind, value);
}

/// Put @p text in column @p columnId of @p row; an empty one stays out
void putText(MaterialRow& row, const QString& columnId, const QString& text) {
    if (!text.isEmpty()) {
        row.texts.insert(columnId, text);
    }
}

/// What card @p element of kind @p kind shows in the columns of the fields: those of its kind,
/// a value to choose sorted by the order of the choices, and those of the writer's
void setFieldTexts(const core::ProjectManager& book, MaterialRow& row,
                   const core::ProjectElement& element, const core::ElementKind& kind) {
    for (const core::CardField& field : kind.fields) {
        const QString id = QString::fromLatin1(CardList::FIELD_PREFIX) + field.id;
        putText(row, id, CardList::textOf(book, element, field).simplified());
        if (field.type == core::CardFieldType::Choice) {
            row.order.insert(id, choiceOrder(field, element.card.value(field.id)));
        }
    }
    for (const core::ElementField& field : element.fields) {
        const QString name = field.name.trimmed();
        if (!name.isEmpty()) {
            row.texts.insert(QString::fromLatin1(MaterialList::FIELD_PREFIX) + name,
                             field.value.simplified());
        }
    }
}

/// What the search finds card @p element by, in small letters: its name with its forms and
/// its other names in the book's dictionary, its fields and In Short
QString searchOf(const core::ProjectManager& book, const core::ProjectElement& element,
                 const MaterialRow& row) {
    QStringList all{element.title, element.description};
    if (const core::DictionaryName* name = book.bookDictionary().findNameOfCard(element.id)) {
        all << core::cardForms(*name);
    }
    for (auto it = row.texts.cbegin(); it != row.texts.cend(); ++it) {
        if (it.key().startsWith(QLatin1String(CardList::FIELD_PREFIX)) ||
            it.key().startsWith(QLatin1String(MaterialList::FIELD_PREFIX))) {
            all << it.value();
        }
    }
    for (const core::ElementField& field : element.fields) {
        all << field.name;
    }
    return all.join(QLatin1Char('\n')).toLower();
}

/// The row of card @p element of kind @p kind, in @p chapters of the books, with @p relations
MaterialRow rowOf(const core::ProjectManager& book, const core::ProjectElement& element,
                  const core::KindRef& kind, int chapters, int relations) {
    MaterialRow row;
    row.id = element.id;
    row.kind = kind.reference();
    row.iconId = NavigatorPanel::iconIdOf(book.bookTypes(), element);
    row.card = true;
    row.color = element.card.color;
    row.summary = element.description.simplified();
    row.links = element.linkTargets();
    row.copy = element.title;
    putText(row, QString::fromLatin1(MaterialList::TITLE), element.title);
    setFieldTexts(book, row, element, *kind.kind);
    putText(row, QString::fromLatin1(MaterialList::DESCRIPTION), row.summary);
    putText(row, QString::fromLatin1(MaterialList::DETAILS), subtitleOf(book, element));
    const QString chaptersId = QString::fromLatin1(CardList::CHAPTERS);
    const QString relationsId = QString::fromLatin1(CardList::RELATIONS);
    putText(row, chaptersId, QLocale().toString(chapters));
    row.order.insert(chaptersId, chapters);
    putText(row, relationsId, QLocale().toString(relations));
    row.order.insert(relationsId, relations);
    row.search = searchOf(book, element, row);
    return row;
}

}  // namespace

bool CardList::isCardList(const QString& list) {
    return static_cast<bool>(cardKindOf(core::ProjectManager::installedBookTypes(), list));
}

QList<MaterialColumn> CardList::columnsOf(const core::ProjectManager& book, const QString& list) {
    const core::KindRef kind = cardKindOf(book.bookTypes(), list);
    const bool editable = book.isProjectOpen() && !book.isReadOnly();
    //: The column of the names of the cards in the list of cards. In Polish: Nazwa
    QList<MaterialColumn> columns{{QString::fromLatin1(MaterialList::TITLE), tr("Name")}};
    if (kind) {
        for (const core::CardField& field : kind.kind->fields) {
            columns << MaterialColumn{QString::fromLatin1(FIELD_PREFIX) + field.id,
                                      field.name.text(programLanguage()), field.inList, true,
                                      editable};
        }
    }
    //: The column of the short description of a card: who a character is, what a location is.
    //: In Polish: W skrócie
    columns << MaterialColumn{QString::fromLatin1(MaterialList::DESCRIPTION), tr("In Short"), true,
                              true, editable}
            //: How many chapters a card is in. In Polish: Rozdziały
            << MaterialColumn{QString::fromLatin1(CHAPTERS), tr("Chapters")}
            //: How many relations a card has with the other cards. In Polish: Relacje
            << MaterialColumn{QString::fromLatin1(RELATIONS), tr("Relations")};
    MaterialList::appendFieldColumns(columns, cardsOf(book, kind));
    return columns;
}

QList<MaterialRow> CardList::rowsOf(const core::ProjectManager& book, const QString& list) {
    QList<MaterialRow> rows;
    const core::KindRef kind = cardKindOf(book.bookTypes(), list);
    const core::BookProject* project = book.project();
    if (!kind || project == nullptr) {
        return rows;
    }
    const QList<const core::ProjectElement*> chapters = bookElements(*project);
    const core::CardRelations relations = book.cardRelations();
    for (const core::ProjectElement* element : cardsOf(book, kind)) {
        const auto in = static_cast<int>(
            std::count_if(chapters.cbegin(), chapters.cend(),
                          [element](const core::ProjectElement* chapter) {
                              return element->linkTo(chapter->id) != nullptr;
                          }));
        rows << rowOf(book, *element, kind, in, static_cast<int>(relations.of(element->id).size()));
    }
    return rows;
}

QString CardList::fieldOf(const QString& columnId) {
    const QLatin1String prefix(FIELD_PREFIX);
    return columnId.startsWith(prefix) ? columnId.mid(prefix.size()) : QString();
}

QString CardList::textOf(const core::ProjectManager& book, const core::ProjectElement& card,
                         const core::CardField& field) {
    const QString value = card.card.value(field.id);
    if (field.type == core::CardFieldType::Choice) {
        const core::CardFieldChoice* choice = field.choice(value);
        return choice != nullptr ? choice->name.text(programLanguage()) : value;
    }
    return field.type == core::CardFieldType::Card ? book.cardFieldText(card.id, field.id) : value;
}

bool CardList::setText(core::ProjectManager& book, const QString& cardId, const QString& columnId,
                       const QString& text) {
    if (columnId == QLatin1String(MaterialList::DESCRIPTION)) {
        return book.setDescription(cardId, text);
    }
    const QString field = fieldOf(columnId);
    return !field.isEmpty() && book.setCardFieldText(cardId, field, text);
}

}  // namespace kalahari::gui
