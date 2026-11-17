// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_names_coordinator.cpp
/// @brief The names of the cards of the Workshop in the book's dictionary

#include "kalahari/gui/card_names_coordinator.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_dictionary.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/card_names.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/name_declension.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/book_dictionary_coordinator.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/utils/access_keys.h"

#include <QList>
#include <QLocale>
#include <QSet>
#include <QStringList>
#include <QTabWidget>

#include <algorithm>
#include <optional>

namespace kalahari::gui {

namespace {

/// @brief The kinds of the cards of the Workshop of @p book that can be added, in the order
///        of its type
QList<core::KindRef> addableCardKinds(const core::ProjectManager& book) {
    QList<core::KindRef> kinds;
    if (const core::BookProject* project = book.project()) {
        for (const core::KindRef& kind :
             project->kindsIn(book.bookTypes(), core::BookPlace::Workshop)) {
            if (kind && kind.kind->isCard() && core::ProjectManager::canAdd(kind)) {
                kinds << kind;
            }
        }
    }
    return kinds;
}

}  // namespace

CardNamesCoordinator::CardNamesCoordinator(core::ProjectManager& book,
                                           BookDictionaryCoordinator& dictionary,
                                           QTabWidget* centralTabs, QObject* parent)
    : QObject(parent), m_book(book), m_dictionary(dictionary), m_centralTabs(centralTabs) {
    connect(&m_book, &core::ProjectManager::cardAdded, this, &CardNamesCoordinator::nameCard);
    connect(&m_book, &core::ProjectManager::elementRenamed, this,
            &CardNamesCoordinator::retitleCard);
    if (m_centralTabs != nullptr) {
        connect(&DocumentWindows::of(*m_centralTabs), &DocumentWindows::frontChanged, this,
                &CardNamesCoordinator::offerCards);
    }
}

core::NameKind CardNamesCoordinator::nameKindOf(const core::ProjectManager& book,
                                                const core::ProjectElement& card) {
    const core::KindRef kind = book.kindOf(card);
    return kind ? core::nameKindOf(*kind.kind) : core::NameKind::Other;
}

bool CardNamesCoordinator::editName(const QString& cardId) {
    const core::ProjectElement* card = m_book.findElement(cardId);
    return card != nullptr &&
           m_dictionary.editCardName(cardId, card->title, nameKindOf(m_book, *card));
}

bool CardNamesCoordinator::addOtherName(const QString& cardId, QWidget* window) {
    const core::ProjectElement* card = m_book.findElement(cardId);
    if (card == nullptr || m_book.isReadOnly()) {
        return false;
    }
    //: The window of a new other name of a card: a diminutive, a nickname, a surname alone.
    //: In Polish: Inne imię lub nazwa
    const std::optional<QString> text = dialogs::TextInputDialog::getText(
        window, tr("Other Name"),
        //: %1: the name of the card in quotes. In Polish: Inne imię lub inna nazwa karty %1
        //: (zdrobnienie, przydomek)
        tr("Another name of %1 (a diminutive, a nickname)")
            .arg(QLocale().quoteString(card->title)),
        QString(), tr("&Add"));
    const QString written = text ? text->simplified() : QString();
    if (written.isEmpty()) {
        return false;
    }
    const core::NameKind kind = nameKindOf(m_book, *card);
    core::DictionaryName name =
        core::nameOfCard(m_book.bookDictionary(), cardId, card->title, kind,
                         m_dictionary.declension(), m_dictionary.dictionaryForms());
    // A name it has already is not added again
    const bool known = std::ranges::any_of(name.variants, [&written](const auto& each) {
        return each.text.compare(written, Qt::CaseInsensitive) == 0;
    });
    if (known) {
        return false;
    }
    core::NameVariant variant;
    variant.text = written;
    core::refreshForms(variant, kind, m_dictionary.declension(), m_dictionary.dictionaryForms());
    name.variants.append(variant);
    core::Logger::getInstance().info("Card names: other name {} of {}", written.toStdString(),
                                     cardId.toStdString());
    return m_book.changeBookDictionary(
        [&name](core::BookDictionary& dictionary) { dictionary.putName(name); });
}

void CardNamesCoordinator::nameCard(const QString& cardId) {
    const core::ProjectElement* card = m_book.findElement(cardId);
    if (card == nullptr || m_book.bookDictionary().findNameOfCard(cardId) != nullptr) {
        return;
    }
    const core::DictionaryName name =
        core::nameOfCard(m_book.bookDictionary(), cardId, card->title, nameKindOf(m_book, *card),
                         m_dictionary.declension(), m_dictionary.dictionaryForms());
    if (!m_book.changeBookDictionary(
            [&name](core::BookDictionary& dictionary) { dictionary.putName(name); })) {
        core::Logger::getInstance().warn("Card names: {} has no name in the book's dictionary",
                                         cardId.toStdString());
    }
}

void CardNamesCoordinator::retitleCard(const QString& cardId) {
    const core::ProjectElement* card = m_book.findElement(cardId);
    const core::DictionaryName* own = m_book.bookDictionary().findNameOfCard(cardId);
    if (card == nullptr || own == nullptr) {
        return;
    }
    core::DictionaryName name = *own;
    if (core::retitleName(name, card->title, m_dictionary.declension(),
                          m_dictionary.dictionaryForms())) {
        m_book.changeBookDictionary(
            [&name](core::BookDictionary& dictionary) { dictionary.putName(name); });
    }
}

void CardNamesCoordinator::offerCards(QWidget* page) {
    auto* panel = qobject_cast<EditorPanel*>(page);
    editor::BookEditor* text = panel != nullptr ? panel->getBookEditor() : nullptr;
    if (text == nullptr) {
        return;
    }
    // The texts of the book, while it can change; the names of the kinds with free keys
    QList<editor::BookEditor::CardKindChoice> choices;
    const bool ofBook =
        m_book.isProjectOpen() && !panel->property("elementId").toString().isEmpty();
    if (ofBook && !m_book.isReadOnly()) {
        const QList<core::KindRef> kinds = addableCardKinds(m_book);
        QStringList names;
        for (const core::KindRef& kind : kinds) {
            names << kind.kind->name.text(programLanguage());
        }
        QSet<QChar> used;
        const QStringList texts = withFreeAccessKeys(names, used);
        auto& art = core::ArtProvider::getInstance();
        for (qsizetype i = 0; i < kinds.size(); ++i) {
            const core::KindRef& kind = kinds.at(i);
            choices.append({.kind = kind.reference(),
                            .text = texts.value(i) + QStringLiteral("..."),
                            .icon = art.getIcon(kind.kind->icon, core::IconContext::Menu)});
        }
    }
    text->setCardKinds(choices);
    connect(text, &editor::BookEditor::createCardAsked, this,
            &CardNamesCoordinator::onCreateCardAsked, Qt::UniqueConnection);
}

void CardNamesCoordinator::onCreateCardAsked(const QString& title, const QString& kind) {
    const std::optional<core::KindReference> reference = core::KindReference::parse(kind);
    const core::KindRef found =
        reference ? m_book.bookTypes().findKind(reference->packageId, reference->kindId)
                  : core::KindRef();
    if (found) {
        emit addCardRequested(title, found);
    }
}

}  // namespace kalahari::gui
