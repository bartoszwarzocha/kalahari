// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_dictionary.cpp
/// @brief The dictionary of a book: its names in all their forms, the author's rules of
///        spelling and the words right only in this book

#include <kalahari/core/book_dictionary.h>

#include <kalahari/core/affix_expander.h>
#include <kalahari/core/name_declension.h>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
#include <QUuid>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <iterator>
#include <utility>

namespace kalahari::core {

namespace {

// =============================================================================
// Words
// =============================================================================

/// A word of a text: where it starts and how long it is
struct TextWord {
    qsizetype start = 0;   ///< Where it starts
    qsizetype length = 0;  ///< How long it is
};

/// An apostrophe or a hyphen: joins the letters around it into one word (e-mail, Anna's)
bool isJoiner(QChar character) {
    return character == QLatin1Char('-') || character == QLatin1Char('\'') ||
           character == QChar(u'‐') || character == QChar(u'‑') || character == QChar(u'’');
}

/// Whether @p character belongs to a word: a letter or an accent
bool isWordCharacter(QChar character) {
    return character.isLetter() || character.isMark();
}

/// The words of @p text: letters, also joined by apostrophes and hyphens
QList<TextWord> wordsOf(const QString& text) {
    QList<TextWord> words;
    qsizetype index = 0;
    while (index < text.size()) {
        if (!text.at(index).isLetter()) {
            ++index;
            continue;
        }
        qsizetype end = index;
        while (end < text.size() &&
               (isWordCharacter(text.at(end)) ||
                (isJoiner(text.at(end)) && end + 1 < text.size() && text.at(end + 1).isLetter()))) {
            ++end;
        }
        words.append(TextWord{.start = index, .length = end - index});
        index = end;
    }
    return words;
}

/// The words of a form or a spelling (split as wordsOf() splits a text)
QStringList wordTexts(const QString& text) {
    QStringList texts;
    for (const TextWord& word : wordsOf(text)) {
        texts.append(text.mid(word.start, word.length));
    }
    return texts;
}

/// @p text with a capital first letter
QString capitalFirst(const QString& text) {
    return text.isEmpty() ? text : text.front().toUpper() + text.mid(1);
}

/// @p text with a small first letter
QString smallFirst(const QString& text) {
    return text.isEmpty() ? text : text.front().toLower() + text.mid(1);
}

/// Whether @p text is in lower case
bool isLowerCase(const QString& text) {
    return text == text.toLower();
}

/// The word before an apostrophe in it (Anna's -> Anna), or empty
QString beforeApostrophe(const QString& word) {
    for (qsizetype index = 1; index < word.size(); ++index) {
        if (word.at(index) == QLatin1Char('\'') || word.at(index) == QChar(u'’')) {
            return word.left(index);
        }
    }
    return {};
}

/// Opening quotes, brackets and dashes: what can come before the first word of a sentence
bool opensSentence(QChar character) {
    static const QString opening = QStringLiteral("\"'([{„“«‹–—-");
    return character.isSpace() || opening.contains(character);
}

/// Closing quotes and brackets: what can come after the end of a sentence
bool closesSentence(QChar character) {
    static const QString closing = QStringLiteral("\"')]}”’»›");
    return closing.contains(character);
}

/// Whether the word at @p start starts a sentence: the paragraph starts there (after quotes
/// or a dash of a dialogue), or the text before it ends a sentence
bool startsSentence(const QString& text, qsizetype start) {
    qsizetype index = start - 1;
    while (index >= 0 && opensSentence(text.at(index))) {
        --index;
    }
    if (index < 0) {
        return true;
    }
    while (index >= 0 && closesSentence(text.at(index))) {
        --index;
    }
    static const QString ends = QStringLiteral(".!?…");
    return index >= 0 && ends.contains(text.at(index));
}

// =============================================================================
// Deviations from the rules
// =============================================================================

/// Finds the words of a paragraph that go against the rules
class DeviationFinder {
public:
    DeviationFinder(const QString& text, const QHash<QString, QList<WrongSpelling>>& wrong)
        : m_text(text), m_wrong(wrong), m_words(wordsOf(text)) {}

    /// The deviations, in their order
    [[nodiscard]] QList<RuleDeviation> find() const {
        QList<RuleDeviation> found;
        qsizetype index = 0;
        while (index < m_words.size()) {
            index += std::max<qsizetype>(matchAt(index, found), 1);
        }
        return found;
    }

private:
    /// The word @p index of the text
    [[nodiscard]] QString wordAt(qsizetype index) const {
        const TextWord& word = m_words.at(index);
        return m_text.mid(word.start, word.length);
    }

    /// Whether only spaces are between word @p index and the one before it
    [[nodiscard]] bool spacesBefore(qsizetype index) const {
        const TextWord& previous = m_words.at(index - 1);
        const qsizetype gapStart = previous.start + previous.length;
        const QString gap = m_text.mid(gapStart, m_words.at(index).start - gapStart);
        return !gap.isEmpty() && gap.trimmed().isEmpty();
    }

    /// Whether the words of @p spelling are in the text from word @p first on, one space
    /// apart; with @p capitalized, its first word with a capital letter
    [[nodiscard]] bool matches(qsizetype first, const WrongSpelling& spelling,
                               bool capitalized) const {
        if (first + spelling.words.size() > m_words.size()) {
            return false;
        }
        for (qsizetype index = 0; index < spelling.words.size(); ++index) {
            const QString wanted = index == 0 && capitalized ? capitalFirst(spelling.words.front())
                                                             : spelling.words.at(index);
            if (wordAt(first + index) != wanted || (index > 0 && !spacesBefore(first + index))) {
                return false;
            }
        }
        return true;
    }

    /// Add the deviation that starts at word @p index, if any
    /// @return How many words the wrong spelling found there takes (0: none is there)
    qsizetype matchAt(qsizetype index, QList<RuleDeviation>& found) const {
        const QString word = wordAt(index);
        const bool sentenceStart = startsSentence(m_text, m_words.at(index).start);
        bool capitalized = false;
        auto spellings = m_wrong.constFind(word);
        if (spellings == m_wrong.constEnd() && sentenceStart && word.front().isUpper()) {
            spellings = m_wrong.constFind(smallFirst(word));
            capitalized = true;
        }
        if (spellings == m_wrong.constEnd()) {
            return 0;
        }
        const auto spelling =
            std::find_if(spellings->begin(), spellings->end(),
                         [this, index, capitalized](const WrongSpelling& candidate) {
                             return matches(index, candidate, capitalized);
                         });
        if (spelling == spellings->end()) {
            return 0;
        }
        const TextWord& last = m_words.at(index + spelling->words.size() - 1);
        const qsizetype start = m_words.at(index).start;
        const qsizetype length = last.start + last.length - start;
        const QString written = m_text.mid(start, length);
        const QString wanted = capitalized ? capitalFirst(spelling->right) : spelling->right;
        // A capital letter at the start of a sentence is right: "Okej" for "okej"
        const bool startOfRight = sentenceStart && written == capitalFirst(spelling->right);
        if (written != wanted && !startOfRight) {
            found.append(RuleDeviation{.start = start,
                                       .length = length,
                                       .text = written,
                                       .right = wanted,
                                       .ruleId = spelling->ruleId});
        }
        return spelling->words.size();
    }

    QString m_text;                                ///< The paragraph
    QHash<QString, QList<WrongSpelling>> m_wrong;  ///< First word -> wrong spellings
    QList<TextWord> m_words;                       ///< The words of the paragraph
};

// =============================================================================
// File
// =============================================================================

/// The names of the kinds of names in the file
const std::array<QString, 4>& kindNames() {
    static const std::array<QString, 4> names{QStringLiteral("character"), QStringLiteral("place"),
                                              QStringLiteral("item"), QStringLiteral("other")};
    return names;
}

/// The name of a kind in the file
QString kindName(NameKind kind) {
    return kindNames().at(static_cast<std::size_t>(kind));
}

/// The kind of a name in the file (a character when it is none)
NameKind kindOfName(const QString& name) {
    const auto& names = kindNames();
    const auto* const found = std::find(names.begin(), names.end(), name);
    return found == names.end() ? NameKind::Character
                                : static_cast<NameKind>(std::distance(names.begin(), found));
}

/// The texts of a JSON array
QStringList stringsOf(const QJsonValue& value) {
    QStringList strings;
    for (const auto& item : value.toArray()) {
        if (item.isString()) {
            strings.append(item.toString());
        }
    }
    return strings;
}

/// A form table as the file has it: the cells with forms
QJsonObject formsToJson(const FormTable& forms) {
    QJsonObject json;
    for (const GrammaticalCase grammaticalCase : FormTable::allCases()) {
        for (const bool plural : {false, true}) {
            const QStringList& cell = forms.cell(grammaticalCase, plural);
            if (!cell.isEmpty()) {
                json.insert(FormTable::cellKey(grammaticalCase, plural),
                            QJsonArray::fromStringList(cell));
            }
        }
    }
    return json;
}

/// A form table from the file
FormTable formsFromJson(const QJsonObject& json) {
    FormTable forms;
    for (auto cell = json.constBegin(); cell != json.constEnd(); ++cell) {
        GrammaticalCase grammaticalCase = GrammaticalCase::Nominative;
        bool plural = false;
        if (FormTable::cellOfKey(cell.key(), grammaticalCase, plural)) {
            forms.cell(grammaticalCase, plural) = stringsOf(cell.value());
        }
    }
    return forms;
}

QJsonObject variantToJson(const NameVariant& variant) {
    QJsonObject json{{QStringLiteral("text"), variant.text},
                     {QStringLiteral("models"), QJsonArray::fromStringList(variant.models)},
                     {QStringLiteral("forms"), formsToJson(variant.forms)}};
    if (!variant.edited.isEmpty()) {
        json.insert(QStringLiteral("edited"), QJsonArray::fromStringList(variant.edited));
    }
    if (variant.automatic) {
        json.insert(QStringLiteral("automatic"), true);
    }
    return json;
}

NameVariant variantFromJson(const QJsonObject& json) {
    return NameVariant{.text = json.value(QStringLiteral("text")).toString(),
                       .models = stringsOf(json.value(QStringLiteral("models"))),
                       .forms = formsFromJson(json.value(QStringLiteral("forms")).toObject()),
                       .edited = stringsOf(json.value(QStringLiteral("edited"))),
                       .automatic = json.value(QStringLiteral("automatic")).toBool(false)};
}

QJsonObject nameToJson(const DictionaryName& name) {
    QJsonArray variants;
    for (const NameVariant& variant : name.variants) {
        variants.append(variantToJson(variant));
    }
    QJsonObject json{{QStringLiteral("id"), name.id},
                     {QStringLiteral("kind"), kindName(name.kind)},
                     {QStringLiteral("variants"), variants}};
    if (!name.card.isEmpty()) {
        json.insert(QStringLiteral("card"), name.card);
    }
    if (name.plural) {
        json.insert(QStringLiteral("plural"), true);
    }
    if (!name.quickInsert) {
        json.insert(QStringLiteral("quickInsert"), false);
    }
    return json;
}

DictionaryName nameFromJson(const QJsonObject& json) {
    DictionaryName name{.id = json.value(QStringLiteral("id")).toString(),
                        .kind = kindOfName(json.value(QStringLiteral("kind")).toString()),
                        .card = json.value(QStringLiteral("card")).toString(),
                        .plural = json.value(QStringLiteral("plural")).toBool(false),
                        .quickInsert = json.value(QStringLiteral("quickInsert")).toBool(true),
                        .variants = {}};
    for (const auto& variant : json.value(QStringLiteral("variants")).toArray()) {
        NameVariant read = variantFromJson(variant.toObject());
        if (!read.text.isEmpty()) {
            name.variants.append(std::move(read));
        }
    }
    return name;
}

QJsonObject ruleToJson(const SpellingRule& rule) {
    QJsonObject forms;
    for (auto form = rule.forms.constBegin(); form != rule.forms.constEnd(); ++form) {
        forms.insert(form.key(), form.value());
    }
    QJsonObject json{{QStringLiteral("id"), rule.id},
                     {QStringLiteral("right"), rule.right},
                     {QStringLiteral("wrong"), QJsonArray::fromStringList(rule.wrong)},
                     {QStringLiteral("forms"), forms}};
    if (!rule.allForms) {
        json.insert(QStringLiteral("allForms"), false);
    }
    if (!rule.note.isEmpty()) {
        json.insert(QStringLiteral("note"), rule.note);
    }
    if (!rule.pattern.isEmpty()) {
        json.insert(QStringLiteral("pattern"), rule.pattern);
    }
    return json;
}

SpellingRule ruleFromJson(const QJsonObject& json) {
    SpellingRule rule{.id = json.value(QStringLiteral("id")).toString(),
                      .right = json.value(QStringLiteral("right")).toString(),
                      .wrong = stringsOf(json.value(QStringLiteral("wrong"))),
                      .allForms = json.value(QStringLiteral("allForms")).toBool(true),
                      .note = json.value(QStringLiteral("note")).toString(),
                      .pattern = json.value(QStringLiteral("pattern")).toString(),
                      .forms = {}};
    const QJsonObject forms = json.value(QStringLiteral("forms")).toObject();
    for (auto form = forms.constBegin(); form != forms.constEnd(); ++form) {
        rule.forms.insert(form.key(), form.value().toString());
    }
    return rule;
}

/// A new id of a name or a rule
QString newId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

// =============================================================================
// Rule forms
// =============================================================================

/// The shortest part of a word that splitting it into two words or finding its pattern takes
constexpr qsizetype MIN_PART = 3;

/// How a form is made of its word: so many characters taken from its end, and an ending
struct FormChange {
    qsizetype dropped = 0;  ///< Characters taken from the end of the word
    QString ending;         ///< Put in their place
    QString droppedText;    ///< The characters taken, in lower case
};

/// How @p form is made of @p word (smok -> smoka: nothing dropped, "a" added)
FormChange changeOf(const QString& word, const QString& form) {
    const QString lowerWord = word.toLower();
    const QString lowerForm = form.toLower();
    qsizetype common = 0;
    while (common < lowerWord.size() && common < lowerForm.size() &&
           lowerWord.at(common) == lowerForm.at(common)) {
        ++common;
    }
    return FormChange{.dropped = word.size() - common,
                      .ending = form.mid(common),
                      .droppedText = lowerWord.mid(common)};
}

/// @p word changed as a form is made of its word; empty when its end is not the same
QString applyChange(const QString& word, const FormChange& change) {
    if (word.size() < change.dropped || !word.toLower().endsWith(change.droppedText)) {
        return {};
    }
    return word.chopped(change.dropped) + change.ending;
}

/// The words the forms of a rule can come from, in the order they are tried: its right and
/// wrong spellings (also with a small first letter), then the words its right spelling ends
/// with, the longest first, as the pattern
std::pair<QStringList, QStringList> baseCandidates(const SpellingRule& rule) {
    QStringList own{rule.right, smallFirst(rule.right)};
    for (const QString& wrong : rule.wrong) {
        own.append(wrong);
        own.append(smallFirst(wrong));
    }
    own.removeDuplicates();
    QStringList patterns;
    const QString& right = rule.right;
    for (qsizetype start = 1; start + MIN_PART <= right.size(); ++start) {
        const QString tail = right.mid(start);
        if (tail.front().isLetter() && std::all_of(tail.begin(), tail.end(), isWordCharacter)) {
            patterns.append(tail);
        }
    }
    return {own, patterns};
}

}  // namespace

// =============================================================================
// DictionaryName
// =============================================================================

QString DictionaryName::text() const {
    return variants.isEmpty() ? QString() : variants.front().text;
}

QStringList DictionaryName::allForms() const {
    QStringList forms;
    for (const NameVariant& variant : variants) {
        for (const QString& form : variant.forms.allForms(plural)) {
            if (!forms.contains(form)) {
                forms.append(form);
            }
        }
    }
    return forms;
}

// =============================================================================
// BookDictionary: content
// =============================================================================

QString BookDictionary::fileName() {
    return QStringLiteral("dictionary.json");
}

const QList<DictionaryName>& BookDictionary::names() const {
    return m_names;
}

const QList<SpellingRule>& BookDictionary::rules() const {
    return m_rules;
}

const QStringList& BookDictionary::words() const {
    return m_words;
}

bool BookDictionary::isEmpty() const {
    return m_names.isEmpty() && m_rules.isEmpty() && m_words.isEmpty();
}

const DictionaryName* BookDictionary::findName(const QString& nameId) const {
    const auto found =
        std::find_if(m_names.begin(), m_names.end(),
                     [&nameId](const DictionaryName& name) { return name.id == nameId; });
    return found == m_names.end() ? nullptr : &*found;
}

const DictionaryName* BookDictionary::findNameOfCard(const QString& card) const {
    if (card.isEmpty()) {
        return nullptr;
    }
    const auto found =
        std::find_if(m_names.begin(), m_names.end(),
                     [&card](const DictionaryName& name) { return name.card == card; });
    return found == m_names.end() ? nullptr : &*found;
}

const DictionaryName* BookDictionary::findNameWithText(const QString& text) const {
    const QString wanted = text.simplified();
    const auto found =
        std::find_if(m_names.begin(), m_names.end(), [&wanted](const DictionaryName& name) {
            return name.text().compare(wanted, Qt::CaseInsensitive) == 0;
        });
    return found == m_names.end() ? nullptr : &*found;
}

const SpellingRule* BookDictionary::findRule(const QString& ruleId) const {
    const auto found =
        std::find_if(m_rules.begin(), m_rules.end(),
                     [&ruleId](const SpellingRule& rule) { return rule.id == ruleId; });
    return found == m_rules.end() ? nullptr : &*found;
}

QString BookDictionary::putName(DictionaryName name) {
    if (name.id.isEmpty()) {
        name.id = newId();
    }
    const QString nameId = name.id;
    const auto found =
        std::find_if(m_names.begin(), m_names.end(),
                     [&nameId](const DictionaryName& known) { return known.id == nameId; });
    if (found == m_names.end()) {
        m_names.append(std::move(name));
    } else {
        *found = std::move(name);
    }
    rebuild();
    return nameId;
}

bool BookDictionary::removeName(const QString& nameId) {
    const qsizetype removed =
        m_names.removeIf([&nameId](const DictionaryName& name) { return name.id == nameId; });
    if (removed == 0) {
        return false;
    }
    rebuild();
    return true;
}

QString BookDictionary::putRule(SpellingRule rule) {
    if (rule.id.isEmpty()) {
        rule.id = newId();
    }
    const QString ruleId = rule.id;
    const auto found =
        std::find_if(m_rules.begin(), m_rules.end(),
                     [&ruleId](const SpellingRule& known) { return known.id == ruleId; });
    if (found == m_rules.end()) {
        m_rules.append(std::move(rule));
    } else {
        *found = std::move(rule);
    }
    rebuild();
    return ruleId;
}

bool BookDictionary::removeRule(const QString& ruleId) {
    const qsizetype removed =
        m_rules.removeIf([&ruleId](const SpellingRule& rule) { return rule.id == ruleId; });
    if (removed == 0) {
        return false;
    }
    rebuild();
    return true;
}

bool BookDictionary::addWord(const QString& word) {
    const QString added = word.trimmed();
    if (added.isEmpty() || m_words.contains(added)) {
        return false;
    }
    m_words.append(added);
    m_words.sort(Qt::CaseInsensitive);
    rebuild();
    return true;
}

bool BookDictionary::removeWord(const QString& word) {
    if (!m_words.removeOne(word)) {
        return false;
    }
    rebuild();
    return true;
}

bool BookDictionary::hasWord(const QString& word) const {
    return m_words.contains(word);
}

void BookDictionary::clear() {
    m_names.clear();
    m_rules.clear();
    m_words.clear();
    rebuild();
}

// =============================================================================
// BookDictionary: file
// =============================================================================

bool BookDictionary::load(const QString& path, QString* error) {
    clear();
    QFile file(path);
    if (!file.exists()) {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("%1: %2").arg(path, file.errorString());
        }
        return false;
    }
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!document.isObject()) {
        if (error != nullptr) {
            *error = QStringLiteral("%1: %2").arg(path, parseError.errorString());
        }
        return false;
    }
    return fromJson(document.object(), error);
}

bool BookDictionary::save(const QString& path, QString* error) const {
    if (isEmpty() && !QFile::exists(path)) {
        return true;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("%1: %2").arg(path, file.errorString());
        }
        return false;
    }
    file.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error != nullptr) {
            *error = QStringLiteral("%1: %2").arg(path, file.errorString());
        }
        return false;
    }
    return true;
}

QJsonObject BookDictionary::toJson() const {
    QJsonArray names;
    for (const DictionaryName& name : m_names) {
        names.append(nameToJson(name));
    }
    QJsonArray rules;
    for (const SpellingRule& rule : m_rules) {
        rules.append(ruleToJson(rule));
    }
    return QJsonObject{{QStringLiteral("format"), FORMAT},
                       {QStringLiteral("names"), names},
                       {QStringLiteral("rules"), rules},
                       {QStringLiteral("words"), QJsonArray::fromStringList(m_words)}};
}

bool BookDictionary::fromJson(const QJsonObject& json, QString* error) {
    m_names.clear();
    m_rules.clear();
    m_words.clear();
    const int format = json.value(QStringLiteral("format")).toInt(FORMAT);
    if (format > FORMAT) {
        if (error != nullptr) {
            *error = QStringLiteral(
                         "the dictionary has format %1, this version of Kalahari reads "
                         "format %2")
                         .arg(format)
                         .arg(FORMAT);
        }
        rebuild();
        return false;
    }
    for (const auto& value : json.value(QStringLiteral("names")).toArray()) {
        DictionaryName name = nameFromJson(value.toObject());
        if (!name.variants.isEmpty()) {
            if (name.id.isEmpty()) {
                name.id = newId();
            }
            m_names.append(std::move(name));
        }
    }
    for (const auto& value : json.value(QStringLiteral("rules")).toArray()) {
        SpellingRule rule = ruleFromJson(value.toObject());
        if (!rule.right.isEmpty()) {
            if (rule.id.isEmpty()) {
                rule.id = newId();
            }
            m_rules.append(std::move(rule));
        }
    }
    for (const QString& word : stringsOf(json.value(QStringLiteral("words")))) {
        if (!word.isEmpty() && !m_words.contains(word)) {
            m_words.append(word);
        }
    }
    m_words.sort(Qt::CaseInsensitive);
    rebuild();
    return true;
}

// =============================================================================
// BookDictionary: checking
// =============================================================================

void BookDictionary::addNameWords(const QString& form) {
    for (const QString& word : wordTexts(form)) {
        m_nameWords.insert(word);
        m_nameWordCase.insert(word.toLower(), word);
        // A hyphenated word is checked by its parts too (Curie-Skłodowska)
        for (const QString& part : word.split(QLatin1Char('-'), Qt::SkipEmptyParts)) {
            m_nameWords.insert(part);
        }
    }
}

void BookDictionary::rebuild() {
    m_nameWords.clear();
    m_nameWordCase.clear();
    m_rightWords.clear();
    m_bookWords = QSet<QString>(m_words.begin(), m_words.end());
    m_wrong.clear();

    for (const DictionaryName& name : std::as_const(m_names)) {
        for (const NameVariant& variant : name.variants) {
            addNameWords(variant.text);  // also without forms (in a language without models)
        }
        for (const QString& form : name.allForms()) {
            addNameWords(form);
        }
    }
    for (const SpellingRule& rule : std::as_const(m_rules)) {
        QMap<QString, QString> forms = rule.forms;
        for (const QString& wrong : rule.wrong) {
            if (!forms.contains(wrong) && wrong != rule.right) {
                forms.insert(wrong, rule.right);
            }
        }
        for (const QString& word : wordTexts(rule.right)) {
            m_rightWords.insert(word);
        }
        for (auto form = forms.constBegin(); form != forms.constEnd(); ++form) {
            for (const QString& word : wordTexts(form.value())) {
                m_rightWords.insert(word);
            }
            const QStringList words = wordTexts(form.key());
            if (!words.isEmpty() && form.key() != form.value()) {
                m_wrong[words.front()].append(WrongSpelling{
                    .words = words, .text = form.key(), .right = form.value(), .ruleId = rule.id});
            }
        }
    }
    // The longest spellings first: "e mail" before "e"
    for (QList<WrongSpelling>& spellings : m_wrong) {
        std::stable_sort(spellings.begin(), spellings.end(),
                         [](const WrongSpelling& first, const WrongSpelling& second) {
                             return first.words.size() > second.words.size();
                         });
    }
}

bool BookDictionary::isRightWord(const QString& word) const {
    if (m_nameWords.contains(word) || m_bookWords.contains(word) ||
        m_bookWords.contains(word.toLower()) || m_rightWords.contains(word)) {
        return true;
    }
    // A right spelling in lower case at the start of a sentence (Półelf)
    const QString small = smallFirst(word);
    return small != word && isLowerCase(small) && m_rightWords.contains(small);
}

bool BookDictionary::isRight(const QString& word) const {
    if (word.isEmpty()) {
        return false;
    }
    const QString stem = beforeApostrophe(word);
    return isRightWord(word) || (!stem.isEmpty() && isRightWord(stem));
}

QString BookDictionary::nameSpelling(const QString& word) const {
    if (m_nameWords.contains(word)) {
        return {};
    }
    return m_nameWordCase.value(word.toLower());
}

QList<RuleDeviation> BookDictionary::deviations(const QString& text) const {
    if (m_wrong.isEmpty()) {
        return {};
    }
    return DeviationFinder(text, m_wrong).find();
}

// =============================================================================
// BookDictionary: search
// =============================================================================

QStringList BookDictionary::formsOf(const QString& text) const {
    const QString wanted = text.simplified();
    QStringList forms;
    if (wanted.isEmpty()) {
        return forms;
    }
    for (const DictionaryName& name : m_names) {
        for (const NameVariant& variant : name.variants) {
            const QStringList variantForms = variant.forms.allForms(name.plural);
            const bool isForm = std::any_of(
                variantForms.begin(), variantForms.end(), [&wanted](const QString& form) {
                    return form.compare(wanted, Qt::CaseInsensitive) == 0;
                });
            if (!isForm) {
                continue;
            }
            for (const QString& form : variantForms) {
                if (!forms.contains(form)) {
                    forms.append(form);
                }
            }
        }
    }
    return forms;
}

// =============================================================================
// Making names and rules
// =============================================================================

void refreshForms(NameVariant& variant, NameKind kind, const NameDeclension& declension,
                  const NameDeclension::DictionaryForms& dictionary) {
    if (variant.models.size() != NameDeclension::wordsOf(variant.text).size()) {
        variant.models = declension.modelsFor(variant.text, kind, dictionary);
    }
    const FormTable generated =
        declension.nameForms(variant.text, variant.models, kind, dictionary);
    for (const GrammaticalCase grammaticalCase : FormTable::allCases()) {
        for (const bool plural : {false, true}) {
            if (!variant.edited.contains(FormTable::cellKey(grammaticalCase, plural))) {
                variant.forms.cell(grammaticalCase, plural) =
                    generated.cell(grammaticalCase, plural);
            }
        }
    }
}

DictionaryName makeName(const QString& text, NameKind kind, const NameDeclension& declension,
                        const NameDeclension::DictionaryForms& dictionary) {
    DictionaryName name;
    name.kind = kind;
    NameVariant main{
        .text = text.simplified(), .models = {}, .forms = {}, .edited = {}, .automatic = false};
    if (main.text.isEmpty()) {
        return name;
    }
    main.models = declension.modelsFor(main.text, kind, dictionary);
    refreshForms(main, kind, declension, dictionary);
    name.variants.append(main);

    // A character's first name and surname alone, with their models in the name
    const QStringList words = NameDeclension::wordsOf(main.text);
    if (kind != NameKind::Character || words.size() < 2) {
        return name;
    }
    for (const qsizetype index : {qsizetype{0}, words.size() - 1}) {
        NameVariant alone{.text = words.at(index),
                          .models = QStringList{main.models.at(index)},
                          .forms = {},
                          .edited = {},
                          .automatic = true};
        refreshForms(alone, kind, declension, dictionary);
        name.variants.append(std::move(alone));
    }
    return name;
}

QStringList suggestedWrongSpellings(const QString& right,
                                    const std::function<bool(const QString&)>& isWord) {
    QStringList spellings;
    const QString text = right.simplified();
    if (text.isEmpty()) {
        return spellings;
    }
    const QString small = smallFirst(text);
    if (small != text && text != text.toUpper()) {
        spellings.append(small);  // Smok: smok
    }
    if (text.contains(QLatin1Char('-'))) {
        spellings.append(QString(text).remove(QLatin1Char('-')));  // e-mail: email
        spellings.append(QString(text).replace(QLatin1Char('-'), QLatin1Char(' ')));
    } else if (text.contains(QLatin1Char(' '))) {
        spellings.append(QString(text).remove(QLatin1Char(' ')));  // na pewno: napewno
        spellings.append(QString(text).replace(QLatin1Char(' '), QLatin1Char('-')));
    } else if (isWord) {
        // A word made of two words: półelf -> pół-elf, pół elf
        for (qsizetype split = MIN_PART; split + MIN_PART <= text.size(); ++split) {
            const QString first = text.left(split);
            const QString second = text.mid(split);
            if (isWord(first) && isWord(second)) {
                spellings.append(first + QLatin1Char('-') + second);
                spellings.append(first + QLatin1Char(' ') + second);
                break;
            }
        }
    }
    spellings.removeDuplicates();
    spellings.removeAll(text);
    return spellings;
}

void fillRuleForms(SpellingRule& rule, const AffixExpander* dictionary) {
    rule.forms.clear();
    rule.pattern.clear();
    for (const QString& wrong : std::as_const(rule.wrong)) {
        if (!wrong.isEmpty() && wrong != rule.right) {
            rule.forms.insert(wrong, rule.right);
        }
    }
    if (!rule.allForms || dictionary == nullptr || !dictionary->isLoaded() ||
        rule.right.isEmpty()) {
        return;
    }

    const auto [own, patterns] = baseCandidates(rule);
    QString base;
    for (const QString& candidate : own) {
        if (dictionary->hasEntry(candidate)) {
            base = candidate;
            break;
        }
    }
    for (qsizetype index = 0; base.isEmpty() && index < patterns.size(); ++index) {
        if (dictionary->hasEntry(patterns.at(index))) {
            base = patterns.at(index);
            rule.pattern = base;
        }
    }
    if (base.isEmpty()) {
        return;
    }

    for (const QString& form : dictionary->formsOf(base)) {
        const FormChange change = changeOf(base, form);
        const QString rightForm = applyChange(rule.right, change);
        if (rightForm.isEmpty()) {
            continue;
        }
        for (const QString& wrong : std::as_const(rule.wrong)) {
            const QString wrongForm = applyChange(wrong, change);
            if (!wrongForm.isEmpty() && wrongForm != rightForm) {
                rule.forms.insert(wrongForm, rightForm);
            }
        }
    }
}

}  // namespace kalahari::core
