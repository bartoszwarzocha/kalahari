// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file name_declension_polish.cpp
/// @brief The Polish models of declension: the forms each gives a word

#include <kalahari/core/detail/name_declension_internal.h>
#include <kalahari/core/name_declension.h>

#include <QString>
#include <QStringView>

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

namespace kalahari::core::declension {

namespace {

// =============================================================================
// Cells
// =============================================================================

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

// =============================================================================
// The end of a stem
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
// The forms of the models
// =============================================================================

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
            std::ranges::none_of(QStringView(lower).mid(letterE + 1), isVowel);
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

/// The nominative plural of a masculine noun of things by its stem (Gdańsk -> Gdańsk-i,
/// Londyn -> Londyn-y, Kraków -> Krakow-y)
QString thingsNominativePlural(const QString& stem, StemEnd end, bool kOrG) {
    if (end == StemEnd::Hard || stem.toLower().endsWith(QLatin1String("ch"))) {
        return stem + QStringLiteral("y");
    }
    return stem + (kOrG ? QStringLiteral("i") : QStringLiteral("e"));
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
    const QString nominative = thingsNominativePlural(stem, end, kOrG);
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

/// The forms of a stem ending in j: after a vowel without it (Maja -> Mai, Maju), after a
/// consonant with it (Patrycja -> Patrycji)
void jForms(AForms& forms, const QString& stem, const QString& lower) {
    const bool afterVowel = lower.size() >= 2 && isVowel(lower.at(lower.size() - 2));
    forms.genitive = (afterVowel ? stem.chopped(1) : stem) + QStringLiteral("i");
    if (afterVowel) {
        forms.vocative = stem + QStringLiteral("u");
    }
}

/// The forms of a stem ending in l: Ola -> Oli, Olu; but Urszula -> Urszulo
void lForms(AForms& forms, const QString& stem, const QString& lower) {
    forms.genitive = stem + QStringLiteral("i");
    if (lower.size() <= 2) {
        forms.vocative = stem + QStringLiteral("u");
    }
}

/// The forms of a velar stem: Monika -> Moniki, Monice; Bacha -> Bachy, Basze
void velarForms(AForms& forms, const QString& stem, const QString& lower) {
    if (!lower.endsWith(QLatin1String("ch"))) {
        forms.genitive = stem + QStringLiteral("i");
    }
    forms.dative = velarLocative(stem);
}

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
    case StemEnd::J:
        jForms(forms, stem, lower);
        break;
    case StemEnd::L:
        lForms(forms, stem, lower);
        break;
    case StemEnd::Hardened:
        break;
    case StemEnd::Velar:
        velarForms(forms, stem, lower);
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

}  // namespace

// =============================================================================
// The models
// =============================================================================

bool isVowel(QChar character) {
    static const QString vowels = QStringLiteral("aąeęioóuy");
    return vowels.contains(character.toLower());
}

bool endsWithAny(const QString& text, const QStringList& endings) {
    return std::ranges::any_of(endings,
                               [&text](const QString& ending) { return text.endsWith(ending); });
}

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

const QStringList& polishModelIds() {
    static const QStringList ids{
        QStringLiteral("jan"),      QStringLiteral("marek"),    QStringLiteral("kuba"),
        QStringLiteral("kowalski"), QStringLiteral("anna"),     QStringLiteral("kowalska"),
        QStringLiteral("gdansk"),   QStringLiteral("londyn"),   QStringLiteral("gniezno"),
        QStringLiteral("opole"),    QStringLiteral("zakopane"), QStringLiteral("none")};
    return ids;
}

PolishModel polishModelOf(const QString& model) {
    const qsizetype index = polishModelIds().indexOf(model);
    return index < 0 ? PolishModel::None : static_cast<PolishModel>(index);
}

Candidates polishCandidates(const QString& word, PolishModel model, NameKind kind) {
    // Each model's, in the order of PolishModel; None leaves the word as it is
    using Make = Candidates (*)(const QString& word, bool personal);
    static const std::array<Make, POLISH_MODEL_COUNT> makers{
        [](const QString& text, bool) { return masculineNoun(text, true, false, false); },
        [](const QString& text, bool) { return masculineNoun(text, true, false, true); },
        [](const QString& text, bool) { return aNoun(text, true); },
        [](const QString& text, bool personal) { return masculineAdjective(text, personal); },
        [](const QString& text, bool) { return aNoun(text, false); },
        [](const QString& text, bool) { return feminineAdjective(text); },
        [](const QString& text, bool) { return masculineNoun(text, false, false, false); },
        [](const QString& text, bool) { return masculineNoun(text, false, true, false); },
        [](const QString& text, bool) { return neuterNoun(text); },
        [](const QString& text, bool) { return neuterNoun(text); },
        [](const QString& text, bool) { return neuterAdjective(text); }};
    const auto index = static_cast<std::size_t>(model);
    if (index >= makers.size()) {
        return unchangedCandidates(word);
    }
    return makers.at(index)(word, kind == NameKind::Character);
}

bool fitsWord(const QString& word, PolishModel model) {
    // The last letters a model takes, in the order of PolishModel; none: a consonant
    static const std::array<QString, POLISH_MODEL_COUNT> lastLetters{
        QString(),           QString(),          QStringLiteral("ao"), QStringLiteral("iy"),
        QStringLiteral("a"), QStringLiteral("a"), QString(),           QString(),
        QStringLiteral("o"), QStringLiteral("e"), QStringLiteral("e")};
    const auto index = static_cast<std::size_t>(model);
    if (index >= lastLetters.size()) {
        return true;  // None: any word
    }
    const QChar last = word.back().toLower();
    const QString& letters = lastLetters.at(index);
    return letters.isEmpty() ? !isVowel(last) : letters.contains(last);
}

}  // namespace kalahari::core::declension
