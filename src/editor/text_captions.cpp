// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file text_captions.cpp
/// @brief Numbered captions, cross-references to them and the list of figures

#include "kalahari/editor/text_captions.h"
#include "kalahari/editor/text_footnotes.h"
#include "kalahari/editor/kml_format_registry.h"
#include "kalahari/editor/paragraph_styles.h"
#include "kalahari/editor/text_images.h"
#include "kalahari/editor/text_tables.h"

#include <QCoreApplication>
#include <QRandomGenerator>
#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextTable>
#include <QXmlStreamAttributes>
#include <QXmlStreamReader>

#include <array>

namespace kalahari::editor {

namespace {

/// The properties of the fields
constexpr std::array<int, 5> FIELD_PROPERTIES = {
    CAPTION_LABEL_PROPERTY, CAPTION_ID_PROPERTY, REFERENCE_TARGET_PROPERTY,
    REFERENCE_SHOWS_PROPERTY, REFERENCE_PLAIN_PROPERTY};

/// The names of what a cross-reference shows, in the order of ReferenceShows
constexpr std::array<const char*, 4> SHOWS_NAMES = {"number", "label", "caption", "position"};

/// The names of the number formats, in the order of ListNumberFormat (without Bullet)
constexpr std::array<const char*, 5> FORMAT_NAMES = {"1", "a", "A", "i", "I"};

/// The hexadecimal digits of a new id
constexpr int ID_DIGITS = 6;
constexpr quint32 ID_RANGE = 0x1000000;

/// The elements of KML that are paragraphs
bool isParagraphElement(QStringView name)
{
    return name == u"p" || name == u"paragraph" || name == u"li" || name == u"h1" ||
           name == u"h2" || name == u"h3";
}

/// Add text to the pieces, to the text piece at their end when there is one
void appendText(std::vector<CaptionPiece>& pieces, const QString& text)
{
    if (text.isEmpty()) {
        return;
    }
    if (!pieces.empty() && pieces.back().kind == CaptionPiece::Kind::Text) {
        pieces.back().text += text;
        return;
    }
    CaptionPiece piece;
    piece.text = text;
    pieces.push_back(piece);
}

/// A piece of a field
CaptionPiece numberPiece(const QString& id)
{
    CaptionPiece piece;
    piece.kind = CaptionPiece::Kind::Number;
    piece.text = id;
    return piece;
}

CaptionPiece referencePiece(const CrossReference& reference)
{
    CaptionPiece piece;
    piece.kind = CaptionPiece::Kind::Reference;
    piece.reference = reference;
    return piece;
}

/// The text of a block's characters: a line break or a tab a space
QString plainPart(const QString& text)
{
    QString part = text;
    part.replace(QChar(QChar::LineSeparator), QLatin1Char(' '));
    part.replace(QLatin1Char('\t'), QLatin1Char(' '));
    return part;
}

/// The words of a cross-reference that shows where its caption is
QString aboveText()
{
    //: A cross-reference to a caption earlier in the book. In Polish: powyżej
    return QCoreApplication::translate("CaptionNumbers", "above");
}

QString belowText()
{
    //: A cross-reference to a caption later in the book. In Polish: poniżej
    return QCoreApplication::translate("CaptionNumbers", "below");
}

/// The text a broken field shows
QString brokenText()
{
    return QStringLiteral("?");
}

}  // namespace

// =============================================================================
// Numbering
// =============================================================================

const QStringList& captionSeparators()
{
    static const QStringList separators = {QStringLiteral("."), QString(QChar(0x2013)),
                                           QStringLiteral(":")};
    return separators;
}

CaptionNumbering normalizedNumbering(const CaptionNumbering& numbering)
{
    CaptionNumbering normalized = numbering;
    if (normalized.format == ListNumberFormat::Bullet) {
        normalized.format = ListNumberFormat::Decimal;
    }
    if (!captionSeparators().contains(normalized.separator)) {
        normalized.separator = captionSeparators().first();
    }
    normalized.withChapter = normalized.perChapter && normalized.withChapter;
    return normalized;
}

QString captionNumberText(int number, const CaptionNumbering& numbering, int chapter)
{
    const CaptionNumbering normalized = normalizedNumbering(numbering);
    QString own = listNumberText(number, normalized.format);
    if (normalized.withChapter && chapter > 0) {
        return QString::number(chapter) + normalized.separator + own;
    }
    return own;
}

QString numberFormatName(ListNumberFormat format)
{
    const auto index = static_cast<size_t>(format);
    return index < FORMAT_NAMES.size() ? QLatin1String(FORMAT_NAMES.at(index))
                                       : QLatin1String(FORMAT_NAMES.front());
}

ListNumberFormat numberFormatOfName(const QString& name)
{
    for (size_t i = 0; i < FORMAT_NAMES.size(); ++i) {
        if (name == QLatin1String(FORMAT_NAMES.at(i))) {
            return static_cast<ListNumberFormat>(i);
        }
    }
    return ListNumberFormat::Decimal;
}

QString referenceShowsName(ReferenceShows shows)
{
    const auto index = static_cast<size_t>(shows);
    return index < SHOWS_NAMES.size() ? QLatin1String(SHOWS_NAMES.at(index))
                                      : QLatin1String(SHOWS_NAMES.front());
}

ReferenceShows referenceShowsOfName(const QString& name)
{
    for (size_t i = 0; i < SHOWS_NAMES.size(); ++i) {
        if (name == QLatin1String(SHOWS_NAMES.at(i))) {
            return static_cast<ReferenceShows>(i);
        }
    }
    return ReferenceShows::Number;
}

// =============================================================================
// The fields in the formats
// =============================================================================

std::optional<CaptionNumberField> captionNumberOf(const QTextFormat& format)
{
    if (format.objectType() != CAPTION_NUMBER_OBJECT) {
        return std::nullopt;
    }
    return CaptionNumberField{format.stringProperty(CAPTION_LABEL_PROPERTY),
                              format.stringProperty(CAPTION_ID_PROPERTY)};
}

void setCaptionNumber(QTextCharFormat& format, const CaptionNumberField& field)
{
    clearField(format);
    format.setObjectType(CAPTION_NUMBER_OBJECT);
    format.setProperty(CAPTION_LABEL_PROPERTY, normalizedLabel(field.label));
    format.setProperty(CAPTION_ID_PROPERTY, field.id);
}

std::optional<CrossReference> crossReferenceOf(const QTextFormat& format)
{
    if (format.objectType() != CROSS_REFERENCE_OBJECT) {
        return std::nullopt;
    }
    CrossReference reference;
    reference.target = format.stringProperty(REFERENCE_TARGET_PROPERTY);
    const int shows = format.intProperty(REFERENCE_SHOWS_PROPERTY);
    if (shows >= 0 && shows < static_cast<int>(SHOWS_NAMES.size())) {
        reference.shows = static_cast<ReferenceShows>(shows);
    }
    reference.link = !format.boolProperty(REFERENCE_PLAIN_PROPERTY);
    return reference;
}

void setCrossReference(QTextCharFormat& format, const CrossReference& reference)
{
    clearField(format);
    format.setObjectType(CROSS_REFERENCE_OBJECT);
    format.setProperty(REFERENCE_TARGET_PROPERTY, reference.target);
    // The rest only where it is not the default: formats of the same reference compare equal
    if (reference.shows != ReferenceShows::Number) {
        format.setProperty(REFERENCE_SHOWS_PROPERTY, static_cast<int>(reference.shows));
    }
    if (!reference.link) {
        format.setProperty(REFERENCE_PLAIN_PROPERTY, true);
    }
}

bool isFieldFormat(const QTextFormat& format)
{
    const int type = format.objectType();
    return type == CAPTION_NUMBER_OBJECT || type == CROSS_REFERENCE_OBJECT;
}

void clearField(QTextCharFormat& format)
{
    if (isFieldFormat(format)) {
        format.clearProperty(QTextFormat::ObjectType);
    }
    for (const int property : FIELD_PROPERTIES) {
        format.clearProperty(property);
    }
}

bool hasFields(const QTextBlock& block)
{
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
        if (it.fragment().isValid() && isFieldFormat(it.fragment().charFormat())) {
            return true;
        }
    }
    return false;
}

namespace {

/// Whether a block is a caption: a paragraph in the style Caption
bool isCaptionBlock(const QTextBlock& block)
{
    return block.isValid() &&
           paragraphStyleOf(block.blockFormat()) == QLatin1String(CAPTION_STYLE_ID);
}

/// Whether two blocks are next to each other in one place: both outside tables, or in one
/// cell of a table
bool inOnePlace(const QTextBlock& one, const QTextBlock& other)
{
    const QTextTable* table = tableOf(one);
    if (table != tableOf(other)) {
        return false;
    }
    return table == nullptr || table->cellAt(one.position()) == table->cellAt(other.position());
}

/// The characters that part a caption's number from its text: "Figure 3. The bridge"
bool isNumberSeparator(QChar character)
{
    return character == QLatin1Char('.') || character == QLatin1Char(':') ||
           character == QChar(0x2013) || character == QChar(0x2014) || character.isSpace();
}

}  // namespace

QTextBlock captionBlockOf(const QTextBlock& imageBlock)
{
    if (!imageBlock.isValid()) {
        return QTextBlock();
    }
    const QTextBlock next = imageBlock.next();
    if (isCaptionBlock(next) && !isCaptionAbove(next.blockFormat()) && inOnePlace(imageBlock, next)) {
        return next;
    }
    const QTextBlock previous = imageBlock.previous();
    if (isCaptionBlock(previous) && isCaptionAbove(previous.blockFormat()) &&
        inOnePlace(imageBlock, previous)) {
        return previous;
    }
    return QTextBlock();
}

QTextBlock imageBlockOfCaption(const QTextBlock& captionBlock)
{
    if (!isCaptionBlock(captionBlock)) {
        return QTextBlock();
    }
    const QTextBlock image =
        isCaptionAbove(captionBlock.blockFormat()) ? captionBlock.next() : captionBlock.previous();
    return image.isValid() && hasImages(image) && captionBlockOf(image) == captionBlock
               ? image
               : QTextBlock();
}

CaptionParts captionParts(const QTextBlock& block)
{
    CaptionParts parts;
    parts.textAt = block.position();
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment fragment = it.fragment();
        if (!fragment.isValid()) {
            continue;
        }
        if (const std::optional<CaptionNumberField> number = captionNumberOf(fragment.charFormat())) {
            parts.numberAt = fragment.position();
            parts.number = *number;
            break;
        }
    }
    if (parts.numberAt < 0) {
        return parts;
    }
    // After the number: its separator ". ", ": ", " – "
    const QString text = block.text();
    int offset = parts.numberAt - block.position() + 1;
    while (offset < text.size() && isNumberSeparator(text.at(offset))) {
        ++offset;
    }
    parts.textAt = block.position() + offset;
    return parts;
}

QStringList captionIdsOf(const QTextBlock& block)
{
    QStringList ids;
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment fragment = it.fragment();
        if (!fragment.isValid()) {
            continue;
        }
        const std::optional<CaptionNumberField> number = captionNumberOf(fragment.charFormat());
        const QString id = number ? number->id : QString();
        if (id.isEmpty()) {
            continue;
        }
        for (int i = 0; i < fragment.length(); ++i) {
            ids << id;
        }
    }
    return ids;
}

bool isCaptionAbove(const QTextBlockFormat& format)
{
    return format.boolProperty(CAPTION_ABOVE_PROPERTY);
}

void setCaptionAbove(QTextBlockFormat& format, bool above)
{
    if (above) {
        format.setProperty(CAPTION_ABOVE_PROPERTY, true);
    } else {
        format.clearProperty(CAPTION_ABOVE_PROPERTY);
    }
}

QString normalizedLabel(const QString& label)
{
    QString kept = label;
    for (QChar& ch : kept) {
        if (ch.unicode() < 0x20 || ch.unicode() == 0x7F || ch == QChar::LineSeparator ||
            ch == QChar::ParagraphSeparator || ch == QChar::ObjectReplacementCharacter) {
            ch = QLatin1Char(' ');
        }
    }
    return kept.simplified().left(MAX_CAPTION_LABEL).trimmed();
}

QString newCaptionId(const QSet<QString>& taken)
{
    QString id;
    do {
        const quint32 value = QRandomGenerator::global()->bounded(ID_RANGE);
        id = QStringLiteral("c%1").arg(value, ID_DIGITS, 16, QLatin1Char('0'));
    } while (taken.contains(id));
    return id;
}

// =============================================================================
// The captions of a text
// =============================================================================

std::vector<CaptionEntry> captionEntries(const QTextDocument& document)
{
    std::vector<CaptionEntry> entries;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        // The pieces of the paragraph, and its numbers
        std::vector<CaptionPiece> pieces;
        std::vector<CaptionEntry> numbers;
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid()) {
                continue;
            }
            const QTextCharFormat format = fragment.charFormat();
            const QString text = fragment.text();
            if (format.objectType() == QTextFormat::NoObject) {
                appendText(pieces, plainPart(text));
                continue;
            }
            const std::optional<CaptionNumberField> number = captionNumberOf(format);
            const std::optional<CrossReference> reference = crossReferenceOf(format);
            const CaptionNumberField* field = number ? &*number : nullptr;
            const CrossReference* named = reference ? &*reference : nullptr;
            for (qsizetype i = 0; i < text.size(); ++i) {
                if (text.at(i) != QChar::ObjectReplacementCharacter) {
                    appendText(pieces, plainPart(text.mid(i, 1)));
                } else if (field != nullptr) {
                    pieces.push_back(numberPiece(field->id));
                    numbers.push_back({field->id, field->label, {},
                                       fragment.position() + static_cast<int>(i)});
                } else if (named != nullptr) {
                    pieces.push_back(referencePiece(*named));
                }
                // Another object (an image) is no part of the caption's text
            }
        }
        for (CaptionEntry& entry : numbers) {
            entry.pieces = pieces;
            entries.push_back(std::move(entry));
        }
    }
    return entries;
}

std::vector<CaptionEntry> captionEntriesOfKml(const QString& kml)
{
    std::vector<CaptionEntry> entries;
    if (!kml.contains(QLatin1String("<caption-number"))) {
        return entries;
    }
    QXmlStreamReader reader(kml);
    std::vector<CaptionPiece> pieces;
    std::vector<CaptionEntry> numbers;
    bool inParagraph = false;
    const auto finishParagraph = [&]() {
        for (CaptionEntry& entry : numbers) {
            entry.pieces = pieces;
            entries.push_back(std::move(entry));
        }
        numbers.clear();
        pieces.clear();
    };
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) {
            const QStringView name = reader.name();
            if (name == u"annotations" || name == u"img" || name == u"endnote" ||
                (name == u"footnote" && !isOldFootnote(reader.attributes()))) {
                // The writer's comments, the images and the notes are no part of a caption's
                // text
                reader.skipCurrentElement();
            } else if (isParagraphElement(name)) {
                finishParagraph();
                inParagraph = true;
            } else if (!inParagraph) {
                continue;
            } else if (name == u"caption-number") {
                if (const std::optional<CaptionNumberField> field =
                        captionNumberOfKml(reader.attributes())) {
                    pieces.push_back(numberPiece(field->id));
                    numbers.push_back({field->id, field->label, {}, -1});
                }
                reader.skipCurrentElement();
            } else if (name == u"cross-reference") {
                if (const std::optional<CrossReference> reference =
                        crossReferenceOfKml(reader.attributes())) {
                    pieces.push_back(referencePiece(*reference));
                }
                reader.skipCurrentElement();
            } else if (name == u"br") {
                appendText(pieces, QStringLiteral(" "));
                reader.skipCurrentElement();
            }
        } else if (reader.isEndElement()) {
            if (isParagraphElement(reader.name())) {
                finishParagraph();
                inParagraph = false;
            }
        } else if (reader.isCharacters() && inParagraph) {
            appendText(pieces, plainPart(reader.text().toString()));
        }
    }
    finishParagraph();
    return entries;
}

QHash<QString, int> crossReferenceCountsOfKml(const QString& kml)
{
    QHash<QString, int> counts;
    if (!kml.contains(QLatin1String("<cross-reference"))) {
        return counts;
    }
    QXmlStreamReader reader(kml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement()) {
            continue;
        }
        if (reader.name() == u"annotations") {
            reader.skipCurrentElement();  // the writer's comments are not the text
        } else if (reader.name() == u"cross-reference") {
            if (const std::optional<CrossReference> reference =
                    crossReferenceOfKml(reader.attributes())) {
                ++counts[reference->target];
            }
            reader.skipCurrentElement();
        }
    }
    return counts;
}

QHash<QString, int> crossReferenceCounts(const QTextDocument& document)
{
    QHash<QString, int> counts;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid()) {
                continue;
            }
            if (const std::optional<CrossReference> reference =
                    crossReferenceOf(fragment.charFormat())) {
                counts[reference->target] += fragment.length();
            }
        }
    }
    return counts;
}

QSet<QString> captionIds(const QTextDocument& document)
{
    QSet<QString> ids;
    for (const CaptionEntry& entry : captionEntries(document)) {
        ids.insert(entry.id);
    }
    return ids;
}

// =============================================================================
// The book around a text
// =============================================================================

CaptionNumbering CaptionContext::numberingOf(const QString& label) const
{
    return normalizedNumbering(numbering.value(label));
}

CaptionNumbers::CaptionNumbers(const QTextDocument& document, const CaptionContext& context)
    : m_before(context.before), m_after(context.after)
{
    // Through the whole book a label goes on from the numbers of the texts before
    QHash<QString, int> counted;
    for (const BookCaption& caption : m_before) {
        ++counted[caption.label];
    }
    QHash<QString, int> counters;
    for (const CaptionEntry& entry : captionEntries(document)) {
        const CaptionNumbering numbering = context.numberingOf(entry.label);
        auto counter = counters.find(entry.label);
        if (counter == counters.end()) {
            const auto given = context.counted.constFind(entry.label);
            counter = counters.insert(entry.label,
                                      given != context.counted.cend() ? *given
                                      : numbering.perChapter          ? 0
                                                                      : counted.value(entry.label));
        }
        ++*counter;
        BookCaption caption;
        caption.id = entry.id;
        caption.label = entry.label;
        caption.number = captionNumberText(*counter, numbering, context.chapter);
        caption.pieces = entry.pieces;
        caption.position = entry.position;
        m_ownAt.insert(entry.position, static_cast<int>(m_own.size()));
        if (!entry.id.isEmpty() && !m_ownById.contains(entry.id)) {
            m_ownById.insert(entry.id, static_cast<int>(m_own.size()));
        }
        m_own.push_back(std::move(caption));
    }
    for (size_t i = 0; i < m_before.size(); ++i) {
        if (!m_beforeById.contains(m_before[i].id)) {
            m_beforeById.insert(m_before[i].id, static_cast<int>(i));
        }
    }
    for (size_t i = 0; i < m_after.size(); ++i) {
        if (!m_afterById.contains(m_after[i].id)) {
            m_afterById.insert(m_after[i].id, static_cast<int>(i));
        }
    }
}

std::vector<BookCaption> CaptionNumbers::book() const
{
    std::vector<BookCaption> all;
    all.reserve(m_before.size() + m_own.size() + m_after.size());
    all.insert(all.end(), m_before.begin(), m_before.end());
    all.insert(all.end(), m_own.begin(), m_own.end());
    all.insert(all.end(), m_after.begin(), m_after.end());
    return all;
}

const BookCaption* CaptionNumbers::find(const QString& id) const
{
    if (id.isEmpty()) {
        return nullptr;
    }
    if (const auto own = m_ownById.constFind(id); own != m_ownById.cend()) {
        return &m_own[static_cast<size_t>(*own)];
    }
    if (const auto before = m_beforeById.constFind(id); before != m_beforeById.cend()) {
        return &m_before[static_cast<size_t>(*before)];
    }
    if (const auto after = m_afterById.constFind(id); after != m_afterById.cend()) {
        return &m_after[static_cast<size_t>(*after)];
    }
    return nullptr;
}

FieldText CaptionNumbers::fieldText(int position, const QTextFormat& format) const
{
    if (format.objectType() == CAPTION_NUMBER_OBJECT) {
        const auto own = m_ownAt.constFind(position);
        if (own == m_ownAt.cend()) {
            return {brokenText(), false};  // not counted yet (in the middle of a change)
        }
        return {m_own[static_cast<size_t>(*own)].number, false};
    }
    if (const std::optional<CrossReference> reference = crossReferenceOf(format)) {
        return referenceText(*reference, position);
    }
    return {};
}

FieldText CaptionNumbers::referenceText(const CrossReference& reference, int position) const
{
    const BookCaption* caption = find(reference.target);
    if (caption == nullptr) {
        return {brokenText(), true};
    }
    switch (reference.shows) {
        case ReferenceShows::Number:
            break;
        case ReferenceShows::LabelAndNumber:
            return {caption->label + QLatin1Char(' ') + caption->number, false};
        case ReferenceShows::Caption:
            return {captionText(*caption), false};
        case ReferenceShows::Position: {
            bool above = false;
            if (caption->elementId.isEmpty() && caption->position >= 0 &&
                m_ownById.contains(caption->id)) {
                above = caption->position < position;
            } else {
                above = m_beforeById.contains(caption->id);
            }
            return {above ? aboveText() : belowText(), false};
        }
    }
    return {caption->number, false};
}

QString CaptionNumbers::captionText(const BookCaption& caption, int* numberStart,
                                    int* numberLength) const
{
    QString text;
    int start = -1;
    int length = 0;
    for (const CaptionPiece& piece : caption.pieces) {
        switch (piece.kind) {
            case CaptionPiece::Kind::Text:
                text += piece.text;
                break;
            case CaptionPiece::Kind::Number: {
                if (piece.text == caption.id && start < 0) {
                    start = static_cast<int>(text.size());
                    length = static_cast<int>(caption.number.size());
                    text += caption.number;
                } else {
                    const BookCaption* other = find(piece.text);
                    text += other != nullptr ? other->number : brokenText();
                }
                break;
            }
            case CaptionPiece::Kind::Reference: {
                // A caption's own cross-references show at most a label and a number: a
                // caption shown in a caption might show itself again
                CrossReference shown = piece.reference;
                if (shown.shows == ReferenceShows::Caption ||
                    shown.shows == ReferenceShows::Position) {
                    shown.shows = ReferenceShows::LabelAndNumber;
                }
                text += referenceText(shown, -1).text;
                break;
            }
        }
    }
    if (numberStart != nullptr) {
        *numberStart = start;
    }
    if (numberLength != nullptr) {
        *numberLength = length;
    }
    return text;
}

std::vector<FigureListRow> CaptionNumbers::figureList() const
{
    std::vector<FigureListRow> rows;
    for (const BookCaption& caption : book()) {
        FigureListRow row;
        row.id = caption.id;
        row.elementId = caption.elementId;
        int start = -1;
        int length = 0;
        row.text = captionText(caption, &start, &length);
        row.numberStart = start;
        row.numberLength = start >= 0 ? length : 0;
        rows.push_back(std::move(row));
    }
    return rows;
}

void CaptionNumbers::shift(int from, int delta)
{
    if (delta == 0) {
        return;
    }
    m_ownAt.clear();
    for (size_t i = 0; i < m_own.size(); ++i) {
        BookCaption& caption = m_own[i];
        if (caption.position >= from) {
            caption.position += delta;
        }
        m_ownAt.insert(caption.position, static_cast<int>(i));
    }
}

std::vector<QString> fieldTexts(const QTextDocument& document, const CaptionNumbers& numbers,
                                int from, int to)
{
    std::vector<QString> texts;
    QString list;
    bool listKnown = false;
    for (QTextBlock block = document.findBlock(from); block.isValid() && block.position() < to;
         block = block.next()) {
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid() || fragment.position() + fragment.length() <= from) {
                continue;
            }
            if (fragment.position() >= to) {
                break;
            }
            const QTextCharFormat format = fragment.charFormat();
            const bool field = isFieldFormat(format);
            if (!field && textObjectOf(format) != TextObject::FigureList) {
                continue;
            }
            const QString text = fragment.text();
            for (int i = 0; i < text.size(); ++i) {
                const int position = fragment.position() + i;
                if (position < from || position >= to ||
                    text.at(i) != QChar::ObjectReplacementCharacter) {
                    continue;
                }
                if (field) {
                    texts.push_back(numbers.fieldText(position, format).text);
                    continue;
                }
                if (!listKnown) {
                    QStringList rows;
                    for (const FigureListRow& row : numbers.figureList()) {
                        rows << row.text;
                    }
                    list = rows.join(QLatin1Char('\n'));
                    listKnown = true;
                }
                texts.push_back(list);
            }
        }
    }
    return texts;
}

// =============================================================================
// KML
// =============================================================================

QString captionNumberKml(const CaptionNumberField& field)
{
    QString kml = QStringLiteral("<caption-number label=\"") +
                  KmlFormatRegistry::escapeXml(normalizedLabel(field.label)) + QLatin1Char('"');
    if (!field.id.isEmpty()) {
        kml += QStringLiteral(" id=\"") + KmlFormatRegistry::escapeXml(field.id) + QLatin1Char('"');
    }
    return kml + QStringLiteral("/>");
}

QString crossReferenceKml(const CrossReference& reference)
{
    QString kml = QStringLiteral("<cross-reference to=\"") +
                  KmlFormatRegistry::escapeXml(reference.target) + QLatin1Char('"');
    if (reference.shows != ReferenceShows::Number) {
        kml += QStringLiteral(" show=\"") + referenceShowsName(reference.shows) + QLatin1Char('"');
    }
    if (!reference.link) {
        kml += QStringLiteral(" link=\"false\"");
    }
    return kml + QStringLiteral("/>");
}

std::optional<CaptionNumberField> captionNumberOfKml(const QXmlStreamAttributes& attributes)
{
    CaptionNumberField field;
    field.label = normalizedLabel(attributes.value(QStringLiteral("label")).toString());
    field.id = attributes.value(QStringLiteral("id")).toString().trimmed();
    if (field.label.isEmpty()) {
        return std::nullopt;
    }
    return field;
}

std::optional<CrossReference> crossReferenceOfKml(const QXmlStreamAttributes& attributes)
{
    CrossReference reference;
    reference.target = attributes.value(QStringLiteral("to")).toString().trimmed();
    if (reference.target.isEmpty()) {
        return std::nullopt;
    }
    reference.shows =
        referenceShowsOfName(attributes.value(QStringLiteral("show")).toString().trimmed());
    const QString link = attributes.value(QStringLiteral("link")).toString().trimmed().toLower();
    reference.link = link != QLatin1String("false") && link != QLatin1String("0");
    return reference;
}

}  // namespace kalahari::editor
