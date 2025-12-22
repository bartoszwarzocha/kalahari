/// @file quick_insert_popup.cpp
/// @brief Quick insert popup implementation (OpenSpec #00042 Tasks 7.11-7.12)

#include <kalahari/editor/quick_insert_popup.h>
#include <kalahari/core/logger.h>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QScrollBar>
#include <QVBoxLayout>

namespace kalahari::editor {

// =============================================================================
// Construction
// =============================================================================

QuickInsertPopup::QuickInsertPopup(QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
    , m_layout(nullptr)
    , m_listWidget(nullptr)
    , m_emptyLabel(nullptr)
{
    setupUI();
    stylePopup();

    core::Logger::getInstance().debug("QuickInsertPopup created");
}

// =============================================================================
// Setup
// =============================================================================

void QuickInsertPopup::setupUI()
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(2, 2, 2, 2);
    m_layout->setSpacing(0);

    // Create list widget
    m_listWidget = new QListWidget(this);
    m_listWidget->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listWidget->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_listWidget->setFocusPolicy(Qt::NoFocus);  // We handle focus ourselves
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);

    connect(m_listWidget, &QListWidget::itemClicked,
            this, &QuickInsertPopup::onItemClicked);
    connect(m_listWidget, &QListWidget::itemDoubleClicked,
            this, &QuickInsertPopup::onItemDoubleClicked);

    m_layout->addWidget(m_listWidget);

    // Create empty label (shown when no items match)
    m_emptyLabel = new QLabel(tr("No matches"), this);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet("color: gray; padding: 8px;");
    m_emptyLabel->hide();
    m_layout->addWidget(m_emptyLabel);

    setLayout(m_layout);
}

void QuickInsertPopup::stylePopup()
{
    // Style the popup with a clean, modern look
    setStyleSheet(
        "QuickInsertPopup {"
        "  background-color: palette(base);"
        "  border: 1px solid palette(mid);"
        "  border-radius: 4px;"
        "}"
        "QListWidget {"
        "  background-color: transparent;"
        "  border: none;"
        "  outline: none;"
        "}"
        "QListWidget::item {"
        "  padding: 4px 8px;"
        "  border-radius: 2px;"
        "}"
        "QListWidget::item:selected {"
        "  background-color: palette(highlight);"
        "  color: palette(highlighted-text);"
        "}"
        "QListWidget::item:hover:!selected {"
        "  background-color: palette(alternateBase);"
        "}"
    );
}

// =============================================================================
// Content
// =============================================================================

void QuickInsertPopup::setItems(const QList<QuickInsertItem>& items)
{
    m_items = items;
    m_listWidget->clear();

    if (items.isEmpty()) {
        m_listWidget->hide();
        m_emptyLabel->show();
    } else {
        m_emptyLabel->hide();
        m_listWidget->show();

        for (const auto& item : items) {
            QListWidgetItem* listItem = createListItem(item);
            m_listWidget->addItem(listItem);
        }

        // Select first item by default
        if (m_listWidget->count() > 0) {
            m_listWidget->setCurrentRow(0);
        }
    }

    updateSize();
}

void QuickInsertPopup::setFilter(const QString& filter)
{
    m_filter = filter;

    // Could highlight matching text in items here
    // For now, just store the filter
}

QuickInsertItem QuickInsertPopup::selectedItem() const
{
    QListWidgetItem* item = m_listWidget->currentItem();
    if (item == nullptr) {
        return QuickInsertItem();
    }

    // Retrieve stored data
    int index = item->data(ItemDataRole).toInt();
    if (index >= 0 && index < m_items.size()) {
        return m_items[index];
    }

    return QuickInsertItem();
}

int QuickInsertPopup::itemCount() const
{
    return m_listWidget->count();
}

bool QuickInsertPopup::hasItems() const
{
    return m_listWidget->count() > 0;
}

// =============================================================================
// Display
// =============================================================================

void QuickInsertPopup::showAt(const QPoint& globalPos)
{
    move(globalPos);
    show();
    raise();
    activateWindow();
}

void QuickInsertPopup::setMaxVisibleItems(int count)
{
    m_maxVisibleItems = qMax(1, count);
    updateSize();
}

void QuickInsertPopup::setMinimumPopupWidth(int width)
{
    setMinimumWidth(width);
}

void QuickInsertPopup::updateSize()
{
    // Calculate appropriate height based on item count
    int itemCount = qMin(m_listWidget->count(), m_maxVisibleItems);

    if (itemCount == 0) {
        // Show empty label
        setFixedHeight(40);
        setMinimumWidth(150);
    } else {
        // Calculate height based on items
        int height = itemCount * m_itemHeight + 8;  // 8 for margins

        // Add scrollbar width if needed
        if (m_listWidget->count() > m_maxVisibleItems) {
            int scrollWidth = m_listWidget->verticalScrollBar()->width();
            setMinimumWidth(200 + scrollWidth);
        }

        setFixedHeight(height);
        setMinimumWidth(200);
    }

    adjustSize();
}

// =============================================================================
// Navigation
// =============================================================================

void QuickInsertPopup::selectNext()
{
    int current = m_listWidget->currentRow();
    int count = m_listWidget->count();

    if (count == 0) {
        return;
    }

    int next = (current + 1) % count;
    m_listWidget->setCurrentRow(next);
}

void QuickInsertPopup::selectPrevious()
{
    int current = m_listWidget->currentRow();
    int count = m_listWidget->count();

    if (count == 0) {
        return;
    }

    int prev = (current - 1 + count) % count;
    m_listWidget->setCurrentRow(prev);
}

void QuickInsertPopup::acceptSelection()
{
    QuickInsertItem item = selectedItem();
    if (!item.id.isEmpty()) {
        emit itemSelected(item);
        hide();
    }
}

// =============================================================================
// Event Handlers
// =============================================================================

void QuickInsertPopup::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
        case Qt::Key_Up:
            selectPrevious();
            event->accept();
            break;

        case Qt::Key_Down:
            selectNext();
            event->accept();
            break;

        case Qt::Key_Return:
        case Qt::Key_Enter:
            acceptSelection();
            event->accept();
            break;

        case Qt::Key_Escape:
            hide();
            emit cancelled();
            event->accept();
            break;

        case Qt::Key_Tab:
            acceptSelection();
            event->accept();
            break;

        default:
            // Let parent handle other keys (for continued typing)
            QWidget::keyPressEvent(event);
            break;
    }
}

void QuickInsertPopup::focusOutEvent(QFocusEvent* event)
{
    Q_UNUSED(event)

    // Don't hide immediately - this causes issues with click handling
    // The popup will hide when user clicks elsewhere due to Qt::Popup flag
}

void QuickInsertPopup::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    // Ensure first item is selected
    if (m_listWidget->count() > 0 && m_listWidget->currentRow() < 0) {
        m_listWidget->setCurrentRow(0);
    }
}

// =============================================================================
// Private Slots
// =============================================================================

void QuickInsertPopup::onItemClicked(QListWidgetItem* item)
{
    if (item != nullptr) {
        // Selection already set by click
        // Don't accept yet - wait for double-click or Enter
    }
}

void QuickInsertPopup::onItemDoubleClicked(QListWidgetItem* item)
{
    Q_UNUSED(item)
    acceptSelection();
}

// =============================================================================
// Private Methods
// =============================================================================

QListWidgetItem* QuickInsertPopup::createListItem(const QuickInsertItem& item)
{
    QListWidgetItem* listItem = new QListWidgetItem();

    // Format display text
    QString displayText = item.name;
    if (!item.description.isEmpty()) {
        displayText += QString(" - %1").arg(item.description);
    }

    listItem->setText(displayText);

    // Store index in the original list for retrieval
    int index = m_items.indexOf(item);
    listItem->setData(ItemDataRole, index);

    // Set icon based on type
    QString iconPrefix;
    switch (item.type) {
        case QuickInsertType::Character:
            iconPrefix = "@";
            break;
        case QuickInsertType::Location:
            iconPrefix = "#";
            break;
    }

    // Could set an icon here using ArtProvider

    return listItem;
}

}  // namespace kalahari::editor
