/// @file kml_document.h
/// @brief KML Document - root container for paragraphs (OpenSpec #00042 Phase 1.8)
///
/// KmlDocument is the root container that holds all paragraphs in a document.
/// It provides an observer interface for content change notifications and
/// paragraph-level operations for document manipulation.
///
/// Key responsibilities:
/// - Paragraph container with add/remove/insert operations
/// - Content change notifications (observer pattern)
/// - Full document serialization to KML format
///
/// Note: This class uses an observer pattern instead of Qt signals to avoid
/// Q_OBJECT DLL export issues in the shared library.

#pragma once

#include <kalahari/editor/kml_paragraph.h>
#include <kalahari/editor/editor_types.h>
#include <QString>
#include <vector>
#include <memory>
#include <functional>

namespace kalahari::editor {

/// @brief Observer interface for document changes
///
/// Implement this interface to receive notifications about document modifications.
/// All methods have default empty implementations for convenience.
class IDocumentObserver {
public:
    virtual ~IDocumentObserver() = default;

    /// @brief Called when any content in the document changes
    virtual void onContentChanged() {}

    /// @brief Called when a paragraph is inserted
    /// @param index The index where the paragraph was inserted
    virtual void onParagraphInserted(int index) { (void)index; }

    /// @brief Called when a paragraph is removed
    /// @param index The index where the paragraph was removed
    virtual void onParagraphRemoved(int index) { (void)index; }

    /// @brief Called when a paragraph is modified
    /// @param index The index of the modified paragraph
    virtual void onParagraphModified(int index) { (void)index; }
};

/// @brief Root container for KML document content
///
/// KmlDocument is the top-level container that holds all paragraphs in a KML document.
/// It uses an observer pattern for change notifications, enabling views to react
/// to document modifications.
///
/// The document maintains a sequential list of paragraphs and provides operations
/// for paragraph management (add, remove, insert) and full document serialization.
///
/// Example usage:
/// @code
/// auto doc = std::make_unique<KmlDocument>();
/// doc->addParagraph(std::make_unique<KmlParagraph>("First paragraph"));
/// doc->addParagraph(std::make_unique<KmlParagraph>("Second paragraph"));
/// QString kml = doc->toKml();
/// @endcode
///
/// To receive change notifications, add an observer:
/// @code
/// class MyObserver : public IDocumentObserver {
///     void onContentChanged() override { /* handle change */ }
/// };
/// MyObserver observer;
/// doc->addObserver(&observer);
/// @endcode
class KmlDocument {
public:
    /// @brief Construct an empty document
    KmlDocument();

    /// @brief Destructor
    ~KmlDocument();

    /// @brief Copy constructor
    KmlDocument(const KmlDocument& other);

    /// @brief Move constructor
    KmlDocument(KmlDocument&& other) noexcept;

    /// @brief Copy assignment
    KmlDocument& operator=(const KmlDocument& other);

    /// @brief Move assignment
    KmlDocument& operator=(KmlDocument&& other) noexcept;

    // =========================================================================
    // Observer Management
    // =========================================================================

    /// @brief Add an observer for document changes
    /// @param observer The observer to add (must outlive this document or be removed)
    void addObserver(IDocumentObserver* observer);

    /// @brief Remove an observer
    /// @param observer The observer to remove
    void removeObserver(IDocumentObserver* observer);

    // =========================================================================
    // Paragraph Container Methods
    // =========================================================================

    /// @brief Get the number of paragraphs in the document
    /// @return Number of paragraphs
    int paragraphCount() const;

    /// @brief Get a paragraph by index
    /// @param index The index (0-based)
    /// @return Pointer to the paragraph, or nullptr if index out of range
    const KmlParagraph* paragraph(int index) const;

    /// @brief Get a mutable paragraph by index
    /// @param index The index (0-based)
    /// @return Pointer to the paragraph, or nullptr if index out of range
    KmlParagraph* paragraph(int index);

    /// @brief Add a paragraph to the end of the document
    /// @param paragraph The paragraph to add (ownership transferred)
    /// @note Notifies observers via onParagraphInserted and onContentChanged
    void addParagraph(std::unique_ptr<KmlParagraph> paragraph);

    /// @brief Insert a paragraph at a specific index
    /// @param index The insertion position (0 = before first)
    /// @param paragraph The paragraph to insert (ownership transferred)
    /// @note Notifies observers via onParagraphInserted and onContentChanged
    void insertParagraph(int index, std::unique_ptr<KmlParagraph> paragraph);

    /// @brief Remove a paragraph by index
    /// @param index The index to remove
    /// @return The removed paragraph, or nullptr if index out of range
    /// @note Notifies observers via onParagraphRemoved and onContentChanged
    std::unique_ptr<KmlParagraph> removeParagraph(int index);

    /// @brief Remove all paragraphs
    /// @note Notifies observers via onContentChanged
    void clear();

    /// @brief Check if the document is empty (no paragraphs)
    /// @return true if no paragraphs
    bool isEmpty() const;

    /// @brief Get direct access to paragraphs (for iteration)
    /// @return Const reference to the paragraph vector
    const std::vector<std::unique_ptr<KmlParagraph>>& paragraphs() const;

    // =========================================================================
    // Content Methods
    // =========================================================================

    /// @brief Get the plain text content of the entire document
    /// @return Concatenated plain text from all paragraphs (separated by newlines)
    QString plainText() const;

    /// @brief Get total character count across all paragraphs
    /// @return Sum of character counts from all paragraphs
    int length() const;

    // =========================================================================
    // Text Operations (Phase 1.9)
    // =========================================================================

    /// @brief Insert text at a cursor position
    /// @param position The position to insert at (paragraph + offset)
    /// @param text The text to insert
    /// @return true if successful, false if position is invalid
    /// @note Notifies observers via onParagraphModified and onContentChanged
    bool insertText(const CursorPosition& position, const QString& text);

    /// @brief Delete text between two cursor positions
    /// @param start The start position (inclusive)
    /// @param end The end position (exclusive)
    /// @return true if successful, false if range is invalid
    /// @note Handles single-paragraph and multi-paragraph deletions
    /// @note Multi-paragraph deletion merges the first and last paragraphs
    /// @note Notifies observers via onParagraphModified/Removed and onContentChanged
    bool deleteText(const CursorPosition& start, const CursorPosition& end);

    /// @brief Apply a style to a selection range
    /// @param range The selection range to apply style to
    /// @param styleId The style ID to apply (paragraph style)
    /// @return true if successful, false if range is invalid
    /// @note Notifies observers via onParagraphModified and onContentChanged
    bool applyStyle(const SelectionRange& range, const QString& styleId);

    /// @brief Split a paragraph at a cursor position (Enter key)
    /// @param position The position to split at
    /// @return true if successful, false if position is invalid
    /// @note Creates a new paragraph after the current one
    /// @note Content after the cursor is moved to the new paragraph
    /// @note Notifies observers via onParagraphInserted and onContentChanged
    bool splitParagraph(const CursorPosition& position);

    /// @brief Merge a paragraph with the previous one (Backspace at start)
    /// @param paragraphIndex The index of the paragraph to merge (must be > 0)
    /// @return The cursor offset in the merged paragraph where the join occurred,
    ///         or -1 if merge failed (invalid index or first paragraph)
    /// @note Content of the paragraph is appended to the previous paragraph
    /// @note The paragraph at paragraphIndex is removed
    /// @note Notifies observers via onParagraphRemoved and onContentChanged
    int mergeParagraphWithPrevious(int paragraphIndex);

    // =========================================================================
    // Modification Tracking
    // =========================================================================

    /// @brief Check if the document has been modified since last reset
    /// @return true if modified
    bool isModified() const;

    /// @brief Mark the document as modified
    void setModified(bool modified = true);

    /// @brief Reset the modified flag to false
    void resetModified();

    // =========================================================================
    // Serialization
    // =========================================================================

    /// @brief Serialize the entire document to KML format
    /// @return QString containing valid KML markup
    QString toKml() const;

    /// @brief Create a deep copy of this document
    /// @return unique_ptr to a new document with same content (no observers copied)
    std::unique_ptr<KmlDocument> clone() const;

    // =========================================================================
    // Notification Methods (for use by paragraph manipulation code)
    // =========================================================================

    /// @brief Notify that a paragraph was modified
    /// @param index The index of the modified paragraph
    /// @note This is called when external code modifies a paragraph directly
    void notifyParagraphModified(int index);

private:
    /// @brief Notify all observers of content change
    void notifyContentChanged();

    /// @brief Notify all observers of paragraph insertion
    void notifyParagraphInserted(int index);

    /// @brief Notify all observers of paragraph removal
    void notifyParagraphRemoved(int index);

    std::vector<std::unique_ptr<KmlParagraph>> m_paragraphs;  ///< Paragraph storage
    std::vector<IDocumentObserver*> m_observers;  ///< Registered observers
    bool m_modified;  ///< Modification flag
};

}  // namespace kalahari::editor
