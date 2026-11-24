// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file relations_box.cpp
/// @brief The relations of a card of a character, a location or an item with the other cards

#include "kalahari/gui/widgets/relations_box.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/card_relations.h"
#include "kalahari/core/kind_words.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dialogs/relation_dialog.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/panels/card_parts.h"
#include "kalahari/gui/widgets/palette_edit.h"

#include <QAction>
#include <QColor>
#include <QEvent>
#include <QHeaderView>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPalette>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <functional>
#include <optional>

namespace kalahari::gui {

namespace {

/// The relation of a row: its place among those the card shows
constexpr int INDEX_ROLE = Qt::UserRole;
/// Side of the color of the other card
constexpr int SWATCH_SIDE = 12;
/// The room a row takes in from its group
constexpr int GROUP_INDENT = 12;
/// The columns: the relation, the other card, and its note on the full card
constexpr int NAME_COLUMN = 0;
constexpr int CARD_COLUMN = 1;
constexpr int NOTE_COLUMN = 2;

/// The items of @p list in its order, each group before its rows
QList<QTreeWidgetItem*> itemsOf(const QTreeWidget* list) {
    QList<QTreeWidgetItem*> items;
    for (int i = 0; i < list->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = list->topLevelItem(i);
        items << item;
        for (int j = 0; j < item->childCount(); ++j) {
            items << item->child(j);
        }
    }
    return items;
}

/// The ids of the cards of @p book
QSet<QString> cardIds(const core::ProjectManager& book) {
    QSet<QString> ids;
    const core::CardRelations relations = book.cardRelations();
    for (const core::ProjectElement* card : relations.cards()) {
        ids.insert(card->id);
    }
    return ids;
}

}  // namespace

RelationsBox::RelationsBox(core::ProjectManager& book, bool full, QWidget* parent)
    : QGroupBox(parent),
      m_full(full),
      m_list(new QTreeWidget(this)),
      m_menu(new QMenu(this)),
      m_book(book) {
    setObjectName(QStringLiteral("cardRelations"));
    //: The group of the relations of a card with the other cards. In Polish: Relacje
    setTitle(tr("Relations"));
    QVBoxLayout* layout = card::groupLayout(this);
    //: In Polish: Nowa relacja... (Insert)
    m_addButton = card::listButton(this, "common.add", tr("New Relation... (Insert)"),
                                   QStringLiteral("relationsAdd"));
    //: In Polish: Usuń relację (Delete)
    m_removeButton = card::listButton(this, "common.remove", tr("Delete Relation (Delete)"),
                                      QStringLiteral("relationsRemove"));
    layout->addLayout(card::buttonRow(m_addButton, m_removeButton));

    m_list->setObjectName(QStringLiteral("relationsList"));
    m_list->setColumnCount(full ? NOTE_COLUMN + 1 : CARD_COLUMN + 1);
    m_list->setHeaderHidden(true);
    m_list->setRootIsDecorated(false);
    m_list->setItemsExpandable(false);
    m_list->setIndentation(full ? GROUP_INDENT : 0);
    m_list->setUniformRowHeights(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setIconSize(QSize(SWATCH_SIDE, SWATCH_SIDE));
    m_list->header()->setStretchLastSection(true);
    m_list->header()->setSectionResizeMode(NAME_COLUMN, QHeaderView::ResizeToContents);
    m_list->header()->setSectionResizeMode(CARD_COLUMN, full ? QHeaderView::ResizeToContents
                                                             : QHeaderView::Stretch);
    //: In Polish: Relacje karty
    m_list->setAccessibleName(tr("Relations of the card"));
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->installEventFilter(this);
    layout->addWidget(m_list);
    //: In Polish: Brak relacji. „+” albo Insert dodaje relację.
    m_emptyLabel = card::hintLabel(this, tr("No relations. \"+\" or Insert adds one."),
                                   QStringLiteral("relationsEmpty"));
    layout->addWidget(m_emptyLabel);
    m_menu->setObjectName(QStringLiteral("relationsMenu"));

    connect(m_addButton, &QToolButton::clicked, this, &RelationsBox::addRelation);
    connect(m_removeButton, &QToolButton::clicked, this, &RelationsBox::removeRelation);
    connect(m_list, &QTreeWidget::currentItemChanged, this, &RelationsBox::updateButtons);
    connect(m_list, &QTreeWidget::itemDoubleClicked, this, [this]() {
        if (const std::optional<core::ShownRelation> relation = currentRelation()) {
            emit requestShowCard(relation->otherId);
        }
    });
    connect(m_list, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint& at) {
        fillMenu();
        m_menu->popup(m_list->viewport()->mapToGlobal(at));
    });
    // The relations, the names, the genders and the colors of the cards change elsewhere too
    const auto changed = [this]() {
        if (!m_cardId.isEmpty()) {
            refresh();
        }
    };
    connect(&m_book, &core::ProjectManager::linksChanged, this, changed);
    connect(&m_book, &core::ProjectManager::cardChanged, this, changed);
    connect(&m_book, &core::ProjectManager::elementRenamed, this, changed);
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged, this, changed);
    fitList();
    updateButtons();
}

RelationsBox::~RelationsBox() {
    card::cutOffChildren(this);
}

void RelationsBox::showCard(const QString& cardId) {
    if (cardId != m_cardId) {
        m_cardId = cardId;
        m_list->clear();
    }
    refresh();
}

void RelationsBox::refresh() {
    const std::optional<core::ShownRelation> current = currentRelation();
    m_list->clear();
    const core::CardRelations relations = m_book.cardRelations();
    m_shown = m_cardId.isEmpty() ? QList<core::ShownRelation>() : relations.of(m_cardId);
    if (m_full) {
        addGroups(relations);
    } else {
        for (int i = 0; i < m_shown.size(); ++i) {
            addRow(nullptr, i, relations);
        }
    }
    m_list->expandAll();
    if (current) {
        selectRow([&current](const core::ShownRelation& each) { return each == *current; });
    }
    fitList();
    updateButtons();
}

QStringList RelationsBox::groupKinds(const core::CardRelations& relations) const {
    QStringList kinds;
    if (const core::BookProject* project = m_book.project()) {
        for (const core::KindRef& kind :
             project->kindsIn(m_book.bookTypes(), core::BookPlace::Workshop)) {
            kinds << kind.reference();
        }
    }
    for (const core::ShownRelation& relation : m_shown) {
        const core::ProjectElement* other = m_book.findElement(relation.otherId);
        if (other != nullptr && !kinds.contains(relations.kindOf(*other))) {
            kinds << relations.kindOf(*other);
        }
    }
    return kinds;
}

void RelationsBox::addGroups(const core::CardRelations& relations) {
    // The groups in the order of the kinds of the Workshop
    for (const QString& kind : groupKinds(relations)) {
        QTreeWidgetItem* group = nullptr;
        for (int i = 0; i < m_shown.size(); ++i) {
            const core::ProjectElement* other = m_book.findElement(m_shown.at(i).otherId);
            if (other == nullptr || relations.kindOf(*other) != kind) {
                continue;
            }
            if (group == nullptr) {
                group = new QTreeWidgetItem(m_list);
                group->setText(NAME_COLUMN,
                               core::fillWords(QStringLiteral("{Kind:plural}"),
                                               {{QStringLiteral("kind"), wordsOf(m_book, *other)}}));
                group->setFlags(Qt::ItemIsEnabled);
                group->setFirstColumnSpanned(true);
                QFont bold = group->font(NAME_COLUMN);
                bold.setBold(true);
                group->setFont(NAME_COLUMN, bold);
            }
            addRow(group, i, relations);
        }
    }
}

void RelationsBox::addRow(QTreeWidgetItem* parent, int index,
                          const core::CardRelations& relations) {
    const core::ShownRelation& relation = m_shown.at(index);
    const core::ProjectElement* card = m_book.findElement(m_cardId);
    const core::ProjectElement* other = m_book.findElement(relation.otherId);
    if (card == nullptr || other == nullptr) {
        return;
    }
    auto* item = parent != nullptr ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(m_list);
    item->setText(NAME_COLUMN, relations.labelOf(relation.relation, relations.kindOf(*card),
                                                 *other, programLanguage()));
    item->setText(CARD_COLUMN, other->title);
    item->setIcon(CARD_COLUMN,
                  QIcon(PaletteEdit::swatchOf(QColor(other->card.color), SWATCH_SIDE,
                                              devicePixelRatioF(),
                                              palette().color(QPalette::Mid))));
    if (m_full) {
        item->setText(NOTE_COLUMN, relation.note);
        item->setForeground(NOTE_COLUMN, palette().brush(QPalette::PlaceholderText));
    }
    for (int column = 0; column < m_list->columnCount(); ++column) {
        item->setToolTip(column, relation.note);
    }
    item->setData(NAME_COLUMN, INDEX_ROLE, index);
}

std::optional<core::ShownRelation> RelationsBox::currentRelation() const {
    const QTreeWidgetItem* item = m_list->currentItem();
    const QVariant index = item != nullptr ? item->data(NAME_COLUMN, INDEX_ROLE) : QVariant();
    if (!index.isValid() || index.toInt() < 0 || index.toInt() >= m_shown.size()) {
        return std::nullopt;
    }
    return m_shown.at(index.toInt());
}

void RelationsBox::selectRow(const std::function<bool(const core::ShownRelation&)>& matches) {
    for (QTreeWidgetItem* item : itemsOf(m_list)) {
        const QVariant index = item->data(NAME_COLUMN, INDEX_ROLE);
        if (index.isValid() && index.toInt() >= 0 && index.toInt() < m_shown.size() &&
            matches(m_shown.at(index.toInt()))) {
            m_list->setCurrentItem(item);
            return;
        }
    }
}

void RelationsBox::addRelation() {
    openRelation(std::nullopt);
}

void RelationsBox::changeRelation() {
    if (const std::optional<core::ShownRelation> relation = currentRelation()) {
        openRelation(relation);
    }
}

void RelationsBox::openRelation(const std::optional<core::ShownRelation>& edited) {
    if (m_cardId.isEmpty() || m_book.isReadOnly()) {
        return;
    }
    const QSet<QString> before = cardIds(m_book);
    const QList<core::ShownRelation> had = m_book.cardRelations().of(m_cardId);
    const QString cardId = m_cardId;
    const QString otherId = dialogs::editRelation(m_book, m_cardId, edited, window());
    if (otherId.isEmpty()) {
        return;
    }
    if (!before.contains(otherId)) {
        emit cardAdded(otherId);
    }
    refresh();
    selectRow([&otherId](const core::ShownRelation& each) { return each.otherId == otherId; });
    m_list->setFocus(Qt::OtherFocusReason);
    if (!edited) {
        // The relation the card did not have before
        for (const core::ShownRelation& relation : m_book.cardRelations().of(cardId)) {
            if (relation.otherId == otherId && !had.contains(relation)) {
                emit relationAdded(this, cardId, otherId, relation.relation);
                break;
            }
        }
    }
}

void RelationsBox::showOffer(QWidget* offer) {
    if (offer == nullptr) {
        return;
    }
    offer->setParent(this);
    layout()->addWidget(offer);
    offer->show();
}

void RelationsBox::removeRelation() {
    const std::optional<core::ShownRelation> relation = currentRelation();
    const QTreeWidgetItem* item = m_list->currentItem();
    if (!relation || item == nullptr || m_book.isReadOnly()) {
        return;
    }
    if (!relation->note.isEmpty()) {
        //: %1: the relation, %2: the other card. In Polish: Usunąć relację „%1 – %2” razem z
        //: jej notatką?
        const QString question = tr("Delete the relation \"%1 – %2\" with its note?")
                                     .arg(item->text(NAME_COLUMN), item->text(CARD_COLUMN));
        //: In Polish: Usuń relację
        if (!dialogs::MessageDialog::confirm(this, tr("Delete Relation"), question,
                                             //: In Polish: &Usuń
                                             tr("&Delete"))) {
            return;
        }
    }
    if (!m_book.removeRelation(*relation)) {
        //: In Polish: Nie zapisano relacji
        dialogs::MessageDialog::warning(this, tr("Relation Not Saved"),
                                        tr("Failed to save changes."));
    }
    refresh();
    m_list->setFocus(Qt::OtherFocusReason);
}

void RelationsBox::fillMenu() {
    m_menu->clear();
    const bool row = currentRelation().has_value();
    const bool writable = !m_book.isReadOnly() && !m_cardId.isEmpty();
    //: The menu of a relation of a card. In Polish: Otwórz kartę, Pokaż kartę
    QAction* show = m_menu->addAction(m_full ? tr("Open the Card") : tr("Show the Card"));
    show->setEnabled(row);
    connect(show, &QAction::triggered, this, [this]() {
        if (const std::optional<core::ShownRelation> relation = currentRelation()) {
            emit requestShowCard(relation->otherId);
        }
    });
    //: In Polish: Zmień relację... (F2)
    QAction* change = m_menu->addAction(tr("Change the Relation... (F2)"));
    change->setEnabled(row && writable);
    connect(change, &QAction::triggered, this, &RelationsBox::changeRelation);
    //: In Polish: Usuń relację (Delete)
    QAction* remove = m_menu->addAction(tr("Delete Relation (Delete)"));
    remove->setEnabled(row && writable);
    connect(remove, &QAction::triggered, this, &RelationsBox::removeRelation);
    m_menu->addSeparator();
    QAction* add = m_menu->addAction(tr("New Relation... (Insert)"));
    add->setEnabled(writable);
    connect(add, &QAction::triggered, this, &RelationsBox::addRelation);
}

void RelationsBox::fitList() {
    const int rows = static_cast<int>(itemsOf(m_list).size());
    const int rowHeight =
        rows > 0 ? m_list->sizeHintForRow(0) : m_list->fontMetrics().height() + 6;
    m_list->setFixedHeight(card::listHeight(m_list, rows, rowHeight, 0));
    m_list->setVisible(rows > 0);
    m_emptyLabel->setVisible(rows == 0);
}

void RelationsBox::updateButtons() {
    const bool writable = !m_book.isReadOnly() && !m_cardId.isEmpty();
    m_addButton->setEnabled(writable);
    m_removeButton->setEnabled(writable && currentRelation().has_value());
}

std::function<void()> RelationsBox::keyAction(const QKeyEvent* key) {
    if (card::isListKey(key, Qt::Key_Insert)) {
        return [this]() { addRelation(); };
    }
    if (card::isListKey(key, Qt::Key_Delete)) {
        return [this]() { removeRelation(); };
    }
    if (card::isListKey(key, Qt::Key_F2)) {
        return [this]() { changeRelation(); };
    }
    const std::optional<core::ShownRelation> relation = currentRelation();
    if (card::isOpenKey(key) && relation.has_value()) {
        return [this, otherId = relation->otherId]() { emit requestShowCard(otherId); };
    }
    return {};
}

bool RelationsBox::eventFilter(QObject* watched, QEvent* event) {
    if (watched != m_list || !card::isKeyEvent(event)) {
        return QGroupBox::eventFilter(watched, event);
    }
    const std::function<void()> action = keyAction(static_cast<const QKeyEvent*>(event));
    if (!action) {
        return QGroupBox::eventFilter(watched, event);
    }
    // The same keys of the window's commands wait while the list has the keys
    event->accept();
    if (event->type() == QEvent::KeyPress) {
        action();
    }
    return true;
}

}  // namespace kalahari::gui
