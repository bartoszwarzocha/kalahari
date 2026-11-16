// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_mind_map.cpp
/// @brief A mind map of the Workshop: its tree, loose nodes and connections, its changes, its
/// outlines, its file and its layouts

#include <catch2/catch_test_macros.hpp>
#include <kalahari/mindmap/mind_map.h>
#include <kalahari/mindmap/mind_map_layout.h>

#include <QJsonArray>
#include <QJsonObject>
#include <QSizeF>
#include <QTemporaryDir>

using namespace kalahari::mindmap;

namespace {

/// The text a node shows: its own, else "<title>" of the map
QString shownText(const MindMapNode& node) {
    return node.text.isEmpty() ? QStringLiteral("Wątki") : node.text;
}

/// A map: the main topic with "Kompas" (two nodes inside) and "Idris", and a loose node
struct Threads {
    MindMap map;
    QString compass;
    QString name;
    QString wearer;
    QString idris;
    QString loose;

    Threads() {
        compass = map.addChild(map.root(), QStringLiteral("Kompas"));
        name = map.addChild(compass, QStringLiteral("Imię Selima na kopercie"));
        wearer = map.addChild(compass, QStringLiteral("Kto go teraz nosi?"));
        idris = map.addChild(map.root(), QStringLiteral("Idris"));
        loose = map.addLoose(QStringLiteral("Sprawdzić burze"), QPointF(300, -200));
    }
};

QStringList textsOf(const MindMap& map, const QStringList& ids) {
    QStringList texts;
    for (const QString& id : ids) {
        texts.append(map.node(id)->text);
    }
    return texts;
}

/// Every node is 100 x 20
QSizeF fixedSize(const MindMapNode& /*node*/, int /*depth*/) {
    return QSizeF(100, 20);
}

}  // namespace

TEST_CASE("A mind map is a tree from its main topic, with loose nodes", "[mind_map]") {
    Threads threads;
    MindMap& map = threads.map;
    CHECK(map.size() == 6);
    CHECK(map.nodeIds() == QStringList{map.root(), threads.compass, threads.name, threads.wearer,
                                       threads.idris, threads.loose});
    CHECK(map.parentOf(threads.name) == threads.compass);
    CHECK(map.parentOf(threads.loose).isEmpty());
    CHECK(map.depthOf(map.root()) == 0);
    CHECK(map.depthOf(threads.compass) == 1);
    CHECK(map.depthOf(threads.wearer) == 2);
    CHECK(map.depthOf(threads.loose) == 1);
    CHECK(map.branchOf(threads.wearer) == threads.compass);
    CHECK(map.branchOf(map.root()).isEmpty());
    CHECK(map.isInLooseBranch(map.addChild(threads.loose, QStringLiteral("W kwietniu?"))));
    CHECK(map.descendantsOf(threads.compass) == QStringList{threads.name, threads.wearer});
}

TEST_CASE("Nodes of a mind map change places", "[mind_map]") {
    Threads threads;
    MindMap& map = threads.map;

    SECTION("A node beside another") {
        const QString before = map.addSibling(threads.wearer, QStringLiteral("Zegarmistrz"), true);
        CHECK(textsOf(map, map.node(threads.compass)->children) ==
              QStringList{"Imię Selima na kopercie", "Zegarmistrz", "Kto go teraz nosi?"});
        // Beside the main topic is inside it, beside a loose node is loose
        CHECK(map.parentOf(map.addSibling(map.root(), QStringLiteral("Motywy"))) == map.root());
        const QString loose = map.addSibling(threads.loose, QStringLiteral("I w maju?"));
        CHECK(map.looseNodes() == QStringList{threads.loose, loose});
        CHECK(map.node(loose)->loosePlace == QPointF(300, -154));
        CHECK_FALSE(before.isEmpty());
    }
    SECTION("A branch moves, but not into itself") {
        CHECK_FALSE(map.move(threads.compass, threads.name, 0));
        CHECK_FALSE(map.move(map.root(), threads.idris, 0));
        REQUIRE(map.move(threads.compass, threads.idris, 0));
        CHECK(map.parentOf(threads.compass) == threads.idris);
        CHECK(map.depthOf(threads.wearer) == 3);
        // Within its own list, a place counts without the node itself
        REQUIRE(map.move(threads.name, threads.compass, 2));
        CHECK(map.node(threads.compass)->children == QStringList{threads.wearer, threads.name});
    }
    SECTION("A loose node joins the tree, and a branch becomes loose") {
        REQUIRE(map.move(threads.loose, threads.idris, -1));
        CHECK(map.looseNodes().isEmpty());
        CHECK_FALSE(map.node(threads.loose)->loosePlace.has_value());
        REQUIRE(map.makeLoose(threads.compass, QPointF(10, 20)));
        CHECK(map.looseNodes() == QStringList{threads.compass});
        CHECK(map.parentOf(threads.compass).isEmpty());
        CHECK(map.node(map.root())->children == QStringList{threads.idris});
        CHECK(map.isInLooseBranch(threads.wearer));
    }
    SECTION("Up and down among its siblings, in and out of a level") {
        CHECK(map.moveAmongSiblings(threads.idris, -1));
        CHECK(map.node(map.root())->children == QStringList{threads.idris, threads.compass});
        CHECK_FALSE(map.moveAmongSiblings(threads.idris, -1));
        CHECK_FALSE(map.indent(threads.idris));
        CHECK(map.indent(threads.compass));
        CHECK(map.parentOf(threads.compass) == threads.idris);
        CHECK(map.outdent(threads.compass));
        CHECK(map.node(map.root())->children == QStringList{threads.idris, threads.compass});
        CHECK_FALSE(map.outdent(threads.compass));
    }
    SECTION("A removed branch takes its nodes and their connections") {
        REQUIRE(map.connect(threads.name, threads.idris, QStringLiteral("ta sama wyprawa")));
        REQUIRE(map.connect(threads.idris, threads.loose));
        CHECK(map.remove(map.root()).isEmpty());
        CHECK(map.remove(threads.compass) ==
              QStringList{threads.compass, threads.name, threads.wearer});
        CHECK(map.size() == 3);
        REQUIRE(map.connections().size() == 1);
        CHECK(map.connections().first().from == threads.idris);
    }
}

TEST_CASE("Connections of a mind map", "[mind_map]") {
    Threads threads;
    MindMap& map = threads.map;
    CHECK(map.connect(threads.name, threads.idris, QStringLiteral("ta sama wyprawa")));
    CHECK_FALSE(map.connect(threads.idris, threads.name));
    CHECK_FALSE(map.connect(threads.idris, threads.idris));
    CHECK_FALSE(map.connect(threads.idris, QStringLiteral("none")));
    MindMapConnection turned = map.connections().first();
    std::swap(turned.from, turned.to);
    CHECK(map.setConnection(0, turned));
    CHECK(map.connections().first().from == threads.idris);
    CHECK(map.disconnect(0));
    CHECK(map.connections().isEmpty());
}

TEST_CASE("A node keeps its children when it changes", "[mind_map]") {
    Threads threads;
    MindMap& map = threads.map;
    MindMapNode compass = *map.node(threads.compass);
    compass.text = QStringLiteral("Kompas Selima");
    compass.markers = {QString::fromLatin1(MindMapNode::QUESTION)};
    compass.children.clear();
    compass.loosePlace = QPointF(1, 1);
    REQUIRE(map.setNode(compass));
    CHECK(map.node(threads.compass)->text == QStringLiteral("Kompas Selima"));
    CHECK(map.node(threads.compass)->children.size() == 2);
    CHECK_FALSE(map.node(threads.compass)->loosePlace.has_value());

    MindMapNode loose = *map.node(threads.loose);
    loose.loosePlace.reset();
    REQUIRE(map.setNode(loose));
    CHECK(map.node(threads.loose)->loosePlace == QPointF(300, -200));
}

TEST_CASE("A mind map forgets an element that is gone", "[mind_map]") {
    Threads threads;
    MindMap& map = threads.map;
    MindMapNode idris = *map.node(threads.idris);
    idris.text.clear();
    idris.link = QStringLiteral("card-idris");
    REQUIRE(map.setNode(idris));
    CHECK(map.forgetElement(QStringLiteral("card-idris"), QStringLiteral("Idris")));
    CHECK(map.node(threads.idris)->link.isEmpty());
    CHECK(map.node(threads.idris)->text == QStringLiteral("Idris"));
    CHECK_FALSE(map.forgetElement(QStringLiteral("card-idris"), QStringLiteral("Idris")));
}

TEST_CASE("A branch of a mind map as an outline, and an outline as a branch", "[mind_map]") {
    Threads threads;
    MindMap& map = threads.map;
    CHECK(map.outlineOf(map.root(), shownText) ==
          QStringLiteral("Wątki\n\tKompas\n\t\tImię Selima na kopercie\n\t\tKto go teraz "
                         "nosi?\n\tIdris"));
    CHECK(map.outlineOf(threads.idris, shownText) == QStringLiteral("Idris"));

    const QString outline = QStringLiteral(
        "- Wygląd\n"
        "    * blizna na dłoni\n"
        "    * zawsze w kapeluszu\n"
        "\n"
        "2. Charakter\r\n"
        "\tuparta\n"
        "\t\tod dziecka\n"
        "Cel");
    const QStringList added = map.addOutline(map.root(), outline, 1);
    CHECK(textsOf(map, added) == QStringList{"Wygląd", "Charakter", "Cel"});
    CHECK(textsOf(map, map.node(map.root())->children) ==
          QStringList{"Kompas", "Wygląd", "Charakter", "Cel", "Idris"});
    CHECK(textsOf(map, map.node(added.at(0))->children) ==
          QStringList{"blizna na dłoni", "zawsze w kapeluszu"});
    const QString stubborn = map.node(added.at(1))->children.first();
    CHECK(textsOf(map, map.node(stubborn)->children) == QStringList{"od dziecka"});
    CHECK(map.addOutline(QStringLiteral("none"), outline).isEmpty());
}

TEST_CASE("A mind map file keeps all it has", "[mind_map]") {
    Threads threads;
    MindMap& map = threads.map;
    MindMapNode wearer = *map.node(threads.wearer);
    wearer.markers = {QString::fromLatin1(MindMapNode::QUESTION)};
    wearer.note = QStringLiteral("Może Idris?");
    wearer.link = QStringLiteral("chapter-2");
    wearer.color = QStringLiteral("amber");
    wearer.freePlace = QPointF(250, 40);
    REQUIRE(map.setNode(wearer));
    MindMapNode compass = *map.node(threads.compass);
    compass.collapsed = true;
    REQUIRE(map.setNode(compass));
    REQUIRE(map.connect(threads.name, threads.idris, QStringLiteral("ta sama wyprawa")));
    map.setLayout(MindMapLayout::Down);
    map.setMode(MindMapMode::List);
    map.setZoom(1.5);

    const QJsonObject json = map.toJson();
    CHECK(json.value(QStringLiteral("format")).toInt() == MindMap::FORMAT);
    CHECK(json.value(QStringLiteral("view")).toObject().value(QStringLiteral("layout")) ==
          QStringLiteral("down"));
    MindMap read;
    REQUIRE(read.fromJson(json));
    CHECK(read == map);

    SECTION("Fields this version does not know are saved back") {
        QJsonObject later = json;
        QJsonArray nodes = later.value(QStringLiteral("nodes")).toArray();
        QJsonObject root = nodes.at(0).toObject();
        root.insert(QStringLiteral("shape"), QStringLiteral("cloud"));
        nodes.replace(0, root);
        later.insert(QStringLiteral("nodes"), nodes);
        later.insert(QStringLiteral("theme"), QStringLiteral("sand"));
        MindMap kept;
        REQUIRE(kept.fromJson(later));
        const QJsonObject again = kept.toJson();
        CHECK(again.value(QStringLiteral("theme")) == QStringLiteral("sand"));
        CHECK(again.value(QStringLiteral("nodes")).toArray().at(0).toObject().value(
                  QStringLiteral("shape")) == QStringLiteral("cloud"));
    }
    SECTION("A broken tree is mended") {
        QJsonObject broken = json;
        QJsonArray nodes = broken.value(QStringLiteral("nodes")).toArray();
        // The last node names the main topic and an unknown node as its children, and a
        // node no parent reaches becomes loose
        QJsonObject last = nodes.last().toObject();
        last.insert(QStringLiteral("children"),
                    QJsonArray{read.root(), QStringLiteral("ghost")});
        nodes.replace(nodes.size() - 1, last);
        nodes.append(QJsonObject{{QStringLiteral("id"), QStringLiteral("orphan")},
                                 {QStringLiteral("text"), QStringLiteral("Sierota")}});
        broken.insert(QStringLiteral("nodes"), nodes);
        MindMap mended;
        REQUIRE(mended.fromJson(broken));
        CHECK(mended.node(last.value(QStringLiteral("id")).toString())->children.isEmpty());
        CHECK(mended.looseNodes().contains(QStringLiteral("orphan")));
        CHECK(mended.node(QStringLiteral("orphan"))->loosePlace.has_value());
    }
    SECTION("A map without its main topic, or of a later format, is not read") {
        QJsonObject rootless = json;
        rootless.insert(QStringLiteral("root"), QStringLiteral("ghost"));
        MindMap notRead;
        QString error;
        CHECK_FALSE(notRead.fromJson(rootless, &error));
        CHECK_FALSE(error.isEmpty());
        QJsonObject later = json;
        later.insert(QStringLiteral("format"), MindMap::FORMAT + 1);
        CHECK_FALSE(notRead.fromJson(later));
        CHECK(notRead.size() == 1);
    }
    SECTION("On disk") {
        QTemporaryDir folder;
        REQUIRE(folder.isValid());
        const QString path = folder.filePath(QStringLiteral("mindmap_001.kmap"));
        REQUIRE(map.save(path));
        MindMap loaded;
        REQUIRE(loaded.load(path));
        CHECK(loaded == map);
    }
}

TEST_CASE("The layouts of a mind map", "[mind_map]") {
    Threads threads;
    MindMap& map = threads.map;
    const QString motifs = map.addChild(map.root(), QStringLiteral("Motywy"));

    SECTION("On both sides: the first half of the main branches right, the rest left") {
        const auto places = layOutMindMap(map, MindMapLayout::BothSides, fixedSize);
        CHECK(places.value(map.root()).centre == QPointF(0, 0));
        CHECK(places.value(threads.compass).side == 1);
        CHECK(places.value(threads.idris).side == 1);
        CHECK(places.value(motifs).side == -1);
        CHECK(places.value(threads.compass).centre.x() > 0);
        CHECK(places.value(motifs).centre.x() < 0);
        // The nodes inside a branch stand further out, one under another
        CHECK(places.value(threads.name).centre.x() > places.value(threads.compass).centre.x());
        CHECK(places.value(threads.name).centre.y() < places.value(threads.wearer).centre.y());
        CHECK(places.value(threads.wearer).depth == 2);
        // The loose node stands at its place
        CHECK(places.value(threads.loose).centre == QPointF(300, -200));
        CHECK(places.value(threads.loose).loose);
    }
    SECTION("To the right and down") {
        const auto right = layOutMindMap(map, MindMapLayout::Right, fixedSize);
        CHECK(right.value(motifs).side == 1);
        const auto down = layOutMindMap(map, MindMapLayout::Down, fixedSize);
        CHECK(down.value(threads.compass).centre.y() > down.value(map.root()).centre.y());
        CHECK(down.value(threads.compass).centre.x() < down.value(motifs).centre.x());
        CHECK(down.value(threads.compass).side == 0);
    }
    SECTION("A collapsed node hides its nodes") {
        MindMapNode compass = *map.node(threads.compass);
        compass.collapsed = true;
        REQUIRE(map.setNode(compass));
        const auto places = layOutMindMap(map, MindMapLayout::BothSides, fixedSize);
        CHECK_FALSE(places.contains(threads.name));
        CHECK(places.contains(threads.compass));
    }
    SECTION("Free: the writer's places, else places near those on both sides, kept") {
        MindMapNode idris = *map.node(threads.idris);
        idris.freePlace = QPointF(-500, 500);
        REQUIRE(map.setNode(idris));
        const auto places = layOutMindMap(map, MindMapLayout::Free, fixedSize);
        CHECK(places.value(threads.idris).centre == QPointF(-500, 500));
        CHECK(places.value(map.root()).centre == QPointF(0, 0));
        const auto both = layOutMindMap(map, MindMapLayout::BothSides, fixedSize);
        CHECK(places.value(threads.compass).centre.x() > both.value(threads.compass).centre.x());
        // The same each time
        CHECK(layOutMindMap(map, MindMapLayout::Free, fixedSize) == places);
        QHash<QString, QPointF> centres;
        for (auto it = places.cbegin(); it != places.cend(); ++it) {
            centres.insert(it.key(), it->centre);
        }
        map.fixFreePlaces(centres);
        CHECK(map.node(threads.compass)->freePlace == places.value(threads.compass).centre);
        CHECK(map.node(threads.idris)->freePlace == QPointF(-500, 500));
    }
}
