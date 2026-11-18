// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_export_coordinator.cpp
/// @brief File > Export > DOCX, PDF, Markdown and EPUB, and File > Print: the windows of the
///        export and of the print, and the book as the program has it (the open texts with
///        their unsaved changes) for the file or the printer

#include "kalahari/gui/book_export_coordinator.h"
#include "kalahari/gui/plugin_coordinator.h"

#include "kalahari/core/cover_material.h"
#include "kalahari/core/extension_points.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/book_pages.h"
#include "kalahari/editor/book_print.h"
#include "kalahari/editor/clipboard_handler.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/gui/book_styles.h"
#include "kalahari/gui/caption_coordinator.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/gui/footnote_coordinator.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/publishing_set.h"
#include "kalahari/gui/utils/language_names.h"
#include "kalahari/gui/utils/program_folders.h"

#include <QFileInfo>
#include <QPageLayout>
#include <QPrinter>
#include <QPrinterInfo>
#include <QSet>
#include <QTabWidget>
#include <QTextBlock>
#include <QTextDocument>
#include <QUuid>

#include <algorithm>
#include <memory>
#include <utility>

namespace kalahari::gui {

namespace {

constexpr const char* DOCX_COMMAND = "file.export.docx";
constexpr const char* PDF_COMMAND = "file.export.pdf";
constexpr const char* MARKDOWN_COMMAND = "file.export.markdown";
constexpr const char* EPUB_COMMAND = "file.export.epub";
constexpr const char* PRINT_COMMAND = "file.print";

/// The style of the title of a part (a group of the book), when the book has it
const QString PART_TITLE_STYLE = QStringLiteral("part_title");

/// The styles of the title of the book and of the names of the parts of the set for a
/// publisher: the headings of the first and of the second level
const QString SET_TITLE_STYLE = QStringLiteral("heading_1");
const QString SET_PART_STYLE = QStringLiteral("heading_2");
/// The style of the title of a text, when the book has it
const QString CHAPTER_TITLE_STYLE = QStringLiteral("chapter_title");

/// The kinds whose texts get no title: the pages that have none in a printed book
bool isUntitledKind(const QString& kindId) {
    static const QSet<QString> kinds = {QStringLiteral("title_page"), QStringLiteral("copyright"),
                                        QStringLiteral("dedication"), QStringLiteral("motto")};
    return kinds.contains(kindId);
}

/// The kind of the table of contents of a book
bool isContentsKind(const QString& kindId) {
    return kindId == QLatin1String("toc");
}

/// The materials of the cover an e-book takes: the design of its front and its description
struct CoverOfBook {
    const core::ProjectElement* design = nullptr;       ///< None: no design is chosen
    const core::ProjectElement* description = nullptr;  ///< None: no description is chosen
};

/// The chosen materials of the cover in the Workshop (and in its libraries): the design of the
/// front (else of the whole cover) with its picture, the description of the back (else of a
/// flap)
CoverOfBook coverOfBook(const core::ProjectManager& projects) {
    using core::CoverMaterial;
    CoverOfBook cover;
    const core::BookProject* project = projects.project();
    if (project == nullptr) {
        return cover;
    }
    bool frontDesign = false;
    bool backDescription = false;
    const std::function<void(const QList<core::ProjectElement>&)> visit =
        [&](const QList<core::ProjectElement>& elements) {
            for (const core::ProjectElement& element : elements) {
                visit(element.elements);
                const core::CoverData& data = element.cover;
                if (data.state != QLatin1String(CoverMaterial::CHOSEN)) {
                    continue;
                }
                if (data.part == QLatin1String(CoverMaterial::DESIGN) && !element.file.isEmpty() &&
                    !frontDesign &&
                    (data.place == QLatin1String(CoverMaterial::FRONT) ||
                     (data.place == QLatin1String(CoverMaterial::WHOLE) &&
                      cover.design == nullptr))) {
                    cover.design = &element;
                    frontDesign = data.place == QLatin1String(CoverMaterial::FRONT);
                } else if (data.part == QLatin1String(CoverMaterial::BLURB) &&
                           !element.description.trimmed().isEmpty() && !backDescription &&
                           (data.place == QLatin1String(CoverMaterial::BACK) ||
                            cover.description == nullptr)) {
                    cover.description = &element;
                    backDescription = data.place == QLatin1String(CoverMaterial::BACK);
                }
            }
        };
    visit(project->workshop.elements);
    return cover;
}

/// The pages of the print with the export they show, which they need while they live
struct PrintedBook {
    editor::BookExport book;
    std::unique_ptr<editor::BookPages> pages;
};

/// The state of a printer, as the window says it
dialogs::PrintBookDialog::PrinterState stateOf(QPrinter::PrinterState state) {
    using State = dialogs::PrintBookDialog::PrinterState;
    switch (state) {
        case QPrinter::Active:
            return State::Printing;
        case QPrinter::Aborted:
            return State::Stopped;
        case QPrinter::Error:
            return State::Error;
        case QPrinter::Idle:
            break;
    }
    return State::Ready;
}

/// How the printer prints on both sides: by itself, or on one side (also by hand, when the
/// window gives the fronts and the backs one after the other)
QPrinter::DuplexMode duplexModeOf(dialogs::PrintBookDialog::Duplex duplex) {
    using Duplex = dialogs::PrintBookDialog::Duplex;
    switch (duplex) {
        case Duplex::LongEdge:
            return QPrinter::DuplexLongSide;
        case Duplex::ShortEdge:
            return QPrinter::DuplexShortSide;
        case Duplex::None:
        case Duplex::Manual:
            break;
    }
    return QPrinter::DuplexNone;
}

/// The printer @p printer set for @p paper, lying sideways or not, on the whole sheet: its
/// minimum margins say where it prints
void setPaper(QPrinter& printer, const QPageSize& paper, bool sideways) {
    printer.setFullPage(true);
    printer.setPageSize(paper);
    printer.setPageOrientation(sideways ? QPageLayout::Landscape : QPageLayout::Portrait);
}

/// The lasting identifier of the e-book of the open book: the same for the book in each export,
/// another for each book (a UUID made of the project's and the book's ids)
/// The set for a publisher of @p projects in @p shown, as one text: its title and its parts
void addSetOf(const core::ProjectManager& projects, dialogs::ExportBookDialog::Book& shown) {
    shown.setParts = static_cast<int>(PublishingSetContents::textsOf(projects).size());
    if (shown.setParts > 0) {
        //: The set for a publisher as the name of its file; %1: the title of the book. In
        //: Polish: %1 – zestaw dla wydawnictwa
        shown.setTitle = BookExportCoordinator::tr("%1 – set for the publisher").arg(shown.title);
    }
}

/// Plain text @p text in KML: each line a paragraph
QString kmlOfText(const QString& text) {
    return QStringLiteral("<kml>") + editor::ClipboardHandler::textToKml(text) +
           QStringLiteral("</kml>");
}

QString bookIdentifier(const core::ProjectManager& projects) {
    const core::BookProject* project = projects.project();
    const core::ProjectBook* book = projects.isProjectOpen() ? projects.book() : nullptr;
    if (project == nullptr || book == nullptr) {
        return QString();
    }
    const QUuid projectId(project->id);
    // A project of an id that is not a UUID: its id in the namespace of the URLs
    static const QUuid urlNamespace(QStringLiteral("{6ba7b811-9dad-11d1-80b4-00c04fd430c8}"));
    const QUuid space =
        projectId.isNull() ? QUuid::createUuidV5(urlNamespace, project->id) : projectId;
    return QUuid::createUuidV5(space, book->id).toString(QUuid::WithoutBraces);
}

}  // namespace

BookExportCoordinator::BookExportCoordinator(core::ProjectManager& book, CommandRegistry& commands,
                                             QTabWidget* centralTabs, CurrentEditor currentEditor,
                                             const CaptionCoordinator* captions,
                                             const FootnoteCoordinator* footnotes, QWidget* window,
                                             QObject* parent)
    : QObject(parent),
      m_book(book),
      m_commands(commands),
      m_centralTabs(centralTabs),
      m_currentEditor(std::move(currentEditor)),
      m_captions(captions),
      m_footnotes(footnotes),
      m_window(window) {}

void BookExportCoordinator::connectCommands() {
    for (const auto& [id, format] : {std::pair{DOCX_COMMAND, editor::ExportFormat::Docx},
                                     std::pair{PDF_COMMAND, editor::ExportFormat::Pdf},
                                     std::pair{MARKDOWN_COMMAND, editor::ExportFormat::Markdown},
                                     std::pair{EPUB_COMMAND, editor::ExportFormat::Epub}}) {
        if (Command* command = m_commands.getCommand(id)) {
            command->execute = [this, format = format]() { exportBook(format); };
            command->isEnabled = [this]() { return whyNotExport().isEmpty(); };
            command->disabledReason = [this]() { return whyNotExport(); };
        }
    }
    if (Command* command = m_commands.getCommand(PRINT_COMMAND)) {
        command->execute = [this]() { printBook(); };
        command->isEnabled = [this]() { return whyNotPrint().isEmpty(); };
        command->disabledReason = [this]() { return whyNotPrint(); };
    }
    updateCommands();
}

void BookExportCoordinator::updateCommands() {
    for (const char* id :
         {DOCX_COMMAND, PDF_COMMAND, MARKDOWN_COMMAND, EPUB_COMMAND, PRINT_COMMAND}) {
        m_commands.updateActionState(id);
    }
}

QString BookExportCoordinator::whyNotExport() const {
    const EditorPanel* front = m_currentEditor ? m_currentEditor() : nullptr;
    if (m_book.isProjectOpen() ||
        (front != nullptr && front->getBookEditor() != nullptr)) {
        return QString();
    }
    //: The tooltip of File > Export > DOCX, PDF, Markdown and EPUB while they are greyed out.
    //: In Polish: Otwórz książkę albo tekst, który chcesz wyeksportować
    return tr("Open a book or a text to export");
}

QString BookExportCoordinator::whyNotPrint() const {
    if (whyNotExport().isEmpty()) {
        return QString();
    }
    //: The tooltip of File > Print while it is greyed out. In Polish: Otwórz książkę albo
    //: tekst, który chcesz wydrukować
    return tr("Open a book or a text to print");
}

void BookExportCoordinator::exportBook(editor::ExportFormat format) {
    exportContent(format, std::nullopt);
}

void BookExportCoordinator::exportSet() {
    exportContent(editor::ExportFormat::Docx, dialogs::BookOutputDialog::Content::Set);
}

void BookExportCoordinator::exportContent(
    editor::ExportFormat format, std::optional<dialogs::BookOutputDialog::Content> content) {
    if (!whyNotExport().isEmpty()) {
        return;
    }
    auto& logger = core::Logger::getInstance();
    dialogs::ExportBookDialog dialog(
        dialogBook(), format, ProgramFolders::startFolder(ProgramFolders::Operation::ExportBook),
        [this, &logger](const QString& path, const dialogs::ExportBookDialog::Choice& choice,
                        const dialogs::ArchiveDialog::Progress& progress,
                        editor::ExportResult* result, editor::ExportProblem* problem) {
            bool written = false;
            if (const auto exporter = dialogs::ExportBookDialog::exporterOf(choice.formatId)) {
                // The plugin reads what the writer chose in the window (kalahari_api: the
                // book given to export_document())
                const core::PluginHost::ExportScope scope(pluginBookOf(bookExport(choice)));
                written = exportWithPlugin(*exporter, choice.formatId, path, progress, result,
                                           problem);
                if (written) {
                    logger.info("Export: the plugin {} wrote {}", exporter->getPluginId(),
                                path.toStdString());
                } else {
                    for (const QString& line : problem->lines) {
                        logger.error("Export: {}", line.toStdString());
                    }
                }
                return written;
            }
            const editor::BookExport book = bookExport(choice);
            switch (choice.format) {
                case editor::ExportFormat::Docx:
                    written = editor::exportDocx(book, path, progress, result, problem);
                    break;
                case editor::ExportFormat::Pdf:
                    written = editor::exportPdf(book, path, progress, result, problem);
                    break;
                case editor::ExportFormat::Markdown:
                    written = editor::exportMarkdown(book, path, choice.imagesFolder, progress,
                                                     result, problem);
                    break;
                case editor::ExportFormat::Epub:
                    // The book keeps the ISBN and the publisher of its e-book
                    if (m_book.isProjectOpen() &&
                        (choice.content == dialogs::ExportBookDialog::Content::Book ||
                         choice.content == dialogs::ExportBookDialog::Content::Chosen)) {
                        m_book.setBookPublication(choice.isbn, choice.publisher);
                    }
                    written = editor::exportEpub(book, path, progress, result, problem);
                    break;
            }
            if (written) {
                logger.info("Export: {} texts to {}", book.items.size(), path.toStdString());
            } else {
                for (const QString& line : problem->lines) {
                    logger.error("Export: {}", line.toStdString());
                }
            }
            return written;
        },
        m_window);
    // The content asked for; else the one the window chose
    dialog.setContent(content.value_or(dialog.layoutChoice().content));
    dialog.exec();
}

void BookExportCoordinator::printBook() {
    if (!whyNotPrint().isEmpty()) {
        return;
    }
    auto& logger = core::Logger::getInstance();
    const dialogs::PrintBookDialog::Book book = dialogBook();
    const QList<dialogs::PrintBookDialog::Printer> printers = systemPrinters();
    logger.info("Print: the system has {} printers", printers.size());
    dialogs::PrintBookDialog dialog(
        book, printers,
        [this](const dialogs::BookOutputDialog::Layout& layout) { return printedPages(layout); },
        [&logger, &book](const dialogs::PrintBookDialog::Choice& choice,
                         const editor::BookPages& pages,
                         const std::vector<editor::SheetSide>& sides,
                         const std::function<bool(int printed)>& progress, QStringList* problem) {
            // A printer gone since the window opened: not the default one instead
            const QPrinterInfo info = QPrinterInfo::printerInfo(choice.printer);
            if (info.isNull()) {
                problem->append(QStringLiteral("%1: the system has no such printer")
                                    .arg(choice.printer));
                logger.error("Print: {}", problem->constLast().toStdString());
                return false;
            }
            QPrinter printer(info, QPrinter::HighResolution);
            // The name of the print in the queue of the printer
            QString title = book.title;
            if (choice.content == dialogs::BookOutputDialog::Content::Front || title.isEmpty()) {
                title = book.frontTitle;
            } else if (choice.content == dialogs::BookOutputDialog::Content::Set) {
                title = book.setTitle;
            }
            printer.setDocName(title.isEmpty() ? QStringLiteral("Kalahari") : title);
            const bool printed = printOn(printer, choice, pages, sides, progress, problem);
            if (printed) {
                logger.info("Print: {} sides of sheets to {}", sides.size(),
                            choice.printer.toStdString());
            } else {
                for (const QString& line : std::as_const(*problem)) {
                    logger.error("Print: {}", line.toStdString());
                }
            }
            return printed;
        },
        {}, m_window);
    dialog.exec();
}

dialogs::ExportBookDialog::Book BookExportCoordinator::dialogBook() const {
    dialogs::ExportBookDialog::Book book;
    core::ProjectManager& projects = m_book;
    const core::ProjectBook* projectBook = projects.isProjectOpen() ? projects.book() : nullptr;
    if (projectBook != nullptr) {
        book.title = projectBook->title.trimmed().isEmpty() ? tr("Book") : projectBook->title;
        book.author = projectBook->author;
        book.language = projectBook->language.isEmpty()
                            ? QString()
                            : utils::languageName(projectBook->language);
        book.isbn = projectBook->isbn;
        book.publisher = projectBook->publisher;
        const CoverOfBook cover = coverOfBook(projects);
        if (cover.design != nullptr) {
            book.coverTitle = cover.design->title;
            book.coverFile = projects.filePathOf(*cover.design);
        }
        if (cover.description != nullptr) {
            book.description = cover.description->description;
        }
        addSetOf(projects, book);
        // The texts with their groups, as the Navigator shows them
        const std::function<void(const QList<core::ProjectElement>&, int)> addRows =
            [&](const QList<core::ProjectElement>& elements, int level) {
                for (const core::ProjectElement& element : elements) {
                    const core::ElementForm form = projects.formOf(element);
                    const bool text = form == core::ElementForm::Text && !element.file.isEmpty();
                    const bool contents =
                        form == core::ElementForm::Window && isContentsKind(element.kind.kindId);
                    const bool group = form == core::ElementForm::Group;
                    if (!text && !contents && !group) {
                        continue;
                    }
                    dialogs::ExportBookDialog::Row row;
                    row.id = element.id;
                    row.title = element.title;
                    row.status = text ? element.status : QString();
                    row.iconId = NavigatorPanel::iconIdOf(projects.bookTypes(), element);
                    row.level = level;
                    row.leaf = !group;
                    book.rows.append(row);
                    if (group) {
                        addRows(element.elements, level + 1);
                    }
                }
            };
        const std::pair<const QList<core::ProjectElement>*, core::BookPlace> parts[] = {
            {&projectBook->frontElements, core::BookPlace::Front},
            {&projectBook->mainElements, core::BookPlace::Main},
            {&projectBook->backElements, core::BookPlace::Back}};
        for (const auto& [elements, place] : parts) {
            if (elements->isEmpty()) {
                continue;
            }
            int level = 0;
            if (projectBook->partsLayer) {
                dialogs::ExportBookDialog::Row section;
                section.title = projectBook->sectionName(place);
                section.iconId = place == core::BookPlace::Front ? QStringLiteral("structure.frontmatter")
                                 : place == core::BookPlace::Main ? QStringLiteral("structure.body")
                                                                  : QStringLiteral("structure.backmatter");
                section.leaf = false;
                book.rows.append(section);
                level = 1;
            }
            addRows(*elements, level);
        }
        // A section or a group without texts is no row of the list
        for (qsizetype i = book.rows.size() - 1; i >= 0; --i) {
            const bool emptyGroup = !book.rows[i].leaf &&
                                    (i + 1 == book.rows.size() ||
                                     book.rows[i + 1].level <= book.rows[i].level);
            if (emptyGroup) {
                book.rows.removeAt(i);
            }
        }
    }

    // The text in front: of the book, or a file outside it
    const EditorPanel* front = m_currentEditor ? m_currentEditor() : nullptr;
    if (front != nullptr && front->getBookEditor() != nullptr) {
        const QString elementId = front->property("elementId").toString();
        const core::ProjectElement* element =
            elementId.isEmpty() ? nullptr : projects.findElement(elementId);
        if (element != nullptr) {
            book.frontId = elementId;
            book.frontTitle = element->title;
        } else {
            const QString path = front->property("standaloneFilePath").toString();
            QString title = path.isEmpty() ? DocumentWindows::tabText(front)
                                           : QFileInfo(path).completeBaseName();
            while (title.startsWith(QLatin1Char('*'))) {
                title.remove(0, 1);
            }
            book.frontTitle = title.trimmed();
        }
    }
    return book;
}

editor::BookExport BookExportCoordinator::bookExport(
    const dialogs::ExportBookDialog::Choice& choice) const {
    using Content = dialogs::ExportBookDialog::Content;
    const core::ProjectManager& projects = m_book;
    const core::ProjectBook* projectBook = projects.isProjectOpen() ? projects.book() : nullptr;

    editor::BookExport book =
        bookExport(static_cast<const dialogs::BookOutputDialog::Layout&>(choice));
    book.annotationsAsComments = choice.comments;
    book.pdf = choice.pdf;
    book.copyImages = choice.copyImages;
    // An EPUB: its look, its font, its cover and its data. Its font is the editor's: the window
    // has no choice of it for an EPUB
    if (choice.format == editor::ExportFormat::Epub) {
        const auto& settings = core::SettingsManager::getInstance();
        book.fontFamily = QString::fromStdString(settings.get<std::string>("editor.fontFamily"));
    }
    book.epub.bookStyles = choice.bookStyles;
    book.epub.embedFont = choice.embedFont;
    book.epub.coverImage = choice.coverImage;
    book.epub.isbn = choice.isbn;
    book.epub.publisher = choice.publisher;
    if (projectBook != nullptr) {
        if (const CoverOfBook cover = coverOfBook(projects); cover.description != nullptr) {
            book.epub.description = cover.description->description;
        }
        // The book's own identifier for the whole book; a part of it is another e-book
        if (choice.content == Content::Book) {
            book.epub.identifier = bookIdentifier(projects);
        }
    }
    return book;
}

editor::BookExport BookExportCoordinator::bookExport(
    const dialogs::BookOutputDialog::Layout& layout) const {
    using Content = dialogs::BookOutputDialog::Content;
    const core::ProjectManager& projects = m_book;
    const auto& settings = core::SettingsManager::getInstance();
    const core::ProjectBook* projectBook = projects.isProjectOpen() ? projects.book() : nullptr;

    editor::BookExport book;
    if (projectBook != nullptr) {
        book.title = projectBook->title;
        book.author = projectBook->author;
        book.language = projectBook->language;
        book.imagesFolder = projects.imagesBaseFolder();
    }
    book.styles = bookParagraphStyles(m_book);
    // The pages of a PDF and of the print have the colors of the light paper (Settings >
    // Editor > Colors)
    book.colors = EditorPanel::themeColors();
    book.fontFamily = layout.fontFamily.isEmpty()
                          ? QString::fromStdString(settings.get<std::string>("editor.fontFamily"))
                          : layout.fontFamily;
    book.sceneBreak = editor::sceneBreakLookFromName(
        QString::fromStdString(settings.get<std::string>("editor.sceneBreak.style")));
    book.page = pageOfSettings();
    book.page.pageNumbers = layout.pageNumbers;
    book.textsOnNewPage = layout.newPage;
    book.titleHeadings = layout.titles;
    book.runningHead = layout.runningHead;
    //: The word before the text of a to-do in a comment of Word. In Polish: Do zrobienia
    book.annotationLabels.insert(editor::AnnotationKind::Todo, tr("To do"));
    //: The word before the text of a note in a comment of Word. In Polish: Notatka
    book.annotationLabels.insert(editor::AnnotationKind::Note, tr("Note"));

    const QHash<QString, const editor::BookEditor*> open = openEditors();
    if (layout.content == Content::Front) {
        addFrontText(open, book);
        return book;
    }
    if (projectBook == nullptr) {
        return book;
    }
    if (layout.content == Content::Set) {
        addSetItems(book);
        return book;
    }

    // The book, or its texts chosen in the window, in reading order with their groups
    const QSet<QString> chosen(layout.chosen.cbegin(), layout.chosen.cend());
    const QSet<QString>* wanted = layout.content == Content::Book ? nullptr : &chosen;
    addItems(projectBook->frontElements, wanted, open, book.items);
    addItems(projectBook->mainElements, wanted, open, book.items);
    addItems(projectBook->backElements, wanted, open, book.items);

    // The endnotes of a book that has them at its end
    book.endnotesAtBookEnd = std::any_of(book.items.cbegin(), book.items.cend(),
                                         [](const editor::ExportItem& item) {
                                             return item.kind == editor::ExportItemKind::Text &&
                                                    !item.footnotes.endnotesHere;
                                         });
    return book;
}

void BookExportCoordinator::addSetItems(editor::BookExport& book) const {
    // The title of the book with its author, then each part under its name
    editor::ExportItem title;
    title.kind = editor::ExportItemKind::Text;
    title.title = PublishingSetContents::titleOf(m_book);
    title.titleStyle = SET_TITLE_STYLE;
    title.kml = kmlOfText(PublishingSetContents::aboutOf(m_book));
    book.items.push_back(title);
    for (const core::PublishingSetText& part : PublishingSetContents::textsOf(m_book)) {
        editor::ExportItem item;
        item.kind = editor::ExportItemKind::Text;
        item.title = part.heading;
        item.titleStyle = SET_PART_STYLE;
        item.kml = kmlOfText(part.text);
        book.items.push_back(item);
    }
}

editor::ExportItem BookExportCoordinator::textItem(
    const core::ProjectElement& element,
    const QHash<QString, const editor::BookEditor*>& open) const {
    editor::ExportItem item;
    item.kind = editor::ExportItemKind::Text;
    item.elementId = element.id;
    item.title = isUntitledKind(element.kind.kindId) ? QString() : element.title;
    item.titleStyle = CHAPTER_TITLE_STYLE;
    const editor::BookEditor* editor = open.value(element.id, nullptr);
    item.kml = editor != nullptr ? editor->toKml() : m_book.chapterText(element.id);
    if (m_captions != nullptr) {
        item.captions = m_captions->contextFor(element.id);
    }
    if (m_footnotes != nullptr) {
        item.footnotes = m_footnotes->contextFor(element.id);
    }
    item.firstPageLeft = element.firstPageLeft;
    return item;
}

void BookExportCoordinator::addFrontText(const QHash<QString, const editor::BookEditor*>& open,
                                         editor::BookExport& book) const {
    const EditorPanel* front = m_currentEditor ? m_currentEditor() : nullptr;
    if (front == nullptr || front->getBookEditor() == nullptr) {
        return;
    }
    const QString elementId = front->property("elementId").toString();
    const core::ProjectElement* element =
        elementId.isEmpty() ? nullptr : m_book.findElement(elementId);
    if (element != nullptr) {
        book.items.push_back(textItem(*element, open));
        return;
    }
    // A file outside the book: its images are next to it, its numbers its own
    const QString path = front->property("standaloneFilePath").toString();
    editor::ExportItem item;
    item.kind = editor::ExportItemKind::Text;
    item.title = dialogBook().frontTitle;
    item.titleStyle = CHAPTER_TITLE_STYLE;
    item.kml = front->getBookEditor()->toKml();
    if (m_captions != nullptr) {
        item.captions = m_captions->contextFor(QString());
    }
    if (m_footnotes != nullptr) {
        item.footnotes = m_footnotes->contextFor(QString());
    }
    book.imagesFolder = path.isEmpty() ? QString() : QFileInfo(path).absolutePath();
    book.title = item.title;
    book.language = front->textLanguage();
    book.items.push_back(std::move(item));
}

void BookExportCoordinator::addItems(const QList<core::ProjectElement>& elements,
                                     const QSet<QString>* chosen,
                                     const QHash<QString, const editor::BookEditor*>& open,
                                     std::vector<editor::ExportItem>& items) const {
    const auto wanted = [chosen](const core::ProjectElement& element) {
        return chosen == nullptr || chosen->contains(element.id);
    };
    for (const core::ProjectElement& element : elements) {
        const core::ElementForm form = m_book.formOf(element);
        if (form == core::ElementForm::Group) {
            // A part goes with its title when any of its texts goes
            std::vector<editor::ExportItem> inside;
            addItems(element.elements, chosen, open, inside);
            if (!inside.empty()) {
                editor::ExportItem group;
                group.kind = editor::ExportItemKind::Group;
                group.elementId = element.id;
                group.title = element.title;
                group.titleStyle = PART_TITLE_STYLE;
                group.firstPageLeft = element.firstPageLeft;
                items.push_back(std::move(group));
                std::move(inside.begin(), inside.end(), std::back_inserter(items));
            }
        } else if (form == core::ElementForm::Text && !element.file.isEmpty()) {
            if (wanted(element)) {
                items.push_back(textItem(element, open));
            }
        } else if (form == core::ElementForm::Window && isContentsKind(element.kind.kindId) &&
                   wanted(element)) {
            editor::ExportItem contents;
            contents.kind = editor::ExportItemKind::Contents;
            contents.elementId = element.id;
            contents.title = element.title;
            items.push_back(std::move(contents));
        }
    }
}

bool BookExportCoordinator::exportWithPlugin(core::IExporter& exporter, const QString& formatId,
                                             const QString& path,
                                             const dialogs::ArchiveDialog::Progress& progress,
                                             editor::ExportResult* result,
                                             editor::ExportProblem* problem) {
    // The plugin writes the file as core::IExporter::exportDocument() says: the format and the
    // path; it has no progress of its own
    bool written = false;
    try {
        written = exporter.exportDocument(formatId.toStdString(), path.toStdString());
    } catch (const std::exception& error) {
        problem->lines << QStringLiteral("%1: %2").arg(
            QString::fromStdString(exporter.getPluginId()), QString::fromUtf8(error.what()));
    }
    if (!written) {
        problem->kind = editor::ExportProblem::Kind::Other;
        if (problem->lines.isEmpty()) {
            problem->lines << QStringLiteral("%1: the plugin did not write %2 in the format %3")
                                  .arg(QString::fromStdString(exporter.getPluginId()), path,
                                       formatId);
        }
        return false;
    }
    if (progress) {
        progress(100);
    }
    result->file = path;
    return true;
}

editor::ExportPage BookExportCoordinator::pageOfSettings() {
    const auto& settings = core::SettingsManager::getInstance();
    editor::PageLayout layout;
    layout.pageSize = editor::PageLayout::pageSizeFromId(
        QString::fromStdString(settings.get<std::string>("editor.page.size")));
    layout.customWidth = settings.get<double>("editor.page.customWidth");
    layout.customHeight = settings.get<double>("editor.page.customHeight");
    const QSizeF size = layout.pageSizeMm();
    editor::ExportPage page;
    page.width = size.width();
    page.height = size.height();
    page.top = settings.get<double>("editor.margins.pageTop");
    page.bottom = settings.get<double>("editor.margins.pageBottom");
    page.left = settings.get<double>("editor.margins.pageLeft");
    page.right = settings.get<double>("editor.margins.pageRight");
    page.mirrored = settings.get<bool>("editor.margins.mirrorEnabled");
    page.inner = settings.get<double>("editor.margins.pageInner");
    page.outer = settings.get<double>("editor.margins.pageOuter");
    page.pageNumbers = settings.get<bool>("editor.page.showNumbers");
    return page;
}

dialogs::PrintBookDialog::LaidOut BookExportCoordinator::printedPages(
    const dialogs::BookOutputDialog::Layout& layout) const {
    dialogs::PrintBookDialog::LaidOut laidOut;
    auto printed = std::make_shared<PrintedBook>();
    printed->book = bookExport(layout);
    if (printed->book.items.empty()) {
        return laidOut;
    }
    // On paper: the links are their text
    printed->pages = std::make_unique<editor::BookPages>(printed->book,
                                                         editor::BookPages::Options{false});
    printed->pages->layOut();
    laidOut.pages = std::shared_ptr<const editor::BookPages>(printed, printed->pages.get());

    // The page of the cursor of the text in front: of the book, or the file outside it alone
    const EditorPanel* front = m_currentEditor ? m_currentEditor() : nullptr;
    const editor::BookEditor* editor = front != nullptr ? front->getBookEditor() : nullptr;
    if (editor == nullptr) {
        return laidOut;
    }
    QString elementId = front->property("elementId").toString();
    if (!elementId.isEmpty() && m_book.findElement(elementId) == nullptr) {
        elementId.clear();
    }
    if (elementId.isEmpty() && layout.content != dialogs::BookOutputDialog::Content::Front) {
        return laidOut;
    }
    const editor::CursorPosition cursor = editor->cursorPosition();
    const QTextDocument* document = editor->textDocument();
    const QTextBlock block =
        document != nullptr ? document->findBlockByNumber(cursor.paragraph) : QTextBlock();
    const int position = block.isValid() ? block.position() + cursor.offset : 0;
    laidOut.current = laidOut.pages->pageAt(elementId, position);
    return laidOut;
}

QList<dialogs::PrintBookDialog::Printer> BookExportCoordinator::systemPrinters() {
    QList<dialogs::PrintBookDialog::Printer> printers;
    const QString defaultName = QPrinterInfo::defaultPrinterName();
    for (const QPrinterInfo& info : QPrinterInfo::availablePrinters()) {
        dialogs::PrintBookDialog::Printer printer;
        printer.name = info.printerName();
        printer.isDefault = info.isDefault() || printer.name == defaultName;
        printer.state = stateOf(info.state());
        printer.color = info.supportedColorModes().contains(QPrinter::Color);
        const QList<QPrinter::DuplexMode> duplex = info.supportedDuplexModes();
        printer.duplex = duplex.contains(QPrinter::DuplexLongSide) ||
                         duplex.contains(QPrinter::DuplexShortSide) ||
                         duplex.contains(QPrinter::DuplexAuto);
        printer.papers = info.supportedPageSizes();
        printer.paper = info.defaultPageSize();
        // A printer that names no paper: its own, else A4
        if (!printer.paper.isValid()) {
            printer.paper = printer.papers.isEmpty() ? QPageSize(QPageSize::A4)
                                                     : printer.papers.constFirst();
        }
        if (printer.papers.isEmpty()) {
            printer.papers.append(printer.paper);
        }
        // Where it does not print: its minimum margins on the paper as it lies
        printer.margins = [name = printer.name](const QPageSize& paper, bool sideways) {
            const QPrinterInfo current = QPrinterInfo::printerInfo(name);
            if (current.isNull()) {
                return QMarginsF();
            }
            QPrinter device(current, QPrinter::HighResolution);
            setPaper(device, paper, sideways);
            QPageLayout layout = device.pageLayout();
            layout.setUnits(QPageLayout::Point);
            return layout.minimumMargins();
        };
        printers.append(std::move(printer));
    }
    return printers;
}

bool BookExportCoordinator::printOn(QPrinter& printer,
                                    const dialogs::PrintBookDialog::Choice& choice,
                                    const editor::BookPages& pages,
                                    const std::vector<editor::SheetSide>& sides,
                                    const std::function<bool(int printed)>& progress,
                                    QStringList* problem) {
    if (!printer.isValid()) {
        problem->append(QStringLiteral("%1: the printer is not valid").arg(choice.printer));
        return false;
    }
    setPaper(printer, choice.paper.isValid() ? choice.paper : printer.pageLayout().pageSize(),
             editor::sheetSideways(choice.sheet, pages.pageSize()));
    printer.setDuplex(duplexModeOf(choice.duplex));
    printer.setColorMode(choice.color ? QPrinter::Color : QPrinter::GrayScale);
    // The copies are the sides the window gives, in the order it chose
    printer.setCopyCount(1);
    printer.setCollateCopies(false);
    if (!editor::printSheetSides(&printer, pages, sides, choice.sheet, progress)) {
        problem->append(printer.printerState() == QPrinter::Aborted
                            ? QStringLiteral("%1: the print was stopped").arg(printer.printerName())
                            : QStringLiteral("%1: the printer cannot print")
                                  .arg(printer.printerName()));
        return false;
    }
    return true;
}

QHash<QString, const editor::BookEditor*> BookExportCoordinator::openEditors() const {
    QHash<QString, const editor::BookEditor*> editors;
    if (m_centralTabs == nullptr) {
        return editors;
    }
    for (QWidget* page : DocumentWindows::of(*m_centralTabs).pages()) {
        const auto* panel = qobject_cast<EditorPanel*>(page);
        if (panel == nullptr || panel->getBookEditor() == nullptr) {
            continue;
        }
        const QString elementId = panel->property("elementId").toString();
        if (!elementId.isEmpty()) {
            editors.insert(elementId, panel->getBookEditor());
        }
    }
    return editors;
}

}  // namespace kalahari::gui
