/// @file navigator_panel.cpp
/// @brief Navigator panel implementation
///
/// OpenSpec #00033 Phase D: Enhanced with icons, element IDs, and theme refresh.
/// OpenSpec #00033 Phase F: Added "Other Files" section for standalone files.
/// OpenSpec #00034 Phase C: Added editor synchronization (highlight current chapter).
/// OpenSpec #00034 Phase E: Section-specific icons for better differentiation.
/// OpenSpec #00034 Phase F: Added expansion state persistence between sessions.

#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/document.h"
#include "kalahari/core/book.h"
#include "kalahari/core/part.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/core/settings_manager.h"
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidgetItem>
#include <QFileInfo>
#include <QLineEdit>
#include <QToolButton>
#include <QComboBox>
#include <QTimer>
#include <QMenu>
#include <QActionGroup>
#include <QPalette>
#include <QBrush>
#include <functional>


namespace {
// Helper: Get display title with status suffix
// Final status = no suffix, others show [STATUS]
QString getDisplayTitle(const kalahari::core::BookElement* element) {
    QString title = QString::fromStdString(element->getTitle());
    auto status = element->getMetadata("status");
    if (status.has_value()) {
        QString statusStr = QString::fromStdString(status.value()).toLower();
        if (statusStr != "final" && !statusStr.isEmpty()) {
            // Capitalize first letter
            statusStr[0] = statusStr[0].toUpper();
            title += QString(" [%1]").arg(statusStr);
        }
    }
    return title;
}
} // anonymous namespace

namespace kalahari {
namespace gui {

NavigatorPanel::NavigatorPanel(QWidget* parent)
    : QWidget(parent)
    , m_treeWidget(nullptr)
    , m_otherFilesItem(nullptr)
    , m_typeFilter(nullptr)
    , m_currentFilterType(FilterType::All)
    , m_searchEdit(nullptr)
    , m_clearButton(nullptr)
    , m_expandAllButton(nullptr)
    , m_collapseAllButton(nullptr)
    , m_filterDebounceTimer(nullptr)
    , m_contextMenuItem(nullptr)
    , m_highlightedItem(nullptr)
    , m_highlightColor()
    , m_currentIconSize(0)
{
    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel constructor called");

    auto& artProvider = core::ArtProvider::getInstance();

    // Create main layout
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    // Create search/filter bar
    QHBoxLayout* searchLayout = new QHBoxLayout();
    searchLayout->setContentsMargins(4, 4, 4, 0);
    searchLayout->setSpacing(2);

    // Create type filter combo box
    m_typeFilter = new QComboBox(this);
    m_typeFilter->addItem(tr("All"), static_cast<int>(FilterType::All));
    m_typeFilter->addItem(tr("Text"), static_cast<int>(FilterType::TextFiles));
    m_typeFilter->addItem(tr("Mind Maps"), static_cast<int>(FilterType::MindMaps));
    m_typeFilter->addItem(tr("Timelines"), static_cast<int>(FilterType::Timelines));
    m_typeFilter->addItem(tr("Other"), static_cast<int>(FilterType::OtherFiles));
    m_typeFilter->setToolTip(tr("Filter by document type"));
    m_typeFilter->setMinimumWidth(80);

    connect(m_typeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NavigatorPanel::onTypeFilterChanged);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Filter tree..."));
    m_searchEdit->setClearButtonEnabled(false);  // We use our own clear button

    m_clearButton = new QToolButton(this);
    m_clearButton->setIcon(artProvider.getIcon("common.cancel", core::IconContext::Menu));
    m_clearButton->setToolTip(tr("Clear filter"));
    m_clearButton->setAutoRaise(true);
    m_clearButton->setVisible(false);  // Hidden until text is entered

    // Create expand/collapse all buttons
    m_expandAllButton = new QToolButton(this);
    m_expandAllButton->setIcon(artProvider.getIcon("common.expandMore", core::IconContext::Menu));
    m_expandAllButton->setToolTip(tr("Expand All"));
    m_expandAllButton->setAutoRaise(true);

    m_collapseAllButton = new QToolButton(this);
    m_collapseAllButton->setIcon(artProvider.getIcon("common.expandLess", core::IconContext::Menu));
    m_collapseAllButton->setToolTip(tr("Collapse All"));
    m_collapseAllButton->setAutoRaise(true);

    searchLayout->addWidget(m_typeFilter);
    searchLayout->addWidget(m_searchEdit);
    searchLayout->addWidget(m_clearButton);
    searchLayout->addWidget(m_expandAllButton);
    searchLayout->addWidget(m_collapseAllButton);
    layout->addLayout(searchLayout);

    // Create debounce timer for filter (300ms delay)
    m_filterDebounceTimer = new QTimer(this);
    m_filterDebounceTimer->setSingleShot(true);
    m_filterDebounceTimer->setInterval(300);

    // Connect filter signals
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        // Show/hide clear button based on text
        m_clearButton->setVisible(!text.isEmpty());
        // Start debounce timer
        m_filterDebounceTimer->start();
    });

    connect(m_filterDebounceTimer, &QTimer::timeout, this, [this]() {
        filterTree(m_searchEdit->text());
    });

    connect(m_clearButton, &QToolButton::clicked, this, &NavigatorPanel::clearFilter);

    // Create tree widget
    m_treeWidget = new QTreeWidget(this);

    // Connect expand/collapse all buttons (after tree widget creation)
    connect(m_expandAllButton, &QToolButton::clicked, m_treeWidget, &QTreeWidget::expandAll);
    connect(m_collapseAllButton, &QToolButton::clicked, m_treeWidget, &QTreeWidget::collapseAll);

    // Initialize icon size from ArtProvider (reads from settings)
    // This must be done BEFORE any icons are added to prevent pixelation
    m_currentIconSize = artProvider.getIconSize(core::IconContext::TreeView);
    m_treeWidget->setIconSize(QSize(m_currentIconSize, m_currentIconSize));
    logger.debug("NavigatorPanel: Initial icon size set to {}px", m_currentIconSize);

    // No document loaded yet - tree will be populated via loadDocument()
    m_treeWidget->setHeaderLabel(tr("Project Structure (no document loaded)"));

    // Enable drag & drop for reordering (OpenSpec #00034 Phase D)
    m_treeWidget->setDragEnabled(true);
    m_treeWidget->setAcceptDrops(true);
    m_treeWidget->setDropIndicatorShown(true);
    m_treeWidget->setDragDropMode(QAbstractItemView::InternalMove);
    m_treeWidget->setDefaultDropAction(Qt::MoveAction);

    // Connect to model's rowsMoved signal to detect drag & drop completion
    connect(m_treeWidget->model(), &QAbstractItemModel::rowsMoved,
            this, [this](const QModelIndex& sourceParent, int sourceStart, int sourceEnd,
                         const QModelIndex& destParent, int destRow) {
        Q_UNUSED(sourceEnd)
        auto& logger = core::Logger::getInstance();
        logger.debug("NavigatorPanel: rowsMoved signal - sourceStart={}, destRow={}",
                     sourceStart, destRow);

        // Get source item (after move, it's at destRow)
        QTreeWidgetItem* parentItem = nullptr;
        if (destParent.isValid()) {
            parentItem = m_treeWidget->itemFromIndex(destParent);
        }

        if (!parentItem) {
            logger.debug("NavigatorPanel: rowsMoved - no valid parent item");
            return;
        }

        // Get the moved item
        QTreeWidgetItem* movedItem = parentItem->child(destRow);
        if (!movedItem) {
            logger.debug("NavigatorPanel: rowsMoved - no valid moved item");
            return;
        }

        QString elementType = movedItem->data(0, Qt::UserRole + 1).toString();
        QString elementId = movedItem->data(0, Qt::UserRole).toString();

        logger.debug("NavigatorPanel: Moved item type='{}', id='{}'",
                     elementType.toStdString(), elementId.toStdString());

        // Validate and emit appropriate signal
        QString parentType = parentItem->data(0, Qt::UserRole + 1).toString();
        QString parentId = parentItem->data(0, Qt::UserRole).toString();

        // Calculate original index (sourceStart is the index before move)
        int fromIndex = sourceStart;
        int toIndex = destRow;

        // Adjust toIndex if moving down (Qt reports destination differently)
        if (sourceParent == destParent && fromIndex < destRow) {
            // Moving down within same parent, Qt already adjusted
        }

        if (elementType == "chapter" && parentType == "part") {
            // Chapter moved within a part
            logger.info("NavigatorPanel: Chapter reordered in part '{}': {} -> {}",
                       parentId.toStdString(), fromIndex, toIndex);
            emit chapterReordered(parentId, fromIndex, toIndex);
        } else if (elementType == "part" && parentType == "section_body") {
            // Part moved within body section
            logger.info("NavigatorPanel: Part reordered: {} -> {}", fromIndex, toIndex);
            emit partReordered(fromIndex, toIndex);
        } else {
            // Invalid move - items should not be movable here
            // In theory Qt shouldn't allow this with proper flags, but log it
            logger.warn("NavigatorPanel: Invalid drag & drop: type='{}' to parent type='{}'",
                       elementType.toStdString(), parentType.toStdString());
        }
    });

    // Enable context menu
    m_treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_treeWidget, &QTreeWidget::customContextMenuRequested,
            this, &NavigatorPanel::showContextMenu);

    // Connect keyboard navigation - update Properties panel when arrow keys change selection
    connect(m_treeWidget, &QTreeWidget::currentItemChanged,
            this, &NavigatorPanel::onCurrentItemChanged);

    // Connect Enter key / double-click activation - open element
    connect(m_treeWidget, &QTreeWidget::itemActivated,
            this, &NavigatorPanel::onItemActivated);

    // Connect single-click signal - emit requestProperties for any element
    connect(m_treeWidget, &QTreeWidget::itemClicked,
            this, [this](QTreeWidgetItem* item, int column) {
                Q_UNUSED(column);
                auto& logger = core::Logger::getInstance();

                QString elementId = item->data(0, Qt::UserRole).toString();
                QString elementType = item->data(0, Qt::UserRole + 1).toString();
                QString elementTitle = item->text(0);

                logger.debug("NavigatorPanel: Item single-clicked: {} (type={}, id={})",
                           elementTitle.toStdString(),
                           elementType.toStdString(),
                           elementId.toStdString());

                // Emit requestProperties for all elements
                // Document root uses empty ID to show project properties
                // Sections and parts emit with their type for aggregate statistics
                if (elementType == "document") {
                    emit requestProperties("");  // Project properties
                } else if (elementType == "section_frontmatter" ||
                           elementType == "section_body" ||
                           elementType == "section_backmatter") {
                    emit requestSectionProperties(elementType);
                } else if (elementType == "part") {
                    emit requestPartProperties(elementId);
                } else if (!elementId.isEmpty()) {
                    emit requestProperties(elementId);
                }
            });

    // NOTE: Double-click is handled by itemActivated signal (onItemActivated slot)
    // which fires on BOTH Enter key AND double-click.
    // DO NOT connect itemDoubleClicked separately - it causes duplicate document opens!

    // Connect to ArtProvider for theme refresh
    connect(&artProvider, &core::ArtProvider::resourcesChanged,
            this, &NavigatorPanel::refreshIcons);

    // Connect to ThemeManager for highlight color updates (OpenSpec #00034 Phase C)
    connect(&core::ThemeManager::getInstance(), &core::ThemeManager::themeChanged,
            this, &NavigatorPanel::updateHighlightColor);

    // Initialize highlight color from current theme
    updateHighlightColor();

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
    rootItem->setIcon(0, artProvider.getIcon("project.book", core::IconContext::TreeView));
    rootItem->setExpanded(true);
    // Document is not draggable or droppable
    rootItem->setFlags(rootItem->flags() & ~Qt::ItemIsDragEnabled & ~Qt::ItemIsDropEnabled);

    // Get book structure
    const auto& book = document.getBook();

    // Add Front Matter section
    const auto& frontMatter = book.getFrontMatter();
    if (!frontMatter.empty()) {
        QTreeWidgetItem* frontMatterItem = new QTreeWidgetItem(rootItem);
        frontMatterItem->setText(0, tr("Front Matter"));
        frontMatterItem->setData(0, Qt::UserRole, QString());  // Section has no ID
        frontMatterItem->setData(0, Qt::UserRole + 1, "section_frontmatter");
        frontMatterItem->setIcon(0, artProvider.getIcon("structure.frontmatter", core::IconContext::TreeView));
        // Section is not draggable, but can receive drops (for future feature)
        frontMatterItem->setFlags(frontMatterItem->flags() & ~Qt::ItemIsDragEnabled & ~Qt::ItemIsDropEnabled);

        for (const auto& element : frontMatter) {
            QTreeWidgetItem* item = new QTreeWidgetItem(frontMatterItem);
            item->setText(0, getDisplayTitle(element.get()));
            item->setData(0, Qt::UserRole, QString::fromStdString(element->getId()));
            item->setData(0, Qt::UserRole + 1, QString::fromStdString(element->getType()));
            item->setIcon(0, artProvider.getIcon("template.chapter", core::IconContext::TreeView));
            // Front matter items are not draggable (no reordering support yet)
            item->setFlags(item->flags() & ~Qt::ItemIsDragEnabled & ~Qt::ItemIsDropEnabled);
        }

        frontMatterItem->setExpanded(false);  // Collapsed by default
    }

    // Add Body section (Parts -> Chapters)
    const auto& body = book.getBody();
    if (!body.empty()) {
        QTreeWidgetItem* bodyItem = new QTreeWidgetItem(rootItem);
        bodyItem->setText(0, tr("Body"));
        bodyItem->setData(0, Qt::UserRole, QString());  // Section has no ID
        bodyItem->setData(0, Qt::UserRole + 1, "section_body");
        bodyItem->setIcon(0, artProvider.getIcon("structure.body", core::IconContext::TreeView));
        // Body section is not draggable, but accepts parts as drops
        bodyItem->setFlags(bodyItem->flags() & ~Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);

        for (const auto& part : body) {
            QTreeWidgetItem* partItem = new QTreeWidgetItem(bodyItem);
            partItem->setText(0, QString::fromStdString(part->getTitle()));
            partItem->setData(0, Qt::UserRole, QString::fromStdString(part->getId()));
            partItem->setData(0, Qt::UserRole + 1, "part");
            partItem->setIcon(0, artProvider.getIcon("structure.part", core::IconContext::TreeView));
            // Parts are draggable within Body section and accept chapter drops
            partItem->setFlags(partItem->flags() | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);

            const auto& chapters = part->getChapters();
            for (const auto& chapter : chapters) {
                QTreeWidgetItem* chapterItem = new QTreeWidgetItem(partItem);
                chapterItem->setText(0, getDisplayTitle(chapter.get()));
                chapterItem->setData(0, Qt::UserRole, QString::fromStdString(chapter->getId()));
                chapterItem->setData(0, Qt::UserRole + 1, QString::fromStdString(chapter->getType()));
                chapterItem->setIcon(0, artProvider.getIcon("template.chapter", core::IconContext::TreeView));
                // Chapters are draggable within their parent part only
                chapterItem->setFlags(chapterItem->flags() | Qt::ItemIsDragEnabled);
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
        backMatterItem->setData(0, Qt::UserRole + 1, "section_backmatter");
        backMatterItem->setIcon(0, artProvider.getIcon("structure.backmatter", core::IconContext::TreeView));
        // Section is not draggable
        backMatterItem->setFlags(backMatterItem->flags() & ~Qt::ItemIsDragEnabled & ~Qt::ItemIsDropEnabled);

        for (const auto& element : backMatter) {
            QTreeWidgetItem* item = new QTreeWidgetItem(backMatterItem);
            item->setText(0, getDisplayTitle(element.get()));
            item->setData(0, Qt::UserRole, QString::fromStdString(element->getId()));
            item->setData(0, Qt::UserRole + 1, QString::fromStdString(element->getType()));
            item->setIcon(0, artProvider.getIcon("template.chapter", core::IconContext::TreeView));
            // Back matter items are not draggable (no reordering support yet)
            item->setFlags(item->flags() & ~Qt::ItemIsDragEnabled & ~Qt::ItemIsDropEnabled);
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

    auto& artProvider = core::ArtProvider::getInstance();

    // Check if icon size changed and update tree widget if needed
    int newIconSize = artProvider.getIconSize(core::IconContext::TreeView);
    if (newIconSize != m_currentIconSize) {
        m_currentIconSize = newIconSize;
        m_treeWidget->setIconSize(QSize(m_currentIconSize, m_currentIconSize));
        logger.info("NavigatorPanel: Icon size updated to {}px", m_currentIconSize);
    }

    // Refresh toolbar button icons
    m_clearButton->setIcon(artProvider.getIcon("common.cancel", core::IconContext::Menu));
    m_expandAllButton->setIcon(artProvider.getIcon("common.expandMore", core::IconContext::Menu));
    m_collapseAllButton->setIcon(artProvider.getIcon("common.expandLess", core::IconContext::Menu));

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
    // Structure icons (OpenSpec #00034)
    if (elementType == "document") {
        return "project.book";
    } else if (elementType == "section_frontmatter") {
        return "structure.frontmatter";
    } else if (elementType == "section_body") {
        return "structure.body";
    } else if (elementType == "section_backmatter") {
        return "structure.backmatter";
    } else if (elementType == "part") {
        return "structure.part";
    } else if (elementType == "other_files") {
        return "structure.otherfiles";
    } else if (elementType == "section" || elementType == "root") {
        // Legacy fallback for generic sections
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
    m_otherFilesItem->setIcon(0, artProvider.getIcon("structure.otherfiles", core::IconContext::TreeView));
    m_otherFilesItem->setExpanded(true);
    // Other Files section is not draggable or droppable
    m_otherFilesItem->setFlags(m_otherFilesItem->flags() & ~Qt::ItemIsDragEnabled & ~Qt::ItemIsDropEnabled);
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
    // Standalone files are not draggable or droppable
    fileItem->setFlags(fileItem->flags() & ~Qt::ItemIsDragEnabled & ~Qt::ItemIsDropEnabled);

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

void NavigatorPanel::filterTree(const QString& text) {
    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::filterTree() - Text: '{}', TypeFilter: {}",
                 text.toStdString(), static_cast<int>(m_currentFilterType));

    if (text.isEmpty() && m_currentFilterType == FilterType::All) {
        // Show all items when both filters are empty/all
        for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
            setItemVisibleRecursive(m_treeWidget->topLevelItem(i), true);
        }
        return;
    }

    // Convert to lowercase for case-insensitive matching
    QString filterText = text.toLower();

    // Process all top-level items
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        processFilterItem(m_treeWidget->topLevelItem(i), filterText);
    }
}

void NavigatorPanel::clearFilter() {
    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::clearFilter()");

    m_searchEdit->clear();
    // The textChanged signal will trigger filterTree with empty text
}

void NavigatorPanel::onTypeFilterChanged(int index) {
    auto& logger = core::Logger::getInstance();

    m_currentFilterType = static_cast<FilterType>(m_typeFilter->itemData(index).toInt());
    logger.debug("NavigatorPanel::onTypeFilterChanged() - Type: {}",
                 static_cast<int>(m_currentFilterType));

    // Re-apply filter with new type
    filterTree(m_searchEdit->text());
}

bool NavigatorPanel::matchesTypeFilter(QTreeWidgetItem* item) const {
    if (m_currentFilterType == FilterType::All) {
        return true;
    }

    QString elementType = item->data(0, Qt::UserRole + 1).toString();
    QString elementId = item->data(0, Qt::UserRole).toString();

    // Section headers always match (to show their children)
    if (elementType == "document" || elementType == "root" ||
        elementType == "section" || elementType == "section_frontmatter" ||
        elementType == "section_body" || elementType == "section_backmatter" ||
        elementType == "part" || elementType == "other_files") {
        return true;
    }

    switch (m_currentFilterType) {
    case FilterType::TextFiles:
        // Text files: chapters and all frontmatter/backmatter item types
        return (elementType == "chapter" ||
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
                elementType == "about_author");

    case FilterType::MindMaps:
        // Mind maps: standalone files with .kmap extension or mindmap type
        if (elementType == "standalone_file") {
            QString path = elementId;  // For standalone files, ID is the path
            return path.toLower().endsWith(".kmap");
        }
        return elementType.toLower().contains("mindmap");

    case FilterType::Timelines:
        // Timelines: standalone files with .ktl extension or timeline type
        if (elementType == "standalone_file") {
            QString path = elementId;  // For standalone files, ID is the path
            return path.toLower().endsWith(".ktl");
        }
        return elementType.toLower().contains("timeline");

    case FilterType::OtherFiles:
        // Other files: only items in "Other Files" section (standalone files)
        return (elementType == "standalone_file");

    case FilterType::All:
    default:
        return true;
    }
}

bool NavigatorPanel::processFilterItem(QTreeWidgetItem* item, const QString& filterText) {
    if (!item) {
        return false;
    }

    QString elementType = item->data(0, Qt::UserRole + 1).toString();

    // Check if this item matches both text filter and type filter
    bool textMatches = filterText.isEmpty() || item->text(0).toLower().contains(filterText);
    bool typeMatches = matchesTypeFilter(item);

    // For container types (sections, parts), we need to check children too
    bool isContainer = (elementType == "document" || elementType == "root" ||
                        elementType == "section" || elementType == "section_frontmatter" ||
                        elementType == "section_body" || elementType == "section_backmatter" ||
                        elementType == "part" || elementType == "other_files");

    // Process all children recursively
    bool hasMatchingChild = false;
    for (int i = 0; i < item->childCount(); ++i) {
        if (processFilterItem(item->child(i), filterText)) {
            hasMatchingChild = true;
        }
    }

    // Determine visibility:
    // - Leaf items: must match both text and type filters
    // - Container items: visible if has matching children OR (matches text AND has content)
    bool shouldBeVisible;
    if (isContainer) {
        // Container is visible if it has matching children
        shouldBeVisible = hasMatchingChild;
    } else {
        // Leaf item must match both filters
        shouldBeVisible = textMatches && typeMatches;
    }

    item->setHidden(!shouldBeVisible);

    // Auto-expand parent items that have matching children
    if (hasMatchingChild && (!filterText.isEmpty() || m_currentFilterType != FilterType::All)) {
        item->setExpanded(true);
    }

    return shouldBeVisible;
}

void NavigatorPanel::setItemVisibleRecursive(QTreeWidgetItem* item, bool visible) {
    if (!item) {
        return;
    }

    item->setHidden(!visible);

    for (int i = 0; i < item->childCount(); ++i) {
        setItemVisibleRecursive(item->child(i), visible);
    }
}

void NavigatorPanel::showContextMenu(const QPoint& pos) {
    auto& logger = core::Logger::getInstance();
    auto& artProvider = core::ArtProvider::getInstance();

    // Get item at position
    QTreeWidgetItem* item = m_treeWidget->itemAt(pos);
    if (!item) {
        logger.debug("NavigatorPanel::showContextMenu() - No item at position");
        return;
    }

    // Store item for context menu action handlers
    m_contextMenuItem = item;

    // Get element type
    QString elementType = item->data(0, Qt::UserRole + 1).toString();
    QString elementId = item->data(0, Qt::UserRole).toString();

    logger.debug("NavigatorPanel::showContextMenu() - Type: {}, ID: {}",
                 elementType.toStdString(), elementId.toStdString());

    // Create context menu
    QMenu menu(this);

    // Build menu based on element type
    if (elementType == "chapter" || elementType == "title_page" ||
        elementType == "copyright" || elementType == "dedication" ||
        elementType == "epigraph" || elementType == "foreword" ||
        elementType == "preface" || elementType == "introduction" ||
        elementType == "prologue" || elementType == "epilogue" ||
        elementType == "afterword" || elementType == "acknowledgments" ||
        elementType == "appendix" || elementType == "glossary" ||
        elementType == "bibliography" || elementType == "index" ||
        elementType == "colophon" || elementType == "about_author") {
        // Leaf elements (chapters, front/back matter items)
        QAction* openAction = menu.addAction(
            artProvider.getIcon("file.open", core::IconContext::Menu),
            tr("Open"));
        connect(openAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuOpen);

        menu.addSeparator();

        QAction* renameAction = menu.addAction(
            artProvider.getIcon("edit.rename", core::IconContext::Menu),
            tr("Rename..."));
        connect(renameAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuRename);

        QAction* deleteAction = menu.addAction(
            artProvider.getIcon("edit.delete", core::IconContext::Menu),
            tr("Delete"));
        connect(deleteAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuDelete);

        menu.addSeparator();

        // Move Up/Down - check if enabled
        QTreeWidgetItem* parent = item->parent();
        int index = parent ? parent->indexOfChild(item) : -1;
        int siblingCount = parent ? parent->childCount() : 0;

        QAction* moveUpAction = menu.addAction(
            artProvider.getIcon("nav.up", core::IconContext::Menu),
            tr("Move Up"));
        connect(moveUpAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuMoveUp);
        moveUpAction->setEnabled(index > 0);

        QAction* moveDownAction = menu.addAction(
            artProvider.getIcon("nav.down", core::IconContext::Menu),
            tr("Move Down"));
        connect(moveDownAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuMoveDown);
        moveDownAction->setEnabled(index >= 0 && index < siblingCount - 1);

        menu.addSeparator();

        // Add "Set Status" submenu for chapter-type elements
        QMenu* statusMenu = menu.addMenu(tr("Set Status"));

        QActionGroup* statusGroup = new QActionGroup(statusMenu);
        statusGroup->setExclusive(true);

        // Get current status
        QString currentStatus = "draft";
        auto& pm = core::ProjectManager::getInstance();
        if (auto* element = pm.findElement(elementId)) {
            auto status = element->getMetadata("status");
            if (status.has_value()) {
                currentStatus = QString::fromStdString(status.value()).toLower();
            }
        }

        // Create radio actions
        QAction* draftAction = statusMenu->addAction(tr("Draft"));
        draftAction->setCheckable(true);
        draftAction->setChecked(currentStatus == "draft");
        draftAction->setData("draft");
        statusGroup->addAction(draftAction);

        QAction* revisionAction = statusMenu->addAction(tr("Revision"));
        revisionAction->setCheckable(true);
        revisionAction->setChecked(currentStatus == "revision");
        revisionAction->setData("revision");
        statusGroup->addAction(revisionAction);

        QAction* finalAction = statusMenu->addAction(tr("Final"));
        finalAction->setCheckable(true);
        finalAction->setChecked(currentStatus == "final");
        finalAction->setData("final");
        statusGroup->addAction(finalAction);

        connect(statusGroup, &QActionGroup::triggered,
                this, &NavigatorPanel::onContextMenuSetStatus);

        menu.addSeparator();

        QAction* propertiesAction = menu.addAction(
            artProvider.getIcon("common.properties", core::IconContext::Menu),
            tr("Properties..."));
        connect(propertiesAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuProperties);

    } else if (elementType == "part") {
        // Part container
        QAction* addChapterAction = menu.addAction(
            artProvider.getIcon("template.chapter", core::IconContext::Menu),
            tr("Add Chapter"));
        connect(addChapterAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuAddChapter);

        menu.addSeparator();

        QAction* renameAction = menu.addAction(
            artProvider.getIcon("edit.rename", core::IconContext::Menu),
            tr("Rename..."));
        connect(renameAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuRename);

        QAction* deleteAction = menu.addAction(
            artProvider.getIcon("edit.delete", core::IconContext::Menu),
            tr("Delete"));
        connect(deleteAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuDelete);

        menu.addSeparator();

        QAction* expandAllAction = menu.addAction(
            artProvider.getIcon("common.expand", core::IconContext::Menu),
            tr("Expand All"));
        connect(expandAllAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuExpandAll);

        QAction* collapseAllAction = menu.addAction(
            artProvider.getIcon("common.collapse", core::IconContext::Menu),
            tr("Collapse All"));
        connect(collapseAllAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuCollapseAll);

    } else if (elementType == "section" || elementType == "section_frontmatter" ||
               elementType == "section_body" || elementType == "section_backmatter") {
        // Section (Front Matter, Body, Back Matter)
        if (elementType == "section_body") {
            // Body section - can add parts
            QAction* addPartAction = menu.addAction(
                artProvider.getIcon("structure.part", core::IconContext::Menu),
                tr("Add Part"));
            connect(addPartAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuAddPart);
        } else if (elementType == "section_frontmatter" || elementType == "section_backmatter") {
            // Front/Back Matter - can add items
            QAction* addItemAction = menu.addAction(
                artProvider.getIcon("template.chapter", core::IconContext::Menu),
                tr("Add Item"));
            connect(addItemAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuAddItem);
        } else {
            // Legacy "section" fallback - use text to determine type
            QString sectionName = item->text(0);
            if (sectionName == tr("Body")) {
                QAction* addPartAction = menu.addAction(
                    artProvider.getIcon("structure.part", core::IconContext::Menu),
                    tr("Add Part"));
                connect(addPartAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuAddPart);
            } else {
                QAction* addItemAction = menu.addAction(
                    artProvider.getIcon("template.chapter", core::IconContext::Menu),
                    tr("Add Item"));
                connect(addItemAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuAddItem);
            }
        }

        menu.addSeparator();

        QAction* expandAllAction = menu.addAction(
            artProvider.getIcon("common.expand", core::IconContext::Menu),
            tr("Expand All"));
        connect(expandAllAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuExpandAll);

        QAction* collapseAllAction = menu.addAction(
            artProvider.getIcon("common.collapse", core::IconContext::Menu),
            tr("Collapse All"));
        connect(collapseAllAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuCollapseAll);

    } else if (elementType == "document") {
        // Document root
        QAction* propertiesAction = menu.addAction(
            artProvider.getIcon("common.properties", core::IconContext::Menu),
            tr("Project Properties..."));
        connect(propertiesAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuProperties);

    } else if (elementType == "standalone_file") {
        // Standalone file
        QAction* openAction = menu.addAction(
            artProvider.getIcon("file.open", core::IconContext::Menu),
            tr("Open"));
        connect(openAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuOpen);

        menu.addSeparator();

        QAction* addToProjectAction = menu.addAction(
            artProvider.getIcon("common.add", core::IconContext::Menu),
            tr("Add to Project"));
        connect(addToProjectAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuAddToProject);

        QAction* removeFromListAction = menu.addAction(
            artProvider.getIcon("edit.delete", core::IconContext::Menu),
            tr("Remove from List"));
        connect(removeFromListAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuRemoveFromList);

    } else if (elementType == "other_files") {
        // Other Files header - only expand/collapse
        QAction* expandAllAction = menu.addAction(
            artProvider.getIcon("common.expand", core::IconContext::Menu),
            tr("Expand All"));
        connect(expandAllAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuExpandAll);

        QAction* collapseAllAction = menu.addAction(
            artProvider.getIcon("common.collapse", core::IconContext::Menu),
            tr("Collapse All"));
        connect(collapseAllAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuCollapseAll);

    } else {
        // Unknown type - no menu
        logger.debug("NavigatorPanel: No context menu for type: {}", elementType.toStdString());
        m_contextMenuItem = nullptr;
        return;
    }

    // Show menu at global position
    menu.exec(m_treeWidget->viewport()->mapToGlobal(pos));

    // Clear stored item after menu closes
    m_contextMenuItem = nullptr;
}

void NavigatorPanel::onContextMenuOpen() {
    if (!m_contextMenuItem) return;

    QString elementId = m_contextMenuItem->data(0, Qt::UserRole).toString();
    QString elementTitle = m_contextMenuItem->text(0);

    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::onContextMenuOpen() - ID: {}", elementId.toStdString());

    emit elementSelected(elementId, elementTitle);
}

void NavigatorPanel::onContextMenuRename() {
    if (!m_contextMenuItem) return;

    QString elementId = m_contextMenuItem->data(0, Qt::UserRole).toString();
    QString currentTitle = m_contextMenuItem->text(0);

    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::onContextMenuRename() - ID: {}", elementId.toStdString());

    emit requestRename(elementId, currentTitle);
}

void NavigatorPanel::onContextMenuDelete() {
    if (!m_contextMenuItem) return;

    QString elementId = m_contextMenuItem->data(0, Qt::UserRole).toString();
    QString elementType = m_contextMenuItem->data(0, Qt::UserRole + 1).toString();

    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::onContextMenuDelete() - ID: {}", elementId.toStdString());

    emit requestDelete(elementId, elementType);
}

void NavigatorPanel::onContextMenuMoveUp() {
    if (!m_contextMenuItem) return;

    QString elementId = m_contextMenuItem->data(0, Qt::UserRole).toString();

    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::onContextMenuMoveUp() - ID: {}", elementId.toStdString());

    emit requestMoveElement(elementId, -1);
}

void NavigatorPanel::onContextMenuMoveDown() {
    if (!m_contextMenuItem) return;

    QString elementId = m_contextMenuItem->data(0, Qt::UserRole).toString();

    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::onContextMenuMoveDown() - ID: {}", elementId.toStdString());

    emit requestMoveElement(elementId, +1);
}

void NavigatorPanel::onContextMenuAddChapter() {
    if (!m_contextMenuItem) return;

    QString partId = m_contextMenuItem->data(0, Qt::UserRole).toString();

    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::onContextMenuAddChapter() - Part ID: {}", partId.toStdString());

    emit requestAddChapter(partId);
}

void NavigatorPanel::onContextMenuAddPart() {
    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::onContextMenuAddPart()");

    emit requestAddPart();
}

void NavigatorPanel::onContextMenuAddItem() {
    if (!m_contextMenuItem) return;

    // Determine section type from element type or text
    QString elementType = m_contextMenuItem->data(0, Qt::UserRole + 1).toString();
    QString sectionType;

    if (elementType == "section_frontmatter") {
        sectionType = "front_matter";
    } else if (elementType == "section_backmatter") {
        sectionType = "back_matter";
    } else {
        // Legacy fallback: determine from text
        QString sectionName = m_contextMenuItem->text(0);
        if (sectionName == tr("Front Matter")) {
            sectionType = "front_matter";
        } else if (sectionName == tr("Back Matter")) {
            sectionType = "back_matter";
        } else {
            sectionType = "unknown";
        }
    }

    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::onContextMenuAddItem() - Section: {}", sectionType.toStdString());

    emit requestAddItem(sectionType);
}

void NavigatorPanel::onContextMenuExpandAll() {
    if (!m_contextMenuItem) return;

    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::onContextMenuExpandAll()");

    // Expand this item and all children recursively
    std::function<void(QTreeWidgetItem*)> expandRecursive = [&expandRecursive](QTreeWidgetItem* item) {
        if (!item) return;
        item->setExpanded(true);
        for (int i = 0; i < item->childCount(); ++i) {
            expandRecursive(item->child(i));
        }
    };

    expandRecursive(m_contextMenuItem);
}

void NavigatorPanel::onContextMenuCollapseAll() {
    if (!m_contextMenuItem) return;

    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::onContextMenuCollapseAll()");

    // Collapse this item and all children recursively
    std::function<void(QTreeWidgetItem*)> collapseRecursive = [&collapseRecursive](QTreeWidgetItem* item) {
        if (!item) return;
        item->setExpanded(false);
        for (int i = 0; i < item->childCount(); ++i) {
            collapseRecursive(item->child(i));
        }
    };

    collapseRecursive(m_contextMenuItem);
}

void NavigatorPanel::onContextMenuProperties() {
    QString elementId;
    if (m_contextMenuItem) {
        elementId = m_contextMenuItem->data(0, Qt::UserRole).toString();
    }

    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::onContextMenuProperties() - ID: {}", elementId.toStdString());

    emit requestProperties(elementId);
}

void NavigatorPanel::onContextMenuAddToProject() {
    if (!m_contextMenuItem) return;

    QString filePath = m_contextMenuItem->data(0, Qt::UserRole).toString();

    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::onContextMenuAddToProject() - Path: {}", filePath.toStdString());

    emit requestAddToProject(filePath);
}

void NavigatorPanel::onContextMenuRemoveFromList() {
    if (!m_contextMenuItem) return;

    QString filePath = m_contextMenuItem->data(0, Qt::UserRole).toString();

    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::onContextMenuRemoveFromList() - Path: {}", filePath.toStdString());

    emit requestRemoveStandaloneFile(filePath);
}

// =============================================================================
// Keyboard Navigation Support
// =============================================================================

void NavigatorPanel::onCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous) {
    Q_UNUSED(previous);

    if (!current) {
        return;
    }

    auto& logger = core::Logger::getInstance();

    QString elementId = current->data(0, Qt::UserRole).toString();
    QString elementType = current->data(0, Qt::UserRole + 1).toString();
    QString elementTitle = current->text(0);

    logger.debug("NavigatorPanel: Current item changed (keyboard nav): {} (type={}, id={})",
                 elementTitle.toStdString(),
                 elementType.toStdString(),
                 elementId.toStdString());

    // Emit requestProperties for all elements (same logic as itemClicked)
    // Document root uses empty ID to show project properties
    // Sections and parts emit with their type for aggregate statistics
    if (elementType == "document") {
        emit requestProperties("");  // Project properties
    } else if (elementType == "section_frontmatter" ||
               elementType == "section_body" ||
               elementType == "section_backmatter") {
        emit requestSectionProperties(elementType);
    } else if (elementType == "part") {
        emit requestPartProperties(elementId);
    } else if (!elementId.isEmpty()) {
        emit requestProperties(elementId);
    }
}

void NavigatorPanel::onItemActivated(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);

    if (!item) {
        return;
    }

    auto& logger = core::Logger::getInstance();

    QString elementId = item->data(0, Qt::UserRole).toString();
    QString elementType = item->data(0, Qt::UserRole + 1).toString();
    QString elementTitle = item->text(0);

    logger.debug("NavigatorPanel: Item activated (Enter/double-click): {} (type={}, id={})",
                 elementTitle.toStdString(),
                 elementType.toStdString(),
                 elementId.toStdString());

    // Only emit for leaf elements (chapters, frontmatter items, backmatter items, standalone files)
    // Skip section headers and part containers - same logic as itemDoubleClicked
    if (!elementId.isEmpty() &&
        elementType != "section" &&
        elementType != "section_frontmatter" &&
        elementType != "section_body" &&
        elementType != "section_backmatter" &&
        elementType != "part" &&
        elementType != "document" &&
        elementType != "other_files") {
        emit elementSelected(elementId, elementTitle);
    }
}

// =============================================================================
// Item Refresh (Status change notification)
// =============================================================================

void NavigatorPanel::refreshItem(const QString& elementId) {
    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::refreshItem() - ID: {}", elementId.toStdString());

    if (elementId.isEmpty()) {
        return;
    }

    // Find item by elementId
    QTreeWidgetItem* item = findItemByElementId(elementId);
    if (!item) {
        logger.debug("NavigatorPanel: Item not found for elementId: {}", elementId.toStdString());
        return;
    }

    // Get element from ProjectManager and update display text
    auto& pm = core::ProjectManager::getInstance();
    core::BookElement* element = pm.findElement(elementId);
    if (!element) {
        logger.warn("NavigatorPanel: Element not found in ProjectManager: {}", elementId.toStdString());
        return;
    }

    // Update display title using the same helper function used in loadDocument()
    item->setText(0, getDisplayTitle(element));

    logger.debug("NavigatorPanel: Refreshed item text to: {}", item->text(0).toStdString());
}

// =============================================================================
// Editor Synchronization (OpenSpec #00034 Phase C)
// =============================================================================

void NavigatorPanel::highlightElement(const QString& elementId) {
    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::highlightElement() - ID: {}", elementId.toStdString());

    // Clear previous highlight
    clearHighlight();

    if (elementId.isEmpty()) {
        return;
    }

    // Find item by elementId
    QTreeWidgetItem* item = findItemByElementId(elementId);
    if (!item) {
        logger.debug("NavigatorPanel: Item not found for elementId: {}", elementId.toStdString());
        return;
    }

    // Store reference
    m_highlightedItem = item;

    // Apply highlight color
    item->setBackground(0, QBrush(m_highlightColor));

    // Expand all parent nodes
    QTreeWidgetItem* parent = item->parent();
    while (parent) {
        parent->setExpanded(true);
        parent = parent->parent();
    }

    // Scroll to make item visible
    m_treeWidget->scrollToItem(item, QAbstractItemView::EnsureVisible);

    logger.debug("NavigatorPanel: Highlighted item: {}", item->text(0).toStdString());
}

void NavigatorPanel::clearHighlight() {
    if (!m_highlightedItem) {
        return;
    }

    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::clearHighlight()");

    // Reset background to default (no brush = transparent)
    m_highlightedItem->setBackground(0, QBrush());

    m_highlightedItem = nullptr;
}

void NavigatorPanel::updateHighlightColor() {
    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::updateHighlightColor()");

    // Get highlight color from current theme palette
    const core::Theme& theme = core::ThemeManager::getInstance().getCurrentTheme();
    QColor highlight = theme.palette.toQPalette().highlight().color();

    // Apply alpha for semi-transparency (100 out of 255)
    highlight.setAlpha(100);
    m_highlightColor = highlight;

    // Re-apply highlight if item is currently highlighted
    if (m_highlightedItem) {
        m_highlightedItem->setBackground(0, QBrush(m_highlightColor));
    }

    logger.debug("NavigatorPanel: Highlight color updated to {}", m_highlightColor.name().toStdString());
}

QTreeWidgetItem* NavigatorPanel::findItemByElementId(const QString& elementId) const {
    if (elementId.isEmpty()) {
        return nullptr;
    }

    // Search through all top-level items
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* found = findItemByElementIdRecursive(m_treeWidget->topLevelItem(i), elementId);
        if (found) {
            return found;
        }
    }

    return nullptr;
}

QTreeWidgetItem* NavigatorPanel::findItemByElementIdRecursive(QTreeWidgetItem* parent,
                                                               const QString& elementId) const {
    if (!parent) {
        return nullptr;
    }

    // Check if this item matches
    QString itemId = parent->data(0, Qt::UserRole).toString();
    if (itemId == elementId) {
        return parent;
    }

    // Search children recursively
    for (int i = 0; i < parent->childCount(); ++i) {
        QTreeWidgetItem* found = findItemByElementIdRecursive(parent->child(i), elementId);
        if (found) {
            return found;
        }
    }

    return nullptr;
}

// =============================================================================
// Expansion State Persistence (OpenSpec #00034 Phase F)
// =============================================================================

void NavigatorPanel::saveExpansionState(const QString& projectId) {
    auto& logger = core::Logger::getInstance();

    if (projectId.isEmpty()) {
        logger.debug("NavigatorPanel::saveExpansionState() - Empty projectId, skipping");
        return;
    }

    logger.debug("NavigatorPanel::saveExpansionState() - Project: {}", projectId.toStdString());

    QStringList expandedIds;
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        collectExpandedIds(m_treeWidget->topLevelItem(i), expandedIds);
    }

    // Store in settings (comma-separated list)
    auto& settings = core::SettingsManager::getInstance();
    std::string key = "navigator.expansion." + projectId.toStdString();
    settings.set(key, expandedIds.join(",").toStdString());

    logger.debug("NavigatorPanel: Saved {} expanded items for project {}",
                 expandedIds.size(), projectId.toStdString());
}

void NavigatorPanel::restoreExpansionState(const QString& projectId) {
    auto& logger = core::Logger::getInstance();

    if (projectId.isEmpty()) {
        logger.debug("NavigatorPanel::restoreExpansionState() - Empty projectId, skipping");
        return;
    }

    logger.debug("NavigatorPanel::restoreExpansionState() - Project: {}", projectId.toStdString());

    auto& settings = core::SettingsManager::getInstance();
    std::string key = "navigator.expansion." + projectId.toStdString();
    std::string value = settings.get<std::string>(key, "");

    if (value.empty()) {
        logger.debug("NavigatorPanel: No saved expansion state for project {}", projectId.toStdString());
        return;
    }

    QStringList ids = QString::fromStdString(value).split(",", Qt::SkipEmptyParts);
    expandItemsById(ids);

    logger.debug("NavigatorPanel: Restored {} expanded items for project {}",
                 ids.size(), projectId.toStdString());
}

void NavigatorPanel::collectExpandedIds(QTreeWidgetItem* item, QStringList& expandedIds) const {
    if (!item) {
        return;
    }

    // Only collect if item is expanded
    if (item->isExpanded()) {
        QString elementId = item->data(0, Qt::UserRole).toString();
        QString elementType = item->data(0, Qt::UserRole + 1).toString();

        if (!elementId.isEmpty()) {
            // Item has an ID, use it directly
            expandedIds.append(elementId);
        } else if (!elementType.isEmpty()) {
            // Section without ID - use type:text format
            // Format: "type:<elementType>:<text>"
            QString identifier = QString("type:%1:%2").arg(elementType, item->text(0));
            expandedIds.append(identifier);
        }
    }

    // Process children recursively
    for (int i = 0; i < item->childCount(); ++i) {
        collectExpandedIds(item->child(i), expandedIds);
    }
}

void NavigatorPanel::expandItemsById(const QStringList& ids) {
    auto& logger = core::Logger::getInstance();

    for (const QString& id : ids) {
        QTreeWidgetItem* item = nullptr;

        if (id.startsWith("type:")) {
            // Format: "type:<elementType>:<text>"
            QStringList parts = id.split(":");
            if (parts.size() >= 3) {
                QString elementType = parts[1];
                // Join remaining parts in case text contained colons
                QString text = parts.mid(2).join(":");
                item = findItemByTypeAndText(elementType, text);
            }
        } else {
            // Regular element ID
            item = findItemByElementId(id);
        }

        if (item) {
            item->setExpanded(true);
        } else {
            logger.debug("NavigatorPanel: Item not found for ID: {}", id.toStdString());
        }
    }
}

QTreeWidgetItem* NavigatorPanel::findItemByTypeAndText(const QString& elementType,
                                                        const QString& text) const {
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        QTreeWidgetItem* found = findItemByTypeAndTextRecursive(
            m_treeWidget->topLevelItem(i), elementType, text);
        if (found) {
            return found;
        }
    }
    return nullptr;
}

QTreeWidgetItem* NavigatorPanel::findItemByTypeAndTextRecursive(QTreeWidgetItem* parent,
                                                                 const QString& elementType,
                                                                 const QString& text) const {
    if (!parent) {
        return nullptr;
    }

    // Check if this item matches
    QString itemType = parent->data(0, Qt::UserRole + 1).toString();
    if (itemType == elementType && parent->text(0) == text) {
        return parent;
    }

    // Search children recursively
    for (int i = 0; i < parent->childCount(); ++i) {
        QTreeWidgetItem* found = findItemByTypeAndTextRecursive(
            parent->child(i), elementType, text);
        if (found) {
            return found;
        }
    }

    return nullptr;
}

void NavigatorPanel::onContextMenuSetStatus(QAction* action) {
    if (!m_contextMenuItem) return;

    QString elementId = m_contextMenuItem->data(0, Qt::UserRole).toString();
    QString newStatus = action->data().toString();

    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::onContextMenuSetStatus() - ID: {}, Status: {}",
                 elementId.toStdString(), newStatus.toStdString());

    auto& pm = core::ProjectManager::getInstance();
    if (auto* element = pm.findElement(elementId)) {
        element->setMetadata("status", newStatus.toStdString());
        pm.saveChapterMetadata(QString::fromStdString(element->getId()));
        refreshItem(elementId);

        // Also emit requestProperties to update Properties panel if visible
        emit requestProperties(elementId);
    }
}

} // namespace gui
} // namespace kalahari
