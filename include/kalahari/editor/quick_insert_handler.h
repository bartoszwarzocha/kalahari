/// @file quick_insert_handler.h
/// @brief Quick insert handler for @ and # triggers (OpenSpec #00042 Tasks 7.11-7.12)
///
/// QuickInsertHandler detects @ and # prefixes while typing and triggers
/// autocomplete popups for character and location references.
///
/// Features:
/// - Detects @ for character mentions
/// - Detects # for location references
/// - Filters items as user types
/// - Inserts formatted references

#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QPoint>

namespace kalahari::editor {

// Forward declaration
class BookEditor;

/// @brief Types of quick insert triggers
enum class QuickInsertType {
    Character,  ///< @character reference (triggered by @)
    Location    ///< #location reference (triggered by #)
};

/// @brief Item that can be inserted via quick insert
///
/// Represents a character, location, or other entity that can be
/// referenced in the text using @ or # prefix.
struct QuickInsertItem {
    QString id;         ///< Unique identifier
    QString name;       ///< Display name (what user sees)
    QString description;///< Optional description/subtitle
    QuickInsertType type;///< Type of reference

    /// @brief Default constructor
    QuickInsertItem() = default;

    /// @brief Construct with all fields
    QuickInsertItem(const QString& itemId, const QString& itemName,
                    const QString& desc, QuickInsertType itemType)
        : id(itemId)
        , name(itemName)
        , description(desc)
        , type(itemType)
    {}

    /// @brief Equality comparison
    bool operator==(const QuickInsertItem& other) const {
        return id == other.id && type == other.type;
    }
};

/// @brief Handler for quick insert functionality
///
/// Monitors text input and detects @ and # triggers. When a trigger
/// is detected, emits a signal so the popup can be shown.
///
/// Usage:
/// @code
/// auto handler = new QuickInsertHandler(this);
/// handler->setEditor(bookEditor);
/// handler->setCharacters(characterList);
/// handler->setLocations(locationList);
///
/// connect(handler, &QuickInsertHandler::triggered,
///         this, &MyWidget::showPopup);
/// @endcode
class QuickInsertHandler : public QObject {
    Q_OBJECT

public:
    /// @brief Constructor
    /// @param parent Parent QObject for ownership
    explicit QuickInsertHandler(QObject* parent = nullptr);

    /// @brief Destructor
    ~QuickInsertHandler() override = default;

    // Non-copyable
    QuickInsertHandler(const QuickInsertHandler&) = delete;
    QuickInsertHandler& operator=(const QuickInsertHandler&) = delete;

    // =========================================================================
    // Setup
    // =========================================================================

    /// @brief Set the editor to monitor
    /// @param editor BookEditor to monitor for text input
    void setEditor(BookEditor* editor);

    /// @brief Get the current editor
    /// @return Current editor, or nullptr if none set
    BookEditor* editor() const { return m_editor; }

    /// @brief Set available characters for @mention
    /// @param characters List of characters that can be referenced
    void setCharacters(const QList<QuickInsertItem>& characters);

    /// @brief Set available locations for #reference
    /// @param locations List of locations that can be referenced
    void setLocations(const QList<QuickInsertItem>& locations);

    /// @brief Add a character to the list
    /// @param character Character item to add
    void addCharacter(const QuickInsertItem& character);

    /// @brief Add a location to the list
    /// @param location Location item to add
    void addLocation(const QuickInsertItem& location);

    /// @brief Clear all characters
    void clearCharacters();

    /// @brief Clear all locations
    void clearLocations();

    // =========================================================================
    // Trigger Detection
    // =========================================================================

    /// @brief Process text input, detect @ or # trigger
    /// @param text The text that was just typed
    /// @return true if trigger detected and popup should show
    ///
    /// Call this when the user types a character. If the character
    /// is @ or #, quick insert mode is activated.
    bool processTextInput(const QString& text);

    /// @brief Update filter with additional characters
    /// @param text Additional text typed after trigger
    /// @return true if still in quick insert mode
    ///
    /// Call this when the user types more characters after the trigger.
    /// Returns false if the filter no longer matches any items.
    bool updateFilter(const QString& text);

    /// @brief Check if in quick insert mode
    /// @return true if trigger has been detected and popup should be open
    bool isActive() const { return m_active; }

    /// @brief Get current trigger type
    /// @return The type of quick insert currently active
    QuickInsertType currentType() const { return m_currentType; }

    /// @brief Get current filter string
    /// @return Text typed after the trigger character
    QString currentFilter() const { return m_filter; }

    /// @brief Get the position where trigger was detected
    /// @return Character offset in paragraph where @ or # was typed
    int triggerPosition() const { return m_triggerPosition; }

    // =========================================================================
    // Item Access
    // =========================================================================

    /// @brief Get filtered items for current input
    /// @return List of items matching current filter
    ///
    /// Filters the character or location list based on the current
    /// filter string (text typed after @ or #).
    QList<QuickInsertItem> filteredItems() const;

    /// @brief Get all characters
    /// @return List of all character items
    QList<QuickInsertItem> characters() const { return m_characters; }

    /// @brief Get all locations
    /// @return List of all location items
    QList<QuickInsertItem> locations() const { return m_locations; }

    // =========================================================================
    // Insertion
    // =========================================================================

    /// @brief Insert selected item at cursor
    /// @param item The item to insert
    ///
    /// Replaces the trigger character and filter text with the
    /// formatted reference (e.g., "@John Smith" for a character).
    void insertItem(const QuickInsertItem& item);

    /// @brief Cancel quick insert mode
    ///
    /// Call this when the popup is dismissed without selection.
    void cancel();

    /// @brief Check if a character is a trigger
    /// @param ch The character to check
    /// @return true if @ or #
    static bool isTriggerChar(QChar ch);

signals:
    /// @brief Emitted when a trigger is detected
    /// @param type The type of quick insert (Character or Location)
    /// @param position Screen position where popup should appear
    void triggered(QuickInsertType type, const QPoint& position);

    /// @brief Emitted when filter changes (user types more characters)
    /// @param filter The current filter string
    void filterChanged(const QString& filter);

    /// @brief Emitted when an item is inserted
    void completed();

    /// @brief Emitted when quick insert is cancelled
    void cancelled();

private:
    /// @brief Filter items by search string
    /// @param items List of items to filter
    /// @param filter Search string
    /// @return Filtered list of items
    QList<QuickInsertItem> filterItems(const QList<QuickInsertItem>& items,
                                       const QString& filter) const;

    /// @brief Get cursor position in screen coordinates
    /// @return Screen position for popup placement
    QPoint getCursorScreenPosition() const;

    BookEditor* m_editor{nullptr};          ///< Editor being monitored
    bool m_active{false};                   ///< Is quick insert mode active?
    QuickInsertType m_currentType{QuickInsertType::Character}; ///< Current trigger type
    QString m_filter;                       ///< Filter text (after @ or #)
    int m_triggerPosition{-1};              ///< Position where trigger was typed

    QList<QuickInsertItem> m_characters;    ///< Available characters
    QList<QuickInsertItem> m_locations;     ///< Available locations
};

}  // namespace kalahari::editor
