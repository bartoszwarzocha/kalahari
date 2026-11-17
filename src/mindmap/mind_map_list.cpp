// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_list.cpp
/// @brief A mind map as a list with indents

#include <kalahari/mindmap/mind_map_list.h>

#include <kalahari/mindmap/mind_map_document.h>
#include <kalahari/mindmap/mind_map_menus.h>
#include <kalahari/mindmap/mind_map_scene.h>

#include <QContextMenuEvent>
#include <QFontMetricsF>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QTimer>

#include <algorithm>
#include <utility>

namespace kalahari::mindmap {

namespace {

constexpr int ROW_HEIGHT = 22;
constexpr int INDENT = 20;
constexpr double PAD = 4.0;
constexpr double BULLET = 9.0;
constexpr double GAP = 6.0;
constexpr double MARKER = 14.0;
constexpr double MARKER_GAP = 4.0;
constexpr double SIGN_PIXELS = 9.5;
constexpr double POINTS_PER_PIXEL = 0.75;
constexpr double ICON = 12.0;
constexpr double DIMMED = 0.35;
constexpr double ROOT_CORNER = 2.0;

/// Where the text of a row starts, after its bullet and its markers
double textLeft(const QRectF& row, qsizetype markers) {
    return row.left() + PAD + BULLET + GAP + static_cast<double>(markers) * (MARKER + MARKER_GAP);
}

/// The rows of the list: the bullet in the color of the branch, the markers, the text, the
/// element the node is about and its note
class RowDelegate : public QStyledItemDelegate {
public:
    explicit RowDelegate(MindMapList& list) : QStyledItemDelegate(&list), m_list(list) {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        QStyleOptionViewItem item = option;
        initStyleOption(&item, index);
        const QString id = index.data(MindMapList::ID_ROLE).toString();
        const MindMapLook& look = m_list.look();
        const MindMap& map = m_list.document().map();
        const MindMapNode* node = map.node(id);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        if (node == nullptr) {
            // The heading of the loose nodes
            QFont font = item.font;
            font.setWeight(QFont::DemiBold);
            painter->setFont(font);
            painter->setPen(look.mutedText);
            painter->drawText(item.rect.adjusted(static_cast<int>(PAD), 0, 0, 0),
                              Qt::AlignLeft | Qt::AlignBottom, item.text);
            painter->restore();
            return;
        }
        const QString text = item.text;
        item.text.clear();
        item.widget->style()->drawControl(QStyle::CE_ItemViewItem, &item, painter, item.widget);
        if (!m_list.matches(id)) {
            painter->setOpacity(DIMMED);
        }
        const QRectF row(item.rect);
        const double middle = row.center().y();
        const bool root = id == map.root();

        // The bullet: a square for the main topic
        const QRectF bullet(row.left() + PAD, middle - BULLET / 2.0, BULLET, BULLET);
        painter->setPen(Qt::NoPen);
        if (root) {
            painter->setBrush(look.text);
            painter->drawRoundedRect(bullet, ROOT_CORNER, ROOT_CORNER);
        } else {
            painter->setBrush(MindMapScene::branchColorOf(map, look, id));
            painter->drawEllipse(bullet);
        }

        // The markers
        QFont sign = item.font;
        sign.setPointSizeF(SIGN_PIXELS * POINTS_PER_PIXEL);
        sign.setWeight(QFont::Bold);
        double x = bullet.right() + GAP;
        for (const QString& marker : node->markers) {
            const QRectF circle(x, middle - MARKER / 2.0, MARKER, MARKER);
            painter->setPen(Qt::NoPen);
            painter->setBrush(look.markerColor(marker));
            painter->drawEllipse(circle);
            painter->setFont(sign);
            painter->setPen(look.markerText);
            painter->drawText(circle, Qt::AlignCenter, markerSign(marker));
            x += MARKER + MARKER_GAP;
        }

        // The text, then what the node is about and its note
        const bool selected = (item.state & QStyle::State_Selected) != 0;
        const QColor ink = selected ? item.palette.color(QPalette::HighlightedText)
                                    : item.palette.color(QPalette::Text);
        const QColor muted = selected ? ink : look.mutedText;
        QFont font = item.font;
        const int depth = map.depthOf(id);
        font.setWeight(root ? QFont::Bold : depth == 1 ? QFont::DemiBold : QFont::Normal);
        const QString about = node->link.isEmpty() ? QString() : m_list.elementTitle(node->link);
        //: In the list of a mind map: the node has a note. In Polish: notatka
        const QString noted = node->note.isEmpty() ? QString() : MindMapList::tr("note");
        QFont small = item.font;
        small.setPointSizeF(item.font.pointSizeF() * 0.92);
        const QFontMetricsF smallMetrics(small);
        const double extras =
            (about.isEmpty() ? 0.0 : ICON + 3.0 + smallMetrics.horizontalAdvance(about) + GAP) +
            (noted.isEmpty() ? 0.0 : ICON + 3.0 + smallMetrics.horizontalAdvance(noted) + GAP);
        const double room = std::max(0.0, row.right() - PAD - x - extras);
        const QFontMetricsF metrics(font);
        const QString shown = metrics.elidedText(QString(text).replace(QLatin1Char('\n'),
                                                                       QLatin1Char(' ')),
                                                 Qt::ElideRight, room);
        painter->setFont(font);
        painter->setPen(ink);
        painter->drawText(QRectF(x, row.top(), room, row.height()),
                          Qt::AlignLeft | Qt::AlignVCenter, shown);
        x += metrics.horizontalAdvance(shown) + GAP;
        painter->setFont(small);
        if (!about.isEmpty()) {
            MindMapMenus::paintLinkIcon(*painter, QRectF(x, middle - ICON / 2.0, ICON, ICON),
                                        muted);
            x += ICON + 3.0;
            painter->setPen(muted);
            const double width = smallMetrics.horizontalAdvance(about);
            painter->drawText(QRectF(x, row.top(), width + 1.0, row.height()),
                              Qt::AlignLeft | Qt::AlignVCenter, about);
            x += width + GAP;
        }
        if (!noted.isEmpty()) {
            MindMapMenus::paintNoteIcon(*painter, QRectF(x, middle - ICON / 2.0, ICON, ICON),
                                        muted);
            x += ICON + 3.0;
            painter->setPen(muted);
            painter->drawText(QRectF(x, row.top(), smallMetrics.horizontalAdvance(noted) + 1.0,
                                     row.height()),
                              Qt::AlignLeft | Qt::AlignVCenter, noted);
        }
        painter->restore();
    }

    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const override {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        size.setHeight(std::max(size.height(), ROW_HEIGHT));
        return size;
    }

    [[nodiscard]] QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                                        const QModelIndex& index) const override {
        QWidget* editor = QStyledItemDelegate::createEditor(parent, option, index);
        //: The accessible name of the field of the text of a node. In Polish: Tekst węzła
        editor->setAccessibleName(MindMapList::tr("Text of the node"));
        return editor;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override {
        // The node's own text, else what it shows (the title of its element)
        if (auto* line = qobject_cast<QLineEdit*>(editor); line != nullptr) {
            line->setText(m_list.document().shownText(index.data(MindMapList::ID_ROLE).toString()));
            line->selectAll();
            return;
        }
        QStyledItemDelegate::setEditorData(editor, index);
    }

    void setModelData(QWidget* editor, QAbstractItemModel* /*model*/,
                      const QModelIndex& index) const override {
        if (const auto* line = qobject_cast<QLineEdit*>(editor); line != nullptr) {
            m_pending = line->text();
            m_pendingId = index.data(MindMapList::ID_ROLE).toString();
        }
    }

    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option,
                              const QModelIndex& index) const override {
        const MindMapNode* node =
            m_list.document().map().node(index.data(MindMapList::ID_ROLE).toString());
        QRect rect = option.rect;
        rect.setLeft(static_cast<int>(
            textLeft(QRectF(option.rect), node != nullptr ? node->markers.size() : 0) - 3.0));
        editor->setGeometry(rect);
    }

    /// The text written in the field, and its node; taken once
    [[nodiscard]] std::pair<QString, QString> takeWritten() const {
        std::pair<QString, QString> written{m_pendingId, m_pending};
        m_pendingId.clear();
        m_pending.clear();
        return written;
    }

private:
    MindMapList& m_list;
    mutable QString m_pending;
    mutable QString m_pendingId;
};

/// Whether key @p key types a sign
bool typed(const QKeyEvent& key) {
    const QString text = key.text();
    return !text.isEmpty() && text.at(0).isPrint() && !text.trimmed().isEmpty();
}

}  // namespace

MindMapList::MindMapList(MindMapDocument& document, QWidget* parent)
    : QTreeWidget(parent), m_document(document), m_look(MindMapLook::current()) {
    setHeaderHidden(true);
    setColumnCount(1);
    header()->setStretchLastSection(true);
    setIndentation(INDENT);
    setUniformRowHeights(true);
    setExpandsOnDoubleClick(false);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed |
                    QAbstractItemView::DoubleClicked);
    setItemDelegate(new RowDelegate(*this));
    setFrameShape(QFrame::NoFrame);
    //: The accessible name of the list of a mind map. In Polish: Lista mapy myśli
    setAccessibleName(tr("List of the mind map"));

    connect(&m_document, &MindMapDocument::changed, this, &MindMapList::refresh);
    connect(&m_document, &MindMapDocument::selectionChanged, this, &MindMapList::showSelected);
    connect(this, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem* current) {
        if (!m_filling && current != nullptr) {
            m_document.select(current->data(0, ID_ROLE).toString());
        }
    });
    const auto toggled = [this](QTreeWidgetItem* item, bool expanded) {
        const QString id = item->data(0, ID_ROLE).toString();
        const MindMapNode* node = m_document.map().node(id);
        if (!m_filling && node != nullptr && node->collapsed == expanded) {
            m_document.toggleCollapsed(id);
        }
    };
    connect(this, &QTreeWidget::itemExpanded, this,
            [toggled](QTreeWidgetItem* item) { toggled(item, true); });
    connect(this, &QTreeWidget::itemCollapsed, this,
            [toggled](QTreeWidgetItem* item) { toggled(item, false); });
    rebuild();
}

// =============================================================================
// The rows
// =============================================================================

void MindMapList::refresh() {
    // Not under a field being closed, whose row would go
    if (m_refreshing) {
        return;
    }
    m_refreshing = true;
    QTimer::singleShot(0, this, [this]() {
        m_refreshing = false;
        if (state() == QAbstractItemView::EditingState) {
            refresh();
            return;
        }
        rebuild();
    });
}

void MindMapList::setLook(const MindMapLook& look) {
    m_look = look;
    viewport()->update();
}

void MindMapList::setHighlight(const QString& filter) {
    m_highlight = filter;
    viewport()->update();
}

bool MindMapList::matches(const QString& id) const {
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

QString MindMapList::elementTitle(const QString& elementId) const {
    return m_elementTitle ? m_elementTitle(elementId) : QString();
}

void MindMapList::rebuild() {
    m_filling = true;
    const int scrolled = verticalScrollBar() != nullptr ? verticalScrollBar()->value() : 0;
    clear();
    m_items.clear();
    const MindMap& map = m_document.map();
    addBranch(nullptr, map.root());
    if (!map.looseNodes().isEmpty()) {
        auto* heading = new QTreeWidgetItem(this);
        //: The heading of the loose nodes in the list of a mind map. In Polish: Luźne węzły
        heading->setText(0, tr("Loose nodes"));
        heading->setFlags(Qt::ItemIsEnabled);
        for (const QString& id : map.looseNodes()) {
            addBranch(nullptr, id);
        }
    }
    verticalScrollBar()->setValue(scrolled);
    m_filling = false;
    showSelected();
    if (!m_editAfter.isEmpty()) {
        const QString id = m_editAfter;
        m_editAfter.clear();
        startEditing(id);
    }
}

void MindMapList::addBranch(QTreeWidgetItem* parent, const QString& id) {
    const MindMapNode* node = m_document.map().node(id);
    if (node == nullptr) {
        return;
    }
    auto* item = parent != nullptr ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(this);
    item->setText(0, m_document.shownText(*node));
    item->setData(0, ID_ROLE, id);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
    if (!node->note.isEmpty()) {
        item->setToolTip(0, node->note);
    }
    m_items.insert(id, item);
    for (const QString& child : node->children) {
        addBranch(item, child);
    }
    item->setExpanded(!node->collapsed);
}

void MindMapList::showSelected() {
    QTreeWidgetItem* item = m_items.value(m_document.selected());
    if (item == nullptr || item == currentItem()) {
        return;
    }
    const bool filling = m_filling;
    m_filling = true;
    for (QTreeWidgetItem* above = item->parent(); above != nullptr; above = above->parent()) {
        above->setExpanded(true);
    }
    setCurrentItem(item);
    scrollToItem(item);
    m_filling = filling;
}

QString MindMapList::selectedId() const {
    const MindMap& map = m_document.map();
    return map.node(m_document.selected()) != nullptr ? m_document.selected() : map.root();
}

// =============================================================================
// Changes
// =============================================================================

void MindMapList::startEditing(const QString& id) {
    QTreeWidgetItem* item = m_items.value(id);
    if (item == nullptr) {
        return;
    }
    m_document.select(id);
    setCurrentItem(item);
    m_editing = id;
    editItem(item, 0);
}

void MindMapList::editNew(const QString& id) {
    if (id.isEmpty()) {
        return;
    }
    // The row comes with the next refresh
    m_editAfter = id;
    m_editingNew = true;
    refresh();
}

void MindMapList::addChild() {
    editNew(m_document.addChild(selectedId()));
}

void MindMapList::addSibling(bool before) {
    editNew(m_document.addSibling(selectedId(), before));
}

void MindMapList::level(bool down) {
    const QString id = selectedId();
    const MindMap& map = m_document.map();
    if (id == map.root()) {
        return;
    }
    const QString text = m_document.shownText(id);
    if (down) {
        if (map.parentOf(id).isEmpty()) {
            //: In Polish: Luźny węzeł wraca do gałęzi, gdy przeciągnie się go na węzeł w mapie.
            emit told(tr("A loose node goes back into a branch when it is dragged onto a node "
                         "of the map."),
                      false);
        } else if (!m_document.indent(id)) {
            //: In Polish: Przed „%1” nie ma węzła, do którego mógłby wejść.
            emit told(tr("There is no node before “%1” that it could go into.").arg(text),
                      false);
        }
        return;
    }
    const QString parent = map.parentOf(id);
    if (parent.isEmpty() || parent == map.root() || !m_document.outdent(id)) {
        //: In Polish: „%1” jest już na najwyższym poziomie.
        emit told(tr("“%1” is at the top level already.").arg(text), false);
    }
}

void MindMapList::closeEditor(QWidget* editor, QAbstractItemDelegate::EndEditHint hint) {
    // Tab and Shift+Tab put the node a level down and up, as in the list without a field
    const bool next = hint == QAbstractItemDelegate::EditNextItem;
    const bool previous = hint == QAbstractItemDelegate::EditPreviousItem;
    QTreeWidget::closeEditor(editor, next || previous ? QAbstractItemDelegate::NoHint : hint);
    const QString added = m_editingNew ? m_editing : QString();
    m_editing.clear();
    m_editingNew = false;
    const auto [writtenId, text] = static_cast<RowDelegate*>(itemDelegate())->takeWritten();
    if (!added.isEmpty() && (writtenId != added || text.trimmed().isEmpty())) {
        // A node just added, with nothing written in it, goes
        m_document.undo();
        return;
    }
    if (!writtenId.isEmpty()) {
        m_document.setText(writtenId, text);
    }
    if (next || previous) {
        level(next);
    }
    setFocus(Qt::OtherFocusReason);
}

// =============================================================================
// The keys and the menu
// =============================================================================

bool MindMapList::focusNextPrevChild(bool /*next*/) {
    // Tab and Shift+Tab put the node a level down and up (keyPressEvent())
    return false;
}

bool MindMapList::event(QEvent* event) {
    if (event->type() == QEvent::ShortcutOverride && state() != EditingState) {
        // The keys of the list go to it before the commands of the window
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
                       code == Qt::Key_F10 || typed(*key))) ||
            (control && (code == Qt::Key_Z || code == Qt::Key_Y || code == Qt::Key_C ||
                         code == Qt::Key_V || code == Qt::Key_Return || code == Qt::Key_Enter ||
                         code == Qt::Key_Up || code == Qt::Key_Down)) ||
            (modifiers == Qt::AltModifier && (code == Qt::Key_Return || code == Qt::Key_Enter));
        if (own) {
            event->accept();
            return true;
        }
    }
    return QTreeWidget::event(event);
}

void MindMapList::keyPressEvent(QKeyEvent* event) {
    const QString id = selectedId();
    const int key = event->key();
    const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
    const bool control = (modifiers & Qt::ControlModifier) != 0;
    const bool shift = (modifiers & Qt::ShiftModifier) != 0;
    const bool alt = (modifiers & Qt::AltModifier) != 0;
    const bool enter = key == Qt::Key_Return || key == Qt::Key_Enter;
    QString message;

    if ((key == Qt::Key_F10 && shift) || key == Qt::Key_Menu) {
        QMenu menu(this);
        fillMenu(menu);
        const QRect row = visualItemRect(currentItem());
        menu.exec(viewport()->mapToGlobal(row.isValid() ? row.center() : QPoint()));
    } else if (control && (key == Qt::Key_Y || (key == Qt::Key_Z && shift))) {
        const QString what = m_document.redoText();
        m_document.redo();
        if (!what.isEmpty()) {
            emit told(tr("Done again: %1.").arg(what), false);
        }
    } else if (control && key == Qt::Key_Z) {
        const QString what = m_document.undoText();
        m_document.undo();
        if (!what.isEmpty()) {
            emit told(tr("Undone: %1.").arg(what), false);
        }
    } else if (control && (key == Qt::Key_Up || key == Qt::Key_Down)) {
        if (!m_document.moveAmongSiblings(id, key == Qt::Key_Up ? -1 : 1)) {
            emit told(key == Qt::Key_Up ? tr("It is the first node of this branch already.")
                                        : tr("It is the last node of this branch already."),
                      false);
        }
    } else if (key == Qt::Key_Tab && !shift) {
        level(true);
    } else if (key == Qt::Key_Backtab) {
        level(false);
    } else if (key == Qt::Key_Insert) {
        addChild();
    } else if (enter && control) {
        const MindMapNode* node = m_document.map().node(id);
        if (node != nullptr && !node->link.isEmpty()) {
            emit openElementRequested(node->link);
        }
    } else if (enter && alt) {
        emit noteRequested(id);
    } else if (enter) {
        if (id == m_document.map().root()) {
            addChild();
        } else {
            addSibling(shift);
        }
    } else if (key == Qt::Key_Space && !control && !alt) {
        const MindMapNode* node = m_document.map().node(id);
        if (node != nullptr && !node->children.isEmpty()) {
            m_document.toggleCollapsed(id);
        }
    } else if (key == Qt::Key_Delete) {
        const bool removed = MindMapMenus::remove(m_document, id, &message);
        if (!message.isEmpty()) {
            emit told(message, removed);
        }
    } else if (control && key == Qt::Key_C) {
        emit told(MindMapMenus::copy(m_document, id), false);
    } else if (control && key == Qt::Key_V) {
        MindMapMenus::paste(m_document, id, &message);
        if (!message.isEmpty()) {
            emit told(message, true);
        }
    } else if (key == Qt::Key_F2) {
        startEditing(id);
    } else {
        QTreeWidget::keyPressEvent(event);
    }
}

void MindMapList::contextMenuEvent(QContextMenuEvent* event) {
    if (QTreeWidgetItem* item = itemAt(event->pos()); item != nullptr) {
        const QString id = item->data(0, ID_ROLE).toString();
        if (m_document.map().node(id) == nullptr) {
            return;
        }
        m_document.select(id);
    }
    QMenu menu(this);
    fillMenu(menu);
    menu.exec(event->globalPos());
}

void MindMapList::fillMenu(QMenu& menu) {
    const QString id = selectedId();
    const MindMapNode* node = m_document.map().node(id);
    if (node == nullptr) {
        return;
    }
    const bool root = id == m_document.map().root();
    connect(menu.addAction(tr("New node inside")), &QAction::triggered, this,
            &MindMapList::addChild);
    if (!root) {
        connect(menu.addAction(tr("New node after it")), &QAction::triggered, this,
                [this]() { addSibling(false); });
        connect(menu.addAction(tr("New node before it")), &QAction::triggered, this,
                [this]() { addSibling(true); });
    }
    menu.addSeparator();
    connect(menu.addAction(tr("Change the text")), &QAction::triggered, this,
            [this, id]() { startEditing(id); });
    connect(menu.addAction(tr("Note…")), &QAction::triggered, this,
            [this, id]() { emit noteRequested(id); });
    connect(menu.addAction(tr("Element of the book…")), &QAction::triggered, this,
            [this, id]() { emit linkRequested(id); });
    if (!node->link.isEmpty()) {
        const QString link = node->link;
        connect(menu.addAction(tr("Open the element")), &QAction::triggered, this,
                [this, link]() { emit openElementRequested(link); });
        connect(menu.addAction(tr("Unlink the element")), &QAction::triggered, this,
                [this, id]() { m_document.setLink(id, QString()); });
    }
    MindMapMenus::addMarkers(menu, m_document, m_look, id);
    if (!root) {
        MindMapMenus::addColors(menu, m_document, m_look, id);
    }
    menu.addSeparator();
    connect(menu.addAction(tr("Copy as text")), &QAction::triggered, this,
            [this, id]() { emit told(MindMapMenus::copy(m_document, id), false); });
    QAction* paste = menu.addAction(tr("Paste"));
    paste->setEnabled(MindMapMenus::canPaste());
    connect(paste, &QAction::triggered, this, [this, id]() {
        QString message;
        MindMapMenus::paste(m_document, id, &message);
        if (!message.isEmpty()) {
            emit told(message, true);
        }
    });
    menu.addSeparator();
    QAction* remove = menu.addAction(tr("Delete"));
    remove->setEnabled(!root);
    connect(remove, &QAction::triggered, this, [this, id]() {
        QString message;
        const bool removed = MindMapMenus::remove(m_document, id, &message);
        if (!message.isEmpty()) {
            emit told(message, removed);
        }
    });
}

}  // namespace kalahari::mindmap
