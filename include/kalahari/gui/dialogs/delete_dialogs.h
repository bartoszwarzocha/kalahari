// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file delete_dialogs.h
/// @brief The windows that delete an element from the book and that delete files: what goes,
/// and where its files go

#ifndef KALAHARI_GUI_DIALOGS_DELETE_DIALOGS_H
#define KALAHARI_GUI_DIALOGS_DELETE_DIALOGS_H

#include "kalahari/core/book_project.h"
#include "kalahari/core/book_type_registry.h"
#include "kalahari/core/kind_words.h"
#include "kalahari/gui/dialogs/kalahari_dialog.h"

#include <QList>
#include <QString>
#include <QStringList>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTreeWidget;
class QWidget;

namespace kalahari {
namespace core {
struct UnassignedFile;
} // namespace core

namespace gui::dialogs {

/// @brief The name of the system's Trash in the program's sentences: "Recycle Bin" on Windows,
/// "Trash" on other systems ("{bin}" in core/kind_words.h)
core::KindWords trashWords();

/// @brief Delete from Book: an element with the elements inside it, and their files if the
/// writer wants
///
/// The list shows what goes from the book, with the file of each text. By default the files
/// stay in the book's folder, and the window says where: the Navigator lists them under
/// "Unassigned Files", from where they go back to the book or are deleted. "Also delete the
/// files" sends them to the system's Trash, or deletes them for good from a folder without a
/// Trash (a network drive); the window says which before the writer chooses. What it says
/// about the files that stay and about the files that go takes the place of the longer of the
/// two: checking the box moves nothing in the window.
///
/// Cancel is the default button: deleting needs Delete or its mnemonic.
class DeleteFromBookDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief What the window shows
    struct Content {
        core::ProjectElement element;  ///< The element, with the elements inside it
        QStringList files;             ///< Their files on the disk (ProjectManager::filesOf())
        QString projectFolder;         ///< Folder of the project: the files of the elements are
                                       ///< relative to it
        bool trash = true;             ///< The files can go to the Trash (core::canMoveToTrash())
        bool unassigned = true;        ///< The Navigator lists the files that stay, under
                                       ///< "Unassigned Files" (ProjectManager::listsAsUnassigned())
        /// The name of a card in the book's dictionary (core::DictionaryName::text()); empty:
        /// none
        QString dictionaryName;
    };

    /// @brief Create the window
    /// @param content The element, its files and where they can go
    /// @param registry Kinds of the elements: their names and icons; it must outlive the window
    /// @param parent Parent widget
    DeleteFromBookDialog(const Content& content, const core::BookTypeRegistry& registry,
                         QWidget* parent = nullptr);

    /// @brief Whether the files go too ("Also delete the files" is checked)
    [[nodiscard]] bool deleteFiles() const;

    /// @brief Whether the name of the card stays in the book's dictionary, without the card
    ///        ("Keep the name in the book's dictionary" is checked); false without a name
    [[nodiscard]] bool keepName() const;

    /// @brief "Keep the name in the book's dictionary"; hidden without a name of a card
    [[nodiscard]] QCheckBox* keepNameBox() const { return m_keepNameBox; }

    /// @brief The text Copy puts on the clipboard: what the window says and lists
    [[nodiscard]] QString clipboardText() const;

    /// @brief "Also delete the files"; hidden when the elements have no files
    [[nodiscard]] QCheckBox* deleteFilesBox() const { return m_deleteFilesBox; }

    /// @brief The list of what goes from the book
    [[nodiscard]] QTreeWidget* list() const { return m_list; }

    /// @brief What the window says about the files: where they stay, or where they go when
    /// "Also delete the files" is checked
    [[nodiscard]] QLabel* filesNote() const { return deleteFiles() ? m_goText : m_stayText; }

    /// @brief The folder of the files that stay
    [[nodiscard]] QLineEdit* folderField() const { return m_folderField; }

    /// @brief Opens the folder of the files in the file manager of the system
    [[nodiscard]] QPushButton* showInFolderButton() const { return m_showInFolderButton; }

    /// @brief Copies what the window says to the clipboard
    [[nodiscard]] QPushButton* copyButton() const { return m_copyButton; }

private:
    /// @brief What the window says about the files, for the state of "Also delete the files"
    void updateFilesNote();

    /// @brief What the window says about the files when they stay (@p stay) or go
    [[nodiscard]] QString filesText(bool stay) const;

    /// @brief @p text with the names of the kinds in it: of the element ("{kind}"), of the
    /// elements inside it ("{child}"), of the elements whose files these are ("{owner}") and
    /// of the Trash ("{bin}")
    [[nodiscard]] QString named(const QString& text) const;

    Content m_content;
    const core::BookTypeRegistry* m_registry = nullptr;
    core::KindWords m_childWords;   ///< The elements inside it: their kind, or "element"
    core::KindWords m_ownerWords;   ///< The elements whose files these are
    QString m_title;
    QString m_description;
    QTreeWidget* m_list = nullptr;
    QCheckBox* m_deleteFilesBox = nullptr;
    QCheckBox* m_keepNameBox = nullptr;
    QStackedWidget* m_filesPages = nullptr;  ///< What it says when the files stay, and go
    QLabel* m_stayText = nullptr;
    QLabel* m_goText = nullptr;
    QLineEdit* m_folderField = nullptr;
    QPushButton* m_showInFolderButton = nullptr;
    QPushButton* m_copyButton = nullptr;
};

/// @brief Delete files that no element of the book has ("Unassigned Files" in the Navigator)
///
/// The window lists the files and says where they go: to the system's Trash, or nowhere from a
/// folder without a Trash. Cancel is the default button.
class DeleteFilesDialog : public KalahariDialog {
    Q_OBJECT

public:
    /// @brief Create the window
    /// @param files The files (ProjectManager::unassignedFiles()); not empty
    /// @param projectFolder Folder of the project: the files are relative to it
    /// @param trash The files can go to the Trash (core::canMoveToTrash())
    /// @param parent Parent widget
    DeleteFilesDialog(const QList<core::UnassignedFile>& files, const QString& projectFolder,
                      bool trash, QWidget* parent = nullptr);

    /// @brief The text Copy puts on the clipboard: what the window says and lists
    [[nodiscard]] QString clipboardText() const;

    /// @brief The list of the files
    [[nodiscard]] QTreeWidget* list() const { return m_list; }

    /// @brief What the window says about where the files go
    [[nodiscard]] QLabel* filesNote() const { return m_noteText; }

    /// @brief Copies what the window says to the clipboard
    [[nodiscard]] QPushButton* copyButton() const { return m_copyButton; }

private:
    QString m_title;
    QTreeWidget* m_list = nullptr;
    QLabel* m_noteText = nullptr;
    QPushButton* m_copyButton = nullptr;
};

} // namespace gui::dialogs

} // namespace kalahari

#endif  // KALAHARI_GUI_DIALOGS_DELETE_DIALOGS_H
