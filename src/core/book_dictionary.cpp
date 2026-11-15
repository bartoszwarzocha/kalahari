// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_dictionary.cpp
/// @brief The dictionary of a book: its names in all their forms, the author's rules of
///        spelling and the words right only in this book

#include <kalahari/core/book_dictionary.h>

#include <kalahari/core/detail/book_dictionary_internal.h>

#include <kalahari/core/name_declension.h>

#include <algorithm>
#include <functional>
#include <utility>

namespace kalahari::core {

namespace {

using dictionary::beforeApostrophe;
using dictionary::isLowerCase;
using dictionary::smallFirst;
using dictionary::wordTexts;

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
    const auto found = std::ranges::find(m_names, nameId, &DictionaryName::id);
    return found == m_names.end() ? nullptr : &*found;
}

const DictionaryName* BookDictionary::findNameOfCard(const QString& card) const {
    if (card.isEmpty()) {
        return nullptr;
    }
    const auto found = std::ranges::find(m_names, card, &DictionaryName::card);
    return found == m_names.end() ? nullptr : &*found;
}

const DictionaryName* BookDictionary::findNameWithText(const QString& text) const {
    const QString wanted = text.simplified();
    const auto found = std::ranges::find_if(m_names, [&wanted](const DictionaryName& name) {
        return name.text().compare(wanted, Qt::CaseInsensitive) == 0;
    });
    return found == m_names.end() ? nullptr : &*found;
}

const SpellingRule* BookDictionary::findRule(const QString& ruleId) const {
    const auto found = std::ranges::find(m_rules, ruleId, &SpellingRule::id);
    return found == m_rules.end() ? nullptr : &*found;
}

QString BookDictionary::putName(DictionaryName name) {
    if (name.id.isEmpty()) {
        name.id = dictionary::newId();
    }
    const QString nameId = name.id;
    const auto found = std::ranges::find(m_names, nameId, &DictionaryName::id);
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
        rule.id = dictionary::newId();
    }
    const QString ruleId = rule.id;
    const auto found = std::ranges::find(m_rules, ruleId, &SpellingRule::id);
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
        addRuleSpellings(rule);
    }
    // The longest spellings first: "e mail" before "e"
    for (QList<WrongSpelling>& spellings : m_wrong) {
        std::ranges::stable_sort(spellings, std::ranges::greater{},
                                 [](const WrongSpelling& spelling) { return spelling.words.size(); });
    }
}

void BookDictionary::addRuleSpellings(const SpellingRule& rule) {
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
    return dictionary::findDeviations(text, m_wrong);
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
            const bool isForm = std::ranges::any_of(variantForms, [&wanted](const QString& form) {
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
// Making names
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

}  // namespace kalahari::core
