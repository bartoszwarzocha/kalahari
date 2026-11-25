// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file paragraph_cards_box.cpp
/// @brief The cards of characters, locations and items linked with the paragraph at the cursor
/// of a chapter, with their roles in it

#include "kalahari/gui/panels/paragraph_cards_box.h"

#include "kalahari/core/book_project.h"
#include "kalahari/core/link_roles.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/link_names.h"
#include "kalahari/gui/panels/card_parts.h"
#include "kalahari/gui/widgets/palette_edit.h"

#include <QColor>
#include <QEvent>
#include <QHeaderView>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QPalette>
#include <QTextBlock>
#include <QTextDocument>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <functional>
#include <utility>

namespace kalahari::gui {

namespace {

/// The card of a row
constexpr int CARD_ROLE = Qt::UserRole;
/// Side of the color of the card
constexpr int SWATCH_SIDE = 12;
/// The columns: the role of the card in the paragraph, and the card
constexpr int ROLE_COLUMN = 0;
constexpr int CARD_COLUMN = 1;

}  // namespace

QList<const core::ProjectElement*> cardsOfParagraph(const core::ProjectManager& book,
                                                    const QString& elementId,
                                                    const editor::BookEditor& text,
                                                    int paragraph) {
    QList<const core::ProjectElement*> cards;
    const QTextDocument* document = text.textDocument();
    if (elementId.isEmpty() || document == nullptr || paragraph < 0 ||
        !core::LinkRoles::paragraphsShown()) {
        return cards;
    }
    const QList<const core::ProjectElement*> linked = book.cardsLinkedTo(elementId);
    if (linked.isEmpty()) {
        return cards;
    }
    // As the marks beside the paragraphs find them
    QStringList paragraphs;
    for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
        paragraphs << block.text();
    }
    for (const core::ProjectElement* card : linked) {
        const core::ElementLink* link = card->linkTo(elementId);
        if (link != nullptr && core::LinkRoles::paragraphOf(paragraphs, *link) == paragraph + 1) {
            cards << card;
        }
    }
    return cards;
}

ParagraphCardsBox::ParagraphCardsBox(core::ProjectManager& book, QWidget* parent)
    : QGroupBox(parent), m_list(new QTreeWidget(this)), m_book(book) {
    setObjectName(QStringLiteral("paragraphCards"));
    //: The group of the Properties panel with the cards of characters, locations and items
    //: linked with the paragraph at the cursor. In Polish: Karty akapitu
    setTitle(tr("Cards of the Paragraph"));
    QVBoxLayout* layout = card::groupLayout(this);
    //: In Polish: Powiąż akapit z kartą... (Insert)
    m_addButton = card::listButton(this, "common.add", tr("Link Paragraph with Card... (Insert)"),
                                   QStringLiteral("paragraphCardsAdd"));
    //: The card stays linked with the chapter. In Polish: Odłącz kartę od akapitu (Delete)
    m_removeButton =
        card::listButton(this, "common.remove", tr("Take the Card off the Paragraph (Delete)"),
                         QStringLiteral("paragraphCardsRemove"));
    layout->addLayout(card::buttonRow(m_addButton, m_removeButton));

    m_list->setObjectName(QStringLiteral("paragraphCardsList"));
    m_list->setColumnCount(CARD_COLUMN + 1);
    m_list->setHeaderHidden(true);
    m_list->setRootIsDecorated(false);
    m_list->setIndentation(0);
    m_list->setUniformRowHeights(true);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setIconSize(QSize(SWATCH_SIDE, SWATCH_SIDE));
    m_list->header()->setStretchLastSection(true);
    m_list->header()->setSectionResizeMode(ROLE_COLUMN, QHeaderView::ResizeToContents);
    //: In Polish: Karty akapitu
    m_list->setAccessibleName(tr("Cards of the paragraph"));
    m_list->installEventFilter(this);
    layout->addWidget(m_list);
    //: In Polish: Ten akapit nie ma jeszcze kart.
    m_emptyLabel = card::hintLabel(this, tr("This paragraph has no cards yet."),
                                   QStringLiteral("paragraphCardsEmpty"));
    layout->addWidget(m_emptyLabel);

    connect(m_addButton, &QToolButton::clicked, this, &ParagraphCardsBox::requestLink);
    connect(m_removeButton, &QToolButton::clicked, this, &ParagraphCardsBox::unlinkCurrent);
    connect(m_list, &QTreeWidget::currentItemChanged, this, &ParagraphCardsBox::updateButtons);
    connect(m_list, &QTreeWidget::itemDoubleClicked, this, [this]() {
        if (const QString card = currentCard(); !card.isEmpty()) {
            emit requestOpenCard(card);
        }
    });
    // The roles, the names and the colors of the cards change elsewhere too
    connect(&m_book, &core::ProjectManager::linksChanged, this, &ParagraphCardsBox::refresh);
    connect(&m_book, &core::ProjectManager::cardChanged, this, &ParagraphCardsBox::refresh);
    connect(&m_book, &core::ProjectManager::elementRenamed, this, &ParagraphCardsBox::refresh);
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged, this,
            &ParagraphCardsBox::refresh);
    fitList();
    updateButtons();
}

ParagraphCardsBox::~ParagraphCardsBox() {
    card::cutOffChildren(this);
}

void ParagraphCardsBox::showParagraph(const QString& elementId, int paragraph,
                                      const QStringList& cardIds) {
    if (elementId == m_elementId && paragraph == m_paragraph && cardIds == m_cards) {
        return;
    }
    m_elementId = elementId;
    m_paragraph = paragraph;
    m_cards = cardIds;
    refresh();
}

void ParagraphCardsBox::refresh() {
    const QString current = currentCard();
    m_list->clear();
    for (const QString& id : std::as_const(m_cards)) {
        const core::ProjectElement* card = m_book.findElement(id);
        const core::ElementLink* link = card != nullptr ? card->linkTo(m_elementId) : nullptr;
        if (link == nullptr) {
            continue;  // Gone meanwhile: the panel shows the paragraph again at once
        }
        auto* item = new QTreeWidgetItem(m_list);
        item->setText(ROLE_COLUMN, cardRoleName(link->role));
        item->setForeground(ROLE_COLUMN, palette().brush(QPalette::PlaceholderText));
        item->setText(CARD_COLUMN, card->title);
        item->setIcon(CARD_COLUMN,
                      QIcon(PaletteEdit::swatchOf(QColor(card->card.color), SWATCH_SIDE,
                                                  devicePixelRatioF(),
                                                  palette().color(QPalette::Mid))));
        item->setData(CARD_COLUMN, CARD_ROLE, id);
        if (id == current) {
            m_list->setCurrentItem(item);
        }
    }
    fitList();
    updateButtons();
}

QString ParagraphCardsBox::currentCard() const {
    const QTreeWidgetItem* item = m_list->currentItem();
    return item != nullptr ? item->data(CARD_COLUMN, CARD_ROLE).toString() : QString();
}

void ParagraphCardsBox::unlinkCurrent() {
    const QString card = currentCard();
    if (card.isEmpty() || m_book.isReadOnly()) {
        return;
    }
    if (!m_book.unlinkParagraph(card, m_elementId)) {
        //: In Polish: Nie zapisano zmiany
        dialogs::MessageDialog::warning(this, tr("Change Not Saved"),
                                        tr("Failed to save changes."));
        return;
    }
    m_list->setFocus(Qt::OtherFocusReason);
}

void ParagraphCardsBox::fitList() {
    const int rows = m_list->topLevelItemCount();
    const int rowHeight =
        rows > 0 ? m_list->sizeHintForRow(0) : m_list->fontMetrics().height() + 6;
    m_list->setFixedHeight(card::listHeight(m_list, rows, rowHeight, 0));
    m_list->setVisible(rows > 0);
    m_emptyLabel->setVisible(rows == 0);
}

void ParagraphCardsBox::updateButtons() {
    const bool writable = !m_book.isReadOnly() && !m_elementId.isEmpty();
    m_addButton->setEnabled(writable);
    m_removeButton->setEnabled(writable && !currentCard().isEmpty());
}

std::function<void()> ParagraphCardsBox::keyAction(const QKeyEvent* key) {
    if (card::isListKey(key, Qt::Key_Insert)) {
        return [this]() {
            if (m_addButton->isEnabled()) {
                emit requestLink();
            }
        };
    }
    if (card::isListKey(key, Qt::Key_Delete)) {
        return [this]() { unlinkCurrent(); };
    }
    if (const QString card = currentCard(); card::isOpenKey(key) && !card.isEmpty()) {
        return [this, card]() { emit requestOpenCard(card); };
    }
    return {};
}

bool ParagraphCardsBox::eventFilter(QObject* watched, QEvent* event) {
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
