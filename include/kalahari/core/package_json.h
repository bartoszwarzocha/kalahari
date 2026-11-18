// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file package_json.h
/// @brief Reading the JSON of a package of book types: what each part of booktype.json shares –
/// the problems found, the keys, the ids and the texts by language

#ifndef KALAHARI_CORE_PACKAGE_JSON_H
#define KALAHARI_CORE_PACKAGE_JSON_H

#include "kalahari/core/book_type_package.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

#include <initializer_list>

namespace kalahari::core::package_json {

/// @brief Add the problem @p message of field @p field ("kinds.chapter.name") to @p problems
void addProblem(QStringList& problems, const QString& field, const QString& message);

/// @brief Whether @p text matches the regular expression @p pattern
[[nodiscard]] bool matches(const QString& text, const char* pattern);

/// @brief Whether @p text is an id of a kind, a style, a window or a field: "chapter",
/// "heading_1", lowercase letters, digits and _
[[nodiscard]] bool isIdentifier(const QString& text);

/// @brief Whether @p text is a language code: "en", "pl", "pt_BR"
[[nodiscard]] bool isLanguageCode(const QString& text);

/// @brief Whether @p value is a number without a fraction: 3, not 3.5 or "3"
[[nodiscard]] bool isWholeNumber(const QJsonValue& value);

/// @brief Report the keys of @p object that are not in @p known: a misspelt key would be
/// ignored
void checkKeys(const QJsonObject& object, const QString& field,
               std::initializer_list<const char*> known, QStringList& problems);

/// @brief The texts by language of @p value, {"en": "...", "pl": "..."}, with the English one;
/// a @p required one that is missing is a problem
[[nodiscard]] LocalizedText readText(const QJsonValue& value, const QString& field,
                                     bool required, QStringList& problems);

/// @brief The list of ids of @p value, each one @p isValid (@p what names one); @p unique
/// reports an id listed twice
[[nodiscard]] QStringList readIdList(const QJsonValue& value, const QString& field,
                                     bool (*isValid)(const QString&), const QString& what,
                                     bool unique, QStringList& problems);

}  // namespace kalahari::core::package_json

#endif  // KALAHARI_CORE_PACKAGE_JSON_H
