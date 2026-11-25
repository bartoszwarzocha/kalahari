// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file element_choice_dialog.h
/// @brief Choose an element of the book: a list of the book and of the Workshop with a filter

#pragma once

#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QString>
#include <QStringList>

class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;

namespace kalahari {
namespace core {
class ProjectManager;
struct BookProject;
struct ProjectElement;
}  // namespace core

namespace gui {
namespace dialogs {

/// @brief Choose one element of the open book: what a node of a mind map is about, a chapter
/// an event of a timeline is told in...
///
/// The list has the book with its elements and the Workshop with its elements, the groups
/// with the elements inside them; the book and the Workshop cannot be chosen. Typing filters
/// the list, also while the list has the keys; the arrows choose, Enter chooses and Esc
/// closes. A double click chooses.
class ElementChoiceDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief The list of the book of @p book
    /// @param title The title of the window ("Element of the Node")
    /// @param description The sentence under it
    /// @param current The element chosen now; empty: none
    /// @param excluded Elements that cannot be chosen (the map itself)
    ElementChoiceDialog(const core::ProjectManager& book, const QString& title,
                        const QString& description, const QString& current,
                        const QStringList& excluded, QWidget* parent = nullptr);

    /// @brief The element chosen; empty: none
    [[nodiscard]] QString chosen() const;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void addElements(QTreeWidgetItem* parent, const QList<core::ProjectElement>& elements);

    /// @brief The node of the book, or of each volume of a series, with its elements
    void addBooks(const core::BookProject& project);
    void applyFilter(const QString& text);
    void updateAccept();

    const core::ProjectManager& m_book;
    QStringList m_excluded;
    QLineEdit* m_filter = nullptr;
    QTreeWidget* m_tree = nullptr;
};

}  // namespace dialogs
}  // namespace gui
}  // namespace kalahari
