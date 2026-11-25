// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file book_export_coordinator.h
/// @brief File > Export > DOCX, PDF, Markdown and EPUB, and File > Print: the windows of the
///        export and of the print, and the book as the program has it (the open texts with
///        their unsaved changes) for the file or the printer

#ifndef KALAHARI_GUI_BOOK_EXPORT_COORDINATOR_H
#define KALAHARI_GUI_BOOK_EXPORT_COORDINATOR_H

#include "kalahari/editor/book_export.h"
#include "kalahari/gui/dialogs/export_book_dialog.h"
#include "kalahari/gui/dialogs/print_book_dialog.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>

#include <QStringList>

#include <functional>
#include <optional>
#include <vector>

class QPrinter;
class QTabWidget;
class QWidget;

namespace kalahari::core {
class IExporter;
class ProjectManager;
struct ProjectBook;
struct ProjectElement;
}  // namespace kalahari::core

namespace kalahari::editor {
class BookEditor;
} // namespace kalahari::editor

namespace kalahari::gui {

class CaptionCoordinator;
class CommandRegistry;
class EditorPanel;
class FootnoteCoordinator;

/// @brief Exports the book, some of its texts or the text in front to a Word document, a PDF
///        document, Markdown or an EPUB e-book, or prints them
///
/// The texts go in reading order, the open ones as their editors have them and the others as
/// the program keeps them (an unsaved text, else its file), each with the numbers of its
/// captions and notes in the book around it. A PDF has the colors of the light paper; an EPUB
/// the chosen design of the front of the cover and the chosen description of the materials of
/// the cover, and the ISBN and the publisher the book keeps (given in the window). The print
/// has the pages of a PDF without its links, on the printers of the system. The commands work
/// with a book open or a text in front; without either they are greyed out with the reason.
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

    /// @brief Give File > Export > DOCX, PDF, Markdown and EPUB, and File > Print their
    ///        callbacks and their states
    /// @note Call after the commands are registered
    void connectCommands();

    /// @brief Show the states of the commands again: a book opened or closed, another tab in
    ///        front
    void updateCommands();

    /// @brief Why the commands of the export are greyed out; empty when they work
    [[nodiscard]] QString whyNotExport() const;

    /// @brief Why File > Print is greyed out; empty when it works
    QString whyNotPrint() const;

    /// @brief Open the window of the export, with @p format chosen
    void exportBook(editor::ExportFormat format);

    /// @brief Open the window of the export with the set for a publisher chosen, as a Word
    ///        document (Export Set... of the set)
    void exportSet();

    /// @brief Open the window of the print, with the printers of the system
    void printBook();

    /// @brief Open the window of the print with the set for a publisher chosen (Print Set... of
    ///        the card For Publishers)
    void printSet();

    /// @brief What the window says about the book and the text in front
    /// @param volumeId The volume of a series; empty: the current book
    [[nodiscard]] dialogs::ExportBookDialog::Book dialogBook(
        const QString& volumeId = QString()) const;

    /// @brief The export of @p choice: the texts of the book as the program has them, the
    ///        styles, the page and the options
    [[nodiscard]] editor::BookExport bookExport(
        const dialogs::ExportBookDialog::Choice& choice) const;

    /// @brief The export of the texts and the layout of @p layout, as a PDF has them: the
    ///        texts of the book as the program has them, the styles and the page
    editor::BookExport bookExport(const dialogs::BookOutputDialog::Layout& layout) const;

    /// @brief The pages of the texts of @p layout for the print, without links, as on paper;
    ///        the page of the cursor of the text in front among them
    dialogs::PrintBookDialog::LaidOut printedPages(
        const dialogs::BookOutputDialog::Layout& layout) const;

    /// @brief The printers of the system, the default one first, with what each can do
    static QList<dialogs::PrintBookDialog::Printer> systemPrinters();

    /// @brief Print @p sides of @p pages on @p printer as @p choice says: the paper, which
    ///        way it lies, both sides, the color; a side a page of the printer
    /// @param progress Told the sides printed so far; false stops the print
    /// @return false, with what went wrong in @p problem, when it cannot
    static bool printOn(QPrinter& printer, const dialogs::PrintBookDialog::Choice& choice,
                        const editor::BookPages& pages,
                        const std::vector<editor::SheetSide>& sides,
                        const std::function<bool(int printed)>& progress, QStringList* problem);

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
    /// @brief Open the window of the print with, when given, @p content chosen
    void printContent(std::optional<dialogs::BookOutputDialog::Content> content);

    /// @brief Open the window of the export with @p format and, when given, @p content chosen
    void exportContent(editor::ExportFormat format,
                       std::optional<dialogs::BookOutputDialog::Content> content);

    /// @brief Add to @p book the set for a publisher of book @p volume of the project: the
    ///        title of the book with its author, then each part with a text under its name
    void addSetItems(const core::ProjectBook& volume, editor::BookExport& book) const;

    /// @brief Write @p path as @p choice says, with a plugin's exporter or the program's own;
    ///        false, with what went wrong in @p problem, when it cannot
    bool writeExport(const QString& path, const dialogs::ExportBookDialog::Choice& choice,
                     const dialogs::ArchiveDialog::Progress& progress,
                     editor::ExportResult* result, editor::ExportProblem* problem);

    /// @brief Write @p book to @p path in the program's own format of @p choice
    bool writeFile(const editor::BookExport& book, const QString& path,
                   const dialogs::ExportBookDialog::Choice& choice,
                   const dialogs::ArchiveDialog::Progress& progress,
                   editor::ExportResult* result, editor::ExportProblem* problem);

    /// @brief The open texts of the book, by their elements, as their editors have them
    [[nodiscard]] QHash<QString, const editor::BookEditor*> openEditors() const;

    /// @brief A text of the book: as its editor in @p open has it, else as the program keeps
    ///        it, with the numbers of its captions and notes
    editor::ExportItem textItem(const core::ProjectElement& element,
                                const QHash<QString, const editor::BookEditor*>& open) const;

    /// @brief A spread of the book with its look, its caption numbered in the book around it
    editor::ExportItem spreadItem(const core::ProjectElement& element) const;

    /// @brief Add the text in front to @p book: of the book, or a file outside it with its
    ///        own title, language and images; or the spread in front
    void addFrontText(const QHash<QString, const editor::BookEditor*>& open,
                      editor::BookExport& book) const;

    /// @brief Add to @p items the texts of @p elements in reading order, each part with its
    ///        title when any of its texts goes
    /// @param chosen The ids of the texts that go; null: all of them
    void addItems(const QList<core::ProjectElement>& elements, const QSet<QString>* chosen,
                  const QHash<QString, const editor::BookEditor*>& open,
                  std::vector<editor::ExportItem>& items) const;

    core::ProjectManager& m_book;  ///< The book of its window
    CommandRegistry& m_commands;   ///< The commands of its window
    QTabWidget* m_centralTabs;
    CurrentEditor m_currentEditor;
    const CaptionCoordinator* m_captions;
    const FootnoteCoordinator* m_footnotes;
    QWidget* m_window;
};

}  // namespace kalahari::gui

#endif  // KALAHARI_GUI_BOOK_EXPORT_COORDINATOR_H
