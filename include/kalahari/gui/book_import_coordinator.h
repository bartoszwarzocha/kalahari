// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_import_coordinator.h
/// @brief File > Import > DOCX, Markdown and Plain Text: the system window of the files, the
///        window of the import, and the texts added to the open book or to a new one

#pragma once

#include "kalahari/gui/book_import_plan.h"
#include "kalahari/gui/dialogs/import_book_dialog.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <functional>
#include <vector>

class QWidget;

namespace kalahari::core {
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::gui {

class CommandRegistry;

/// @brief Imports Word documents, Markdown and plain text into the book of the window or into
///        a new book
///
/// The commands work always: without an open book the import makes a new one. A command
/// opens the system window of the files first, then the window of the import. After the
/// import the Navigator shows the book as it is; a new book opens as a book of File > Open
/// Book does, when the window closes.
class BookImportCoordinator : public QObject {
    Q_OBJECT

   public:
    /// @brief What the coordinator does in the book window
    struct Window {
        std::function<void()> refresh;                    ///< Show the book again (the Navigator)
        std::function<void(const QString&)> showElement;  ///< Show an element in the Navigator
        std::function<void(const QString&)> openElement;  ///< Open a text in the editor
        std::function<void(const QString&)> openBook;     ///< Open a book (its .klh file)
    };

    /// @param book The book of the window
    /// @param commands The commands of the window
    /// @param window What the coordinator does in the book window
    /// @param parent The window over which the windows of the import open
    BookImportCoordinator(core::ProjectManager& book, CommandRegistry& commands, Window window,
                          QWidget* parent);

    BookImportCoordinator(const BookImportCoordinator&) = delete;
    BookImportCoordinator& operator=(const BookImportCoordinator&) = delete;

    /// @brief Give File > Import > DOCX, Markdown and Plain Text their callbacks
    /// @note Call after the commands are registered
    void connectCommands();

    /// @brief Choose the files of @p format in the system window, then import them
    void importFormat(ImportFormat format);

    /// @brief Open the window of the import of @p files of @p format
    void importFiles(ImportFormat format, const QStringList& files);

    /// @brief What the window says about the book of the window
    dialogs::ImportBookDialog::Book dialogBook() const;

    /// @brief Add the texts of the checked @p rows to @p book as @p choice says (the open book
    ///        or its Other Files): the new styles, the images and the texts
    ///
    /// When a text cannot be added, what the import added before goes, and the book is as it
    /// was.
    static bool importInto(core::ProjectManager& book,
                           const dialogs::ImportBookDialog::Choice& choice,
                           const std::vector<ImportFile>& files, const std::vector<ImportRow>& rows,
                           const ImportKinds& kinds,
                           const dialogs::ArchiveDialog::Progress& progress,
                           dialogs::ImportBookDialog::Outcome* outcome, QString* problem,
                           QStringList* details);

    /// @brief Make the new book of @p choice and add the texts to it; the book is closed after
    ///        it, and the outcome names its .klh file
    static bool importIntoNewBook(const dialogs::ImportBookDialog::Choice& choice,
                                  const std::vector<ImportFile>& files,
                                  const std::vector<ImportRow>& rows, const ImportKinds& kinds,
                                  const dialogs::ArchiveDialog::Progress& progress,
                                  dialogs::ImportBookDialog::Outcome* outcome, QString* problem,
                                  QStringList* details);

   private:
    core::ProjectManager& m_book;  ///< The book of its window
    CommandRegistry& m_commands;   ///< The commands of its window
    Window m_window;
    QWidget* m_parent;
};

}  // namespace kalahari::gui
