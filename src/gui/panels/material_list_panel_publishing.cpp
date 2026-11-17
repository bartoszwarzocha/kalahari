// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file material_list_panel_publishing.cpp
/// @brief The commands of the list of the texts for publishers: From the Cover, the filter
///        Kind, Add with the parts, and the set in the menu of a text

#include "kalahari/gui/panels/material_list_panel.h"

#include "kalahari/core/art_provider.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/gui/material_list.h"
#include "kalahari/gui/publishing_names.h"

#include <QAction>
#include <QComboBox>
#include <QMenu>
#include <QSignalBlocker>
#include <QToolButton>

#include <algorithm>

namespace kalahari::gui {

namespace {

using core::PublishingTexts;

/// Whether element @p elementId is the version of a part of the set of the book in @p book
bool inSet(const core::ProjectManager& book, const QString& elementId) {
    const QList<core::PublishingSetPart> parts = book.publishingSetParts();
    return std::any_of(parts.cbegin(), parts.cend(), [&elementId](const auto& part) {
        return part.elementId == elementId;
    });
}

/// Whether @p part is a description or a tagline, which the cover has
bool ofCover(const QString& part) {
    return part == QLatin1String(PublishingTexts::TAGLINE) ||
           part == QLatin1String(PublishingTexts::BLURB);
}

}  // namespace

void MaterialListPanel::createPublishingCommands() {
    //: Shows or hides the descriptions and the taglines of the cover in the list of the texts
    //: for publishers. In Polish: Z Okładki
    m_fromCoverButton->setText(MaterialList::tr("From the Cover"));
    //: In Polish: Pokaż albo schowaj opisy i hasła z Okładki
    m_fromCoverButton->setToolTip(
        MaterialList::tr("Show or hide the descriptions and the taglines of the cover"));
    m_fromCoverButton->setCheckable(true);
    m_fromCoverButton->hide();
    connect(m_fromCoverButton, &QToolButton::toggled, this, [this](bool shown) {
        if (MaterialList::isPublishing(m_list) && MaterialList::fromCoverShown(m_list) != shown) {
            MaterialList::setFromCoverShown(m_list, shown);
            refresh();
        }
    });
}

void MaterialListPanel::updatePublishingCommands() {
    const bool publishing = MaterialList::isPublishing(m_list);
    m_fromCoverButton->setVisible(publishing);
    if (!publishing) {
        return;
    }
    {
        const QSignalBlocker blocker(m_fromCoverButton);
        m_fromCoverButton->setChecked(MaterialList::fromCoverShown(m_list));
    }
    //: The gear of the list of the texts for publishers: Settings > Navigator > Texts for
    //: Publishers. In Polish: Ustawienia tekstów dla wydawnictwa
    m_settingsButton->setToolTip(MaterialList::tr("Texts for Publishers Settings"));
}

void MaterialListPanel::fillPublishingChoices() {
    auto& art = core::ArtProvider::getInstance();
    QStringList parts = PublishingTexts::parts();
    const QStringList cover{QString::fromLatin1(PublishingTexts::TAGLINE),
                            QString::fromLatin1(PublishingTexts::BLURB)};
    const QString chosen = m_states.value(m_list).part;
    {
        const QSignalBlocker blocker(m_partBox);
        m_partBox->clear();
        //: The filter Kind of the list of the cover that shows all of them. In Polish:
        //: wszystkie
        m_partBox->addItem(MaterialList::tr("all"), QString());
        // The descriptions and the taglines while the list shows them
        const QStringList shown =
            MaterialList::fromCoverShown(m_list) ? parts + cover : parts;
        for (const QString& part : shown) {
            m_partBox->addItem(art.getIcon(PublishingNames::partIconId(part),
                                           core::IconContext::ComboBox),
                               PublishingNames::partName(part), part);
        }
        m_partBox->setCurrentIndex(std::max(0, m_partBox->findData(chosen)));
    }
    // Add with the part chosen last (Insert), or a part of its menu; a description and a
    // tagline go to the cover
    m_addMenu->clear();
    for (const QString& part : parts + cover) {
        if (part == cover.constFirst()) {
            m_addMenu->addSeparator();
        }
        //: An item of Add of the list of the texts for publishers that adds a material of the
        //: cover; %1: what it is. In Polish: %1... (do Okładki)
        const QString text = ofCover(part)
                                 ? MaterialList::tr("%1... (to the Cover)")
                                       .arg(PublishingNames::partName(part))
                                 : PublishingNames::partName(part) + QStringLiteral("...");
        QAction* action = m_addMenu->addAction(text);
        action->setProperty("iconId", PublishingNames::partIconId(part));
        connect(action, &QAction::triggered, this,
                [this, part]() { emit requestAddCover(m_list, part); });
    }
    m_addButton->setMenu(m_addMenu);
    m_addButton->setPopupMode(QToolButton::MenuButtonPopup);
}

void MaterialListPanel::addPublishingItems(QMenu& menu, const MaterialRow& row) {
    if (!row.publishing && !row.fromCover) {
        return;
    }
    auto& art = core::ArtProvider::getInstance();
    const bool changing = !m_book.isReadOnly();
    const QString id = row.id;
    const bool taken = inSet(m_book, id);
    //: In the menu of a text for publishers that the set has. In Polish: Jest w zestawie
    const QString inTheSet = MaterialList::tr("In the Set");
    //: Makes a text for publishers, a description or a tagline the version of its part in the
    //: set for publishers. In Polish: &Wybierz do zestawu
    const QString chooseText = MaterialList::tr("C&hoose for the Set");
    QAction* choose = menu.addAction(
        art.getIcon(QStringLiteral("cover.choose"), core::IconContext::Menu),
        taken ? inTheSet : chooseText);
    choose->setEnabled(changing && !taken);
    if (row.fromCover) {
        connect(choose, &QAction::triggered, this, [this, id]() { emit requestSetPick(id); });
        //: Shows a description or a tagline in the list of the cover. In Polish: Pokaż w
        //: &Okładce
        QAction* show = menu.addAction(
            art.getIcon(QStringLiteral("material.cover"), core::IconContext::Menu),
            MaterialList::tr("Show in the &Cover"));
        connect(show, &QAction::triggered, this, [this, id]() { emit requestShowInCover(id); });
        return;
    }
    connect(choose, &QAction::triggered, this, [this, id]() {
        emit requestPublishingState(id, QString::fromLatin1(PublishingTexts::CHOSEN));
    });
    //: Rejects a text for publishers; it stays in the list. In Polish: O&drzuć
    QAction* reject = menu.addAction(
        art.getIcon(QStringLiteral("cover.reject"), core::IconContext::Menu),
        MaterialList::tr("Re&ject"));
    reject->setEnabled(changing && row.state != QLatin1String(PublishingTexts::REJECTED));
    connect(reject, &QAction::triggered, this, [this, id]() {
        emit requestPublishingState(id, QString::fromLatin1(PublishingTexts::REJECTED));
    });
    if (!row.state.isEmpty()) {
        //: Makes a text for publishers chosen or rejected a draft again. In Polish:
        //: Przywróć jako &roboczy
        QAction* draft = menu.addAction(MaterialList::tr("&Back to Draft"));
        draft->setEnabled(changing);
        connect(draft, &QAction::triggered, this,
                [this, id]() { emit requestPublishingState(id, QString()); });
    }
}

}  // namespace kalahari::gui
