// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file paragraph_card_dialog.cpp
/// @brief Link Paragraph with Card: a card of a character, a location or an item linked with a
/// paragraph of a chapter, in a role

#include "kalahari/gui/dialogs/paragraph_card_dialog.h"

#include "kalahari/core/book_dictionary.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/card_names.h"
#include "kalahari/core/card_relations.h"
#include "kalahari/core/kind_words.h"
#include "kalahari/core/link_roles.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/dialogs/dialog_parts.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/link_names.h"
#include "kalahari/gui/panels/navigator_panel.h"

#include <QCollator>
#include <QComboBox>
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

namespace kalahari::gui::dialogs {

namespace {

/// The card of an item of the list
constexpr int CARD_ROLE = Qt::UserRole;
/// Rows the list of the cards shows
constexpr int LIST_ROWS = 8;

/// The first words of @p text as the heading shows them: the excerpt of a link, with "..."
/// when the paragraph goes on
QString excerptShown(const QString& text) {
    const QString excerpt = core::LinkRoles::excerptOf(text);
    return excerpt.size() < text.simplified().size() ? excerpt + QStringLiteral("…") : excerpt;
}

}  // namespace

ParagraphCardDialog::ParagraphCardDialog(const core::ProjectManager& book,
                                         const QString& elementId, int paragraph,
                                         const QString& paragraphText, const QString& cardId,
                                         QWidget* parent)
    : KalahariDialog(parent),
      m_book(book),
      m_elementId(elementId),
      m_paragraph(paragraph),
      m_filter(new QLineEdit(this)),
      m_cards(new RowList(this)),
      m_role(new QComboBox(this)),
      m_note(new QLabel(this)) {
    setObjectName(QStringLiteral("paragraphCardDialog"));
    const core::ProjectElement* chapter = book.findElement(elementId);
    //: The window that links a paragraph of a chapter with a card of a character, a location
    //: or an item. In Polish: Powiąż akapit z kartą
    setHeading(tr("Link Paragraph with Card"),
               //: The chapter, the number of the paragraph and its first words. In Polish:
               //: %1, akapit %2: „%3”
               tr("%1, paragraph %2: \"%3\"")
                   .arg(chapter != nullptr ? chapter->title : QString(),
                        QLocale().toString(paragraph), excerptShown(paragraphText)));
    setHeadingIcon(QStringLiteral("common.link"));
    setCompactHeading(true);
    //: The button of the window Link Paragraph with Card. In Polish: &Powiąż
    setAcceptText(tr("&Link"));

    m_filter->setObjectName(QStringLiteral("paragraphCardFilter"));
    //: In Polish: Pisz, żeby filtrować...
    m_filter->setPlaceholderText(tr("Type to filter..."));
    m_filter->setClearButtonEnabled(true);
    //: In Polish: Filtr kart
    m_filter->setAccessibleName(tr("Filter of the cards"));
    m_cards->setObjectName(QStringLiteral("paragraphCardList"));
    m_cards->setColumnCount(2);
    m_cards->setHeaderHidden(true);
    m_cards->setRootIsDecorated(false);
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

    m_role->setObjectName(QStringLiteral("paragraphCardRole"));
    //: In Polish: Rola w akapicie
    m_role->setAccessibleName(tr("Role in the paragraph"));
    m_note->setObjectName(QStringLiteral("paragraphCardNote"));
    m_note->setWordWrap(true);
    m_note->setTextFormat(Qt::PlainText);
    m_note->setForegroundRole(QPalette::PlaceholderText);

    auto* form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    //: In Polish: &Karta:
    auto* cardsLabel = new QLabel(tr("&Card:"), this);
    cardsLabel->setBuddy(m_filter);
    form->addRow(cardsLabel, cards);
    //: In Polish: &Rola w akapicie:
    auto* roleLabel = new QLabel(tr("&Role in the paragraph:"), this);
    roleLabel->setBuddy(m_role);
    form->addRow(roleLabel, m_role);
    form->addRow(QString(), m_note);
    contentLayout()->addLayout(form, 1);
    //: The keys of the window Link Paragraph with Card, beside its buttons. In Polish: Enter
    //: zapisuje, Esc zamyka
    auto* keys = new QLabel(tr("Enter saves, Esc closes"));
    keys->setForegroundRole(QPalette::PlaceholderText);
    addSideWidget(keys);

    fillCards();
    connect(m_filter, &QLineEdit::textChanged, this, &ParagraphCardDialog::filter);
    connect(m_cards, &QTreeWidget::currentItemChanged, this, &ParagraphCardDialog::fillRoles);
    m_filter->installEventFilter(this);
    m_cards->installEventFilter(this);
    setCurrentCard(cardId);
    if (m_cards->currentItem() == nullptr) {
        m_cards->setCurrentItem(firstShown());
    }
    fillRoles();
    m_filter->setFocus(Qt::OtherFocusReason);
}

void ParagraphCardDialog::fillCards() {
    QList<const core::ProjectElement*> all = m_book.cardRelations().cards();
    QCollator collator{QLocale(programLanguage())};
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::stable_sort(all.begin(), all.end(),
                     [&collator](const core::ProjectElement* a, const core::ProjectElement* b) {
                         return collator.compare(a->title, b->title) < 0;
                     });
    for (const core::ProjectElement* card : all) {
        auto* item = new QTreeWidgetItem(m_cards);
        item->setText(0, card->title);
        item->setIcon(0, NavigatorPanel::iconOf(m_book, *card, core::IconContext::TreeView));
        item->setText(1, core::fillWords(QStringLiteral("{kind}"),
                                         {{QStringLiteral("kind"), wordsOf(m_book, *card)}}));
        item->setForeground(1, palette().brush(QPalette::PlaceholderText));
        item->setData(0, CARD_ROLE, card->id);
    }
}

void ParagraphCardDialog::filter(const QString& text) {
    const QString wanted = text.trimmed();
    for (int i = 0; i < m_cards->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_cards->topLevelItem(i);
        item->setHidden(!matches(item, wanted));
    }
    const QTreeWidgetItem* current = m_cards->currentItem();
    if (current == nullptr || current->isHidden()) {
        m_cards->setCurrentItem(firstShown());
    }
    acceptButton()->setEnabled(currentCard() != nullptr && !m_book.isReadOnly());
}

bool ParagraphCardDialog::matches(const QTreeWidgetItem* item, const QString& wanted) const {
    if (wanted.isEmpty() || item->text(0).contains(wanted, Qt::CaseInsensitive)) {
        return true;
    }
    const core::DictionaryName* name =
        m_book.bookDictionary().findNameOfCard(item->data(0, CARD_ROLE).toString());
    return name != nullptr && core::nameHas(*name, wanted);
}

void ParagraphCardDialog::fillRoles() {
    const core::ProjectElement* card = currentCard();
    const core::KindRef kind = card != nullptr ? m_book.kindOf(*card) : core::KindRef{};
    const core::ElementLink* link = card != nullptr ? card->linkTo(m_elementId) : nullptr;
    const QSignalBlocker blocker(m_role);
    m_role->clear();
    if (kind) {
        for (const QString& role : core::LinkRoles::ofCard(*kind.kind)) {
            m_role->addItem(cardRoleName(role), role);
        }
        const QString current = link != nullptr && !link->role.isEmpty()
                                    ? link->role
                                    : core::LinkRoles::suggestedForCard(*kind.kind);
        m_role->setCurrentIndex(std::max(0, m_role->findData(current)));
    }
    m_role->setEnabled(m_role->count() > 0);
    showNote();
    acceptButton()->setEnabled(card != nullptr && !m_book.isReadOnly());
}

void ParagraphCardDialog::showNote() {
    const core::ProjectElement* card = currentCard();
    const core::ElementLink* link = card != nullptr ? card->linkTo(m_elementId) : nullptr;
    QString note;
    if (link != nullptr && link->paragraph == m_paragraph) {
        //: In Polish: Karta jest już powiązana z tym akapitem. Zmieni się jej rola.
        note = tr("The card is linked with this paragraph already. Its role changes.");
    } else if (link != nullptr && link->paragraph > 0) {
        //: A card links with one paragraph of a chapter. In Polish: Karta jest powiązana z
        //: akapitem %1 tego rozdziału. Powiązanie przejdzie na ten akapit.
        note = tr("The card is linked with paragraph %1 of this chapter. The link moves to "
                  "this paragraph.")
                   .arg(QLocale().toString(link->paragraph));
    }
    m_note->setText(note);
    m_note->setVisible(!note.isEmpty());
}

const core::ProjectElement* ParagraphCardDialog::currentCard() const {
    const QTreeWidgetItem* item = m_cards->currentItem();
    return item != nullptr && !item->isHidden()
               ? m_book.findElement(item->data(0, CARD_ROLE).toString())
               : nullptr;
}

QTreeWidgetItem* ParagraphCardDialog::firstShown() const {
    for (int i = 0; i < m_cards->topLevelItemCount(); ++i) {
        if (QTreeWidgetItem* item = m_cards->topLevelItem(i); !item->isHidden()) {
            return item;
        }
    }
    return nullptr;
}

ParagraphCardChoice ParagraphCardDialog::choice() const {
    const core::ProjectElement* card = currentCard();
    return ParagraphCardChoice{card != nullptr ? card->id : QString(),
                               m_role->currentData().toString()};
}

void ParagraphCardDialog::setCurrentCard(const QString& cardId) {
    for (int i = 0; i < m_cards->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_cards->topLevelItem(i);
        if (!cardId.isEmpty() && item->data(0, CARD_ROLE).toString() == cardId) {
            m_cards->setCurrentItem(item);
            return;
        }
    }
}

bool ParagraphCardDialog::eventFilter(QObject* watched, QEvent* event) {
    if (takeFilterKeys(watched, event, m_filter, m_cards)) {
        return true;
    }
    return KalahariDialog::eventFilter(watched, event);
}

QString linkParagraphWithCard(core::ProjectManager& book, const QString& elementId,
                              int paragraph, const QString& paragraphText,
                              const QString& cardId, QWidget* parent) {
    ParagraphCardDialog dialog(book, elementId, paragraph, paragraphText, cardId, parent);
    if (dialog.exec() != QDialog::Accepted) {
        return {};
    }
    const ParagraphCardChoice choice = dialog.choice();
    if (choice.cardId.isEmpty()) {
        return {};
    }
    if (!book.linkParagraph(choice.cardId, elementId, paragraph, paragraphText, choice.role)) {
        //: In Polish: Nie powiązano akapitu
        MessageDialog::warning(parent, ParagraphCardDialog::tr("Paragraph Not Linked"),
                               ParagraphCardDialog::tr("Failed to save changes."));
        return {};
    }
    return choice.cardId;
}

}  // namespace kalahari::gui::dialogs
