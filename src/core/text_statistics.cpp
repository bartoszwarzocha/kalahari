/// @file text_statistics.cpp
/// @brief Word and character counting shared by the whole application

#include <kalahari/core/text_statistics.h>
#include <kalahari/core/settings_manager.h>

#include <QChar>

namespace kalahari::core {

namespace {

/// @brief Whether @p ch breaks a line or a paragraph (not a character of the text)
bool isLineBreak(char32_t ch) {
    switch (ch) {
    case U'\n':
    case U'\v':
    case U'\f':
    case U'\r':
    case 0x0085:  // next line
        return true;
    default: {
        const QChar::Category category = QChar::category(ch);
        return category == QChar::Separator_Line || category == QChar::Separator_Paragraph;
    }
    }
}

/// @brief Whether @p ch ends a paragraph: a new line, a page break or the paragraph
///        separator, but not the line separator or the vertical tab, which only break a line
bool isParagraphBreak(char32_t ch) {
    switch (ch) {
    case U'\n':
    case U'\f':
    case U'\r':  // a CR LF ends one paragraph: the LF ends an empty one, which does not count
    case 0x0085:  // next line
    case 0x2029:  // paragraph separator
        return true;
    default:
        return false;
    }
}

}  // namespace

TextCounts countText(QStringView text, const WordCountRules& rules) {
    TextCounts counts;
    bool inRun = false;        // inside a run of non-space characters
    bool runIsWord = false;    // the run has a letter or a digit
    bool runIsDashes = false;  // the run is only dashes
    bool inText = false;       // the paragraph has a character other than whitespace

    for (qsizetype i = 0; i < text.size(); ++i) {
        char32_t ch = text[i].unicode();
        if (QChar::isHighSurrogate(ch) && i + 1 < text.size() && text[i + 1].isLowSurrogate()) {
            ch = QChar::surrogateToUcs4(text[i], text[i + 1]);
            ++i;
        }

        if (QChar::isSpace(ch)) {
            if (inRun && (runIsWord || (runIsDashes && rules.dashesAreWords))) {
                ++counts.words;
            }
            inRun = false;
            if (!isLineBreak(ch)) {
                ++counts.characters;
            } else if (isParagraphBreak(ch)) {
                if (inText) {
                    ++counts.paragraphs;
                }
                inText = false;
            }
            continue;
        }

        ++counts.characters;
        ++counts.nonSpaceCharacters;
        inText = true;
        const bool dash = QChar::category(ch) == QChar::Punctuation_Dash;
        runIsDashes = dash && (runIsDashes || !inRun);
        runIsWord = (runIsWord && inRun) || QChar::isLetterOrNumber(ch);
        inRun = true;
    }
    if (inRun && (runIsWord || (runIsDashes && rules.dashesAreWords))) {
        ++counts.words;
    }
    if (inText) {
        ++counts.paragraphs;
    }
    return counts;
}

int readingMinutes(int words) {
    return words > 0 ? (words + READING_WORDS_PER_MINUTE - 1) / READING_WORDS_PER_MINUTE : 0;
}

WordCountRules wordCountRules() {
    WordCountRules rules;
    rules.dashesAreWords =
        SettingsManager::getInstance().get<bool>("editor.wordCount.dashesAsWords");
    return rules;
}

}  // namespace kalahari::core
