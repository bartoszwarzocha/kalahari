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
#include "kalahari/gui/material_source.h"
#include "kalahari/gui/paste_target.h"
#include "kalahari/gui/read_only_book.h"
#include "kalahari/gui/shortcut_rules.h"
#include "kalahari/gui/utils/access_keys.h"
#include "kalahari/gui/utils/file_location.h"
#include "kalahari/core/logger.h"
#include "kalahari/core/book_project.h"
#include "kalahari/core/project_manager.h"
#include "kalahari/core/art_provider.h"
#include "kalahari/core/package_card.h"
#include "kalahari/core/theme_manager.h"
#include "kalahari/core/settings_manager.h"
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidgetItem>
#include <QTreeWidgetItemIterator>
#include <QDir>
#include <QFileInfo>
#include <QLineEdit>
#include <QToolButton>
#include <QComboBox>
#include <QTimer>
#include <QMenu>
#include <QActionGroup>
#include <QPalette>
#include <QApplication>
#include <QBrush>
#include <QDropEvent>
#include <QBasicTimer>
#include <QClipboard>
#include <QCollator>
#include <QCoreApplication>
#include <QFont>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLocale>
#include <QMimeData>
#include <QMimeDatabase>
#include <QPainter>
#include <QPen>
#include <QPersistentModelIndex>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
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
/// Package of an element's kind that the program does not have: the book's card of it has the
/// icon of the kind (ICON_ROLE)
constexpr int CARD_ROLE = Qt::UserRole + 5;
/// The element is of the Workshop: no status, not dragged
constexpr int WORKSHOP_ROLE = Qt::UserRole + 6;
/// The number of the elements of the Materials of the Workshop, and of each of its groups,
/// which the tree shows at the right of the name
constexpr int COUNT_ROLE = Qt::UserRole + 7;
/// Room between the name of a node and its number, in multiples of the width of a space
constexpr int COUNT_GAP = 3;
/// Milliseconds before a collapsed group or section that a dragged element rests on opens
constexpr int AUTO_EXPAND_DELAY = 700;
/// The id of a group of the Workshop starts with it: "workshop:kalahari.base:work_note"
const QString WORKSHOP_ID_PREFIX = QStringLiteral("workshop:");
/// In a list of open items, the tree has the book and the Workshop as nodes of their own; a
/// list without it (saved before them) opens them
const QString WORKSHOP_LAYOUT = QStringLiteral("layout:workshop");
/// Icon of a file of the book outside its text, as New File shows an empty text file
const QString BOOK_FILE_ICON = QStringLiteral("common.file");
/// Icon of a new file outside the book that has no file yet, as its tab shows it
const QString NEW_FILE_ICON = QStringLiteral("template.chapter");

/// Whether an item of @p type holds other items: the project, the book, its sections, groups,
/// the Workshop and its groups...
bool isContainerType(const QString& type) {
    return type == QLatin1String("document") || type == QLatin1String("book") ||
           type == QLatin1String("section_frontmatter") || type == QLatin1String("section_body") ||
           type == QLatin1String("section_backmatter") || type == QLatin1String("group_element") ||
           type == QLatin1String("book_files") || type == QLatin1String("workshop") ||
           type == QLatin1String("workshop_shelf") || type == QLatin1String("workshop_group") ||
           type == QLatin1String("unassigned_files") || type == QLatin1String("other_files");
}

/// Whether an item of @p type is a node of the Workshop: the Workshop or a group of it
bool isWorkshopNode(const QString& type) {
    return type == QLatin1String("workshop") || type == QLatin1String("workshop_shelf") ||
           type == QLatin1String("workshop_group");
}

/// @p text of a command of a menu with its keys @p keys in the column of the keys, unless the
/// program shows no keys in menus
QString withKeys(const QString& text, QKeyCombination keys) {
    if (QCoreApplication::testAttribute(Qt::AA_DontShowShortcutsInContextMenus)) {
        return text;
    }
    return text + QLatin1Char('\t') + kalahari::gui::ShortcutRules::keysText(keys);
}

/// The keys of Add... on a node of the Navigator, as in the editor: Ctrl+Shift+Enter
QKeyCombination addKeys() {
    return QKeyCombination(Qt::ControlModifier | Qt::ShiftModifier, Qt::Key_Return);
}

/// The keys of Properties... on a node of the Navigator: Alt+Enter, as in the file managers
QKeyCombination propertiesKeys() {
    return QKeyCombination(Qt::AltModifier, Qt::Key_Return);
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

/// @p icon as a disabled item shows it: grey, for the files that are not in the book and for
/// a book open read-only
QIcon greyIcon(const QIcon& icon, int size) {
    QIcon grey;
    grey.addPixmap(icon.pixmap(QSize(size, size), QIcon::Disabled));
    return grey;
}

/// Whether an item of @p type is of the open book: not one of the files open outside it
bool isBookType(const QString& type) {
    return type != QLatin1String("other_files") && type != QLatin1String("standalone_file");
}

/// The elements of section @p place of @p book
const QList<kalahari::core::ProjectElement>& sectionOf(const kalahari::core::ProjectBook& book,
                                                       kalahari::core::BookPlace place) {
    switch (place) {
    case kalahari::core::BookPlace::Front:
        return book.frontElements;
    case kalahari::core::BookPlace::Back:
        return book.backElements;
    default:
        return book.mainElements;
    }
}

/// Where Move Up (@p up) or Move Down takes element @p elementId of @p book from the start or
/// the end of its list, where its kind can be (@p places, of movePlacesOf()): before or after
/// its group; else to the end of the nearest section before it or the start of the nearest
/// one after it, or, in a book without sections, one step up or down in the one list of its
/// three sections. None when its kind cannot be there.
std::optional<kalahari::core::ElementPlace> placeBeyondList(
    const kalahari::core::BookProject& project, const kalahari::core::ProjectBook* book,
    const QList<kalahari::core::ElementPlace>& places, const QString& elementId, bool up) {
    const std::optional<kalahari::core::ElementPlace> from = project.placeOf(elementId);
    if (!from) {
        return std::nullopt;
    }
    if (!from->groupId.isEmpty()) {
        const std::optional<kalahari::core::ElementPlace> group =
            project.placeOf(from->groupId);
        if (!group) {
            return std::nullopt;
        }
        const kalahari::core::ElementPlace beyond{group->place, group->groupId,
                                                  group->index + (up ? 0 : 1)};
        return places.contains(beyond) ? std::optional(beyond) : std::nullopt;
    }
    const qsizetype part = bookParts().indexOf(from->place);
    if (part < 0) {
        return std::nullopt;
    }

    // Without the sections, the end of a section and the start of the next one are the same
    // place in the list: the element goes past the nearest element of another section, into
    // the section nearer to its own of the two
    if (book && !book->partsLayer) {
        const auto lineOf = [book, &from](const kalahari::core::ElementPlace& place) {
            qsizetype line = place.index;
            for (const kalahari::core::BookPlace before : bookParts()) {
                if (before == place.place) {
                    break;
                }
                line += sectionOf(*book, before).size() - (before == from->place ? 1 : 0);
            }
            return line;
        };
        const qsizetype now = lineOf(*from);
        std::optional<kalahari::core::ElementPlace> found;
        for (const kalahari::core::ElementPlace& place : places) {
            if (!place.groupId.isEmpty() || place.place == from->place) {
                continue;
            }
            const qsizetype line = lineOf(place);
            if (up ? line < now && (!found || line >= lineOf(*found))
                   : line > now && (!found || line < lineOf(*found))) {
                found = place;
            }
        }
        return found;
    }

    for (qsizetype i = part + (up ? -1 : 1); i >= 0 && i < bookParts().size();
         i += up ? -1 : 1) {
        std::optional<kalahari::core::ElementPlace> found;
        for (const kalahari::core::ElementPlace& place : places) {
            if (place.place == bookParts().at(i) && place.groupId.isEmpty() &&
                (up ? !found || place.index > found->index : place.index == 0)) {
                found = place;
            }
        }
        if (found) {
            return found;
        }
    }
    return std::nullopt;
}

/// Draws the number of a node that has one (COUNT_ROLE) at the right of the row, grey, and
/// shortens the name to leave room for it
class CountDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override {
        const QVariant count = index.data(COUNT_ROLE);
        if (!count.isValid()) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
        QStyleOptionViewItem item(option);
        initStyleOption(&item, index);
        const QWidget* widget = option.widget;
        QStyle* style = widget ? widget->style() : QApplication::style();
        const QString number = QLocale().toString(count.toInt());
        const int room = item.fontMetrics.horizontalAdvance(number) + gap(item.fontMetrics);
        const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &item, widget);
        item.text = item.fontMetrics.elidedText(item.text, item.textElideMode,
                                                std::max(0, textRect.width() - room));
        style->drawControl(QStyle::CE_ItemViewItem, &item, painter, widget);

        // On the selected row in the color of its text, elsewhere grey
        const bool selected = (option.state & QStyle::State_Selected) != 0;
        const QPalette::ColorGroup group = (option.state & QStyle::State_Enabled) == 0
                                               ? QPalette::Disabled
                                           : (option.state & QStyle::State_Active) != 0
                                               ? QPalette::Active
                                               : QPalette::Inactive;
        painter->save();
        painter->setFont(item.font);
        painter->setPen(option.palette.color(
            group, selected ? QPalette::HighlightedText : QPalette::PlaceholderText));
        painter->drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, number);
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        QSize size = QStyledItemDelegate::sizeHint(option, index);
        const QVariant count = index.data(COUNT_ROLE);
        if (count.isValid()) {
            const QString number = QLocale().toString(count.toInt());
            size.rwidth() += option.fontMetrics.horizontalAdvance(number) + gap(option.fontMetrics);
        }
        return size;
    }

private:
    static int gap(const QFontMetrics& metrics) {
        return COUNT_GAP * metrics.horizontalAdvance(QLatin1Char(' '));
    }
};

/// Tree of the Navigator. An element is dragged to any place of the book its kind can have
/// (NavigatorPanel::dropPlaceOf()); the drop does not change the tree, it asks for the move,
/// and the tree shows the book as it is once it is loaded again. In a book without sections
/// the elements of the three sections are all under the book's item, each section after the
/// one before it.
///
/// Files and web addresses dragged from outside the program, and Paste of the clipboard, give
/// new materials (onMaterials); a dashed frame shows the node the drop links them with, or the
/// whole tree when it links them with none.
class NavigatorTree : public QTreeWidget, public kalahari::gui::PasteTarget {
public:
    using QTreeWidget::QTreeWidget;

    /// Called after a drop with the dragged element and its new place
    std::function<void(const QString& elementId, const kalahari::core::ElementPlace& place)>
        onMove;

    /// Called with the materials dropped from outside the program or pasted, and the link of
    /// the node they were dropped or pasted on (empty: none)
    std::function<void(const kalahari::gui::IncomingMaterials& incoming, const QString& link)>
        onMaterials;

    /// Whether the book takes new materials now, and whether they are linked with the node
    /// they are dropped on
    std::function<bool()> takesMaterials;
    std::function<bool(const QTreeWidgetItem* item)> linksMaterials;

    bool canPasteHere() const override {
        const QMimeData* mime = QGuiApplication::clipboard()->mimeData();
        return takesMaterials && takesMaterials() && mime != nullptr &&
               (mime->hasUrls() || mime->hasImage() || mime->hasText());
    }

    void pasteHere() override {
        if (onMaterials) {
            onMaterials(
                kalahari::gui::IncomingMaterials::of(QGuiApplication::clipboard()->mimeData()),
                linkOf(currentItem()));
        }
    }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override {
        if (takesFromOutside(event)) {
            showDrop(event);
            return;
        }
        QTreeWidget::dragEnterEvent(event);
    }

    void dragMoveEvent(QDragMoveEvent* event) override {
        // The item under the dragged element opens when it rests there (timerEvent())
        m_openPos = event->position().toPoint();
        m_openTimer.start(AUTO_EXPAND_DELAY, this);

        if (takesFromOutside(event)) {
            showDrop(event);
            return;
        }
        QTreeWidget::dragMoveEvent(event);
        if (event->isAccepted() && !dropPlace(event->position().toPoint())) {
            event->ignore();
        }
    }

    void dragLeaveEvent(QDragLeaveEvent* event) override {
        m_openTimer.stop();
        hideDrop();
        QTreeWidget::dragLeaveEvent(event);
    }

    /// The dashed frame of a drop from outside the program
    void paintEvent(QPaintEvent* event) override {
        QTreeWidget::paintEvent(event);
        if (!m_outsideDrop) {
            return;
        }
        QPainter painter(viewport());
        QPen pen(palette().color(QPalette::Highlight), 2, Qt::DashLine);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        const QRect frame = m_dropIndex.isValid() ? visualRect(m_dropIndex)
                                                  : viewport()->rect();
        painter.drawRect(frame.adjusted(1, 1, -2, -2));
    }

    /// The dragged nodes also give their links, which the links of a material take
    /// (NavigatorPanel::LINKS_MIME_TYPE)
    QMimeData* mimeData(const QList<QTreeWidgetItem*>& items) const override {
        QMimeData* mime = QTreeWidget::mimeData(items);
        QStringList links;
        for (const QTreeWidgetItem* item : items) {
            const QString link = kalahari::gui::NavigatorPanel::linkOf(item);
            if (!link.isEmpty()) {
                links << link;
            }
        }
        if (mime && !links.isEmpty()) {
            mime->setData(QLatin1String(kalahari::gui::NavigatorPanel::LINKS_MIME_TYPE),
                          links.join(QLatin1Char('\n')).toUtf8());
        }
        return mime;
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
        if (takesFromOutside(event)) {
            const kalahari::gui::IncomingMaterials incoming =
                kalahari::gui::IncomingMaterials::of(event->mimeData());
            const QString link = linkOf(itemAt(event->position().toPoint()));
            event->setDropAction(Qt::CopyAction);
            event->accept();
            hideDrop();
            if (onMaterials) {
                // After the drop has ended: the new materials load the tree again
                QTimer::singleShot(0, this,
                                   [this, incoming, link]() { onMaterials(incoming, link); });
            }
            return;
        }
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
    /// Whether @p event brings files, web addresses or a picture from outside the program that
    /// the book takes now
    bool takesFromOutside(const QDropEvent* event) const {
        return event->source() == nullptr && takesMaterials && takesMaterials() &&
               kalahari::gui::IncomingMaterials::of(event->mimeData()).hasMaterials();
    }

    /// Accept the drop from outside the program of @p event as a copy, with the frame of the
    /// node it links the materials with
    void showDrop(QDragMoveEvent* event) {
        const QTreeWidgetItem* item = itemAt(event->position().toPoint());
        const QModelIndex index = !linkOf(item).isEmpty() ? indexFromItem(item) : QModelIndex();
        if (!m_outsideDrop || index != m_dropIndex) {
            m_outsideDrop = true;
            m_dropIndex = index;
            viewport()->update();
        }
        event->setDropAction(Qt::CopyAction);
        event->accept();
    }

    void hideDrop() {
        if (m_outsideDrop) {
            m_outsideDrop = false;
            m_dropIndex = QPersistentModelIndex();
            viewport()->update();
        }
    }

    /// The link new materials get from node @p item; empty when it gives none
    QString linkOf(const QTreeWidgetItem* item) const {
        return item && linksMaterials && linksMaterials(item)
                   ? kalahari::gui::NavigatorPanel::linkOf(item)
                   : QString();
    }

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
    bool m_outsideDrop = false;            ///< Files or addresses are dragged over the tree
    QPersistentModelIndex m_dropIndex;     ///< The node they would be linked with
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
    // Narrower than its texts only in a narrow dock, where the bar keeps its buttons in view
    m_typeFilter->setMinimumWidth(64);

    connect(m_typeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NavigatorPanel::onTypeFilterChanged);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Filter tree..."));
    m_searchEdit->setClearButtonEnabled(false);  // We use our own clear button
    // As the combo box: narrower than its own minimum only in a narrow dock, where the fonts
    // of Windows make that minimum wider than the room the bar has
    m_searchEdit->setMinimumWidth(36);

    m_clearButton = new QToolButton(this);
    m_clearButton->setIcon(artProvider.getIcon("common.cancel", core::IconContext::Menu));
    m_clearButton->setToolTip(tr("Clear filter"));
    m_clearButton->setAutoRaise(true);
    m_clearButton->setVisible(false);  // Hidden until text is entered

    // Add... on the current node, as its menu and Ctrl+Shift+Enter
    m_addButton = new QToolButton(this);
    m_addButton->setIcon(artProvider.getIcon("book.addElement", core::IconContext::Menu));
    //: The "+" button of the Navigator: Add... on the current node; %1: its keys. In Polish:
    //: Dodaj... (%1)
    m_addButton->setToolTip(tr("Add... (%1)").arg(ShortcutRules::keysText(addKeys())));
    m_addButton->setAutoRaise(true);
    m_addButton->setEnabled(false);

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
    searchLayout->addWidget(m_addButton);
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
    tree->setItemDelegate(new CountDelegate(tree));
    tree->onMove = [this](const QString& elementId, const core::ElementPlace& place) {
        core::Logger::getInstance().info("NavigatorPanel: Element {} dragged to place {} of {}",
                                         elementId.toStdString(), place.index,
                                         place.groupId.isEmpty()
                                             ? core::bookPlaceName(place.place).toStdString()
                                             : place.groupId.toStdString());
        emit elementMovedTo(elementId, place);
    };
    // Files, web addresses and pictures from outside the program are new materials, linked
    // with the node of the book they were dropped or pasted on (QUICK_MATERIALS_SETTING)
    tree->takesMaterials = [this]() {
        return !m_readOnly && core::ProjectManager::getInstance().isProjectOpen();
    };
    tree->linksMaterials = [](const QTreeWidgetItem* item) {
        return quickMaterials() == QLatin1String("link") && !isWorkshopItem(item) &&
               !NavigatorPanel::linkOf(item).isEmpty();
    };
    tree->onMaterials = [this](const IncomingMaterials& incoming, const QString& link) {
        emit requestAddMaterials(incoming, link);
    };
    m_treeWidget = tree;

    // Connect expand/collapse all buttons (after tree widget creation)
    connect(m_addButton, &QToolButton::clicked, this,
            [this]() { addOn(m_treeWidget->currentItem()); });
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
    // The keys of the tree: Add..., Rename and Delete on the current node
    m_treeWidget->installEventFilter(this);

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

                // The card of a material is in the Properties panel: it comes to the front
                if (requestPropertiesOf(item) && isMaterialItem(item)) {
                    emit requestPropertiesPanel();
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
    updateAddButton();
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

    // The project, named after the title of its book: the book, its Workshop and the files
    // that no element has (setUnassignedFiles()); the files open outside the book have a node
    // of their own after it
    QTreeWidgetItem* rootItem = new QTreeWidgetItem(m_treeWidget);
    rootItem->setText(0, book ? book->title : QString());
    rootItem->setData(0, Qt::UserRole, QString());  // No ID for root
    rootItem->setData(0, Qt::UserRole + 1, "document");
    rootItem->setIcon(0, artProvider.getIcon("project.book", core::IconContext::TreeView));
    rootItem->setExpanded(true);
    // Document is not draggable or droppable
    rootItem->setFlags(rootItem->flags() & ~Qt::ItemIsDragEnabled & ~Qt::ItemIsDropEnabled);

    // The book, which the writer can name ("Book")
    const core::ProjectBook noBook;
    QTreeWidgetItem* bookItem = new QTreeWidgetItem(rootItem);
    bookItem->setText(0, bookName(book ? *book : noBook));
    bookItem->setData(0, Qt::UserRole, QString());
    bookItem->setData(0, Qt::UserRole + 1, QStringLiteral("book"));
    bookItem->setIcon(0, artProvider.getIcon(getIconIdForType(QStringLiteral("book")),
                                             core::IconContext::TreeView));
    bookItem->setFlags(bookItem->flags() & ~Qt::ItemIsDragEnabled & ~Qt::ItemIsDropEnabled);

    if (book && !book->partsLayer) {
        // A book without sections: the elements of its three sections in reading order. The
        // book's item takes the drops of the elements.
        bookItem->setFlags(bookItem->flags() | Qt::ItemIsDropEnabled);
        addElementItems(bookItem, book->frontElements, registry);
        addElementItems(bookItem, book->mainElements, registry);
        addElementItems(bookItem, book->backElements, registry);
    } else {
        // The three sections of the book with its names, also when empty, so that elements can
        // be added to them. Elements are dragged to the sections and the groups, which
        // therefore take drops.
        const core::ProjectBook& names = book ? *book : noBook;
        QTreeWidgetItem* frontMatterItem =
            addSectionItem(bookItem, names.sectionName(core::BookPlace::Front),
                           QStringLiteral("section_frontmatter"));
        QTreeWidgetItem* bodyItem = addSectionItem(
            bookItem, names.sectionName(core::BookPlace::Main), QStringLiteral("section_body"));
        QTreeWidgetItem* backMatterItem =
            addSectionItem(bookItem, names.sectionName(core::BookPlace::Back),
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
        addBookFileItems(bookItem, book->files);
    }
    bookItem->setExpanded(true);

    // The Workshop, which the writer can name ("Workshop"), with the groups of its kinds
    QTreeWidgetItem* workshopItem = new QTreeWidgetItem(rootItem);
    workshopItem->setText(0, workshopName(project));
    workshopItem->setData(0, Qt::UserRole, QString());
    workshopItem->setData(0, Qt::UserRole + 1, QStringLiteral("workshop"));
    workshopItem->setIcon(0, artProvider.getIcon(getIconIdForType(QStringLiteral("workshop")),
                                                 core::IconContext::TreeView));
    workshopItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    addWorkshopItems(workshopItem, project, registry);
    workshopItem->setExpanded(true);

    // A book open read-only is grey (itemIcon())
    if (m_readOnly) {
        refreshItemIcons(rootItem);
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
    updateAddButton();

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

    // At the end of the project, after the Workshop: the files can be of the book or of the
    // Workshop; not dragged, and nothing is dropped on it
    m_unassignedItem = new QTreeWidgetItem(rootItem);
    //: The item of the Navigator with the chapter files that no element of the book has; %1:
    //: their number. In Polish: Pliki bez przydziału (%1)
    m_unassignedItem->setText(0, tr("Unassigned Files (%1)").arg(files.size()));
    m_unassignedItem->setData(0, Qt::UserRole, QString());
    m_unassignedItem->setData(0, Qt::UserRole + 1, QStringLiteral("unassigned_files"));
    //: In Polish: Pliki tekstów w folderach książki i Warsztatu, które nie należą do żadnego
    //: elementu
    m_unassignedItem->setToolTip(
        0, tr("Chapter files in the folders of the book and of the Workshop that no element "
              "has"));
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
        item->setIcon(0, greyIcon(core::ArtProvider::getInstance().getIcon(
                                      getIconIdForType(item->data(0, Qt::UserRole + 1).toString()),
                                      core::IconContext::TreeView),
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
    // Sections take the drops of their elements; they are dragged only to the links of a
    // material
    item->setFlags(item->flags() | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
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

    // The places the line or the frame shows, the first ones first; in a book without
    // sections, the places of a line between two sections
    QList<core::ElementPlace> candidates;
    QList<core::ElementPlace> sameLine;
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
                             typeOf(target->parent()) == QLatin1String("book");
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
            if (candidates.size() > 1) {
                sameLine = candidates;
            }
        } else {
            // Below a group that shows its elements: first in it
            if (targetType == QLatin1String("group_element") && showsChildren(target)) {
                candidates.append({at->place, targetId, 0});
            }
            const core::ElementPlace after{at->place, at->groupId, at->index + 1};
            const qsizetype afterAt = candidates.size();
            candidates.append(after);
            const bool atEnd =
                oneList && part >= 0 && after.index == without.elementsIn(at->place).size();
            for (qsizetype i = part + 1; atEnd && i < bookParts().size(); ++i) {
                candidates.append({bookParts().at(i), QString(), 0});
                if (!without.elementsIn(bookParts().at(i)).isEmpty()) {
                    break;
                }
            }
            if (candidates.size() - afterAt > 1) {
                sameLine = candidates.mid(afterAt);
            }
        }
    }

    // The first place its kind can have; none when the element stays where it is, also on a
    // line between two sections where it is already
    const QList<core::ElementPlace> places = project.movePlacesOf(registry, elementId);
    if (sameLine.contains(*from)) {
        return std::nullopt;
    }
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

/// The order of the elements in a group of the Workshop: alphabetical by title, in the
/// language of the book, "Note 2" before "Note 10"
QCollator workshopOrder(const core::BookProject& project) {
    const core::ProjectBook* book = project.books.isEmpty() ? nullptr : &project.books.first();
    QCollator collator(
        QLocale(book && !book->language.isEmpty() ? book->language : programLanguage()));
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    return collator;
}

}  // namespace

QString NavigatorPanel::iconIdOf(const core::BookTypeRegistry& registry,
                                 const core::ProjectElement& element,
                                 const core::PackageCard* card) {
    // A file of the book outside its text has no kind
    if (!element.hasKind()) {
        return BOOK_FILE_ICON;
    }
    const core::KindRef kind = core::BookProject::kindOf(registry, element);
    // A material shows where it comes from: a picture or another file, a web page, a picture
    // on the web, neither
    if (kind && kind.kind->isMaterial()) {
        switch (materialSourceOf(element)) {
        case MaterialSource::Page:
            return QStringLiteral("material.page");
        case MaterialSource::WebPicture:
            return QStringLiteral("material.webPicture");
        case MaterialSource::None:
            return QStringLiteral("material.noFile");
        case MaterialSource::File:
            if (QMimeDatabase().mimeTypeForFile(element.file, QMimeDatabase::MatchExtension)
                    .name()
                    .startsWith(QLatin1String("image/"))) {
                return QStringLiteral("common.image");
            }
            break;
        }
    }
    if (kind && !kind.kind->icon.isEmpty()) {
        return kind.kind->icon;
    }
    // A kind the program does not have: its icon on the book's card of its package
    const core::ElementKind* onCard =
        !kind && card != nullptr ? card->kind(element.kind.kindId) : nullptr;
    if (onCard != nullptr && !onCard->icon.isEmpty()) {
        return onCard->icon;
    }
    return formIconId(core::BookProject::formOf(registry, element));
}

QIcon NavigatorPanel::iconOf(const core::BookTypeRegistry& registry,
                             const core::ProjectElement& element, core::IconContext context) {
    const core::ProjectManager& pm = core::ProjectManager::getInstance();
    const core::PackageCard* card = element.hasKind() && !core::BookProject::kindOf(registry, element)
                                        ? pm.packageCard(element.kind.packageId)
                                        : nullptr;
    return core::ArtProvider::getInstance().getPackageIcon(iconIdOf(registry, element, card), card,
                                                           context);
}

QString NavigatorPanel::iconIdOf(const core::KindRef& kind) {
    if (!kind) {
        return formIconId(core::ElementForm::Text);
    }
    return kind.kind->icon.isEmpty() ? formIconId(kind.kind->form) : kind.kind->icon;
}

void NavigatorPanel::addElementItems(QTreeWidgetItem* parent,
                                     const QList<core::ProjectElement>& elements,
                                     const core::BookTypeRegistry& registry, bool workshop) {
    const core::ProjectManager& pm = core::ProjectManager::getInstance();

    for (const core::ProjectElement& element : elements) {
        const core::ElementForm form = core::BookProject::formOf(registry, element);
        // A kind the program does not have shows its icon on the book's card of its package
        const bool cardKind = element.hasKind() && !core::BookProject::kindOf(registry, element);
        const QString iconId = iconIdOf(
            registry, element, cardKind ? pm.packageCard(element.kind.packageId) : nullptr);

        // The elements of the Workshop are in alphabetical order: they are dragged only to the
        // links of a material
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

        // The elements of the Workshop have no status
        QTreeWidgetItem* item = new QTreeWidgetItem(parent);
        item->setText(0, getDisplayTitle(element, form == core::ElementForm::Text && !workshop,
                                         m_modifiedElements.contains(element.id)));
        item->setData(0, Qt::UserRole, element.id);
        item->setData(0, Qt::UserRole + 1, itemType);
        item->setData(0, WORKSHOP_ROLE, workshop);
        item->setData(0, ICON_ROLE, iconId);
        item->setData(0, KIND_ROLE, element.kind.toString());
        item->setData(0, TITLE_ROLE, element.title);
        if (cardKind) {
            item->setData(0, CARD_ROLE, element.kind.packageId);
        }
        item->setIcon(0, itemIcon(item, iconId));
        item->setFlags((item->flags() & ~Qt::ItemIsDragEnabled & ~Qt::ItemIsDropEnabled) | flags);

        if (form == core::ElementForm::Group) {
            addElementItems(item, element.elements, registry, workshop);
            item->setExpanded(true);  // Expand groups by default
        }
    }
}

void NavigatorPanel::addWorkshopItems(QTreeWidgetItem* workshopItem,
                                      const core::BookProject& project,
                                      const core::BookTypeRegistry& registry) {
    const core::ProjectManager& pm = core::ProjectManager::getInstance();
    const QString language = programLanguage();

    // A group for each kind of the Workshop, in the order of the book's type; then the kinds
    // the program does not have, of the elements that have them
    struct KindGroup {
        core::KindRef kind;
        QString key;      ///< The kind with its package: "kalahari.base:work_note"
        QString name;     ///< "Working notes"
        QString iconId;
        QString card;     ///< The package of a kind the program does not have
        core::WorkshopGroup shelf = core::WorkshopGroup::None;
        QList<core::ProjectElement> elements;
    };
    QList<KindGroup> groups;
    for (const core::KindRef& kind : project.kindsIn(registry, core::BookPlace::Workshop)) {
        groups.append({kind, kind.reference(), kind.kind->plural.text(language), iconIdOf(kind),
                       QString(), kind.kind->workshopGroup, {}});
    }
    for (const core::ProjectElement& element : project.workshop.elements) {
        const core::KindRef kind = core::BookProject::kindOf(registry, element);
        const QString key = kind ? kind.reference() : element.kind.toString();
        auto group = std::find_if(groups.begin(), groups.end(), [&](const KindGroup& known) {
            return kind ? known.kind.kind == kind.kind : !known.kind && known.key == key;
        });
        if (group == groups.end()) {
            const core::PackageCard* card = kind ? nullptr : pm.packageCard(element.kind.packageId);
            groups.append({kind, key,
                           core::fillWords(QStringLiteral("{Kind:plural}"),
                                           {{QStringLiteral("kind"),
                                             wordsOf(registry, element, card)}}),
                           iconIdOf(registry, element, card),
                           kind ? QString() : element.kind.packageId,
                           kind ? kind.kind->workshopGroup : core::WorkshopGroup::None, {}});
            group = std::prev(groups.end());
        }
        group->elements.append(element);
    }

    // The elements in alphabetical order of their titles
    const QCollator collator = workshopOrder(project);

    // The working notes and the materials are in Materials, and, grouped, the groups of cards
    // in Libraries, each where its first group would be
    QTreeWidgetItem* libraries = nullptr;
    QTreeWidgetItem* materials = nullptr;
    const auto parentOf = [&](core::WorkshopGroup shelf) -> QTreeWidgetItem* {
        const bool library = shelf == core::WorkshopGroup::Libraries;
        if (shelf == core::WorkshopGroup::None || (library && !project.workshop.grouped)) {
            return workshopItem;
        }
        QTreeWidgetItem*& item = library ? libraries : materials;
        if (!item) {
            item = new QTreeWidgetItem(workshopItem);
            if (library) {
                //: The group of the cards of the Workshop (characters, locations, items) when
                //: it is grouped. In Polish: Biblioteki
                item->setText(0, tr("Libraries"));
            } else {
                //: The node of the Workshop with the working notes and the materials (quotes,
                //: web pages, pictures...), in groups of their kinds. In Polish: Materiały
                item->setText(0, tr("Materials"));
            }
            item->setData(0, Qt::UserRole,
                          WORKSHOP_ID_PREFIX + (library ? QStringLiteral("libraries")
                                                        : QStringLiteral("materials")));
            item->setData(0, Qt::UserRole + 1, QStringLiteral("workshop_shelf"));
            const QString iconId = library ? QStringLiteral("structure.libraries")
                                           : QStringLiteral("structure.materials");
            item->setData(0, ICON_ROLE, iconId);
            item->setIcon(0, itemIcon(item, iconId));
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            item->setExpanded(true);
        }
        return item;
    };

    // A kind the program makes elements of has its group also empty, so that one can be added
    // to it; the other kinds when they have elements
    for (KindGroup& group : groups) {
        if (group.elements.isEmpty() && !core::ProjectManager::canAdd(group.kind)) {
            continue;
        }
        std::sort(group.elements.begin(), group.elements.end(),
                  [&collator](const core::ProjectElement& a, const core::ProjectElement& b) {
                      return collator.compare(a.title, b.title) < 0;
                  });
        auto* item = new QTreeWidgetItem(parentOf(group.shelf));
        item->setText(0, group.name);
        item->setData(0, Qt::UserRole, WORKSHOP_ID_PREFIX + group.key);
        item->setData(0, Qt::UserRole + 1, QStringLiteral("workshop_group"));
        item->setData(0, ICON_ROLE, group.iconId);
        item->setData(0, KIND_ROLE, group.key);
        if (!group.card.isEmpty()) {
            item->setData(0, CARD_ROLE, group.card);
        }
        item->setIcon(0, itemIcon(item, group.iconId));
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        addElementItems(item, group.elements, registry, true);
        item->setExpanded(true);
        if (item->parent() == materials) {
            setCount(item, group.elements.size());
        }
    }

    // Materials has the number of all of them
    if (materials) {
        int all = 0;
        for (int i = 0; i < materials->childCount(); ++i) {
            all += materials->child(i)->data(0, COUNT_ROLE).toInt();
        }
        setCount(materials, all);
    }
}

void NavigatorPanel::setCount(QTreeWidgetItem* item, qsizetype count) {
    item->setData(0, COUNT_ROLE, static_cast<int>(count));
    //: A node of the Navigator with its number of elements, as a screen reader reads it: %1
    //: the name, %2 the number. In Polish: %1 (%2)
    item->setData(0, Qt::AccessibleTextRole,
                  tr("%1 (%2)").arg(item->text(0), QLocale().toString(count)));
}

QString NavigatorPanel::bookName(const core::ProjectBook& book) {
    //: The node of the book in the Navigator, unless the writer names it. In Polish: Książka
    return book.name.isEmpty() ? tr("Book") : book.name;
}

QString NavigatorPanel::workshopName(const core::BookProject& project) {
    //: The node of the Workshop in the Navigator: the elements of the work on the book, which
    //: the finished book does not have, unless the writer names it. In Polish: Warsztat
    return project.workshop.name.isEmpty() ? tr("Workshop") : project.workshop.name;
}

bool NavigatorPanel::isWorkshopItem(const QTreeWidgetItem* item) {
    return item && item->data(0, WORKSHOP_ROLE).toBool();
}

bool NavigatorPanel::isMaterialItem(const QTreeWidgetItem* item) {
    if (!isWorkshopItem(item) || typeOf(item) != QLatin1String("window_element")) {
        return false;
    }
    const auto& pm = core::ProjectManager::getInstance();
    const core::ProjectElement* element = pm.findElement(item->data(0, Qt::UserRole).toString());
    return element && pm.isMaterial(*element);
}

QString NavigatorPanel::linkOf(const QTreeWidgetItem* item) {
    if (!item) {
        return {};
    }
    const QString type = typeOf(item);
    if (isElementType(type)) {
        return item->data(0, Qt::UserRole).toString();
    }
    const std::optional<core::BookPlace> section = sectionPlaceOf(type);
    const core::BookProject* project = core::ProjectManager::getInstance().project();
    return section && project ? project->sectionLink(*section) : QString();
}

QStringList NavigatorPanel::linksOf(const QMimeData* data) {
    if (!data || !data->hasFormat(QLatin1String(LINKS_MIME_TYPE))) {
        return {};
    }
    return QString::fromUtf8(data->data(QLatin1String(LINKS_MIME_TYPE)))
        .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
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
    m_addButton->setIcon(artProvider.getIcon("book.addElement", core::IconContext::Menu));
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
        item->setIcon(0, itemIcon(item, iconId));
    }

    // Recursively refresh children
    for (int i = 0; i < item->childCount(); ++i) {
        refreshItemIcons(item->child(i));
    }
}

QIcon NavigatorPanel::itemIcon(const QTreeWidgetItem* item, const QString& iconId) const {
    // The icon of a kind the program does not have is on the book's card of its package
    const QString package = item->data(0, CARD_ROLE).toString();
    const core::PackageCard* card =
        package.isEmpty() ? nullptr : core::ProjectManager::getInstance().packageCard(package);
    QIcon icon =
        core::ArtProvider::getInstance().getPackageIcon(iconId, card, core::IconContext::TreeView);
    // Grey: the files that are not in the book, and the whole book while it is read-only
    const QString type = typeOf(item);
    if (isUnassignedType(type) || (m_readOnly && isBookType(type))) {
        return greyIcon(icon, m_currentIconSize);
    }
    return icon;
}

void NavigatorPanel::setReadOnly(bool readOnly) {
    m_readOnly = readOnly;
    // Nothing is dragged in a book open read-only, and nothing is dropped on it
    m_treeWidget->setDragDropMode(readOnly ? QAbstractItemView::NoDragDrop
                                           : QAbstractItemView::InternalMove);
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        refreshItemIcons(m_treeWidget->topLevelItem(i));
    }
}

QString NavigatorPanel::getIconIdForType(const QString& elementType) const {
    // Map element types to icon IDs
    // Structure icons (OpenSpec #00034)
    if (elementType == "document") {
        return "project.book";
    } else if (elementType == "book") {
        return "structure.book";
    } else if (elementType == "workshop") {
        return "structure.workshop";
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
    } else if (elementType == "book_file") {
        return BOOK_FILE_ICON;
    } else if (elementType == "unassigned_files") {
        return "structure.unassigned";
    } else if (elementType == "book_files") {
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

    // A node of its own after the project, if one is open: the files do not belong to it
    m_otherFilesItem = new QTreeWidgetItem(m_treeWidget);
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
    // The commands that change the book: greyed out in a book open read-only, saying why
    QList<QAction*> changing;

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

        // From the start or the end of its list, Move Up and Move Down take the element out
        // of it: before or after its part, or to the section before or after. Right after the
        // title page, which nothing goes before, an element is at the start of its list.
        QList<core::ProjectElement> without = list ? *list : QList<core::ProjectElement>();
        if (index >= 0) {
            without.removeAt(index);
        }
        const core::KindRef kind = element ? pm.kindOf(*element) : core::KindRef{};
        const auto canStepTo = [&pm, &without, &kind](int to) {
            return core::BookProject::canBeAt(pm.bookTypes(), without, kind, to);
        };
        const bool upInList = index > 0 && canStepTo(index - 1);
        const bool downInList = index >= 0 && index < siblingCount - 1 && canStepTo(index + 1);
        const std::optional<core::ElementPlace> above =
            project && index >= 0 && !upInList
                ? placeBeyondList(*project, pm.book(), places, elementId, true)
                : std::nullopt;
        const std::optional<core::ElementPlace> below =
            project && index >= 0 && !downInList
                ? placeBeyondList(*project, pm.book(), places, elementId, false)
                : std::nullopt;

        QAction* moveUpAction = menu.addAction(
            artProvider.getIcon("navigation.up", core::IconContext::Menu),
            tr("Move &Up"));
        connect(moveUpAction, &QAction::triggered, this, [this, elementId, above]() {
            if (above) {
                emit elementMovedTo(elementId, *above);
            } else {
                onContextMenuMoveUp();
            }
        });
        moveUpAction->setEnabled(upInList || above);

        QAction* moveDownAction = menu.addAction(
            artProvider.getIcon("navigation.down", core::IconContext::Menu),
            tr("Move Do&wn"));
        connect(moveDownAction, &QAction::triggered, this, [this, elementId, below]() {
            if (below) {
                emit elementMovedTo(elementId, *below);
            } else {
                onContextMenuMoveDown();
            }
        });
        moveDownAction->setEnabled(downInList || below);

        QAction* moveToEndAction = menu.addAction(
            artProvider.getIcon("navigation.bottom", core::IconContext::Menu),
            tr("Move to E&nd"));
        connect(moveToEndAction, &QAction::triggered, this,
                [this, elementId, endIndex]() { emit elementMoved(elementId, endIndex); });
        moveToEndAction->setEnabled(index >= 0 && index < endIndex);

        // An element with no other place in the book (its only part): no command can move it,
        // and each says why
        if (knownKind && !elsewhere) {
            //: Why the Move commands cannot be used: the element's kind can be nowhere else in
            //: the book. In Polish: W tej książce nie ma innego miejsca na {kind:accusative}.
            const QString why =
                core::fillWords(tr("The book has no other place for the {kind}."),
                                {{QStringLiteral("kind"), wordsOf(*element)}});
            for (QAction* action : {moveToAction, moveToStartAction, moveUpAction, moveDownAction,
                                    moveToEndAction}) {
                action->setToolTip(why);
                action->setStatusTip(why);
            }
        }
        changing << moveToAction << moveToStartAction << moveUpAction << moveDownAction
                 << moveToEndAction;
    };

    // Add...: the Add Element window with the kinds of the node, in bold: the command most
    // used there. In variant A (ADD_COMMANDS_SETTING) also the kinds most added there, by name
    // ("Add Chapter..."), which open the window with the kind chosen.
    const auto addAddActions = [&]() {
        QAction* addAction = menu.addAction(
            artProvider.getIcon("book.addElement", core::IconContext::Menu),
            //: The command of a node of the Navigator: the Add Element window with the kinds
            //: of the node. In Polish: Dod&aj...
            withKeys(tr("&Add..."), addKeys()));
        QFont bold = addAction->font();
        bold.setBold(true);
        addAction->setFont(bold);
        addAction->setEnabled(addTargetOf(item).has_value());
        connect(addAction, &QAction::triggered, this, [this, item]() { addOn(item); });
        changing << addAction;
        for (const core::KindRef& kind : namedKindsOf(item)) {
            // "Add Story...", "Dodaj opowiadanie..."
            QAction* namedAction = menu.addAction(
                artProvider.getIcon(iconIdOf(kind), core::IconContext::Menu),
                core::fillWords(tr("Add {Kind}..."),
                                {{QStringLiteral("kind"), menuWords(wordsOf(kind))}}));
            connect(namedAction, &QAction::triggered, this,
                    [this, item, kind]() { addOn(item, kind); });
            changing << namedAction;
        }
    };

    const auto addRenameDeleteActions = [&]() {
        QAction* renameAction = menu.addAction(
            artProvider.getIcon("edit.rename", core::IconContext::Menu),
            withKeys(tr("Rena&me..."), QKeyCombination(Qt::Key_F2)));
        connect(renameAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuRename);

        QAction* deleteAction = menu.addAction(
            artProvider.getIcon("edit.delete", core::IconContext::Menu),
            withKeys(tr("&Delete..."), QKeyCombination(Qt::Key_Delete)));
        connect(deleteAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuDelete);
        changing << renameAction << deleteAction;
    };

    // Rename of a node that is no element: a section, the book, the Workshop
    const auto addRenameNodeAction = [&](const std::function<void()>& rename) {
        QAction* renameAction = menu.addAction(
            artProvider.getIcon("edit.rename", core::IconContext::Menu),
            withKeys(tr("Rena&me..."), QKeyCombination(Qt::Key_F2)));
        connect(renameAction, &QAction::triggered, this, rename);
        renameAction->setEnabled(projectOpen);
        changing << renameAction;
    };

    const auto addOpenAction = [&]() {
        QAction* openAction = menu.addAction(
            artProvider.getIcon("file.open", core::IconContext::Menu),
            withKeys(tr("&Open"), QKeyCombination(Qt::Key_Enter)));
        connect(openAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuOpen);
    };

    // The icon of the Properties panel, which Properties... brings to the front
    const auto addPropertiesAction = [&]() {
        QAction* propertiesAction = menu.addAction(
            artProvider.getIcon("view.properties", core::IconContext::Menu),
            withKeys(tr("P&roperties..."), propertiesKeys()));
        connect(propertiesAction, &QAction::triggered, this,
                &NavigatorPanel::onContextMenuProperties);
    };

    // The icons of the panel's Expand All and Collapse All buttons
    const auto addExpandCollapseActions = [&]() {
        QAction* expandAllAction = menu.addAction(
            artProvider.getIcon("common.expandMore", core::IconContext::Menu),
            tr("&Expand All"));
        connect(expandAllAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuExpandAll);

        QAction* collapseAllAction = menu.addAction(
            artProvider.getIcon("common.expandLess", core::IconContext::Menu),
            tr("&Collapse All"));
        connect(collapseAllAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuCollapseAll);
    };

    // Build menu based on element type
    if (elementType == "text_element" && isWorkshopItem(item)) {
        // A text of the Workshop (a working note) opens in the editor; no status and no moves:
        // the finished book does not have it, and its place is that of its title
        addOpenAction();
        addAddActions();
        menu.addSeparator();
        addRenameDeleteActions();
        menu.addSeparator();
        addPropertiesAction();

    } else if (elementType == "window_element" && isWorkshopItem(item)) {
        // A material opens as the settings say: its picture on a preview tab, its file in the
        // program the system gives it, its page in the browser; its card opens in the middle
        // of the window. The other kinds wait for the windows of the program.
        const core::ProjectElement* element = pm.findElement(elementId);
        const core::KindRef kind = element ? pm.kindOf(*element) : core::KindRef{};
        if (kind && kind.kind->isMaterial()) {
            QAction* openAction = menu.addAction(
                artProvider.getIcon("file.open", core::IconContext::Menu),
                withKeys(tr("&Open"), QKeyCombination(Qt::Key_Enter)));
            connect(openAction, &QAction::triggered, this,
                    [this, elementId]() { emit requestOpenMaterial(elementId); });
            //: Opens the card of a material in the middle of the window. In Polish: Otwórz
            //: &kartę
            QAction* cardAction = menu.addAction(
                artProvider.getIcon("material.noFile", core::IconContext::Menu),
                tr("Open &Card"));
            connect(cardAction, &QAction::triggered, this,
                    [this, elementId]() { emit requestOpenMaterialCard(elementId); });
            // A material without a file has no folder to show
            if (!element->file.isEmpty()) {
                const QString path = pm.filePathOf(*element);
                QAction* showAction = menu.addAction(
                    artProvider.getIcon("common.folder", core::IconContext::Menu),
                    tr("Show in &Folder"));
                connect(showAction, &QAction::triggered, this,
                        [this, path]() { showInFileManager(path); });
            }
        }
        addAddActions();
        menu.addSeparator();
        addRenameDeleteActions();
        menu.addSeparator();
        addPropertiesAction();

    } else if (elementType == "text_element") {
        // Text elements (chapters, front/back matter items)
        addOpenAction();
        addAddActions();
        menu.addSeparator();
        addRenameDeleteActions();
        menu.addSeparator();
        addMoveActions();
        menu.addSeparator();

        // Add "Set Status" submenu for text elements
        QMenu* statusMenu = menu.addMenu(tr("Set &Status"));
        changing << statusMenu->menuAction();

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
        addPropertiesAction();

    } else if (elementType == "book_file") {
        // A file of the book outside its text: no place in the text and no status
        addOpenAction();
        menu.addSeparator();
        addRenameDeleteActions();
        menu.addSeparator();
        addPropertiesAction();

    } else if (elementType == "group_element") {
        // Group (part): the main texts of the book (chapters, stories...) are added inside it
        addAddActions();
        menu.addSeparator();
        addRenameDeleteActions();
        menu.addSeparator();
        addMoveActions();
        menu.addSeparator();
        // The statistics of the group, in the Properties panel brought to the front
        QAction* propertiesAction = menu.addAction(
            artProvider.getIcon("view.properties", core::IconContext::Menu),
            withKeys(tr("P&roperties..."), propertiesKeys()));
        connect(propertiesAction, &QAction::triggered, this, [this, elementId]() {
            emit requestPartProperties(elementId);
            emit requestPropertiesPanel();
        });
        menu.addSeparator();
        addExpandCollapseActions();

    } else if (elementType == "window_element") {
        // Element that opens in a window of its own; the program has none of them yet
        addAddActions();
        menu.addSeparator();
        addRenameDeleteActions();
        menu.addSeparator();
        addMoveActions();

    } else if (sectionPlaceOf(elementType)) {
        // Section (Front Matter, Body, Back Matter), which the writer can name
        addAddActions();
        menu.addSeparator();
        addRenameNodeAction([this, elementType]() { emit requestRenameSection(elementType); });
        menu.addSeparator();
        addExpandCollapseActions();

    } else if (elementType == "book") {
        // The book: the kinds of all its sections, also when it shows none
        const core::ProjectBook* book = pm.book();
        const bool sectionsShown = !book || book->partsLayer;
        addAddActions();
        menu.addSeparator();
        addRenameNodeAction([this]() { emit requestRenameNode(QStringLiteral("book")); });
        if (projectOpen) {
            // Showing or hiding the sections moves no element
            QAction* showSectionsAction = menu.addAction(tr("&Show Sections"));
            showSectionsAction->setCheckable(true);
            showSectionsAction->setChecked(sectionsShown);
            connect(showSectionsAction, &QAction::toggled, this,
                    [this](bool shown) { emit requestShowSections(shown); });
            changing << showSectionsAction;
        }
        menu.addSeparator();

        // The icon of Book > Book Properties...
        QAction* propertiesAction = menu.addAction(
            artProvider.getIcon("book.properties", core::IconContext::Menu),
            tr("Book P&roperties..."));
        connect(propertiesAction, &QAction::triggered, this, &NavigatorPanel::onContextMenuProperties);
        // As Book > Book Properties..., which edits them
        changing << propertiesAction;
        menu.addSeparator();
        addExpandCollapseActions();

    } else if (elementType == "document") {
        // The project: its window comes with the types of books (stage 4); a click on the node
        // shows the properties of the book. Add... starts at the book when the Add Element
        // window chooses where it adds.
        if (addTargetOf(item)) {
            addAddActions();
            menu.addSeparator();
        }
        QAction* propertiesAction = menu.addAction(
            artProvider.getIcon("view.properties", core::IconContext::Menu),
            //: The command of the project's node in the Navigator. In Polish: Właściwości
            //: &projektu...
            tr("Project P&roperties..."));
        propertiesAction->setEnabled(false);
        //: Why Project Properties... is grey. In Polish: Właściwości projektu (w przygotowaniu)
        const QString why = tr("Project Properties (in preparation)");
        propertiesAction->setToolTip(why);
        propertiesAction->setStatusTip(why);
        menu.addSeparator();
        addExpandCollapseActions();

    } else if (elementType == "workshop") {
        // The Workshop: its kinds, its name, and its groups of cards and materials
        addAddActions();
        menu.addSeparator();
        addRenameNodeAction([this]() { emit requestRenameNode(QStringLiteral("workshop")); });
        if (const core::BookProject* project = pm.project()) {
            //: Whether the Workshop has its cards (characters, locations, items) in
            //: Libraries. In Polish: &Grupowanie
            QAction* groupedAction = menu.addAction(tr("&Grouping"));
            groupedAction->setCheckable(true);
            groupedAction->setChecked(project->workshop.grouped);
            connect(groupedAction, &QAction::toggled, this,
                    [this](bool grouped) { emit requestWorkshopGrouped(grouped); });
            changing << groupedAction;
        }
        menu.addSeparator();
        addExpandCollapseActions();

    } else if (elementType == "workshop_shelf" || elementType == "workshop_group") {
        // A group of the Workshop is named after its kind, and is there as long as the kind
        addAddActions();
        menu.addSeparator();
        addExpandCollapseActions();

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
        changing << addToProjectAction;

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
        changing << addAction << deleteAction;

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
        changing << deleteAllAction;
        menu.addSeparator();
        addExpandCollapseActions();

    } else if (elementType == "other_files" || elementType == "book_files") {
        // The headers of the files outside the book and of the other files of the book - only
        // expand/collapse
        addExpandCollapseActions();

    } else {
        // Unknown type - no menu
        logger.debug("NavigatorPanel: No context menu for type: {}", elementType.toStdString());
        m_contextMenuItem = nullptr;
        return;
    }

    if (m_readOnly) {
        const QString why = readOnlyReason();
        for (QAction* action : std::as_const(changing)) {
            action->setEnabled(false);
            action->setToolTip(why);
            action->setStatusTip(why);
        }
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
    updateAddButton();

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

    // As a click; a material chosen with the keys brings its card to the front, one chosen
    // by the program (a book opened, an element shown) does not
    if (requestPropertiesOf(current) && isMaterialItem(current) && m_treeWidget->hasFocus()) {
        emit requestPropertiesPanel();
    }
}

bool NavigatorPanel::requestPropertiesOf(const QTreeWidgetItem* item) {
    if (!item) {
        return false;
    }
    const QString elementId = item->data(0, Qt::UserRole).toString();
    const QString elementType = typeOf(item);
    // The project and the book show the properties of the book
    // Sections and groups emit with their type for aggregate statistics
    if (elementType == "document" || elementType == "book") {
        emit requestProperties("");  // Book properties
    } else if (sectionPlaceOf(elementType)) {
        emit requestSectionProperties(elementType);
    } else if (elementType == "group_element") {
        emit requestPartProperties(elementId);
    } else if (elementType == "text_element" || elementType == "book_file" ||
               (elementType == "window_element" && isWorkshopItem(item))) {
        emit requestProperties(elementId);
    } else {
        return false;
    }
    return true;
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
    // window elements do not. A material of the Workshop opens as the settings say
    // (NavigatorCoordinator::onRequestOpenMaterial()).
    if (elementType == "text_element" || elementType == "book_file") {
        emit elementSelected(elementId, item->data(0, TITLE_ROLE).toString());
    } else if (elementType == "window_element" && isWorkshopItem(item)) {
        const auto& pm = core::ProjectManager::getInstance();
        const core::ProjectElement* element = pm.findElement(elementId);
        const core::KindRef kind = element ? pm.kindOf(*element) : core::KindRef{};
        if (kind && kind.kind->isMaterial()) {
            emit requestOpenMaterial(elementId);
        }
    }
}

// =============================================================================
// Add..., Rename and Delete on a node
// =============================================================================

QString NavigatorPanel::quickMaterials() {
    const QString value = QString::fromStdString(
        core::SettingsManager::getInstance().get<std::string>(QUICK_MATERIALS_SETTING, "link"));
    return value == QLatin1String("add") || value == QLatin1String("window")
               ? value
               : QStringLiteral("link");
}

AddChooser NavigatorPanel::addChooser() {
    const std::string chooser =
        core::SettingsManager::getInstance().get<std::string>(ADD_WINDOW_SETTING, "sections");
    if (chooser == "book") {
        return AddChooser::Book;
    }
    return chooser == "place" ? AddChooser::None : AddChooser::Sections;
}

std::optional<AddTarget> NavigatorPanel::addTargetOf(const QTreeWidgetItem* item) const {
    const auto& pm = core::ProjectManager::getInstance();
    const core::BookProject* project = pm.project();
    if (!project) {
        return std::nullopt;
    }
    // The project and no node: the book, when the window chooses where it adds
    if (!item || typeOf(item) == QLatin1String("document")) {
        return addChooser() == AddChooser::None ? std::nullopt
                                                : std::optional<AddTarget>(AddTarget{});
    }
    const QString type = typeOf(item);
    const QString id = item->data(0, Qt::UserRole).toString();
    const core::BookTypeRegistry& registry = pm.bookTypes();
    AddTarget target;

    // The Workshop: all its kinds; a group of it or an element of it: its kind; a group of
    // groups (Libraries, Materials): the kinds of its groups the program makes elements of
    if (isWorkshopNode(type) || isWorkshopItem(item)) {
        target.scope = AddTarget::Scope::Workshop;
        // The kind of a group of the Workshop, or of an element of it
        const auto kindOfItem = [&registry, &pm](const QTreeWidgetItem* node) {
            if (isWorkshopItem(node)) {
                const core::ProjectElement* element =
                    pm.findElement(node->data(0, Qt::UserRole).toString());
                return element ? pm.kindOf(*element) : core::KindRef{};
            }
            const std::optional<core::KindReference> reference =
                core::KindReference::parse(node->data(0, KIND_ROLE).toString());
            return reference ? registry.findKind(reference->packageId, reference->kindId)
                             : core::KindRef{};
        };
        if (type == QLatin1String("workshop_shelf")) {
            for (int i = 0; i < item->childCount(); ++i) {
                if (const core::KindRef kind = kindOfItem(item->child(i));
                    core::ProjectManager::canAdd(kind)) {
                    target.kinds.append(kind);
                }
            }
        } else if (type != QLatin1String("workshop")) {
            const core::KindRef kind = kindOfItem(item);
            if (!kind) {
                return std::nullopt;  // A kind the program does not have
            }
            target.kinds.append(kind);
            target.kind = kind;
        }
        return target;
    }

    if (type == QLatin1String("book")) {
        target.scope = AddTarget::Scope::Book;
        return target;
    }
    if (const std::optional<core::BookPlace> place = sectionPlaceOf(type)) {
        target.scope = AddTarget::Scope::Section;
        target.place = *place;
        return target;
    }
    if (type == QLatin1String("group_element")) {
        target.scope = AddTarget::Scope::Group;
        target.id = id;
        return target;
    }
    if (type == QLatin1String("text_element") || type == QLatin1String("window_element")) {
        target.scope = AddTarget::Scope::Element;
        target.id = id;
        return target;
    }
    return std::nullopt;
}

QList<core::KindRef> NavigatorPanel::namedKindsOf(const QTreeWidgetItem* item) const {
    QList<core::KindRef> kinds;
    const auto& settings = core::SettingsManager::getInstance();
    const std::optional<AddTarget> target = addTargetOf(item);
    if (!target || settings.get<std::string>(ADD_COMMANDS_SETTING, "named") == "single") {
        return kinds;
    }
    const auto& pm = core::ProjectManager::getInstance();
    const core::BookProject* project = pm.project();

    // A kind the program can add, below its limit there
    const auto offered = [](const QList<core::KindRef>& available, const core::KindRef& kind) {
        return kind && core::ProjectManager::canAdd(kind) &&
               std::any_of(available.cbegin(), available.cend(),
                           [&kind](const core::KindRef& other) { return other.kind == kind.kind; });
    };
    const auto append = [&kinds](const core::KindRef& kind) {
        if (kind && std::none_of(kinds.cbegin(), kinds.cend(), [&kind](const core::KindRef& other) {
                return other.kind == kind.kind;
            })) {
            kinds.append(kind);
        }
    };

    switch (target->scope) {
    case AddTarget::Scope::Workshop: {
        // The kinds of the node the program makes elements of; a node of several kinds (the
        // Workshop, Materials) leaves the kinds of materials, many of the same weight, to the
        // window, as the front and back sections do
        const QList<core::KindRef> available = pm.kindsFor(core::BookPlace::Workshop);
        const QList<core::KindRef> all =
            target->kinds.isEmpty() && project
                ? project->kindsIn(pm.bookTypes(), core::BookPlace::Workshop)
                : target->kinds;
        const bool several = all.size() > 1;
        for (const core::KindRef& kind : all) {
            if (offered(available, kind) && !(several && kind.kind->isMaterial())) {
                append(kind);
            }
        }
        break;
    }
    case AddTarget::Scope::Book:
        // The main text (a chapter) and the group (a part) of the book
        append(pm.chapterKindFor(core::BookPlace::Main));
        append(pm.partKind());
        break;
    case AddTarget::Scope::Section:
        // The main section: a part and a chapter; the front and back sections have many kinds
        // of the same weight, so the window shows them
        if (target->place == core::BookPlace::Main) {
            append(pm.partKind());
            append(pm.chapterKindFor(core::BookPlace::Main));
        }
        break;
    case AddTarget::Scope::Group:
        append(pm.chapterKindFor(core::BookPlace::Main, target->id));
        break;
    case AddTarget::Scope::Element: {
        // Its own kind, after it; else the main text of the book where its list has it (a
        // chapter after the prologue)
        const std::optional<core::ElementPlace> at =
            project ? project->placeOf(target->id) : std::nullopt;
        const core::ProjectElement* element = pm.findElement(target->id);
        if (!at || !element) {
            break;
        }
        const QList<core::KindRef> available = pm.kindsFor(at->place, at->groupId);
        if (const core::KindRef own = pm.kindOf(*element); offered(available, own)) {
            append(own);
        } else if (offered(available, pm.mainTextKind())) {
            append(pm.mainTextKind());
        }
        break;
    }
    }
    return kinds;
}

void NavigatorPanel::addOn(const QTreeWidgetItem* item, const core::KindRef& kind) {
    std::optional<AddTarget> target = addTargetOf(item);
    if (!target || !changeRefusal().isEmpty()) {
        return;
    }
    if (kind) {
        target->kind = kind;
    }
    emit requestAdd(*target);
}

void NavigatorPanel::renameItem(const QTreeWidgetItem* item) {
    if (!item || !changeRefusal().isEmpty()) {
        return;
    }
    const QString type = typeOf(item);
    if (isElementType(type) || type == QLatin1String("book_file")) {
        emit requestRename(item->data(0, Qt::UserRole).toString(),
                           item->data(0, TITLE_ROLE).toString());
    } else if (sectionPlaceOf(type)) {
        emit requestRenameSection(type);
    } else if (type == QLatin1String("book") || type == QLatin1String("workshop")) {
        emit requestRenameNode(type);
    }
}

void NavigatorPanel::deleteItem(const QTreeWidgetItem* item) {
    if (!item || !changeRefusal().isEmpty()) {
        return;
    }
    const QString type = typeOf(item);
    const QString id = item->data(0, Qt::UserRole).toString();
    if (isElementType(type) || type == QLatin1String("book_file")) {
        emit requestDelete(id);
    } else if (type == QLatin1String("unassigned_file")) {
        emit requestDeleteUnassigned(id);
    }
}

QString NavigatorPanel::changeRefusal() const {
    if (!core::ProjectManager::getInstance().isProjectOpen()) {
        return tr("No book is open");
    }
    return m_readOnly ? readOnlyReason() : QString();
}

void NavigatorPanel::onSettingsChanged() {
    updateAddButton();
}

void NavigatorPanel::updateAddButton() {
    if (!m_addButton) {
        return;
    }
    const QString refusal = changeRefusal();
    const bool canAdd = refusal.isEmpty() && addTargetOf(m_treeWidget->currentItem()).has_value();
    m_addButton->setEnabled(canAdd);
    // A book open read-only says why nothing can be added
    const QString tip = tr("Add... (%1)").arg(ShortcutRules::keysText(addKeys()));
    m_addButton->setToolTip(m_readOnly ? tip + QLatin1Char('\n') + refusal : tip);
}

bool NavigatorPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched != m_treeWidget ||
        (event->type() != QEvent::ShortcutOverride && event->type() != QEvent::KeyPress)) {
        return QWidget::eventFilter(watched, event);
    }
    // The keys of the current node; the same keys of the window's commands (Delete of the
    // Edit menu) wait while the tree has the keys
    auto* key = static_cast<QKeyEvent*>(event);
    const QKeyCombination keys = ShortcutRules::keysOf(key);
    const Qt::KeyboardModifiers modifiers = keys.keyboardModifiers();
    const bool enter = keys.key() == Qt::Key_Return || keys.key() == Qt::Key_Enter;
    const bool add = (keys.key() == Qt::Key_Insert && modifiers == Qt::NoModifier) ||
                     (enter && modifiers == (Qt::ControlModifier | Qt::ShiftModifier));
    const bool rename = keys.key() == Qt::Key_F2 && modifiers == Qt::NoModifier;
    const bool remove = keys.key() == Qt::Key_Delete && modifiers == Qt::NoModifier;
    const bool properties = enter && modifiers == Qt::AltModifier;
    if (!add && !rename && !remove && !properties) {
        return QWidget::eventFilter(watched, event);
    }
    if (event->type() == QEvent::ShortcutOverride) {
        key->accept();
        return true;
    }
    QTreeWidgetItem* item = m_treeWidget->currentItem();
    if (properties) {
        if (requestPropertiesOf(item)) {
            emit requestPropertiesPanel();
            emit requestPropertiesFocus();
        }
    } else if (add) {
        addOn(item);
    } else if (rename) {
        renameItem(item);
    } else {
        deleteItem(item);
    }
    return true;
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
    // Pass the modified state so the "*" indicator is re-applied on every refresh. The
    // elements of the Workshop have no status.
    item->setText(0, getDisplayTitle(*element,
                                     pm.formOf(*element) == core::ElementForm::Text &&
                                         !isWorkshopItem(item),
                                     m_modifiedElements.contains(elementId)));
    item->setData(0, TITLE_ROLE, element->title);
    if (item->parent() && typeOf(item->parent()) == QLatin1String("workshop_group")) {
        placeInWorkshopGroup(item);
    }

    logger.debug("NavigatorPanel: Refreshed item text to: {}", item->text(0).toStdString());
}

void NavigatorPanel::placeInWorkshopGroup(QTreeWidgetItem* item) {
    const core::BookProject* project = core::ProjectManager::getInstance().project();
    QTreeWidgetItem* group = item->parent();
    if (!project || !group) {
        return;
    }
    const QCollator order = workshopOrder(*project);
    const QString title = item->data(0, TITLE_ROLE).toString();
    const auto titleAt = [group](int index) {
        return group->child(index)->data(0, TITLE_ROLE).toString();
    };
    const int at = group->indexOfChild(item);
    const bool afterPrevious = at == 0 || order.compare(titleAt(at - 1), title) <= 0;
    const bool beforeNext =
        at == group->childCount() - 1 || order.compare(title, titleAt(at + 1)) <= 0;
    if (afterPrevious && beforeNext) {
        return;
    }
    // Its place: after the others whose titles do not come after its own
    int place = 0;
    for (int i = 0; i < group->childCount(); ++i) {
        if (i != at && order.compare(titleAt(i), title) <= 0) {
            ++place;
        }
    }

    // The tree keeps its current item, the items open under the element and its scroll; the
    // move asks for no properties, so the Properties panel keeps what is typed in it
    const bool current = m_treeWidget->currentItem() == item;
    QStringList expanded;
    collectExpandedIds(item, expanded);
    const int scrolled = m_treeWidget->verticalScrollBar()->value();
    {
        const QSignalBlocker blocker(m_treeWidget);
        group->takeChild(at);
        group->insertChild(place, item);
        if (current) {
            m_treeWidget->setCurrentItem(item);
        }
    }
    expandItemsById(expanded);
    m_treeWidget->verticalScrollBar()->setValue(scrolled);
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
    // The list knows the nodes of the book and of the Workshop (setExpandedItemIds())
    if (m_treeWidget->topLevelItemCount() > 0) {
        expandedIds.append(WORKSHOP_LAYOUT);
    }
    return expandedIds;
}

void NavigatorPanel::setExpandedItemIds(const QStringList& ids) {
    // Items expanded by default (document, body, parts) must close too,
    // otherwise a collapsed part would reopen on every load
    m_treeWidget->collapseAll();
    expandItemsById(ids);

    // A list saved before the book and the Workshop had nodes of their own opens them, and
    // the groups of the Workshop, as a new book shows them
    if (!ids.contains(WORKSHOP_LAYOUT)) {
        const std::function<void(QTreeWidgetItem*)> open = [&open](QTreeWidgetItem* item) {
            const QString type = typeOf(item);
            if (type == QLatin1String("book") || isWorkshopNode(type)) {
                item->setExpanded(true);
            }
            for (int i = 0; i < item->childCount(); ++i) {
                open(item->child(i));
            }
        };
        for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
            open(m_treeWidget->topLevelItem(i));
        }
    }
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

void NavigatorPanel::revealSection(core::BookPlace place) {
    for (QTreeWidgetItemIterator it(m_treeWidget); *it; ++it) {
        const std::optional<core::BookPlace> section = sectionPlaceOf(typeOf(*it));
        if (!section || *section != place) {
            continue;
        }
        for (QTreeWidgetItem* parent = (*it)->parent(); parent; parent = parent->parent()) {
            parent->setExpanded(true);
        }
        m_treeWidget->setCurrentItem(*it);
        m_treeWidget->scrollToItem(*it, QAbstractItemView::EnsureVisible);
        return;
    }
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

    // Check if this item matches. The tree has one document, one book, one of each section,
    // one Workshop and one "Other Files"; their names change (a renamed book or section), so
    // the type finds them.
    QString itemType = parent->data(0, Qt::UserRole + 1).toString();
    const bool single = itemType == QLatin1String("document") ||
                        itemType == QLatin1String("book") ||
                        itemType == QLatin1String("workshop") ||
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
