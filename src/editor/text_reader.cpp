// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file text_reader.cpp
/// @brief Import of plain text: its encodings, paragraphs and the titles of its texts and parts

#include <kalahari/editor/book_import.h>

#include <QRegularExpression>
#include <QStringDecoder>

#include <algorithm>
#include <array>
#include <map>
#include <optional>

namespace kalahari::editor {

namespace {

// =============================================================================
// 8-bit encodings
// =============================================================================

/// The characters of the bytes 0x80 to 0xFF of Windows-1250 (U+FFFD: none), as Python's codec
/// cp1250 gives them
constexpr std::array<char16_t, 128> WINDOWS_1250{
    0x20AC, 0xFFFD, 0x201A, 0xFFFD, 0x201E, 0x2026, 0x2020, 0x2021,
    0xFFFD, 0x2030, 0x0160, 0x2039, 0x015A, 0x0164, 0x017D, 0x0179,
    0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0xFFFD, 0x2122, 0x0161, 0x203A, 0x015B, 0x0165, 0x017E, 0x017A,
    0x00A0, 0x02C7, 0x02D8, 0x0141, 0x00A4, 0x0104, 0x00A6, 0x00A7,
    0x00A8, 0x00A9, 0x015E, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x017B,
    0x00B0, 0x00B1, 0x02DB, 0x0142, 0x00B4, 0x00B5, 0x00B6, 0x00B7,
    0x00B8, 0x0105, 0x015F, 0x00BB, 0x013D, 0x02DD, 0x013E, 0x017C,
    0x0154, 0x00C1, 0x00C2, 0x0102, 0x00C4, 0x0139, 0x0106, 0x00C7,
    0x010C, 0x00C9, 0x0118, 0x00CB, 0x011A, 0x00CD, 0x00CE, 0x010E,
    0x0110, 0x0143, 0x0147, 0x00D3, 0x00D4, 0x0150, 0x00D6, 0x00D7,
    0x0158, 0x016E, 0x00DA, 0x0170, 0x00DC, 0x00DD, 0x0162, 0x00DF,
    0x0155, 0x00E1, 0x00E2, 0x0103, 0x00E4, 0x013A, 0x0107, 0x00E7,
    0x010D, 0x00E9, 0x0119, 0x00EB, 0x011B, 0x00ED, 0x00EE, 0x010F,
    0x0111, 0x0144, 0x0148, 0x00F3, 0x00F4, 0x0151, 0x00F6, 0x00F7,
    0x0159, 0x016F, 0x00FA, 0x0171, 0x00FC, 0x00FD, 0x0163, 0x02D9,
};

/// The characters of the bytes 0xA0 to 0xFF of ISO 8859-2 (0x80 to 0x9F are the controls of
/// those codes), as Python's codec iso8859_2 gives them
constexpr std::array<char16_t, 96> ISO_8859_2{
    0x00A0, 0x0104, 0x02D8, 0x0141, 0x00A4, 0x013D, 0x015A, 0x00A7,
    0x00A8, 0x0160, 0x015E, 0x0164, 0x0179, 0x00AD, 0x017D, 0x017B,
    0x00B0, 0x0105, 0x02DB, 0x0142, 0x00B4, 0x013E, 0x015B, 0x02C7,
    0x00B8, 0x0161, 0x015F, 0x0165, 0x017A, 0x02DD, 0x017E, 0x017C,
    0x0154, 0x00C1, 0x00C2, 0x0102, 0x00C4, 0x0139, 0x0106, 0x00C7,
    0x010C, 0x00C9, 0x0118, 0x00CB, 0x011A, 0x00CD, 0x00CE, 0x010E,
    0x0110, 0x0143, 0x0147, 0x00D3, 0x00D4, 0x0150, 0x00D6, 0x00D7,
    0x0158, 0x016E, 0x00DA, 0x0170, 0x00DC, 0x00DD, 0x0162, 0x00DF,
    0x0155, 0x00E1, 0x00E2, 0x0103, 0x00E4, 0x013A, 0x0107, 0x00E7,
    0x010D, 0x00E9, 0x0119, 0x00EB, 0x011B, 0x00ED, 0x00EE, 0x010F,
    0x0111, 0x0144, 0x0148, 0x00F3, 0x00F4, 0x0151, 0x00F6, 0x00F7,
    0x0159, 0x016F, 0x00FA, 0x0171, 0x00FC, 0x00FD, 0x0163, 0x02D9,
};

/// The characters of the bytes 0x80 to 0x9F of Windows-1252 (U+FFFD: none), as Python's codec
/// cp1252 gives them; the bytes 0xA0 to 0xFF are those of Latin-1
constexpr std::array<char16_t, 32> WINDOWS_1252{
    0x20AC, 0xFFFD, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
    0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0xFFFD, 0x017D, 0xFFFD,
    0xFFFD, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0xFFFD, 0x017E, 0x0178,
};

constexpr unsigned char HIGH_BYTES = 0x80;
constexpr unsigned char ISO_LETTERS = 0xA0;

/// The character of a byte in an 8-bit encoding
char16_t characterOf(unsigned char byte, TextEncoding encoding) {
    if (byte < HIGH_BYTES) {
        return byte;
    }
    if (encoding == TextEncoding::Iso8859_2) {
        return byte < ISO_LETTERS ? char16_t(byte) : ISO_8859_2[byte - ISO_LETTERS];
    }
    return WINDOWS_1250[byte - HIGH_BYTES];
}

/// How well the bytes above 127 read as text in an 8-bit encoding: letters count, the Polish
/// ones more, a control or a byte without a character counts against it
int scoreOf(const QByteArray& data, TextEncoding encoding) {
    static const QString polish = QStringLiteral("ąćęłńóśźżĄĆĘŁŃÓŚŹŻ");
    constexpr int LETTER = 1;
    constexpr int POLISH_LETTER = 2;
    constexpr int NO_CHARACTER = -5;
    int score = 0;
    for (const char byte : data) {
        const auto value = static_cast<unsigned char>(byte);
        if (value < HIGH_BYTES) {
            continue;
        }
        const QChar ch(characterOf(value, encoding));
        if (ch.unicode() == 0xFFFD || ch.category() == QChar::Other_Control) {
            score += NO_CHARACTER;
        } else if (ch.isLetter()) {
            score += polish.contains(ch) ? LETTER + POLISH_LETTER : LETTER;
        }
    }
    return score;
}

/// The line ends of a text as '\n'
QString withLineFeeds(QString text) {
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text;
}

// =============================================================================
// Titles
// =============================================================================

/// The words of the Polish ordinal numbers, their stems (pierwszy, pierwsza: "pierwsz")
const QString POLISH_ORDINALS = QStringLiteral(
    "(?:pierwsz|drug|trzec|czwart|piąt|szóst|siódm|ósm|dziewiąt|dziesiąt|jedenast|dwunast|"
    "trzynast|czternast|piętnast|szesnast|siedemnast|osiemnast|dziewiętnast|dwudziest|"
    "trzydziest|czterdziest|pięćdziesiąt)\\w*");

/// The English numbers in words, up to ninety-nine
const QString ENGLISH_NUMBERS = QStringLiteral(
    "(?:(?:twenty|thirty|forty|fifty|sixty|seventy|eighty|ninety)(?:[- ](?:one|two|three|four|"
    "five|six|seven|eight|nine))?|one|two|three|four|five|six|seven|eight|nine|ten|eleven|"
    "twelve|thirteen|fourteen|fifteen|sixteen|seventeen|eighteen|nineteen)");

/// A Roman number (I to MMMCMXCIX)
const QString ROMAN = QStringLiteral(
    "(?=[mdclxvi])m{0,3}(?:cm|cd|d?c{0,3})(?:xc|xl|l?x{0,3})(?:ix|iv|v?i{0,3})");

/// What may come after the number of a title: its end, or the title after a space or a
/// punctuation mark
const QString TITLE_END = QStringLiteral("(?=$|[\\s.:,;–—-])");

/// A title that starts with @p word and a number
QRegularExpression titleOf(const QString& word, const QString& numbers) {
    return QRegularExpression(QStringLiteral("^") + word + QStringLiteral("\\s+(?:\\d+|") + ROMAN +
                                  QLatin1Char('|') + numbers + QLatin1Char(')') + TITLE_END,
                              QRegularExpression::CaseInsensitiveOption |
                                  QRegularExpression::UseUnicodePropertiesOption);
}

/// The longest line that can be a title
constexpr qsizetype MAX_TITLE_LENGTH = 100;

bool isTextTitle(const QString& line, const TextReadOptions& options) {
    static const QRegularExpression polish =
        titleOf(QStringLiteral("rozdział"), POLISH_ORDINALS + QStringLiteral("(?:\\s+") + POLISH_ORDINALS +
                                                QLatin1Char(')') + QLatin1Char('?'));
    static const QRegularExpression english = titleOf(QStringLiteral("chapter"), ENGLISH_NUMBERS);
    static const QRegularExpression number(QStringLiteral("^(?:\\d{1,4}|") + ROMAN + QStringLiteral(")\\.?$"),
                                           QRegularExpression::CaseInsensitiveOption);
    if (line.size() > MAX_TITLE_LENGTH) {
        return false;
    }
    for (const QString& name : options.kindTitles) {
        if (!name.isEmpty() && line.compare(name, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    switch (options.titles) {
        case TitlePattern::None:
            return false;
        case TitlePattern::Polish:
            return polish.match(line).hasMatch();
        case TitlePattern::English:
            return english.match(line).hasMatch();
        case TitlePattern::Number:
            return number.match(line).hasMatch() && line.compare(QStringLiteral("."), Qt::CaseInsensitive) != 0;
        case TitlePattern::Capitals: {
            int letters = 0;
            for (const QChar ch : line) {
                if (ch.isLower()) {
                    return false;
                }
                letters += ch.isLetter() ? 1 : 0;
            }
            return letters >= 2;
        }
        case TitlePattern::Prefix:
            return !options.prefix.trimmed().isEmpty() &&
                   line.startsWith(options.prefix.trimmed(), Qt::CaseInsensitive);
    }
    return false;
}

bool isPartTitle(const QString& line, PartPattern pattern) {
    static const QRegularExpression polish = titleOf(QStringLiteral("(?:część|księga|tom)"), POLISH_ORDINALS);
    static const QRegularExpression english = titleOf(QStringLiteral("(?:part|book|volume)"), ENGLISH_NUMBERS);
    if (line.size() > MAX_TITLE_LENGTH) {
        return false;
    }
    switch (pattern) {
        case PartPattern::None:
            return false;
        case PartPattern::Polish:
            return polish.match(line).hasMatch();
        case PartPattern::English:
            return english.match(line).hasMatch();
    }
    return false;
}

/// A paragraph of plain text
ImportBlock paragraphOf(const QString& text, const QString& style, int headingLevel) {
    ImportBlock block;
    block.style = style;
    block.headingLevel = headingLevel;
    QString characters;
    const auto flush = [&]() {
        if (!characters.isEmpty()) {
            ImportRun run;
            run.text = characters;
            block.runs.push_back(std::move(run));
            characters.clear();
        }
    };
    for (const QChar ch : text) {
        if (ch == QLatin1Char('\t') || ch == QChar::LineSeparator) {
            flush();
            ImportRun run;
            run.kind = ch == QLatin1Char('\t') ? ImportRun::Kind::Tab : ImportRun::Kind::LineBreak;
            block.runs.push_back(std::move(run));
        } else if (ch.category() != QChar::Other_Control) {
            characters += ch;
        }
    }
    flush();
    return block;
}

}  // namespace

// =============================================================================
// Encodings
// =============================================================================

TextEncoding detectTextEncoding(const QByteArray& data) {
    if (data.startsWith("\xEF\xBB\xBF")) {
        return TextEncoding::Utf8;
    }
    if (data.startsWith("\xFF\xFE") || data.startsWith("\xFE\xFF")) {
        return TextEncoding::Utf16;
    }
    // UTF-16 without a byte order mark: the zero bytes of the ASCII characters
    if (data.size() >= 2) {
        qsizetype evenZeros = 0;
        qsizetype oddZeros = 0;
        for (qsizetype i = 0; i < data.size(); ++i) {
            if (data.at(i) == '\0') {
                ++((i % 2 == 0) ? evenZeros : oddZeros);
            }
        }
        const qsizetype half = data.size() / 2;
        if (evenZeros * 4 > half * 3 || oddZeros * 4 > half * 3) {
            return TextEncoding::Utf16;
        }
    }
    QStringDecoder utf8(QStringConverter::Utf8);
    const QString text = utf8.decode(data);
    if (!utf8.hasError()) {
        return TextEncoding::Utf8;
    }
    return scoreOf(data, TextEncoding::Iso8859_2) > scoreOf(data, TextEncoding::Windows1250)
               ? TextEncoding::Iso8859_2
               : TextEncoding::Windows1250;
}

std::optional<QChar> windowsCharacter(unsigned char byte, int codePage) {
    if (byte < HIGH_BYTES) {
        return QChar(byte);
    }
    switch (codePage) {
        case 1250:
            return QChar(WINDOWS_1250[byte - HIGH_BYTES]);
        case 1252:
            return QChar(byte < ISO_LETTERS ? WINDOWS_1252[byte - HIGH_BYTES] : char16_t(byte));
        default:
            return std::nullopt;
    }
}

QString decodeText(const QByteArray& data, TextEncoding encoding) {
    switch (encoding) {
        case TextEncoding::Utf8: {
            QStringDecoder decoder(QStringConverter::Utf8, QStringConverter::Flag::Stateless);
            return withLineFeeds(decoder.decode(data.startsWith("\xEF\xBB\xBF") ? data.mid(3) : data));
        }
        case TextEncoding::Utf16: {
            QStringConverter::Encoding utf16 = QStringConverter::Utf16LE;
            QByteArray bytes = data;
            if (bytes.startsWith("\xFE\xFF")) {
                utf16 = QStringConverter::Utf16BE;
                bytes.remove(0, 2);
            } else if (bytes.startsWith("\xFF\xFE")) {
                bytes.remove(0, 2);
            } else {
                // Without a byte order mark: big endian has the zero bytes first
                qsizetype evenZeros = 0;
                qsizetype oddZeros = 0;
                for (qsizetype i = 0; i < bytes.size(); ++i) {
                    if (bytes.at(i) == '\0') {
                        ++((i % 2 == 0) ? evenZeros : oddZeros);
                    }
                }
                utf16 = evenZeros > oddZeros ? QStringConverter::Utf16BE : QStringConverter::Utf16LE;
            }
            QStringDecoder decoder(utf16, QStringConverter::Flag::Stateless);
            return withLineFeeds(decoder.decode(bytes));
        }
        case TextEncoding::Windows1250:
        case TextEncoding::Iso8859_2: {
            QString text;
            text.reserve(data.size());
            for (const char byte : data) {
                text += QChar(characterOf(static_cast<unsigned char>(byte), encoding));
            }
            return withLineFeeds(text);
        }
    }
    return QString();
}

// =============================================================================
// Plain text
// =============================================================================

bool isPartTitle(const QString& title) {
    const QString line = title.simplified();
    return isPartTitle(line, PartPattern::Polish) || isPartTitle(line, PartPattern::English);
}

bool paragraphsByEmptyLines(const QString& text) {
    const QStringList lines = text.split(QLatin1Char('\n'));
    int withText = 0;
    int followed = 0;
    for (qsizetype i = 0; i < lines.size(); ++i) {
        if (lines[i].trimmed().isEmpty()) {
            continue;
        }
        ++withText;
        if (i + 1 < lines.size() && lines[i + 1].trimmed().isEmpty()) {
            ++followed;
        }
    }
    return withText > 0 && followed * 2 > withText;
}

ImportDocument readPlainText(const QString& text, const TextReadOptions& options) {
    ImportDocument document;
    const int textLevel = options.parts != PartPattern::None ? 2 : 1;

    int emptyLines = 0;  // before the paragraph that comes next
    QStringList lines;   // of the paragraph (TextReadOptions::emptyLineParagraphs)

    const auto addBlock = [&](ImportBlock block) { document.blocks.push_back(std::move(block)); };
    // Whether the empty lines before a paragraph start a text; when not, they are empty
    // paragraphs (but the one that ends a paragraph, and those at the start of the file)
    const auto startsText = [&]() {
        const bool starts = !document.blocks.empty() && options.emptyLinesForText > 0 &&
                            emptyLines >= options.emptyLinesForText;
        if (!starts && !document.blocks.empty()) {
            const int empties = options.emptyLineParagraphs ? emptyLines - 1 : emptyLines;
            for (int i = 0; i < empties; ++i) {
                addBlock(paragraphOf(QString(), QStringLiteral("normal"), 0));
            }
        }
        emptyLines = 0;
        return starts;
    };
    const auto addParagraph = [&](const QString& paragraph, bool singleLine) {
        const bool starts = startsText();
        const QString line = paragraph.trimmed();
        int level = 0;
        if (singleLine && !line.isEmpty()) {
            if (isPartTitle(line, options.parts)) {
                level = 1;
            } else if (isTextTitle(line, options)) {
                level = textLevel;
            }
        }
        ImportBlock block = paragraphOf(level > 0 ? line : paragraph,
                                        level > 0 ? QStringLiteral("heading_%1").arg(level)
                                                  : QStringLiteral("normal"),
                                        level);
        block.startsText = starts;
        addBlock(std::move(block));
    };
    const auto flushLines = [&]() {
        if (!lines.isEmpty()) {
            addParagraph(lines.join(QLatin1Char(' ')), lines.size() == 1);
            lines.clear();
        }
    };

    // A form feed is a page break between the lines around it
    QString pages = text;
    pages.replace(QChar(0x000C), QStringLiteral("\n\f\n"));
    for (const QString& line : pages.split(QLatin1Char('\n'))) {
        if (line == QChar(0x000C)) {
            flushLines();
            emptyLines = 0;
            ImportBlock block;
            block.kind = ImportBlock::Kind::PageBreak;
            addBlock(std::move(block));
        } else if (line.trimmed().isEmpty()) {
            flushLines();
            ++emptyLines;
        } else if (options.emptyLineParagraphs) {
            lines << (lines.isEmpty() ? line : line.trimmed());
        } else {
            addParagraph(line, true);
        }
    }
    flushLines();

    // Without the titles of parts the texts' titles are of the first level
    std::vector<ImportBlock>& blocks = document.blocks;
    if (std::none_of(blocks.begin(), blocks.end(), [](const ImportBlock& block) { return block.headingLevel == 1; })) {
        for (ImportBlock& block : blocks) {
            if (block.headingLevel == 2) {
                block.headingLevel = 1;
                block.style = QStringLiteral("heading_1");
            }
        }
    }
    std::map<QString, int> used;
    for (const ImportBlock& block : blocks) {
        if (block.kind == ImportBlock::Kind::Paragraph) {
            ++used[block.style];
        }
    }
    for (const auto& [key, count] : used) {
        ImportStyle style;
        style.key = key;
        style.name = key;
        style.headingLevel = key.startsWith(QLatin1String("heading_")) ? key.mid(8).toInt() : 0;
        style.paragraphs = count;
        document.styles.push_back(std::move(style));
    }
    return document;
}

}  // namespace kalahari::editor
