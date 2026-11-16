// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_page_portraits.cpp
/// @brief The portraits of a card: put together in the window Portrait, the main one, the
/// arrows to the others, saved as a picture

#include "kalahari/gui/panels/card_page.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/book_type_package.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/card_data.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/avatar.h"
#include "kalahari/gui/avatar_art.h"
#include "kalahari/gui/dialogs/portrait_dialog.h"
#include "kalahari/gui/panels/card_parts.h"

#include <QAction>
#include <QDialog>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QMenu>
#include <QToolButton>

#include <algorithm>

namespace kalahari::gui {

namespace {

/// The fields of a character the window Portrait reads and writes
constexpr const char* GENDER = "gender";
constexpr const char* AGE = "age";
constexpr const char* APPEARANCE = "appearance";

/// @p name of a portrait with the star of the main one before it
QString starred(const QString& name) {
    return QStringLiteral("\u2605 ") + name;
}

/// The name of portrait @p index of @p card: its label, the title of its picture, else
/// "Portrait 2"
QString portraitName(const core::ProjectManager& book, const core::CardData& card,
                     qsizetype index) {
    const core::CardPortrait& portrait = card.portraits.at(index);
    if (!portrait.label.isEmpty()) {
        return portrait.label;
    }
    if (const core::ProjectElement* image =
            portrait.image.isEmpty() ? nullptr : book.findElement(portrait.image);
        image != nullptr && !image->title.isEmpty()) {
        return image->title;
    }
    //: A portrait of a card without a label; %1: its number. In Polish: Portret %1
    return CardPage::tr("Portrait %1").arg(index + 1);
}

/// The title of the picture of the portrait @p label of the card @p title in the materials
QString pictureTitle(const QString& title, const QString& label) {
    return label.isEmpty()
               //: The title of a portrait saved as a picture; %1: the name of the card. In
               //: Polish: Portret – %1
               ? CardPage::tr("Portrait – %1").arg(title)
               //: %2: the label of the portrait ("youth"). In Polish: Portret – %1 (%2)
               : CardPage::tr("Portrait – %1 (%2)").arg(title, label);
}

/// Whether @p element is a card of a character, whose portrait can be put together
bool isCharacter(const core::ProjectManager& book, const core::ProjectElement& element) {
    const core::KindRef kind = book.kindOf(element);
    return kind && kind.kind->id == QLatin1String("character");
}

/// The card @p element of book @p book for the window Portrait, showing its portrait @p index
dialogs::PortraitSubject subjectOf(const core::ProjectManager& book,
                                   const core::ProjectElement& element, qsizetype index) {
    const core::KindRef kind = book.kindOf(element);
    dialogs::PortraitSubject subject;
    subject.name = element.title;
    subject.gender = element.card.value(QLatin1String(GENDER));
    subject.age = element.card.value(QLatin1String(AGE));
    subject.hasAppearance = kind && book.cardField(kind, QLatin1String(APPEARANCE)) != nullptr;
    subject.appearance = element.card.value(QLatin1String(APPEARANCE));
    subject.color = QColor(element.card.color);
    subject.portraits = element.card.portraits;
    subject.current = index;
    return subject;
}

/// @p text of the field Appearance with @p description in a line after it; the text of the
/// writer stays, without the spaces at its end
QString appended(QString text, const QString& description) {
    while (!text.isEmpty() && text.back().isSpace()) {
        text.chop(1);
    }
    return text.isEmpty() ? description : text + QLatin1Char('\n') + description;
}

}  // namespace

QWidget* CardPage::portraitArrows(QWidget* parent) {
    m_portraitArrows = new QWidget(parent);
    m_portraitArrows->setObjectName(QStringLiteral("cardPortraitArrows"));
    auto* row = new QHBoxLayout(m_portraitArrows);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(0);
    const auto arrow = [this](const char* name, const char* iconId, const QString& tip) {
        auto* button = new QToolButton(m_portraitArrows);
        button->setObjectName(QLatin1String(name));
        button->setAutoRaise(true);
        card::keepIcon(button, QLatin1String(iconId), core::IconContext::Menu);
        button->setToolTip(tip);
        button->setAccessibleName(tip);
        return button;
    };
    //: The arrow under the portrait of a card to its portrait before. In Polish: Poprzedni
    //: portret
    m_previousPortrait =
        arrow("cardPreviousPortrait", "common.chevronLeft", tr("Previous Portrait"));
    //: In Polish: Następny portret
    m_nextPortrait = arrow("cardNextPortrait", "common.chevronRight", tr("Next Portrait"));
    m_portraitLabel = new QLabel(m_portraitArrows);
    m_portraitLabel->setObjectName(QStringLiteral("cardPortraitLabel"));
    m_portraitLabel->setAlignment(Qt::AlignCenter);
    m_portraitLabel->setForegroundRole(QPalette::PlaceholderText);
    row->addWidget(m_previousPortrait);
    row->addWidget(m_portraitLabel, 1);
    row->addWidget(m_nextPortrait);
    m_portraitArrows->setFixedWidth(FULL_PORTRAIT);
    connect(m_previousPortrait, &QToolButton::clicked, this, [this]() { showOtherPortrait(-1); });
    connect(m_nextPortrait, &QToolButton::clicked, this, [this]() { showOtherPortrait(1); });
    m_portraitArrows->hide();
    return m_portraitArrows;
}

void CardPage::showPortraitArrows(const core::ProjectElement& element) {
    if (m_portraitArrows == nullptr) {
        return;
    }
    const qsizetype count = element.card.portraits.size();
    m_portraitArrows->setVisible(count > 1);
    if (count < 2) {
        return;
    }
    const QString name = portraitName(m_book, element.card, m_shownPortrait);
    const QString shown = m_shownPortrait == 0 ? starred(name) : name;
    const int room = FULL_PORTRAIT - m_previousPortrait->sizeHint().width() -
                     m_nextPortrait->sizeHint().width();
    m_portraitLabel->setText(
        m_portraitLabel->fontMetrics().elidedText(shown, Qt::ElideRight, std::max(1, room)));
    //: Under the portrait of a card; %1: which of its portraits, %2: how many it has, %3: the
    //: name of the portrait. In Polish: Portret %1 z %2: %3
    m_portraitLabel->setToolTip(
        tr("Portrait %1 of %2: %3").arg(m_shownPortrait + 1).arg(count).arg(shown));
}

void CardPage::showOtherPortrait(int step) {
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    const qsizetype count = element != nullptr ? element->card.portraits.size() : 0;
    if (count < 2) {
        return;
    }
    m_shownPortrait = ((m_shownPortrait + step) % count + count) % count;
    showPortrait(*element);
}

void CardPage::addComposeAction(const core::ProjectElement& element) {
    if (!isCharacter(m_book, element)) {
        return;
    }
    const QList<core::CardPortrait>& portraits = element.card.portraits;
    const bool composed =
        m_shownPortrait < portraits.size() && portraits.at(m_shownPortrait).isComposed();
    //: Opens the window Portrait: the portrait of a character put together from ready parts.
    //: In Polish: &Zestaw portret...
    QAction* compose = m_portraitMenu->addAction(
        core::ArtProvider::getInstance().getIcon(QStringLiteral("template.character"),
                                                 core::IconContext::Menu),
        tr("&Compose Portrait..."));
    compose->setObjectName(QStringLiteral("cardComposePortrait"));
    compose->setEnabled(!m_book.isReadOnly());
    connect(compose, &QAction::triggered, this,
            [this, index = composed ? m_shownPortrait : -1]() { composePortrait(index); });
    m_portraitMenu->addSeparator();
}

void CardPage::addPortraitsMenu(const core::ProjectElement& element) {
    const core::CardData& card = element.card;
    const bool readOnly = m_book.isReadOnly();
    if (card.portraits.isEmpty()) {
        return;
    }
    m_portraitMenu->addSeparator();
    //: The submenu of the portraits of a card: its versions with their labels. In Polish:
    //: Portrety &karty
    QMenu* menu = m_portraitMenu->addMenu(tr("Card &Portraits"));
    menu->setObjectName(QStringLiteral("cardPortraits"));
    for (qsizetype i = 0; i < card.portraits.size(); ++i) {
        QString name = i == 0 ? starred(portraitName(m_book, card, i))
                              : portraitName(m_book, card, i);
        QAction* action = menu->addAction(name.replace(QLatin1Char('&'), QStringLiteral("&&")));
        action->setCheckable(true);
        action->setChecked(i == m_shownPortrait);
        connect(action, &QAction::triggered, this, [this, i]() {
            m_shownPortrait = i;
            if (const core::ProjectElement* shown = m_book.findElement(m_elementId)) {
                showPortrait(*shown);
            }
        });
    }
    menu->addSeparator();
    //: Makes the portrait shown the main one: on the card, in the lists and in Properties. In
    //: Polish: Ustaw jako &główny
    QAction* main = menu->addAction(tr("Set as &Main"));
    main->setObjectName(QStringLiteral("cardMainPortrait"));
    main->setEnabled(!readOnly && m_shownPortrait > 0);
    connect(main, &QAction::triggered, this, &CardPage::setMainPortrait);
    if (isCharacter(m_book, element)) {
        //: A new portrait of a character put together from parts. In Polish: &Nowy portret...
        QAction* added = menu->addAction(tr("&New Portrait..."));
        added->setObjectName(QStringLiteral("cardNewPortrait"));
        added->setEnabled(!readOnly);
        connect(added, &QAction::triggered, this, [this]() { composePortrait(-1); });
    }
    if (m_shownPortrait < card.portraits.size() &&
        card.portraits.at(m_shownPortrait).isComposed()) {
        //: Saves the portrait put together from parts as a PNG picture among the materials of
        //: the book. In Polish: Zapisz portret jako o&braz w materiałach
        QAction* save = m_portraitMenu->addAction(tr("&Save Portrait as a Picture in the "
                                                     "Materials"));
        save->setObjectName(QStringLiteral("cardSavePortrait"));
        save->setEnabled(!readOnly);
        connect(save, &QAction::triggered, this, &CardPage::savePortraitPicture);
    }
}

void CardPage::composePortrait(qsizetype index) {
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    if (element == nullptr || m_book.isReadOnly()) {
        return;
    }
    const QString title = element->title;
    dialogs::PortraitDialog dialog(subjectOf(m_book, *element, index), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    // The window waited for the writer: the card as it is now
    element = m_book.findElement(m_elementId);
    if (element == nullptr) {
        return;
    }
    core::CardData card = element->card;
    card.portraits = dialog.portraits();
    if (dialog.appendsDescription()) {
        card.values.insert(QLatin1String(APPEARANCE),
                           appended(card.value(QLatin1String(APPEARANCE)), dialog.description()));
    }
    m_shownPortrait = dialog.current();
    saveCard(card);
    if (dialog.savesPicture()) {
        static_cast<void>(addPicture(dialog.picture(),
                                     pictureTitle(title, dialog.portraits()
                                                             .at(dialog.current())
                                                             .label)));
    }
}

void CardPage::setMainPortrait() {
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    if (element == nullptr || m_book.isReadOnly() || m_shownPortrait <= 0 ||
        m_shownPortrait >= element->card.portraits.size()) {
        return;
    }
    core::CardData card = element->card;
    card.portraits.move(m_shownPortrait, 0);
    m_shownPortrait = 0;
    saveCard(card);
}

void CardPage::savePortraitPicture() {
    const core::ProjectElement* element = m_book.findElement(m_elementId);
    if (element == nullptr || m_book.isReadOnly() ||
        m_shownPortrait >= element->card.portraits.size() ||
        !element->card.portraits.at(m_shownPortrait).isComposed()) {
        return;
    }
    const core::CardPortrait portrait = element->card.portraits.at(m_shownPortrait);
    const QImage picture = dialogs::PortraitDialog::pictureOf(
        portrait, AvatarFrame::Bust, QColor(element->card.color),
        element->card.value(QLatin1String(GENDER)));
    static_cast<void>(addPicture(picture, pictureTitle(element->title, portrait.label)));
}

}  // namespace kalahari::gui
