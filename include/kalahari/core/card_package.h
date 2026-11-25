// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2025 Bartosz W. Warzocha
// Additional terms under section 7 of the GNU GPL apply: see NOTICE.md

/// @file card_package.h
/// @brief What a package of book types says of the cards: the fields of the kinds of cards and
/// the relations between the cards

#ifndef KALAHARI_CORE_CARD_PACKAGE_H
#define KALAHARI_CORE_CARD_PACKAGE_H

#include "kalahari/core/book_type_package.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QString>
#include <QStringList>

namespace kalahari::core::card_package {

/// @brief The fields of the cards of a kind, "fields": [{"id": "role", "type": "choice", ...}],
/// each id once and one subtitle at most
[[nodiscard]] QList<CardField> readCardFields(const QJsonValue& value, const QString& field,
                                              QStringList& problems);

/// @brief The parts of kind @p kind of what only a kind of cards has, read from its object
/// @p object: "fields" (readCardFields()) and "chapterRoles" (core::LinkRoles::ofCards())
void readCardKind(const QJsonObject& object, const QString& field, ElementKind& kind,
                  QStringList& problems);

/// @brief The relations between the cards of a package, "relations": [{"id": "sibling",
/// "kind": "character", "other": "character", "name": {...}, ...}], each id once
[[nodiscard]] QList<CardRelation> readRelations(const QJsonValue& value, const QString& field,
                                                QStringList& problems);

/// @brief The problems of the relations of @p package, read with its kinds: each one between
/// two kinds of cards, with an inverse relation between the same kinds the other way round,
/// and each field of a kind of cards that makes a relation naming one between its kind and
/// the kind of the cards it names
void checkRelations(const BookTypePackage& package, QStringList& problems);

}  // namespace kalahari::core::card_package

#endif  // KALAHARI_CORE_CARD_PACKAGE_H
