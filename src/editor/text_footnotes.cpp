// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file text_footnotes.cpp
/// @brief Footnotes and endnotes: a number in the text with the text of its note inside it

#include "kalahari/editor/text_footnotes.h"
#include "kalahari/editor/kml_document_model.h"
#include "kalahari/editor/kml_serializer.h"
#include "kalahari/editor/text_link.h"
#include "kalahari/editor/text_lists.h"

#include <QFont>
#include <QRandomGenerator>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextFragment>
#include <QXmlStreamAttributes>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <algorithm>
#include <array>
#include <utility>

namespace kalahari::editor {

namespace {

/// The digits of an id after its letter, and their range
constexpr int ID_DIGITS = 6;
constexpr quint32 ID_RANGE = 0x1000000;

/// The marks of the format Symbols, each number past them the same marks more times
constexpr std::array<char16_t, 4> NOTE_SYMBOLS = {u'*', u'†', u'‡', u'§'};

/// The format of a list's number a note's format is
ListNumberFormat listFormatOf(NoteNumberFormat format) {
    switch (format) {
        case NoteNumberFormat::LowerAlpha:
            return ListNumberFormat::LowerAlpha;
        case NoteNumberFormat::UpperAlpha:
            return ListNumberFormat::UpperAlpha;
        case NoteNumberFormat::LowerRoman:
            return ListNumberFormat::LowerRoman;
        case NoteNumberFormat::UpperRoman:
            return ListNumberFormat::UpperRoman;
        case NoteNumberFormat::Decimal:
        case NoteNumberFormat::Symbols:
            break;
    }
    return ListNumberFormat::Decimal;
}

/// Whether a note's text is written as <p> elements: its first element is a <p>
bool hasParagraphElements(const QString& content) {
    const QString start = content.trimmed();
    return start.startsWith(QLatin1String("<p>")) || start.startsWith(QLatin1String("<p "));
}

/// The notes of a paragraph, added to @p notes
void collectNotes(const QTextBlock& block, std::vector<NotePlace>& notes) {
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
        const QTextFragment fragment = it.fragment();
        if (!fragment.isValid()) {
            continue;
        }
        const std::optional<Footnote> found = footnoteOf(fragment.charFormat());
        if (!found) {
            continue;
        }
        const Footnote& note = *found;
        // Numbers of one format next to each other are one fragment (a note pasted twice)
        const QString text = fragment.text();
        for (int i = 0; i < text.size(); ++i) {
            if (text.at(i) == QChar::ObjectReplacementCharacter) {
                notes.push_back({fragment.position() + i, note});
            }
        }
    }
}

}  // namespace

NoteNumberFormat noteNumberFormatOf(const QString& code) {
    static constexpr std::array<std::pair<const char*, NoteNumberFormat>, 6> formats{{
        {"1", NoteNumberFormat::Decimal},    {"a", NoteNumberFormat::LowerAlpha},
        {"A", NoteNumberFormat::UpperAlpha}, {"i", NoteNumberFormat::LowerRoman},
        {"I", NoteNumberFormat::UpperRoman}, {"*", NoteNumberFormat::Symbols}}};
    for (const auto& [name, format] : formats) {
        if (code == QLatin1String(name)) {
            return format;
        }
    }
    return NoteNumberFormat::Decimal;
}

QString noteNumberText(int number, NoteNumberFormat format) {
    if (number < 1) {
        return QString();
    }
    if (format == NoteNumberFormat::Symbols) {
        const auto index = static_cast<size_t>((number - 1) % static_cast<int>(NOTE_SYMBOLS.size()));
        const int times = (number - 1) / static_cast<int>(NOTE_SYMBOLS.size()) + 1;
        return QString(times, QChar(NOTE_SYMBOLS.at(index)));
    }
    return listNumberText(number, listFormatOf(format));
}

// =============================================================================
// The notes in the formats
// =============================================================================

std::optional<Footnote> footnoteOf(const QTextFormat& format) {
    if (format.objectType() != FOOTNOTE_OBJECT) {
        return std::nullopt;
    }
    Footnote note;
    note.id = format.stringProperty(FOOTNOTE_ID_PROPERTY);
    note.kind = format.boolProperty(FOOTNOTE_ENDNOTE_PROPERTY) ? NoteKind::Endnote
                                                               : NoteKind::Footnote;
    note.content = format.stringProperty(FOOTNOTE_CONTENT_PROPERTY);
    return note;
}

void setFootnote(QTextCharFormat& format, const Footnote& note) {
    format.setObjectType(FOOTNOTE_OBJECT);
    format.setProperty(FOOTNOTE_ID_PROPERTY, note.id);
    if (note.kind == NoteKind::Endnote) {
        format.setProperty(FOOTNOTE_ENDNOTE_PROPERTY, true);
    } else {
        format.clearProperty(FOOTNOTE_ENDNOTE_PROPERTY);
    }
    format.setProperty(FOOTNOTE_CONTENT_PROPERTY, note.content);
}

bool isFootnoteFormat(const QTextFormat& format) {
    return format.objectType() == FOOTNOTE_OBJECT;
}

void clearFootnote(QTextCharFormat& format) {
    if (format.objectType() == FOOTNOTE_OBJECT) {
        format.setObjectType(QTextFormat::NoObject);
    }
    format.clearProperty(FOOTNOTE_ID_PROPERTY);
    format.clearProperty(FOOTNOTE_ENDNOTE_PROPERTY);
    format.clearProperty(FOOTNOTE_CONTENT_PROPERTY);
}

bool hasNotes(const QTextBlock& block) {
    for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
        if (it.fragment().isValid() && isFootnoteFormat(it.fragment().charFormat())) {
            return true;
        }
    }
    return false;
}

QString newFootnoteId(const QSet<QString>& taken) {
    QString id;
    do {
        const quint32 value = QRandomGenerator::global()->bounded(ID_RANGE);
        id = QStringLiteral("f%1").arg(value, ID_DIGITS, 16, QLatin1Char('0'));
    } while (taken.contains(id));
    return id;
}

// =============================================================================
// The notes of a text
// =============================================================================

std::vector<NotePlace> notesIn(const QTextDocument& document) {
    std::vector<NotePlace> notes;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        collectNotes(block, notes);
    }
    return notes;
}

std::vector<NotePlace> notesIn(const QTextBlock& block) {
    std::vector<NotePlace> notes;
    if (block.isValid()) {
        collectNotes(block, notes);
    }
    return notes;
}

std::optional<NotePlace> findNote(const QTextDocument& document, const QString& id) {
    if (id.isEmpty()) {
        return std::nullopt;
    }
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (!fragment.isValid()) {
                continue;
            }
            const QTextCharFormat format = fragment.charFormat();
            if (isFootnoteFormat(format) && format.stringProperty(FOOTNOTE_ID_PROPERTY) == id) {
                if (const std::optional<Footnote> note = footnoteOf(format)) {
                    return NotePlace{fragment.position(), *note};
                }
            }
        }
    }
    return std::nullopt;
}

std::optional<Footnote> noteAtPosition(const QTextDocument& document, int position) {
    if (position < 0 || position >= document.characterCount() - 1) {
        return std::nullopt;
    }
    QTextCursor cursor(const_cast<QTextDocument*>(&document));
    cursor.setPosition(position + 1);  // the format of the character before it
    return footnoteOf(cursor.charFormat());
}

bool setNoteContentAt(QTextDocument& document, int position, const QString& content) {
    std::optional<Footnote> note = noteAtPosition(document, position);
    if (!note) {
        return false;
    }
    QTextCursor cursor(&document);
    cursor.setPosition(position);
    cursor.setPosition(position + 1, QTextCursor::KeepAnchor);
    QTextCharFormat format = cursor.charFormat();
    note->content = content;
    setFootnote(format, *note);
    cursor.setCharFormat(format);
    return true;
}

QSet<QString> noteIds(const QTextDocument& document) {
    QSet<QString> ids;
    for (const NotePlace& place : notesIn(document)) {
        ids.insert(place.note.id);
    }
    return ids;
}

FootnoteNumbers::FootnoteNumbers(const QTextDocument& document, const FootnoteContext& context)
    : m_notes(notesIn(document))
    , m_context(context)
{
    int footnotes = context.footnotesBefore;
    int endnotes = context.endnotesBefore;
    m_numbers.reserve(m_notes.size());
    for (const NotePlace& place : m_notes) {
        m_numbers.push_back(place.note.kind == NoteKind::Endnote
                                ? noteNumberText(++endnotes, context.endnoteFormat)
                                : noteNumberText(++footnotes, context.footnoteFormat));
    }
}

const NotePlace* FootnoteNumbers::noteAt(int position) const {
    const auto it = std::lower_bound(
        m_notes.begin(), m_notes.end(), position,
        [](const NotePlace& place, int value) { return place.position < value; });
    return it != m_notes.end() && it->position == position ? &*it : nullptr;
}

QString FootnoteNumbers::numberAt(int position) const {
    const NotePlace* place = noteAt(position);
    return place != nullptr ? m_numbers.at(static_cast<size_t>(place - m_notes.data())) : QString();
}

QString FootnoteNumbers::numberOf(const QString& id) const {
    for (size_t i = 0; i < m_notes.size(); ++i) {
        if (m_notes[i].note.id == id) {
            return m_numbers[i];
        }
    }
    return QString();
}

void FootnoteNumbers::shift(int from, int delta) {
    for (NotePlace& place : m_notes) {
        if (place.position >= from) {
            place.position += delta;
        }
    }
}

// =============================================================================
// The text of a note
// =============================================================================

bool NoteParagraph::operator==(const NoteParagraph& other) const {
    if (text != other.text || formats.size() != other.formats.size()) {
        return false;
    }
    for (size_t i = 0; i < formats.size(); ++i) {
        if (formats[i].start != other.formats[i].start || formats[i].end != other.formats[i].end ||
            formats[i].format != other.formats[i].format) {
            return false;
        }
    }
    return true;
}

QString noteParagraphsKml(const QString& content) {
    return hasParagraphElements(content) ? content
                                         : QStringLiteral("<p>") + content + QStringLiteral("</p>");
}

std::vector<NoteParagraph> noteParagraphs(const QString& content) {
    KmlDocumentModel model;
    model.loadKml(noteParagraphsKml(content));
    std::vector<NoteParagraph> paragraphs;
    for (size_t i = 0; i < model.paragraphCount(); ++i) {
        if (model.paragraphObject(i) != TextObject::None) {
            continue;  // a note has no objects between its paragraphs
        }
        NoteParagraph paragraph;
        paragraph.text = model.paragraphText(i);
        for (FormatRun run : model.paragraphFormats(i)) {
            run.format = noteFormatOf(run.format);
            paragraph.formats.push_back(run);
        }
        paragraphs.push_back(std::move(paragraph));
    }
    if (paragraphs.empty()) {
        paragraphs.emplace_back();
    }
    return paragraphs;
}

QString notePlainText(const QString& content) {
    QStringList lines;
    for (const NoteParagraph& paragraph : noteParagraphs(content)) {
        QString line = paragraph.text;
        line.replace(QChar::LineSeparator, QLatin1Char(' '));
        line.remove(QChar::ObjectReplacementCharacter);
        lines << line;
    }
    return lines.join(QLatin1Char('\n'));
}

bool isEmptyNoteContent(const QString& content) {
    return notePlainText(content).trimmed().isEmpty();
}

QString noteText(const QString& content) {
    QStringList lines;
    for (const NoteParagraph& paragraph : noteParagraphs(content)) {
        lines << QString(paragraph.text).remove(QChar::ObjectReplacementCharacter);
    }
    return lines.join(QLatin1Char('\n'));
}

QString replacedNoteContent(const QString& content,
                            const std::vector<std::pair<int, int>>& ranges, const QString& text) {
    QTextDocument document;
    setNoteContent(document, content);
    const int end = std::max(0, document.characterCount() - 1);
    QTextCursor cursor(&document);
    cursor.beginEditBlock();
    for (auto range = ranges.rbegin(); range != ranges.rend(); ++range) {
        const int from = std::clamp(range->first, 0, end);
        const int to = std::clamp(range->first + range->second, from, end);
        // The format of its first character: the cursor has the one before it
        cursor.setPosition(to > from ? from + 1 : from);
        const QTextCharFormat format = cursor.charFormat();
        cursor.setPosition(from);
        cursor.setPosition(to, QTextCursor::KeepAnchor);
        cursor.insertText(text, format);
    }
    cursor.endEditBlock();
    return noteContentOf(document);
}

QTextCharFormat noteFormatOf(const QTextCharFormat& format) {
    QTextCharFormat kept;
    if (format.fontWeight() > QFont::Normal) {
        kept.setFontWeight(QFont::Bold);
    }
    if (format.fontItalic()) {
        kept.setFontItalic(true);
    }
    if (format.fontUnderline()) {
        kept.setFontUnderline(true);
    }
    if (format.fontStrikeOut()) {
        kept.setFontStrikeOut(true);
    }
    const QTextCharFormat::VerticalAlignment alignment = format.verticalAlignment();
    if (alignment == QTextCharFormat::AlignSuperScript ||
        alignment == QTextCharFormat::AlignSubScript) {
        kept.setVerticalAlignment(alignment);
    }
    const QString link = linkAddressOf(format);
    if (!link.isEmpty()) {
        setLinkAddress(kept, link);
    }
    return kept;
}

void setNoteContent(QTextDocument& document, const QString& content) {
    document.clear();
    QTextCursor cursor(&document);
    cursor.beginEditBlock();
    bool first = true;
    for (const NoteParagraph& paragraph : noteParagraphs(content)) {
        if (!first) {
            cursor.insertBlock();
        }
        first = false;
        size_t written = 0;
        const auto insert = [&](size_t end, const QTextCharFormat& format) {
            if (end > written) {
                const QString part = paragraph.text.mid(static_cast<qsizetype>(written),
                                                        static_cast<qsizetype>(end - written));
                cursor.insertText(QString(part).remove(QChar::ObjectReplacementCharacter), format);
                written = end;
            }
        };
        for (const FormatRun& run : paragraph.formats) {
            insert(run.start, QTextCharFormat());
            insert(run.end, run.format);
        }
        insert(static_cast<size_t>(paragraph.text.size()), QTextCharFormat());
    }
    cursor.endEditBlock();
}

QString noteContentOf(const QTextDocument& document) {
    // A copy with only the formatting a note keeps; empty paragraphs at its end go
    QTextDocument kept;
    QTextCursor cursor(&kept);
    bool first = true;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        if (!first) {
            cursor.insertBlock(QTextBlockFormat(), QTextCharFormat());
        }
        first = false;
        for (QTextBlock::iterator it = block.begin(); !it.atEnd(); ++it) {
            const QTextFragment fragment = it.fragment();
            if (fragment.isValid()) {
                cursor.insertText(fragment.text().remove(QChar::ObjectReplacementCharacter),
                                  noteFormatOf(fragment.charFormat()));
            }
        }
    }
    QStringList paragraphs;
    const KmlSerializer serializer;
    for (QTextBlock block = kept.begin(); block.isValid(); block = block.next()) {
        paragraphs << serializer.blockToKml(block);
    }
    while (paragraphs.size() > 1 && paragraphs.last().isEmpty()) {
        paragraphs.removeLast();
    }
    if (paragraphs.size() <= 1) {
        return paragraphs.isEmpty() ? QString() : paragraphs.first();
    }
    QString content;
    for (const QString& paragraph : paragraphs) {
        content += QStringLiteral("<p>") + paragraph + QStringLiteral("</p>");
    }
    return content;
}

// =============================================================================
// KML
// =============================================================================

QString noteKml(const Footnote& note) {
    const QString name = note.kind == NoteKind::Endnote ? QStringLiteral("endnote")
                                                        : QStringLiteral("footnote");
    QString id = note.id;
    id.replace(QLatin1Char('&'), QStringLiteral("&amp;"))
        .replace(QLatin1Char('<'), QStringLiteral("&lt;"))
        .replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    return QLatin1Char('<') + name + QStringLiteral(" id=\"") + id + QStringLiteral("\">") +
           note.content + QStringLiteral("</") + name + QLatin1Char('>');
}

bool isOldFootnote(const QXmlStreamAttributes& attributes) {
    return attributes.hasAttribute(QStringLiteral("number"));
}

Footnote readNoteElement(QXmlStreamReader& reader) {
    Footnote note;
    note.id = reader.attributes().value(QStringLiteral("id")).toString().trimmed();
    note.kind = reader.name() == u"endnote" ? NoteKind::Endnote : NoteKind::Footnote;

    // The inside of the element written again as it is read: its formatting, its paragraphs
    QXmlStreamWriter writer(&note.content);
    int depth = 1;
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement()) {
            ++depth;
            writer.writeStartElement(reader.name().toString());
            writer.writeAttributes(reader.attributes());
        } else if (reader.isEndElement()) {
            if (--depth == 0) {
                break;
            }
            writer.writeEndElement();
        } else if (reader.isCharacters()) {
            writer.writeCharacters(reader.text().toString());
        }
    }
    return note;
}

std::vector<Footnote> notesOfKml(const QString& kml) {
    std::vector<Footnote> notes;
    if (!kml.contains(QLatin1String("<footnote")) && !kml.contains(QLatin1String("<endnote"))) {
        return notes;
    }
    QXmlStreamReader reader(kml);
    QSet<QString> ids;
    while (!reader.atEnd()) {
        reader.readNext();
        if (!reader.isStartElement()) {
            continue;
        }
        const QStringView name = reader.name();
        if (name == u"annotations") {
            reader.skipCurrentElement();  // the writer's comments have no notes
        } else if (name == u"endnote" ||
                   (name == u"footnote" && !isOldFootnote(reader.attributes()))) {
            Footnote note = readNoteElement(reader);
            if (note.id.isEmpty() || ids.contains(note.id)) {
                note.id = newFootnoteId(ids);
            }
            ids.insert(note.id);
            notes.push_back(std::move(note));
        }
    }
    return notes;
}

}  // namespace kalahari::editor
