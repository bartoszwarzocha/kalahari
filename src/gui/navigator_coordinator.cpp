/// @file navigator_coordinator.cpp
/// @brief Navigator panel interaction coordination implementation
///
/// OpenSpec #00038 - Phase 6: Extract Navigator Handlers from MainWindow

#include "kalahari/gui/navigator_coordinator.h"
#include "kalahari/gui/dialogs/add_to_book_dialog.h"
#include "kalahari/gui/dialogs/delete_dialogs.h"
#include "kalahari/gui/dialogs/file_preview_dialog.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dialogs/move_element_dialog.h"
#include "kalahari/gui/dialogs/new_file_dialog.h"
#include "kalahari/gui/dialogs/rename_element_dialog.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/panels/image_preview_panel.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/material_file.h"
#include "kalahari/gui/section_words.h"
#include "kalahari/gui/utils/file_location.h"
#include "kalahari/core/chapter_document.h"
#include "kalahari/core/file_trash.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/package_card.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/settings_manager.h"
#include "kalahari/editor/book_editor.h"
#include "kalahari/editor/statistics_collector.h"
#include <QTabWidget>
#include <QStatusBar>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <algorithm>
#include <chrono>
#include <optional>
#include <utility>

namespace kalahari {
namespace gui {

namespace {

/// How long after the last change in a chapter its words are counted for the counts of the
/// book: often enough to follow the writing, not at each key
constexpr int WORD_COUNT_DELAY_MS = 500;

/// A section of the book as the Navigator names its item, with its icon
struct SectionItem {
    const char* type;     ///< Type of the Navigator's item, e.g. "section_body"
    core::BookPlace place;
    const char* iconId;   ///< Icon of the item
};

constexpr SectionItem SECTION_ITEMS[] = {
    {"section_frontmatter", core::BookPlace::Front, "structure.frontmatter"},
    {"section_body", core::BookPlace::Main, "structure.body"},
    {"section_backmatter", core::BookPlace::Back, "structure.backmatter"},
};

/// The section whose Navigator's item has type @p type, or nullptr
const SectionItem* sectionItem(const QString& type) {
    for (const SectionItem& section : SECTION_ITEMS) {
        if (type == QLatin1String(section.type)) {
            return &section;
        }
    }
    return nullptr;
}

/// @p paths one a line, with the separators of the system; several paths are indented, as a
/// list
QString pathLines(const QStringList& paths) {
    if (paths.size() == 1) {
        return QDir::toNativeSeparators(paths.first());
    }
    QStringList lines;
    for (const QString& path : paths) {
        lines << QStringLiteral("  ") + QDir::toNativeSeparators(path);
    }
    return lines.join(QLatin1Char('\n'));
}

/// Absolute path of @p file, relative to the folder of the open project
QString projectFilePath(const QString& file) {
    return QDir::cleanPath(
        QDir(core::ProjectManager::getInstance().getProjectPath()).filePath(file));
}

}  // namespace

NavigatorCoordinator::NavigatorCoordinator(NavigatorPanel* navigatorPanel,
                                             PropertiesPanel* propertiesPanel,
                                             QTabWidget* centralTabs,
                                             QStatusBar* statusBar,
                                             QObject* parent)
    : QObject(parent)
    , m_navigatorPanel(navigatorPanel)
    , m_propertiesPanel(propertiesPanel)
    , m_centralTabs(centralTabs)
    , m_statusBar(statusBar)
    , m_wordCountTimer(new QTimer(this))
{
    // The counts of the book follow the text being written, without counting at each key
    m_wordCountTimer->setSingleShot(true);
    m_wordCountTimer->setInterval(WORD_COUNT_DELAY_MS);
    connect(m_wordCountTimer, &QTimer::timeout, this, &NavigatorCoordinator::countWords);

    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorCoordinator created");
}

bool NavigatorCoordinator::isChapterDirty(const QString& elementId) const {
    return m_dirtyChapters.value(elementId, false);
}

void NavigatorCoordinator::setChapterDirty(const QString& elementId, bool dirty) {
    m_dirtyChapters[elementId] = dirty;
}

void NavigatorCoordinator::clearDirtyChapters() {
    m_dirtyChapters.clear();
    m_uncountedChapters.clear();
}

void NavigatorCoordinator::discardChapterChanges(const QString& elementId) {
    auto& logger = core::Logger::getInstance();

    // Forget the unsaved text ProjectManager keeps (single source of truth), with its words
    core::ProjectManager::getInstance().discardChapterContent(elementId);

    // Clear the display cache
    m_dirtyChapters[elementId] = false;
    m_uncountedChapters.remove(elementId);

    // Clear the navigator "*" indicator
    emit chapterDirtyStateChanged(elementId, false);

    // The counts of the book are those of the file again
    m_propertiesPanel->updateWordCounts();

    logger.debug("NavigatorCoordinator: Discarded changes for chapter: {}",
                 elementId.toStdString());
}

void NavigatorCoordinator::countWords() {
    auto& pm = core::ProjectManager::getInstance();
    const QSet<QString> changed = std::exchange(m_uncountedChapters, {});
    if (!pm.isProjectOpen() || changed.isEmpty()) {
        return;
    }
    for (int i = 0; i < m_centralTabs->count(); ++i) {
        auto* editor = qobject_cast<EditorPanel*>(m_centralTabs->widget(i));
        if (!editor) {
            continue;
        }
        // A chapter saved or with its changes discarded meanwhile has the words of its file
        const QString elementId = editor->property("elementId").toString();
        if (!changed.contains(elementId) || !m_dirtyChapters.value(elementId, false)) {
            continue;
        }
        // Under any rules, so that the counts of the book follow new ones at once
        if (const editor::BookEditor* bookEditor = editor->getBookEditor()) {
            pm.setEditorWords(elementId, core::WordTally::of(bookEditor->textCounts(),
                                                             bookEditor->wordCountRules()));
        }
    }
    m_propertiesPanel->updateWordCounts();
}

EditorPanel* NavigatorCoordinator::getCurrentEditor() const {
    if (!m_centralTabs) {
        return nullptr;
    }
    QWidget* currentWidget = m_centralTabs->currentWidget();
    return qobject_cast<EditorPanel*>(currentWidget);
}

void NavigatorCoordinator::refreshNavigator() {
    auto& pm = core::ProjectManager::getInstance();
    if (const core::BookProject* project = pm.project()) {
        // Rebuilding the tree resets expansion to defaults, so keep the user's state
        const QStringList expandedIds = m_navigatorPanel->expandedItemIds();
        m_navigatorPanel->loadProject(*project, pm.bookTypes());
        m_navigatorPanel->setExpandedItemIds(expandedIds);
        // The files no element has, after the elements: it keeps whether it was open
        m_navigatorPanel->setUnassignedFiles(pm.unassignedFiles(), pm.getProjectPath());
    }
    emit refreshNavigatorRequested();
}

void NavigatorCoordinator::refreshTabTitle(const QString& elementId) {
    const core::ProjectElement* element =
        core::ProjectManager::getInstance().findElement(elementId);
    if (!element) {
        return;
    }
    // The "*" dirty indicator stays
    const QString title =
        m_dirtyChapters.value(elementId, false) ? "*" + element->title : element->title;
    for (int i = 0; i < m_centralTabs->count(); ++i) {
        QWidget* widget = m_centralTabs->widget(i);
        if (widget && widget->property("elementId").toString() == elementId) {
            m_centralTabs->setTabText(i, title);
        }
    }
}

void NavigatorCoordinator::onElementSelected(const QString& elementId, const QString& elementTitle) {
    auto& logger = core::Logger::getInstance();
    auto startTime = std::chrono::high_resolution_clock::now();
    auto logElapsed = [&](const char* step) {
        auto now = std::chrono::high_resolution_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
        logger.info("NavigatorCoordinator::onElementSelected [{}ms] {}", ms, step);
    };

    logElapsed("START");
    logger.info("Navigator element selected: {} (id={})",
                elementTitle.toStdString(), elementId.toStdString());

    // Check if project is loaded via ProjectManager
    auto& pm = core::ProjectManager::getInstance();
    if (!pm.isProjectOpen()) {
        logger.debug("No project loaded - ignoring Navigator selection");
        m_statusBar->showMessage(tr("No book loaded"), 2000);
        return;
    }

    // Check if this chapter is already open in a tab - if so, switch to it
    for (int i = 0; i < m_centralTabs->count(); ++i) {
        QWidget* widget = m_centralTabs->widget(i);
        if (widget && widget->property("elementId").toString() == elementId) {
            logger.debug("Chapter already open in tab {} - switching to it", i);
            m_centralTabs->setCurrentIndex(i);
            m_currentElementId = elementId;
            return;
        }
    }

    // Switching chapters does NOT prompt to save. The current chapter's tab stays
    // open with its "*" modified indicator (tab + navigator tree), and any unsaved
    // changes are reported once at application close. This avoids nagging on every
    // chapter switch (per the user's UX design: signal with an icon, prompt at close).

    // Load chapter content from file via ProjectManager
    logElapsed("Before loadChapterContent");
    QString content = pm.loadChapterContent(elementId);
    logElapsed("After loadChapterContent");

    if (content.isEmpty()) {
        logger.warn("Failed to load content for element: {}", elementId.toStdString());
        // Still create tab but with empty content
    }

    // Create new editor tab with the element's icon, as in the navigator
    logElapsed("Before new EditorPanel");
    EditorPanel* newEditor = new EditorPanel(m_centralTabs);
    logElapsed("After new EditorPanel");
    // A chapter of a book open read-only is read, copied and searched only; before the tab
    // comes, as the commands follow the tab in front
    newEditor->getBookEditor()->setReadOnly(pm.isReadOnly());
    const core::ProjectElement* element = pm.findElement(elementId);
    // A kind the program does not have: its icon on the book's card of its package
    const core::PackageCard* card =
        element != nullptr && element->hasKind() && !pm.kindOf(*element)
            ? pm.packageCard(element->kind.packageId)
            : nullptr;
    const QString iconId = element ? NavigatorPanel::iconIdOf(pm.bookTypes(), *element, card)
                                   : QStringLiteral("template.chapter");
    QIcon chapterIcon = core::ArtProvider::getInstance().getPackageIcon(iconId, card);
    // Store element ID for save operations, before the tab comes: the tab in front decides
    // what Save and Save As do
    newEditor->setProperty("elementId", elementId);
    int tabIndex = m_centralTabs->addTab(newEditor, chapterIcon, elementTitle);
    newEditor->setProperty("tabIconId", iconId);
    if (card != nullptr) {
        newEditor->setProperty("tabIconPackage", card->id);  // refreshTabIcons()
    }
    m_centralTabs->setCurrentIndex(tabIndex);
    m_currentElementId = elementId;

    // The text loaded is the saved text (BookEditor::fromKml()); the signals below are
    // connected after it, so loading counts no words
    logElapsed("Before setContent");
    const bool complete = newEditor->setContent(content);
    logElapsed("After setContent");

    // The tab and the Navigator show a "*" while the text differs from the saved text: a
    // letter typed and deleted again, or an edit undone, leaves none
    connect(newEditor, &EditorPanel::modifiedChanged, this,
            [this, elementId, newEditor](bool modified) {
                showChapterModified(elementId, newEditor, modified);
            });
    // The words of the book follow the text, before it is saved
    connect(newEditor, &EditorPanel::contentChanged, this, [this, elementId]() {
        if (core::ProjectManager::getInstance().isProjectOpen()) {
            m_uncountedChapters.insert(elementId);
            m_wordCountTimer->start();
        }
    });

    // Connect to statistics collector if available (OpenSpec #00042 Task 7.7)
    if (m_statisticsCollector) {
        newEditor->setStatisticsCollector(m_statisticsCollector);
        logger.debug("NavigatorCoordinator: EditorPanel connected to StatisticsCollector");
    }

    // Update PropertiesPanel to show chapter properties
    m_propertiesPanel->showChapterProperties(elementId);

    emit elementOpened(elementId);

    logger.info("Opened chapter: {} ({})", elementTitle.toStdString(), elementId.toStdString());
    m_statusBar->showMessage(tr("Opened: %1").arg(elementTitle), 2000);

    if (!complete) {
        const core::KindWords kind = element ? wordsOf(*element) : mainWords();
        EditorPanel::warnDamagedChapter(m_centralTabs->window(), elementTitle, &kind);
    }
}

void NavigatorCoordinator::onRequestRename(const QString& elementId, const QString& currentTitle) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) {
        logger.warn("NavigatorCoordinator: Rename requested but no project open");
        return;
    }

    Q_UNUSED(currentTitle);  // display text is decorated; the clean title comes from the model

    // Resolve the target and read its REAL title from the model. The passed-in
    // currentTitle is the tree item's DISPLAY text, which carries decorations (the
    // "*" dirty indicator and a " [Status]" suffix); using it would leak those into
    // the renamed title (e.g. a doubled "[Draft]").
    const core::ProjectElement* element = pm.findElement(elementId);
    if (!element) {
        logger.warn("NavigatorCoordinator: Element not found for rename: {}",
                    elementId.toStdString());
        return;
    }
    const QString cleanTitle = element->title;

    // Ask for the new name (it starts as the clean title)
    dialogs::RenameElementDialog dialog(cleanTitle,
                                        NavigatorPanel::iconIdOf(pm.bookTypes(), *element),
                                        qobject_cast<QWidget*>(parent()));
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QString newTitle = dialog.name();
    if (newTitle.isEmpty() || newTitle == cleanTitle) {
        return;  // No change
    }

    // ProjectManager saves the project at once
    if (!pm.renameElement(elementId, newTitle)) {
        logger.error("NavigatorCoordinator: Failed to save the project after rename");
        dialogs::MessageDialog::warning(qobject_cast<QWidget*>(parent()), tr("Rename Failed"),
                                        tr("Failed to save changes."));
        return;
    }
    logger.info("NavigatorCoordinator: Renamed element '{}' to '{}'",
                elementId.toStdString(), newTitle.toStdString());

    // Refresh navigator and the open tab to show new name; in the Workshop the element goes
    // to the place of its new title
    refreshNavigator();
    m_navigatorPanel->revealElement(elementId);
    refreshTabTitle(elementId);

    m_statusBar->showMessage(tr("Renamed to '%1'").arg(newTitle), 2000);
    emit documentModified();
}

void NavigatorCoordinator::onRequestDelete(const QString& elementId) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();
    auto* window = qobject_cast<QWidget*>(parent());

    if (!pm.isProjectOpen()) {
        logger.warn("NavigatorCoordinator: Delete requested but no project open");
        return;
    }

    const core::ProjectElement* found = pm.findElement(elementId);
    if (!found) {
        logger.warn("NavigatorCoordinator: Element not found for delete: {}", elementId.toStdString());
        dialogs::MessageDialog::warning(window, tr("Delete Failed"),
                                        tr("Could not find the element to delete."));
        return;
    }
    // A copy: deleting it changes the project
    const core::ProjectElement element = *found;

    // The window lists what goes from the book and says where the files go: they stay in the
    // book's folder, or go to the system's Trash, or for good from a folder without one
    dialogs::DeleteFromBookDialog::Content content;
    content.element = element;
    content.files = pm.filesOf(element);
    content.projectFolder = pm.getProjectPath();
    content.trash = !content.files.isEmpty() &&
                    std::all_of(content.files.cbegin(), content.files.cend(),
                                [](const QString& file) { return core::canMoveToTrash(file); });
    content.unassigned =
        std::all_of(content.files.cbegin(), content.files.cend(),
                    [&pm](const QString& file) { return pm.listsAsUnassigned(file); });
    dialogs::DeleteFromBookDialog dialog(content, pm.bookTypes(), window);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const bool withFiles = dialog.deleteFiles();

    // The files that stay, or go to the Trash, keep the changes of the open tabs
    if (!withFiles || content.trash) {
        const QList<const core::ProjectElement*> unsaved = saveTextsOf(element);
        if (!unsaved.isEmpty()) {
            QString question;
            if (unsaved.size() == 1) {
                const QHash<QString, core::KindWords> nouns{
                    {QStringLiteral("kind"), wordsOf(*unsaved.first())}};
                //: In Polish: Zmian w {kind:locative} „%1” nie udało się zapisać w pliku.
                //: Mimo to usunąć {kind:m=go|f=ją|n=je|p=je} z książki? Plik zachowa tekst
                //: zapisany wcześniej.
                question = core::fillWords(tr("The changes in the {kind} \"%1\" could not be "
                                              "saved to its file.\n\nDelete {kind:s=it|p=them} "
                                              "from the book anyway? The file keeps the text "
                                              "saved before."),
                                           nouns)
                               .arg(unsaved.first()->title);
            } else {
                QStringList titles;
                for (const core::ProjectElement* each : unsaved) {
                    titles << QStringLiteral("  ") + each->title;
                }
                question = tr("The changes in these elements could not be saved to their "
                              "files:\n%1\n\nDelete them from the book anyway? Their files keep "
                              "the text saved before.")
                               .arg(titles.join(QLatin1Char('\n')));
            }
            if (!dialogs::MessageDialog::confirm(window, tr("Delete from Book"), question,
                                                 tr("Delete &Anyway"),
                                                 dialogs::MessageDialog::Kind::Warning,
                                                 dialogs::MessageDialog::DefaultButton::Cancel)) {
                return;
            }
        }
    }

    // ProjectManager takes the element, with the elements inside it, out of the project and
    // saves it at once
    const std::optional<core::ProjectElement> removed = pm.removeElement(elementId);
    if (!removed) {
        logger.error("NavigatorCoordinator: Failed to save the project after delete");
        dialogs::MessageDialog::warning(window, tr("Delete Failed"),
                                        tr("Failed to save changes."));
        return;
    }
    logger.info("NavigatorCoordinator: Deleted element: {}", elementId.toStdString());
    closeTabsOf(*removed);

    // The files go to the Trash, or for good from a folder without one; those that stay are
    // under "Unassigned Files"
    if (withFiles) {
        deleteFiles(content.files, content.trash);
    }
    refreshNavigator();
    m_propertiesPanel->refresh();  // a deleted element leaves the panel to the book

    const QHash<QString, core::KindWords> nouns{{QStringLiteral("kind"), wordsOf(element)}};
    //: In Polish: Usunięto {kind:accusative} z książki: %1
    m_statusBar->showMessage(
        core::fillWords(tr("{Kind} deleted from the book: %1"), nouns).arg(element.title), 2000);
    emit documentModified();
}

QList<const core::ProjectElement*> NavigatorCoordinator::saveTextsOf(
    const core::ProjectElement& element) {
    auto& pm = core::ProjectManager::getInstance();
    QList<const core::ProjectElement*> unsaved;

    // A text with a file gets the changes of its open tab; a part or a window element has no
    // text
    if (pm.formOf(element) == core::ElementForm::Text && !element.file.isEmpty()) {
        EditorPanel* editor = nullptr;
        for (int i = 0; i < m_centralTabs->count() && !editor; ++i) {
            auto* tab = qobject_cast<EditorPanel*>(m_centralTabs->widget(i));
            if (tab && tab->property("elementId").toString() == element.id) {
                editor = tab;
            }
        }
        const bool changed = editor && editor->isModified();  // than the saved text
        if (changed) {
            pm.setChapterContent(element.id, editor->getContent());
        }
        if (!pm.saveChapterContent(element.id)) {
            unsaved.append(&element);
        } else if (changed) {
            // Saved: the tab and the Navigator no longer show changes
            m_dirtyChapters[element.id] = false;
            emit chapterDirtyStateChanged(element.id, false);
            editor->setUnmodified();
            const int tab = m_centralTabs->indexOf(editor);
            if (m_centralTabs->tabText(tab).startsWith(QLatin1Char('*'))) {
                m_centralTabs->setTabText(tab, m_centralTabs->tabText(tab).mid(1));
            }
        }
        // The file goes back to the book with its title, status and notes: a book made before
        // Kalahari wrote them to the files has others there
        if (!unsaved.contains(&element) && !pm.saveChapterDetails(element.id)) {
            core::Logger::getInstance().warn("NavigatorCoordinator: The file of '{}' keeps "
                                             "the title, the status and the notes it had",
                                             element.title.toStdString());
        }
    }
    for (const core::ProjectElement& inner : element.elements) {
        unsaved.append(saveTextsOf(inner));
    }
    return unsaved;
}

QStringList NavigatorCoordinator::deleteFiles(const QStringList& files, bool trash,
                                             bool* forGood) {
    auto& logger = core::Logger::getInstance();
    auto* window = qobject_cast<QWidget*>(parent());
    bool gone = false;
    QStringList refused;
    QStringList failed;
    for (const QString& file : files) {
        if (trash) {
            if (!core::moveToTrash(file)) {
                refused << file;
            }
        } else if (QFile::remove(file)) {
            gone = true;
        } else {
            failed << file;
        }
    }

    // What the Trash did not take goes for good only when the writer says so
    QStringList kept;
    if (!refused.isEmpty()) {
        const bool one = refused.size() == 1;
        const QHash<QString, core::KindWords> nouns{
            {QStringLiteral("bin"), dialogs::trashWords()}};
        const QString question =
            core::fillWords(one ? tr("The {bin} did not take the file:\n%1\n\nDelete it for "
                                     "good? It cannot be restored then.")
                                : tr("The {bin} did not take these files:\n%1\n\nDelete them "
                                     "for good? They cannot be restored then."),
                            nouns)
                .arg(pathLines(refused));
        dialogs::MessageDialog dialog(dialogs::MessageDialog::Kind::Warning,
                                      one ? tr("Delete File") : tr("Delete Files"), question,
                                      window);
        dialog.setQuestionButtons(tr("Delete for &Good"),
                                  one ? tr("&Keep the File") : tr("&Keep the Files"));
        dialog.setDefaultButton(dialogs::MessageDialog::DefaultButton::Cancel);
        dialog.exec();
        if (dialog.answer() == dialogs::MessageDialog::Answer::Accept) {
            for (const QString& file : std::as_const(refused)) {
                if (QFile::remove(file)) {
                    gone = true;
                } else {
                    failed << file;
                }
            }
        } else {
            kept << refused;
        }
    }

    if (!failed.isEmpty()) {
        logger.error("NavigatorCoordinator: Could not delete {} file(s)", failed.size());
        const bool one = failed.size() == 1;
        dialogs::MessageDialog::error(window, one ? tr("Delete File") : tr("Delete Files"),
                                      (one ? tr("The file could not be deleted:\n%1")
                                           : tr("These files could not be deleted:\n%1"))
                                          .arg(pathLines(failed)));
        kept << failed;
    }
    if (forGood) {
        *forGood = gone;
    }
    logger.info("NavigatorCoordinator: Deleted {} of {} file(s){}", files.size() - kept.size(),
                files.size(), trash ? " to the Trash" : "");
    return kept;
}

void NavigatorCoordinator::onRequestPreviewUnassigned(const QString& file) {
    auto& pm = core::ProjectManager::getInstance();
    auto* window = qobject_cast<QWidget*>(parent());
    if (!pm.isProjectOpen()) {
        return;
    }

    const QString path = projectFilePath(file);
    const std::optional<core::ChapterDocument> chapter = core::ChapterDocument::load(path);
    if (!chapter) {
        dialogs::MessageDialog::warning(
            window, tr("Preview"),
            tr("The file could not be read:\n%1").arg(QDir::toNativeSeparators(path)));
        refreshNavigator();  // The Navigator shows the files there are
        return;
    }
    dialogs::FilePreviewDialog dialog(*chapter, path, pm.getProjectPath(), window);
    dialog.exec();
    switch (dialog.choice()) {
    case dialogs::FilePreviewDialog::Choice::AddToBook:
        onRequestAddUnassigned(file);
        break;
    case dialogs::FilePreviewDialog::Choice::Delete:
        onRequestDeleteUnassigned(file);
        break;
    case dialogs::FilePreviewDialog::Choice::Close:
        break;
    }
}

void NavigatorCoordinator::onRequestAddUnassigned(const QString& file) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();
    auto* window = qobject_cast<QWidget*>(parent());
    if (!pm.isProjectOpen()) {
        return;
    }

    // The element takes the title, the status and the notes of the file
    const QString path = projectFilePath(file);
    const std::optional<core::ChapterDocument> chapter = core::ChapterDocument::load(path);
    if (!chapter) {
        dialogs::MessageDialog::warning(
            window, tr("Add to Book"),
            tr("The file could not be read:\n%1").arg(QDir::toNativeSeparators(path)));
        refreshNavigator();
        return;
    }
    const dialogs::AddToBookDialog::Book book = dialogs::AddToBookDialog::openBook(path);
    if (book.choices.isEmpty()) {
        dialogs::MessageDialog::warning(window, tr("Add to Book"),
                                        tr("No element of this book can have a text."));
        return;
    }
    dialogs::AddToBookDialog dialog({path, chapter->title(), chapter->status(), true}, book,
                                    pm.bookTypes(), window);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const core::ElementPlace at = dialog.place();
    const QString title = dialog.title();
    const QHash<QString, core::KindWords> nouns{{QStringLiteral("kind"), wordsOf(dialog.kind())}};
    const QString elementId =
        pm.addUnassignedFile(file, dialog.kind(), title, at.place, at.groupId, at.index);
    if (elementId.isEmpty()) {
        logger.error("NavigatorCoordinator: Failed to add '{}' to the book", file.toStdString());
        //: In Polish: Nie udało się dodać {kind:genitive}
        dialogs::MessageDialog::warning(window, core::fillWords(tr("Add {Kind} Failed"), nouns),
                                        tr("The file could not be added to the book. The "
                                           "program's log has the details."));
        refreshNavigator();
        return;
    }
    logger.info("NavigatorCoordinator: Added '{}' to the book as '{}' (id={})",
                file.toStdString(), title.toStdString(), elementId.toStdString());

    // The new element is shown, also inside a collapsed part
    refreshNavigator();
    m_navigatorPanel->revealElement(elementId);
    //: In Polish: Dodano {kind:accusative} do książki: %1
    m_statusBar->showMessage(
        core::fillWords(tr("{Kind} added to the book: %1"), nouns).arg(title), 2000);
    emit documentModified();
}

void NavigatorCoordinator::onRequestDeleteUnassigned(const QString& file) {
    auto& pm = core::ProjectManager::getInstance();
    if (!pm.isProjectOpen()) {
        return;
    }
    QList<core::UnassignedFile> files = pm.unassignedFiles();
    files.removeIf([&file](const core::UnassignedFile& each) { return each.file != file; });
    deleteUnassigned(files);
}

void NavigatorCoordinator::onRequestDeleteAllUnassigned() {
    auto& pm = core::ProjectManager::getInstance();
    if (pm.isProjectOpen()) {
        deleteUnassigned(pm.unassignedFiles());
    }
}

void NavigatorCoordinator::deleteUnassigned(const QList<core::UnassignedFile>& files) {
    // Files that are gone already: the Navigator shows the files there are
    if (files.isEmpty()) {
        refreshNavigator();
        return;
    }

    auto& pm = core::ProjectManager::getInstance();
    QStringList paths;
    for (const core::UnassignedFile& file : files) {
        paths << projectFilePath(file.file);
    }
    const bool trash = std::all_of(paths.cbegin(), paths.cend(), [](const QString& path) {
        return core::canMoveToTrash(path);
    });
    dialogs::DeleteFilesDialog dialog(files, pm.getProjectPath(), trash,
                                      qobject_cast<QWidget*>(parent()));
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    bool forGood = false;
    const QStringList kept = deleteFiles(paths, trash, &forGood);
    refreshNavigator();

    // What happened to them, as the window said it would
    const qsizetype deleted = paths.size() - kept.size();
    if (deleted > 0) {
        const bool inTrash = trash && !forGood;
        const QHash<QString, core::KindWords> nouns{
            {QStringLiteral("bin"), dialogs::trashWords()}};
        QString message;
        if (paths.size() == 1) {
            //: In Polish: Przeniesiono plik do Kosza: %1
            message = inTrash ? core::fillWords(tr("File moved to the {bin}: %1"), nouns)
                              : tr("File deleted: %1");
            message = message.arg(QFileInfo(paths.first()).fileName());
        } else {
            //: In Polish: Przeniesiono pliki do Kosza: %1
            message = inTrash ? core::fillWords(tr("Files moved to the {bin}: %1"), nouns)
                              : tr("Files deleted: %1");
            message = message.arg(deleted);
        }
        m_statusBar->showMessage(message, 2000);
    }
}

void NavigatorCoordinator::closeTabsOf(const core::ProjectElement& element) {
    for (int i = m_centralTabs->count() - 1; i >= 0; --i) {
        QWidget* widget = m_centralTabs->widget(i);
        if (widget && widget->property("elementId").toString() == element.id) {
            m_centralTabs->removeTab(i);
            widget->deleteLater();
        }
    }

    // Its unsaved changes went with it
    if (m_dirtyChapters.take(element.id)) {
        emit chapterDirtyStateChanged(element.id, false);
    }
    if (m_currentElementId == element.id) {
        m_currentElementId.clear();
    }

    for (const core::ProjectElement& inner : element.elements) {
        closeTabsOf(inner);
    }
}

void NavigatorCoordinator::showChapterModified(const QString& elementId, EditorPanel* editor,
                                              bool modified) {
    if (!core::ProjectManager::getInstance().isProjectOpen()
        || modified == m_dirtyChapters.value(elementId, false)) {
        return;  // shown already, e.g. by the save that made the text the saved text
    }
    const int index = m_centralTabs->indexOf(editor);
    const QString tabText = index >= 0 ? m_centralTabs->tabText(index) : QString();
    if (modified) {
        // Only the text of the chapter changed, not the book (.klh): its structure and data are
        // saved at once
        m_dirtyChapters[elementId] = true;
        if (index >= 0 && !tabText.startsWith(QLatin1Char('*'))) {
            m_centralTabs->setTabText(index, QLatin1Char('*') + tabText);
        }
        emit chapterDirtyStateChanged(elementId, true);
        emit documentModified();
    } else {
        // Back to the saved text: nothing to save, and the words are those of its file
        discardChapterChanges(elementId);
        if (tabText.startsWith(QLatin1Char('*'))) {
            m_centralTabs->setTabText(index, tabText.mid(1));
        }
    }
}

void NavigatorCoordinator::onRequestMove(const QString& elementId, int direction) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    core::BookProject* project = pm.project();
    if (!project) {
        logger.warn("NavigatorCoordinator: Move requested but no project open");
        return;
    }

    // An element moves within its list: a section of the book or a part
    qsizetype index = -1;
    const QList<core::ProjectElement>* list = project->listOf(elementId, &index);
    if (!list) {
        logger.warn("NavigatorCoordinator: Element not found for move: {}",
                    elementId.toStdString());
        return;
    }
    const qsizetype newIndex = index + direction;
    if (newIndex < 0 || newIndex >= list->size()) {
        logger.debug("NavigatorCoordinator: Could not move element: {} (at boundary)",
                     elementId.toStdString());
        return;
    }

    // ProjectManager saves the project at once
    if (!pm.moveElement(elementId, newIndex)) {
        logger.error("NavigatorCoordinator: Failed to save the project after move");
        return;
    }
    logger.info("NavigatorCoordinator: Moved element {} from {} to {}",
                elementId.toStdString(), index, newIndex);

    refreshNavigator();
    m_navigatorPanel->revealElement(elementId);
    m_statusBar->showMessage(direction < 0 ? tr("Moved up") : tr("Moved down"), 2000);
    emit documentModified();
}

void NavigatorCoordinator::onRequestProperties(const QString& elementId) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) {
        logger.warn("NavigatorCoordinator: Properties requested but no project open");
        return;
    }

    if (elementId.isEmpty() || elementId == "document") {
        // Show project properties
        logger.debug("NavigatorCoordinator: Showing project properties");
        m_propertiesPanel->showProjectProperties();
    } else {
        // Show element properties
        logger.debug("NavigatorCoordinator: Showing properties for element: {}", elementId.toStdString());
        m_propertiesPanel->showChapterProperties(elementId);
    }
}

void NavigatorCoordinator::onRequestSectionProperties(const QString& sectionType) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) {
        logger.warn("NavigatorCoordinator: Section properties requested but no project open");
        return;
    }

    logger.debug("NavigatorCoordinator: Showing section properties: {}", sectionType.toStdString());
    m_propertiesPanel->showSectionProperties(sectionType);
}

void NavigatorCoordinator::onRequestPartProperties(const QString& partId) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) {
        logger.warn("NavigatorCoordinator: Part properties requested but no project open");
        return;
    }

    logger.debug("NavigatorCoordinator: Showing part properties: {}", partId.toStdString());
    m_propertiesPanel->showPartProperties(partId);
}

void NavigatorCoordinator::onElementMoved(const QString& elementId, int index) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    // ProjectManager saves the project at once
    if (pm.moveElement(elementId, index)) {
        logger.info("NavigatorCoordinator: Moved element {} to place {}",
                    elementId.toStdString(), index);
        emit documentModified();
    } else {
        logger.error("NavigatorCoordinator: Failed to move element {}", elementId.toStdString());
    }

    // The tree shows the order of the project, also when the move failed
    refreshNavigator();
    m_navigatorPanel->revealElement(elementId);
}

void NavigatorCoordinator::onRequestMoveTo(const QString& elementId) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();
    if (!pm.isProjectOpen()) {
        logger.warn("NavigatorCoordinator: Move requested but no project open");
        return;
    }

    const dialogs::MoveElementDialog::Book book = dialogs::MoveElementDialog::openBook(elementId);
    if (book.sections.isEmpty()) {
        logger.warn("NavigatorCoordinator: Element not found for move: {}",
                    elementId.toStdString());
        refreshNavigator();
        return;
    }
    dialogs::MoveElementDialog dialog(book, pm.bookTypes(), qobject_cast<QWidget*>(parent()));
    if (dialog.exec() == QDialog::Accepted && !(dialog.place() == book.from)) {
        moveElementTo(elementId, dialog.place());
    }
}

void NavigatorCoordinator::onElementMovedTo(const QString& elementId,
                                            const core::ElementPlace& place) {
    moveElementTo(elementId, place);
}

void NavigatorCoordinator::moveElementTo(const QString& elementId,
                                         const core::ElementPlace& place) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();
    const core::ProjectElement* element = pm.findElement(elementId);
    if (!element) {
        logger.warn("NavigatorCoordinator: Element not found for move: {}",
                    elementId.toStdString());
        refreshNavigator();
        return;
    }
    const QString title = element->title;
    const QHash<QString, core::KindWords> nouns{{QStringLiteral("kind"), wordsOf(*element)}};

    // ProjectManager saves the project at once
    if (!pm.moveElementTo(elementId, place)) {
        logger.error("NavigatorCoordinator: Failed to move element {}", elementId.toStdString());
        refreshNavigator();
        //: In Polish: Nie udało się przenieść {kind:genitive}
        dialogs::MessageDialog::warning(qobject_cast<QWidget*>(parent()),
                                        core::fillWords(tr("Move {Kind} Failed"), nouns),
                                        tr("The changes in the book could not be saved. The "
                                           "program's log has the details."));
        return;
    }

    // The element is shown at its new place, also inside a collapsed part
    refreshNavigator();
    m_navigatorPanel->revealElement(elementId);
    m_propertiesPanel->refresh();
    //: %1: the title of the element. In Polish: Przeniesiono {kind:accusative}: %1
    m_statusBar->showMessage(core::fillWords(tr("{Kind} moved: %1"), nouns).arg(title), 2000);
    emit documentModified();
}


// =============================================================================
// Add Element
// =============================================================================

void NavigatorCoordinator::onRequestAdd(const AddTarget& target) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();
    auto* window = qobject_cast<QWidget*>(parent());

    if (!pm.isProjectOpen() || pm.isReadOnly()) {
        logger.warn("NavigatorCoordinator: Add requested but no book can be changed");
        return;
    }

    // The window starts at the place it was opened on; without a choice at its top it offers
    // the kinds of that place only
    const dialogs::NewFileDialog::Book book =
        dialogs::NewFileDialog::forAdding(target, NavigatorPanel::addChooser());
    if (book.kinds.isEmpty()) {
        logger.warn("NavigatorCoordinator: Nothing can be added there");
        return;
    }
    dialogs::NewFileDialog dialog(book, pm.bookTypes(), window);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const core::KindRef kind = dialog.kind();
    const QString title = dialog.title();
    // The messages name the kind added: "Working note added", "Could not add a story"
    const QHash<QString, core::KindWords> nouns{{QStringLiteral("kind"), wordsOf(kind)}};

    // A material is a copy of the file the writer chose, in the Workshop's folder; any other
    // element gets its chapter file, and ProjectManager saves the project at once
    const bool material = kind && kind.kind->isMaterial();
    const core::ElementPlace at = dialog.place();
    const QString elementId =
        material ? pm.addMaterial(dialog.materialFile(), kind, title)
                 : pm.addElement(kind, title, at.place, at.groupId, at.index, dialog.takeInside(),
                                 std::nullopt, dialog.status());
    if (elementId.isEmpty()) {
        logger.error("NavigatorCoordinator: Failed to add '{}'", title.toStdString());
        //: In Polish: Nie udało się dodać {kind:genitive}
        const QString heading = core::fillWords(tr("Add {Kind} Failed"), nouns);
        if (material) {
            //: %1: the file the writer chose. In Polish: Nie udało się dodać kopii pliku do
            //: książki:\n%1\nSzczegóły są w dzienniku programu.
            dialogs::MessageDialog::warning(
                window, heading,
                tr("The copy of the file could not be added to the book:\n%1\nThe program's "
                   "log has the details.")
                    .arg(QDir::toNativeSeparators(dialog.materialFile())));
        } else {
            dialogs::MessageDialog::warning(window, heading, tr("Failed to save changes."));
        }
        return;
    }
    logger.info("NavigatorCoordinator: Added '{}' (id={})", title.toStdString(),
                elementId.toStdString());

    showAdded(elementId, title);
    //: In Polish: Dodano {kind:accusative}: %1
    m_statusBar->showMessage(core::fillWords(tr("{Kind} added: %1"), nouns).arg(title), 2000);
    emit documentModified();
}

void NavigatorCoordinator::onRequestAddHere() {
    onRequestAdd(addTargetHere());
}

void NavigatorCoordinator::onRequestNewChapter() {
    const auto& pm = core::ProjectManager::getInstance();
    const core::BookProject* project = pm.project();
    if (!project) {
        core::Logger::getInstance().warn(
            "NavigatorCoordinator: New chapter requested but no project open");
        return;
    }

    // Right after the element in front when its list can have a chapter there (in a part, the
    // kind of a chapter in it); else in the whole book, at the end of the main section
    AddTarget target = addTargetHere();
    const std::optional<core::ElementPlace> at =
        target.scope == AddTarget::Scope::Element ? project->placeOf(target.id) : std::nullopt;
    const core::KindRef chapter =
        at && at->place == core::BookPlace::Main ? pm.chapterKindFor(at->place, at->groupId)
                                                 : core::KindRef{};
    if (chapter && pm.newPlaceAfter(target.id, chapter)) {
        target.kind = chapter;
    } else {
        target = AddTarget{};
        target.kind = pm.chapterKindFor(core::BookPlace::Main);
    }
    onRequestAdd(target);
}

AddTarget NavigatorCoordinator::addTargetHere() const {
    const auto& pm = core::ProjectManager::getInstance();
    const core::BookProject* project = pm.project();
    AddTarget target;  // The whole book
    const EditorPanel* editor = getCurrentEditor();
    if (!project || !editor) {
        return target;
    }
    const QString elementId = editor->property("elementId").toString();
    const core::ProjectElement* element = pm.findElement(elementId);
    const std::optional<core::ElementPlace> at = project->placeOf(elementId);
    if (!element || !at) {
        return target;  // A file of the book outside its text, or a file outside the book
    }

    // An element of the Workshop: the group of its kind; one of the book: right after it
    if (at->place == core::BookPlace::Workshop) {
        target.scope = AddTarget::Scope::Workshop;
        if (const core::KindRef kind = pm.kindOf(*element)) {
            target.kinds.append(kind);
            target.kind = kind;
        }
    } else {
        target.scope = AddTarget::Scope::Element;
        target.id = elementId;
    }
    return target;
}

void NavigatorCoordinator::showAdded(const QString& elementId, const QString& title) {
    auto& pm = core::ProjectManager::getInstance();

    // The new element is shown, also inside a collapsed part
    refreshNavigator();
    m_navigatorPanel->revealElement(elementId);
    const core::ProjectElement* element = pm.findElement(elementId);
    if (!element || pm.formOf(*element) != core::ElementForm::Text) {
        return;
    }

    // A text opens in the editor, ready for writing at its start
    onElementSelected(elementId, title);
    if (EditorPanel* editor = getCurrentEditor();
        editor != nullptr && editor->property("elementId").toString() == elementId) {
        editor->getBookEditor()->setCursorPosition(editor::CursorPosition{});
        editor->getBookEditor()->setFocus(Qt::OtherFocusReason);
    }
}

void NavigatorCoordinator::onRequestRenameNode(const QString& itemType) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    const core::BookProject* project = pm.project();
    const core::ProjectBook* book = pm.book();
    const bool workshop = itemType == QLatin1String("workshop");
    if (!project || !book || (!workshop && itemType != QLatin1String("book"))) {
        logger.warn("NavigatorCoordinator: No node to rename: {}", itemType.toStdString());
        return;
    }

    const QString currentName =
        workshop ? NavigatorPanel::workshopName(*project) : NavigatorPanel::bookName(*book);
    dialogs::RenameElementDialog dialog(
        currentName, workshop ? QStringLiteral("structure.workshop") : QStringLiteral("structure.book"),
        qobject_cast<QWidget*>(parent()));
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QString newName = dialog.name();
    if (newName.isEmpty() || newName == currentName) {
        return;  // No change
    }

    // The default name is no name of the writer's own: it follows the language of the program
    const QString defaultName = workshop ? NavigatorPanel::workshopName(core::BookProject{})
                                         : NavigatorPanel::bookName(core::ProjectBook{});
    const QString name = newName == defaultName ? QString() : newName;
    if (!(workshop ? pm.setWorkshopName(name) : pm.setBookName(name))) {
        logger.error("NavigatorCoordinator: Failed to save the project after renaming the {}",
                     itemType.toStdString());
        dialogs::MessageDialog::warning(qobject_cast<QWidget*>(parent()), tr("Rename Failed"),
                                        tr("Failed to save changes."));
        return;
    }
    logger.info("NavigatorCoordinator: Renamed the {} '{}' to '{}'", itemType.toStdString(),
                currentName.toStdString(), newName.toStdString());

    refreshNavigator();
    m_statusBar->showMessage(tr("Renamed to '%1'").arg(newName), 2000);
    emit documentModified();
}

void NavigatorCoordinator::onRequestWorkshopGrouped(bool grouped) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    const core::BookProject* project = pm.project();
    if (!project) {
        logger.warn("NavigatorCoordinator: Workshop grouping requested but no project open");
        return;
    }
    if (project->workshop.grouped == grouped) {
        return;
    }
    if (!pm.setWorkshopGrouped(grouped)) {
        logger.error("NavigatorCoordinator: Failed to save the project after grouping the "
                     "Workshop");
        //: In Polish: Nie udało się zmienić grupowania
        dialogs::MessageDialog::warning(qobject_cast<QWidget*>(parent()),
                                        tr("Grouping Failed"), tr("Failed to save changes."));
        refreshNavigator();  // The check of its menu shows the Workshop as it still is
        return;
    }
    logger.info("NavigatorCoordinator: Workshop {}", grouped ? "grouped" : "not grouped");

    refreshNavigator();
    m_statusBar->showMessage(grouped
                                 //: In Polish: Warsztat jest pogrupowany
                                 ? tr("Workshop grouped")
                                 //: In Polish: Warsztat nie jest pogrupowany
                                 : tr("Workshop not grouped"),
                             2000);
    emit documentModified();
}

void NavigatorCoordinator::onRequestOpenMaterial(const QString& elementId) {
    const auto& pm = core::ProjectManager::getInstance();
    const core::ProjectElement* element = pm.findElement(elementId);
    if (!element) {
        core::Logger::getInstance().warn("NavigatorCoordinator: No material to open: {}",
                                         elementId.toStdString());
        return;
    }
    // A picture opens on its preview tab, which also says why it shows none; the program has
    // no preview of the other files
    const bool card = core::SettingsManager::getInstance().get<std::string>(
                          NavigatorPanel::OPEN_MATERIALS_SETTING, "card") != "system";
    if (card && MaterialFile::of(pm.filePathOf(*element)).picture) {
        onRequestPreview(elementId);
    } else {
        onRequestOpenWithSystem(elementId);
    }
}

void NavigatorCoordinator::onRequestPreview(const QString& elementId) {
    const auto& pm = core::ProjectManager::getInstance();
    const core::ProjectElement* element = pm.findElement(elementId);
    if (!element) {
        return;
    }
    for (int i = 0; i < m_centralTabs->count(); ++i) {
        if (auto* preview = qobject_cast<ImagePreviewPanel*>(m_centralTabs->widget(i));
            preview && preview->elementId() == elementId) {
            m_centralTabs->setCurrentIndex(i);
            preview->setFocus(Qt::OtherFocusReason);
            return;
        }
    }
    auto* preview = new ImagePreviewPanel(elementId, m_centralTabs);
    connect(preview, &ImagePreviewPanel::openWithSystemRequested, this,
            &NavigatorCoordinator::onRequestOpenWithSystem);
    // The card and the materials of a chapter show the picture turned as the preview does
    connect(preview, &ImagePreviewPanel::rotationChanged, m_propertiesPanel,
            &PropertiesPanel::refresh);
    const int index = m_centralTabs->addTab(
        preview,
        core::ArtProvider::getInstance().getIcon(preview->property("tabIconId").toString()),
        element->title);
    m_centralTabs->setCurrentIndex(index);
    preview->setFocus(Qt::OtherFocusReason);
}

void NavigatorCoordinator::onRequestOpenLink(const QString& link) {
    const auto& pm = core::ProjectManager::getInstance();
    const core::BookProject* project = pm.project();
    if (!project) {
        return;
    }
    if (const std::optional<core::BookPlace> section = project->linkedSection(link)) {
        m_navigatorPanel->revealSection(*section);
        return;
    }
    const core::ProjectElement* element = pm.findElement(link);
    if (!element) {
        return;
    }
    if (pm.isMaterial(*element)) {
        onRequestOpenMaterial(link);
        return;
    }
    // Shown where it is in the book; a text opens in the editor too
    m_navigatorPanel->revealElement(link);
    if (pm.formOf(*element) == core::ElementForm::Text) {
        onElementSelected(link, element->title);
    }
}

void NavigatorCoordinator::onRequestOpenWithSystem(const QString& elementId) {
    const auto& pm = core::ProjectManager::getInstance();
    const core::ProjectElement* element = pm.findElement(elementId);
    if (!element) {
        core::Logger::getInstance().warn("NavigatorCoordinator: No material to open: {}",
                                         elementId.toStdString());
        return;
    }

    const QString path = pm.filePathOf(*element);
    if (!openWithSystem(path)) {
        const QHash<QString, core::KindWords> nouns{{QStringLiteral("kind"), wordsOf(*element)}};
        //: In Polish: Nie udało się otworzyć {kind:genitive}
        const QString heading = core::fillWords(tr("Open {Kind} Failed"), nouns);
        //: %1: the file. In Polish: System nie otworzył pliku:\n%1
        const QString text =
            tr("The system could not open the file:\n%1").arg(QDir::toNativeSeparators(path));
        dialogs::MessageDialog::warning(qobject_cast<QWidget*>(parent()), heading, text);
        return;
    }
    m_statusBar->showMessage(tr("Opened: %1").arg(element->title), 2000);
}

NavigatorCoordinator::NewFile NavigatorCoordinator::onRequestNewFile(const QString& untitled) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();
    auto* window = qobject_cast<QWidget*>(parent());

    if (!pm.isProjectOpen()) {
        logger.warn("NavigatorCoordinator: New file requested but no project open");
        return {};
    }

    // The writer chooses the kind, the title and the place, or an empty text file
    dialogs::NewFileDialog dialog(dialogs::NewFileDialog::openBook(untitled), pm.bookTypes(),
                                  window);
    if (dialog.exec() != QDialog::Accepted) {
        return {};
    }
    const QString title = dialog.title();
    if (dialog.outsideBook()) {
        return {NewFile::Made::OutsideFile, title};
    }

    // An empty text file of the book is no element of its text: it goes to "Other Files"
    if (dialog.emptyFileInBook()) {
        const QString fileId = pm.addBookFile(title);
        if (fileId.isEmpty()) {
            logger.error("NavigatorCoordinator: Failed to add the file '{}'", title.toStdString());
            //: In Polish: Nie udało się dodać pliku
            dialogs::MessageDialog::warning(window, tr("Add File Failed"),
                                            tr("Failed to save changes."));
            return {};
        }
        logger.info("NavigatorCoordinator: New file of the book '{}' (id={})",
                    title.toStdString(), fileId.toStdString());
        refreshNavigator();
        m_navigatorPanel->revealElement(fileId);
        onElementSelected(fileId, title);
        //: An empty text file added to the files of the book outside its text. In Polish:
        //: Dodano plik do grupy „Inne pliki”: %1
        m_statusBar->showMessage(tr("File added to Other Files: %1").arg(title), 2000);
        emit documentModified();
        return {NewFile::Made::BookFile, title};
    }

    const core::KindRef kind = dialog.kind();
    const core::ElementPlace at = dialog.place();
    // The messages name the kind added: "Story added", "Could not add a story"
    const QHash<QString, core::KindWords> nouns{{QStringLiteral("kind"), wordsOf(kind)}};

    // ProjectManager makes the chapter file of the element and saves the project at once
    const QString elementId = pm.addElement(kind, title, at.place, at.groupId, at.index);
    if (elementId.isEmpty()) {
        logger.error("NavigatorCoordinator: Failed to add '{}'", title.toStdString());
        //: In Polish: Nie udało się dodać {kind:genitive}
        dialogs::MessageDialog::warning(window, core::fillWords(tr("Add {Kind} Failed"), nouns),
                                        tr("Failed to save changes."));
        return {};
    }
    logger.info("NavigatorCoordinator: New file '{}' (id={})", title.toStdString(),
                elementId.toStdString());

    // The new element is shown, also inside a collapsed part, and opens in the editor
    refreshNavigator();
    m_navigatorPanel->revealElement(elementId);
    onElementSelected(elementId, title);
    //: In Polish: Dodano {kind:accusative}: %1
    m_statusBar->showMessage(core::fillWords(tr("{Kind} added: %1"), nouns).arg(title), 2000);
    emit documentModified();
    return {NewFile::Made::Element, title};
}

void NavigatorCoordinator::onRequestShowSections(bool shown) {
    const core::ProjectBook* book = core::ProjectManager::getInstance().book();
    if (!book) {
        core::Logger::getInstance().warn(
            "NavigatorCoordinator: Show sections requested but no project open");
        return;
    }

    // The elements stay in their sections; only the Navigator shows them another way
    core::BookSections sections = book->sections();
    sections.shown = shown;
    onRequestSections(sections);
}

void NavigatorCoordinator::onRequestSections(const core::BookSections& sections) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    const core::ProjectBook* book = pm.book();
    if (!book) {
        logger.warn("NavigatorCoordinator: Sections requested but no project open");
        return;
    }
    const core::BookSections previous = book->sections();
    if (sections == previous) {
        return;
    }

    const bool showing = sections.shown && !previous.shown;
    const bool hiding = !sections.shown && previous.shown;
    if (!pm.setSections(sections)) {
        logger.error("NavigatorCoordinator: Failed to save the project after changing sections");
        const QString title = showing  ? tr("Show Sections Failed")
                              : hiding ? tr("Hide Sections Failed")
                                       : tr("Rename Sections Failed");
        dialogs::MessageDialog::warning(qobject_cast<QWidget*>(parent()), title,
                                        tr("Failed to save changes."));
        m_propertiesPanel->refresh();  // its fields show the sections the book still has
        return;
    }
    logger.info("NavigatorCoordinator: Sections {}, names: {}",
                pm.book()->partsLayer ? "shown" : "hidden",
                pm.book()->sectionSet.toStdString());

    refreshNavigator();
    if (showing) {
        // The sections come back as when the book opens: the main one expanded
        QStringList expanded = m_navigatorPanel->expandedItemIds();
        expanded << QStringLiteral("type:section_body:%1")
                        .arg(pm.book()->sectionName(core::BookPlace::Main));
        m_navigatorPanel->setExpandedItemIds(expanded);
    }
    if (hiding && m_propertiesPanel->currentPage() == PropertiesPanel::Page::Section) {
        // A hidden section has no item to show the properties of
        m_propertiesPanel->showProjectProperties();
    } else {
        m_propertiesPanel->refresh();
    }
    m_statusBar->showMessage(showing  ? tr("Sections shown")
                             : hiding ? tr("Sections hidden")
                                      : tr("Sections renamed"),
                             2000);
    emit documentModified();
}

void NavigatorCoordinator::onRequestRenameSection(const QString& sectionType) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    const core::ProjectBook* book = pm.book();
    const SectionItem* section = sectionItem(sectionType);
    if (!book || !section) {
        logger.warn("NavigatorCoordinator: No section to rename: {}", sectionType.toStdString());
        return;
    }

    const QString currentName = book->sectionName(section->place);
    dialogs::RenameElementDialog dialog(currentName, QLatin1String(section->iconId),
                                        qobject_cast<QWidget*>(parent()));
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QString newName = dialog.name();
    if (newName.isEmpty() || newName == currentName) {
        return;  // No change
    }

    // The section gets the writer's own name; the other two keep the names they have
    core::BookSections sections = book->sections();
    sections.set = QString::fromLatin1(core::ProjectBook::CUSTOM_SECTIONS);
    sections.names.clear();
    for (const SectionItem& item : SECTION_ITEMS) {
        sections.names << (item.place == section->place ? newName
                                                         : book->sectionName(item.place));
    }
    if (!pm.setSections(sections)) {
        logger.error("NavigatorCoordinator: Failed to save the project after renaming a section");
        dialogs::MessageDialog::warning(qobject_cast<QWidget*>(parent()), tr("Rename Failed"),
                                        tr("Failed to save changes."));
        return;
    }
    logger.info("NavigatorCoordinator: Renamed section '{}' to '{}'",
                currentName.toStdString(), newName.toStdString());

    refreshNavigator();
    m_propertiesPanel->refresh();
    m_statusBar->showMessage(tr("Renamed to '%1'").arg(newName), 2000);
    emit documentModified();
}

} // namespace gui
} // namespace kalahari
