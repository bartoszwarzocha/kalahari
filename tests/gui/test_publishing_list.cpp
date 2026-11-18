// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_publishing_list.cpp
/// @brief The list of the texts for publishers: its groups and columns, the texts of the
///        Cover, the filter Kind, Add and the set in the menu of a text

#include "kalahari_tests/gui/document_window.h"
#include "kalahari_tests/gui/publishing_test_utils.h"
#include "kalahari_tests/test_support/window_parts.h"

#include "kalahari/core/cover_material.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/publishing_texts.h"
#include "kalahari/gui/icon_registrar.h"
#include "kalahari/gui/material_list.h"
#include "kalahari/gui/material_list_coordinator.h"
#include "kalahari/gui/panels/material_list_panel.h"

#include <catch2/catch_test_macros.hpp>

#include <QAction>
#include <QComboBox>
#include <QMenu>
#include <QSortFilterProxyModel>
#include <QToolButton>

#include <string>

namespace core = kalahari::core;
namespace gui = kalahari::gui;
namespace test = kalahari::test;
using core::CoverMaterial;
using core::PublishingTexts;
using gui::MaterialList;
using gui::MaterialListPanel;

namespace {

/// The list of the texts for publishers: "kalahari.base:publishing"
constexpr QLatin1String LIST("kalahari.base:publishing");

/// The names of the groups of the rows of @p panel, in their order
QStringList headingsOf(const MaterialListPanel& panel) {
    QStringList headings;
    const QSortFilterProxyModel* rows = panel.rows();
    for (int row = 0; row < rows->rowCount(); ++row) {
        const QString heading =
            rows->index(row, 0).data(MaterialListPanel::HEADING_ROLE).toString();
        if (!heading.isEmpty()) {
            headings << heading;
        }
    }
    return headings;
}

/// The elements of the rows of @p panel, in their order
QStringList idsOf(const MaterialListPanel& panel) {
    QStringList ids;
    const QSortFilterProxyModel* rows = panel.rows();
    for (int row = 0; row < rows->rowCount(); ++row) {
        const QString id = rows->index(row, 0).data(MaterialListPanel::ID_ROLE).toString();
        if (!id.isEmpty()) {
            ids << id;
        }
    }
    return ids;
}

/// What the row of @p elementId shows in column @p columnId of @p panel
QString cellOf(const MaterialListPanel& panel, const QString& elementId, const char* columnId) {
    const QSortFilterProxyModel* rows = panel.rows();
    // The columns of the model are those of the list
    const QList<gui::MaterialColumn> columns = MaterialList::columnsOf(test::book(), panel.list());
    int column = -1;
    for (int each = 0; each < columns.size(); ++each) {
        if (columns.at(each).id == QLatin1String(columnId)) {
            column = each;
        }
    }
    REQUIRE(column >= 0);
    for (int row = 0; row < rows->rowCount(); ++row) {
        const QModelIndex index = rows->index(row, column);
        if (index.data(MaterialListPanel::ID_ROLE).toString() == elementId) {
            return index.data(Qt::DisplayRole).toString();
        }
    }
    return {};
}

/// The items of the menu of the material chosen in @p panel, without their access keys and
/// their keys, between " | "; "(off)" after an item greyed out
std::string menuOf(MaterialListPanel& panel, const QString& elementId) {
    panel.selectMaterial(elementId);
    QMenu menu;
    panel.fillMenu(menu);
    QStringList items;
    for (const QAction* action : menu.actions()) {
        if (action->isSeparator()) {
            continue;
        }
        const QString text =
            action->text().section(QLatin1Char('\t'), 0, 0).remove(QLatin1Char('&'));
        items << (action->isEnabled() ? text : text + QStringLiteral(" (off)"));
    }
    return items.join(QStringLiteral(" | ")).toStdString();
}

/// The texts of the items of @p box
QStringList itemsOf(const QComboBox& box) {
    QStringList items;
    for (int item = 0; item < box.count(); ++item) {
        items << box.itemText(item);
    }
    return items;
}

}  // namespace

TEST_CASE("Texts for publishers: the list in groups by kind, with the texts of the Cover",
          "[gui][publishing][materials]") {
    gui::registerAllIcons();
    const test::PublishingSettings kept;
    test::OpenNovel novel(QStringLiteral("Sand"));
    auto& pm = novel.pm;
    const QString pitch = test::addPublishingText(
        PublishingTexts::PITCH, QStringLiteral("Sentence – version 1"),
        QStringLiteral("When the well runs dry, Leila must find water."),
        QStringLiteral("kalahari.novel:when_must"));
    const QString other = test::addPublishingText(PublishingTexts::PITCH,
                                                  QStringLiteral("Sentence – version 2"),
                                                  QStringLiteral("A map of sand."));
    const QString synopsis = test::addPublishingText(
        PublishingTexts::SYNOPSIS, QStringLiteral("Synopsis – version 1"),
        QStringLiteral("Leila crosses the desert and finds the last well."));
    const QString blurb = test::addCoverText(QStringLiteral("Description 1"), CoverMaterial::BLURB,
                                             QStringLiteral("Road."));
    const QString tagline = test::addCoverText(QStringLiteral("Tagline 1"), CoverMaterial::TAGLINE,
                                               QStringLiteral("Sand remembers."));
    REQUIRE(pm.setPublishingState(other, QString::fromLatin1(PublishingTexts::REJECTED)));
    CHECK(MaterialList::isPublishing(LIST));
    CHECK(MaterialList::hasGroups(LIST));
    CHECK(MaterialList::partColumnOf(LIST) == QString::fromLatin1(MaterialList::PUBLISHING_PART));
    CHECK(MaterialList::kinds(pm).contains(LIST));
    // The Navigator counts the texts, not those of the Cover
    CHECK(MaterialList::countOf(pm, LIST) == 3);

    test::Window window;
    gui::MaterialListCoordinator lists(pm, window.tabs, &window.navigator, &window.statusBar,
                                       [](bool) {});
    MaterialListPanel* panel = lists.openList(LIST, false);
    REQUIRE(panel != nullptr);

    // In groups at first: the sentences, the synopses, then the texts of the Cover
    CHECK(panel->groupsButton()->isVisibleTo(panel));
    CHECK(panel->groupsButton()->isChecked());
    // The set for a publisher is its second view
    CHECK(panel->planButton()->isVisibleTo(panel));
    CHECK(headingsOf(*panel) ==
          QStringList{"Sentences about the book · 2", "Synopses · 1",
                      "Cover descriptions · from the Cover · 1", "Taglines · from the Cover · 1"});
    CHECK(idsOf(*panel).size() == 5);

    // A sentence shows its words, a synopsis its title; the template, the state, the length
    // with its target
    CHECK(cellOf(*panel, pitch, MaterialList::PUBLISHING_TEXT) ==
          QStringLiteral("When the well runs dry, Leila must find water."));
    CHECK(cellOf(*panel, synopsis, MaterialList::PUBLISHING_TEXT) ==
          QStringLiteral("Synopsis – version 1"));
    CHECK(cellOf(*panel, pitch, MaterialList::PUBLISHING_TEMPLATE) ==
          QStringLiteral("When… must… before…"));
    CHECK(cellOf(*panel, synopsis, MaterialList::PUBLISHING_TEMPLATE) ==
          QStringLiteral("No template"));
    CHECK(cellOf(*panel, other, MaterialList::PUBLISHING_STATE) == QStringLiteral("rejected"));
    CHECK(cellOf(*panel, pitch, MaterialList::PUBLISHING_LENGTH) == QStringLiteral("46 / 200 ch."));
    CHECK(cellOf(*panel, blurb, MaterialList::PUBLISHING_TEMPLATE) == QStringLiteral("–"));
    CHECK(cellOf(*panel, tagline, MaterialList::PUBLISHING_TEXT) ==
          QStringLiteral("Sand remembers."));
    // Copy takes the words of a text
    CHECK(MaterialList::copyTextOf(pm, synopsis) ==
          QStringLiteral("Leila crosses the desert and finds the last well."));

    // Kind: the parts of the texts and of the Cover
    REQUIRE(panel->partBox()->isVisibleTo(panel));
    CHECK(itemsOf(*panel->partBox()) ==
          QStringList{"all", "Sentence about the book", "Synopsis", "Tagline",
                      "Cover description"});
    panel->setPartFilter(QString::fromLatin1(PublishingTexts::SYNOPSIS));
    CHECK(idsOf(*panel) == QStringList{synopsis});
    CHECK(headingsOf(*panel) == QStringList{"Synopses · 1"});
    panel->setPartFilter(QString());

    // From the Cover hides the descriptions and the taglines, and their kinds
    QToolButton* fromCover = panel->fromCoverButton();
    REQUIRE(fromCover->isVisibleTo(panel));
    CHECK(fromCover->isChecked());
    fromCover->click();
    CHECK_FALSE(MaterialList::fromCoverShown(LIST));
    CHECK(idsOf(*panel).size() == 3);
    CHECK(headingsOf(*panel) == QStringList{"Sentences about the book · 2", "Synopses · 1"});
    CHECK(itemsOf(*panel->partBox()) ==
          QStringList{"all", "Sentence about the book", "Synopsis"});
    fromCover->click();
    CHECK(idsOf(*panel).size() == 5);

    // Add: the parts of the texts, then those that go to the Cover
    REQUIRE(panel->addButton()->menu() != nullptr);
    QStringList adds;
    for (const QAction* action : panel->addButton()->menu()->actions()) {
        if (!action->isSeparator()) {
            adds << action->text();
        }
    }
    CHECK(adds == QStringList{"Sentence about the book...", "Synopsis...",
                              "Tagline... (to the Cover)", "Cover description... (to the Cover)"});
    // The gear names the settings of the texts
    CHECK(panel->settingsButton()->toolTip() == QStringLiteral("Texts for Publishers Settings"));

    // The other lists do not have From the Cover
    MaterialListPanel* covers = lists.openList(QStringLiteral("kalahari.base:cover"), false);
    REQUIRE(covers != nullptr);
    CHECK_FALSE(covers->fromCoverButton()->isVisibleTo(covers));
}

TEST_CASE("Texts for publishers: the menu of a text, the set and the Cover",
          "[gui][publishing][materials]") {
    gui::registerAllIcons();
    const test::PublishingSettings kept;
    test::OpenNovel novel(QStringLiteral("Sand"));
    auto& pm = novel.pm;
    const QString first = test::addPublishingText(PublishingTexts::PITCH,
                                                  QStringLiteral("Sentence – version 1"),
                                                  QStringLiteral("A well."));
    const QString second = test::addPublishingText(PublishingTexts::PITCH,
                                                   QStringLiteral("Sentence – version 2"),
                                                   QStringLiteral("A map."));
    const QString chosen = test::addCoverText(QStringLiteral("Description 1"), CoverMaterial::BLURB,
                                              QStringLiteral("Road."));
    const QString draft = test::addCoverText(QStringLiteral("Description 2"), CoverMaterial::BLURB,
                                             QStringLiteral("Sand."));
    REQUIRE(pm.setCoverState(chosen, QString::fromLatin1(CoverMaterial::CHOSEN)));

    test::Window window;
    gui::MaterialListCoordinator lists(pm, window.tabs, &window.navigator, &window.statusBar,
                                       [](bool) {});
    MaterialListPanel* panel = lists.openList(LIST, false);
    REQUIRE(panel != nullptr);

    // A draft: chosen for the set or rejected
    CHECK(menuOf(*panel, first) == "Edit | Choose for the Set | Reject | Copy Text | Rename... | "
                                   "Delete... | Properties | Add Text for publishers...");
    emit panel->requestPublishingState(first, QString::fromLatin1(PublishingTexts::CHOSEN));
    CHECK(pm.findElement(first)->publishing.state == PublishingTexts::CHOSEN);
    CHECK(window.statusBar.currentMessage().contains(QStringLiteral("Sentence – version 1")));
    panel->refresh();
    CHECK(menuOf(*panel, first).starts_with("Edit | In the Set (off) | Reject | Back to Draft | "));
    // Another one chosen: the first one is a draft again
    emit panel->requestPublishingState(second, QString::fromLatin1(PublishingTexts::CHOSEN));
    CHECK(pm.findElement(second)->publishing.state == PublishingTexts::CHOSEN);
    CHECK(pm.findElement(first)->publishing.state.isEmpty());
    emit panel->requestPublishingState(first, QString::fromLatin1(PublishingTexts::REJECTED));
    CHECK(pm.findElement(first)->publishing.state == PublishingTexts::REJECTED);
    panel->refresh();
    CHECK(menuOf(*panel, first).starts_with(
        "Edit | Choose for the Set | Reject (off) | Back to Draft | "));

    // A description of the Cover: the set takes the one chosen for the cover, or another one
    CHECK(menuOf(*panel, chosen) ==
          "Edit | Open | In the Set (off) | Show in the Cover | Copy Text | Link... | Rename... | "
          "Delete... | Properties | Add Text for publishers...");
    CHECK(menuOf(*panel, draft).starts_with("Edit | Open | Choose for the Set | "));
    emit panel->requestSetPick(draft);
    CHECK(pm.publishingSet().picks.value(QString::fromLatin1(PublishingTexts::BLURB)) == draft);
    CHECK(window.statusBar.currentMessage() ==
          QStringLiteral("\"Description 2\" is now in the set for publishers."));
    CHECK(pm.findElement(draft)->cover.state.isEmpty());  // the cover stays as it was
    CHECK(menuOf(*panel, chosen).starts_with("Edit | Open | Choose for the Set | "));
    emit panel->requestSetPick(chosen);
    CHECK(pm.publishingSet().picks.isEmpty());
    // A text chosen for the set takes the place of a version the set picked
    core::PublishingSet set = pm.publishingSet();
    set.picks.insert(QString::fromLatin1(PublishingTexts::PITCH), first);
    REQUIRE(pm.setPublishingSet(set));
    REQUIRE(pm.setPublishingState(second, QString::fromLatin1(PublishingTexts::CHOSEN)));
    CHECK(pm.publishingSet().picks.isEmpty());
    // A chapter is no version of a part of the set
    CHECK_FALSE(pm.pickForPublishingSet(novel.chapterId));

    // Show in the Cover: the list of the cover with the description chosen
    emit panel->requestShowInCover(draft);
    auto* cover = qobject_cast<MaterialListPanel*>(window.tabs->currentWidget());
    REQUIRE(cover != nullptr);
    CHECK(MaterialList::isCover(cover->list()));
    CHECK(cover->currentMaterial() == draft);
}
