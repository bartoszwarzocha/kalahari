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

}  // namespace

TextCounts countText(QStringView text, const WordCountRules& rules) {
    TextCounts counts;
    bool inRun = false;        // inside a run of non-space characters
    bool runIsWord = false;    // the run has a letter or a digit
    bool runIsDashes = false;  // the run is only dashes

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
            }
            continue;
        }

        ++counts.characters;
        ++counts.nonSpaceCharacters;
        const bool dash = QChar::category(ch) == QChar::Punctuation_Dash;
        runIsDashes = dash && (runIsDashes || !inRun);
        runIsWord = (runIsWord && inRun) || QChar::isLetterOrNumber(ch);
        inRun = true;
    }
    if (inRun && (runIsWord || (runIsDashes && rules.dashesAreWords))) {
        ++counts.words;
    }
    return counts;
}

WordCountRules wordCountRules() {
    WordCountRules rules;
    rules.dashesAreWords =
        SettingsManager::getInstance().get<bool>("editor.wordCount.dashesAsWords");
    return rules;
}

}  // namespace kalahari::core
