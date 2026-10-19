/// @file paragraph_styles.cpp
/// @brief The paragraph styles of a book and the objects of its text

#include <kalahari/editor/paragraph_styles.h>
#include <kalahari/core/book_type_package.h>
#include <kalahari/editor/annotation.h>
#include <QHash>
#include <QSet>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <optional>
#include <utility>

namespace kalahari::editor {

// =============================================================================
// The style of a paragraph
// =============================================================================

QString headingStyleId(int level) {
    return QStringLiteral("heading_%1").arg(level);
}

int headingLevelOf(const QString& styleId) {
    for (int level = 1; level <= HEADING_LEVELS; ++level) {
        if (styleId == headingStyleId(level)) {
            return level;
        }
    }
    return 0;
}

int headingLevelOfElement(QStringView element) {
    if (element.size() != 2 || element.at(0) != QLatin1Char('h')) {
        return 0;
    }
    const int level = element.at(1).digitValue();
    return level >= 1 && level <= HEADING_LEVELS ? level : 0;
}

QString paragraphStyleOf(const QTextBlockFormat& format) {
    const QString id = format.stringProperty(PARAGRAPH_STYLE_PROPERTY);
    return id.isEmpty() ? QString::fromLatin1(NORMAL_STYLE_ID) : id;
}

void setParagraphStyle(QTextBlockFormat& format, const QString& styleId) {
    if (styleId.isEmpty() || styleId == QLatin1String(NORMAL_STYLE_ID)) {
        format.clearProperty(PARAGRAPH_STYLE_PROPERTY);
    } else {
        format.setProperty(PARAGRAPH_STYLE_PROPERTY, styleId);
    }
}

// =============================================================================
// Objects of the text
// =============================================================================

TextObject textObjectOf(const QTextFormat& format) {
    switch (format.objectType()) {
        case HORIZONTAL_RULE_OBJECT: return TextObject::HorizontalRule;
        case SCENE_BREAK_OBJECT: return TextObject::SceneBreak;
        case PAGE_BREAK_OBJECT: return TextObject::PageBreak;
        default: return TextObject::None;
    }
}

QTextCharFormat objectCharFormat(TextObject object) {
    QTextCharFormat format;
    switch (object) {
        case TextObject::HorizontalRule: format.setObjectType(HORIZONTAL_RULE_OBJECT); break;
        case TextObject::SceneBreak: format.setObjectType(SCENE_BREAK_OBJECT); break;
        case TextObject::PageBreak: format.setObjectType(PAGE_BREAK_OBJECT); break;
        case TextObject::None: break;
    }
    return format;
}

TextObject blockObject(const QTextBlock& block) {
    // The object's character and the paragraph separator
    if (!block.isValid() || block.length() != 2) {
        return TextObject::None;
    }
    const QTextBlock::iterator it = block.begin();
    if (it.atEnd()) {
        return TextObject::None;
    }
    const QTextFragment fragment = it.fragment();
    if (fragment.length() != 1 || fragment.text().at(0) != QChar::ObjectReplacementCharacter) {
        return TextObject::None;
    }
    return textObjectOf(fragment.charFormat());
}

QString sceneBreakLookName(SceneBreakLook look) {
    switch (look) {
        case SceneBreakLook::Asterisks: return QStringLiteral("asterisks");
        case SceneBreakLook::Asterism: return QStringLiteral("asterism");
        case SceneBreakLook::Fleuron: return QStringLiteral("fleuron");
        case SceneBreakLook::Line: return QStringLiteral("line");
        case SceneBreakLook::Blank: return QStringLiteral("blank");
    }
    return QStringLiteral("asterisks");
}

SceneBreakLook sceneBreakLookFromName(const QString& name) {
    for (const SceneBreakLook look : {SceneBreakLook::Asterisks, SceneBreakLook::Asterism,
                                      SceneBreakLook::Fleuron, SceneBreakLook::Line,
                                      SceneBreakLook::Blank}) {
        if (name == sceneBreakLookName(look)) {
            return look;
        }
    }
    return SceneBreakLook::Asterisks;
}

QString sceneBreakText(SceneBreakLook look) {
    switch (look) {
        // Spaced as wide as a letter M: a break, not three words
        case SceneBreakLook::Asterisks: return QStringLiteral("*\u2003*\u2003*");
        case SceneBreakLook::Asterism: return QString(QChar(0x2042));
        case SceneBreakLook::Fleuron: return QString(QChar(0x2766));
        case SceneBreakLook::Line:
        case SceneBreakLook::Blank: break;
    }
    return {};
}

QString kmlElementOf(TextObject object) {
    switch (object) {
        case TextObject::HorizontalRule: return QStringLiteral("hr");
        case TextObject::SceneBreak: return QStringLiteral("scene-break");
        case TextObject::PageBreak: return QStringLiteral("page-break");
        case TextObject::None: break;
    }
    return {};
}

TextObject textObjectOfElement(const QString& element) {
    if (element == QLatin1String("hr")) {
        return TextObject::HorizontalRule;
    }
    if (element == QLatin1String("scene-break")) {
        return TextObject::SceneBreak;
    }
    if (element == QLatin1String("page-break")) {
        return TextObject::PageBreak;
    }
    return TextObject::None;
}

// =============================================================================
// Editing around objects
// =============================================================================

namespace {

/// @brief A character format for the start of a paragraph of text: without annotations and
///        without an object type
QTextCharFormat textCharFormat(QTextCharFormat format) {
    setAnnotations(format, {});
    format.clearProperty(QTextFormat::ObjectType);
    return format;
}

/// @brief The annotations on a place that stay when a block goes: those on its start and on
///        its characters
AnnotationList annotationsOfBlock(const QTextDocument& document, const QTextBlock& block) {
    AnnotationList staying;
    for (const Annotation& annotation : annotationsOf(block.charFormat())) {
        if (annotation.point) {
            staying = withAnnotation(staying, annotation);
        }
    }
    const int end = block.position() + block.length() - 1;
    for (const Annotation& annotation : annotationsLeavingWith(document, block.position(), end)) {
        staying = withAnnotation(staying, annotation);
    }
    return staying;
}

}  // namespace

QTextBlockFormat paragraphBlockFormat() {
    QTextBlockFormat format;
    format.setTopMargin(0);
    format.setBottomMargin(0);
    return format;
}

QTextBlockFormat objectBlockFormat() {
    return paragraphBlockFormat();
}

bool isObjectAt(const QTextDocument& document, int position) {
    if (position < 0 || position >= document.characterCount() - 1 ||
        document.characterAt(position) != QChar::ObjectReplacementCharacter) {
        return false;
    }
    QTextCursor cursor(const_cast<QTextDocument*>(&document));
    cursor.setPosition(position + 1);
    return textObjectOf(cursor.charFormat()) != TextObject::None;  // of the character before
}

bool isAtObject(const QTextCursor& cursor) {
    return !cursor.hasSelection() && blockObject(cursor.block()) != TextObject::None;
}

void separateObjects(QTextCursor& cursor, int from, int to, const QTextBlockFormat* tailFormat,
                     const QTextCharFormat* tailCharFormat) {
    QTextDocument& document = *cursor.document();
    const bool keepPosition = cursor.keepPositionOnInsert();
    QTextCursor edit(&document);
    int end = to;

    // The format of the text after an object, carried from the block it was split off from
    std::optional<std::pair<QTextBlockFormat, QTextCharFormat>> carried;
    for (QTextBlock block = document.findBlock(std::max(0, from));
         block.isValid() && block.position() <= end;) {
        const QString text = block.text();
        int split = -1;  // where in the block a paragraph break goes
        if (text.size() > 1) {
            for (int i = 0; i < text.size(); ++i) {
                if (isObjectAt(document, block.position() + i)) {
                    split = i > 0 ? i : 1;  // before the object, or after it at the block's start
                    break;
                }
            }
        }
        if (split < 0) {
            carried.reset();
            block = block.next();
            continue;
        }

        const int position = block.position() + split;
        const bool afterObject = split == 1 && isObjectAt(document, block.position());
        QTextBlockFormat format;
        QTextCharFormat charFormat;
        if (!afterObject || isObjectAt(document, position)) {
            // The object (or the next one) and what follows it get a block of their own;
            // the text after the objects later the format of the block they came from
            if (!carried && !afterObject) {
                carried.emplace(block.blockFormat(), textCharFormat(block.charFormat()));
            }
            format = objectBlockFormat();
        } else if (carried) {
            format = carried->first;
            charFormat = carried->second;
        } else if (tailFormat != nullptr) {
            format = *tailFormat;
            charFormat = textCharFormat(tailCharFormat != nullptr ? *tailCharFormat
                                                                  : QTextCharFormat());
        } else {
            format = block.blockFormat();
            charFormat = textCharFormat(block.charFormat());
        }

        // The editor's cursor at the break: after an object it goes on to the text after it
        cursor.setKeepPositionOnInsert(cursor.position() == position && !afterObject);
        edit.setPosition(position);
        edit.insertBlock(format, charFormat);
        cursor.setKeepPositionOnInsert(keepPosition);
        ++end;
        if (afterObject && !isObjectAt(document, edit.position())) {
            carried.reset();
        }
        block = edit.block();  // the new block, which may hold more objects
    }
}

void removeSelectionKeepingObjects(QTextCursor& cursor) {
    if (!cursor.hasSelection()) {
        return;
    }
    QTextDocument& document = *cursor.document();
    const QTextBlock first = document.findBlock(cursor.selectionStart());
    const bool fromObject = blockObject(first) != TextObject::None &&
                            cursor.selectionStart() == first.position();
    const QTextBlock last = document.findBlock(cursor.selectionEnd());
    const QTextBlockFormat tailFormat = last.blockFormat();
    const QTextCharFormat tailCharFormat = last.charFormat();

    cursor.beginEditBlock();
    cursor.removeSelectedText();
    if (fromObject && blockObject(cursor.block()) == TextObject::None &&
        !isObjectAt(document, cursor.position())) {
        // The text after the selection in its own paragraph's format
        cursor.setBlockFormat(tailFormat);
        cursor.setBlockCharFormat(textCharFormat(tailCharFormat));
    }
    separateObjects(cursor, cursor.position(), cursor.position(), &tailFormat, &tailCharFormat);
    cursor.endEditBlock();
}

bool leaveObject(QTextCursor& cursor) {
    if (!isAtObject(cursor)) {
        return false;
    }
    cursor.beginEditBlock();
    if (cursor.atBlockStart()) {
        // The object goes on to a new block; the one it leaves becomes the paragraph
        const QTextBlock block = cursor.block();
        cursor.insertBlock(block.blockFormat(), block.charFormat());
        cursor.movePosition(QTextCursor::PreviousBlock);
        cursor.setBlockFormat(paragraphBlockFormat());
        cursor.setBlockCharFormat(QTextCharFormat());
    } else {
        cursor.insertBlock(paragraphBlockFormat(), QTextCharFormat());
    }
    cursor.endEditBlock();
    return true;
}

void insertObject(QTextCursor& cursor, TextObject object, const QTextBlockFormat& nextFormat) {
    if (object == TextObject::None || cursor.hasSelection()) {
        return;
    }
    const QString character(QChar::ObjectReplacementCharacter);
    const QTextCharFormat format = objectCharFormat(object);
    const QTextBlock block = cursor.block();
    const QTextBlockFormat blockFormat = block.blockFormat();
    const QTextCharFormat blockCharFormat = block.charFormat();

    cursor.beginEditBlock();
    if (blockObject(block) != TextObject::None) {
        if (cursor.atBlockStart()) {
            // The new object takes the block; the old one goes on to a block after it
            cursor.insertText(character, format);
            cursor.insertBlock(blockFormat, blockCharFormat);
        } else {
            cursor.insertBlock(objectBlockFormat(), QTextCharFormat());
            cursor.insertText(character, format);
        }
        cursor.endEditBlock();
        return;
    }

    const int offset = cursor.positionInBlock();
    const int length = block.length() - 1;
    QTextBlockFormat tailFormat = blockFormat;
    QTextCharFormat tailCharFormat = blockCharFormat;
    if (offset > 0) {
        // The text before the cursor stays; the object starts a block
        if (offset == length) {
            tailFormat = nextFormat;
            tailCharFormat = QTextCharFormat();
        } else {
            tailCharFormat = textCharFormat(cursor.charFormat());
        }
        cursor.insertBlock(objectBlockFormat(), QTextCharFormat());
    }
    // At the start of a block: the object, then the rest of the block in a block of its own
    cursor.insertText(character, format);
    cursor.insertBlock(tailFormat, tailCharFormat);
    if (offset == 0) {
        // The paragraph's formats went on with its text
        QTextCursor objectCursor(cursor.block().previous());
        objectCursor.setBlockFormat(objectBlockFormat());
        objectCursor.setBlockCharFormat(QTextCharFormat());
    }
    cursor.endEditBlock();
}

void removeBlock(QTextCursor& cursor, const QTextBlock& block) {
    if (!block.isValid()) {
        return;
    }
    QTextDocument& document = *cursor.document();
    const QTextBlock next = block.next();
    const QTextBlock previous = block.previous();
    const int end = block.position() + block.length() - 1;
    const AnnotationList staying = annotationsOfBlock(document, block);

    cursor.beginEditBlock();
    if (next.isValid()) {
        // Removed with the break after it, the block would give its formats to the next one
        const QTextBlockFormat format = next.blockFormat();
        const QTextCharFormat charFormat = next.charFormat();
        cursor.setPosition(block.position());
        cursor.setPosition(next.position(), QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.setBlockFormat(format);
        cursor.setBlockCharFormat(charFormat);
    } else if (previous.isValid()) {
        cursor.setPosition(previous.position() + previous.length() - 1);
        cursor.setPosition(end, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
    } else {
        cursor.setPosition(block.position());
        cursor.setPosition(end, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.setBlockFormat(paragraphBlockFormat());
        cursor.setBlockCharFormat(QTextCharFormat());
    }
    anchorToPlace(document, cursor.position(), staying);
    cursor.endEditBlock();
}

// =============================================================================
// The styles of a book
// =============================================================================

ParagraphStyleSet::ParagraphStyleSet() {
    ParagraphStyle normal;
    normal.id = QString::fromLatin1(NORMAL_STYLE_ID);
    m_styles.append(normal);
}

ParagraphStyleSet ParagraphStyleSet::fromPackage(
    const QList<core::PackageParagraphStyle>& styles, const QString& language) {
    QHash<QString, const core::PackageParagraphStyle*> byId;
    for (const core::PackageParagraphStyle& style : styles) {
        byId.insert(style.id, &style);
    }

    // A style's properties: those of its base styles, the nearest last (a circle, which the
    // registry does not load, ends where it closes)
    const auto resolved = [&byId](const core::PackageParagraphStyle& style) {
        QList<const core::PackageParagraphStyle*> chain;
        QSet<QString> seen;
        for (const core::PackageParagraphStyle* current = &style;
             current != nullptr && !seen.contains(current->id);
             current = byId.value(current->baseStyle, nullptr)) {
            seen.insert(current->id);
            chain.prepend(current);
        }
        QVariantMap properties;
        for (const core::PackageParagraphStyle* link : chain) {
            for (auto it = link->properties.cbegin(); it != link->properties.cend(); ++it) {
                properties.insert(it.key(), it.value());
            }
        }
        return properties;
    };

    ParagraphStyleSet set;
    set.m_styles.clear();
    for (const core::PackageParagraphStyle& style : styles) {
        ParagraphStyle resolvedStyle;
        resolvedStyle.id = style.id;
        resolvedStyle.name = style.name.text(language);
        if (resolvedStyle.name.isEmpty()) {
            resolvedStyle.name = style.id;
        }
        resolvedStyle.nextStyle = style.nextStyle;
        resolvedStyle.properties = resolved(style);
        set.m_styles.append(resolvedStyle);
    }
    if (!set.contains(QString::fromLatin1(NORMAL_STYLE_ID))) {
        set.m_styles.prepend(ParagraphStyleSet().normal());
    }
    return set;
}

const ParagraphStyle* ParagraphStyleSet::find(const QString& id) const {
    for (const ParagraphStyle& style : m_styles) {
        if (style.id == id) {
            return &style;
        }
    }
    return nullptr;
}

const ParagraphStyle& ParagraphStyleSet::normal() const {
    // The constructor and fromPackage() always give the set a style Normal
    return *find(QString::fromLatin1(NORMAL_STYLE_ID));
}

const ParagraphStyle& ParagraphStyleSet::shownStyle(const QString& id) const {
    const ParagraphStyle* style = find(id);
    return style != nullptr ? *style : normal();
}

QString ParagraphStyleSet::nextStyleOf(const QString& id) const {
    const ParagraphStyle* style = find(id);
    if (style == nullptr) {
        return id;  // a style the book does not have stays
    }
    return style->nextStyle.isEmpty() || !contains(style->nextStyle) ? style->id
                                                                      : style->nextStyle;
}

QVariant ParagraphStyleSet::property(const QString& id, const QString& name) const {
    return shownStyle(id).properties.value(name);
}

bool ParagraphStyleSet::isChecked(const QString& id) const {
    const QVariant check = property(id, QStringLiteral("spellCheck"));
    return !check.isValid() || check.toBool();
}

}  // namespace kalahari::editor
