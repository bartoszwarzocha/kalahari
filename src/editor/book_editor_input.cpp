/// @file book_editor_input.cpp
/// @brief BookEditor: keyboard, input method, mouse, drag and drop and context menu events

#include <kalahari/editor/book_editor.h>
#include <kalahari/editor/buffer_commands.h>
#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QScopedValueRollback>
#include <QTimer>
#include <algorithm>
#include <utility>

namespace kalahari::editor {

// Automatic scrolling while selecting with the mouse or dragging text: one step per
// interval, longer the further the mouse is past the edge (or into the edge band, for
// dragged text)
constexpr int AUTO_SCROLL_INTERVAL = 25;           // ms
constexpr qreal AUTO_SCROLL_DROP_BAND = 24.0;      // px at the top and bottom edges
constexpr qreal AUTO_SCROLL_MIN_STEP = 2.0;        // px
constexpr qreal AUTO_SCROLL_MAX_STEP = 60.0;       // px

namespace {

/// @brief Whether a key types: a character (also with AltGr), Backspace or Delete
bool isTypingKey(const QKeyEvent* event) {
    if (event->key() == Qt::Key_Backspace || event->key() == Qt::Key_Delete) {
        return true;
    }
    const bool ctrlOnly = (event->modifiers() & Qt::ControlModifier) &&
                          !(event->modifiers() & Qt::AltModifier);
    return !ctrlOnly && !event->text().isEmpty() && event->text().at(0).isPrint();
}

}  // namespace

void BookEditor::keyPressEvent(QKeyEvent* event)
{
    // The word being typed gets no spelling wave until the cursor leaves it
    const QScopedValueRollback<bool> typing(m_spellTypingEdit, isTypingKey(event));

    // Handle cursor navigation keys
    bool handled = false;
    bool ctrl = event->modifiers() & Qt::ControlModifier;
    bool shift = event->modifiers() & Qt::ShiftModifier;

    switch (event->key()) {
        case Qt::Key_Left:
            if (ctrl && shift) {
                moveCursorWordLeftWithSelection(true);
            } else if (ctrl) {
                moveCursorWordLeftWithSelection(false);
            } else if (shift) {
                moveCursorLeftWithSelection(true);
            } else {
                moveCursorLeftWithSelection(false);
            }
            handled = true;
            break;

        case Qt::Key_Right:
            if (ctrl && shift) {
                moveCursorWordRightWithSelection(true);
            } else if (ctrl) {
                moveCursorWordRightWithSelection(false);
            } else if (shift) {
                moveCursorRightWithSelection(true);
            } else {
                moveCursorRightWithSelection(false);
            }
            handled = true;
            break;

        case Qt::Key_Up:
            if (shift) {
                moveCursorUpWithSelection(true);
            } else {
                moveCursorUpWithSelection(false);
            }
            handled = true;
            break;

        case Qt::Key_Down:
            if (shift) {
                moveCursorDownWithSelection(true);
            } else {
                moveCursorDownWithSelection(false);
            }
            handled = true;
            break;

        case Qt::Key_Home:
            if (ctrl && shift) {
                moveCursorToDocStartWithSelection(true);
            } else if (ctrl) {
                moveCursorToDocStartWithSelection(false);
            } else if (shift) {
                moveCursorToLineStartWithSelection(true);
            } else {
                moveCursorToLineStartWithSelection(false);
            }
            handled = true;
            break;

        case Qt::Key_End:
            if (ctrl && shift) {
                moveCursorToDocEndWithSelection(true);
            } else if (ctrl) {
                moveCursorToDocEndWithSelection(false);
            } else if (shift) {
                moveCursorToLineEndWithSelection(true);
            } else {
                moveCursorToLineEndWithSelection(false);
            }
            handled = true;
            break;

        case Qt::Key_PageUp:
        case Qt::Key_PageDown:
            // One view height in every view mode (Ctrl+PageUp/PageDown would be the page
            // jumps of a word processor); Shift extends the selection
            if (shift && m_selection.isEmpty()) {
                m_selectionAnchor = m_cursorPosition;
            }
            if (event->key() == Qt::Key_PageUp) {
                moveCursorPageUp();
            } else {
                moveCursorPageDown();
            }
            if (shift) {
                extendSelection(m_cursorPosition);
            } else {
                clearSelection();
            }
            handled = true;
            break;

        case Qt::Key_A:
            if (ctrl) {
                selectAll();
                handled = true;
            }
            break;

        case Qt::Key_Z:
            if (ctrl && !shift) {
                undo();
                handled = true;
            } else if (ctrl && shift) {
                redo();  // Ctrl+Shift+Z is redo on some platforms
                handled = true;
            }
            break;

        case Qt::Key_Y:
            if (ctrl) {
                redo();
                handled = true;
            }
            break;

        case Qt::Key_C:
            if (ctrl) {
                copy();
                handled = true;
            }
            break;

        case Qt::Key_X:
            if (ctrl) {
                cut();
                handled = true;
            }
            break;

        case Qt::Key_V:
            if (ctrl) {
                paste();
                handled = true;
            }
            break;

        case Qt::Key_Return:
        case Qt::Key_Enter:
            insertNewline();
            handled = true;
            break;

        case Qt::Key_Backspace:
            deleteBackward();
            handled = true;
            break;

        case Qt::Key_Delete:
            deleteForward();
            handled = true;
            break;

        case Qt::Key_F10:
            // Shift+F10 opens the context menu, as the menu key does, also where the system
            // does not make a context menu event of it
            if (event->modifiers() == Qt::ShiftModifier) {
                showContextMenuAtCursor();
                handled = true;
            }
            break;

        default:
            break;
    }

    // Handle printable characters (if not already handled)
    // Note: On Windows, AltGr sends Ctrl+Alt, so we must allow text when both are pressed
    // Only block Ctrl-only combinations (real shortcuts like Ctrl+C)
    bool alt = event->modifiers() & Qt::AltModifier;
    bool ctrlOnly = ctrl && !alt;
    if (!handled && !ctrlOnly && !event->text().isEmpty()) {
        QString text = event->text();
        // Only handle printable characters
        if (!text.isEmpty() && text.at(0).isPrint()) {
            insertText(text);
            handled = true;
        }
    }

    if (handled) {
        if (m_spellTypingEdit) {
            noteSpellingTyping();
        }
        event->accept();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void BookEditor::inputMethodEvent(QInputMethodEvent* event)
{
    if (!m_textBuffer) {
        event->ignore();
        return;
    }

    // Composed text is typed too (see keyPressEvent())
    const QScopedValueRollback<bool> typing(
        m_spellTypingEdit, !event->commitString().isEmpty() || !event->preeditString().isEmpty() ||
                               m_hasComposition);

    // Phase 11: Use QTextCursor for IME operations
    if (!m_textBuffer) return;

    // Handle committed text (final input)
    const QString& commitString = event->commitString();
    if (!commitString.isEmpty()) {
        // If we had composition, it's been replaced by commit
        if (m_hasComposition) {
            // The preedit text is already in the document, delete it first
            if (!m_preeditString.isEmpty()) {
                QTextBlock block = m_textBuffer->findBlockByNumber(m_preeditStart.paragraph);
                if (block.isValid()) {
                    QTextCursor cursor(m_textBuffer.get());
                    int startPos = block.position() + m_preeditStart.offset;
                    cursor.setPosition(startPos);
                    cursor.setPosition(startPos + m_preeditString.length(), QTextCursor::KeepAnchor);
                    cursor.removeSelectedText();
                }
                setCursorPosition(m_preeditStart);
            }
            m_preeditString.clear();
            m_hasComposition = false;
        }

        // Insert committed text (this handles selection deletion too)
        insertText(commitString);
    }

    // Handle preedit text (composition in progress)
    const QString& preeditString = event->preeditString();
    if (m_hasComposition) {
        // Remove old preedit text
        if (!m_preeditString.isEmpty()) {
            QTextBlock block = m_textBuffer->findBlockByNumber(m_preeditStart.paragraph);
            if (block.isValid()) {
                QTextCursor cursor(m_textBuffer.get());
                int startPos = block.position() + m_preeditStart.offset;
                cursor.setPosition(startPos);
                cursor.setPosition(startPos + m_preeditString.length(), QTextCursor::KeepAnchor);
                cursor.removeSelectedText();
            }
            setCursorPosition(m_preeditStart);
        }
    }

    if (!preeditString.isEmpty()) {
        // Store composition state
        m_preeditStart = m_cursorPosition;
        m_preeditString = preeditString;
        m_hasComposition = true;

        // Insert preedit text using QTextCursor, with the format typed text gets
        QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
        if (block.isValid()) {
            QTextCursor cursor(m_textBuffer.get());
            cursor.setPosition(block.position() + m_cursorPosition.offset);
            cursor.insertText(preeditString, insertionFormat(cursor));
        }

        // Move cursor to end of preedit
        CursorPosition newPos = m_cursorPosition;
        newPos.offset += preeditString.length();
        setCursorPosition(newPos);
    } else {
        // No preedit, clear composition state
        m_preeditString.clear();
        m_hasComposition = false;
    }

    if (m_spellTypingEdit) {
        noteSpellingTyping();
    }
    event->accept();
    update();
}

QVariant BookEditor::inputMethodQuery(Qt::InputMethodQuery query) const
{
    switch (query) {
        case Qt::ImEnabled:
            return true;

        case Qt::ImCursorRectangle: {
            // Cursor rectangle for IME positioning, from the pipeline that paints the cursor
            const QRectF rect = m_renderPipeline ? m_renderPipeline->cursorRect() : QRectF();
            if (rect.isEmpty()) {
                // Default position at top-left with some offset
                return QRectF(10, 10, 2, 20);
            }
            return rect;
        }

        case Qt::ImFont:
            return font();

        case Qt::ImCursorPosition:
            return m_cursorPosition.offset;

        case Qt::ImSurroundingText: {
            // Phase 11: Return text of current paragraph using QTextBlock
            if (m_textBuffer) {
                QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
                if (block.isValid()) {
                    return block.text();
                }
            }
            return QString();
        }

        case Qt::ImCurrentSelection: {
            if (hasSelection()) {
                return selectedText();
            }
            return QString();
        }

        case Qt::ImAnchorPosition:
            return m_selectionAnchor.offset;

        case Qt::ImHints:
            return static_cast<int>(Qt::ImhMultiLine);

        default:
            break;
    }

    return QWidget::inputMethodQuery(query);
}

// =============================================================================
// Mouse Event Handlers (Phase 3.9/3.10/3.11)
// =============================================================================

void BookEditor::mousePressEvent(QMouseEvent* event)
{
    // The mouse places the cursor: typewriter scrolling leaves the view where it is
    const QScopedValueRollback<bool> pointerMove(m_pointerMovesCursor, true);

    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    // A click on an annotation's mark opens the annotation; the cursor stays
    if (!(event->modifiers() & Qt::ShiftModifier)) {
        const QString markId = annotationMarkAt(event->position());
        if (!markId.isEmpty()) {
            emit annotationMarkClicked(markId);
            event->accept();
            return;
        }
    }

    // Set focus on click
    setFocus();

    QPointF clickPos = event->position();

    // Check for multi-click (double/triple)
    bool isMultiClick = false;
    if (m_clickTimer != nullptr && m_clickTimer->isActive()) {
        // Check distance from last click
        qreal distance = (clickPos - m_lastClickPos).manhattanLength();
        if (distance <= MULTI_CLICK_DISTANCE) {
            ++m_clickCount;
            isMultiClick = true;
        } else {
            m_clickCount = 1;
        }
    } else {
        m_clickCount = 1;
    }

    m_lastClickPos = clickPos;

    // Start/restart click timer
    if (m_clickTimer == nullptr) {
        m_clickTimer = new QTimer(this);
        m_clickTimer->setSingleShot(true);
        connect(m_clickTimer, &QTimer::timeout, this, [this]() {
            m_clickCount = 0;
        });
    }
    m_clickTimer->start(MULTI_CLICK_INTERVAL);

    // Convert click to cursor position
    CursorPosition clickPosition = positionFromPoint(clickPos);

    if (m_clickCount == 3) {
        // Triple click - select paragraph
        m_cursorPosition = clickPosition;
        selectParagraphAtCursor();
    } else if (m_clickCount == 2 || isMultiClick) {
        // Double click - select word (handled in mouseDoubleClickEvent)
        // But we still need to set position for the case when double-click
        // is detected through our click counting
        m_cursorPosition = clickPosition;
        selectWordAtCursor();
    } else if (!(event->modifiers() & Qt::ShiftModifier) && isOverSelectedText(clickPos)) {
        // A press on the selected text drags it once the mouse moves far enough; released
        // without moving, it places the cursor (mouseReleaseEvent())
        m_textDragPending = true;
        m_textDragStartPos = clickPos;
    } else {
        // Single click - position cursor
        if (event->modifiers() & Qt::ShiftModifier) {
            // Shift+click extends selection
            extendSelection(clickPosition);
            update();
        } else {
            // Normal click clears selection and positions cursor
            clearSelection();
            m_selectionAnchor = clickPosition;
            setCursorPosition(clickPosition);
        }

        // Start drag selection
        m_isDragging = true;
    }

    event->accept();
}

void BookEditor::mouseMoveEvent(QMouseEvent* event)
{
    // The mouse places the cursor: typewriter scrolling leaves the view where it is
    const QScopedValueRollback<bool> pointerMove(m_pointerMovesCursor, true);

    // In Distraction-Free, the texts at the edges come back when the mouse nears an edge
    if (m_distractionFree) {
        // Check if mouse is near edges for fade trigger
        QPointF pos = event->position();
        qreal edgeThreshold = 50.0;  // Pixels from edge
        bool nearEdge = (pos.y() < edgeThreshold ||
                         pos.y() > height() - edgeThreshold ||
                         pos.x() < edgeThreshold ||
                         pos.x() > width() - edgeThreshold);

        if (nearEdge && m_appearance.distractionFree.fadeOnMouseMove) {
            // With mouse tracking this runs on every move: repaint only to show faded UI
            const bool wasFaded = m_uiOpacity < 1.0;
            m_uiOpacity = 1.0;
            startUiFade();
            if (wasFaded) {
                update();
            }
        }
    }

    const QPointF pos = event->position();
    if (!(event->buttons() & Qt::LeftButton)) {
        // A hand over an annotation's mark, which opens it; an arrow over the selected text,
        // which can be dragged; an I-beam elsewhere
        Qt::CursorShape shape = Qt::IBeamCursor;
        if (!annotationMarkAt(pos).isEmpty()) {
            shape = Qt::PointingHandCursor;
        } else if (isOverSelectedText(pos)) {
            shape = Qt::ArrowCursor;
        }
        if (cursor().shape() != shape) {
            setCursor(shape);
        }
        QWidget::mouseMoveEvent(event);
        return;
    }

    if (m_textDragPending) {
        if ((pos - m_textDragStartPos).manhattanLength() >= QApplication::startDragDistance()) {
            m_textDragPending = false;
            startTextDrag();
        }
        event->accept();
        return;
    }

    if (!m_isDragging) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    // The selection follows the mouse; past the top or bottom edge the view scrolls
    extendMouseSelection(pos);
    ensureCursorVisible();
    updateAutoScroll(pos, false);

    event->accept();
}

void BookEditor::mouseReleaseEvent(QMouseEvent* event)
{
    // The mouse places the cursor: typewriter scrolling leaves the view where it is
    const QScopedValueRollback<bool> pointerMove(m_pointerMovesCursor, true);

    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    stopAutoScroll();
    m_isDragging = false;
    if (m_textDragPending) {
        // A click on the selected text without dragging places the cursor there
        m_textDragPending = false;
        clearSelection();
        const CursorPosition position = positionFromPoint(event->position());
        m_selectionAnchor = position;
        setCursorPosition(position);
    }
    event->accept();
}

void BookEditor::mouseDoubleClickEvent(QMouseEvent* event)
{
    // The mouse places the cursor: typewriter scrolling leaves the view where it is
    const QScopedValueRollback<bool> pointerMove(m_pointerMovesCursor, true);

    if (event->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }

    // Position cursor at double-click location
    CursorPosition clickPosition = positionFromPoint(event->position());
    m_cursorPosition = clickPosition;

    // Select word at cursor
    selectWordAtCursor();

    // Update click count for triple-click detection
    m_clickCount = 2;
    m_lastClickPos = event->position();
    if (m_clickTimer != nullptr) {
        m_clickTimer->start(MULTI_CLICK_INTERVAL);
    }

    event->accept();
}

// =============================================================================
// Drag and drop of text (stage 3)
// =============================================================================

void BookEditor::dragEnterEvent(QDragEnterEvent* event)
{
    if (!m_textBuffer || !canInsertFromMimeData(event->mimeData())) {
        event->ignore();
        return;
    }
    event->acceptProposedAction();
}

void BookEditor::dragMoveEvent(QDragMoveEvent* event)
{
    if (!m_textBuffer || !canInsertFromMimeData(event->mimeData())) {
        event->ignore();
        return;
    }

    const QPointF pos = event->position();
    updateAutoScroll(pos, true);

    // The selected text is not moved onto itself
    const CursorPosition position = positionFromPoint(pos);
    if (event->source() == this && event->proposedAction() == Qt::MoveAction &&
        isInSelection(position)) {
        m_renderPipeline->setDropCaret(std::nullopt);
        event->ignore();
        return;
    }

    m_renderPipeline->setDropCaret(position);
    event->acceptProposedAction();
}

void BookEditor::dragLeaveEvent(QDragLeaveEvent* event)
{
    stopAutoScroll();
    m_renderPipeline->setDropCaret(std::nullopt);
    event->accept();
}

void BookEditor::dropEvent(QDropEvent* event)
{
    // The mouse places the cursor: typewriter scrolling leaves the view where it is
    const QScopedValueRollback<bool> pointerMove(m_pointerMovesCursor, true);

    stopAutoScroll();
    m_renderPipeline->setDropCaret(std::nullopt);

    const bool moveSelection = event->source() == this && event->dropAction() == Qt::MoveAction;
    if (!dropMimeData(event->mimeData(), positionFromPoint(event->position()), moveSelection)) {
        event->ignore();
        return;
    }
    setFocus();
    event->accept();
}

bool BookEditor::isOverSelectedText(const QPointF& widgetPos) const
{
    if (!hasSelection() || !m_textBuffer) {
        return false;
    }

    // The character under the point: the layout's exact hit, which misses the space around
    // the text and the gaps between pages
    const QPointF docPoint = m_renderPipeline->widgetToDocument(widgetPos);
    const int hit = m_textBuffer->documentLayout()->hitTest(docPoint, Qt::ExactHit);
    if (hit < 0) {
        return false;
    }
    const QTextBlock block = m_textBuffer->findBlock(hit);
    const CursorPosition position{block.blockNumber(), hit - block.position()};

    const SelectionRange sel = m_selection.normalized();
    return sel.start <= position && position < sel.end;
}

bool BookEditor::isInSelection(const CursorPosition& position) const
{
    const SelectionRange sel = m_selection.normalized();
    return hasSelection() && sel.start <= position && position <= sel.end;
}

void BookEditor::startTextDrag()
{
    std::unique_ptr<QMimeData> mimeData = createMimeDataFromSelection();
    if (!mimeData) {
        return;
    }

    // Stays at the dragged text if the drop target edits the document before it
    const SelectionRange sel = m_selection.normalized();
    QTextCursor dragged = createCursor(m_textBuffer.get(), sel.start, sel.end);

    auto* drag = new QDrag(this);  // Qt deletes it once the drag is over
    drag->setMimeData(mimeData.release());
    m_draggingText = true;
    const Qt::DropAction action = drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::MoveAction);
    m_draggingText = false;
    stopAutoScroll();

    // Moved to another widget or program: the text leaves the editor (dropMimeData()
    // moves it within the editor). The editor's own find bar only takes a copy.
    const auto* target = qobject_cast<QWidget*>(drag->target());
    const bool movedAway =
        action == Qt::MoveAction && target != this && (target == nullptr || !isAncestorOf(target));
    if (movedAway && dragged.hasSelection()) {
        clearSelection();
        dragged.removeSelectedText();
        finishEdit(dragged);
    }
}

void BookEditor::extendMouseSelection(const QPointF& widgetPos)
{
    // Past the top or bottom edge, the selection ends in the first or last visible line;
    // automatic scrolling brings the next lines in
    const double y = std::clamp(widgetPos.y(), 0.0, std::max(0.0, height() - 1.0));
    const CursorPosition position = positionFromPoint(QPointF(widgetPos.x(), y));

    m_cursorPosition = position;
    setSelection({m_selectionAnchor, position});
    syncPipelineCursor();
    update();
}

void BookEditor::updateAutoScroll(const QPointF& widgetPos, bool forDrop)
{
    m_autoScrollPos = widgetPos;
    m_autoScrollForDrop = forDrop;
    if (autoScrollStep() == 0.0) {
        stopAutoScroll();
        return;
    }

    if (m_autoScrollTimer == nullptr) {
        m_autoScrollTimer = new QTimer(this);
        m_autoScrollTimer->setInterval(AUTO_SCROLL_INTERVAL);
        connect(m_autoScrollTimer, &QTimer::timeout, this, &BookEditor::onAutoScrollTimeout);
    }
    if (!m_autoScrollTimer->isActive()) {
        m_autoScrollTimer->start();
    }
}

void BookEditor::stopAutoScroll()
{
    if (m_autoScrollTimer != nullptr) {
        m_autoScrollTimer->stop();
    }
}

double BookEditor::autoScrollStep() const
{
    // Dragged text scrolls the view in a band along the top and bottom edges, a mouse
    // selection past them
    const double band = m_autoScrollForDrop ? AUTO_SCROLL_DROP_BAND : 0.0;
    const double y = m_autoScrollPos.y();
    double distance = 0.0;
    if (y < band) {
        distance = y - band;
    } else if (y > height() - band) {
        distance = y - (height() - band);
    }
    if (distance == 0.0) {
        return 0.0;
    }
    const double step =
        std::clamp(std::abs(distance) / 2.0, AUTO_SCROLL_MIN_STEP, AUTO_SCROLL_MAX_STEP);
    return distance < 0.0 ? -step : step;
}

void BookEditor::onAutoScrollTimeout()
{
    // The mouse places the cursor: typewriter scrolling leaves the view where it is
    const QScopedValueRollback<bool> pointerMove(m_pointerMovesCursor, true);

    // The step is in view pixels (page mode zooms the document)
    const double scale = m_viewportManager ? m_viewportManager->viewScale() : 1.0;
    const double step = autoScrollStep() / scale;
    const qreal before = scrollOffset();
    if (step != 0.0) {
        scrollBy(step, false);
    }
    if (scrollOffset() == before) {
        stopAutoScroll();  // the mouse is back inside, or the view is at the end
        return;
    }

    // The text under the mouse has moved: the selection or the drop caret follows it
    if (m_autoScrollForDrop) {
        const CursorPosition position = positionFromPoint(m_autoScrollPos);
        if (m_draggingText && isInSelection(position)) {
            m_renderPipeline->setDropCaret(std::nullopt);
        } else {
            m_renderPipeline->setDropCaret(position);
        }
    } else if (m_isDragging) {
        extendMouseSelection(m_autoScrollPos);
    }
}

void BookEditor::setContextMenuActions(const QList<QAction*>& actions)
{
    m_contextMenuActions = actions;
}

void BookEditor::contextMenuEvent(QContextMenuEvent* event)
{
    if (!m_textBuffer) {
        QWidget::contextMenuEvent(event);
        return;
    }

    // From the keyboard (the menu key, Shift+F10) the menu is for the cursor's place
    if (event->reason() == QContextMenuEvent::Keyboard) {
        showContextMenuAtCursor();
        return;
    }

    const CursorPosition pos = positionFromPoint(event->pos());
    if (pos.paragraph < 0) {
        QWidget::contextMenuEvent(event);
        return;
    }
    showContextMenu(pos, event->globalPos(), event->reason() == QContextMenuEvent::Mouse);
}

void BookEditor::showContextMenuAtCursor()
{
    if (!m_textBuffer || m_cursorPosition.paragraph < 0) {
        return;
    }
    checkSpellingAtCursor();  // also the word just typed, without its wave yet

    // Under the cursor, so the word the menu is for stays in sight
    QPoint menuPos = mapToGlobal(rect().center());
    if (m_renderPipeline) {
        const QRectF caret = m_renderPipeline->cursorRect();
        if (!caret.isEmpty()) {
            menuPos = mapToGlobal(caret.bottomLeft().toPoint());
        }
    }
    showContextMenu(m_cursorPosition, menuPos, false);
}

void BookEditor::showContextMenu(const CursorPosition& pos, const QPoint& globalPos,
                                 bool fromMouse)
{
    // A misspelled word: the words to put in its place, on top of the usual menu
    const auto [word, startOffset, endOffset] = getMisspelledWordAt(pos.paragraph, pos.offset);

    // Check if position is in a grammar error (Phase 6.17)
    auto grammarError = word.isEmpty() ? getGrammarErrorAt(pos.paragraph, pos.offset)
                                       : std::nullopt;
    if (grammarError.has_value()) {
        // Create grammar check context menu
        QMenu* menu = createGrammarContextMenu(*grammarError, pos.paragraph);
        menu->exec(globalPos);
        delete menu;
        return;
    }

    // A right click outside the selection puts the cursor there, as a click does, so what
    // the menu does (pasting, adding an annotation) happens where the writer clicked
    if (fromMouse && !isInSelection(pos)) {
        const QScopedValueRollback<bool> pointerMove(m_pointerMovesCursor, true);
        clearSelection();
        m_selectionAnchor = pos;
        setCursorPosition(pos);
    }

    // Default context menu
    QMenu menu(this);
    if (!word.isEmpty()) {
        addSpellingActions(menu, word, pos.paragraph, startOffset, endOffset);
    }

    if (hasSelection()) {
        menu.addAction(tr("Cut"), this, &BookEditor::cut);
        menu.addAction(tr("Copy"), this, &BookEditor::copy);
    }
    menu.addAction(tr("Paste"), this, &BookEditor::paste);

    if (hasSelection()) {
        menu.addSeparator();
        menu.addAction(tr("Select All"), this, &BookEditor::selectAll);
    }

    // The application's commands for the text (adding annotations)
    bool separated = false;
    for (QAction* action : std::as_const(m_contextMenuActions)) {
        if (action == nullptr) {
            continue;
        }
        if (!separated) {
            menu.addSeparator();
            separated = true;
        }
        menu.addAction(action);
    }

    // Color mode toggle
    menu.addSeparator();
    QString colorModeText = (m_appearance.colorMode == EditorColorMode::Light)
        ? tr("Switch to Dark Mode")
        : tr("Switch to Light Mode");
    menu.addAction(colorModeText, this, &BookEditor::toggleEditorColorMode);

    menu.exec(globalPos);
}

}  // namespace kalahari::editor
