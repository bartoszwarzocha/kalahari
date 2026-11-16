// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_import_coordinator.cpp
/// @brief File > Import > DOCX, Markdown and Plain Text: the system window of the files, the
///        window of the import, and the texts added to the open book or to a new one

#include "kalahari/gui/book_import_coordinator.h"

#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/gui/book_styles.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/section_words.h"
#include "kalahari/gui/utils/program_folders.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>

#include <algorithm>
#include <utility>

namespace kalahari::gui {

namespace {

constexpr const char* DOCX_COMMAND = "file.import.docx";
constexpr const char* MARKDOWN_COMMAND = "file.import.markdown";
constexpr const char* TEXT_COMMAND = "file.import.text";

/// Progress of the import: the styles and the images up to this, then the texts
constexpr int FILES_PROGRESS = 40;

/// The texts and the parts of @p texts, those inside the parts too
void countTexts(const std::vector<ImportedText>& texts, int* count, int* parts) {
    for (const ImportedText& text : texts) {
        if (text.kind && text.kind.kind->form == core::ElementForm::Group) {
            ++*parts;
            countTexts(text.texts, count, parts);
        } else {
            ++*count;
        }
    }
}

/// The first text of the elements the import added: a part's first text
QString firstTextOf(const core::ProjectManager& book, const QStringList& added) {
    for (const QString& id : added) {
        const core::ProjectElement* element = book.findElement(id);
        if (element == nullptr) {
            continue;
        }
        if (book.formOf(*element) != core::ElementForm::Group) {
            return id;
        }
        if (!element->elements.isEmpty()) {
            return element->elements.front().id;
        }
    }
    return QString();
}

/// Add the texts to @p book: the new styles of the mapping, the images of the files, then the
/// texts at @p place; with @p newBook, in place of the elements a new book starts with
bool addTexts(core::ProjectManager& book, const dialogs::ImportBookDialog::Choice& choice,
              const std::vector<ImportFile>& files, const std::vector<ImportRow>& rows,
              const ImportKinds& kinds, ImportPlace place, bool newBook,
              const dialogs::ArchiveDialog::Progress& progress,
              dialogs::ImportBookDialog::Outcome* outcome, QString* problem, QStringList* details) {
    auto& logger = core::Logger::getInstance();
    ImportedElements& added = outcome->added;

    // The new styles of the book the writer chose for styles of the files
    QHash<QString, QString> mapping = choice.styles;
    editor::ParagraphStyleSet styles = choice.kml.bookStyles;
    const QList<core::BookParagraphStyle> made = newBookStyles(files, &mapping, styles);
    if (!made.isEmpty()) {
        QList<core::BookParagraphStyle> entries = book.paragraphStyles();
        entries += made;
        if (!book.setParagraphStyles(entries)) {
            //: In Polish: Kalahari nie może dodać nowych stylów do książki.
            *problem = BookImportCoordinator::tr("Kalahari cannot add the new styles to the book.");
            *details << QStringLiteral("Import: the new styles could not be saved");
            return false;
        }
        for (const core::BookParagraphStyle& style : made) {
            added.styles << style.id;
        }
        outcome->styles = static_cast<int>(made.size());
        styles = bookParagraphStyles(book);
    }

    // The images of each file in the book's folder of images, and how its texts are written
    std::vector<editor::ImportKmlOptions> options;
    for (size_t i = 0; i < files.size(); ++i) {
        editor::ImportKmlOptions each = choice.kml;
        each.bookStyles = styles;
        if (files[i].isRead()) {
            each.imageSources = saveImportedImages(book, *files[i].document, &added.images);
            each.styles = fileMapping(*files[i].document, mapping);
        }
        options.push_back(std::move(each));
        if (progress) {
            progress(static_cast<int>(FILES_PROGRESS * (i + 1) / std::max<size_t>(1, files.size())));
        }
    }
    outcome->images = static_cast<int>(added.images.size());

    std::vector<ImportedText> texts = importedTexts(files, rows, kinds, options);
    if (newBook) {
        clearNewBookStart(book, texts);
        place.at = {core::BookPlace::Main, QString(),
                    book.book() ? book.book()->mainElements.size() : 0};
    }
    countTexts(texts, &outcome->texts, &outcome->parts);
    if (place.otherFiles) {
        outcome->parts = 0;  // the texts of the parts one after another
    }
    if (!addImportedTexts(book, texts, place, &added)) {
        // What the import added before goes: the book is as it was
        //: In Polish: Nie udało się dodać do książki wszystkich tekstów, więc to, co import
        //: dodał, zostało usunięte. Import przerywa rodzaj tekstu, którego książka nie może mieć
        //: w tym miejscu, albo plik, którego nie można zapisać.
        *problem = BookImportCoordinator::tr(
            "Kalahari could not add every text to the book, so it took out those it had added. A "
            "kind of text "
            "the book cannot have there, or a file that cannot be written, stops the import.");
        *details << QStringLiteral(
            "Import: addImportedTexts() failed at a text; the log has which");
        logger.error("Import: a text could not be added; the import is taken out");
        removeImported(book, added);
        *outcome = dialogs::ImportBookDialog::Outcome();
        return false;
    }
    if (progress) {
        progress(100);
    }
    outcome->firstText = firstTextOf(book, added.elements);
    logger.info("Import: {} texts in {} parts, {} images, {} new styles", outcome->texts,
                outcome->parts, outcome->images, outcome->styles);
    return true;
}

}  // namespace

BookImportCoordinator::BookImportCoordinator(core::ProjectManager& book, CommandRegistry& commands,
                                             Window window, QWidget* parent)
    : QObject(parent),
      m_book(book),
      m_commands(commands),
      m_window(std::move(window)),
      m_parent(parent) {}

void BookImportCoordinator::connectCommands() {
    for (const auto& [id, format] : {std::pair{DOCX_COMMAND, ImportFormat::Docx},
                                     std::pair{MARKDOWN_COMMAND, ImportFormat::Markdown},
                                     std::pair{TEXT_COMMAND, ImportFormat::PlainText}}) {
        if (Command* command = m_commands.getCommand(id)) {
            command->execute = [this, format = format]() {
                importFormat(format);
            };
        }
    }
}

void BookImportCoordinator::importFormat(ImportFormat format) {
    QStringList patterns;
    for (const QString& extension : importExtensions(format)) {
        patterns << QLatin1Char('*') + extension;
    }
    QString name;
    switch (format) {
        case ImportFormat::Docx:
            //: The files of the system window of File > Import > DOCX. In Polish: Dokumenty Worda
            name = tr("Word documents");
            break;
        case ImportFormat::Markdown:
            //: The files of the system window of File > Import > Markdown. In Polish: Pliki
            //: Markdown
            name = tr("Markdown files");
            break;
        case ImportFormat::PlainText:
            //: The files of the system window of File > Import > Plain Text. In Polish: Pliki
            //: zwykłego tekstu
            name = tr("Plain text files");
            break;
    }
    //: The title of the system window that chooses the files of the import. In Polish: Pliki
    //: do importu
    const QStringList files = QFileDialog::getOpenFileNames(
        m_parent, tr("Files to Import"),
        ProgramFolders::startFolder(ProgramFolders::Operation::ImportText),
        QStringLiteral("%1 (%2)").arg(name, patterns.join(QLatin1Char(' '))));
    if (files.isEmpty()) {
        return;
    }
    ProgramFolders::remember(ProgramFolders::Operation::ImportText,
                             QFileInfo(files.front()).absolutePath());
    importFiles(format, files);
}

void BookImportCoordinator::importFiles(ImportFormat format, const QStringList& files) {
    dialogs::ImportBookDialog::Actions actions;
    actions.import = [this](const dialogs::ImportBookDialog::Choice& choice,
                            const std::vector<ImportFile>& read, const std::vector<ImportRow>& rows,
                            const ImportKinds& kinds,
                            const dialogs::ArchiveDialog::Progress& progress,
                            dialogs::ImportBookDialog::Outcome* outcome, QString* problem,
                            QStringList* details) {
        if (choice.target == dialogs::ImportBookDialog::Target::NewBook) {
            return importIntoNewBook(choice, read, rows, kinds, progress, outcome, problem, details);
        }
        const bool done = importInto(m_book, choice, read, rows, kinds, progress, outcome, problem,
                                     details);
        if (done && m_window.refresh) {
            m_window.refresh();
        }
        return done;
    };
    actions.undo = [this](const dialogs::ImportBookDialog::Outcome& outcome) {
        const bool undone = removeImported(m_book, outcome.added);
        core::Logger::getInstance().info("Import: undone{}",
                                         undone ? "" : ", but not every file was deleted");
        if (m_window.refresh) {
            m_window.refresh();
        }
        return undone;
    };
    actions.showInNavigator = [this](const dialogs::ImportBookDialog::Outcome& outcome) {
        if (m_window.showElement && !outcome.added.elements.isEmpty()) {
            m_window.showElement(outcome.added.elements.front());
        }
    };
    actions.openFirstText = [this](const dialogs::ImportBookDialog::Outcome& outcome) {
        if (m_window.openElement && !outcome.firstText.isEmpty()) {
            m_window.openElement(outcome.firstText);
        }
    };
    dialogs::ImportBookDialog dialog(dialogBook(), core::ProjectManager::installedBookTypes(),
                                     format, files, std::move(actions), m_parent);
    dialog.exec();

    // A new book opens when the window closes, as a book of File > Open Book
    const QString made = dialog.outcome().bookFile;
    if (!made.isEmpty() && m_window.openBook) {
        m_window.openBook(made);
    }
}

dialogs::ImportBookDialog::Book BookImportCoordinator::dialogBook() const {
    dialogs::ImportBookDialog::Book book;
    const core::ProjectManager& pm = m_book;
    const core::BookProject* project = pm.isProjectOpen() ? pm.project() : nullptr;
    const core::ProjectBook* projectBook = project != nullptr ? pm.book() : nullptr;
    if (projectBook == nullptr) {
        return book;
    }
    //: The title of a book without one. In Polish: Książka
    book.title = projectBook->title.trimmed().isEmpty() ? tr("Book") : projectBook->title;
    book.language = projectBook->language;
    book.readOnly = pm.isReadOnly();
    for (const core::BookPlace place :
         {core::BookPlace::Front, core::BookPlace::Main, core::BookPlace::Back}) {
        // A book without sections has no rows of its parts
        book.sections.append(
            {place, projectBook->partsLayer ? projectBook->sectionName(place) : QString(),
             SectionWords::forPart(projectBook, place), project->elementsIn(place)});
    }
    book.kinds = importKindsOf(pm);
    if (book.kinds.main) {
        book.place = pm.newPlaceOf(book.kinds.main, core::BookPlace::Main);
    }
    book.styles = bookParagraphStyles(pm);
    return book;
}

bool BookImportCoordinator::importInto(core::ProjectManager& book,
                                       const dialogs::ImportBookDialog::Choice& choice,
                                       const std::vector<ImportFile>& files,
                                       const std::vector<ImportRow>& rows, const ImportKinds& kinds,
                                       const dialogs::ArchiveDialog::Progress& progress,
                                       dialogs::ImportBookDialog::Outcome* outcome,
                                       QString* problem, QStringList* details) {
    if (!book.isProjectOpen() || book.isReadOnly()) {
        //: In Polish: Żadna książka nie jest otwarta do zmian.
        *problem = tr("No book is open for changes.");
        return false;
    }
    ImportPlace place;
    place.otherFiles = choice.target == dialogs::ImportBookDialog::Target::OtherFiles;
    place.at = choice.place;
    return addTexts(book, choice, files, rows, kinds, place, false, progress, outcome, problem,
                    details);
}

bool BookImportCoordinator::importIntoNewBook(const dialogs::ImportBookDialog::Choice& choice,
                                              const std::vector<ImportFile>& files,
                                              const std::vector<ImportRow>& rows,
                                              const ImportKinds& kinds,
                                              const dialogs::ArchiveDialog::Progress& progress,
                                              dialogs::ImportBookDialog::Outcome* outcome,
                                              QString* problem, QStringList* details) {
    core::NewProject request;
    request.parentDir = choice.newBook.folder;
    request.title = choice.newBook.title;
    request.author = choice.newBook.author;
    request.language = choice.newBook.language;
    request.typeId = choice.newBook.typeId;
    request.createSubfolder = true;

    // The book of its own, closed after the import: the window opens it as any other book
    core::ProjectManager book;
    QStringList problems;
    if (!book.createProject(request, &problems)) {
        //: %1: the title of the new book. In Polish: Kalahari nie może utworzyć książki „%1”.
        *problem = tr("Kalahari cannot make the book \"%1\".").arg(request.title);
        *details = problems;
        return false;
    }
    const QString manifest = book.getManifestPath();
    ProgramFolders::remember(ProgramFolders::Operation::NewBook, choice.newBook.folder);
    ImportPlace place;
    const bool done = addTexts(book, choice, files, rows, kinds, place, true, progress, outcome,
                               problem, details);
    book.closeProject(true);
    if (!done) {
        //: After the texts that could not be added to a new book; %1: its folder. In Polish:
        //: Książka powstała w folderze %1 bez tych tekstów.
        problem->append(QLatin1Char(' ') +
                        tr("The book was made in the folder %1 without them.")
                            .arg(QDir::toNativeSeparators(QFileInfo(manifest).absolutePath())));
        return false;
    }
    outcome->bookFile = manifest;
    outcome->added = ImportedElements();  // a new book has no Undo Import: it can be deleted
    return true;
}

}  // namespace kalahari::gui
