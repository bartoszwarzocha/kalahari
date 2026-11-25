// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file mind_map_relation_map.cpp
/// @brief Maps of relations: which cards they show, New Relation Map and the offer after the
/// first relation of a card

#include <kalahari/mindmap/mind_map_relation_map.h>

#include <kalahari/core/card_relations.h>
#include <kalahari/core/kind_words.h>
#include <kalahari/core/logger.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/gui/kind_names.h>
#include <kalahari/mindmap/mind_map_content.h>
#include <kalahari/mindmap/mind_map_look.h>
#include <kalahari/mindmap/mind_map_module.h>

#include <QButtonGroup>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QTimer>
#include <QVBoxLayout>

namespace kalahari::mindmap {

namespace {

/// How long the offer after the first relation stays
constexpr int OFFER_TIME = 30000;
const QString CHARACTER = QStringLiteral("kalahari.base:character");

/// The kind of the mind maps of the Workshop of @p book
core::KindRef mindMapKind(const core::ProjectManager& book) {
    for (const core::KindRef& kind : book.kindsFor(core::BookPlace::Workshop)) {
        if (kind.kind->editor == QLatin1String(MindMapModule::EDITOR_ID)) {
            return kind;
        }
    }
    return {};
}

/// Whether card @p cardId has relations that a map of @p options shows, with cards of its
/// kinds
bool hasShownRelations(const core::ProjectManager& book, const MindMapContent& content,
                       const core::CardRelations& relations, const RelationMapOptions& options,
                       const core::ProjectElement& card) {
    const QString kind = card.kind.toString();
    for (const core::ShownRelation& relation : relations.of(card.id)) {
        const core::ProjectElement* other = book.findElement(relation.otherId);
        if (other == nullptr) {
            continue;
        }
        const QString otherKind = other->kind.toString();
        if (!options.kinds.contains(otherKind)) {
            continue;
        }
        const QString owner = relation.ownerId == card.id ? kind : otherKind;
        if (MindMapContent::shows(options, kind, otherKind,
                                  content.groupOf(relation.linkRole, owner))) {
            return true;
        }
    }
    return false;
}

/// Whether the book has a map of relations with cards of kind @p kind
bool hasRelationMapOf(const core::ProjectManager& book, const QString& kind) {
    const core::KindRef mapKind = mindMapKind(book);
    if (!mapKind || book.project() == nullptr) {
        return false;
    }
    const auto visit = [&book, &kind, &mapKind](
                           const auto& self, const QList<core::ProjectElement>& elements) -> bool {
        for (const core::ProjectElement& element : elements) {
            if (element.kind.toString() == mapKind.reference()) {
                MindMap map;
                if (map.load(book.filePathOf(element)) && map.relationMap() &&
                    map.relationMap()->kinds.contains(kind)) {
                    return true;
                }
            }
            if (self(self, element.elements)) {
                return true;
            }
        }
        return false;
    };
    return visit(visit, book.project()->elementsIn(core::BookPlace::Workshop));
}

}  // namespace

QStringList relationMapCards(const core::ProjectManager& book, const RelationMapOptions& options,
                             bool withoutRelations) {
    QStringList cards;
    const MindMapContent content(&book);
    const core::CardRelations relations = book.cardRelations();
    for (const core::ProjectElement* card : relations.cards()) {
        if (options.kinds.contains(card->kind.toString()) &&
            (withoutRelations || hasShownRelations(book, content, relations, options, *card))) {
            cards.append(card->id);
        }
    }
    return cards;
}

QStringList missingCards(const core::ProjectManager& book, const MindMap& map,
                         const RelationMapOptions& options) {
    QStringList missing;
    for (const QString& card : relationMapCards(book, options)) {
        if (map.nodeOfElement(card).isEmpty()) {
            missing.append(card);
        }
    }
    return missing;
}

void addCardNodes(MindMap& map, const QStringList& cards) {
    for (const QString& card : cards) {
        const QString id = map.addNode(QString::fromLatin1(MindMapNode::ELEMENT_KIND), QString(),
                                       QPointF());
        if (const MindMapNode* node = map.node(id)) {
            MindMapNode changed = *node;
            changed.link = card;
            changed.place.reset();
            map.setNode(changed);
        }
    }
}

QString createRelationMap(core::ProjectManager& book, const QString& title,
                          const RelationMapOptions& options) {
    const core::KindRef kind = mindMapKind(book);
    if (!kind) {
        core::Logger::getInstance().warn("createRelationMap: the book has no mind maps");
        return QString();
    }
    QString id = book.addElement(kind, title, core::BookPlace::Workshop);
    const core::ProjectElement* element = id.isEmpty() ? nullptr : book.findElement(id);
    if (element == nullptr) {
        return QString();
    }
    MindMap map;
    map.setLayout(MindMapLayout::Right);
    map.setRelationMap(options);
    QStringList cards = relationMapCards(book, options);
    if (cards.isEmpty()) {
        cards = relationMapCards(book, options, true);
    }
    addCardNodes(map, cards);
    QString error;
    if (!map.save(book.filePathOf(*element), &error)) {
        core::Logger::getInstance().error("createRelationMap: {}", error.toStdString());
    }
    return id;
}

// =============================================================================
// New Relation Map
// =============================================================================

RelationMapDialog::RelationMapDialog(const core::ProjectManager& book, const QStringList& kinds,
                                     QWidget* parent)
    : KalahariDialog(parent) {
    //: The window that makes a map of the relations of the cards. In Polish: Nowa mapa relacji
    setHeading(tr("New Relation Map"),
               //: In Polish: Karty Warsztatu na mapie myśli, z relacjami, które mają.
               tr("The cards of the Workshop on a mind map, with the relations they have."));
    setHeadingIcon(QString::fromLatin1(MindMapIcons::RELATION_MAP));

    m_name = new QLineEdit(this);
    //: The name of a new map of relations. In Polish: Relacje postaci
    m_name->setText(tr("Relations of the characters"));
    //: In Polish: Nazwa
    addField(tr("Name"), m_name);

    auto* cards = new QWidget(this);
    auto* cardRow = new QHBoxLayout(cards);
    cardRow->setContentsMargins(0, 0, 0, 0);
    const MindMapContent content(&book);
    const QStringList checked = kinds.isEmpty() ? QStringList{CHARACTER} : kinds;
    for (const QString& kind : content.cardKinds()) {
        auto* box = new QCheckBox(content.pluralOf(kind), cards);
        box->setChecked(checked.contains(kind));
        cardRow->addWidget(box);
        m_kinds.append(box);
        m_kindIds.append(kind);
        connect(box, &QCheckBox::toggled, this, &RelationMapDialog::updateAccept);
    }
    cardRow->addStretch(1);
    //: In Polish: Karty
    addField(tr("Cards"), cards);

    auto* groups = new QWidget(this);
    auto* groupColumn = new QVBoxLayout(groups);
    groupColumn->setContentsMargins(0, 0, 0, 0);
    //: The relations a map of relations shows. In Polish: Rodzina
    m_family = new QCheckBox(tr("Family"), groups);
    //: In Polish: Inne relacje
    m_others = new QCheckBox(tr("Other relations"), groups);
    //: In Polish: Miejsca i przedmioty postaci
    m_across = new QCheckBox(tr("Places and items of the characters"), groups);
    for (QCheckBox* box : {m_family, m_others, m_across}) {
        box->setChecked(true);
        groupColumn->addWidget(box);
        connect(box, &QCheckBox::toggled, this, &RelationMapDialog::updateAccept);
    }
    //: In Polish: Relacje
    addField(tr("Relations"), groups);

    auto* layouts = new QWidget(this);
    auto* layoutRow = new QHBoxLayout(layouts);
    layoutRow->setContentsMargins(0, 0, 0, 0);
    //: The layout of a new map of relations. In Polish: rodzina w pokoleniach
    m_generations = new QRadioButton(tr("the family in generations"), layouts);
    //: In Polish: swobodny
    m_free = new QRadioButton(tr("free"), layouts);
    m_generations->setChecked(true);
    auto* group = new QButtonGroup(layouts);
    group->addButton(m_generations);
    group->addButton(m_free);
    layoutRow->addWidget(m_generations);
    layoutRow->addWidget(m_free);
    layoutRow->addStretch(1);
    //: In Polish: Układ
    addField(tr("Layout"), layouts);

    //: In Polish: Mapa bierze relacje, które karty mają teraz; nowe relacje dochodzą do niej
    //: przy otwarciu.
    auto* note = new QLabel(
        tr("The map takes the relations the cards have now; new ones come to it when it opens."),
        this);
    note->setWordWrap(true);
    note->setForegroundRole(QPalette::PlaceholderText);
    contentLayout()->addWidget(note);

    //: In Polish: Utwórz
    setAcceptText(tr("Create"));
    connect(m_name, &QLineEdit::textChanged, this, &RelationMapDialog::updateAccept);
    updateAccept();
    m_name->selectAll();
    m_name->setFocus();
}

QString RelationMapDialog::title() const {
    return m_name->text().simplified();
}

RelationMapOptions RelationMapDialog::options() const {
    RelationMapOptions options;
    for (qsizetype i = 0; i < m_kinds.size(); ++i) {
        if (m_kinds.at(i)->isChecked()) {
            options.kinds.append(m_kindIds.at(i));
        }
    }
    options.family = m_family->isChecked();
    options.others = m_others->isChecked();
    options.across = m_across->isChecked();
    options.generations = m_generations->isChecked();
    return options;
}

void RelationMapDialog::updateAccept() {
    const RelationMapOptions chosen = options();
    acceptButton()->setEnabled(!title().isEmpty() && !chosen.kinds.isEmpty() &&
                               (chosen.family || chosen.others || chosen.across));
}

void askNewRelationMap(const core::WindowEditorContext& context, const QStringList& kinds,
                       QWidget* parent) {
    RelationMapDialog dialog(context.book, kinds, parent);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QString id = createRelationMap(context.book, dialog.title(), dialog.options());
    if (!id.isEmpty() && context.openElement) {
        context.openElement(id);
    }
}

// =============================================================================
// The offer after the first relation of a card
// =============================================================================

RelationMapOffer::RelationMapOffer(const QString& message, QWidget* parent) : QFrame(parent) {
    setFrameShape(QFrame::StyledPanel);
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::AlternateBase);
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(8, 6, 8, 6);
    auto* text = new QLabel(message, this);
    text->setWordWrap(true);
    column->addWidget(text);
    auto* buttons = new QHBoxLayout;
    //: The button of the offer of a map of relations. In Polish: Utwórz mapę relacji
    m_create = new QPushButton(tr("Create Relation Map"), this);
    //: In Polish: Nie teraz
    m_later = new QPushButton(tr("Not Now"), this);
    //: In Polish: Nie pytaj więcej
    m_never = new QPushButton(tr("Do Not Ask Again"), this);
    buttons->addWidget(m_create);
    buttons->addWidget(m_later);
    buttons->addWidget(m_never);
    buttons->addStretch(1);
    column->addLayout(buttons);
    //: The accessible name of the offer of a map of relations. In Polish: Mapa relacji
    setAccessibleName(tr("Relation map"));
    connect(m_create, &QPushButton::clicked, this, [this]() {
        emit createRequested();
        deleteLater();
    });
    connect(m_later, &QPushButton::clicked, this, &QObject::deleteLater);
    connect(m_never, &QPushButton::clicked, this, [this]() {
        core::SettingsManager::getInstance().set<bool>(MindMapSettings::ASK_RELATION_MAP, false);
        deleteLater();
    });
    QTimer::singleShot(OFFER_TIME, this, &QObject::deleteLater);
}

RelationMapOffer* offerRelationMap(const core::WindowEditorContext& context, const QString& cardId,
                                   const QString& otherId, const QString& relationId,
                                   QWidget* parent) {
    core::ProjectManager& book = context.book;
    if (!core::SettingsManager::getInstance().get<bool>(MindMapSettings::ASK_RELATION_MAP, true)) {
        return nullptr;
    }
    const core::ProjectElement* card = book.findElement(cardId);
    const core::ProjectElement* other = book.findElement(otherId);
    if (card == nullptr || other == nullptr || !mindMapKind(book)) {
        return nullptr;
    }
    const core::CardRelations relations = book.cardRelations();
    if (relations.of(cardId).size() != 1) {
        return nullptr;  // Not its first relation
    }
    const QString kind = card->kind.toString();
    if (hasRelationMapOf(book, kind)) {
        return nullptr;
    }
    const QString name = relations.labelOf(relationId, kind, *other, gui::programLanguage());
    //: The offer after the first relation of a card: %1 the card, %2 the relation, %3 the
    //: other card, {kind:plural} its kind ("characters"). In Polish: %1 ma pierwszą relację:
    //: „%2” – %3. Utworzyć mapę relacji {kind:genitivePlural}? Relacje kart zobaczysz na jednej
    //: mapie.
    const QString message = core::fillWords(RelationMapOffer::tr(
                                                "%1 has the first relation: “%2” – %3. Create a "
                                                "relation map of the {kind:plural}? You will see "
                                                "the relations of the cards on one map."),
                                            {{QStringLiteral("kind"), gui::wordsOf(book, *card)}})
                                .arg(card->title, name, other->title);
    auto* offer = new RelationMapOffer(message, parent);
    const core::WindowEditorContext kept = context;
    QObject::connect(offer, &RelationMapOffer::createRequested, parent,
                     [kept, kind, parent]() { askNewRelationMap(kept, {kind}, parent); });
    return offer;
}

}  // namespace kalahari::mindmap
