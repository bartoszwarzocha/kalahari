/// @file text_statistics.cpp
/// @brief Word and character counting shared by the whole application

#include <kalahari/core/text_statistics.h>

#include <QChar>

namespace kalahari::core {

TextCounts countText(QStringView text) {
    TextCounts counts;
    bool runIsWord = false;  // the current run of non-space characters has a letter/digit

    for (qsizetype i = 0; i < text.size(); ++i) {
        char32_t ch = text[i].unicode();
        if (QChar::isHighSurrogate(ch) && i + 1 < text.size() && text[i + 1].isLowSurrogate()) {
            ch = QChar::surrogateToUcs4(text[i], text[i + 1]);
            ++i;
        }

        if (QChar::isSpace(ch)) {
            if (runIsWord) {
                ++counts.words;
            }
            runIsWord = false;
        } else {
            ++counts.nonSpaceCharacters;
            runIsWord = runIsWord || QChar::isLetterOrNumber(ch);
        }
    }
    if (runIsWord) {
        ++counts.words;
    }
    return counts;
}

}  // namespace kalahari::core
