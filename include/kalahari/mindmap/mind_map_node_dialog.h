// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file mind_map_node_dialog.h
/// @brief Node Properties: the name of a node of a mind map, its kind, its note, the rows of
/// the book it shows, the writer's own attributes and its picture

#ifndef KALAHARI_MINDMAP_MIND_MAP_NODE_DIALOG_H
#define KALAHARI_MINDMAP_MIND_MAP_NODE_DIALOG_H

#include <kalahari/gui/dialogs/kalahari_dialog.h>
#include <kalahari/mindmap/mind_map.h>

#include <QList>
#include <QString>
#include <QStringList>

#include <utility>

class QComboBox;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QVBoxLayout;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::mindmap {

class MindMapContent;

/// @brief Node Properties
///
/// +--------------------------------------------------------------+
/// | Name:       [Amara                                 ]         |
/// | Kind:       [Idea v]               (the writer's nodes)      |
/// | Note:       [                                      ]         |
/// | Rows:       [x] Role                    [Up]                 |
/// |             [x] Family                  [Down]               |
/// |             [ ] Age                     [As at first]        |
/// | Attributes: [Motive     ] [the compass        ] [x]          |
/// |             [Add Attribute]                                  |
/// | Picture:    [as the settings say v]                          |
/// |                                          [Save]  [Cancel]    |
/// +--------------------------------------------------------------+
///
/// The rows are what the node shows of the book: the fields of its card and its relations, of
/// a text its status and words, of a scene its description, its point of view, its place and
/// its time, of a thread its events. The attributes are the writer's own: a name and a value,
/// the names of the other nodes of the map offered.
class MindMapNodeDialog : public gui::dialogs::KalahariDialog {
    Q_OBJECT

public:
    /// @param mapTitle The title of the map, which its main topic shows without its own name
    /// @param book The book, for the pictures of the Workshop; nullptr: none
    MindMapNodeDialog(const MindMap& map, const MindMapNode& node, const MindMapContent& content,
                      const QString& mapTitle, const core::ProjectManager* book,
                      QWidget* parent = nullptr);

    /// @brief The node as the writer changed it
    [[nodiscard]] MindMapNode node() const;

    [[nodiscard]] QLineEdit* nameEdit() const { return m_name; }
    [[nodiscard]] QComboBox* kindCombo() const { return m_kind; }
    [[nodiscard]] QPlainTextEdit* noteEdit() const { return m_note; }
    [[nodiscard]] QListWidget* rowList() const { return m_rows; }
    [[nodiscard]] QComboBox* imageCombo() const { return m_image; }
    [[nodiscard]] QPushButton* addAttributeButton() const { return m_addAttribute; }

    /// @brief Add the row of an attribute named @p name with value @p value
    void addAttribute(const QString& name, const QString& value);
    /// @brief The attributes as they are written, without those without a name
    [[nodiscard]] QList<NodeAttribute> attributes() const;

private:
    void moveRow(int by);
    void fillRows(const QStringList& shown);

    MindMapNode m_node;
    QString m_titleWithout;  ///< The name the node shows without its own
    QStringList m_defaultRows;
    QStringList m_attributeNames;
    QLineEdit* m_name = nullptr;
    QComboBox* m_kind = nullptr;
    QPlainTextEdit* m_note = nullptr;
    QListWidget* m_rows = nullptr;
    QVBoxLayout* m_attributeRows = nullptr;
    QList<std::pair<QLineEdit*, QLineEdit*>> m_attributes;
    QPushButton* m_addAttribute = nullptr;
    QComboBox* m_image = nullptr;
};

}  // namespace kalahari::mindmap

#endif  // KALAHARI_MINDMAP_MIND_MAP_NODE_DIALOG_H
