// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file cursor_card_box.cpp
/// @brief The card at the cursor in the Properties panel: the card whose name the cursor of a
/// text of the book is in

#include "kalahari/gui/panels/cursor_card_box.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/card_name_index.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/editor_types.h"
#include "kalahari/gui/panels/card_page.h"
#include "kalahari/gui/panels/card_parts.h"
#include "kalahari/gui/widgets/relations_box.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

#include <cstddef>
#include <string>

namespace kalahari::gui {

namespace {

/// Space between the parts of the group, and its margins
constexpr int SPACING = 11;
/// Space between the lines of the name and the kind
constexpr int LINE_SPACING = 2;
/// Side of the gear at the title of the group
constexpr int TITLE_BUTTON = 16;

}  // namespace

QString cardNamedAt(const core::ProjectManager& book, const editor::BookEditor& text) {
    const editor::CursorPosition cursor = text.cursorPosition();
    const QString card =
        book.cardNames()
            .cardAt(text.paragraphPlainText(static_cast<size_t>(cursor.paragraph)),
                    cursor.offset)
            .card;
    return book.findElement(card) != nullptr ? card : QString();  // Not a card deleted
}

CursorCardBox::Shown CursorCardBox::cardsAtCursor() {
    const auto value = core::SettingsManager::getInstance().get<std::string>(SETTING, NAMES);
    if (value == PARAGRAPHS) {
        return Shown::Paragraphs;
    }
    return value == NONE ? Shown::None : Shown::NamesAndParagraphs;
}

CursorCardBox::CursorCardBox(core::ProjectManager& book, QWidget* parent)
    : QGroupBox(parent), m_settingsButton(new QToolButton(this)), m_book(book) {
    setObjectName(QStringLiteral("cursorCardGroup"));
    //: The group of the Properties panel with the card whose name the cursor of the text is
    //: in. In Polish: Karta przy kursorze
    setTitle(tr("Card at the Cursor"));
    auto* layout = card::groupLayout(this);
    layout->setSpacing(SPACING);

    // The gear at the title of the group
    m_settingsButton->setObjectName(QStringLiteral("cursorCardSettings"));
    m_settingsButton->setAutoRaise(true);
    m_settingsButton->setFixedSize(TITLE_BUTTON, TITLE_BUTTON);
    //: The gear of the card at the cursor: Settings > Cards. In Polish: Ustawienia kart
    m_settingsButton->setToolTip(tr("Card Settings"));
    m_settingsButton->setAccessibleName(m_settingsButton->toolTip());
    card::keepIcon(m_settingsButton, QStringLiteral("dock.settings"), core::IconContext::Menu);
    connect(m_settingsButton, &QToolButton::clicked, this, &CursorCardBox::requestSettings);

    createHeader(layout);
    m_short = card::hintLabel(this, QString(), QStringLiteral("cursorCardShort"));
    layout->addWidget(m_short);

    m_relations = new RelationsBox(m_book, false, this);
    m_relations->setFlat(true);
    layout->addWidget(m_relations);
    connect(m_relations, &RelationsBox::requestShowCard, this,
            &CursorCardBox::requestSelectCard);
    connect(m_relations, &RelationsBox::cardAdded, this, &CursorCardBox::cardAdded);

    //: The button of the card at the cursor in the Properties panel, which opens its full
    //: card. In Polish: Otwórz kartę
    m_openButton = new QPushButton(tr("Open Card"), this);
    m_openButton->setObjectName(QStringLiteral("cursorCardOpen"));
    card::keepIcon(m_openButton, QStringLiteral("file.open"), core::IconContext::Button);
    //: In Polish: Otwórz pełną kartę obok rozdziałów
    m_openButton->setToolTip(tr("Open the full card beside the chapters"));
    connect(m_openButton, &QPushButton::clicked, this,
            [this]() { emit requestOpenCard(m_cardId); });
    auto* openRow = new QHBoxLayout();
    openRow->addWidget(m_openButton);
    openRow->addStretch(1);
    layout->addLayout(openRow);
    setVisible(false);

    // A change made elsewhere: on the card, in the Navigator
    const auto changed = [this](const QString& elementId) {
        if (elementId == m_cardId) {
            refresh();
        }
    };
    connect(&m_book, &core::ProjectManager::cardChanged, this, changed);
    connect(&m_book, &core::ProjectManager::elementRenamed, this, changed);
}

void CursorCardBox::createHeader(QVBoxLayout* layout) {
    m_portrait = new QLabel(this);
    m_portrait->setObjectName(QStringLiteral("cursorCardPortrait"));
    m_portrait->setFixedSize(PORTRAIT, PORTRAIT);
    m_name = new QLabel(this);
    m_name->setObjectName(QStringLiteral("cursorCardName"));
    m_name->setWordWrap(true);
    m_name->setTextFormat(Qt::PlainText);
    QFont bold = m_name->font();
    bold.setBold(true);
    m_name->setFont(bold);
    m_subtitle = new QLabel(this);
    m_subtitle->setObjectName(QStringLiteral("cursorCardKind"));
    m_subtitle->setWordWrap(true);
    m_subtitle->setTextFormat(Qt::PlainText);

    auto* titles = new QVBoxLayout();
    titles->setSpacing(LINE_SPACING);
    titles->addWidget(m_name);
    titles->addWidget(m_subtitle);
    titles->addStretch(1);
    auto* header = new QHBoxLayout();
    header->setSpacing(SPACING);
    header->addWidget(m_portrait, 0, Qt::AlignTop);
    header->addLayout(titles, 1);
    layout->addLayout(header);
}

void CursorCardBox::showCard(const QString& cardId) {
    m_cardId = cardId;
    m_relations->showCard(cardId);
    refresh();
}

void CursorCardBox::refresh() {
    const core::ProjectElement* element =
        m_cardId.isEmpty() ? nullptr : m_book.findElement(m_cardId);
    const core::KindRef kind = element != nullptr ? m_book.kindOf(*element) : core::KindRef{};
    if (!kind || !kind.kind->isCard()) {
        setVisible(false);
        return;
    }
    m_portrait->setPixmap(CardPage::portraitOf(m_book, *element, PORTRAIT, devicePixelRatioF()));
    m_portrait->setAccessibleName(element->title);
    m_name->setText(element->title);
    m_subtitle->setText(CardPage::subtitleOf(m_book, *element));
    const QString summary = element->description.simplified();
    m_short->setText(summary);
    m_short->setVisible(!summary.isEmpty());
    setVisible(true);
}

void CursorCardBox::resizeEvent(QResizeEvent* event) {
    QGroupBox::resizeEvent(event);
    card::placeAtTitle(this, m_settingsButton);
}

}  // namespace kalahari::gui
