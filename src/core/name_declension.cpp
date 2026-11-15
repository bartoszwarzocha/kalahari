// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file name_declension.cpp
/// @brief The forms of names in the grammatical cases: models of declension of a language

#include <kalahari/core/name_declension.h>

#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

namespace kalahari::core {

namespace {

// =============================================================================
// Cells
// =============================================================================

/// The forms a model gives one case: those it is sure of, and others that may be right in
/// their place, which count only when the dictionary knows them
struct Cell {
    QStringList forms;    ///< Right as far as the model can tell, the main first
    QStringList guesses;  ///< Likely, but only the dictionary can tell (Ksiąg for Księg)
};

/// The cells of one number
using Cells = std::array<Cell, CASE_COUNT>;

/// The cells a model gives a word
struct Candidates {
    Cells singular;  ///< By case
    Cells plural;    ///< By case
};

/// Where a case is in the arrays
constexpr std::size_t at(GrammaticalCase grammaticalCase) {
    return static_cast<std::size_t>(grammaticalCase);
}

/// Set the forms of a case
void put(Cells& cells, GrammaticalCase grammaticalCase, const QString& form,
         const QStringList& guesses = {}) {
    Cell& cell = cells.at(at(grammaticalCase));
    cell.forms = QStringList{form};
    cell.guesses = guesses;
}

/// The plural of a noun from its stem: the dative, instrumental and locative endings, the
/// vocative as the nominative and the accusative as the genitive (of persons) or the
/// nominative (of things)
void putPlural(Cells& cells, const QString& stem, const QString& nominative, const Cell& genitive,
               bool personal) {
    put(cells, GrammaticalCase::Nominative, nominative);
    cells.at(at(GrammaticalCase::Genitive)) = genitive;
    put(cells, GrammaticalCase::Dative, stem + QStringLiteral("om"));
    if (personal) {
        cells.at(at(GrammaticalCase::Accusative)) = genitive;
    } else {
        put(cells, GrammaticalCase::Accusative, nominative);
    }
    put(cells, GrammaticalCase::Instrumental, stem + QStringLiteral("ami"));
    put(cells, GrammaticalCase::Locative, stem + QStringLiteral("ach"));
    put(cells, GrammaticalCase::Vocative, nominative);
}

/// Every cell of both numbers with @p form
Candidates unchangedCandidates(const QString& form) {
    Candidates candidates;
    for (Cell& cell : candidates.singular) {
        cell.forms = QStringList{form};
    }
    for (Cell& cell : candidates.plural) {
        cell.forms = QStringList{form};
    }
    return candidates;
}

// =============================================================================
// Polish: the end of a stem
// =============================================================================

/// What the end of a stem is, for the endings it takes
enum class StemEnd {
    Hard,        ///< b, d, f, ł, m, n, p, r, s, t, w, z...: Anna -> Annie
    Velar,       ///< k, g, ch: Monika -> Moniki, Monice
    Hardened,    ///< c, cz, dz, dż, rz, sz, ż: Wieża -> Wieży
    SoftI,       ///< ci, dzi, ni, si, zi (the i only softens): Kasia -> Kasi
    ConsonantI,  ///< i after another consonant (a vowel): Zofia -> Zofii
    J,           ///< j: Maja -> Mai, Patrycja -> Patrycji
    L            ///< l: Ola -> Oli
};

/// Whether @p character is a Polish vowel
bool isVowel(QChar character) {
    static const QString vowels = QStringLiteral("aąeęioóuy");
    return vowels.contains(character.toLower());
}

/// Whether @p text ends with one of @p endings
bool endsWithAny(const QString& text, const QStringList& endings) {
    return std::any_of(endings.begin(), endings.end(),
                       [&text](const QString& ending) { return text.endsWith(ending); });
}

/// What the end of @p stem (in lower case) is
StemEnd stemEnd(const QString& stem) {
    static const QStringList velar{QStringLiteral("k"), QStringLiteral("g"), QStringLiteral("ch")};
    static const QStringList hardened{
        QStringLiteral("c"),  QStringLiteral("cz"), QStringLiteral("dz"), QStringLiteral("dż"),
        QStringLiteral("rz"), QStringLiteral("sz"), QStringLiteral("ż")};
    static const QStringList softI{QStringLiteral("ci"), QStringLiteral("ni"), QStringLiteral("si"),
                                   QStringLiteral("zi")};
    if (endsWithAny(stem, velar)) {
        return StemEnd::Velar;
    }
    if (endsWithAny(stem, hardened)) {
        return StemEnd::Hardened;
    }
    if (endsWithAny(stem, softI)) {
        return StemEnd::SoftI;
    }
    if (stem.endsWith(QLatin1Char('i'))) {
        return StemEnd::ConsonantI;
    }
    if (stem.endsWith(QLatin1Char('j'))) {
        return StemEnd::J;
    }
    if (stem.endsWith(QLatin1Char('l'))) {
        return StemEnd::L;
    }
    return StemEnd::Hard;
}

/// @p stem with its last @p count characters replaced by @p ending
QString replaceEnd(const QString& stem, qsizetype count, const QString& ending) {
    return stem.chopped(count) + ending;
}

/// The locative of a hard stem: its last consonants softened, and -e (Anna -> Annie,
/// Marta -> Marcie, Piotr -> Piotrze, Gniezno -> Gnieźnie)
QString hardLocative(const QString& stem) {
    static const QList<std::pair<QString, QString>> endings{
        {QStringLiteral("st"), QStringLiteral("ście")},
        {QStringLiteral("zd"), QStringLiteral("ździe")},
        {QStringLiteral("sł"), QStringLiteral("śle")},
        {QStringLiteral("zł"), QStringLiteral("źle")},
        {QStringLiteral("sn"), QStringLiteral("śnie")},
        {QStringLiteral("zn"), QStringLiteral("źnie")},
        {QStringLiteral("t"), QStringLiteral("cie")},
        {QStringLiteral("d"), QStringLiteral("dzie")},
        {QStringLiteral("r"), QStringLiteral("rze")},
        {QStringLiteral("ł"), QStringLiteral("le")},
        {QStringLiteral("n"), QStringLiteral("nie")},
        {QStringLiteral("s"), QStringLiteral("sie")},
        {QStringLiteral("z"), QStringLiteral("zie")},
        {QStringLiteral("m"), QStringLiteral("mie")},
        {QStringLiteral("p"), QStringLiteral("pie")},
        {QStringLiteral("b"), QStringLiteral("bie")},
        {QStringLiteral("w"), QStringLiteral("wie")},
        {QStringLiteral("f"), QStringLiteral("fie")},
        {QStringLiteral("v"), QStringLiteral("vie")}};
    const QString lower = stem.toLower();
    for (const auto& [end, ending] : endings) {
        if (lower.endsWith(end)) {
            return replaceEnd(stem, end.size(), ending);
        }
    }
    return stem + QStringLiteral("ie");
}

/// The dative and locative of a feminine velar stem (Monika -> Monice, Olga -> Oldze)
QString velarLocative(const QString& stem) {
    const QString lower = stem.toLower();
    if (lower.endsWith(QLatin1String("ch"))) {
        return replaceEnd(stem, 2, QStringLiteral("sze"));
    }
    if (lower.endsWith(QLatin1Char('g'))) {
        return replaceEnd(stem, 1, QStringLiteral("dze"));
    }
    return replaceEnd(stem, 1, QStringLiteral("ce"));
}

/// The genitive plural of a soft stem written with i: its last consonant with an accent
/// (Kasi -> Kaś, Ani -> Ań)
QString softened(const QString& stem) {
    static const QList<std::pair<QString, QString>> endings{
        {QStringLiteral("ci"), QStringLiteral("ć")},
        {QStringLiteral("ni"), QStringLiteral("ń")},
        {QStringLiteral("si"), QStringLiteral("ś")},
        {QStringLiteral("zi"), QStringLiteral("ź")}};
    const QString lower = stem.toLower();
    for (const auto& [end, ending] : endings) {
        if (lower.endsWith(end)) {
            return replaceEnd(stem, end.size(), ending);
        }
    }
    return stem;
}

/// The genitive plural of a hard stem: the stem, or one of its changed forms that the
/// dictionary may know (Księg -> Ksiąg, Agnieszk -> Agnieszek)
Cell genitivePlural(const QString& stem) {
    Cell cell{.forms = QStringList{stem}, .guesses = {}};
    const QString lower = stem.toLower();
    for (qsizetype index = lower.size() - 1; index >= 0; --index) {
        if (!isVowel(lower.at(index))) {
            continue;
        }
        QString changed = stem;
        if (lower.at(index) == QChar(u'ę')) {
            changed[index] = QChar(u'ą');
            cell.guesses.append(changed);
        } else if (lower.at(index) == QLatin1Char('o')) {
            changed[index] = QChar(u'ó');
            cell.guesses.append(changed);
        }
        break;
    }
    const bool twoConsonants =
        lower.size() >= 2 && !isVowel(lower.back()) && !isVowel(lower.at(lower.size() - 2));
    if (twoConsonants) {
        cell.guesses.append(stem.chopped(1) + QLatin1Char('e') + stem.back());
    }
    return cell;
}

// =============================================================================
// Polish: the models
// =============================================================================

/// The models of Polish
enum class PolishModel {
    Jan,       ///< A man's name ending in a consonant: Jan, Piotr, Eldrion
    Marek,     ///< ... whose e drops: Marek -> Marka, Paweł -> Pawła
    Kuba,      ///< A man's name ending in -a or -o: Kuba, Fredro
    Kowalski,  ///< A masculine adjective: Kowalski, Jerzy, Czarny (Las)
    Anna,      ///< A feminine noun ending in -a: Anna, Kasia, Zofia, Olga, Wieża
    Kowalska,  ///< A feminine adjective: Kowalska, Nowakowa, Biała (Wieża)
    Gdansk,    ///< A masculine thing ending in a consonant, genitive -a: Gdańsk, Kraków
    Londyn,    ///< ... genitive -u: Londyn
    Gniezno,   ///< A neuter noun ending in -o: Gniezno, Kłodzko
    Opole,     ///< A neuter noun ending in -e: Opole, Zabrze
    Zakopane,  ///< A neuter adjective: Zakopane, Złote (Wzgórze)
    None       ///< The word does not change
};

/// The ids of the Polish models, in the order of PolishModel
const QStringList& polishModelIds() {
    static const QStringList ids{
        QStringLiteral("jan"),      QStringLiteral("marek"),    QStringLiteral("kuba"),
        QStringLiteral("kowalski"), QStringLiteral("anna"),     QStringLiteral("kowalska"),
        QStringLiteral("gdansk"),   QStringLiteral("londyn"),   QStringLiteral("gniezno"),
        QStringLiteral("opole"),    QStringLiteral("zakopane"), QStringLiteral("none")};
    return ids;
}

/// The model of an id, or None
PolishModel polishModelOf(const QString& model) {
    const qsizetype index = polishModelIds().indexOf(model);
    return index < 0 ? PolishModel::None : static_cast<PolishModel>(index);
}

/// The stem the endings of a masculine noun follow: -ów as -ow (Kraków -> Krakow), the e
/// that drops (Marek -> Mark), and a soft consonant written with i (Staś -> Stasi)
QString masculineStem(const QString& word, bool dropsE) {
    const QString lower = word.toLower();
    if (lower.endsWith(QStringLiteral("ów"))) {
        return word.chopped(2) + QStringLiteral("ow");
    }
    QString stem = word;
    if (dropsE) {
        const qsizetype letterE = lower.lastIndexOf(QLatin1Char('e'));
        const bool consonantsAfter =
            letterE > 0 && letterE < lower.size() - 1 &&
            std::none_of(lower.begin() + letterE + 1, lower.end(), isVowel);
        if (consonantsAfter) {
            stem.remove(letterE, 1);
        }
    }
    static const QList<std::pair<QChar, QString>> soft{{QChar(u'ć'), QStringLiteral("ci")},
                                                       {QChar(u'ń'), QStringLiteral("ni")},
                                                       {QChar(u'ś'), QStringLiteral("si")},
                                                       {QChar(u'ź'), QStringLiteral("zi")}};
    for (const auto& [letter, written] : soft) {
        if (lower.endsWith(letter)) {
            return replaceEnd(stem, 1, written);
        }
    }
    return stem;
}

/// A masculine noun ending in a consonant: Jan, Marek (personal), Gdańsk, Londyn (things)
Candidates masculineNoun(const QString& word, bool personal, bool genitiveU, bool dropsE) {
    const QString stem = masculineStem(word, dropsE);
    const QString lower = stem.toLower();
    const StemEnd end = stemEnd(lower);
    const bool kOrG = lower.endsWith(QLatin1Char('k')) || lower.endsWith(QLatin1Char('g'));

    Candidates candidates;
    Cells& singular = candidates.singular;
    const QString genitive = stem + (genitiveU ? QStringLiteral("u") : QStringLiteral("a"));
    const QString locative = end == StemEnd::Hard ? hardLocative(stem) : stem + QStringLiteral("u");
    put(singular, GrammaticalCase::Nominative, word);
    put(singular, GrammaticalCase::Genitive, genitive);
    put(singular, GrammaticalCase::Dative, stem + QStringLiteral("owi"));
    put(singular, GrammaticalCase::Accusative, personal ? genitive : word);
    put(singular, GrammaticalCase::Instrumental,
        stem + (kOrG ? QStringLiteral("iem") : QStringLiteral("em")));
    put(singular, GrammaticalCase::Locative, locative);
    put(singular, GrammaticalCase::Vocative, locative);

    if (personal) {
        putPlural(candidates.plural, stem, stem + QStringLiteral("owie"),
                  Cell{.forms = QStringList{stem + QStringLiteral("ów")}, .guesses = {}}, true);
        return candidates;
    }
    QString nominative = stem + QStringLiteral("e");
    if (end == StemEnd::Hard || lower.endsWith(QLatin1String("ch"))) {
        nominative = stem + QStringLiteral("y");
    } else if (kOrG) {
        nominative = stem + QStringLiteral("i");
    }
    const QString genitivePluralForm = end == StemEnd::SoftI ? stem : stem + QStringLiteral("ów");
    putPlural(candidates.plural, stem, nominative,
              Cell{.forms = QStringList{genitivePluralForm}, .guesses = {}}, false);
    return candidates;
}

/// The genitive, dative (as the locative) and vocative of a noun ending in -a, by its stem
struct AForms {
    QString genitive;  ///< Anny
    QString dative;    ///< Annie
    QString vocative;  ///< Anno
};

/// The forms of the singular of a noun ending in -a that depend on its stem
AForms aForms(const QString& stem) {
    const QString lower = stem.toLower();
    AForms forms{.genitive = stem + QStringLiteral("y"),
                 .dative = QString(),
                 .vocative = stem + QStringLiteral("o")};
    switch (stemEnd(lower)) {
    case StemEnd::SoftI:
        forms.genitive = stem;
        forms.vocative = stem + QStringLiteral("u");
        break;
    case StemEnd::ConsonantI:
        forms.genitive = stem + QStringLiteral("i");
        break;
    case StemEnd::J: {
        const bool afterVowel = lower.size() >= 2 && isVowel(lower.at(lower.size() - 2));
        forms.genitive =
            afterVowel ? stem.chopped(1) + QStringLiteral("i") : stem + QStringLiteral("i");
        if (afterVowel) {
            forms.vocative = stem + QStringLiteral("u");
        }
        break;
    }
    case StemEnd::L:
        forms.genitive = stem + QStringLiteral("i");
        if (lower.size() <= 2) {
            forms.vocative = stem + QStringLiteral("u");  // Ola -> Olu, but Urszula -> Urszulo
        }
        break;
    case StemEnd::Hardened:
        break;
    case StemEnd::Velar:
        if (!lower.endsWith(QLatin1String("ch"))) {
            forms.genitive = stem + QStringLiteral("i");
        }
        forms.dative = velarLocative(stem);
        break;
    case StemEnd::Hard:
        forms.dative = hardLocative(stem);
        break;
    }
    if (forms.dative.isEmpty()) {
        forms.dative = forms.genitive;
    }
    return forms;
}

/// A noun ending in -a (or -o, for a man): Anna, Kasia, Zofia, Olga (feminine); Kuba,
/// Fredro (a man, with the plural of persons)
Candidates aNoun(const QString& word, bool masculine) {
    const QString stem = word.chopped(1);
    const QString lower = stem.toLower();
    const AForms forms = aForms(stem);
    const bool endsInO = word.back().toLower() == QLatin1Char('o');

    Candidates candidates;
    Cells& singular = candidates.singular;
    put(singular, GrammaticalCase::Nominative, word);
    put(singular, GrammaticalCase::Genitive, forms.genitive);
    put(singular, GrammaticalCase::Dative, forms.dative);
    put(singular, GrammaticalCase::Accusative, stem + QStringLiteral("ę"));
    put(singular, GrammaticalCase::Instrumental, stem + QStringLiteral("ą"));
    put(singular, GrammaticalCase::Locative, forms.dative);
    put(singular, GrammaticalCase::Vocative, endsInO ? word : forms.vocative);

    if (masculine) {
        putPlural(candidates.plural, stem, stem + QStringLiteral("owie"),
                  Cell{.forms = QStringList{stem + QStringLiteral("ów")}, .guesses = {}}, true);
        return candidates;
    }
    const StemEnd end = stemEnd(lower);
    QString nominative = stem + QStringLiteral("e");
    Cell genitive{.forms = QStringList{forms.genitive}, .guesses = {}};
    if (end == StemEnd::Hard || (end == StemEnd::Velar && lower.endsWith(QLatin1String("ch")))) {
        nominative = stem + QStringLiteral("y");
        genitive = genitivePlural(stem);
    } else if (end == StemEnd::Velar) {
        nominative = stem + QStringLiteral("i");
        genitive = genitivePlural(stem);
    } else if (end == StemEnd::SoftI) {
        genitive.forms = QStringList{softened(stem)};
    } else if (end == StemEnd::Hardened || end == StemEnd::L) {
        genitive.forms = QStringList{stem};  // Wież, Ol
    }
    putPlural(candidates.plural, stem, nominative, genitive, false);
    return candidates;
}

/// The plural of an adjective with its stem (Kowalsk, Czarn), of things or of persons
void adjectivePlural(Cells& cells, const QString& stem, const QString& nominative,
                     const QString& personalNominative, bool personal) {
    const QString lower = stem.toLower();
    const bool withI = lower.endsWith(QLatin1Char('k')) || lower.endsWith(QLatin1Char('g')) ||
                       lower.endsWith(QLatin1Char('i'));
    const QString soft = lower.endsWith(QLatin1Char('i')) ? QString() : QStringLiteral("i");
    const QString vowel = withI ? soft : QStringLiteral("y");
    const QString genitive = stem + vowel + QStringLiteral("ch");
    const QString first = personal ? personalNominative : nominative;
    put(cells, GrammaticalCase::Nominative, first);
    put(cells, GrammaticalCase::Genitive, genitive);
    put(cells, GrammaticalCase::Dative, stem + vowel + QStringLiteral("m"));
    put(cells, GrammaticalCase::Accusative, personal ? genitive : first);
    put(cells, GrammaticalCase::Instrumental, stem + vowel + QStringLiteral("mi"));
    put(cells, GrammaticalCase::Locative, genitive);
    put(cells, GrammaticalCase::Vocative, first);
}

/// The nominative plural of a masculine adjective of persons (Kowalski -> Kowalscy,
/// Czarny -> Czarni)
QString personalAdjectivePlural(const QString& word) {
    static const QList<std::pair<QString, QString>> endings{
        {QStringLiteral("ki"), QStringLiteral("cy")},
        {QStringLiteral("gi"), QStringLiteral("dzy")},
        {QStringLiteral("chy"), QStringLiteral("si")},
        {QStringLiteral("ry"), QStringLiteral("rzy")},
        {QStringLiteral("ny"), QStringLiteral("ni")},
        {QStringLiteral("ty"), QStringLiteral("ci")},
        {QStringLiteral("dy"), QStringLiteral("dzi")},
        {QStringLiteral("ły"), QStringLiteral("li")},
        {QStringLiteral("wy"), QStringLiteral("wi")},
        {QStringLiteral("sy"), QStringLiteral("si")},
        {QStringLiteral("my"), QStringLiteral("mi")},
        {QStringLiteral("py"), QStringLiteral("pi")},
        {QStringLiteral("by"), QStringLiteral("bi")}};
    const QString lower = word.toLower();
    for (const auto& [end, ending] : endings) {
        if (lower.endsWith(end)) {
            return replaceEnd(word, end.size(), ending);
        }
    }
    return word;
}

/// A masculine adjective: Kowalski, Jerzy, Antoni (persons); Czarny (Las) (things)
Candidates masculineAdjective(const QString& word, bool personal) {
    const bool endsInI = word.back().toLower() == QLatin1Char('i');
    const QString stem = endsInI ? word : word.chopped(1);  // Kowalski-ego, Jerz-ego
    const QString instrumental = stem + (endsInI ? QStringLiteral("m") : QStringLiteral("ym"));
    const QString genitive = stem + QStringLiteral("ego");

    Candidates candidates;
    Cells& singular = candidates.singular;
    put(singular, GrammaticalCase::Nominative, word);
    put(singular, GrammaticalCase::Genitive, genitive);
    put(singular, GrammaticalCase::Dative, stem + QStringLiteral("emu"));
    put(singular, GrammaticalCase::Accusative, personal ? genitive : word);
    put(singular, GrammaticalCase::Instrumental, instrumental);
    put(singular, GrammaticalCase::Locative, instrumental);
    put(singular, GrammaticalCase::Vocative, word);

    const QString pluralStem = word.chopped(1);
    const QString nominative = pluralStem + (endsInI ? QStringLiteral("ie") : QStringLiteral("e"));
    adjectivePlural(candidates.plural, pluralStem, nominative, personalAdjectivePlural(word),
                    personal);
    return candidates;
}

/// A feminine adjective: Kowalska, Nowakowa, Biała (Wieża)
Candidates feminineAdjective(const QString& word) {
    const QString stem = word.chopped(1);
    const QString lower = stem.toLower();
    const bool velar = lower.endsWith(QLatin1Char('k')) || lower.endsWith(QLatin1Char('g'));
    const QString genitive = stem + (velar ? QStringLiteral("iej") : QStringLiteral("ej"));

    Candidates candidates;
    Cells& singular = candidates.singular;
    put(singular, GrammaticalCase::Nominative, word);
    put(singular, GrammaticalCase::Genitive, genitive);
    put(singular, GrammaticalCase::Dative, genitive);
    put(singular, GrammaticalCase::Accusative, stem + QStringLiteral("ą"));
    put(singular, GrammaticalCase::Instrumental, stem + QStringLiteral("ą"));
    put(singular, GrammaticalCase::Locative, genitive);
    put(singular, GrammaticalCase::Vocative, word);

    const QString nominative = stem + (velar ? QStringLiteral("ie") : QStringLiteral("e"));
    adjectivePlural(candidates.plural, stem, nominative, nominative, false);
    return candidates;
}

/// A neuter adjective: Zakopane, Wysokie, Złote (Wzgórze)
Candidates neuterAdjective(const QString& word) {
    const QString stem = word.chopped(1);  // Zakopan, Wysoki
    const bool endsInI = stem.back().toLower() == QLatin1Char('i');

    Candidates candidates;
    Cells& singular = candidates.singular;
    put(singular, GrammaticalCase::Nominative, word);
    put(singular, GrammaticalCase::Genitive, stem + QStringLiteral("ego"));
    put(singular, GrammaticalCase::Dative, stem + QStringLiteral("emu"));
    put(singular, GrammaticalCase::Accusative, word);
    Cell& instrumental = singular.at(at(GrammaticalCase::Instrumental));
    instrumental.forms =
        endsInI ? QStringList{stem + QStringLiteral("m")}
                : QStringList{stem + QStringLiteral("em"), stem + QStringLiteral("ym")};
    singular.at(at(GrammaticalCase::Locative)) = instrumental;
    put(singular, GrammaticalCase::Vocative, word);

    const QString pluralStem = endsInI ? stem.chopped(1) : stem;
    adjectivePlural(candidates.plural, pluralStem, word, word, false);
    return candidates;
}

/// A neuter noun ending in -o (Gniezno, Kłodzko) or -e (Opole, Zabrze)
Candidates neuterNoun(const QString& word) {
    const QString stem = word.chopped(1);
    const QString lower = stem.toLower();
    const bool endsInO = word.back().toLower() == QLatin1Char('o');
    const bool kOrG = lower.endsWith(QLatin1Char('k')) || lower.endsWith(QLatin1Char('g'));
    const bool hard = endsInO && stemEnd(lower) == StemEnd::Hard;

    Candidates candidates;
    Cells& singular = candidates.singular;
    put(singular, GrammaticalCase::Nominative, word);
    put(singular, GrammaticalCase::Genitive, stem + QStringLiteral("a"));
    put(singular, GrammaticalCase::Dative, stem + QStringLiteral("u"));
    put(singular, GrammaticalCase::Accusative, word);
    put(singular, GrammaticalCase::Instrumental,
        stem + (kOrG ? QStringLiteral("iem") : QStringLiteral("em")));
    put(singular, GrammaticalCase::Locative,
        hard ? hardLocative(stem) : stem + QStringLiteral("u"));
    put(singular, GrammaticalCase::Vocative, word);

    putPlural(candidates.plural, stem, stem + QStringLiteral("a"),
              endsInO ? genitivePlural(stem) : Cell{.forms = QStringList{stem}, .guesses = {}},
              false);
    return candidates;
}

/// The cells a Polish model gives a word
Candidates polishCandidates(const QString& word, PolishModel model, NameKind kind) {
    const bool personal = kind == NameKind::Character;
    switch (model) {
    case PolishModel::Jan:
        return masculineNoun(word, true, false, false);
    case PolishModel::Marek:
        return masculineNoun(word, true, false, true);
    case PolishModel::Kuba:
        return aNoun(word, true);
    case PolishModel::Kowalski:
        return masculineAdjective(word, personal);
    case PolishModel::Anna:
        return aNoun(word, false);
    case PolishModel::Kowalska:
        return feminineAdjective(word);
    case PolishModel::Gdansk:
        return masculineNoun(word, false, false, false);
    case PolishModel::Londyn:
        return masculineNoun(word, false, true, false);
    case PolishModel::Gniezno:
    case PolishModel::Opole:
        return neuterNoun(word);
    case PolishModel::Zakopane:
        return neuterAdjective(word);
    case PolishModel::None:
        break;
    }
    return unchangedCandidates(word);
}

/// Whether a model can be given to @p word: the endings it takes off are there
bool fitsWord(const QString& word, PolishModel model) {
    const QChar last = word.back().toLower();
    switch (model) {
    case PolishModel::Jan:
    case PolishModel::Marek:
    case PolishModel::Gdansk:
    case PolishModel::Londyn:
        return !isVowel(last);
    case PolishModel::Kuba:
        return last == QLatin1Char('a') || last == QLatin1Char('o');
    case PolishModel::Anna:
    case PolishModel::Kowalska:
        return last == QLatin1Char('a');
    case PolishModel::Kowalski:
        return last == QLatin1Char('i') || last == QLatin1Char('y');
    case PolishModel::Gniezno:
        return last == QLatin1Char('o');
    case PolishModel::Opole:
    case PolishModel::Zakopane:
        return last == QLatin1Char('e');
    case PolishModel::None:
        break;
    }
    return true;
}

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

// =============================================================================
// Polish: picking a model
// =============================================================================

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
    return std::any_of(words.begin(), words.end(), [](const QString& word) {
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

/// The model a Polish word takes by its ending, the kind of its name and its place in it
PolishModel modelByEnding(const QString& word, NameKind kind, bool feminine, bool first) {
    const QString lower = word.toLower();
    const bool character = kind == NameKind::Character;
    if (word.size() < 2 || word == word.toUpper() ||
        (!first && endsWithAny(lower, {QStringLiteral("ów"), QStringLiteral("ych"),
                                       QStringLiteral("ich")}))) {
        return PolishModel::None;  // NATO; already a genitive plural: (Wieża) Ossów
    }
    const QChar last = lower.back();
    if (last == QLatin1Char('a')) {
        return modelOfA(lower, character && !feminine);
    }
    if (last == QLatin1Char('o')) {
        if (!character) {
            return PolishModel::Gniezno;
        }
        return feminine ? PolishModel::None : PolishModel::Kuba;
    }
    if (last == QLatin1Char('e')) {
        return modelOfE(lower, character);
    }
    if (last == QLatin1Char('i') || last == QLatin1Char('y')) {
        return character && feminine ? PolishModel::None : PolishModel::Kowalski;
    }
    if (isVowel(last) || !last.isLetter()) {
        return PolishModel::None;  // Malibu
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

/// The forms of a cell of a name from the forms of its words in that cell
QStringList combine(const QList<QStringList>& wordForms) {
    QStringList combined{QString()};
    for (const QStringList& forms : wordForms) {
        if (forms.isEmpty()) {
            return {};
        }
        QStringList next;
        for (const QString& start : std::as_const(combined)) {
            for (const QString& form : forms) {
                if (next.size() < MAX_COMBINATIONS) {
                    next.append(start.isEmpty() ? form : start + QLatin1Char(' ') + form);
                }
            }
        }
        combined = next;
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
    QStringList models;
    if (m_language != Language::Polish) {
        for (qsizetype index = 0; index < words.size(); ++index) {
            const QString& word = words.at(index);
            const bool regular = m_language == Language::English && index == words.size() - 1 &&
                                 word != word.toUpper() && word.front().isUpper();
            models.append(QString(regular ? REGULAR : UNCHANGED));
        }
        return models;
    }

    const bool feminine = kind == NameKind::Character && isFeminineName(words);
    bool afterPreposition = false;
    for (qsizetype index = 0; index < words.size(); ++index) {
        const QString& word = words.at(index);
        const bool particle = index > 0 && word.front().isLower();
        afterPreposition = afterPreposition || (particle && isPreposition(word));
        if (afterPreposition || particle) {
            models.append(QString(UNCHANGED));  // nad Morzem; van (Beethoven)
            continue;
        }
        models.append(polishModel(word, kind, feminine, index == 0, dictionary));
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
