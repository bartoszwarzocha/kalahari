/// @file quick_insert.cpp
/// @brief Quick insert: the items the typed letters find

#include <kalahari/editor/quick_insert.h>

#include <QStringList>

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
    const QString letters = foldForSearch(query);
    QList<QuickInsertItem> first;
    QList<QuickInsertItem> others;
    for (const QuickInsertItem& item : items) {
        const QString name = foldForSearch(item.name);
        if (name.startsWith(letters)) {
            first.append(item);
            continue;
        }
        const QStringList words = wordsOf(name);
        if (std::any_of(words.cbegin(), words.cend(),
                        [&letters](const QString& word) { return word.startsWith(letters); })) {
            others.append(item);
        }
    }
    first.append(others);
    return first;
}

bool isEnclosingPair(const QString& text) {
    return enclosingPairs().contains(text);
}

}  // namespace kalahari::editor
