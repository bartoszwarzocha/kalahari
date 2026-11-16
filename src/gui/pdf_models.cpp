// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file pdf_models.cpp
/// @brief The search in a PDF file and its table of contents, as models of views

#include "kalahari/gui/pdf_models.h"

#include <QElapsedTimer>
#include <QTimerEvent>

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

namespace kalahari::gui {

// --- PdfSearchModel ----------------------------------------------------------------------

PdfSearchModel::PdfSearchModel(QObject* parent) : QAbstractListModel(parent) {}

void PdfSearchModel::setDocument(PdfDocument* document) {
    if (m_document == document) {
        return;
    }
    if (m_document != nullptr) {
        disconnect(m_document, nullptr, this, nullptr);
    }
    m_document = document;
    if (m_document != nullptr) {
        connect(m_document, &QObject::destroyed, this, [this]() {
            m_document = nullptr;
            restart();
        });
    }
    restart();
}

void PdfSearchModel::setSearchString(const QString& text) {
    if (text == m_text) {
        return;
    }
    m_text = text;
    restart();
}

int PdfSearchModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_hits.size());
}

QVariant PdfSearchModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_hits.size()) {
        return {};
    }
    const PdfHit& hit = m_hits.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return QString(hit.contextBefore + m_text + hit.contextAfter);
    case static_cast<int>(Role::Page):
        return hit.page;
    case static_cast<int>(Role::ContextBefore):
        return hit.contextBefore;
    case static_cast<int>(Role::ContextAfter):
        return hit.contextAfter;
    default:
        return {};
    }
}

PdfHit PdfSearchModel::resultAtIndex(int index) const {
    if (index < 0 || index >= m_hits.size()) {
        return {};
    }
    return m_hits.at(index);
}

QList<PdfHit> PdfSearchModel::resultsOnPage(int page) const {
    QList<PdfHit> hits;
    auto each = std::lower_bound(m_hits.cbegin(), m_hits.cend(), page,
                                 [](const PdfHit& hit, int wanted) { return hit.page < wanted; });
    for (; each != m_hits.cend() && each->page == page; ++each) {
        hits.append(*each);
    }
    return hits;
}

void PdfSearchModel::timerEvent(QTimerEvent* event) {
    if (event->timerId() != m_timer.timerId()) {
        QAbstractListModel::timerEvent(event);
        return;
    }
    if (m_document == nullptr || m_text.isEmpty()) {
        m_timer.stop();
        return;
    }
    QElapsedTimer clock;
    clock.start();
    const int pages = m_document->pageCount();
    while (m_nextPage < pages && clock.elapsed() < SEARCH_SLICE) {
        const QList<PdfHit> found = m_document->find(m_nextPage, m_text);
        ++m_nextPage;
        if (found.isEmpty()) {
            continue;
        }
        const auto first = static_cast<int>(m_hits.size());
        beginInsertRows(QModelIndex(), first, first + static_cast<int>(found.size()) - 1);
        m_hits.append(found);
        endInsertRows();
    }
    if (m_nextPage >= pages) {
        m_timer.stop();
    }
}

void PdfSearchModel::restart() {
    beginResetModel();
    m_timer.stop();
    m_hits.clear();
    m_nextPage = 0;
    endResetModel();
    if (m_document != nullptr && !m_text.isEmpty() && m_document->pageCount() > 0) {
        m_timer.start(0, this);
    }
}

// --- PdfContentsModel --------------------------------------------------------------------

PdfContentsModel::PdfContentsModel(QObject* parent) : QAbstractItemModel(parent) {
    m_nodes.emplace_back();
}

void PdfContentsModel::setDocument(PdfDocument* document) {
    beginResetModel();
    m_nodes.clear();
    m_nodes.emplace_back();
    if (document != nullptr) {
        // The last entry at each level, from the root: an entry goes under the one of the
        // level above it
        std::vector<int> above{0};
        for (const PdfContentsEntry& entry : document->contents()) {
            const auto level =
                static_cast<std::size_t>(std::clamp(entry.level, 0,
                                                    static_cast<int>(above.size()) - 1));
            above.resize(level + 1);
            const int parent = above.back();
            Node node;
            node.title = entry.title.simplified();
            node.page = entry.page;
            node.parent = parent;
            node.row = static_cast<int>(m_nodes.at(static_cast<std::size_t>(parent)).below.size());
            const auto id = static_cast<int>(m_nodes.size());
            m_nodes.push_back(std::move(node));
            m_nodes.at(static_cast<std::size_t>(parent)).below.push_back(id);
            above.push_back(id);
        }
    }
    endResetModel();
}

QModelIndex PdfContentsModel::index(int row, int column, const QModelIndex& parent) const {
    if (row < 0 || column != 0) {
        return {};
    }
    const std::vector<int>& below = m_nodes.at(static_cast<std::size_t>(nodeOf(parent))).below;
    if (static_cast<std::size_t>(row) >= below.size()) {
        return {};
    }
    return createIndex(row, column, static_cast<quintptr>(below.at(static_cast<std::size_t>(row))));
}

QModelIndex PdfContentsModel::parent(const QModelIndex& child) const {
    const int node = nodeOf(child);
    if (node == 0) {
        return {};
    }
    const int above = m_nodes.at(static_cast<std::size_t>(node)).parent;
    if (above == 0) {
        return {};
    }
    return createIndex(m_nodes.at(static_cast<std::size_t>(above)).row, 0,
                       static_cast<quintptr>(above));
}

int PdfContentsModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid() && parent.column() != 0) {
        return 0;
    }
    return static_cast<int>(m_nodes.at(static_cast<std::size_t>(nodeOf(parent))).below.size());
}

int PdfContentsModel::columnCount(const QModelIndex& parent) const {
    Q_UNUSED(parent)
    return 1;
}

QVariant PdfContentsModel::data(const QModelIndex& index, int role) const {
    const int node = nodeOf(index);
    if (node == 0) {
        return {};
    }
    const Node& entry = m_nodes.at(static_cast<std::size_t>(node));
    switch (role) {
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
        return entry.title;
    case static_cast<int>(Role::Page):
        return entry.page;
    default:
        return {};
    }
}

int PdfContentsModel::nodeOf(const QModelIndex& index) const {
    if (!index.isValid() || index.model() != this || index.internalId() >= m_nodes.size()) {
        return 0;
    }
    return static_cast<int>(index.internalId());
}

}  // namespace kalahari::gui
