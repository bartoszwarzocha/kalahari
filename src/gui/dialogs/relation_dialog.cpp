// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file relation_dialog.cpp
/// @brief Relation: a relation of a card of a character, a location or an item with another
/// card

#include "kalahari/gui/dialogs/relation_dialog.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/card_relations.h"
#include "kalahari/core/kind_words.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/dialogs/dialog_parts.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/card_parts.h"
#include "kalahari/gui/panels/navigator_panel.h"

#include <QCollator>
#include <QComboBox>
#include <QEvent>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPalette>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <optional>

namespace kalahari::gui::dialogs {

namespace {

/// The card of an item of the list
constexpr int CARD_ROLE = Qt::UserRole;
/// The kind of the card that an item of the list creates
constexpr int CREATE_ROLE = Qt::UserRole + 1;
/// "Own Relation..." among the relations, whose id (Qt::UserRole) is empty
constexpr int OWN_ROLE = Qt::UserRole + 1;
/// Rows the list of the cards shows
constexpr int LIST_ROWS = 8;
/// The room a card takes in from its group in the list
constexpr int LIST_INDENT = 12;

/// The kinds of the cards of the Workshop of @p book, in the order of its type
QList<core::KindRef> cardKinds(const core::ProjectManager& book) {
    QList<core::KindRef> kinds;
    if (const core::BookProject* project = book.project()) {
        for (const core::KindRef& kind :
             project->kindsIn(book.bookTypes(), core::BookPlace::Workshop)) {
            if (kind && kind.kind->isCard()) {
                kinds << kind;
            }
        }
    }
    return kinds;
}

/// Whether @p item is a card, or a card to create
bool isChoice(const QTreeWidgetItem* item) {
    return item != nullptr &&
           (item->data(0, CARD_ROLE).isValid() || item->data(0, CREATE_ROLE).isValid());
}

/// Whether @p item and the group it is in are shown
bool isShown(const QTreeWidgetItem* item) {
    return !item->isHidden() && (item->parent() == nullptr || !item->parent()->isHidden());
}

/// How card @p card shows a relation in the window: "On the card "Anna": brother – Jan", in
/// rich text
QString shownOn(const QString& card, const QString& name, const QString& other) {
    //: How a card shows a relation, in the window Relation: %1 the card, %2 the name of the
    //: relation, %3 the other card. Keep <b> and </b>. In Polish: Na karcie „%1”: <b>%2</b> –
    //: %3
    return RelationDialog::tr("On the card \"%1\": <b>%2</b> – %3")
        .arg(card.toHtmlEscaped(),
             name.isEmpty() ? QStringLiteral("…") : name.toHtmlEscaped(),
             other.toHtmlEscaped());
}

/// The new card of @p choice in the Workshop of @p book, of the kind and with the title it
/// says; empty when it was not added
QString addCard(core::ProjectManager& book, const RelationChoice& choice) {
    const std::optional<core::KindReference> reference = core::KindReference::parse(choice.newKind);
    const core::KindRef kind =
        reference ? book.bookTypes().findKind(reference->packageId, reference->kindId)
                  : core::KindRef();
    return kind ? book.addElement(kind, choice.newTitle, core::BookPlace::Workshop) : QString();
}

}  // namespace

RelationDialog::RelationDialog(const core::ProjectManager& book, const QString& cardId,
                               const std::optional<core::ShownRelation>& edited, QWidget* parent)
    : KalahariDialog(parent),
      m_book(book),
      m_relations(book.cardRelations()),
      m_card(book.findElement(cardId)),
      m_filter(new QLineEdit(this)),
      m_cards(new RowList(this)),
      m_relation(new QComboBox(this)),
      m_ownNames(new QWidget(this)),
      m_ownForm(new QFormLayout(m_ownNames)),
      m_name(new QLineEdit(m_ownNames)),
      m_inverse(new QLineEdit(m_ownNames)),
      m_preview(new QLabel(this)),
      m_note(new QLineEdit(this)) {
    setObjectName(QStringLiteral("relationDialog"));
    m_kind = m_card != nullptr ? m_relations.kindOf(*m_card) : QString();
    const QString title = m_card != nullptr ? m_card->title : QString();
    //: The window of a relation of a card with another card; %1: the name of the card. In
    //: Polish: Relacja karty „%1”
    setHeading(tr("Relation of \"%1\"").arg(title),
               //: In Polish: Wybierz kartę i rodzaj relacji. Relacja pokaże się na obu kartach.
               tr("Choose the card and the kind of the relation. Both cards show it."));
    setHeadingIcon(QStringLiteral("common.link"));
    setCompactHeading(true);
    //: The button of the window Relation. In Polish: &Dodaj, &Zmień
    setAcceptText(edited ? tr("&Change") : tr("&Add"));

    m_filter->setObjectName(QStringLiteral("relationFilter"));
    //: In Polish: Pisz, żeby filtrować...
    m_filter->setPlaceholderText(tr("Type to filter..."));
    m_filter->setClearButtonEnabled(true);
    //: In Polish: Filtr kart
    m_filter->setAccessibleName(tr("Filter of the cards"));
    m_cards->setObjectName(QStringLiteral("relationCards"));
    m_cards->setColumnCount(2);
    m_cards->setHeaderHidden(true);
    m_cards->setRootIsDecorated(false);
    m_cards->setItemsExpandable(false);
    m_cards->setIndentation(LIST_INDENT);
    m_cards->setUniformRowHeights(true);
    m_cards->header()->setStretchLastSection(false);
    m_cards->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_cards->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_cards->setRows(LIST_ROWS);
    //: In Polish: Karty
    m_cards->setAccessibleName(tr("Cards"));
    auto* cards = new QVBoxLayout();
    cards->setSpacing(4);
    cards->addWidget(m_filter);
    cards->addWidget(m_cards, 1);

    m_relation->setObjectName(QStringLiteral("relationKind"));
    //: In Polish: Relacja
    m_relation->setAccessibleName(tr("Relation"));
    m_ownForm->setContentsMargins(0, 0, 0, 0);
    m_name->setObjectName(QStringLiteral("relationName"));
    //: What the other card is to the card, in a relation the writer names. In Polish: np.
    //: wierzyciel
    m_name->setPlaceholderText(tr("e.g. creditor"));
    m_inverse->setObjectName(QStringLiteral("relationInverse"));
    //: In Polish: np. dłużniczka
    m_inverse->setPlaceholderText(tr("e.g. debtor"));
    m_ownForm->addRow(new QLabel(m_ownNames), m_name);
    m_ownForm->addRow(new QLabel(m_ownNames), m_inverse);
    auto* relation = new QVBoxLayout();
    relation->setSpacing(4);
    relation->addWidget(m_relation);
    relation->addWidget(m_ownNames);

    m_preview->setObjectName(QStringLiteral("relationPreview"));
    m_preview->setTextFormat(Qt::RichText);
    m_preview->setWordWrap(true);
    m_note->setObjectName(QStringLiteral("relationNote"));
    //: In Polish: np. od dzieciństwa
    m_note->setPlaceholderText(tr("e.g. since childhood"));
    m_note->setClearButtonEnabled(true);

    auto* form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    //: In Polish: &Z kim albo z czym:
    auto* cardsLabel = new QLabel(tr("&With whom or what:"), this);
    cardsLabel->setBuddy(m_filter);
    form->addRow(cardsLabel, cards);
    //: In Polish: &Relacja:
    auto* relationLabel = new QLabel(tr("&Relation:"), this);
    relationLabel->setBuddy(m_relation);
    form->addRow(relationLabel, relation);
    //: In Polish: Na obu kartach:
    form->addRow(new QLabel(tr("On both cards:"), this), m_preview);
    //: In Polish: &Notatka:
    auto* noteLabel = new QLabel(tr("&Note:"), this);
    noteLabel->setBuddy(m_note);
    form->addRow(noteLabel, m_note);
    contentLayout()->addLayout(form, 1);
    //: The keys of the window Relation, beside its buttons. In Polish: Enter zapisuje, Esc
    //: zamyka
    auto* keys = new QLabel(tr("Enter saves, Esc closes"));
    keys->setForegroundRole(QPalette::PlaceholderText);
    addSideWidget(keys);

    fillCards();
    filter(QString());
    connect(m_filter, &QLineEdit::textChanged, this, &RelationDialog::filter);
    connect(m_cards, &QTreeWidget::currentItemChanged, this, &RelationDialog::fillRelations);
    connect(m_relation, &QComboBox::currentIndexChanged, this, [this]() {
        showOwnNames();
        showPreview();
        updateAccept();
    });
    for (const QLineEdit* name : {m_name, m_inverse}) {
        connect(name, &QLineEdit::textChanged, this, [this]() {
            showPreview();
            updateAccept();
        });
    }
    m_filter->installEventFilter(this);
    m_cards->installEventFilter(this);

    if (edited) {
        setCurrentCard(edited->otherId);
        setCurrentRelation(edited->relation);
        m_note->setText(edited->note);
    } else {
        m_cards->setCurrentItem(firstChoice());
    }
    fillRelations();
    m_filter->setFocus(Qt::OtherFocusReason);
}

void RelationDialog::fillCards() {
    QCollator collator{QLocale(programLanguage())};
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    for (const core::KindRef& kind : cardKinds(m_book)) {
        QList<const core::ProjectElement*> cards;
        for (const core::ProjectElement* each : m_relations.cards()) {
            if (each != m_card && m_relations.kindOf(*each) == kind.reference()) {
                cards << each;
            }
        }
        if (cards.isEmpty()) {
            continue;
        }
        std::stable_sort(cards.begin(), cards.end(),
                         [&collator](const core::ProjectElement* a, const core::ProjectElement* b) {
                             return collator.compare(a->title, b->title) < 0;
                         });
        auto* group = new QTreeWidgetItem(m_cards);
        group->setText(0, core::fillWords(QStringLiteral("{Kind:plural}"),
                                          {{QStringLiteral("kind"), wordsOf(kind)}}));
        group->setFlags(Qt::ItemIsEnabled);
        group->setFirstColumnSpanned(true);
        QFont bold = group->font(0);
        bold.setBold(true);
        group->setFont(0, bold);
        for (const core::ProjectElement* card : cards) {
            auto* item = new QTreeWidgetItem(group);
            item->setText(0, card->title);
            item->setIcon(0, NavigatorPanel::iconOf(m_book, *card, core::IconContext::TreeView));
            item->setText(1, card::subtitleValue(m_book, *card));
            item->setForeground(1, palette().brush(QPalette::PlaceholderText));
            item->setData(0, CARD_ROLE, card->id);
        }
        group->setExpanded(true);
    }
}

void RelationDialog::filter(const QString& text) {
    const QString wanted = text.trimmed();
    // The items that create a card go, and come back for the name typed
    for (int i = m_cards->topLevelItemCount() - 1; i >= 0; --i) {
        if (m_cards->topLevelItem(i)->data(0, CREATE_ROLE).isValid()) {
            delete m_cards->takeTopLevelItem(i);
        }
    }
    for (int i = 0; i < m_cards->topLevelItemCount(); ++i) {
        QTreeWidgetItem* group = m_cards->topLevelItem(i);
        bool any = false;
        for (int j = 0; j < group->childCount(); ++j) {
            QTreeWidgetItem* item = group->child(j);
            const bool shown =
                wanted.isEmpty() || item->text(0).contains(wanted, Qt::CaseInsensitive);
            item->setHidden(!shown);
            any = any || shown;
        }
        group->setHidden(!any);
    }
    addCreateItems(wanted);
    const QTreeWidgetItem* current = m_cards->currentItem();
    if (!isChoice(current) || !isShown(current)) {
        m_cards->setCurrentItem(firstChoice());
    }
}

void RelationDialog::addCreateItems(const QString& title) {
    if (title.isEmpty() || m_book.isReadOnly()) {
        return;
    }
    auto& art = core::ArtProvider::getInstance();
    for (const core::KindRef& kind : cardKinds(m_book)) {
        const QString reference = kind.reference();
        if (!core::ProjectManager::canAdd(kind) ||
            m_relations.cardTitled(title, reference) != nullptr) {
            continue;
        }
        auto* item = new QTreeWidgetItem(m_cards);
        //: The last items of the list of the window Relation: a new card of a kind with the
        //: name typed, %1. In Polish: Utwórz {kind:accusative} „%1”...
        item->setText(0, core::fillWords(tr("Create {kind} \"%1\"..."),
                                         {{QStringLiteral("kind"), wordsOf(kind)}})
                             .arg(title));
        item->setIcon(0, art.getIcon(QStringLiteral("common.add"), core::IconContext::TreeView));
        item->setData(0, CREATE_ROLE, reference);
        item->setFirstColumnSpanned(true);
    }
}

void RelationDialog::fillRelations() {
    const QString previous = m_relation->currentData().toString();
    const bool wasOwn = ownChosen();
    {
        const QSignalBlocker blocker(m_relation);
        m_relation->clear();
        const QString other = otherKind();
        if (!other.isEmpty()) {
            const QString language = programLanguage();
            for (const core::RelationInfo& each : m_relations.between(m_kind, other)) {
                //: The group of a relation the writer named, in the window Relation. In
                //: Polish: własna
                const QString group = each.isOwn() ? tr("own") : each.groupText(language);
                //: A relation in the window Relation with its group: "siblings (family)". In
                //: Polish: %1 (%2)
                m_relation->addItem(group.isEmpty()
                                        ? each.choiceText(language)
                                        : tr("%1 (%2)").arg(each.choiceText(language), group),
                                    each.id);
            }
            if (m_relation->count() > 0) {
                m_relation->insertSeparator(m_relation->count());
            }
            //: The last relation of the window Relation, which the writer names. In Polish:
            //: Własna relacja...
            m_relation->addItem(tr("Own Relation..."), QString());
            m_relation->setItemData(m_relation->count() - 1, true, OWN_ROLE);
        }
        int index = 0;
        if (wasOwn) {
            index = m_relation->count() - 1;
        } else if (!previous.isEmpty()) {
            index = std::max(m_relation->findData(previous), 0);
        }
        m_relation->setCurrentIndex(index);
        m_relation->setEnabled(!other.isEmpty());
    }
    showOwnNames();
    showPreview();
    updateAccept();
}

void RelationDialog::showOwnNames() {
    const bool own = ownChosen();
    m_ownNames->setVisible(own);
    if (!own || m_card == nullptr) {
        return;
    }
    //: The name of a relation the writer names, as card %1 shows it. In Polish: Na karcie
    //: „%1”:
    const QString onCard = tr("On the card \"%1\":");
    if (auto* label = qobject_cast<QLabel*>(m_ownForm->labelForField(m_name))) {
        label->setText(onCard.arg(m_card->title));
    }
    if (auto* label = qobject_cast<QLabel*>(m_ownForm->labelForField(m_inverse))) {
        label->setText(onCard.arg(otherTitle()));
    }
}

void RelationDialog::showPreview() {
    const QString other = otherTitle();
    if (m_card == nullptr || other.isEmpty() || m_relation->currentIndex() < 0) {
        m_preview->clear();
        return;
    }
    QString here;   // what the other card is to the card
    QString there;  // what the card is to the other card
    if (ownChosen()) {
        here = m_name->text().trimmed();
        there = m_inverse->text().trimmed();
    } else if (const std::optional<core::RelationInfo> relation =
                   m_relations.find(m_relation->currentData().toString(), m_kind)) {
        const QString language = programLanguage();
        const core::ProjectElement* card = otherCard();
        here = relation->nameFor(card != nullptr ? core::CardRelations::genderOf(*card)
                                                 : QString(),
                                 language);
        const std::optional<core::RelationInfo> inverse =
            m_relations.find(relation->inverse, otherKind());
        there = inverse ? inverse->nameFor(core::CardRelations::genderOf(*m_card), language)
                        : relation->inverse;
    }
    m_preview->setText(shownOn(m_card->title, here, other) + QStringLiteral("<br>") +
                       shownOn(other, there, m_card->title));
}

void RelationDialog::updateAccept() {
    const bool chosen = !otherTitle().isEmpty() && m_relation->currentIndex() >= 0;
    const bool named = !ownChosen() || (!m_name->text().trimmed().isEmpty() &&
                                        !m_inverse->text().trimmed().isEmpty());
    acceptButton()->setEnabled(!m_book.isReadOnly() && m_card != nullptr && chosen && named);
}

QString RelationDialog::otherKind() const {
    const QTreeWidgetItem* item = m_cards->currentItem();
    if (item == nullptr) {
        return {};
    }
    if (const core::ProjectElement* card = otherCard()) {
        return m_relations.kindOf(*card);
    }
    return item->data(0, CREATE_ROLE).toString();
}

QString RelationDialog::otherTitle() const {
    const QTreeWidgetItem* item = m_cards->currentItem();
    if (const core::ProjectElement* card = otherCard()) {
        return card->title;
    }
    return item != nullptr && item->data(0, CREATE_ROLE).isValid() ? m_filter->text().trimmed()
                                                                    : QString();
}

const core::ProjectElement* RelationDialog::otherCard() const {
    const QTreeWidgetItem* item = m_cards->currentItem();
    return item != nullptr && item->data(0, CARD_ROLE).isValid()
               ? m_book.findElement(item->data(0, CARD_ROLE).toString())
               : nullptr;
}

bool RelationDialog::ownChosen() const {
    return m_relation->currentData(OWN_ROLE).toBool();
}

QTreeWidgetItem* RelationDialog::firstChoice() const {
    for (int i = 0; i < m_cards->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_cards->topLevelItem(i);
        if (isChoice(item) && isShown(item)) {
            return item;
        }
        for (int j = 0; j < item->childCount(); ++j) {
            if (isShown(item->child(j))) {
                return item->child(j);
            }
        }
    }
    return nullptr;
}

RelationChoice RelationDialog::choice() const {
    RelationChoice choice;
    if (const QTreeWidgetItem* item = m_cards->currentItem()) {
        choice.otherId = item->data(0, CARD_ROLE).toString();
        choice.newKind = item->data(0, CREATE_ROLE).toString();
        if (!choice.newKind.isEmpty()) {
            choice.newTitle = m_filter->text().trimmed();
        }
    }
    if (ownChosen()) {
        choice.name = m_name->text().trimmed();
        choice.inverseName = m_inverse->text().trimmed();
    } else {
        choice.relationId = m_relation->currentData().toString();
    }
    choice.note = m_note->text().trimmed();
    return choice;
}

void RelationDialog::setCurrentCard(const QString& cardId) {
    for (int i = 0; i < m_cards->topLevelItemCount(); ++i) {
        QTreeWidgetItem* group = m_cards->topLevelItem(i);
        for (int j = 0; j < group->childCount(); ++j) {
            if (group->child(j)->data(0, CARD_ROLE).toString() == cardId) {
                m_cards->setCurrentItem(group->child(j));
                m_cards->scrollToItem(group->child(j));
                return;
            }
        }
    }
}

void RelationDialog::setCurrentRelation(const QString& relationId) {
    const int index = relationId.isEmpty() ? -1 : m_relation->findData(relationId);
    if (index >= 0) {
        m_relation->setCurrentIndex(index);
    }
}

bool RelationDialog::eventFilter(QObject* watched, QEvent* event) {
    if (takeFilterKeys(watched, event, m_filter, m_cards)) {
        return true;
    }
    return KalahariDialog::eventFilter(watched, event);
}

QString editRelation(core::ProjectManager& book, const QString& cardId,
                     const std::optional<core::ShownRelation>& edited, QWidget* parent) {
    RelationDialog dialog(book, cardId, edited, parent);
    if (dialog.exec() != QDialog::Accepted) {
        return {};
    }
    return saveRelation(book, cardId, dialog.choice(), edited, parent);
}

QString saveRelation(core::ProjectManager& book, const QString& cardId,
                     const RelationChoice& choice,
                     const std::optional<core::ShownRelation>& edited, QWidget* parent) {
    QString otherId = choice.otherId;
    if (otherId.isEmpty()) {
        otherId = addCard(book, choice);
        if (otherId.isEmpty()) {
            //: In Polish: Nie dodano karty
            MessageDialog::warning(parent, RelationDialog::tr("Card Not Added"),
                                   RelationDialog::tr("Failed to save changes."));
            return {};
        }
    }
    QString relationId = choice.relationId;
    const core::ProjectElement* card = book.findElement(cardId);
    const core::ProjectElement* other = book.findElement(otherId);
    if (relationId.isEmpty() && card != nullptr && other != nullptr) {
        const core::CardRelations relations = book.cardRelations();
        relationId = book.ownRelation(relations.kindOf(*card), relations.kindOf(*other),
                                      choice.name, choice.inverseName);
    }
    const bool saved =
        !relationId.isEmpty() &&
        (edited ? book.replaceRelation(cardId, *edited, otherId, relationId, choice.note)
                : book.addRelation(cardId, otherId, relationId, choice.note));
    if (!saved) {
        //: In Polish: Nie zapisano relacji
        MessageDialog::warning(parent, RelationDialog::tr("Relation Not Saved"),
                               RelationDialog::tr("Failed to save changes."));
        return {};
    }
    return otherId;
}

}  // namespace kalahari::gui::dialogs
