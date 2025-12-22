/// @file quick_insert_handler.cpp
/// @brief Quick insert handler implementation (OpenSpec #00042 Tasks 7.11-7.12)

#include <kalahari/editor/quick_insert_handler.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/core/logger.h>
#include <QWidget>

namespace kalahari::editor {

// =============================================================================
// Construction
// =============================================================================

QuickInsertHandler::QuickInsertHandler(QObject* parent)
    : QObject(parent)
{
    core::Logger::getInstance().debug("QuickInsertHandler created");
}

// =============================================================================
// Setup
// =============================================================================

void QuickInsertHandler::setEditor(BookEditor* editor)
{
    m_editor = editor;

    // Cancel any active quick insert when editor changes
    if (m_active) {
        cancel();
    }
}

void QuickInsertHandler::setCharacters(const QList<QuickInsertItem>& characters)
{
    m_characters = characters;
    core::Logger::getInstance().debug("QuickInsertHandler: {} characters set",
                                       m_characters.size());
}

void QuickInsertHandler::setLocations(const QList<QuickInsertItem>& locations)
{
    m_locations = locations;
    core::Logger::getInstance().debug("QuickInsertHandler: {} locations set",
                                       m_locations.size());
}

void QuickInsertHandler::addCharacter(const QuickInsertItem& character)
{
    m_characters.append(character);
}

void QuickInsertHandler::addLocation(const QuickInsertItem& location)
{
    m_locations.append(location);
}

void QuickInsertHandler::clearCharacters()
{
    m_characters.clear();
}

void QuickInsertHandler::clearLocations()
{
    m_locations.clear();
}

// =============================================================================
// Trigger Detection
// =============================================================================

bool QuickInsertHandler::processTextInput(const QString& text)
{
    if (text.isEmpty() || m_editor == nullptr) {
        return false;
    }

    QChar ch = text.at(0);

    // Check for trigger characters
    if (ch == '@') {
        m_active = true;
        m_currentType = QuickInsertType::Character;
        m_filter.clear();
        m_triggerPosition = m_editor->cursorPosition().offset - 1;

        core::Logger::getInstance().debug("QuickInsert: @ trigger at position {}",
                                           m_triggerPosition);

        emit triggered(m_currentType, getCursorScreenPosition());
        return true;
    }

    if (ch == '#') {
        m_active = true;
        m_currentType = QuickInsertType::Location;
        m_filter.clear();
        m_triggerPosition = m_editor->cursorPosition().offset - 1;

        core::Logger::getInstance().debug("QuickInsert: # trigger at position {}",
                                           m_triggerPosition);

        emit triggered(m_currentType, getCursorScreenPosition());
        return true;
    }

    // If already active, update the filter
    if (m_active) {
        // Space or punctuation ends quick insert mode
        if (ch.isSpace() || (!ch.isLetterOrNumber() && ch != '_' && ch != '-')) {
            cancel();
            return false;
        }

        // Update filter with the new character
        m_filter.append(ch);
        emit filterChanged(m_filter);

        // Check if any items still match
        auto filtered = filteredItems();
        if (filtered.isEmpty()) {
            cancel();
            return false;
        }

        return true;
    }

    return false;
}

bool QuickInsertHandler::updateFilter(const QString& text)
{
    if (!m_active) {
        return false;
    }

    // Handle backspace
    if (text.isEmpty()) {
        if (!m_filter.isEmpty()) {
            m_filter.chop(1);
            emit filterChanged(m_filter);
            return true;
        } else {
            // Backspace with empty filter - cancel
            cancel();
            return false;
        }
    }

    // Add to filter
    m_filter.append(text);
    emit filterChanged(m_filter);

    // Check if any items still match
    auto filtered = filteredItems();
    return !filtered.isEmpty();
}

bool QuickInsertHandler::isTriggerChar(QChar ch)
{
    return ch == '@' || ch == '#';
}

// =============================================================================
// Item Access
// =============================================================================

QList<QuickInsertItem> QuickInsertHandler::filteredItems() const
{
    if (!m_active) {
        return {};
    }

    switch (m_currentType) {
        case QuickInsertType::Character:
            return filterItems(m_characters, m_filter);
        case QuickInsertType::Location:
            return filterItems(m_locations, m_filter);
    }

    return {};
}

QList<QuickInsertItem> QuickInsertHandler::filterItems(
    const QList<QuickInsertItem>& items,
    const QString& filter) const
{
    if (filter.isEmpty()) {
        return items;
    }

    QList<QuickInsertItem> result;
    QString lowerFilter = filter.toLower();

    for (const auto& item : items) {
        // Match against name (case-insensitive)
        if (item.name.toLower().contains(lowerFilter)) {
            result.append(item);
            continue;
        }

        // Also match against description
        if (!item.description.isEmpty() &&
            item.description.toLower().contains(lowerFilter)) {
            result.append(item);
        }
    }

    return result;
}

// =============================================================================
// Insertion
// =============================================================================

void QuickInsertHandler::insertItem(const QuickInsertItem& item)
{
    if (m_editor == nullptr || !m_active) {
        return;
    }

    core::Logger::getInstance().debug("QuickInsert: inserting '{}' at position {}",
                                       item.name.toStdString(), m_triggerPosition);

    // Calculate text to replace (trigger + filter)
    int replaceLength = 1 + m_filter.length();  // @ or # plus filter text

    // Calculate the position to start deletion
    CursorPosition deleteStart;
    deleteStart.paragraph = m_editor->cursorPosition().paragraph;
    deleteStart.offset = m_triggerPosition;

    CursorPosition deleteEnd;
    deleteEnd.paragraph = deleteStart.paragraph;
    deleteEnd.offset = m_triggerPosition + replaceLength;

    // Delete the trigger and filter text
    // Move cursor to trigger position first
    m_editor->setCursorPosition(deleteStart);

    // Select the text to replace
    for (int i = 0; i < replaceLength; ++i) {
        m_editor->deleteForward();
    }

    // Format the reference based on type
    QString reference;
    switch (item.type) {
        case QuickInsertType::Character:
            reference = QString("@%1").arg(item.name);
            break;
        case QuickInsertType::Location:
            reference = QString("#%1").arg(item.name);
            break;
    }

    // Insert the formatted reference
    m_editor->insertText(reference);

    // Add a space after the reference
    m_editor->insertText(" ");

    // Reset state
    m_active = false;
    m_filter.clear();
    m_triggerPosition = -1;

    emit completed();
}

void QuickInsertHandler::cancel()
{
    if (!m_active) {
        return;
    }

    core::Logger::getInstance().debug("QuickInsert: cancelled");

    m_active = false;
    m_filter.clear();
    m_triggerPosition = -1;

    emit cancelled();
}

// =============================================================================
// Private Methods
// =============================================================================

QPoint QuickInsertHandler::getCursorScreenPosition() const
{
    if (m_editor == nullptr) {
        return QPoint(0, 0);
    }

    // Get cursor rectangle in widget coordinates
    // For now, use a simple estimation based on cursor position
    // A more accurate implementation would use the layout manager

    QPoint widgetPos(50, 50);  // Default position

    // Convert to global coordinates
    return m_editor->mapToGlobal(widgetPos);
}

}  // namespace kalahari::editor
