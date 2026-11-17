// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_view.cpp
/// @brief The map of a mind map: its coordinates, zoom and the text being changed

#include <kalahari/mindmap/mind_map_view.h>

#include <kalahari/core/settings_manager.h>
#include <kalahari/mindmap/mind_map_document.h>

#include <QApplication>
#include <QFontMetricsF>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTextDocument>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>

namespace kalahari::mindmap {

namespace {

/// Room around all that is drawn
constexpr double ROOM = 90.0;
/// The steps of the zoom, as in the editor
constexpr std::array<double, 15> ZOOMS{0.25, 0.33, 0.5, 0.67, 0.75, 0.9, 1.0, 1.1,
                                       1.25, 1.5,  1.75, 2.0, 2.5,  3.0, 4.0};
/// The field of the text being changed is at least this wide
constexpr double LEAST_EDITOR_WIDTH = 170.0;
/// The field of a label being changed is at least this wide
constexpr double LEAST_LABEL_WIDTH = 140.0;
constexpr int EDITOR_ROOM = 6;
/// One step of the wheel
constexpr int WHEEL_STEP = 120;

/// Whether key @p key types a sign, which starts changing the text of a node
bool typed(const QKeyEvent& key) {
    const QString text = key.text();
    return !text.isEmpty() && text.at(0).isPrint() && !text.trimmed().isEmpty();
}

/// The step of the zoom nearest to @p zoom
std::size_t nearestZoom(double zoom) {
    std::size_t best = 0;
    for (std::size_t i = 1; i < ZOOMS.size(); ++i) {
        if (std::abs(ZOOMS.at(i) - zoom) < std::abs(ZOOMS.at(best) - zoom)) {
            best = i;
        }
    }
    return best;
}

}  // namespace

MindMapView::MindMapView(MindMapDocument& document, QWidget* parent)
    : QAbstractScrollArea(parent), m_document(document), m_look(MindMapLook::current()) {
    setFocusPolicy(Qt::StrongFocus);
    setFrameShape(QFrame::NoFrame);
    viewport()->setMouseTracking(true);
    viewport()->setAutoFillBackground(false);
    setAttribute(Qt::WA_InputMethodEnabled, true);
    //: The accessible name of the map of a mind map. In Polish: Mapa myśli
    setAccessibleName(tr("Mind map"));
    connect(&m_document, &MindMapDocument::changed, this, &MindMapView::refresh);
    connect(&m_document, &MindMapDocument::selectionChanged, this, [this]() {
        ensureShown(m_document.selected());
        viewport()->update();
    });
    refresh();
}

MindMapView::~MindMapView() = default;

// =============================================================================
// The drawing again, the look, the highlight
// =============================================================================

void MindMapView::refresh() {
    // The main topic stays where it is on the screen
    const QPointF anchor = m_scene ? toViewport(QPointF()) : QPointF();
    const QFont font = m_look.editorFont
                           ? QFont(QString::fromStdString(
                                 core::SettingsManager::getInstance().get<std::string>(
                                     "editor.fontFamily", "Georgia")))
                           : QApplication::font();
    m_scene = std::make_unique<MindMapScene>(
        m_document.map(), m_look, font,
        [this](const MindMapNode& node) { return m_document.shownText(node); });
    updateScrollBars();
    if (m_shown) {
        scrollMapPointTo(QPointF(), anchor);
    } else if (viewport()->width() > 0) {
        scrollMapPointTo(QPointF(), QPointF(viewport()->width() / 2.0, viewport()->height() / 2.0));
        m_shown = isVisible();
    }
    fixFreePlaces();
    placeEditor();
    viewport()->update();
}

void MindMapView::setLook(const MindMapLook& look) {
    m_look = look;
    refresh();
}

void MindMapView::setHighlight(const QString& filter) {
    m_highlight = filter;
    viewport()->update();
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

void MindMapView::fixFreePlaces() {
    // The free layout keeps the places it gave the nodes, so that they stay where they were
    // seen (MindMap::fixFreePlaces())
    if (m_document.map().layout() != MindMapLayout::Free || !m_scene) {
        return;
    }
    QHash<QString, QPointF> places;
    for (const NodeBox& box : m_scene->nodes()) {
        const MindMapNode* node = m_document.map().node(box.id);
        if (node != nullptr && !node->freePlace) {
            places.insert(box.id, box.rect.center());
        }
    }
    if (!places.isEmpty()) {
        m_document.changeView([&places](MindMap& map) { map.fixFreePlaces(places); });
    }
}

// =============================================================================
// Coordinates and zoom
// =============================================================================

double MindMapView::zoom() const {
    return m_document.map().zoom();
}

QRectF MindMapView::content() const {
    const QRectF bounds = m_scene ? m_scene->bounds() : QRectF();
    return bounds.united(QRectF(0.0, 0.0, 1.0, 1.0)).adjusted(-ROOM, -ROOM, ROOM, ROOM);
}

QPointF MindMapView::toViewport(const QPointF& point) const {
    const QRectF all = content();
    const double z = zoom();
    const double shiftX = std::max(0.0, (viewport()->width() - all.width() * z) / 2.0);
    const double shiftY = std::max(0.0, (viewport()->height() - all.height() * z) / 2.0);
    return {(point.x() - all.left()) * z + shiftX - horizontalScrollBar()->value(),
            (point.y() - all.top()) * z + shiftY - verticalScrollBar()->value()};
}

QRectF MindMapView::toViewport(const QRectF& rect) const {
    return {toViewport(rect.topLeft()), toViewport(rect.bottomRight())};
}

QPointF MindMapView::toMap(const QPointF& point) const {
    const QRectF all = content();
    const double z = zoom();
    const double shiftX = std::max(0.0, (viewport()->width() - all.width() * z) / 2.0);
    const double shiftY = std::max(0.0, (viewport()->height() - all.height() * z) / 2.0);
    return {(point.x() + horizontalScrollBar()->value() - shiftX) / z + all.left(),
            (point.y() + verticalScrollBar()->value() - shiftY) / z + all.top()};
}

void MindMapView::updateScrollBars() {
    const QRectF all = content();
    const double z = zoom();
    const int width = static_cast<int>(std::ceil(all.width() * z));
    const int height = static_cast<int>(std::ceil(all.height() * z));
    horizontalScrollBar()->setRange(0, std::max(0, width - viewport()->width()));
    horizontalScrollBar()->setPageStep(viewport()->width());
    horizontalScrollBar()->setSingleStep(20);
    verticalScrollBar()->setRange(0, std::max(0, height - viewport()->height()));
    verticalScrollBar()->setPageStep(viewport()->height());
    verticalScrollBar()->setSingleStep(20);
}

void MindMapView::scrollMapPointTo(const QPointF& mapPoint, const QPointF& viewportPoint) {
    const QPointF now = toViewport(mapPoint);
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() +
                                    static_cast<int>(std::lround(now.x() - viewportPoint.x())));
    verticalScrollBar()->setValue(verticalScrollBar()->value() +
                                  static_cast<int>(std::lround(now.y() - viewportPoint.y())));
}

void MindMapView::setZoom(double zoomTo, const QPointF& viewportAnchor) {
    const QPointF mapPoint = toMap(viewportAnchor);
    m_document.changeView([zoomTo](MindMap& map) { map.setZoom(zoomTo); });
    updateScrollBars();
    scrollMapPointTo(mapPoint, viewportAnchor);
    placeEditor();
    viewport()->update();
    emit zoomChanged(zoom());
}

void MindMapView::zoomBy(int steps) {
    zoomAt(steps, QPointF(viewport()->width() / 2.0, viewport()->height() / 2.0));
}

void MindMapView::zoomAt(int steps, const QPointF& viewportAnchor) {
    const auto at = static_cast<int>(nearestZoom(zoom()));
    const int to = std::clamp(at + steps, 0, static_cast<int>(ZOOMS.size()) - 1);
    if (to != at || std::abs(ZOOMS.at(static_cast<std::size_t>(to)) - zoom()) > 1e-6) {
        setZoom(ZOOMS.at(static_cast<std::size_t>(to)), viewportAnchor);
    }
}

void MindMapView::wheelEvent(QWheelEvent* event) {
    if (!(event->modifiers() & Qt::ControlModifier)) {
        QAbstractScrollArea::wheelEvent(event);
        return;
    }
    // Ctrl and the wheel zoom about the mouse, a step for each step of the wheel
    m_wheel += event->angleDelta().y();
    const int steps = m_wheel / WHEEL_STEP;
    if (steps != 0) {
        m_wheel -= steps * WHEEL_STEP;
        zoomAt(steps, event->position());
    }
    event->accept();
}

void MindMapView::resetZoom() {
    setZoom(1.0, QPointF(viewport()->width() / 2.0, viewport()->height() / 2.0));
}

void MindMapView::ensureShown(const QString& id) {
    const NodeBox* box = m_scene ? m_scene->box(id) : nullptr;
    if (box == nullptr) {
        return;
    }
    constexpr double MARGIN = 16.0;
    const QRectF shown = toViewport(box->rect).adjusted(-MARGIN, -MARGIN, MARGIN, MARGIN);
    const QRectF seen(QPointF(), QSizeF(viewport()->size()));
    double dx = 0.0;
    double dy = 0.0;
    if (shown.left() < seen.left()) {
        dx = shown.left() - seen.left();
    } else if (shown.right() > seen.right()) {
        dx = std::min(shown.right() - seen.right(), shown.left() - seen.left());
    }
    if (shown.top() < seen.top()) {
        dy = shown.top() - seen.top();
    } else if (shown.bottom() > seen.bottom()) {
        dy = std::min(shown.bottom() - seen.bottom(), shown.top() - seen.top());
    }
    horizontalScrollBar()->setValue(horizontalScrollBar()->value() +
                                    static_cast<int>(std::lround(dx)));
    verticalScrollBar()->setValue(verticalScrollBar()->value() + static_cast<int>(std::lround(dy)));
}

void MindMapView::resizeEvent(QResizeEvent* event) {
    const QPointF centre = m_shown ? toMap(QPointF(viewport()->width() / 2.0,
                                                   viewport()->height() / 2.0))
                                   : QPointF();
    QAbstractScrollArea::resizeEvent(event);
    updateScrollBars();
    if (m_shown) {
        scrollMapPointTo(centre, QPointF(viewport()->width() / 2.0, viewport()->height() / 2.0));
    } else {
        scrollMapPointTo(QPointF(), QPointF(viewport()->width() / 2.0, viewport()->height() / 2.0));
        m_shown = isVisible();
    }
    placeEditor();
}

void MindMapView::scrollContentsBy(int /*dx*/, int /*dy*/) {
    placeEditor();
    viewport()->update();
}

// =============================================================================
// The text being changed
// =============================================================================

bool MindMapView::isEditing() const {
    return !m_editing.isEmpty() || m_editingLabel >= 0;
}

void MindMapView::editNew(const QString& id) {
    if (id.isEmpty()) {
        return;
    }
    placeNewInFree(id);
    startEditing(id);
    m_editingNew = true;
}

void MindMapView::startEditing(const QString& id, const QString& seed, bool selectAll) {
    const MindMapNode* node = m_document.map().node(id);
    if (node == nullptr) {
        return;
    }
    finishEditing(true);
    m_document.select(id);
    ensureShown(id);
    m_editing = id;
    makeEditor();
    //: The accessible name of the field of the text of a node. In Polish: Tekst węzła
    m_editor->setAccessibleName(tr("Text of the node"));
    QPalette colors = m_editor->palette();
    colors.setColor(QPalette::Base, m_look.node);
    colors.setColor(QPalette::Text, m_look.text);
    m_editor->setPalette(colors);
    m_editor->setPlainText(seed.isEmpty() ? m_document.shownText(*node) : seed);
    QTextCursor cursor = m_editor->textCursor();
    if (seed.isEmpty() && selectAll) {
        cursor.select(QTextCursor::Document);
    } else {
        cursor.movePosition(QTextCursor::End);
    }
    m_editor->setTextCursor(cursor);
    placeEditor();
    m_editor->show();
    m_editor->setFocus(Qt::OtherFocusReason);
}

void MindMapView::makeEditor() {
    if (m_editor) {
        return;
    }
    m_editor = new QPlainTextEdit(viewport());
    m_editor->setFrameShape(QFrame::Box);
    m_editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_editor->setTabChangesFocus(false);
    m_editor->installEventFilter(this);
    connect(m_editor, &QPlainTextEdit::textChanged, this, &MindMapView::placeEditor);
}

void MindMapView::startEditingLabel(qsizetype index) {
    const QList<MindMapConnection>& connections = m_document.map().connections();
    if (index < 0 || index >= connections.size()) {
        return;
    }
    finishEditing(true);
    m_document.selectConnection(index);
    m_editingLabel = index;
    makeEditor();
    //: The accessible name of the field of the label of a connection. In Polish: Podpis
    //: połączenia
    m_editor->setAccessibleName(tr("Label of the connection"));
    QPalette colors = m_editor->palette();
    colors.setColor(QPalette::Base, m_look.background == MapBackground::Own ? m_look.ownBackground
                                                                            : m_look.mapBackground);
    colors.setColor(QPalette::Text, m_look.text);
    m_editor->setPalette(colors);
    m_editor->setPlainText(connections.at(index).label);
    QTextCursor cursor = m_editor->textCursor();
    cursor.select(QTextCursor::Document);
    m_editor->setTextCursor(cursor);
    placeEditor();
    m_editor->show();
    m_editor->setFocus(Qt::OtherFocusReason);
}

void MindMapView::placeEditor() {
    if (!m_editor || !isEditing() || !m_scene) {
        return;
    }
    if (m_editingLabel >= 0) {
        // Over the label, or where it would be
        const auto line = std::find_if(m_scene->connections().cbegin(),
                                       m_scene->connections().cend(),
                                       [this](const ConnectionLine& each) {
                                           return each.index == m_editingLabel;
                                       });
        if (line == m_scene->connections().cend()) {
            finishEditing(false);
            return;
        }
        QFont font = m_scene->labelFont();
        font.setPointSizeF(font.pointSizeF() * zoom());
        if (m_editor->font() != font) {
            m_editor->setFont(font);
        }
        const QPointF centre = toViewport(line->labelCentre);
        const double width = std::max(LEAST_LABEL_WIDTH * zoom(),
                                      m_editor->document()->idealWidth() + 2.0 * EDITOR_ROOM);
        const double height = QFontMetricsF(font).height() + 2.0 * EDITOR_ROOM;
        m_editor->setGeometry(static_cast<int>(centre.x() - width / 2.0),
                              static_cast<int>(centre.y() - height / 2.0),
                              static_cast<int>(std::ceil(width)),
                              static_cast<int>(std::ceil(height)));
        return;
    }
    const NodeBox* box = m_scene->box(m_editing);
    if (box == nullptr) {
        // The node went (a change elsewhere): nothing to take back
        m_editingNew = false;
        finishEditing(false);
        return;
    }
    QFont font = box->font;
    font.setPointSizeF(box->font.pointSizeF() * zoom());
    if (m_editor->font() != font) {
        m_editor->setFont(font);
    }
    const QRectF shown = toViewport(box->rect);
    const double width = std::max(shown.width(), LEAST_EDITOR_WIDTH * zoom());
    const int lines = std::max(1, m_editor->document()->lineCount());
    const double lineHeight = box->lineHeight * zoom();
    const double height = std::max(shown.height(), lines * lineHeight + 2.0 * EDITOR_ROOM);
    m_editor->setGeometry(static_cast<int>(shown.left()),
                          static_cast<int>(shown.center().y() - height / 2.0),
                          static_cast<int>(std::ceil(width)), static_cast<int>(std::ceil(height)));
}

void MindMapView::finishEditing(bool keep) {
    if (!isEditing() || !m_editor) {
        return;
    }
    const QString id = m_editing;
    const qsizetype label = m_editingLabel;
    const bool added = m_editingNew;
    const QString text = m_editor->toPlainText();
    m_editing.clear();
    m_editingLabel = -1;
    m_editingNew = false;
    m_editor->hide();
    setFocus(Qt::OtherFocusReason);
    if (label >= 0) {
        if (keep) {
            m_document.setConnectionLabel(label, text.simplified());
        }
        return;
    }
    if (added && (!keep || text.trimmed().isEmpty())) {
        // A node just added, with nothing written in it, goes
        m_document.undo();
        return;
    }
    if (keep) {
        m_document.setText(id, text);
    }
}

bool MindMapView::eventFilter(QObject* watched, QEvent* event) {
    if (watched != m_editor || !isEditing()) {
        return QAbstractScrollArea::eventFilter(watched, event);
    }
    if (event->type() == QEvent::KeyPress) {
        const auto* key = static_cast<QKeyEvent*>(event);
        const bool enter = key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter;
        if (enter && (m_editingLabel >= 0 || !(key->modifiers() & Qt::ShiftModifier))) {
            finishEditing(true);
            return true;
        }
        if (key->key() == Qt::Key_Escape) {
            finishEditing(false);
            return true;
        }
        if (key->key() == Qt::Key_Tab && m_editingLabel < 0) {
            finishEditing(true);
            addChild();
            return true;
        }
    } else if (event->type() == QEvent::FocusOut) {
        // A click elsewhere keeps what was written; the menu of the field does not
        const auto* focus = static_cast<QFocusEvent*>(event);
        if (focus->reason() != Qt::PopupFocusReason) {
            finishEditing(true);
        }
    }
    return QAbstractScrollArea::eventFilter(watched, event);
}

bool MindMapView::focusNextPrevChild(bool /*next*/) {
    // Tab adds a node and Shift+Tab goes to the parent (keyPressEvent()); the map keeps the
    // focus, as a text does
    return false;
}

bool MindMapView::event(QEvent* event) {
    if (event->type() == QEvent::ShortcutOverride && !isEditing()) {
        // The keys of the map go to it before the commands of the window: Delete, Ctrl+Z...
        auto* key = static_cast<QKeyEvent*>(event);
        const Qt::KeyboardModifiers modifiers =
            key->modifiers() & ~(Qt::KeypadModifier | Qt::ShiftModifier);
        const int code = key->key();
        const bool plain = modifiers == Qt::NoModifier;
        const bool control = modifiers == Qt::ControlModifier;
        const bool own =
            (plain && (code == Qt::Key_Delete || code == Qt::Key_F2 || code == Qt::Key_Space ||
                       code == Qt::Key_Return || code == Qt::Key_Enter ||
                       code == Qt::Key_Insert || code == Qt::Key_Home || code == Qt::Key_End ||
                       code == Qt::Key_Escape || code == Qt::Key_Left || code == Qt::Key_Right ||
                       code == Qt::Key_Up || code == Qt::Key_Down || code == Qt::Key_F10 ||
                       typed(*key))) ||
            (control && (code == Qt::Key_Z || code == Qt::Key_Y || code == Qt::Key_C ||
                         code == Qt::Key_V || code == Qt::Key_Plus || code == Qt::Key_Equal ||
                         code == Qt::Key_Minus || code == Qt::Key_0 || code == Qt::Key_Return ||
                         code == Qt::Key_Enter || code == Qt::Key_Left ||
                         code == Qt::Key_Right || code == Qt::Key_Up || code == Qt::Key_Down)) ||
            (modifiers == Qt::AltModifier &&
             (code == Qt::Key_Return || code == Qt::Key_Enter));
        if (own) {
            event->accept();
            return true;
        }
    }
    return QAbstractScrollArea::event(event);
}

}  // namespace kalahari::mindmap
