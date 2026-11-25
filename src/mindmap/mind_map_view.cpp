// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file mind_map_view.cpp
/// @brief The map of nodes: its drawing, its coordinates, its zoom and the text being changed

#include <kalahari/mindmap/mind_map_view.h>

#include <kalahari/core/settings_manager.h>
#include <kalahari/mindmap/mind_map_content.h>
#include <kalahari/mindmap/mind_map_document.h>
#include <kalahari/mindmap/mind_map_layout.h>

#include <QApplication>
#include <QEasingCurve>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QLineEdit>
#include <QResizeEvent>
#include <QTimer>
#include <QVariantAnimation>

#include <algorithm>
#include <cmath>
#include <utility>

namespace kalahari::mindmap {

namespace {

/// The zoom of the wheel and of the buttons, a step of each
constexpr double WHEEL_FACTOR = 1.15;
constexpr double BUTTON_FACTOR = 1.25;
constexpr int WHEEL_STEP = 120;
/// How fast the zoom comes to where it goes: this part of the way at each frame
constexpr double ZOOM_EASE = 0.28;
constexpr int FRAME_MS = 16;
/// The view is saved this long after it changed
constexpr int SAVE_VIEW_MS = 600;
/// How long the whole map and 100% take to show, and the nodes to go to their places
constexpr int VIEW_MS = 320;
constexpr int PLACES_MS = 380;
/// Room around the map shown whole, and around a node scrolled to
constexpr double FIT_ROOM = 40.0;
constexpr double SHOWN_ROOM = 24.0;
/// A new map, or one of format 1: its nodes put in their layout this much wider apart, and
/// its places of format 1 (free, loose) this much further, as the nodes are boxes now
constexpr double NEW_GAPS = 2.0;
constexpr double FORMAT1_SCALE_X = 1.9;
constexpr double FORMAT1_SCALE_Y = 1.6;
/// The room of the field of the text being changed
constexpr double EDITOR_ROOM = 2.0;
constexpr double LEAST_LABEL_WIDTH = 120.0;

}  // namespace

MindMapView::MindMapView(MindMapDocument& document, MindMapContent& content,
                         core::ProjectManager* book, QWidget* parent)
    : QWidget(parent),
      m_document(document),
      m_content(content),
      m_book(book),
      m_look(MindMapLook::current()),
      m_zoomTimer(new QTimer(this)),
      m_viewTimer(new QTimer(this)),
      m_viewAnimation(new QVariantAnimation(this)),
      m_placesAnimation(new QVariantAnimation(this)) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAcceptDrops(true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    //: The accessible name of the map of a mind map. In Polish: Mapa myśli
    setAccessibleName(tr("Mind map"));
    m_zoom = std::clamp(m_document.map().zoom(), MindMap::MIN_ZOOM, MindMap::MAX_ZOOM);
    m_zoomTarget = m_zoom;

    m_zoomTimer->setInterval(FRAME_MS);
    connect(m_zoomTimer, &QTimer::timeout, this, [this]() {
        m_zoom += (m_zoomTarget - m_zoom) * ZOOM_EASE;
        if (std::abs(m_zoomTarget - m_zoom) < 0.002) {
            m_zoom = m_zoomTarget;
            m_zoomTimer->stop();
        }
        m_offset = m_zoomAnchor - m_zoomMapAnchor * m_zoom;
        viewMoved();
    });
    m_viewTimer->setSingleShot(true);
    m_viewTimer->setInterval(SAVE_VIEW_MS);
    connect(m_viewTimer, &QTimer::timeout, this, &MindMapView::saveView);

    m_viewAnimation->setDuration(VIEW_MS);
    m_viewAnimation->setStartValue(0.0);
    m_viewAnimation->setEndValue(1.0);
    m_viewAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_viewAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& at) {
        const double k = at.toDouble();
        m_zoom = m_fromZoom + (m_toZoom - m_fromZoom) * k;
        m_zoomTarget = m_zoom;
        m_offset = m_fromOffset + (m_toOffset - m_fromOffset) * k;
        viewMoved();
    });

    m_placesAnimation->setDuration(PLACES_MS);
    m_placesAnimation->setStartValue(0.0);
    m_placesAnimation->setEndValue(1.0);
    m_placesAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_placesAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& at) {
        if (!m_scene) {
            return;
        }
        const double k = at.toDouble();
        QHash<QString, QPointF> deltas;
        for (auto it = m_toPlaces.cbegin(); it != m_toPlaces.cend(); ++it) {
            const NodeBox* box = m_scene->box(it.key());
            const QPointF from = m_fromPlaces.value(it.key(), it.value());
            if (box != nullptr) {
                deltas.insert(it.key(), from + (it.value() - from) * k - box->rect.topLeft());
            }
        }
        m_scene->moveNodes(deltas);
        update();
    });
    connect(m_placesAnimation, &QVariantAnimation::finished, this, &MindMapView::applyPlaces);

    connect(&m_document, &MindMapDocument::changed, this, &MindMapView::refresh);
    connect(&m_document, &MindMapDocument::selectionChanged, this, [this]() {
        if (!m_document.selected().isEmpty()) {
            m_link.reset();
            m_frame.clear();
            ensureShown(m_document.selected());
        }
        update();
        emit selectionChanged();
    });
    refresh();
}

MindMapView::~MindMapView() = default;

// =============================================================================
// The drawing again, the look, the highlight
// =============================================================================

void MindMapView::refresh() {
    m_font = m_look.editorFont
                 ? QFont(QString::fromStdString(
                       core::SettingsManager::getInstance().get<std::string>("editor.fontFamily",
                                                                             "Georgia")))
                 : QApplication::font();
    rebuild();
    placeNewNodes();
    // A node that is not drawn (the main topic of a map of relations) is not selected
    const QStringList& selection = m_document.selection();
    if (std::any_of(selection.cbegin(), selection.cend(),
                    [this](const QString& id) { return m_scene->box(id) == nullptr; })) {
        m_document.clearSelection();
    }
    if (m_link && linkIndexOf(*m_link) < 0) {
        m_link.reset();
    }
    if (!m_frame.isEmpty() && m_document.map().frame(m_frame) == nullptr) {
        m_frame.clear();
    }
    placeEditor();
    update();
}

void MindMapView::rebuild() {
    const MindMap& map = m_document.map();
    m_scene = std::make_unique<MindMapScene>(
        map, m_look, m_font,
        [this](const MindMapNode& node) {
            return m_content.contentOf(m_document.map(), node, m_mapTitle, m_look);
        },
        m_content.relationLinks(map));
}

void MindMapView::placeNewNodes() {
    const MindMap& map = m_document.map();
    if (!map.hasNodesWithoutPlace() || !m_scene) {
        return;
    }
    QHash<QString, QPointF> places;
    const MindMapNode* root = map.node(map.root());
    if (root != nullptr && !root->place && map.relationMap()) {
        // A new map of relations: its cards in generations, or in a grid, right of the main
        // topic, which it does not draw
        const QSizeF size = m_scene->sizeOf(map.root());
        const QRectF anchor(QPointF(-size.width(), -size.height()) / 2.0, size);
        places.insert(map.root(), anchor.topLeft());
        const QHash<QString, QPointF> cards =
            map.relationMap()->generations && hasFamilies()
                ? generationPlaces(anchor)
                : gridPlaces(QPointF(anchor.right() + NodeMetrics::GAP_ACROSS, anchor.top()));
        for (auto it = cards.cbegin(); it != cards.cend(); ++it) {
            const MindMapNode* node = map.node(it.key());
            if (node != nullptr && !node->place) {
                places.insert(it.key(), it.value());
            }
        }
    } else if (root != nullptr && !root->place) {
        // A new map, or one of format 1: every node where the layout of the map puts it, the
        // main topic in the middle; the places of format 1 further apart, as the nodes are
        // boxes now
        MindMapLayoutOptions options;
        options.everyNode = true;
        options.gaps = NEW_GAPS;
        options.placeScale = QPointF(FORMAT1_SCALE_X, FORMAT1_SCALE_Y);
        const QHash<QString, MindMapPlace> laid = layOutMindMap(
            map, map.layout(),
            [this](const MindMapNode& node, int /*depth*/) { return m_scene->sizeOf(node.id); },
            options);
        for (auto it = laid.cbegin(); it != laid.cend(); ++it) {
            const MindMapNode* node = map.node(it.key());
            if (node != nullptr && !node->place) {
                const QSizeF size = m_scene->sizeOf(it.key());
                places.insert(it.key(), it->centre - QPointF(size.width(), size.height()) / 2.0);
            }
        }
    }
    // The others (added by the list, pasted): right of their parents under their last nodes;
    // a loose node under all
    QList<QRectF> taken;
    for (const NodeBox& box : m_scene->nodes()) {
        const MindMapNode* node = map.node(box.id);
        if (node != nullptr && node->place) {
            taken.append(box.rect);
        }
    }
    for (auto it = places.cbegin(); it != places.cend(); ++it) {
        taken.append(QRectF(it.value(), m_scene->sizeOf(it.key())));
    }
    const auto placeOf = [&](const QString& id) -> std::optional<QPointF> {
        if (const auto found = places.constFind(id); found != places.cend()) {
            return *found;
        }
        const MindMapNode* node = map.node(id);
        return node != nullptr ? node->place : std::nullopt;
    };
    const auto freeFrom = [&taken](QPointF at, const QSizeF& size) {
        for (int guard = 0; guard < 200; ++guard) {
            const QRectF wanted(at, size);
            const auto hit = std::find_if(taken.cbegin(), taken.cend(), [&wanted](const QRectF& r) {
                return r
                    .adjusted(-NodeMetrics::GAP_ALONG / 2.0, -NodeMetrics::GAP_ALONG / 2.0,
                              NodeMetrics::GAP_ALONG / 2.0, NodeMetrics::GAP_ALONG / 2.0)
                    .intersects(wanted);
            });
            if (hit == taken.cend()) {
                break;
            }
            at.setY(hit->bottom() + NodeMetrics::GAP_ALONG);
        }
        return at;
    };
    QStringList order{map.root()};
    order += map.descendantsOf(map.root());
    for (const QString& loose : map.looseNodes()) {
        order.append(loose);
        order += map.descendantsOf(loose);
    }
    for (const QString& id : std::as_const(order)) {
        if (placeOf(id)) {
            continue;
        }
        const QSizeF size = m_scene->sizeOf(id);
        QPointF at;
        const QString parent = map.parentOf(id);
        // A card that came to a map of relations: under a card it has a relation with
        QString related;
        for (const RelationLink& relation : m_scene->relations()) {
            const QString other = relation.from == id ? relation.to
                                  : relation.to == id ? relation.from
                                                      : QString();
            if (!other.isEmpty() && placeOf(other)) {
                related = other;
                break;
            }
        }
        if (const std::optional<QPointF> parentPlace = parent.isEmpty() ? std::nullopt
                                                                        : placeOf(parent)) {
            at = *parentPlace +
                 QPointF(m_scene->sizeOf(parent).width() + NodeMetrics::GAP_ACROSS, 0.0);
        } else if (!related.isEmpty()) {
            at = *placeOf(related) +
                 QPointF(0.0, m_scene->sizeOf(related).height() + 2.0 * NodeMetrics::GAP_ALONG);
        } else {
            double bottom = 0.0;
            double left = 0.0;
            bool any = false;
            for (const QRectF& rect : std::as_const(taken)) {
                if (!any || rect.bottom() > bottom) {
                    bottom = rect.bottom();
                }
                if (!any || rect.left() < left) {
                    left = rect.left();
                }
                any = true;
            }
            at = any ? QPointF(left, bottom + 3.0 * NodeMetrics::GAP_ALONG)
                     : QPointF(-size.width() / 2.0, -size.height() / 2.0);
        }
        at = freeFrom(at, size);
        places.insert(id, at);
        taken.append(QRectF(at, size));
    }
    if (places.isEmpty()) {
        return;
    }
    // Not a step of Undo: the nodes were there all along, only not placed yet
    m_document.changeView([&places](MindMap& changed) {
        for (auto it = places.cbegin(); it != places.cend(); ++it) {
            changed.setPlace(it.key(), it.value());
        }
    });
    rebuild();
}

void MindMapView::setLook(const MindMapLook& look) {
    m_look = look;
    m_icons.clear();
    refresh();
}

void MindMapView::setMapTitle(const QString& title) {
    if (title != m_mapTitle) {
        m_mapTitle = title;
        refresh();
    }
}

void MindMapView::setHighlight(const QString& filter) {
    m_highlight = filter;
    update();
}

bool MindMapView::matches(const QString& id) const {
    const MindMapNode* node = m_document.map().node(id);
    if (m_highlight.isEmpty() || node == nullptr || id == m_document.map().root()) {
        return true;
    }
    if (m_highlight == QLatin1String("link")) {
        return !node->link.isEmpty();
    }
    if (m_highlight == QLatin1String("note")) {
        return !node->note.isEmpty();
    }
    return node->markers.contains(m_highlight);
}

QString MindMapView::titleOf(const QString& id) const {
    const MindMapNode* node = m_document.map().node(id);
    return node != nullptr ? m_content.titleOf(m_document.map(), *node, m_mapTitle) : QString();
}

// =============================================================================
// Coordinates and zoom
// =============================================================================

QPointF MindMapView::toMap(const QPointF& point) const {
    return (point - m_offset) / m_zoom;
}

QPointF MindMapView::toView(const QPointF& point) const {
    return point * m_zoom + m_offset;
}

QRectF MindMapView::toView(const QRectF& rect) const {
    return {toView(rect.topLeft()), toView(rect.bottomRight())};
}

void MindMapView::viewMoved() {
    placeEditor();
    update();
    emit zoomChanged(m_zoom);
    m_viewTimer->start();
}

void MindMapView::saveView() {
    const double zoom = m_zoom;
    const QPointF centre = toMap(QPointF(width() / 2.0, height() / 2.0));
    m_document.changeView([zoom, centre](MindMap& map) {
        map.setZoom(zoom);
        map.setCentre(centre);
    });
}

void MindMapView::setZoomAt(double zoom, const QPointF& viewPoint, bool animated) {
    m_viewAnimation->stop();
    const double to = std::clamp(zoom, MindMap::MIN_ZOOM, MindMap::MAX_ZOOM);
    // The point under @p viewPoint stays there; while the zoom goes, from where it is now
    m_zoomAnchor = viewPoint;
    m_zoomMapAnchor = toMap(viewPoint);
    m_zoomTarget = to;
    if (animated && isVisible()) {
        if (!m_zoomTimer->isActive()) {
            m_zoomTimer->start();
        }
        return;
    }
    m_zoomTimer->stop();
    m_zoom = to;
    m_offset = m_zoomAnchor - m_zoomMapAnchor * m_zoom;
    viewMoved();
}

void MindMapView::zoomBy(int steps) {
    setZoomAt(m_zoomTarget * std::pow(BUTTON_FACTOR, steps), QPointF(width() / 2.0, height() / 2.0),
              true);
}

void MindMapView::resetZoom() {
    setZoomAt(1.0, QPointF(width() / 2.0, height() / 2.0), true);
}

void MindMapView::animateView(double zoom, const QPointF& offset) {
    m_zoomTimer->stop();
    m_viewAnimation->stop();
    if (!isVisible()) {
        m_zoom = zoom;
        m_zoomTarget = zoom;
        m_offset = offset;
        viewMoved();
        return;
    }
    m_fromZoom = m_zoom;
    m_toZoom = zoom;
    m_fromOffset = m_offset;
    m_toOffset = offset;
    m_viewAnimation->start();
}

void MindMapView::showAll(bool animated) {
    if (!m_scene || width() <= 0 || height() <= 0) {
        return;
    }
    const QRectF all = m_scene->bounds();
    if (all.isNull()) {
        return;
    }
    const double zoom = std::clamp(std::min({1.0, (width() - FIT_ROOM) / all.width(),
                                             (height() - FIT_ROOM) / all.height()}),
                                   MindMap::MIN_ZOOM, MindMap::MAX_ZOOM);
    const QPointF offset = QPointF(width() / 2.0, height() / 2.0) - all.center() * zoom;
    if (animated) {
        animateView(zoom, offset);
        return;
    }
    m_zoomTimer->stop();
    m_viewAnimation->stop();
    m_zoom = zoom;
    m_zoomTarget = zoom;
    m_offset = offset;
    viewMoved();
}

void MindMapView::wheelEvent(QWheelEvent* event) {
    finishAnimations();
    const bool trackpad = !event->pixelDelta().isNull() && event->phase() != Qt::NoScrollPhase;
    if ((trackpad && !(event->modifiers() & Qt::ControlModifier)) ||
        (event->modifiers() & Qt::ShiftModifier)) {
        // A trackpad moves the map; Shift and the wheel move it across
        QPointF by = trackpad ? QPointF(event->pixelDelta()) : QPointF(event->angleDelta());
        if (!trackpad) {
            by = QPointF(by.y() + by.x(), 0.0) / 2.0;
        }
        m_offset += by;
        viewMoved();
        event->accept();
        return;
    }
    // The wheel zooms around the mouse, smoothly
    const double steps = event->angleDelta().y() / static_cast<double>(WHEEL_STEP);
    if (std::abs(steps) > 1e-6) {
        setZoomAt(m_zoomTarget * std::pow(WHEEL_FACTOR, steps), event->position(), true);
    }
    event->accept();
}

void MindMapView::ensureShown(const QString& id) {
    const NodeBox* box = m_scene ? m_scene->box(id) : nullptr;
    if (box == nullptr || width() <= 0 || !m_shown) {
        return;
    }
    const QRectF shown = toView(box->rect).adjusted(-SHOWN_ROOM, -SHOWN_ROOM, SHOWN_ROOM,
                                                    SHOWN_ROOM);
    const QRectF seen{QPointF(), QSizeF(size())};
    double dx = 0.0;
    double dy = 0.0;
    if (shown.left() < seen.left()) {
        dx = seen.left() - shown.left();
    } else if (shown.right() > seen.right()) {
        dx = std::max(seen.right() - shown.right(), seen.left() - shown.left());
    }
    if (shown.top() < seen.top()) {
        dy = seen.top() - shown.top();
    } else if (shown.bottom() > seen.bottom()) {
        dy = std::max(seen.bottom() - shown.bottom(), seen.top() - shown.top());
    }
    if (std::abs(dx) > 0.5 || std::abs(dy) > 0.5) {
        m_offset += QPointF(dx, dy);
        viewMoved();
    }
}

void MindMapView::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!m_shown && width() > 0 && height() > 0) {
        m_shown = true;
        const std::optional<QPointF>& centre = m_document.map().centre();
        if (centre) {
            m_offset = QPointF(width() / 2.0, height() / 2.0) - *centre * m_zoom;
            update();
        } else {
            showAll(false);
        }
    }
}

void MindMapView::resizeEvent(QResizeEvent* event) {
    if (m_shown && event->oldSize().isValid() && event->oldSize().width() > 0) {
        // The middle of the map stays in the middle
        const QPointF before(event->oldSize().width() / 2.0, event->oldSize().height() / 2.0);
        const QPointF centre = (before - m_offset) / m_zoom;
        m_offset = QPointF(width() / 2.0, height() / 2.0) - centre * m_zoom;
    } else if (!m_shown && isVisible() && width() > 0 && height() > 0) {
        m_shown = true;
        const std::optional<QPointF>& centre = m_document.map().centre();
        if (centre) {
            m_offset = QPointF(width() / 2.0, height() / 2.0) - *centre * m_zoom;
        } else {
            showAll(false);
        }
    }
    QWidget::resizeEvent(event);
    placeEditor();
}

void MindMapView::finishAnimations() {
    if (m_zoomTimer->isActive()) {
        m_zoomTimer->stop();
        m_zoom = m_zoomTarget;
        m_offset = m_zoomAnchor - m_zoomMapAnchor * m_zoom;
        viewMoved();
    }
    if (m_viewAnimation->state() == QAbstractAnimation::Running) {
        m_viewAnimation->setCurrentTime(m_viewAnimation->duration());
    }
    if (m_placesAnimation->state() == QAbstractAnimation::Running) {
        m_placesAnimation->setCurrentTime(m_placesAnimation->duration());
    }
}

// =============================================================================
// The nodes to their places: Arrange
// =============================================================================

void MindMapView::animateTo(const QHash<QString, QPointF>& places, const QString& what,
                            std::optional<MindMapLayout> layout) {
    finishAnimations();
    m_toPlaces = places;
    m_placesWhat = what;
    m_placesLayout = layout;
    m_fromPlaces.clear();
    if (m_scene) {
        for (auto it = places.cbegin(); it != places.cend(); ++it) {
            if (const NodeBox* box = m_scene->box(it.key())) {
                m_fromPlaces.insert(it.key(), box->rect.topLeft());
            }
        }
    }
    if (!isVisible()) {
        applyPlaces();
        return;
    }
    m_placesAnimation->start();
}

void MindMapView::applyPlaces() {
    const QHash<QString, QPointF> places = std::exchange(m_toPlaces, {});
    const std::optional<MindMapLayout> layout = std::exchange(m_placesLayout, std::nullopt);
    m_fromPlaces.clear();
    if (places.isEmpty()) {
        return;
    }
    const bool changed = m_document.change(m_placesWhat, [&places, layout](MindMap& map) {
        for (auto it = places.cbegin(); it != places.cend(); ++it) {
            map.setPlace(it.key(), it.value());
        }
        if (layout) {
            map.setLayout(*layout);
        }
        return true;
    });
    if (!changed) {
        refresh();  // The nodes were there already: the drawing goes back to the map
        return;
    }
    //: In the bar under a mind map. In Polish: Uporządkowano mapę.
    emit told(tr("The map is arranged."), true);
}

// =============================================================================
// The links and the frames selected
// =============================================================================

std::optional<MindMapView::LinkRef> MindMapView::linkRefOf(qsizetype index) const {
    if (!m_scene || index < 0 || index >= m_scene->links().size()) {
        return std::nullopt;
    }
    const LinkLine& line = m_scene->links().at(index);
    LinkRef link;
    link.type = line.type;
    link.from = line.from;
    link.to = line.to;
    if (line.type == LinkLine::Type::Connection) {
        link.connection = line.index;
    } else if (line.type == LinkLine::Type::Relation && line.index >= 0 &&
               line.index < m_scene->relations().size()) {
        link.relation = m_scene->relations().at(line.index).relation;
    }
    return link;
}

qsizetype MindMapView::linkIndexOf(const LinkRef& link) const {
    if (!m_scene) {
        return -1;
    }
    for (qsizetype i = 0; i < m_scene->links().size(); ++i) {
        if (linkRefOf(i) == link) {
            return i;
        }
    }
    return -1;
}

void MindMapView::selectLink(const std::optional<LinkRef>& link) {
    if (link == m_link && (!link || m_frame.isEmpty())) {
        return;
    }
    m_link = link;
    m_frame.clear();
    if (link) {
        m_document.clearSelection();
        if (link->type == LinkLine::Type::Connection) {
            m_document.selectConnection(link->connection);
        }
    }
    update();
    emit selectionChanged();
}

void MindMapView::selectFrame(const QString& id) {
    if (id == m_frame && !m_link) {
        return;
    }
    m_frame = id;
    m_link.reset();
    if (!id.isEmpty()) {
        m_document.clearSelection();
    }
    update();
    emit selectionChanged();
}

void MindMapView::clearSelection() {
    const bool had = m_link || !m_frame.isEmpty();
    m_link.reset();
    m_frame.clear();
    m_document.clearSelection();
    if (had) {
        update();
        emit selectionChanged();
    }
}

// =============================================================================
// The text being changed
// =============================================================================

bool MindMapView::isEditing() const {
    return !m_editingNode.isEmpty() || m_editingLabel >= 0 || !m_editingFrame.isEmpty();
}

namespace {

/// The field of a text changed in its place, in the colors of the map
QLineEdit* makeEditor(QWidget* parent, QObject* filter) {
    auto* editor = new QLineEdit(parent);
    editor->setFrame(false);
    editor->installEventFilter(filter);
    editor->hide();
    return editor;
}

}  // namespace

void MindMapView::startEditing(const QString& id) {
    finishAnimations();
    finishEditing(true);
    const MindMapNode* node = m_document.map().node(id);
    if (node == nullptr || !m_scene) {
        return;
    }
    m_document.select(id);
    ensureShown(id);
    if (!m_editor) {
        m_editor = makeEditor(this, this);
    }
    m_editingNode = id;
    //: The accessible name of the field of the title of a node. In Polish: Nazwa węzła
    m_editor->setAccessibleName(tr("Name of the node"));
    QPalette colors = m_editor->palette();
    colors.setColor(QPalette::Base, m_look.node);
    colors.setColor(QPalette::Text, m_look.text);
    m_editor->setPalette(colors);
    m_editor->setText(titleOf(id));
    m_editor->selectAll();
    placeEditor();
    m_editor->show();
    m_editor->setFocus(Qt::OtherFocusReason);
}

void MindMapView::startEditingLabel(qsizetype index) {
    finishAnimations();
    finishEditing(true);
    const QList<MindMapConnection>& connections = m_document.map().connections();
    if (index < 0 || index >= connections.size() || !m_scene) {
        return;
    }
    if (!m_editor) {
        m_editor = makeEditor(this, this);
    }
    m_editingLabel = index;
    //: The accessible name of the field of the label of a connection. In Polish: Podpis
    //: połączenia
    m_editor->setAccessibleName(tr("Label of the connection"));
    QPalette colors = m_editor->palette();
    colors.setColor(QPalette::Base, m_look.node);
    colors.setColor(QPalette::Text, m_look.text);
    m_editor->setPalette(colors);
    m_editor->setText(connections.at(index).label);
    m_editor->selectAll();
    placeEditor();
    m_editor->show();
    m_editor->setFocus(Qt::OtherFocusReason);
}

void MindMapView::startEditingFrame(const QString& id) {
    finishAnimations();
    finishEditing(true);
    const MindMapFrame* frame = m_document.map().frame(id);
    if (frame == nullptr || !m_scene) {
        return;
    }
    selectFrame(id);
    if (!m_editor) {
        m_editor = makeEditor(this, this);
    }
    m_editingFrame = id;
    //: The accessible name of the field of the title of a frame. In Polish: Tytuł ramki
    m_editor->setAccessibleName(tr("Title of the frame"));
    QPalette colors = m_editor->palette();
    colors.setColor(QPalette::Base, m_look.frame);
    colors.setColor(QPalette::Text, m_look.text);
    m_editor->setPalette(colors);
    m_editor->setText(frame->title);
    m_editor->selectAll();
    placeEditor();
    m_editor->show();
    m_editor->setFocus(Qt::OtherFocusReason);
}

void MindMapView::placeEditor() {
    if (!m_editor || !isEditing() || !m_scene) {
        return;
    }
    QFont font = m_editingLabel >= 0 ? m_scene->labelFont() : m_scene->titleFont();
    font.setBold(m_editingLabel < 0);
    font.setPixelSize(std::max(1, static_cast<int>(std::lround(font.pixelSize() * m_zoom))));
    if (m_editor->font() != font) {
        m_editor->setFont(font);
    }
    const double lineHeight = QFontMetricsF(font).height() + 2.0 * EDITOR_ROOM;
    QRectF place;
    if (!m_editingNode.isEmpty()) {
        const NodeBox* box = m_scene->box(m_editingNode);
        if (box == nullptr) {
            finishEditing(false);  // The node went: a change elsewhere
            return;
        }
        place = toView(box->title);
    } else if (!m_editingFrame.isEmpty()) {
        const auto frame = std::find_if(m_scene->frames().cbegin(), m_scene->frames().cend(),
                                        [this](const FrameBox& each) {
                                            return each.id == m_editingFrame;
                                        });
        if (frame == m_scene->frames().cend()) {
            finishEditing(false);
            return;
        }
        place = toView(frame->title.adjusted(6.0, 0.0, -6.0, 0.0));
    } else {
        const qsizetype line = m_scene->linkOfConnection(m_editingLabel);
        if (line < 0) {
            finishEditing(false);
            return;
        }
        const QPointF centre = toView(m_scene->links().at(line).labelCentre);
        const double width = std::max(LEAST_LABEL_WIDTH * m_zoom,
                                      QFontMetricsF(font).horizontalAdvance(m_editor->text()) +
                                          4.0 * EDITOR_ROOM);
        place = QRectF(centre.x() - width / 2.0, centre.y() - lineHeight / 2.0, width, lineHeight);
    }
    const double height = std::max(lineHeight, std::min(place.height(), lineHeight * 1.4));
    m_editor->setGeometry(static_cast<int>(std::floor(place.left())),
                          static_cast<int>(std::floor(place.center().y() - height / 2.0)),
                          static_cast<int>(std::ceil(place.width())),
                          static_cast<int>(std::ceil(height)));
}

void MindMapView::finishEditing(bool keep) {
    if (!isEditing() || !m_editor) {
        m_editingNode.clear();
        m_editingLabel = -1;
        m_editingFrame.clear();
        return;
    }
    const QString node = std::exchange(m_editingNode, QString());
    const qsizetype label = std::exchange(m_editingLabel, -1);
    const QString frameId = std::exchange(m_editingFrame, QString());
    const QString text = m_editor->text();
    m_editor->hide();
    if (m_editor->hasFocus() || !hasFocus()) {
        setFocus(Qt::OtherFocusReason);
    }
    if (!keep) {
        return;
    }
    if (label >= 0) {
        m_document.setConnectionLabel(label, text.simplified());
    } else if (!frameId.isEmpty()) {
        if (const MindMapFrame* frame = m_document.map().frame(frameId);
            frame != nullptr && frame->title != text.simplified()) {
            MindMapFrame changed = *frame;
            changed.title = text.simplified();
            m_document.setFrame(changed);
        }
    } else if (!node.isEmpty() && text.trimmed() != titleOf(node)) {
        m_document.setText(node, text.trimmed());
    }
}

bool MindMapView::eventFilter(QObject* watched, QEvent* event) {
    if (watched != m_editor || !isEditing()) {
        return QWidget::eventFilter(watched, event);
    }
    if (event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            finishEditing(true);
            return true;
        }
        if (key->key() == Qt::Key_Escape) {
            finishEditing(false);
            return true;
        }
        if (key->key() == Qt::Key_Tab && !m_editingNode.isEmpty()) {
            finishEditing(true);
            addBranch();
            return true;
        }
    } else if (event->type() == QEvent::FocusOut) {
        // A click elsewhere keeps what was written; the menu of the field does not
        if (static_cast<QFocusEvent*>(event)->reason() != Qt::PopupFocusReason) {
            finishEditing(true);
        }
    }
    return QWidget::eventFilter(watched, event);
}

bool MindMapView::focusNextPrevChild(bool /*next*/) {
    // Tab adds a branch (keyPressEvent()); the map keeps the focus, as a text does
    return false;
}

}  // namespace kalahari::mindmap
