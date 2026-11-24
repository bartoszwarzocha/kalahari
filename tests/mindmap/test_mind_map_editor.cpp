// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file test_mind_map_editor.cpp
/// @brief The editor of mind maps: the map open on its tab with Undo, its drawing, the map and
/// the list worked with the keys and the mouse, Node Properties and the module the program knows

#include <catch2/catch_test_macros.hpp>
#include <kalahari/core/book_project.h>
#include <kalahari/core/book_type_registry.h>
#include <kalahari/core/extension_points.h>
#include <kalahari/core/project_manager.h>
#include <kalahari/core/window_editor_provider.h>
#include <kalahari/gui/settings/settings_page.h>
#include <kalahari/mindmap/mind_map.h>
#include <kalahari/mindmap/mind_map_content.h>
#include <kalahari/mindmap/mind_map_document.h>
#include <kalahari/mindmap/mind_map_editor.h>
#include <kalahari/mindmap/mind_map_list.h>
#include <kalahari/mindmap/mind_map_look.h>
#include <kalahari/mindmap/mind_map_module.h>
#include <kalahari/mindmap/mind_map_node_dialog.h>
#include <kalahari/mindmap/mind_map_picker.h>
#include <kalahari/mindmap/mind_map_scene.h>
#include <kalahari/mindmap/mind_map_view.h>

#include "kalahari_tests/test_support/window_parts.h"

#include <QApplication>
#include <QComboBox>
#include <QFileInfo>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>

#include <algorithm>
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

/// A drag of the left button from @p from to @p to, over the start distance of a drag
void drag(QWidget* widget, const QPointF& from, const QPointF& to) {
    const auto send = [widget](QEvent::Type type, const QPointF& at, Qt::MouseButton button,
                               Qt::MouseButtons buttons) {
        QMouseEvent event(type, at, widget->mapToGlobal(at), button, buttons, Qt::NoModifier);
        QApplication::sendEvent(widget, &event);
    };
    send(QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
    const QPointF step(QApplication::startDragDistance() + 2.0, 0.0);
    send(QEvent::MouseMove, from + step, Qt::NoButton, Qt::LeftButton);
    send(QEvent::MouseMove, to, Qt::NoButton, Qt::LeftButton);
    send(QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
}

/// A map open on its tab, its file in a folder of its own; the main topic shows "Wątki"
struct OpenMap {
    QTemporaryDir dir;
    MindMapDocument document{dir.filePath(QStringLiteral("map.kmap"))};

    OpenMap() {
        document.setShownText([this](const MindMapNode& node) {
            return node.text.isEmpty() && node.id == document.map().root() ? QStringLiteral("Wątki")
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

TEST_CASE("Mind map scene: each node is a box with its sockets, and the links join them",
          "[mindmap][scene]") {
    MindMap map;
    map.setPlace(map.root(), QPointF(0.0, 0.0));
    const QString idea = QString::fromLatin1(MindMapNode::IDEA_KIND);
    const QString compass = map.addNode(idea, QStringLiteral("Kompas"), QPointF(260.0, 0.0),
                                        map.root());
    const QString wearer = map.addNode(QString::fromLatin1(MindMapNode::QUESTION_KIND),
                                       QStringLiteral("Kto go teraz nosi?"), QPointF(520.0, 0.0),
                                       compass);
    const QString idris = map.addNode(idea, QStringLiteral("Idris"), QPointF(260.0, 140.0),
                                      map.root());
    map.connect(wearer, idris, QStringLiteral("ta sama wyprawa"));
    const MindMapLook look;
    const MindMapContent content(nullptr);
    const auto shown = [&map, &content, &look](const MindMapNode& node) {
        return content.contentOf(map, node, QStringLiteral("Wątki"), look);
    };

    MindMapScene scene(map, look, QFont(), shown, {});
    REQUIRE(scene.nodes().size() == 4);
    CHECK(scene.links().size() == 4);  // Three branches and a connection
    for (const NodeBox& box : scene.nodes()) {
        CHECK(scene.nodeAt(box.rect.center()) == box.id);
        CHECK(box.rect.topLeft() == map.node(box.id)->place.value());
        CHECK(box.rect.width() == NodeMetrics::WIDTH);
    }
    CHECK(scene.box(compass)->content.title == QStringLiteral("Kompas"));
    CHECK(scene.box(map.root())->content.title == QStringLiteral("Wątki"));
    // The main topic has only its output; the other nodes an input too
    CHECK(scene.box(map.root())->sockets.size() == 1);
    REQUIRE(scene.box(idris)->sockets.size() == 2);
    const SocketBox output = scene.box(map.root())->sockets.front();
    CHECK(output.right);
    const std::optional<SocketBox> caught = scene.socketAt(output.centre, 4.0);
    REQUIRE(caught);
    CHECK(caught->node == map.root());
    const SocketBox input = scene.box(idris)->sockets.front();
    CHECK(MindMapScene::joinable(output, input) == SocketKind::Branch);
    CHECK_FALSE(MindMapScene::joinable(output, scene.box(compass)->sockets.back()));
    CHECK_FALSE(MindMapScene::joinable(input, input));
    // The connection is found at its label and on its line
    const qsizetype line = scene.linkOfConnection(0);
    REQUIRE(line >= 0);
    CHECK(scene.linkAt(scene.labelRect(line).center(), 4.0) == line);
    // The arrows go to the nearest node that way
    CHECK(scene.nodeToward(map.root(), 1, 0) == compass);
    CHECK(scene.nodeToward(compass, 0, 1) == idris);

    // A node dragged takes its links with it
    const QPointF before = scene.links().at(line).end;
    scene.moveNodes({idris}, QPointF(0.0, 40.0));
    CHECK(scene.box(idris)->rect.top() == 180.0);
    CHECK(scene.links().at(scene.linkOfConnection(0)).end == before + QPointF(0.0, 40.0));

    // A folded node shows only its header; a frame goes around its nodes
    MindMapNode folded = *map.node(wearer);
    folded.note = QStringLiteral("Może Idris?");
    map.setNode(folded);
    const qreal unfolded = MindMapScene(map, look, QFont(), shown, {}).box(wearer)->rect.height();
    CHECK(unfolded > NodeMetrics::HEADER);
    folded.folded = true;
    map.setNode(folded);
    map.addFrame(QStringLiteral("Wyprawa"), {compass, wearer});
    const MindMapScene again(map, look, QFont(), shown, {});
    CHECK(again.box(wearer)->rect.height() == NodeMetrics::HEADER);
    REQUIRE(again.frames().size() == 1);
    CHECK(again.frames().front().rect.contains(again.box(compass)->rect));
    CHECK(again.frameAt(again.frames().front().title.center()) == again.frames().front().id);
}

TEST_CASE("Mind map view: the keys add, change and delete nodes", "[mindmap][view][gui]") {
    OpenMap open;
    MindMapDocument& document = open.document;
    const QString root = document.map().root();
    MindMapContent content(nullptr);
    MindMapView view(document, content, nullptr);
    view.resize(800, 600);
    QStringList told;
    QObject::connect(&view, &MindMapView::told,
                     [&told](const QString& message, bool) { told.append(message); });
    // The main topic gets its place when the map is drawn first
    CHECK(document.map().node(root)->place.has_value());
    CHECK_FALSE(document.canUndo());

    // The keys of the map go to it before the commands of the window
    CHECK(keepsFromShortcuts(&view, Qt::Key_Delete, Qt::NoModifier));
    CHECK(keepsFromShortcuts(&view, Qt::Key_Z, Qt::ControlModifier));
    CHECK(keepsFromShortcuts(&view, Qt::Key_H, Qt::NoModifier));
    CHECK(keepsFromShortcuts(&view, Qt::Key_A, Qt::ShiftModifier));
    CHECK(keepsFromShortcuts(&view, Qt::Key_K, Qt::NoModifier) == false);

    // Tab adds a branch on the right, its name changed in its place
    press(&view, Qt::Key_Tab);
    REQUIRE(view.isEditing());
    const QString compass = document.selected();
    REQUIRE(compass != root);
    CHECK(document.map().parentOf(compass) == root);
    CHECK(view.scene()->box(compass)->rect.left() > view.scene()->box(root)->rect.right());
    auto* field = view.findChild<QLineEdit*>();
    REQUIRE(field != nullptr);
    CHECK(field->text() == QStringLiteral("New idea"));
    field->setText(QStringLiteral("Kompas"));
    press(field, Qt::Key_Return);
    CHECK_FALSE(view.isEditing());
    CHECK(open.textOf(compass) == QStringLiteral("Kompas"));

    // Enter adds a node below, of the same branch; Esc keeps it with its first name
    press(&view, Qt::Key_Return);
    REQUIRE(view.isEditing());
    const QString below = document.selected();
    CHECK(document.map().parentOf(below) == root);
    CHECK(view.scene()->box(below)->rect.top() > view.scene()->box(compass)->rect.bottom());
    press(field, Qt::Key_Escape);
    CHECK_FALSE(view.isEditing());
    CHECK(open.textOf(below) == QStringLiteral("New idea"));

    // H folds the node to its header, and unfolds it
    press(&view, Qt::Key_H);
    CHECK(document.map().node(below)->folded);
    press(&view, Qt::Key_H);
    CHECK_FALSE(document.map().node(below)->folded);

    // Delete removes the node alone: the nodes of its branches stay, loose; Ctrl+Z brings it
    // back
    const QString inside = document.addNode(QString::fromLatin1(MindMapNode::IDEA_KIND),
                                            QStringLiteral("Kto go teraz nosi?"),
                                            QPointF(500.0, 0.0), compass);
    document.select(compass);
    press(&view, Qt::Key_Delete);
    CHECK(document.map().node(compass) == nullptr);
    CHECK(document.map().node(inside) != nullptr);
    CHECK(document.map().parentOf(inside).isEmpty());
    REQUIRE_FALSE(told.isEmpty());
    CHECK(told.last().contains(QStringLiteral("Kompas")));
    press(&view, Qt::Key_Z, Qt::ControlModifier);
    CHECK(document.map().node(compass) != nullptr);
    CHECK(document.map().parentOf(inside) == compass);
    // The main topic stays
    document.select(root);
    press(&view, Qt::Key_Delete);
    CHECK(document.map().node(root) != nullptr);

    // Ctrl+J puts a frame around the nodes selected, its title changed in its place
    document.setSelection({compass, inside});
    press(&view, Qt::Key_J, Qt::ControlModifier);
    REQUIRE(document.map().frames().size() == 1);
    CHECK(document.map().frames().front().nodes.contains(inside));
    REQUIRE(view.isEditing());
    field->setText(QStringLiteral("Wyprawa"));
    press(field, Qt::Key_Return);
    CHECK(document.map().frames().front().title == QStringLiteral("Wyprawa"));
    // Delete removes the frame selected; its nodes stay
    view.selectFrame(document.map().frames().front().id);
    press(&view, Qt::Key_Delete);
    CHECK(document.map().frames().isEmpty());
    CHECK(document.map().node(compass) != nullptr);

    // A link selected: Delete detaches the branch, the node stays where it is
    const qsizetype branch = [&view, &compass]() {
        for (qsizetype i = 0; i < view.scene()->links().size(); ++i) {
            if (view.scene()->links().at(i).to == compass) {
                return i;
            }
        }
        return qsizetype{-1};
    }();
    REQUIRE(branch >= 0);
    const QPointF place = document.map().node(compass)->place.value();
    view.selectLink(MindMapView::LinkRef{LinkLine::Type::Branch, root, compass, -1, {}});
    press(&view, Qt::Key_Delete);
    CHECK(document.map().parentOf(compass).isEmpty());
    CHECK(document.map().node(compass)->place.value() == place);

    // Arrange puts the nodes in a layout, one step of Undo
    view.arrange(MindMapLayout::Right);
    CHECK(document.undoText() == QStringLiteral("arranging the map"));
    CHECK(document.map().layout() == MindMapLayout::Right);

    // The zoom
    view.resetZoom();
    CHECK(view.zoom() == 1.0);
    view.zoomBy(1);
    CHECK(view.zoom() > 1.0);
    press(&view, Qt::Key_0, Qt::ControlModifier);
    CHECK(view.zoom() == 1.0);
}

TEST_CASE("Mind map view: the mouse drags the nodes and the links between their sockets",
          "[mindmap][view][gui]") {
    OpenMap open;
    MindMapDocument& document = open.document;
    const QString root = document.map().root();
    MindMapContent content(nullptr);
    MindMapView view(document, content, nullptr);
    view.resize(1000, 700);
    view.show();
    QApplication::processEvents();
    const QString idea = QString::fromLatin1(MindMapNode::IDEA_KIND);
    const QPointF origin = document.map().node(root)->place.value();
    const QString compass = document.addNode(idea, QStringLiteral("Kompas"),
                                             origin + QPointF(320.0, -80.0), QString());
    const QString well = document.addNode(idea, QStringLiteral("Studnia"),
                                          origin + QPointF(320.0, 120.0), QString());
    REQUIRE(document.map().parentOf(compass).isEmpty());
    const auto socketOf = [&view](const QString& node, bool right) {
        const NodeBox* box = view.scene()->box(node);
        REQUIRE(box != nullptr);
        const auto socket = std::find_if(
            box->sockets.cbegin(), box->sockets.cend(),
            [right](const SocketBox& each) { return each.row.isEmpty() && each.right == right; });
        REQUIRE(socket != box->sockets.cend());
        return view.toView(socket->centre);
    };

    // From the output of the main topic to the input of a loose node: a branch; Ctrl+Z
    // takes it back
    drag(&view, socketOf(root, true), socketOf(compass, false));
    CHECK(document.map().parentOf(compass) == root);
    press(&view, Qt::Key_Z, Qt::ControlModifier);
    CHECK(document.map().parentOf(compass).isEmpty());
    // A link dropped on the body of the node finds its socket
    drag(&view, socketOf(root, true), view.toView(view.scene()->box(well)->rect.center()));
    CHECK(document.map().parentOf(well) == root);

    // A node dragged by its header: a step of Undo
    const QPointF before = document.map().node(compass)->place.value();
    const QPointF header = view.toView(view.scene()->box(compass)->header.center());
    drag(&view, header, header + QPointF(60.0, 40.0));
    const QPointF moved = document.map().node(compass)->place.value();
    CHECK(moved.x() > before.x());
    CHECK(moved.y() > before.y());
    press(&view, Qt::Key_Z, Qt::ControlModifier);
    CHECK(document.map().node(compass)->place.value() == before);

    // From the output to an empty place: Add Node there, the node picked joined
    const qsizetype nodes = document.map().size();
    const QPointF empty = socketOf(compass, true) + QPointF(240.0, 0.0);
    REQUIRE(view.scene()->nodeAt(view.toMap(empty)).isEmpty());
    drag(&view, socketOf(compass, true), empty);
    auto* picker = view.findChild<MindMapPicker*>();
    REQUIRE(picker != nullptr);
    CHECK(picker->isVisible());
    CHECK(picker->list()->count() >= 3);  // The kinds of the writer's nodes
    picker->pickRow(picker->list()->currentRow());
    CHECK(document.map().size() == nodes + 1);
    const QString added = document.selected();
    CHECK(document.map().parentOf(added) == compass);
    CHECK(document.map().node(added)->kindOrIdea(false) == idea);
    CHECK(document.map().node(added)->place.value().x() > before.x());
    if (auto* field = view.findChild<QLineEdit*>(); field != nullptr && view.isEditing()) {
        press(field, Qt::Key_Escape);
    }
}

TEST_CASE("Mind map node properties: the name, the kind, the note and the attributes",
          "[mindmap][dialog][gui]") {
    MindMap map;
    const QString idea = QString::fromLatin1(MindMapNode::IDEA_KIND);
    const QString compass = map.addNode(idea, QStringLiteral("Kompas"), QPointF(260.0, 0.0),
                                        map.root());
    const QString well = map.addNode(idea, QStringLiteral("Studnia"), QPointF(260.0, 140.0),
                                     map.root());
    MindMapNode node = *map.node(compass);
    NodeAttribute owner;
    owner.name = QStringLiteral("Właściciel");
    owner.value = QStringLiteral("Selim");
    owner.extra.insert(QStringLiteral("later"), 1);
    node.attributes = {owner};
    REQUIRE(map.setNode(node));
    MindMapNode other = *map.node(well);
    other.attributes = {NodeAttribute{QStringLiteral("Motyw"), QStringLiteral("woda"), {}}};
    REQUIRE(map.setNode(other));
    CHECK(map.attributeNames().contains(QStringLiteral("Motyw")));
    const MindMapContent content(nullptr);

    MindMapNodeDialog dialog(map, *map.node(compass), content, QStringLiteral("Wątki"), nullptr);
    CHECK(dialog.nameEdit()->text() == QStringLiteral("Kompas"));
    REQUIRE(dialog.kindCombo() != nullptr);
    CHECK(dialog.kindCombo()->count() == 3);
    CHECK(dialog.rowList() == nullptr);    // An own node shows nothing of the book
    CHECK(dialog.imageCombo() == nullptr);  // No book, no pictures
    REQUIRE(dialog.attributes().size() == 1);

    dialog.nameEdit()->setText(QStringLiteral("  Kompas ojca  "));
    dialog.kindCombo()->setCurrentIndex(
        dialog.kindCombo()->findData(QString::fromLatin1(MindMapNode::QUESTION_KIND)));
    dialog.noteEdit()->setPlainText(QStringLiteral("Kto go teraz nosi?\n"));
    dialog.addAttribute(QStringLiteral("Motyw"), QStringLiteral("zemsta"));
    dialog.addAttribute(QString(), QStringLiteral("bez nazwy"));  // Not kept
    dialog.addAttributeButton()->click();                          // An empty row, not kept
    const MindMapNode changed = dialog.node();
    CHECK(changed.id == compass);
    CHECK(changed.text == QStringLiteral("Kompas ojca"));
    CHECK(changed.kind == QString::fromLatin1(MindMapNode::QUESTION_KIND));
    CHECK(changed.note == QStringLiteral("Kto go teraz nosi?"));
    REQUIRE(changed.attributes.size() == 2);
    CHECK(changed.attributes.at(0).name == QStringLiteral("Właściciel"));
    CHECK(changed.attributes.at(0).extra == owner.extra);  // What a later version wrote stays
    CHECK(changed.attributes.at(1) ==
          NodeAttribute{QStringLiteral("Motyw"), QStringLiteral("zemsta"), {}});

    // The main topic has no kind to choose
    const MindMapNodeDialog topic(map, *map.node(map.root()), content, QStringLiteral("Wątki"),
                                  nullptr);
    CHECK(topic.kindCombo() == nullptr);
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
    const kalahari::core::KindRef kind = book.bookTypes().findKind(QStringLiteral("kalahari.base"),
                                                                   QStringLiteral("mindmap"));
    REQUIRE(kind);
    CHECK(kalahari::core::ProjectManager::canAdd(kind));
    const QString id = book.addElement(kind, "Wątki", kalahari::core::BookPlace::Workshop);
    REQUIRE_FALSE(id.isEmpty());
    const kalahari::core::ProjectElement* element = book.findElement(id);
    REQUIRE(element != nullptr);
    CHECK(element->file.endsWith(QStringLiteral(".kmap")));
    MindMap added;
    REQUIRE(added.load(book.filePathOf(*element)));

    const kalahari::core::WindowEditorContext context{book, kalahari::test::commands(), id, {}, {}};
    const std::unique_ptr<QWidget> tab(provider->createEditor(context, nullptr));
    auto* editor = qobject_cast<MindMapEditor*>(tab.get());
    REQUIRE(editor != nullptr);
    CHECK(editor->document().shownText(editor->document().map().root()) == QStringLiteral("Wątki"));
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
