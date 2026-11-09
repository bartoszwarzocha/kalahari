/// @file paragraph_format.cpp
/// @brief A paragraph's own formatting: indents, spacing, line spacing, breaks

#include <kalahari/editor/paragraph_format.h>

#include <QTextBlockFormat>

#include <algorithm>
#include <cmath>

namespace kalahari::editor {

namespace {

/// KML attributes of the paragraph's own formatting
constexpr char KML_LEFT_INDENT[] = "indent-left";
constexpr char KML_RIGHT_INDENT[] = "indent-right";
constexpr char KML_FIRST_LINE_INDENT[] = "indent-first";
constexpr char KML_SPACE_BEFORE[] = "space-before";
constexpr char KML_SPACE_AFTER[] = "space-after";
constexpr char KML_LINE_HEIGHT[] = "line-height";
constexpr char KML_LINE_HEIGHT_MIN[] = "line-height-min";
constexpr char KML_LINE_HEIGHT_EXACT[] = "line-height-exact";
constexpr char KML_KEEP_WITH_NEXT[] = "keep-with-next";
constexpr char KML_KEEP_TOGETHER[] = "keep-together";
constexpr char KML_PAGE_BREAK_BEFORE[] = "page-break-before";
constexpr char KML_WIDOW_CONTROL[] = "widow-control";

/// Decimals of a number of points in KML
constexpr int KML_DECIMALS = 2;

std::optional<qreal> numberProperty(const QTextBlockFormat& format, int property) {
    if (!format.hasProperty(property)) {
        return std::nullopt;
    }
    return format.doubleProperty(property);
}

std::optional<bool> flagProperty(const QTextBlockFormat& format, int property) {
    if (!format.hasProperty(property)) {
        return std::nullopt;
    }
    return format.boolProperty(property);
}

template <typename T>
void putProperty(QTextBlockFormat& format, int property, const std::optional<T>& value) {
    if (value.has_value()) {
        format.setProperty(property, *value);
    }
}

/// A number of an attribute, brought into [minimum, maximum]; nothing without one
std::optional<qreal> kmlNumberOf(const QXmlStreamAttributes& attributes, const char* name,
                                 qreal minimum, qreal maximum) {
    const QString name16 = QLatin1String(name);
    if (!attributes.hasAttribute(name16)) {
        return std::nullopt;
    }
    bool ok = false;
    const qreal value = attributes.value(name16).toString().trimmed().toDouble(&ok);
    if (!ok || !std::isfinite(value)) {
        return std::nullopt;
    }
    return std::clamp(value, minimum, maximum);
}

/// A flag of an attribute: "true" or "1" set it (any letter case), anything else clears it
std::optional<bool> kmlFlagOf(const QXmlStreamAttributes& attributes, const char* name) {
    const QString name16 = QLatin1String(name);
    if (!attributes.hasAttribute(name16)) {
        return std::nullopt;
    }
    const QString value = attributes.value(name16).toString().trimmed().toLower();
    return value == QLatin1String("true") || value == QLatin1String("1");
}

QString kmlFlag(bool value) {
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

}  // namespace

bool ParagraphFormat::isEmpty() const {
    return *this == ParagraphFormat();
}

const std::vector<int>& paragraphFormatProperties() {
    static const std::vector<int> properties = {
        PARAGRAPH_LEFT_INDENT_PROPERTY,      PARAGRAPH_RIGHT_INDENT_PROPERTY,
        PARAGRAPH_FIRST_LINE_INDENT_PROPERTY, PARAGRAPH_SPACE_BEFORE_PROPERTY,
        PARAGRAPH_SPACE_AFTER_PROPERTY,      PARAGRAPH_LINE_SPACING_RULE_PROPERTY,
        PARAGRAPH_LINE_SPACING_PROPERTY,     PARAGRAPH_KEEP_WITH_NEXT_PROPERTY,
        PARAGRAPH_KEEP_TOGETHER_PROPERTY,    PARAGRAPH_PAGE_BREAK_BEFORE_PROPERTY,
        PARAGRAPH_WIDOW_CONTROL_PROPERTY,
    };
    return properties;
}

ParagraphFormat paragraphFormatOf(const QTextBlockFormat& format) {
    ParagraphFormat paragraph;
    paragraph.leftIndent = numberProperty(format, PARAGRAPH_LEFT_INDENT_PROPERTY);
    paragraph.rightIndent = numberProperty(format, PARAGRAPH_RIGHT_INDENT_PROPERTY);
    paragraph.firstLineIndent = numberProperty(format, PARAGRAPH_FIRST_LINE_INDENT_PROPERTY);
    paragraph.spaceBefore = numberProperty(format, PARAGRAPH_SPACE_BEFORE_PROPERTY);
    paragraph.spaceAfter = numberProperty(format, PARAGRAPH_SPACE_AFTER_PROPERTY);
    if (format.hasProperty(PARAGRAPH_LINE_SPACING_PROPERTY)) {
        LineSpacing spacing;
        const int rule = format.intProperty(PARAGRAPH_LINE_SPACING_RULE_PROPERTY);
        spacing.rule = rule == static_cast<int>(LineSpacingRule::AtLeast)   ? LineSpacingRule::AtLeast
                       : rule == static_cast<int>(LineSpacingRule::Exactly) ? LineSpacingRule::Exactly
                                                                            : LineSpacingRule::Multiple;
        spacing.value = format.doubleProperty(PARAGRAPH_LINE_SPACING_PROPERTY);
        paragraph.lineSpacing = spacing;
    }
    paragraph.keepWithNext = flagProperty(format, PARAGRAPH_KEEP_WITH_NEXT_PROPERTY);
    paragraph.keepTogether = flagProperty(format, PARAGRAPH_KEEP_TOGETHER_PROPERTY);
    paragraph.pageBreakBefore = flagProperty(format, PARAGRAPH_PAGE_BREAK_BEFORE_PROPERTY);
    paragraph.widowControl = flagProperty(format, PARAGRAPH_WIDOW_CONTROL_PROPERTY);
    return paragraph;
}

void mergeParagraphFormat(QTextBlockFormat& format, const ParagraphFormat& paragraph) {
    putProperty(format, PARAGRAPH_LEFT_INDENT_PROPERTY, paragraph.leftIndent);
    putProperty(format, PARAGRAPH_RIGHT_INDENT_PROPERTY, paragraph.rightIndent);
    putProperty(format, PARAGRAPH_FIRST_LINE_INDENT_PROPERTY, paragraph.firstLineIndent);
    putProperty(format, PARAGRAPH_SPACE_BEFORE_PROPERTY, paragraph.spaceBefore);
    putProperty(format, PARAGRAPH_SPACE_AFTER_PROPERTY, paragraph.spaceAfter);
    if (paragraph.lineSpacing.has_value()) {
        format.setProperty(PARAGRAPH_LINE_SPACING_RULE_PROPERTY,
                           static_cast<int>(paragraph.lineSpacing->rule));
        format.setProperty(PARAGRAPH_LINE_SPACING_PROPERTY, paragraph.lineSpacing->value);
    }
    putProperty(format, PARAGRAPH_KEEP_WITH_NEXT_PROPERTY, paragraph.keepWithNext);
    putProperty(format, PARAGRAPH_KEEP_TOGETHER_PROPERTY, paragraph.keepTogether);
    putProperty(format, PARAGRAPH_PAGE_BREAK_BEFORE_PROPERTY, paragraph.pageBreakBefore);
    putProperty(format, PARAGRAPH_WIDOW_CONTROL_PROPERTY, paragraph.widowControl);
}

void setParagraphFormat(QTextBlockFormat& format, const ParagraphFormat& paragraph) {
    clearParagraphFormat(format);
    mergeParagraphFormat(format, paragraph);
}

void clearParagraphFormat(QTextBlockFormat& format) {
    for (const int property : paragraphFormatProperties()) {
        format.clearProperty(property);
    }
}

QString paragraphFormatKey(const QTextBlockFormat& format) {
    QString key;
    for (const int property : paragraphFormatProperties()) {
        if (format.hasProperty(property)) {
            key += QString::number(property - QTextFormat::UserProperty) + QLatin1Char('=') +
                   format.property(property).toString() + QLatin1Char(';');
        }
    }
    return key;
}

ParagraphFormat paragraphFormatFromKml(const QXmlStreamAttributes& attributes) {
    ParagraphFormat paragraph;
    paragraph.leftIndent =
        kmlNumberOf(attributes, KML_LEFT_INDENT, -MAX_PARAGRAPH_INDENT, MAX_PARAGRAPH_INDENT);
    paragraph.rightIndent =
        kmlNumberOf(attributes, KML_RIGHT_INDENT, -MAX_PARAGRAPH_INDENT, MAX_PARAGRAPH_INDENT);
    paragraph.firstLineIndent =
        kmlNumberOf(attributes, KML_FIRST_LINE_INDENT, -MAX_PARAGRAPH_INDENT, MAX_PARAGRAPH_INDENT);
    paragraph.spaceBefore = kmlNumberOf(attributes, KML_SPACE_BEFORE, 0.0, MAX_PARAGRAPH_SPACING);
    paragraph.spaceAfter = kmlNumberOf(attributes, KML_SPACE_AFTER, 0.0, MAX_PARAGRAPH_SPACING);
    if (const auto multiple = kmlNumberOf(attributes, KML_LINE_HEIGHT, MIN_LINE_SPACING_MULTIPLE,
                                          MAX_LINE_SPACING_MULTIPLE)) {
        paragraph.lineSpacing = LineSpacing{LineSpacingRule::Multiple, *multiple};
    } else if (const auto atLeast = kmlNumberOf(attributes, KML_LINE_HEIGHT_MIN,
                                                MIN_LINE_SPACING_POINTS, MAX_LINE_SPACING_POINTS)) {
        paragraph.lineSpacing = LineSpacing{LineSpacingRule::AtLeast, *atLeast};
    } else if (const auto exactly = kmlNumberOf(attributes, KML_LINE_HEIGHT_EXACT,
                                                MIN_LINE_SPACING_POINTS, MAX_LINE_SPACING_POINTS)) {
        paragraph.lineSpacing = LineSpacing{LineSpacingRule::Exactly, *exactly};
    }
    paragraph.keepWithNext = kmlFlagOf(attributes, KML_KEEP_WITH_NEXT);
    paragraph.keepTogether = kmlFlagOf(attributes, KML_KEEP_TOGETHER);
    paragraph.pageBreakBefore = kmlFlagOf(attributes, KML_PAGE_BREAK_BEFORE);
    paragraph.widowControl = kmlFlagOf(attributes, KML_WIDOW_CONTROL);
    return paragraph;
}

std::vector<std::pair<QString, QString>> paragraphFormatToKml(const ParagraphFormat& paragraph) {
    std::vector<std::pair<QString, QString>> attributes;
    const auto number = [&attributes](const char* name, const std::optional<qreal>& value) {
        if (value.has_value()) {
            attributes.emplace_back(QLatin1String(name), kmlNumber(*value));
        }
    };
    const auto flag = [&attributes](const char* name, const std::optional<bool>& value) {
        if (value.has_value()) {
            attributes.emplace_back(QLatin1String(name), kmlFlag(*value));
        }
    };
    number(KML_LEFT_INDENT, paragraph.leftIndent);
    number(KML_RIGHT_INDENT, paragraph.rightIndent);
    number(KML_FIRST_LINE_INDENT, paragraph.firstLineIndent);
    number(KML_SPACE_BEFORE, paragraph.spaceBefore);
    number(KML_SPACE_AFTER, paragraph.spaceAfter);
    if (paragraph.lineSpacing.has_value()) {
        const char* name = paragraph.lineSpacing->rule == LineSpacingRule::AtLeast ? KML_LINE_HEIGHT_MIN
                           : paragraph.lineSpacing->rule == LineSpacingRule::Exactly
                               ? KML_LINE_HEIGHT_EXACT
                               : KML_LINE_HEIGHT;
        number(name, paragraph.lineSpacing->value);
    }
    flag(KML_KEEP_WITH_NEXT, paragraph.keepWithNext);
    flag(KML_KEEP_TOGETHER, paragraph.keepTogether);
    flag(KML_PAGE_BREAK_BEFORE, paragraph.pageBreakBefore);
    flag(KML_WIDOW_CONTROL, paragraph.widowControl);
    return attributes;
}

QString kmlNumber(qreal value) {
    // -0 is written as 0
    const qreal rounded = std::round(value * 100.0) / 100.0;
    QString text = QString::number(rounded == 0.0 ? 0.0 : rounded, 'f', KML_DECIMALS);
    while (text.contains(QLatin1Char('.')) &&
           (text.endsWith(QLatin1Char('0')) || text.endsWith(QLatin1Char('.')))) {
        text.chop(1);
    }
    return text;
}

}  // namespace kalahari::editor
