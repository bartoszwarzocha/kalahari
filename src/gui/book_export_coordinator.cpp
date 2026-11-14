/// @file book_export_coordinator.cpp
/// @brief File > Export > DOCX and Markdown: the window of the export, and the book as the
///        program has it (the open texts with their unsaved changes) for the file

#include "kalahari/gui/book_export_coordinator.h"

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
#include "kalahari/gui/utils/program_folders.h"

#include <QFileInfo>
#include <QSet>
#include <QTabWidget>

#include <algorithm>
#include <utility>

namespace kalahari::gui {

namespace {

constexpr const char* DOCX_COMMAND = "file.export.docx";
constexpr const char* MARKDOWN_COMMAND = "file.export.markdown";

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

}  // namespace

BookExportCoordinator::BookExportCoordinator(QTabWidget* centralTabs, CurrentEditor currentEditor,
                                             const CaptionCoordinator* captions,
                                             const FootnoteCoordinator* footnotes, QWidget* window,
                                             QObject* parent)
    : QObject(parent),
      m_centralTabs(centralTabs),
      m_currentEditor(std::move(currentEditor)),
      m_captions(captions),
      m_footnotes(footnotes),
      m_window(window) {}

void BookExportCoordinator::connectCommands() {
    auto& registry = CommandRegistry::getInstance();
    for (const auto& [id, format] : {std::pair{DOCX_COMMAND, editor::ExportFormat::Docx},
                                     std::pair{MARKDOWN_COMMAND, editor::ExportFormat::Markdown}}) {
        if (Command* command = registry.getCommand(id)) {
            command->execute = [this, format = format]() { exportBook(format); };
            command->isEnabled = [this]() { return whyNotExport().isEmpty(); };
            command->disabledReason = [this]() { return whyNotExport(); };
        }
    }
    updateCommands();
}

void BookExportCoordinator::updateCommands() {
    auto& registry = CommandRegistry::getInstance();
    registry.updateActionState(DOCX_COMMAND);
    registry.updateActionState(MARKDOWN_COMMAND);
}

QString BookExportCoordinator::whyNotExport() const {
    const EditorPanel* front = m_currentEditor ? m_currentEditor() : nullptr;
    if (core::ProjectManager::getInstance().isProjectOpen() ||
        (front != nullptr && front->getBookEditor() != nullptr)) {
        return QString();
    }
    //: The tooltip of File > Export > DOCX and Markdown while they are greyed out. In Polish:
    //: Otwórz książkę albo tekst, który chcesz wyeksportować
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
            if (choice.format == editor::ExportFormat::Docx) {
                written = editor::exportDocx(book, path, progress, result, problem);
            } else {
                written = editor::exportMarkdown(book, path, choice.imagesFolder, progress,
                                                 result, problem);
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
    auto& projects = core::ProjectManager::getInstance();
    const core::ProjectBook* projectBook = projects.isProjectOpen() ? projects.book() : nullptr;
    if (projectBook != nullptr) {
        book.title = projectBook->title.trimmed().isEmpty() ? tr("Book") : projectBook->title;
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
    auto& projects = core::ProjectManager::getInstance();
    const auto& settings = core::SettingsManager::getInstance();
    const core::ProjectBook* projectBook = projects.isProjectOpen() ? projects.book() : nullptr;

    editor::BookExport book;
    if (projectBook != nullptr) {
        book.title = projectBook->title;
        book.author = projectBook->author;
        book.language = projectBook->language;
        book.imagesFolder = projects.imagesBaseFolder();
    }
    book.styles = bookParagraphStyles();
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
    book.copyImages = choice.copyImages;
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
