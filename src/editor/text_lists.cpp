/// @file text_lists.cpp
/// @brief Bulleted and numbered lists: items, formats of the levels, numbers

#include <kalahari/editor/text_lists.h>
#include <kalahari/editor/paragraph_format.h>

#include <QStringList>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QUrl>
#include <QXmlStreamAttributes>

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace kalahari::editor {

namespace {

/// Letters of the alphabetic numbers
constexpr int ALPHABET_LETTERS = 26;

/// Roman numerals from the largest, with their values
struct RomanNumeral {
    int value;
    const char* lower;
};
constexpr std::array<RomanNumeral, 13> ROMAN_NUMERALS = {{
    {1000, "m"}, {900, "cm"}, {500, "d"}, {400, "cd"}, {100, "c"}, {90, "xc"}, {50, "l"},
    {40, "xl"}, {10, "x"}, {9, "ix"}, {5, "v"}, {4, "iv"}, {1, "i"},
}};

/// Separators of the definition's text: of the levels and of the parts of a level
constexpr QChar LEVEL_SEPARATOR = QLatin1Char(';');
constexpr QChar PART_SEPARATOR = QLatin1Char(',');

/// Parts of a level in the definition's text
constexpr int LEVEL_PARTS = 6;

/// The bullets of <ul type="...">, by name
struct BulletName {
    const char* name;
    char16_t bullet;
};
constexpr std::array<BulletName, 6> BULLET_NAMES = {{
    {"disc", BULLET_DISC},
    {"circle", BULLET_CIRCLE},
    {"square", BULLET_SQUARE},
    {"dash", BULLET_DASH},
    {"check", BULLET_CHECK},
    {"arrow", BULLET_ARROW},
}};

/// Code of a number format in the definition's text and in <ol type="...">
QString formatCode(ListNumberFormat format) {
    switch (format) {
        case ListNumberFormat::Decimal:
            return QStringLiteral("1");
        case ListNumberFormat::LowerAlpha:
            return QStringLiteral("a");
        case ListNumberFormat::UpperAlpha:
            return QStringLiteral("A");
        case ListNumberFormat::LowerRoman:
            return QStringLiteral("i");
        case ListNumberFormat::UpperRoman:
            return QStringLiteral("I");
        case ListNumberFormat::Bullet:
            return QStringLiteral("b");
    }
    return QStringLiteral("1");
}

/// The number format of a code; nothing for an unknown one
std::optional<ListNumberFormat> formatOfCode(const QString& code) {
    for (const ListNumberFormat format :
         {ListNumberFormat::Decimal, ListNumberFormat::LowerAlpha, ListNumberFormat::UpperAlpha,
          ListNumberFormat::LowerRoman, ListNumberFormat::UpperRoman, ListNumberFormat::Bullet}) {
        if (formatCode(format) == code) {
            return format;
        }
    }
    return std::nullopt;
}

QString alignmentCode(ListMarkerAlignment alignment) {
    switch (alignment) {
        case ListMarkerAlignment::Center:
            return QStringLiteral("center");
        case ListMarkerAlignment::Right:
            return QStringLiteral("right");
        case ListMarkerAlignment::Left:
            break;
    }
    return QStringLiteral("left");
}

ListMarkerAlignment alignmentOfCode(const QString& code) {
    if (code == QLatin1String("center")) {
        return ListMarkerAlignment::Center;
    }
    if (code == QLatin1String("right")) {
        return ListMarkerAlignment::Right;
    }
    return ListMarkerAlignment::Left;
}

/// The default indents of a level: the number one step further in than the level above,
/// the text one step further than the number
void setDefaultIndents(ListLevel& format, int level) {
    format.markerIndent = LIST_INDENT_STEP * (level + 1);
    format.textIndent = LIST_INDENT_STEP * (level + 2);
}

QString percentEncoded(const QString& text) {
    return QString::fromLatin1(QUrl::toPercentEncoding(text));
}

QString percentDecoded(const QString& text) {
    return QUrl::fromPercentEncoding(text.toLatin1());
}

/// A number of a part of the definition's text, or @p fallback
qreal numberOfPart(const QString& part, qreal fallback) {
    bool ok = false;
    const qreal value = part.toDouble(&ok);
    return ok && std::isfinite(value) ? value : fallback;
}

/// The first grapheme of a text: a bullet is one character, also one of a surrogate pair or
/// with combining marks
QString firstCharacter(const QString& text) {
    if (text.isEmpty()) {
        return text;
    }
    qsizetype length = text.at(0).isHighSurrogate() && text.size() > 1 ? 2 : 1;
    while (length < text.size() && text.at(length).isMark()) {
        ++length;
    }
    return text.left(length);
}

/// An attribute's value, or empty without one
QString attributeOf(const QXmlStreamAttributes& attributes, const char* name) {
    return attributes.value(QLatin1String(name)).toString();
}

}  // namespace

// =============================================================================
// Formats of the levels
// =============================================================================

ListLevel ListDefinition::defaultLevel(ListKind kind, int level) {
    ListLevel format;
    const int cycle = level % 3;
    if (kind == ListKind::Bulleted) {
        format.format = ListNumberFormat::Bullet;
        format.suffix.clear();
        format.bullet = QString(cycle == 0   ? QChar(BULLET_DISC)
                                : cycle == 1 ? QChar(BULLET_CIRCLE)
                                             : QChar(BULLET_SQUARE));
    } else {
        format.format = cycle == 0   ? ListNumberFormat::Decimal
                        : cycle == 1 ? ListNumberFormat::LowerAlpha
                                     : ListNumberFormat::LowerRoman;
        format.suffix = cycle == 1 ? QStringLiteral(")") : QStringLiteral(".");
    }
    setDefaultIndents(format, level);
    return format;
}

ListDefinition ListDefinition::ofKind(ListKind kind) {
    ListDefinition definition;
    for (int level = 0; level < LIST_LEVELS; ++level) {
        definition.levels[static_cast<size_t>(level)] = defaultLevel(kind, level);
    }
    return definition;
}

ListDefinition ListDefinition::numbered() {
    return ofKind(ListKind::Numbered);
}

ListDefinition ListDefinition::bulleted() {
    return ofKind(ListKind::Bulleted);
}

ListKind ListDefinition::kind() const {
    return levels[0].isBullet() ? ListKind::Bulleted : ListKind::Numbered;
}

QString ListDefinition::toString() const {
    QStringList parts;
    parts.reserve(LIST_LEVELS);
    for (const ListLevel& level : levels) {
        const QString text = level.isBullet() ? level.bullet : level.suffix;
        parts.append(QStringList{formatCode(level.format), percentEncoded(text),
                                 QString::number(level.start), kmlNumber(level.markerIndent),
                                 kmlNumber(level.textIndent), alignmentCode(level.alignment)}
                         .join(PART_SEPARATOR));
    }
    return parts.join(LEVEL_SEPARATOR);
}

ListDefinition ListDefinition::fromString(const QString& text) {
    ListDefinition definition = numbered();
    const QStringList levels = text.split(LEVEL_SEPARATOR);
    for (qsizetype i = 0; i < levels.size() && i < LIST_LEVELS; ++i) {
        const QStringList parts = levels.at(i).split(PART_SEPARATOR);
        if (parts.size() < LEVEL_PARTS) {
            continue;
        }
        const std::optional<ListNumberFormat> format = formatOfCode(parts.at(0));
        if (!format) {
            continue;
        }
        ListLevel& level = definition.levels[static_cast<size_t>(i)];
        level.format = *format;
        const QString textPart = percentDecoded(parts.at(1));
        if (level.isBullet()) {
            level.suffix.clear();
            level.bullet = textPart.isEmpty() ? QString(QChar(BULLET_DISC)) : firstCharacter(textPart);
        } else {
            level.suffix = textPart;
            level.bullet.clear();
        }
        level.start = std::clamp(static_cast<int>(numberOfPart(parts.at(2), 1)), MIN_LIST_NUMBER,
                                 MAX_LIST_NUMBER);
        level.markerIndent = std::clamp(numberOfPart(parts.at(3), level.markerIndent), 0.0,
                                        MAX_PARAGRAPH_INDENT);
        level.textIndent =
            std::clamp(numberOfPart(parts.at(4), level.textIndent), 0.0, MAX_PARAGRAPH_INDENT);
        level.alignment = alignmentOfCode(parts.at(5));
    }
    return definition;
}

QString listNumberText(int number, ListNumberFormat format) {
    switch (format) {
        case ListNumberFormat::Bullet:
            return QString();
        case ListNumberFormat::Decimal:
            return QString::number(number);
        case ListNumberFormat::LowerAlpha:
        case ListNumberFormat::UpperAlpha: {
            if (number < 1) {
                return QString::number(number);
            }
            // a ... z, aa ... zz, aaa ...
            const QChar letter(static_cast<char16_t>(u'a' + (number - 1) % ALPHABET_LETTERS));
            const QString text(1 + (number - 1) / ALPHABET_LETTERS, letter);
            return format == ListNumberFormat::UpperAlpha ? text.toUpper() : text;
        }
        case ListNumberFormat::LowerRoman:
        case ListNumberFormat::UpperRoman: {
            if (number < 1) {
                return QString::number(number);
            }
            QString text;
            int rest = number;
            for (const RomanNumeral& numeral : ROMAN_NUMERALS) {
                while (rest >= numeral.value) {
                    text += QLatin1String(numeral.lower);
                    rest -= numeral.value;
                }
            }
            return format == ListNumberFormat::UpperRoman ? text.toUpper() : text;
        }
    }
    return QString::number(number);
}

QString listMarkerText(const ListLevel& level, int number) {
    if (level.isBullet()) {
        return level.bullet;
    }
    return listNumberText(number, level.format) + level.suffix;
}

// =============================================================================
// The items of a document
// =============================================================================

std::optional<ListItem> listItemOf(const QTextBlockFormat& format) {
    const int id = format.intProperty(LIST_ID_PROPERTY);
    if (id <= 0) {
        return std::nullopt;
    }
    ListItem item;
    item.id = id;
    item.level = std::clamp(format.intProperty(LIST_LEVEL_PROPERTY), 0, LIST_LEVELS - 1);
    item.definition = ListDefinition::fromString(format.stringProperty(LIST_DEFINITION_PROPERTY));
    if (format.hasProperty(LIST_VALUE_PROPERTY)) {
        item.value = std::clamp(format.intProperty(LIST_VALUE_PROPERTY), MIN_LIST_NUMBER,
                                MAX_LIST_NUMBER);
    }
    item.numbered = !format.boolProperty(LIST_UNNUMBERED_PROPERTY);
    return item;
}

bool isListItem(const QTextBlockFormat& format) {
    return format.intProperty(LIST_ID_PROPERTY) > 0;
}

void setListItem(QTextBlockFormat& format, const ListItem& item) {
    format.setProperty(LIST_ID_PROPERTY, item.id);
    format.setProperty(LIST_LEVEL_PROPERTY, std::clamp(item.level, 0, LIST_LEVELS - 1));
    format.setProperty(LIST_DEFINITION_PROPERTY, item.definition.toString());
    if (item.value.has_value()) {
        format.setProperty(LIST_VALUE_PROPERTY, std::clamp(*item.value, MIN_LIST_NUMBER, MAX_LIST_NUMBER));
    } else {
        format.clearProperty(LIST_VALUE_PROPERTY);
    }
    if (item.numbered) {
        format.clearProperty(LIST_UNNUMBERED_PROPERTY);
    } else {
        format.setProperty(LIST_UNNUMBERED_PROPERTY, true);
    }
}

void clearListItem(QTextBlockFormat& format) {
    for (const int property : {LIST_ID_PROPERTY, LIST_LEVEL_PROPERTY, LIST_DEFINITION_PROPERTY,
                               LIST_VALUE_PROPERTY, LIST_UNNUMBERED_PROPERTY}) {
        format.clearProperty(property);
    }
}

int newListId(const QTextDocument& document) {
    int largest = 0;
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        largest = std::max(largest, block.blockFormat().intProperty(LIST_ID_PROPERTY));
    }
    return largest + 1;
}

void giveListsNewIds(QTextDocument& content, const QTextDocument& target) {
    const int offset = newListId(target) - 1;
    if (offset <= 0) {
        return;
    }
    QTextCursor cursor(&content);
    for (QTextBlock block = content.begin(); block.isValid(); block = block.next()) {
        QTextBlockFormat format = block.blockFormat();
        const int id = format.intProperty(LIST_ID_PROPERTY);
        if (id > 0) {
            format.setProperty(LIST_ID_PROPERTY, id + offset);
            cursor.setPosition(block.position());
            cursor.setBlockFormat(format);
        }
    }
}

std::vector<int> listNumbers(const QTextDocument& document) {
    // The counters of each list, by level; a level without a counter starts at its start
    struct Counters {
        std::array<int, LIST_LEVELS> value{};
        std::array<bool, LIST_LEVELS> started{};
    };
    std::unordered_map<int, Counters> lists;
    std::vector<int> numbers;
    numbers.reserve(static_cast<size_t>(document.blockCount()));
    for (QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
        const QTextBlockFormat format = block.blockFormat();
        const int id = format.intProperty(LIST_ID_PROPERTY);
        if (id <= 0 || format.boolProperty(LIST_UNNUMBERED_PROPERTY)) {
            numbers.push_back(-1);
            continue;
        }
        const int level = std::clamp(format.intProperty(LIST_LEVEL_PROPERTY), 0, LIST_LEVELS - 1);
        Counters& counters = lists[id];
        const auto index = static_cast<size_t>(level);
        // A deeper level starts again after an item of this one
        std::fill(counters.started.begin() + level + 1, counters.started.end(), false);
        if (format.hasProperty(LIST_VALUE_PROPERTY)) {
            counters.value[index] =
                std::clamp(format.intProperty(LIST_VALUE_PROPERTY), MIN_LIST_NUMBER, MAX_LIST_NUMBER);
        } else if (counters.started[index]) {
            counters.value[index] = std::min(counters.value[index] + 1, MAX_LIST_NUMBER);
        } else {
            counters.value[index] =
                ListDefinition::fromString(format.stringProperty(LIST_DEFINITION_PROPERTY))
                    .levels[index]
                    .start;
        }
        counters.started[index] = true;
        numbers.push_back(counters.value[index]);
    }
    return numbers;
}

std::vector<QString> listMarkers(const QTextDocument& document) {
    const std::vector<int> numbers = listNumbers(document);
    std::vector<QString> markers(numbers.size());
    QTextBlock block = document.begin();
    for (size_t i = 0; i < numbers.size() && block.isValid(); ++i, block = block.next()) {
        if (numbers[i] < 0) {
            continue;
        }
        if (const std::optional<ListItem> item = listItemOf(block.blockFormat())) {
            markers[i] = listMarkerText(item->levelFormat(), numbers[i]);
        }
    }
    return markers;
}

// =============================================================================
// KML
// =============================================================================

QString kmlListElementOf(const ListLevel& level) {
    return level.isBullet() ? QStringLiteral("ul") : QStringLiteral("ol");
}

ListLevel listLevelFromKml(bool numbered, const QXmlStreamAttributes& attributes, int level) {
    ListLevel format;
    setDefaultIndents(format, level);
    const QString type = attributeOf(attributes, "type").trimmed();
    if (numbered) {
        const std::optional<ListNumberFormat> numberFormat = formatOfCode(type);
        format.format = numberFormat.has_value() && *numberFormat != ListNumberFormat::Bullet
                            ? *numberFormat
                            : ListNumberFormat::Decimal;
        if (attributes.hasAttribute(QStringLiteral("suffix"))) {
            format.suffix = attributeOf(attributes, "suffix");
        }
        bool ok = false;
        const int start = attributeOf(attributes, "start").trimmed().toInt(&ok);
        if (ok) {
            format.start = std::clamp(start, MIN_LIST_NUMBER, MAX_LIST_NUMBER);
        }
    } else {
        format.format = ListNumberFormat::Bullet;
        format.suffix.clear();
        format.bullet = QString(QChar(BULLET_DISC));
        const QString name = type.toLower();
        const auto known = std::find_if(BULLET_NAMES.begin(), BULLET_NAMES.end(),
                                        [&name](const BulletName& bullet) {
                                            return name == QLatin1String(bullet.name);
                                        });
        if (known != BULLET_NAMES.end()) {
            format.bullet = QString(QChar(known->bullet));
        } else if (!type.isEmpty()) {
            format.bullet = firstCharacter(type);
        }
    }
    const auto indent = [&attributes](const char* name, qreal fallback) {
        bool ok = false;
        const qreal value = attributeOf(attributes, name).trimmed().toDouble(&ok);
        return ok && std::isfinite(value) ? std::clamp(value, 0.0, MAX_PARAGRAPH_INDENT) : fallback;
    };
    format.markerIndent = indent("marker-indent", format.markerIndent);
    format.textIndent = indent("text-indent", format.textIndent);
    format.alignment = alignmentOfCode(attributeOf(attributes, "marker-align").trimmed().toLower());
    return format;
}

QString listLevelToKml(const ListLevel& level, int levelIndex) {
    const auto attribute = [](const char* name, const QString& value) {
        QString escaped = value;
        escaped.replace(QLatin1Char('&'), QStringLiteral("&amp;"))
            .replace(QLatin1Char('<'), QStringLiteral("&lt;"))
            .replace(QLatin1Char('>'), QStringLiteral("&gt;"))
            .replace(QLatin1Char('"'), QStringLiteral("&quot;"));
        return QLatin1Char(' ') + QLatin1String(name) + QStringLiteral("=\"") + escaped +
               QLatin1Char('"');
    };
    QString attributes;
    if (level.isBullet()) {
        const auto known = std::find_if(BULLET_NAMES.begin(), BULLET_NAMES.end(),
                                        [&level](const BulletName& bullet) {
                                            return level.bullet == QString(QChar(bullet.bullet));
                                        });
        if (known == BULLET_NAMES.end()) {
            attributes += attribute("type", level.bullet);
        } else if (known->bullet != BULLET_DISC) {
            attributes += attribute("type", QLatin1String(known->name));
        }
    } else {
        if (level.format != ListNumberFormat::Decimal) {
            attributes += attribute("type", formatCode(level.format));
        }
        if (level.suffix != QLatin1String(".")) {
            attributes += attribute("suffix", level.suffix);
        }
        if (level.start != 1) {
            attributes += attribute("start", QString::number(level.start));
        }
    }
    ListLevel defaults;
    setDefaultIndents(defaults, levelIndex);
    if (kmlNumber(level.markerIndent) != kmlNumber(defaults.markerIndent)) {
        attributes += attribute("marker-indent", kmlNumber(level.markerIndent));
    }
    if (kmlNumber(level.textIndent) != kmlNumber(defaults.textIndent)) {
        attributes += attribute("text-indent", kmlNumber(level.textIndent));
    }
    if (level.alignment != ListMarkerAlignment::Left) {
        attributes += attribute("marker-align", alignmentCode(level.alignment));
    }
    return attributes;
}

}  // namespace kalahari::editor
