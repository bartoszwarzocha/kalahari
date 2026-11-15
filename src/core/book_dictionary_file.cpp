// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file book_dictionary_file.cpp
/// @brief The book dictionary's file: its content in JSON

#include <kalahari/core/book_dictionary.h>

#include "book_dictionary_internal.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>

#include <array>
#include <cstddef>
#include <utility>

namespace kalahari::core {

namespace {

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
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (names.at(i) == name) {
            return static_cast<NameKind>(i);
        }
    }
    return NameKind::Character;
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

/// The names of the file, each with an id; a name without a text is left out
QList<DictionaryName> namesFromJson(const QJsonArray& values) {
    QList<DictionaryName> names;
    for (const auto& value : values) {
        DictionaryName name = nameFromJson(value.toObject());
        if (!name.variants.isEmpty()) {
            if (name.id.isEmpty()) {
                name.id = dictionary::newId();
            }
            names.append(std::move(name));
        }
    }
    return names;
}

/// The rules of the file, each with an id; a rule without a right spelling is left out
QList<SpellingRule> rulesFromJson(const QJsonArray& values) {
    QList<SpellingRule> rules;
    for (const auto& value : values) {
        SpellingRule rule = ruleFromJson(value.toObject());
        if (!rule.right.isEmpty()) {
            if (rule.id.isEmpty()) {
                rule.id = dictionary::newId();
            }
            rules.append(std::move(rule));
        }
    }
    return rules;
}

/// The words of the book in the file, each once, sorted
QStringList wordsFromJson(const QJsonValue& value) {
    QStringList words;
    for (const QString& word : stringsOf(value)) {
        if (!word.isEmpty() && !words.contains(word)) {
            words.append(word);
        }
    }
    words.sort(Qt::CaseInsensitive);
    return words;
}

}  // namespace

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
    m_names = namesFromJson(json.value(QStringLiteral("names")).toArray());
    m_rules = rulesFromJson(json.value(QStringLiteral("rules")).toArray());
    m_words = wordsFromJson(json.value(QStringLiteral("words")));
    rebuild();
    return true;
}

}  // namespace kalahari::core
