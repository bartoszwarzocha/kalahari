// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_file.cpp
/// @brief A mind map as an outline of text, and as its .kmap file

#include <kalahari/mindmap/mind_map.h>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

namespace kalahari::mindmap {

namespace {

// Keys of the file
const QString ID = QStringLiteral("id");
const QString TEXT = QStringLiteral("text");
const QString LINK = QStringLiteral("link");
const QString NOTE = QStringLiteral("note");
const QString MARKERS = QStringLiteral("markers");
const QString COLOR = QStringLiteral("color");
const QString CHILDREN = QStringLiteral("children");
const QString COLLAPSED = QStringLiteral("collapsed");
const QString FREE = QStringLiteral("free");
const QString LOOSE = QStringLiteral("loose");
const QString FROM = QStringLiteral("from");
const QString TO = QStringLiteral("to");
const QString LABEL = QStringLiteral("label");
const QString FORMAT_KEY = QStringLiteral("format");
const QString ROOT = QStringLiteral("root");
const QString NODES = QStringLiteral("nodes");
const QString CONNECTIONS = QStringLiteral("connections");
const QString VIEW = QStringLiteral("view");
const QString LAYOUT = QStringLiteral("layout");
const QString MODE = QStringLiteral("mode");
const QString ZOOM = QStringLiteral("zoom");

constexpr std::array<const char*, 4> LAYOUT_NAMES{"bothSides", "right", "down", "free"};
constexpr std::array<const char*, 2> MODE_NAMES{"map", "list"};

/// Spaces a tab stands for in the indent of an outline
constexpr int TAB_WIDTH = 4;

QJsonObject unknownOf(QJsonObject json, std::initializer_list<QString> known) {
    for (const QString& key : known) {
        json.remove(key);
    }
    return json;
}

void setError(QString* error, const QString& text) {
    if (error != nullptr) {
        *error = text;
    }
}

QJsonArray pointToJson(const QPointF& point) {
    return QJsonArray{point.x(), point.y()};
}

std::optional<QPointF> pointFromJson(const QJsonValue& value) {
    const QJsonArray array = value.toArray();
    if (array.size() != 2 || !array.at(0).isDouble() || !array.at(1).isDouble()) {
        return std::nullopt;
    }
    return QPointF(array.at(0).toDouble(), array.at(1).toDouble());
}

QJsonObject nodeToJson(const MindMapNode& node) {
    QJsonObject json = node.extra;
    json.insert(ID, node.id);
    json.insert(TEXT, node.text);
    const std::array<std::pair<const QString*, const QString*>, 3> texts{
        {{&LINK, &node.link}, {&NOTE, &node.note}, {&COLOR, &node.color}}};
    for (const auto& [key, value] : texts) {
        if (!value->isEmpty()) {
            json.insert(*key, *value);
        }
    }
    if (!node.markers.isEmpty()) {
        json.insert(MARKERS, QJsonArray::fromStringList(node.markers));
    }
    if (!node.children.isEmpty()) {
        json.insert(CHILDREN, QJsonArray::fromStringList(node.children));
    }
    if (node.collapsed) {
        json.insert(COLLAPSED, true);
    }
    if (node.freePlace) {
        json.insert(FREE, pointToJson(*node.freePlace));
    }
    if (node.loosePlace) {
        json.insert(LOOSE, pointToJson(*node.loosePlace));
    }
    return json;
}

QStringList textsOf(const QJsonValue& value) {
    QStringList texts;
    for (const QJsonValue& item : value.toArray()) {
        if (item.isString() && !item.toString().isEmpty() && !texts.contains(item.toString())) {
            texts.append(item.toString());
        }
    }
    return texts;
}

MindMapNode nodeFromJson(const QJsonObject& json) {
    MindMapNode node;
    node.id = json.value(ID).toString();
    node.text = json.value(TEXT).toString();
    node.link = json.value(LINK).toString();
    node.note = json.value(NOTE).toString();
    node.markers = textsOf(json.value(MARKERS));
    node.color = json.value(COLOR).toString();
    node.children = textsOf(json.value(CHILDREN));
    node.collapsed = json.value(COLLAPSED).toBool();
    node.freePlace = pointFromJson(json.value(FREE));
    node.loosePlace = pointFromJson(json.value(LOOSE));
    node.extra = unknownOf(json, {ID, TEXT, LINK, NOTE, MARKERS, COLOR, CHILDREN, COLLAPSED,
                                  FREE, LOOSE});
    return node;
}

/// A line of an outline: how deep its indent is, and its text without a bullet
struct OutlineLine {
    int indent = 0;
    QString text;
};

std::optional<OutlineLine> outlineLine(const QString& line) {
    static const QRegularExpression bullet(
        QStringLiteral("^(?:[-*+•·–]|\\d{1,3}[.)])\\s+"));
    int indent = 0;
    qsizetype at = 0;
    for (; at < line.size() && line.at(at).isSpace(); ++at) {
        indent += line.at(at) == QLatin1Char('\t') ? TAB_WIDTH : 1;
    }
    QString text = line.mid(at).trimmed();
    text.remove(bullet);
    if (text.isEmpty()) {
        return std::nullopt;
    }
    return OutlineLine{indent, text};
}

}  // namespace

// =============================================================================
// Outlines
// =============================================================================

QString MindMap::outlineOf(const QString& id,
                           const std::function<QString(const MindMapNode&)>& shownText) const {
    QStringList lines;
    const std::function<void(const QString&, int)> add = [&](const QString& at, int level) {
        const MindMapNode* own = node(at);
        if (own == nullptr) {
            return;
        }
        lines.append(QString(level, QLatin1Char('\t')) + shownText(*own));
        for (const QString& child : own->children) {
            add(child, level + 1);
        }
    };
    add(id, 0);
    return lines.join(QLatin1Char('\n'));
}

QStringList MindMap::addOutline(const QString& parent, const QString& text, qsizetype index) {
    QStringList added;
    if (node(parent) == nullptr) {
        return added;
    }
    // The nodes the next line can go into, with their indents
    QList<std::pair<int, QString>> open;
    qsizetype at = index;
    for (const QString& line : text.split(QRegularExpression(QStringLiteral("\r\n|\r|\n")))) {
        const std::optional<OutlineLine> item = outlineLine(line);
        if (!item) {
            continue;
        }
        while (!open.isEmpty() && open.last().first >= item->indent) {
            open.removeLast();
        }
        QString id;
        if (open.isEmpty()) {
            id = addChild(parent, item->text, at);
            if (at >= 0) {
                ++at;
            }
            added.append(id);
        } else {
            id = addChild(open.last().second, item->text);
        }
        open.append({item->indent, id});
    }
    return added;
}

// =============================================================================
// File
// =============================================================================

QJsonObject MindMap::toJson() const {
    QJsonArray nodes;
    for (const QString& id : nodeIds()) {
        nodes.append(nodeToJson(m_nodes.value(id)));
    }
    QJsonArray connections;
    for (const MindMapConnection& connection : m_connections) {
        QJsonObject json = connection.extra;
        json.insert(FROM, connection.from);
        json.insert(TO, connection.to);
        if (!connection.label.isEmpty()) {
            json.insert(LABEL, connection.label);
        }
        connections.append(json);
    }
    QJsonObject view = m_viewExtra;
    view.insert(LAYOUT, nameOf(m_layout));
    view.insert(MODE, QLatin1String(MODE_NAMES.at(static_cast<std::size_t>(m_mode))));
    view.insert(ZOOM, m_zoom);
    QJsonObject json = m_extra;
    json.insert(FORMAT_KEY, FORMAT);
    json.insert(ROOT, m_root);
    json.insert(NODES, nodes);
    json.insert(CONNECTIONS, connections);
    json.insert(VIEW, view);
    return json;
}

bool MindMap::fromJson(const QJsonObject& json, QString* error) {
    *this = MindMap();
    const int format = json.value(FORMAT_KEY).toInt(FORMAT);
    if (format > FORMAT) {
        setError(error, QStringLiteral("the mind map has format %1, this version of Kalahari "
                                       "reads format %2")
                            .arg(format)
                            .arg(FORMAT));
        return false;
    }
    QHash<QString, MindMapNode> read;
    QStringList order;
    for (const QJsonValue& value : json.value(NODES).toArray()) {
        const MindMapNode own = nodeFromJson(value.toObject());
        if (!own.id.isEmpty() && !read.contains(own.id)) {
            read.insert(own.id, own);
            order.append(own.id);
        }
    }
    const QString root = json.value(ROOT).toString();
    if (!read.contains(root)) {
        setError(error, QStringLiteral("the mind map has no main topic"));
        return false;
    }
    m_nodes.clear();
    m_root = root;

    // Each node once, under its first parent; nodes that no parent reaches are loose
    QSet<QString> placed;
    const std::function<void(const QString&)> place = [&](const QString& id) {
        placed.insert(id);
        MindMapNode own = read.value(id);
        own.children.removeIf([&](const QString& child) {
            return !read.contains(child) || placed.contains(child) || child == root;
        });
        for (const QString& child : own.children) {
            placed.insert(child);
        }
        m_nodes.insert(id, own);
        for (const QString& child : own.children) {
            m_parents.insert(child, id);
            place(child);
        }
    };
    place(root);
    m_nodes[root].loosePlace.reset();
    for (const QString& id : order) {
        if (!placed.contains(id)) {
            place(id);
            m_loose.append(id);
            if (!m_nodes[id].loosePlace) {
                m_nodes[id].loosePlace = QPointF();
            }
        }
    }
    for (MindMapNode& own : m_nodes) {
        if (!m_loose.contains(own.id)) {
            own.loosePlace.reset();
        }
    }

    for (const QJsonValue& value : json.value(CONNECTIONS).toArray()) {
        const QJsonObject item = value.toObject();
        if (connect(item.value(FROM).toString(), item.value(TO).toString(),
                    item.value(LABEL).toString())) {
            m_connections.last().extra = unknownOf(item, {FROM, TO, LABEL});
        }
    }
    const QJsonObject view = json.value(VIEW).toObject();
    m_layout = layoutOf(view.value(LAYOUT).toString()).value_or(m_layout);
    m_mode = view.value(MODE).toString() == QLatin1String(MODE_NAMES.at(1)) ? MindMapMode::List
                                                                            : MindMapMode::Map;
    setZoom(view.value(ZOOM).toDouble(m_zoom));
    m_viewExtra = unknownOf(view, {LAYOUT, MODE, ZOOM});
    m_extra = unknownOf(json, {FORMAT_KEY, ROOT, NODES, CONNECTIONS, VIEW});
    return true;
}

bool MindMap::load(const QString& path, QString* error) {
    *this = MindMap();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("%1: %2").arg(path, file.errorString()));
        return false;
    }
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!document.isObject()) {
        setError(error, QStringLiteral("%1: %2").arg(path, parseError.errorString()));
        return false;
    }
    if (!fromJson(document.object(), error)) {
        if (error != nullptr) {
            *error = QStringLiteral("%1: %2").arg(path, *error);
        }
        return false;
    }
    return true;
}

bool MindMap::save(const QString& path, QString* error) const {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(error, QStringLiteral("%1: %2").arg(path, file.errorString()));
        return false;
    }
    file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        setError(error, QStringLiteral("%1: %2").arg(path, file.errorString()));
        return false;
    }
    return true;
}

QString MindMap::nameOf(MindMapLayout layout) {
    return QString::fromLatin1(LAYOUT_NAMES.at(static_cast<std::size_t>(layout)));
}

std::optional<MindMapLayout> MindMap::layoutOf(const QString& name) {
    for (std::size_t i = 0; i < LAYOUT_NAMES.size(); ++i) {
        if (name == QLatin1String(LAYOUT_NAMES.at(i))) {
            return static_cast<MindMapLayout>(i);
        }
    }
    return std::nullopt;
}

}  // namespace kalahari::mindmap
