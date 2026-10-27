/// @file save_type_dialog.h
/// @brief Save as Type: the kinds of a book, saved as a book type of the writer's own

#pragma once

#include "kalahari/core/custom_book_type.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QString>

class QLabel;
class QLineEdit;
class QPlainTextEdit;

namespace kalahari {
namespace gui {
namespace dialogs {

/// @brief "Save as Type": a book type of the writer's own, made of the kinds of a book
///
/// +--------------------------------------------------------------+
/// | [icon] Save as Type                                           |
/// |        The type will be in the New Book window.               |
/// +--------------------------------------------------------------+
/// | Name                                                          |
/// | [Historical novel                                          ]  |
/// | Description                                                   |
/// | [A novel with a bibliography and an annex.                 ]  |
/// | Type: Novel, with the kinds of other types: bibliography      |
/// | (Non-fiction).                                                |
/// | To start with: "Title page", "Chapter 1".                     |
/// |                                             [Save] [Cancel]   |
/// +--------------------------------------------------------------+
///
/// Under the fields the window says what the type is: its book type and the kinds it takes
/// from other types, or, without a type, how many kinds it has, and what a new book of it
/// starts with. Save writes the type to the folder of the writer's own types
/// (core::CustomBookTypes), and the New Book window offers it after the book types. The
/// description may be empty. When a type of the same name is there, the window asks whether
/// to replace it; the replaced type keeps its file.
class SaveTypeDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief The window that saves @p type under the name the writer gives it
    /// @param type The type: its book type, its kinds and its elements to start with
    /// @param registry Packages of its kinds, for what the window says of it
    /// @param parent Parent widget
    SaveTypeDialog(const core::CustomBookType& type, const core::BookTypeRegistry& registry,
                   QWidget* parent = nullptr);

    /// @brief Save the type to @p folder instead of the folder of the writer's own types
    /// (core::CustomBookTypes::folder()), for tests
    void setFolder(const QString& folder);

    /// @brief The saved type, with its id and its file
    /// @note Only valid after the window is accepted
    const core::CustomBookType& savedType() const { return m_type; }

    /// @brief The name of the type
    QLineEdit* nameEdit() const { return m_nameEdit; }

    /// @brief The writer's sentence about the type
    QPlainTextEdit* descriptionEdit() const { return m_descriptionEdit; }

    /// @brief What the type is and what a new book of it starts with
    QLabel* summaryLabel() const { return m_summaryLabel; }

public slots:
    /// @brief Save: asks before it replaces a type of the same name, writes the file, then
    /// closes the window; a file that cannot be written keeps it open
    void accept() override;

private:
    /// @brief What the type is, as rich text
    QString summary() const;

    core::CustomBookType m_type;
    const core::BookTypeRegistry& m_registry;
    QString m_folder;
    QLineEdit* m_nameEdit = nullptr;
    QPlainTextEdit* m_descriptionEdit = nullptr;
    QLabel* m_summaryLabel = nullptr;
};

} // namespace dialogs
} // namespace gui
} // namespace kalahari
