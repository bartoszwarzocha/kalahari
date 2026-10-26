/// @file book_editor_lists.cpp
/// @brief BookEditor: lists, the indents of paragraphs and their own formatting

#include <kalahari/editor/book_editor.h>
#include "book_editor_internal.h"
#include <kalahari/editor/buffer_commands.h>
#include <kalahari/editor/kalahari_text_document_layout.h>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <set>

namespace kalahari::editor {

namespace {

/// @brief Difference of two lengths in points below which they are the same (KML keeps two
/// decimals)
constexpr qreal SAME_POINTS = 0.005;

/// @brief The kind of a list item: of its level (a numbered list may have bullets on a level)
ListKind itemKind(const ListItem& item)
{
    return item.levelFormat().isBullet() ? ListKind::Bulleted : ListKind::Numbered;
}

/// @brief The list item a block of text is; nothing for an object and outside a list
std::optional<ListItem> textListItem(const QTextBlock& block)
{
    if (!block.isValid() || blockObject(block) != TextObject::None) {
        return std::nullopt;
    }
    return listItemOf(block.blockFormat());
}

/// @brief Whether a block is the first item of its list
bool isFirstItem(const QTextBlock& block, int listId)
{
    for (QTextBlock earlier = block.previous(); earlier.isValid(); earlier = earlier.previous()) {
        if (earlier.blockFormat().intProperty(LIST_ID_PROPERTY) == listId) {
            return false;
        }
    }
    return true;
}

/// @brief The nearest item before a block of a list of the kind other than @p listId
std::optional<ListItem> previousList(const QTextBlock& block, int listId, ListKind kind)
{
    for (QTextBlock earlier = block.previous(); earlier.isValid(); earlier = earlier.previous()) {
        const std::optional<ListItem> item = textListItem(earlier);
        if (item && item->id != listId && item->definition.kind() == kind) {
            return item;
        }
    }
    return std::nullopt;
}

/// @brief The indent a step of Increase or Decrease Indent gives: the next multiple of the
/// step in the direction, not past the edge of the text
qreal steppedIndent(qreal current, int direction)
{
    const qreal steps = current / INDENT_STEP_POINTS;
    const qreal next = direction > 0 ? std::floor(steps + 0.01) + 1.0 : std::ceil(steps - 0.01) - 1.0;
    return std::clamp(next * INDENT_STEP_POINTS, 0.0, MAX_PARAGRAPH_INDENT);
}

bool sameValue(qreal a, qreal b)
{
    return std::abs(a - b) < SAME_POINTS;
}

bool sameValue(bool a, bool b)
{
    return a == b;
}

bool sameValue(const LineSpacing& a, const LineSpacing& b)
{
    return a.rule == b.rule && std::abs(a.value - b.value) < SAME_POINTS;
}

/// @brief A paragraph's own part after a change: none when the change gives what its style
/// does
template <typename T>
void changePart(std::optional<T>& own, const std::optional<T>& change, const std::optional<T>& style)
{
    if (!change) {
        return;
    }
    if (style && sameValue(*change, *style)) {
        own.reset();
    } else {
        own = change;
    }
}

}  // namespace

// =============================================================================
// The paragraphs a change goes to
// =============================================================================

std::vector<QTextBlock> BookEditor::selectedTextBlocks() const
{
    std::vector<QTextBlock> blocks;
    if (!m_textBuffer) {
        return blocks;
    }
    int first = m_cursorPosition.paragraph;
    int last = m_cursorPosition.paragraph;
    if (hasSelection()) {
        const SelectionRange selection = m_selection.normalized();
        first = selection.start.paragraph;
        last = selection.end.paragraph;
    }
    for (QTextBlock block = m_textBuffer->findBlockByNumber(first);
         block.isValid() && block.blockNumber() <= last; block = block.next()) {
        if (blockObject(block) == TextObject::None) {
            blocks.push_back(block);
        }
    }
    return blocks;
}

void BookEditor::setBlockFormats(
    const std::vector<std::pair<QTextBlock, QTextBlockFormat>>& formats)
{
    std::vector<std::pair<QTextBlock, QTextBlockFormat>> changed;
    std::copy_if(formats.begin(), formats.end(), std::back_inserter(changed),
                 [](const auto& entry) { return entry.first.blockFormat() != entry.second; });
    if (changed.empty()) {
        return;
    }

    // One undo step that brings back the cursor and selection it was made with, as the
    // alignment does (setParagraphAlignment())
    QTextCursor cursor(m_textBuffer.get());
    cursor.beginEditBlock();
    m_textBuffer->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{m_cursorPosition, m_selection}] { m_stepCursor = state; }));
    for (const auto& [block, format] : changed) {
        cursor.setPosition(block.position());
        cursor.setBlockFormat(format);
    }
    cursor.endEditBlock();

    if (m_renderPipeline) {
        m_renderPipeline->markAllDirty();
    }
    ensureCursorVisible();
    emit contentChanged();
    emit paragraphModified(m_cursorPosition.paragraph);
    update();
}

// =============================================================================
// Lists
// =============================================================================

void BookEditor::toggleList(ListKind kind)
{
    if (refusesChange() || !m_textBuffer) {
        return;
    }
    const std::vector<QTextBlock> blocks = selectedTextBlocks();
    if (blocks.empty()) {
        return;
    }
    std::vector<std::pair<QTextBlock, QTextBlockFormat>> formats;

    // All of them items of the kind with numbers or bullets: they leave their lists
    if (std::all_of(blocks.begin(), blocks.end(), [kind](const QTextBlock& block) {
            const std::optional<ListItem> item = listItemOf(block.blockFormat());
            return item && item->numbered && itemKind(*item) == kind;
        })) {
        for (const QTextBlock& block : blocks) {
            QTextBlockFormat format = block.blockFormat();
            clearListItem(format);
            formats.emplace_back(block, format);
        }
        setBlockFormats(formats);
        return;
    }

    // The list they join: one of the kind among them, else the one right before or after
    // them, else a new one
    std::optional<ListItem> target;
    for (const QTextBlock& block : blocks) {
        if (const std::optional<ListItem> item = listItemOf(block.blockFormat());
            item && item->definition.kind() == kind) {
            target = item;
            break;
        }
    }
    for (const QTextBlock& neighbour : {blocks.front().previous(), blocks.back().next()}) {
        if (target) {
            break;
        }
        if (const std::optional<ListItem> item = textListItem(neighbour);
            item && item->definition.kind() == kind) {
            target = item;
        }
    }
    if (!target) {
        target = ListItem{};
        target->id = newListId(*m_textBuffer);
        target->definition = ListDefinition::ofKind(kind);
    }

    for (const QTextBlock& block : blocks) {
        QTextBlockFormat format = block.blockFormat();
        const std::optional<ListItem> item = listItemOf(format);
        ListItem joined;
        joined.id = target->id;
        joined.definition = target->definition;
        if (item) {
            joined.level = item->level;
            if (item->id == target->id) {
                joined.value = item->value;
            }
        }
        setListItem(format, joined);
        formats.emplace_back(block, format);
    }
    setBlockFormats(formats);
}

std::optional<ListKind> BookEditor::currentListKind() const
{
    const std::optional<ListItem> item = currentListItem();
    if (!item || !item->numbered) {
        return std::nullopt;
    }
    return itemKind(*item);
}

std::optional<ListItem> BookEditor::currentListItem() const
{
    if (!m_textBuffer) {
        return std::nullopt;
    }
    return textListItem(m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph));
}

int BookEditor::currentListNumber() const
{
    if (!m_textBuffer || !currentListItem()) {
        return -1;
    }
    const std::vector<int> numbers = listNumbers(*m_textBuffer);
    const auto index = static_cast<size_t>(m_cursorPosition.paragraph);
    return index < numbers.size() ? numbers[index] : -1;
}

void BookEditor::increaseIndent()
{
    if (refusesChange()) {
        return;
    }
    changeIndent(1, true);
}

void BookEditor::decreaseIndent()
{
    if (refusesChange()) {
        return;
    }
    changeIndent(-1, true);
}

void BookEditor::changeIndent(int direction, bool otherParagraphs)
{
    if (!m_textBuffer) {
        return;
    }
    const auto* layout =
        qobject_cast<const KalahariTextDocumentLayout*>(m_textBuffer->documentLayout());
    std::vector<std::pair<QTextBlock, QTextBlockFormat>> formats;
    for (const QTextBlock& block : selectedTextBlocks()) {
        QTextBlockFormat format = block.blockFormat();
        if (std::optional<ListItem> item = listItemOf(format)) {
            item->level = std::clamp(item->level + direction, 0, LIST_LEVELS - 1);
            setListItem(format, *item);
        } else if (otherParagraphs) {
            // From the indent it is shown with; the one of its style is not its own
            ParagraphFormat own = paragraphFormatOf(format);
            const qreal styleIndent =
                layout ? layout->styleParagraphFormat(block).leftIndent.value_or(0.0) : 0.0;
            const qreal indent = steppedIndent(own.leftIndent.value_or(styleIndent), direction);
            if (sameValue(indent, styleIndent)) {
                own.leftIndent.reset();
            } else {
                own.leftIndent = indent;
            }
            setParagraphFormat(format, own);
        }
        formats.emplace_back(block, format);
    }
    setBlockFormats(formats);
}

void BookEditor::restartNumbering()
{
    if (refusesChange()) {
        return;
    }
    std::optional<ListItem> item = currentListItem();
    if (!item || !item->numbered) {
        return;
    }
    const int start = item->levelFormat().start;
    if (currentListNumber() == start) {
        return;
    }
    const QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
    QTextBlockFormat format = block.blockFormat();
    item->value = start;
    setListItem(format, *item);
    setBlockFormats({{block, format}});
}

void BookEditor::continueNumbering()
{
    if (refusesChange() || !canContinueNumbering()) {
        return;
    }
    const QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
    std::optional<ListItem> item = textListItem(block);
    std::vector<std::pair<QTextBlock, QTextBlockFormat>> formats;

    // The first item of a list: its list goes on from the list of the kind before it
    const std::optional<ListItem> previous =
        isFirstItem(block, item->id) ? previousList(block, item->id, item->definition.kind())
                                     : std::nullopt;
    for (QTextBlock later = block; later.isValid(); later = later.next()) {
        QTextBlockFormat format = later.blockFormat();
        std::optional<ListItem> laterItem = listItemOf(format);
        if (!laterItem || laterItem->id != item->id) {
            continue;
        }
        if (later == block) {
            laterItem->value.reset();
        } else if (!previous) {
            break;
        }
        if (previous) {
            laterItem->id = previous->id;
            laterItem->definition = previous->definition;
        }
        setListItem(format, *laterItem);
        formats.emplace_back(later, format);
    }
    setBlockFormats(formats);
}

bool BookEditor::canContinueNumbering() const
{
    const std::optional<ListItem> item = currentListItem();
    if (!item) {
        return false;
    }
    if (item->value) {
        return true;
    }
    const QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
    return isFirstItem(block, item->id) &&
           previousList(block, item->id, item->definition.kind()).has_value();
}

void BookEditor::setNumberingValue(int value)
{
    if (refusesChange()) {
        return;
    }
    std::optional<ListItem> item = currentListItem();
    if (!item) {
        return;
    }
    value = std::clamp(value, MIN_LIST_NUMBER, MAX_LIST_NUMBER);
    if (item->numbered && currentListNumber() == value) {
        return;
    }
    const QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
    QTextBlockFormat format = block.blockFormat();
    item->value = value;
    item->numbered = true;
    setListItem(format, *item);
    setBlockFormats({{block, format}});
}

void BookEditor::applyListDefinition(const ListDefinition& definition)
{
    if (refusesChange() || !m_textBuffer) {
        return;
    }
    const std::vector<QTextBlock> blocks = selectedTextBlocks();
    if (blocks.empty()) {
        return;
    }

    // The lists of the selected items, all their items; the other paragraphs join the first
    std::set<int> lists;
    int firstList = 0;
    for (const QTextBlock& block : blocks) {
        if (const std::optional<ListItem> item = listItemOf(block.blockFormat())) {
            lists.insert(item->id);
            if (firstList == 0) {
                firstList = item->id;
            }
        }
    }
    if (firstList == 0) {
        firstList = newListId(*m_textBuffer);
    }

    std::vector<std::pair<QTextBlock, QTextBlockFormat>> formats;
    const auto isSelected = [&blocks](const QTextBlock& block) {
        return block.blockNumber() >= blocks.front().blockNumber() &&
               block.blockNumber() <= blocks.back().blockNumber() &&
               blockObject(block) == TextObject::None;
    };
    for (QTextBlock block = m_textBuffer->begin(); block.isValid(); block = block.next()) {
        QTextBlockFormat format = block.blockFormat();
        std::optional<ListItem> item = listItemOf(format);
        if (item && lists.count(item->id) > 0) {
            item->definition = definition;
            if (isSelected(block)) {
                item->numbered = true;
            }
        } else if (!item && isSelected(block)) {
            item = ListItem{};
            item->id = firstList;
            item->definition = definition;
        } else {
            continue;
        }
        setListItem(format, *item);
        formats.emplace_back(block, format);
    }
    setBlockFormats(formats);
}

// =============================================================================
// The keys in a list
// =============================================================================

bool BookEditor::tabChangesListLevel() const
{
    if (m_readOnly || !m_textBuffer) {
        return false;
    }
    if (!hasSelection()) {
        return m_cursorPosition.offset == 0 && currentListItem().has_value();
    }
    const SelectionRange selection = m_selection.normalized();
    const std::vector<QTextBlock> blocks = selectedTextBlocks();
    return (selection.start.paragraph != selection.end.paragraph || selection.start.offset == 0) &&
           !blocks.empty() && std::all_of(blocks.begin(), blocks.end(), [](const QTextBlock& block) {
               return isListItem(block.blockFormat());
           });
}

bool BookEditor::endListAtEmptyItem()
{
    if (hasSelection() || !m_textBuffer) {
        return false;
    }
    const QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
    std::optional<ListItem> item = textListItem(block);
    if (!item || block.length() > 1) {
        return false;
    }
    // As in Word: a level up, and out of the list from the first level
    QTextBlockFormat format = block.blockFormat();
    if (item->numbered && item->level > 0) {
        item->level -= 1;
        setListItem(format, *item);
    } else {
        clearListItem(format);
    }
    setBlockFormats({{block, format}});
    return true;
}

bool BookEditor::removeListMarkAtCursor()
{
    if (hasSelection() || m_cursorPosition.offset != 0 || !m_textBuffer) {
        return false;
    }
    const QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
    std::optional<ListItem> item = textListItem(block);
    if (!item) {
        return false;
    }
    // The number goes first, the item stays in the indent of the list; then it leaves it
    QTextBlockFormat format = block.blockFormat();
    if (item->numbered) {
        item->numbered = false;
        setListItem(format, *item);
    } else {
        clearListItem(format);
    }
    setBlockFormats({{block, format}});
    return true;
}

bool BookEditor::autoCorrectList(const QTextBlock& block)
{
    if (isListItem(block.blockFormat())) {
        return false;
    }
    const QString typed = block.text().left(m_cursorPosition.offset);
    static const QRegularExpression numberedStart(QStringLiteral("^(\\d{1,4})([.)]) $"));
    std::optional<ListKind> kind;
    int start = 1;
    QString suffix;
    if (m_autoCorrect.bulletedLists && typed == QStringLiteral("* ")) {
        kind = ListKind::Bulleted;
    } else if (m_autoCorrect.numberedLists) {
        if (const QRegularExpressionMatch match = numberedStart.match(typed); match.hasMatch()) {
            kind = ListKind::Numbered;
            start = std::clamp(match.captured(1).toInt(), MIN_LIST_NUMBER, MAX_LIST_NUMBER);
            suffix = match.captured(2);
        }
    }
    if (!kind) {
        return false;
    }

    // The list right before it, when the item goes on with it; else a new list
    ListItem item;
    const QTextBlock previous = block.previous();
    const std::optional<ListItem> before = textListItem(previous);
    bool joins = before && before->definition.kind() == *kind;
    if (joins && *kind == ListKind::Numbered) {
        const ListLevel& first = before->definition.levels[0];
        const std::vector<int> numbers = listNumbers(*m_textBuffer);
        joins = before->level == 0 && first.format == ListNumberFormat::Decimal &&
                first.suffix == suffix &&
                numbers[static_cast<size_t>(previous.blockNumber())] + 1 == start;
    }
    if (joins) {
        item.id = before->id;
        item.definition = before->definition;
    } else {
        item.id = newListId(*m_textBuffer);
        item.definition = ListDefinition::ofKind(*kind);
        if (*kind == ListKind::Numbered) {
            item.definition.levels[0].suffix = suffix;
            item.definition.levels[0].start = start;
        }
    }

    // An undo step of its own, as every autocorrection: undone right after, it brings back
    // what was typed with the cursor after it (its first undo item, undone last)
    const CursorPosition typedAt = m_cursorPosition;
    const CursorPosition changed{typedAt.paragraph, 0};
    QTextCursor cursor(block);
    cursor.beginEditBlock();
    m_textBuffer->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{typedAt, {}}] { m_stepCursor = state; }));
    cursor.setPosition(block.position() + static_cast<int>(typed.size()), QTextCursor::KeepAnchor);
    removeKeepingAnnotations(cursor);
    QTextBlockFormat format = block.blockFormat();
    setListItem(format, item);
    cursor.setBlockFormat(format);
    m_textBuffer->appendUndoItem(new CallbackUndoItem(
        [this, state = StepCursor{changed, {}}] { m_stepCursor = state; }));
    cursor.endEditBlock();
    m_learnableChange.reset();

    if (m_renderPipeline) {
        m_renderPipeline->markAllDirty();
    }
    finishEdit(createCursor(m_textBuffer.get(), changed));
    return true;
}

// =============================================================================
// A paragraph's own formatting
// =============================================================================

ParagraphFormat BookEditor::shownParagraphFormat() const
{
    if (!m_textBuffer) {
        return {};
    }
    const auto* layout =
        qobject_cast<const KalahariTextDocumentLayout*>(m_textBuffer->documentLayout());
    const QTextBlock block = m_textBuffer->findBlockByNumber(m_cursorPosition.paragraph);
    return layout && block.isValid() ? layout->shownParagraphFormat(block)
                                     : paragraphFormatOf(block.blockFormat());
}

void BookEditor::applyParagraphFormat(const ParagraphFormat& changes,
                                      std::optional<Qt::Alignment> alignment)
{
    if (refusesChange() || !m_textBuffer) {
        return;
    }
    const auto* layout =
        qobject_cast<const KalahariTextDocumentLayout*>(m_textBuffer->documentLayout());
    std::vector<std::pair<QTextBlock, QTextBlockFormat>> formats;
    for (const QTextBlock& block : selectedTextBlocks()) {
        QTextBlockFormat format = block.blockFormat();
        const ParagraphFormat style = layout ? layout->styleParagraphFormat(block) : ParagraphFormat{};
        ParagraphFormat own = paragraphFormatOf(format);
        // The indents of a list item are those of its level
        if (!isListItem(format)) {
            changePart(own.leftIndent, changes.leftIndent, style.leftIndent);
            changePart(own.firstLineIndent, changes.firstLineIndent, style.firstLineIndent);
        }
        changePart(own.rightIndent, changes.rightIndent, style.rightIndent);
        changePart(own.spaceBefore, changes.spaceBefore, style.spaceBefore);
        changePart(own.spaceAfter, changes.spaceAfter, style.spaceAfter);
        changePart(own.lineSpacing, changes.lineSpacing, style.lineSpacing);
        changePart(own.keepWithNext, changes.keepWithNext, style.keepWithNext);
        changePart(own.keepTogether, changes.keepTogether, style.keepTogether);
        changePart(own.pageBreakBefore, changes.pageBreakBefore, style.pageBreakBefore);
        changePart(own.widowControl, changes.widowControl, style.widowControl);
        setParagraphFormat(format, own);
        if (alignment) {
            format.setAlignment(*alignment);
        }
        formats.emplace_back(block, format);
    }
    setBlockFormats(formats);
}

}  // namespace kalahari::editor
