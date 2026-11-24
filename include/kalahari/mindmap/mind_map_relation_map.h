// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_relation_map.h
/// @brief Maps of relations: the cards of the Workshop on a mind map with the relations they
/// have; the window New Relation Map and the offer after the first relation of a card

#ifndef KALAHARI_MINDMAP_MIND_MAP_RELATION_MAP_H
#define KALAHARI_MINDMAP_MIND_MAP_RELATION_MAP_H

#include <kalahari/core/window_editor_provider.h>
#include <kalahari/gui/dialogs/kalahari_dialog.h>
#include <kalahari/mindmap/mind_map.h>

#include <QFrame>
#include <QList>
#include <QString>
#include <QStringList>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::mindmap {

/// @brief The cards a map of relations of @p options puts on it: the cards of its kinds that
/// have relations it shows with each other, in the order of the Workshop
/// @param withoutRelations Also the cards without such relations
[[nodiscard]] QStringList relationMapCards(const core::ProjectManager& book,
                                           const RelationMapOptions& options,
                                           bool withoutRelations = false);

/// @brief The cards with relations that map @p map of relations of @p options does not have
/// yet: those that came to the book, or had their first relations, since it was made
[[nodiscard]] QStringList missingCards(const core::ProjectManager& book, const MindMap& map,
                                       const RelationMapOptions& options);

/// @brief Add a node of each card of @p cards to @p map, loose and not placed yet (the map
/// places them when it is drawn)
void addCardNodes(MindMap& map, const QStringList& cards);

/// @brief Add a map of relations titled @p title with @p options to the Workshop of @p book,
/// with the cards it shows
/// @return Its id; empty when it could not be made
QString createRelationMap(core::ProjectManager& book, const QString& title,
                          const RelationMapOptions& options);

/// @brief New Relation Map: its name, the kinds of its cards, the relations it shows and its
/// layout
///
/// +--------------------------------------------------------------+
/// | Name:      [Relacje postaci                          ]       |
/// | Cards:     [x] Characters  [ ] Places  [ ] Items             |
/// | Relations: [x] Family  [x] Other relations                   |
/// |            [x] Places and items of the characters            |
/// | Layout:    (o) The family in generations  ( ) Free           |
/// | The map takes the relations the cards have now; new ones     |
/// | come to it when it opens.                                    |
/// |                                          [Create]  [Cancel]  |
/// +--------------------------------------------------------------+
class RelationMapDialog : public gui::dialogs::KalahariDialog {
    Q_OBJECT

public:
    /// @param kinds The kinds of the cards checked first; empty: the characters
    RelationMapDialog(const core::ProjectManager& book, const QStringList& kinds,
                      QWidget* parent = nullptr);

    [[nodiscard]] QString title() const;
    [[nodiscard]] RelationMapOptions options() const;

    [[nodiscard]] QLineEdit* nameEdit() const { return m_name; }
    [[nodiscard]] const QList<QCheckBox*>& kindBoxes() const { return m_kinds; }

private:
    void updateAccept();

    QLineEdit* m_name = nullptr;
    QList<QCheckBox*> m_kinds;
    QStringList m_kindIds;
    QCheckBox* m_family = nullptr;
    QCheckBox* m_others = nullptr;
    QCheckBox* m_across = nullptr;
    QRadioButton* m_generations = nullptr;
    QRadioButton* m_free = nullptr;
};

/// @brief Open New Relation Map for the book of @p context, make the map and open it
/// @param kinds The kinds of the cards checked first; empty: the characters
void askNewRelationMap(const core::WindowEditorContext& context, const QStringList& kinds,
                       QWidget* parent);

/// @brief The small bar on a card after its first relation: it offers a map of relations
///
/// "Amara has her first relation: “mother” – Jan. Create a map of the relations of the
/// characters? You will see the relations of the cards on one map."
/// [Create Relation Map] [Not Now] [Do Not Ask Again]
///
/// It closes itself after a while, or when a button is pressed.
class RelationMapOffer : public QFrame {
    Q_OBJECT

public:
    RelationMapOffer(const QString& message, QWidget* parent);

    [[nodiscard]] QPushButton* createButton() const { return m_create; }
    [[nodiscard]] QPushButton* laterButton() const { return m_later; }
    [[nodiscard]] QPushButton* neverButton() const { return m_never; }

signals:
    /// @brief The writer wants the map
    void createRequested();

private:
    QPushButton* m_create = nullptr;
    QPushButton* m_later = nullptr;
    QPushButton* m_never = nullptr;
};

/// @brief After relation @p relationId of card @p cardId with card @p otherId was added: when
/// it is the first relation of the card, the book has no map of relations of its kind and the
/// settings allow it (Settings > Mind Maps), the offer, for the caller to show it
/// @return The offer, a child of @p parent; nullptr: none
RelationMapOffer* offerRelationMap(const core::WindowEditorContext& context, const QString& cardId,
                                   const QString& otherId, const QString& relationId,
                                   QWidget* parent);

}  // namespace kalahari::mindmap

#endif  // KALAHARI_MINDMAP_MIND_MAP_RELATION_MAP_H
