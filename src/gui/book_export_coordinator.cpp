// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_export_coordinator.cpp
/// @brief File > Export > DOCX, PDF, Markdown and EPUB: the window of the export, and the book
///        as the program has it (the open texts with their unsaved changes) for the file

#include "kalahari/gui/book_export_coordinator.h"

#include "kalahari/core/cover_material.h"
#include "kalahari/core/extension_points.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/editor_appearance.h"
#include "kalahari/gui/book_styles.h"
#include "kalahari/gui/caption_coordinator.h"
#include "kalahari/gui/command_registry.h"
#include "kalahari/gui/document_windows.h"
#include "kalahari/gui/footnote_coordinator.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/utils/language_names.h"
#include "kalahari/gui/utils/program_folders.h"

#include <QFileInfo>
#include <QSet>
#include <QTabWidget>
#include <QUuid>

#include <algorithm>
#include <utility>

namespace kalahari::gui {

namespace {

constexpr const char* DOCX_COMMAND = "file.export.docx";
constexpr const char* PDF_COMMAND = "file.export.pdf";
constexpr const char* MARKDOWN_COMMAND = "file.export.markdown";
constexpr const char* EPUB_COMMAND = "file.export.epub";

/// The style of the title of a part (a group of the book), when the book has it
const QString PART_TITLE_STYLE = QStringLiteral("part_title");
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
                     (data.place == QLatin1String(CoverMaterial::WHOLE) && cover.design == nullptr))) {
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

/// The lasting identifier of the e-book of the open book: the same for the book in each export,
/// another for each book (a UUID made of the project's and the book's ids)
QString bookIdentifier(const core::ProjectManager& projects) {
    const core::BookProject* project = projects.project();
    const core::ProjectBook* book = projects.isProjectOpen() ? projects.book() : nullptr;
    if (project == nullptr || book == nullptr) {
        return QString();
    }
    const QUuid projectId(project->id);
    // A project of an id that is not a UUID: its id in the namespace of the URLs
    static const QUuid urlNamespace(QStringLiteral("{6ba7b811-9dad-11d1-80b4-00c04fd430c8}"));
    const QUuid space = projectId.isNull() ? QUuid::createUuidV5(urlNamespace, project->id) : projectId;
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
    updateCommands();
}

void BookExportCoordinator::updateCommands() {
    for (const char* id : {DOCX_COMMAND, PDF_COMMAND, MARKDOWN_COMMAND, EPUB_COMMAND}) {
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

void BookExportCoordinator::exportBook(editor::ExportFormat format) {
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
                        choice.content != dialogs::ExportBookDialog::Content::Front) {
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
    dialog.exec();
}

dialogs::ExportBookDialog::Book BookExportCoordinator::dialogBook() const {
    dialogs::ExportBookDialog::Book book;
    core::ProjectManager& projects = m_book;
    const core::ProjectBook* projectBook = projects.isProjectOpen() ? projects.book() : nullptr;
    if (projectBook != nullptr) {
        book.title = projectBook->title.trimmed().isEmpty() ? tr("Book") : projectBook->title;
        book.author = projectBook->author;
        book.language = projectBook->language.isEmpty() ? QString()
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
    core::ProjectManager& projects = m_book;
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
    // The pages of a PDF have the colors of the light paper (Settings > Editor > Colors)
    book.colors = EditorPanel::themeColors();
    book.fontFamily = choice.fontFamily.isEmpty()
                          ? QString::fromStdString(settings.get<std::string>("editor.fontFamily"))
                          : choice.fontFamily;
    book.sceneBreak = editor::sceneBreakLookFromName(
        QString::fromStdString(settings.get<std::string>("editor.sceneBreak.style")));
    book.page = pageOfSettings();
    book.page.pageNumbers = choice.pageNumbers;
    book.textsOnNewPage = choice.newPage;
    book.titleHeadings = choice.titles;
    book.annotationsAsComments = choice.comments;
    book.runningHead = choice.runningHead;
    book.pdf = choice.pdf;
    book.copyImages = choice.copyImages;
    // An EPUB: its look, its font, its cover and its data. Its font is the editor's: the window
    // has no choice of it for an EPUB
    if (choice.format == editor::ExportFormat::Epub) {
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
    //: The word before the text of a to-do in a comment of Word. In Polish: Do zrobienia
    book.annotationLabels.insert(editor::AnnotationKind::Todo, tr("To do"));
    //: The word before the text of a note in a comment of Word. In Polish: Notatka
    book.annotationLabels.insert(editor::AnnotationKind::Note, tr("Note"));

    const QHash<QString, const editor::BookEditor*> open = openEditors();
    // A text of the book: as its editor has it, else as the program keeps it
    const auto textItem = [&](const core::ProjectElement& element) {
        editor::ExportItem item;
        item.kind = editor::ExportItemKind::Text;
        item.elementId = element.id;
        item.title = isUntitledKind(element.kind.kindId) ? QString() : element.title;
        item.titleStyle = CHAPTER_TITLE_STYLE;
        const editor::BookEditor* editor = open.value(element.id, nullptr);
        item.kml = editor != nullptr ? editor->toKml() : projects.chapterText(element.id);
        if (m_captions != nullptr) {
            item.captions = m_captions->contextFor(element.id);
        }
        if (m_footnotes != nullptr) {
            item.footnotes = m_footnotes->contextFor(element.id);
        }
        item.firstPageLeft = element.firstPageLeft;
        return item;
    };

    const EditorPanel* front = m_currentEditor ? m_currentEditor() : nullptr;
    if (choice.content == Content::Front) {
        if (front == nullptr || front->getBookEditor() == nullptr) {
            return book;
        }
        const QString elementId = front->property("elementId").toString();
        const core::ProjectElement* element =
            elementId.isEmpty() ? nullptr : projects.findElement(elementId);
        if (element != nullptr) {
            book.items.push_back(textItem(*element));
        } else {
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
        return book;
    }
    if (projectBook == nullptr) {
        return book;
    }

    // The book, or its texts chosen in the window, in reading order with their groups
    const QSet<QString> chosen(choice.chosen.cbegin(), choice.chosen.cend());
    const auto wanted = [&](const core::ProjectElement& element) {
        return choice.content == Content::Book || chosen.contains(element.id);
    };
    const std::function<void(const QList<core::ProjectElement>&, std::vector<editor::ExportItem>&)>
        addItems = [&](const QList<core::ProjectElement>& elements,
                       std::vector<editor::ExportItem>& items) {
            for (const core::ProjectElement& element : elements) {
                const core::ElementForm form = projects.formOf(element);
                if (form == core::ElementForm::Group) {
                    // A part goes with its title when any of its texts goes
                    std::vector<editor::ExportItem> inside;
                    addItems(element.elements, inside);
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
                        items.push_back(textItem(element));
                    }
                } else if (form == core::ElementForm::Window &&
                           isContentsKind(element.kind.kindId) && wanted(element)) {
                    editor::ExportItem contents;
                    contents.kind = editor::ExportItemKind::Contents;
                    contents.elementId = element.id;
                    contents.title = element.title;
                    items.push_back(std::move(contents));
                }
            }
        };
    addItems(projectBook->frontElements, book.items);
    addItems(projectBook->mainElements, book.items);
    addItems(projectBook->backElements, book.items);

    // The endnotes of a book that has them at its end
    book.endnotesAtBookEnd = std::any_of(book.items.cbegin(), book.items.cend(),
                                         [](const editor::ExportItem& item) {
                                             return item.kind == editor::ExportItemKind::Text &&
                                                    !item.footnotes.endnotesHere;
                                         });
    return book;
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

QHash<QString, const editor::BookEditor*> BookExportCoordinator::openEditors() const {
    QHash<QString, const editor::BookEditor*> editors;
    if (m_centralTabs == nullptr) {
        return editors;
    }
    for (QWidget* page : DocumentWindows::of(*m_centralTabs).pages()) {
        const auto* panel = qobject_cast<EditorPanel*>(page);
        const QString elementId = panel != nullptr ? panel->property("elementId").toString() : QString();
        if (!elementId.isEmpty() && panel->getBookEditor() != nullptr) {
            editors.insert(elementId, panel->getBookEditor());
        }
    }
    return editors;
}

}  // namespace kalahari::gui
