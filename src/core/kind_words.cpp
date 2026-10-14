/// @file kind_words.cpp
/// @brief How the program's sentences name kinds of elements: the forms of their names

#include <kalahari/core/kind_words.h>

#include <optional>

namespace kalahari::core {

namespace {

const QString SINGULAR = QStringLiteral("singular");
const QString PLURAL = QStringLiteral("plural");
const QString PLURAL_SUFFIX = QStringLiteral("Plural");

/// A marker: the noun, with a capital letter or not, and what comes after the colon
struct Marker {
    QString noun;      ///< "kind"
    bool capital = false;
    QString spec;      ///< "genitive", "m=dodany|f=dodana|n=dodane", or empty
};

/// The marker of the text between braces, or nullopt when it is not one
std::optional<Marker> parseMarker(const QString& inside) {
    const qsizetype colon = inside.indexOf(QLatin1Char(':'));
    const QString name = colon < 0 ? inside : inside.left(colon);
    if (name.isEmpty()) {
        return std::nullopt;
    }
    for (const QChar character : name) {
        if (!character.isLetter() || character.unicode() > 0x7f) {
            return std::nullopt;
        }
    }
    Marker marker;
    marker.capital = name.front().isUpper();
    marker.noun = name.left(1).toLower() + name.mid(1);
    marker.spec = colon < 0 ? QString() : inside.mid(colon + 1);
    return marker;
}

/// The texts of a choice, "m=dodany|f=dodana|n=dodane|p=dodane" or "s=is|p=are", by their
/// letters
QHash<QChar, QString> choiceTexts(const QString& spec) {
    QHash<QChar, QString> texts;
    for (const QString& option : spec.split(QLatin1Char('|'))) {
        if (option.size() >= 2 && option.at(1) == QLatin1Char('=')) {
            texts.insert(option.at(0), option.mid(2));
        }
    }
    return texts;
}

/// The letter of @p gender in a choice by gender, or by number when @p byNumber
QChar choiceLetter(Gender gender, bool byNumber) {
    switch (gender) {
    case Gender::Plural:
        return QLatin1Char('p');
    case Gender::Feminine:
        return byNumber ? QLatin1Char('s') : QLatin1Char('f');
    case Gender::Neuter:
        return byNumber ? QLatin1Char('s') : QLatin1Char('n');
    case Gender::Masculine:
        break;
    }
    return byNumber ? QLatin1Char('s') : QLatin1Char('m');
}

/// Whether @p texts are a whole choice: by gender (m, f, n, p) or by number (s, p)
bool isWholeChoice(const QHash<QChar, QString>& texts, const QString& spec) {
    const auto has = [&texts](char letter) { return texts.contains(QLatin1Char(letter)); };
    const bool byGender = texts.size() == 4 && has('m') && has('f') && has('n') && has('p');
    const bool byNumber = texts.size() == 2 && has('s') && has('p');
    return (byGender || byNumber) && spec.count(QLatin1Char('|')) == texts.size() - 1;
}

QString capitalized(const QString& text) {
    return text.left(1).toUpper() + text.mid(1);
}

/// The text of @p marker for @p words, or nullopt when the marker asks for what is not there
std::optional<QString> fill(const Marker& marker, const KindWords& words) {
    QString text;
    if (marker.spec.contains(QLatin1Char('='))) {
        const QHash<QChar, QString> texts = choiceTexts(marker.spec);
        const auto it =
            texts.constFind(choiceLetter(words.gender, texts.contains(QLatin1Char('s'))));
        if (it == texts.constEnd()) {
            return std::nullopt;
        }
        text = it.value();
    } else {
        const QString form = marker.spec.isEmpty() ? SINGULAR : marker.spec;
        if (!KindWords::formNames().contains(form)) {
            return std::nullopt;
        }
        text = words.form(form);
    }
    return marker.capital ? capitalized(text) : text;
}

}  // namespace

QString KindWords::form(const QString& name) const {
    if (!formNames().contains(name)) {
        return QString();
    }
    const auto given = forms.constFind(name);
    if (given != forms.constEnd()) {
        return given.value();
    }
    if (name == SINGULAR || name == PLURAL) {
        return QString();
    }
    // A case of the plural comes from the plural, any other form from the singular
    return form(name.endsWith(PLURAL_SUFFIX) ? PLURAL : SINGULAR);
}

const QStringList& KindWords::formNames() {
    static const QStringList names{
        SINGULAR,
        PLURAL,
        QStringLiteral("genitive"),
        QStringLiteral("dative"),
        QStringLiteral("accusative"),
        QStringLiteral("instrumental"),
        QStringLiteral("locative"),
        QStringLiteral("genitivePlural"),
        QStringLiteral("dativePlural"),
        QStringLiteral("accusativePlural"),
        QStringLiteral("instrumentalPlural"),
        QStringLiteral("locativePlural"),
        QStringLiteral("indefinite"),
    };
    return names;
}

bool KindWords::genderFromName(const QString& name, Gender& gender) {
    if (name == QLatin1String("masculine")) {
        gender = Gender::Masculine;
    } else if (name == QLatin1String("feminine")) {
        gender = Gender::Feminine;
    } else if (name == QLatin1String("neuter")) {
        gender = Gender::Neuter;
    } else if (name == QLatin1String("plural")) {
        gender = Gender::Plural;
    } else {
        return false;
    }
    return true;
}

QString fillWords(const QString& text, const QHash<QString, KindWords>& nouns) {
    QString result;
    qsizetype from = 0;
    // Each '}' ends the marker that starts at the nearest '{' before it
    for (qsizetype close = text.indexOf(QLatin1Char('}')); close >= 0;
         close = text.indexOf(QLatin1Char('}'), from)) {
        const qsizetype open = text.lastIndexOf(QLatin1Char('{'), close);
        if (open < from) {
            result += QStringView(text).mid(from, close - from + 1);
            from = close + 1;
            continue;
        }
        result += QStringView(text).mid(from, open - from);
        std::optional<QString> filled;
        if (const std::optional<Marker> marker =
                parseMarker(text.mid(open + 1, close - open - 1))) {
            if (const auto words = nouns.constFind(marker->noun); words != nouns.constEnd()) {
                filled = fill(*marker, words.value());
            }
        }
        result += filled ? *filled : text.mid(open, close - open + 1);
        from = close + 1;
    }
    result += QStringView(text).mid(from);
    return result;
}

QStringList wordMarkerProblems(const QString& text, QStringList* nouns) {
    QStringList problems;
    qsizetype from = 0;
    for (qsizetype close = text.indexOf(QLatin1Char('}')); close >= 0;
         close = text.indexOf(QLatin1Char('}'), from)) {
        const qsizetype open = text.lastIndexOf(QLatin1Char('{'), close);
        const qsizetype start = from;
        from = close + 1;
        if (open < start) {
            problems << QStringLiteral("'}' without '{'");
            continue;
        }
        if (open > start && text.lastIndexOf(QLatin1Char('{'), open - 1) >= start) {
            problems << QStringLiteral("'{' without '}'");
        }
        const QString inside = text.mid(open + 1, close - open - 1);
        const std::optional<Marker> marker = parseMarker(inside);
        if (!marker) {
            problems << QStringLiteral("'{%1}' does not name a noun").arg(inside);
            continue;
        }
        if (nouns && !nouns->contains(marker->noun)) {
            *nouns << marker->noun;
        }
        if (marker->spec.contains(QLatin1Char('='))) {
            if (!isWholeChoice(choiceTexts(marker->spec), marker->spec)) {
                problems << QStringLiteral("'{%1}' needs the text of each gender, "
                                           "m=...|f=...|n=...|p=..., or of each number, "
                                           "s=...|p=...")
                                .arg(inside);
            }
        } else if (!marker->spec.isEmpty() && !KindWords::formNames().contains(marker->spec)) {
            problems << QStringLiteral("'{%1}': '%2' is not a form").arg(inside, marker->spec);
        }
    }
    if (text.indexOf(QLatin1Char('{'), from) >= 0) {
        problems << QStringLiteral("'{' without '}'");
    }
    return problems;
}

}  // namespace kalahari::core
