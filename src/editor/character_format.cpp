/// @file character_format.cpp
/// @brief The character formatting of Format > Font and the changes of Format > Change Case

#include <kalahari/editor/character_format.h>
#include <kalahari/editor/kml_format_registry.h>
#include <QBrush>
#include <QTextBoundaryFinder>
#include <algorithm>
#include <cmath>

namespace kalahari::editor {

namespace {

/// @brief Whether a character ends a sentence
bool endsSentence(QChar ch)
{
    return ch == QLatin1Char('.') || ch == QLatin1Char('!') || ch == QLatin1Char('?') ||
           ch == QChar(0x2026);  // ellipsis
}

/// @brief Whether a character may close a sentence after its end: a quotation mark or a
///        bracket, as in ‘…end.” Next’
bool closesSentence(QChar ch)
{
    return ch.category() == QChar::Punctuation_Close ||
           ch.category() == QChar::Punctuation_FinalQuote ||
           ch.category() == QChar::Punctuation_InitialQuote || ch == QLatin1Char('"') ||
           ch == QLatin1Char('\'');
}

}  // namespace

void mergeCharacterFormat(QTextCharFormat& format, const CharacterFormat& changes)
{
    if (changes.family) {
        format.setFontFamilies({*changes.family});
    }
    if (changes.size) {
        format.setFontPointSize(*changes.size);
    }
    if (changes.bold) {
        format.setFontWeight(*changes.bold ? QFont::Bold : QFont::Normal);
    }
    if (changes.italic) {
        format.setFontItalic(*changes.italic);
    }
    if (changes.underline) {
        format.clearProperty(QTextFormat::FontUnderline);
        format.clearProperty(QTextFormat::TextUnderlineStyle);
        format.setFontUnderline(*changes.underline);
    }
    if (changes.strikethrough) {
        format.setFontStrikeOut(*changes.strikethrough);
    }
    if (changes.position) {
        format.setVerticalAlignment(*changes.position);
    }
    if (changes.capitals) {
        format.setFontCapitalization(*changes.capitals);
    }
    if (changes.letterSpacing) {
        if (*changes.letterSpacing == 0.0) {
            format.clearProperty(KmlPropLetterSpacing);
        } else {
            format.setProperty(KmlPropLetterSpacing,
                               std::clamp(*changes.letterSpacing, MIN_LETTER_SPACING,
                                          MAX_LETTER_SPACING));
        }
    }
    if (changes.color) {
        if (changes.color->isValid()) {
            format.setForeground(QBrush(*changes.color));
        } else {
            format.clearProperty(QTextFormat::ForegroundBrush);
        }
    }
    if (changes.highlight) {
        if (changes.highlight->isValid()) {
            format.setBackground(QBrush(*changes.highlight));
        } else {
            format.clearProperty(QTextFormat::BackgroundBrush);
        }
    }
}

QColor textColorOf(const QTextCharFormat& format)
{
    return format.foreground().style() != Qt::NoBrush ? format.foreground().color() : QColor();
}

QColor highlightColorOf(const QTextCharFormat& format)
{
    const QBrush background = format.background();
    return background.style() != Qt::NoBrush && background.color().alpha() > 0
               ? background.color()
               : QColor();
}

namespace {

/// @brief Relative luminance of a color (WCAG): 0 black, 1 white
qreal relativeLuminance(const QColor& color)
{
    const auto channel = [](qreal value) {
        return value <= 0.03928 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    const QColor rgb = color.toRgb();
    return 0.2126 * channel(rgb.redF()) + 0.7152 * channel(rgb.greenF()) +
           0.0722 * channel(rgb.blueF());
}

/// @brief Step of the lightness when a color is made lighter or darker
constexpr float LIGHTNESS_STEP = 0.02f;

/// @brief Below this luminance a paper is dark: halfway between black and white to the eye
constexpr qreal DARK_PAPER_LUMINANCE = 0.18;

/// @brief @p color made lighter (@p step above 0) or darker until it has the contrast of
///        text with @p other, or can go no further
QColor withContrast(const QColor& color, const QColor& other, float step)
{
    float hue = 0.0f;
    float saturation = 0.0f;
    float lightness = 0.0f;
    float alpha = 1.0f;
    color.toHsl().getHslF(&hue, &saturation, &lightness, &alpha);
    QColor shown = color;
    while (contrastRatio(shown, other) < MIN_TEXT_CONTRAST &&
           (step > 0 ? lightness < 1.0f : lightness > 0.0f)) {
        lightness = std::clamp(lightness + step, 0.0f, 1.0f);
        shown = QColor::fromHslF(hue, saturation, lightness, alpha).toRgb();
    }
    return shown;
}

}  // namespace

qreal contrastRatio(const QColor& first, const QColor& second)
{
    const qreal a = relativeLuminance(first);
    const qreal b = relativeLuminance(second);
    return (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
}

bool isDarkPaper(const QColor& paper)
{
    return relativeLuminance(paper) < DARK_PAPER_LUMINANCE;
}

QColor lightenedForDarkPaper(const QColor& color, const QColor& under)
{
    return withContrast(color, under, LIGHTNESS_STEP);
}

QColor darkenedForDarkPaper(const QColor& highlight, const QColor& text)
{
    return withContrast(highlight, text, -LIGHTNESS_STEP);
}

QString changedCase(const QString& text, qsizetype from, qsizetype to, CaseChange change)
{
    QString result = text;
    from = std::clamp<qsizetype>(from, 0, text.size());
    to = std::clamp<qsizetype>(to, from, text.size());
    switch (change) {
        case CaseChange::Lower:
            for (qsizetype i = from; i < to; ++i) {
                result[i] = text[i].toLower();
            }
            break;
        case CaseChange::Upper:
            for (qsizetype i = from; i < to; ++i) {
                result[i] = text[i].toUpper();
            }
            break;
        case CaseChange::Toggle:
            for (qsizetype i = from; i < to; ++i) {
                result[i] = text[i].isUpper() || text[i].isTitleCase() ? text[i].toLower()
                                                                        : text[i].toUpper();
            }
            break;
        case CaseChange::Capitalize: {
            // The first letter of each word, as the rules of Unicode find words (an
            // apostrophe inside one does not start another)
            QTextBoundaryFinder finder(QTextBoundaryFinder::Word, text);
            bool first = false;
            for (qsizetype i = from; i < to; ++i) {
                finder.setPosition(i);
                if (finder.isAtBoundary() &&
                    finder.boundaryReasons().testFlag(QTextBoundaryFinder::StartOfItem)) {
                    first = true;
                }
                if (text[i].isLetter()) {
                    result[i] = first ? text[i].toTitleCase() : text[i].toLower();
                    first = false;
                } else if (!text[i].isMark()) {
                    first = false;
                }
            }
            break;
        }
        case CaseChange::Sentence: {
            // The first letter of the part, and the one after the end of a sentence (a full
            // stop, an exclamation or question mark, an ellipsis, maybe with closing marks)
            // and a space, as in Word
            bool capital = true;
            bool ended = false;
            for (qsizetype i = from; i < to; ++i) {
                const QChar ch = text[i];
                if (ch.isLetter()) {
                    result[i] = capital ? ch.toTitleCase() : ch.toLower();
                    capital = false;
                    ended = false;
                } else if (endsSentence(ch)) {
                    ended = true;
                } else if (ch.isSpace()) {
                    capital = capital || ended;
                } else if (ch.isDigit()) {
                    capital = false;
                    ended = false;
                } else if (!closesSentence(ch)) {
                    ended = false;  // a dash or an opening mark keeps a capital to come
                }
            }
            break;
        }
    }
    return result;
}

}  // namespace kalahari::editor
