/// @file quick_insert_popup.h
/// @brief Autocomplete popup for quick insert (OpenSpec #00042 Tasks 7.11-7.12)
///
/// QuickInsertPopup displays a list of items that can be inserted using
/// @ or # triggers. It provides keyboard navigation and filtering.

#pragma once

#include <kalahari/editor/quick_insert_handler.h>
#include <QWidget>
#include <QString>
#include <QList>

class QListWidget;
class QListWidgetItem;
class QLabel;
class QVBoxLayout;

namespace kalahari::editor {

/// @brief Autocomplete popup widget for quick insert
///
/// Shows a dropdown list of items matching the user's input.
/// Supports keyboard navigation (arrow keys, Enter, Escape).
///
/// Usage:
/// @code
/// auto popup = new QuickInsertPopup(parentWidget);
///
/// // Connect to handler
/// connect(handler, &QuickInsertHandler::triggered,
///         [popup](QuickInsertType type, const QPoint& pos) {
///             popup->setItems(handler->filteredItems());
///             popup->show(pos);
///         });
///
/// connect(popup, &QuickInsertPopup::itemSelected,
///         [handler](const QuickInsertItem& item) {
///             handler->insertItem(item);
///         });
/// @endcode
class QuickInsertPopup : public QWidget {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent widget
    explicit QuickInsertPopup(QWidget* parent = nullptr);

    /// @brief Destructor
    ~QuickInsertPopup() override = default;

    // Non-copyable
    QuickInsertPopup(const QuickInsertPopup&) = delete;
    QuickInsertPopup& operator=(const QuickInsertPopup&) = delete;

    // =========================================================================
    // Content
    // =========================================================================

    /// @brief Set items to display in the popup
    /// @param items List of items to show
    void setItems(const QList<QuickInsertItem>& items);

    /// @brief Update filter and refresh item highlighting
    /// @param filter Current filter string
    void setFilter(const QString& filter);

    /// @brief Get currently selected item
    /// @return The selected item, or empty item if none selected
    QuickInsertItem selectedItem() const;

    /// @brief Get number of items in the list
    /// @return Item count
    int itemCount() const;

    /// @brief Check if there are any items
    /// @return true if popup has items to show
    bool hasItems() const;

    // =========================================================================
    // Display
    // =========================================================================

    /// @brief Show popup at the specified position
    /// @param globalPos Position in global (screen) coordinates
    void showAt(const QPoint& globalPos);

    /// @brief Set the maximum number of visible items
    /// @param count Maximum visible items (default 8)
    void setMaxVisibleItems(int count);

    /// @brief Get maximum visible items
    /// @return Maximum visible items
    int maxVisibleItems() const { return m_maxVisibleItems; }

    /// @brief Set the minimum width of the popup
    /// @param width Minimum width in pixels
    void setMinimumPopupWidth(int width);

    // =========================================================================
    // Navigation
    // =========================================================================

    /// @brief Select the next item in the list
    void selectNext();

    /// @brief Select the previous item in the list
    void selectPrevious();

    /// @brief Accept the current selection
    void acceptSelection();

signals:
    /// @brief Emitted when an item is selected (Enter or click)
    /// @param item The selected item
    void itemSelected(const QuickInsertItem& item);

    /// @brief Emitted when popup is cancelled (Escape or click outside)
    void cancelled();

protected:
    /// @brief Handle key press events
    /// @param event The key event
    void keyPressEvent(QKeyEvent* event) override;

    /// @brief Handle focus out events
    /// @param event The focus event
    void focusOutEvent(QFocusEvent* event) override;

    /// @brief Handle show events
    /// @param event The show event
    void showEvent(QShowEvent* event) override;

private slots:
    /// @brief Handle item click
    /// @param item The clicked item
    void onItemClicked(QListWidgetItem* item);

    /// @brief Handle item double-click
    /// @param item The double-clicked item
    void onItemDoubleClicked(QListWidgetItem* item);

private:
    /// @brief Setup UI components
    void setupUI();

    /// @brief Update popup size based on content
    void updateSize();

    /// @brief Create list item for a QuickInsertItem
    /// @param item The data item
    /// @return The created list widget item
    QListWidgetItem* createListItem(const QuickInsertItem& item);

    /// @brief Style the popup appearance
    void stylePopup();

    QVBoxLayout* m_layout;              ///< Main layout
    QListWidget* m_listWidget;          ///< List of items
    QLabel* m_emptyLabel;               ///< Label shown when no items
    QString m_filter;                   ///< Current filter string

    QList<QuickInsertItem> m_items;     ///< Current items
    int m_maxVisibleItems{8};           ///< Maximum visible items
    int m_itemHeight{24};               ///< Height of each item

    /// @brief Custom role for storing QuickInsertItem data
    static constexpr int ItemDataRole = Qt::UserRole + 1;
};

}  // namespace kalahari::editor
