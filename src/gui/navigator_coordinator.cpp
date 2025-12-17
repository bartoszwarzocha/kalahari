/// @file navigator_coordinator.cpp
/// @brief Navigator panel interaction coordination implementation
///
/// OpenSpec #00038 - Phase 6: Extract Navigator Handlers from MainWindow

#include "kalahari/gui/navigator_coordinator.h"
#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/panels/properties_panel.h"
#include "kalahari/gui/panels/editor_panel.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/document.h"
#include "kalahari/core/book.h"
#include "kalahari/core/book_element.h"
#include "kalahari/core/part.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/logger.h"
#include <QTabWidget>
#include <QStatusBar>
#include <QDockWidget>
#include <QTextEdit>
#include <QInputDialog>
#include <QMessageBox>

namespace kalahari {
namespace gui {

NavigatorCoordinator::NavigatorCoordinator(NavigatorPanel* navigatorPanel,
                                             PropertiesPanel* propertiesPanel,
                                             QTabWidget* centralTabs,
                                             QDockWidget* propertiesDock,
                                             QStatusBar* statusBar,
                                             QObject* parent)
    : QObject(parent)
    , m_navigatorPanel(navigatorPanel)
    , m_propertiesPanel(propertiesPanel)
    , m_centralTabs(centralTabs)
    , m_propertiesDock(propertiesDock)
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

EditorPanel* NavigatorCoordinator::getCurrentEditor() const {
    if (!m_centralTabs) {
        return nullptr;
    }
    QWidget* currentWidget = m_centralTabs->currentWidget();
    return qobject_cast<EditorPanel*>(currentWidget);
}

void NavigatorCoordinator::refreshNavigator() {
    auto& pm = core::ProjectManager::getInstance();
    if (pm.getDocument()) {
        m_navigatorPanel->loadDocument(*pm.getDocument());
    }
    emit refreshNavigatorRequested();
}

void NavigatorCoordinator::onElementSelected(const QString& elementId, const QString& elementTitle) {
    auto& logger = core::Logger::getInstance();
    logger.info("Navigator element selected: {} (id={})",
                elementTitle.toStdString(), elementId.toStdString());

    // Check if project is loaded via ProjectManager
    auto& pm = core::ProjectManager::getInstance();
    if (!pm.isProjectOpen()) {
        logger.debug("No project loaded - ignoring Navigator selection");
        m_statusBar->showMessage(tr("No project loaded"), 2000);
        return;
    }

    // Save current chapter if dirty before switching
    if (!m_currentElementId.isEmpty() && m_dirtyChapters.value(m_currentElementId, false)) {
        EditorPanel* currentEditor = getCurrentEditor();
        if (currentEditor) {
            // Update element content cache
            core::BookElement* element = pm.findElement(m_currentElementId);
            if (element) {
                element->setContent(currentEditor->getContent());
                logger.debug("Cached content for element: {}", m_currentElementId.toStdString());
            }
        }
    }

    // Load chapter content from file via ProjectManager
    QString content = pm.loadChapterContent(elementId);

    if (content.isEmpty()) {
        logger.warn("Failed to load content for element: {}", elementId.toStdString());
        // Still create tab but with empty content
    }

    // Create new editor tab with chapter icon
    EditorPanel* newEditor = new EditorPanel(m_centralTabs);
    QIcon chapterIcon = core::ArtProvider::getInstance().getIcon("template.chapter");
    int tabIndex = m_centralTabs->addTab(newEditor, chapterIcon, elementTitle);
    m_centralTabs->setCurrentIndex(tabIndex);

    // Store element ID for save operations
    newEditor->setProperty("elementId", elementId);
    m_currentElementId = elementId;

    // Connect textChanged signal for per-chapter dirty tracking
    connect(newEditor->getTextEdit(), &QTextEdit::textChanged,
            this, [this, elementId, elementTitle, newEditor]() {
                auto& pm = core::ProjectManager::getInstance();
                if (pm.isProjectOpen()) {
                    // Mark chapter as dirty
                    if (!m_dirtyChapters.value(elementId, false)) {
                        m_dirtyChapters[elementId] = true;
                        pm.setDirty(true);

                        // Update tab title with asterisk
                        int currentIdx = m_centralTabs->indexOf(newEditor);
                        if (currentIdx >= 0) {
                            QString tabText = m_centralTabs->tabText(currentIdx);
                            if (!tabText.startsWith("*")) {
                                m_centralTabs->setTabText(currentIdx, "*" + tabText);
                            }
                        }

                        emit documentModified();
                    }
                }
            });

    // Set content using setContent method
    newEditor->setContent(content);

    // Update PropertiesPanel to show chapter properties
    m_propertiesPanel->showChapterProperties(elementId);

    emit elementOpened(elementId);

    logger.info("Opened chapter: {} ({})", elementTitle.toStdString(), elementId.toStdString());
    m_statusBar->showMessage(tr("Opened: %1").arg(elementTitle), 2000);
}

void NavigatorCoordinator::onRequestRename(const QString& elementId, const QString& currentTitle) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) {
        logger.warn("NavigatorCoordinator: Rename requested but no project open");
        return;
    }

    // Show input dialog for new name
    bool ok;
    QString newTitle = QInputDialog::getText(
        qobject_cast<QWidget*>(parent()),
        tr("Rename"),
        tr("New name:"),
        QLineEdit::Normal,
        currentTitle,
        &ok
    );

    if (!ok || newTitle.isEmpty() || newTitle == currentTitle) {
        return;  // Cancelled or no change
    }

    // Find and rename the element
    core::BookElement* element = pm.findElement(elementId);
    if (element) {
        element->setTitle(newTitle.toStdString());
        element->touch();  // Update modified timestamp
        pm.setDirty(true);

        // Save manifest to persist the change
        if (pm.saveManifest()) {
            logger.info("NavigatorCoordinator: Renamed element '{}' to '{}'",
                        elementId.toStdString(), newTitle.toStdString());

            // Refresh navigator to show new name
            refreshNavigator();

            // Update tab title if this element is open
            for (int i = 0; i < m_centralTabs->count(); ++i) {
                QWidget* widget = m_centralTabs->widget(i);
                if (widget->property("elementId").toString() == elementId) {
                    m_centralTabs->setTabText(i, newTitle);
                    break;
                }
            }

            m_statusBar->showMessage(tr("Renamed to '%1'").arg(newTitle), 2000);
            emit documentModified();
        } else {
            logger.error("NavigatorCoordinator: Failed to save manifest after rename");
            QMessageBox::warning(
                qobject_cast<QWidget*>(parent()),
                tr("Rename Failed"),
                tr("Failed to save changes.")
            );
        }
        return;
    }

    // Check if it's a Part
    core::Part* part = pm.findPart(elementId);
    if (part) {
        part->setTitle(newTitle.toStdString());
        pm.setDirty(true);

        if (pm.saveManifest()) {
            logger.info("NavigatorCoordinator: Renamed part '{}' to '{}'",
                        elementId.toStdString(), newTitle.toStdString());

            refreshNavigator();
            m_statusBar->showMessage(tr("Renamed to '%1'").arg(newTitle), 2000);
            emit documentModified();
        } else {
            logger.error("NavigatorCoordinator: Failed to save manifest after part rename");
            QMessageBox::warning(
                qobject_cast<QWidget*>(parent()),
                tr("Rename Failed"),
                tr("Failed to save changes.")
            );
        }
        return;
    }

    logger.warn("NavigatorCoordinator: Element not found for rename: {}", elementId.toStdString());
}

void NavigatorCoordinator::onRequestDelete(const QString& elementId, const QString& elementType) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) {
        logger.warn("NavigatorCoordinator: Delete requested but no project open");
        return;
    }

    core::Document* doc = pm.getDocument();
    if (!doc) {
        logger.error("NavigatorCoordinator: No document available for delete");
        return;
    }

    // Confirm deletion
    QString typeDisplayName = elementType;
    if (elementType == "chapter") typeDisplayName = tr("chapter");
    else if (elementType == "part") typeDisplayName = tr("part");
    else if (elementType == "title_page") typeDisplayName = tr("title page");
    else if (elementType == "dedication") typeDisplayName = tr("dedication");
    else if (elementType == "preface") typeDisplayName = tr("preface");
    else if (elementType == "epilogue") typeDisplayName = tr("epilogue");
    else if (elementType == "glossary") typeDisplayName = tr("glossary");
    else if (elementType == "bibliography") typeDisplayName = tr("bibliography");
    else if (elementType == "about_author") typeDisplayName = tr("about author");

    auto reply = QMessageBox::question(
        qobject_cast<QWidget*>(parent()),
        tr("Confirm Delete"),
        tr("Are you sure you want to delete this %1?\n\nThis action cannot be undone.")
            .arg(typeDisplayName),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply != QMessageBox::Yes) {
        return;
    }

    bool deleted = false;
    core::Book& book = doc->getBook();

    // Close tab if element is open
    for (int i = 0; i < m_centralTabs->count(); ++i) {
        QWidget* widget = m_centralTabs->widget(i);
        if (widget->property("elementId").toString() == elementId) {
            m_centralTabs->removeTab(i);
            widget->deleteLater();
            break;
        }
    }

    // Try to delete from different sections
    if (book.removeFrontMatter(elementId.toStdString())) {
        deleted = true;
        logger.info("NavigatorCoordinator: Deleted front matter element: {}", elementId.toStdString());
    } else if (book.removeBackMatter(elementId.toStdString())) {
        deleted = true;
        logger.info("NavigatorCoordinator: Deleted back matter element: {}", elementId.toStdString());
    } else if (elementType == "part" && book.removePart(elementId.toStdString())) {
        deleted = true;
        logger.info("NavigatorCoordinator: Deleted part: {}", elementId.toStdString());
    } else {
        // Try to find chapter in any part
        for (auto& part : book.getBody()) {
            if (part->removeChapter(elementId.toStdString())) {
                deleted = true;
                logger.info("NavigatorCoordinator: Deleted chapter: {} from part: {}",
                            elementId.toStdString(), part->getId());
                break;
            }
        }
    }

    if (deleted) {
        pm.setDirty(true);
        if (pm.saveManifest()) {
            // Refresh navigator
            refreshNavigator();
            m_statusBar->showMessage(tr("Deleted successfully"), 2000);
            emit documentModified();
        } else {
            logger.error("NavigatorCoordinator: Failed to save manifest after delete");
            QMessageBox::warning(
                qobject_cast<QWidget*>(parent()),
                tr("Delete Error"),
                tr("Element was deleted but failed to save manifest.")
            );
        }
    } else {
        logger.warn("NavigatorCoordinator: Element not found for delete: {}", elementId.toStdString());
        QMessageBox::warning(
            qobject_cast<QWidget*>(parent()),
            tr("Delete Failed"),
            tr("Could not find the element to delete.")
        );
    }
}

void NavigatorCoordinator::onRequestMove(const QString& elementId, int direction) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) {
        logger.warn("NavigatorCoordinator: Move requested but no project open");
        return;
    }

    core::Document* doc = pm.getDocument();
    if (!doc) {
        logger.error("NavigatorCoordinator: No document available for move");
        return;
    }

    core::Book& book = doc->getBook();
    bool moved = false;

    // Try to find and move the element
    // Check if it's a Part
    auto& body = book.getBody();
    for (size_t i = 0; i < body.size(); ++i) {
        if (body[i]->getId() == elementId.toStdString()) {
            // It's a part - move it
            int newIndex = static_cast<int>(i) + direction;
            if (newIndex >= 0 && newIndex < static_cast<int>(body.size())) {
                if (book.movePart(i, static_cast<size_t>(newIndex))) {
                    moved = true;
                    logger.info("NavigatorCoordinator: Moved part from {} to {}", i, newIndex);
                }
            }
            break;
        }

        // Check chapters within this part
        auto& chapters = body[i]->getChapters();
        for (size_t j = 0; j < chapters.size(); ++j) {
            if (chapters[j]->getId() == elementId.toStdString()) {
                int newIndex = static_cast<int>(j) + direction;
                if (newIndex >= 0 && newIndex < static_cast<int>(chapters.size())) {
                    if (body[i]->moveChapter(j, static_cast<size_t>(newIndex))) {
                        moved = true;
                        logger.info("NavigatorCoordinator: Moved chapter from {} to {} in part {}",
                                    j, newIndex, body[i]->getId());
                    }
                }
                break;
            }
        }
        if (moved) break;
    }

    // TODO: Add support for moving front matter and back matter elements

    if (moved) {
        pm.setDirty(true);
        if (pm.saveManifest()) {
            refreshNavigator();
            m_statusBar->showMessage(direction < 0 ? tr("Moved up") : tr("Moved down"), 2000);
            emit documentModified();
        } else {
            logger.error("NavigatorCoordinator: Failed to save manifest after move");
        }
    } else {
        logger.debug("NavigatorCoordinator: Could not move element: {} (at boundary or not found)",
                     elementId.toStdString());
    }
}

void NavigatorCoordinator::onRequestProperties(const QString& elementId) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    if (!pm.isProjectOpen()) {
        logger.warn("NavigatorCoordinator: Properties requested but no project open");
        return;
    }

    // Make sure Properties dock is visible
    if (m_propertiesDock) {
        m_propertiesDock->show();
        m_propertiesDock->raise();
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

    // Make sure Properties dock is visible
    if (m_propertiesDock) {
        m_propertiesDock->show();
        m_propertiesDock->raise();
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

    // Make sure Properties dock is visible
    if (m_propertiesDock) {
        m_propertiesDock->show();
        m_propertiesDock->raise();
    }

    logger.debug("NavigatorCoordinator: Showing part properties: {}", partId.toStdString());
    m_propertiesPanel->showPartProperties(partId);
}

void NavigatorCoordinator::onChapterReordered(const QString& partId, int fromIndex, int toIndex) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    if (pm.reorderChapter(partId, fromIndex, toIndex)) {
        logger.info("NavigatorCoordinator: Chapter reordered successfully");
        emit documentModified();
    } else {
        logger.error("NavigatorCoordinator: Failed to reorder chapter");
        // Refresh navigator to restore correct order
        refreshNavigator();
    }
}

void NavigatorCoordinator::onPartReordered(int fromIndex, int toIndex) {
    auto& logger = core::Logger::getInstance();
    auto& pm = core::ProjectManager::getInstance();

    if (pm.reorderPart(fromIndex, toIndex)) {
        logger.info("NavigatorCoordinator: Part reordered successfully");
        emit documentModified();
    } else {
        logger.error("NavigatorCoordinator: Failed to reorder part");
        // Refresh navigator to restore correct order
        refreshNavigator();
    }
}

} // namespace gui
} // namespace kalahari
