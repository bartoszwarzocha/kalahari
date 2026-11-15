// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file name_declension.cpp
/// @brief The forms of names in the grammatical cases: models of declension of a language

#include <kalahari/core/name_declension.h>

#include <kalahari/core/detail/name_declension_internal.h>

#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <array>
#include <utility>

namespace kalahari::core {

namespace {

using declension::at;
using declension::Candidates;
using declension::Cell;
using declension::Cells;
using declension::endsWithAny;
using declension::fitsWord;
using declension::isVowel;
using declension::PolishModel;
using declension::polishCandidates;
using declension::polishModelIds;
using declension::polishModelOf;
using declension::unchangedCandidates;

// =============================================================================
// Polish: picking a model
// =============================================================================

/// The models a word of a name of @p kind can have: of a woman, of a man, of a thing
QList<PolishModel> modelsOfKind(NameKind kind, bool feminine) {
    if (kind == NameKind::Character) {
        return feminine ? QList<PolishModel>{PolishModel::Anna, PolishModel::Kowalska}
                        : QList<PolishModel>{PolishModel::Jan, PolishModel::Marek,
                                             PolishModel::Kuba, PolishModel::Kowalski};
    }
    return {PolishModel::Gdansk,   PolishModel::Londyn,  PolishModel::Anna,  PolishModel::Kowalska,
            PolishModel::Kowalski, PolishModel::Gniezno, PolishModel::Opole, PolishModel::Zakopane};
}

/// Polish prepositions: the words of a name after one do not change ("Wieża nad Morzem")
bool isPreposition(const QString& word) {
    static const QSet<QString> prepositions{
        QStringLiteral("do"),   QStringLiteral("na"),   QStringLiteral("nad"),
        QStringLiteral("od"),   QStringLiteral("pod"),  QStringLiteral("przed"),
        QStringLiteral("przy"), QStringLiteral("spod"), QStringLiteral("u"),
        QStringLiteral("w"),    QStringLiteral("we"),   QStringLiteral("z"),
        QStringLiteral("za"),   QStringLiteral("ze"),   QStringLiteral("znad"),
        QStringLiteral("zza")};
    return prepositions.contains(word);
}

/// Whether the name of a character is a woman's: its first word ends in -a (and is not a
/// man's name such as Kuba), or a word is a feminine surname (Kowalska, Nowakowa)
bool isFeminineName(const QStringList& words) {
    static const QSet<QString> menInA{
        QStringLiteral("barnaba"), QStringLiteral("bonawentura"), QStringLiteral("jarema"),
        QStringLiteral("kosma"),   QStringLiteral("kuba"),        QStringLiteral("zawisza"),
        QStringLiteral("sasza"),   QStringLiteral("misza"),       QStringLiteral("grisza"),
        QStringLiteral("nikita"),  QStringLiteral("ilja"),        QStringLiteral("luca"),
        QStringLiteral("jona"),    QStringLiteral("mustafa")};
    static const QStringList feminineSurnames{QStringLiteral("ska"), QStringLiteral("cka"),
                                              QStringLiteral("dzka"), QStringLiteral("owa"),
                                              QStringLiteral("ówna")};
    if (words.isEmpty()) {
        return false;
    }
    const QString first = words.front().toLower();
    if (first.endsWith(QLatin1Char('a')) && !menInA.contains(first)) {
        return true;
    }
    return std::ranges::any_of(words, [](const QString& word) {
        return endsWithAny(word.toLower(), feminineSurnames);
    });
}

/// The model of a Polish word ending in -a: an adjective (Kowalska), a man's name (Kuba) or
/// a feminine noun (Anna)
PolishModel modelOfA(const QString& lower, bool man) {
    if (endsWithAny(lower, {QStringLiteral("ska"), QStringLiteral("cka"), QStringLiteral("dzka"),
                            QStringLiteral("owa")})) {
        return PolishModel::Kowalska;
    }
    return man ? PolishModel::Kuba : PolishModel::Anna;
}

/// The model of a Polish word ending in -e: a neuter adjective (Zakopane) or noun (Opole);
/// a character's does not change
PolishModel modelOfE(const QString& lower, bool character) {
    if (character) {
        return PolishModel::None;
    }
    return endsWithAny(lower, {QStringLiteral("ne"), QStringLiteral("kie"), QStringLiteral("gie"),
                               QStringLiteral("we"), QStringLiteral("te")})
               ? PolishModel::Zakopane
               : PolishModel::Opole;
}

/// The model of a Polish word ending in a consonant: a thing (Gdańsk), a man's name (Jan,
/// Marek); a woman's surname does not change (Anna Nowak)
PolishModel modelOfConsonant(const QString& lower, NameKind kind, bool feminine) {
    if (kind != NameKind::Character) {
        return PolishModel::Gdansk;
    }
    if (feminine) {
        return PolishModel::None;
    }
    const bool dropsE =
        lower.size() > 3 &&
        endsWithAny(lower, {QStringLiteral("ek"), QStringLiteral("eł"), QStringLiteral("ec")});
    return dropsE ? PolishModel::Marek : PolishModel::Jan;
}

/// Whether a Polish word keeps its form whatever its kind: too short, an acronym (NATO), or
/// after the first word already a genitive plural ((Wieża) Ossów)
bool keepsForm(const QString& word, const QString& lower, bool first) {
    return word.size() < 2 || word == word.toUpper() ||
           (!first && endsWithAny(lower, {QStringLiteral("ów"), QStringLiteral("ych"),
                                          QStringLiteral("ich")}));
}

/// The model of a Polish word ending in -o: a thing (Gniezno), a man's name (Fredro); a
/// woman's does not change
PolishModel modelOfO(bool character, bool feminine) {
    if (!character) {
        return PolishModel::Gniezno;
    }
    return feminine ? PolishModel::None : PolishModel::Kuba;
}

/// The model of a Polish word ending in a vowel or in another character than a letter
PolishModel modelOfVowel(const QString& lower, bool character, bool feminine) {
    switch (lower.back().unicode()) {
    case u'a':
        return modelOfA(lower, character && !feminine);
    case u'o':
        return modelOfO(character, feminine);
    case u'e':
        return modelOfE(lower, character);
    case u'i':
    case u'y':
        return character && feminine ? PolishModel::None : PolishModel::Kowalski;
    default:
        return PolishModel::None;  // Malibu
    }
}

/// The model a Polish word takes by its ending, the kind of its name and its place in it
PolishModel modelByEnding(const QString& word, NameKind kind, bool feminine, bool first) {
    const QString lower = word.toLower();
    if (keepsForm(word, lower, first)) {
        return PolishModel::None;
    }
    const QChar last = lower.back();
    if (isVowel(last) || !last.isLetter()) {
        return modelOfVowel(lower, kind == NameKind::Character, feminine);
    }
    return modelOfConsonant(lower, kind, feminine);
}

/// The forms of @p word that the dictionary knows, in lower case; empty when it does not
/// know the word or there is no dictionary
QSet<QString> knownForms(const QString& word, const NameDeclension::DictionaryForms& dictionary) {
    QSet<QString> known;
    if (!dictionary) {
        return known;
    }
    // A name the dictionary has as it is written (Biała, a town), and as a common word (biała)
    QStringList forms = dictionary(word);
    if (word != word.toLower()) {
        forms += dictionary(word.toLower());
    }
    for (const QString& form : std::as_const(forms)) {
        known.insert(form.toLower());
    }
    return known;
}

/// How many of the forms a model gives, but the nominative, the dictionary knows
int knownCount(const Candidates& candidates, const QString& word, const QSet<QString>& known) {
    QSet<QString> counted;
    const auto count = [&counted, &known, &word](const Cells& cells) {
        for (const Cell& cell : cells) {
            for (const QStringList* list : {&cell.forms, &cell.guesses}) {
                for (const QString& form : *list) {
                    const QString lower = form.toLower();
                    if (form != word && known.contains(lower)) {
                        counted.insert(lower);
                    }
                }
            }
        }
    };
    count(candidates.singular);
    count(candidates.plural);
    return static_cast<int>(counted.size());
}

/// The forms of @p forms that the dictionary knows
QStringList knownOf(const QStringList& forms, const QSet<QString>& known) {
    QStringList kept;
    for (const QString& form : forms) {
        if (known.contains(form.toLower())) {
            kept.append(form);
        }
    }
    return kept;
}

/// The forms of a cell: the model's that the dictionary knows; when it knows none of them,
/// the guesses it knows; else the model's
QStringList resolve(const Cell& cell, const QSet<QString>& known) {
    if (!known.isEmpty()) {
        QStringList kept = knownOf(cell.forms, known);
        if (kept.isEmpty()) {
            kept = knownOf(cell.guesses, known);
        }
        if (!kept.isEmpty()) {
            return kept;
        }
    }
    return cell.forms;
}

// =============================================================================
// English
// =============================================================================

/// The plural of an English noun: -s, -es after a hissing sound, -ies after a consonant and y
QString englishPlural(const QString& word) {
    const QString lower = word.toLower();
    if (endsWithAny(lower, {QStringLiteral("s"), QStringLiteral("x"), QStringLiteral("z"),
                            QStringLiteral("ch"), QStringLiteral("sh")})) {
        return word + QStringLiteral("es");
    }
    if (lower.size() >= 2 && lower.endsWith(QLatin1Char('y')) &&
        !isVowel(lower.at(lower.size() - 2))) {
        return word.chopped(1) + QStringLiteral("ies");
    }
    return word + QStringLiteral("s");
}

/// The id of the English model of a regular noun
constexpr QLatin1String REGULAR("regular");

/// The id of the model of a word that does not change
constexpr QLatin1String UNCHANGED("none");

/// The forms of cells written together word by word, at most this many in a cell
constexpr qsizetype MAX_COMBINATIONS = 4;

/// Each form of @p starts followed by each of @p forms, at most MAX_COMBINATIONS of them
QStringList joined(const QStringList& starts, const QStringList& forms) {
    QStringList next;
    for (const QString& start : starts) {
        for (const QString& form : forms) {
            if (next.size() == MAX_COMBINATIONS) {
                return next;
            }
            next.append(start.isEmpty() ? form : start + QLatin1Char(' ') + form);
        }
    }
    return next;
}

/// The forms of a cell of a name from the forms of its words in that cell
QStringList combine(const QList<QStringList>& wordForms) {
    QStringList combined{QString()};
    for (const QStringList& forms : wordForms) {
        if (forms.isEmpty()) {
            return {};
        }
        combined = joined(combined, forms);
    }
    return combined;
}

}  // namespace

// =============================================================================
// FormTable
// =============================================================================

const QStringList& FormTable::cell(GrammaticalCase grammaticalCase, bool inPlural) const {
    return (inPlural ? plural : singular).at(at(grammaticalCase));
}

QStringList& FormTable::cell(GrammaticalCase grammaticalCase, bool inPlural) {
    return (inPlural ? plural : singular).at(at(grammaticalCase));
}

QStringList FormTable::allForms(bool withPlural) const {
    QStringList forms;
    const auto add = [&forms](const std::array<QStringList, CASE_COUNT>& cells) {
        for (const QStringList& cellForms : cells) {
            for (const QString& form : cellForms) {
                if (!form.isEmpty() && !forms.contains(form)) {
                    forms.append(form);
                }
            }
        }
    };
    add(singular);
    if (withPlural) {
        add(plural);
    }
    return forms;
}

bool FormTable::isEmpty() const {
    return allForms(true).isEmpty();
}

const std::array<GrammaticalCase, CASE_COUNT>& FormTable::allCases() {
    static const std::array<GrammaticalCase, CASE_COUNT> cases{
        GrammaticalCase::Nominative, GrammaticalCase::Genitive,     GrammaticalCase::Dative,
        GrammaticalCase::Accusative, GrammaticalCase::Instrumental, GrammaticalCase::Locative,
        GrammaticalCase::Vocative};
    return cases;
}

QString FormTable::cellKey(GrammaticalCase grammaticalCase, bool inPlural) {
    static const std::array<QString, CASE_COUNT> names{
        QStringLiteral("nominative"), QStringLiteral("genitive"),     QStringLiteral("dative"),
        QStringLiteral("accusative"), QStringLiteral("instrumental"), QStringLiteral("locative"),
        QStringLiteral("vocative")};
    const QString& name = names.at(at(grammaticalCase));
    return inPlural ? name + QStringLiteral("Plural") : name;
}

bool FormTable::cellOfKey(const QString& key, GrammaticalCase& grammaticalCase, bool& inPlural) {
    for (const GrammaticalCase candidate : allCases()) {
        for (const bool plural : {false, true}) {
            if (cellKey(candidate, plural) == key) {
                grammaticalCase = candidate;
                inPlural = plural;
                return true;
            }
        }
    }
    return false;
}

// =============================================================================
// NameDeclension
// =============================================================================

NameDeclension::NameDeclension(const QString& language)
    : m_code(language.section(QLatin1Char('_'), 0, 0).section(QLatin1Char('-'), 0, 0).toLower()) {
    if (m_code == QLatin1String("pl")) {
        m_language = Language::Polish;
    } else if (m_code == QLatin1String("en")) {
        m_language = Language::English;
    }
}

QString NameDeclension::language() const {
    return m_code;
}

QList<GrammaticalCase> NameDeclension::cases() const {
    if (m_language == Language::Polish) {
        return {FormTable::allCases().begin(), FormTable::allCases().end()};
    }
    return {GrammaticalCase::Nominative};
}

QStringList NameDeclension::models() const {
    switch (m_language) {
    case Language::Polish:
        return polishModelIds();
    case Language::English:
        return {QString(REGULAR), QString(UNCHANGED)};
    case Language::Other:
        break;
    }
    return {QString(UNCHANGED)};
}

QString NameDeclension::unchanged() {
    return {UNCHANGED};
}

QStringList NameDeclension::wordsOf(const QString& name) {
    static const QRegularExpression spaces(QStringLiteral("\\s+"));
    return name.split(spaces, Qt::SkipEmptyParts);
}

QString NameDeclension::polishModel(const QString& word, NameKind kind, bool feminine, bool first,
                                    const DictionaryForms& dictionary) {
    const PolishModel byEnding = modelByEnding(word, kind, feminine, first);
    const QSet<QString> known =
        byEnding == PolishModel::None ? QSet<QString>() : knownForms(word, dictionary);
    if (known.isEmpty()) {
        return polishModelIds().at(static_cast<qsizetype>(byEnding));
    }

    // The model whose forms the dictionary knows best; the one by the ending on a tie
    PolishModel best = byEnding;
    int bestCount = knownCount(polishCandidates(word, byEnding, kind), word, known);
    for (const PolishModel model : modelsOfKind(kind, feminine)) {
        if (model == byEnding || !fitsWord(word, model)) {
            continue;
        }
        const int count = knownCount(polishCandidates(word, model, kind), word, known);
        if (count > bestCount) {
            best = model;
            bestCount = count;
        }
    }
    // A later word the dictionary knows, but none of whose forms a model gives, is already a
    // form of another word: (Księga) Popiołów
    if (bestCount == 0 && !first) {
        best = PolishModel::None;
    }
    return polishModelIds().at(static_cast<qsizetype>(best));
}

QStringList NameDeclension::modelsFor(const QString& name, NameKind kind,
                                      const DictionaryForms& dictionary) const {
    const QStringList words = wordsOf(name);
    if (m_language == Language::Polish) {
        return polishModels(words, kind, dictionary);
    }
    // In English the last word has the plural with -s (but an acronym, and a word written
    // small); in another language no word changes
    QStringList models;
    for (qsizetype index = 0; index < words.size(); ++index) {
        const QString& word = words.at(index);
        const bool regular = m_language == Language::English && index == words.size() - 1 &&
                             word != word.toUpper() && word.front().isUpper();
        models.append(QString(regular ? REGULAR : UNCHANGED));
    }
    return models;
}

QStringList NameDeclension::polishModels(const QStringList& words, NameKind kind,
                                         const DictionaryForms& dictionary) {
    QStringList models;
    const bool feminine = kind == NameKind::Character && isFeminineName(words);
    bool afterPreposition = false;
    for (qsizetype index = 0; index < words.size(); ++index) {
        const QString& word = words.at(index);
        const bool particle = index > 0 && word.front().isLower();
        afterPreposition = afterPreposition || (particle && isPreposition(word));
        if (afterPreposition || particle) {
            models.append(QString(UNCHANGED));  // nad Morzem; van (Beethoven)
        } else {
            models.append(polishModel(word, kind, feminine, index == 0, dictionary));
        }
    }
    return models;
}

FormTable NameDeclension::wordForms(const QString& word, const QString& model, NameKind kind,
                                    const DictionaryForms& dictionary) const {
    FormTable table;
    if (word.isEmpty()) {
        return table;
    }
    if (m_language == Language::Polish) {
        const PolishModel polish = polishModelOf(model);
        const Candidates candidates = fitsWord(word, polish) ? polishCandidates(word, polish, kind)
                                                             : unchangedCandidates(word);
        const QSet<QString> known =
            polish == PolishModel::None ? QSet<QString>() : knownForms(word, dictionary);
        for (const GrammaticalCase grammaticalCase : FormTable::allCases()) {
            table.cell(grammaticalCase, false) =
                resolve(candidates.singular.at(at(grammaticalCase)), known);
            table.cell(grammaticalCase, true) =
                resolve(candidates.plural.at(at(grammaticalCase)), known);
        }
        return table;
    }
    table.cell(GrammaticalCase::Nominative, false) = QStringList{word};
    const bool regular = m_language == Language::English && model == REGULAR;
    table.cell(GrammaticalCase::Nominative, true) =
        QStringList{regular ? englishPlural(word) : word};
    return table;
}

FormTable NameDeclension::nameForms(const QString& name, const QStringList& models, NameKind kind,
                                    const DictionaryForms& dictionary) const {
    const QStringList words = wordsOf(name);
    const QStringList wordModels =
        models.size() == words.size() ? models : modelsFor(name, kind, dictionary);
    QList<FormTable> tables;
    for (qsizetype index = 0; index < words.size(); ++index) {
        tables.append(wordForms(words.at(index), wordModels.at(index), kind, dictionary));
    }

    FormTable table;
    if (tables.isEmpty()) {
        return table;
    }
    for (const GrammaticalCase grammaticalCase : FormTable::allCases()) {
        for (const bool plural : {false, true}) {
            QList<QStringList> forms;
            for (const FormTable& wordTable : std::as_const(tables)) {
                forms.append(wordTable.cell(grammaticalCase, plural));
            }
            table.cell(grammaticalCase, plural) = combine(forms);
        }
    }
    return table;
}

}  // namespace kalahari::core
