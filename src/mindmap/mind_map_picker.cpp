// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_picker.cpp
/// @brief The small window over a mind map that picks from a list with a search

#include <kalahari/mindmap/mind_map_picker.h>

#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPixmap>
#include <QScreen>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace kalahari::mindmap {

namespace {

/// The role of the index of an item in the list of the picker; a heading has none
constexpr int INDEX_ROLE = Qt::UserRole + 1;
/// The size of the picker, in lines of its text
constexpr int WIDTH_CHARS = 30;
constexpr int MOST_LINES = 16;
constexpr int DOT = 10;

QIcon dotOf(const QColor& color, qreal ratio) {
    QPixmap pixmap(QSize(DOT, DOT) * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    if (color.isValid()) {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawEllipse(QRectF(1.0, 1.0, DOT - 2.0, DOT - 2.0));
    }
    return QIcon(pixmap);
}

}  // namespace

MindMapPicker::MindMapPicker(const QString& title, QList<PickItem> items, QWidget* parent)
    : QFrame(parent, Qt::Popup), m_items(std::move(items)) {
    setAttribute(Qt::WA_DeleteOnClose);
    setFrameShape(QFrame::StyledPanel);
    setAccessibleName(title);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    m_search = new QLineEdit(this);
    //: The search of the small window that adds a node to a mind map or connects nodes. In
    //: Polish: Szukaj…
    m_search->setPlaceholderText(tr("Search…"));
    m_search->setClearButtonEnabled(true);
    m_search->setAccessibleName(title);
    m_search->installEventFilter(this);
    layout->addWidget(m_search);
    m_list = new QListWidget(this);
    m_list->setAccessibleName(title);
    m_list->setUniformItemSizes(false);
    m_list->installEventFilter(this);
    layout->addWidget(m_list);
    const QFontMetrics metrics(font());
    setFixedWidth(metrics.averageCharWidth() * WIDTH_CHARS);
    m_list->setMinimumHeight(metrics.lineSpacing() * 4);
    m_list->setMaximumHeight(metrics.lineSpacing() * MOST_LINES);
    connect(m_search, &QLineEdit::textChanged, this, &MindMapPicker::fill);
    connect(m_list, &QListWidget::itemClicked, this,
            [this](const QListWidgetItem* item) { pickRow(m_list->row(item)); });
    connect(m_list, &QListWidget::itemActivated, this,
            [this](const QListWidgetItem* item) { pickRow(m_list->row(item)); });
    fill(QString());
}

void MindMapPicker::fill(const QString& filter) {
    m_list->clear();
    const QString wanted = filter.trimmed();
    QString group;
    bool first = true;
    for (qsizetype i = 0; i < m_items.size(); ++i) {
        const PickItem& item = m_items.at(i);
        if (!wanted.isEmpty() && !item.text.contains(wanted, Qt::CaseInsensitive)) {
            continue;
        }
        if (first || item.group != group) {
            group = item.group;
            if (!group.isEmpty()) {
                auto* heading = new QListWidgetItem(group, m_list);
                heading->setFlags(Qt::ItemIsEnabled);
                QFont bold = heading->font();
                bold.setBold(true);
                heading->setFont(bold);
                heading->setForeground(palette().brush(QPalette::PlaceholderText));
            }
            first = false;
        }
        auto* entry = new QListWidgetItem(dotOf(item.color, devicePixelRatioF()), item.text, m_list);
        entry->setData(INDEX_ROLE, static_cast<int>(i));
    }
    if (m_list->count() == 0) {
        //: The small window that adds a node to a mind map, when nothing matches the search. In
        //: Polish: Nic nie pasuje
        auto* none = new QListWidgetItem(tr("Nothing matches"), m_list);
        none->setFlags(Qt::ItemIsEnabled);
    }
    for (int row = 0; row < m_list->count(); ++row) {
        if (m_list->item(row)->data(INDEX_ROLE).isValid()) {
            m_list->setCurrentRow(row);
            break;
        }
    }
}

void MindMapPicker::pickRow(int row) {
    const QListWidgetItem* item = m_list->item(row);
    if (item == nullptr || !item->data(INDEX_ROLE).isValid()) {
        return;
    }
    const int index = item->data(INDEX_ROLE).toInt();
    close();
    emit picked(index);
}

void MindMapPicker::popUp(const QPoint& globalPoint) {
    adjustSize();
    QPoint at = globalPoint;
    if (const QScreen* screen = QApplication::screenAt(globalPoint)) {
        const QRect room = screen->availableGeometry();
        at.setX(std::min(at.x(), room.right() - width()));
        at.setY(std::min(at.y(), room.bottom() - height()));
        at.setX(std::max(at.x(), room.left()));
        at.setY(std::max(at.y(), room.top()));
    }
    move(at);
    show();
    m_search->setFocus(Qt::PopupFocusReason);
}

bool MindMapPicker::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() != QEvent::KeyPress) {
        return QFrame::eventFilter(watched, event);
    }
    const auto* key = static_cast<QKeyEvent*>(event);
    const auto step = [this](int by) {
        int row = m_list->currentRow();
        for (int tries = 0; tries < m_list->count(); ++tries) {
            row = (row + by + m_list->count()) % m_list->count();
            if (m_list->item(row)->data(INDEX_ROLE).isValid()) {
                m_list->setCurrentRow(row);
                return;
            }
        }
    };
    switch (key->key()) {
    case Qt::Key_Down:
        step(1);
        return true;
    case Qt::Key_Up:
        step(-1);
        return true;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        pickRow(m_list->currentRow());
        return true;
    case Qt::Key_Escape:
        close();
        return true;
    default:
        break;
    }
    // The letters typed in the list go to the search
    if (watched == m_list && !key->text().isEmpty() && key->text().at(0).isPrint()) {
        m_search->setFocus(Qt::OtherFocusReason);
        m_search->insert(key->text());
        return true;
    }
    return QFrame::eventFilter(watched, event);
}

}  // namespace kalahari::mindmap
