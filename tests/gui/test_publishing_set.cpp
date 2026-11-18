// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_publishing_set.cpp
/// @brief The set for a publisher, the second view of the list of the texts for publishers:
///        its parts with their versions and texts, its keys, Copy and Copy All

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
#include "kalahari/gui/widgets/publishing_set_view.h"

#include <catch2/catch_test_macros.hpp>

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QTableView>
#include <QToolButton>

namespace core = kalahari::core;
namespace gui = kalahari::gui;
namespace test = kalahari::test;
using core::CoverMaterial;
using core::PublishingTexts;
using gui::MaterialList;
using gui::MaterialListPanel;
using gui::PublishingSetCard;

namespace {

/// The list of the texts for publishers: "kalahari.base:publishing"
constexpr QLatin1String LIST("kalahari.base:publishing");

/// Press @p key with @p modifiers on @p widget
void press(QWidget* widget, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    REQUIRE(widget != nullptr);
    QKeyEvent event(QEvent::KeyPress, key, modifiers);
    QCoreApplication::sendEvent(widget, &event);
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

TEST_CASE("Set for the publisher: the second view of the list of the texts",
          "[gui][publishing][materials]") {
    gui::registerAllIcons();
    const test::PublishingSettings kept;
    test::OpenNovel novel(QStringLiteral("Sand"));
    auto& pm = novel.pm;
    const QString first = test::addPublishingText(
        PublishingTexts::PITCH, QStringLiteral("Sentence – version 1"),
        QStringLiteral("When the well runs dry, Leila must find water."));
    const QString second = test::addPublishingText(
        PublishingTexts::PITCH, QStringLiteral("Sentence – version 2"), QStringLiteral("Sand."));
    const QString synopsis = test::addPublishingText(
        PublishingTexts::SYNOPSIS, QStringLiteral("Synopsis – version 1"),
        QStringLiteral("Leila crosses the desert."));
    const QString blurb = test::addCoverText(QStringLiteral("Description 1"), CoverMaterial::BLURB,
                                             QStringLiteral("A road through the sand."));
    REQUIRE(pm.setPublishingState(first, QString::fromLatin1(PublishingTexts::CHOSEN)));
    REQUIRE(pm.setPublishingState(synopsis, QString::fromLatin1(PublishingTexts::CHOSEN)));
    REQUIRE(pm.setCoverState(blurb, QString::fromLatin1(CoverMaterial::CHOSEN)));

    test::Window window;
    gui::MaterialListCoordinator lists(pm, window.tabs, &window.navigator, &window.statusBar,
                                       [](bool) {});
    MaterialListPanel* panel = lists.openList(LIST, false);
    REQUIRE(panel != nullptr);
    // The second view, kept for the list
    REQUIRE(panel->planButton()->isVisibleTo(panel));
    CHECK(panel->planButton()->toolTip() == QStringLiteral("Set for the Publisher"));
    QStringList properties;
    QObject::connect(panel, &MaterialListPanel::requestSetProperties,
                     [&properties](const QString& part) { properties << part; });
    panel->showPlan(true);
    CHECK(MaterialList::planShown(LIST));
    CHECK(panel->planButton()->isChecked());
    CHECK(properties.count() == 1);
    // The search and the filters do not filter the set
    CHECK_FALSE(panel->searchField()->isVisibleTo(panel));

    gui::PublishingSetView* set = panel->setView();
    REQUIRE(set != nullptr);
    CHECK(set->titleLabel()->text() == QStringLiteral("SAND"));
    CHECK(set->parts() == PublishingTexts::setParts());
    CHECK(panel->setHeading()->text().contains(QStringLiteral("Set for the Publisher · 5 of 5")));
    // A sentence: its versions, the one chosen marked, its length with its target
    PublishingSetCard* pitch = set->card(QStringLiteral("pitch"));
    REQUIRE(pitch != nullptr);
    CHECK(itemsOf(*pitch->versionBox()) ==
          QStringList{QStringLiteral("Sentence – version 1 ★"),
                      QStringLiteral("Sentence – version 2")});
    CHECK(pitch->versionBox()->currentData().toString() == first);
    CHECK(pitch->textLabel()->text() ==
          QStringLiteral("When the well runs dry, Leila must find water."));
    CHECK(pitch->textLabel()->font().italic());
    CHECK(pitch->findChild<QLabel*>(QStringLiteral("publishingSetLength"))->text() ==
          QStringLiteral("46 / 200 ch."));
    // A description of the Cover, chosen for the cover; a tagline the book does not have
    PublishingSetCard* cover = set->card(QStringLiteral("blurb"));
    REQUIRE(cover != nullptr);
    CHECK(cover->findChild<QLabel*>(QStringLiteral("publishingSetFromCover")) != nullptr);
    CHECK(itemsOf(*cover->versionBox()) ==
          QStringList{QStringLiteral("Description 1 ★ on the cover")});
    CHECK_FALSE(cover->versionBox()->isEnabled());
    PublishingSetCard* tagline = set->card(QStringLiteral("tagline"));
    REQUIRE(tagline != nullptr);
    CHECK(tagline->textLabel()->text() == QStringLiteral("The book has no such text yet."));
    CHECK_FALSE(tagline->openButton()->isEnabled());
    CHECK_FALSE(tagline->copyButton()->isEnabled());

    // Copy All: the title and the author, then each part with a text under its name
    panel->copyAllButton()->click();
    CHECK(QApplication::clipboard()->text().toStdString() ==
          "Sand\nAuthor\n\n"
          "SENTENCE ABOUT THE BOOK\nWhen the well runs dry, Leila must find water.\n\n"
          "COVER DESCRIPTION\nA road through the sand.\n\n"
          "SYNOPSIS\nLeila crosses the desert.\n");
    // Copy of a part: its text
    pitch->copyButton()->click();
    CHECK(QApplication::clipboard()->text() ==
          QStringLiteral("When the well runs dry, Leila must find water."));

    // The keys: Down goes to the next part, which the Properties panel shows
    panel->focusList();
    CHECK(set->current() == QStringLiteral("pitch"));
    press(pitch, Qt::Key_Down);
    CHECK(set->current() == QStringLiteral("tagline"));
    CHECK(properties.last() == QStringLiteral("tagline"));
    // Enter opens the version of the part
    QStringList opened;
    QObject::connect(panel, &MaterialListPanel::requestOpen,
                     [&opened](const QString& elementId) { opened << elementId; });
    press(set->card(QStringLiteral("blurb")), Qt::Key_Return);
    CHECK(opened.contains(blurb));
    // Space leaves a part out, and includes it again
    press(set->card(QStringLiteral("synopsis")), Qt::Key_Space);
    CHECK_FALSE(pm.publishingSet().includes(QStringLiteral("synopsis")));
    CHECK(panel->setHeading()->text().contains(QStringLiteral("4 of 5")));
    CHECK(set->card(QStringLiteral("synopsis"))->accessibleName() ==
          QStringLiteral("Synopsis (left out of the set)"));
    panel->copyAllButton()->click();
    CHECK_FALSE(QApplication::clipboard()->text().contains(QStringLiteral("SYNOPSIS")));
    press(set->card(QStringLiteral("synopsis")), Qt::Key_Space);
    CHECK(pm.publishingSet().includes(QStringLiteral("synopsis")));

    // Another version for the set alone: the sentence chosen stays chosen
    QComboBox* versions = set->card(QStringLiteral("pitch"))->versionBox();
    const int other = versions->findData(second);
    REQUIRE(other >= 0);
    versions->setCurrentIndex(other);
    emit versions->activated(other);
    CHECK(pm.publishingSet().picks.value(QStringLiteral("pitch")) == second);
    CHECK(pm.findElement(first)->publishing.state == PublishingTexts::CHOSEN);
    CHECK(set->card(QStringLiteral("pitch"))->textLabel()->text() == QStringLiteral("Sand."));

    // Back to the table: the search again
    panel->showPlan(false);
    CHECK_FALSE(MaterialList::planShown(LIST));
    CHECK(panel->searchField()->isVisibleTo(panel));
    CHECK(panel->view() == static_cast<QAbstractItemView*>(panel->table()));
}
