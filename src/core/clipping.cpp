// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file clipping.cpp
/// @brief Clippings: their titles and the places they were cut from

#include "kalahari/core/clipping.h"

#include <QRegularExpression>

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <optional>
#include <vector>

namespace kalahari::core {

namespace {

/// @brief The end of a title cut short (…)
constexpr char16_t ELLIPSIS = u'\u2026';

/// @brief Whether @p character is between words: a space, a tab, the end of a line
bool isSpace(QChar character) {
    return character.isSpace() || character == QChar::LineSeparator ||
           character == QChar::ParagraphSeparator;
}

/// @brief A place found and how far its paragraph is from the paragraph of the origin
struct Candidate {
    ClippingPlace place;
    int distance = std::numeric_limits<int>::max();
};

/// @brief Keep @p place in @p best when its paragraph is nearer to @p wanted
void consider(Candidate& best, int paragraph, int offset, int wanted, bool exact) {
    const int distance = std::abs(paragraph - wanted);
    if (distance < best.distance) {
        best = Candidate{
            .place = ClippingPlace{.paragraph = paragraph, .offset = offset, .exact = exact},
            .distance = distance};
    }
}

/// @brief Whether @p candidate holds a place found
bool isFound(const Candidate& candidate) {
    return candidate.distance != std::numeric_limits<int>::max();
}

/// @brief Whether a place at @p position of @p text has nothing of its paragraph before it
/// (the start, or an object of the text right before it)
bool startsThere(const QString& text, qsizetype position) {
    return position == 0 || text.at(position - 1) == QChar::ObjectReplacementCharacter;
}

/// @brief Whether a place at @p position of @p text has nothing of its paragraph after it (the
/// end, or an object of the text right after it)
bool endsThere(const QString& text, qsizetype position) {
    return position == text.size() || text.at(position) == QChar::ObjectReplacementCharacter;
}

/// @brief The places of @p text with @p before right before them and @p after right after
/// them (an empty one: nothing of the paragraph there)
std::vector<qsizetype> placesWith(const QString& text, const QString& before,
                                  const QString& after) {
    std::vector<qsizetype> places;
    const auto fits = [&text, &after](qsizetype position) {
        return after.isEmpty() ? endsThere(text, position)
                               : text.mid(position).startsWith(after);
    };
    if (before.isEmpty()) {
        for (qsizetype position = 0; position <= text.size(); ++position) {
            if (startsThere(text, position) && fits(position)) {
                places.push_back(position);
            }
        }
        return places;
    }
    for (qsizetype found = text.indexOf(before); found >= 0;
         found = text.indexOf(before, found + 1)) {
        if (const qsizetype position = found + before.size(); fits(position)) {
            places.push_back(position);
        }
    }
    return places;
}

/// @brief The place nearest to paragraph @p wanted with the words of @p origin on both sides
Candidate exactPlace(const QStringList& paragraphs, const ClippingOrigin& origin, int wanted) {
    Candidate best;
    for (int i = 0; i < static_cast<int>(paragraphs.size()); ++i) {
        const std::vector<qsizetype> places =
            placesWith(paragraphs.at(i), origin.before, origin.after);
        if (!places.empty()) {
            consider(best, i, static_cast<int>(places.front()), wanted, true);
        }
    }
    return best;
}

/// @brief Where @p words are in @p text; -1 for none, or when there are no words
qsizetype wordsIn(const QString& text, const QString& words) {
    return words.isEmpty() ? -1 : text.indexOf(words);
}

/// @brief The place nearest to paragraph @p wanted with the words of @p origin on one side:
///        after those before it, or before those after it
Candidate nearPlace(const QStringList& paragraphs, const ClippingOrigin& origin, int wanted) {
    Candidate best;
    for (int i = 0; i < static_cast<int>(paragraphs.size()); ++i) {
        const QString& text = paragraphs.at(i);
        if (const qsizetype found = wordsIn(text, origin.before); found >= 0) {
            consider(best, i, static_cast<int>(found + origin.before.size()), wanted, false);
        }
        if (const qsizetype found = wordsIn(text, origin.after); found >= 0) {
            consider(best, i, static_cast<int>(found), wanted, false);
        }
    }
    return best;
}

}  // namespace

QString Clippings::titleOf(const QString& text) {
    QString line = text;
    line.remove(QChar::ObjectReplacementCharacter);
    line.replace(QChar::LineSeparator, QLatin1Char(' '));
    line.replace(QChar::ParagraphSeparator, QLatin1Char(' '));
    const QStringList words = line.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (words.size() <= TITLE_WORDS) {
        return words.join(QLatin1Char(' '));
    }
    // The words cut short: without the punctuation after the last of them
    static const QRegularExpression trailing(QStringLiteral("[\\s.,;:!?\\x{2013}\\x{2014}-]+$"));
    QString title = words.mid(0, TITLE_WORDS).join(QLatin1Char(' '));
    title.remove(trailing);
    return title + QChar(ELLIPSIS);
}

QString Clippings::wordsBefore(const QString& text) {
    const qsizetype object = text.lastIndexOf(QChar::ObjectReplacementCharacter);
    const QString part = object >= 0 ? text.mid(object + 1) : text;
    // From the start of the first of the words, with the spaces after the last of them
    qsizetype position = part.size();
    qsizetype start = part.size();
    for (int words = 0; words < PLACE_WORDS; ++words) {
        while (position > 0 && isSpace(part.at(position - 1))) {
            --position;
        }
        if (position == 0) {
            break;
        }
        while (position > 0 && !isSpace(part.at(position - 1))) {
            --position;
        }
        start = position;
    }
    return part.mid(start);
}

QString Clippings::wordsAfter(const QString& text) {
    const qsizetype object = text.indexOf(QChar::ObjectReplacementCharacter);
    const QString part = object >= 0 ? text.left(object) : text;
    qsizetype position = 0;
    for (int words = 0; position < part.size() && words < PLACE_WORDS; ++words) {
        while (position < part.size() && isSpace(part.at(position))) {
            ++position;
        }
        while (position < part.size() && !isSpace(part.at(position))) {
            ++position;
        }
    }
    return part.left(position);
}

ClippingOrigin Clippings::originAt(const QString& element, const QStringList& paragraphs,
                                   int paragraph, int offset, const QDateTime& when) {
    ClippingOrigin origin;
    origin.element = element;
    origin.cut = when;
    if (paragraph < 0 || paragraph >= paragraphs.size()) {
        return origin;
    }
    const QString& text = paragraphs.at(paragraph);
    const auto position =
        static_cast<qsizetype>(std::clamp(offset, 0, static_cast<int>(text.size())));
    origin.paragraph = paragraph + 1;
    origin.before = wordsBefore(text.left(position));
    origin.after = wordsAfter(text.mid(position));
    return origin;
}

std::optional<ClippingPlace> Clippings::placeIn(const QStringList& paragraphs,
                                                const ClippingOrigin& origin) {
    if (paragraphs.isEmpty()) {
        return std::nullopt;
    }
    const int wanted = std::clamp(origin.paragraph - 1, 0, static_cast<int>(paragraphs.size()) - 1);
    // The words on both sides as they were
    if (const Candidate bothSides = exactPlace(paragraphs, origin, wanted); isFound(bothSides)) {
        return bothSides.place;
    }
    // The words on one side: after those before it, else before those after it
    if (const Candidate oneSide = nearPlace(paragraphs, origin, wanted); isFound(oneSide)) {
        return oneSide.place;
    }
    // Only the paragraph of its number is left
    return ClippingPlace{.paragraph = wanted, .offset = 0, .exact = false};
}

}  // namespace kalahari::core
