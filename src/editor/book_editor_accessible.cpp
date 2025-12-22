/// @file book_editor_accessible.cpp
/// @brief Accessibility interface implementation for BookEditor (OpenSpec #00042 Task 7.16)

#include <kalahari/editor/book_editor_accessible.h>
#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/kml_document.h>
#include <kalahari/editor/kml_paragraph.h>
#include <kalahari/editor/layout_manager.h>
#include <kalahari/editor/paragraph_layout.h>
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
        // Return current line text for screen readers
        if (editor->document()) {
            const auto& pos = editor->cursorPosition();
            if (pos.paragraph >= 0 &&
                pos.paragraph < editor->document()->paragraphCount()) {
                auto* para = editor->document()->paragraph(pos.paragraph);
                if (para) {
                    return para->plainText();
                }
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
    if (!editor || !editor->document()) {
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
    if (!editor || !editor->document()) {
        return QRect();
    }

    int paraIndex = 0, charOffset = 0;
    fromAbsoluteOffset(offset, paraIndex, charOffset);

    // Get layout for paragraph
    auto& layoutManager = editor->layoutManager();
    auto* layout = layoutManager.paragraphLayout(paraIndex);
    if (!layout) {
        return QRect();
    }

    // Get cursor rect from layout
    QRectF cursorRect = layout->cursorRect(charOffset);
    if (cursorRect.isEmpty()) {
        return QRect();
    }

    // Convert to screen coordinates
    QPoint widgetPos = editor->mapToGlobal(QPoint(0, 0));
    qreal scrollOffset = editor->scrollOffset();
    qreal paraY = editor->scrollManager().paragraphY(paraIndex);

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

    // Scroll to make paragraph visible
    editor->scrollManager().ensureParagraphVisible(paraIndex);
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
    if (!editor || !editor->document()) {
        if (startOffset) *startOffset = offset;
        if (endOffset) *endOffset = offset;
        return QString();
    }

    int paraIndex = 0, charOffset = 0;
    fromAbsoluteOffset(offset, paraIndex, charOffset);

    // Get paragraph for style info
    auto* para = editor->document()->paragraph(paraIndex);
    if (!para) {
        if (startOffset) *startOffset = offset;
        if (endOffset) *endOffset = offset;
        return QString();
    }

    // Calculate run boundaries (for now, whole paragraph)
    int paraStart = toAbsoluteOffset(paraIndex, 0);
    int paraEnd = toAbsoluteOffset(paraIndex, para->plainText().length());

    if (startOffset) *startOffset = paraStart;
    if (endOffset) *endOffset = paraEnd;

    // Return basic style attributes
    // Format: "key1:value1;key2:value2"
    QString attrs;

    // Style ID
    QString styleId = para->styleId();
    if (!styleId.isEmpty()) {
        attrs += QString("style:%1;").arg(styleId);
    }

    // Text runs could add bold/italic info here
    // For now, return basic attributes
    return attrs;
}

BookEditor* BookEditorAccessible::bookEditor() const
{
    return qobject_cast<BookEditor*>(widget());
}

int BookEditorAccessible::toAbsoluteOffset(int paragraphIndex, int charOffset) const
{
    BookEditor* editor = bookEditor();
    if (!editor || !editor->document()) {
        return 0;
    }

    int offset = 0;
    auto* doc = editor->document();

    for (int i = 0; i < paragraphIndex && i < doc->paragraphCount(); ++i) {
        auto* para = doc->paragraph(i);
        if (para) {
            offset += para->plainText().length();
            offset += 1;  // Newline between paragraphs
        }
    }

    return offset + charOffset;
}

void BookEditorAccessible::fromAbsoluteOffset(int absoluteOffset, int& paragraphIndex, int& charOffset) const
{
    BookEditor* editor = bookEditor();
    if (!editor || !editor->document()) {
        paragraphIndex = 0;
        charOffset = 0;
        return;
    }

    auto* doc = editor->document();
    int remaining = absoluteOffset;

    for (int i = 0; i < doc->paragraphCount(); ++i) {
        auto* para = doc->paragraph(i);
        if (!para) continue;

        int paraLen = para->plainText().length();
        if (remaining <= paraLen) {
            paragraphIndex = i;
            charOffset = remaining;
            return;
        }

        remaining -= paraLen;
        remaining -= 1;  // Newline

        if (remaining < 0) {
            // In the newline itself, go to end of this paragraph
            paragraphIndex = i;
            charOffset = paraLen;
            return;
        }
    }

    // Past end of document
    paragraphIndex = doc->paragraphCount() - 1;
    if (paragraphIndex < 0) paragraphIndex = 0;

    if (doc->paragraphCount() > 0) {
        auto* lastPara = doc->paragraph(paragraphIndex);
        charOffset = lastPara ? lastPara->plainText().length() : 0;
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
    if (!editor || !editor->document()) {
        return QString();
    }

    QStringList parts;
    auto* doc = editor->document();

    for (int i = 0; i < doc->paragraphCount(); ++i) {
        auto* para = doc->paragraph(i);
        if (para) {
            parts.append(para->plainText());
        }
    }

    m_cachedText = parts.join('\n');
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
