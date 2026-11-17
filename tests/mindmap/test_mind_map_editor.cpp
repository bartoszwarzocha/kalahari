// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_mind_map_editor.cpp
/// @brief The editor of mind maps: the map open on its tab with Undo, its drawing, the map and
/// the list worked with the keys, and the module the program knows

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_registry.h>
#include <kalahari/core/extension_points.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/window_editor_provider.h>
#include <kalahari/gui/settings/settings_page.h>
#include <kalahari/mindmap/mind_map.h>
#include <kalahari/mindmap/mind_map_document.h>
#include <kalahari/mindmap/mind_map_editor.h>
#include <kalahari/mindmap/mind_map_list.h>
#include <kalahari/mindmap/mind_map_look.h>
#include <kalahari/mindmap/mind_map_module.h>
#include <kalahari/mindmap/mind_map_scene.h>
#include <kalahari/mindmap/mind_map_view.h>

#include "kalahari_tests/test_support/window_parts.h"

#include <QApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QTemporaryDir>

#include <memory>

using namespace kalahari::mindmap;

namespace {

/// A key press sent to a widget
void press(QWidget* widget, int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier,
           const QString& text = QString()) {
    QKeyEvent event(QEvent::KeyPress, key, modifiers, text);
    QApplication::sendEvent(widget, &event);
}

/// Whether a widget with the keys keeps a key from the window's shortcuts
bool keepsFromShortcuts(QWidget* widget, int key, Qt::KeyboardModifiers modifiers) {
    QKeyEvent event(QEvent::ShortcutOverride, key, modifiers);
    event.ignore();
    QApplication::sendEvent(widget, &event);
    return event.isAccepted();
}

/// A map open on its tab, its file in a folder of its own; the main topic shows "Wątki"
struct OpenMap {
    QTemporaryDir dir;
    MindMapDocument document{dir.filePath(QStringLiteral("map.kmap"))};

    OpenMap() {
        document.setShownText([this](const MindMapNode& node) {
            return node.text.isEmpty() && node.id == document.map().root()
                       ? QStringLiteral("Wątki")
                       : node.text;
        });
        document.load();
    }

    /// Node @p text inside @p parent
    QString add(const QString& parent, const QString& text) {
        const QString id = document.addChild(parent);
        document.setText(id, text);
        return id;
    }

    [[nodiscard]] QString textOf(const QString& id) const {
        const MindMapNode* node = document.map().node(id);
        return node != nullptr ? node->text : QString();
    }
};

}  // namespace

TEST_CASE("Mind map document: each change is a step of Undo, saved to the file at once",
          "[mindmap][document]") {
    OpenMap open;
    MindMapDocument& document = open.document;
    const QString root = document.map().root();
    CHECK(document.selected() == root);
    CHECK_FALSE(document.canUndo());

    int changes = 0;
    QObject::connect(&document, &MindMapDocument::changed, [&changes]() { ++changes; });
    const QString compass = document.addChild(root);
    REQUIRE_FALSE(compass.isEmpty());
    CHECK(document.selected() == compass);
    CHECK(changes == 1);
    CHECK(document.undoText() == QStringLiteral("adding a node"));
    CHECK(QFileInfo::exists(document.path()));

    REQUIRE(document.setText(compass, QStringLiteral("Kompas")));
    MindMap saved;
    REQUIRE(saved.load(document.path()));
    CHECK(saved.node(compass)->text == QStringLiteral("Kompas"));

    // A change that changes nothing, or says no, is no step
    CHECK_FALSE(document.setText(compass, QStringLiteral("Kompas")));
    CHECK_FALSE(document.change(QStringLiteral("nothing"), [](MindMap& map) {
        map.addChild(map.root(), QStringLiteral("gone"));
        return false;
    }));
    CHECK(document.map().size() == 2);

    document.undo();
    CHECK(open.textOf(compass).isEmpty());
    CHECK(document.canRedo());
    document.redo();
    CHECK(open.textOf(compass) == QStringLiteral("Kompas"));

    // The text the main topic would show anyway leaves it empty, so that it follows the title
    REQUIRE_FALSE(document.setText(root, QStringLiteral("Wątki")));
    CHECK(open.textOf(root).isEmpty());

    // The view is saved, but it is no step of Undo
    const QString before = document.undoText();
    document.changeView([](MindMap& map) { map.setZoom(1.5); });
    CHECK(document.undoText() == before);
    REQUIRE(saved.load(document.path()));
    CHECK(saved.zoom() == 1.5);
}

TEST_CASE("Mind map scene: each node that shows has its box, and the mouse finds it",
          "[mindmap][scene]") {
    MindMap map;
    const QString compass = map.addChild(map.root(), QStringLiteral("Kompas"));
    const QString wearer = map.addChild(compass, QStringLiteral("Kto go teraz nosi?"));
    const QString idris = map.addChild(map.root(), QStringLiteral("Idris"));
    map.connect(wearer, idris, QStringLiteral("ta sama wyprawa"));
    const MindMapLook look;
    const auto shown = [](const MindMapNode& node) {
        return node.text.isEmpty() ? QStringLiteral("Wątki") : node.text;
    };

    {
        const MindMapScene scene(map, look, QFont(), shown);
        REQUIRE(scene.nodes().size() == 4);
        CHECK(scene.lines().size() == 3);
        REQUIRE(scene.connections().size() == 1);
        for (const NodeBox& box : scene.nodes()) {
            CHECK(scene.nodeAt(box.rect.center()) == box.id);
            CHECK(box.rect.width() > 0.0);
        }
        CHECK(scene.box(map.root())->root);
        CHECK(scene.box(map.root())->rect.center() == QPointF(0.0, 0.0));
        // The connection is found at its label
        const QRectF label = scene.labelRect(0, map.connections().front().label);
        CHECK(scene.connectionAt(label.center()) == 0);
        // The arrows go to the nearest node that way
        CHECK_FALSE(scene.nodeToward(map.root(), 1, 0).isEmpty());
    }

    // A collapsed node hides its nodes and shows how many on a badge
    MindMapNode collapsed = *map.node(compass);
    collapsed.collapsed = true;
    map.setNode(collapsed);
    const MindMapScene scene(map, look, QFont(), shown);
    CHECK(scene.box(wearer) == nullptr);
    const NodeBox* box = scene.box(compass);
    REQUIRE(box != nullptr);
    CHECK(box->hidden == 1);
    CHECK(scene.badgeAt(box->badge) == compass);
    CHECK(scene.connections().isEmpty());
}

TEST_CASE("Mind map view: the keys add, change and delete nodes", "[mindmap][view][gui]") {
    OpenMap open;
    MindMapDocument& document = open.document;
    const QString root = document.map().root();
    MindMapView view(document);
    view.resize(800, 600);
    QStringList told;
    QObject::connect(&view, &MindMapView::told,
                     [&told](const QString& message, bool) { told.append(message); });

    // The keys of the map go to it before the commands of the window
    CHECK(keepsFromShortcuts(&view, Qt::Key_Delete, Qt::NoModifier));
    CHECK(keepsFromShortcuts(&view, Qt::Key_Z, Qt::ControlModifier));
    CHECK(keepsFromShortcuts(&view, Qt::Key_K, Qt::NoModifier) == false);

    // Tab adds a node inside the selected one and changes its text in its place
    press(&view, Qt::Key_Tab);
    REQUIRE(view.isEditing());
    const QString compass = document.selected();
    REQUIRE(compass != root);
    auto* field = view.findChild<QPlainTextEdit*>();
    REQUIRE(field != nullptr);
    field->setPlainText(QStringLiteral("Kompas"));
    press(field, Qt::Key_Return);
    CHECK_FALSE(view.isEditing());
    CHECK(open.textOf(compass) == QStringLiteral("Kompas"));

    // A new node left empty goes
    const qsizetype nodes = document.map().size();
    press(&view, Qt::Key_Return);
    REQUIRE(view.isEditing());
    CHECK(document.map().size() == nodes + 1);
    press(field, Qt::Key_Escape);
    CHECK(document.map().size() == nodes);
    CHECK(document.selected() == compass);

    // Typing changes the text, the sign typed first
    press(&view, Qt::Key_K, Qt::NoModifier, QStringLiteral("K"));
    REQUIRE(view.isEditing());
    CHECK(field->toPlainText() == QStringLiteral("K"));
    press(field, Qt::Key_Escape);
    CHECK(open.textOf(compass) == QStringLiteral("Kompas"));

    // Delete removes the node, Ctrl+Z brings it back; the main topic stays
    press(&view, Qt::Key_Delete);
    CHECK(document.map().node(compass) == nullptr);
    CHECK(document.selected() == root);
    REQUIRE_FALSE(told.isEmpty());
    CHECK(told.last().contains(QStringLiteral("Kompas")));
    press(&view, Qt::Key_Z, Qt::ControlModifier);
    CHECK(document.map().node(compass) != nullptr);
    press(&view, Qt::Key_Home);
    CHECK(document.selected() == root);
    press(&view, Qt::Key_Delete);
    CHECK(document.map().node(root) != nullptr);

    // Space collapses a branch with nodes inside it
    document.select(compass);
    const QString inside = open.add(compass, QStringLiteral("Kto go teraz nosi?"));
    document.select(compass);
    press(&view, Qt::Key_Space);
    CHECK(document.map().node(compass)->collapsed);
    CHECK(view.scene()->box(inside) == nullptr);

    // The zoom goes by its steps
    view.resetZoom();
    view.zoomBy(1);
    CHECK(document.map().zoom() > 1.0);
    press(&view, Qt::Key_0, Qt::ControlModifier);
    CHECK(document.map().zoom() == 1.0);
}

TEST_CASE("Mind map list: Tab and Shift+Tab put a node a level down and up",
          "[mindmap][list][gui]") {
    OpenMap open;
    MindMapDocument& document = open.document;
    const QString root = document.map().root();
    const QString compass = open.add(root, QStringLiteral("Kompas"));
    const QString idris = open.add(root, QStringLiteral("Idris"));
    MindMapList list(document);
    list.resize(500, 400);
    QApplication::processEvents();
    CHECK(list.topLevelItemCount() == 1);
    CHECK(list.topLevelItem(0)->childCount() == 2);

    document.select(idris);
    press(&list, Qt::Key_Tab);
    CHECK(document.map().parentOf(idris) == compass);
    press(&list, Qt::Key_Backtab);
    CHECK(document.map().parentOf(idris) == root);
    // The first node has none before it to go into
    document.select(compass);
    press(&list, Qt::Key_Tab);
    CHECK(document.map().parentOf(compass) == root);

    // Ctrl+Down moves it among the nodes beside it
    press(&list, Qt::Key_Down, Qt::ControlModifier);
    CHECK(document.map().node(root)->children.indexOf(compass) == 1);
    QApplication::processEvents();
    CHECK(list.topLevelItem(0)->child(1)->data(0, MindMapList::ID_ROLE).toString() == compass);
}

TEST_CASE("Mind map module: the program finds its editor, its new files and its settings",
          "[mindmap][module]") {
    registerMindMapModule();
    const std::shared_ptr<kalahari::core::IWindowEditorProvider> provider =
        kalahari::core::windowEditorProvider(QStringLiteral("mindmap"));
    REQUIRE(provider != nullptr);
    CHECK(provider->fileSuffix() == QStringLiteral("kmap"));
    CHECK_FALSE(provider->settingsTitle().isEmpty());

    MindMap map;
    REQUIRE(map.fromJson(QJsonDocument::fromJson(provider->newFile()).object()));
    CHECK(map.size() == 1);
    CHECK(map.layout() == MindMapLook::newLayout());

    const std::unique_ptr<QWidget> page(provider->createSettingsPage());
    CHECK(qobject_cast<kalahari::gui::SettingsPage*>(page.get()) != nullptr);

    // A mind map added to the Workshop gets its file from the module, and opens on its tab
    const QTemporaryDir dir;
    auto& book = kalahari::test::book();
    kalahari::core::ProjectManager::loadBookTypes(
        {QStringLiteral(KALAHARI_SOURCE_DIR "/resources/booktypes")});
    REQUIRE(book.createProject(dir.path(), "Maps", "Anna", "pl", true, "kalahari.novel"));
    const kalahari::core::KindRef kind = book.bookTypes().findKind(
        QStringLiteral("kalahari.base"), QStringLiteral("mindmap"));
    REQUIRE(kind);
    CHECK(kalahari::core::ProjectManager::canAdd(kind));
    const QString id = book.addElement(kind, "Wątki", kalahari::core::BookPlace::Workshop);
    REQUIRE_FALSE(id.isEmpty());
    const kalahari::core::ProjectElement* element = book.findElement(id);
    REQUIRE(element != nullptr);
    CHECK(element->file.endsWith(QStringLiteral(".kmap")));
    MindMap added;
    REQUIRE(added.load(book.filePathOf(*element)));

    const kalahari::core::WindowEditorContext context{book, kalahari::test::commands(), id, {},
                                                      {}};
    const std::unique_ptr<QWidget> tab(provider->createEditor(context, nullptr));
    auto* editor = qobject_cast<MindMapEditor*>(tab.get());
    REQUIRE(editor != nullptr);
    CHECK(editor->document().shownText(editor->document().map().root()) ==
          QStringLiteral("Wątki"));
    if (const QByteArray shot = qgetenv("KALAHARI_MIND_MAP_SHOT"); !shot.isEmpty()) {
        // A picture of the tab, to look at it (not checked)
        MindMapDocument& document = editor->document();
        const QString root = document.map().root();
        const QString compass = document.addChild(root);
        document.setText(compass, QStringLiteral("Kompas"));
        document.setText(document.addChild(compass), QStringLiteral("Imię Selima na kopercie"));
        const QString wearer = document.addChild(compass);
        document.setText(wearer, QStringLiteral("Kto go teraz nosi?"));
        document.toggleMarker(wearer, QStringLiteral("question"));
        document.setNote(wearer, QStringLiteral("Może Idris?"));
        const QString idris = document.addChild(root);
        document.setText(idris, QStringLiteral("Idris"));
        document.setText(document.addChild(idris), QStringLiteral("Na co mu pieniądze?"));
        const QString well = document.addChild(root);
        document.setText(well, QStringLiteral("Studnia Trzech Palm"));
        document.toggleMarker(well, QStringLiteral("important"));
        const QString amara = document.addChild(root);
        document.setText(amara, QStringLiteral("Amara"));
        document.setText(document.addChild(amara), QStringLiteral("Wyznanie w oazie"));
        const QString loose = document.addLoose(QPointF(380.0, -200.0));
        document.setText(loose, QStringLiteral("Sprawdzić: burze piaskowe w kwietniu"));
        document.toggleMarker(loose, QStringLiteral("check"));
        document.connectNodes(wearer, idris);
        document.setConnectionLabel(0, QStringLiteral("ta sama wyprawa"));
        document.select(wearer);
        editor->resize(1100, 700);
        editor->show();
        QApplication::processEvents();
        editor->grab().save(QString::fromLocal8Bit(shot));
    }
    REQUIRE(book.closeProject(false));
    kalahari::core::ExtensionPointRegistry::getInstance().unregisterPlugin("kalahari.mindmaps");
    CHECK(kalahari::core::windowEditorProvider(QStringLiteral("mindmap")) == nullptr);
}
