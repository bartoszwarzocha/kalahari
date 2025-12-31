/// @file book_editor_accessible.cpp
/// @brief Accessibility interface implementation for BookEditor (OpenSpec #00042 Task 7.16)
/// Phase 11: Migrated to QTextDocument-based architecture

#include <kalahari/editor/book_editor_accessible.h>
#include <kalahari/editor/book_editor.h>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextDocument>
#include <QTextLayout>
#include <QTextLine>
#include <QWidget>

namespace kalahari::editor {

BookEditorAccessible::BookEditorAccessible(BookEditor* editor)
    : QAccessibleWidget(editor, QAccessible::EditableText, tr("Book Editor"))
{
}

QAccessible::Role BookEditorAccessible::role() const
{
    return QAccessible::EditableText;
}

QAccessible::State BookEditorAccessible::state() const
{
    QAccessible::State s = QAccessibleWidget::state();

    BookEditor* editor = bookEditor();
    if (editor) {
        // Mark as editable
        s.editable = true;

        // Multi-line editor
        s.multiLine = true;

        // Selection state
        s.selectable = true;
        if (editor->hasSelection()) {
            s.selected = true;
        }

        // Focus state
        if (editor->hasFocus()) {
            s.focused = true;
        }

        // Read-only state (always editable for now)
        s.readOnly = false;
    }

    return s;
}

QString BookEditorAccessible::text(QAccessible::Text t) const
{
    BookEditor* editor = bookEditor();
    if (!editor) {
        return QString();
    }

    switch (t) {
    case QAccessible::Name:
        return tr("Book Editor");

    case QAccessible::Description:
        return tr("Text editor for writing and editing book content");

    case QAccessible::Value:
        // Return current line text for screen readers (Phase 11: QTextDocument)
        if (auto* doc = editor->textDocument()) {
            const auto& pos = editor->cursorPosition();
            QTextBlock block = doc->findBlockByNumber(pos.paragraph);
            if (block.isValid()) {
                return block.text();
            }
        }
        return QString();

    case QAccessible::Help:
        return tr("Use arrow keys to navigate, Ctrl+Home/End for document start/end");

    default:
        return QAccessibleWidget::text(t);
    }
}

void* BookEditorAccessible::interface_cast(QAccessible::InterfaceType t)
{
    if (t == QAccessible::TextInterface) {
        return static_cast<QAccessibleTextInterface*>(this);
    }
    return QAccessibleWidget::interface_cast(t);
}

int BookEditorAccessible::selectionCount() const
{
    BookEditor* editor = bookEditor();
    if (editor && editor->hasSelection()) {
        return 1;
    }
    return 0;
}

void BookEditorAccessible::addSelection(int startOffset, int endOffset)
{
    BookEditor* editor = bookEditor();
    if (!editor || !editor->textDocument()) {
        return;
    }

    int startPara = 0, startChar = 0;
    int endPara = 0, endChar = 0;

    fromAbsoluteOffset(startOffset, startPara, startChar);
    fromAbsoluteOffset(endOffset, endPara, endChar);

    SelectionRange selection;
    selection.start = CursorPosition{startPara, startChar};
    selection.end = CursorPosition{endPara, endChar};

    editor->setSelection(selection);
}

void BookEditorAccessible::removeSelection(int /*selectionIndex*/)
{
    BookEditor* editor = bookEditor();
    if (editor) {
        editor->clearSelection();
    }
}

void BookEditorAccessible::setSelection(int /*selectionIndex*/, int startOffset, int endOffset)
{
    addSelection(startOffset, endOffset);
}

int BookEditorAccessible::cursorPosition() const
{
    BookEditor* editor = bookEditor();
    if (!editor) {
        return 0;
    }

    const auto& pos = editor->cursorPosition();
    return toAbsoluteOffset(pos.paragraph, pos.offset);
}

void BookEditorAccessible::setCursorPosition(int position)
{
    BookEditor* editor = bookEditor();
    if (!editor) {
        return;
    }

    int paraIndex = 0, charOffset = 0;
    fromAbsoluteOffset(position, paraIndex, charOffset);

    editor->setCursorPosition(CursorPosition{paraIndex, charOffset});
}

QString BookEditorAccessible::text(int startOffset, int endOffset) const
{
    QString fullText = documentText();
    if (startOffset < 0) startOffset = 0;
    if (endOffset > fullText.length()) endOffset = fullText.length();
    if (startOffset >= endOffset) return QString();

    return fullText.mid(startOffset, endOffset - startOffset);
}

int BookEditorAccessible::characterCount() const
{
    return documentText().length();
}

QRect BookEditorAccessible::characterRect(int offset) const
{
    BookEditor* editor = bookEditor();
    auto* doc = editor ? editor->textDocument() : nullptr;
    if (!doc) {
        return QRect();
    }

    int paraIndex = 0, charOffset = 0;
    fromAbsoluteOffset(offset, paraIndex, charOffset);

    // Get block and layout (Phase 11: QTextDocument)
    QTextBlock block = doc->findBlockByNumber(paraIndex);
    if (!block.isValid()) {
        return QRect();
    }

    QTextLayout* layout = block.layout();
    if (!layout) {
        return QRect();
    }

    // Get cursor rect from QTextLayout
    QTextLine line = layout->lineForTextPosition(charOffset);
    if (!line.isValid()) {
        return QRect();
    }

    qreal x = line.cursorToX(charOffset);
    QRectF cursorRect(x, line.y(), 2.0, line.height());

    // Convert to screen coordinates
    QPoint widgetPos = editor->mapToGlobal(QPoint(0, 0));
    qreal scrollOffset = editor->scrollOffset();
    qreal paraY = layout->position().y();

    QRect screenRect;
    screenRect.setX(widgetPos.x() + static_cast<int>(cursorRect.x()));
    screenRect.setY(widgetPos.y() + static_cast<int>(paraY - scrollOffset + cursorRect.y()));
    screenRect.setWidth(std::max(1, static_cast<int>(cursorRect.width())));
    screenRect.setHeight(static_cast<int>(cursorRect.height()));

    return screenRect;
}

int BookEditorAccessible::offsetAtPoint(const QPoint& point) const
{
    BookEditor* editor = bookEditor();
    if (!editor) {
        return -1;
    }

    // Convert from screen to widget coordinates
    QPoint widgetPos = editor->mapFromGlobal(point);

    // Use BookEditor's hit testing
    // This is simplified - would need proper positionFromPoint integration
    // For now, return cursor position
    return cursorPosition();
}

void BookEditorAccessible::scrollToSubstring(int startOffset, int /*endOffset*/)
{
    BookEditor* editor = bookEditor();
    if (!editor) {
        return;
    }

    int paraIndex = 0, charOffset = 0;
    fromAbsoluteOffset(startOffset, paraIndex, charOffset);

    // Move cursor to position and ensure visible (Phase 11)
    editor->setCursorPosition(CursorPosition{paraIndex, charOffset});
    editor->ensureCursorVisible();
    editor->update();
}

void BookEditorAccessible::selection(int selectionIndex, int* startOffset, int* endOffset) const
{
    if (selectionIndex != 0 || !startOffset || !endOffset) {
        if (startOffset) *startOffset = 0;
        if (endOffset) *endOffset = 0;
        return;
    }

    BookEditor* editor = bookEditor();
    if (!editor || !editor->hasSelection()) {
        *startOffset = 0;
        *endOffset = 0;
        return;
    }

    SelectionRange sel = editor->selection();
    *startOffset = toAbsoluteOffset(sel.start.paragraph, sel.start.offset);
    *endOffset = toAbsoluteOffset(sel.end.paragraph, sel.end.offset);
}

QString BookEditorAccessible::attributes(int offset, int* startOffset, int* endOffset) const
{
    BookEditor* editor = bookEditor();
    auto* doc = editor ? editor->textDocument() : nullptr;
    if (!doc) {
        if (startOffset) *startOffset = offset;
        if (endOffset) *endOffset = offset;
        return QString();
    }

    int paraIndex = 0, charOffset = 0;
    fromAbsoluteOffset(offset, paraIndex, charOffset);

    // Get block for style info (Phase 11: QTextDocument)
    QTextBlock block = doc->findBlockByNumber(paraIndex);
    if (!block.isValid()) {
        if (startOffset) *startOffset = offset;
        if (endOffset) *endOffset = offset;
        return QString();
    }

    // Calculate run boundaries (for now, whole paragraph)
    int paraStart = toAbsoluteOffset(paraIndex, 0);
    int paraEnd = toAbsoluteOffset(paraIndex, block.text().length());

    if (startOffset) *startOffset = paraStart;
    if (endOffset) *endOffset = paraEnd;

    // Return basic style attributes from QTextBlockFormat
    // Format: "key1:value1;key2:value2"
    QString attrs;

    QTextBlockFormat format = block.blockFormat();
    // Could add alignment, indent, etc. from format
    // For now, return minimal attributes
    return attrs;
}

BookEditor* BookEditorAccessible::bookEditor() const
{
    return qobject_cast<BookEditor*>(widget());
}

int BookEditorAccessible::toAbsoluteOffset(int paragraphIndex, int charOffset) const
{
    BookEditor* editor = bookEditor();
    auto* doc = editor ? editor->textDocument() : nullptr;
    if (!doc) {
        return 0;
    }

    int offset = 0;

    // Phase 11: iterate QTextBlocks
    for (int i = 0; i < paragraphIndex && i < doc->blockCount(); ++i) {
        QTextBlock block = doc->findBlockByNumber(i);
        if (block.isValid()) {
            offset += block.text().length();
            offset += 1;  // Newline between paragraphs
        }
    }

    return offset + charOffset;
}

void BookEditorAccessible::fromAbsoluteOffset(int absoluteOffset, int& paragraphIndex, int& charOffset) const
{
    BookEditor* editor = bookEditor();
    auto* doc = editor ? editor->textDocument() : nullptr;
    if (!doc) {
        paragraphIndex = 0;
        charOffset = 0;
        return;
    }

    int remaining = absoluteOffset;

    // Phase 11: iterate QTextBlocks
    for (int i = 0; i < doc->blockCount(); ++i) {
        QTextBlock block = doc->findBlockByNumber(i);
        if (!block.isValid()) continue;

        int blockLen = block.text().length();
        if (remaining <= blockLen) {
            paragraphIndex = i;
            charOffset = remaining;
            return;
        }

        remaining -= blockLen;
        remaining -= 1;  // Newline

        if (remaining < 0) {
            // In the newline itself, go to end of this paragraph
            paragraphIndex = i;
            charOffset = blockLen;
            return;
        }
    }

    // Past end of document
    paragraphIndex = doc->blockCount() - 1;
    if (paragraphIndex < 0) paragraphIndex = 0;

    if (doc->blockCount() > 0) {
        QTextBlock lastBlock = doc->findBlockByNumber(paragraphIndex);
        charOffset = lastBlock.isValid() ? lastBlock.text().length() : 0;
    } else {
        charOffset = 0;
    }
}

QString BookEditorAccessible::documentText() const
{
    if (m_cacheValid) {
        return m_cachedText;
    }

    BookEditor* editor = bookEditor();
    auto* doc = editor ? editor->textDocument() : nullptr;
    if (!doc) {
        return QString();
    }

    // Phase 11: use QTextDocument::toPlainText()
    m_cachedText = doc->toPlainText();
    m_cacheValid = true;

    return m_cachedText;
}

void BookEditorAccessible::invalidateCache()
{
    m_cacheValid = false;
    m_cachedText.clear();
}

// =========================================================================
// Factory Registration
// =========================================================================

static QAccessibleInterface* bookEditorAccessibleFactory(const QString& classname, QObject* object)
{
    if (classname == QLatin1String("kalahari::editor::BookEditor") && object && object->isWidgetType()) {
        BookEditor* editor = qobject_cast<BookEditor*>(object);
        if (editor) {
            return new BookEditorAccessible(editor);
        }
    }
    return nullptr;
}

void installBookEditorAccessibility()
{
    QAccessible::installFactory(bookEditorAccessibleFactory);
}

}  // namespace kalahari::editor
