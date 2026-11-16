// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file package_json.cpp
/// @brief Reading the JSON of a package of book types: the parts that booktype.json shares

#include "kalahari/core/package_json.h"

#include "kalahari/core/book_type_package.h"

#include <QJsonArray>
#include <QRegularExpression>

#include <algorithm>
#include <initializer_list>

namespace kalahari::core::package_json {

void addProblem(QStringList& problems, const QString& field, const QString& message) {
    problems << field + QStringLiteral(": ") + message;
}

bool matches(const QString& text, const char* pattern) {
    const QRegularExpression expression(QString::fromLatin1(pattern));
    return expression.match(text).hasMatch();
}

bool isIdentifier(const QString& text) {
    return matches(text, "^[a-z][a-z0-9_]*$");
}

bool isLanguageCode(const QString& text) {
    return matches(text, "^[a-z]{2,3}(_[A-Z]{2})?$");
}

void checkKeys(const QJsonObject& object, const QString& field,
               std::initializer_list<const char*> known, QStringList& problems) {
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        const QString key = it.key();
        const bool isKnown = std::any_of(known.begin(), known.end(), [&key](const char* name) {
            return key == QLatin1String(name);
        });
        if (!isKnown) {
            addProblem(problems, field, QStringLiteral("unknown key '%1'").arg(key));
        }
    }
}

LocalizedText readText(const QJsonValue& value, const QString& field, bool required,
                       QStringList& problems) {
    LocalizedText text;
    if (value.isUndefined()) {
        if (required) {
            addProblem(problems, field, QStringLiteral("is missing"));
        }
        return text;
    }
    if (!value.isObject()) {
        addProblem(problems, field,
                   QStringLiteral("must be texts by language, e.g. "
                                  "{\"en\": \"...\", \"pl\": \"...\"}"));
        return text;
    }
    const QJsonObject object = value.toObject();
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        if (!isLanguageCode(it.key())) {
            addProblem(problems, field,
                       QStringLiteral("'%1' is not a language code such as \"en\" or \"pt_BR\"")
                           .arg(it.key()));
        } else if (!it.value().isString() || it.value().toString().trimmed().isEmpty()) {
            addProblem(problems, field + QLatin1Char('.') + it.key(),
                       QStringLiteral("must be a text that is not empty"));
        } else {
            text.values.insert(it.key(), it.value().toString());
        }
    }
    if (!object.contains(QStringLiteral("en"))) {
        addProblem(problems, field,
                   QStringLiteral("the English text (\"en\") is missing; it is shown in "
                                  "languages the package does not have"));
    }
    return text;
}

QStringList readIdList(const QJsonValue& value, const QString& field,
                       bool (*isValid)(const QString&), const QString& what, bool unique,
                       QStringList& problems) {
    QStringList list;
    if (value.isUndefined()) {
        return list;
    }
    if (!value.isArray()) {
        addProblem(problems, field, QStringLiteral("must be a list of %1s").arg(what));
        return list;
    }
    const QJsonArray array = value.toArray();
    for (const auto& item : array) {
        const QString id = item.toString();
        if (!item.isString() || !isValid(id)) {
            addProblem(problems, field, QStringLiteral("'%1' is not a %2").arg(id, what));
        } else if (unique && list.contains(id)) {
            addProblem(problems, field, QStringLiteral("'%1' is listed twice").arg(id));
        } else {
            list << id;
        }
    }
    return list;
}

}  // namespace kalahari::core::package_json
