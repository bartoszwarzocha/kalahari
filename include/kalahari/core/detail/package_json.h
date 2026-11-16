// SPDX-License-Identifier: MIT
// Copyright (c) 2025 Bartosz W. Warzocha & Kalahari Team

/// @file package_json.h
/// @brief The reading of the JSON of the packages that their kinds, styles and templates share
///
/// For book_type_package.cpp and publishing_texts.cpp only; defined in book_type_package.cpp.

#ifndef KALAHARI_CORE_DETAIL_PACKAGE_JSON_H
#define KALAHARI_CORE_DETAIL_PACKAGE_JSON_H

#include <kalahari/core/localized_text.h>

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>

#include <initializer_list>

namespace kalahari::core::detail {

/// @brief Add the problem "<field>: <message>" to @p problems
void addProblem(QStringList& problems, const QString& field, const QString& message);

/// @brief Whether @p text is the id of a kind, a style or a template: "heading_1"
bool isIdentifier(const QString& text);

/// @brief Whether @p value is a number without a fraction
bool isWholeNumber(const QJsonValue& value);

/// @brief Report the keys of @p object that are not in @p known (a misspelt key would be
/// ignored)
void checkKeys(const QJsonObject& object, const QString& field,
               std::initializer_list<const char*> known, QStringList& problems);

/// @brief The texts by language of @p value, with the English one; empty when it is missing
/// and not @p required
LocalizedText readText(const QJsonValue& value, const QString& field, bool required,
                       QStringList& problems);

}  // namespace kalahari::core::detail

#endif  // KALAHARI_CORE_DETAIL_PACKAGE_JSON_H
