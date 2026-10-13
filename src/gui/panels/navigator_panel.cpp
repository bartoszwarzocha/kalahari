/// @file navigator_panel.cpp
/// @brief Navigator panel implementation
///
/// OpenSpec #00033 Phase D: Enhanced with icons, element IDs, and theme refresh.
/// OpenSpec #00033 Phase F: Added "Other Files" section for standalone files.
/// OpenSpec #00034 Phase C: Added editor synchronization (highlight current chapter).
/// OpenSpec #00034 Phase E: Section-specific icons for better differentiation.
/// OpenSpec #00034 Phase F: Added expansion state persistence between sessions.

#include "kalahari/gui/panels/navigator_panel.h"
#include "kalahari/gui/dialogs/message_dialog.h"
#include "kalahari/gui/kind_names.h"
#include "kalahari/gui/utils/access_keys.h"
#include "kalahari/gui/utils/file_location.h"
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
#include <QDir>
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
#include <QBasicTimer>
#include <QTimerEvent>
#include <algorithm>
#include <functional>


namespace {

/// Icon id of ArtProvider for the item (elements; other items take theirs from their type)
constexpr int ICON_ROLE = Qt::UserRole + 2;
/// Kind of an element with the package that defines it, e.g. "kalahari.base:chapter"
constexpr int KIND_ROLE = Qt::UserRole + 3;
/// Title of an element, without the status and modified marks
constexpr int TITLE_ROLE = Qt::UserRole + 4;
/// Milliseconds before a collapsed group or section that a dragged element rests on opens
constexpr int AUTO_EXPAND_DELAY = 700;
/// Icon of a file of the book outside its text, as New File shows an empty text file
const QString BOOK_FILE_ICON = QStringLiteral("common.file");
/// Icon of a new file outside the book that has no file yet, as its tab shows it
const QString NEW_FILE_ICON = QStringLiteral("template.chapter");

/// Whether an item of @p type holds other items: the document, its sections, groups...
bool isContainerType(const QString& type) {
    return type == QLatin1String("document") || type == QLatin1String("root") ||
           type == QLatin1String("section_frontmatter") || type == QLatin1String("section_body") ||
           type == QLatin1String("section_backmatter") || type == QLatin1String("group_element") ||
           type == QLatin1String("book_files") || type == QLatin1String("unassigned_files") ||
           type == QLatin1String("other_files");
}

/// Whether an item of @p type is "Unassigned Files" or one of its files, which are grey
bool isUnassignedType(const QString& type) {
    return type == QLatin1String("unassigned_files") || type == QLatin1String("unassigned_file");
}

/// Type of @p item (Qt::UserRole + 1): "text_element", "section_body"...
QString typeOf(const QTreeWidgetItem* item) {
    return item->data(0, Qt::UserRole + 1).toString();
}

/// Whether an item of @p type is an element of the book
bool isElementType(const QString& type) {
    return type == QLatin1String("text_element") || type == QLatin1String("group_element") ||
           type == QLatin1String("window_element");
}

/// The part of the book of a section item of @p type; none for another item
std::optional<kalahari::core::BookPlace> sectionPlaceOf(const QString& type) {
    if (type == QLatin1String("section_frontmatter")) {
        return kalahari::core::BookPlace::Front;
    }
    if (type == QLatin1String("section_body")) {
        return kalahari::core::BookPlace::Main;
    }
    if (type == QLatin1String("section_backmatter")) {
        return kalahari::core::BookPlace::Back;
    }
    return std::nullopt;
}

/// The parts of a book, in reading order
const QList<kalahari::core::BookPlace>& bookParts() {
    static const QList<kalahari::core::BookPlace> parts{kalahari::core::BookPlace::Front,
                                                        kalahari::core::BookPlace::Main,
                                                        kalahari::core::BookPlace::Back};
    return parts;
}

/// Whether @p item shows items under it: it is expanded, and not all of them are hidden by
/// the filter
bool showsChildren(const QTreeWidgetItem* item) {
    if (!item->isExpanded()) {
        return false;
    }
    for (int i = 0; i < item->childCount(); ++i) {
        if (!item->child(i)->isHidden()) {
            return true;
        }
    }
    return false;
}

/// Icon @p iconId as a disabled item shows it: grey, for the files that are not in the book
QIcon greyIcon(const QString& iconId, int size) {
    const QIcon icon =
        kalahari::core::ArtProvider::getInstance().getIcon(iconId,
                                                           kalahari::core::IconContext::TreeView);
    QIcon grey;
    grey.addPixmap(icon.pixmap(QSize(size, size), QIcon::Disabled));
    return grey;
}

/// Tree of the Navigator. An element is dragged to any place of the book its kind can have
/// (NavigatorPanel::dropPlaceOf()); the drop does not change the tree, it asks for the move,
/// and the tree shows the book as it is once it is loaded again. In a book without sections
/// the elements of the three sections are all under the book's item, each section after the
/// one before it.
class NavigatorTree : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;

    /// Called after a drop with the dragged element and its new place
    std::function<void(const QString& elementId, const kalahari::core::ElementPlace& place)>
        onMove;

protected:
    void dragMoveEvent(QDragMoveEvent* event) override {
        // The item under the dragged element opens when it rests there (timerEvent())
        m_openPos = event->position().toPoint();
        m_openTimer.start(AUTO_EXPAND_DELAY, this);

        QTreeWidget::dragMoveEvent(event);
        if (event->isAccepted() && !dropPlace(event->position().toPoint())) {
            event->ignore();
        }
    }

    void dragLeaveEvent(QDragLeaveEvent* event) override {
        m_openTimer.stop();
        QTreeWidget::dragLeaveEvent(event);
    }

    /// A collapsed group or section that the dragged element rests on opens, so that the
    /// element can go inside it. An open one stays open: the view's own autoExpandDelay
    /// would close it.
    void timerEvent(QTimerEvent* event) override {
        if (event->timerId() != m_openTimer.timerId()) {
            QTreeWidget::timerEvent(event);
            return;
        }
        m_openTimer.stop();
        QTreeWidgetItem* item = itemAt(m_openPos);
        if (state() == DraggingState && item && item->childCount() > 0) {
            item->setExpanded(true);
        }
    }

    void dropEvent(QDropEvent* event) override {
        m_openTimer.stop();
        const std::optional<kalahari::core::ElementPlace> place =
            dropPlace(event->position().toPoint());
        const QTreeWidgetItem* dragged = selectedItems().value(0);
        const QString elementId = dragged ? dragged->data(0, Qt::UserRole).toString() : QString();

        // Ignored, so that the view neither moves nor removes the dragged item
        event->ignore();
        stopAutoScroll();
        setState(NoState);
        viewport()->update();

        if (place && !elementId.isEmpty() && onMove) {
            // After the drag has ended: the move loads the tree again
            QTimer::singleShot(0, this,
                               [this, elementId, to = *place]() { onMove(elementId, to); });
        }
    }

private:
    /// The new place of the dragged element when it is dropped at @p pos; none when it cannot
    /// go there or stays where it is
    std::optional<kalahari::core::ElementPlace> dropPlace(const QPoint& pos) const {
        using Spot = kalahari::gui::NavigatorPanel::DropSpot;
        Spot spot = Spot::On;
        switch (dropIndicatorPosition()) {
        case AboveItem:
            spot = Spot::Above;
            break;
        case BelowItem:
            spot = Spot::Below;
            break;
        case OnItem:
            break;
        case OnViewport:  // Beside the items
            return std::nullopt;
        }
        const auto& pm = kalahari::core::ProjectManager::getInstance();
        const kalahari::core::BookProject* project = pm.project();
        if (!project) {
            return std::nullopt;
        }
        return kalahari::gui::NavigatorPanel::dropPlaceOf(*project, pm.bookTypes(),
                                                          selectedItems().value(0), itemAt(pos),
                                                          spot);
    }

    QBasicTimer m_openTimer;  ///< Opens the item under the dragged element
    QPoint m_openPos;         ///< Where the dragged element is in the viewport
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
    tree->onMove = [this](const QString& elementId, const core::ElementPlace& place) {
        core::Logger::getInstance().info("NavigatorPanel: Element {} dragged to place {} of {}",
                                         elementId.toStdString(), place.index,
                                         place.groupId.isEmpty()
                                             ? core::bookPlaceName(place.place).toStdString()
                                             : place.groupId.toStdString());
        emit elementMovedTo(elementId, place);
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

    // No book is open yet - loadProject() fills the tree
    //: The header of the Navigator while no book is open. In Polish: Żadna książka nie jest
    //: otwarta
    m_treeWidget->setHeaderLabel(tr("No book is open"));

    // Enable drag & drop for reordering (OpenSpec #00034 Phase D); a collapsed group or
    // section that the dragged element rests on opens (NavigatorTree), so that it can go inside
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
                } else if (elementType == "text_element" || elementType == "book_file") {
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
    m_unassignedItem = nullptr;
    m_unassignedCount = 0;
    m_unassignedExpanded = true;
    m_projectFolder.clear();
    m_standaloneFiles.clear();
    //: The header of the Navigator while no book is open. In Polish: Żadna książka nie jest
    //: otwarta
    m_treeWidget->setHeaderLabel(tr("No book is open"));
}

void NavigatorPanel::loadProject(const core::BookProject& project,
                                 const core::BookTypeRegistry& registry) {
    auto& logger = core::Logger::getInstance();
    const core::ProjectBook* book = project.books.isEmpty() ? nullptr : &project.books.first();
    logger.debug("NavigatorPanel::loadProject() - Book: {}",
                 book ? book->title.toStdString() : std::string());

    auto& artProvider = core::ArtProvider::getInstance();

    // Save standalone files and the highlighted element to restore them after clearing, and
    // whether "Unassigned Files" was open, which setUnassignedFiles() gives back
    QStringList standaloneFilePaths = m_standaloneFiles.keys();
    const QString highlightedId =
        m_highlightedItem ? m_highlightedItem->data(0, Qt::UserRole).toString() : QString();
    if (m_unassignedItem) {
        m_unassignedExpanded = m_unassignedItem->isExpanded();
    }

    // Clear existing items (this clears tree and standalone file tracking)
    m_treeWidget->clear();
    m_otherFilesItem = nullptr;
    m_highlightedItem = nullptr;
    m_unassignedItem = nullptr;
    m_standaloneFiles.clear();
    //: The header of the Navigator with the open book. In Polish: Struktura książki
    m_treeWidget->setHeaderLabel(tr("Book Structure"));

    // Create root item: the title of the book
    QTreeWidgetItem* rootItem = new QTreeWidgetItem(m_treeWidget);
    rootItem->setText(0, book ? book->title : QString());
    rootItem->setData(0, Qt::UserRole, QString());  // No ID for root
    rootItem->setData(0, Qt::UserRole + 1, "document");
    rootItem->setIcon(0, artProvider.getIcon("project.book", core::IconContext::TreeView));
    rootItem->setExpanded(true);
    // Document is not draggable or droppable
    rootItem->setFlags(rootItem->flags() & ~Qt::ItemIsDragEnabled & ~Qt::ItemIsDropEnabled);

    if (book && !book->partsLayer) {
        // A book without sections: the elements of its three sections in reading order. The
        // book's item takes the drops of the elements.
        rootItem->setFlags(rootItem->flags() | Qt::ItemIsDropEnabled);
        addElementItems(rootItem, book->frontElements, registry);
        addElementItems(rootItem, book->mainElements, registry);
        addElementItems(rootItem, book->backElements, registry);
    } else {
        // The three sections of the book with its names, also when empty, so that elements can
        // be added to them. Elements are dragged to the sections and the groups, which
        // therefore take drops.
        const core::ProjectBook noBook;
        const core::ProjectBook& names = book ? *book : noBook;
        QTreeWidgetItem* frontMatterItem =
            addSectionItem(rootItem, names.sectionName(core::BookPlace::Front),
                           QStringLiteral("section_frontmatter"));
        QTreeWidgetItem* bodyItem = addSectionItem(
            rootItem, names.sectionName(core::BookPlace::Main), QStringLiteral("section_body"));
        QTreeWidgetItem* backMatterItem =
            addSectionItem(rootItem, names.sectionName(core::BookPlace::Back),
                           QStringLiteral("section_backmatter"));

        if (book) {
            addElementItems(frontMatterItem, book->frontElements, registry);
            addElementItems(bodyItem, book->mainElements, registry);
            addElementItems(backMatterItem, book->backElements, registry);
        }

        frontMatterItem->setExpanded(false);  // Collapsed by default
        bodyItem->setExpanded(true);          // Expand body by default
        backMatterItem->setExpanded(false);   // Collapsed by default
    }

    // The files of the book outside its text, after its text
    if (book) {
        addBookFileItems(rootItem, book->files);
    }

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

void NavigatorPanel::setUnassignedFiles(const QList<core::UnassignedFile>& files,
                                        const QString& projectFolder) {
    // Open when it gets more files: those of the elements just deleted from the book
    const bool more = files.size() > m_unassignedCount;
    const bool expanded =
        more || (m_unassignedItem ? m_unassignedItem->isExpanded() : m_unassignedExpanded);
    delete m_unassignedItem;
    m_unassignedItem = nullptr;
    m_unassignedCount = static_cast<int>(files.size());
    m_projectFolder = projectFolder;

    QTreeWidgetItem* rootItem =
        m_treeWidget->topLevelItemCount() > 0 ? m_treeWidget->topLevelItem(0) : nullptr;
    if (files.isEmpty() || !rootItem ||
        rootItem->data(0, Qt::UserRole + 1).toString() != QLatin1String("document")) {
        m_unassignedExpanded = true;  // The next files show at once
        return;
    }

    // After the book, before "Other Files"; not dragged, and nothing is dropped on it
    m_unassignedItem = new QTreeWidgetItem();
    const int otherFiles = m_otherFilesItem ? rootItem->indexOfChild(m_otherFilesItem) : -1;
    if (otherFiles >= 0) {
        rootItem->insertChild(otherFiles, m_unassignedItem);
    } else {
        rootItem->addChild(m_unassignedItem);
    }
    //: The item of the Navigator with the chapter files that no element of the book has; %1:
    //: their number. In Polish: Pliki bez przydziału (%1)
    m_unassignedItem->setText(0, tr("Unassigned Files (%1)").arg(files.size()));
    m_unassignedItem->setData(0, Qt::UserRole, QString());
    m_unassignedItem->setData(0, Qt::UserRole + 1, QStringLiteral("unassigned_files"));
    m_unassignedItem->setToolTip(
        0, tr("Chapter files in the book's folder that no element of the book has"));
    m_unassignedItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    const QDir project(projectFolder);
    for (const core::UnassignedFile& file : files) {
        auto* item = new QTreeWidgetItem(m_unassignedItem);
        const QString name = QFileInfo(file.file).fileName();
        //: A file of "Unassigned Files": %1 the title of its text, %2 the name of the file
        item->setText(0, file.title.isEmpty() ? name : tr("%1 – %2").arg(file.title, name));
        item->setData(0, Qt::UserRole, file.file);
        item->setData(0, Qt::UserRole + 1, QStringLiteral("unassigned_file"));
        item->setData(0, TITLE_ROLE, file.title);
        item->setToolTip(0, QDir::toNativeSeparators(QDir::cleanPath(project.filePath(file.file))));
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    }
    updateUnassignedLook();
    m_unassignedItem->setExpanded(expanded);

    // A filter of the tree applies to the new items too
    if (!m_searchEdit->text().isEmpty() || m_currentFilterType != FilterType::All) {
        filterTree(m_searchEdit->text());
    }
}

void NavigatorPanel::updateUnassignedLook() {
    if (!m_unassignedItem) {
        return;
    }
    // Grey and italic, as what is not in the book
    const QColor grey = core::ThemeManager::getInstance()
                            .getCurrentTheme()
                            .palette.toQPalette()
                            .color(QPalette::PlaceholderText);
    QFont italic = m_treeWidget->font();
    italic.setItalic(true);
    const std::function<void(QTreeWidgetItem*)> apply = [&](QTreeWidgetItem* item) {
        item->setForeground(0, grey);
        item->setFont(0, italic);
        item->setIcon(0, greyIcon(getIconIdForType(item->data(0, Qt::UserRole + 1).toString()),
                                  m_currentIconSize));
        for (int i = 0; i < item->childCount(); ++i) {
            apply(item->child(i));
        }
    };
    apply(m_unassignedItem);
}

void NavigatorPanel::showInFileManager(const QString& path) {
    if (!showInFolder(path)) {
        dialogs::MessageDialog::warning(
            this, tr("Show in Folder"),
            tr("The file manager of the system could not show:\n%1")
                .arg(QDir::toNativeSeparators(path)));
    }
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

std::optional<core::ElementPlace> NavigatorPanel::dropPlaceOf(
    const core::BookProject& project, const core::BookTypeRegistry& registry,
    const QTreeWidgetItem* dragged, const QTreeWidgetItem* target, DropSpot spot) {
    if (!dragged || !target || target == dragged || !isElementType(typeOf(dragged))) {
        return std::nullopt;
    }
    const QString elementId = dragged->data(0, Qt::UserRole).toString();
    const std::optional<core::ElementPlace> from = project.placeOf(elementId);
    if (!from || from->place == core::BookPlace::Workshop) {
        return std::nullopt;
    }

    // The places are in the book without the element
    core::BookProject without = project;
    const std::optional<core::ProjectElement> element = without.takeElement(elementId);
    if (!element) {
        return std::nullopt;
    }
    const core::KindRef kind = core::BookProject::kindOf(registry, *element);
    const QString targetId = target->data(0, Qt::UserRole).toString();
    const QString targetType = typeOf(target);

    // The places the line or the frame shows, the first ones first
    QList<core::ElementPlace> candidates;
    if (spot == DropSpot::On) {
        // Where an element of its kind is added to the section or the group
        const std::optional<core::BookPlace> section = sectionPlaceOf(targetType);
        const core::ProjectElement* group =
            targetType == QLatin1String("group_element") ? without.findElement(targetId)
                                                         : nullptr;
        const std::optional<core::ElementPlace> groupPlace =
            group ? without.placeOf(targetId) : std::nullopt;
        if (kind && section) {
            candidates.append({*section, QString(),
                               core::BookProject::newIndexIn(registry,
                                                             without.elementsIn(*section), kind)});
        } else if (kind && group && groupPlace) {
            candidates.append({groupPlace->place, targetId,
                               core::BookProject::newIndexIn(registry, group->elements, kind)});
        }
    } else if (isElementType(targetType)) {
        const std::optional<core::ElementPlace> at = without.placeOf(targetId);
        if (!at) {
            return std::nullopt;  // Inside the element itself
        }

        // In a book without sections, at the end of a section the line is also at the start
        // of the next ones up to the first one with elements, and the other way round
        const bool oneList = at->groupId.isEmpty() && target->parent() &&
                             typeOf(target->parent()) == QLatin1String("document");
        const qsizetype part = bookParts().indexOf(at->place);
        if (spot == DropSpot::Above) {
            candidates.append(*at);
            for (qsizetype i = part - 1; oneList && at->index == 0 && i >= 0; --i) {
                const QList<core::ProjectElement>& before = without.elementsIn(bookParts().at(i));
                candidates.append({bookParts().at(i), QString(), before.size()});
                if (!before.isEmpty()) {
                    break;
                }
            }
        } else {
            // Below a group that shows its elements: first in it
            if (targetType == QLatin1String("group_element") && showsChildren(target)) {
                candidates.append({at->place, targetId, 0});
            }
            const core::ElementPlace after{at->place, at->groupId, at->index + 1};
            candidates.append(after);
            const bool atEnd =
                oneList && part >= 0 && after.index == without.elementsIn(at->place).size();
            for (qsizetype i = part + 1; atEnd && i < bookParts().size(); ++i) {
                candidates.append({bookParts().at(i), QString(), 0});
                if (!without.elementsIn(bookParts().at(i)).isEmpty()) {
                    break;
                }
            }
        }
    }

    // The first place its kind can have; none when the element stays where it is
    const QList<core::ElementPlace> places = project.movePlacesOf(registry, elementId);
    for (const core::ElementPlace& candidate : std::as_const(candidates)) {
        if (places.contains(candidate)) {
            return candidate == *from ? std::nullopt : std::optional(candidate);
        }
    }
    return std::nullopt;
}

namespace {

/// ArtProvider id of the icon of the elements of @p form without an icon of their own
QString formIconId(core::ElementForm form) {
    switch (form) {
    case core::ElementForm::Group:
        return QStringLiteral("structure.part");
    case core::ElementForm::Window:
        return QStringLiteral("common.file");
    case core::ElementForm::Text:
        break;
    }
    return QStringLiteral("template.chapter");
}

}  // namespace

QString NavigatorPanel::iconIdOf(const core::BookTypeRegistry& registry,
                                 const core::ProjectElement& element) {
    // A file of the book outside its text has no kind
    if (!element.hasKind()) {
        return BOOK_FILE_ICON;
    }
    const core::KindRef kind = core::BookProject::kindOf(registry, element);
    if (kind && !kind.kind->icon.isEmpty()) {
        return kind.kind->icon;
    }
    return formIconId(core::BookProject::formOf(registry, element));
}

QString NavigatorPanel::iconIdOf(const core::KindRef& kind) {
    if (!kind) {
        return formIconId(core::ElementForm::Text);
    }
    return kind.kind->icon.isEmpty() ? formIconId(kind.kind->form) : kind.kind->icon;
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

void NavigatorPanel::addBookFileItems(QTreeWidgetItem* parent,
                                      const QList<core::ProjectElement>& files) {
    if (files.isEmpty()) {
        return;
    }
    auto& artProvider = core::ArtProvider::getInstance();

    // Not dragged, and nothing is dropped on it: the files have no place in the text
    auto* group = new QTreeWidgetItem(parent);
    //: The item of the Navigator with the files of the book outside its text, such as notes,
    //: which the finished book does not have. In Polish: Inne pliki
    group->setText(0, tr("Other Files"));
    group->setData(0, Qt::UserRole, QString());
    group->setData(0, Qt::UserRole + 1, QStringLiteral("book_files"));
    //: In Polish: Pliki trzymane razem z książką, które nie należą do jej treści: gotowa książka
    //: ich nie zawiera
    group->setToolTip(0, tr("Files kept with the book that are not part of its text: the "
                            "finished book does not have them"));
    group->setIcon(0, artProvider.getIcon(getIconIdForType(QStringLiteral("book_files")),
                                          core::IconContext::TreeView));
    group->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);

    for (const core::ProjectElement& file : files) {
        auto* item = new QTreeWidgetItem(group);
        item->setText(0, getDisplayTitle(file, false, m_modifiedElements.contains(file.id)));
        item->setData(0, Qt::UserRole, file.id);
        item->setData(0, Qt::UserRole + 1, QStringLiteral("book_file"));
        item->setData(0, ICON_ROLE, BOOK_FILE_ICON);
        item->setData(0, TITLE_ROLE, file.title);
        item->setIcon(0, artProvider.getIcon(BOOK_FILE_ICON, core::IconContext::TreeView));
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    }
    group->setExpanded(true);
}

QString NavigatorPanel::getDisplayTitle(const core::ProjectElement& element, bool isText,
                                        bool isModified) {
    QString title = element.title;
    // Final status = no suffix, others show [Status] with the names of the "Set Status" menu;
    // a file of the book outside its text has no status
    if (isText && element.hasKind()) {
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

    if (isUnassignedType(elementType)) {
        item->setIcon(0, greyIcon(iconId, m_currentIconSize));
    } else if (!iconId.isEmpty()) {
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
    } else if (elementType == "book_files") {
        return "common.folder";
    } else if (elementType == "book_file") {
        return BOOK_FILE_ICON;
    } else if (elementType == "unassigned_files") {
        return "structure.unassigned";
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
    //: The item of the Navigator with the files open beside the book, which are not in it.
    //: In Polish: Pliki spoza książki
    m_otherFilesItem->setText(0, tr("Files Outside the Book"));
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

    // Create item for the file, with "*" while it has unsaved changes; a new file that has no
    // file yet has the name of its tab
    const bool newFile = m_newFileNames.contains(path);
    const QString name = newFile ? m_newFileNames.value(path) : QFileInfo(path).fileName();
    QTreeWidgetItem* fileItem = new QTreeWidgetItem(m_otherFilesItem);
    fileItem->setText(0, m_modifiedFiles.contains(path) ? QLatin1Char('*') + name : name);
    fileItem->setData(0, Qt::UserRole, path);  // Store full path as ID
    fileItem->setData(0, Qt::UserRole + 1, "standalone_file");
    //: The tooltip of a new file in the Navigator, which has no file yet. In Polish: Nowy plik –
    //: jeszcze niezapisany
    fileItem->setToolTip(0, newFile ? tr("New file – not saved yet") : path);

    // Set icon based on file extension; a new file has the icon of its tab
    const QString iconId = newFile ? NEW_FILE_ICON : getIconIdForFile(path);
    if (newFile) {
        fileItem->setData(0, ICON_ROLE, iconId);
    }
    fileItem->setIcon(0, artProvider.getIcon(iconId, core::IconContext::TreeView));
    // Standalone files are not draggable or droppable
    fileItem->setFlags(fileItem->flags() & ~Qt::ItemIsDragEnabled & ~Qt::ItemIsDropEnabled);

    // Store reference
    m_standaloneFiles.insert(path, fileItem);

    logger.info("NavigatorPanel: Added standalone file: {}", path.toStdString());
}

void NavigatorPanel::addNewFile(const QString& key, const QString& name) {
    m_newFileNames.insert(key, name);
    addStandaloneFile(key);
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

    // Remove the item; the file's changes are no longer shown
    QTreeWidgetItem* item = it.value();
    m_standaloneFiles.erase(it);
    m_modifiedFiles.remove(path);
    m_newFileNames.remove(path);
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
    m_modifiedFiles.clear();
    m_newFileNames.clear();

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
        // Text files: chapters and all front/back matter items with text, and the files of the
        // book outside its text
        return elementType == "text_element" || elementType == "book_file";

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
        // Other files: the files of the book outside its text, the files outside the book
        // (standalone files) and those of "Unassigned Files"
        return elementType == "book_file" || elementType == "standalone_file" ||
               elementType == "unassigned_file";

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

    // Create context menu: each item with its access key, unique in its menu
    // (test_menu_access_keys); an action that cannot be used may say why in its tooltip
    QMenu menu(this);
    menu.setToolTipsVisible(true);

    auto& pm = core::ProjectManager::getInstance();
    const bool projectOpen = pm.isProjectOpen();

    // Move to any place of the book the element's kind can have, in the Move dialog; or within
    // the element's list: a section of the book or a group. Move to Start and Move to End keep
    // a prologue first and an epilogue last
    const auto addMoveActions = [&]() {
        const core::ProjectElement* element = pm.findElement(elementId);
        const bool knownKind = element && pm.kindOf(*element);
        QAction* moveToAction = menu.addAction(
            artProvider.getIcon("common.driveFileMove", core::IconContext::Menu),
            tr("Mo&ve..."));
        connect(moveToAction, &QAction::triggered, this,
                [this, elementId]() { emit requestMoveTo(elementId); });

        // The element can go to a place other than the one it has; one where its kind cannot be
        // (from an older package) has none among the places of its kind
        const std::optional<core::ElementPlace> current =
            pm.project() ? pm.project()->placeOf(elementId) : std::nullopt;
        const QList<core::ElementPlace> places = pm.movePlacesOf(elementId);
        const bool elsewhere =
            current && std::any_of(places.cbegin(), places.cend(),
                                   [&current](const core::ElementPlace& place) {
                                       return !(place == *current);
                                   });
        moveToAction->setEnabled(knownKind && elsewhere);
        if (element && !knownKind) {
            //: Why Move... cannot be used: the element's kind comes from a package of book
            //: types that this installation does not have. In Polish: Program nie ma pakietu,
            //: z którego pochodzi rodzaj tego elementu.
            const QString why = tr("The program does not have the package of this element's "
                                   "kind.");
            moveToAction->setToolTip(why);
            moveToAction->setStatusTip(why);
        }

        const core::BookProject* project = pm.project();
        qsizetype listIndex = -1;
        const QList<core::ProjectElement>* list =
            project ? project->listOf(elementId, &listIndex) : nullptr;
        const int index = list ? static_cast<int>(listIndex) : -1;
        const int siblingCount = list ? static_cast<int>(list->size()) : 0;
        const int startIndex = static_cast<int>(pm.startIndexOf(elementId));
        const int endIndex = static_cast<int>(pm.endIndexOf(elementId));

        QAction* moveToStartAction = menu.addAction(
            artProvider.getIcon("navigation.top", core::IconContext::Menu),
            tr("Move to S&tart"));
        connect(moveToStartAction, &QAction::triggered, this,
                [this, elementId, startIndex]() { emit elementMoved(elementId, startIndex); });
        moveToStartAction->setEnabled(startIndex >= 0 && index > startIndex);

        QAction* moveUpAction = menu.addAction(
            artProvider.getIcon("navigation.up", core::IconContext::Menu),
            tr("Move &Up"));
        connect(moveUpAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuMoveUp);
        moveUpAction->setEnabled(index > 0);

        QAction* moveDownAction = menu.addAction(
            artProvider.getIcon("navigation.down", core::IconContext::Menu),
            tr("Move Do&wn"));
        connect(moveDownAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuMoveDown);
        moveDownAction->setEnabled(index >= 0 && index < siblingCount - 1);

        QAction* moveToEndAction = menu.addAction(
            artProvider.getIcon("navigation.bottom", core::IconContext::Menu),
            tr("Move to E&nd"));
        connect(moveToEndAction, &QAction::triggered, this,
                [this, elementId, endIndex]() { emit elementMoved(elementId, endIndex); });
        moveToEndAction->setEnabled(index >= 0 && index < endIndex);
    };

    // "Add Story...", "Dodaj opowiadanie...": a new element of @p kind; none when the place takes
    // no element of the kind
    const auto addKindAction = [&](const core::KindRef& kind) -> QAction* {
        if (!kind) {
            return nullptr;
        }
        QString iconId = kind.kind->icon;
        if (iconId.isEmpty()) {
            iconId = kind.kind->form == core::ElementForm::Group
                         ? QStringLiteral("structure.part")
                         : QStringLiteral("template.chapter");
        }
        return menu.addAction(
            artProvider.getIcon(iconId, core::IconContext::Menu),
            core::fillWords(tr("Add {Kind}..."),
                            {{QStringLiteral("kind"), menuWords(wordsOf(kind))}}));
    };

    const auto addRenameDeleteActions = [&]() {
        QAction* renameAction = menu.addAction(
            artProvider.getIcon("edit.rename", core::IconContext::Menu),
            tr("Rena&me..."));
        connect(renameAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuRename);

        QAction* deleteAction = menu.addAction(
            artProvider.getIcon("edit.delete", core::IconContext::Menu),
            tr("&Delete..."));
        connect(deleteAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuDelete);
    };

    const auto addExpandCollapseActions = [&]() {
        QAction* expandAllAction = menu.addAction(
            artProvider.getIcon("common.expand", core::IconContext::Menu),
            tr("&Expand All"));
        connect(expandAllAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuExpandAll);

        QAction* collapseAllAction = menu.addAction(
            artProvider.getIcon("common.collapse", core::IconContext::Menu),
            tr("&Collapse All"));
        connect(collapseAllAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuCollapseAll);
    };

    // Build menu based on element type
    if (elementType == "text_element") {
        // Text elements (chapters, front/back matter items)
        QAction* openAction = menu.addAction(
            artProvider.getIcon("file.open", core::IconContext::Menu),
            tr("&Open"));
        connect(openAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuOpen);

        menu.addSeparator();
        addRenameDeleteActions();
        menu.addSeparator();
        addMoveActions();
        menu.addSeparator();

        // Add "Set Status" submenu for text elements
        QMenu* statusMenu = menu.addMenu(tr("Set &Status"));

        QActionGroup* statusGroup = new QActionGroup(statusMenu);
        statusGroup->setExclusive(true);

        // Get current status
        QString currentStatus = "draft";
        if (const core::ProjectElement* element = pm.findElement(elementId)) {
            currentStatus = core::ProjectManager::statusOf(*element);
        }

        // Create radio actions
        QAction* draftAction = statusMenu->addAction(tr("&Draft"));
        draftAction->setCheckable(true);
        draftAction->setChecked(currentStatus == "draft");
        draftAction->setData("draft");
        statusGroup->addAction(draftAction);

        QAction* revisionAction = statusMenu->addAction(tr("&Revision"));
        revisionAction->setCheckable(true);
        revisionAction->setChecked(currentStatus == "revision");
        revisionAction->setData("revision");
        statusGroup->addAction(revisionAction);

        QAction* finalAction = statusMenu->addAction(tr("&Final"));
        finalAction->setCheckable(true);
        finalAction->setChecked(currentStatus == "final");
        finalAction->setData("final");
        statusGroup->addAction(finalAction);

        connect(statusGroup, &QActionGroup::triggered,
                this, &NavigatorPanel::onContextMenuSetStatus);

        menu.addSeparator();

        QAction* propertiesAction = menu.addAction(
            artProvider.getIcon("common.properties", core::IconContext::Menu),
            tr("P&roperties..."));
        connect(propertiesAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuProperties);

    } else if (elementType == "book_file") {
        // A file of the book outside its text: no place in the text and no status
        QAction* openAction = menu.addAction(
            artProvider.getIcon("file.open", core::IconContext::Menu), tr("&Open"));
        connect(openAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuOpen);
        menu.addSeparator();
        addRenameDeleteActions();
        menu.addSeparator();
        QAction* propertiesAction = menu.addAction(
            artProvider.getIcon("common.properties", core::IconContext::Menu),
            tr("P&roperties..."));
        connect(propertiesAction, &QAction::triggered, this,
                &NavigatorPanel::onContextMenuProperties);

    } else if (elementType == "book_files") {
        addExpandCollapseActions();

    } else if (elementType == "group_element") {
        // Group (part): the main texts of the book (chapters, stories...) are added inside it
        if (QAction* addChapterAction =
                addKindAction(pm.chapterKindFor(core::BookPlace::Main, elementId))) {
            connect(addChapterAction, &QAction::triggered, this,
                    &NavigatorPanel::onContextMenuAddChapter);
        }

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
            // Body section: parts and the main texts of the book, of the kinds of its type
            if (QAction* addPartAction = addKindAction(pm.partKind())) {
                connect(addPartAction, &QAction::triggered, this,
                        &NavigatorPanel::onContextMenuAddPart);
            }
            if (QAction* addChapterAction =
                    addKindAction(pm.chapterKindFor(core::BookPlace::Main))) {
                connect(addChapterAction, &QAction::triggered, this,
                        &NavigatorPanel::onContextMenuAddChapter);
            }
        } else {
            // Front/Back Matter - can add items
            const core::BookPlace place = elementType == "section_frontmatter"
                                              ? core::BookPlace::Front
                                              : core::BookPlace::Back;
            QAction* addItemAction = menu.addAction(
                artProvider.getIcon("template.chapter", core::IconContext::Menu),
                tr("&Add Item..."));
            connect(addItemAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuAddItem);
            addItemAction->setEnabled(projectOpen && !pm.textKindsFor(place).isEmpty());
        }

        // The writer can name each section
        menu.addSeparator();
        QAction* renameAction = menu.addAction(
            artProvider.getIcon("edit.rename", core::IconContext::Menu), tr("Rena&me..."));
        connect(renameAction, &QAction::triggered, this,
                [this, elementType]() { emit requestRenameSection(elementType); });
        renameAction->setEnabled(projectOpen);

        menu.addSeparator();
        addExpandCollapseActions();

    } else if (elementType == "document") {
        // Document root
        const core::ProjectBook* book = pm.book();
        const bool sectionsShown = !book || book->partsLayer;
        if (projectOpen && !sectionsShown) {
            // A book without sections: its elements are added from its item
            if (QAction* addPartAction = addKindAction(pm.partKind())) {
                connect(addPartAction, &QAction::triggered,
                        this, &NavigatorPanel::onContextMenuAddPart);
            }
            if (QAction* addChapterAction =
                    addKindAction(pm.chapterKindFor(core::BookPlace::Main))) {
                connect(addChapterAction, &QAction::triggered, this,
                        [this]() { emit requestAddChapter(QString()); });
            }

            QAction* addFirstAction = menu.addAction(
                artProvider.getIcon("structure.frontmatter", core::IconContext::Menu),
                tr("Add Item at the &Beginning of the Book..."));
            connect(addFirstAction, &QAction::triggered, this,
                    [this]() { emit requestAddItem(QStringLiteral("front_matter")); });
            addFirstAction->setEnabled(!pm.textKindsFor(core::BookPlace::Front).isEmpty());

            QAction* addLastAction = menu.addAction(
                artProvider.getIcon("structure.backmatter", core::IconContext::Menu),
                tr("Add Item at the &End of the Book..."));
            connect(addLastAction, &QAction::triggered, this,
                    [this]() { emit requestAddItem(QStringLiteral("back_matter")); });
            addLastAction->setEnabled(!pm.textKindsFor(core::BookPlace::Back).isEmpty());

            menu.addSeparator();
        }
        if (projectOpen) {
            // Showing or hiding the sections moves no element
            QAction* showSectionsAction = menu.addAction(tr("&Show Sections"));
            showSectionsAction->setCheckable(true);
            showSectionsAction->setChecked(sectionsShown);
            connect(showSectionsAction, &QAction::toggled, this,
                    [this](bool shown) { emit requestShowSections(shown); });
            menu.addSeparator();
        }

        QAction* propertiesAction = menu.addAction(
            artProvider.getIcon("common.properties", core::IconContext::Menu),
            tr("Book P&roperties..."));
        connect(propertiesAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuProperties);

    } else if (elementType == "standalone_file") {
        // Standalone file
        QAction* openAction = menu.addAction(
            artProvider.getIcon("file.open", core::IconContext::Menu),
            tr("&Open"));
        connect(openAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuOpen);

        menu.addSeparator();

        // A new file that has no file yet goes into the book at once, as the button of its
        // info bar adds it: no window follows
        const bool newFile = m_newFileNames.contains(elementId);
        QAction* addToProjectAction = menu.addAction(
            artProvider.getIcon("book.newItem", core::IconContext::Menu),
            //: The command of a new file that has no file yet; no window follows. In Polish:
            //: &Dodaj do książki
            newFile ? tr("&Add to Book") : tr("&Add to Book..."));
        connect(addToProjectAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuAddToProject);
        // Without a book it could only say that one must be opened first
        addToProjectAction->setEnabled(projectOpen);
        if (!projectOpen) {
            addToProjectAction->setToolTip(tr("No book is open"));
        }

        // The list has every file open outside the book: a new file leaves it with its tab
        if (!newFile) {
            QAction* removeFromListAction = menu.addAction(
                artProvider.getIcon("edit.delete", core::IconContext::Menu),
                tr("&Remove from List"));
            connect(removeFromListAction, &QAction::triggered, this,
                    &NavigatorPanel::onContextMenuRemoveFromList);
        }

    } else if (elementType == "unassigned_file") {
        // A chapter file that no element has: its text, back to the book, or deleted
        const QString path = QDir::cleanPath(QDir(m_projectFolder).filePath(elementId));
        QAction* previewAction = menu.addAction(
            artProvider.getIcon("common.preview", core::IconContext::Menu), tr("&Preview"));
        connect(previewAction, &QAction::triggered, this,
                [this, elementId]() { emit requestPreviewUnassigned(elementId); });
        QAction* addAction = menu.addAction(
            artProvider.getIcon("book.newItem", core::IconContext::Menu), tr("&Add to Book..."));
        connect(addAction, &QAction::triggered, this,
                [this, elementId]() { emit requestAddUnassigned(elementId); });
        menu.addSeparator();
        QAction* showAction = menu.addAction(
            artProvider.getIcon("common.folder", core::IconContext::Menu), tr("Show in &Folder"));
        connect(showAction, &QAction::triggered, this,
                [this, path]() { showInFileManager(path); });
        menu.addSeparator();
        QAction* deleteAction = menu.addAction(
            artProvider.getIcon("edit.delete", core::IconContext::Menu), tr("&Delete File..."));
        connect(deleteAction, &QAction::triggered, this,
                [this, elementId]() { emit requestDeleteUnassigned(elementId); });

    } else if (elementType == "unassigned_files") {
        // The files that no element has: their folder, or all of them deleted
        const QString first =
            item->childCount() > 0 ? item->child(0)->data(0, Qt::UserRole).toString() : QString();
        const QString folder =
            QFileInfo(QDir::cleanPath(QDir(m_projectFolder).filePath(first))).absolutePath();
        QAction* showAction = menu.addAction(
            artProvider.getIcon("common.folder", core::IconContext::Menu), tr("Show in &Folder"));
        connect(showAction, &QAction::triggered, this,
                [this, folder]() { showInFileManager(folder); });
        menu.addSeparator();
        QAction* deleteAllAction = menu.addAction(
            artProvider.getIcon("edit.delete", core::IconContext::Menu), tr("&Delete All..."));
        connect(deleteAllAction, &QAction::triggered, this,
                [this]() { emit requestDeleteAllUnassigned(); });
        menu.addSeparator();
        addExpandCollapseActions();

    } else if (elementType == "other_files") {
        // Other Files header - only expand/collapse
        addExpandCollapseActions();

    } else {
        // Unknown type - no menu
        logger.debug("NavigatorPanel: No context menu for type: {}", elementType.toStdString());
        m_contextMenuItem = nullptr;
        return;
    }

    // The items named after the kinds of the book ("Add Story...") get keys that no other item
    // of the menu has
    giveFreeAccessKeys(&menu);

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
    emit requestPropertiesPanel();
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
    } else if (elementType == "text_element" || elementType == "book_file") {
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

    // A file that no element has shows its text; it opens in the editor once it is in the book
    if (elementType == "unassigned_file") {
        emit requestPreviewUnassigned(elementId);
        return;
    }

    // Only text elements and the files of the book open in the editor; sections, groups and
    // window elements do not
    if (elementType == "text_element" || elementType == "book_file") {
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

    // The files that no element has stay grey in the colors of the theme
    updateUnassignedLook();

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

void NavigatorPanel::revealElement(const QString& elementId) {
    QTreeWidgetItem* item = findItemByElementId(elementId);
    if (!item) {
        return;
    }
    for (QTreeWidgetItem* parent = item->parent(); parent; parent = parent->parent()) {
        parent->setExpanded(true);
    }
    m_treeWidget->setCurrentItem(item);
    m_treeWidget->scrollToItem(item, QAbstractItemView::EnsureVisible);
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

    // Check if this item matches. The tree has one document, one of each section and one
    // "Other Files"; their names change (a renamed book or section), so the type finds them.
    QString itemType = parent->data(0, Qt::UserRole + 1).toString();
    const bool single = itemType == QLatin1String("document") ||
                        itemType == QLatin1String("section_frontmatter") ||
                        itemType == QLatin1String("section_body") ||
                        itemType == QLatin1String("section_backmatter") ||
                        itemType == QLatin1String("book_files") ||
                        itemType == QLatin1String("unassigned_files") ||
                        itemType == QLatin1String("other_files");
    if (itemType == elementType && (single || parent->text(0) == text)) {
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
        dialogs::MessageDialog::warning(this, tr("Status Change Failed"),
                                        tr("Failed to save changes."));
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

void NavigatorPanel::setFileModified(const QString& path, bool isModified) {
    if (path.isEmpty()) {
        return;  // no file: the list names a new file by its key
    }
    if (isModified) {
        m_modifiedFiles.insert(path);
    } else {
        m_modifiedFiles.remove(path);
    }
    if (QTreeWidgetItem* item = m_standaloneFiles.value(path)) {
        const QString name =
            m_newFileNames.contains(path) ? m_newFileNames.value(path) : QFileInfo(path).fileName();
        item->setText(0, isModified ? QLatin1Char('*') + name : name);
    }
}

} // namespace gui
} // namespace kalahari
