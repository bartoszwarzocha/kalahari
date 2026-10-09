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
#include "kalahari/core/book_project.h"
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
#include <QDropEvent>
#include <QMessageBox>
#include <functional>


namespace {

/// Icon id of ArtProvider for the item (elements; other items take theirs from their type)
constexpr int ICON_ROLE = Qt::UserRole + 2;
/// Kind of an element with the package that defines it, e.g. "kalahari.base:chapter"
constexpr int KIND_ROLE = Qt::UserRole + 3;
/// Title of an element, without the status and modified marks
constexpr int TITLE_ROLE = Qt::UserRole + 4;

/// Whether an item of @p type holds other items: the document, its sections, groups...
bool isContainerType(const QString& type) {
    return type == QLatin1String("document") || type == QLatin1String("root") ||
           type == QLatin1String("section_frontmatter") || type == QLatin1String("section_body") ||
           type == QLatin1String("section_backmatter") || type == QLatin1String("group_element") ||
           type == QLatin1String("other_files");
}

/// Tree of the Navigator. An element is dragged only to another place of its list (a part of
/// the book or a group); the drop does not change the tree, it asks for the move, and the tree
/// shows the new order once it is loaded again.
class NavigatorTree : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;

    /// Called after a drop with the dragged element and its new index in its list
    std::function<void(const QString& elementId, int index)> onMove;

protected:
    void dragMoveEvent(QDragMoveEvent* event) override {
        QTreeWidget::dragMoveEvent(event);
        if (event->isAccepted() && dropIndex(event->position().toPoint()) < 0) {
            event->ignore();
        }
    }

    void dropEvent(QDropEvent* event) override {
        const int index = dropIndex(event->position().toPoint());
        const QTreeWidgetItem* dragged = selectedItems().value(0);
        const QString elementId = dragged ? dragged->data(0, Qt::UserRole).toString() : QString();

        // Ignored, so that the view neither moves nor removes the dragged item
        event->ignore();
        stopAutoScroll();
        setState(NoState);
        viewport()->update();

        if (index >= 0 && !elementId.isEmpty() && onMove) {
            // After the drag has ended: the move loads the tree again
            QTimer::singleShot(0, this, [this, elementId, index]() { onMove(elementId, index); });
        }
    }

private:
    /// New index of the dragged element in its list when it is dropped at @p pos; -1 when it
    /// cannot go there or stays where it is
    int dropIndex(const QPoint& pos) const {
        QTreeWidgetItem* dragged = selectedItems().value(0);
        QTreeWidgetItem* target = itemAt(pos);
        if (!dragged || !target || target == dragged || !dragged->parent() ||
            target->parent() != dragged->parent()) {
            return -1;
        }
        const QTreeWidgetItem* list = dragged->parent();
        switch (dropIndicatorPosition()) {
        case AboveItem:
        case BelowItem:
            return kalahari::gui::NavigatorPanel::dropIndex(list->indexOfChild(dragged),
                                                            list->indexOfChild(target),
                                                            dropIndicatorPosition() == BelowItem);
        default:  // On the item or beside the items: not a place in the list
            return -1;
        }
    }
};

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
    auto* tree = new NavigatorTree(this);
    tree->onMove = [this](const QString& elementId, int index) {
        core::Logger::getInstance().info("NavigatorPanel: Element {} dragged to place {}",
                                         elementId.toStdString(), index);
        emit elementMoved(elementId, index);
    };
    m_treeWidget = tree;

    // Connect expand/collapse all buttons (after tree widget creation)
    connect(m_expandAllButton, &QToolButton::clicked, m_treeWidget, &QTreeWidget::expandAll);
    connect(m_collapseAllButton, &QToolButton::clicked, m_treeWidget, &QTreeWidget::collapseAll);

    // Initialize icon size from ArtProvider (reads from settings)
    // This must be done BEFORE any icons are added to prevent pixelation
    m_currentIconSize = artProvider.getIconSize(core::IconContext::TreeView);
    m_treeWidget->setIconSize(QSize(m_currentIconSize, m_currentIconSize));
    logger.debug("NavigatorPanel: Initial icon size set to {}px", m_currentIconSize);

    // No project loaded yet - tree will be populated via loadProject()
    m_treeWidget->setHeaderLabel(tr("Project Structure (no document loaded)"));

    // Enable drag & drop for reordering (OpenSpec #00034 Phase D)
    m_treeWidget->setDragEnabled(true);
    m_treeWidget->setAcceptDrops(true);
    m_treeWidget->setDropIndicatorShown(true);
    m_treeWidget->setDragDropMode(QAbstractItemView::InternalMove);
    m_treeWidget->setDefaultDropAction(Qt::MoveAction);

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

                // Document root uses empty ID to show project properties
                // Sections and groups emit with their type for aggregate statistics
                if (elementType == "document") {
                    emit requestProperties("");  // Project properties
                } else if (elementType == "section_frontmatter" ||
                           elementType == "section_body" ||
                           elementType == "section_backmatter") {
                    emit requestSectionProperties(elementType);
                } else if (elementType == "group_element") {
                    emit requestPartProperties(elementId);
                } else if (elementType == "text_element") {
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
    m_highlightedItem = nullptr;
    m_standaloneFiles.clear();
    m_treeWidget->setHeaderLabel(tr("Project Structure (no document loaded)"));
}

void NavigatorPanel::loadProject(const core::BookProject& project,
                                 const core::BookTypeRegistry& registry) {
    auto& logger = core::Logger::getInstance();
    const core::ProjectBook* book = project.books.isEmpty() ? nullptr : &project.books.first();
    logger.debug("NavigatorPanel::loadProject() - Book: {}",
                 book ? book->title.toStdString() : std::string());

    auto& artProvider = core::ArtProvider::getInstance();

    // Save standalone files and the highlighted element to restore them after clearing
    QStringList standaloneFilePaths = m_standaloneFiles.keys();
    const QString highlightedId =
        m_highlightedItem ? m_highlightedItem->data(0, Qt::UserRole).toString() : QString();

    // Clear existing items (this clears tree and standalone file tracking)
    m_treeWidget->clear();
    m_otherFilesItem = nullptr;
    m_highlightedItem = nullptr;
    m_standaloneFiles.clear();
    m_treeWidget->setHeaderLabel(tr("Project Structure"));

    // Create root item: the title of the book
    QTreeWidgetItem* rootItem = new QTreeWidgetItem(m_treeWidget);
    rootItem->setText(0, book ? book->title : QString());
    rootItem->setData(0, Qt::UserRole, QString());  // No ID for root
    rootItem->setData(0, Qt::UserRole + 1, "document");
    rootItem->setIcon(0, artProvider.getIcon("project.book", core::IconContext::TreeView));
    rootItem->setExpanded(true);
    // Document is not draggable or droppable
    rootItem->setFlags(rootItem->flags() & ~Qt::ItemIsDragEnabled & ~Qt::ItemIsDropEnabled);

    // The three parts of the book, also when empty, so that elements can be added to them.
    // Elements are dragged only within their section or group, which therefore take drops.
    QTreeWidgetItem* frontMatterItem =
        addSectionItem(rootItem, tr("Front Matter"), QStringLiteral("section_frontmatter"));
    QTreeWidgetItem* bodyItem =
        addSectionItem(rootItem, tr("Body"), QStringLiteral("section_body"));
    QTreeWidgetItem* backMatterItem =
        addSectionItem(rootItem, tr("Back Matter"), QStringLiteral("section_backmatter"));

    if (book) {
        addElementItems(frontMatterItem, book->frontElements, registry);
        addElementItems(bodyItem, book->mainElements, registry);
        addElementItems(backMatterItem, book->backElements, registry);
    }

    frontMatterItem->setExpanded(false);  // Collapsed by default
    bodyItem->setExpanded(true);          // Expand body by default
    backMatterItem->setExpanded(false);   // Collapsed by default

    // Re-add standalone files (they were saved before clearing)
    for (const QString& path : standaloneFilePaths) {
        addStandaloneFile(path);
    }

    // The element open in the editor stays highlighted
    if (QTreeWidgetItem* item = findItemByElementId(highlightedId)) {
        m_highlightedItem = item;
        item->setBackground(0, QBrush(m_highlightColor));
    }

    logger.debug("NavigatorPanel::loadProject() complete");
}

QTreeWidgetItem* NavigatorPanel::addSectionItem(QTreeWidgetItem* parent, const QString& title,
                                                const QString& sectionType) {
    QTreeWidgetItem* item = new QTreeWidgetItem(parent);
    item->setText(0, title);
    item->setData(0, Qt::UserRole, QString());  // Section has no ID
    item->setData(0, Qt::UserRole + 1, sectionType);
    item->setIcon(0, core::ArtProvider::getInstance().getIcon(getIconIdForType(sectionType),
                                                             core::IconContext::TreeView));
    // Sections are not draggable, but take the drops of their elements
    item->setFlags((item->flags() & ~Qt::ItemIsDragEnabled) | Qt::ItemIsDropEnabled);
    return item;
}

int NavigatorPanel::dropIndex(int from, int target, bool below) {
    int to = below ? target + 1 : target;
    if (from < to) {
        --to;  // The places after the element move up when it leaves its place
    }
    return to == from ? -1 : to;
}

QString NavigatorPanel::iconIdOf(const core::BookTypeRegistry& registry,
                                 const core::ProjectElement& element) {
    const core::KindRef kind = core::BookProject::kindOf(registry, element);
    if (kind && !kind.kind->icon.isEmpty()) {
        return kind.kind->icon;
    }
    switch (core::BookProject::formOf(registry, element)) {
    case core::ElementForm::Group:
        return QStringLiteral("structure.part");
    case core::ElementForm::Window:
        return QStringLiteral("common.file");
    case core::ElementForm::Text:
        break;
    }
    return QStringLiteral("template.chapter");
}

void NavigatorPanel::addElementItems(QTreeWidgetItem* parent,
                                     const QList<core::ProjectElement>& elements,
                                     const core::BookTypeRegistry& registry) {
    auto& artProvider = core::ArtProvider::getInstance();

    for (const core::ProjectElement& element : elements) {
        const core::ElementForm form = core::BookProject::formOf(registry, element);
        const QString iconId = iconIdOf(registry, element);

        QString itemType;
        Qt::ItemFlags flags = Qt::ItemIsDragEnabled;
        switch (form) {
        case core::ElementForm::Text:
            itemType = QStringLiteral("text_element");
            break;
        case core::ElementForm::Group:
            itemType = QStringLiteral("group_element");
            flags |= Qt::ItemIsDropEnabled;  // Takes the drops of its elements
            break;
        case core::ElementForm::Window:
            itemType = QStringLiteral("window_element");
            break;
        }

        QTreeWidgetItem* item = new QTreeWidgetItem(parent);
        item->setText(0, getDisplayTitle(element, form == core::ElementForm::Text,
                                         m_modifiedElements.contains(element.id)));
        item->setData(0, Qt::UserRole, element.id);
        item->setData(0, Qt::UserRole + 1, itemType);
        item->setData(0, ICON_ROLE, iconId);
        item->setData(0, KIND_ROLE, element.kind.toString());
        item->setData(0, TITLE_ROLE, element.title);
        item->setIcon(0, artProvider.getIcon(iconId, core::IconContext::TreeView));
        item->setFlags((item->flags() & ~Qt::ItemIsDragEnabled & ~Qt::ItemIsDropEnabled) | flags);

        if (form == core::ElementForm::Group) {
            addElementItems(item, element.elements, registry);
            item->setExpanded(true);  // Expand groups by default
        }
    }
}

QString NavigatorPanel::getDisplayTitle(const core::ProjectElement& element, bool isText,
                                        bool isModified) {
    QString title = element.title;
    // Final status = no suffix, others show [Status] with the names of the "Set Status" menu
    if (isText) {
        const QString status = core::ProjectManager::statusOf(element);
        QString statusName;
        if (status == QLatin1String("draft")) {
            statusName = tr("Draft");
        } else if (status == QLatin1String("revision")) {
            statusName = tr("Revision");
        } else if (status != QLatin1String("final")) {
            // An unknown status is shown as it is, capitalized
            statusName = status;
            statusName[0] = statusName[0].toUpper();
        }
        if (!statusName.isEmpty()) {
            title += QStringLiteral(" [%1]").arg(statusName);
        }
    }
    // The "*" modified indicator is part of the canonical display text so it survives
    // every tree rebuild / refreshItem (instead of being string-spliced on separately,
    // which broke on refresh and leaked into renames).
    if (isModified) {
        title.prepend('*');
    }
    return title;
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

    // Get appropriate icon ID and set the icon: an element keeps the icon of its kind
    QString iconId = item->data(0, ICON_ROLE).toString();
    if (iconId.isEmpty()) {
        if (elementType == "standalone_file") {
            // For standalone files, use file path to determine icon
            QString path = item->data(0, Qt::UserRole).toString();
            iconId = getIconIdForFile(path);
        } else {
            iconId = getIconIdForType(elementType);
        }
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
    } else if (elementType == "group_element") {
        return "structure.part";
    } else if (elementType == "other_files") {
        return "structure.otherfiles";
    } else if (elementType == "root") {
        return "common.folder";
    } else if (elementType == "standalone_file" || elementType == "window_element") {
        // Standalone files use getIconIdForFile() and elements the icon of their kind;
        // this shouldn't be reached normally, but return generic file icon
        return "common.file";
    }

    // Default: use chapter icon for text elements and unknown types
    return "template.chapter";
}

QString NavigatorPanel::getIconIdForFile(const QString& path) const {
    QFileInfo fileInfo(path);
    QString extension = fileInfo.suffix().toLower();

    // Map extensions to icon IDs
    if (extension == "kchapter" || extension == "rtf") {
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
    // Id of the element's kind, without its package: "mindmap"
    const QString kindId = item->data(0, KIND_ROLE).toString().section(':', -1);

    // Section headers and groups always match (to show their children)
    if (isContainerType(elementType)) {
        return true;
    }

    switch (m_currentFilterType) {
    case FilterType::TextFiles:
        // Text files: chapters and all front/back matter items with text
        return elementType == "text_element";

    case FilterType::MindMaps:
        // Mind maps: standalone files with .kmap extension or mind map elements
        if (elementType == "standalone_file") {
            QString path = elementId;  // For standalone files, ID is the path
            return path.toLower().endsWith(".kmap");
        }
        return kindId == "mindmap";

    case FilterType::Timelines:
        // Timelines: standalone files with .ktl extension or timeline elements
        if (elementType == "standalone_file") {
            QString path = elementId;  // For standalone files, ID is the path
            return path.toLower().endsWith(".ktl");
        }
        return kindId == "timeline";

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

    // For container types (sections, groups), we need to check children too
    bool isContainer = isContainerType(elementType);

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

    auto& pm = core::ProjectManager::getInstance();
    const bool projectOpen = pm.isProjectOpen();

    // Move Up/Down within the element's list
    const auto addMoveActions = [&]() {
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
    };

    const auto addRenameDeleteActions = [&]() {
        QAction* renameAction = menu.addAction(
            artProvider.getIcon("edit.rename", core::IconContext::Menu),
            tr("Rename..."));
        connect(renameAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuRename);

        QAction* deleteAction = menu.addAction(
            artProvider.getIcon("edit.delete", core::IconContext::Menu),
            tr("Delete"));
        connect(deleteAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuDelete);
    };

    const auto addExpandCollapseActions = [&]() {
        QAction* expandAllAction = menu.addAction(
            artProvider.getIcon("common.expand", core::IconContext::Menu),
            tr("Expand All"));
        connect(expandAllAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuExpandAll);

        QAction* collapseAllAction = menu.addAction(
            artProvider.getIcon("common.collapse", core::IconContext::Menu),
            tr("Collapse All"));
        connect(collapseAllAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuCollapseAll);
    };

    // Build menu based on element type
    if (elementType == "text_element") {
        // Text elements (chapters, front/back matter items)
        QAction* openAction = menu.addAction(
            artProvider.getIcon("file.open", core::IconContext::Menu),
            tr("Open"));
        connect(openAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuOpen);

        menu.addSeparator();
        addRenameDeleteActions();
        menu.addSeparator();
        addMoveActions();
        menu.addSeparator();

        // Add "Set Status" submenu for text elements
        QMenu* statusMenu = menu.addMenu(tr("Set Status"));

        QActionGroup* statusGroup = new QActionGroup(statusMenu);
        statusGroup->setExclusive(true);

        // Get current status
        QString currentStatus = "draft";
        if (const core::ProjectElement* element = pm.findElement(elementId)) {
            currentStatus = core::ProjectManager::statusOf(*element);
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

    } else if (elementType == "group_element") {
        // Group (part): chapters are added inside it
        QAction* addChapterAction = menu.addAction(
            artProvider.getIcon("template.chapter", core::IconContext::Menu),
            tr("Add Chapter"));
        connect(addChapterAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuAddChapter);
        addChapterAction->setEnabled(
            projectOpen && pm.chapterKindFor(core::BookPlace::Main, elementId));

        menu.addSeparator();
        addRenameDeleteActions();
        menu.addSeparator();
        addMoveActions();
        menu.addSeparator();
        addExpandCollapseActions();

    } else if (elementType == "window_element") {
        // Element that opens in a window of its own; the program has none of them yet
        addRenameDeleteActions();
        menu.addSeparator();
        addMoveActions();

    } else if (elementType == "section_frontmatter" || elementType == "section_body" ||
               elementType == "section_backmatter") {
        // Section (Front Matter, Body, Back Matter)
        if (elementType == "section_body") {
            // Body section - can add parts and chapters
            QAction* addPartAction = menu.addAction(
                artProvider.getIcon("structure.part", core::IconContext::Menu),
                tr("Add Part"));
            connect(addPartAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuAddPart);
            addPartAction->setEnabled(projectOpen && pm.partKind());

            QAction* addChapterAction = menu.addAction(
                artProvider.getIcon("template.chapter", core::IconContext::Menu),
                tr("Add Chapter"));
            connect(addChapterAction, &QAction::triggered, this,
                    &NavigatorPanel::onContextMenuAddChapter);
            addChapterAction->setEnabled(projectOpen &&
                                         pm.chapterKindFor(core::BookPlace::Main));
        } else {
            // Front/Back Matter - can add items
            const core::BookPlace place = elementType == "section_frontmatter"
                                              ? core::BookPlace::Front
                                              : core::BookPlace::Back;
            QAction* addItemAction = menu.addAction(
                artProvider.getIcon("template.chapter", core::IconContext::Menu),
                tr("Add Item"));
            connect(addItemAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuAddItem);
            addItemAction->setEnabled(projectOpen && !pm.textKindsFor(place).isEmpty());
        }

        menu.addSeparator();
        addExpandCollapseActions();

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
        addExpandCollapseActions();

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
    QString elementTitle = m_contextMenuItem->data(0, TITLE_ROLE).toString();

    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::onContextMenuOpen() - ID: {}", elementId.toStdString());

    if (m_contextMenuItem->data(0, Qt::UserRole + 1).toString() == "standalone_file") {
        emit standaloneFileSelected(elementId);  // its ID is the path
        return;
    }
    emit elementSelected(elementId, elementTitle);
}

void NavigatorPanel::onContextMenuRename() {
    if (!m_contextMenuItem) return;

    QString elementId = m_contextMenuItem->data(0, Qt::UserRole).toString();
    QString currentTitle = m_contextMenuItem->data(0, TITLE_ROLE).toString();

    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::onContextMenuRename() - ID: {}", elementId.toStdString());

    emit requestRename(elementId, currentTitle);
}

void NavigatorPanel::onContextMenuDelete() {
    if (!m_contextMenuItem) return;

    QString elementId = m_contextMenuItem->data(0, Qt::UserRole).toString();

    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::onContextMenuDelete() - ID: {}", elementId.toStdString());

    emit requestDelete(elementId);
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

    // A group, or the body of the book (no ID)
    QString groupId = m_contextMenuItem->data(0, Qt::UserRole).toString();

    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::onContextMenuAddChapter() - Group ID: {}", groupId.toStdString());

    emit requestAddChapter(groupId);
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
        return;
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

    // Same logic as itemClicked
    // Document root uses empty ID to show project properties
    // Sections and groups emit with their type for aggregate statistics
    if (elementType == "document") {
        emit requestProperties("");  // Project properties
    } else if (elementType == "section_frontmatter" ||
               elementType == "section_body" ||
               elementType == "section_backmatter") {
        emit requestSectionProperties(elementType);
    } else if (elementType == "group_element") {
        emit requestPartProperties(elementId);
    } else if (elementType == "text_element") {
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

    if (elementType == "standalone_file") {
        emit standaloneFileSelected(elementId);  // its ID is the path
        return;
    }

    // Only text elements open in the editor; sections, groups and window elements do not
    if (elementType == "text_element") {
        emit elementSelected(elementId, item->data(0, TITLE_ROLE).toString());
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
    const core::ProjectElement* element = pm.findElement(elementId);
    if (!element) {
        logger.warn("NavigatorPanel: Element not found in ProjectManager: {}", elementId.toStdString());
        return;
    }

    // Update display title using the same helper function used in loadProject().
    // Pass the modified state so the "*" indicator is re-applied on every refresh.
    item->setText(0, getDisplayTitle(*element, pm.formOf(*element) == core::ElementForm::Text,
                                     m_modifiedElements.contains(elementId)));
    item->setData(0, TITLE_ROLE, element->title);

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

    QStringList expandedIds = expandedItemIds();

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
    if (!settings.hasKey(key)) {
        logger.debug("NavigatorPanel: No saved expansion state for project {}", projectId.toStdString());
        return;
    }

    // An empty value is a valid state: everything collapsed
    std::string value = settings.get<std::string>(key, "");
    QStringList ids = QString::fromStdString(value).split(",", Qt::SkipEmptyParts);
    setExpandedItemIds(ids);

    logger.debug("NavigatorPanel: Restored {} expanded items for project {}",
                 ids.size(), projectId.toStdString());
}

QStringList NavigatorPanel::expandedItemIds() const {
    QStringList expandedIds;
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        collectExpandedIds(m_treeWidget->topLevelItem(i), expandedIds);
    }
    return expandedIds;
}

void NavigatorPanel::setExpandedItemIds(const QStringList& ids) {
    // Items expanded by default (document, body, parts) must close too,
    // otherwise a collapsed part would reopen on every load
    m_treeWidget->collapseAll();
    expandItemsById(ids);
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

    // ProjectManager saves the project at once
    auto& pm = core::ProjectManager::getInstance();
    if (!pm.setStatus(elementId, newStatus)) {
        QMessageBox::warning(this, tr("Status Change Failed"), tr("Failed to save changes."));
        return;
    }
    refreshItem(elementId);

    // Also emit requestProperties to update Properties panel if visible
    emit requestProperties(elementId);
}

// =============================================================================
// Modified State Tracking (OpenSpec #00042 Phase 7.5)
// =============================================================================

void NavigatorPanel::setElementModified(const QString& elementId, bool isModified) {
    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::setElementModified() - ID: {}, modified: {}",
                 elementId.toStdString(), isModified);

    if (elementId.isEmpty()) {
        return;
    }

    // Track modified state, then rebuild the item's text through refreshItem() so the
    // "*" indicator is produced by getDisplayTitle() (the single canonical text source).
    // This keeps the indicator correct across tree rebuilds, renames and status changes.
    bool wasModified = m_modifiedElements.contains(elementId);
    if (isModified) {
        m_modifiedElements.insert(elementId);
    } else {
        m_modifiedElements.remove(elementId);
    }

    if (wasModified != isModified) {
        refreshItem(elementId);
    }
}

void NavigatorPanel::clearAllModifiedIndicators() {
    auto& logger = core::Logger::getInstance();
    logger.debug("NavigatorPanel::clearAllModifiedIndicators()");

    // Snapshot the ids, clear the set, then rebuild each item's text via refreshItem()
    // so the "*" indicator is dropped through the single canonical text source.
    const QSet<QString> ids = m_modifiedElements;
    m_modifiedElements.clear();
    for (const QString& elementId : ids) {
        refreshItem(elementId);
    }

    logger.debug("NavigatorPanel: Cleared all modified indicators");
}

} // namespace gui
} // namespace kalahari
