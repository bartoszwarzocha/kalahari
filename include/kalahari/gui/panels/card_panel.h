// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file card_panel.h
/// @brief The full card of a character, a location or an item on a tab beside the chapters

#ifndef KALAHARI_GUI_PANELS_CARD_PANEL_H
#define KALAHARI_GUI_PANELS_CARD_PANEL_H

#include "kalahari/gui/panels/editor_panel.h"

#include <QString>

namespace kalahari::gui {

class CardPage;

/// @brief The full card of a character, a location or an item (CardPage) on a tab beside the
/// chapters, with the editor of its text, its long description, beside its data
///
/// It is the editor of the text of the card (EditorPanel): the tab and the Navigator show a
/// "*" while the description is not saved, Save saves it, and its spelling, words, versions
/// and formats are those of a text of the Workshop.
class CardPanel : public EditorPanel {
    Q_OBJECT

public:
    /// @param book The book of its window
    /// @param commands The commands of its window
    /// @param elementId The card
    /// @param parent Parent widget
    CardPanel(core::ProjectManager& book, CommandRegistry& commands, const QString& elementId,
              QWidget* parent = nullptr);

    /// @brief The card
    [[nodiscard]] CardPage* page() const { return m_page; }

    /// @brief The element of the card
    [[nodiscard]] QString elementId() const;

private:
    CardPage* m_page = nullptr;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PANELS_CARD_PANEL_H
