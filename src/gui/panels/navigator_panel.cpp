/// @file navigator_panel.cpp
/// @brief Navigator panel implementation
///
/// OpenSpec #00033 Phase D: Enhanced with icons, element IDs, and theme refresh.
/// OpenSpec #00033 Phase F: Added "Other Files" section for standalone files.

#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/document.h"
#include "kalahari/core/book.h"
#include "kalahari/core/part.h"
#include "kalahari/core/art_provider.h"
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QTreeWidgetItem>
#include <QFileInfo>

namespace kalahari {
namespace gui {

NavigatorPanel::NavigatorPanel(QWidget* parent)
    : QWidget(parent)
    , m_treeWidget(nullptr)
    , m_otherFilesItem(nullptr)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel constructor called");

    // Create layout
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create tree widget
    m_treeWidget = new QTreeWidget(this);

    // No document loaded yet - tree will be populated via loadDocument()
    m_treeWidget->setHeaderLabel(tr("Project Structure (no document loaded)"));

    // Connect double-click signal - emit elementSelected for leaf elements only
    connect(m_treeWidget, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int column) {
                Q_UNUSED(column);
                auto& logger = core::Logger::getInstance();

                QString elementId = item->data(0, Qt::UserRole).toString();
                QString elementType = item->data(0, Qt::UserRole + 1).toString();
                QString elementTitle = item->text(0);

                logger.info("NavigatorPanel: Item double-clicked: {} (type={}, id={})",
                           elementTitle.toStdString(),
                           elementType.toStdString(),
                           elementId.toStdString());

                // Only emit for leaf elements (chapters, frontmatter items, backmatter items)
                // Skip section headers (section) and part containers (part)
                if (!elementId.isEmpty() &&
                    elementType != "section" &&
                    elementType != "part" &&
                    elementType != "document") {
                    emit elementSelected(elementId, elementTitle);
                }
            });

    // Connect to ArtProvider for theme refresh
    connect(&core::ArtProvider::getInstance(), &core::ArtProvider::resourcesChanged,
            this, &NavigatorPanel::refreshIcons);

    layout->addWidget(m_treeWidget);
    setLayout(layout);

    logger.debug("NavigatorPanel initialized");
}

void NavigatorPanel::clearDocument() {
    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::clearDocument()");

    m_treeWidget->clear();
    m_otherFilesItem = nullptr;
    m_standaloneFiles.clear();
    m_treeWidget->setHeaderLabel(tr("Project Structure (no document loaded)"));
}

void NavigatorPanel::loadDocument(const core::Document& document) {
    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::loadDocument() - Document: {}", document.getTitle());

    auto& artProvider = core::ArtProvider::getInstance();

    // Save standalone files to re-add after clearing
    QStringList standaloneFilePaths = m_standaloneFiles.keys();

    // Clear existing items (this clears tree and standalone file tracking)
    m_treeWidget->clear();
    m_otherFilesItem = nullptr;
    m_standaloneFiles.clear();
    m_treeWidget->setHeaderLabel(tr("Project Structure"));

    // Create root item: Document title
    QTreeWidgetItem* rootItem = new QTreeWidgetItem(m_treeWidget);
    rootItem->setText(0, QString::fromStdString(document.getTitle()));
    rootItem->setData(0, Qt::UserRole, QString());  // No ID for root
    rootItem->setData(0, Qt::UserRole + 1, "document");
    rootItem->setIcon(0, artProvider.getIcon("common.folder", core::IconContext::TreeView));
    rootItem->setExpanded(true);

    // Get book structure
    const auto& book = document.getBook();

    // Add Front Matter section
    const auto& frontMatter = book.getFrontMatter();
    if (!frontMatter.empty()) {
        QTreeWidgetItem* frontMatterItem = new QTreeWidgetItem(rootItem);
        frontMatterItem->setText(0, tr("Front Matter"));
        frontMatterItem->setData(0, Qt::UserRole, QString());  // Section has no ID
        frontMatterItem->setData(0, Qt::UserRole + 1, "section");
        frontMatterItem->setIcon(0, artProvider.getIcon("common.folder", core::IconContext::TreeView));

        for (const auto& element : frontMatter) {
            QTreeWidgetItem* item = new QTreeWidgetItem(frontMatterItem);
            item->setText(0, QString::fromStdString(element->getTitle()));
            item->setData(0, Qt::UserRole, QString::fromStdString(element->getId()));
            item->setData(0, Qt::UserRole + 1, QString::fromStdString(element->getType()));
            item->setIcon(0, artProvider.getIcon("template.chapter", core::IconContext::TreeView));
        }

        frontMatterItem->setExpanded(false);  // Collapsed by default
    }

    // Add Body section (Parts -> Chapters)
    const auto& body = book.getBody();
    if (!body.empty()) {
        QTreeWidgetItem* bodyItem = new QTreeWidgetItem(rootItem);
        bodyItem->setText(0, tr("Body"));
        bodyItem->setData(0, Qt::UserRole, QString());  // Section has no ID
        bodyItem->setData(0, Qt::UserRole + 1, "section");
        bodyItem->setIcon(0, artProvider.getIcon("common.folder", core::IconContext::TreeView));

        for (const auto& part : body) {
            QTreeWidgetItem* partItem = new QTreeWidgetItem(bodyItem);
            partItem->setText(0, QString::fromStdString(part->getTitle()));
            partItem->setData(0, Qt::UserRole, QString::fromStdString(part->getId()));
            partItem->setData(0, Qt::UserRole + 1, "part");
            partItem->setIcon(0, artProvider.getIcon("common.folder", core::IconContext::TreeView));

            const auto& chapters = part->getChapters();
            for (const auto& chapter : chapters) {
                QTreeWidgetItem* chapterItem = new QTreeWidgetItem(partItem);
                chapterItem->setText(0, QString::fromStdString(chapter->getTitle()));
                chapterItem->setData(0, Qt::UserRole, QString::fromStdString(chapter->getId()));
                chapterItem->setData(0, Qt::UserRole + 1, QString::fromStdString(chapter->getType()));
                chapterItem->setIcon(0, artProvider.getIcon("template.chapter", core::IconContext::TreeView));
            }

            partItem->setExpanded(true);  // Expand parts by default
        }

        bodyItem->setExpanded(true);  // Expand body by default
    }

    // Add Back Matter section
    const auto& backMatter = book.getBackMatter();
    if (!backMatter.empty()) {
        QTreeWidgetItem* backMatterItem = new QTreeWidgetItem(rootItem);
        backMatterItem->setText(0, tr("Back Matter"));
        backMatterItem->setData(0, Qt::UserRole, QString());  // Section has no ID
        backMatterItem->setData(0, Qt::UserRole + 1, "section");
        backMatterItem->setIcon(0, artProvider.getIcon("common.folder", core::IconContext::TreeView));

        for (const auto& element : backMatter) {
            QTreeWidgetItem* item = new QTreeWidgetItem(backMatterItem);
            item->setText(0, QString::fromStdString(element->getTitle()));
            item->setData(0, Qt::UserRole, QString::fromStdString(element->getId()));
            item->setData(0, Qt::UserRole + 1, QString::fromStdString(element->getType()));
            item->setIcon(0, artProvider.getIcon("template.chapter", core::IconContext::TreeView));
        }

        backMatterItem->setExpanded(false);  // Collapsed by default
    }

    // Re-add standalone files (they were saved before clearing)
    for (const QString& path : standaloneFilePaths) {
        addStandaloneFile(path);
    }

    logger.debug("NavigatorPanel::loadDocument() complete");
}

void NavigatorPanel::refreshIcons() {
    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::refreshIcons()");

    // Refresh all items in the tree
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        refreshItemIcons(m_treeWidget->topLevelItem(i));
    }
}

void NavigatorPanel::refreshItemIcons(QTreeWidgetItem* item) {
    if (!item) {
        return;
    }

    auto& artProvider = core::ArtProvider::getInstance();

    // Get element type from stored data
    QString elementType = item->data(0, Qt::UserRole + 1).toString();

    // Get appropriate icon ID and set the icon
    QString iconId;
    if (elementType == "standalone_file") {
        // For standalone files, use file path to determine icon
        QString path = item->data(0, Qt::UserRole).toString();
        iconId = getIconIdForFile(path);
    } else {
        iconId = getIconIdForType(elementType);
    }

    if (!iconId.isEmpty()) {
        item->setIcon(0, artProvider.getIcon(iconId, core::IconContext::TreeView));
    }

    // Recursively refresh children
    for (int i = 0; i < item->childCount(); ++i) {
        refreshItemIcons(item->child(i));
    }
}

QString NavigatorPanel::getIconIdForType(const QString& elementType) const {
    // Map element types to icon IDs
    if (elementType == "document" ||
        elementType == "section" ||
        elementType == "part" ||
        elementType == "other_files" ||
        elementType == "root") {
        return "common.folder";
    } else if (elementType == "standalone_file") {
        // Standalone files use getIconIdForFile() based on extension
        // This shouldn't be reached normally, but return generic file icon
        return "common.file";
    } else if (elementType == "chapter" ||
               elementType == "title_page" ||
               elementType == "copyright" ||
               elementType == "dedication" ||
               elementType == "epigraph" ||
               elementType == "foreword" ||
               elementType == "preface" ||
               elementType == "introduction" ||
               elementType == "prologue" ||
               elementType == "epilogue" ||
               elementType == "afterword" ||
               elementType == "acknowledgments" ||
               elementType == "appendix" ||
               elementType == "glossary" ||
               elementType == "bibliography" ||
               elementType == "index" ||
               elementType == "colophon" ||
               elementType == "about_author") {
        return "template.chapter";
    }

    // Default: use chapter icon for unknown types
    return "template.chapter";
}

QString NavigatorPanel::getIconIdForFile(const QString& path) const {
    QFileInfo fileInfo(path);
    QString extension = fileInfo.suffix().toLower();

    // Map extensions to icon IDs
    if (extension == "rtf") {
        return "template.chapter";
    } else if (extension == "kmap") {
        return "book.newMindMap";
    } else if (extension == "ktl") {
        return "book.newTimeline";
    }

    // Default icon for unknown file types
    return "common.file";
}

void NavigatorPanel::ensureOtherFilesSection() {
    if (m_otherFilesItem) {
        return;  // Section already exists
    }

    auto& artProvider = core::ArtProvider::getInstance();

    // Get or create root item
    QTreeWidgetItem* rootItem = nullptr;
    if (m_treeWidget->topLevelItemCount() > 0) {
        rootItem = m_treeWidget->topLevelItem(0);
    } else {
        // No document loaded, create a placeholder root
        rootItem = new QTreeWidgetItem(m_treeWidget);
        rootItem->setText(0, tr("Files"));
        rootItem->setData(0, Qt::UserRole, QString());
        rootItem->setData(0, Qt::UserRole + 1, "root");
        rootItem->setIcon(0, artProvider.getIcon("common.folder", core::IconContext::TreeView));
        rootItem->setExpanded(true);
    }

    // Create "Other Files" section as child of root (always at bottom)
    m_otherFilesItem = new QTreeWidgetItem(rootItem);
    m_otherFilesItem->setText(0, tr("Other Files"));
    m_otherFilesItem->setData(0, Qt::UserRole, QString());
    m_otherFilesItem->setData(0, Qt::UserRole + 1, "other_files");
    m_otherFilesItem->setIcon(0, artProvider.getIcon("common.folder", core::IconContext::TreeView));
    m_otherFilesItem->setExpanded(true);
}

void NavigatorPanel::addStandaloneFile(const QString& path) {
    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::addStandaloneFile() - Path: {}", path.toStdString());

    // Check if already added
    if (m_standaloneFiles.contains(path)) {
        logger.debug("NavigatorPanel: File already in navigator: {}", path.toStdString());
        return;
    }

    auto& artProvider = core::ArtProvider::getInstance();

    // Ensure "Other Files" section exists
    ensureOtherFilesSection();

    // Create item for the file
    QFileInfo fileInfo(path);
    QTreeWidgetItem* fileItem = new QTreeWidgetItem(m_otherFilesItem);
    fileItem->setText(0, fileInfo.fileName());
    fileItem->setData(0, Qt::UserRole, path);  // Store full path as ID
    fileItem->setData(0, Qt::UserRole + 1, "standalone_file");
    fileItem->setToolTip(0, path);

    // Set icon based on file extension
    QString iconId = getIconIdForFile(path);
    fileItem->setIcon(0, artProvider.getIcon(iconId, core::IconContext::TreeView));

    // Store reference
    m_standaloneFiles.insert(path, fileItem);

    logger.info("NavigatorPanel: Added standalone file: {}", path.toStdString());
}

void NavigatorPanel::removeStandaloneFile(const QString& path) {
    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::removeStandaloneFile() - Path: {}", path.toStdString());

    // Find the item
    auto it = m_standaloneFiles.find(path);
    if (it == m_standaloneFiles.end()) {
        logger.debug("NavigatorPanel: File not found in navigator: {}", path.toStdString());
        return;
    }

    // Remove the item
    QTreeWidgetItem* item = it.value();
    m_standaloneFiles.erase(it);
    delete item;

    // Hide "Other Files" section if empty
    if (m_standaloneFiles.isEmpty() && m_otherFilesItem) {
        delete m_otherFilesItem;
        m_otherFilesItem = nullptr;
    }

    logger.info("NavigatorPanel: Removed standalone file: {}", path.toStdString());
}

void NavigatorPanel::clearStandaloneFiles() {
    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::clearStandaloneFiles()");

    // Clear all standalone files
    m_standaloneFiles.clear();

    // Remove "Other Files" section
    if (m_otherFilesItem) {
        delete m_otherFilesItem;
        m_otherFilesItem = nullptr;
    }

    logger.debug("NavigatorPanel: Cleared all standalone files");
}

bool NavigatorPanel::hasStandaloneFiles() const {
    return !m_standaloneFiles.isEmpty();
}

} // namespace gui
} // namespace kalahari
