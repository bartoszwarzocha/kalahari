// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file mind_map_picker.h
/// @brief The small window over a mind map that picks from a list with a search: Add Node
/// (the kinds and what the book has), Connect To (the nodes of the map)

#ifndef KALAHARI_MINDMAP_MIND_MAP_PICKER_H
#define KALAHARI_MINDMAP_MIND_MAP_PICKER_H

#include <QColor>
#include <QFrame>
#include <QList>
#include <QString>

class QLineEdit;
class QListWidget;

namespace kalahari::mindmap {

/// @brief An item of the list of a picker
struct PickItem {
    QString text;   ///< "Amara"
    QString group;  ///< The heading it stands under: "Postacie"; empty: the first group
    QColor color;   ///< The dot before it; invalid: none

    bool operator==(const PickItem&) const = default;
};

/// @brief A list with a search that closes when an item is picked, when Esc is pressed or the
/// mouse clicks outside it
///
/// +---------------------------+
/// | [Search...              ] |
/// |  New                      |
/// |  ● Idea                   |
/// |  ● Question               |
/// |  Characters               |
/// |  ● Amara                  |
/// +---------------------------+
///
/// The letters typed filter the list, the arrows go through it and Enter picks the item: the
/// first one shown when none is current.
class MindMapPicker : public QFrame {
    Q_OBJECT

public:
    /// @param title What it picks, for the readers of the screen: "Add Node"
    /// @param items The items, each group in one run
    MindMapPicker(const QString& title, QList<PickItem> items, QWidget* parent);

    /// @brief Show it at @p globalPoint, within the screen, the search ready
    void popUp(const QPoint& globalPoint);

    /// @brief The search
    [[nodiscard]] QLineEdit* searchEdit() const { return m_search; }
    /// @brief The list
    [[nodiscard]] QListWidget* list() const { return m_list; }

    /// @brief Pick the item shown at row @p row of the list, as a click does
    void pickRow(int row);

signals:
    /// @brief Item @p index of the items was picked; the picker closes
    void picked(int index);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void fill(const QString& filter);

    QList<PickItem> m_items;
    QLineEdit* m_search = nullptr;
    QListWidget* m_list = nullptr;
};

}  // namespace kalahari::mindmap

#endif  // KALAHARI_MINDMAP_MIND_MAP_PICKER_H
