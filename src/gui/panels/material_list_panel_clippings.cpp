// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file material_list_panel_clippings.cpp
/// @brief The commands of the list of the clippings: Restore to Its Place, Go to Its Place and
///        the gear of their settings, and the gear of the texts for publishers

#include "kalahari/gui/panels/material_list_panel.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/clipping_texts.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"
#include "kalahari/gui/material_list.h"
#include "kalahari/gui/settings_dialog.h"

#include <QAction>
#include <QMenu>
#include <QToolButton>

namespace kalahari::gui {

namespace {

/// @brief Where the clipping of @p row comes from, as the list has it
ClippingFrom fromOf(const MaterialRow& row) {
    return {.title = row.texts.value(QString::fromLatin1(MaterialList::CLIPPING_FROM)),
            .live = row.originLive};
}

}  // namespace

void MaterialListPanel::createClippingCommands() {
    //: Inserts the clipping chosen where it was cut from. In Polish: Przywróć na miejsce
    m_restoreButton->setText(MaterialList::tr("Restore to Its Place"));
    m_restoreButton->hide();
    connect(m_restoreButton, &QToolButton::clicked, this, [this]() {
        if (const QString id = currentMaterial(); !id.isEmpty()) {
            emit requestRestore(id);
        }
    });
}

void MaterialListPanel::updateClippingCommands(const MaterialRow* row) {
    const bool clippings = MaterialList::isClippings(m_list);
    m_restoreButton->setVisible(clippings);
    const bool clipping = row != nullptr && row->clipping;
    m_restoreButton->setEnabled(clipping && row->originLive && !m_book.isReadOnly());
    m_restoreButton->setToolTip(clipping ? ClippingTexts::restoreToolTip(fromOf(*row))
                                         : m_restoreButton->text());
    if (m_cards) {
        return;  // The gear of a list of cards opens Settings > Cards (applyListTexts())
    }
    //: The gear of the list of the clippings: Settings > Editor > Clippings. In Polish:
    //: Ustawienia wycinków
    const QString clippingSettings = MaterialList::tr("Clipping Settings");
    //: The gear of a list of materials: Settings > Navigator. In Polish: Ustawienia materiałów
    const QString materialSettings = MaterialList::tr("Material Settings");
    m_settingsButton->setToolTip(clippings ? clippingSettings : materialSettings);
}

void MaterialListPanel::addClippingItems(QMenu& menu, const MaterialRow& row) {
    if (!row.clipping) {
        return;
    }
    auto& art = core::ArtProvider::getInstance();
    const QString id = row.id;
    menu.addSeparator();
    //: In the menu of a clipping of a list: inserts it where it was cut from. In Polish:
    //: &Przywróć na miejsce
    QAction* restore = menu.addAction(
        art.getIcon(QStringLiteral("clipping.restore"), core::IconContext::Menu),
        MaterialList::tr("Restore to Its &Place"));
    restore->setEnabled(row.originLive && !m_book.isReadOnly());
    restore->setToolTip(ClippingTexts::restoreToolTip(fromOf(row)));
    connect(restore, &QAction::triggered, this, [this, id]() { emit requestRestore(id); });
    //: In the menu of a clipping of a list: opens the text it was cut from at the place. In
    //: Polish: Przejdź do &miejsca
    QAction* go = menu.addAction(
        art.getIcon(QStringLiteral("clipping.showOrigin"), core::IconContext::Menu),
        MaterialList::tr("&Go to Its Place"));
    go->setEnabled(row.originLive);
    connect(go, &QAction::triggered, this, [this, id]() { emit requestShowOrigin(id); });
}

void MaterialListPanel::openSettings() {
    QString page;
    if (MaterialList::isClippings(m_list)) {
        page = QString::fromLatin1(SettingsDialog::CLIPPINGS_PAGE);
    } else if (MaterialList::isPublishing(m_list)) {
        page = QString::fromLatin1(SettingsDialog::PUBLISHING_PAGE);
    }
    if (!page.isEmpty() && dialogs::KalahariDialog::openSettings(this, page)) {
        return;
    }
    emit requestSettings();
}

}  // namespace kalahari::gui
