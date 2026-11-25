// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file test_mind_map_relations.cpp
/// @brief Maps of relations: the cards they take, the map made and drawn in generations, the new
/// relations of the cards, the offer after the first relation and the commands of the module

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_type_registry.h>
#include <kalahari/core/extension_points.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/settings_manager.h>
#include <kalahari/core/window_editor_provider.h>
#include <kalahari/mindmap/mind_map.h>
#include <kalahari/mindmap/mind_map_content.h>
#include <kalahari/mindmap/mind_map_document.h>
#include <kalahari/mindmap/mind_map_editor.h>
#include <kalahari/mindmap/mind_map_look.h>
#include <kalahari/mindmap/mind_map_module.h>
#include <kalahari/mindmap/mind_map_relation_map.h>
#include <kalahari/mindmap/mind_map_scene.h>
#include <kalahari/mindmap/mind_map_view.h>

#include "kalahari_tests/test_support/window_parts.h"

#include <QApplication>
#include <QKeyEvent>
#include <QPointer>
#include <QTemporaryDir>
#include <QWidget>

#include <memory>

using namespace kalahari::mindmap;
using kalahari::core::BookPlace;
using kalahari::core::KindRef;
using kalahari::core::ProjectManager;

namespace {

const QString CHARACTER = QStringLiteral("kalahari.base:character");
const QString LOCATION = QStringLiteral("kalahari.base:location");
const QString ITEM = QStringLiteral("kalahari.base:item");

/// A card of kind @p kindId titled @p title in the Workshop of @p book
QString addCard(ProjectManager& book, const char* kindId, const QString& title,
                const QString& gender = QString()) {
    const KindRef kind = book.bookTypes().findKind(QStringLiteral("kalahari.base"),
                                                   QString::fromLatin1(kindId));
    REQUIRE(kind);
    const QString id = book.addElement(kind, title, BookPlace::Workshop);
    REQUIRE_FALSE(id.isEmpty());
    if (!gender.isEmpty()) {
        REQUIRE(book.setCardValue(id, QStringLiteral("gender"), gender));
    }
    return id;
}

/// The options of a map of the characters, their places and their items
RelationMapOptions allKinds() {
    RelationMapOptions options;
    options.kinds = {CHARACTER, LOCATION, ITEM};
    return options;
}

}  // namespace

TEST_CASE("Mind map of relations: the cards with their relations, in generations",
          "[mindmap][relations][gui]") {
    registerMindMapModule();
    const std::shared_ptr<kalahari::core::IWindowEditorProvider> provider =
        kalahari::core::windowEditorProvider(QStringLiteral("mindmap"));
    REQUIRE(provider != nullptr);

    const QTemporaryDir dir;
    auto& book = kalahari::test::book();
    ProjectManager::loadBookTypes({QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(book.createProject(dir.path(), "Karawana", "Anna", "pl", true, "kalahari.novel"));
    const kalahari::core::WindowEditorContext context{book, kalahari::test::commands(), {}, {}, {}};

    const QString amara = addCard(book, "character", QStringLiteral("Amara"),
                                  QStringLiteral("woman"));
    const QString selim = addCard(book, "character", QStringLiteral("Dziadek Selim"),
                                  QStringLiteral("man"));
    const QString jusuf = addCard(book, "character", QStringLiteral("Jusuf"), QStringLiteral("man"));
    const QString idris = addCard(book, "character", QStringLiteral("Idris"), QStringLiteral("man"));
    const QString well = addCard(book, "location", QStringLiteral("Studnia Trzech Palm"));
    const QString compass = addCard(book, "item", QStringLiteral("Kompas"));
    const QString map = addCard(book, "item", QStringLiteral("Mapa"));
    REQUIRE(book.setCardValue(amara, QStringLiteral("role"), QStringLiteral("main")));

    // The first relation of a card: the offer of a map of relations, unless the writer said no
    QWidget parent;
    REQUIRE(book.addRelation(amara, selim, QStringLiteral("parent"), QString()));
    {
        const QPointer<RelationMapOffer> offer = offerRelationMap(context, amara, selim,
                                                                  QStringLiteral("parent"),
                                                                  &parent);
        REQUIRE(offer != nullptr);
        CHECK(offer->createButton() != nullptr);
        delete offer.data();
        auto& settings = kalahari::core::SettingsManager::getInstance();
        settings.set<bool>(MindMapSettings::ASK_RELATION_MAP, false);
        CHECK(offerRelationMap(context, amara, selim, QStringLiteral("parent"), &parent) == nullptr);
        settings.set<bool>(MindMapSettings::ASK_RELATION_MAP, true);
    }
    REQUIRE(book.addRelation(jusuf, amara, QStringLiteral("friend"), QStringLiteral("przewodnik")));
    REQUIRE(book.addRelation(idris, selim, QStringLiteral("colleague"), QString()));
    REQUIRE(book.addRelation(selim, well, QStringLiteral("visits"), QString()));
    REQUIRE(book.addRelation(selim, compass, QStringLiteral("property"), QString()));
    // Not the first relation of Amara's card: nothing offered
    CHECK(offerRelationMap(context, amara, jusuf, QStringLiteral("friend"), &parent) == nullptr);

    // The cards with relations among the kinds; a card without them stays out
    const QStringList cards = relationMapCards(book, allKinds());
    CHECK(cards.size() == 6);
    CHECK_FALSE(cards.contains(map));
    RelationMapOptions characters;
    characters.kinds = {CHARACTER};
    CHECK(relationMapCards(book, characters) == (QStringList{amara, selim, jusuf, idris}));
    characters.family = false;
    characters.others = false;
    CHECK(relationMapCards(book, characters).isEmpty());

    // The module's command in the Book menu and in the Navigator
    const std::vector<kalahari::core::WindowEditorCommand> commands = provider->commands();
    REQUIRE(commands.size() == 1);
    CHECK(commands.front().id == MindMapModule::NEW_RELATION_MAP);
    CHECK(commands.front().run != nullptr);

    // The map made in the Workshop, with a node of each card
    const QString id = createRelationMap(book, QStringLiteral("Relacje postaci"), allKinds());
    REQUIRE_FALSE(id.isEmpty());
    {
        const kalahari::core::ProjectElement* element = book.findElement(id);
        REQUIRE(element != nullptr);
        MindMap made;
        REQUIRE(made.load(book.filePathOf(*element)));
        REQUIRE(made.relationMap().has_value());
        CHECK(made.relationMap()->kinds == allKinds().kinds);
        CHECK(made.size() == 7);  // The main topic and the six cards
        CHECK_FALSE(made.showsRoot());
        for (const QString& card : cards) {
            CHECK_FALSE(made.nodeOfElement(card).isEmpty());
        }
    }
    // The book has a map of the characters now: no more offers
    const QString nur = addCard(book, "character", QStringLiteral("Nur"), QStringLiteral("woman"));
    REQUIRE(book.addRelation(nur, amara, QStringLiteral("sibling"), QString()));
    CHECK(offerRelationMap(context, nur, amara, QStringLiteral("sibling"), &parent) == nullptr);

    // Open: the cards without the main topic, placed in generations, the relations drawn
    const kalahari::core::WindowEditorContext mapContext{
        book, kalahari::test::commands(), id, {}, {}};
    std::unique_ptr<QWidget> tab(provider->createEditor(mapContext, nullptr));
    auto* editor = qobject_cast<MindMapEditor*>(tab.get());
    REQUIRE(editor != nullptr);
    editor->resize(1200, 760);
    editor->show();
    QApplication::processEvents();
    MindMapDocument& document = editor->document();
    MindMapView* view = editor->view();
    REQUIRE(view->scene() != nullptr);
    const QString root = document.map().root();
    CHECK(view->scene()->box(root) == nullptr);
    CHECK_FALSE(document.map().hasNodesWithoutPlace());
    CHECK(document.selection().isEmpty());
    CHECK_FALSE(editor->infoText().isEmpty());
    // Nur came with the relation she has with Amara, as the map opened
    CHECK_FALSE(document.map().nodeOfElement(nur).isEmpty());
    const auto boxOf = [&](const QString& card) {
        const NodeBox* box = view->scene()->box(document.map().nodeOfElement(card));
        REQUIRE(box != nullptr);
        return box->rect;
    };
    CHECK(boxOf(selim).left() < boxOf(amara).left());  // The grandfather a generation before
    CHECK(boxOf(amara).left() < boxOf(well).left());   // The places after the characters
    const qsizetype relations = std::count_if(
        view->scene()->links().cbegin(), view->scene()->links().cend(),
        [](const LinkLine& link) { return link.type == LinkLine::Type::Relation; });
    CHECK(relations >= 6);

    // A card node selected: the line under the map says what it is (after the message of the
    // cards that came to the map)
    document.select(document.map().nodeOfElement(amara));
    CHECK(view->hint().contains(QStringLiteral("Amara")));

    if (const QByteArray shot = qgetenv("KALAHARI_RELATION_MAP_SHOT"); !shot.isEmpty()) {
        // A picture of the tab, to look at it (not checked)
        view->showAll(false);
        QApplication::processEvents();
        editor->grab().save(QString::fromLocal8Bit(shot));
    }

    // Tab with nothing selected: a loose node, as the main topic is not drawn
    document.clearSelection();
    QKeyEvent tabKey(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier);
    QApplication::sendEvent(view, &tabKey);
    const QString added = document.selected();
    REQUIRE_FALSE(added.isEmpty());
    CHECK(document.map().parentOf(added).isEmpty());
    CHECK(view->scene()->box(root) == nullptr);

    tab.reset();
    REQUIRE(book.closeProject(false));
    kalahari::core::ExtensionPointRegistry::getInstance().unregisterPlugin("kalahari.mindmaps");
}
