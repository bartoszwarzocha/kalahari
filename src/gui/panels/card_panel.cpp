// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_panel.cpp
/// @brief The full card of a character, a location or an item on a tab beside the chapters

#include "kalahari/gui/panels/card_panel.h"

#include "kalahari/editor/book_editor.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/panels/card_page.h"
#include "kalahari/gui/panels/editor_panel.h"

#include <QLayout>

namespace kalahari::gui {

CardPanel::CardPanel(core::ProjectManager& book, CommandRegistry& commands,
                     const QString& elementId, QWidget* parent)
    : EditorPanel(book, commands, parent), m_page(new CardPage(book, true, this)) {
    setObjectName(QStringLiteral("cardPanel"));
    // The editor of the text goes on the card, beside the data
    editor::BookEditor* description = getBookEditor();
    layout()->removeWidget(description);
    layout()->addWidget(m_page);
    m_page->setDescriptionWidget(description);
    //: The editor of the long description of a card, for screen readers. In Polish: Opis karty
    description->setAccessibleName(tr("Description of the card"));
    m_page->showCard(elementId);
}

QString CardPanel::elementId() const {
    return m_page->elementId();
}

}  // namespace kalahari::gui
