// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_list.h
/// @brief A mind map as a list with indents: its nodes as an outline, worked with the keyboard

#ifndef KALAHARI_MINDMAP_MIND_MAP_LIST_H
#define KALAHARI_MINDMAP_MIND_MAP_LIST_H

#include <kalahari/mindmap/mind_map_look.h>

#include <QHash>
#include <QString>
#include <QTreeWidget>

#include <functional>
#include <utility>

class QMenu;

namespace kalahari::mindmap {

class MindMapDocument;

/// @brief A mind map as a list with indents, the loose nodes under it
///
/// Its keys are those of the map where they make sense in a list: Tab and Shift+Tab put a
/// node a level down and up, Insert adds a node inside it, Enter one after it (Shift+Enter
/// before it), F2 or typing changes its text, Space and the arrows to the sides collapse and
/// expand it, Delete removes it, Ctrl+Up and Ctrl+Down move it, Ctrl+Z and Ctrl+Y undo and
/// redo.
class MindMapList : public QTreeWidget {
    Q_OBJECT

public:
    /// The title of element @p elementId of the book
    using ElementTitle = std::function<QString(const QString& elementId)>;

    /// @brief The list of @p document
    explicit MindMapList(MindMapDocument& document, QWidget* parent = nullptr);

    /// @brief Show the map again after it changed (soon: not while a text is being changed)
    void refresh();

    /// @brief How the list looks: the colors of the branches and of the markers
    void setLook(const MindMapLook& look);

    /// @brief Dim the nodes that do not match @p filter: a marker, "link", "note"; empty: none
    void setHighlight(const QString& filter);

    /// @brief The titles of the elements the nodes are about
    void setElementTitle(ElementTitle elementTitle) { m_elementTitle = std::move(elementTitle); }

    /// @brief Change the text of node @p id in its row
    void startEditing(const QString& id);

    /// @brief What the delegate paints
    [[nodiscard]] const MindMapLook& look() const { return m_look; }
    [[nodiscard]] bool matches(const QString& id) const;
    [[nodiscard]] QString elementTitle(const QString& elementId) const;
    [[nodiscard]] const MindMapDocument& document() const { return m_document; }

    /// @brief The role of the items that holds the id of their node
    static constexpr int ID_ROLE = Qt::UserRole + 1;

signals:
    /// @brief Open element @p elementId of the book (Ctrl+Enter)
    void openElementRequested(const QString& elementId);
    /// @brief The note of node @p id, in its window
    void noteRequested(const QString& id);
    /// @brief Choose the element of the book that node @p id is about
    void linkRequested(const QString& id);
    /// @brief A message for the bar under the list; with @p undoable it offers Undo
    void told(const QString& message, bool undoable);

protected:
    bool event(QEvent* event) override;
    bool focusNextPrevChild(bool next) override;
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void closeEditor(QWidget* editor, QAbstractItemDelegate::EndEditHint hint) override;

private:
    void rebuild();
    void addBranch(QTreeWidgetItem* parent, const QString& id);
    void showSelected();
    void addChild();
    void addSibling(bool before);
    void editNew(const QString& id);
    void level(bool down);
    void fillMenu(QMenu& menu);
    [[nodiscard]] QString selectedId() const;

    MindMapDocument& m_document;
    MindMapLook m_look;
    QString m_highlight;
    ElementTitle m_elementTitle;
    QHash<QString, QTreeWidgetItem*> m_items;
    bool m_filling = false;        ///< The rows are being made: their signals change nothing
    bool m_refreshing = false;     ///< A refresh is on its way
    QString m_editAfter;           ///< The node whose text is changed once the rows are made
    QString m_editing;             ///< The node whose text is being changed
    bool m_editingNew = false;     ///< A node just added: it goes when nothing is written in it
};

}  // namespace kalahari::mindmap

#endif  // KALAHARI_MINDMAP_MIND_MAP_LIST_H
