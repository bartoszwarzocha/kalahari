/// @file text_diff.cpp
/// @brief Differences between two texts, paragraph by paragraph and word by word

#include <kalahari/core/text_diff.h>
#include <kalahari/core/text_statistics.h>

#include <QHash>

#include <algorithm>
#include <utility>
#include <vector>

namespace kalahari {
namespace core {

namespace {

/// Largest table of the longest common subsequence (cells): 16 MB of memory, a fraction of a
/// second; a bigger part is cut at the items that it has once (addCommonPairs())
constexpr qint64 MAX_CELLS = 4'000'000;

/// Largest table of the pairing of the changed paragraphs between two same ones
constexpr qint64 MAX_PAIRING_CELLS = 250'000;

/// Paragraphs that share less of their words (a third) are deleted and added whole
constexpr double MIN_SIMILARITY = 0.3;

using Pairs = std::vector<std::pair<qsizetype, qsizetype>>;

/// The longest common subsequence of a[aStart..aEnd) and b[bStart..bEnd), added to @p pairs
/// as pairs of the places of its items, in order. Of the equal ways, the one that deletes
/// before it adds, so that a word replaced by another reads as one change. A part too big for
/// the table is cut at the items that each part has once (patience diff); a part without such
/// items has no pairs.
template <typename T>
void addCommonPairs(const std::vector<T>& a, qsizetype aStart, qsizetype aEnd,
                    const std::vector<T>& b, qsizetype bStart, qsizetype bEnd, Pairs& pairs) {
    // The same start and the same end need no table
    while (aStart < aEnd && bStart < bEnd && a[aStart] == b[bStart]) {
        pairs.emplace_back(aStart++, bStart++);
    }
    qsizetype same = 0;
    while (aEnd - same > aStart && bEnd - same > bStart &&
           a[aEnd - 1 - same] == b[bEnd - 1 - same]) {
        ++same;
    }
    aEnd -= same;
    bEnd -= same;
    const qsizetype rows = aEnd - aStart;
    const qsizetype columns = bEnd - bStart;

    if (rows > 0 && columns > 0 && (rows + 1) * (columns + 1) <= MAX_CELLS) {
        // length[i][j]: the longest common subsequence of a[aStart + i..] and b[bStart + j..]
        const qsizetype width = columns + 1;
        std::vector<int> length(static_cast<size_t>((rows + 1) * width), 0);
        for (qsizetype i = rows - 1; i >= 0; --i) {
            for (qsizetype j = columns - 1; j >= 0; --j) {
                const size_t cell = static_cast<size_t>(i * width + j);
                length[cell] = a[aStart + i] == b[bStart + j]
                                   ? length[cell + width + 1] + 1
                                   : std::max(length[cell + width], length[cell + 1]);
            }
        }
        qsizetype i = 0;
        qsizetype j = 0;
        while (i < rows && j < columns) {
            const size_t cell = static_cast<size_t>(i * width + j);
            if (a[aStart + i] == b[bStart + j]) {
                pairs.emplace_back(aStart + i, bStart + j);
                ++i;
                ++j;
            } else if (length[cell + width] >= length[cell + 1]) {
                ++i;  // deleted first
            } else {
                ++j;
            }
        }
    } else if (rows > 0 && columns > 0) {
        // The items that each part has once, in the order of both parts
        QHash<T, std::pair<int, qsizetype>> inA;  // how many times, and where
        for (qsizetype i = aStart; i < aEnd; ++i) {
            auto& [count, place] = inA[a[i]];
            ++count;
            place = i;
        }
        QHash<T, std::pair<int, qsizetype>> inB;
        for (qsizetype j = bStart; j < bEnd; ++j) {
            auto& [count, place] = inB[b[j]];
            ++count;
            place = j;
        }
        Pairs unique;  // in the order of a
        for (qsizetype i = aStart; i < aEnd; ++i) {
            const auto [countA, placeA] = inA.value(a[i]);
            const auto [countB, placeB] = inB.value(a[i], {0, -1});
            if (countA == 1 && countB == 1) {
                unique.emplace_back(i, placeB);
            }
        }
        // The longest run of them in the order of b too (patience sorting)
        std::vector<qsizetype> tails;  // the pile tops: indexes of unique
        std::vector<qsizetype> previous(unique.size(), -1);
        for (size_t k = 0; k < unique.size(); ++k) {
            const auto pile = std::lower_bound(
                tails.begin(), tails.end(), unique[k].second,
                [&unique](qsizetype top, qsizetype place) {
                    return unique[static_cast<size_t>(top)].second < place;
                });
            if (pile != tails.begin()) {
                previous[k] = *(pile - 1);
            }
            if (pile == tails.end()) {
                tails.push_back(static_cast<qsizetype>(k));
            } else {
                *pile = static_cast<qsizetype>(k);
            }
        }
        Pairs anchors;
        for (qsizetype k = tails.empty() ? -1 : tails.back(); k >= 0;
             k = previous[static_cast<size_t>(k)]) {
            anchors.push_back(unique[static_cast<size_t>(k)]);
        }
        std::reverse(anchors.begin(), anchors.end());
        // The parts between them
        qsizetype i = aStart;
        qsizetype j = bStart;
        for (const auto& [anchorA, anchorB] : anchors) {
            addCommonPairs(a, i, anchorA, b, j, anchorB, pairs);
            pairs.emplace_back(anchorA, anchorB);
            i = anchorA + 1;
            j = anchorB + 1;
        }
        if (!anchors.empty()) {
            addCommonPairs(a, i, aEnd, b, j, bEnd, pairs);
        }
    }

    for (qsizetype k = 0; k < same; ++k) {
        pairs.emplace_back(aEnd + k, bEnd + k);
    }
}

/// The same items of @p a and @p b, as pairs of their places, in order (addCommonPairs())
template <typename T>
Pairs commonPairs(const std::vector<T>& a, const std::vector<T>& b) {
    Pairs pairs;
    addCommonPairs(a, 0, static_cast<qsizetype>(a.size()), b, 0,
                   static_cast<qsizetype>(b.size()), pairs);
    return pairs;
}

/// Whether @p c is a character of a word: a letter, a digit or a mark on a letter
bool isWordCharacter(QChar c) {
    return c.isLetterOrNumber() || c.isMark() || c == QLatin1Char('_');
}

/// A piece of a paragraph: a word or another character, with the whitespace after it
struct Token {
    QString text;   ///< The word or the character; whitespace at the start of a paragraph
    QString space;  ///< The whitespace after it

    QString whole() const { return text + space; }
};

/// Pieces of a paragraph: words and each other character, with the whitespace after them, so
/// that the words are compared and not the spaces between them
QList<Token> tokensOf(const QString& text) {
    QList<Token> tokens;
    const qsizetype size = text.size();
    qsizetype i = 0;
    const auto spaceFrom = [&text, size](qsizetype from) {
        qsizetype end = from;
        while (end < size && text.at(end).isSpace()) {
            ++end;
        }
        return end;
    };
    if (size > 0 && text.at(0).isSpace()) {  // whitespace at the start
        i = spaceFrom(0);
        tokens.append({text.left(i), QString()});
    }
    while (i < size) {
        const QChar c = text.at(i);
        qsizetype next = i + 1;
        if (isWordCharacter(c)) {
            while (next < size && isWordCharacter(text.at(next))) {
                ++next;
            }
        } else if (c.isHighSurrogate() && next < size && text.at(next).isLowSurrogate()) {
            ++next;  // one character outside the basic plane
        }
        const qsizetype spaceEnd = spaceFrom(next);
        tokens.append({text.mid(i, next - i), text.mid(next, spaceEnd - next)});
        i = spaceEnd;
    }
    return tokens;
}

/// The words and characters of @p tokens, without the whitespace after them
QStringList textsOf(const QList<Token>& tokens) {
    QStringList texts;
    for (const Token& token : tokens) {
        texts << token.text;
    }
    return texts;
}

/// Numbers in place of texts, the same number for the same text, so that comparing them is fast
class Numbers {
public:
    std::vector<int> of(const QStringList& texts) {
        std::vector<int> numbers;
        numbers.reserve(static_cast<size_t>(texts.size()));
        for (const QString& text : texts) {
            auto found = m_numbers.constFind(text);
            if (found == m_numbers.cend()) {
                found = m_numbers.insert(text, static_cast<int>(m_numbers.size()));
            }
            numbers.push_back(found.value());
        }
        return numbers;
    }

private:
    QHash<QString, int> m_numbers;
};

/// Add @p text as a piece of @p kind to @p pieces, joined to the last piece of its kind
void append(QList<DiffPiece>& pieces, DiffPiece::Kind kind, const QString& text) {
    if (text.isEmpty()) {
        return;
    }
    if (!pieces.isEmpty() && pieces.last().kind == kind) {
        pieces.last().text += text;
    } else {
        pieces.append({kind, text});
    }
}

/// Whether @p text has only whitespace
bool isBlank(const QString& text) {
    return std::all_of(text.cbegin(), text.cend(), [](QChar c) { return c.isSpace(); });
}

/// Join changes parted only by spaces into one: "deleted, space, deleted, added" reads as
/// one deleted and one added piece. Both texts keep their spaces: the space goes to the
/// deleted and to the added piece. Not when one of them would be only the space.
QList<DiffPiece> joinChanges(const QList<DiffPiece>& pieces) {
    QList<DiffPiece> joined;
    qsizetype i = 0;
    while (i < pieces.size()) {
        if (pieces.at(i).kind == DiffPiece::Kind::Same) {
            joined << pieces.at(i);
            ++i;
            continue;
        }
        // The run of changes and spaces, ending with a change
        qsizetype end = i;
        qsizetype last = i;
        while (end < pieces.size() &&
               (pieces.at(end).kind != DiffPiece::Kind::Same || isBlank(pieces.at(end).text))) {
            if (pieces.at(end).kind != DiffPiece::Kind::Same) {
                last = end;
            }
            ++end;
        }
        QString deleted;
        QString added;
        bool spaces = false;
        for (qsizetype k = i; k <= last; ++k) {
            const DiffPiece& piece = pieces.at(k);
            if (piece.kind != DiffPiece::Kind::Added) {
                deleted += piece.text;
            }
            if (piece.kind != DiffPiece::Kind::Deleted) {
                added += piece.text;
            }
            spaces = spaces || piece.kind == DiffPiece::Kind::Same;
        }
        if (spaces && ((!deleted.isEmpty() && isBlank(deleted)) ||
                       (!added.isEmpty() && isBlank(added)))) {
            for (qsizetype k = i; k <= last; ++k) {
                joined << pieces.at(k);
            }
        } else {
            append(joined, DiffPiece::Kind::Deleted, deleted);
            append(joined, DiffPiece::Kind::Added, added);
        }
        i = last + 1;
    }

    // The whitespace that ends both a deleted and the added piece after it is the same in both
    // texts: "[-ciągu kilku]{+kilka} minut" rather than "[-ciągu kilku ]{+kilka }minut"
    for (qsizetype k = 0; k + 1 < joined.size(); ++k) {
        DiffPiece& deleted = joined[k];
        DiffPiece& added = joined[k + 1];
        if (deleted.kind != DiffPiece::Kind::Deleted || added.kind != DiffPiece::Kind::Added) {
            continue;
        }
        qsizetype space = 0;
        while (space < deleted.text.size() - 1 && space < added.text.size() - 1 &&
               deleted.text.at(deleted.text.size() - 1 - space).isSpace() &&
               deleted.text.at(deleted.text.size() - 1 - space) ==
                   added.text.at(added.text.size() - 1 - space)) {
            ++space;
        }
        if (space == 0) {
            continue;
        }
        const QString common = added.text.right(space);
        deleted.text.chop(space);
        added.text.chop(space);
        if (k + 2 < joined.size() && joined.at(k + 2).kind == DiffPiece::Kind::Same) {
            joined[k + 2].text.prepend(common);
        } else {
            joined.insert(k + 2, {DiffPiece::Kind::Same, common});
        }
    }
    return joined;
}

/// Pieces of paragraph @p older changed to @p newer, word by word
QList<DiffPiece> diffParagraph(const QString& older, const QString& newer) {
    const QList<Token> olderTokens = tokensOf(older);
    const QList<Token> newerTokens = tokensOf(newer);
    Numbers numbers;
    const Pairs pairs =
        commonPairs(numbers.of(textsOf(olderTokens)), numbers.of(textsOf(newerTokens)));

    QList<DiffPiece> pieces;
    qsizetype i = 0;
    qsizetype j = 0;
    const auto changesUpTo = [&](qsizetype olderEnd, qsizetype newerEnd) {
        for (; i < olderEnd; ++i) {
            append(pieces, DiffPiece::Kind::Deleted, olderTokens.at(i).whole());
        }
        for (; j < newerEnd; ++j) {
            append(pieces, DiffPiece::Kind::Added, newerTokens.at(j).whole());
        }
    };
    for (const auto& [olderPlace, newerPlace] : pairs) {
        changesUpTo(olderPlace, newerPlace);
        const Token& olderToken = olderTokens.at(olderPlace);
        const Token& newerToken = newerTokens.at(newerPlace);
        append(pieces, DiffPiece::Kind::Same, olderToken.text);
        if (olderToken.space == newerToken.space) {
            append(pieces, DiffPiece::Kind::Same, olderToken.space);
        } else {
            append(pieces, DiffPiece::Kind::Deleted, olderToken.space);
            append(pieces, DiffPiece::Kind::Added, newerToken.space);
        }
        ++i;
        ++j;
    }
    changesUpTo(olderTokens.size(), newerTokens.size());
    return joinChanges(pieces);
}

/// Words of a paragraph, small letters, with how many times it has each
QHash<QString, int> wordsOf(const QString& text) {
    QHash<QString, int> words;
    for (const Token& token : tokensOf(text)) {
        if (isWordCharacter(token.text.front())) {
            ++words[token.text.toLower()];
        }
    }
    return words;
}

/// How much paragraphs with words @p a and @p b share: 0 (nothing) to 1 (all)
double similarity(const QHash<QString, int>& a, const QHash<QString, int>& b) {
    int total = 0;
    int shared = 0;
    for (auto it = a.cbegin(); it != a.cend(); ++it) {
        total += it.value();
        shared += std::min(it.value(), b.value(it.key()));
    }
    for (auto it = b.cbegin(); it != b.cend(); ++it) {
        total += it.value();
    }
    return total == 0 ? 0.0 : 2.0 * shared / total;
}

/// The changed paragraphs @p older and @p newer between two same ones, as pairs of their
/// places in these lists: those that share most of their words, in order
Pairs pairChanged(const QStringList& older, const QStringList& newer) {
    Pairs pairs;
    const qsizetype rows = older.size();
    const qsizetype columns = newer.size();
    if (rows == 0 || columns == 0 || (rows + 1) * (columns + 1) > MAX_PAIRING_CELLS) {
        return pairs;
    }
    std::vector<QHash<QString, int>> olderWords;
    std::vector<QHash<QString, int>> newerWords;
    for (const QString& paragraph : older) {
        olderWords.push_back(wordsOf(paragraph));
    }
    for (const QString& paragraph : newer) {
        newerWords.push_back(wordsOf(paragraph));
    }

    // best[i][j]: the most similarity of the pairs of older[i..] and newer[j..]
    const qsizetype width = columns + 1;
    std::vector<double> best(static_cast<size_t>((rows + 1) * width), 0.0);
    std::vector<double> similar(static_cast<size_t>(rows * columns), 0.0);
    for (qsizetype i = rows - 1; i >= 0; --i) {
        for (qsizetype j = columns - 1; j >= 0; --j) {
            const size_t cell = static_cast<size_t>(i * width + j);
            const double s = similarity(olderWords[static_cast<size_t>(i)],
                                        newerWords[static_cast<size_t>(j)]);
            similar[static_cast<size_t>(i * columns + j)] = s;
            const double paired = s >= MIN_SIMILARITY ? s + best[cell + width + 1] : -1.0;
            best[cell] = std::max({paired, best[cell + width], best[cell + 1]});
        }
    }
    qsizetype i = 0;
    qsizetype j = 0;
    while (i < rows && j < columns) {
        const size_t cell = static_cast<size_t>(i * width + j);
        const double s = similar[static_cast<size_t>(i * columns + j)];
        if (s >= MIN_SIMILARITY && best[cell] == s + best[cell + width + 1]) {
            pairs.emplace_back(i, j);
            ++i;
            ++j;
        } else if (best[cell] == best[cell + width]) {
            ++i;
        } else {
            ++j;
        }
    }
    return pairs;
}

}  // namespace

bool DiffParagraph::isSame() const {
    return std::all_of(pieces.cbegin(), pieces.cend(), [](const DiffPiece& piece) {
        return piece.kind == DiffPiece::Kind::Same;
    });
}

TextDiff diffTexts(const QStringList& older, const QStringList& newer) {
    TextDiff diff;
    Numbers numbers;
    const Pairs same = commonPairs(numbers.of(older), numbers.of(newer));

    const auto sameParagraph = [&diff](const QString& text, qsizetype olderPlace,
                                       qsizetype newerPlace) {
        DiffParagraph paragraph;
        append(paragraph.pieces, DiffPiece::Kind::Same, text);
        paragraph.older = static_cast<int>(olderPlace);
        paragraph.newer = static_cast<int>(newerPlace);
        diff.paragraphs << paragraph;
    };
    const auto wholeParagraph = [&diff](DiffPiece::Kind kind, const QString& text,
                                        qsizetype place) {
        DiffParagraph paragraph;
        append(paragraph.pieces, kind, text);
        (kind == DiffPiece::Kind::Deleted ? paragraph.older : paragraph.newer) =
            static_cast<int>(place);
        diff.paragraphs << paragraph;
    };
    // The paragraphs between two same ones: paired ones word by word, the others whole,
    // deleted before added
    const auto changed = [&](qsizetype olderStart, qsizetype olderEnd, qsizetype newerStart,
                             qsizetype newerEnd) {
        const QStringList olderPart = older.mid(olderStart, olderEnd - olderStart);
        const QStringList newerPart = newer.mid(newerStart, newerEnd - newerStart);
        qsizetype i = 0;
        qsizetype j = 0;
        const auto wholeUpTo = [&](qsizetype olderUpTo, qsizetype newerUpTo) {
            for (; i < olderUpTo; ++i) {
                wholeParagraph(DiffPiece::Kind::Deleted, olderPart.at(i), olderStart + i);
            }
            for (; j < newerUpTo; ++j) {
                wholeParagraph(DiffPiece::Kind::Added, newerPart.at(j), newerStart + j);
            }
        };
        for (const auto& [olderPlace, newerPlace] : pairChanged(olderPart, newerPart)) {
            wholeUpTo(olderPlace, newerPlace);
            DiffParagraph paragraph;
            paragraph.pieces = diffParagraph(olderPart.at(i), newerPart.at(j));
            paragraph.older = static_cast<int>(olderStart + i);
            paragraph.newer = static_cast<int>(newerStart + j);
            diff.paragraphs << paragraph;
            ++i;
            ++j;
        }
        wholeUpTo(olderPart.size(), newerPart.size());
    };

    qsizetype olderNext = 0;
    qsizetype newerNext = 0;
    for (const auto& [olderPlace, newerPlace] : same) {
        changed(olderNext, olderPlace, newerNext, newerPlace);
        sameParagraph(older.at(olderPlace), olderPlace, newerPlace);
        olderNext = olderPlace + 1;
        newerNext = newerPlace + 1;
    }
    changed(olderNext, older.size(), newerNext, newer.size());

    // The words and the changes, in reading order; a change goes on in the next paragraph
    // when both are deleted or added whole
    bool inChange = false;
    bool lastWhole = false;
    for (qsizetype p = 0; p < diff.paragraphs.size(); ++p) {
        const DiffParagraph& paragraph = diff.paragraphs.at(p);
        const bool whole = paragraph.older < 0 || paragraph.newer < 0;
        if (!(whole && lastWhole)) {
            inChange = false;
        }
        lastWhole = whole;
        if (paragraph.pieces.isEmpty()) {  // an empty paragraph
            if (whole && !inChange) {
                diff.changes.append({static_cast<int>(p), 0});
            }
            inChange = whole;
            continue;
        }
        for (qsizetype k = 0; k < paragraph.pieces.size(); ++k) {
            const DiffPiece& piece = paragraph.pieces.at(k);
            if (piece.kind == DiffPiece::Kind::Same) {
                inChange = false;
                continue;
            }
            (piece.kind == DiffPiece::Kind::Deleted ? diff.deletedWords : diff.addedWords) +=
                tallyWords(piece.text).words;
            if (!inChange) {
                diff.changes.append({static_cast<int>(p), static_cast<int>(k)});
                inChange = true;
            }
        }
    }
    return diff;
}

}  // namespace core
}  // namespace kalahari
