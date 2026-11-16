// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file pdf_models.h
/// @brief The search in a PDF file and its table of contents, as models of views

#ifndef KALAHARI_GUI_PDF_MODELS_H
#define KALAHARI_GUI_PDF_MODELS_H

#include "kalahari/gui/pdf_document.h"

#include <QAbstractItemModel>
#include <QAbstractListModel>
#include <QBasicTimer>
#include <QList>
#include <QString>

#include <vector>

class QTimerEvent;

namespace kalahari::gui {

/// @brief The hits of a text in a PDF file, a row each, in the order of the pages
///
/// The search goes through the pages a few at a time while the program waits: each page
/// that has hits adds them (rowsInserted()). A new text or document starts it again
/// (modelReset()).
class PdfSearchModel : public QAbstractListModel {
    Q_OBJECT

public:
    /// @brief What a row tells besides the words around the hit (Qt::DisplayRole)
    enum class Role {
        Page = Qt::UserRole + 1,  ///< The page of the hit, from 0 (int)
        ContextBefore,            ///< The words before it (QString)
        ContextAfter              ///< The words after it (QString)
    };

    /// @brief How long the search goes on at a time, in milliseconds
    static constexpr int SEARCH_SLICE = 20;

    /// @brief No document: no hits
    explicit PdfSearchModel(QObject* parent = nullptr);

    /// @brief Search @p document (not owned); nullptr: none
    void setDocument(PdfDocument* document);

    /// @brief The document searched
    [[nodiscard]] PdfDocument* document() const { return m_document; }

    /// @brief Search for @p text, in any case; empty: no search
    void setSearchString(const QString& text);

    /// @brief The text searched for
    [[nodiscard]] const QString& searchString() const { return m_text; }

    /// @brief Whether pages remain to be searched
    [[nodiscard]] bool isSearching() const { return m_timer.isActive(); }

    /// @brief The hits found so far
    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;

    /// @brief The page and the words around hit @p index
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;

    /// @brief Hit @p index; an empty one (page -1) when there is none
    [[nodiscard]] PdfHit resultAtIndex(int index) const;

    /// @brief The hits found on page @p page
    [[nodiscard]] QList<PdfHit> resultsOnPage(int page) const;

protected:
    /// @brief Search the next pages
    void timerEvent(QTimerEvent* event) override;

private:
    /// @brief Forget the hits and search from the first page
    void restart();

    PdfDocument* m_document = nullptr;  ///< Not owned; nullptr once destroyed
    QString m_text;
    QList<PdfHit> m_hits;  ///< In the order of the pages
    int m_nextPage = 0;
    QBasicTimer m_timer;
};

/// @brief The table of contents of a PDF file as a tree
class PdfContentsModel : public QAbstractItemModel {
    Q_OBJECT

public:
    /// @brief What an entry tells besides its title (Qt::DisplayRole)
    enum class Role {
        Page = Qt::UserRole + 1  ///< The page it goes to, from 0; -1: none (int)
    };

    /// @brief No document: no entries
    explicit PdfContentsModel(QObject* parent = nullptr);

    /// @brief Show the table of contents of @p document (not owned); nullptr: none
    void setDocument(PdfDocument* document);

    [[nodiscard]] QModelIndex index(int row, int column,
                                    const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QModelIndex parent(const QModelIndex& child) const override;
    [[nodiscard]] int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;

private:
    /// @brief An entry in the tree; entry 0 is the root, above the entries at the top
    struct Node {
        QString title;
        int page = -1;
        int parent = 0;          ///< Its entry above
        int row = 0;             ///< Its row under it
        std::vector<int> below;  ///< Its entries below, in order
    };

    /// @brief The entry of @p index; 0 (the root) for an invalid one
    [[nodiscard]] int nodeOf(const QModelIndex& index) const;

    std::vector<Node> m_nodes;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_PDF_MODELS_H
