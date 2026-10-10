/// @file book_editor_accessible.h
/// @brief Accessibility interface for BookEditor (OpenSpec #00042 Task 7.16)
///
/// Provides QAccessibleWidget implementation with QAccessibleTextInterface
/// for screen reader support. Enables NVDA, JAWS, Narrator, and VoiceOver
/// to read and navigate document content.

#pragma once

#include <QAccessibleWidget>
#include <QAccessibleTextInterface>
#include <QCoreApplication>

class QTextDocument;

namespace kalahari::editor {

class BookEditor;

/// @brief Accessibility interface for BookEditor widget
///
/// Implements QAccessibleWidget with QAccessibleTextInterface to provide
/// full screen reader support for the custom text editor.
///
/// Key features:
/// - Text navigation (character, word, line, paragraph)
/// - Selection announcements
/// - Cursor position tracking
/// - Document structure navigation
///
/// Screen readers use this interface to:
/// - Read text content at cursor position
/// - Announce selections and changes
/// - Navigate document structure
/// - Report formatting information
class BookEditorAccessible : public QAccessibleWidget, public QAccessibleTextInterface {
    // Without it tr() would be that of QAccessibleActionInterface, whose context has none
    // of the translations of this class
    Q_DECLARE_TR_FUNCTIONS(kalahari::editor::BookEditorAccessible)

public:
    /// @brief Construct accessibility interface for BookEditor
    /// @param editor The BookEditor widget to provide accessibility for
    explicit BookEditorAccessible(BookEditor* editor);

    /// @brief Destructor
    ~BookEditorAccessible() override = default;

    // =========================================================================
    // QAccessibleInterface overrides
    // =========================================================================

    /// @brief Get the role of this accessible object
    /// @return QAccessible::EditableText for text editor
    QAccessible::Role role() const override;

    /// @brief Get accessibility state flags
    /// @return State flags (focusable, editable, etc.)
    QAccessible::State state() const override;

    /// @brief Get text for a specific accessibility text type
    /// @param t The text type (Name, Description, Value, etc.)
    /// @return The text for the requested type
    QString text(QAccessible::Text t) const override;

    /// @brief Cast to specific interface type
    /// @param t The interface type to cast to
    /// @return Pointer to interface, or nullptr if not supported
    void* interface_cast(QAccessible::InterfaceType t) override;

    // =========================================================================
    // QAccessibleTextInterface implementation
    // =========================================================================

    /// @brief Get text selection count
    /// @return Number of selections (0 or 1)
    int selectionCount() const override;

    /// @brief Add a text selection
    /// @param startOffset Start position of selection
    /// @param endOffset End position of selection
    void addSelection(int startOffset, int endOffset) override;

    /// @brief Remove a text selection
    /// @param selectionIndex Index of selection to remove (always 0)
    void removeSelection(int selectionIndex) override;

    /// @brief Set the selection range
    /// @param selectionIndex Index of selection (always 0)
    /// @param startOffset New start position
    /// @param endOffset New end position
    void setSelection(int selectionIndex, int startOffset, int endOffset) override;

    /// @brief Get the cursor position
    /// @return Current cursor offset in document
    int cursorPosition() const override;

    /// @brief Set the cursor position
    /// @param position New cursor offset
    void setCursorPosition(int position) override;

    /// @brief Get text within a range
    /// @param startOffset Start position
    /// @param endOffset End position
    /// @return Text between the offsets
    QString text(int startOffset, int endOffset) const override;

    /// @brief Get total character count
    /// @return Number of characters in document
    int characterCount() const override;

    /// @brief Get character rectangle
    /// @param offset Character offset
    /// @return Rectangle of character in screen coordinates
    QRect characterRect(int offset) const override;

    /// @brief Get offset at a point
    /// @param point Screen coordinates
    /// @return Character offset at that point, or -1
    int offsetAtPoint(const QPoint& point) const override;

    /// @brief Scroll to make text visible
    /// @param startOffset Start of range to scroll to
    /// @param endOffset End of range to scroll to
    void scrollToSubstring(int startOffset, int endOffset) override;

    /// @brief Get selection range
    /// @param selectionIndex Index of selection (always 0)
    /// @param startOffset [out] Start of selection
    /// @param endOffset [out] End of selection
    void selection(int selectionIndex, int* startOffset, int* endOffset) const override;

    /// @brief Get text attributes at offset
    /// @param offset Character offset
    /// @param startOffset [out] Start of attribute run
    /// @param endOffset [out] End of attribute run
    /// @return Attribute string (e.g., "font-weight:bold")
    QString attributes(int offset, int* startOffset, int* endOffset) const override;

private:
    /// @brief Get the BookEditor widget
    /// @return Pointer to BookEditor
    BookEditor* bookEditor() const;

    /// @brief Convert paragraph+offset to absolute document offset
    /// @param paragraphIndex Paragraph index
    /// @param charOffset Character offset within paragraph
    /// @return Absolute offset from document start
    int toAbsoluteOffset(int paragraphIndex, int charOffset) const;

    /// @brief Convert absolute offset to paragraph+offset
    /// @param absoluteOffset Absolute offset from document start
    /// @param paragraphIndex [out] Paragraph index
    /// @param charOffset [out] Character offset within paragraph
    void fromAbsoluteOffset(int absoluteOffset, int& paragraphIndex, int& charOffset) const;

    /// @brief Get full document text
    /// @return Concatenated document text, cached until the document changes
    QString documentText() const;

    mutable QString m_cachedText;                     ///< Text of m_cachedDocument
    mutable const QTextDocument* m_cachedDocument{nullptr};  ///< Document the text came from
    mutable int m_cachedRevision{-1};                 ///< Its revision() at that time
};

/// @brief Install accessibility interface factory for BookEditor
///
/// Call this once at application startup to register the accessibility
/// factory for BookEditor widgets. After registration, Qt will automatically
/// create BookEditorAccessible instances for BookEditor widgets.
///
/// Usage:
/// @code
/// // In main() or application initialization
/// installBookEditorAccessibility();
/// @endcode
void installBookEditorAccessibility();

}  // namespace kalahari::editor
