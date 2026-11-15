// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_export_coordinator.h
/// @brief File > Export > DOCX and Markdown: the window of the export, and the book as the
///        program has it (the open texts with their unsaved changes) for the file

#pragma once

#include "kalahari/editor/book_export.h"
#include "kalahari/gui/dialogs/export_book_dialog.h"

#include <QHash>
#include <QObject>
#include <QString>

#include <functional>

class QTabWidget;
class QWidget;

namespace kalahari::core {
class IExporter;
class ProjectManager;
}  // namespace kalahari::core

namespace kalahari::editor {
class BookEditor;
}

namespace kalahari::gui {

class CaptionCoordinator;
class CommandRegistry;
class EditorPanel;
class FootnoteCoordinator;

/// @brief Exports the book, some of its texts or the text in front to a Word document or to
///        Markdown
///
/// The texts go in reading order, the open ones as their editors have them and the others as
/// the program keeps them (an unsaved text, else its file), each with the numbers of its
/// captions and notes in the book around it. The commands work with a book open or a text in
/// front; without either they are greyed out with the reason.
class BookExportCoordinator : public QObject {
    Q_OBJECT

public:
    /// @brief The editor of the tab in front; nullptr: none
    using CurrentEditor = std::function<EditorPanel*()>;

    /// @param book The book of the window
    /// @param commands The commands of the window
    /// @param centralTabs The tabs of the book window (DocumentWindows gives all the texts)
    /// @param currentEditor The editor of the tab in front
    /// @param captions The numbers of the captions of the book
    /// @param footnotes The numbers of the notes of the book
    /// @param window Over which the window opens
    /// @param parent Parent object
    BookExportCoordinator(core::ProjectManager& book, CommandRegistry& commands,
                          QTabWidget* centralTabs, CurrentEditor currentEditor,
                          const CaptionCoordinator* captions, const FootnoteCoordinator* footnotes,
                          QWidget* window, QObject* parent = nullptr);

    BookExportCoordinator(const BookExportCoordinator&) = delete;
    BookExportCoordinator& operator=(const BookExportCoordinator&) = delete;

    /// @brief Give File > Export > DOCX and Markdown their callbacks and their states
    /// @note Call after the commands are registered
    void connectCommands();

    /// @brief Show the states of the commands again: a book opened or closed, another tab in
    ///        front
    void updateCommands();

    /// @brief Why the commands are greyed out; empty when they work
    QString whyNotExport() const;

    /// @brief Open the window of the export, with @p format chosen
    void exportBook(editor::ExportFormat format);

    /// @brief What the window says about the book and the text in front
    dialogs::ExportBookDialog::Book dialogBook() const;

    /// @brief The export of @p choice: the texts of the book as the program has them, the
    ///        styles, the page and the options
    editor::BookExport bookExport(const dialogs::ExportBookDialog::Choice& choice) const;

    /// @brief The page of the settings of the pages (Settings > Editor > Pages and Margins)
    static editor::ExportPage pageOfSettings();

    /// @brief Write @p path in the format @p formatId of a plugin's @p exporter, which reads
    ///        its options from its tab of the window; false, with what went wrong in
    ///        @p problem, when it cannot
    static bool exportWithPlugin(core::IExporter& exporter, const QString& formatId,
                                 const QString& path,
                                 const dialogs::ArchiveDialog::Progress& progress,
                                 editor::ExportResult* result, editor::ExportProblem* problem);

private:
    /// @brief The open texts of the book, by their elements, as their editors have them
    QHash<QString, const editor::BookEditor*> openEditors() const;

    core::ProjectManager& m_book;  ///< The book of its window
    CommandRegistry& m_commands;   ///< The commands of its window
    QTabWidget* m_centralTabs;
    CurrentEditor m_currentEditor;
    const CaptionCoordinator* m_captions;
    const FootnoteCoordinator* m_footnotes;
    QWidget* m_window;
};

}  // namespace kalahari::gui
