/// @file navigator_coordinator.cpp
/// @brief Navigator panel interaction coordination implementation
///
/// OpenSpec #00038 - Phase 6: Extract Navigator Handlers from MainWindow

#include "kalahari/gui/navigator_coordinator.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/dialogs/new_element_dialog.h"
#include "kalahari/gui/dialogs/rename_element_dialog.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/gui/section_words.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/logger.h"
#include "kalahari/editor/statistics_collector.h"
#include <QTabWidget>
#include <QStatusBar>
#include <QDir>
#include <chrono>
#include <optional>

namespace kalahari {
namespace gui {

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
{
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
}

void NavigatorCoordinator::discardChapterChanges(const QString& elementId) {
    auto& logger = core::Logger::getInstance();

    // Forget the unsaved text ProjectManager keeps (single source of truth)
    core::ProjectManager::getInstance().discardChapterContent(elementId);

    // Clear the display cache
    m_dirtyChapters[elementId] = false;

    // Clear the navigator "*" indicator
    emit chapterDirtyStateChanged(elementId, false);

    logger.debug("NavigatorCoordinator: Discarded changes for chapter: {}",
                 elementId.toStdString());
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
        m_statusBar->showMessage(tr("No project loaded"), 2000);
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
    const core::ProjectElement* element = pm.findElement(elementId);
    const QString iconId = element ? NavigatorPanel::iconIdOf(pm.bookTypes(), *element)
                                   : QStringLiteral("template.chapter");
    QIcon chapterIcon = core::ArtProvider::getInstance().getIcon(iconId);
    int tabIndex = m_centralTabs->addTab(newEditor, chapterIcon, elementTitle);
    newEditor->setProperty("tabIconId", iconId);
    m_centralTabs->setCurrentIndex(tabIndex);

    // Store element ID for save operations
    newEditor->setProperty("elementId", elementId);
    m_currentElementId = elementId;

    // Set content BEFORE wiring dirty tracking. BookEditor::fromKml() emits
    // contentChanged while loading; if the dirty-tracking slot were connected first,
    // merely opening a chapter would mark it modified (spurious "*" in the tab + a
    // "save changes?" prompt on close/switch with no real edit). The standalone-file
    // path already connects after setContent for exactly this reason.
    logElapsed("Before setContent");
    const bool complete = newEditor->setContent(content);
    logElapsed("After setContent");

    // Connect contentChanged signal for per-chapter dirty tracking (AFTER load, so
    // only genuine user edits mark the chapter dirty).
    connect(newEditor, &EditorPanel::contentChanged,
            this, [this, elementId, elementTitle, newEditor]() {
                auto& pm = core::ProjectManager::getInstance();
                if (pm.isProjectOpen()) {
                    // Mark chapter as dirty
                    if (!m_dirtyChapters.value(elementId, false)) {
                        m_dirtyChapters[elementId] = true;
                        // Chapter CONTENT dirtiness is tracked per open tab here, set
                        // ONLY on genuine edits (this slot is connected AFTER load).
                        // Do NOT mark the project (.klh) dirty: its structure and data
                        // have not changed, and a dirty project resurfaces as a spurious
                        // save prompt.

                        // Update tab title with asterisk
                        int currentIdx = m_centralTabs->indexOf(newEditor);
                        if (currentIdx >= 0) {
                            QString tabText = m_centralTabs->tabText(currentIdx);
                            if (!tabText.startsWith("*")) {
                                m_centralTabs->setTabText(currentIdx, "*" + tabText);
                            }
                        }

                        // Notify NavigatorPanel about dirty state (OpenSpec #00042 Phase 7.5)
                        emit chapterDirtyStateChanged(elementId, true);

                        emit documentModified();
                    }
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
        EditorPanel::warnDamagedChapter(m_centralTabs->window(), elementTitle);
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

    // Refresh navigator and the open tab to show new name
    refreshNavigator();
    refreshTabTitle(elementId);

    m_statusBar->showMessage(tr("Renamed to '%1'").arg(newTitle), 2000);
    emit documentModified();
}

void NavigatorCoordinator::onRequestDelete(const QString& elementId) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) {
        logger.warn("NavigatorCoordinator: Delete requested but no project open");
        return;
    }

    const core::ProjectElement* element = pm.findElement(elementId);
    if (!element) {
        logger.warn("NavigatorCoordinator: Element not found for delete: {}", elementId.toStdString());
        dialogs::MessageDialog::warning(qobject_cast<QWidget*>(parent()), tr("Delete Failed"),
                                        tr("Could not find the element to delete."));
        return;
    }

    // Confirm deletion, saying where the files go: they stay in the book's folder
    QString question = tr("Delete \"%1\" from the book?").arg(element->title);
    if (!element->file.isEmpty()) {
        question += QStringLiteral("\n\n") +
                    tr("Its file stays in the book's folder:\n%1")
                        .arg(QDir::toNativeSeparators(pm.filePathOf(*element)));
    }
    if (!element->elements.isEmpty()) {
        question += QStringLiteral("\n\n") +
                    tr("The elements inside it are deleted from the book too; their files stay "
                       "in the book's folder.");
    }
    if (!dialogs::MessageDialog::confirm(qobject_cast<QWidget*>(parent()), tr("Confirm Delete"),
                                         question, tr("&Delete"),
                                         dialogs::MessageDialog::Kind::Warning,
                                         dialogs::MessageDialog::DefaultButton::Cancel)) {
        return;
    }

    // ProjectManager takes the element, with the elements inside it, out of the project and
    // saves it at once; their files stay in the project's folder
    const std::optional<core::ProjectElement> removed = pm.removeElement(elementId);
    if (!removed) {
        logger.error("NavigatorCoordinator: Failed to save the project after delete");
        dialogs::MessageDialog::warning(qobject_cast<QWidget*>(parent()), tr("Delete Failed"),
                                        tr("Failed to save changes."));
        return;
    }
    logger.info("NavigatorCoordinator: Deleted element: {}", elementId.toStdString());

    closeTabsOf(*removed);
    refreshNavigator();
    m_statusBar->showMessage(tr("Deleted successfully"), 2000);
    emit documentModified();
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


// =============================================================================
// Add Chapter/Part/Item Handlers (OpenSpec #00042 Task 7.19 Issue #1)
// =============================================================================

void NavigatorCoordinator::onRequestAddChapter(const QString& groupId) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) {
        logger.warn("NavigatorCoordinator: Add chapter requested but no project open");
        return;
    }

    // The chapter goes to a part, or to the body of the book
    QString groupTitle;
    if (!groupId.isEmpty()) {
        const core::ProjectElement* group = pm.findElement(groupId);
        if (!group) {
            logger.error("NavigatorCoordinator: Part not found: {}", groupId.toStdString());
            dialogs::MessageDialog::warning(qobject_cast<QWidget*>(parent()),
                                            tr("Add Chapter Failed"), tr("Part not found."));
            return;
        }
        groupTitle = group->title;
    }

    // A chapter can be of any text kind there (a prologue, a chapter...); the type's main one
    // is chosen at the start
    addElement(dialogs::NewElementKind::Chapter,
               pm.textKindsFor(core::BookPlace::Main, groupId),
               pm.chapterKindFor(core::BookPlace::Main, groupId), core::BookPlace::Main, groupId,
               groupTitle);
}

void NavigatorCoordinator::onRequestAddPart() {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) {
        logger.warn("NavigatorCoordinator: Add part requested but no project open");
        return;
    }

    // A part can be of any group kind of the body (a part, a cycle...)
    QList<core::KindRef> kinds = pm.kindsFor(core::BookPlace::Main);
    kinds.removeIf([](const core::KindRef& kind) {
        return kind.kind->form != core::ElementForm::Group;
    });
    addElement(dialogs::NewElementKind::Part, kinds, pm.partKind(), core::BookPlace::Main,
               QString(), QString());
}

void NavigatorCoordinator::onRequestAddItem(const QString& sectionType) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) {
        logger.warn("NavigatorCoordinator: Add item requested but no project open");
        return;
    }

    // An item can be of any text kind of its section (a dedication, a preface...)
    const bool front = sectionType == QLatin1String("front_matter");
    const core::BookPlace place = front ? core::BookPlace::Front : core::BookPlace::Back;
    addElement(front ? dialogs::NewElementKind::FrontMatterItem
                     : dialogs::NewElementKind::BackMatterItem,
               pm.textKindsFor(place), core::KindRef{}, place, QString(), QString());
}

void NavigatorCoordinator::addElement(dialogs::NewElementKind dialogKind,
                                      const QList<core::KindRef>& kinds,
                                      const core::KindRef& current, core::BookPlace place,
                                      const QString& groupId, const QString& groupTitle) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    // The navigator offers the command only when a kind can be added there
    if (kinds.isEmpty()) {
        logger.warn("NavigatorCoordinator: No kind can be added there");
        return;
    }

    // The part of the book the dialog was opened on, or a group in it
    const core::BookProject* project = pm.project();
    const core::ProjectBook* book = pm.book();
    if (!project || !book || (!groupId.isEmpty() && !pm.findElement(groupId))) {
        logger.warn("NavigatorCoordinator: No list to add to");
        return;
    }

    // Each kind with the title its new element starts with ("Chapter 3") and its place (a
    // chapter of the body goes before the epilogue that ends the book)
    QList<dialogs::NewElementChoice> choices;
    int currentIndex = 0;
    for (const core::KindRef& kind : kinds) {
        if (kind.kind == current.kind) {
            currentIndex = static_cast<int>(choices.size());
        }
        choices.append({kind, pm.defaultTitle(kind), pm.newPlaceOf(kind, place, groupId)});
    }

    // The dialog shows where the element goes in its part of the book, and lets the writer
    // choose another place; a book without sections has no rows of its parts
    dialogs::NewElementDialog dialog(dialogKind, choices, currentIndex, groupTitle,
                                     qobject_cast<QWidget*>(parent()));
    dialog.setSection(project->elementsIn(place),
                      book->partsLayer ? book->sectionName(place) : QString(),
                      SectionWords::forPart(book, place), pm.bookTypes(), groupId);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const QString title = dialog.title();
    const core::ElementPlace at = dialog.place();

    // ProjectManager makes the chapter file of a text element and saves the project at once
    const QString elementId = pm.addElement(dialog.kind(), title, at.place, at.groupId, at.index,
                                            dialog.takeInside());
    if (elementId.isEmpty()) {
        logger.error("NavigatorCoordinator: Failed to add '{}'", title.toStdString());
        QString failedTitle = tr("Add Item Failed");
        if (dialogKind == dialogs::NewElementKind::Chapter) {
            failedTitle = tr("Add Chapter Failed");
        } else if (dialogKind == dialogs::NewElementKind::Part) {
            failedTitle = tr("Add Part Failed");
        }
        dialogs::MessageDialog::warning(qobject_cast<QWidget*>(parent()), failedTitle,
                                        tr("Failed to save changes."));
        return;
    }
    logger.info("NavigatorCoordinator: Added '{}' (id={})", title.toStdString(),
                elementId.toStdString());

    // The new element is shown, also inside a collapsed part
    refreshNavigator();
    m_navigatorPanel->revealElement(elementId);
    if (dialogKind == dialogs::NewElementKind::Chapter) {
        m_statusBar->showMessage(tr("Chapter added: %1").arg(title), 2000);
    } else if (dialogKind == dialogs::NewElementKind::Part) {
        m_statusBar->showMessage(tr("Part added: %1").arg(title), 2000);
    } else {
        m_statusBar->showMessage(tr("Item added: %1").arg(title), 2000);
    }
    emit documentModified();
}

} // namespace gui
} // namespace kalahari
