// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_relations.h
/// @brief The relations between the cards of a book: what each card shows of them, and the
/// relations the window Relation offers between two cards

#ifndef KALAHARI_CORE_CARD_RELATIONS_H
#define KALAHARI_CORE_CARD_RELATIONS_H

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/card_data.h"

#include <QList>
#include <QString>

#include <optional>

namespace kalahari::core {

class ProjectManager;

/// @brief A relation between two cards, found by its id: of a package (CardRelation) or named
/// by the writer (OwnRelation)
struct RelationInfo {
    QString id;                              ///< "sibling", "own-1"
    QString inverse;                         ///< The relation of the other card
    QString kind;                            ///< The kind of the card that has it:
                                             ///< "kalahari.base:character"
    QString otherKind;                       ///< The kind of the other card
    const CardRelation* relation = nullptr;  ///< Of a package; nullptr: named by the writer
    const OwnRelation* own = nullptr;        ///< Named by the writer; nullptr: of a package

    /// @brief Whether the writer named it
    [[nodiscard]] bool isOwn() const { return own != nullptr; }

    /// @brief How the window Relation offers it in @p language: "siblings", "creditor"
    [[nodiscard]] QString choiceText(const QString& language) const;

    /// @brief Its group in the window Relation in @p language: "family"; empty: none
    [[nodiscard]] QString groupText(const QString& language) const;

    /// @brief What the other card, of gender @p gender (empty: not said), is to the card in
    /// @p language: "brother"
    [[nodiscard]] QString nameFor(const QString& gender, const QString& language) const;
};

/// @brief A relation of a card as the card shows it
struct ShownRelation {
    QString otherId;   ///< The other card
    QString relation;  ///< The relation of the card with the other one, as the card has it
    QString note;      ///< The note of the relation; empty: none
    QString ownerId;   ///< The card whose link it is: the card itself or the other card
    QString linkTo;    ///< The card that link names: the other card or the card itself
    QString linkRole;  ///< The role of that link: the relation as its owner has it

    /// @brief Whether it is the same relation, of the same link
    bool operator==(const ShownRelation&) const = default;
};

/// @brief What a new text of a field of the type "card" with a relation changes
/// (CardField::relation)
struct FieldChange {
    RelationInfo relation;  ///< The relation of the field
    QString before;         ///< The card it named; empty: none
    QString named;          ///< The card it names now, whose title is the text; empty: none
    QString value;          ///< Its value now: the text when it names no card
};

/// @brief The relations between the cards of the open book
///
/// A relation is a link of a card with another card (ElementLink), with the id of the relation
/// as its role, saved with the card it was added to; the other card shows the inverse
/// relation. A field of the type "card" with a relation (CardField::relation) names its card
/// by such a link. The ids are those of the relations of the packages, found in the package of
/// the kind of the card and then in the other packages, and those of the Workshop
/// (OwnRelation), which come first.
class CardRelations {
public:
    /// @param book The open book
    explicit CardRelations(const ProjectManager& book);

    /// @brief Relation @p relationId of a card of kind @p kind ("kalahari.base:character");
    /// nullopt when the book has none
    [[nodiscard]] std::optional<RelationInfo> find(const QString& relationId,
                                                   const QString& kind) const;

    /// @brief The relations that card @p card can have with card @p other, in the order the
    /// window Relation offers them: those of the packages, then those of the writer
    [[nodiscard]] QList<RelationInfo> between(const ProjectElement& card,
                                              const ProjectElement& other) const;

    /// @brief The relations that a card of kind @p kind ("kalahari.base:character") can have
    /// with a card of kind @p otherKind, as between() gives them
    [[nodiscard]] QList<RelationInfo> between(const QString& kind,
                                              const QString& otherKind) const;

    /// @brief Relation @p relationId between card @p cardId and card @p otherId, when it is one
    /// of those between() offers; nullopt for two elements that are not two cards
    [[nodiscard]] std::optional<RelationInfo> offered(const QString& cardId,
                                                      const QString& otherId,
                                                      const QString& relationId) const;

    /// @brief What text @p text of field @p fieldId of card @p card changes: nullopt for a field
    /// without a relation, whose text is its value
    [[nodiscard]] std::optional<FieldChange> fieldChange(const ProjectElement& card,
                                                         const QString& fieldId,
                                                         const QString& text) const;

    /// @brief What card @p other is to a card of kind @p kind under relation @p relationId in
    /// @p language: its name by the gender of @p other; the id for a relation the book does not
    /// have
    [[nodiscard]] QString labelOf(const QString& relationId, const QString& kind,
                                  const ProjectElement& other, const QString& language) const;

    /// @brief The relations of card @p cardId: its links with cards, then the links of the other
    /// cards with it, as the card has them
    [[nodiscard]] QList<ShownRelation> of(const QString& cardId) const;

    /// @brief The card that field @p field of card @p card names by its relation: the card of
    /// its link, or a card whose link with @p card has the inverse relation; nullptr when it
    /// names none
    [[nodiscard]] const ProjectElement* fieldCard(const ProjectElement& card,
                                                  const CardField& field) const;

    /// @brief The card of kind @p kind ("kalahari.base:location") whose title is @p title, with
    /// no regard to the case of the letters and the spaces at its ends; nullptr when there is
    /// none
    [[nodiscard]] const ProjectElement* cardTitled(const QString& title,
                                                   const QString& kind) const;

    /// @brief The kind of card @p card: "kalahari.base:character"; empty when the program does
    /// not have it
    [[nodiscard]] QString kindOf(const ProjectElement& card) const;

    /// @brief The gender of card @p card: the value of its field CardRelation::GENDER_FIELD
    [[nodiscard]] static QString genderOf(const ProjectElement& card);

    /// @brief The cards of the Workshop, in its order, those of its groups too
    [[nodiscard]] const QList<const ProjectElement*>& cards() const { return m_cards; }

private:
    /// @brief Relation @p relationId of the packages for a card of kind @p kind
    [[nodiscard]] std::optional<RelationInfo> findInPackages(const QString& relationId,
                                                             const QString& kind) const;

    const ProjectManager* m_book;          ///< The open book; never nullptr
    QList<const ProjectElement*> m_cards;  ///< The cards of the Workshop, in its order
};

/// @brief The changes of the relations in the links of the cards of a project
namespace card_links {

/// @brief Give card @p cardId relation @p relation with card @p otherId and the note @p note:
/// a new link of the card; the same relation between them, a link of either card, gets the
/// note
void addRelation(BookProject& project, const QString& cardId, const QString& otherId,
                 const RelationInfo& relation, const QString& note);

/// @brief Take relation @p relation out of the links of its owner
void removeRelation(BookProject& project, const ShownRelation& relation);

/// @brief Make card @p cardId name card @p otherId (empty: none) by relation @p relation, of a
/// field of the type "card": the links of either card that named another card by it go
void setFieldCard(BookProject& project, const QString& cardId, const RelationInfo& relation,
                  const QString& otherId);

/// @brief Give field @p fieldId of card @p cardId the value @p value; empty: none
void setValue(BookProject& project, const QString& cardId, const QString& fieldId,
              const QString& value);

/// @brief The id of the relation of the writer named @p name of a card of kind @p kind with a
/// card of kind @p otherKind, whose inverse is named @p inverseName: one of @p relations, or a
/// new one added to them with its inverse (the same names of the same kinds: the same both
/// ways)
QString ownRelation(QList<OwnRelation>& relations, const QString& kind, const QString& otherKind,
                    const QString& name, const QString& inverseName);

}  // namespace card_links

}  // namespace kalahari::core

#endif  // KALAHARI_CORE_CARD_RELATIONS_H
