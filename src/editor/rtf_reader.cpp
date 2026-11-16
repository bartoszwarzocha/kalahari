// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file rtf_reader.cpp
/// @brief Import of RTF: the texts of Scrivener, the files of Word, WordPad and TextEdit
///
/// RTF is groups ({...}) of control words (\b, \par), control symbols (\~, \'e9) and text. A
/// group keeps the formatting of the group around it and gives it back at its end; a group
/// that starts with a destination (\footnote, \pict, {\*\...}) is a part of its own: a note,
/// a picture, or something the program leaves out. The reader walks the bytes once, with a
/// stack of the states of the open groups, and writes the paragraphs into the document, or
/// into the note or the annotation it is in (a stack of sinks).
///
/// RTF repeats the formatting of a paragraph's style in the paragraph and its text; at the
/// end the reader takes out what the style already has, and the font and the size of most
/// of the text when the file has no styles (Scrivener's and TextEdit's texts), so that the
/// text keeps only the formatting of its own.

#include "kalahari/editor/book_import.h"

#include <QByteArrayView>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImageReader>
#include <QRegularExpression>
#include <QSet>
#include <QStringDecoder>

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace kalahari::editor {

namespace {

constexpr double TWIPS_PER_POINT = 20.0;
constexpr double TWIPS_PER_CENTIMETRE = 1440.0 / 2.54;
constexpr double SINGLE_LINE = 240.0;  ///< \sl of single spacing with \slmult1
constexpr double HALF_POINTS = 2.0;    ///< \fs is in half points
constexpr double PERCENT = 100.0;
constexpr double HUNDREDTHS = 100.0;   ///< The widths of images, in hundredths of a centimetre
constexpr double SAME = 0.01;          ///< Lengths closer than this are the same
constexpr int DEFAULT_CODE_PAGE = 1252;
constexpr int MAC_ROMAN = 10000;
constexpr int OEM_UNITED_STATES = 437;
constexpr int OEM_MULTILINGUAL = 850;
constexpr int SYMBOL_CHARSET = 2;
constexpr int MAX_CONTROL_LETTERS = 32;
constexpr int MAX_PARAMETER_DIGITS = 10;
constexpr int MAX_DEPTH = 512;        ///< Groups deeper than this are read as their parent
constexpr int UNICODE_RANGE = 65536;  ///< \u of a negative number is this much more
constexpr int OUTLINE_LEVELS = 9;
constexpr int DEFAULT_RAISE = 6;      ///< \up and \dn without a number: 3 points
constexpr int DECIMAL = 10;
constexpr char16_t NOT_SIGN = 0x00AC;        ///< After TextEdit's attachment
constexpr char16_t SYMBOL_BULLET = 0x00B7;   ///< The bullet of the font Symbol
constexpr char16_t SECTION_SIGN = 0x00A7;    ///< The square of the font Wingdings
constexpr char16_t PRIVATE_BULLET = 0xF0B7;  ///< The bullet of Symbol as Word writes it in \u
constexpr char16_t PRIVATE_SQUARE = 0xF0A7;  ///< The square of Wingdings as Word writes it in \u
constexpr char16_t NO_BREAK_SPACE = 0x00A0;
constexpr char16_t NO_BREAK_HYPHEN = 0x2011;

/// The prefix of Scrivener's links to its comments and footnotes
constexpr QLatin1String LINKED_NOTE("scrivcmt://");

/// What the text of a group is
enum class Destination {
    Body,              ///< Text of the document, of a note or of an annotation
    Skip,              ///< Left out, with its groups
    Info,              ///< \info: its groups are the properties of the document
    FontTable,         ///< \fonttbl: \fN and the names
    ColorTable,        ///< \colortbl: \red, \green, \blue and ';'
    StyleSheet,        ///< \stylesheet: its groups are the styles
    Style,             ///< One style of the stylesheet: its formatting and its name
    Title,             ///< \info \title
    Author,            ///< \info \author
    Picture,           ///< \pict: the bytes of a picture
    FieldInstruction,  ///< \fldinst: HYPERLINK "address"
    ListText,          ///< \listtext, \pntext: the number or the bullet of a list item
    Attachment,        ///< TextEdit's \NeXTGraphic: the name of the file
    AnnotationAuthor,  ///< Word's \atnauthor
};

/// What the end of a group does
enum class GroupEnd {
    Nothing,
    Note,              ///< A note ends: its number goes into the text
    Annotation,        ///< An annotation ends: its place goes into the text
    Picture,           ///< A picture ends: it goes into the text
    Style,             ///< A style of the stylesheet ends
    Field,             ///< The result of a field ends: no more link, no more linked comment
    FieldInstruction,  ///< The instruction of a field ends
    ListText,          ///< The number of a list item ends
    Attachment,        ///< TextEdit's attachment ends
};

/// The paragraph's formatting while it is written
struct ParagraphState {
    QString style;                     ///< "s<N>"; empty: none
    int outline = 0;                   ///< \outlinelevel + 1; 0: none
    QString align = QStringLiteral("left");  ///< RTF's paragraph is aligned left unless it says
    ParagraphFormat own;
    std::optional<int> line;           ///< \sl
    bool lineMultiple = false;         ///< \slmult1
    bool inTable = false;              ///< \intbl
    int list = 0;                      ///< \ls: its list; 0: none
    int listLevel = 0;                 ///< \ilvl
};

/// The state of an open group
struct State {
    Destination destination = Destination::Body;
    GroupEnd end = GroupEnd::Nothing;
    ImportCharFormat format;
    ParagraphState paragraph;
    int font = -1;
    bool hidden = false;    ///< \v
    int unicodeSkip = 1;    ///< \uc: the characters after \u that stand for it
    QString link;           ///< The address the text links to
    int annotation = -1;    ///< The comment the text is in (a linked comment); -1: none
    QString linkedFootnote; ///< GroupEnd::Field: the id of the footnote of Scrivener after it
};

/// A font of the font table
struct Font {
    QString name;
    int charset = -1;
    int codePage = 0;  ///< \cpg; 0: of the charset
};

/// A place the paragraphs go: the document, a note or an annotation
struct Sink {
    enum class Target { Body, Note, Annotation };
    Target target = Target::Body;
    std::vector<ImportBlock> blocks;  ///< Of a note or an annotation: its paragraphs
    ImportBlock paragraph;            ///< The paragraph being written
    bool endnote = false;             ///< A note of \ftnalt
    bool scrivener = false;           ///< Scrivener's inline note or annotation
    bool marker = false;              ///< Scrivener's group only starts it: a group
                                      ///< \Scrv_..._end ends it
};

/// A picture of \pict: its format, its size and its bytes
struct PictureEntry {
    QString suffix;      ///< "png", "jpg"; empty: a format the program does not read
    int goalWidth = 0;   ///< \picwgoal, twips
    int scale = 100;     ///< \picscalex, percent
    QByteArray hex;
    QByteArray binary;
};

/// A style of the stylesheet while it is read
struct StyleEntry {
    QString key;
    QString name;
    QString basedOn;
    QString next;
    bool paragraph = true;
};

/// A cell of a row's definition (\clvmgf ... \cellx)
struct CellDefinition {
    int right = 0;                ///< \cellx: its right edge, twips
    bool merged = false;          ///< \clmrg: a part of the cell before it
    bool verticalFirst = false;   ///< \clvmgf: the first of cells merged down
    bool verticalMerged = false;  ///< \clvmrg: a part of the cell above it
    QString background;
    QString valign;
};

/// A row's definition (\trowd)
struct RowDefinition {
    int left = 0;  ///< \trleft
    bool header = false;
    double height = 0.0;
    QString align;
    std::vector<CellDefinition> cells;
    CellDefinition next;  ///< Of the cell the next \cellx ends
};

/// The code page of a font's \fcharset; 0: the document's
int codePageOfCharset(int charset) {
    static const std::map<int, int> pages{
        {77, MAC_ROMAN}, {128, 932}, {129, 949}, {134, 936}, {136, 950}, {161, 1253},
        {162, 1254},     {163, 1258}, {177, 1255}, {178, 1256}, {186, 1257}, {204, 1251},
        {222, 874},      {238, 1250}, {255, OEM_UNITED_STATES},
    };
    const auto it = pages.find(charset);
    return it == pages.end() ? 0 : it->second;
}

/// The color of a table's entry as the formats keep it; empty for black and white
QString colorOf(int red, int green, int blue) {
    QString color = QStringLiteral("#%1%2%3")
                              .arg(red, 2, 16, QLatin1Char('0'))
                              .arg(green, 2, 16, QLatin1Char('0'))
                              .arg(blue, 2, 16, QLatin1Char('0'));
    if (color == QLatin1String("#000000") || color == QLatin1String("#ffffff")) {
        return QString();
    }
    return color;
}

/// The address of a field's instruction HYPERLINK "address" (also with \l "bookmark")
QString linkOfInstruction(const QString& instruction) {
    if (!instruction.startsWith(QLatin1String("HYPERLINK"), Qt::CaseInsensitive)) {
        return QString();
    }
    const qsizetype open = instruction.indexOf(QLatin1Char('"'));
    const qsizetype close = open < 0 ? -1 : instruction.indexOf(QLatin1Char('"'), open + 1);
    if (close > open) {
        return instruction.mid(open + 1, close - open - 1).trimmed();
    }
    return instruction.mid(QStringLiteral("HYPERLINK").size()).trimmed();
}

/// Whether a field's instruction is Word's table of contents (the book has its own)
bool isTableOfContents(const QString& instruction) {
    return instruction.startsWith(QLatin1String("TOC"), Qt::CaseInsensitive) &&
           (instruction.size() == 3 || instruction.at(3).isSpace());
}

/// The destinations the reader leaves out with their text (also those without \*)
bool isSkippedDestination(const QByteArray& word) {
    static const QSet<QByteArray> skipped{
        "colorschememapping", "datastore", "docvar", "ftnsep", "ftnsepc", "aftnsep",
        "aftnsepc", "ftncn", "aftncn", "generator", "latentstyles", "listtable",
        "listoverridetable", "listpicture", "mmathPr", "nonshppict", "objdata", "objclass",
        "operator", "pgdsctbl", "private", "revtbl", "rsidtbl", "themedata", "txe", "userprops",
        "xmlnstbl", "xe", "tc", "filetbl", "company", "comment", "doccomm", "keywords",
        "subject", "category", "manager", "hlinkbase", "buptim", "creatim", "printim", "revtim",
        "atnid", "atnref", "atndate", "atnicn", "atrfstart", "atrfend", "bkmkstart", "bkmkend",
        "pn", "pntxta", "pntxtb", "shpinst", "sp", "sn", "sv", "picprop", "blipuid",
        "expandedcolortbl", "nesttableprops",
    };
    return skipped.contains(word);
}

/// The headers and footers of the pages
bool isHeaderOrFooter(const QByteArray& word) {
    static const QSet<QByteArray> parts{
        "header", "headerl", "headerr", "headerf", "footer", "footerl", "footerr", "footerf",
    };
    return parts.contains(word);
}

/// Whether @p word starts a part of its own: a destination
bool isDestination(const QByteArray& word) {
    static const QSet<QByteArray> destinations{
        "fonttbl", "colortbl", "stylesheet", "info", "title", "author", "footnote",
        "annotation", "atnauthor", "pict", "shppict", "fldinst", "fldrslt", "listtext",
        "pntext", "NeXTGraphic", "object", "shp",
    };
    return destinations.contains(word) || isHeaderOrFooter(word) || word.startsWith("Scrv_") ||
           isSkippedDestination(word);
}

/// The family of a font's face as Cocoa names it ("Palatino-Italic": "Palatino"), so that the
/// italic and bold text of a family is in its family
QString familyOfFace(const QString& name) {
    static const QSet<QString> faces{
        QStringLiteral("Roman"),        QStringLiteral("Regular"),    QStringLiteral("Italic"),
        QStringLiteral("Bold"),         QStringLiteral("BoldItalic"), QStringLiteral("Oblique"),
        QStringLiteral("BoldOblique"),  QStringLiteral("Light"),      QStringLiteral("LightItalic"),
        QStringLiteral("Medium"),       QStringLiteral("MediumItalic"), QStringLiteral("Semibold"),
        QStringLiteral("SemiboldItalic"), QStringLiteral("Book"),     QStringLiteral("BookItalic"),
        QStringLiteral("It"),           QStringLiteral("BoldIt"),
    };
    const qsizetype dash = name.lastIndexOf(QLatin1Char('-'));
    if (dash > 0 && faces.contains(name.mid(dash + 1))) {
        return name.left(dash);
    }
    return name;
}

bool isLetter(char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
}

bool isDigit(char ch) {
    return ch >= '0' && ch <= '9';
}

bool isHexDigit(char ch) {
    return isDigit(ch) || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

/// Whether the program reads images of files with @p suffix (not EMF and WMF)
bool isReadableImage(const QString& suffix) {
    const QByteArray format = suffix.toLower().toLatin1();
    if (format == "emf" || format == "wmf") {
        return false;
    }
    static const QList<QByteArray> readable = QImageReader::supportedImageFormats();
    return readable.contains(format);
}

/// The value of a Roman number ("iv": 4); 0: not one
int romanValue(const QString& text) {
    static const QHash<QChar, int> digits{
        {u'i', 1}, {u'v', 5}, {u'x', 10}, {u'l', 50}, {u'c', 100}, {u'd', 500}, {u'm', 1000},
    };
    int value = 0;
    int largest = 0;
    for (auto it = text.crbegin(); it != text.crend(); ++it) {
        const int digit = digits.value(it->toLower(), 0);
        if (digit == 0) {
            return 0;
        }
        value += digit < largest ? -digit : digit;
        largest = std::max(largest, digit);
    }
    return value;
}

/// The bullet of the book's lists for a list item's mark; @p fallback when it is none of them
QString bulletOf(QChar mark, const QString& fallback) {
    switch (mark.unicode()) {
        case BULLET_DISC:
        case BULLET_CIRCLE:
        case BULLET_SQUARE:
        case BULLET_DASH:
        case BULLET_CHECK:
        case BULLET_ARROW:
            return QString(mark);
        case SYMBOL_BULLET:
        case PRIVATE_BULLET:
            return QString(QChar(BULLET_DISC));
        case u'o':  // Courier New's "o": Word's bullet of the second level
            return QString(QChar(BULLET_CIRCLE));
        case SECTION_SIGN:
        case PRIVATE_SQUARE:
            return QString(QChar(BULLET_SQUARE));
        case u'-':
            return QString(QChar(BULLET_DASH));
        default:
            return fallback;
    }
}

/// The format of a level of a list, of the mark of its first item ("1.", "a)", "iv.", "•")
ListLevel listLevelOf(const QString& text, int level) {
    static const QRegularExpression pattern(QStringLiteral("^\\(?([0-9]+|[A-Za-z]+)([.)])?\\)?$"));
    const QString mark = text.simplified().remove(QLatin1Char(' '));
    const QRegularExpressionMatch match = pattern.match(mark);
    const QString number = match.captured(1);
    // A letter alone is a bullet of a font ("o" of Courier New)
    if (!match.hasMatch() || (number.at(0).isLetter() && match.captured(2).isEmpty())) {
        ListLevel bullet = ListDefinition::defaultLevel(ListKind::Bulleted, level);
        bullet.bullet = bulletOf(mark.isEmpty() ? QChar() : mark.at(0), bullet.bullet);
        return bullet;
    }
    ListLevel numbered = ListDefinition::defaultLevel(ListKind::Numbered, level);
    if (!match.captured(2).isEmpty()) {
        numbered.suffix = match.captured(2);
    }
    const bool upper = number.at(0).isUpper();
    const int roman = romanValue(number);
    if (number.at(0).isDigit()) {
        numbered.format = ListNumberFormat::Decimal;
        numbered.start = std::clamp(number.toInt(), MIN_LIST_NUMBER, MAX_LIST_NUMBER);
    } else if (roman > 0 &&
               (number.size() > 1 || number.compare(QLatin1String("i"), Qt::CaseInsensitive) == 0)) {
        numbered.format = upper ? ListNumberFormat::UpperRoman : ListNumberFormat::LowerRoman;
        numbered.start = std::min(roman, MAX_LIST_NUMBER);
    } else {
        numbered.format = upper ? ListNumberFormat::UpperAlpha : ListNumberFormat::LowerAlpha;
        numbered.start = number.size() == 1 ? number.at(0).toLower().unicode() - u'a' + 1 : 1;
    }
    return numbered;
}

/// The line spacing of \sl and \slmult
LineSpacing lineSpacingOf(int line, bool multiple) {
    if (multiple) {
        return LineSpacing{LineSpacingRule::Multiple, std::abs(line) / SINGLE_LINE};
    }
    return LineSpacing{line > 0 ? LineSpacingRule::AtLeast : LineSpacingRule::Exactly,
                       std::abs(line) / TWIPS_PER_POINT};
}

/// The width of an image of so many twips, in centimetres rounded to hundredths
double centimetresOf(double twips) {
    return std::round(twips / TWIPS_PER_CENTIMETRE * HUNDREDTHS) / HUNDREDTHS;
}

/// The value most of the text has
template <typename Key>
Key mostCommon(const std::map<Key, qsizetype>& counts) {
    const auto best = std::max_element(counts.begin(), counts.end(), [](const auto& a, const auto& b) {
        return a.second < b.second;
    });
    return best == counts.end() ? Key() : best->first;
}

/// The paragraphs of @p from, also those in the cells of its tables
void collectParagraphs(std::vector<ImportBlock>& from, std::vector<ImportBlock*>& into) {
    for (ImportBlock& block : from) {
        if (block.kind != ImportBlock::Kind::Table || !block.table) {
            into.push_back(&block);
            continue;
        }
        for (ImportRow& row : block.table->rows) {
            for (ImportCell& cell : row.cells) {
                collectParagraphs(cell.blocks, into);
            }
        }
    }
}

/// The length of a paragraph's own formatting without the style's (@p name in its properties)
void resetSame(std::optional<qreal>& value, const QVariantMap& properties, const char* name) {
    if (value && std::abs(*value - properties.value(QLatin1String(name), 0.0).toDouble()) < SAME) {
        value.reset();
    }
}

/// A switch of a paragraph's own formatting without the style's
void resetSame(std::optional<bool>& value, const QVariantMap& properties, const char* name,
               bool byDefault = false) {
    if (value && *value == properties.value(QLatin1String(name), byDefault).toBool()) {
        value.reset();
    }
}

/// The paragraph's formatting without what its style has
void withoutStyleFormatting(ImportBlock& block, const QVariantMap& properties) {
    const QString font = properties.value(QStringLiteral("fontFamily")).toString();
    const double size = properties.value(QStringLiteral("fontSize")).toDouble();
    for (ImportRun& run : block.runs) {
        ImportCharFormat& format = run.format;
        if (!font.isEmpty() && format.font == font) {
            format.font.clear();
        }
        if (size > 0.0 && std::abs(format.size - size) < SAME) {
            format.size = 0.0;
        }
        format.bold = format.bold && !properties.value(QStringLiteral("bold")).toBool();
        format.italic = format.italic && !properties.value(QStringLiteral("italic")).toBool();
        format.underline =
            format.underline && !properties.value(QStringLiteral("underline")).toBool();
    }
    ParagraphFormat& own = block.own;
    resetSame(own.leftIndent, properties, "leftMargin");
    resetSame(own.rightIndent, properties, "rightMargin");
    resetSame(own.firstLineIndent, properties, "firstLineIndent");
    resetSame(own.spaceBefore, properties, "spaceBefore");
    resetSame(own.spaceAfter, properties, "spaceAfter");
    resetSame(own.keepWithNext, properties, "keepWithNext");
    resetSame(own.keepTogether, properties, "keepTogether");
    resetSame(own.pageBreakBefore, properties, "pageBreakBefore");
    resetSame(own.widowControl, properties, "widowControl", true);
    if (own.lineSpacing && own.lineSpacing->rule == LineSpacingRule::Multiple &&
        std::abs(own.lineSpacing->value -
                 properties.value(QStringLiteral("lineHeight"), 1.0).toDouble()) < SAME) {
        own.lineSpacing.reset();
    }
    const QString align = properties.value(QStringLiteral("alignment")).toString();
    if (block.align == align || (block.align == QLatin1String("left") && align.isEmpty())) {
        block.align.clear();
    }
}

/// The reader of one RTF document
class RtfReader {
public:
    RtfReader(const QByteArray& data, const RtfReadOptions& options)
        : m_data(data), m_options(options) {
        m_states.emplace_back();
        m_sinks.emplace_back();
    }

    ImportDocument read() {
        while (m_pos < m_data.size() && !m_finished) {
            const char ch = m_data.at(m_pos++);
            switch (ch) {
                case '{':
                    openGroup();
                    break;
                case '}':
                    closeGroup();
                    break;
                case '\\':
                    readControl();
                    break;
                case '\r':
                case '\n':
                    break;
                case '\t':
                    m_ignorable = false;
                    if (!skipUnicodeFallback()) {
                        addTab();
                    }
                    break;
                default:
                    m_ignorable = false;
                    addText(ch);
                    break;
            }
        }
        finish();
        return std::move(m_document);
    }

private:
    State& state() { return m_states.back(); }
    Sink& sink() { return m_sinks.back(); }

    // -------------------------------------------------------------------------
    // Groups
    // -------------------------------------------------------------------------

    void openGroup() {
        m_ignorable = false;
        if (m_states.size() >= MAX_DEPTH) {
            ++m_tooDeep;
            return;
        }
        State next = state();
        next.end = GroupEnd::Nothing;
        next.linkedFootnote.clear();
        // The groups of the stylesheet are its styles
        if (next.destination == Destination::StyleSheet) {
            next.destination = Destination::Style;
            next.end = GroupEnd::Style;
            next.format = ImportCharFormat();
            next.paragraph = ParagraphState();
            m_style = StyleEntry();
        }
        m_states.push_back(std::move(next));
    }

    void closeGroup() {
        m_ignorable = false;
        m_unicodeLeft = 0;
        if (m_tooDeep > 0) {
            --m_tooDeep;
            return;
        }
        if (m_states.size() <= 1) {
            return;  // a '}' too many
        }
        const GroupEnd end = state().end;
        // The last paragraph of the document, of a note or of an annotation, without \par
        if ((m_states.size() == 2 || end == GroupEnd::Note || end == GroupEnd::Annotation) &&
            !sink().paragraph.runs.empty()) {
            endParagraph();
        }
        const State ended = std::move(m_states.back());
        m_states.pop_back();
        endGroup(ended);
        endInformation(ended.destination);
        m_finished = m_states.size() == 1;
    }

    void endGroup(const State& ended) {
        switch (ended.end) {
            case GroupEnd::Nothing:
                break;
            case GroupEnd::Note:
            case GroupEnd::Annotation:
                endNoteGroup();
                break;
            case GroupEnd::Picture:
                endPicture();
                break;
            case GroupEnd::Style:
                if (ended.destination == Destination::Style) {
                    endStyle(ended);
                }
                m_style = StyleEntry();
                break;
            case GroupEnd::Field:
                endField(ended);
                break;
            case GroupEnd::FieldInstruction:
                m_fieldInstruction = std::exchange(m_text, QString()).trimmed();
                break;
            case GroupEnd::ListText:
                m_listText = std::exchange(m_text, QString()).simplified();
                m_hasListText = true;
                break;
            case GroupEnd::Attachment:
                endAttachment();
                break;
        }
    }

    /// The title, the author of the document or of a comment ends
    void endInformation(Destination ended) {
        if (ended == state().destination) {
            return;  // a group in it
        }
        if (ended == Destination::Title) {
            m_document.title = std::exchange(m_info, QString()).trimmed();
        } else if (ended == Destination::Author) {
            m_document.author = std::exchange(m_info, QString()).trimmed();
        } else if (ended == Destination::AnnotationAuthor) {
            m_annotationAuthor = std::exchange(m_info, QString()).trimmed();
        }
    }

    /// The group starts a destination @p word: what its text is
    void startDestination(const QByteArray& word, bool ignorable) {
        State& now = state();
        if (now.destination == Destination::Skip || startDocumentPart(word, now) ||
            startTextPart(word, now) || word == "shppict") {
            return;
        }
        if (isHeaderOrFooter(word)) {
            m_document.omissions[ImportOmission::HeaderFooter] = 1;
        } else if (word == "object") {
            ++m_document.omissions[ImportOmission::Object];
        } else if (word == "shp") {
            ++m_document.omissions[ImportOmission::Shape];
        } else if (!ignorable && !isSkippedDestination(word)) {
            return;  // \title or \author outside \info
        }
        now.destination = Destination::Skip;
    }

    /// The tables and the properties of the document
    bool startDocumentPart(const QByteArray& word, State& now) {
        const bool inInfo = now.destination == Destination::Info;
        if (word == "fonttbl") {
            now.destination = Destination::FontTable;
        } else if (word == "colortbl") {
            now.destination = Destination::ColorTable;
        } else if (word == "stylesheet") {
            now.destination = Destination::StyleSheet;
        } else if (word == "info") {
            now.destination = Destination::Info;
        } else if (inInfo && word == "title") {
            now.destination = Destination::Title;
            m_info.clear();
        } else if (inInfo && word == "author") {
            now.destination = Destination::Author;
            m_info.clear();
        } else if (word == "atnauthor") {
            now.destination = Destination::AnnotationAuthor;
            m_info.clear();
        } else {
            return false;
        }
        return true;
    }

    /// The parts of the text: notes, annotations, pictures, fields, list items
    bool startTextPart(const QByteArray& word, State& now) {
        if (word == "footnote") {
            startSink(Sink::Target::Note, false);
        } else if (word == "annotation") {
            startSink(Sink::Target::Annotation, false);
        } else if (word == "pict") {
            now.destination = Destination::Picture;
            now.end = GroupEnd::Picture;
            m_picture = PictureEntry();
        } else if (word == "fldinst") {
            now.destination = Destination::FieldInstruction;
            now.end = GroupEnd::FieldInstruction;
            m_text.clear();
        } else if (word == "fldrslt") {
            startFieldResult(std::exchange(m_fieldInstruction, QString()));
        } else if (word == "listtext" || word == "pntext") {
            now.destination = Destination::ListText;
            now.end = GroupEnd::ListText;
            m_text.clear();
        } else if (word == "NeXTGraphic") {
            now.destination = Destination::Attachment;
            now.end = GroupEnd::Attachment;
            m_text.clear();
            m_attachmentWidth = 0;
        } else if (word.startsWith("Scrv_")) {
            startScrivenerPart(word);
        } else {
            return false;
        }
        return true;
    }

    /// Scrivener's inline notes and annotations: groups with their text, or groups that
    /// start them and groups \Scrv_..._end that end them; its other groups are left out
    void startScrivenerPart(const QByteArray& word) {
        if (word.endsWith("_end")) {
            endMarker(word.startsWith("Scrv_fn") ? Sink::Target::Note : Sink::Target::Annotation);
            state().destination = Destination::Skip;
        } else if (word == "Scrv_fn") {
            startSink(Sink::Target::Note, true);
        } else if (word == "Scrv_annot" || word == "Scrv_comm" || word == "Scrv_inl") {
            startSink(Sink::Target::Annotation, true);
        } else {
            state().destination = Destination::Skip;
        }
    }

    // -------------------------------------------------------------------------
    // Control words and symbols
    // -------------------------------------------------------------------------

    void readControl() {
        if (m_pos >= m_data.size()) {
            return;
        }
        const char first = m_data.at(m_pos);
        if (!isLetter(first)) {
            ++m_pos;
            readSymbol(first);
            return;
        }
        const QByteArray word = readWord();
        const std::optional<int> parameter = readParameter();
        if (m_pos < m_data.size() && m_data.at(m_pos) == ' ') {
            ++m_pos;  // the delimiter
        }
        const bool ignorable = std::exchange(m_ignorable, false);
        if (word != "u" && word != "bin" && skipUnicodeFallback()) {
            return;
        }
        if (ignorable || isDestination(word)) {
            startDestination(word, ignorable);
            return;
        }
        handleWord(word, parameter);
    }

    QByteArray readWord() {
        QByteArray word;
        while (m_pos < m_data.size() && word.size() < MAX_CONTROL_LETTERS &&
               isLetter(m_data.at(m_pos))) {
            word += m_data.at(m_pos++);
        }
        // Scrivener's own words have underscores: \Scrv_annot
        if (word == "Scrv") {
            while (m_pos < m_data.size() && word.size() < MAX_CONTROL_LETTERS &&
                   (isLetter(m_data.at(m_pos)) || m_data.at(m_pos) == '_')) {
                word += m_data.at(m_pos++);
            }
        }
        return word;
    }

    std::optional<int> readParameter() {
        const qsizetype start = m_pos;
        const bool negative = m_pos < m_data.size() && m_data.at(m_pos) == '-';
        if (negative) {
            ++m_pos;
        }
        long long value = 0;
        int digits = 0;
        while (m_pos < m_data.size() && isDigit(m_data.at(m_pos))) {
            if (digits < MAX_PARAMETER_DIGITS) {
                value = value * DECIMAL + (m_data.at(m_pos) - '0');
            }
            ++digits;
            ++m_pos;
        }
        if (digits == 0) {
            m_pos = start;  // a '-' alone is text
            return std::nullopt;
        }
        value = std::min<long long>(value, std::numeric_limits<int>::max());
        return static_cast<int>(negative ? -value : value);
    }

    void readSymbol(char symbol) {
        m_ignorable = false;
        switch (symbol) {
            case '*':
                m_ignorable = true;  // the next word is a destination the reader may leave out
                return;
            case '\'':
                readHexByte();
                return;
            case '\r':
            case '\n':
                if (!skipUnicodeFallback()) {
                    endParagraph();  // \<newline>: Cocoa's paragraph
                }
                return;
            default:
                break;
        }
        if (skipUnicodeFallback()) {
            return;
        }
        switch (symbol) {
            case '\\':
            case '{':
            case '}':
                addCharacter(QChar(QLatin1Char(symbol)));
                break;
            case '\t':
                addTab();
                break;
            case '~':
                addCharacter(QChar(NO_BREAK_SPACE));
                break;
            case '_':
                addCharacter(QChar(NO_BREAK_HYPHEN));
                break;
            default:
                break;  // \- (an optional hyphen), \: and \| write nothing
        }
    }

    /// Whether the character stands for the \u before it (and so is left out)
    bool skipUnicodeFallback() {
        if (m_unicodeLeft > 0) {
            --m_unicodeLeft;
            return true;
        }
        return false;
    }

    void readHexByte() {
        if (m_pos + 2 > m_data.size()) {
            m_pos = m_data.size();
            return;
        }
        bool ok = false;
        const int value = m_data.mid(m_pos, 2).toInt(&ok, 16);
        m_pos += 2;
        if (!ok || skipUnicodeFallback() || state().destination == Destination::Picture) {
            return;
        }
        addByte(static_cast<unsigned char>(value));
    }

    void handleWord(const QByteArray& word, std::optional<int> parameter) {
        if (handleUnicodeWord(word, parameter)) {
            return;
        }
        switch (state().destination) {
            case Destination::Picture:
                readPictureWord(word, parameter);
                return;
            case Destination::FontTable:
                readFontWord(word, parameter);
                return;
            case Destination::ColorTable:
                readColorWord(word, parameter.value_or(0));
                return;
            case Destination::Skip:
            case Destination::Info:
                return;
            case Destination::Style:
                if (readStyleWord(word, parameter)) {
                    return;
                }
                break;
            case Destination::Attachment:
                if (word == "width" && parameter) {
                    m_attachmentWidth = *parameter;
                    return;
                }
                break;
            default:
                break;
        }
        const bool on = parameter.value_or(1) != 0;
        if (handleDocumentWord(word, parameter) || handleCharacterWord(word, parameter, on) ||
            handleParagraphWord(word, parameter, on)) {
            return;
        }
        if (state().destination == Destination::Body && handleTableWord(word, parameter)) {
            return;
        }
        handleTextWord(word);
    }

    /// \u, \uc and \bin, in every destination
    bool handleUnicodeWord(const QByteArray& word, std::optional<int> parameter) {
        if (word == "bin") {
            readBinary(parameter.value_or(0));
        } else if (word == "uc") {
            state().unicodeSkip = std::max(0, parameter.value_or(1));
        } else if (word == "u" && parameter) {
            const int code = *parameter < 0 ? *parameter + UNICODE_RANGE : *parameter;
            if (code > 0 && code < UNICODE_RANGE) {
                addCharacter(QChar(static_cast<char16_t>(code)));
            }
            m_unicodeLeft = state().unicodeSkip;
        } else {
            return false;
        }
        return true;
    }

    void readFontWord(const QByteArray& word, std::optional<int> parameter) {
        if (!parameter) {
            return;
        }
        if (word == "f") {
            m_font = *parameter;
            m_fonts[m_font] = Font();
            m_fontName.clear();
        } else if (word == "fcharset") {
            m_fonts[m_font].charset = *parameter;
        } else if (word == "cpg") {
            m_fonts[m_font].codePage = *parameter;
        }
    }

    void readColorWord(const QByteArray& word, int value) {
        if (word == "red") {
            m_red = value;
        } else if (word == "green") {
            m_green = value;
        } else if (word == "blue") {
            m_blue = value;
        }
    }

    bool readStyleWord(const QByteArray& word, std::optional<int> parameter) {
        if (word == "s" && parameter) {
            m_style.key = QStringLiteral("s%1").arg(*parameter);
        } else if (word == "cs" || word == "ds" || word == "ts") {
            m_style.paragraph = false;
        } else if (word == "sbasedon" && parameter) {
            m_style.basedOn = QStringLiteral("s%1").arg(*parameter);
        } else if (word == "snext" && parameter) {
            m_style.next = QStringLiteral("s%1").arg(*parameter);
        } else {
            return false;
        }
        return true;
    }

    /// The words of the whole document: its code page and its default font
    bool handleDocumentWord(const QByteArray& word, std::optional<int> parameter) {
        if (word == "ansicpg" && parameter) {
            m_codePage = *parameter;
        } else if (word == "mac") {
            m_codePage = MAC_ROMAN;
        } else if (word == "pc") {
            m_codePage = OEM_UNITED_STATES;
        } else if (word == "pca") {
            m_codePage = OEM_MULTILINGUAL;
        } else if (word == "deff" && parameter) {
            m_defaultFont = *parameter;
        } else {
            return false;
        }
        return true;
    }

    bool handleCharacterWord(const QByteArray& word, std::optional<int> parameter, bool on) {
        static const QSet<QByteArray> underlines{
            "ul",      "uld",     "uldash", "uldashd", "uldashdd", "uldb",     "ulhwave",
            "ulldash", "ulth",    "ulthd",  "ulthdash", "ulthdashd", "ulthdashdd", "ulthldash",
            "ululdbwave", "ulw",  "ulwave",
        };
        State& now = state();
        ImportCharFormat& format = now.format;
        if (word == "plain") {
            format = ImportCharFormat();
            now.font = m_defaultFont;
            now.hidden = false;
        } else if (word == "b") {
            format.bold = on;
        } else if (word == "i") {
            format.italic = on;
        } else if (underlines.contains(word)) {
            format.underline = on;
        } else if (word == "ulnone") {
            format.underline = false;
        } else if (word == "strike" || word == "striked") {
            format.strike = on;
        } else if (word == "v") {
            now.hidden = on;
        } else if (!parameter) {
            return handlePositionWord(word, std::nullopt, on);
        } else if (word == "f") {
            now.font = *parameter;
            format.font = m_fonts.value(*parameter).name;
        } else if (word == "fs") {
            format.size = *parameter / HALF_POINTS;
        } else if (word == "cf") {
            format.color = m_colors.value(*parameter);
        } else if (word == "cb" || word == "highlight" || word == "chcbpat") {
            format.background = m_colors.value(*parameter);
        } else if (word == "expndtw") {
            format.spacing = *parameter / TWIPS_PER_POINT;
        } else {
            return handlePositionWord(word, parameter, on);
        }
        return true;
    }

    /// Raised and lowered text, capitals
    bool handlePositionWord(const QByteArray& word, std::optional<int> parameter, bool on) {
        ImportCharFormat& format = state().format;
        if (word == "super") {
            format.superscript = on;
            format.subscript = false;
        } else if (word == "sub") {
            format.subscript = on;
            format.superscript = false;
        } else if (word == "nosupersub") {
            format.superscript = false;
            format.subscript = false;
        } else if (word == "up") {
            format.superscript = parameter.value_or(DEFAULT_RAISE) > 0;
        } else if (word == "dn") {
            format.subscript = parameter.value_or(DEFAULT_RAISE) > 0;
        } else if (word == "scaps") {
            format.caps = on ? QStringLiteral("small") : QString();
        } else if (word == "caps") {
            format.caps = on ? QStringLiteral("all") : QString();
        } else {
            return false;
        }
        return true;
    }

    bool handleParagraphWord(const QByteArray& word, std::optional<int> parameter, bool on) {
        static const QHash<QByteArray, QString> alignments{
            {"ql", QStringLiteral("left")},       {"qc", QStringLiteral("center")},
            {"qr", QStringLiteral("right")},      {"qj", QStringLiteral("justify")},
            {"qd", QStringLiteral("justify")},
        };
        ParagraphState& paragraph = state().paragraph;
        if (word == "pard") {
            paragraph = ParagraphState();
        } else if (const auto align = alignments.constFind(word); align != alignments.cend()) {
            paragraph.align = *align;
        } else if (word == "keepn") {
            paragraph.own.keepWithNext = on;
        } else if (word == "keep") {
            paragraph.own.keepTogether = on;
        } else if (word == "pagebb") {
            paragraph.own.pageBreakBefore = on;
        } else if (word == "widctlpar") {
            paragraph.own.widowControl = true;
        } else if (word == "nowidctlpar") {
            paragraph.own.widowControl = false;
        } else if (word == "slmult") {
            paragraph.lineMultiple = on;
        } else if (!parameter) {
            return false;
        } else {
            return handleParagraphValue(word, *parameter);
        }
        return true;
    }

    /// The paragraph's words with numbers: its style, outline level, indents and spacing
    bool handleParagraphValue(const QByteArray& word, int value) {
        ParagraphState& paragraph = state().paragraph;
        const double points = value / TWIPS_PER_POINT;
        if (word == "s") {
            paragraph.style = QStringLiteral("s%1").arg(value);
        } else if (word == "outlinelevel") {
            paragraph.outline = value >= 0 && value < OUTLINE_LEVELS ? value + 1 : 0;
        } else if (word == "fi") {
            paragraph.own.firstLineIndent = points;
        } else if (word == "li") {
            paragraph.own.leftIndent = points;
        } else if (word == "ri") {
            paragraph.own.rightIndent = points;
        } else if (word == "sb") {
            paragraph.own.spaceBefore = points;
        } else if (word == "sa") {
            paragraph.own.spaceAfter = points;
        } else if (word == "sl") {
            paragraph.line = value;
        } else if (word == "ls") {
            paragraph.list = value;
        } else if (word == "ilvl") {
            paragraph.listLevel = value;
        } else {
            return false;
        }
        return true;
    }

    /// The rows and cells of a table
    bool handleTableWord(const QByteArray& word, std::optional<int> parameter) {
        CellDefinition& cell = m_row.next;
        if (word == "trowd") {
            m_row = RowDefinition();
        } else if (word == "trhdr") {
            m_row.header = true;
        } else if (word == "trqc") {
            m_row.align = QStringLiteral("center");
        } else if (word == "trqr") {
            m_row.align = QStringLiteral("right");
        } else if (word == "clmrg") {
            cell.merged = true;
        } else if (word == "clvmgf") {
            cell.verticalFirst = true;
        } else if (word == "clvmrg") {
            cell.verticalMerged = true;
        } else if (word == "clvertalc") {
            cell.valign = QStringLiteral("middle");
        } else if (word == "clvertalb") {
            cell.valign = QStringLiteral("bottom");
        } else if (word == "intbl") {
            state().paragraph.inTable = true;
        } else if (word == "cell") {
            endCell();
        } else if (word == "nestcell") {
            addTab();
        } else if (word == "row") {
            endRow();
        } else if (word == "nestrow") {
            endParagraph();
        } else if (parameter) {
            return handleTableValue(word, *parameter);
        } else {
            return false;
        }
        return true;
    }

    bool handleTableValue(const QByteArray& word, int value) {
        if (word == "trleft") {
            m_row.left = value;
        } else if (word == "trrh") {
            m_row.height = std::abs(value) / TWIPS_PER_POINT;
        } else if (word == "clcbpat") {
            m_row.next.background = m_colors.value(value);
        } else if (word == "cellx") {
            m_row.next.right = value;
            m_row.cells.push_back(std::exchange(m_row.next, CellDefinition()));
        } else if (word == "itap") {
            if (value > 0) {
                state().paragraph.inTable = true;
            }
        } else {
            return false;
        }
        return true;
    }

    /// The words that write something: breaks, tabs, the characters with names
    void handleTextWord(const QByteArray& word) {
        static const QHash<QByteArray, char16_t> characters{
            {"emdash", 0x2014},    {"endash", 0x2013},  {"bullet", 0x2022},
            {"lquote", 0x2018},    {"rquote", 0x2019},  {"ldblquote", 0x201C},
            {"rdblquote", 0x201D}, {"emspace", 0x2003}, {"enspace", 0x2002},
            {"qmspace", 0x2005},
        };
        if (word == "par") {
            endParagraph();
        } else if (word == "sect") {
            endSection();
        } else if (word == "sbknone" || word == "sbkcol") {
            continueSection();
        } else if (word == "line" || word == "softline") {
            addRun(ImportRun::Kind::LineBreak);
        } else if (word == "tab") {
            addTab();
        } else if (word == "page") {
            addPageBreak();
        } else if (word == "ftnalt") {
            sink().endnote = sink().target == Sink::Target::Note;
        } else if (const auto it = characters.constFind(word); it != characters.cend()) {
            addCharacter(QChar(*it));
        }
        // The rest (\chftn, \chatn, \cocoartf, \rtlch...) writes nothing
    }

    // -------------------------------------------------------------------------
    // Text
    // -------------------------------------------------------------------------

    /// A character of the text between the control words
    void addText(char ch) {
        if (state().destination == Destination::Picture) {
            if (isHexDigit(ch)) {
                m_picture.hex += ch;
            }
            return;
        }
        addByte(static_cast<unsigned char>(ch));
    }

    /// A byte of the text, in the code page of its font or of the document
    void addByte(unsigned char byte) {
        if (skipUnicodeFallback()) {
            return;
        }
        if (byte < 0x80) {
            addCharacter(QChar(QLatin1Char(static_cast<char>(byte))));
            return;
        }
        const int fontId = state().destination == Destination::FontTable ? m_font : state().font;
        const auto font = m_fonts.constFind(fontId);
        int codePage = m_codePage;
        if (font != m_fonts.cend() && font->codePage > 0) {
            codePage = font->codePage;
        } else if (font != m_fonts.cend() && font->charset == SYMBOL_CHARSET) {
            addCharacter(byte == SYMBOL_BULLET ? QChar(BULLET_DISC) : QChar(char16_t(byte)));
            return;
        } else if (font != m_fonts.cend() && codePageOfCharset(font->charset) > 0) {
            codePage = codePageOfCharset(font->charset);
        }
        const QString text = textOf(byte, codePage);
        for (const QChar ch : text) {
            addCharacter(ch);
        }
    }

    /// The text of @p byte in @p codePage: the program's own tables, else the system's, else
    /// Latin-1; empty for the first byte of a character of two
    QString textOf(unsigned char byte, int codePage) {
        if (const std::optional<QChar> known = windowsCharacter(byte, codePage)) {
            return QString(*known);
        }
        auto it = m_decoders.find(codePage);
        if (it == m_decoders.end()) {
            const QByteArray name = codePage == MAC_ROMAN
                                        ? QByteArray("macintosh")
                                        : "windows-" + QByteArray::number(codePage);
            QStringDecoder decoder(name.constData());
            if (!decoder.isValid() && codePage < 1000) {
                decoder = QStringDecoder(("IBM" + QByteArray::number(codePage)).constData());
            }
            it = m_decoders.emplace(codePage, std::move(decoder)).first;
        }
        if (it->second.isValid()) {
            const char ch = static_cast<char>(byte);
            return it->second.decode(QByteArrayView(&ch, 1));
        }
        return QString(QChar(QLatin1Char(static_cast<char>(byte))));
    }

    void addCharacter(QChar ch) {
        if (!addToDestination(ch) || state().hidden) {
            return;
        }
        if (std::exchange(m_afterAttachment, false) && ch == QChar(NOT_SIGN)) {
            return;  // TextEdit's mark of the attachment before it
        }
        const State& now = state();
        ImportBlock& paragraph = sink().paragraph;
        const std::vector<int> annotations = annotationsNow();
        if (!paragraph.runs.empty()) {
            ImportRun& last = paragraph.runs.back();
            if (last.kind == ImportRun::Kind::Text && last.format == now.format &&
                last.link == now.link && last.annotations == annotations) {
                last.text += ch;
                return;
            }
        }
        ImportRun run;
        run.text = QString(ch);
        run.format = now.format;
        run.link = now.link;
        run.annotations = annotations;
        paragraph.runs.push_back(std::move(run));
    }

    /// A character of a destination other than the text; true: of the text
    bool addToDestination(QChar ch) {
        switch (state().destination) {
            case Destination::Body:
                return true;
            case Destination::FontTable:
                if (ch == QLatin1Char(';')) {
                    m_fonts[m_font].name =
                        familyOfFace(std::exchange(m_fontName, QString()).trimmed());
                } else {
                    m_fontName += ch;
                }
                break;
            case Destination::ColorTable:
                if (ch == QLatin1Char(';')) {
                    m_colors.insert(m_colorIndex++,
                                    m_red < 0 ? QString() : colorOf(m_red, m_green, m_blue));
                    m_red = m_green = m_blue = -1;
                }
                break;
            case Destination::Style:
                m_style.name += ch;
                break;
            case Destination::Title:
            case Destination::Author:
            case Destination::AnnotationAuthor:
                m_info += ch;
                break;
            case Destination::FieldInstruction:
            case Destination::ListText:
            case Destination::Attachment:
                m_text += ch;
                break;
            case Destination::Skip:
            case Destination::Info:
            case Destination::StyleSheet:
            case Destination::Picture:
                break;
        }
        return false;
    }

    std::vector<int> annotationsNow() const {
        const int annotation = m_states.back().annotation;
        return annotation < 0 ? std::vector<int>() : std::vector<int>{annotation};
    }

    void addTab() {
        if (state().destination == Destination::Body) {
            addRun(ImportRun::Kind::Tab);
        } else if (state().destination == Destination::ListText ||
                   state().destination == Destination::FieldInstruction) {
            m_text += QLatin1Char(' ');
        }
    }

    void addRun(ImportRun::Kind kind, int index = -1) {
        if (state().destination != Destination::Body || state().hidden) {
            return;
        }
        ImportRun run;
        run.kind = kind;
        run.index = index;
        run.format = state().format;
        run.link = state().link;
        if (kind == ImportRun::Kind::Note) {
            run.format.superscript = false;  // a note's number is raised by itself
            run.format.subscript = false;
        }
        sink().paragraph.runs.push_back(std::move(run));
    }

    // -------------------------------------------------------------------------
    // Paragraphs, breaks and sections
    // -------------------------------------------------------------------------

    /// The paragraph ends: it goes into the document, a cell, a note or an annotation
    void endParagraph() {
        const State& now = state();
        if (now.destination != Destination::Body || now.hidden) {
            return;
        }
        ImportBlock block = finishParagraph(now.paragraph);
        if (m_sinks.size() == 1 && now.paragraph.inTable) {
            m_cell.blocks.push_back(std::move(block));
            return;
        }
        if (block.own.pageBreakBefore.value_or(false) && m_sinks.size() == 1) {
            block.own.pageBreakBefore.reset();
            addPageBreak();
        }
        pushBlock(std::move(block));
    }

    /// The paragraph being written, with its formatting
    ImportBlock finishParagraph(const ParagraphState& paragraph) {
        Sink& into = sink();
        ImportBlock block = std::exchange(into.paragraph, ImportBlock());
        block.style = styleOf(paragraph.style);
        block.headingLevel = paragraph.outline;
        block.align = paragraph.align;
        block.own = paragraph.own;
        if (paragraph.line && *paragraph.line != 0) {
            block.own.lineSpacing = lineSpacingOf(*paragraph.line, paragraph.lineMultiple);
        }
        if (paragraph.list > 0 || m_hasListText) {
            const int level = std::clamp(paragraph.listLevel, 0, LIST_LEVELS - 1);
            block.list = listOf(paragraph.list, level);
            block.listLevel = level;
            block.own.leftIndent.reset();  // the list's levels have them
            block.own.firstLineIndent.reset();
        }
        m_listText.clear();
        m_hasListText = false;
        if (into.target == Sink::Target::Body && !block.style.isEmpty()) {
            ++m_used[block.style];
        }
        return block;
    }

    /// The key of a paragraph's style: its own, else the default one (\s0)
    QString styleOf(const QString& key) const {
        if (m_styleNames.contains(key)) {
            return key;
        }
        const QString normal = QStringLiteral("s0");
        return m_styleNames.contains(normal) ? normal : QString();
    }

    /// The index in the document of the list of @p key (\ls; 0: of a \pntext), whose level
    /// @p level the mark of the list item says, when it is its first item of the level
    int listOf(int key, int level) {
        auto it = m_lists.constFind(key);
        if (it == m_lists.cend()) {
            const ListLevel first = listLevelOf(m_listText, level);
            it = m_lists.insert(key, static_cast<int>(m_document.lists.size()));
            m_document.lists.push_back(
                ListDefinition::ofKind(first.isBullet() ? ListKind::Bulleted : ListKind::Numbered));
        }
        const int index = *it;
        const qint64 seen = static_cast<qint64>(index) * LIST_LEVELS + level;
        if (!m_listLevels.contains(seen)) {
            m_listLevels.insert(seen);
            m_document.lists[static_cast<size_t>(index)].levels[static_cast<size_t>(level)] =
                listLevelOf(m_listText, level);
        }
        return index;
    }

    void pushBlock(ImportBlock block) {
        Sink& into = sink();
        if (into.target != Sink::Target::Body) {
            into.blocks.push_back(std::move(block));
            return;
        }
        flushTable();
        m_sectionBreak = false;
        m_document.blocks.push_back(std::move(block));
    }

    void addPageBreak() {
        const State& now = state();
        if (now.destination != Destination::Body || now.hidden || m_sinks.size() > 1 ||
            now.paragraph.inTable) {
            return;
        }
        if (!sink().paragraph.runs.empty()) {
            endParagraph();
        }
        flushTable();
        m_sectionBreak = false;
        if (!m_document.blocks.empty() &&
            m_document.blocks.back().kind == ImportBlock::Kind::PageBreak) {
            return;
        }
        ImportBlock page;
        page.kind = ImportBlock::Kind::PageBreak;
        m_document.blocks.push_back(std::move(page));
    }

    /// \sect: a page break, unless the next section continues on the page (\sbknone)
    void endSection() {
        if (state().destination != Destination::Body || m_sinks.size() > 1) {
            return;
        }
        if (!sink().paragraph.runs.empty()) {
            endParagraph();
        }
        addPageBreak();
        m_sectionBreak = !m_document.blocks.empty() &&
                         m_document.blocks.back().kind == ImportBlock::Kind::PageBreak;
    }

    void continueSection() {
        if (m_sectionBreak && !m_document.blocks.empty() &&
            m_document.blocks.back().kind == ImportBlock::Kind::PageBreak) {
            m_document.blocks.pop_back();
        }
        m_sectionBreak = false;
    }

    // -------------------------------------------------------------------------
    // Tables
    // -------------------------------------------------------------------------

    void endCell() {
        if (m_sinks.size() > 1) {
            addTab();
            return;
        }
        if (!sink().paragraph.runs.empty() || m_cell.blocks.empty()) {
            m_cell.blocks.push_back(finishParagraph(state().paragraph));
        }
        m_rowCells.push_back(std::exchange(m_cell, ImportCell()));
    }

    void endRow() {
        if (m_sinks.size() > 1) {
            endParagraph();
            return;
        }
        if (!sink().paragraph.runs.empty()) {
            endCell();
        }
        if (!m_rowCells.empty()) {
            finishRow();
        }
    }

    /// The cells of the row go into the table, with the row's definition
    void finishRow() {
        if (!m_table) {
            m_table = std::make_shared<ImportTable>();
            m_table->align = m_row.align;
        }
        ImportRow row;
        row.header = m_row.header;
        row.height = m_row.height;
        const size_t rowIndex = m_table->rows.size();
        for (size_t column = 0; column < m_rowCells.size(); ++column) {
            const CellDefinition definition =
                column < m_row.cells.size() ? m_row.cells[column] : CellDefinition();
            if (definition.merged && !row.cells.empty()) {
                ++row.cells.back().columnSpan;
                continue;
            }
            const auto above = m_verticalStarts.find(column);
            if (definition.verticalMerged && above != m_verticalStarts.end()) {
                ++m_table->rows[above->second.first].cells[above->second.second].rowSpan;
                continue;
            }
            if (definition.verticalFirst) {
                m_verticalStarts[column] = {rowIndex, row.cells.size()};
            } else {
                m_verticalStarts.erase(column);
            }
            ImportCell cell = std::move(m_rowCells[column]);
            cell.background = definition.background;
            cell.valign = definition.valign;
            row.cells.push_back(std::move(cell));
        }
        m_rowCells.clear();
        if (m_row.cells.size() > m_table->columns.size()) {
            m_table->columns = columnsOf(m_row);
        }
        m_table->rows.push_back(std::move(row));
    }

    /// The widths of the columns of a row's definition, in twips; empty: equal
    static std::vector<double> columnsOf(const RowDefinition& row) {
        std::vector<double> columns;
        int left = row.left;
        for (const CellDefinition& cell : row.cells) {
            if (cell.right <= left) {
                return {};
            }
            columns.push_back(cell.right - left);
            left = cell.right;
        }
        return columns;
    }

    /// The table being read goes into the document (a paragraph after it is not in it)
    void flushTable() {
        if (!m_cell.blocks.empty()) {
            m_rowCells.push_back(std::exchange(m_cell, ImportCell()));
        }
        if (!m_rowCells.empty()) {
            finishRow();
        }
        if (!m_table) {
            return;
        }
        ImportBlock block;
        block.kind = ImportBlock::Kind::Table;
        block.table = std::move(m_table);
        m_table.reset();
        m_verticalStarts.clear();
        m_document.blocks.push_back(std::move(block));
    }

    // -------------------------------------------------------------------------
    // Notes and annotations
    // -------------------------------------------------------------------------

    void startSink(Sink::Target target, bool scrivener) {
        State& now = state();
        now.destination = Destination::Body;
        now.end = target == Sink::Target::Note ? GroupEnd::Note : GroupEnd::Annotation;
        now.hidden = false;
        now.link.clear();
        now.annotation = -1;
        Sink part;
        part.target = target;
        part.scrivener = scrivener;
        m_sinks.push_back(std::move(part));
    }

    /// The group of a note or an annotation ends; Scrivener's group without text only
    /// starts it
    void endNoteGroup() {
        if (m_sinks.size() <= 1) {
            return;
        }
        Sink& part = sink();
        if (part.scrivener && !part.marker && part.blocks.empty() && part.paragraph.runs.empty()) {
            part.marker = true;
            return;
        }
        endSink();
    }

    /// Scrivener's group \Scrv_..._end: the note or the annotation its group started ends
    void endMarker(Sink::Target target) {
        if (m_sinks.size() <= 1 || !sink().marker || sink().target != target) {
            return;
        }
        if (!sink().paragraph.runs.empty()) {
            endParagraph();
        }
        endSink();
    }

    /// The note or the annotation goes into the text
    void endSink() {
        Sink part = std::move(m_sinks.back());
        m_sinks.pop_back();
        if (!part.paragraph.runs.empty()) {
            part.blocks.push_back(std::move(part.paragraph));
        }
        if (part.target == Sink::Target::Note) {
            ImportNote note;
            note.endnote = part.endnote;
            note.paragraphs = std::move(part.blocks);
            addNote(std::move(note));
            return;
        }
        QStringList paragraphs;
        for (const ImportBlock& block : part.blocks) {
            if (const QString text = importPlainText(block).trimmed(); !text.isEmpty()) {
                paragraphs << text;
            }
        }
        if (paragraphs.isEmpty()) {
            return;
        }
        ImportAnnotation made;
        made.author = std::exchange(m_annotationAuthor, QString());
        made.text = paragraphs.join(QStringLiteral("\n\n"));
        const auto index = static_cast<int>(m_document.annotations.size());
        m_document.annotations.push_back(std::move(made));
        addRun(ImportRun::Kind::Annotation, index);
    }

    void addNote(ImportNote note) {
        dropNoteMark(note);
        const auto index = static_cast<int>(m_document.notes.size());
        m_document.notes.push_back(std::move(note));
        addRun(ImportRun::Kind::Note, index);
    }

    /// The space or the tab after Word's number of a note (\chftn writes no number)
    static void dropNoteMark(ImportNote& note) {
        if (note.paragraphs.empty()) {
            return;
        }
        std::vector<ImportRun>& runs = note.paragraphs.front().runs;
        while (!runs.empty()) {
            ImportRun& first = runs.front();
            if (first.kind == ImportRun::Kind::Tab) {
                runs.erase(runs.begin());
                continue;
            }
            if (first.kind != ImportRun::Kind::Text) {
                return;
            }
            const qsizetype spaces = std::distance(
                first.text.cbegin(), std::find_if(first.text.cbegin(), first.text.cend(),
                                                  [](QChar ch) { return !ch.isSpace(); }));
            first.text.remove(0, spaces);
            if (!first.text.isEmpty()) {
                return;
            }
            runs.erase(runs.begin());
        }
    }

    /// A note or an annotation of Scrivener that its end marker did not end: its text goes
    /// back into the text around it, so that nothing of the text is lost
    void restoreMarker() {
        Sink part = std::move(m_sinks.back());
        m_sinks.pop_back();
        if (!part.paragraph.runs.empty()) {
            part.blocks.push_back(std::move(part.paragraph));
        }
        for (ImportBlock& block : part.blocks) {
            std::vector<ImportRun>& before = sink().paragraph.runs;
            block.runs.insert(block.runs.begin(), std::make_move_iterator(before.begin()),
                              std::make_move_iterator(before.end()));
            before.clear();
            pushBlock(std::move(block));
        }
    }

    // -------------------------------------------------------------------------
    // Fields: links, and the comments and footnotes Scrivener links to
    // -------------------------------------------------------------------------

    void startFieldResult(const QString& instruction) {
        State& now = state();
        if (isTableOfContents(instruction)) {
            ++m_document.omissions[ImportOmission::TableOfContents];
            now.destination = Destination::Skip;
            return;
        }
        now.end = GroupEnd::Field;
        const QString link = linkOfInstruction(instruction);
        if (!link.startsWith(LINKED_NOTE, Qt::CaseInsensitive)) {
            if (!link.isEmpty()) {
                now.link = link;
            }
            return;
        }
        const QString id = link.mid(LINKED_NOTE.size());
        const auto it = m_options.linked.constFind(id);
        if (it == m_options.linked.cend()) {
            return;
        }
        if (it->footnote) {
            now.linkedFootnote = id;  // its number after the text it is about
        } else {
            now.annotation = addLinkedComment(*it);
        }
    }

    int addLinkedComment(const LinkedNote& note) {
        QStringList paragraphs;
        for (const ImportBlock& block : note.paragraphs) {
            if (const QString text = importPlainText(block).trimmed(); !text.isEmpty()) {
                paragraphs << text;
            }
        }
        ImportAnnotation made;
        made.text = paragraphs.join(QStringLiteral("\n\n"));
        const auto index = static_cast<int>(m_document.annotations.size());
        m_document.annotations.push_back(std::move(made));
        return index;
    }

    void endField(const State& ended) {
        if (ended.linkedFootnote.isEmpty()) {
            return;
        }
        ImportNote note;
        note.paragraphs = m_options.linked.value(ended.linkedFootnote).paragraphs;
        addNote(std::move(note));
    }

    // -------------------------------------------------------------------------
    // Pictures
    // -------------------------------------------------------------------------

    void readPictureWord(const QByteArray& word, std::optional<int> parameter) {
        static const QSet<QByteArray> unread{
            "emfblip", "wmetafile", "macpict", "dibitmap", "wbitmap", "pmmetafile",
        };
        if (word == "pngblip") {
            m_picture.suffix = QStringLiteral("png");
        } else if (word == "jpegblip") {
            m_picture.suffix = QStringLiteral("jpg");
        } else if (unread.contains(word)) {
            m_picture.suffix.clear();
        } else if (word == "picwgoal" && parameter) {
            m_picture.goalWidth = *parameter;
        } else if (word == "picscalex" && parameter) {
            m_picture.scale = *parameter;
        }
    }

    void readBinary(int size) {
        const qsizetype count = std::clamp<qsizetype>(size, 0, m_data.size() - m_pos);
        if (state().destination == Destination::Picture) {
            m_picture.binary += m_data.mid(m_pos, count);
        }
        m_pos += count;
    }

    void endPicture() {
        const PictureEntry picture = std::exchange(m_picture, PictureEntry());
        if (picture.suffix.isEmpty()) {
            ++m_document.omissions[ImportOmission::Image];
            return;
        }
        ImportImage image;
        image.data = picture.binary.isEmpty() ? QByteArray::fromHex(picture.hex) : picture.binary;
        if (image.data.isEmpty()) {
            return;
        }
        const auto index = static_cast<int>(m_document.images.size());
        image.fileName = QStringLiteral("image%1.%2").arg(index + 1).arg(picture.suffix);
        if (picture.goalWidth > 0) {
            image.look.unit = ImageWidthUnit::Centimeters;
            image.look.width = centimetresOf(picture.goalWidth * picture.scale / PERCENT);
        }
        m_document.images.push_back(std::move(image));
        addRun(ImportRun::Kind::Image, index);
    }

    /// TextEdit's attachment: a file beside the text (in an RTFD folder)
    void endAttachment() {
        const QString name = QFileInfo(std::exchange(m_text, QString()).trimmed()).fileName();
        m_afterAttachment = true;
        if (name.isEmpty()) {
            return;
        }
        if (!isReadableImage(QFileInfo(name).suffix())) {
            ++m_document.omissions[ImportOmission::Image];
            return;
        }
        ImportImage image;
        image.address = name;
        image.fileName = name;
        if (!m_options.folder.isEmpty()) {
            QFile file(QDir(m_options.folder).filePath(name));
            if (file.open(QIODevice::ReadOnly)) {
                image.data = file.readAll();
            }
        }
        image.missing = image.data.isEmpty();
        if (m_attachmentWidth > 0) {
            image.look.unit = ImageWidthUnit::Centimeters;
            image.look.width = centimetresOf(m_attachmentWidth);
        }
        const auto index = static_cast<int>(m_document.images.size());
        m_document.images.push_back(std::move(image));
        addRun(ImportRun::Kind::Image, index);
    }

    // -------------------------------------------------------------------------
    // The stylesheet and the formatting
    // -------------------------------------------------------------------------

    void endStyle(const State& ended) {
        if (!m_style.paragraph) {
            return;
        }
        ImportStyle style;
        style.key = m_style.key.isEmpty() ? QStringLiteral("s0") : m_style.key;  // \s0: default
        style.name = m_style.name.trimmed();
        if (style.name.endsWith(QLatin1Char(';'))) {
            style.name.chop(1);
        }
        style.name = style.name.trimmed();
        style.basedOn = m_style.basedOn == style.key ? QString() : m_style.basedOn;
        style.next = m_style.next == style.key ? QString() : m_style.next;
        style.headingLevel = ended.paragraph.outline > 0 ? ended.paragraph.outline
                                                         : headingLevelOfStyleName(style.name);
        style.properties = propertiesOf(ended);
        m_styleNames.insert(style.key);
        m_styles.push_back(std::move(style));
    }

    /// The own formatting of a style, as the book's styles name it
    static QVariantMap propertiesOf(const State& style) {
        QVariantMap properties;
        const ParagraphState& paragraph = style.paragraph;
        const ParagraphFormat& own = paragraph.own;
        const auto put = [&properties](const char* name, const auto& value) {
            if (value) {
                properties.insert(QLatin1String(name), *value);
            }
        };
        put("leftMargin", own.leftIndent);
        put("rightMargin", own.rightIndent);
        put("firstLineIndent", own.firstLineIndent);
        put("spaceBefore", own.spaceBefore);
        put("spaceAfter", own.spaceAfter);
        put("keepWithNext", own.keepWithNext);
        put("keepTogether", own.keepTogether);
        put("pageBreakBefore", own.pageBreakBefore);
        if (own.widowControl == std::optional<bool>(false)) {
            properties.insert(QStringLiteral("widowControl"), false);
        }
        if (paragraph.line && paragraph.lineMultiple) {
            properties.insert(QStringLiteral("lineHeight"), std::abs(*paragraph.line) / SINGLE_LINE);
        }
        if (paragraph.align != QLatin1String("left")) {
            properties.insert(QStringLiteral("alignment"), paragraph.align);
        }
        const ImportCharFormat& format = style.format;
        if (!format.font.isEmpty()) {
            properties.insert(QStringLiteral("fontFamily"), format.font);
        }
        if (format.size > 0.0) {
            properties.insert(QStringLiteral("fontSize"), format.size);
        }
        const auto putSwitch = [&properties](const char* name, bool value) {
            if (value) {
                properties.insert(QLatin1String(name), true);
            }
        };
        putSwitch("bold", format.bold);
        putSwitch("italic", format.italic);
        putSwitch("underline", format.underline);
        return properties;
    }

    void finish() {
        while (m_tooDeep > 0 || m_states.size() > 1) {
            closeGroup();  // the groups that the file did not close
        }
        if (!sink().paragraph.runs.empty()) {
            endParagraph();
        }
        while (m_sinks.size() > 1) {
            restoreMarker();
        }
        flushTable();
        while (!m_document.blocks.empty() &&
               m_document.blocks.back().kind == ImportBlock::Kind::PageBreak) {
            m_document.blocks.pop_back();
        }
        for (ImportStyle& style : m_styles) {
            style.paragraphs = m_used.value(style.key);
            m_document.styles.push_back(std::move(style));
        }
        m_styles.clear();
        cleanUpFormatting();
    }

    /// The text keeps only the formatting of its own: without its styles' and without the
    /// font and the size of most of it
    void cleanUpFormatting() {
        std::vector<ImportBlock*> body;
        collectParagraphs(m_document.blocks, body);
        std::vector<ImportBlock*> all = body;
        for (ImportNote& note : m_document.notes) {
            collectParagraphs(note.paragraphs, all);
        }
        static const QVariantMap none;
        for (ImportBlock* block : all) {
            const ImportStyle* style = m_document.style(block->style);
            withoutStyleFormatting(*block, style ? style->properties : none);
        }
        std::map<QString, qsizetype> fonts;
        std::map<double, qsizetype> sizes;
        for (const ImportBlock* block : body) {
            for (const ImportRun& run : block->runs) {
                fonts[run.format.font] += run.text.size();
                sizes[run.format.size] += run.text.size();
            }
        }
        const QString font = mostCommon(fonts);
        const double size = mostCommon(sizes);
        for (ImportBlock* block : all) {
            for (ImportRun& run : block->runs) {
                if (!font.isEmpty() && run.format.font == font) {
                    run.format.font.clear();
                }
                if (size > 0.0 && std::abs(run.format.size - size) < SAME) {
                    run.format.size = 0.0;
                }
            }
        }
    }

    const QByteArray& m_data;
    const RtfReadOptions& m_options;
    qsizetype m_pos = 0;
    bool m_finished = false;    ///< The group of the document ended
    ImportDocument m_document;
    std::vector<State> m_states;
    std::vector<Sink> m_sinks;
    int m_tooDeep = 0;          ///< The groups deeper than MAX_DEPTH that are open
    bool m_ignorable = false;   ///< After \*: the destination may be left out
    int m_unicodeLeft = 0;      ///< The characters after \u that stand for it

    int m_codePage = DEFAULT_CODE_PAGE;
    int m_defaultFont = -1;
    QHash<int, Font> m_fonts;
    int m_font = -1;            ///< The font of the font table being read
    QString m_fontName;
    QHash<int, QString> m_colors;
    int m_colorIndex = 0;
    int m_red = -1;
    int m_green = -1;
    int m_blue = -1;
    std::map<int, QStringDecoder> m_decoders;

    std::vector<ImportStyle> m_styles;
    QSet<QString> m_styleNames;
    QHash<QString, int> m_used;  ///< The paragraphs of each style
    StyleEntry m_style;

    QString m_info;              ///< Of the title, the author, a comment's author
    QString m_annotationAuthor;
    QString m_text;              ///< Of a field's instruction, a list item's mark, an attachment
    QString m_fieldInstruction;
    QString m_listText;          ///< The mark of the list item being written
    bool m_hasListText = false;
    QHash<int, int> m_lists;     ///< \ls: the index of its list in the document
    QSet<qint64> m_listLevels;   ///< The levels of the lists whose format is known
    PictureEntry m_picture;
    int m_attachmentWidth = 0;
    bool m_afterAttachment = false;
    bool m_sectionBreak = false;  ///< The last block is the page break of a \sect

    RowDefinition m_row;
    ImportCell m_cell;                  ///< The cell being written
    std::vector<ImportCell> m_rowCells; ///< The cells of the row being written
    std::shared_ptr<ImportTable> m_table;
    std::map<size_t, std::pair<size_t, size_t>> m_verticalStarts;  ///< Column: the row and
                                                                    ///< the cell merged down
};

}  // namespace

ImportDocument readRtf(const QByteArray& rtf, const RtfReadOptions& options) {
    RtfReader reader(rtf, options);
    return reader.read();
}

}  // namespace kalahari::editor
