// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file quick_insert.cpp
/// @brief Quick insert: the items the typed letters find

#include <kalahari/editor/quick_insert.h>

#include <QStringList>

#include <algorithm>
#include <functional>
#include <utility>

namespace kalahari::editor {

namespace {

/// @brief The characters after which a trigger starts a word, beside a space: dashes, a
///        slash, the opening quotation marks and brackets (also » of Polish »quotes«)
const QString& wordOpeners() {
    static const QString openers = QStringLiteral("([{<\"'„“‘‚«»‹›—–-/");
    return openers;
}

/// @brief The pairs isEnclosingPair() knows: quotation marks of Polish, English, German and
///        French, and the brackets
const QStringList& enclosingPairs() {
    static const QStringList pairs = {
        QStringLiteral("„”"), QStringLiteral("“”"), QStringLiteral("‚’"), QStringLiteral("‘’"),
        QStringLiteral("„“"), QStringLiteral("‚‘"), QStringLiteral("«»"), QStringLiteral("»«"),
        QStringLiteral("‹›"), QStringLiteral("›‹"), QStringLiteral("()"), QStringLiteral("[]"),
        QStringLiteral("{}"), QStringLiteral("\"\""), QStringLiteral("''")};
    return pairs;
}

/// @brief The words of a folded name: its parts between the characters that are not letters
///        or digits ("anna wolska" -> anna, wolska; "jean-luc" -> jean, luc)
QStringList wordsOf(const QString& folded) {
    QStringList words;
    QString word;
    for (const QChar c : folded) {
        if (c.isLetterOrNumber()) {
            word.append(c);
        } else if (!word.isEmpty()) {
            words.append(word);
            word.clear();
        }
    }
    if (!word.isEmpty()) {
        words.append(word);
    }
    return words;
}

/// @brief How well the letters typed match a text, the best first
enum class Fit {
    Whole,  ///< The text is the letters
    Start,  ///< It starts with them
    Word,   ///< A later word of it starts with them
    None    ///< They do not match it
};

/// @brief How well @p letters match @p text (both in the same case and folding)
Fit fitOf(const QString& text, const QString& letters) {
    if (text == letters) {
        return Fit::Whole;
    }
    if (text.startsWith(letters)) {
        return Fit::Start;
    }
    const QStringList words = wordsOf(text);
    return std::ranges::any_of(words,
                               [&letters](const QString& word) { return word.startsWith(letters); })
               ? Fit::Word
               : Fit::None;
}

/// @brief An item found, and how well
struct Found {
    Fit fit = Fit::None;
    QuickInsertItem item;
};

/// @brief How a text is compared with the letters: as typed (in lower case) or folded
using Compared = std::function<QString(const QString&)>;

/// @brief Add the forms of @p item that @p letters match (compared as @p compare makes them)
/// @return Whether one was added
bool addForms(const QuickInsertItem& item, const QString& letters, const Compared& compare,
              QList<Found>& found) {
    bool added = false;
    for (const QuickInsertForm& form : item.forms) {
        const Fit fit = fitOf(compare(form.text), letters);
        if (fit != Fit::None) {
            found.append({.fit = fit, .item = formItem(item, form)});
            added = true;
        }
    }
    return added;
}

/// @brief Add @p item, or the forms of it, that the letters typed match: its name as typed,
///        its forms as typed, its name folded, its forms folded, the first that matches
void addMatches(const QuickInsertItem& item, const QString& lower, const QString& folded,
                QList<Found>& found) {
    const Compared asTyped = [](const QString& text) { return text.toLower(); };
    const Compared withoutMarks = [](const QString& text) { return foldForSearch(text); };
    if (const Fit fit = fitOf(asTyped(item.name), lower); fit != Fit::None) {
        found.append({.fit = fit, .item = item});
        return;
    }
    if (addForms(item, lower, asTyped, found)) {
        return;
    }
    if (const Fit fit = fitOf(withoutMarks(item.name), folded); fit != Fit::None) {
        found.append({.fit = fit, .item = item});
        return;
    }
    addForms(item, folded, withoutMarks, found);
}

}  // namespace

QString foldForSearch(const QString& text) {
    // The letters apart from their marks (NFD), without the marks; Ł and ł have no mark of
    // their own in Unicode, so they are given their base letter here
    const QString decomposed = text.normalized(QString::NormalizationForm_D);
    QString folded;
    folded.reserve(decomposed.size());
    for (const QChar c : decomposed) {
        if (c.category() == QChar::Mark_NonSpacing) {
            continue;
        }
        if (c == QChar(0x0141) || c == QChar(0x0142)) {  // Ł ł
            folded.append(QLatin1Char('l'));
            continue;
        }
        folded.append(c.toLower());
    }
    return folded;
}

bool startsQuickInsertWord(const QString& before) {
    if (before.isEmpty()) {
        return true;
    }
    const QChar last = before.back();
    return last.isSpace() || last == QChar::LineSeparator || wordOpeners().contains(last);
}

bool continuesQuickInsertQuery(QChar character) {
    return character.isLetterOrNumber() || character == QLatin1Char('-') ||
           character == QLatin1Char('_') || character == QLatin1Char('\'') ||
           character == QChar(0x2019);  // ’
}

QList<QuickInsertItem> matchQuickInsert(const QList<QuickInsertItem>& items, const QString& query) {
    if (query.isEmpty()) {
        return items;
    }
    const QString lower = query.toLower();
    const QString folded = foldForSearch(query);
    QList<Found> found;
    for (const QuickInsertItem& item : items) {
        addMatches(item, lower, folded, found);
    }
    std::ranges::stable_sort(found, std::ranges::less{}, &Found::fit);
    QList<QuickInsertItem> matched;
    matched.reserve(found.size());
    for (Found& one : found) {
        matched.append(std::move(one.item));
    }
    return matched;
}

QuickInsertItem formItem(const QuickInsertItem& item, const QuickInsertForm& form) {
    QuickInsertItem made = item;
    made.name = form.text;
    made.text = form.text;
    made.forms.clear();
    const QString of = item.owner.isEmpty()
                           ? item.name
                           : QStringLiteral("%1 (%2)").arg(item.name, item.owner);
    made.detail = form.cases.isEmpty() ? of : QStringLiteral("%1 · %2").arg(form.cases, of);
    return made;
}

bool isEnclosingPair(const QString& text) {
    return enclosingPairs().contains(text);
}

}  // namespace kalahari::editor
